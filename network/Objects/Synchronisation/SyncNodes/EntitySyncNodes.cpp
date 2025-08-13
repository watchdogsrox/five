//
// name:        EntitySyncNodes.cpp
// description: Network sync nodes used by CNetObjEntities
// written by:    John Gurney
//

#include "network/network.h"
#include "network/objects/synchronisation/SyncTrees/ProjectSyncTrees.h"
#include "network/objects/synchronisation/syncnodes/EntitySyncNodes.h"
#include "peds/ped.h"
#include "script/script.h"

NETWORK_OPTIMISATIONS()

DATA_ACCESSOR_ID_IMPL(IEntityNodeDataAccessor);

#if __ASSERT
bool CEntityScriptInfoDataNode::IsReadyToSendToPlayer(netSyncTreeTargetObject* pObj, const PhysicalPlayerIndex player, SerialiseModeFlags serMode, ActivationFlags actFlags, const unsigned currTime ) const
{
	bool bReadyToSend = CSyncDataNodeInfrequent::IsReadyToSendToPlayer(pObj, player, serMode, actFlags, currTime);

	if (!bReadyToSend && serMode == SERIALISEMODE_CREATE)
	{
		netObject* pNetObj = SafeCast(netObject, pObj);

		if (pNetObj->IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_SCRIPTOBJECT))
		{
			gnetAssertf(0, "A create message for %s is being sent without any script info!", pObj->GetLogName());
		}
	}

	return bReadyToSend;
}
#endif // __ASSERT

bool CEntityScriptInfoDataNode::CanApplyData(netSyncTreeTargetObject* pObj) const
{
	if (m_hasScriptInfo && !NetworkInterface::GetObjectManager().CanAcceptScriptObject(*static_cast<CNetObjGame*>(pObj), m_scriptInfo))
	{
		return false;
	}

	return true;
}

void CEntityScriptInfoDataNode::Serialise(CSyncDataBase& serialiser)
{
	SERIALISE_BOOL(serialiser, m_hasScriptInfo, "Has script info");

	if (m_hasScriptInfo || serialiser.GetIsMaximumSizeSerialiser())
	{
		m_scriptInfo.Serialise(serialiser);
		if (serialiser.GetLog()) serialiser.GetLog()->WriteDataValue("Script", "%s", m_scriptInfo.GetScriptId().GetLogName());
	}
}

bool CEntityScriptInfoDataNode::HasDefaultState() const 
{ 
	return !m_hasScriptInfo; 
}

void CEntityScriptInfoDataNode::SetDefaultState() 
{ 
	m_hasScriptInfo = false; 
}

void CEntityScriptGameStateDataNode::Serialise(CSyncDataBase& serialiser)
{
	SERIALISE_BOOL(serialiser, m_usesCollision, "Uses Collision");
	SERIALISE_BOOL(serialiser, m_isFixed, "Fixed");
	SERIALISE_BOOL(serialiser, m_disableCollisionCompletely, "Disable Collision Completely");
}

bool CEntityScriptGameStateDataNode::HasDefaultState() const 
{ 
	return !m_isFixed && m_usesCollision && !m_disableCollisionCompletely; 
}

void CEntityScriptGameStateDataNode::SetDefaultState() 
{ 
	m_isFixed = false; 
	m_usesCollision = true;
	m_disableCollisionCompletely = false;
}

PlayerFlags CEntityOrientationDataNode::StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags UNUSED_PARAM(serMode)) const
{
	if (GetIsInCutscene(pObj))
	{
		return ~0u;
	}

	PlayerFlags playerFlags = GetIsAttachedForPlayers(pObj, false);

	return playerFlags;
}

void CEntityOrientationDataNode::Serialise(CSyncDataBase& serialiser)
{
	SERIALISE_ORIENTATION(serialiser, m_orientation, "Eulers");
}




