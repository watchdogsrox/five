//
// name:        PickupPlacementSyncNodes.cpp
// description: Network sync nodes used by CNetObjPickupPlacements
// written by:    John Gurney
//

#include "network/objects/synchronisation/syncnodes/PickupPlacementSyncNodes.h"

#include "fwnet/netlog.h"

#include "network/network.h"
#include "Network/Objects/NetworkObjectMgr.h"
#include "pickups/Pickup.h"
#include "pickups/PickupManager.h"
#include "pickups/PickupPlacement.h"
#include "script/handlers/GameScriptHandler.h"
#include "script/handlers/GameScriptMgr.h"
#include "script/script.h"

NETWORK_OPTIMISATIONS()

DATA_ACCESSOR_ID_IMPL(IPickupPlacementNodeDataAccessor);

bool CPickupPlacementCreationDataNode::CanApplyData(netSyncTreeTargetObject* pObj) const
{
	netObject* pNetObj = SafeCast(netObject, pObj);

	bool bSuccess = true;

#if __BANK
	if (!m_mapPlacement && (m_placementFlags & CPickupPlacement::PLACEMENT_CREATION_FLAG_DEBUG_CREATED))
	{
		CNetwork::GetMessageLog().Log("\tACCEPT: Debug created\r\n");
		return true;
	}
#endif
	scriptHandler* pHandler = CTheScripts::GetScriptHandlerMgr().GetScriptHandler(m_scriptObjInfo.GetScriptId());

	if (!pHandler)
	{
		CNetwork::GetMessageLog().Log("\tREJECT: No script handler exists\r\n");

		// no corresponding script handler
		bSuccess = false;
	}
	else if (!pHandler->GetThread())
	{
		CNetwork::GetMessageLog().Log("\tREJECT: No script thread exists\r\n");

		// script thread has gone, handler is shutting down
		bSuccess = false;
	}
	
	if (bSuccess && m_mapPlacement)
	{
		// find the map placement for this pickup
		scriptHandlerObject* pScriptObj = pHandler->GetScriptObject(m_scriptObjInfo.GetObjectId());

		if (!pScriptObj)
		{
			CNetwork::GetMessageLog().Log("\tREJECT: No pickup placement exists\r\n");

			// no corresponding pickup
			bSuccess = false;
		}
		else if (pScriptObj->GetType() != SCRIPT_HANDLER_OBJECT_TYPE_PICKUP)
		{
			gnetAssertf(0, "CPickupPlacementCreationDataNode::CanApplyData: %s - the script object is not a pickup (%s)", pNetObj ? pNetObj->GetLogName() : "??", m_scriptObjInfo.GetScriptId().GetLogName());

			CNetwork::GetMessageLog().Log("\tREJECT: The script object with this id is not a pickup\r\n");

			// no corresponding pickup
			bSuccess = false;
		}

		if (bSuccess)
		{
			CPickupPlacement* pPlacement = SafeCast(CPickupPlacement, pScriptObj);

			if (!pPlacement)
			{
				CNetwork::GetMessageLog().Log("\tREJECT: Script id does not correspond to a pickup placement\r\n");
				bSuccess = false;
			}
			else
			{
				// is this placement already registered to another machine?
				if (pPlacement->GetNetworkObject())
				{
					if (pPlacement->GetNetworkObject()->GetPlayerOwner())
					{
						u64 pickupPeerID = pPlacement->GetNetworkObject()->GetPlayerOwner()->GetRlPeerId();

						if (AssertVerify(pNetObj) && AssertVerify(pNetObj->GetPlayerOwner()))
						{
							u64 newPeerId = pNetObj->GetPlayerOwner()->GetRlPeerId();

							// if we have two network objects for this placement, dump the current one if the new peer id is higher
							// (two machines may have registered a placement at the same time)
							// The peer id may be the same, this can happen if a pickup is collected, respawns and is immediately collected again by the same player. The
							// create for the second collection network object may arrive before the remove for the previous one.
							if (pickupPeerID <= newPeerId)
							{
								CNetwork::GetMessageLog().Log("\tACCEPT: Ownership conflict: Current owner loses ownership\r\n");

								CNetwork::GetObjectManager().UnregisterNetworkObject(pPlacement->GetNetworkObject(), CNetworkObjectMgr::OWNERSHIP_CONFLICT, false, false);
								gnetAssert(!pPlacement->GetNetworkObject());
							}
							else
							{
								CNetwork::GetMessageLog().Log("\tREJECT: Ownership conflict: Current owner keeps ownership\r\n");

								// the create fails
								bSuccess = false;
							}
						}
						else
						{
							// the create fails
							bSuccess = false;
						}
					}
					else
					{
						gnetAssert(pPlacement->GetNetworkObject()->IsLocalFlagSet(netObject::LOCALFLAG_BEINGREASSIGNED));
						CNetwork::GetMessageLog().Log("\tREJECT: Placement is on reassign list\r\n");

						// the placement is being reassigned
						bSuccess = false;
					}
				}
			}
		}
	}

	return bSuccess;
}

void CPickupPlacementCreationDataNode::ExtractDataForSerialising(IPickupPlacementNodeDataAccessor &pickupNodeData)
{
	pickupNodeData.GetPickupPlacementCreateData(*this);

	m_teamPermits &= (1<<SIZEOF_TEAM_PERMITS)-1;
}

void CPickupPlacementCreationDataNode::ApplyDataFromSerialising(IPickupPlacementNodeDataAccessor &pickupNodeData)
{
	pickupNodeData.SetPickupPlacementCreateData(*this);
}

const unsigned int CPickupPlacementCreationDataNode::SIZEOF_TEAM_PERMITS = MAX_NUM_TEAMS;

void CPickupPlacementCreationDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned int SIZEOF_PICKUP_ORIENTATION    = 12;
	static const unsigned int SIZEOF_PICKUP_HASH           = 32;
	static const unsigned int SIZEOF_PLACEMENT_FLAGS       = CPickupPlacement::PLACEMENT_FLAG_NUM_CREATION_FLAGS;
	static const unsigned int SIZEOF_AMOUNT				   = 32;
	static const unsigned int SIZEOF_CUSTOM_REGEN_TIME	   = 8;

	SERIALISE_BOOL(serialiser, m_mapPlacement);

	if (!m_mapPlacement || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_POSITION(serialiser, m_pickupPosition, "Pickup pos");
		SERIALISE_VECTOR(serialiser, m_pickupOrientation, (2 * PI), SIZEOF_PICKUP_ORIENTATION, "Pickup orientation");
		SERIALISE_UNSIGNED(serialiser, m_pickupHash, SIZEOF_PICKUP_HASH, "Pickup type");
		SERIALISE_UNSIGNED(serialiser, m_placementFlags, SIZEOF_PLACEMENT_FLAGS, "Placement Flags");

		bool hasAmount = m_amount > 0;

		SERIALISE_BOOL(serialiser, hasAmount);

		if (hasAmount || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_UNSIGNED(serialiser, m_amount, SIZEOF_AMOUNT, "Amount");
		}
		else
		{
			m_amount = 0;
		}

		bool hasCustomModel = m_customModelHash != 0;

		SERIALISE_BOOL(serialiser, hasCustomModel);

		if (hasCustomModel || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_MODELHASH(serialiser, m_customModelHash, "Custom model");
		}
		else
		{
			m_customModelHash = 0;
		}

		bool hasCustomRegenTime = m_customRegenTime != 0;

		SERIALISE_BOOL(serialiser, hasCustomRegenTime);

		if (hasCustomRegenTime || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_UNSIGNED(serialiser, m_customRegenTime, SIZEOF_CUSTOM_REGEN_TIME, "Custom regeneration time");
		}
		else
		{
			m_customRegenTime = 0;
		}

		SERIALISE_UNSIGNED(serialiser, m_teamPermits, SIZEOF_TEAM_PERMITS, "Team permits");

#if __BANK
		if (m_placementFlags & CPickupPlacement::PLACEMENT_CREATION_FLAG_DEBUG_CREATED)
		{
			return;
		}
#endif
	}
	else
	{
		m_placementFlags = 0;
	}

	if (serialiser.GetLog()) serialiser.GetLog()->WriteDataValue("Script", "%s", m_scriptObjInfo.GetScriptId().GetLogName());

	m_scriptObjInfo.Serialise(serialiser);
}

void CPickupPlacementStateDataNode::Serialise(CSyncDataBase& serialiser)
{
	SERIALISE_BOOL(serialiser, m_collected, "Collected");
	SERIALISE_BOOL(serialiser, m_destroyed, "Destroyed");

	if (m_collected || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_OBJECTID(serialiser, m_collector, "Collector");
	}

	if (m_collected || m_destroyed || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_BOOL(serialiser, m_regenerates);

		if (m_regenerates|| serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_UNSIGNED(serialiser, m_regenerationTime, 32, "Regeneration time");
		}
	}
}
