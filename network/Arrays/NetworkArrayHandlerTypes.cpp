//
// name:		NetworkArrayHandlerTypes.cpp
// description:	The different array handler types
// written by:	John Gurney
//

// rage includes
#include "data/bitbuffer.h"

// framework includes
#include "fwscript/scriptHandler.h"
#include "fwscript/scriptinterface.h"
#include "fwutil/KeyGen.h"

// game includes
#include "event/EventGroup.h"
#include "event/EventNetwork.h"
#include "frontend/PauseMenu.h"
#include "network/arrays/NetworkArrayHandlerTypes.h"
#include "network/General/NetworkPendingProjectiles.h"
#include "Network/NetworkInterface.h"
#include "Network/Arrays/NetworkArrayMgr.h"
#include "network/events/NetworkEventTypes.h"
#include "Network/Objects/NetworkObjectMgr.h"
#include "peds/ped.h"
#include "peds/PedIntelligence.h"
#include "scene/world/GameWorld.h"
#include "script/Handlers/GameScriptHandlerNetwork.h"
#include "script/Handlers/GameScriptResources.h"
#include "script/script.h"

NETWORK_OPTIMISATIONS()

FW_INSTANTIATE_CLASS_POOL(CGamePlayerBroadcastDataHandler_Remote, CONFIGURED_FROM_FILE, atHashString("GamePlayerBroadcastDataHandler_Remote",0x55d2a6e6));

// ================================================================================================================
// CGameHostBroadcastDataHandler
// ================================================================================================================

netBroadcastDataArrayIdentifier<CGameScriptId>	CGameHostBroadcastDataHandler::sm_StaticIdentifier;

void CGameHostBroadcastDataHandler::Init()
{
	netHostBroadcastDataHandlerBase::Init();

#if __BANK
	m_cloningTimer = 0;
#endif
}

bool CGameHostBroadcastDataHandler::CanSendUpdate(const netPlayer& player) const
{
	// prevent sending updates while the script is still cloning its entities. This is to prevent entities not being created when the
	// host migrates at an awkward time. (If a host creates a few objects and sends out a host broadcast update at the same time, then leaves,
	// the next host may have received the broadcast update but not all of the entities. It will then proceed with the script and some entities 
	// will be missing)
	if (!((CGameScriptHandlerNetwork*)m_pScriptHandler)->HaveAllScriptEntitiesFinishedCloning())
	{
		netLogSplitter logSplitter(netInterface::GetArrayManager().GetLog(), *scriptInterface::GetScriptManager().GetLog());
		logSplitter.Log("\t## %s : Host broadcast handler for cannot send data - waiting for script entities to finish cloning ##\r\n", GetIdentifier()->GetLogName());

#if __BANK
		// display a message on screen when the script has been waiting for more than 5 secs for its entities to clone
		// this is to avoid bugs being reported when one participant is stuck on an assert and the script is not progressing for other participants
		m_cloningTimer += fwTimer::GetTimeStepInMilliseconds();

		if (m_cloningTimer > 10000)
		{
			if (!m_HasPrintedTTY)
			{
				m_HasPrintedTTY = true;
				gnetDebug1("%s cannot proceed - failing to clone script entities because:", m_pScriptHandler->GetScriptId().GetLogName());
				((CGameScriptHandlerNetwork*)m_pScriptHandler)->DebugPrintUnClonedEntities();
			}

#if ENABLE_NETWORK_LOGGING
			if (CTheScripts::GetScriptHandlerMgr().GetLog() && 
				CTheScripts::GetScriptHandlerMgr().GetLog()->IsEnabled())
			{
				char message[100];
				formatf(message, "%s cannot proceed - failing to clone script entities", m_pScriptHandler->GetScriptId().GetLogName());

				NetworkDebug::DisplayDebugMessage(message);
			}
#endif // ENABLE_NETWORK_LOGGING
		}
#endif // __BANK

		return false;
	}

	BANK_ONLY(m_cloningTimer = 0);
	BANK_ONLY(m_HasPrintedTTY = false);

	return netHostBroadcastDataHandlerBase::CanSendUpdate(player);
}

void CGameHostBroadcastDataHandler::SendChecksumToNewArbitrator()
{
	CScriptArrayDataVerifyEvent::Trigger(*GetIdentifier(), CalculateChecksum());
}

bool CGameHostBroadcastDataHandler::ReadUpdate(const netPlayer& player, datBitBuffer& bitBuffer, unsigned dataSize, netSequence updateSeq)
{
#if RSG_PC && ENABLE_SCRIPT_TAMPER_CHECKER
	CScriptGlobalTamperChecker::UncheckedScope uncheckedScope;
#endif
	return netHostBroadcastDataHandlerBase::ReadUpdate(player, bitBuffer, dataSize, updateSeq);
}

// ================================================================================================================
// CGamePlayerBroadcastDataHandler_Remote
// ================================================================================================================
void CGamePlayerBroadcastDataHandler_Remote::Init()
{
	netPlayerBroadcastDataHandlerBase::Init();

	m_bArbitratorChanged = false;
}

void CGamePlayerBroadcastDataHandler_Remote::Shutdown()
{
#if __DEV
	if (GetBDBackupBuffer())
	{
		RemoveBDBackupBuffer();
	}
#endif //DEV

	netPlayerBroadcastDataHandlerBase::Shutdown();
}

bool CGamePlayerBroadcastDataHandler_Remote::ReadUpdate(const netPlayer& player, datBitBuffer& bitBuffer, unsigned dataSize, netSequence updateSeq)
{
#if RSG_PC && ENABLE_SCRIPT_TAMPER_CHECKER
	CScriptGlobalTamperChecker::UncheckedScope uncheckedScope;
#endif
	return netScriptBroadcastDataHandlerBase::ReadUpdate(player, bitBuffer, dataSize, updateSeq);
}

#if __DEV
void  CGamePlayerBroadcastDataHandler_Remote::CreateBDBackupBuffer()
{
	if(m_NumArrayElements && !m_pBDBackupBuffer)
	{
		CNetwork::UseNetworkHeap(NET_MEM_SCRIPT_ARRAY_HANDLERS);
		m_pBDBackupBuffer	= rage_new u8[m_NumArrayElements*ELEMENT_SIZE_IN_BYTES];
		CNetwork::StopUsingNetworkHeap();
	}

	CacheArrayContents(CalculateChecksum());
}

void CGamePlayerBroadcastDataHandler_Remote::RemoveBDBackupBuffer()
{
	if (m_pBDBackupBuffer)
	{
		CNetwork::UseNetworkHeap(NET_MEM_SCRIPT_ARRAY_HANDLERS);
		delete[] m_pBDBackupBuffer;
		CNetwork::StopUsingNetworkHeap();
		m_pBDBackupBuffer = NULL;
	}
}

#endif //DEV




// ================================================================================================================
// CGamePlayerBroadcastDataHandler_Local
// ================================================================================================================

netBroadcastDataArrayIdentifier<CGameScriptId>	CGamePlayerBroadcastDataHandler_Local::sm_StaticIdentifier;

CGamePlayerBroadcastDataHandler_Local::CGamePlayerBroadcastDataHandler_Local(unsigned* pArray,
																			  unsigned sizeOfDataInBytes,
																			  const scriptHandler& scriptHandler,
																			  unsigned dataId,
																			  unsigned index
																			  BANK_ONLY(, const char* debugArrayName))
: netPlayerBroadcastDataHandlerBase_Local(pArray, sizeOfDataInBytes, scriptHandler)
, m_Identifier(scriptHandler.GetScriptId(), scriptHandler.GetNetworkComponent()->GetParticipantUsingSlot(index), index, dataId BANK_ONLY(, debugArrayName))
, m_initialStateBuffer(NULL)
{
	// the script id must have a timestamp to distinguish it from other scripts that have ran in the past
	gnetAssert(static_cast<CGameScriptId&>(m_Identifier.GetScriptId()).GetTimeStamp() != 0);
}

void CGamePlayerBroadcastDataHandler_Local::Init()
{
	if (!m_Initialised && AssertVerify(!m_initialStateBuffer))
	{
		// use the maximum size that the buffer can possibly be
		m_initialStateBuffer = rage_new u8[m_NumArrayElements*ELEMENT_SIZE_IN_BYTES];

		if (m_Array)
		{
			// copy initial state of the broadcast data, this will be used to reset the broadcast data for a leaving player
			sysMemCpy(m_initialStateBuffer, m_Array, m_SizeOfArrayInBytes);
		}
	}

	netPlayerBroadcastDataHandlerBase_Local::Init();
}

void CGamePlayerBroadcastDataHandler_Local::Shutdown()
{
	if (m_initialStateBuffer)
	{
		delete[] m_initialStateBuffer;
		m_initialStateBuffer = NULL;
	}

	netPlayerBroadcastDataHandlerBase_Local::Shutdown();
}

bool CGamePlayerBroadcastDataHandler_Local::ReadUpdate(const netPlayer& player, datBitBuffer& bitBuffer, unsigned dataSize, netSequence updateSeq)
{
#if RSG_PC && ENABLE_SCRIPT_TAMPER_CHECKER
	CScriptGlobalTamperChecker::UncheckedScope uncheckedScope;
#endif
	return netScriptBroadcastDataHandlerBase::ReadUpdate(player, bitBuffer, dataSize, updateSeq);
}

XPARAM(scriptProtectGlobals);

void CGamePlayerBroadcastDataHandler_Local::SetArrayData(CBroadcastDataElement* arrayData, unsigned sizeOfArrayInBytes)
{
#if RSG_PC && ENABLE_SCRIPT_TAMPER_CHECKER
	CScriptGlobalTamperChecker::UncheckedScope uncheckedScope;
#endif
	netPlayerBroadcastDataHandlerBase_Local::SetArrayData(arrayData, sizeOfArrayInBytes);
	if (m_Array && AssertVerify(m_initialStateBuffer))
	{
		// copy initial state of the broadcast data, this will be used to reset the broadcast data for a leaving player
		sysMemCpy(m_initialStateBuffer, m_Array, m_SizeOfArrayInBytes);
	}
}

void CGamePlayerBroadcastDataHandler_Local::ResetBroadcastData()
{
#if RSG_PC && ENABLE_SCRIPT_TAMPER_CHECKER
	CScriptGlobalTamperChecker::UncheckedScope uncheckedScope;
#endif
	if (AssertVerify(m_Array) && AssertVerify(m_initialStateBuffer))
	{
		sysMemCpy(m_Array, m_initialStateBuffer, m_SizeOfArrayInBytes);
	}
}

// ================================================================================================================
// CGamePlayerBroadcastDataHandler_Remote
// ================================================================================================================

netBroadcastDataArrayIdentifier<CGameScriptId>	CGamePlayerBroadcastDataHandler_Remote::sm_StaticIdentifier;

void CGamePlayerBroadcastDataHandler_Remote::SetPlayerArbitrator(const netPlayer* player)
{
#if RSG_PC && ENABLE_SCRIPT_TAMPER_CHECKER
	CScriptGlobalTamperChecker::UncheckedScope uncheckedScope;
#endif
	if (GetPlayerArbitrator() && GetPlayerArbitrator() != player)
	{
		// need to set a flag here, because the arbitrator will be cleared before being set to another player
		m_bArbitratorChanged = true;
	}

	netPlayerBroadcastDataHandlerBase::SetPlayerArbitrator(player);

	if (player && m_bArbitratorChanged)
	{
		// Reset the data in the broadcast array for this new player. This data may have been altered by a previous player using this handler,
		// and so we need to reset it ready for fresh updates from the new player, otherwise the scripts will be using the previous player's data
		// assuming it has come from the new player.
		netBroadcastDataArrayIdentifier<CGameScriptId>* pIdentifier = static_cast<netBroadcastDataArrayIdentifier<CGameScriptId>*>(GetIdentifier());

		// The initial state of the player broadcast data is held in our local player broadcast data handler to avoid storing it for every remote player
		// (this data will be identical for all players).
		CGamePlayerBroadcastDataHandler_Local* pLocalPlayerBroadcastDataHandler = static_cast<CGamePlayerBroadcastDataHandler_Local*>(NetworkInterface::GetArrayManager().GetScriptPlayerBroadcastDataArrayHandler(pIdentifier->GetScriptId(), pIdentifier->GetDataId(), *NetworkInterface::GetLocalPlayer()));

#if !__FINAL
		scrThread::CheckIfMonitoredScriptArraySizeHasChanged("SetPlayerArbitrator - start", 0);
#endif
		if (gnetVerify(pLocalPlayerBroadcastDataHandler) && pLocalPlayerBroadcastDataHandler->IsArrayDataValid())
		{
			sysMemCpy(m_Array, pLocalPlayerBroadcastDataHandler->GetInitialStateBuffer(), GetSizeOfArrayDataInBytes());
		}

#if !__FINAL
		char str[100];
		formatf(str, "SetPlayerArbitrator - end (m_Array = 0x%p)", m_Array);
		scrThread::CheckIfMonitoredScriptArraySizeHasChanged(str, 0);
#endif

#if __DEV
		CacheArrayContents(CalculateChecksum());
#endif
		m_bArbitratorChanged = false;
	}
}

// ================================================================================================================
// CPedGroupsArrayHandler - Deals with an array of CPedGroup
// ================================================================================================================
const netPlayer* CPedGroupsArrayHandler::GetTargetElementArbitration(unsigned index) const
{
	CPedGroup* pPedGroup = &m_Array[index];

	if (pPedGroup->IsActive())
	{
		if (pPedGroup->PopTypeIsMission())
		{
			// the host of the script that created a mission group should always be arbitrating over the group, otherwise arbitration migrates with
			// the leader of the group
			CGameScriptHandlerNetwork* netHandler = static_cast<CGameScriptHandlerNetwork*>(pPedGroup->GetScriptHandler());

			if (gnetVerify(netHandler) && gnetVerify(netHandler->GetNetworkComponent()))
			{
				return netHandler->GetNetworkComponent()->GetHost();
			}
		}
		else 
		{
			return netSharedNetObjArrayHandler<CPedGroup, CPedGroupsArrayHandler>::GetTargetElementArbitration(index);
		}
	}

	return NULL;
}

void CPedGroupsArrayHandler::ExtractDataForSerialising(unsigned index)
{
	m_Element.CopyNetworkInfo(m_Array[index], false);
}

void CPedGroupsArrayHandler::ApplyElementData(unsigned index, const netPlayer& UNUSED_PARAM(player))
{
	m_Array[index].CopyNetworkInfo(m_Element, true);
}

bool CPedGroupsArrayHandler::IsElementEmpty(unsigned index) const
{
	return !m_Array[index].IsActive();
}

void CPedGroupsArrayHandler::SetElementEmpty(unsigned index)
{
	CPedGroups::RemoveGroup(GetPedGroupsArrayIndex(index), false);

	return netSharedNetObjArrayHandler<CPedGroup, CPedGroupsArrayHandler>::SetElementEmpty(index);
}

void CPedGroupsArrayHandler::SetElementArbitration(unsigned index, const netPlayer& player)
{
	CPedGroup* pPedGroup = &m_Array[index];

	if (pPedGroup->PopTypeIsMission())
	{
		if (gnetVerify(pPedGroup->GetScriptHandler()))
		{
			CGameScriptHandlerNetwork* netHandler = static_cast<CGameScriptHandlerNetwork*>(pPedGroup->GetScriptHandler());

			if (gnetVerify(netHandler) && gnetVerify(netHandler->GetNetworkComponent()))
			{
				const netPlayer* prevArbitrator = GetElementArbitration(index);

				if (prevArbitrator && prevArbitrator->IsLocal())
				{
					// remove group from script resource list as we are no longer the host of the script which created the ped group
					pPedGroup->GetScriptHandler()->RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_PEDGROUP, static_cast<ScriptResourceRef>(index));
				}
				else if (player.IsLocal())
				{
					// add group to script resource list, so that it is cleaned up when the script terminates
					CScriptResource_PedGroup resource(index);
					pPedGroup->GetScriptHandler()->RegisterScriptResource(resource);
				}
			}
		}
	}
	else
	{
		netObject* netObj = m_Array[index].GetNetworkObject();

		if (netObj && netObj->GetPlayerOwner() && netObj->GetPlayerOwner()->IsLocal())
		{
#if __ASSERT
			const netPlayer* pTargetArbitrator = GetTargetElementArbitration(index);
			gnetAssertf(netObj->GetPlayerOwner() == &player || netObj->GetPendingPlayerIndex() == player.GetPhysicalPlayerIndex(), "Ambient ped group in index %d is migrating to a different owner (%s) than that of the leader (%s, player owner : %s). Target arbitrator: %s", index, player.GetLogName(), netObj->GetLogName(), netObj->GetPlayerOwner() ? netObj->GetPlayerOwner()->GetLogName() : "Unknown player", pTargetArbitrator ? pTargetArbitrator->GetLogName() : "??");
#endif
		}
	}

	netSharedNetObjArrayHandler<CPedGroup, CPedGroupsArrayHandler>::SetElementArbitration(index, player);
}

bool CPedGroupsArrayHandler::ElementCanBeCleared(unsigned index) const
{
	CPedGroup* pPedGroup = &m_Array[index];

	bool bCanBeCleared = netSharedNetObjArrayHandler<CPedGroup, CPedGroupsArrayHandler>::ElementCanBeCleared(index);

	ObjectId leaderId = pPedGroup->GetGroupMembership()->GetLeaderNetId();

	if (leaderId == NETWORK_INVALID_OBJECT_ID)
	{
		bCanBeCleared = true;
	}
	else if (leaderId != pPedGroup->GetGroupMembership()->GetInitialLeaderNetId())
	{
		if (pPedGroup->GetGroupMembership()->GetInitialLeaderNetId() == NETWORK_INVALID_OBJECT_ID)
		{
			gnetAssertf(0, "Initial leader id not set on ped group %d", index);
		}
		else
		{
			gnetAssertf(0, "Leader has changed on ped group %d (initial leader: %d, current leader: %d)", index, pPedGroup->GetGroupMembership()->GetInitialLeaderNetId(), leaderId);		
		}

		bCanBeCleared = true;
	}

	return bCanBeCleared;
}

unsigned CPedGroupsArrayHandler::GetPedGroupsArrayIndex(unsigned index)
{
	// player groups are not handled by the array handler
	return index + MAX_NUM_PHYSICAL_PLAYERS;
}

// ================================================================================================================
// CIncidentsArrayHandler - Deals with an array of CIncident
// ================================================================================================================

bool CIncidentsArrayHandler::IsElementInScopeAndSyncedWithPlayer(unsigned index, const netPlayer& player) const
{
	bool bInScopeAndSynced = false;

	if (IsElementInScopeInternal(index, player))
	{
		bInScopeAndSynced = IsElementSyncedWithPlayer(index, GetSyncDataIndexForPlayer(player));
	}

	return bInScopeAndSynced;
}

void CIncidentsArrayHandler::ExtractDataForSerialising(unsigned index)
{
	if (gnetVerify(m_Array[index].Get()))
	{
		m_Element.Copy(*m_Array[index].Get());
	}
}

void CIncidentsArrayHandler::ApplyElementData(unsigned index, const netPlayer& UNUSED_PARAM(player))
{
	if (gnetVerify(m_Element.Get()))
	{
		m_Array[index].Copy(*m_Element.Get());

		if (gnetVerify(m_Array[index].Get()))
		{
			m_Array[index].Get()->SetIncidentIndex(index);
			m_Array[index].Get()->SetIncidentID(m_ElementID);
			m_Array[index].Get()->SetEntityID(CIncident::GetNetworkObjectIDFromIncidentID(m_ElementID));
			m_Array[index].Get()->NetworkInit();
		}

		// free up the incident allocated for the serialisation
		m_Element.Clear();
	}
}

bool CIncidentsArrayHandler::IsElementEmpty(unsigned index) const
{
	return (m_Array[index].Get() == 0);
}

void CIncidentsArrayHandler::SetElementEmpty(unsigned index)
{
	if (m_Array[index].Get())
	{
		NetworkInterface::GetArrayManager().GetOrdersArrayHandler()->ClearAllOrdersForIncident(m_Array[index].Get()->GetIncidentID());
	}
	
	m_Array[index].Clear();

	netSharedNetObjArrayHandler<CIncidentManager::CIncidentPtr, CIncidentsArrayHandler>::SetElementEmpty(index);
}

bool CIncidentsArrayHandler::IsElementInScope(unsigned index, const netPlayer& player) const
{
	CIncident* pIncident = m_Array[index].Get();

	bool bInScope = false;

	if (pIncident)
	{
		if (pIncident->GetType() == CIncident::IT_Wanted)
		{
			Vector3 dist = pIncident->GetLocation() - NetworkUtils::GetNetworkPlayerPosition(static_cast<const CNetGamePlayer&>(player));

			return dist.XYMag2() < WANTED_INCIDENT_SCOPE_SQR;
		}
		else
		{
			bInScope = netSharedNetObjArrayHandler<CIncidentManager::CIncidentPtr, CIncidentsArrayHandler>::IsElementInScope(index, player);
		}
	}

	return bInScope;
}

void CIncidentsArrayHandler::ElementScopeChanged(unsigned index)
{
	CIncident* pIncident = m_Array[index].Get();

	if (gnetVerify(pIncident))
	{
		NetworkInterface::GetArrayManager().GetOrdersArrayHandler()->IncidentScopeHasChanged(*pIncident);
	}
}

void CIncidentsArrayHandler::SetElementArbitration(unsigned index, const netPlayer& player)
{
	const netPlayer* pPrevArbitrator = GetElementArbitration(index);

	netSharedNetObjArrayHandler<CIncidentManager::CIncidentPtr, CIncidentsArrayHandler>::SetElementArbitration(index, player);

	if (pPrevArbitrator && pPrevArbitrator != &player)
	{
		CIncident* pIncident = m_Array[index].Get();

		if (pIncident)
		{
			pIncident->Migrate();
		}
	}
}

void CIncidentsArrayHandler::WriteElement(datBitBuffer& bitBuffer, unsigned index, netLoggingInterface* pLog)
{
	netSharedNetObjArrayHandler<CIncidentManager::CIncidentPtr, CIncidentsArrayHandler>::WriteElement(bitBuffer, index, pLog);

	// free up the incident allocated for the serialisation
	m_Element.Clear();
}

bool CIncidentsArrayHandler::RemoveElementWhenArbitratorLeaves(unsigned index) const
{
	CIncident* pIncident = m_Array[index].Get();

	if (pIncident && pIncident->GetType() == CIncident::IT_Wanted)
	{
		return true;
	}
	else
	{
		return netSharedNetObjArrayHandler<CIncidentManager::CIncidentPtr, CIncidentsArrayHandler>::RemoveElementWhenArbitratorLeaves(index);
	}
}

#if __BANK
void CIncidentsArrayHandler::DisplayDebugInfo()
{
	u32 reservedPeds, reservedVehicles, reservedObjects;
	u32 createdPeds, createdVehicles, createdObjects;

	CDispatchManager::GetInstance().GetNetworkReservations(reservedPeds, reservedVehicles, reservedObjects, createdPeds, createdVehicles, createdObjects);

	grcDebugDraw::AddDebugOutput("Reservations: %d/%d peds, %d/%d vehicles, %d/%d objects", createdPeds, reservedPeds, createdVehicles, reservedVehicles, createdObjects, reservedObjects);

	for (u32 i=0; i<m_NumElementsInUse; i++)
	{
		CIncident* pIncident = m_Array[i].Get();

		const char *incidentStr = "UNKNOWN";
		const char *playerString = "UNKNOWN";
		u32 numOrders = 0;
		u32 numAssignedOrders = 0;
		u32 numEmptyOrders = 0;

		if (pIncident)
		{
			switch(pIncident->GetType())
			{
			case CIncident::IT_Invalid:
				incidentStr = "INVALID";
				break;
			case CIncident::IT_Wanted:
				incidentStr = "WANTED";
				break;
			case CIncident::IT_Fire:
				incidentStr = "FIRE";
				break;
			case CIncident::IT_Injury:
				incidentStr = "INJURY";
				break;
			case CIncident::IT_Scripted:
				incidentStr = "SCRIPTED";
				break;
			case CIncident::IT_Arrest:
				incidentStr = "ARREST";
				break;
			default:
				break;
			}

			numOrders = NetworkInterface::GetArrayManager().GetOrdersArrayHandler()->GetNumOrdersForIncident(*pIncident, numAssignedOrders, numEmptyOrders);
		}
		else
		{
			incidentStr = "EMPTY";
		}

		if (m_ElementArbitrators[i] == INVALID_PLAYER_INDEX)
		{
			playerString = "** NO ARBITRATOR! **";
		}
		else if (m_ElementArbitrators[i] == NetworkInterface::GetLocalPhysicalPlayerIndex())
		{
			playerString = "us";
		}
		else
		{
			CNetGamePlayer* pPlayer = NetworkInterface::GetPhysicalPlayerFromIndex(m_ElementArbitrators[i]);

			if (pPlayer)
			{
				playerString = pPlayer->GetLogName();
			}
		}

		if (pIncident || m_ElementArbitrators[i] != INVALID_PLAYER_INDEX)
		{
			if (pIncident && pIncident->GetType() == CIncident::IT_Fire)
			{
				grcDebugDraw::AddDebugOutput("%d : %s : ID %u : %s. Num fires: %d. Num orders: %d (num assigned: %d). Num empty slots: %d. Pool size :%d)", i, incidentStr, pIncident ? pIncident->GetIncidentID() : 0, playerString, ((CFireIncident*)pIncident)->GetNumFires(), numOrders, numAssignedOrders, numEmptyOrders, COrder::GetPool()->GetNoOfUsedSpaces());
			}
			else
			{
				grcDebugDraw::AddDebugOutput("%d : %s : ID %u : %s. Num orders: %d (num assigned: %d). Num empty slots: %d. Pool size :%d)", i, incidentStr, pIncident ? pIncident->GetIncidentID() : 0, playerString, numOrders, numAssignedOrders, numEmptyOrders, COrder::GetPool()->GetNoOfUsedSpaces());
			}
		}
	}
}
#endif // __DEBUG

// ================================================================================================================
// COrdersArrayHandler - Deals with an array of COrder
// ================================================================================================================
void COrdersArrayHandler::Init()
{
	netSharedNetObjArrayHandler<COrderManager::COrderPtr, COrdersArrayHandler>::Init();

	for (u32 i=0; i<MAX_ELEMENTS; i++)
	{
		m_incidentCreated[i] = 0;
	}
}

void COrdersArrayHandler::PlayerHasLeft(const netPlayer& player)
{
	netSharedNetObjArrayHandler<COrderManager::COrderPtr, COrdersArrayHandler>::PlayerHasLeft(player);

	for (unsigned i=0; i<MAX_ELEMENTS; i++)
	{
		m_incidentCreated[i] &= ~(1<<player.GetPhysicalPlayerIndex());
	}
}

void COrdersArrayHandler::ExtractDataForSerialising(unsigned index)
{
	if (gnetVerify(m_Array[index].Get()))
	{
		m_Element.Copy(*m_Array[index].Get());
	}
}

void COrdersArrayHandler::ApplyElementData(unsigned index, const netPlayer& UNUSED_PARAM(player))
{
	if (gnetVerify(m_Element.Get()))
	{
		m_Array[index].Copy(*m_Element.Get());

		m_Array[index].Get()->SetOrderIndex(index);
		m_Array[index].Get()->SetOrderID(m_ElementID);
		m_Array[index].Get()->SetEntityID((ObjectId)m_ElementID);

#if __ASSERT
		netObject* pNetObj = NetworkInterface::GetObjectManager().GetNetworkObject((ObjectId)m_ElementID);

		gnetAssertf(!pNetObj || !pNetObj->GetEntity() || pNetObj->GetEntity()->GetIsTypePed(), "Received an order update for %s!", pNetObj->GetLogName());
#endif

		m_Array[index].Get()->AssignOrderToEntity();

		// free up the order allocated for the serialisation
		m_Element.Clear();
	}
}

void COrdersArrayHandler::ClearElementArbitration(unsigned index)
{
	netSharedArrayHandlerWithElementScope<COrderManager::COrderPtr, COrdersArrayHandler>::ClearElementArbitration(index);

	m_incidentCreated[index] = 0;
}

bool COrdersArrayHandler::IsElementEmpty(unsigned index) const
{
	return (m_Array[index].Get() == 0);
}

void COrdersArrayHandler::SetElementEmpty(unsigned index)
{
	m_Array[index].Clear();

	netSharedNetObjArrayHandler<COrderManager::COrderPtr, COrdersArrayHandler>::SetElementEmpty(index);
}

bool COrdersArrayHandler::CanApplyElementData(unsigned index, const netPlayer& player, bool elementEmpty)
{
	bool bCanApplyData = true;
	bool bReassigned = false;

	netLoggingInterface &log = netInterface::GetArrayManager().GetLog();
	netLogSplitter logSplitter(netInterface::GetMessageLog(), log);

	if (index == INVALID_SLOT)
	{
		logSplitter.WriteDataValue("FAIL", "Ran out of free slots");
		bCanApplyData = false;
	}
	else if (COrder::GetPool()->GetNoOfFreeSpaces() == 0)
	{
		gnetAssertf(0, "COrdersArrayHandler::CanApplyElementData : Ran out of COrders");
		logSplitter.WriteDataValue("FAIL", "The order pool is full");
		bCanApplyData = false;
	}
	else if (!elementEmpty)
	{
		if (!m_Element.Get())
		{
			logSplitter.WriteDataValue("FAIL", "Failed to create order");
			bCanApplyData = false;
		}
		else if (!CIncidentManager::GetInstance().GetIncidentFromIncidentId(m_Element.Get()->GetIncidentID()))
		{
			// we have to reject the order if we have no incident for it		
			logSplitter.WriteDataValue("FAIL", "Incident does not exist");
			bCanApplyData = false;
		}
		else
		{
			COrder* pNewOrder = m_Element.Get();
			COrder* pExistingOrder = m_Array[index].Get();

			if (pExistingOrder && AssertVerify(pExistingOrder) && pExistingOrder->GetIncidentID() != pNewOrder->GetIncidentID())
			{
				// catch when 2 orders have been assigned to the same ped for different incidents. In this case the incident with the lowest id wins
				if (pExistingOrder->GetIncidentID() < pNewOrder->GetIncidentID())
				{
					logSplitter.WriteDataValue("FAIL", "Another order already exists for this ped with a lower incident id (%d)", pExistingOrder->GetIncidentID());
					bCanApplyData = false;
				}
				else
				{
					logSplitter.WriteDataValue("REASSIGN", "Replace previous order assigned by incident %d, which has a higher incident id", pExistingOrder->GetIncidentID());
					SetElementArbitration(index, player);
					bReassigned = true;
				}
			}
		}
	}

	if (bCanApplyData && !bReassigned)
	{
		// deliberately skip netSharedNetObjArrayHandler::CanApplyElementData as we still want to create orders with no associated network object
		bCanApplyData = netSharedArrayHandlerWithElementScope<COrderManager::COrderPtr, COrdersArrayHandler>::CanApplyElementData(index, player, elementEmpty);
	}

	// make sure we destroy the order created when we read the element, otherwise we will leak orders
	if (!bCanApplyData)
	{
		m_Element.Clear();
	}

	return bCanApplyData;
}

bool COrdersArrayHandler::IsElementInScope(unsigned index, const netPlayer& player) const
{
	COrder* pOrder = m_Array[index].Get();

	unsigned playerIndex = GetSyncDataIndexForPlayer(player);
	PlayerFlags playerFlag = (1<<playerIndex);

	// orders have the same scope as their associated incident
	if (pOrder && pOrder->GetIncident())
	{
		CIncidentsArrayHandler* incidentsArray = CIncidentManager::GetInstance().GetIncidentsArrayHandler();

		if (AssertVerify(incidentsArray))
		{
			s32 incidentIndex = pOrder->GetIncident()->GetIncidentIndex();

			// make sure the incident has been created on this players machine, or the order may be removed by ElementCanBeCleared()
			if ((m_incidentCreated[index] & playerFlag) != 0)
			{
				if ((incidentsArray->GetElementScope(incidentIndex) & playerFlag) == 0)
				{
					m_incidentCreated[index] = 0;
				}
			}
			else if (incidentsArray->IsElementInScopeAndSyncedWithPlayer(incidentIndex, player))
			{
				m_incidentCreated[index] |= playerFlag;
			}
		}
	}

	return ((m_incidentCreated[index] & playerFlag) != 0);
}

void COrdersArrayHandler::WriteElement(datBitBuffer& bitBuffer, unsigned index, netLoggingInterface* pLog)
{
	netSharedNetObjArrayHandler<COrderManager::COrderPtr, COrdersArrayHandler>::WriteElement(bitBuffer, index, pLog);

	// free up the order allocated for the serialisation
	m_Element.Clear();
}

bool COrdersArrayHandler::ElementCanBeCleared(unsigned index) const
{
	if (IsElementRemotelyArbitrated(index))
	{
		COrder* pOrder = m_Array[index].Get();

		if (pOrder && !pOrder->GetIncident())
		{
			netInterface::GetArrayManager().GetLog().Log("\t\t## %s : Clearing remotely arbitrated element %d. The order has no incident. ##\r\n", this->GetHandlerName(), index );
			return true;
		}
	}

	return false;
}

void COrdersArrayHandler::ClearAllOrdersForIncident(unsigned incidentID)
{
	netLoggingInterface &log = netInterface::GetArrayManager().GetLog();
	NetworkLogUtils::WriteLogEvent(log, "CLEAR_ALL_ORDERS_FOR_INCIDENT", "%d", incidentID);

	for (u32 i=0; i<m_NumArrayElements; i++)
	{
		COrder* pOrder = m_Array[i].Get();

		if (pOrder && pOrder->GetIncidentID() == incidentID)
		{
			SetElementEmpty(i);

			if (IsElementRemotelyArbitrated(i))
			{
				ClearElementArbitration(i);
			}
		}
	}
}

void COrdersArrayHandler::IncidentHasMigrated(CIncident& incident)
{
	// incidents migrate with the entity they are based on
	if (AssertVerify(incident.GetNetworkObject()))
	{
		for (u32 i=0; i<m_NumArrayElements; i++)
		{
			COrder* pOrder = m_Array[i].Get();

			if (pOrder && pOrder->GetIncident() == &incident)
			{
                if(incident.GetNetworkObject()->GetPlayerOwner())
                {
				    SetElementArbitration(i, *incident.GetNetworkObject()->GetPlayerOwner());
                }
			}
		}
	}
}

void COrdersArrayHandler::IncidentScopeHasChanged(CIncident& incident)
{
	// incidents migrate with the entity they are based on
	if (AssertVerify(incident.GetNetworkObject()))
	{
		for (u32 i=0; i<m_NumArrayElements; i++)
		{
			COrder* pOrder = m_Array[i].Get();

			if (pOrder && pOrder->GetIncident() == &incident)
			{
				RecalculateElementScope(i, i+1);
			}
		}
	}
}

#if __BANK
u32 COrdersArrayHandler::GetNumOrdersForIncident(CIncident& incident, u32& numAssigned, u32& numEmpty)
{
	u32 count = 0;

	numAssigned = numEmpty = 0;

	for (u32 i=0; i<m_NumArrayElements; i++)
	{
		COrder* pOrder = m_Array[i].Get();

		if (pOrder)
		{
			if (gnetVerifyf(pOrder->GetIncident(), "Order in slot %d has no incident", i) && pOrder->GetIncident() == &incident)
			{
				if (pOrder->GetAssignedToEntity())
				{
					netObject* pNetObj = NetworkInterface::GetNetworkObject((ObjectId)pOrder->GetOrderID());

					if (gnetVerifyf(pNetObj, "Order in slot %d is assigned to a non-existent PED_%d", i, pOrder->GetOrderID()) &&
						gnetVerifyf(pNetObj->GetObjectType() == NET_OBJ_TYPE_PED, "Order in slot %d is assigned to a non ped %d", i, pOrder->GetOrderID()))
					{
						CPed* pPed = SafeCast(CPed, pNetObj->GetEntity());

						if (gnetVerifyf(pPed, "Order in slot %d is assigned to %s, that has no ped entity", i, pNetObj->GetLogName()))
						{
							if (!pPed->GetPedIntelligence()->GetOrder())
							{
								gnetAssertf(0, "Order in slot %d is assigned to %s, which has no order", i, pNetObj->GetLogName());
							}
							else if (pPed->GetPedIntelligence()->GetOrder() != pOrder)
							{
								gnetAssertf(0, "Order in slot %d is assigned to %s, which has a different order in slot %d (id : %d)", i, pNetObj->GetLogName(), pPed->GetPedIntelligence()->GetOrder()->GetOrderIndex(), pPed->GetPedIntelligence()->GetOrder()->GetOrderID());
							}
						}
					}

					numAssigned++;
				}

				count++;
			}
		}
		else if (IsElementLocallyArbitrated(i))
		{
			numEmpty++;
		}
		else
		{
			 gnetAssertf(GetElementArbitration(i)==0, "Empty order slot %d still has remote arbitrator %s", i , GetElementArbitration(i)->GetLogName());
		}
	}

	return count;
}
#endif

 //================================================================================================================
 //CStickyBombsArrayHandler - Deals with an array of CProjectiles
 //================================================================================================================

void CStickyBombsArrayHandler::ExtractDataForSerialising(unsigned index )
{
	m_Element.Copy(m_Array[index]);
}

void CStickyBombsArrayHandler::ApplyElementData(unsigned index, const netPlayer& player)
{
	m_Array[index].CopyNetworkInfoCreateProjectile(player, index);
	m_Element.Clear(false);
}

bool CStickyBombsArrayHandler::IsElementEmpty(unsigned index) const
{
	return (m_Array[index].Get() == 0);
}

void CStickyBombsArrayHandler::SetElementEmpty(unsigned index)
{
	m_Array[index].Clear();
	CNetworkPendingProjectiles::RemoveStickyBomb(index);
	netSharedArrayHandler<CProjectileManager::CProjectilePtr,  CStickyBombsArrayHandler>::SetElementEmpty(index);
}

bool CStickyBombsArrayHandler::MoveElementToFreeSlot(unsigned index, unsigned& newSlot)
{
	return CProjectileManager::MoveNetSyncProjectile(index, (s32&)newSlot);
}

bool CStickyBombsArrayHandler::CanApplyElementData(unsigned index, const netPlayer& player, bool elementEmpty)
{
	bool bCanApplyData = true;

	netLoggingInterface &log = netInterface::GetArrayManager().GetLog();
	netLogSplitter logSplitter(netInterface::GetMessageLog(), log);

	if (!elementEmpty)
	{
		if (!m_Element.IsProjectileModelLoaded())
		{
			logSplitter.WriteDataValue("FAIL", "Sticky bomb model not loaded");
			bCanApplyData = false;
		}
	}

	if (bCanApplyData)
	{
		bCanApplyData = netSharedArrayHandler<CProjectileManager::CProjectilePtr, CStickyBombsArrayHandler>::CanApplyElementData(index, player, elementEmpty);
	}

	if (!bCanApplyData)
	{
		m_Element.Clear();
	}

	return bCanApplyData;
}

void CStickyBombsArrayHandler::ResendAllStickyBombsToPlayer(const netPlayer& player)
{
	if (GetSyncData())
	{
		for (u32 i=0; i<m_NumArrayElements; i++)
		{
			if (IsElementLocallyArbitrated(i))
			{
				GetSyncData()->SetSyncDataUnitDirtyForPlayer(i, GetSyncDataIndexForPlayer(player));
			}
		}
	}
}

//================================================================================================================
// CAnimRequestArrayHandler - Deals with an array of CNetworkStreamingRequestManager::NetworkAnimRequestData's
//================================================================================================================
void CAnimRequestArrayHandler::ExtractDataForSerialising(unsigned index)
{
	m_Element.Copy(m_Array[index]);
}

void CAnimRequestArrayHandler::ApplyElementData(unsigned index, const netPlayer& UNUSED_PARAM(player))
{
	m_Array[index].Copy(m_Element);
	m_Array[index].SetPedID((ObjectId)m_ElementID);

	m_Element.Clear();
}

bool CAnimRequestArrayHandler::IsElementEmpty(unsigned index) const
{
	return m_Array[index].m_numAnims == 0;
}

void CAnimRequestArrayHandler::SetElementEmpty(unsigned index)
{
	m_Array[index].Clear();
	netSharedNetObjArrayHandler<CNetworkStreamingRequestManager::NetworkAnimRequestData, CAnimRequestArrayHandler>::SetElementEmpty(index);
}

bool CAnimRequestArrayHandler::ElementCanBeCleared(unsigned index) const
{
	CPed* pPed = m_Array[index].GetPed();
	if(!pPed || pPed->IsDead())
	{
		return true;
	}

	return false;
}