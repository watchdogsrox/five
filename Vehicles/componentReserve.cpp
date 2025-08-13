// Title	:	componentReserve.cpp
// Author	:	Bryan Musson
// Started	:	06/04/11
//

#include "componentReserve.h"
// 
// // Game headers
#include "debug/DebugVehicleInteraction.h"
#include "peds/Ped.h"
#include "modelinfo/BaseModelInfo.h"
#include "modelinfo/PedModelInfo.h"
#include "modelinfo/VehicleModelInfo.h"
#include "network/events/NetworkEventTypes.h"

AI_OPTIMISATIONS()

bank_bool CComponentReservation::DISPLAY_COMPONENT_USE = false;

//-------------------------------------------------------------------------
// Class for reserving each component
//-------------------------------------------------------------------------
CComponentReservation::CComponentReservation()
{
	m_pMyManager = NULL;
	m_pMyEntity = NULL;
	m_pPedUsingComponent = NULL;
	m_pLocalPedRequestingUse = NULL;
	m_iBoneIndex = -1;
	m_bAwaitingRequestResponse = false;
	m_requestTimer = 0;
}

void CComponentReservation::Init(CDynamicEntity* pDynamicEntity, CComponentReservationManager* pCRM)
{
	m_pMyManager = pCRM; 
	m_pMyEntity = pDynamicEntity;
}

CComponentReservation::~CComponentReservation()
{
	if( m_pPedUsingComponent )
	{
		m_pPedUsingComponent = NULL;
	}
	if( m_pLocalPedRequestingUse )
	{
		m_pLocalPedRequestingUse = NULL;
	}
}

//-------------------------------------------------------------------------
// SetBoneIndex to a particular component
//-------------------------------------------------------------------------
void CComponentReservation::SetBoneIndex(int iBoneIndex)
{
	m_iBoneIndex = (s16)iBoneIndex;
}


//-------------------------------------------------------------------------
// Get index into the vehicles array that this component is stored in
//-------------------------------------------------------------------------
s32 CComponentReservation::GetVehicleArrayIndex()
{
	return m_pMyManager->FindComponentReservationArrayIndex(m_iBoneIndex);
}


//-------------------------------------------------------------------------
// Send a request across the network to whoever owns this vehicle
//-------------------------------------------------------------------------
void CComponentReservation::SendRequest(bool bReserve)
{
	// Send a request to m_pMyEntity owner to reserve or release it for m_pLocalPedRequestingUse
	if( bReserve )
	{
		if (AssertVerify(m_pMyEntity) && m_pMyEntity->GetNetworkObject())
		{
			if (AssertVerify(m_pLocalPedRequestingUse) && m_pLocalPedRequestingUse->GetNetworkObject())
			{
                CVehicleComponentControlEvent::Trigger(m_pMyEntity->GetNetworkObject()->GetObjectID(), m_pLocalPedRequestingUse->GetNetworkObject()->GetObjectID(), GetVehicleArrayIndex(), bReserve);
				m_bAwaitingRequestResponse = true;
				m_requestTimer = 200;
			}
		}
	}
	else
	{
		if (AssertVerify(m_pMyEntity) && m_pMyEntity->GetNetworkObject())
		{
			if (AssertVerify(m_pPedUsingComponent) && m_pPedUsingComponent->GetNetworkObject())
			{
                CVehicleComponentControlEvent::Trigger(m_pMyEntity->GetNetworkObject()->GetObjectID(), m_pPedUsingComponent->GetNetworkObject()->GetObjectID(), GetVehicleArrayIndex(), bReserve);
				m_bAwaitingRequestResponse = true;
				m_requestTimer = 200;
			}
		}
	}
}

//-------------------------------------------------------------------------
// Receive a request across the network for whoever owns ped CPed*
//-------------------------------------------------------------------------
void CComponentReservation::ReceiveRequestResponse( CPed* pPed, bool bReserve, bool bGranted )
{
	Assert(pPed);

	if(bGranted)
	{
		if(bReserve)
		{
			UpdatePedUsingComponentFromNetwork(pPed);
		}
		else
		{
			if (IsComponentReservedForPed(pPed))
				UpdatePedUsingComponentFromNetwork(0);

			Assert(!IsComponentReservedForPed(pPed));
		}
	}

	m_bAwaitingRequestResponse = false;
	m_requestTimer = 0;
}

//-------------------------------------------------------------------------
// Update the ped using the component from a network update
//-------------------------------------------------------------------------
void CComponentReservation::UpdatePedUsingComponentFromNetwork( CPed* pPed )
{
#if DR_ENABLED
	debugPlayback::RecordUpdatePedUsingComponentFromNetwork(this, pPed);
#endif // DR_ENABLED
	m_pPedUsingComponent = pPed;
}

//-------------------------------------------------------------------------
// Reserves the use of the component for the ped passed
//-------------------------------------------------------------------------
bool CComponentReservation::ReserveUseOfComponent( CPed* pPed )
{
	Assert( !m_pLocalPedRequestingUse );
	Assert( !m_pPedUsingComponent );

#if DR_ENABLED
	debugPlayback::RecordReserveUseOfComponent(this, *pPed);
#endif // DR_ENABLED

	// Set up the local request
	if (!pPed->IsNetworkClone())
	{
		m_pLocalPedRequestingUse = pPed;
	}

	bool bVehicleIsControlledLocally = true;
	bVehicleIsControlledLocally = !m_pMyEntity->IsNetworkClone() && !(m_pMyEntity->GetNetworkObject() && m_pMyEntity->GetNetworkObject()->IsPendingOwnerChange());

	if( bVehicleIsControlledLocally )
	{
#if __DEV
		taskDebugf2("Frame : %u - %s%s : 0x%p : Reserving Use Of Component With Reservation 0x%p : Bone Index %i", fwTimer::GetFrameCount(), pPed->IsNetworkClone() ? "Clone ped : " : "Local ped : ", pPed->GetDebugName(), pPed, this, m_iBoneIndex);
#endif
		m_pPedUsingComponent = pPed;
		return true;
	}

	// Must wait to get control of component
	return false;
}


//-------------------------------------------------------------------------
// Un-reserves the use of the component for the ped passed
//-------------------------------------------------------------------------
bool CComponentReservation::UnreserveUseOfComponent( CPed* pPed )
{
#if DR_ENABLED
	if (pPed)
	{
		debugPlayback::RecordUnReserveUseOfComponent(this, *pPed);
	}
#endif // DR_ENABLED

	Assert(m_pPedUsingComponent == pPed);
	if( m_pLocalPedRequestingUse == pPed )
	{
		m_pLocalPedRequestingUse = NULL;
	}

	bool bVehicleIsControlledLocally = true;
	bVehicleIsControlledLocally = !m_pMyEntity->IsNetworkClone() && !(m_pMyEntity->GetNetworkObject() && m_pMyEntity->GetNetworkObject()->IsPendingOwnerChange());

	if( bVehicleIsControlledLocally )
	{
		m_pPedUsingComponent = NULL;
		return true;
	}

	return false;
}


//-------------------------------------------------------------------------
// Returns if this component is reserved for this ped
//-------------------------------------------------------------------------
bool	CComponentReservation::IsComponentReservedForPed( const CPed* pPed ) const
{
	return m_pPedUsingComponent == pPed;
}


//-------------------------------------------------------------------------
// Lets the point know that the ped is still using this component
//-------------------------------------------------------------------------
void CComponentReservation::SetPedStillUsingComponent( CPed* pPed )
{
#if DR_ENABLED
	debugPlayback::RecordKeepUseOfComponent(this, *pPed);
#endif // DR_ENABLED

	m_pLocalPedRequestingUse = pPed;
#if __DEV
	taskDebugf2("Frame : %u - %s%s : 0x%p : Set To Still Use Component With Reservation 0x%p : Bone Index %i", fwTimer::GetFrameCount(), pPed->IsNetworkClone() ? "Clone ped : " : "Local ped : ", pPed->GetDebugName(), pPed, this, m_iBoneIndex);
	if (m_pPedUsingComponent != pPed)
	{
		taskWarningf("Ped using component %s not the same as the ped set to keep using it %s", m_pPedUsingComponent ? m_pPedUsingComponent->GetModelName() : "NULL", pPed ? pPed->GetModelName() : "NULL");
	}
#endif
}

//-------------------------------------------------------------------------
// Returns if this component is already in use
//-------------------------------------------------------------------------
bool	CComponentReservation::IsComponentInUse()
{
	return m_pPedUsingComponent != (CPed*)NULL || m_pLocalPedRequestingUse != (CPed*)NULL;
}

//-------------------------------------------------------------------------
// Update called each frame, to update componenet requests
//-------------------------------------------------------------------------
void	CComponentReservation::Update()
{	
#if DR_ENABLED
	debugPlayback::RecordComponentReservation(this, true);
#endif // DR_ENABLED

#if DEBUG_DRAW
	// Component reservation debug.
	if( CComponentReservation::DISPLAY_COMPONENT_USE )
	{
		Vector3 vComponentPos;
		s32 boneId = GetBoneIndex();
		if(boneId!=-1)
		{
			Matrix34 m_BoneMat;
			m_pMyEntity->GetGlobalMtx(boneId, m_BoneMat);
			vComponentPos = m_BoneMat.d;

			vComponentPos.z += 1.5f;
			Color32 colour(0.0f, 1.0f, 0.0f);
			if(m_pPedUsingComponent)
			{
				colour = Color32(1.0f, 0.0f, 0.0f);
				grcDebugDraw::Line( vComponentPos, VEC3V_TO_VECTOR3(m_pPedUsingComponent->GetTransform().GetPosition()), colour );
			}
			else if( m_pLocalPedRequestingUse )
			{
				colour = Color32(0.0f, 0.0f, 1.0f);
				grcDebugDraw::Line( vComponentPos, VEC3V_TO_VECTOR3(m_pLocalPedRequestingUse->GetTransform().GetPosition()), colour );
			}
			grcDebugDraw::Sphere(vComponentPos, 0.25f, colour );
		}
	}
#endif

	bool bVehicleIsControlledLocally = true;
    bool bVehiclePendingOwnerChange  = (m_pMyEntity->GetNetworkObject() && m_pMyEntity->GetNetworkObject()->IsPendingOwnerChange());
	bVehicleIsControlledLocally = !m_pMyEntity->IsNetworkClone() && !bVehiclePendingOwnerChange;

	if (m_requestTimer > 0)
	{
		Assert(fwTimer::GetTimeStepInMilliseconds() < 32768);
		m_requestTimer = m_requestTimer - static_cast<s16>(fwTimer::GetTimeStepInMilliseconds());

		if (m_requestTimer <= 0)
		{
			m_requestTimer = 0;
			m_bAwaitingRequestResponse = false;
		}
	}

	// Check to see if any requests need to be sent
	if( m_pLocalPedRequestingUse )
	{
		bool bCheckPedIsControlledLocally = true;
		bCheckPedIsControlledLocally = !m_pLocalPedRequestingUse->IsNetworkClone();
		// If there is currently no ped reserving the componenet, send a request
		if( bCheckPedIsControlledLocally &&
			m_pPedUsingComponent == (CPed*)NULL )
		{
			// send the request requesting the network owner of the vehicle to reserve the component for this ped
			if( bVehicleIsControlledLocally )
			{
				m_pPedUsingComponent = m_pLocalPedRequestingUse;
				m_bAwaitingRequestResponse = false;
			}
			else
			{
				if(!m_bAwaitingRequestResponse && !bVehiclePendingOwnerChange)
				{
					SendRequest( true );
				}
			}
		}

		// Remove the local ped reserved component, this needs to be set by the local ped each frame
		m_pLocalPedRequestingUse = NULL;
	}
	// No local peds should need to reserve this component
	else if( m_pPedUsingComponent )
	{
		bool bPedIsControlledLocally = true;
		bPedIsControlledLocally = !m_pPedUsingComponent->IsNetworkClone();
		// Send a message telling the network owner of the vehicle to free the component
		if( bPedIsControlledLocally )
		{
			if( bVehicleIsControlledLocally )
			{
				m_pPedUsingComponent = NULL;
			}
			else
			{
				if(!m_bAwaitingRequestResponse && !bVehiclePendingOwnerChange)
				{
					SendRequest( false );
				}
			}
		}
	}

#if DR_ENABLED
	debugPlayback::RecordComponentReservation(this, false);
#endif // DR_ENABLED
}

CComponentReservationManager::CComponentReservationManager() 
{
	m_iNumReserveComponents = 0;
	m_pModelInfo = NULL;
}

void CComponentReservationManager::Init(CDynamicEntity* pDynamicEntity, CBaseModelInfo* pModelInfo) {
	m_iNumReserveComponents = 0;
	m_pModelInfo = pModelInfo;
	int i;
	// Set up the reserve components vehicle pointers
	for(i = 0; i < CComponentReservation::MAX_NUM_COMPONENTS; i++)
		m_aComponentReservations[i].Init(pDynamicEntity, this);

	// Go through each entry exit point in the model and add in components
	for( i = 0; i < GetModelSeatInfo()->GetNumberEntryExitPoints(); i++ )
	{
		// secondary parameter, adds if its not already present
		FindComponentReservation(GetModelSeatInfo()->GetBoneIndexFromEntryPoint(i), true);
	}

	// Do the same for seats
	for( i = 0; i < GetModelSeatInfo()->GetNumSeats(); i++ )
	{
		FindComponentReservation(GetModelSeatInfo()->GetBoneIndexFromSeat(i), true);
	}
}

const CModelSeatInfo* CComponentReservationManager::GetModelSeatInfo() 
{
	if (m_pModelInfo) {
		switch (m_pModelInfo->GetModelType()) 
		{
			case MI_TYPE_VEHICLE:
				return static_cast<CVehicleModelInfo*>(m_pModelInfo)->GetModelSeatInfo(); 
			case MI_TYPE_PED:
				return static_cast<CPedModelInfo*>(m_pModelInfo)->GetModelSeatInfo(); 
			default:
				return NULL;
		}
	}
	return NULL;
}

//-------------------------------------------------------------------------
// Updates the reserve components
//-------------------------------------------------------------------------
void CComponentReservationManager::Update()
{
	//////////////////////////////////
	// Update the reserve components
	for( u32 i = 0; i < m_iNumReserveComponents; i++ )
	{
		m_aComponentReservations[i].Update();
	}
}

//-------------------------------------------------------------------------
// Returns the reservation component for the entrypoints seat
//-------------------------------------------------------------------------
CComponentReservation* CComponentReservationManager::GetSeatReservation(s32 iEntryExitPoint, SeatAccess whichSeatAccess, s32 iSeat)
{
	if (!GetModelSeatInfo())
		return NULL;
	const CEntryExitPoint* pEntryExitPoint = GetModelSeatInfo()->GetEntryExitPoint(iEntryExitPoint);
	aiAssertf(pEntryExitPoint, "NULL entry point for index %u", iEntryExitPoint);
	int iSeatIndex = pEntryExitPoint->GetSeat(whichSeatAccess, iSeat);
	aiAssertf(iSeatIndex > -1, "Invalid seat index %u", iSeatIndex);
	return FindComponentReservation(GetModelSeatInfo()->GetBoneIndexFromSeat(iSeatIndex), false);
}

//-------------------------------------------------------------------------
// Returns the reservation component for the seat
//-------------------------------------------------------------------------
CComponentReservation*	CComponentReservationManager::GetSeatReservationFromSeat( s32 whichSeat )
{
	if (!GetModelSeatInfo())
		return NULL;
	int iSeatBoneIndex = GetModelSeatInfo()->GetBoneIndexFromSeat(whichSeat);
	return FindComponentReservation(iSeatBoneIndex, false);
}


//-------------------------------------------------------------------------
// Returns the reservation component for the entrypoints door
//-------------------------------------------------------------------------
CComponentReservation* CComponentReservationManager::GetDoorReservation(s32 iEntryExitPoint)
{
	if (!GetModelSeatInfo())
		return NULL;
	return FindComponentReservation(GetModelSeatInfo()->GetBoneIndexFromEntryPoint(iEntryExitPoint), false);
}


//-------------------------------------------------------------------------
// Finds the component in the array
//-------------------------------------------------------------------------
bool CComponentReservationManager::GetAreAnyComponentsReserved()
{
	for( u32 i = 0; i < m_iNumReserveComponents; i++ )
	{
		if( m_aComponentReservations[i].IsComponentInUse() )
		{
			return true;
		}
	}

	return false;
}

//-------------------------------------------------------------------------
// Finds the component in the array
//-------------------------------------------------------------------------
CComponentReservation* CComponentReservationManager::FindComponentReservation(int iBoneIndex, bool bAddIfNotFound)
{
	for( u32 i = 0; i < m_iNumReserveComponents; i++ )
	{
		if( m_aComponentReservations[i].GetBoneIndex() == iBoneIndex )
		{
			return &m_aComponentReservations[i];
		}
	}
	if( bAddIfNotFound && iBoneIndex > -1)
	{
		return AddComponentReservation(iBoneIndex);
	}
	return NULL;
}

//-------------------------------------------------------------------------
// Returns the array index of the component
//-------------------------------------------------------------------------
s32	CComponentReservationManager::FindComponentReservationArrayIndex(int iBoneIndex)
{
	for( u32 i = 0; i < m_iNumReserveComponents; i++ )
	{
		if( m_aComponentReservations[i].GetBoneIndex() == iBoneIndex )
		{
			return i;
		}
	}
	return -1;
}
//-------------------------------------------------------------------------
// Finds the component in the array
//-------------------------------------------------------------------------
CComponentReservation*	CComponentReservationManager::FindComponentReservationByArrayIndex(s32 iComponent)
{
	Assert(u32(iComponent) < m_iNumReserveComponents);
	return &m_aComponentReservations[iComponent];
}

//-------------------------------------------------------------------------
// Adds a new component in and returns a pointer to it
//-------------------------------------------------------------------------
CComponentReservation* CComponentReservationManager::AddComponentReservation(int iBoneIndex)
{
	Assert( m_iNumReserveComponents < CComponentReservation::MAX_NUM_COMPONENTS );
	m_aComponentReservations[m_iNumReserveComponents].SetBoneIndex(iBoneIndex);
	++m_iNumReserveComponents;
	return &m_aComponentReservations[m_iNumReserveComponents-1];
}

//-------------------------------------------------------------------------
// If it returns true, the component has been reserved for this ped
//-------------------------------------------------------------------------
bool CComponentReservationManager::ReserveVehicleComponent( CPed* pPed, CDynamicEntity* pEntity, CComponentReservation* pComponentReservation )
{
	if (pComponentReservation)
	{
		// Already reserved point, skip to the next stage
		if( pComponentReservation->IsComponentReservedForPed( pPed ) )
		{
			return true;
		}
		// If the component is currently in use, wait till it's available
		else if( pComponentReservation->IsComponentInUse() )
		{
			return false;
		}
		// we can't reserve use if the vehicle is migrating
		else if (pEntity->GetNetworkObject() && pEntity->GetNetworkObject()->IsPendingOwnerChange())
		{
			return false;
		}
		// Try to immediately reserve use of the door, if this succeeds the task can move to the next stage
		else if( pComponentReservation->ReserveUseOfComponent( pPed ) )
		{
			return true;
		}
		// Waiting to reserve point, check next frame
		else
		{
			return false;
		}
	}
	return false;
}

//-------------------------------------------------------------------------
// If it returns true, the component has been reserved for this ped
//-------------------------------------------------------------------------
void CComponentReservationManager::UnreserveVehicleComponent( CPed* pPed, CComponentReservation* pComponentReservation )
{
	if( pComponentReservation->GetPedUsingComponent() == pPed )
	{
		pComponentReservation->UnreserveUseOfComponent(pPed);
	}
}
