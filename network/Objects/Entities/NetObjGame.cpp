//
// name:		NetObjGame.cpp
// description:	Project specific network object functionality
// written by:	Daniel Yelland
//

#include "network/objects/entities/NetObjGame.h"

#include "network/Debug/NetworkDebug.h"
#include "network/objects/NetworkObjectTypes.h"
#include "network/objects/synchronisation/synctrees/ProjectSyncTrees.h"
#include "script/handlers/GameScriptHandlerNetwork.h"
#include "script/handlers/GameScriptIds.h"
#include "script/handlers/GameScriptMgr.h"
#include "script/script.h"

#include "fwnet/netsyncdata.h"

NETWORK_OPTIMISATIONS()

ActivationFlags CNetObjGame::GetActivationFlags() const
{
	// objects that used to be script objects still sync all the script data so that any state changes are still sent after the object 
	// reverts to ambient
	if (IsGlobalFlagSet(GLOBALFLAG_WAS_SCRIPTOBJECT))
	{
		return ACTIVATIONFLAG_SCRIPTOBJECT;
	}
	else
	{
		return IsScriptObject(true) ? ACTIVATIONFLAG_SCRIPTOBJECT : ACTIVATIONFLAG_AMBIENTOBJECT;
	}
}

CNetGamePlayer *CNetObjGame::GetPlayerOwner() const
{
	if (netObject::GetPlayerOwner())
	{
		gnetAssert(dynamic_cast<CNetGamePlayer *>(netObject::GetPlayerOwner()));
		return static_cast<CNetGamePlayer *>(netObject::GetPlayerOwner());
	}

	return NULL;
}

bool CNetObjGame::IsScriptObject(bool UNUSED_PARAM(bIncludePlayer)) const
{
	return IsGlobalFlagSet(GLOBALFLAG_SCRIPTOBJECT);
}

bool CNetObjGame::IsOrWasScriptObject(bool bIncludePlayer) const
{
	return IsScriptObject(bIncludePlayer) || IsGlobalFlagSet(GLOBALFLAG_WAS_SCRIPTOBJECT);
}

bool CNetObjGame::AlwaysClonedForPlayer(PhysicalPlayerIndex playerIndex) const
{
    return ((m_AlwaysClonedForPlayers & (1<<playerIndex)) != 0);
}

void CNetObjGame::SetAlwaysClonedForPlayer(PhysicalPlayerIndex playerIndex, bool alwaysCloned)
{
    if(gnetVerifyf(playerIndex < MAX_NUM_PHYSICAL_PLAYERS, "Invalid player index!"))
    {
        if(alwaysCloned)
        {
            m_AlwaysClonedForPlayers |= (1<<playerIndex);
        }
        else
        {
            m_AlwaysClonedForPlayers &= (~(1<<playerIndex));
        }
    }
}

void CNetObjGame::Init(const unsigned numPlayers)
{
	netObject::Init(numPlayers);

	if (HasGameObject() && CanInitialiseInitialStateBuffer())
	{
		CProjectSyncTree* pSyncTree = static_cast<CProjectSyncTree*>(GetSyncTree());

		if (AssertVerify(pSyncTree) && !pSyncTree->HasInitialStateBufferBeenInitialised())
		{
			pSyncTree->InitialiseInitialStateBuffer(this);
		}
	}
}

void CNetObjGame::SetGameObject(void* gameObject)
{
	netObject::SetGameObject(gameObject);

	if (HasGameObject() && CanInitialiseInitialStateBuffer())
	{
		CProjectSyncTree* pSyncTree = static_cast<CProjectSyncTree*>(GetSyncTree());

		if (AssertVerify(pSyncTree) && !pSyncTree->HasInitialStateBufferBeenInitialised())
		{
			pSyncTree->InitialiseInitialStateBuffer(this);
		}
	}
}


bool CNetObjGame::CanClone(const netPlayer& player, unsigned *resultCode) const
{
	// don't clone objects for players not in our bubble
	if (!AssertVerify(NetworkInterface::GetLocalPlayer()->IsInSameRoamingBubble(static_cast<const CNetGamePlayer&>(player))))
	{
        if(resultCode)
        {
            *resultCode = CC_DIFFERENT_BUBBLE;
        }

		return false;
	}

	if (!AssertVerify(player.GetPhysicalPlayerIndex() != INVALID_PLAYER_INDEX))
	{
        if(resultCode)
        {
            *resultCode = CC_INVALID_PLAYER_INDEX;
        }

		return false;
	}

	return netObject::CanClone(player, resultCode);
}


bool CNetObjGame::CanPassControl(const netPlayer& player, eMigrationType migrationType, unsigned *resultCode) const
{
	if (!HasGameObject() && !CanPassControlWithNoGameObject())
    {
        if(resultCode)
        {
            *resultCode = CPC_FAIL_NO_GAME_OBJECT;
        }
		return false;
    }

	bool bCanPassControl = netObject::CanPassControl(player, migrationType, resultCode);

	if (bCanPassControl && IsProximityMigration(migrationType))
	{
		if(!NetworkDebug::IsProximityOwnershipAllowed())
		{
			if(resultCode)
			{
				*resultCode = CPC_FAIL_DEBUG_NO_PROXIMITY;
			}

			bCanPassControl = false;
		}

		// don't pass control of objects with unacked critical state, this is a bandwidth optimisation to avoid too much state
		// being sent in proximity migration messages (and subsequent state broadcast from the new owner)
		if (bCanPassControl && GetSyncData() && (GetSyncData()->GetCriticalStateUnsyncedPlayers() & NetworkInterface::GetPlayerMgr().GetRemotePhysicalPlayersBitmask()) != 0)
		{
			if(resultCode)
			{
				*resultCode = CPC_FAIL_CRITICAL_STATE_UNSYNCED;
			}

			bCanPassControl = false;
		}
	}

	// can't pass control of an object that is set to only migrate to other machines running the same script if we are not running that script.
	if (bCanPassControl && GetScriptObjInfo())
	{
		scriptHandlerObject* pScriptHandlerObj = GetScriptHandlerObject();

		if (gnetVerify(pScriptHandlerObj))
		{
			scriptHandler* pHandler = pScriptHandlerObj->GetScriptHandler();

			if (pHandler &&
				pHandler->RequiresANetworkComponent() &&
				!static_cast<CGameScriptHandlerNetwork*>(pHandler)->CanAcceptMigratingObject(*pScriptHandlerObj, player, migrationType))
			{
                if(resultCode)
                {
                    *resultCode = CPC_FAIL_NOT_SCRIPT_PARTICIPANT;
                }
				bCanPassControl = false;
			}
		}
	}

	if (bCanPassControl && !NetworkDebug::IsOwnershipChangeAllowed())
	{
		if(resultCode)
		{
			*resultCode = CPC_FAIL_DEBUG_NO_OWNERSHIP_CHANGE;
		}

		bCanPassControl = false;
	}

	return bCanPassControl;
}

bool CNetObjGame::CanAcceptControl(const netPlayer& player, eMigrationType migrationType, unsigned *resultCode) const
{
	bool bCanAcceptControl = netObject::CanAcceptControl(player, migrationType, resultCode);

	if (!NetworkDebug::IsOwnershipChangeAllowed())
	{
		if(resultCode)
		{
			*resultCode = CPC_FAIL_DEBUG_NO_OWNERSHIP_CHANGE;
		}

		bCanAcceptControl = false;
	}

	if (bCanAcceptControl && IsProximityMigration(migrationType))
	{
		if(!NetworkDebug::IsProximityOwnershipAllowed())
		{
			if(resultCode)
			{
				*resultCode = CPC_FAIL_DEBUG_NO_PROXIMITY;
			}

			bCanAcceptControl = false;
		}

		// can't accept control of script objects if we are spectating as we are not processing the in-game script stuff
		if (bCanAcceptControl && IsScriptObject() && NetworkInterface::IsInSpectatorMode() && !GetAllowMigrateToSpectator())
		{
            if(resultCode)
            {
                *resultCode = CAC_FAIL_SPECTATING;
            }

			bCanAcceptControl = false;
		}
	}

	if (bCanAcceptControl)
	{
		// can't accept control of objects if we are about to change our tutorial session state
		if (NetworkInterface::IsTutorialSessionChangePending())
		{
			if(resultCode)
			{
				*resultCode = CAC_FAIL_PENDING_TUTORIAL_CHANGE;
			}

			return false;
		}
	}

	// can't accept control of an object that is set to only migrate to other machines running the same script if we are not running that script.
	if (bCanAcceptControl && GetScriptObjInfo())
	{
		scriptHandlerObject* pScriptHandlerObj = GetScriptHandlerObject();

		if (gnetVerify(pScriptHandlerObj))
		{
			scriptHandler* pHandler = pScriptHandlerObj->GetScriptHandler();

			if (pHandler)
			{
				if (pHandler->RequiresANetworkComponent() &&
					!static_cast<CGameScriptHandlerNetwork*>(pHandler)->CanAcceptMigratingObject(*pScriptHandlerObj, *NetworkInterface::GetLocalPlayer(), migrationType))
				{
                    if(resultCode)
                    {
                        *resultCode = CAC_FAIL_SCRIPT_REJECT;
                    }
					bCanAcceptControl = false;
				}
			}
			else if (IsGlobalFlagSet(GLOBALFLAG_SCRIPT_MIGRATION) || IsGlobalFlagSet(GLOBALFLAG_CLONEONLY_SCRIPT))
			{
                if(resultCode)
                {
                    *resultCode = CAC_FAIL_NOT_SCRIPT_PARTICIPANT;
                }
				bCanAcceptControl = false;
			}
		}
	}

    return bCanAcceptControl;
}

// Name         :   ChangeOwner
// Purpose      :   Called when the player controlling this object changes
// Parameters   :   playerIndex - the id of the new player controlling this object
//					migrationType - how the entity migrated
void CNetObjGame::ChangeOwner(const netPlayer& player, eMigrationType migrationType)
{
	if (!IsClone())
	{
		if(GetSyncData() && HasGameObject())
		{
			// cache which nodes were dirtied while the object was local
			GetSyncTree()->GetDirtyNodes(this, m_dirtyNodes);
		}
		else
		{
			m_dirtyNodes = 0;
		}
	}
	

	netObject::ChangeOwner(player, migrationType);

	if (!IsClone())
	{
		// if this is a script object, and the script that created it has terminated, or it is no longer needed, then cleanup its mission state
		if (GetScriptObjInfo() && CTheScripts::GetScriptHandlerMgr().IsScriptTerminated(static_cast<const CGameScriptId&>(GetScriptObjInfo()->GetScriptId())))
		{
            // flag for conversion to ambient in the next update - this can't be done immediately here as the ownership change
            // process is not fully complete, and the object is in an inconsistent state
			SetLocalFlag(CNetObjGame::LOCALFLAG_NOLONGERNEEDED, true);
		}
	}
}

void CNetObjGame::PostMigrate(eMigrationType migrationType)
{
	netObject::PostMigrate(migrationType);

	if (!IsClone())
	{
		if (GetSyncData() && HasGameObject())
		{
			netLoggingInterface &log = NetworkInterface::GetObjectManager().GetLog();

			NetworkLogUtils::WriteLogEvent(log, "DIRTYING_NODES", GetLogName());
			CSyncDataLogger logger(&log);
			logger.SerialiseBitField(m_dirtyNodes, sizeof(m_dirtyNodes)<<3, "Node flags");

			// dirty the nodes that were altered while the object was a clone or when it was local
			GetSyncTree()->DirtyNodes(this, m_dirtyNodes);

			// force the global flags node to be sent immediately that the the new ownership token is sent out
			CProximityMigrateableSyncTreeBase* pSyncTree = static_cast<CProximityMigrateableSyncTreeBase*>(GetSyncTree());
			pSyncTree->ForceSendOfNodeData(SERIALISEMODE_UPDATE, GetActivationFlags(), this, *pSyncTree->GetGlobalFlagsNode());
		}
	}
}

void CNetObjGame::PlayerHasLeft(const netPlayer& player)
{
    if(player.GetPhysicalPlayerIndex() < MAX_NUM_PHYSICAL_PLAYERS)
    {
        SetAlwaysClonedForPlayer(player.GetPhysicalPlayerIndex(), false);
    }

    netObject::PlayerHasLeft(player);
}

bool CNetObjGame::OnReassigned()
{
	// if we have an object reassigned to our machine, and we are not running the script that created it then we need to remove it
	// if this object must only exist on machines running that script (if it has been reassigned to our machine this means that no other machines
	// are running the script)
	if (!IsClone() && IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_CLONEONLY_SCRIPT))
	{
		scriptHandlerObject* scriptObj = GetScriptHandlerObject();

		if (scriptObj)
		{
			if (!scriptObj->GetScriptHandler() ||
				!scriptObj->GetScriptHandler()->GetNetworkComponent() ||
				scriptObj->GetScriptHandler()->GetNetworkComponent()->GetState() != scriptHandlerNetComponent::NETSCRIPT_PLAYING)
			{
				return true;
			}
		}
	}

	return netObject::OnReassigned();
}

void CNetObjGame::StartSynchronising()
{
	netObject::StartSynchronising();

	static_cast<CProjectSyncTree*>(GetSyncTree())->DirtyNodesThatDifferFromInitialState(this, 0);
}

void CNetObjGame::OnConversionToScriptObject()
{
	// make sure LOCALFLAG_NOLONGERNEEDED is cleared (it may be left set by a previous script unregistration)
	SetLocalFlag(CNetObjGame::LOCALFLAG_NOLONGERNEEDED, false);

	if (!IsClone())
	{
		scriptHandlerObject* scriptObj = GetScriptHandlerObject();

		// if we are in a cutscene, allow the object to be displayed but don't clone it 
		if (!NetworkInterface::AreCutsceneEntitiesNetworked())
		{
			if (gnetVerify(scriptObj) && scriptObj->GetScriptHandler() && scriptObj->GetScriptHandler()->RequiresANetworkComponent() && static_cast<CGameScriptHandlerNetwork*>(scriptObj->GetScriptHandler())->GetInMPCutscene() && !IsClone())
			{
				SetLocalFlag(CNetObjGame::LOCALFLAG_SHOWINCUTSCENE, true);
				SetLocalFlag(CNetObjGame::LOCALFLAG_NOCLONE, true);
			}
		}
	}
}

void CNetObjGame::OnConversionToAmbientObject()
{
	// clear the commonly used script object global flags
	SetGlobalFlag(netObject::GLOBALFLAG_PERSISTENTOWNER,	   false);
	SetGlobalFlag(netObject::GLOBALFLAG_CLONEALWAYS,		   false);
	SetGlobalFlag(CNetObjGame::GLOBALFLAG_SCRIPTOBJECT,		   false);
	SetGlobalFlag(CNetObjGame::GLOBALFLAG_CLONEALWAYS_SCRIPT,  false);
	SetGlobalFlag(CNetObjGame::GLOBALFLAG_SCRIPT_MIGRATION,    false);
	SetLocalFlag(CNetObjGame::LOCALFLAG_NOLONGERNEEDED,		   false);
	SetGlobalFlag(CNetObjGame::GLOBALFLAG_WAS_SCRIPTOBJECT,    true);

	// use the tree to reset the script state on both the local object and the clones. This ensures that they all have the same state.
	if (GetEntity())
	{
		static_cast<CProjectSyncTree*>(GetSyncTree())->ResetScriptState(this);
	}

    // need to update the sync tree here as the activation flags have changed
    if(!IsClone() && GetSyncData())
    {
        GetSyncTree()->Update(this, GetActivationFlags(), netInterface::GetSynchronisationTime());
    }

	NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "CONVERT_TO_AMBIENT", GetLogName());
}

bool CNetObjGame::IsLocallyRunningScriptObject() const
{
	if (IsScriptObject() && GetScriptObjInfo())
	{
		scriptHandlerObject* pScriptHandlerObj = GetScriptHandlerObject();

		if (gnetVerify(pScriptHandlerObj))
		{
			scriptHandler* pHandler = pScriptHandlerObj->GetScriptHandler();

			if (pHandler)
			{
				return true;
			}
		}
	}

	return false;
}

#if ENABLE_NETWORK_LOGGING
void CNetObjGame::LogScopeReason(bool inScope, const netPlayer& player, unsigned reason)
{
	netLoggingInterface &log = NetworkInterface::GetObjectManager().GetLog();

	if (inScope)
	{
		NetworkLogUtils::WriteLogEvent(log, "IN_SCOPE", GetLogName());
	}
	else
	{
		NetworkLogUtils::WriteLogEvent(log, "OUT_OF_SCOPE", GetLogName());
	}

	log.WriteDataValue("Player", player.GetLogName());
	log.WriteDataValue("Reason", NetworkUtils::GetScopeReason(reason));

	netObject::LogScopeReason(inScope, player, reason);
}
#endif

const char *CNetObjGame::GetGlobalFlagName(unsigned LOGGING_ONLY(flag))
{
#if ENABLE_NETWORK_LOGGING
    static const char *flagNames[] =
    {
        "Persistent Owner",
		"Clone Always",
        "Script Object",
		"Script Clone Always",
		"Script Clone Only",
		"Script Migrate Only",
		"Was A Script Object",
		"Reserved Object"
    };

    CompileTimeAssert((sizeof(flagNames)/sizeof(const char *)) == SIZEOF_NETOBJ_GLOBALFLAGS);
#endif // ENABLE_NETWORK_LOGGING

    const char *name = "Unknown";

#if ENABLE_NETWORK_LOGGING
    if(gnetVerifyf(flag < SIZEOF_NETOBJ_GLOBALFLAGS, "Unexpected Global Flag!"))
    {
        name = flagNames[flag];
    }
#endif // ENABLE_NETWORK_LOGGING

    return name;
}

#if ENABLE_NETWORK_LOGGING

const char *CNetObjGame::GetCanCloneErrorString(unsigned resultCode)
{
    return NetworkUtils::GetCanCloneErrorString(resultCode);
}

const char *CNetObjGame::GetCannotDeleteReason(unsigned reason)
{
	switch(reason)
	{
	case CTD_RESPAWNING:
		return "Local Object Respawning";
	default:
		return netObject::GetCannotDeleteReason(reason);
	}
}

#endif // ENABLE_NETWORK_LOGGING