//
// name:		ProjectSyncTree.h
// description: The network sync tree class for this project
// written by:    John Gurney
//

#include "network/Objects/Synchronisation/SyncTrees/ProjectSyncTree.h"
#include "network/Objects/Synchronisation/SyncTrees/ProjectSyncTrees.h"
#include "script/Handlers/GameScriptEntity.h"

NETWORK_OPTIMISATIONS()

u8 CProjectSyncTree::ms_backupBuffer[SIZEOF_BACKUP_BUFFER];

void CProjectSyncTree::ApplyNodeData(netSyncTreeTargetObject* pObj, DataNodeFlags* UNUSED_PARAM(updatedNodes))
{
	if (gnetVerify(pObj))
	{
		gnetAssert(dynamic_cast<CNetObjGame*>(pObj));
		CNetObjGame* pGameObj = static_cast<CNetObjGame*>(pObj);

		bool bScriptObject = pGameObj->IsScriptObject();

        pGameObj->PreSync();

		DataNodeFlags dirtyNodes = 0;

		netSyncTree::ApplyNodeData(pObj, &dirtyNodes);

		pGameObj->SetDirtyNodes(dirtyNodes);

		// detect when the target object goes from being a script to an ambient object and cleanup any script state.
		// this is done here because ApplyNodeData can be called from a variety of places (from a sync or a migrate event)
		// and so it is safer to do it here to make sure this code is always called
		if (m_RootNode && bScriptObject && !pGameObj->IsScriptObject() && gnetVerify(pGameObj->IsClone()))
		{
			pGameObj->PostSync();
			pGameObj->ConvertToAmbientObject();
		}
	}
}

bool CProjectSyncTree::Write(SerialiseModeFlags serMode, ActivationFlags actFlags, netSyncTreeTargetObject* pObj, datBitBuffer& bitBuffer, const unsigned currTime, 
				   netLoggingInterface* pLog, const PhysicalPlayerIndex player, DataNodeFlags* pNodeFlags)
{
	if (serMode == SERIALISEMODE_CREATE)
	{
		DirtyNodesThatDifferFromInitialState(pObj, 0, player);
	}

	return netSyncTree::Write(serMode, actFlags, pObj, bitBuffer, currTime, pLog, player, pNodeFlags);
}


void CProjectSyncTree::ResetScriptState(netSyncTreeTargetObject* pObj)
{
	if (gnetVerify(pObj) && gnetVerify(m_RootNode))
	{
		ClearUpdatedFlags();

		for (u32 i=0; i<m_NumDataNodes; i++)
		{
			SafeCast(CProjectBaseSyncDataNode, m_DataNodes[i])->ResetScriptState();
		}

		ApplyNodeData(pObj);
	}
}

void CProjectSyncTree::InitialiseInitialStateBuffer(netSyncTreeTargetObject* pObj, bool bForce)
{
	if ((!m_bInitialStateBufferInitialised || bForce) && m_sizeOfInitialStateBuffer > 0)
	{
#if ENABLE_NETWORK_LOGGING
		netLoggingInterface& log = NetworkInterface::GetObjectManager().GetLog();
		NetworkLogUtils::WriteLogEvent(log, "INITIALISE_INITIAL_STATE_BUFFER", pObj->GetLogName());
#endif

		bool bTreeBackedUp = false;

		datBitBuffer backupBitBuffer;
		backupBitBuffer.SetReadWriteBits(ms_backupBuffer, SIZEOF_BACKUP_BUFFER<<3, 0);

		// we have to backup the current tree if it already being used by this object. This happens when processing clone create messages
		if (m_TargetObject == pObj)
		{
			for (u32 i=0; i<m_NumSyncUpdateNodes; i++)
			{
				if (m_SyncUpdateNodes[i]->GetWasUpdated())
				{
					m_SyncUpdateNodes[i]->WriteData(pObj, backupBitBuffer, 0, false);
				}
			}	

			bTreeBackedUp = true;
		}

		SetTargetObject(pObj);

		datBitBuffer initialStateBitBuffer;
		initialStateBitBuffer.SetReadWriteBits(m_initialStateBuffer, m_sizeOfInitialStateBuffer<<3, 0);

		for (u32 i=0; i<m_NumSyncUpdateNodes; i++)
		{
			if (!m_SyncUpdateNodes[i]->IsAlwaysSentWithCreate())
			{
				m_SyncUpdateNodes[i]->WriteData(pObj, initialStateBitBuffer, 0);
			}
		}
			
		if (bTreeBackedUp)
		{
			backupBitBuffer.SetCursorPos(0);

			for (u32 i=0; i<m_NumSyncUpdateNodes; i++)
			{
				if (m_SyncUpdateNodes[i]->GetWasUpdated())
				{
					m_SyncUpdateNodes[i]->ReadData(backupBitBuffer, 0);
				}
			}	
		}

		m_bInitialStateBufferInitialised = true;
	}
}

void CProjectSyncTree::DirtyNodesThatDifferFromInitialState(netSyncTreeTargetObject* pObj, ActivationFlags actFlags, PhysicalPlayerIndex player)
{
	if (pObj->HasGameObject() && (m_sizeOfInitialStateBuffer > 0 && gnetVerifyf(m_bInitialStateBufferInitialised, "Initial state buffer not set up for %s", pObj->GetLogName()) && pObj->GetSyncData()))
	{
		SetTargetObject(pObj);

		datBitBuffer* pCurrentStateBuffer = pObj->GetSyncData()->GetCurrentStateBuffer();

		if (pCurrentStateBuffer && gnetVerifyf(pCurrentStateBuffer->GetMaxBits() == (int)(m_sizeOfInitialStateBuffer<<3), "Initial state buffer for %s differs in size from the current state buffer", pObj->GetLogName()))
		{
			const u8 *currentStateData = (const u8 *)pCurrentStateBuffer->GetReadWriteBits();

#if ENABLE_NETWORK_LOGGING
			bool bHeaderLogged = false;
#endif
			for (u32 i=0; i<m_NumSyncUpdateNodes; i++)
			{
				if ((actFlags==0 || (m_SyncUpdateNodes[i]->GetActivationFlags() & actFlags)) && !m_SyncUpdateNodes[i]->IsAlwaysSentWithCreate())
				{
					u32 dataStartByte = BITS_TO_BYTES(m_SyncUpdateNodes[i]->GetDataStart());
					u32 dataSizeBytes = BITS_TO_BYTES(m_SyncUpdateNodes[i]->GetCurrentSyncDataUnit()->GetSizeOfCurrentData());

					if (memcmp(currentStateData + dataStartByte, m_initialStateBuffer + dataStartByte, dataSizeBytes) != 0)
					{
#if ENABLE_NETWORK_LOGGING
						netLoggingInterface& log = NetworkInterface::GetObjectManager().GetLog();

						if (!bHeaderLogged)
						{
							NetworkLogUtils::WriteLogEvent(log, "DIRTY_INITIAL_STATE", pObj->GetLogName());

							if (player != INVALID_PLAYER_INDEX)
							{
								CNetGamePlayer* pPlayer = NetworkInterface::GetPhysicalPlayerFromIndex(player);

								if (AssertVerify(pPlayer))
								{
									log.WriteDataValue("Player", pPlayer->GetLogName());
								}
							}

							bHeaderLogged = true;
						}

						log.WriteDataValue("Node", m_SyncUpdateNodes[i]->GetNodeName());
#endif
						if (player != INVALID_PLAYER_INDEX)
						{
							pObj->GetSyncData()->SetSyncDataUnitDirtyForPlayer(i, player);
						}
						else
						{
							pObj->GetSyncData()->SetSyncDataUnitDirty(i);
						}
					}
				}
			}
		}
	}
}

//PURPOSE
//	Writes the activation flags to the specified message buffer
void CProjectSyncTree::WriteActivationFlags(SerialiseModeFlags serMode, ActivationFlags actFlags, datBitBuffer& bitBuffer, netLoggingInterface* UNUSED_PARAM(pLog))
{
	// SERIALISEMODE_VERIFY should only be used for reading the tree
	gnetAssert(serMode != SERIALISEMODE_VERIFY);

    if(serMode == SERIALISEMODE_UPDATE || serMode == SERIALISEMODE_MIGRATE)
    {
        bool isScriptObject = actFlags & ACTIVATIONFLAG_SCRIPTOBJECT;

        bitBuffer.WriteBool(isScriptObject);
    }
}

//PURPOSE
//	Reads the activation flags from the specified message buffer
void CProjectSyncTree::ReadActivationFlags(SerialiseModeFlags serMode, ActivationFlags &actFlags, datBitBuffer& bitBuffer, netLoggingInterface* UNUSED_PARAM(pLog))
{
    if(serMode == SERIALISEMODE_UPDATE || serMode == SERIALISEMODE_MIGRATE || serMode == SERIALISEMODE_VERIFY)
    {
        bool isScriptObject = false;
        bitBuffer.ReadBool(isScriptObject);

        if(isScriptObject)
        {
            actFlags |= ACTIVATIONFLAG_SCRIPTOBJECT;
        }
		else
		{
			actFlags |= ACTIVATIONFLAG_AMBIENTOBJECT;
		}
    }
}

