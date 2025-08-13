// Title	:	Orders.cpp
// Purpose	:	Defines incidents that exist in the world
// Started	:	13/05/2010

#include "Game/Dispatch/Orders.h"

// Game headers
#include "game/Dispatch/Incidents.h"
#include "Game/Dispatch/OrderManager.h"
#include "Network/Events/NetworkEventTypes.h"
#include "peds/ped.h"
#include "peds/pedIntelligence.h"
#include "PedGroup/PedGroup.h"
#include "task/Response/TaskGangs.h"	//for CTaskGangPatrol
#include "weapons/inventory/PedInventoryLoadOut.h"

// Framework includes
#include "ai/aichannel.h"

AI_OPTIMISATIONS()

// allocate an extra order to be used by the network array handler
FW_INSTANTIATE_BASECLASS_POOL(COrder, COrderManager::MAX_ORDERS+1, atHashString("Orders",0x2fa3b9a2), sizeof(CPoliceOrder));

COrder::COrder( )
: m_dispatchType(DT_Invalid)
, m_flagForDeletion(false)
, m_entityArrivedAtIncident(false)
, m_entityFinishedWithIncident(false)
, m_assignedToEntity(false)
, m_orderIndex(0xff)
, m_orderID(0xff)
, m_incidentIndex(0xff)
, m_incidentID(0xff)
, m_uTimeOfAssignment(0)
{

}

COrder::COrder( DispatchType dispatchType, CEntity* pEntity, CIncident* pIncident )
: m_dispatchType(dispatchType)
, m_Entity(pEntity)
, m_flagForDeletion(false)
, m_entityArrivedAtIncident(false)
, m_entityFinishedWithIncident(false)
, m_assignedToEntity(false)
, m_orderIndex(0xff)
, m_orderID(0)
, m_incidentIndex(0xff)
, m_incidentID(0)
, m_uTimeOfAssignment(0)
{
#if __ASSERT
	if (pEntity->GetIsTypePed())
	{
		CPed* pPed = SafeCast(CPed, pEntity);

		Assertf(pPed->GetPedIntelligence()->GetOrder() == NULL, "Order being created for a ped that already has an assigned order!");
	}
#endif

	if (aiVerifyf(pIncident, "Order created with no corresponding incident"))
	{
		s32 incidentIndex = CIncidentManager::GetInstance().GetIncidentIndex(pIncident);

		if (aiVerifyf(incidentIndex >= 0, "Can't find incident for order in incident array"))
		{
			m_incidentIndex = static_cast<u16>(incidentIndex);
		}

		Assign(m_incidentID, pIncident->GetIncidentID());
	}

	if (NetworkInterface::IsGameInProgress())
	{
		if (aiVerifyf(pEntity, "Orders without an entity are not permitted in MP"))
		{
			netObject* pNetObj = NetworkUtils::GetNetworkObjectFromEntity(pEntity);

			if (pNetObj)
			{
				m_orderID = static_cast<u16>(pNetObj->GetObjectID());
			}
		}

		Assertf(m_incidentID < static_cast<u32>(((u64)1u<<COrder::SIZEOF_INCIDENT_ID)-1), 
			"Created order with out of bounds incident id! [%u]  objType %s   orderId %u", 
			m_incidentID, NetworkUtils::GetNetworkObjectFromEntity(pEntity)? NetworkUtils::GetNetworkObjectFromEntity(pEntity)->GetObjectTypeName() : "None", m_orderID);
		
		Assertf(m_incidentID != 0, 
			"Created order with invalid incident id! [%u]  objType %s   orderId %u", 
			m_incidentID, NetworkUtils::GetNetworkObjectFromEntity(pEntity)? NetworkUtils::GetNetworkObjectFromEntity(pEntity)->GetObjectTypeName() : "None", m_orderID);
	}
}

COrder::~COrder()
{
}

bool COrder::GetIsValid() const
{
	return GetEntity() && GetIncident();
}

void COrder::AssignOrderToEntity()
{
	CEntity* pEntity = GetEntity();

	if( pEntity && pEntity->GetIsTypePed() )
	{
		CPed* pPed = static_cast<CPed*>(pEntity);

		bool assignToPed = true;

		if (NetworkInterface::IsGameInProgress())
		{
#if ENABLE_NETWORK_LOGGING
			netLoggingInterface *log = &NetworkInterface::GetObjectManager().GetLog();

			if (pPed->GetNetworkObject())
			{
				netLoggingInterface *log = &NetworkInterface::GetObjectManager().GetLog();

				if (log)
				{
					if (pPed->IsNetworkClone())
					{
						if (m_assignedToEntity)
						{
							NetworkLogUtils::WriteLogEvent(*log, "REASSIGNING_ORDER_TO_CLONE", "%s", pPed->GetNetworkObject()->GetLogName());
						}
						else
						{
							NetworkLogUtils::WriteLogEvent(*log, "ASSIGNING_ORDER_TO_CLONE", "%s", pPed->GetNetworkObject()->GetLogName());
						}
					}
					else
					{
						if (m_assignedToEntity)
						{
							NetworkLogUtils::WriteLogEvent(*log, "REASSIGNING_ORDER_TO_LOCAL", "%s", pPed->GetNetworkObject()->GetLogName());
						}
						else
						{
							NetworkLogUtils::WriteLogEvent(*log, "ASSIGNING_ORDER_TO_LOCAL", "%s", pPed->GetNetworkObject()->GetLogName());
						}
					}

					log->WriteDataValue("Order slot", "%d", m_orderIndex);
					log->WriteDataValue("Order id", "%d", m_orderID);
					log->WriteDataValue("Incident slot", "%d", m_incidentIndex);
					log->WriteDataValue("Incident id", "%d", m_incidentID);
				}
			}
#endif // ENABLE_NETWORK_LOGGING

			// handle the case where the ped already has an assigned order 
			if (pPed->GetPedIntelligence()->GetOrder() && pPed->GetPedIntelligence()->GetOrder() != this)
			{
				CIncident* pCurrentIncident = pPed->GetPedIntelligence()->GetOrder()->GetIncident();
				CIncident* pNewIncident = GetIncident();

				if (AssertVerify(pCurrentIncident) &&
					AssertVerify(pNewIncident) &&
					Verifyf(pCurrentIncident != pNewIncident, "The same incident has assigned 2 separate orders to ped %s", pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : ""))
				{
					LOGGING_ONLY(const netPlayer* pCurrentIncidentArbitrator = pCurrentIncident->GetPlayerArbitrator());

					LOGGING_ONLY(log->WriteDataValue("**ORDER CONFLICT**", "Ped is already assigned and order by incident %d (arbitrated by %s)", pCurrentIncident->GetIncidentID(), pCurrentIncidentArbitrator ? pCurrentIncidentArbitrator->GetLogName() : "??"));
						
					if (IsLocallyControlled())
					{
						LOGGING_ONLY(log->WriteDataValue("FAIL", "Previous order kept, this order dumped"));
						SetFlagForDeletion(true);
						assignToPed = false;
					}
					else
					{
						LOGGING_ONLY(Assertf(0, "Trying to assign a remote order to %s, which has already been assigned an order by incident %d (arbitrated by %s)", pPed->GetNetworkObject()->GetLogName(), pCurrentIncident->GetIncidentID(), pCurrentIncidentArbitrator ? pCurrentIncidentArbitrator->GetLogName(): "??");)
					}
				}
			}
		}
	
		if (assignToPed)
		{
			pPed->GetPedIntelligence()->SetOrder(this);

			if(!m_assignedToEntity)
			{
				m_assignedToEntity = true;
			}

			if(m_uTimeOfAssignment == 0)
			{
				m_uTimeOfAssignment = fwTimer::GetTimeInMilliseconds();
			}
		}
	}
}

void COrder::RemoveOrderFromEntity() 
{
	aiAssert(IsLocallyControlled());

	CEntity* pEntity = GetEntity();

	SetEntityFinishedWithIncident();

	if( pEntity && pEntity->GetIsTypePed() )
	{
		CPed* pPed = static_cast<CPed*>(pEntity);
		if(pPed->GetPedIntelligence()->GetOrder() == this )
		{
#if ENABLE_NETWORK_LOGGING
			if (pPed->GetNetworkObject())
			{
				netLoggingInterface *log = &NetworkInterface::GetObjectManager().GetLog();

				if (log)
				{
					if (pPed->IsNetworkClone())
					{
						NetworkLogUtils::WriteLogEvent(*log, "REMOVING_ORDER_FROM_CLONE", "%s", pPed->GetNetworkObject()->GetLogName());
					}
					else
					{
						NetworkLogUtils::WriteLogEvent(*log, "REMOVING_ORDER_FROM_LOCAL", "%s", pPed->GetNetworkObject()->GetLogName());
					}
					log->WriteDataValue("Order slot", "%d", m_orderIndex);
					log->WriteDataValue("Order id", "%d", m_orderID);
					log->WriteDataValue("Incident slot", "%d", m_incidentIndex);
					log->WriteDataValue("Incident id", "%d", m_incidentID);
				}
			}
#endif // ENABLE_NETWORK_LOGGING
			pPed->GetPedIntelligence()->SetOrder(NULL);

			m_assignedToEntity = false;
		}
	}
}

void COrder::SetFlagForDeletion( bool val )
{
	aiAssert(IsLocallyControlled());

	if( val )
	{
		RemoveOrderFromEntity();
	}
	else
	{
		AssignOrderToEntity();
	}
	m_flagForDeletion = val;
}

CIncident*	COrder::GetIncident() const
{
	if (Verifyf(m_incidentIndex != 0xFF && m_incidentIndex < CIncidentManager::MAX_INCIDENTS, "Order in slot %d (id: %d) has not been assigned an incident (incident idx: %d)"
		, m_orderIndex, m_orderID, m_incidentIndex))
	{
		return CIncidentManager::GetInstance().GetIncident(m_incidentIndex);
	}

	return NULL;
}

void COrder::SetEntityArrivedAtIncident()
{
	if (!m_entityArrivedAtIncident)
	{
		CIncident* pIncident = GetIncident();
		if( pIncident )
		{
			if (IsLocallyControlled())
			{
				pIncident->SetArrivedResources(m_dispatchType, pIncident->GetArrivedResources(m_dispatchType)+1);
			}
			else
			{
				// send an event to the machine which controls this order
				CIncidentEntityEvent::Trigger(m_incidentID, m_dispatchType, CIncidentEntityEvent::ENTITY_ACTION_ARRIVED);
			}
		}

		m_entityArrivedAtIncident = true;
	}
}

void COrder::SetEntityFinishedWithIncident()
{
	if (!m_entityFinishedWithIncident)
	{
		CIncident* pIncident = GetIncident();
		if( pIncident && pIncident->ShouldCountFinishedResources())
		{
			if (pIncident->IsLocallyControlled())
			{
				pIncident->SetFinishedResources(m_dispatchType, pIncident->GetFinishedResources(m_dispatchType)+1);
			}
			else
			{
				// send an event to the machine which controls this order
				CIncidentEntityEvent::Trigger(m_incidentID, m_dispatchType, CIncidentEntityEvent::ENTITY_ACTION_FINISHED);
			}
		}

		m_entityFinishedWithIncident = true;
	}
}

bool COrder::GetAllResourcesAreDone() const
{
	CIncident* pIncident = GetIncident();
	if (pIncident)
	{
		return pIncident->GetArrivedResources(m_dispatchType) >0
			&& pIncident->GetFinishedResources(m_dispatchType) >= pIncident->GetArrivedResources(m_dispatchType);
	}

	return false;
}	

bool COrder::GetIncidentHasBeenAddressed() const
{
	CIncident* pIncident = GetIncident();
	if (pIncident)
	{
		return pIncident->GetHasBeenAddressed();
	}

	return false;
}

void COrder::SetIncidentHasBeenAddressed(bool bHasBeenAddressed)
{
	CIncident* pIncident = GetIncident();
	if (pIncident)
	{
		if (pIncident->IsLocallyControlled())
		{
			pIncident->SetHasBeenAddressed(bHasBeenAddressed);		
		}
		else
		{
			// send an event to the machine which controls this incident
			CIncidentEntityEvent::Trigger(m_incidentID, bHasBeenAddressed);
		}		
	}
}

bool COrder::IsLocallyControlled() const
{
	if (aiVerify(m_orderIndex != 0xff))
	{
		return COrderManager().GetInstance().IsOrderLocallyControlled(m_orderIndex);
	}

	return false;
}

bool COrder::IsRemotelyControlled() const
{
	if (aiVerify(m_orderIndex != 0xff))
	{
		return COrderManager().GetInstance().IsOrderRemotelyControlled(m_orderIndex);
	}

	return false;
}

void COrder::CopyNetworkInfo(const COrder& order)
{
	m_dispatchType					= order.m_dispatchType;
	m_incidentID					= order.m_incidentID;
	m_entityArrivedAtIncident		= order.m_entityArrivedAtIncident;
	m_entityFinishedWithIncident	= order.m_entityFinishedWithIncident;

	unsigned incidentIndex = 0xff;

	CIncidentManager::GetInstance().GetIncidentFromIncidentId(m_incidentID, &incidentIndex);

	m_incidentIndex = (u16)incidentIndex;
}


#if __BANK
const char* COrder::GetDispatchTypeName() const
{
	return CDispatchManager::GetInstance().GetDispatchTypeName(m_dispatchType);
}
#endif

CPoliceOrder::CPoliceOrder()
: m_vDispatchLocation(Vector3::ZeroType)
, m_bNeedsToUpdateLocation(false)
, m_vSearchLocation(Vector3::ZeroType)
, m_ePoliceDispatchOrder(PO_NONE)
{

}

CPoliceOrder::CPoliceOrder(DispatchType dispatchType, CEntity* pEntity, CIncident* pIncident, ePoliceDispatchOrderType ePoliceDispatchOrder, const Vector3& vDispatchLocation)
: COrder(dispatchType, pEntity, pIncident)
, m_vDispatchLocation(vDispatchLocation)
, m_bNeedsToUpdateLocation(false)
, m_vSearchLocation(Vector3::ZeroType)
, m_ePoliceDispatchOrder(ePoliceDispatchOrder)
{

}

CPoliceOrder::~CPoliceOrder()
{

}

CSwatOrder::CSwatOrder()
: m_vStagingLocation(Vector3::ZeroType)
, m_bNeedsToUpdateLocation(false)
, m_eSwatDispatchOrder(SO_NONE)
, m_bAllowAdditionalPed(false)
, m_bAdditionalRappellingOrder(false)
{

}

CSwatOrder::CSwatOrder(DispatchType dispatchType, CEntity* pEntity, CIncident* pIncident, eSwatDispatchOrderType eSwatDispatchOrder, const Vector3& vStagingLocation, bool bAdditionalRappellingOrder)
: COrder(dispatchType, pEntity, pIncident)
, m_vStagingLocation(vStagingLocation)
, m_bNeedsToUpdateLocation(false)
, m_eSwatDispatchOrder(eSwatDispatchOrder)
, m_bAllowAdditionalPed(false)
, m_bAdditionalRappellingOrder(bAdditionalRappellingOrder)
{

}

CSwatOrder::~CSwatOrder()
{

}

void CSwatOrder::AssignOrderToEntity()
{
	COrder::AssignOrderToEntity();

	CEntity* pEntity = GetEntity();
	if(!pEntity || !pEntity->GetIsTypePed())
	{
		return;
	}

	CPed* pPed = static_cast<CPed*>(pEntity);
	if (pPed)
	{
		if (!pPed->IsNetworkClone() && pPed->GetPedIntelligence())
		{
			CTask* pDefaultTask = pPed->ComputeDefaultTask(*pPed);
			if(pDefaultTask)
			{
				if (NetworkInterface::IsGameInProgress())
				{
					// in MP a ped can be given a different order after being assigned an initial one
					CTask* pCurrentTask = pPed->GetPedIntelligence()->GetTaskAtPriority(PED_TASK_TREE_PRIMARY);

					// if the ped is already running a swat task, keep it 
					if (!(pCurrentTask && pCurrentTask->GetTaskType() >= CTaskTypes::TASK_SWAT && pCurrentTask->GetTaskType() <= CTaskTypes::TASK_SWAT_FOLLOW_IN_LINE))
					{
						pPed->GetPedIntelligence()->AddTaskDefault(pDefaultTask);
					}
					else
					{
						delete pDefaultTask;
						pDefaultTask = NULL;
					}
				}
				else
				{
					pPed->GetPedIntelligence()->AddTaskDefault(pDefaultTask);
				}
			}
		}

		if (m_bAdditionalRappellingOrder)
		{
			// Setup our newly spawned ped with the right weapons
			static const atHashWithStringNotFinal COP_HELI_LOADOUT("LOADOUT_SWAT_NO_LASER",0xC9705531);
			CPedInventoryLoadOutManager::SetLoadOut(pPed, COP_HELI_LOADOUT.GetHash());

			CPedGroup* pGroup = pPed->GetPedsGroup();

			// Add the ped to the group
			if(pGroup && pGroup->IsLocallyControlled() && !pGroup->GetGroupMembership()->IsMember(pPed))
			{
				pGroup->GetGroupMembership()->AddFollower(pPed);
				pGroup->Process();
			}
		}
	}
}


CAmbulanceOrder::CAmbulanceOrder( )
{

}

CAmbulanceOrder::CAmbulanceOrder( DispatchType dispatchType, CEntity* pEntity, CIncident* pIncident )
: COrder(dispatchType, pEntity, pIncident)
{

}

CAmbulanceOrder::~CAmbulanceOrder()
{
}

CInjuryIncident* CAmbulanceOrder::GetInjuryIncident() const
{
	CIncident* pIncident = GetIncident();
	if (pIncident)
	{
		CInjuryIncident* pInjuryIncident = pIncident->GetType() == CIncident::IT_Injury ? static_cast<CInjuryIncident*>(pIncident) : NULL;

		return pInjuryIncident;
	}

	return NULL;
}

const CPed* CAmbulanceOrder::GetDriver() const
{
	CInjuryIncident* pIncident = GetInjuryIncident();
	if (pIncident)
	{
		return pIncident->GetDriver();
	}

	return NULL;
}

const CPed* CAmbulanceOrder::GetRespondingPed() const
{
	CInjuryIncident* pIncident = GetInjuryIncident();
	if (pIncident)
	{
		return pIncident->GetRespondingPed();
	}

	return NULL;
}

CVehicle* CAmbulanceOrder::GetEmergencyVehicle() const
{
	CInjuryIncident* pIncident = GetInjuryIncident();
	if (pIncident)
	{
		return pIncident->GetEmergencyVehicle();
	}

	return NULL;
}

void CAmbulanceOrder::SetDriver(CPed* pDriver)
{
	CInjuryIncident* pIncident = GetInjuryIncident();
	if (pIncident)
	{
		pIncident->SetDriver(pDriver);
	}
}

void CAmbulanceOrder::SetRespondingPed(CPed* pMedic)
{
	CInjuryIncident* pIncident = GetInjuryIncident();
	if (pIncident)
	{
		pIncident->SetRespondingPed(pMedic);
	}
}

void CAmbulanceOrder::SetEmergencyVehicle(CVehicle* pAmbulance)
{
	CInjuryIncident* pIncident = GetInjuryIncident();
	if (pIncident)
	{
		pIncident->SetEmergencyVehicle(pAmbulance);
	}
}

CFireOrder::CFireOrder()
{

}

CFireOrder::CFireOrder( DispatchType dispatchType, CEntity* pEntity, CIncident* pIncident )
: COrder(dispatchType, pEntity, pIncident)
{

}

CFireOrder::~CFireOrder()
{
}

CFireIncident* CFireOrder::GetFireIncident() const
{
	CIncident* pIncident = GetIncident();
	if (pIncident)
	{
		CFireIncident* pFireIncident = pIncident->GetType() == CIncident::IT_Fire ? static_cast<CFireIncident*>(pIncident) : NULL;

		return pFireIncident;
	}

	return NULL;
}

CVehicle* CFireOrder::GetFireTruck() const
{
	CFireIncident* pIncident = GetFireIncident();
	if (pIncident)
	{
		return pIncident->GetFireTruck();
	}

	return NULL;
}

void CFireOrder::SetFireTruck(CVehicle* pAmbulance)
{
	CFireIncident* pIncident = GetFireIncident();
	if (pIncident)
	{
		pIncident->SetFireTruck(pAmbulance);
	}
}

CFire* CFireOrder::GetBestFire(CPed* pPed) const
{
	if (!GetFireIncident())
	{
		return NULL;
	}

	return GetFireIncident()->GetBestFire(pPed);
}

CEntity* CFireOrder::GetEntityThatHasBeenOnFire() const
{
	if (!GetFireIncident())
	{
		return NULL;
	}

	return GetFireIncident()->GetEntityThatHasBeenOnFire();
}

int CFireOrder::GetNumFires() const
{
	if (!GetFireIncident())
	{
		return 0;
	}

	return GetFireIncident()->GetNumFires();
}

void CFireOrder::IncFightingFireCount(CFire* pFire, CPed* pPed)
{
	if (!GetFireIncident())
	{
		return;
	}
	
	GetFireIncident()->IncFightingFireCount(pFire,pPed);
}

void CFireOrder::DecFightingFireCount(CFire* pFire)
{
	if (!GetFireIncident())
	{
		return;
	}

	GetFireIncident()->DecFightingFireCount(pFire);
}

CPed* CFireOrder::GetPedRespondingToFire(CFire* pFire)
{
	if (!GetFireIncident())
	{
		return NULL;
	}

	return GetFireIncident()->GetPedRespondingToFire(pFire);
}


CGangOrder::CGangOrder()
{

}

CGangOrder::CGangOrder( DispatchType dispatchType, CEntity* pEntity, CIncident* pIncident )
: COrder(dispatchType, pEntity, pIncident)
{

}

CGangOrder::~CGangOrder()
{
}

void CGangOrder::AssignOrderToEntity()
{
	//Call the base class version.
	COrder::AssignOrderToEntity();

	if(!GetEntity() || !GetEntity()->GetIsTypePed())
	{
		return;
	}

	//Check if the ped is valid.
	CPed* pPed = static_cast<CPed *>(GetEntity());
	if(pPed && !pPed->IsNetworkClone())
	{
		//Never lose the target.
		pPed->GetPedIntelligence()->GetCombatBehaviour().SetTargetLossResponse(CCombatData::TLR_NeverLoseTarget);

		//Allow drive-bys.
		pPed->GetPedIntelligence()->GetCombatBehaviour().SetFlag(CCombatData::BF_CanDoDrivebys);

		//Always fight.
		pPed->GetPedIntelligence()->GetCombatBehaviour().SetFlag(CCombatData::BF_AlwaysFight);

		//Disable all randoms flee.
		pPed->GetPedIntelligence()->GetCombatBehaviour().SetFlag(CCombatData::BF_DisableAllRandomsFlee);

		//Don't flee from indirect threats.
		pPed->GetPedIntelligence()->GetFleeBehaviour().GetFleeFlags().SetFlag(CFleeBehaviour::BF_DisableFleeFromIndirectThreats);

		//Disable flying through the windscreen.
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_WillFlyThroughWindscreen, false);

		//Disable shocking events.
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_DisableShockingEvents, true);

		//Don't remove the ped while they have an order.
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_DontRemoveWithValidOrder, true);

		//Add a pistol.
		pPed->GetInventory()->AddWeaponAndAmmo(WEAPONTYPE_PISTOL, CWeapon::INFINITE_AMMO);

		//Check if the target is valid.
		if(AssertVerify(GetIncident()) && GetIncident()->GetEntity() && GetIncident()->GetEntity()->GetIsTypePed())
		{
			//Set the default task.
			CPed* pTargetPed = static_cast<CPed*>(GetIncident()->GetEntity());
			CTaskGangPatrol* pPatrolTask = rage_new CTaskGangPatrol(pTargetPed);
			pPed->GetPedIntelligence()->AddTaskDefault(pPatrolTask);
		}
	}
}
