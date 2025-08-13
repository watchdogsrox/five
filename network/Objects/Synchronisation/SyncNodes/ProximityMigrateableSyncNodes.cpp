//
// name:        ProximityMigrateableSyncNodes.cpp
// description: Network sync nodes used by CNetObjProximityMigrateables
// written by:    John Gurney
//

#include "network/objects/synchronisation/syncnodes/ProximityMigrateableSyncNodes.h"
#include "network/objects/synchronisation/synctrees/ProjectSyncTrees.h"
#include "network/objects/Entities/NetObjPhysical.h"

DATA_ACCESSOR_ID_IMPL(IProximityMigrateableNodeDataAccessor);

PlayerFlags CSectorDataNode::StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags UNUSED_PARAM(serMode)) const
{
	if (GetIsInCutscene(pObj))
	{
		return ~0u;
	}

	// returns true if the entity is attached to an object that exists on the given player's machine
	PlayerFlags playerFlags = GetIsAttachedForPlayers(pObj);

	return playerFlags;
}

bool CSectorDataNode::IsReadyToSendToPlayer(netSyncTreeTargetObject* pObj, const PhysicalPlayerIndex player, SerialiseModeFlags serMode, ActivationFlags actFlags, const unsigned currTime) const
{
	bool bReady = CSyncDataNodeFrequent::IsReadyToSendToPlayer(pObj, player, serMode, actFlags, currTime);

	if (!bReady)
	{
        if(static_cast<CProximityMigrateableSyncTreeBase*>(m_ParentTree)->IsTimeToSendPositionUpdateToPlayer(pObj, player, currTime))
		{
			if (m_pCurrentSyncDataUnit && !m_pCurrentSyncDataUnit->IsSyncedWithPlayer(player))
			{
				// always send sector with the position if the sector is unacked
				bReady = true;
			}
		}
	}

	return bReady;
}

PlayerFlags CSectorPositionDataNode::StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags UNUSED_PARAM(serMode)) const
{
	if (GetIsInCutscene(pObj))
	{
		return ~0u;
	}

	PlayerFlags playerFlags = GetIsAttachedForPlayers(pObj);

	return playerFlags;
}

bool CGlobalFlagsDataNode::IsReadyToSendToPlayer(netSyncTreeTargetObject* pObj, const PhysicalPlayerIndex player, SerialiseModeFlags serMode, ActivationFlags actFlags, const unsigned currTime) const
{
	bool bReady = CSyncDataNodeInfrequent::IsReadyToSendToPlayer(pObj, player, serMode, actFlags, currTime);

	// always send the node in a migrate message if it is unacked (the new owner must have the ownership token)
	if (!bReady && serMode == SERIALISEMODE_MIGRATE && m_pCurrentSyncDataUnit && !m_pCurrentSyncDataUnit->IsSyncedWithPlayer(player))
	{
		bReady = true;
	}

	return bReady;
}
