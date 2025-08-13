//
// name:		NetObjProximityMigrateable.h
// description:	Derived from netObject, this class deals with network objects that have a position in the game world and
//				migrate by proximity to other players
// written by:	John Gurney
//

#include "network/objects/entities/NetObjProximityMigrateable.h"

// framework headers
#include "fwnet/netblenderlininterp.h"
#include "fwnet/netobjectreassignmgr.h"
#include "fwnet/netsyncdata.h"
#include "fwscene/world/WorldLimits.h"

// game headers
#include "Network/Cloud/Tunables.h"
#include "network/Debug/NetworkDebug.h"
#include "network/Events/NetworkEventTypes.h"
#include "Network/General/NetworkUtil.h"
#include "Network/Objects/Entities/NetObjPlayer.h"
#include "network/objects/NetworkObjectTypes.h"
#include "network/objects/NetworkObjectPopulationMgr.h"
#include "Network/Objects/Synchronisation/SyncNodes/ProximityMigrateableSyncNodes.h"
#include "Network/Objects/Synchronisation/SyncTrees/ProjectSyncTrees.h"
#include "Peds/Ped.h"
#include "Peds/PlayerInfo.h"
#include "renderer/DrawLists/drawListNY.h"
#include "scene/world/GameWorld.h"
#include "script/script.h"

#if __BANK
#include "camera/CamInterface.h"
#include "text/text.h"
#include "text/TextConversion.h"
#endif

NETWORK_OPTIMISATIONS()

static const unsigned int CONTROL_TIME         = 500;                // time between control checks
static const unsigned int CONTROL_HOLD_TIME    = 3000;	             // after control is migrated, it cannot migrate by proximity for this length if time

const float CNetObjProximityMigrateable::CONTROL_DIST_MIN_SQR = rage::square(4.0f);  // control can be taken of an entity if it is at least this distance away from its owner
const float CNetObjProximityMigrateable::CONTROL_DIST_MAX_SQR = rage::square(50.0f); // control of an entity can be transferred to another player if he is closer than this distance to the entity

bool CNetObjProximityMigrateable::ms_blockProxyMigrationBetweenTutorialSessions = false;

PARAM(invincibleMigratingPeds, "Peds are invincible and migrate repeatedly in MP");
PARAM(debugmigrationfailtracking, "Debugs the migration fail tracking code");

#if __BANK
#define DEBUG_MIGRATION_FAIL_TRACKING(s, ...) if(PARAM_debugmigrationfailtracking.Get()) { gnetDebug1(s, __VA_ARGS__); }
#else
#define DEBUG_MIGRATION_FAIL_TRACKING(s, ...)
#endif // __BANK

netINodeDataAccessor *CNetObjProximityMigrateable::GetDataAccessor(u32 dataAccessorType)
{
	netINodeDataAccessor *dataAccessor = 0;

	if(dataAccessorType == IProximityMigrateableNodeDataAccessor::DATA_ACCESSOR_ID())
	{
		dataAccessor = (IProximityMigrateableNodeDataAccessor *)this;
	}

	return dataAccessor;
}

//
// name:		GetScopeDistance
// description:	Calculates the range at which this object is in scope with another player
// Parameters: if pRelativePlayer then this returns the scope distance relative to that player, otherwise it returns the default scope distance
float CNetObjProximityMigrateable::GetScopeDistance(const netPlayer* UNUSED_PARAM(pRelativePlayer)) const
{
	if (IsInInterior())
		return GetScopeData().m_scopeDistanceInterior;

	return GetScopeData().m_scopeDistance;
}

//
// name:		IsInScope
// description:	This is used by the object manager to determine whether we need to create a
//				clone to represent this object on a remote machine. The decision is made using
//				the player that is passed into the method - this decision is usually based on
//				the distance between this player's players and the network object, but other criterion can
//				be used.
// Parameters:	pPlayer - the player that scope is being tested against
bool CNetObjProximityMigrateable::IsInScope(const netPlayer& playerToCheck, unsigned* scopeReason) const
{
    gnetAssert(dynamic_cast<const CNetGamePlayer *>(&playerToCheck));
    const CNetGamePlayer &player = static_cast<const CNetGamePlayer &>(playerToCheck);

    // we can't sync objects to non-physical players
    if(!player.IsPhysical() || !player.GetPlayerPed())
    {
		if (scopeReason)
		{
			*scopeReason = SCOPE_OUT_NON_PHYSICAL_PLAYER;
		}
        return false;
    }

    if(CNetObjGame::IsInScope(player))
    {
		if (scopeReason)
		{
			*scopeReason = SCOPE_IN_ALWAYS;
		}
        return true;
    }

	if (IsGlobalFlagSet(GLOBALFLAG_CLONEONLY_SCRIPT))
	{
		gnetAssertf(GetScriptObjInfo() || IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_WAS_SCRIPTOBJECT), "%s is set cloneonlyscript but it has no script object info", GetLogName());
		if(GetScriptObjInfo())
		{
			// objects with the GLOBALFLAG_CLONEONLY_SCRIPT flag set are only cloned on other machines running the script that created the object
			if (!CTheScripts::GetScriptHandlerMgr().IsPlayerAParticipant(static_cast<const CGameScriptId&>(GetScriptObjInfo()->GetScriptId()), player))
			{
				if (scopeReason)
				{
					*scopeReason = SCOPE_OUT_NOT_SCRIPT_PARTICIPANT;
				}
				return false;
			}
		}
	}

	float scopeDist = GetScopeDistance(&playerToCheck);
	float scopeDistSqr = scopeDist * scopeDist;

	Vector3 playerPos = NetworkInterface::GetPlayerFocusPosition(player);
	Vector3 entityPos = GetScopePosition();

    if(IsInHeightScope(playerToCheck, entityPos, playerPos, scopeReason))
    {
		Vector3 diff = entityPos - playerPos;

	    if (diff.XYMag2() < scopeDistSqr)
		{
			if (scopeReason)
			{
				*scopeReason = SCOPE_IN_PROXIMITY_RANGE_PLAYER;
			}
		    return true;
		}

        grcViewport *viewport = NetworkUtils::GetNetworkPlayerViewPort(player);

        if(viewport)
        {
            diff = GetPosition() - VEC3V_TO_VECTOR3(viewport->GetCameraPosition());

            if (diff.XYMag2() < scopeDistSqr)
			{
				if (scopeReason)
				{
					*scopeReason = SCOPE_IN_PROXIMITY_RANGE_CAMERA;
				}
				return true;
			}
        }

		if (scopeReason)
		{
			*scopeReason = SCOPE_OUT_PROXIMITY_RANGE;
		}
    }

	return false;
}

bool CNetObjProximityMigrateable::IsInSameGarageInterior(const Vector3& entityPos, const Vector3& otherPos, unsigned* scopeReason)
{
	// don't clone entities far beneath the water on players above the water, or vice versa. This is mainly to handle the case where players are in an interior (eg a garage) under
	// the map - in this case we don't care about the entities above ground. 
	static const float UNDERWATER_SCOPE_CUTOFF = -80.0f;  
	bool bInScope = true;

	if (entityPos.z < UNDERWATER_SCOPE_CUTOFF && otherPos.z >= 0.0f)
	{
		if (scopeReason)
		{
			*scopeReason = SCOPE_OUT_BENEATH_WATER;
		}
		bInScope = false;
	}
	else if (entityPos.z >= 0.0f && otherPos.z < UNDERWATER_SCOPE_CUTOFF)
	{
		if (scopeReason)
		{
			*scopeReason = SCOPE_OUT_PLAYER_BENEATH_WATER;
		}
		bInScope = false;
	}

	return bInScope;
}

bool CNetObjProximityMigrateable::IsInHeightScope(const netPlayer& playerToCheck, const Vector3& entityPos, const Vector3& playerPos, unsigned* scopeReason) const
{
	bool bInScope = true;

	bool bPlayerConcealed = false;

	const CNetGamePlayer &player = static_cast<const CNetGamePlayer &>(playerToCheck);

	if (player.GetPlayerPed() && player.GetPlayerPed()->GetNetworkObject())
	{
		CNetObjPlayer* pPlayerObj = static_cast<CNetObjPlayer*>(player.GetPlayerPed()->GetNetworkObject());

		bPlayerConcealed = pPlayerObj->IsConcealed();
	}

	if (!bPlayerConcealed && !ShouldAllowCloneWhileInTutorial())
	{
		bInScope = IsInSameGarageInterior(entityPos, playerPos, scopeReason);
	}

	return bInScope;
}

// name:        Update
// description: Called once a frame to do special network object related stuff
// Returns		:	True if the object wants to unregister itself
bool CNetObjProximityMigrateable::Update()
{
	if (!IsClone())
	{
		NETWORK_DEBUG_BREAK_FOR_FOCUS(BREAK_TYPE_FOCUS_TRY_TO_MIGRATE, *this);

		if(m_failedToPassControl && fwTimer::GetSystemTimeInMilliseconds() > m_proximityMigrationTimer)
		{
			m_failedToPassControl = false;
		}

        if(IsLocalFlagSet(CNetObjGame::LOCALFLAG_REMOVE_POST_TUTORIAL_CHANGE) && !NetworkInterface::IsTutorialSessionChangePending() && CanDelete())
        {
            NetworkLogUtils::WritePlayerText(NetworkInterface::GetObjectManagerLog(), GetPhysicalPlayerIndex(), "REMOVING_AFTER_TUTORIAL", GetLogName());
            return true;
        }

		TryToPassControlProximity();
		TryToPassControlFromTutorial();

		// pass control of any script entities that always migrate to machines running the script that created them
		if ((IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_SCRIPT_MIGRATION) || IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_CLONEONLY_SCRIPT)) && GetScriptObjInfo() && !IsPendingOwnerChange())
		{
			scriptHandler* pHandler = CTheScripts::GetScriptHandlerMgr().GetScriptHandler(GetScriptObjInfo()->GetScriptId());

			if (!pHandler || !pHandler->GetNetworkComponent() || pHandler->GetNetworkComponent()->GetState() == scriptHandlerNetComponent::NETSCRIPT_TERMINATED)
			{
				CGameScriptHandlerMgr::CRemoteScriptInfo* pRemoteInfo = CTheScripts::GetScriptHandlerMgr().GetRemoteScriptInfo(GetScriptObjInfo()->GetScriptId(), true);

				if (pRemoteInfo)
				{
					CNetGamePlayer* pClosestPlayer = CTheScripts::GetScriptHandlerMgr().GetClosestParticipantOfRemoteScript(*pRemoteInfo);

					if (pClosestPlayer)
					{
						if (CanPassControl(*pClosestPlayer, MIGRATE_FORCED))
						{
							CGiveControlEvent::Trigger(*pClosestPlayer, this, MIGRATE_FORCED);
						}
					}
					else if (IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_CLONEONLY_SCRIPT))
					{
						return true;
					}
				}
				else if (!IsLocalFlagSet(LOCALFLAG_NOLONGERNEEDED))
				{
					gnetAssertf(pHandler, "%s has the GLOBALFLAG_SCRIPT_MIGRATION or GLOBALFLAG_CLONEONLY_SCRIPT flag set but has been orphaned and not cleaned up", GetLogName());
				}
			}
		}

		CEntity* pEntity = GetEntity();

		// cleanup any locally owned networked objects held on interior retain lists if possible
		if (pEntity && pEntity->GetIsRetainedByInteriorProxy() && !CInteriorProxy::ShouldEntityFreezeAndRetain(pEntity) && CanDelete() && pEntity->CanBeDeleted())
		{
			NetworkLogUtils::WritePlayerText(NetworkInterface::GetObjectManagerLog(), GetPhysicalPlayerIndex(), "REMOVING_FROM_RETAIN_LIST", GetLogName());
			return true;
		}
	}

	SetLocalFlag(LOCALFLAG_DISABLE_PROXIMITY_MIGRATION, false);

	return netObject::Update();
}

// Name         :   StartSynchronisation
// Purpose      :
void CNetObjProximityMigrateable::StartSynchronising()
{
    CNetObjGame::StartSynchronising();

	// flag the position as dirty, it must go always go out when syncing starts
	if (GetSyncData() && GetEntity())
	{
        CProximityMigrateableSyncTreeBase* pSyncTree = static_cast<CProximityMigrateableSyncTreeBase*>(GetSyncTree());
        pSyncTree->DirtyPositionNodes(*this);
	}
}

// Name         :   CalcReassignPriority
// Purpose      :   Calculates the reassign priority for this object when it's owner player leaves
u32 CNetObjProximityMigrateable::CalcReassignPriority() const
{
	// use the distance from our players to calculate how much we want this object
    CNetGamePlayer *localPlayer = NetworkInterface::GetLocalPlayer();

	const u32 maxPriority = (1<<(SIZEOF_REASSIGNPRIORITY))-1;
    u32 priority = 0;

	if(GetEntity() && localPlayer && localPlayer->CanAcceptMigratingObjects())
    {
		const Vector3 diff        = NetworkUtils::GetNetworkPlayerPosition(*localPlayer) - GetScopePosition();
		const float distToPlayer  = diff.Mag();
        const float scopeDistance = GetScopeDistance(localPlayer);

		if(distToPlayer < scopeDistance)
		{
			// save a bit of the priority for network objects which require to set a very high priority due to other reasons (eg script objects that
			// we may want to grab if we are running the script that created them)
			priority = (maxPriority - (u32)((distToPlayer / scopeDistance) * maxPriority));
		}
    }

    gnetAssertf(priority <= maxPriority, "Calculated a reassign priority higher than the allowed maximum!");
	return priority;
}

bool CNetObjProximityMigrateable::CanPassControl(const netPlayer& player, eMigrationType migrationType, unsigned *resultCode) const
{
	//The entity is being teleported and control can not be passed.
	if (IsLocalFlagSet(CNetObjGame::LOCALFLAG_TELEPORT))
	{
		if(resultCode)
		{
			*resultCode = CPC_FAIL_BEING_TELEPORTED;
		}
		return false;
	}

	if (IsProximityMigration(migrationType))
    {
		// can't pass control of an object by migration to a dead player - he's about to be respawned somewhere else
		// similarly, players in a MP cutscene can not accept migrating objects (the map may not be streamed in around their player ped)
		if (!static_cast<const CNetGamePlayer&>(player).CanAcceptMigratingObjects())
		{
            if(resultCode)
            {
                *resultCode = CPC_FAIL_PLAYER_NO_ACCEPT;
            }
			return false;
		}

        if(fwTimer::GetSystemTimeInMilliseconds() < m_proximityMigrationTimer)
        {
            // if we're not in scope of the object we are controlling we pass regardless of the proximity timer,
            // note we only care about the distance from the object, not the other in scope checks
            bool inLocalScope = false;

            if(FindPlayerPed())
            {
				float scopeDist = GetScopeDistance(NetworkInterface::GetLocalPlayer());
	            float scopeDistSqr = scopeDist * scopeDist;

				Vector3 diff = GetPosition() - NetworkInterface::GetLocalPlayerFocusPosition();

	            if (diff.XYMag2() < scopeDistSqr)
                {
                    inLocalScope = true;
                }
            }

			bool enableBlockingOfMigrationAfterRejection = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("ENABLE_BLOCK_MIGRATE_AFTER_REJECT", 0x5ff1d788), true);

            if(inLocalScope || (m_failedToPassControl && enableBlockingOfMigrationAfterRejection))
            {
                if(resultCode)
                {
                    *resultCode = CPC_FAIL_PROXIMITY_TIMER;
                }
		        return false;
            }
        }
    }

	// if the entity has migrated due to a script request then prevent it migrating while the script migration timer is running
    if(fwTimer::GetSystemTimeInMilliseconds() < m_scriptMigrationTimer)
    {
        if(resultCode)
        {
            *resultCode = CPC_FAIL_SCRIPT_TIMER;
        }
        return false;
    }

	// prevent migration until all other machines know about the new ownership token
	PlayerFlags clonedState = GetClonedState();

	if ((clonedState & NetworkInterface::GetPlayerMgr().GetRemotePhysicalPlayersBitmask()) != clonedState)
	{
        // we do not do this when players are in a transitional state. There may be a short time where players
	    // have been moved off the physical list before being removed from the game (and having their cloned state cleaned up).
        gnetAssertf((netInterface::GetNumActivePlayers() + netInterface::GetNumPendingPlayers()) == netInterface::GetNumPhysicalPlayers(),
                "%s is flagged as cloned on machines that have left! (cloned state : %u, remote player flags : %u)",
                GetLogName(), clonedState, NetworkInterface::GetPlayerMgr().GetRemotePhysicalPlayersBitmask());
	
		clonedState &= NetworkInterface::GetPlayerMgr().GetRemotePhysicalPlayersBitmask();
	}

	if (clonedState & (1<<NetworkInterface::GetLocalPhysicalPlayerIndex()))
	{
		gnetAssertf(0, "%s is flagged as cloned on local machine!", GetLogName());

		clonedState &= ~(1<<NetworkInterface::GetLocalPhysicalPlayerIndex());
	}

	if (!GetSyncTree()->IsNodeSyncedWithPlayers(const_cast<CNetObjProximityMigrateable*>(this), *static_cast<const CProximityMigrateableSyncTreeBase*>(GetSyncTree())->GetGlobalFlagsNode(), clonedState))
	{
		if(resultCode)
		{
			*resultCode = CPC_FAIL_OWNERSHIP_TOKEN_UNSYNCED;
		}
		
		NetworkInterface::GetObjectManager().GetLog().Log("\t\t ### Can't pass control of %s, ownership token unsynced on players %u. Remote players : %u ### \r\n", GetLogName(), clonedState, NetworkInterface::GetPlayerMgr().GetRemotePhysicalPlayersBitmask());
		return false;
	}

	return CNetObjGame::CanPassControl(player, migrationType, resultCode);
}

// Name         :   ChangeOwner
// Purpose      :   Called when the player controlling this object changes
// Parameters   :   playerIndex - the id of the new player controlling this object
//					bProximityMigrate - ownership is being changed during proximity migration
void CNetObjProximityMigrateable::ChangeOwner(const netPlayer& player, eMigrationType migrationType)
{
	if (player.IsLocal() && migrationType != MIGRATE_REASSIGNMENT)
	{
		// the unsynced nodes are passed from the previous owner in a give control event, use these as the dirty nodes so that
		// only unsynced state is sent out once we take ownership.
		m_dirtyNodes = static_cast<CProximityMigrateableSyncTreeBase*>(GetSyncTree())->GetMigrationDataNode()->GetUnsyncedNodes(); 
	}

	CNetObjGame::ChangeOwner(player, migrationType);

	if (!IsClone())
	{
	    // stop object migrating owner for a wee while
	    m_proximityMigrationTimer = fwTimer::GetSystemTimeInMilliseconds() + CONTROL_HOLD_TIME;

        if(migrationType == MIGRATE_SCRIPT)
        {
            m_scriptMigrationTimer = fwTimer::GetSystemTimeInMilliseconds() + CONTROL_HOLD_TIME;
        }
	}

#if __BANK
    if(m_MigrationFailTimer > 0)
    {
        DEBUG_MIGRATION_FAIL_TRACKING("MFT Debug: %s - Resetting migration fail timer - changing owner", GetLogName());
    }

    m_MigrationFailTimer = 0;
#endif // __BANK
}


// Name         :   CheckPlayerHasAuthorityOverObject
// Purpose      :   Checks whether a player is allowed to be sending sync data for the object - check current ownership and ownership changes
//					**This must be called after the data from the player has been read into the sync tree**
bool CNetObjProximityMigrateable::CheckPlayerHasAuthorityOverObject(const netPlayer& player)
{
    bool playerHasAuthority = netObject::CheckPlayerHasAuthorityOverObject(player);

	if (!playerHasAuthority)
	{
		const CGlobalFlagsDataNode* pGlobalFlagsNode = static_cast<CProximityMigrateableSyncTreeBase*>(GetSyncTree())->GetGlobalFlagsNode();

		if (AssertVerify(pGlobalFlagsNode) && pGlobalFlagsNode->GetWasUpdated())
		{
			// we don't change owner based on the token if we own the object - we need to wait for the give control event reply
			if (netUtils::IsSeqGreater(pGlobalFlagsNode->GetOwnershipToken(), GetOwnershipToken(), pGlobalFlagsNode->GetSizeOfOwnershipToken()))
			{
                if(IsLocalFlagSet(netObject::LOCALFLAG_UNREGISTERING))
                {
                    // it is possible to get a clone sync for a local object that has been unregistered. This can happen for CObjects
                    // that have to be deleted on demand if their IPL is streamed out. If this happens in the small window between a remote machine
                    // accepting control of the object and a sync message being received with the new ownership token this will happen
                    gnetAssertf(GetObjectType() == NET_OBJ_TYPE_OBJECT, "Received a sync for an unregistering object unexpectedly!");
                }
                else
                {
				    ClearPendingPlayerIndex();

				    NetworkInterface::GetObjectManager().ChangeOwner(*this, player, MIGRATE_PROXIMITY);

                    if(GetLog())
                    {
				        GetLog()->Log("\t\tChanged owner due to token (%d > %d)\r\n", pGlobalFlagsNode->GetOwnershipToken(), GetOwnershipToken());
                    }
                }

				playerHasAuthority = true;
			}
		}
	}

	return playerHasAuthority;
}

#if ENABLE_NETWORK_LOGGING
void CNetObjProximityMigrateable::LogScopeReason(bool inScope, const netPlayer& player, unsigned reason)
{
    CNetObjGame::LogScopeReason(inScope, player, reason);

    netLoggingInterface &log = NetworkInterface::GetObjectManager().GetLog();
    Vector3 scopePos = GetScopePosition();
    log.WriteDataValue("Scope position", "%.2f, %.2f, %.2f", scopePos.x, scopePos.y, scopePos.z);
    
    bool bAtFocusPos;
    unsigned playerFocus = 0;
    Vector3 playerFocusPos = NetworkInterface::GetPlayerFocusPosition(player, &bAtFocusPos, false, &playerFocus);
    if (!bAtFocusPos)
    {
        log.WriteDataValue("Player focus", NetworkUtils::GetPlayerFocus(playerFocus));
        log.WriteDataValue("Player focus position", "%.2f, %.2f, %.2f", playerFocusPos.x, playerFocusPos.y, playerFocusPos.z);
    }
}
#endif


void CNetObjProximityMigrateable::LogAdditionalData(netLoggingInterface &log) const
{
    CProximityMigrateableSyncTreeBase *syncTree = static_cast<CProximityMigrateableSyncTreeBase *>(const_cast<CNetObjProximityMigrateable *>(this)->GetSyncTree());

    const CSectorDataNode *sectorNode = syncTree->GetSectorNode();

    if((gnetVerifyf(sectorNode, "Sync tree has no sector node!") &&
       sectorNode->GetWasUpdated()) || syncTree->GetPositionWasUpdated())
    {
        Vector3 position = GetPosition();

        log.WriteNodeHeader("World Position");

        log.WriteDataValue("X", "%.2f", position.x);
        log.WriteDataValue("Y", "%.2f", position.y);
        log.WriteDataValue("Z", "%.2f", position.z);
    }
}

void CNetObjProximityMigrateable::PostMigrate(eMigrationType migrationType)
{
	CNetObjGame::PostMigrate(migrationType);

	if (!IsClone())
	{
		// use the temporarily stored bitfield received in the migration data, to send all the sync data to the players we are still connected to, 
		// we don't know what the state of the data they have is in
		PlayerFlags clonedPlayersThatLeft = GetClonedPlayersThatLeft();

		if (clonedPlayersThatLeft != 0 && GetSyncData())
		{
			for (unsigned i=0; i<MAX_NUM_PHYSICAL_PLAYERS; i++)
			{
				if ((clonedPlayersThatLeft & (1<<i)) != 0)
				{
					GetSyncData()->SetAllDataUnitsUnsyncedWithPlayer(i);
				}
			}
		}

		SetClonedPlayersThatLeft(0);
	}
}

#if __BANK

bool CNetObjProximityMigrateable::ShouldDoMigrationChecksForPlayer(const CNetGamePlayer *player, float distToOwnerSqr, float &distToNearestRemotePlayerSqr, const netPlayer *&nearestRemotePlayer)
{
    const float MIGRATION_FAIL_TRACKING_LOCAL_DIST_START_THRESHOLD_SQR  = rage::square(80.0f); // only start tracking migration failures for objects at least this distance from the local player
    const float MIGRATION_FAIL_TRACKING_LOCAL_DIST_RESET_THRESHOLD_SQR  = rage::square(50.0f); // only start tracking migration failures for objects at least this distance from the local player
    const float MIGRATION_FAIL_TRACKING_REMOTE_DIST_START_THRESHOLD_SQR = rage::square(15.0f); // only start tracking migration failures for objects at least this close to another remote player
    const float MIGRATION_FAIL_TRACKING_REMOTE_DIST_RESET_THRESHOLD_SQR = rage::square(40.0f); // only start tracking migration failures for objects at least this close to another remote player

    bool trackThisPlayer = false;

    if(distToOwnerSqr > MIGRATION_FAIL_TRACKING_LOCAL_DIST_START_THRESHOLD_SQR)
    {
        DEBUG_MIGRATION_FAIL_TRACKING("MFT Debug: %s far away from owner (%.2fm) - starting tracking", GetLogName(), rage::Sqrtf(distToOwnerSqr));

        if(!player->IsInDifferentTutorialSession())
        {
            Vector3 diff                         = NetworkInterface::GetPlayerFocusPosition(*player) - GetScopePosition();
            float   distToCurrentRemotePlayerSqr = diff.XYMag2();

            float remoteDistThresholdSqr = (m_MigrationFailTimer > 0) ? MIGRATION_FAIL_TRACKING_REMOTE_DIST_RESET_THRESHOLD_SQR : MIGRATION_FAIL_TRACKING_REMOTE_DIST_START_THRESHOLD_SQR;

            if(distToCurrentRemotePlayerSqr < remoteDistThresholdSqr && distToCurrentRemotePlayerSqr < distToNearestRemotePlayerSqr)
            {
                DEBUG_MIGRATION_FAIL_TRACKING("MFT Debug: %s: Tracking %s (%.2fm away, threshold %.2fm)", GetLogName(), player->GetLogName(), rage::Sqrtf(distToCurrentRemotePlayerSqr), remoteDistThresholdSqr);
                nearestRemotePlayer          = player;
                distToNearestRemotePlayerSqr = distToCurrentRemotePlayerSqr;
                trackThisPlayer              = true;
            }
            else
            {
                DEBUG_MIGRATION_FAIL_TRACKING("MFT Debug: %s: Skipping %s (%.2fm away, threshold %.2fm)", GetLogName(), player->GetLogName(), rage::Sqrtf(distToCurrentRemotePlayerSqr), remoteDistThresholdSqr);
            }
        }
        else
        {
            DEBUG_MIGRATION_FAIL_TRACKING("MFT Debug: %s: %s in different tutorial session", GetLogName(), player->GetLogName());
        }
    }
    else if(distToOwnerSqr < MIGRATION_FAIL_TRACKING_LOCAL_DIST_RESET_THRESHOLD_SQR)
    {
        if(m_MigrationFailTimer > 0)
        {
            DEBUG_MIGRATION_FAIL_TRACKING("MFT Debug: %s within range of owner (%.2fm) - stopping tracking", GetLogName(), rage::Sqrtf(distToOwnerSqr));
        }

        m_MigrationFailTimer = 0;
    }

    return trackThisPlayer;
}

void CNetObjProximityMigrateable::DoMigrationFailureTrackingLogging(const netPlayer &nearestRemotePlayer, unsigned migrationFailReason, unsigned cpcFailReason, eMigrationType migrationType)
{
    bool logFailReason = false;

    const unsigned MIGRATION_FAIL_TRACKING_TIME_THRESHOLD = 10000;

    // log the fail reason when not migrated for the time threshold
    if(m_MigrationFailTimer < MIGRATION_FAIL_TRACKING_TIME_THRESHOLD)
    {
        m_MigrationFailTimer += (u16)fwTimer::GetSystemTimeStepInMilliseconds();

        DEBUG_MIGRATION_FAIL_TRACKING("MFT Debug: %s - increasing migration fail timer: %d MFT Reason:%s, CPC Reason:%s", GetLogName(), m_MigrationFailTimer, NetworkUtils::GetMigrationFailTrackingErrorString(migrationFailReason), GetCanPassControlErrorString(cpcFailReason, migrationType));

        if(m_MigrationFailTimer > MIGRATION_FAIL_TRACKING_TIME_THRESHOLD)
        {
            logFailReason = true;
        }
    }

    // log the fail reason when it has changed
    if(m_MigrationFailTimer > MIGRATION_FAIL_TRACKING_TIME_THRESHOLD)
    {
        if((m_MigrationFailReason != migrationFailReason) || ((m_MigrationFailReason == MFT_CAN_PASS_CONTROL_CHECKS) && (m_MigrationFailCPCReason != cpcFailReason)))
        {
            m_MigrationFailReason    = (u8)migrationFailReason;
            m_MigrationFailCPCReason = (u8)cpcFailReason;
            logFailReason            = true;
        }
    }

    if(logFailReason)
    {
        NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "MIGRATION_FAIL_TRACKING", GetLogName());
        NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Player", nearestRemotePlayer.GetLogName());
        NetworkInterface::GetObjectManager().GetLog().WriteDataValue("MFT Reason", NetworkUtils::GetMigrationFailTrackingErrorString(migrationFailReason));

        if(migrationFailReason == MFT_CAN_PASS_CONTROL_CHECKS)
        {
            NetworkInterface::GetObjectManager().GetLog().WriteDataValue("CPC Reason", GetCanPassControlErrorString(cpcFailReason, migrationType));
        }
    }
}

#define SET_MIGRATION_FAILURE_REASON(track, ReasonStorage, ReasonInput) if(track) { ReasonStorage = ReasonInput; }
#else
#define SET_MIGRATION_FAILURE_REASON(track, ReasonStorage, ReasonInput)
#endif // __BANK

bool CNetObjProximityMigrateable::DoTPCPScriptChecks(const netPlayer *player, bool bMigrateScriptObject, bool nextOwnerIsScriptParticipant, bool bPlayerInSameLocation, bool bLocalPlayerIsParticipantAndNearby, bool &bScriptParticipant, bool &bFoundScriptHost, bool &bSkipThisPlayer) const
{
    bool bCanPassToPlayer = false;
         bFoundScriptHost = false;
         bSkipThisPlayer  = false;

    CGameScriptHandlerMgr::CRemoteScriptInfo* pRemoteScriptInfo = bMigrateScriptObject ? CTheScripts::GetScriptHandlerMgr().GetRemoteScriptInfo(GetScriptObjInfo()->GetScriptId(), true) : 0;

    if (bMigrateScriptObject && pRemoteScriptInfo && bPlayerInSameLocation)
    {
        bScriptParticipant = pRemoteScriptInfo->IsPlayerAParticipant(player->GetPhysicalPlayerIndex());

        if (bScriptParticipant)
        {
            // if this player is a participant and the current candidate player isn't then make this player the current candidate
            if (!nextOwnerIsScriptParticipant && !bLocalPlayerIsParticipantAndNearby)
            {
                bCanPassToPlayer = true;
            }

            // favour the host
            if (FavourMigrationToHost() && pRemoteScriptInfo->GetHost() == player)
            {
                bCanPassToPlayer = true;
                bFoundScriptHost = true;
            }
        }
        else if (nextOwnerIsScriptParticipant || bLocalPlayerIsParticipantAndNearby)
        {
            // ignore this player if the current candidate player is a participant or the owning player is running the script
            // and is nearby
            bSkipThisPlayer = true;
        }
    }

    return bCanPassToPlayer;
}

bool CNetObjProximityMigrateable::DoTPCPInteriorChecks(bool bPlayerInSameLocation, bool bLocalPlayerIsInSameLocation, bool nextOwnerIsInSameLocation, float distToNewPlayer, float minDist, bool &bSkipThisPlayer) const
{
    bool bCanPassToPlayer = false;

    // favour players in the same location
    if (bPlayerInSameLocation && !bLocalPlayerIsInSameLocation)
    {
        if (!nextOwnerIsInSameLocation || distToNewPlayer < minDist)
        {
            bCanPassToPlayer = true;
        }
    }
    else if (!bPlayerInSameLocation && nextOwnerIsInSameLocation)
    {
        // ignore this player if the current candidate player is in the same interior as the entity and this player isn't
        bSkipThisPlayer = true;
    }

    return bCanPassToPlayer;
}

bool CNetObjProximityMigrateable::IsCloserToRemotePlayerForProximity(const CNetGamePlayer &UNUSED_PARAM(remotePlayer), const float distToCurrentOwnerSqr, const float distToRemotePlayerSqr)
{
    const float PROXIMITY_CHECK_THRESHOLD = 0.8f;
    return distToCurrentOwnerSqr > CONTROL_DIST_MIN_SQR && distToRemotePlayerSqr < CONTROL_DIST_MAX_SQR && distToRemotePlayerSqr < distToCurrentOwnerSqr*PROXIMITY_CHECK_THRESHOLD;
} 

//
// name:		CNetObjProximityMigrateable::TryToPassControlProximity
// description:	Tries to pass control of this entity to another machine if a player from that machine is closer
void CNetObjProximityMigrateable::TryToPassControlProximity()
{
	const netPlayer* pNextOwner = NULL;

	gnetAssert(!IsClone());

    if(fwTimer::IsUserPaused())
        return;

	if (IsPendingOwnerChange())
		return;

	if (!NetworkDebug::IsProximityOwnershipAllowed())
		return;

	if (IsGlobalFlagSet(GLOBALFLAG_PERSISTENTOWNER))
		return;

	if (IsLocalFlagSet(LOCALFLAG_DISABLE_PROXIMITY_MIGRATION))
		return;

	if(!HasSyncData() && !CanPassControlWithNoSyncData())
        return;

#if __BANK
    float            distToNearestRemotePlayerSqr = FLT_MAX;
    const netPlayer *nearestRemotePlayer          = 0;
    unsigned         migrationFailReason          = MFT_SUCCESS;
    unsigned         cpcFailReason                = CPC_SUCCESS;
    bool             resetMigrationFailTimer      = false;
#endif // __BANK

	// every so often check to see if control of this entity can be given to another player
	u32 currTime = sysTimer::GetSystemMsTime();

	eMigrationType migrationType = MIGRATE_PROXIMITY;
	
	if (currTime - m_controlTimer >= CONTROL_TIME)
	{
		m_controlTimer = currTime;

		CNetGamePlayer *currentOwner = GetPlayerOwner();

		float localPlayerScopeDist = GetScopeDistance(currentOwner);
		float localPlayerScopeDistSqr = localPlayerScopeDist*localPlayerScopeDist;

		Vector3 diff;
		float distToOwnerSqr = 0.0f;

		bool bPlayerPedAtFocus = true;

		// dead players and players in a cutscene must give away all their objects. Spectating players must give away script objects
		if (!currentOwner->CanAcceptMigratingObjects() || (IsScriptObject() && NetworkInterface::IsInSpectatorMode()))
		{
			CPhysical *physical = (GetEntity() && GetEntity()->GetIsPhysical()) ? static_cast<CPhysical *>(GetEntity()) : 0;
			if(physical && physical->GetAttachParent() != (CPhysical*)currentOwner->GetPlayerPed())
			{
				distToOwnerSqr = GetForcedMigrationRangeWhenPlayerTransitioning();
				migrationType = MIGRATE_FORCED; // force passing of objects around dead player as we are about to respawn
                NetworkInterface::OverrideNetworkBlenderUntilAcceptingObjects(physical);
			}
		}
		else
		{
			diff = NetworkInterface::GetLocalPlayerFocusPosition(&bPlayerPedAtFocus) - GetScopePosition();
			distToOwnerSqr = diff.XYMag2();
		}

        CPed *localPlayer                                           = CGameWorld::FindLocalPlayer();
		bool bMigrateScriptObject									= IsScriptObject() && GetScriptObjInfo();
		bool bEntityInInterior										= IsInInterior();
        bool bLocalPlayerIsInSameLocation							= (bEntityInInterior && localPlayer && localPlayer->GetNetworkObject()) ? (!bPlayerPedAtFocus || IsInSameInterior(*localPlayer->GetNetworkObject())) : true;
		bool bLocalPlayerIsParticipantAndNearby						= (bLocalPlayerIsInSameLocation && migrationType != MIGRATE_FORCED && IsLocallyRunningScriptObject()) ? (distToOwnerSqr < localPlayerScopeDistSqr*0.9f) : false;

		// - if another player is nearer this entity than its owner and the entity is not too close
		// to its owner then the ownership of the entity is passed to the player nearest to it
		// If we are trying to pass script entities on to participants of the script then we must favour them 
		if ((PARAM_invincibleMigratingPeds.Get() && !fwTimer::IsGamePaused() && GetSyncData()) ||
			(bMigrateScriptObject || !bLocalPlayerIsInSameLocation || (distToOwnerSqr > CONTROL_DIST_MIN_SQR)))
		{
			float distToNewPlayerSqr;
			float minDistSqr = -1;
			bool nextOwnerIsScriptParticipant = false;
			bool nextOwnerIsInSameLocation = false;
            BANK_ONLY(resetMigrationFailTimer = true;)

			unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
            const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

	        for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
            {
		        const CNetGamePlayer *player = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);

#if __BANK
				if (PARAM_invincibleMigratingPeds.Get())
				{
					pNextOwner = player;

					if (pNextOwner)
						break; 
				}

                bool trackThisPlayer = ShouldDoMigrationChecksForPlayer(player, distToOwnerSqr, distToNearestRemotePlayerSqr, nearestRemotePlayer);

                if(trackThisPlayer)
                {
                    resetMigrationFailTimer = false;
                }
#endif
				bool bScriptHostObject = IsScriptObject() && IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_CLONEALWAYS_SCRIPT);

				if ((!player->IsInDifferentTutorialSession() || bScriptHostObject) && 
					player->GetPlayerPed() &&
					player->GetPlayerPed()->GetNetworkObject() &&
					player->CanAcceptMigratingObjects())
				{
					bool bPlayerInSameLocation = bEntityInInterior ? IsInSameInterior(*player->GetPlayerPed()->GetNetworkObject()) : true;

					if (bPlayerInSameLocation || (!bLocalPlayerIsInSameLocation && !nextOwnerIsInSameLocation))
					{
						diff = NetworkInterface::GetPlayerFocusPosition(*player) - GetScopePosition();
						distToNewPlayerSqr = diff.XYMag2();

						bool bScriptParticipant = false;

						float remotePlayerScopeDist = GetScopeDistance(player);
						float remotePlayerScopeDistSqr = remotePlayerScopeDist*remotePlayerScopeDist;

						bool bEntityFrozen = (GetEntity() && GetEntity()->GetIsDynamic() && ((CDynamicEntity*)GetEntity())->m_nDEflags.bFrozen);

						// is this player in scope with the entity?
						if( (distToNewPlayerSqr < remotePlayerScopeDistSqr*0.9f) || (bEntityFrozen && distToNewPlayerSqr<distToOwnerSqr*0.95f) || NetworkInterface::IsInMPCutscene())
						{
                            bool bFoundScriptHost = false;
                            bool bSkipThisPlayer  = false;
							bool bCanPassToPlayer = DoTPCPScriptChecks(player, bMigrateScriptObject, nextOwnerIsScriptParticipant, bPlayerInSameLocation, bLocalPlayerIsParticipantAndNearby, bScriptParticipant, bFoundScriptHost, bSkipThisPlayer);
							
                            TUNE_BOOL(FORCE_SCRIPT_FAIL, false);

                            if(bSkipThisPlayer || FORCE_SCRIPT_FAIL)
                            {
                                SET_MIGRATION_FAILURE_REASON(trackThisPlayer, migrationFailReason, MFT_SCRIPT_CHECKS);
                                continue;
                            }

							if (bEntityInInterior)
							{
								bCanPassToPlayer |= DoTPCPInteriorChecks(bPlayerInSameLocation, bLocalPlayerIsInSameLocation, nextOwnerIsInSameLocation, distToNewPlayerSqr, minDistSqr, bSkipThisPlayer);
							}

                            TUNE_BOOL(FORCE_INTERIOR_FAIL, false);

                            if(bSkipThisPlayer || FORCE_INTERIOR_FAIL)
                            {
                                SET_MIGRATION_FAILURE_REASON(trackThisPlayer, migrationFailReason, MFT_INTERIOR_STATE);
                                continue;
                            }

							// standard migration by proximity: pass to this player if our player is out of scope or this player is withing the control passing
							// range and closer than our player
							if (!bCanPassToPlayer && 
								(distToOwnerSqr > localPlayerScopeDistSqr || IsCloserToRemotePlayerForProximity(*player, distToOwnerSqr, distToNewPlayerSqr)))
							{
								if (minDistSqr == -1 || distToNewPlayerSqr < minDistSqr)
								{
									if(!ms_blockProxyMigrationBetweenTutorialSessions || !player->IsInDifferentTutorialSession())
									{
										bCanPassToPlayer = true;
									}
								}
							}

                            if(!bCanPassToPlayer)
                            {
                                SET_MIGRATION_FAILURE_REASON(trackThisPlayer, migrationFailReason, MFT_PROXIMITY_CHECKS);
                            }

							if (bCanPassToPlayer)
							{
                                unsigned *resultCode = 0;

                                BANK_ONLY(resultCode = &cpcFailReason);

                                bool canPassControl         = CanPassControl(*player, migrationType, resultCode);
                                bool canRemoteAcceptObjects = NetworkInterface::GetObjectManager().RemotePlayerCanAcceptObjects(*player, true);

								if (canPassControl && (migrationType == MIGRATE_FORCED || canRemoteAcceptObjects))
								{
									pNextOwner = player;
									minDistSqr = distToNewPlayerSqr;

									gnetAssert(!(nextOwnerIsScriptParticipant && !bScriptParticipant));
									nextOwnerIsScriptParticipant = bScriptParticipant;
									nextOwnerIsInSameLocation = bPlayerInSameLocation;

									// quit once we have found the host of the script as we always want to favour him
									if (bFoundScriptHost)
									{
										break;
									}
								}
                                else
                                {
                                    if(!canPassControl)
                                    {
                                        SET_MIGRATION_FAILURE_REASON(trackThisPlayer, migrationFailReason, MFT_CAN_PASS_CONTROL_CHECKS);
                                    }
                                    else if(!canRemoteAcceptObjects)
                                    {
                                        SET_MIGRATION_FAILURE_REASON(trackThisPlayer, migrationFailReason, MFT_REMOTE_NOT_ACCEPTING_OBJECTS);
                                    }
                                }
							}
						}
                        else
                        {
                            SET_MIGRATION_FAILURE_REASON(trackThisPlayer, migrationFailReason, MFT_SCOPE_CHECKS);
                        }
					}
                    else
                    {
                        SET_MIGRATION_FAILURE_REASON(trackThisPlayer, migrationFailReason, MFT_INTERIOR_STATE);
                    }
				}
                else if(!player->CanAcceptMigratingObjects())
                {
                    SET_MIGRATION_FAILURE_REASON(trackThisPlayer, migrationFailReason, MFT_REMOTE_NOT_ACCEPTING_OBJECTS);
                }
			}
		}
	}

	if (pNextOwner)
	{
        DEBUG_MIGRATION_FAIL_TRACKING("MFT Debug: %s - Resetting migration fail timer - passing control", GetLogName());
        BANK_ONLY(m_MigrationFailTimer = 0);

		//If we are respawn 
		if (migrationType == MIGRATE_FORCED && GetGameObject())
		{
			CNetworkObjectPopulationMgr::FlagObjAsTransitional(GetObjectID());
		}

		CGiveControlEvent::Trigger(*pNextOwner, this, migrationType);
	}
#if __BANK
    else if(nearestRemotePlayer)
    {
        DoMigrationFailureTrackingLogging(*nearestRemotePlayer, migrationFailReason, cpcFailReason, migrationType);
    }
    else if(resetMigrationFailTimer)
    {
        if(m_MigrationFailTimer > 0)
        {
            DEBUG_MIGRATION_FAIL_TRACKING("MFT Debug: %s - Resetting migration fail timer - tracking criteria no longer met", GetLogName());
        }

        m_MigrationFailTimer = 0;
    }
#endif // __BANK
}

// Name         :   TryToPassControlOutOfScope
// Purpose      :   Tries to pass control of this entity when it is going out of scope with our players.
// Parameters   :   None
// Returns      :   true if object is not in scope with any other players and can be deleted.
bool CNetObjProximityMigrateable::TryToPassControlOutOfScope()
{
	if (!NetworkInterface::IsGameInProgress())
		return true;

	gnetAssert(!IsClone());

    netLoggingInterface &log = NetworkInterface::GetObjectManager().GetLog();

	if (IsPendingOwnerChange())
    {
        NetworkLogUtils::WriteLogEvent(log, "FAILED_TO_PASS_OUT_OF_SCOPE", GetLogName());
        log.WriteDataValue("Reason", "Pending owner change");
		return false;
    }

	if (!HasBeenCloned() &&
		!IsPendingCloning() &&
		!IsPendingRemoval())
	{
		return true;
	}

	if (IsGlobalFlagSet(GLOBALFLAG_PERSISTENTOWNER))
		return false;

	const netPlayer *closestPlayers[MAX_NUM_PHYSICAL_PLAYERS];
	u32 numPlayers = NetworkUtils::GetClosestRemotePlayersInScope(*this, closestPlayers);

    unsigned failReasons[MAX_NUM_PHYSICAL_PLAYERS];

	for(u32 index = 0; index < numPlayers; index++)
	{
		if (CanPassControl(*closestPlayers[index], MIGRATE_OUT_OF_SCOPE, &failReasons[index]))
		{
			CGiveControlEvent::Trigger(*closestPlayers[index], this, MIGRATE_OUT_OF_SCOPE);
			return false;
		}
	}

#if ENABLE_NETWORK_LOGGING
    if(numPlayers > 0)
    {
        NetworkLogUtils::WriteLogEvent(log, "FAILED_TO_PASS_OUT_OF_SCOPE", GetLogName());
        for(u32 index = 0; index < numPlayers; index++)
        {
            log.WriteDataValue(closestPlayers[index]->GetLogName(), NetworkUtils::GetCanPassControlErrorString(failReasons[index]));
        }
    }
#endif // ENABLE_NETWORK_LOGGING

	return (numPlayers==0); // we need to keep retrying if a player is nearby - we can't remove the object in front of him!
}

//
// name:		CNetObjProximityMigrateable::TryToPassControlFromTutorial
// description:	Tries to pass control of this object to another machine when the player enters a tutorial session
void CNetObjProximityMigrateable::TryToPassControlFromTutorial()
{
	gnetAssert(!IsClone());

	if(IsPendingOwnerChange())
		return;

	if(!NetworkDebug::IsProximityOwnershipAllowed())
		return;

	if(IsGlobalFlagSet(GLOBALFLAG_PERSISTENTOWNER))
		return;

	// get tutorial state
	bool isInTutorial = NetworkInterface::IsInTutorialSession();
	bool isTutorialPending = NetworkInterface::IsTutorialSessionChangePending()  && !IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_SCRIPTOBJECT);
	bool bControlFlagSet = GetPassControlInTutorial() && (isInTutorial || isTutorialPending);

	// if the flag to always attempt to pass control of this object in tutorial sessions
	// is set or we are currently pending a tutorial session load
	if(bControlFlagSet || isTutorialPending)
	{
		// we need to find players that are not only not in our tutorial session, but not in 
		// any tutorial session 
		const netPlayer* pPlayerToPassTo = NULL;
		float fClosestDistanceSq = -1.0f;

		// check all remote players
		unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
        const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

        for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
        {
	        const CNetGamePlayer *pPlayer = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);

			if(HasBeenCloned(*pPlayer))
			{
                bool isInScope = (GetInScopeState() & (1<<pPlayer->GetPhysicalPlayerIndex())) != 0;

                if(isInScope)
                {
				    bool isRemoteInTutorial = NetworkInterface::IsPlayerInTutorialSession(*pPlayer);
				    bool isRemoteTutorialPending = NetworkInterface::IsPlayerTutorialSessionChangePending(*pPlayer);

				    // if flag based - check that the other player is not in a tutorial and not about to join one
				    if(bControlFlagSet && (isRemoteInTutorial || isRemoteTutorialPending))
					    continue;
				    // if during transition - check that the other player is not pending a change and not in a different tutorial to the local player
				    // this check will return false if both players are in single player - which is what we want
				    else if(!bControlFlagSet && (isRemoteTutorialPending || pPlayer->IsInDifferentTutorialSession()))
					    continue;

				    // work out distance to this player and assign if closer than previous (or no previous)
				    float distanceSq = DistSquared(VECTOR3_TO_VEC3V(NetworkUtils::GetNetworkPlayerPosition(*pPlayer)), VECTOR3_TO_VEC3V(GetPosition())).Getf();
				    if(!pPlayerToPassTo || fClosestDistanceSq < distanceSq)
				    {
						bool bCanPassControl = true;
						unsigned resultCode = 0;
						if(!CanPassControl(*pPlayer, MIGRATE_FORCED, &resultCode))
						{
							bCanPassControl = false;
							NetworkInterface::GetObjectManagerLog().Log("TryToPassControlFromTutorial cannot request control of %s: Reason: %s", GetLogName(), GetCanPassControlErrorString(resultCode));
						}

						if(bCanPassControl)
					    {
						    pPlayerToPassTo = pPlayer;
						    fClosestDistanceSq = distanceSq;
					    }
				    }
                }
			}
		}

		// if we have a player to offload onto, do so
		if(pPlayerToPassTo)
			CGiveControlEvent::Trigger(*pPlayerToPassTo, this, MIGRATE_FORCED);
	}
}

//
// name:		CNetObjProximityMigrateable::GetDistanceToNearestPlayerSquared
// description:	returns the distance to the closest player
//
float CNetObjProximityMigrateable::GetDistanceToNearestPlayerSquared()
{
    float closestDistanceSquared = FLT_MAX;

	unsigned                 numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
    const netPlayer * const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();

    for(unsigned index = 0; index < numPhysicalPlayers; index++)
    {
        const CNetGamePlayer *player = SafeCast(const CNetGamePlayer, allPhysicalPlayers[index]);

		Vector3 vDiff = NetworkUtils::GetNetworkPlayerPosition(*player) - GetPosition();
		float newDistanceSquared = vDiff.Mag2();

		if(newDistanceSquared < closestDistanceSquared)
		{
			closestDistanceSquared = newDistanceSquared;
		}
	}

    return closestDistanceSquared;
}

//
// name:		CNetObjProximityMigrateable::GetDistanceXYToNearestPlayerSquared
// description:	returns the distance to the closest player (considering only the XY plane)
//
float CNetObjProximityMigrateable::GetDistanceXYToNearestPlayerSquared()
{
    float closestDistanceXYSquared = FLT_MAX;

 	unsigned                 numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
    const netPlayer * const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();

    for(unsigned index = 0; index < numPhysicalPlayers; index++)
    {
        const CNetGamePlayer *pPlayer = SafeCast(const CNetGamePlayer, allPhysicalPlayers[index]);
        Vector3 vDiff = NetworkUtils::GetNetworkPlayerPosition(*pPlayer) - GetPosition();
		float newDistanceXYSquared = vDiff.XYMag2();

		if(newDistanceXYSquared < closestDistanceXYSquared)
		{
			closestDistanceXYSquared = newDistanceXYSquared;
		}
    }

    return closestDistanceXYSquared;
}

void CNetObjProximityMigrateable::KeepProximityControl(u32 forTime)
{
	m_proximityMigrationTimer = fwTimer::GetSystemTimeInMilliseconds() + ((forTime > 0) ? forTime : CONTROL_HOLD_TIME);
}

bool CNetObjProximityMigrateable::IsProximityMigrationPermitted() const
{
	return (fwTimer::GetSystemTimeInMilliseconds() > m_proximityMigrationTimer && fwTimer::GetSystemTimeInMilliseconds() > m_scriptMigrationTimer);
}

void CNetObjProximityMigrateable::GetSector(u16& x, u16& y, u16& z)
{
	CEntity* pEntity = GetEntity();
	VALIDATE_GAME_OBJECT(pEntity);

	if (pEntity)
	{
		if(!IsClone())
		{
			GetSectorCoords(GetPosition(), m_sectorX, m_sectorY, m_sectorZ);
		}

		x = m_sectorX; y = m_sectorY; z = m_sectorZ;
	}
	else
	{
		x = y = z = 0;
	}
}

void CNetObjProximityMigrateable::GetSectorPosition(float &sectorPosX, float &sectorPosY, float &sectorPosZ) const
{
    u16 sx, sy, sz;

    Vector3 sectorPos;
	GetSectorCoords(GetPosition(), sx, sy, sz, &sectorPos);
    sectorPosX = sectorPos.x;
    sectorPosY = sectorPos.y;
    sectorPosZ = sectorPos.z;
}

// sync parser setter functions
void CNetObjProximityMigrateable::SetSectorPosition(const float sectorPosX, const float sectorPosY, const float sectorPosZ)
{
    Vector3 sectorPos(sectorPosX, sectorPosY, sectorPosZ);
    Vector3 realPos;

	if (m_sectorX == INVALID_SECTOR || m_sectorY == INVALID_SECTOR || m_sectorZ == INVALID_SECTOR)
	{
		gnetAssertf(0, "%s has received a sector position update but has not had a sector update", GetLogName());

		// set the sector to 0 to avoid problems with entities being moved outside the map
		m_sectorX = 0;
		m_sectorY = 0;
		m_sectorZ = 0;
	}

    GetWorldCoords(m_sectorX, m_sectorY, m_sectorZ, sectorPos, realPos);

    SetPosition(realPos);
}

void CNetObjProximityMigrateable::GetRealMapCoords(const Vector3 &adjustedPos, Vector3 &realPos)
{
	realPos.x = adjustedPos.x;
	realPos.y = adjustedPos.y;
	realPos.z = adjustedPos.z + WORLDLIMITS_ZMIN;
}

void CNetObjProximityMigrateable::GetAdjustedMapCoords(const Vector3 &realPos, Vector3 &adjustedPos)
{
	adjustedPos.x = realPos.x;
	adjustedPos.y = realPos.y;
	adjustedPos.z = realPos.z - WORLDLIMITS_ZMIN;
}

void CNetObjProximityMigrateable::GetAdjustedWorldExtents(Vector3 &min, Vector3 &max)
{
	static Vector3 sectorExtents = Vector3((1<<(CSectorDataNode::SIZEOF_SECTOR_X-1))*WORLD_WIDTHOFSECTOR_NETWORK,
											(1<<(CSectorDataNode::SIZEOF_SECTOR_Y-1))*WORLD_DEPTHOFSECTOR_NETWORK,
											(1<<(CSectorDataNode::SIZEOF_SECTOR_Z))*WORLD_HEIGHTOFSECTOR_NETWORK);

	min.x = -sectorExtents.x;
	min.y = -sectorExtents.y;
	min.z = 0.0f;

	max.x = sectorExtents.x;
	max.y = sectorExtents.y;
	max.z = sectorExtents.z;
}

void CNetObjProximityMigrateable::GetMigrationData(PlayerFlags &clonedState, DataNodeFlags& unsyncedNodes, DataNodeFlags &clonedPlayersThatLeft)
{
	PlayerFlags createState, removeState;

	GetClonedState(clonedState, createState, removeState);

	// swap the cloned state for the new owner player and our player prior to sending
	// this is so the clone state is correct for the new player
	gnetAssert((GetPendingPlayerIndex() != INVALID_PLAYER_INDEX) || IsLocalFlagSet(LOCALFLAG_BEINGREASSIGNED));
	gnetAssertf(GetPhysicalPlayerIndex() != GetPendingPlayerIndex(), "Trying to pass control of object %s to a player that already owns it. player index (%d)", GetLogName(), GetPendingPlayerIndex());

	clonedState |= (1<<GetPhysicalPlayerIndex());
	clonedState &= (~(1<<GetPendingPlayerIndex()));

	PlayerFlags remotePlayers = NetworkInterface::GetPlayerMgr().GetRemotePhysicalPlayersBitmask();

	// ignore the player we are sending the data to, as the unsynced data will be included in the give control event
	if (GetPendingPlayerIndex() != INVALID_PLAYER_INDEX)
	{
		remotePlayers &= ~(1<<GetPendingPlayerIndex());
	}

	unsyncedNodes = 0;

	if (GetSyncData())
	{
		unsyncedNodes = GetSyncData()->GetUnsyncedNodes(remotePlayers);
	}

	clonedPlayersThatLeft = GetClonedPlayersThatLeft();
}

void CNetObjProximityMigrateable::SetMigrationData(PlayerFlags clonedState, DataNodeFlags clonedPlayersThatLeft)
{
#if ENABLE_NETWORK_BOTS
	// remove any local bots from the cloned state
	unsigned                 numLocalPlayers = netInterface::GetPlayerMgr().GetNumLocalPhysicalPlayers();
	const netPlayer * const *localPlayers    = netInterface::GetPlayerMgr().GetLocalPhysicalPlayers();

	for(unsigned index = 0; index < numLocalPlayers; index++)
	{
		const CNetGamePlayer *sourcePlayer = SafeCast(const CNetGamePlayer, localPlayers[index]);

		if(sourcePlayer && sourcePlayer->IsBot())
		{
			clonedState &= (~(1<<sourcePlayer->GetPhysicalPlayerIndex()));
		}
	}
#endif // ENABLE_NETWORK_BOTS

	clonedPlayersThatLeft &= NetworkInterface::GetPlayerMgr().GetRemotePhysicalPlayersBitmask();

	clonedState |= clonedPlayersThatLeft;

	SetClonedState(clonedState, 0, 0);

	// temporarily store the players that left in our local bitfield, to be used in PostMigrate() once the object is local
	SetClonedPlayersThatLeft(clonedPlayersThatLeft);
}

//
// name:		CalcSectorCoords
// description:	Calculates the sector coords of the given world position
void CNetObjProximityMigrateable::GetSectorCoords(const Vector3& worldCoords, u16& sectorX, u16& sectorY, u16& sectorZ, Vector3* pSectorPos)
{
	Vector3 worldCoordsAdjusted;
	Vector3 sectorBase;

	GetAdjustedMapCoords(worldCoords, worldCoordsAdjusted);

	sectorBase.x = rage::Floorf(worldCoordsAdjusted.x / WORLD_WIDTHOFSECTOR_NETWORK);
	sectorBase.y = rage::Floorf(worldCoordsAdjusted.y / WORLD_DEPTHOFSECTOR_NETWORK);
	sectorBase.z = rage::Floorf(worldCoordsAdjusted.z / WORLD_HEIGHTOFSECTOR_NETWORK);

	s32 sx = static_cast<s32>(sectorBase.x) + (1<<(CSectorDataNode::SIZEOF_SECTOR_X-1));
	s32 sy = static_cast<s32>(sectorBase.y) + (1<<(CSectorDataNode::SIZEOF_SECTOR_Y-1));
	s32 sz = static_cast<s32>(sectorBase.z);

	// clamp
	sx = rage::Max<s32>(0, sx);
	sy = rage::Max<s32>(0, sy);
	sz = rage::Max<s32>(0, sz);

	// also clamp the sector pos to the edge of the map if the sector is clamped too
	if (sx > (1<<CSectorDataNode::SIZEOF_SECTOR_X)-1)
	{
		sx = (1<<CSectorDataNode::SIZEOF_SECTOR_X)-1;
		worldCoordsAdjusted.x = (1<<CSectorDataNode::SIZEOF_SECTOR_X)*WORLD_WIDTHOFSECTOR_NETWORK;
	}

	if (sy > (1<<CSectorDataNode::SIZEOF_SECTOR_Y)-1)
	{
		sy = (1<<CSectorDataNode::SIZEOF_SECTOR_Y)-1;
		worldCoordsAdjusted.y = (1<<CSectorDataNode::SIZEOF_SECTOR_Y)*WORLD_DEPTHOFSECTOR_NETWORK;
	}

	if (sz > (1<<CSectorDataNode::SIZEOF_SECTOR_Z)-1)
	{
		sz = (1<<CSectorDataNode::SIZEOF_SECTOR_Z)-1;
		worldCoordsAdjusted.z = (1<<CSectorDataNode::SIZEOF_SECTOR_Z)*WORLD_HEIGHTOFSECTOR_NETWORK;
	}

	sectorX = static_cast<u16>(sx);
	sectorY = static_cast<u16>(sy);
	sectorZ = static_cast<u16>(sz);

	if (pSectorPos)
	{
		pSectorPos->x = worldCoordsAdjusted.x - (sectorBase.x * WORLD_WIDTHOFSECTOR_NETWORK);
		pSectorPos->y = worldCoordsAdjusted.y - (sectorBase.y * WORLD_DEPTHOFSECTOR_NETWORK);
		pSectorPos->z = worldCoordsAdjusted.z - (sectorBase.z * WORLD_HEIGHTOFSECTOR_NETWORK);

		// clamp
		pSectorPos->x = rage::Max<float>(0.0f, rage::Min<float>(pSectorPos->x, WORLD_WIDTHOFSECTOR_NETWORK));
		pSectorPos->y = rage::Max<float>(0.0f, rage::Min<float>(pSectorPos->y, WORLD_DEPTHOFSECTOR_NETWORK));
		pSectorPos->z = rage::Max<float>(0.0f, rage::Min<float>(pSectorPos->z, WORLD_HEIGHTOFSECTOR_NETWORK));
	}
}

//
// name:		GetWorldCoords
// description:	Given a sector position and coordinates within the sector, this calculates the corresponding world position
void CNetObjProximityMigrateable::GetWorldCoords(u16 sectorX, u16 sectorY, u16 sectorZ, Vector3& sectorPos, Vector3& worldCoords)
{
	float sx = static_cast<float>(sectorX) - (1<<(CSectorDataNode::SIZEOF_SECTOR_X-1));
	float sy = static_cast<float>(sectorY) - (1<<(CSectorDataNode::SIZEOF_SECTOR_Y-1));
	float sz = static_cast<float>(sectorZ);

	Vector3 adjustedPos;

	adjustedPos.x = sx*WORLD_WIDTHOFSECTOR_NETWORK + sectorPos.x;
	adjustedPos.y = sy*WORLD_DEPTHOFSECTOR_NETWORK + sectorPos.y;
	adjustedPos.z = sz*WORLD_HEIGHTOFSECTOR_NETWORK + sectorPos.z;

	GetRealMapCoords(adjustedPos, worldCoords);
}

#if __BANK

void CNetObjProximityMigrateable::DisplayNetworkInfoForObject(const Color32 &col, float scale, Vector2 &screenCoords, const float debugTextYIncrement)
{
    const NetworkDebug::NetworkDisplaySettings &displaySettings = NetworkDebug::GetDebugDisplaySettings();

    if(displaySettings.m_displayInformation.m_displayPosition)
    {
        char str[50];

		u16 sectorX = 0;
        u16 sectorY = 0;
        u16 sectorZ = 0;
        GetSector(sectorX, sectorY, sectorZ);
        sprintf(str, "%d, %d, %d", sectorX, sectorY, sectorZ);
		DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, col, scale, str));
        screenCoords.y += debugTextYIncrement;

        sprintf(str, "%.2f, %.2f, %.2f", GetPosition().x, GetPosition().y, GetPosition().z);
        DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, col, scale, str));
        screenCoords.y += debugTextYIncrement;

		if (GetNetBlender())
		{
			netBlenderLinInterp *netBlender = static_cast<netBlenderLinInterp *>(GetNetBlender());
			Vector3 blendTarget = netBlender->GetLastPositionReceived();
			sprintf(str, "(%.2f, %.2f, %.2f)", blendTarget.x, blendTarget.y, blendTarget.z);
			DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, col, scale, str));
			screenCoords.y += debugTextYIncrement;
		}
    }
}

#define DISPLAY_RANGE 30.0f

void CNetObjProximityMigrateable::DisplaySyncTreeInfoForObject()
{
	Vector3 objPos = GetPosition();
	Vector3 vDiff = objPos - camInterface::GetPos();

	float fDist = vDiff.Mag2();
	float displayRange = DISPLAY_RANGE * DISPLAY_RANGE;

	if(fDist <= displayRange)
	{
		static float displayHeight = 1.5f;

		objPos.z += displayHeight;

		float fScale = 1.0f - (fDist / displayRange);
		u8 displayAlpha = (u8)Clamp((int)(255.0f * fScale), 0, 255);

		netLogDisplay displayLog(objPos, displayAlpha);
	
		GetSyncTree()->DisplayNodeInformation(this, displayLog);
	}
}

const char *CNetObjProximityMigrateable::GetCanPassControlErrorString(unsigned cpcFailReason, eMigrationType UNUSED_PARAM(migrationType)) const
{
    return NetworkUtils::GetCanPassControlErrorString(cpcFailReason);
}

#endif // __BANK


