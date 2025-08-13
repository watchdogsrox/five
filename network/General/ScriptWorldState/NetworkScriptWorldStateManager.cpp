//
// name:		NetworkScriptWorldStateManager.cpp
// description:	The script world state manager supports state changes to the game world made
//              by the host a script in the network game, ensuring these changes are applied to
//              newly joining changes and persist when the host leaves the session. The state changes
//              will also be reverted once the script terminates on all machines. This is useful for
//              world state that needs to be synchronised on all machines, regardless of which players
//              are taking part in the script.
// written by:	Daniel Yelland
//
#include "NetworkScriptWorldStateManager.h"

#include "fwnet/NetLogUtils.h"
#include "fwscript/ScriptInterface.h"

#include "Network/Network.h"
#include "Network/NetworkInterface.h"
#include "Network/Events/NetworkEventTypes.h"
#include "Network/Sessions/NetworkSession.h"
#include "Script/Script.h"

NETWORK_OPTIMISATIONS()

namespace rage
{
    XPARAM(migrateScriptHost);
}

CNetworkWorldStateData *CNetworkScriptWorldStateMgr::ms_WorldStateData[CNetworkScriptWorldStateMgr::MAX_WORLD_STATE_DATA];
CNetworkWorldStateData *CNetworkScriptWorldStateMgr::ms_PendingHostResponseData[CNetworkScriptWorldStateMgr::MAX_WORLD_STATE_DATA];
CNetworkScriptWorldStateMgr::PendingWorldStateChange CNetworkScriptWorldStateMgr::ms_PendingHostMigrationData[MAX_WORLD_STATE_DATA];


CNetworkWorldStateData::CNetworkWorldStateData() :
m_LocallyOwned(false)
, m_TimeCreated(sysTimer::GetSystemMsTime())
{
    m_ScriptID.Invalidate();
}

CNetworkWorldStateData::CNetworkWorldStateData(const CGameScriptId &scriptID,
                                               bool                 locallyOwned) :
m_ScriptID(scriptID)
, m_LocallyOwned(locallyOwned)
, m_TimeCreated(sysTimer::GetSystemMsTime())
{
}

bool CNetworkWorldStateData::IsEqualTo(const CNetworkWorldStateData &worldState) const
{
    if(worldState.GetType()  == GetType() &&
       worldState.m_ScriptID == m_ScriptID)
    {
        return true;
    }

    return false;
}

void CNetworkScriptWorldStateMgr::Init()
{
    for(unsigned index = 0; index < MAX_WORLD_STATE_DATA; index++)
    {
        ms_WorldStateData[index] = 0;
        ms_PendingHostResponseData[index] = 0;
        ms_PendingHostMigrationData[index].Reset();
    }
}

void CNetworkScriptWorldStateMgr::Shutdown()
{
    for(unsigned index = 0; index < MAX_WORLD_STATE_DATA; index++)
    {
        CNetworkWorldStateData *worldStateData = ms_WorldStateData[index];

        if(worldStateData)
        {
            worldStateData->RevertWorldState();
            delete worldStateData;
        }

        ms_WorldStateData[index] = 0;

        // revert any pending world state data waiting on a response from the host
        worldStateData = ms_PendingHostResponseData[index];

        if(worldStateData)
        {
            worldStateData->RevertWorldState();
            delete worldStateData;
        }

        // revert any pending world state data waiting on the host to migrate
        worldStateData = ms_PendingHostMigrationData[index].m_WorldState;

        if(worldStateData)
        {
            if(ms_PendingHostMigrationData[index].m_Change)
            {
                worldStateData->RevertWorldState();
            }
        }

        ms_PendingHostMigrationData[index].Reset();
    }
}

void CNetworkScriptWorldStateMgr::Update()
{
    for(unsigned index = 0; index < MAX_WORLD_STATE_DATA; index++)
    {
        CNetworkWorldStateData *worldStateData = ms_WorldStateData[index];

        if(worldStateData)
        {
            worldStateData->Update();

			scriptHandler *handler = CTheScripts::GetScriptHandlerMgr().GetScriptHandler(worldStateData->GetScriptID());

			// revert any changed world state if the script that changed it is no longer running
            if (!handler && !CTheScripts::GetScriptHandlerMgr().IsScriptActive(worldStateData->GetScriptID()))
            {
				const unsigned DELAY_TO_DELETE_AFTER_JOINING_SESSION = 10000;
				if(CNetwork::GetNetworkSession().GetTimeInSession() > DELAY_TO_DELETE_AFTER_JOINING_SESSION)
				{
					// wait a while before reverting the world state, to allow time for the local machine to be informed
					// about any newly created scripts that have changed world state
					const unsigned DELAY_REMOVAL_PERIOD = 5000;
					unsigned timeSinceCreated = sysTimer::GetSystemMsTime() - worldStateData->GetTimeCreated();

					if(timeSinceCreated >= DELAY_REMOVAL_PERIOD)
					{
						NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "REMOVING WORLD STATE DATA",
																								"%s is no longer active",
																								worldStateData->GetScriptID().GetLogName());
						worldStateData->RevertWorldState();
						delete worldStateData;
						ms_WorldStateData[index] = 0;
					}
				}
            }
            else if(worldStateData->HasExpired())
            {
                char header[128];
                formatf(header, 128, "%s HAS EXPIRED", worldStateData->GetName());
                NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), header, "");
                worldStateData->LogState(NetworkInterface::GetObjectManagerLog());
                worldStateData->RevertWorldState();
                delete worldStateData;
                ms_WorldStateData[index] = 0;
            }
            else
            {
                // check if we have become the new host of the script that changed the world state,
                // if so we need to resend the world state change event to all the players to ensure
                // everyone is up-to-date
                if(!worldStateData->IsLocallyOwned())
                {
					bool isHostOfScript = (handler && handler->GetThread() && handler->GetNetworkComponent()) ? handler->GetNetworkComponent()->IsHostLocal() : false;

                    if(isHostOfScript)
                    {
                        CScriptWorldStateEvent::Trigger(*worldStateData, true);

                        char header[128];
                        formatf(header, 128, "TAKING CONTROL OF %s", worldStateData->GetName());
                        NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), header, "");
                        worldStateData->LogState(NetworkInterface::GetObjectManagerLog());
                        worldStateData->SetLocallyOwned(true);
                    }
                }
                else
                {
                    // check if we have lost control of this world state, this can happen if we are debugging migrating the host,
                    // or if a player leaves a script they are hosting and rejoins as a non-host participant
                    if(handler && handler->GetNetworkComponent() && handler->GetNetworkComponent()->GetState() == scriptHandlerNetComponent::NETSCRIPT_PLAYING)
                    {
                        if(!CTheScripts::GetScriptHandlerMgr().IsNetworkHost(worldStateData->GetScriptID()))
                        {
                            char header[128];
                            formatf(header, 128, "LOST CONTROL OF %s", worldStateData->GetName());
                            NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), header, "");
                            worldStateData->LogState(NetworkInterface::GetObjectManagerLog());
                            worldStateData->SetLocallyOwned(false);
                        }
                    }
                }
            }
        }

        // check if there is any pending world state waiting for a host migration
        worldStateData = ms_PendingHostMigrationData[index].m_WorldState;

        if(worldStateData)
        {
            if(!CTheScripts::GetScriptHandlerMgr().IsScriptActive(worldStateData->GetScriptID()))
            {
                NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "REMOVING PENDING WORLD STATE",
                                                                                        "%s is no longer active",
                                                                                        worldStateData->GetScriptID().GetLogName());
                worldStateData->LogState(NetworkInterface::GetObjectManagerLog());

                if(ms_PendingHostMigrationData[index].m_Change)
                {
                    worldStateData->RevertWorldState();
                }

                ms_PendingHostMigrationData[index].Reset();
            }
            else
            {
                scriptHandler *handler = CTheScripts::GetScriptHandlerMgr().GetScriptHandler(worldStateData->GetScriptID());

                if(handler && handler->GetNetworkComponent())
                {
                    const netPlayer *scriptHost = handler->GetNetworkComponent()->GetHost();

                    if(scriptHost)
                    {
                        if(scriptHost->IsLocal())
                        {
                            CScriptWorldStateEvent::Trigger(*worldStateData, ms_PendingHostMigrationData[index].m_Change);
                        }
                        else
                        {
                            AddToHostResponsePendingList(*worldStateData);
                            CScriptWorldStateEvent::Trigger(*worldStateData, ms_PendingHostMigrationData[index].m_Change, scriptHost);
                        }

                        NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "REMOVING PENDING WORLD STATE",
                                                                                                "%s is new host",
                                                                                                scriptHost->GetLogName());
                        worldStateData->LogState(NetworkInterface::GetObjectManagerLog());

                        ms_PendingHostMigrationData[index].Reset();
                    }
                }
            }
        }
    }
}

void CNetworkScriptWorldStateMgr::PlayerHasJoined(const netPlayer &player)
{
    if(!player.IsMyPlayer())
    {
        for(unsigned index = 0; index < MAX_WORLD_STATE_DATA; index++)
        {
            CNetworkWorldStateData *worldStateData = ms_WorldStateData[index];

            if(worldStateData)
            {
                bool isHostOfScript = CTheScripts::GetScriptHandlerMgr().IsNetworkHost(worldStateData->GetScriptID());

                NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "PLAYER_HAS_JOINED", "%s (%s: I am script host %s)", player.GetLogName(), worldStateData->GetName(), isHostOfScript ? "TRUE" : "FALSE");
                worldStateData->LogState(NetworkInterface::GetObjectManagerLog());

                if(isHostOfScript)
                {
                    CScriptWorldStateEvent::Trigger(*worldStateData, true, &player);
                }

				worldStateData->PlayerHasJoined(player);
            }
        }
    }
}

void CNetworkScriptWorldStateMgr::PlayerHasLeft(const netPlayer &player)
{
	if(!player.IsMyPlayer())
	{
		for(unsigned index = 0; index < MAX_WORLD_STATE_DATA; index++)
		{
			CNetworkWorldStateData *worldStateData = ms_WorldStateData[index];

			if(worldStateData)
			{
				worldStateData->PlayerHasLeft(player);
			}
		}
	}
}

void CNetworkScriptWorldStateMgr::RevertWorldState(CNetworkWorldStateData &worldStateData,
                                                   bool                    fromNetwork)
{
    bool found = false;
	bool destroyWorldState = true;
	scriptHandler *handler = CTheScripts::GetScriptHandlerMgr().GetScriptHandler(worldStateData.GetScriptID());
	const netPlayer* hostPlayer = (handler && handler->GetNetworkComponent()) ? handler->GetNetworkComponent()->GetHost() : NULL;
	bool isHostOfScript = hostPlayer && hostPlayer->IsMyPlayer();

    for(unsigned index = 0; index < MAX_WORLD_STATE_DATA && !found; index++)
    {
        CNetworkWorldStateData *currentStateData = ms_WorldStateData[index];

        if(currentStateData && currentStateData->IsEqualTo(worldStateData))
        { 
            if(fromNetwork)
            {
                currentStateData->RevertWorldState();

                if(isHostOfScript)
                {
                    CScriptWorldStateEvent::Trigger(worldStateData, false);
                }
            }
            else
            {
                if(!isHostOfScript)
                {
                    if(hostPlayer)
                    {
                        CScriptWorldStateEvent::Trigger(worldStateData, false, hostPlayer);
                    }
                    else
                    {
                        AddToHostMigrationPendingList(worldStateData, false);
                    }

                    destroyWorldState = false;
                }
                else
                {
                    char header[128];
                    formatf(header, 128, "REVERTING_WORLD_STATE_%s", worldStateData.GetName());
                    NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), header, "");
                    worldStateData.LogState(NetworkInterface::GetObjectManagerLog());

                    CScriptWorldStateEvent::Trigger(worldStateData, false);
                }
            }

            if(destroyWorldState)
            {
                delete currentStateData;
                ms_WorldStateData[index] = 0;
            }

            found = true;
        }

    }

	// Also check the world states pending response in case we never received the response from the host.
	// We want to delete these as well! 
	if (!found)
	{		
		CNetworkWorldStateData *existingData = FindDataPendingResponse(worldStateData);

		if(existingData)
		{			
			if(!isHostOfScript)
			{
				if(hostPlayer)
				{
					CScriptWorldStateEvent::Trigger(worldStateData, false, hostPlayer);
				}
				else
				{
					AddToHostMigrationPendingList(worldStateData, false);
				}
			}
			NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "REVERTING_DATA_PENDING_RESPONSE", worldStateData.GetName());
			worldStateData.LogState(NetworkInterface::GetObjectManagerLog());

			// Delete the copy we already have
			delete existingData;
		}
	}
}

bool CNetworkScriptWorldStateMgr::ChangeWorldState(CNetworkWorldStateData &worldStateData,
                                                   bool fromNetwork)
{
	scriptHandler *handler = CTheScripts::GetScriptHandlerMgr().GetScriptHandler(worldStateData.GetScriptID());

	const netPlayer* hostPlayer = (handler && handler->GetNetworkComponent()) ? handler->GetNetworkComponent()->GetHost() : NULL;

	bool isHostOfScript = hostPlayer && hostPlayer->IsMyPlayer();

	bool duplicateExists = false;

    if(!isHostOfScript && !fromNetwork)
    {
        if(hostPlayer)
        {
            AddToHostResponsePendingList(worldStateData);
            CScriptWorldStateEvent::Trigger(worldStateData, true, hostPlayer);
        }
        else
        {
            AddToHostMigrationPendingList(worldStateData, true);
        }
    }
    else
    {
        // find a free slot in the world state data and check for duplicates
        unsigned nextFreeSlot    = MAX_WORLD_STATE_DATA;
        
        for(unsigned index = 0; index < MAX_WORLD_STATE_DATA && !duplicateExists; index++)
        {
            CNetworkWorldStateData *currentStateData = ms_WorldStateData[index];

            if(!currentStateData)
            {
                nextFreeSlot = index;
            }
            else if(currentStateData->IsEqualTo(worldStateData))
            {
                duplicateExists = true;
            }
        }

        if(gnetVerifyf(nextFreeSlot < MAX_WORLD_STATE_DATA, "Ran out of network world state data!") && !duplicateExists)
        {
            DEV_ONLY(CheckForConflictingWorldState(worldStateData));

            ms_WorldStateData[nextFreeSlot] = worldStateData.Clone();

            if(fromNetwork && gnetVerifyf(ms_WorldStateData[nextFreeSlot], "Failed to clone world state data!"))
            {
                CNetworkWorldStateData *existingData = FindDataPendingResponse(*ms_WorldStateData[nextFreeSlot]);

                if(existingData)
                {
                    existingData->UpdateHostState(*ms_WorldStateData[nextFreeSlot]);
                    delete existingData;

                    NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "UPDATING_DATA_PENDING_RESPONSE", ms_WorldStateData[nextFreeSlot]->GetName());
                    ms_WorldStateData[nextFreeSlot]->LogState(NetworkInterface::GetObjectManagerLog());
                }
                else
                {
                    ms_WorldStateData[nextFreeSlot]->ChangeWorldState();
                }

                if(isHostOfScript)
                {
                    CScriptWorldStateEvent::Trigger(worldStateData, true);
                }
            }
            else
            {
                char header[128];
                formatf(header, 128, "CHANGING_WORLD_STATE_%s", worldStateData.GetName());
                NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), header, "");
                worldStateData.LogState(NetworkInterface::GetObjectManagerLog());

                CScriptWorldStateEvent::Trigger(worldStateData, true);
            }
        }
    }

	return !duplicateExists;
}

CNetworkWorldStateData *CNetworkScriptWorldStateMgr::FindDataPendingResponse(const CNetworkWorldStateData &hostWorldState)
{
    CNetworkWorldStateData *worldState = 0;

    for(unsigned index = 0; index < MAX_WORLD_STATE_DATA && !worldState; index++)
    {
        CNetworkWorldStateData *currWorldState = ms_PendingHostResponseData[index];

        if(currWorldState && currWorldState->IsEqualTo(hostWorldState))
        {
            worldState = ms_PendingHostResponseData[index];
            ms_PendingHostResponseData[index] = 0;
        }
    }

    return worldState;
}

void CNetworkScriptWorldStateMgr::AddToHostResponsePendingList(const CNetworkWorldStateData &worldStateData)
{
    bool added = false;

    for(unsigned index = 0; index < MAX_WORLD_STATE_DATA && !added; index++)
    {
        if(ms_PendingHostResponseData[index] == 0)
        {
            char header[128];
            formatf(header, 128, "ADDING CLIENT WORLD STATE_%s", worldStateData.GetName());
            NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), header, "");
            worldStateData.LogState(NetworkInterface::GetObjectManagerLog());

            ms_PendingHostResponseData[index] = worldStateData.Clone();
            added = true;
        }
    }

    gnetAssertf(added, "Failed to add world state to host migration pending list!");
}

void CNetworkScriptWorldStateMgr::AddToHostMigrationPendingList(CNetworkWorldStateData &worldStateData, bool change)
{
    bool added = false;

    for(unsigned index = 0; index < MAX_WORLD_STATE_DATA && !added; index++)
    {
        if(ms_PendingHostMigrationData[index].m_WorldState == 0)
        {
            char header[128];
            formatf(header, 128, "ADDING PENDING WORLD STATE_%s", worldStateData.GetName());
            NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), header, "");
            worldStateData.LogState(NetworkInterface::GetObjectManagerLog());

            ms_PendingHostMigrationData[index].m_WorldState = worldStateData.Clone();
            ms_PendingHostMigrationData[index].m_Change     = change;
            added = true;
        }
    }

    gnetAssertf(added, "Failed to add world state to host migration pending list!");
}

#if __DEV

void CNetworkScriptWorldStateMgr::CheckForConflictingWorldState(const CNetworkWorldStateData &worldStateData)
{
    for(unsigned index = 0; index < MAX_WORLD_STATE_DATA; index++)
    {
        CNetworkWorldStateData *currentStateData = ms_WorldStateData[index];

        if(currentStateData && currentStateData->IsConflicting(worldStateData))
        {
            char header[128];
            formatf(header, 128, "EXISTING WORLD STATE_%s", currentStateData->GetName());
            NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), header, "");
            currentStateData->LogState(NetworkInterface::GetObjectManagerLog());

            formatf(header, 128, "NEW WORLD STATE_%s", worldStateData.GetName());
            NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), header, "");
            worldStateData.LogState(NetworkInterface::GetObjectManagerLog());

            gnetAssertf(0, "Conflicting world state detected!");
        }
    }
}

#endif // __DEV
