//
// name:        BaseSyncNodes.cpp
// description: Project base network sync node classes
// written by:    John Gurney
//
#include "Network/Debug/NetworkDebug.h"
#include "Network/Objects/NetworkObjectMgr.h"
#include "network/objects/synchronisation/syncnodes/BaseSyncNodes.h"
#include "network/objects/synchronisation/SyncTrees/ProjectSyncTrees.h"
#include "network/players/NetGamePlayer.h"
#include "peds/ped.h"
#include "scene/physical.h"

#if __BANK
#include "grcore/debugdraw.h"
#endif

NETWORK_OPTIMISATIONS()

u32 CProjectBaseSyncDataNode::ms_BaseResendFrequencyForPlayer[MAX_NUM_PHYSICAL_PLAYERS];

///////////////////////////////////////////////////////////////////////////////////////
// CProjectBaseSyncParentNode
//
// Base class for all parent nodes used in this project. Contains a node name for
// debugging
///////////////////////////////////////////////////////////////////////////////////////

bool CProjectBaseSyncParentNode::Write(SerialiseModeFlags serMode, ActivationFlags actFlags, netSyncTreeTargetObject* pObj, datBitBuffer& bitBuffer, 
									   const unsigned currTime, netLoggingInterface* pLog, const PhysicalPlayerIndex player, DataNodeFlags* pNodeFlags, unsigned &maxNumHeaderBitsRemaining)
{
	if (netSyncParentNode::Write(serMode, actFlags, pObj, bitBuffer, currTime, pLog, player, pNodeFlags, maxNumHeaderBitsRemaining))
	{
#if __BANK
		if (serMode == SERIALISEMODE_UPDATE)
		{
			// count the guard flag
			NetworkInterface::GetObjectManager().GetBandwidthStatistics().AddBandwidthOut(GetGuardFlagBandwidthRecorderID(), 1);
		}
#endif // __BANK

		return true;
	}

	return false;
}

bool CProjectBaseSyncParentNode::Read(SerialiseModeFlags serMode, ActivationFlags actFlags, datBitBuffer& bitBuffer, netLoggingInterface* pLog)
{
	if (netSyncParentNode::Read(serMode, actFlags, bitBuffer, pLog))
	{
#if __BANK
		if (serMode == SERIALISEMODE_UPDATE)
		{
			// count the guard flag
			NetworkInterface::GetObjectManager().GetBandwidthStatistics().AddBandwidthIn(GetGuardFlagBandwidthRecorderID(), 1);
		}
#endif // __BANK

		return true;
	}

	return false;
}

#if __BANK

RecorderId CProjectBaseSyncParentNode::GetGuardFlagBandwidthRecorderID()
{
    RecorderId recorderID = NetworkInterface::GetObjectManager().GetBandwidthStatistics().GetBandwidthRecorder("Node Guard Flags");

    if(recorderID == INVALID_RECORDER_ID)
    {
        recorderID = NetworkInterface::GetObjectManager().GetBandwidthStatistics().AllocateBandwidthRecorder("Node Guard Flags");
    }

    return recorderID;
}

#endif // __BANK

///////////////////////////////////////////////////////////////////////////////////////
// CProjectBaseSyncDataNode
//
// Base class for all nodes used in this project. Holds update level data and provides
// some member functions used by more than one derived class.
///////////////////////////////////////////////////////////////////////////////////////

//PURPOSE
//	Writes the node data to a bit buffer
bool CProjectBaseSyncDataNode::Write(SerialiseModeFlags serMode, ActivationFlags actFlags, netSyncTreeTargetObject* pObj, datBitBuffer& bitBuffer, 
									 const unsigned currTime, netLoggingInterface* pLog, const PhysicalPlayerIndex player, DataNodeFlags* pNodeFlags, unsigned &maxNumHeaderBitsRemaining)
{
	BANK_ONLY(u32 initCursorPos = bitBuffer.GetCursorPos());

	if (netSyncDataNode::Write(serMode, actFlags, pObj, bitBuffer, currTime, pLog, player, pNodeFlags, maxNumHeaderBitsRemaining))
	{
#if __BANK
		if (serMode == SERIALISEMODE_UPDATE)
		{
			gnetAssertf(HasBandwidthRecorder(), "Warning: node %s has no bandwidth recorder", GetNodeName());

			if (HasBandwidthRecorder())
			{
				NetworkInterface::GetObjectManager().GetBandwidthStatistics().AddBandwidthOut(GetBandwidthRecorderID(), bitBuffer.GetCursorPos()-initCursorPos);
			}
		}
#endif // __BANK
		return true;
	}

	return false;
}

//PURPOSE
//	Reads the node data from a bit buffer
bool CProjectBaseSyncDataNode::Read(SerialiseModeFlags serMode, ActivationFlags actFlags, datBitBuffer& bitBuffer, netLoggingInterface* pLog)
{
	BANK_ONLY(u32 initCursorPos = bitBuffer.GetCursorPos());

	if (netSyncDataNode::Read(serMode, actFlags, bitBuffer, pLog))
	{
#if __BANK
		if (serMode == SERIALISEMODE_UPDATE)
		{
			gnetAssertf(HasBandwidthRecorder(), "Warning: node %s has no bandwidth recorder", GetNodeName());

			if (HasBandwidthRecorder())
			{
				NetworkInterface::GetObjectManager().GetBandwidthStatistics().AddBandwidthIn(GetBandwidthRecorderID(), bitBuffer.GetCursorPos()-initCursorPos);
			}
		}
#endif // __BANK

		return true;
	}

	return false;
}


void CProjectBaseSyncDataNode::WriteData(netSyncTreeTargetObject* pObj, datBitBuffer& bitBuffer, netLoggingInterface* pLog, bool extractFromObject) 
{ 
#if __BANK
	if (IsSelectedInBank())
	{
		NETWORK_DEBUG_BREAK_FOR_FOCUS(BREAK_TYPE_FOCUS_WRITE_SYNC_NODE, static_cast<netObject&>(*pObj));
	}
#endif

	m_dataOperations->WriteData(pObj, bitBuffer, pLog, extractFromObject); 
}

//PURPOSE
//	Applies the data held in the node to the target object
void CProjectBaseSyncDataNode::ApplyData(netSyncTreeTargetObject* pObj) 
{ 
#if __BANK
	if (IsSelectedInBank())
	{
		NETWORK_DEBUG_BREAK_FOR_FOCUS(BREAK_TYPE_FOCUS_READ_SYNC_NODE, static_cast<netObject&>(*pObj));
	}
#endif	
	
	m_dataOperations->ApplyData(pObj); 
}



//PURPOSE
//	Returns true if the data represented by the node is ready to be sent to the given player, either because it is dirty or unacked.
//	Can be overidden with different rules.
//PARAMS
//	pObj		- the object using the tree
//	player		- the player we are sending to
//  serMode		- the serialise mode (eg update, create, migrate)
//  currTime	- the current sync time
bool CProjectBaseSyncDataNode::IsReadyToSendToPlayer(netSyncTreeTargetObject* pObj, const PhysicalPlayerIndex player, SerialiseModeFlags serMode, ActivationFlags actFlags, const unsigned currTime) const
{
#if __BANK
	if (IsSelectedInBank())
	{
		NETWORK_DEBUG_BREAK_FOR_FOCUS(BREAK_TYPE_FOCUS_SYNC_NODE_READY, static_cast<netObject&>(*pObj));
	}
#endif

	if (serMode != SERIALISEMODE_UPDATE)
	{
		PlayerFlags ignorePlayers = StopSend(pObj, serMode);
	
		if (ignorePlayers & (1<<player))
		{
			return false;
		}
	}

	if (GetNodeDependency() && GetNodeDependency()->IsReadyToSendToPlayer(pObj, player, serMode, actFlags, currTime))
	{
        PlayerFlags ignorePlayers = StopSend(pObj, serMode);
	
		if((ignorePlayers & (1<<player)) == 0)
		{
			return true;
		}
	}

	switch (serMode)
	{
	case SERIALISEMODE_CRITICAL:
	case SERIALISEMODE_CHECKSUM:
	case SERIALISEMODE_VERIFY:
	{
		return true;
	}
	case SERIALISEMODE_CREATE:
	{
		if (GetIsSyncUpdateNode())
		{
			if (pObj->HasGameObject())
			{
				if (IsAlwaysSentWithCreate())
				{
					return true;
				}
				else if (pObj->GetSyncData())
				{	
					return pObj->GetSyncData()->HasSyncDataUnitEverBeenDirtied(GetDataIndex());
				}
				else
				{
					return false;
				}
			}
			else 
			{
				return false;
			}
		}
		else
		{
			return true;
		}
	}
	case SERIALISEMODE_UPDATE:
	{
		if (pObj->HasGameObject() || CanSendWithNoGameObject())
		{
			return IsTimeToSendUpdateToPlayer(pObj, player, currTime);
		}
		else
		{
			return false;
		}
	}
	case SERIALISEMODE_MIGRATE:

		if (IsAlwaysSentWithMigrate())
		{
			return true;
		}
		// deliberate fall though
	case SERIALISEMODE_FORCE_SEND_OF_DIRTY:
	{
		if (GetIsSyncUpdateNode() && m_pCurrentSyncDataUnit && (pObj->HasGameObject() || CanSendWithNoGameObject()))
		{
			return !m_pCurrentSyncDataUnit->IsSyncedWithPlayer(player);
		}

		return false;
	}
	default:
		gnetAssert(0);
	}

	return false;
}

//PURPOSE
// Returns true if the data represented by the node is ready to be sent to the given player, either because it is dirty or unacked.
bool CProjectBaseSyncDataNode::IsTimeToSendUpdateToPlayer(netSyncTreeTargetObject* UNUSED_PARAM(pObj), const PhysicalPlayerIndex player, const unsigned UNUSED_PARAM(currTime)) const
{
	bool bTimeForUpdate = m_pCurrentSyncDataUnit ? m_pCurrentSyncDataUnit->GetPlayerRequiresAnUpdate(player) : false;
	return bTimeForUpdate;
}

PlayerFlags CProjectBaseSyncDataNode::GetIsAttachedForPlayers(const netSyncTreeTargetObject* pObj, bool includeTrailers) const
{
	PlayerFlags playerFlags = 0;

	if (AssertVerify(pObj))
	{
		netObject* pTargetObj = (netObject*)(pObj);

		// returns true if the entity is attached to an object that exists on the given player's machine
		if (pTargetObj->GetEntity() && pTargetObj->GetEntity()->GetIsPhysical())
		{
			CPhysical       *pPhysical       = static_cast<CPhysical *>(pTargetObj->GetEntity());
            CNetObjPhysical *pNetObjPhysical = SafeCast(CNetObjPhysical, pTargetObj);

			if (pNetObjPhysical->IsAttachedOrDummyAttached())
			{
                bool isTrailer = pPhysical->GetIsTypeVehicle() && static_cast<CVehicle *>(pPhysical)->InheritsFromTrailer();
				bool isParachute = pPhysical->GetIsTypeObject() && static_cast<CObject *>(pPhysical)->GetIsParachute();

                if(!isParachute && (!isTrailer || includeTrailers))
                {
				    CPhysical *pAttachParent    = pNetObjPhysical->GetAttachOrDummyAttachParent();
                    netObject *pAttachParentObj = pAttachParent ? pAttachParent->GetNetworkObject() : 0;

				    if (pAttachParentObj)
				    {
					    bool bAttachEntityIsClone = pAttachParentObj->IsClone();

					    CPed *pTargetPed = NetworkUtils::GetPedFromNetworkObject(pTargetObj);
					    CVehicle* pTargetPedsVehicle = (pTargetPed && pTargetPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle )) ? pTargetPed->GetMyVehicle() : NULL;

					    unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
                        const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

	                    for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
                        {
		                    const CNetGamePlayer *player = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);

						    CPed* pPlayerPed = player->GetPlayerPed();
						    PlayerFlags playerFlag = (1<<player->GetPhysicalPlayerIndex());

						    if (pPlayerPed)
						    {
							    if (bAttachEntityIsClone)
							    {
								    if (pAttachParentObj->GetPlayerOwner() == player)
								    {
									    playerFlags |= playerFlag;
								    }
							    }
							    else if (pAttachParentObj->HasBeenCloned(*player))
							    {
								    playerFlags |= playerFlag;
							    }

							    // also return true for other passengers in your vehicle (vehicle may be owned by a third party and above check will fail)
							    if (pTargetPedsVehicle && !(playerFlags & playerFlag))
							    {
								    if (pPlayerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && 
									    pPlayerPed->GetMyVehicle() == pTargetPedsVehicle)
								    {
									    playerFlags |= playerFlag;
								    }
							    }
						    }
					    }
				    }
                }
			}
		}
	}

	return playerFlags;
}

bool CProjectBaseSyncDataNode::GetIsInInterior(const netSyncTreeTargetObject* pObj) const
{
	bool isInInterior = false;

	if (gnetVerify(pObj))
	{
		netObject* targetObj = (netObject*)(pObj);

		CNetObjPhysical* netobjPhysical = SafeCast(CNetObjPhysical, targetObj);

		if (gnetVerify(netobjPhysical))
		{
			isInInterior = netobjPhysical->IsInInterior();
		}
	}

	return isInInterior;
}

bool CProjectBaseSyncDataNode::GetIsInCutscene(const netSyncTreeTargetObject* pObj) const
{
	bool isInCutscene = false;

	if (gnetVerify(pObj))
	{
		netObject* targetObj = (netObject*)(pObj);

		CNetObjPhysical* netobjPhysical = SafeCast(CNetObjPhysical, targetObj);

		if (gnetVerify(netobjPhysical))
		{
			isInCutscene = netobjPhysical->IsInCutsceneLocally();
		}
	}

	return isInCutscene;
}

#if __BANK
void CProjectBaseSyncDataNode::DisplayNodeInformation(netSyncTreeTargetObject* pObj, netLogDisplay& displayLog)
{
	static Color32 nameColor(255, 255, 0);
	static Color32 dataColor(255, 200, 200);

	if (IsSelectedInBank())
	{
		const Color32 currColor = displayLog.GetDisplayColor();
		u8 alpha = currColor.GetAlpha();

		nameColor.SetAlpha(alpha);
		dataColor.SetAlpha(alpha);

		displayLog.SetDisplayOffset(0);
		displayLog.ChangeDisplayColor(nameColor);
		displayLog.Log(GetNodeName());

		displayLog.SetDisplayOffset(15);
		displayLog.ChangeDisplayColor(dataColor);
		m_dataOperations->DisplayData(pObj, displayLog);
	}
}
#endif
///////////////////////////////////////////////////////////////////////////////////////
// CSyncDataNodeFrequent
//
// A CProjectBaseSyncDataNode that sets up frequent update frequencies
///////////////////////////////////////////////////////////////////////////////////////
void CSyncDataNodeFrequent::SetUpdateRates()
{
	m_NodeUpdateLevels[CNetworkSyncDataULBase::UPDATE_LEVEL_VERY_HIGH] = CNetworkSyncDataULBase::UPDATE_LEVEL_VERY_HIGH;
	m_NodeUpdateLevels[CNetworkSyncDataULBase::UPDATE_LEVEL_HIGH]      = CNetworkSyncDataULBase::UPDATE_LEVEL_HIGH;
	m_NodeUpdateLevels[CNetworkSyncDataULBase::UPDATE_LEVEL_MEDIUM]    = CNetworkSyncDataULBase::UPDATE_LEVEL_MEDIUM;
	m_NodeUpdateLevels[CNetworkSyncDataULBase::UPDATE_LEVEL_LOW]       = CNetworkSyncDataULBase::UPDATE_LEVEL_LOW;
	m_NodeUpdateLevels[CNetworkSyncDataULBase::UPDATE_LEVEL_VERY_LOW]  = CNetworkSyncDataULBase::UPDATE_LEVEL_VERY_LOW;
}

///////////////////////////////////////////////////////////////////////////////////////
// CSyncDataNodeInfrequent
//
// A CProjectBaseSyncDataNode that sets up infrequent update frequencies
///////////////////////////////////////////////////////////////////////////////////////
void CSyncDataNodeInfrequent::SetUpdateRates()
{
	m_NodeUpdateLevels[CNetworkSyncDataULBase::UPDATE_LEVEL_VERY_HIGH]	= CNetworkSyncDataULBase::UPDATE_LEVEL_LOW;
	m_NodeUpdateLevels[CNetworkSyncDataULBase::UPDATE_LEVEL_HIGH]		= CNetworkSyncDataULBase::UPDATE_LEVEL_LOW;
	m_NodeUpdateLevels[CNetworkSyncDataULBase::UPDATE_LEVEL_MEDIUM]	    = CNetworkSyncDataULBase::UPDATE_LEVEL_LOW;
	m_NodeUpdateLevels[CNetworkSyncDataULBase::UPDATE_LEVEL_LOW]		= CNetworkSyncDataULBase::UPDATE_LEVEL_LOW;
	m_NodeUpdateLevels[CNetworkSyncDataULBase::UPDATE_LEVEL_VERY_LOW]	= CNetworkSyncDataULBase::UPDATE_LEVEL_VERY_LOW;
}

void WriteNodeHeader(netLoggingInterface &log, const char *nodeName)
{
    log.WriteNodeHeader(nodeName);
}
