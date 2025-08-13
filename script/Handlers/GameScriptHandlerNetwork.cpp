//
// name:		NetGameScriptHandler.cpp
// description:	Project specific network script handler
// written by:	John Gurney
//


#include "script/handlers/GameScriptHandlerNetwork.h"

// game includes
#include "event/EventGroup.h"
#include "event/EventNetwork.h"
#include "Network/Arrays/NetworkArrayMgr.h"
#include "Network/Events/NetworkEventTypes.h"
#include "Network/Network.h"
#include "Network/NetworkInterface.h"
#include "Network/Objects/Entities/NetObjGame.h"
#include "Network/Players/NetGamePlayer.h"
#include "Network/Sessions/NetworkSession.h"
#include "Peds/Ped.h"
#include "pickups/PickupManager.h"
#include "pickups/PickupPlacement.h"
#include "renderer/DrawLists/drawList.h"
#include "Scene/Physical.h"
#include "scene/world/GameWorld.h"
#include "Script/handlers/GameScriptEntity.h"
#include "Script/handlers/GameScriptResources.h"
#include "Script/gta_thread.h"
#include "script/script.h"
#include "script/script_channel.h"
#include "network/objects/NetworkObjectPopulationMgr.h"

// framework includes
#include "fwnet/netObjectMgrBase.h"
#include "fwnet/netTypes.h"
#include "fwscript/scripthandlermgr.h"
#include "fwscript/scriptinterface.h"

SCRIPT_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()

// all handlers are network ones at the moment
FW_INSTANTIATE_CLASS_POOL(CGameScriptHandlerNetwork, scriptHandlerNetComponent::MAX_NUM_NET_COMPONENTS, atHashString("CGameScriptHandlerNetwork",0x4fe810e8));
FW_INSTANTIATE_CLASS_POOL(CGameScriptHandlerNetComponent, scriptHandlerNetComponent::MAX_NUM_NET_COMPONENTS, atHashString("CGameScriptHandlerNetComponent",0xed9e8fd3));

namespace rage
{
}

CGameScriptHandlerNetwork::CGameScriptHandlerNetwork(scrThread& scriptThread)
: CGameScriptHandler(scriptThread)
, m_nextFreeSequenceId(0)
, m_MaxNumParticipants(0)
, m_NumLocalReservedPeds(0)
, m_NumLocalReservedVehicles(0)
, m_NumLocalReservedObjects(0)
, m_NumGlobalReservedPeds(0)
, m_NumGlobalReservedVehicles(0)
, m_NumGlobalReservedObjects(0)
, m_NumCreatedPeds(0)
, m_NumCreatedVehicles(0)
, m_NumCreatedObjects(0)
, m_AcceptingPlayers(true)
, m_ActiveInSinglePlayer(false)
, m_InMPCutscene(false)
#if __DEV
, m_canRegisterMissionObjectCalled(false)
, m_canRegisterMissionPedCalled(false) 
, m_canRegisterMissionVehicleCalled(false)
#endif
{
}

void CGameScriptHandlerNetwork::CreateNetworkComponent()
{
	if (scriptVerify(!m_NetComponent))
	{
		m_NetComponent = rage_new CGameScriptHandlerNetComponent(*this, m_MaxNumParticipants);
	}
}

void CGameScriptHandlerNetwork::DestroyNetworkComponent()
{
	CGameScriptHandler::DestroyNetworkComponent();

	if (m_Thread)
	{
		static_cast<GtaThread*>(m_Thread)->m_NetComponent = NULL;
	}
}

bool CGameScriptHandlerNetwork::Update()
{
#if __DEV
	// The can register debug flags need to be reset each frame for all active scripts
	m_canRegisterMissionObjectCalled  = 0;
	m_canRegisterMissionPedCalled     = 0;
	m_canRegisterMissionVehicleCalled = 0;
#endif

	return CGameScriptHandler::Update();
}

void CGameScriptHandlerNetwork::Shutdown()
{
	scriptInterface::GetScriptManager().WriteScriptHeader(scriptInterface::GetScriptManager().GetLog(), GetScriptId(), "Destroying handler 0x%x", this);

	Displayf("Destroying handler for script %s (0x%p)\n", GetScriptId().GetLogName(), this);

	return CGameScriptHandler::Shutdown();
}

bool CGameScriptHandlerNetwork::Terminate()
{
	bool bRet = CGameScriptHandler::Terminate();

	if (NetworkInterface::IsGameInProgress())
	{
		CGameScriptHandlerMgr::CRemoteScriptInfo* pInfo = CTheScripts::GetScriptHandlerMgr().GetRemoteScriptInfo(m_ScriptId, true);

		if (pInfo)
		{
			pInfo->SetRunningLocally(false);
		}
	}

#if ENABLE_NETWORK_LOGGING
	if (GetTotalNumReservedPeds() > 0 || GetTotalNumReservedVehicles() > 0 || GetTotalNumReservedObjects() > 0)
	{
		netLoggingInterface *log = CTheScripts::GetScriptHandlerMgr().GetLog();

		if (log)
		{
			CTheScripts::GetScriptHandlerMgr().LogCurrentReservations(*log);
		}
	}
#endif // ENABLE_NETWORK_LOGGING

	return bRet;
}

bool CGameScriptHandlerNetwork::RegisterNewScriptObject(scriptHandlerObject &object, bool hostObject, bool network)
{
	bool success = CGameScriptHandler::RegisterNewScriptObject(object, hostObject, network);

#if __ASSERT
	if (network && GetNetworkComponent() && scriptVerify(object.GetScriptInfo()))
	{
		scriptVerifyf(!hostObject || GetNetworkComponent()->IsHostLocal() || object.HostObjectCanBeCreatedByClient(), "Script %s has created a host object when we are not the host of the script. The object will be removed on other machines", GetScriptName());

		// the script id must have a timestamp to distinguish it from other scripts that have ran in the past
		gnetAssert(static_cast<const CGameScriptId&>(GetScriptId()).GetTimeStamp() != 0);	

		scriptAssertf(GetNetworkComponent()->GetState() == scriptHandlerNetComponent::NETSCRIPT_PLAYING, "Script %s has not waited for NETWORK_GET_SCRIPT_STATUS to return a playing state before proceeding", GetScriptName());
	}
#endif // __ASSERT

	return success;
}

void CGameScriptHandlerNetwork::UnregisterScriptObject(scriptHandlerObject &object)
{
	netObject* netObject = object.GetNetObject();

	bool bHostObject = object.IsHostObject();

	bool bIsPed = false;
	bool bIsVeh = false;
	bool bIsObj = false;

	if (bHostObject && netObject && netObject->GetEntity())
	{
		if (netObject->GetEntity()->GetIsTypePed())
		{
			bIsPed = true;
		}
		else if (netObject->GetEntity()->GetIsTypeVehicle())
		{
			bIsVeh = true;
		}
		else if (netObject->GetEntity()->GetIsTypeObject())
		{
			// ignore doors 
			if (!static_cast<CObject*>(netObject->GetEntity())->IsADoor())
			{
				bIsObj = true;
			}
		}
		else
		{
			gnetAssertf(0, "Unexpected target object type encountered!");
		}
	}

	CGameScriptHandler::UnregisterScriptObject(object);

	netLoggingInterface *log = CTheScripts::GetScriptHandlerMgr().GetLog();

	if (bIsPed)
	{
		if (m_NumCreatedPeds > 0)
		{
			m_NumCreatedPeds--;
		}

		if (log)
		{
			log->WriteDataValue("Ped Reservation",	"%d/%d (%d)", m_NumCreatedPeds, GetTotalNumReservedPeds(), m_NumLocalReservedPeds);
		}
	}
	else if (bIsVeh)
	{
		if (m_NumCreatedVehicles > 0)
		{
			m_NumCreatedVehicles--;
		}

		if (log)
		{
			log->WriteDataValue("Vehicle Reservation",	"%d/%d (%d)", m_NumCreatedVehicles, GetTotalNumReservedVehicles(), m_NumLocalReservedVehicles);
		}
	}
	else if (bIsObj)
	{
		if (m_NumCreatedObjects > 0)
		{
			m_NumCreatedObjects--;
		}

		if (log)
		{
			log->WriteDataValue("Object Reservation",	"%d/%d (%d)", m_NumCreatedObjects, GetTotalNumReservedObjects(), m_NumLocalReservedObjects);
		}
	}
}


ScriptResourceId CGameScriptHandlerNetwork::GetNextFreeResourceId(scriptResource& resource)
{
	ScriptResourceId freeId = 0;

	// sequences have their own id system, so that the ids match across the network when the order of sequences is defined in the same 
	// order by the script, irrespective of other resource allocations.
	if (NetworkInterface::IsGameInProgress() && resource.GetType() == CGameScriptResource::SCRIPT_RESOURCE_SEQUENCE_TASK)
	{
		// check that the normal resource ids are not overlapping the id range for sequences
		Assertf(!CScriptResource_SequenceTask::IsValidResourceId(m_NextFreeResourceId), "Script %s has created too many resources", GetLogName());

		// allow the ids to wrap if there are no other sequence resources
		if (m_nextFreeSequenceId >= CScriptResource_SequenceTask::MAX_NUM_SEQUENCES_PERMITTED)
		{
			if (GetNumScriptResourcesOfType(CGameScriptResource::SCRIPT_RESOURCE_SEQUENCE_TASK) == 0)
			{
				m_nextFreeSequenceId = 0;
			}
			else
			{
				Assertf(0, "Script %s has created too many task sequences (max allowed: %d)", GetLogName(), CScriptResource_SequenceTask::MAX_NUM_SEQUENCES_PERMITTED);
			}
		}

		freeId = CScriptResource_SequenceTask::GetResourceIdFromSequenceId(++m_nextFreeSequenceId);

		Assert(freeId != 0);
	}
	else
	{
		freeId = scriptHandler::GetNextFreeResourceId(resource);
	}

	return freeId;
}


void CGameScriptHandlerNetwork::SetMaxNumParticipants(int n)
{ 
	m_MaxNumParticipants = static_cast<u8>(n); 

	if (GetNetworkComponent())
	{
		GetNetworkComponent()->SetMaxNumParticipants(n);
	}
}

int CGameScriptHandlerNetwork::GetMaxNumParticipants() const	
{ 
	if (GetNetworkComponent())
	{
		return GetNetworkComponent()->GetMaxNumParticipants();
	}

	return static_cast<int>(m_MaxNumParticipants); 
}

void CGameScriptHandlerNetwork::ReserveLocalPeds(int n)				
{ 
	if (n != m_NumLocalReservedPeds)
	{
		netLoggingInterface *log = CTheScripts::GetScriptHandlerMgr().GetLog();

		if(log)
		{
			NetworkLogUtils::WriteLogEvent(*log, "SCRIPT_RESERVING_LOCAL_PEDS", "%s      %d", GetScriptId().GetLogName(), n);
		}

		if (gnetVerifyf(n+GetNumGlobalReservedPeds() >= m_NumCreatedPeds || (GetNetworkComponent() && !GetNetworkComponent()->IsHostLocal()), "Script: %s is reserving less peds (%d) than the number that are currently active (%d)", GetScriptName(), n+GetNumGlobalReservedPeds(), m_NumCreatedPeds))
		{
			m_NumLocalReservedPeds = static_cast<u8>(n); 

			if (log)
			{
				CTheScripts::GetScriptHandlerMgr().LogCurrentReservations(*log);
			}
		}
	}
}

void CGameScriptHandlerNetwork::ReserveLocalVehicles(int n)				
{ 
	if (n != m_NumLocalReservedVehicles)
	{
		netLoggingInterface *log = CTheScripts::GetScriptHandlerMgr().GetLog();

		if(log)
		{
			NetworkLogUtils::WriteLogEvent(*log, "SCRIPT_RESERVING_LOCAL_VEHICLES", "%s      %d", GetScriptId().GetLogName(), n);
		}

		if (gnetVerifyf(n+GetNumGlobalReservedVehicles() >= m_NumCreatedVehicles || (GetNetworkComponent() && !GetNetworkComponent()->IsHostLocal()), "Script: %s is reserving less vehicles (%d) than the number that are currently active (%d)", GetScriptName(), n+GetNumGlobalReservedVehicles(), m_NumCreatedVehicles))
		{
			m_NumLocalReservedVehicles = static_cast<u8>(n); 

			if (log)
			{
				CTheScripts::GetScriptHandlerMgr().LogCurrentReservations(*log);
			}
		}
	}
}

void CGameScriptHandlerNetwork::ReserveLocalObjects(int n)				
{ 
	if (n != m_NumLocalReservedObjects)
	{
		netLoggingInterface *log = CTheScripts::GetScriptHandlerMgr().GetLog();

		if(log)
		{
			NetworkLogUtils::WriteLogEvent(*log, "SCRIPT_RESERVING_LOCAL_OBJECTS", "%s      %d", GetScriptId().GetLogName(), n);
		}

		if (gnetVerifyf(n+GetNumGlobalReservedObjects() >= m_NumCreatedObjects || (GetNetworkComponent() && !GetNetworkComponent()->IsHostLocal()), "Script: %s is reserving less objects (%d) than the number that are currently active (%d)", GetScriptName(), n+GetNumGlobalReservedObjects(), m_NumCreatedObjects))
		{
			m_NumLocalReservedObjects = static_cast<u8>(n); 

			if (log)
			{
				CTheScripts::GetScriptHandlerMgr().LogCurrentReservations(*log);
			}
		}
	}
}

void CGameScriptHandlerNetwork::ReserveGlobalPeds(int n)				
{ 
	if (n != m_NumGlobalReservedPeds)
	{
		netLoggingInterface *log = CTheScripts::GetScriptHandlerMgr().GetLog();

		if(log)
		{
			NetworkLogUtils::WriteLogEvent(*log, "SCRIPT_RESERVING_GLOBAL_PEDS", "%s      %d", GetScriptId().GetLogName(), n);
		}

		if (gnetVerifyf(n+GetNumLocalReservedPeds() >= m_NumCreatedPeds || (GetNetworkComponent() && !GetNetworkComponent()->IsHostLocal()), "Script: %s is reserving less peds (%d) than the number that are currently active (%d)", GetScriptName(), n+GetNumLocalReservedPeds(), m_NumCreatedPeds))
		{
			m_NumGlobalReservedPeds = static_cast<u8>(n); 

			CGameScriptHandlerMgr::CRemoteScriptInfo* pRemoteScriptInfo = CTheScripts::GetScriptHandlerMgr().GetRemoteScriptInfo(GetScriptId(), true);

			if (pRemoteScriptInfo)
			{
				pRemoteScriptInfo->ReservePeds(n);
			}

			if (GetNetworkComponent() && GetNetworkComponent()->IsHostLocal())
			{
				static_cast<CGameScriptHandlerNetComponent*>(GetNetworkComponent())->BroadcastRemoteScriptInfo();
			}

			if (log)
			{
				CTheScripts::GetScriptHandlerMgr().LogCurrentReservations(*log);
			}
		}
	}
}

void CGameScriptHandlerNetwork::ReserveGlobalVehicles(int n)			
{ 
	if (n != m_NumGlobalReservedVehicles)
	{
		netLoggingInterface *log = CTheScripts::GetScriptHandlerMgr().GetLog();

		if(log)
		{
			NetworkLogUtils::WriteLogEvent(*log, "SCRIPT_RESERVING_GLOBAL_VEHICLES", "%s      %d", GetScriptId().GetLogName(), n);
		}

		if (gnetVerifyf(n+GetNumLocalReservedVehicles() >= m_NumCreatedVehicles || (GetNetworkComponent() && !GetNetworkComponent()->IsHostLocal()), "Script: %s is reserving less vehicles (%d) than the number that are currently active (%d)", GetScriptName(), n+GetNumLocalReservedVehicles(), m_NumCreatedVehicles))
		{
			m_NumGlobalReservedVehicles = static_cast<u8>(n); 

			CGameScriptHandlerMgr::CRemoteScriptInfo* pRemoteScriptInfo = CTheScripts::GetScriptHandlerMgr().GetRemoteScriptInfo(GetScriptId(), true);

			if (pRemoteScriptInfo)
			{
				pRemoteScriptInfo->ReserveVehicles(n);
			}

			if (GetNetworkComponent() && GetNetworkComponent()->IsHostLocal())
			{
				static_cast<CGameScriptHandlerNetComponent*>(GetNetworkComponent())->BroadcastRemoteScriptInfo();
			}

			if (log)
			{
				CTheScripts::GetScriptHandlerMgr().LogCurrentReservations(*log);
			}
		}
	}
}

void CGameScriptHandlerNetwork::ReserveGlobalObjects(int n)			
{ 
	if (n != m_NumGlobalReservedObjects)
	{
		netLoggingInterface *log = CTheScripts::GetScriptHandlerMgr().GetLog();

		if(log)
		{
			NetworkLogUtils::WriteLogEvent(*log, "SCRIPT_RESERVING_GLOBAL_OBJECTS", "%s      %d", GetScriptId().GetLogName(), n);
		}

		if (gnetVerifyf(n+GetNumLocalReservedObjects() >= m_NumCreatedObjects || (GetNetworkComponent() && !GetNetworkComponent()->IsHostLocal()), "Script: %s is reserving less objects (%d) than the number that are currently active (%d)", GetScriptName(), n+GetNumLocalReservedObjects(), m_NumCreatedObjects))
		{
			m_NumGlobalReservedObjects = static_cast<u8>(n); 

			CGameScriptHandlerMgr::CRemoteScriptInfo* pRemoteScriptInfo = CTheScripts::GetScriptHandlerMgr().GetRemoteScriptInfo(GetScriptId(), true);

			if (pRemoteScriptInfo)
			{
				pRemoteScriptInfo->ReserveObjects(n);
			}

			if (GetNetworkComponent() && GetNetworkComponent()->IsHostLocal())
			{
				static_cast<CGameScriptHandlerNetComponent*>(GetNetworkComponent())->BroadcastRemoteScriptInfo();
			}

			if (log)
			{
				CTheScripts::GetScriptHandlerMgr().LogCurrentReservations(*log);
			}
		}
	}
}

unsigned CGameScriptHandlerNetwork::GetNumRequiredEntities() const
{
    u32 numPeds     = 0;
    u32 numVehicles = 0;
    u32 numObjects  = 0;
    GetNumRequiredEntities(numPeds, numVehicles, numObjects);

    return numPeds + numVehicles + numObjects;
}

void CGameScriptHandlerNetwork::GetNumRequiredEntities(u32& numRequiredPeds, u32 &numRequiredVehicles, u32 &numRequiredObjects) const
{
	numRequiredPeds = numRequiredVehicles = numRequiredObjects = 0;

	if (gnetVerifyf(GetTotalNumReservedPeds() >= m_NumCreatedPeds,  "%s has more created peds(%d) than reserved ones(%d)", GetScriptId().GetLogName(), m_NumCreatedPeds, GetTotalNumReservedPeds()))
	{
		numRequiredPeds	= static_cast<u32>(GetTotalNumReservedPeds() - m_NumCreatedPeds);
	}

	if (gnetVerifyf(GetTotalNumReservedVehicles() >= m_NumCreatedVehicles, "%s has more created vehicles(%d) than reserved ones(%d)", GetScriptId().GetLogName(), m_NumCreatedVehicles, GetTotalNumReservedVehicles()))
	{
		numRequiredVehicles = static_cast<u32>(GetTotalNumReservedVehicles() - m_NumCreatedVehicles);
	}

	if (gnetVerifyf(GetTotalNumReservedObjects() >= m_NumCreatedObjects, "%s has more created objects(%d) than reserved ones(%d)", GetScriptId().GetLogName(), m_NumCreatedObjects, GetTotalNumReservedObjects()))
	{
		numRequiredObjects	= static_cast<u32>(GetTotalNumReservedObjects() - m_NumCreatedObjects);
	}
}

void CGameScriptHandlerNetwork::AcceptPlayers(bool bAccept)
{ 
	m_AcceptingPlayers = bAccept;

	if (GetNetworkComponent() && GetNetworkComponent()->GetState() == scriptHandlerNetComponent::NETSCRIPT_PLAYING)
	{
		if (scriptVerify(GetNetworkComponent()->IsHostLocal()))
		{
			GetNetworkComponent()->AcceptPlayers(bAccept); 
		}
	}
}

void CGameScriptHandlerNetwork::NetworkGameStarted()
{
	const GtaThread* pGtaThread = static_cast<const GtaThread*>(GetThread());

	// the thread may not exist at this point, if it has been terminated during the joining process
	if (pGtaThread && scriptVerify(GetNetworkComponent()))
	{
		if (GetNetworkComponent()->IsHostLocal())
		{
			AcceptPlayers(m_AcceptingPlayers);
		}
	}
}

bool CGameScriptHandlerNetwork::CanAcceptMigratingObject(scriptHandlerObject &object, const netPlayer& receivingPlayer, eMigrationType migrationType) const
{
	if (gnetVerify(m_NetComponent) && gnetVerify(object.GetScriptHandler() == this))
	{
		CNetObjProximityMigrateable* pNetObj = static_cast<CNetObjProximityMigrateable*>(object.GetNetObject());

		if (gnetVerify(pNetObj))
		{
			if (pNetObj->IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_SCRIPT_MIGRATION) || pNetObj->IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_CLONEONLY_SCRIPT))
			{
				if (receivingPlayer.IsLocal())
				{
					if (m_NetComponent->GetState() != scriptHandlerNetComponent::NETSCRIPT_PLAYING)
						return false;

					if (!m_NetComponent->IsPlayerAParticipant(receivingPlayer))
						return false;
				}
				else if (m_NetComponent->GetState() == scriptHandlerNetComponent::NETSCRIPT_TERMINATED)
				{
					// have to use the remote script info as we will not be informed of leaving players now as we have left the session
					CGameScriptHandlerMgr::CRemoteScriptInfo* pRemoteScriptInfo = CTheScripts::GetScriptHandlerMgr().GetRemoteScriptInfo(GetScriptId(), true);

					if (!pRemoteScriptInfo || !pRemoteScriptInfo->IsPlayerAParticipant(receivingPlayer.GetPhysicalPlayerIndex()))
						return false;
				}
				else
				{
					if (!m_NetComponent->IsPlayerAParticipant(receivingPlayer))
						return false;
				}
			}
			
			if (migrationType == MIGRATE_PROXIMITY && pNetObj->GetPlayerOwner() && pNetObj->GetPlayerOwner()->IsLocal() && pNetObj->GetPlayerOwner()->CanAcceptMigratingObjects())
			{
				bool bInSameInteriorLocation = pNetObj->IsInSameInterior(*pNetObj->GetPlayerOwner()->GetPlayerPed()->GetNetworkObject());

				// don't allow migration to non-participants if we are nearby the object. If we are host prevent migration if we are nearby.
				if (bInSameInteriorLocation && (!m_NetComponent->IsPlayerAParticipant(receivingPlayer) || (m_NetComponent->IsHostLocal() && pNetObj->FavourMigrationToHost())))
				{
					float scopeDist = pNetObj->GetScopeData().m_scopeDistance;
					float scopeDistSqr = scopeDist*scopeDist;

					Vector3 diff = NetworkUtils::GetNetworkPlayerPosition(*pNetObj->GetPlayerOwner()) - pNetObj->GetPosition();
					float distToOwner = diff.XYMag2();

					if (distToOwner < scopeDistSqr)
					{
						return false;
					}
				}
			}
		}
	}

	return true;
}

#define MAX_UNREGISTERING_OBJECTS 100

// cleans up any client created script objects associated with a participant slot 
void CGameScriptHandlerNetwork::CleanupAllClientObjects(ScriptSlotNumber clientSlot)
{
	const atDNode<scriptHandlerObject*, datBase> *node = m_ObjectList.GetHead();

	netLoggingInterface *log = CTheScripts::GetScriptHandlerMgr().GetLog();

	scriptHandlerObject* objectsToUnregister[MAX_UNREGISTERING_OBJECTS];
	u32 numObjectsToUnregister = 0;

	while (node)
	{
		scriptHandlerObject* pObject = node->Data;

		if (AssertVerify(pObject) &&
			AssertVerify(pObject->GetScriptInfo()) && 
			GET_SLOT_FROM_SCRIPT_OBJECT_ID(pObject->GetScriptInfo()->GetObjectId()) == (u32)clientSlot)
		{
			char logName[30];
			pObject->GetScriptInfo()->GetLogName(logName, 30);

			NetworkLogUtils::WriteLogEvent(*log, "UNREGISTER_PREVIOUS_CLIENT_OBJECT", "%s      %s", GetScriptId().GetLogName(), logName);

			if (AssertVerify(numObjectsToUnregister<MAX_UNREGISTERING_OBJECTS-1))
			{
				objectsToUnregister[numObjectsToUnregister++] = pObject;
			}
		}

		node = node->GetNext();
	}

	for (u32 i=0; i<numObjectsToUnregister; i++)
	{
		RemoveOldScriptObject(*objectsToUnregister[i]);
	}
}

bool CGameScriptHandlerNetwork::MigrateObjects()
{
	bool bObjectsToMigrate = false;

	if (!AssertVerify(m_NetComponent))
	{
		return false;
	}

	const CNetGamePlayer* aParticipants[MAX_NUM_PHYSICAL_PLAYERS];
	u32 numParticipants = 0;

	const netPlayer* pHost = NULL;

	// have to use the remote script info as we will not be informed of leaving players now as we have left the session
	CGameScriptHandlerMgr::CRemoteScriptInfo* pRemoteScriptInfo = CTheScripts::GetScriptHandlerMgr().GetRemoteScriptInfo(GetScriptId(), true);

	if (pRemoteScriptInfo)
	{
		unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
        const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

        for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
        {
	        const CNetGamePlayer *pPlayer = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);

			if (pRemoteScriptInfo->IsPlayerAParticipant(pPlayer->GetPhysicalPlayerIndex()))
			{
				aParticipants[numParticipants++] = pPlayer;
			}
		}

		pHost = pRemoteScriptInfo->GetHost();
	}

	static const u32 MAX_OBJECTS_TO_REMOVE = 128;
	CNetObjProximityMigrateable* objectsToRemove[MAX_OBJECTS_TO_REMOVE];
	u32 numObjectsToRemove = 0;

	const atDNode<scriptHandlerObject*, datBase> *node = m_ObjectList.GetHead();

	while (node)
	{
		scriptHandlerObject* pObject = node->Data;
		if (AssertVerify(pObject))
		{
			CNetObjProximityMigrateable* pNetObj = static_cast<CNetObjProximityMigrateable*>(pObject->GetNetObject());

			if (pNetObj && AssertVerify(pObject->GetScriptInfo()))
			{
				if (!pNetObj->IsClone() && pNetObj->IsGlobalFlagSet(netObject::GLOBALFLAG_PERSISTENTOWNER))
				{
					pNetObj->SetGlobalFlag(netObject::GLOBALFLAG_PERSISTENTOWNER, false);
				}

				if (pNetObj->IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_SCRIPT_MIGRATION) || pNetObj->IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_CLONEONLY_SCRIPT))
				{
					if (pNetObj->IsLocalFlagSet(netObject::LOCALFLAG_BEINGREASSIGNED))
					{
						// we must wait for the object to be reassigned, otherwise it may be reassigned to our machine after the script handler has terminated
						scriptInterface::GetScriptManager().WriteScriptHeader(scriptInterface::GetScriptManager().GetLog(), GetScriptId(), "Cannot terminate: waiting for %s to be reassigned", pNetObj->GetLogName());
						bObjectsToMigrate = true;
					}
					else if (!pNetObj->IsClone())
					{
						bool bRemove = numParticipants == 0;

						if (pNetObj->IsPendingOwnerChange())
						{
							scriptInterface::GetScriptManager().WriteScriptHeader(scriptInterface::GetScriptManager().GetLog(), GetScriptId(), "Cannot terminate: waiting for %s to migrate to %s", pNetObj->GetLogName(), pNetObj->GetPendingPlayerOwner() ? pNetObj->GetPendingPlayerOwner()->GetLogName() : "??");
							bObjectsToMigrate = true;
							bRemove = false;
						}
						else if (!bRemove)
						{
							bObjectsToMigrate = true;

							const CNetGamePlayer* pParticipantToMigrateTo = 0;
							Vector3 diff;
							float closestDist = -1.0f;

							float scopeDist = pNetObj->GetScopeDistance(pParticipantToMigrateTo);
							float scopeDistSqr = scopeDist*scopeDist;

							for (u32 i=0; i<numParticipants; i++)
							{
								const CNetGamePlayer* pParticipant = aParticipants[i];

								// check that the player is still a participant (he may have left after we informed him that we left)
								if (CTheScripts::GetScriptHandlerMgr().IsPlayerAParticipant(GetScriptId(), *pParticipant))
								{
									if (pNetObj->HasBeenCloned(*pParticipant))
									{
										diff = NetworkUtils::GetNetworkPlayerPosition(*pParticipant) - pNetObj->GetPosition();
										const float dist = diff.XYMag2();

										if (!pParticipantToMigrateTo || dist < closestDist)
										{
											pParticipantToMigrateTo = pParticipant;
											closestDist = dist;
										}
										else if (dist < scopeDistSqr)
										{
											// favour the host if he is nearby
											if (static_cast<const CNetGamePlayer*>(pHost) == pParticipant)
											{
												pParticipantToMigrateTo = pParticipant;
												break;
											}
										}
									}
								}
							}

							if (pParticipantToMigrateTo)
							{
								unsigned resultCode = 0;

								if (pNetObj->CanPassControl(*pParticipantToMigrateTo, MIGRATE_FORCED, &resultCode))
								{
									scriptInterface::GetScriptManager().WriteScriptHeader(scriptInterface::GetScriptManager().GetLog(), GetScriptId(), "Cannot terminate: starting to migrate %s to %s", pNetObj->GetLogName(), pParticipantToMigrateTo->GetLogName());
									CGiveControlEvent::Trigger(*pParticipantToMigrateTo, pNetObj, MIGRATE_FORCED);
								}
								else
								{
									scriptInterface::GetScriptManager().WriteScriptHeader(scriptInterface::GetScriptManager().GetLog(), GetScriptId(), "Cannot terminate: can't pass control of %s to %s (Reason: %s)", pNetObj->GetLogName(), pParticipantToMigrateTo->GetLogName(), NetworkUtils::GetCanPassControlErrorString(resultCode));
								}
							}
							else
							{
								bRemove = true;
							}
						}

						if (bRemove && AssertVerify(numObjectsToRemove < MAX_OBJECTS_TO_REMOVE))
						{
							// no participants left to take the object, so remove it
							objectsToRemove[numObjectsToRemove++] = pNetObj;
						}
					}
				}
			}
		}

		node = node->GetNext();
	}

	for (u32 i=0; i<numObjectsToRemove; i++)
	{
		objectsToRemove[i]->PrepareForScriptTermination();
		netInterface::GetObjectManager().UnregisterNetworkObject(objectsToRemove[i], CNetworkObjectMgr::SCRIPT_CLONE_ONLY, false, true);
	}

	if (!bObjectsToMigrate)
	{
		if (numParticipants == 0)
		{
			scriptInterface::GetScriptManager().WriteScriptHeader(scriptInterface::GetScriptManager().GetLog(), GetScriptId(), "No objects to migrate, no participants left");
		}
		else
		{
			scriptInterface::GetScriptManager().WriteScriptHeader(scriptInterface::GetScriptManager().GetLog(), GetScriptId(), "No objects to migrate");
		}
	}

	return bObjectsToMigrate;
}

#if __DEV
void CGameScriptHandlerNetwork::DisplayScriptHandlerInfo() const
{
	CGameScriptHandler::DisplayScriptHandlerInfo();

	if (m_NetComponent)
	{
		static_cast<CGameScriptHandlerNetComponent*>(m_NetComponent)->DisplayScriptHandlerInfo();

		grcDebugDraw::AddDebugOutput("Reserved : peds=%d/%d, vehicles=%d/%d, objects=%d/%d", 
			m_NumCreatedPeds, GetTotalNumReservedPeds(), 
			m_NumCreatedVehicles, GetTotalNumReservedVehicles(),
			m_NumCreatedObjects, GetTotalNumReservedObjects());


		if (m_NetComponent)
		{
			grcDebugDraw::AddDebugOutput("Bandwidth: Total in = %d, Total out = %d, Avg in = %d, Avg out = %d, Peak in = %d, Peak out = %d", m_NetComponent->GetTotalBandwidthIn(), m_NetComponent->GetTotalBandwidthOut(), m_NetComponent->GetAverageBandwidthIn(), m_NetComponent->GetAverageBandwidthOut(), m_NetComponent->GetPeakBandwidthIn(), m_NetComponent->GetPeakBandwidthOut());

			for (int i=0; i<m_NetComponent->GetNumHostBroadcastDatasRegistered(); i++)
			{
				netScriptBroadcastDataHandlerBase *hostHandler = m_NetComponent->GetHostBroadcastDataHandler(i);

				if (gnetVerify(hostHandler))
				{	
					grcDebugDraw::AddDebugOutput("Host broadcast data handler %d : total bandwidth out = %d, average out = %d, peak out = %d", i, hostHandler->GetTotalBandwidthOut(), hostHandler->GetAverageBandwidthOut(m_NetComponent->GetTimeHandlerActive()), hostHandler->GetPeakBandwidthOut());
				}
			}

			for (int i=0; i<m_NetComponent->GetNumPlayerBroadcastDatasRegistered(); i++)
			{
				netScriptBroadcastDataHandlerBase *playerHandler = m_NetComponent->GetLocalPlayerBroadcastDataHandler(i);

				if (gnetVerify(playerHandler))
				{	
					grcDebugDraw::AddDebugOutput("Player broadcast data handler %d : total bandwidth out = %d, average out = %d, peak out = %d", i, playerHandler->GetTotalBandwidthOut(), playerHandler->GetAverageBandwidthOut(m_NetComponent->GetTimeHandlerActive()), playerHandler->GetPeakBandwidthOut());
				}
			}
		}
	}
}

void CGameScriptHandlerNetwork::MarkCanRegisterMissionEntitiesCalled(u32 numPeds, u32 numVehicles, u32 numObjects)
{
	gnetAssert(numPeds<(1<<(sizeof(m_canRegisterMissionPedCalled)*8)));
	gnetAssert(numVehicles<(1<<(sizeof(m_canRegisterMissionVehicleCalled)*8)));
	gnetAssert(numObjects<(1<<(sizeof(m_canRegisterMissionObjectCalled)*8)));

	if (numPeds > 0)
	{
		m_canRegisterMissionPedCalled = static_cast<u8>(numPeds);
	}

	if (numVehicles > 0)
	{
		m_canRegisterMissionVehicleCalled = static_cast<u8>(numVehicles);
	}

	if (numObjects > 0)
	{
		m_canRegisterMissionObjectCalled = static_cast<u8>(numObjects);
	}

	CNetworkObjectPopulationMgr::eVehicleGenerationContext eOldGenContext = CNetworkObjectPopulationMgr::GetVehicleGenerationContext();
	CNetworkObjectPopulationMgr::SetVehicleGenerationContext(CNetworkObjectPopulationMgr::VGT_Script);

	gnetAssert(NetworkInterface::CanRegisterObjects(m_canRegisterMissionPedCalled, m_canRegisterMissionVehicleCalled, m_canRegisterMissionObjectCalled, 0, 0, true));

	CNetworkObjectPopulationMgr::SetVehicleGenerationContext(eOldGenContext);
}
#endif // __DEV

#if __BANK
void CGameScriptHandlerNetwork::DebugPrintUnClonedEntities()
{
	if (m_NetComponent && GetThread() && NetworkInterface::GetLocalPlayer() && NetworkInterface::GetLocalPlayer()->GetPhysicalPlayerIndex() != INVALID_PLAYER_INDEX)
	{
		PlayerFlags playerParticipants = m_NetComponent->GetPlayerParticipants();

		// don't include our local player (a network object's cloned state won't)
		playerParticipants &= ~(1 << NetworkInterface::GetLocalPlayer()->GetPhysicalPlayerIndex());

		const atDNode<scriptHandlerObject*, datBase> *node = m_ObjectList.GetHead();

		while (node)
		{
			scriptHandlerObject* pScriptObj = node->Data;

			if (pScriptObj && AssertVerify(pScriptObj->GetScriptInfo()) && pScriptObj->GetScriptInfo()->IsScriptHostObject())
			{
				CNetObjGame* pNetObj = static_cast<CNetObjGame*>(pScriptObj->GetNetObject());

				if (pNetObj &&
					!pNetObj->IsClone() &&
					!pNetObj->IsPendingOwnerChange() &&
					pNetObj->IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_CLONEALWAYS_SCRIPT) &&
					pNetObj->RestrictsBroadcastDataWhenUncloned())
				{
					PlayerFlags clonedState = pNetObj->GetClonedState();

					if ((clonedState & playerParticipants) != playerParticipants)
					{
						for (u32 i = 0; i < MAX_NUM_PHYSICAL_PLAYERS; i++)
						{
							if ((playerParticipants & (1 << i)) && !(clonedState & (1 << i)))
							{
								CNetGamePlayer* pPlayer = NetworkInterface::GetPhysicalPlayerFromIndex(static_cast<PhysicalPlayerIndex>(i));

								gnetDebug1("%s waiting to be clone on %s", pNetObj->GetLogName(), pPlayer ? pPlayer->GetLogName() : "(Missing Player)");
							}
						}
					}
				}
			}

			node = node->GetNext();
		}
	}
}
#endif // __BANK

bool CGameScriptHandlerNetwork::HaveAllScriptEntitiesFinishedCloning() const
{
	bool bFinishedCloning = true;

	if (m_NetComponent && GetThread() && NetworkInterface::GetLocalPlayer() && NetworkInterface::GetLocalPlayer()->GetPhysicalPlayerIndex() != INVALID_PLAYER_INDEX)
	{
		PlayerFlags playerParticipants = m_NetComponent->GetPlayerParticipants();

		// don't include our local player (a network object's cloned state won't)
		playerParticipants &= ~(1<<NetworkInterface::GetLocalPlayer()->GetPhysicalPlayerIndex());

		const atDNode<scriptHandlerObject*, datBase> *node = m_ObjectList.GetHead();

		while (node)
		{
			scriptHandlerObject* pScriptObj = node->Data;
	
			if (pScriptObj && AssertVerify(pScriptObj->GetScriptInfo()) && pScriptObj->GetScriptInfo()->IsScriptHostObject())
			{
				CNetObjGame* pNetObj = static_cast<CNetObjGame*>(pScriptObj->GetNetObject());

				if (pNetObj && 
					!pNetObj->IsClone() && 
					!pNetObj->IsPendingOwnerChange() && 
					pNetObj->IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_CLONEALWAYS_SCRIPT) &&
					pNetObj->RestrictsBroadcastDataWhenUncloned())
				{
					PlayerFlags clonedState = pNetObj->GetClonedState();

					if ((clonedState & playerParticipants) != playerParticipants)
					{
						for (u32 i=0; i<MAX_NUM_PHYSICAL_PLAYERS; i++)
						{
							if ((playerParticipants & (1<<i)) && !(clonedState & (1<<i)))
							{
								CNetGamePlayer* pPlayer = NetworkInterface::GetPhysicalPlayerFromIndex(static_cast<PhysicalPlayerIndex>(i));

								if (pPlayer)
								{
									scriptInterface::GetScriptManager().WriteScriptHeader(scriptInterface::GetScriptManager().GetLog(), GetScriptId(), "*** Broadcast data update delayed - waiting on %s to clone on %s ***", pNetObj->GetLogName(), pPlayer->GetLogName());
								}
								else
								{
									scriptInterface::GetScriptManager().WriteScriptHeader(scriptInterface::GetScriptManager().GetLog(), GetScriptId(), "**** Broadcast data update delayed - waiting on %s to clone on non-existent player %d ***", pNetObj->GetLogName(), i);
								}
							}
						}

						bFinishedCloning = false;

					}
				}
			}

			node = node->GetNext();
		}
	}

	return bFinishedCloning;
}

void CGameScriptHandlerNetwork::RegisterAllExistingScriptObjects()
{
	// When a script handler is created, any objects which are associated with the script need to be registered with it. The objects
	// may exist because they were created by another machine running the script, or because a script is starting up again and a 
	// previous instance of it left objects behind. The registering is done here and not in the object manager to avoid a dependency 
	// between the object manager and the script code. 

	netObject* objects[MAX_NUM_NETOBJECTS];

	for (int i=0; i<MAX_NUM_NETOBJECTS; i++)
	{
		objects[i] = 0;
	}

	unsigned numObjects = netInterface::GetObjectManager().GetAllObjects(objects, MAX_NUM_NETOBJECTS, true, true);

	for(unsigned index = 0; index < numObjects; index++)
	{
		if (gnetVerify(objects[index]) &&
			gnetVerify(objects[index]->GetScriptHandlerObject()) && 
			!objects[index]->GetScriptHandlerObject()->GetScriptHandler())
		{
			const scriptObjInfoBase *scriptInfo = objects[index]->GetScriptObjInfo();

			if (scriptInfo && scriptInfo->GetScriptId() == GetScriptId())
			{
				RegisterExistingScriptObjectInternal(*objects[index]->GetScriptHandlerObject());
			}
		}
	}
}

void CGameScriptHandlerNetwork::RegisterExistingScriptObjectInternal(scriptHandlerObject &object)
{
	// the script id must have a timestamp to distinguish it from other scripts that have ran in the past
	gnetAssert(static_cast<const CGameScriptId&>(GetScriptId()).GetTimeStamp() != 0 || object.IsUnnetworkedObject());	

	// if this a client object created by a previous participant that has left, clean it up
	netObject* pNetObj = object.GetNetObject();

	netLoggingInterface *log = CTheScripts::GetScriptHandlerMgr().GetLog();

	char logName[30];

	if (pNetObj)
	{
		if (object.IsHostObject())
		{
			if (GetNetworkComponent()->IsHostLocal())
			{
				ScriptObjectId objectIdIndex = GET_INDEX_FROM_SCRIPT_OBJECT_ID(object.GetScriptInfo()->GetObjectId());

				// unregister the object if we are the host and it is an object created by a previous host with a higher host object id
				if (objectIdIndex > m_NextFreeHostObjectId)
				{
					if (log)
					{
						object.GetScriptInfo()->GetLogName(logName, 30);
						NetworkLogUtils::WriteLogEvent(*log, "REGISTER_EXISTING_HOST_ENTITY", "%s      %s", object.GetScriptInfo()->GetScriptId().GetLogName(), logName);
						log->WriteDataValue("Script id", "%d", object.GetScriptInfo()->GetObjectId());
						log->WriteDataValue("Net Object", "%s", pNetObj ? pNetObj->GetLogName() : "-none-");
					}

					if (pNetObj->IsLocalFlagSet(CNetObjGame::LOCALFLAG_NOLONGERNEEDED))
					{
						if(log)
						{
							log->WriteDataValue("*Failed*",	"Entity already flagged to be cleaned up");
						}
					}
					else
					{
						if(log)
						{
							log->WriteDataValue("*Failed*",	"Entity to be cleaned up");
							log->WriteDataValue("Reason",	"Entity created by previous host with a higher object id than our current one (%d, current: %d)", objectIdIndex, m_NextFreeHostObjectId);
							log->WriteDataValue("Action",	"Inform owner (%s) to delete this entity", pNetObj->GetPlayerOwner() ? pNetObj->GetPlayerOwner()->GetLogName() : "<none>");

							pNetObj->SetLocalFlag(CNetObjGame::LOCALFLAG_NOLONGERNEEDED, true);

							// tell the owner to delete the entity - we can have the situation where the owner is not running the script anymore, and we are the only machine
							// that knows about the duplicate
							CMarkAsNoLongerNeededEvent::Trigger(*static_cast<CNetObjGame*>(pNetObj), true);
						}
					}

					return; 
				}
			}

			if (pNetObj->GetEntity())
			{
				ValidateRegistrationRequest(static_cast<CPhysical*>(pNetObj->GetEntity()), false);
			}
		}
		else 
		{
			int participantSlot = GET_SLOT_FROM_SCRIPT_OBJECT_ID(object.GetScriptInfo()->GetObjectId());

			bool bPreviousClientEntity = (participantSlot == GetNetworkComponent()->GetLocalSlot()) && !GetNetworkComponent()->IsPlaying();
			bool bParticipantNotActive = !GetNetworkComponent()->IsParticipantActive(participantSlot);

			if (bPreviousClientEntity || bParticipantNotActive)
			{
				if (log)
				{
					object.GetScriptInfo()->GetLogName(logName, 30);
					NetworkLogUtils::WriteLogEvent(*log, "REGISTER_EXISTING_LOCAL_ENTITY", "%s      %s", object.GetScriptInfo()->GetScriptId().GetLogName(), logName);
					log->WriteDataValue("Script id", "%d", object.GetScriptInfo()->GetObjectId());
					log->WriteDataValue("Net Object", "%s", pNetObj ? pNetObj->GetLogName() : "-none-");
				}

				if (pNetObj->IsLocalFlagSet(CNetObjGame::LOCALFLAG_NOLONGERNEEDED))
				{
					if(log)
					{
						log->WriteDataValue("*Failed*",	"Entity already flagged to be cleaned up");
					}
				}
				else
				{
					if(log)
					{
						log->WriteDataValue("*Failed*",	"Entity to be cleaned up");

						if (bPreviousClientEntity)
						{
							log->WriteDataValue("Reason",	"Entity created by previous client in this slot (%d)", participantSlot);
						}
						else if (bParticipantNotActive)
						{
							log->WriteDataValue("Reason",	"Participant in slot %d no longer active", participantSlot);
						}
					}

					RemoveOldScriptObject(object);
				}

				return; 
			}
		}
	}

	CGameScriptHandler::RegisterExistingScriptObjectInternal(object);

	if (!object.GetScriptHandler())
	{
		gnetAssertf(0, "Script object failed to register");
	}
}

void CGameScriptHandlerNetwork::RemoveOldScriptObject(scriptHandlerObject &object)
{
	netLoggingInterface *log = CTheScripts::GetScriptHandlerMgr().GetLog();
	netObject* pNetObj = object.GetNetObject();

	char logName[30];
	object.GetScriptInfo()->GetLogName(logName, 30);

	if(log)
	{
		if (object.IsHostObject())
		{
			NetworkLogUtils::WriteLogEvent(*log, "REMOVE_OLD_HOST_OBJECT", "%s      %s", GetScriptId().GetLogName(), logName);
		}
		else
		{
			NetworkLogUtils::WriteLogEvent(*log, "REMOVE_OLD_CLIENT_OBJECT", "%s      %s", GetScriptId().GetLogName(), logName);
		}
		log->WriteDataValue("Script id", "%d", object.GetScriptInfo()->GetObjectId());
		log->WriteDataValue("Net Object", "%s", pNetObj ? pNetObj->GetLogName() : "-none-");
	}

	if (pNetObj)
	{
		bool bIsClone = pNetObj->IsClone() || pNetObj->IsPendingOwnerChange();

		// try to remove host entities, previous client entities can stick around
		bool bToBeDeleted = object.IsHostObject() && object.GetEntity();

		pNetObj->SetLocalFlag(CNetObjGame::LOCALFLAG_NOLONGERNEEDED, true);

		if (bIsClone && GetNetworkComponent())
		{
			bool bTriggerEvent = GetNetworkComponent()->IsHostLocal();

			if (!bTriggerEvent && !object.IsHostObject())
			{
				int participantSlot = GET_SLOT_FROM_SCRIPT_OBJECT_ID(object.GetScriptInfo()->GetObjectId());
				bTriggerEvent = (participantSlot == GetNetworkComponent()->GetLocalSlot());
			}

			if (bTriggerEvent)
			{
				CMarkAsNoLongerNeededEvent::Trigger(*static_cast<CNetObjGame*>(pNetObj), bToBeDeleted);
			}
		}

		if (bToBeDeleted)
		{
			CNetObjEntity* pEntityObj = SafeCast(CNetObjEntity, pNetObj);

			if (!bIsClone && pEntityObj->CanDelete())
			{
				if (log)
				{
					NetworkLogUtils::WriteLogEvent(*log, "UNREGISTERING_ENTITY", "%s      %s", GetScriptId().GetLogName(), logName);
					log->WriteDataValue("Net Object", "%s", pNetObj ? pNetObj->GetLogName() : "-none-");
				}

				netInterface::GetObjectManager().UnregisterNetworkObject(pEntityObj, CNetworkObjectMgr::DUPLICATE_SCRIPT_OBJECT, false, true);

				return;
			}
			else
			{
				if (log)
				{
					NetworkLogUtils::WriteLogEvent(*log, "FLAGGING_ENTITY_FOR_DELETION", "%s      %s", GetScriptId().GetLogName(), logName);
					log->WriteDataValue("Net Object", "%s", pNetObj ? pNetObj->GetLogName() : "-none-");
				}

				pEntityObj->FlagForDeletion();
			}
		}	
	}

	// the object may not have been registered with this script yet 
	if (object.GetScriptHandler())
	{
		scriptHandler::RemoveOldScriptObject(object);
	}
}

void CGameScriptHandlerNetwork::RemoveOldHostObjects()
{
	const atDNode<scriptHandlerObject*, datBase> *node = m_ObjectList.GetHead();

	scriptHandlerObject* objectsToUnregister[MAX_UNREGISTERING_OBJECTS];
	u32 numObjectsToUnregister = 0;

	while (node)
	{
		scriptHandlerObject* pScriptObj = node->Data;
		const scriptObjInfoBase *pScriptInfo = pScriptObj ? pScriptObj->GetScriptInfo() : NULL;

		if (pScriptInfo && pScriptObj->IsHostObject() && GET_INDEX_FROM_SCRIPT_OBJECT_ID(pScriptInfo->GetObjectId()) > m_NextFreeHostObjectId)
		{
			if (AssertVerify(numObjectsToUnregister<MAX_UNREGISTERING_OBJECTS-1))
			{
				objectsToUnregister[numObjectsToUnregister++] = pScriptObj;
			}
		}

		node = node->GetNext();
	}

	for (u32 i=0; i<numObjectsToUnregister; i++)
	{
		scriptHandlerObject* pScriptObj = objectsToUnregister[i];

		RemoveOldScriptObject(*pScriptObj);
	}
}

void CGameScriptHandlerNetwork::ValidateRegistrationRequest(CPhysical* pTargetEntity, bool DEV_ONLY(bNewObject))
{
#if __ASSERT
	bool bIAmHost = gnetVerify(m_NetComponent) && m_NetComponent->IsHostLocal();
#endif
	netLoggingInterface *log = CTheScripts::GetScriptHandlerMgr().GetLog();

#if __ASSERT
	const char* modelName = pTargetEntity && pTargetEntity->GetModelName() ? pTargetEntity->GetModelName() : "-none-";
#endif

	bool bTargetEntityIsLocalReservation = pTargetEntity && pTargetEntity->GetNetworkObject() && pTargetEntity->GetNetworkObject()->IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_CLONEONLY_SCRIPT);

	if (pTargetEntity->GetIsTypePed())
	{
		m_NumCreatedPeds++;

		if (m_NumCreatedPeds > GetTotalNumReservedPeds())
		{
			ASSERT_ONLY(gnetAssertf(!bIAmHost || !bNewObject, "Script: %s is creating more peds than it has reserved (ped model : %s)", GetScriptName(), modelName);)
			if (bTargetEntityIsLocalReservation)
			{
				m_NumLocalReservedPeds = m_NumCreatedPeds-m_NumGlobalReservedPeds;
			}
			else
			{
				m_NumGlobalReservedPeds = m_NumCreatedPeds-m_NumLocalReservedPeds;
			}
		}
#if __ASSERT
		if (bIAmHost && bNewObject)
		{
			gnetAssertf(m_canRegisterMissionPedCalled > 0, "Script: %s is creating more peds than have been verified using CAN_REGISTER_MISSION_PEDS (ped model: %s)", GetScriptName(), modelName);

			if(m_canRegisterMissionPedCalled > 0)
			{
				m_canRegisterMissionPedCalled--;
			}
		}
#endif // __DEV

        if(log)
        {
		    log->WriteDataValue("Ped Reservation",	"%d/%d (%d)", m_NumCreatedPeds, GetTotalNumReservedPeds(), m_NumLocalReservedPeds);
        }
	}
	else if (pTargetEntity->GetIsTypeVehicle())
	{
		m_NumCreatedVehicles++;

		if (m_NumCreatedVehicles > GetTotalNumReservedVehicles())
		{
			ASSERT_ONLY(gnetAssertf(!bIAmHost || !bNewObject, "Script: %s is creating more vehicles than it has reserved (vehicle model : %s)", GetScriptName(), modelName);)
			if (bTargetEntityIsLocalReservation)
			{
				m_NumLocalReservedVehicles = m_NumCreatedVehicles-m_NumGlobalReservedVehicles;
			}
			else
			{
				m_NumGlobalReservedVehicles = m_NumCreatedVehicles-m_NumLocalReservedVehicles;
			}
		}
#if __ASSERT
		if (bIAmHost)
		{
			if (bNewObject)
			{
				gnetAssertf(m_canRegisterMissionVehicleCalled > 0, "Script: %s is creating more vehicles than have been verified using CAN_REGISTER_MISSION_VEHICLES (vehicle model: %s)", GetScriptName(), modelName);

				if(m_canRegisterMissionVehicleCalled > 0)
				{
					m_canRegisterMissionVehicleCalled--;
				}
			}
		}
#endif // __DEV

        if(log)
        {
		    log->WriteDataValue("Vehicle Reservation",	"%d/%d (%d)", m_NumCreatedVehicles, GetTotalNumReservedVehicles(), m_NumLocalReservedVehicles);
        }
	}
	else if (pTargetEntity->GetIsTypeObject())
	{
		// ignore doors & pickups
		if (!static_cast<CObject*>(pTargetEntity)->IsADoor() && !static_cast<CObject*>(pTargetEntity)->m_nObjectFlags.bIsPickUp)
		{
			m_NumCreatedObjects++;

			if (GetTotalNumReservedObjects() < m_NumCreatedObjects)
			{
				ASSERT_ONLY(gnetAssertf(!bNewObject, "Script: %s is creating more objects than it has reserved (object model : %s)", GetScriptName(), modelName);)
				if (bTargetEntityIsLocalReservation)
				{
					m_NumLocalReservedObjects = m_NumCreatedObjects-m_NumGlobalReservedObjects;
				}
				else
				{
					m_NumGlobalReservedObjects = m_NumCreatedObjects-m_NumLocalReservedObjects;
				}
			}
#if __ASSERT
			if (bIAmHost && bNewObject)
			{
				gnetAssertf(m_canRegisterMissionObjectCalled > 0, "Script: %s is creating more objects than have been verified using CAN_REGISTER_MISSION_OBJECTS (object model: %s)", GetScriptName(), modelName);

				if(m_canRegisterMissionObjectCalled > 0)
				{
					m_canRegisterMissionObjectCalled--;
				}
			}
#endif // __DEV

			if(log)
			{
				log->WriteDataValue("Object Reservation",	"%d/%d (%d)", m_NumCreatedObjects, GetTotalNumReservedObjects(), m_NumLocalReservedObjects);
			}
		}
	}
	else
	{
		gnetAssertf(0, "Unexpected target object type encountered!");
	}
}

// ================================================================================================================
// CGameScriptHandlerNetComponent
// ================================================================================================================

CGameScriptHandlerNetComponent::CGameScriptHandlerNetComponent(CGameScriptHandler& parentHandler, u32 maxNumParticipants)
: scriptHandlerNetComponent(parentHandler)
, m_JoinEventFlags(0)
, m_PendingTimestamp(0)
, m_bScriptReady(false)
, m_bStartedPlaying(false)
, m_bDoneLeaveCleanup(false)
, m_bDoneTerminationCleanup(false)
, m_bBroadcastScriptInfo(false)
#if __BANK
, m_joiningTimer(0)
, m_noHostTimer(0)
#endif
{
	SetMaxNumParticipants(maxNumParticipants);

	if (static_cast<CGameScriptId&>(m_ParentHandler.GetScriptId()).GetTimeStamp() != 0)
	{
		Assertf(0, "Script %s is being networked after having been previously networked. This probably means that it was kept running between two separate network sessions", m_ParentHandler.GetScriptId().GetLogName());
		static_cast<CGameScriptId&>(m_ParentHandler.GetScriptId()).ResetTimestamp();
	}
}

void CGameScriptHandlerNetComponent::TriggerJoinEvents()
{
	// wait until a new participant is fully into the game (has joined our roaming bubble and cloned his player ped) before informing the script
	if (m_JoinEventFlags != 0)
	{
		if (m_State == NETSCRIPT_INTERNAL_START)
		{
			m_JoinEventFlags = 0;
		}
		else if (m_State == NETSCRIPT_INTERNAL_PLAYING && !m_Terminated)
		{
			for (unsigned i=0; i<GetMaxNumParticipants(); i++)
			{
				if (m_JoinEventFlags & (1<<i))
				{
					const CNetGamePlayer* pParticipant = static_cast<const CNetGamePlayer*>(GetParticipantUsingSlot(i));

					if (gnetVerify(pParticipant) && pParticipant->IsPhysical() && pParticipant->GetPlayerPed())
					{
						if (m_ParentHandler.GetThread())
						{
                            // special leave type
                            CEventNetworkPlayerJoinScript::eSource nSource = CEventNetworkPlayerJoinScript::SOURCE_NORMAL;
                            if(pParticipant->HasStartedTransition())
                                nSource = CEventNetworkPlayerJoinScript::SOURCE_TRANSITION;
                            else if(CNetwork::GetNetworkSession().DidGamerLeaveForStore(pParticipant->GetGamerInfo().GetGamerHandle()))
                                nSource = CEventNetworkPlayerJoinScript::SOURCE_STORE;

							if (!GetEventScriptNetworkGroup()->CollateJoinScriptEventForPlayer(*static_cast<const netPlayer*>(pParticipant), nSource, m_ParentHandler.GetThread()->GetThreadId()))
							{
								CEventNetworkPlayerJoinScript joinEvent(m_ParentHandler.GetThread()->GetThreadId(), *pParticipant, nSource);
								GetEventScriptNetworkGroup()->Add(joinEvent);
							}
						}

						m_JoinEventFlags &= ~(1<<i);
					}
				}
			}
		}
	}
}

bool CGameScriptHandlerNetComponent::Update()
{
#if __BANK
#define DEBUG_WARNING_TIME 120000 // 2 minutes

	if (m_State > NETSCRIPT_INTERNAL_START && m_State < NETSCRIPT_INTERNAL_PLAYING)
	{
		// display a message on screen when the script has been taking more than the timeout time to join
		// this is to avoid bugs being reported when the script fails to start up
		m_joiningTimer += fwTimer::GetTimeStepInMilliseconds();

		if (m_joiningTimer > DEBUG_WARNING_TIME)
		{
#if ENABLE_NETWORK_LOGGING
			if (CTheScripts::GetScriptHandlerMgr().GetLog() && 
				CTheScripts::GetScriptHandlerMgr().GetLog()->IsEnabled())
			{
				char message[100];
				formatf(message, "%s cannot start - a machine is not responding", m_ParentHandler.GetScriptId().GetLogName());

				NetworkDebug::DisplayDebugMessage(message);
			}
#endif // ENABLE_NETWORK_LOGGING
		}

		// start logging after 5 secs
		if (m_joiningTimer > 5000)
		{
			for (u32 i=0; i<MAX_NUM_PHYSICAL_PLAYERS; i++)
			{
				if (m_WaitingForAcks & (1<<i))
				{
					CNetGamePlayer* pPlayer = NetworkInterface::GetPhysicalPlayerFromIndex(static_cast<PhysicalPlayerIndex>(i));

					if (pPlayer)
					{
						scriptInterface::GetScriptManager().WriteScriptHeader(scriptInterface::GetScriptManager().GetLog(), m_ParentHandler.GetScriptId(), "*** Delayed response from %s *** ", pPlayer->GetLogName());
					}
					else
					{
						scriptInterface::GetScriptManager().WriteScriptHeader(scriptInterface::GetScriptManager().GetLog(), m_ParentHandler.GetScriptId(), "*** Delayed response from non-existent player in slot %d ***", i);
					}
				}
			}
		}
	}
	else
	{
		m_joiningTimer = 0;
	}

	if (m_State == NETSCRIPT_INTERNAL_PLAYING && !GetHost())
	{
		m_noHostTimer += fwTimer::GetTimeStepInMilliseconds();

		if (m_noHostTimer > DEBUG_WARNING_TIME)
		{
#if ENABLE_NETWORK_LOGGING
			if (CTheScripts::GetScriptHandlerMgr().GetLog() && 
				CTheScripts::GetScriptHandlerMgr().GetLog()->IsEnabled())
			{
				char message[200];
				formatf(message, "%s has no host. Report this as an A.", m_ParentHandler.GetScriptId().GetLogName());

				NetworkDebug::DisplayDebugMessage(message);
			}
#endif // ENABLE_NETWORK_LOGGING
		}

		// start logging after 5 secs
		if (m_noHostTimer > 5000)
		{
			scriptInterface::GetScriptManager().WriteScriptHeader(scriptInterface::GetScriptManager().GetLog(), m_ParentHandler.GetScriptId(), "*** No host (Pending : %s) *** ", m_PendingHost ? m_PendingHost->GetLogName() : "-none-");

			if (m_noHostTimer > DEBUG_WARNING_TIME)
			{
				Assertf(0, "%s has no host. Please provide logs for all machines as this is serious", m_ParentHandler.GetScriptId().GetLogName());
			}
		}
	}
	else
	{
		m_noHostTimer = 0;
	}
#endif // __BANK

	bool bCanTerminate = scriptHandlerNetComponent::Update();

#if __BANK
	if (bCanTerminate)
	{
		Displayf("The handler for script %s has terminated\n", m_ParentHandler.GetScriptId().GetLogName());
	}
	else if (m_State == NETSCRIPT_INTERNAL_LEAVE)
	{
		Displayf("The handler for script %s is terminating: waiting for broadcast data to sync\n", m_ParentHandler.GetScriptId().GetLogName());
	}
	else if (m_State == NETSCRIPT_INTERNAL_LEAVING)
	{
		Displayf("The handler for script %s is terminating: waiting for leave event responses\n", m_ParentHandler.GetScriptId().GetLogName());
	}
#endif

	if (m_bBroadcastScriptInfo && (!IsHostLocal() || BroadcastRemoteScriptInfo()))
	{
		m_bBroadcastScriptInfo = false;
	}

	return bCanTerminate;
}

void CGameScriptHandlerNetComponent::PlayerHasJoined(const netPlayer& player)
{
    if(static_cast<const CNetGamePlayer &>(player).GetPlayerType() == CNetGamePlayer::NETWORK_PLAYER)
    {
		if (IsHostLocal())
		{
			// inform the player about this active script
			if (!BroadcastRemoteScriptInfo(&player))
			{
				m_bBroadcastScriptInfo = true;
			}
		}

	    scriptHandlerNetComponent::PlayerHasJoined(player);
    }
}

void CGameScriptHandlerNetComponent::HandleJoinAckMsg(const msgScriptJoinAck& msg, const ReceivedMessageData &messageData)
{
	if (msg.m_AckCode == SCRIPT_ACK_CODE_PARTICIPANT)
	{
		unsigned msgTimestamp = static_cast<CGameScriptId*>(msg.m_ScriptId)->GetTimeStamp();

		if (m_PendingTimestamp == 0)
		{
			m_PendingTimestamp = msgTimestamp;
		}
		else if (msgTimestamp != 0 && m_PendingTimestamp != msgTimestamp)
		{
			gnetAssertf(0, "Got a join ack message from a participant with an unexpected timestamp!");
			ClearWaitingForAck(*messageData.m_FromPlayer);
			return;
		}
	}

	scriptHandlerNetComponent::HandleJoinAckMsg(msg, messageData);
}

void CGameScriptHandlerNetComponent::HandleJoinHostAckMsg(const msgScriptJoinHostAck& msg, const ReceivedMessageData &messageData)
{
	bool bIsAParticipant = IsPlayerAParticipant(*NetworkInterface::GetLocalPlayer());

	scriptHandlerNetComponent::HandleJoinHostAckMsg(msg, messageData);

	if (!bIsAParticipant && IsPlayerAParticipant(*NetworkInterface::GetLocalPlayer()))
	{
		unsigned msgTimestamp = static_cast<CGameScriptId*>(msg.m_ScriptId)->GetTimeStamp();

		gnetAssertf(msgTimestamp != 0, "Got a join host ack message from a host with a 0 timestamp!");
		gnetAssertf(m_PendingTimestamp == 0 || m_PendingTimestamp == msgTimestamp, "Got a join host ack message from a host with an unexpected timestamp!");
		
		m_PendingTimestamp = msgTimestamp;
	}
}

void CGameScriptHandlerNetComponent::MigrateScriptHost(const netPlayer& player)
{
	scriptHandlerNetComponent::MigrateScriptHost(player);

	if (m_State == NETSCRIPT_INTERNAL_PLAYING && m_ParentHandler.GetThread())
	{
		CEventNetworkAttemptHostMigration migrateEvent(m_ParentHandler.GetThread()->GetThreadId(), static_cast<const CNetGamePlayer&>(player));
		GetEventScriptNetworkGroup()->Add(migrateEvent);
	}
}

CNetGamePlayer* CGameScriptHandlerNetComponent::GetClosestParticipant(const Vector3& pos, bool bIncludeMyPlayer) const
{
	const CNetGamePlayer* pClosestPlayer = NULL;
	float closestDist = 0.0f;

	for (ScriptSlotNumber slot =0; slot<m_MaxNumParticipants; slot++)
	{
		const participantData* pParticipant = m_ParticipantsArray[slot];

		if (pParticipant)
		{
			const CNetGamePlayer* pPlayer = static_cast<const CNetGamePlayer*>(pParticipant->GetPlayer());

			if (pPlayer->GetPlayerPed() && (bIncludeMyPlayer || !pPlayer->IsMyPlayer()))
			{
				Vector3 playerPos = VEC3V_TO_VECTOR3(pPlayer->GetPlayerPed()->GetTransform().GetPosition());
				Vector3 diff = playerPos - pos;

				float playerDist = diff.XYMag2();

				if (!pClosestPlayer || playerDist < closestDist)
				{
					pClosestPlayer = pPlayer;
					closestDist = playerDist;
				}
			}
		}
	}

	return const_cast<CNetGamePlayer*>(pClosestPlayer);
}

PlayerFlags CGameScriptHandlerNetComponent::GetParticipantFlags() const
{
	PlayerFlags participantFlags = 0;

	for (ScriptSlotNumber slot =0; slot<m_MaxNumParticipants; slot++)
	{
		const participantData* pParticipant = m_ParticipantsArray[slot];

		if (pParticipant && gnetVerify(pParticipant->GetPlayer()))
		{
			participantFlags |= (1<<pParticipant->GetPlayer()->GetPhysicalPlayerIndex());
		}
	}

	return participantFlags;
}

bool CGameScriptHandlerNetComponent::IsParticipantPhysical(int participantSlot, const CNetGamePlayer** ppParticipant)
{
	if (scriptHandlerNetComponent::IsParticipantActive(participantSlot))
	{
		const CNetGamePlayer* player = static_cast<const CNetGamePlayer*>(GetParticipantUsingSlot(participantSlot));

		// returns true if the participant with the given id is active, in our roaming bubble, has a player ped and has had a script join event triggered for him
		if (player && player->IsPhysical() && player->GetPlayerPed() && !(m_JoinEventFlags & (1<<participantSlot)))
		{
			if (ppParticipant)
				*ppParticipant = player;

			return true;
		}
	}

	return false;
}

#if __DEV
void CGameScriptHandlerNetComponent::DisplayScriptHandlerInfo() const
{
	switch (m_State)
	{
	case NETSCRIPT_INTERNAL_START:
		grcDebugDraw::AddDebugOutput("State: Waiting to start");
		break;
	case NETSCRIPT_INTERNAL_JOIN:
	case NETSCRIPT_INTERNAL_JOINING:
	case NETSCRIPT_INTERNAL_ACCEPTED:
	case NETSCRIPT_INTERNAL_RESTARTING:
		grcDebugDraw::AddDebugOutput("State: Joining");
		break;
	case NETSCRIPT_INTERNAL_HANDSHAKING:
		grcDebugDraw::AddDebugOutput("State: Handshaking");
		break;
	case NETSCRIPT_INTERNAL_READY_TO_PLAY:
	case NETSCRIPT_INTERNAL_PLAYING:
		grcDebugDraw::AddDebugOutput("State: Playing");
		break;
	case NETSCRIPT_INTERNAL_LEAVE:
	case NETSCRIPT_INTERNAL_LEAVING:
		grcDebugDraw::AddDebugOutput("State: Leaving");
		break;
	case NETSCRIPT_INTERNAL_TERMINATING:
		grcDebugDraw::AddDebugOutput("State: Terminating");
		break;
	case NETSCRIPT_INTERNAL_FINISHED:
		grcDebugDraw::AddDebugOutput("State: Finished");
		break;
	default:
		gnetAssertf(0, "Unrecognised internal script handler net component state");
	}

	grcDebugDraw::AddDebugOutput("Num participants: %d / %d", m_NumParticipants, m_MaxNumParticipants);

	char otherPlayers[200];
	otherPlayers[0] = 0;

	const netPlayer* pHost = NULL;

	for (u32 i=0; i<m_MaxNumParticipants; i++)
	{
		const netPlayer* pParticipant = GetParticipantUsingSlot(i);

		if (pParticipant)
		{
			if (pParticipant == GetHost())
			{
				pHost = pParticipant;
			}
			else
			{
				int len = istrlen(otherPlayers);

				if (len + strlen(pParticipant->GetGamerInfo().GetName()) <= 200)
				{
					if (pParticipant->IsMyPlayer())
					{
						formatf(otherPlayers, "%s Us(%d)", otherPlayers, GetSlotParticipantIsUsing(*pParticipant));
					}
					else
					{
						formatf(otherPlayers, "%s %s(%d)", otherPlayers, pParticipant->GetGamerInfo().GetName(), GetSlotParticipantIsUsing(*pParticipant));
					}
				}
			}
		}
	}

	if (IsPlayerAParticipant(*NetworkInterface::GetPlayerMgr().GetMyPlayer()))
	{
		grcDebugDraw::AddDebugOutput("My participant num: %d", GetSlotParticipantIsUsing(*NetworkInterface::GetPlayerMgr().GetMyPlayer()));
	}
	else
	{
		grcDebugDraw::AddDebugOutput("We are not a participant");
	}

	if (pHost)
	{
		if (pHost->IsMyPlayer())
		{
			grcDebugDraw::AddDebugOutput("Host: Us (%d)", GetSlotParticipantIsUsing(*pHost));
		}
		else
		{
			grcDebugDraw::AddDebugOutput("Host: %s (%d)", pHost->GetLogName(), GetSlotParticipantIsUsing(*pHost));
		}
	}
	else
		grcDebugDraw::AddDebugOutput("Host: -none-");

	if (strlen(otherPlayers) > 0)
		grcDebugDraw::AddDebugOutput("Other participants: %s", otherPlayers);
	else
		grcDebugDraw::AddDebugOutput("No other participants");


}
#endif 

bool CGameScriptHandlerNetComponent::AddPlayerAsParticipant(const netPlayer& player, ScriptParticipantId participantId, ScriptSlotNumber slotNumber)
{
	bool ret = true;

	// we shouldn't be accepting new participants when we are migrating the host
	gnetAssert(!(IsHostLocal() && m_PendingHost));

	if (!IsPlayerAParticipant(player))
	{
		ret = scriptHandlerNetComponent::AddPlayerAsParticipant(player, participantId, slotNumber);
		
		if (ret)
		{
			// we must wait until the script has a timestamp before we can add a player to the remote script info
			if (m_State >= NETSCRIPT_INTERNAL_PLAYING)
			{
				CTheScripts::GetScriptHandlerMgr().AddPlayerToRemoteScript(m_ParentHandler.GetScriptId(), player);
			}

			// if we are the host, inform other non-participant players about the new participant
			if (IsHostLocal())
			{
				m_bBroadcastScriptInfo = true;
			}

			// set a flag for this participant, indicating that we need to trigger a script join event for him
			if (!player.IsMyPlayer())
			{
				m_JoinEventFlags |= (1<<slotNumber);
			}
		}
	}

	return ret;
}

void CGameScriptHandlerNetComponent::RemovePlayerAsParticipant(const netPlayer& player)
{
	participantData* pParticipant = GetParticipantsData(player);

	if (pParticipant)
	{
		// cleanup all objects created by this player
		static_cast<CGameScriptHandlerNetwork*>(&m_ParentHandler)->CleanupAllClientObjects(pParticipant->GetSlotNumber());

		if (!m_Terminated)
		{
			// only trigger a leave event if the player is physical and a join event was triggered
			if (m_JoinEventFlags & (1<<pParticipant->GetSlotNumber()))
			{
				// no join event has been triggered yet, so just dump it
				m_JoinEventFlags &= ~(1<<pParticipant->GetSlotNumber());
			}
		}
	}

	scriptHandlerNetComponent::RemovePlayerAsParticipant(player);

	// we must wait until the script has a timestamp before we can remove a player to the remote script info
	if (m_State >= NETSCRIPT_INTERNAL_PLAYING && m_bStartedPlaying)
	{
		CTheScripts::GetScriptHandlerMgr().RemovePlayerFromRemoteScript(m_ParentHandler.GetScriptId(), player, true);
	}

	// if we are the host, inform other non-participant players about leaving participant, unless the player is leaving the session - 
	// every other player will be informed of this and adjust their remote script info accordingly.
	if (!player.IsLeaving() && IsHostLocal())
	{
		m_bBroadcastScriptInfo = true;
	}
}

void CGameScriptHandlerNetComponent::HandleLeavingPlayer(const netPlayer& player, bool playerLeavingSession)
{
	if (m_ParentHandler.GetThread() && IsPlayerAParticipant(player))
	{
		const CNetGamePlayer& gamePlayer = static_cast<const CNetGamePlayer&>(player);

		// special leave type
		CEventNetworkPlayerLeftScript::eSource nSource = CEventNetworkPlayerLeftScript::SOURCE_NORMAL;
		if(gamePlayer.HasStartedTransition())
			nSource = CEventNetworkPlayerLeftScript::SOURCE_TRANSITION;
		else if(CNetwork::GetNetworkSession().DidGamerLeaveForStore(gamePlayer.GetGamerInfo().GetGamerHandle()))
			nSource = CEventNetworkPlayerLeftScript::SOURCE_STORE;

		// find an existing event to collate, or generate a new event
		if (!GetEventScriptNetworkGroup()->CollateLeftScriptEventForPlayer(player, nSource, m_ParentHandler.GetThread()->GetThreadId()))
		{
			CEventNetworkPlayerLeftScript leaveEvent(m_ParentHandler.GetThread()->GetThreadId(), gamePlayer, nSource, gamePlayer.GetSessionBailReason());
			GetEventScriptNetworkGroup()->Add(leaveEvent);
		}
	}

	scriptHandlerNetComponent::HandleLeavingPlayer(player, playerLeavingSession);
}

void CGameScriptHandlerNetComponent::SetNewScriptHost(const netPlayer* pPlayer, HostToken hostToken, bool bBroadcast)
{
	CGameScriptId* pHandlerId = static_cast<CGameScriptId*>(&m_ParentHandler.GetScriptId());

	if (pPlayer)
	{
		gnetAssert(hostToken != INVALID_HOST_TOKEN);

		if (pHandlerId->GetTimeStamp() == 0)
		{
			if (pPlayer && pPlayer->IsLocal())
			{
				unsigned netTime = NetworkInterface::GetNetworkTime();

				// if the net time is 0, we have to timestamp with a non-zero value
				if (!Verifyf(netTime != 0, "Trying to timestamp a script with a network time of 0! Timestamping with a random number instead"))
				{
					netTime = fwRandom::GetRandomNumber();
				}

				// we are the first host and so the script has just started. Stamp the current time into the network id to make it unique.
				pHandlerId->SetTimeStamp(netTime);
			}
			else
			{
				// set the timestamp received from the host
				gnetAssert(m_PendingTimestamp != 0);
				pHandlerId->SetTimeStamp(m_PendingTimestamp);
				m_PendingTimestamp = 0;
			}
		}
	}

	scriptHandlerNetComponent::SetNewScriptHost(pPlayer, hostToken, bBroadcast);

	if (GetHost() && m_State >= NETSCRIPT_INTERNAL_READY_TO_PLAY && m_State < NETSCRIPT_INTERNAL_LEAVE)
	{
		if (m_State == NETSCRIPT_INTERNAL_PLAYING && m_ParentHandler.GetThread())
		{
			CEventNetworkHostMigration migrateEvent(m_ParentHandler.GetThread()->GetThreadId(), *static_cast<const CNetGamePlayer*>(pPlayer));
			GetEventScriptNetworkGroup()->Add(migrateEvent);
		}

		if (gnetVerify(m_HostData))
		{
			CTheScripts::GetScriptHandlerMgr().SetNewHostOfRemoteScript(m_ParentHandler.GetScriptId(), *pPlayer, hostToken, GetParticipantFlags());
		}

		if (IsHostLocal())
		{
			// inform other non-participant players that we are the new host
			m_bBroadcastScriptInfo = true; 

			// remove any script objects created by the previous host after we got the last host broadcast data update
			static_cast<CGameScriptHandlerNetwork&>(m_ParentHandler).RemoveOldHostObjects();
		}
	}
}

bool CGameScriptHandlerNetComponent::CanStartJoining()
{
	// wait for the script to finish its setup
	if (!m_bScriptReady)
	{
		scriptInterface::GetScriptManager().WriteScriptHeader(scriptInterface::GetScriptManager().GetLog(), m_ParentHandler.GetScriptId(), "Can't start joining - waiting on script");
		return false;
	}

	if (!NetworkInterface::IsGameInProgress())
	{
		scriptInterface::GetScriptManager().WriteScriptHeader(scriptInterface::GetScriptManager().GetLog(), m_ParentHandler.GetScriptId(), "Can't start joining - network game not in progress");
		return false;
	}

	if (!NetworkInterface::IsSessionEstablished())
	{
		scriptInterface::GetScriptManager().WriteScriptHeader(scriptInterface::GetScriptManager().GetLog(), m_ParentHandler.GetScriptId(), "Can't start joining - the session has not started");
		return false;
	}

	// we need to wait until we are accepted into a roaming bubble before proceeding
	if (!NetworkInterface::GetLocalPlayer()->IsInRoamingBubble())
	{
		scriptInterface::GetScriptManager().WriteScriptHeader(scriptInterface::GetScriptManager().GetLog(), m_ParentHandler.GetScriptId(), "Can't start joining - waiting on roaming bubble");
		return false;
	}

	return scriptHandlerNetComponent::CanStartJoining();
}

void CGameScriptHandlerNetComponent::RestartJoiningProcess()
{
	// we have to send out a leave message here if we have been accepted by the previous host, as he may have sent out a remote
	// script info broadcast with a different timestamp on it and the other machines must know we have left so they know to move the
	// remote script info onto the terminated list
	if (IsPlayerAParticipant(*NetworkInterface::GetLocalPlayer()))
	{	
		PlayerFlags participants = GetParticipantFlags();

		CGameScriptId scrId(static_cast<CGameScriptId&>(m_ParentHandler.GetScriptId()));

		if (scrId.GetTimeStamp() == 0)
		{
			scrId.SetTimeStamp(m_PendingTimestamp);
		}

		// send a leave msg to all non-participants
		CRemoteScriptLeaveEvent::Trigger(scrId, ~participants);
	}

	static_cast<CGameScriptId&>(m_ParentHandler.GetScriptId()).ResetTimestamp();

	scriptHandlerNetComponent::RestartJoiningProcess();

	m_PendingTimestamp = 0;
	m_JoinEventFlags = 0;
}

void CGameScriptHandlerNetComponent::StartPlaying()
{
	CGameScriptHandlerNetwork& gameHandler = static_cast<CGameScriptHandlerNetwork&>(m_ParentHandler);

	scriptHandlerNetComponent::StartPlaying();

	if (m_ParentHandler.GetThread())
	{
		static_cast<GtaThread*>(m_ParentHandler.GetThread())->m_NetComponent = this;
	}

	gameHandler.RegisterAllExistingScriptObjects();

	gameHandler.NetworkGameStarted();

	// we should have been given a timestamp by the host at this point
	gnetAssert(static_cast<CGameScriptId&>(m_ParentHandler.GetScriptId()).GetTimeStamp() != 0);

	// move a currently active script matching this one (but with an older timestamp) onto the terminated queue (there may be one if we received a remote script info from a 
	// previous host who left during the joining process)
	CTheScripts::GetScriptHandlerMgr().TerminateActiveScriptInfo(static_cast<CGameScriptId&>(gameHandler.GetScriptId()));

	CGameScriptHandlerNetwork& parentHandler = static_cast<CGameScriptHandlerNetwork&>(m_ParentHandler);

	CGameScriptHandlerMgr::CRemoteScriptInfo scriptInfo(static_cast<CGameScriptId&>(parentHandler.GetScriptId()), 
														GetParticipantFlags(), 
														m_HostToken, 
														GetHost(), 
														parentHandler.GetNumGlobalReservedPeds(), 
														parentHandler.GetNumGlobalReservedVehicles(), 
														parentHandler.GetNumGlobalReservedObjects());

	scriptInfo.SetRunningLocally(true);

	CTheScripts::GetScriptHandlerMgr().AddRemoteScriptInfo(scriptInfo);

	// if we are the host, inform other non-participant players about the new script session
	if (IsHostLocal())
	{
		m_bBroadcastScriptInfo = true;
	}

	m_bDoneLeaveCleanup = false;
	m_bDoneTerminationCleanup = false;
	m_bStartedPlaying = true;
}

bool CGameScriptHandlerNetComponent::DoLeaveCleanup()
{
	// DoLeaveCleanup may get called multiple times, hence m_bDoneLeaveCleanup flag
	if (NetworkInterface::IsGameInProgress())
	{
		if (!m_bDoneLeaveCleanup)
		{
			scriptInterface::GetScriptManager().WriteScriptHeader(scriptInterface::GetScriptManager().GetLog(), m_ParentHandler.GetScriptId(), "Mission Finished: %s", m_ScriptFinished ? "true" : "false");
			
			if (IsHostLocal())
			{
				if (m_ScriptFinished || (m_NumParticipants == 1 && m_State > NETSCRIPT_INTERNAL_PLAYING))
				{
					// update our remote script info with 0 participants to indicate that it is finished
					CGameScriptHandlerMgr::CRemoteScriptInfo scriptInfo(static_cast<CGameScriptId&>(m_ParentHandler.GetScriptId()), 0, m_HostToken, GetHost(), 0, 0, 0);
					CTheScripts::GetScriptHandlerMgr().AddRemoteScriptInfo(scriptInfo);
				}

				// inform all non-participants that the script has terminated
				m_bBroadcastScriptInfo = true;
			}

			// remove all script peds from our local player's group
			CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();
			CPedGroup* pPlayerGroup = pLocalPlayer ? pLocalPlayer->GetPedsGroup() : NULL;

			if (pPlayerGroup)
			{
				CPedGroupMembership* pMembership = pPlayerGroup->GetGroupMembership();

				if (pMembership->CountMembers() > 1)
				{
					for (int i=0; i<CPedGroupMembership::MAX_NUM_MEMBERS; i++)
					{
						CPed* pMember = pMembership->GetMember(i);

						if (pMember && !pMembership->IsLeader(pMember) && AssertVerify(pMember != pLocalPlayer))
						{			
							CScriptEntityExtension* pExtension = pMember->GetExtension<CScriptEntityExtension>();

							if (pExtension && pExtension->GetScriptInfo() && pExtension->GetScriptInfo()->GetScriptId() == m_ParentHandler.GetScriptId())
							{
								scriptInterface::GetScriptManager().WriteScriptHeader(scriptInterface::GetScriptManager().GetLog(), m_ParentHandler.GetScriptId(), "Removing %s from local player's group", pMember->GetNetworkObject() ? pMember->GetNetworkObject()->GetLogName() : "??");
								pMembership->RemoveMember(pMember);
							}
						}
					}
				}
			}

			m_bDoneLeaveCleanup = true;
		}
	}

	return scriptHandlerNetComponent::DoLeaveCleanup();
}

bool CGameScriptHandlerNetComponent::DoTerminationCleanup()
{
	// DoTerminationCleanup may get called multiple times, hence m_bDoneTerminationCleanup flag
	if (NetworkInterface::IsGameInProgress())
	{
		if (!m_bDoneTerminationCleanup)
		{
			scriptInterface::GetScriptManager().WriteScriptHeader(scriptInterface::GetScriptManager().GetLog(), m_ParentHandler.GetScriptId(), "Mission Finished: %s", m_ScriptFinished ? "true" : "false");

			// send a leave msg to all non-participants
			if (IsPlayerAParticipant(*NetworkInterface::GetLocalPlayer()))
			{
				PlayerFlags participants = GetParticipantFlags();

				CTheScripts::GetScriptHandlerMgr().WriteScriptHeader(CTheScripts::GetScriptHandlerMgr().GetLog(), m_ParentHandler.GetScriptId(), "Broadcasting remote script leave event. Players: %u", ~participants);

				CGameScriptId scrId(static_cast<CGameScriptId&>(m_ParentHandler.GetScriptId()));

				if (scrId.GetTimeStamp() == 0)
				{
					scrId.SetTimeStamp(m_PendingTimestamp);
				}

				CRemoteScriptLeaveEvent::Trigger(scrId, ~participants);

				if (m_State >= NETSCRIPT_INTERNAL_PLAYING && m_bStartedPlaying)
				{
					CTheScripts::GetScriptHandlerMgr().RemovePlayerFromRemoteScript(m_ParentHandler.GetScriptId(), *NetworkInterface::GetLocalPlayer(), true);
				}
			}

#if __BANK
			if (GetNumHostBroadcastDatasRegistered() || GetNumPlayerBroadcastDatasRegistered())
			{
				netLoggingInterface* log = scriptInterface::GetScriptManager().GetLog();

				if (log)
				{
					NetworkLogUtils::WriteLogEvent(*log, "BANDWIDTH_STATS", "%s", m_ParentHandler.GetScriptId().GetLogName());
					log->WriteDataValue("Total in", "%d", GetTotalBandwidthIn());
					log->WriteDataValue("Total out", "%d", GetTotalBandwidthOut());
					log->WriteDataValue("Average in", "%d", GetAverageBandwidthIn());
					log->WriteDataValue("Average out", "%d", GetAverageBandwidthOut());
					log->WriteDataValue("Peak in", "%d", GetPeakBandwidthIn());
					log->WriteDataValue("Peak out", "%d", GetPeakBandwidthOut());
				}

				char dvName[100];

				for (int i=0; i<GetNumHostBroadcastDatasRegistered(); i++)
				{
					netScriptBroadcastDataHandlerBase *hostHandler = GetHostBroadcastDataHandler(i);

					if (gnetVerify(hostHandler))
					{	
						formatf(dvName, "Host array handler %d", i);
						log->WriteDataValue(dvName, "Total out: %d, Avg out: %d, Peak out: %d", hostHandler->GetTotalBandwidthOut(), hostHandler->GetAverageBandwidthOut(GetTimeHandlerActive()), hostHandler->GetPeakBandwidthOut());
					}
				}

				for (int i=0; i<GetNumPlayerBroadcastDatasRegistered(); i++)
				{
					netScriptBroadcastDataHandlerBase *playerHandler = GetLocalPlayerBroadcastDataHandler(i);

					if (gnetVerify(playerHandler))
					{	
						formatf(dvName, "Player array handler %d", i);
						log->WriteDataValue(dvName, "Total out: %d, Avg out: %d, Peak out: %d", playerHandler->GetTotalBandwidthOut(), playerHandler->GetAverageBandwidthOut(GetTimeHandlerActive()), playerHandler->GetPeakBandwidthOut());
					}
				}
			}
#endif // __BANK

			m_bDoneTerminationCleanup = true;
		}

		// we must wait until all important objects have migrated before we can terminate the handler
		if (!m_ScriptFinished && static_cast<CGameScriptHandlerNetwork&>(m_ParentHandler).MigrateObjects())
		{
			return false;
		}
	}

	return scriptHandlerNetComponent::DoTerminationCleanup();
}

bool CGameScriptHandlerNetComponent::BroadcastRemoteScriptInfo(const netPlayer* pPlayer)
{
	if (NetworkInterface::IsGameInProgress() && m_State >= NETSCRIPT_INTERNAL_READY_TO_PLAY)
	{
		// TODO: add for bots? Ignore for now
		if ((!pPlayer || !pPlayer->IsBot()) &&
			gnetVerify(IsHostLocal()) && 
			gnetVerify(static_cast<CGameScriptId&>(m_ParentHandler.GetScriptId()).GetTimeStamp() != 0))
		{
			PlayerFlags participants = GetParticipantFlags();
			
			// send out 0 participants if the script is finishing or we are leaving and are the only machine running the script. 
			// This informs everyone that the script has definitely terminated.
			if (m_ScriptFinished || (m_NumParticipants == 1 && m_State > NETSCRIPT_INTERNAL_PLAYING))
				participants = 0;

			// remove our player if we are leaving the script
			if (m_State > NETSCRIPT_INTERNAL_PLAYING)
				participants &= ~(1<<NetworkInterface::GetLocalPhysicalPlayerIndex());

			CTheScripts::GetScriptHandlerMgr().WriteScriptHeader(CTheScripts::GetScriptHandlerMgr().GetLog(), m_ParentHandler.GetScriptId(), "Broadcast script info. Participants: %d. Our host token: %d", participants, GetHostToken());

			// send to all non-participants unless we want to send to a specific player 
			PlayerFlags playersToSendTo = 0;
	
			if (pPlayer)
			{
				if (AssertVerify(!pPlayer->IsLocal()))
				{
					playersToSendTo = (1<<pPlayer->GetPhysicalPlayerIndex());
				}
			}
			else
			{
				playersToSendTo = ~participants & NetworkInterface::GetPlayerMgr().GetRemotePhysicalPlayersBitmask();
				playersToSendTo &= ~(1<<NetworkInterface::GetLocalPhysicalPlayerIndex());
			}
	
			if (playersToSendTo != 0)
			{
				CGameScriptHandlerNetwork& parentHandler = static_cast<CGameScriptHandlerNetwork&>(m_ParentHandler);

				CGameScriptHandlerMgr::CRemoteScriptInfo scriptInfo(static_cast<CGameScriptId&>(parentHandler.GetScriptId()), 
																	participants, 
																	m_HostToken, 
																	GetHost(), 
																	parentHandler.GetNumGlobalReservedPeds(), 
																	parentHandler.GetNumGlobalReservedVehicles(), 
																	parentHandler.GetNumGlobalReservedObjects());
				
				if (!CRemoteScriptInfoEvent::Trigger(scriptInfo, playersToSendTo))
				{
					return false;
				}
			}
		}
	}

	return true;
}

