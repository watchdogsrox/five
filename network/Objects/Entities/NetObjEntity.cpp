//
// name:		NetObjEntity.h
// description:	Derived from netObject, this class handles all entity-related network object
//				calls. See NetworkObject.h for a full description of all the methods.
// written by:	John Gurney
//

#include "network/objects/entities/NetObjEntity.h"

// framework headers
#include "fwnet/netobjectmgrbase.h"
#include "fwnet/netobjectreassignmgr.h"
#include "fwnet/netblenderlininterp.h"

// game headers
#include "camera/gameplay/GameplayDirector.h"
#include "debug/DebugEntityState.h"
#include "network/Debug/NetworkDebug.h"
#include "Network/General/NetworkUtil.h"
#include "network/objects/NetworkObjectTypes.h"
#include "network/players/NetworkPlayerMgr.h"
#include "Peds/ped.h"
#include "renderer/DrawLists/drawListNY.h"
#include "scene/Entity.h"
#include "scene/Physical.h"
#include "scene/world/GameWorld.h"
#include "script/handlers/GameScriptEntity.h"
#include "script/script.h"
#include "text/text.h"
#include "text/TextConversion.h"
#include "control/gamelogic.h"
#include "camera/CamInterface.h"

NETWORK_OPTIMISATIONS()
NETWORK_OBJECT_OPTIMISATIONS()

PARAM(stopCloneFailChecks, "[network] Switches off the logging of failed clone creates");

const float CNetObjEntity::MIN_ENTITY_REMOVAL_DISTANCE_SQ	= (50.0f * 50.0f);

#if __BANK
bool		CNetObjEntity::ms_logLocalVisibilityOverride	= true;
#endif

CNetObjEntity::CNetObjEntity(CEntity                   *entity,
								const NetworkObjectType type,
								const ObjectId          objectID,
								const PhysicalPlayerIndex       playerIndex,
								const NetObjFlags       localFlags,
								const NetObjFlags       globalFlags) 
: CNetObjProximityMigrateable(entity, type, objectID, playerIndex, localFlags, globalFlags)
, m_bUsesCollision(true)
, m_bIsVisible(true)
, m_bLeaveEntityAfterCleanup(false)
, m_ShowRemotelyInCutsene(false)
, m_deletionTimer(0)
, m_ForceSendEntityScriptGameState(false)
, m_FrameForceSendRequested(0)
, m_EnableCollisonPending(false)
, m_DisableCollisionCompletely(false)
, m_KeepIsVisibleForCutsceneFlagSet(false)
#if __ASSERT
, m_notInWorldTimer(0)
#endif
#if __BANK
, m_cloneCreateFailTimer(0)
#endif
{
    m_LastReceivedMatrix.Identity();

#if __ASSERT
	if (entity && !IsClone() && entity->GetExtension<CScriptEntityExtension>() != 0)
	{
		// Object should not be registered with the network after their script extension has been set up, unsupported!
		const CGameScriptId* pScriptId = entity->GetIsPhysical() ? static_cast<CPhysical*>(entity)->GetScriptThatCreatedMe() : NULL;
		gnetAssertf(0, "A single-player script entity still exists when entering MP! Created by: %s, Model: %s", pScriptId ? pScriptId->GetScriptName() : "NONE", entity->GetModelName());
	}
#endif
}

netINodeDataAccessor *CNetObjEntity::GetDataAccessor(u32 dataAccessorType)
{
	netINodeDataAccessor *dataAccessor = 0;

	if(dataAccessorType == IEntityNodeDataAccessor::DATA_ACCESSOR_ID())
	{
		dataAccessor = (IEntityNodeDataAccessor *)this;
	}
	else
	{
		dataAccessor = CNetObjProximityMigrateable::GetDataAccessor(dataAccessorType);
	}

	return dataAccessor;
}

bool CNetObjEntity::IsInInterior() const
{
	return (GetEntity() && (GetEntity()->m_nFlags.bInMloRoom || GetEntity()->GetIsRetainedByInteriorProxy()) );
}

Vector3  CNetObjEntity::GetPosition() const
{
	CEntity* pEntity = GetEntity();
	VALIDATE_GAME_OBJECT(pEntity);

	Vector3 fallbackPos(0.0f, 0.0f, 0.0f);

	if (pEntity)
	{
		fallbackPos = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
	}

	return fallbackPos;
}

void  CNetObjEntity::SetPosition( const Vector3& pos )
{
	m_LastReceivedMatrix.d = pos;
}

// name:		CNetObjEntity::SetOverridingRemoteVisibility
// description: Sets the override visibility flag and override value for remote machines.
//
void CNetObjEntity::SetOverridingRemoteVisibility(bool bOverride, bool bVisible, const char* BANK_ONLY(reason))
{
	gnetAssert(!IsClone());
	gnetAssert(!IsPendingOwnerChange());

#if __BANK
	if (ms_logLocalVisibilityOverride && (bOverride != m_RemoteVisibilityOverride.m_Active || (m_RemoteVisibilityOverride.m_Active && m_RemoteVisibilityOverride.m_Visible != bVisible)))
	{
		NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "OVERRIDING_REMOTE_VISIBILITY", GetLogName());
		NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Override", bOverride ? "true" : "false");
		NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Visible", bVisible ? "true" : "false");
		NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Reason", reason);
	}
#endif

	m_RemoteVisibilityOverride.m_Active  = bOverride;
	m_RemoteVisibilityOverride.m_Visible = bVisible;
	m_RemoteVisibilityOverride.m_Set = true;
}

// name:		CNetObjEntity::SetOverridingRemoteCollision
// description: Sets the override collision flag and override value for remote machines.
//
void CNetObjEntity::SetOverridingRemoteCollision(bool bOverride, bool bCollision, const char* BANK_ONLY(reason))
{
	gnetAssert(!IsClone());
	gnetAssert(!IsPendingOwnerChange());
	
#if __BANK
	if (ms_logLocalVisibilityOverride && (bOverride != m_RemoteCollisionOverride.m_Active || (m_RemoteCollisionOverride.m_Active && m_RemoteCollisionOverride.m_Collision != bCollision)))
	{
		NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "OVERRIDING_REMOTE_COLLISION", GetLogName());
		NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Override", bOverride ? "true" : "false");
		NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Collision", bCollision ? "true" : "false");
		NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Reason", reason);
	}
#endif

	m_RemoteCollisionOverride.m_Active  = bOverride;
	m_RemoteCollisionOverride.m_Collision = bCollision;
}

// name:		CNetObjEntity::SetOverridingLocalVisibility
// description: Sets the override visibility flag and override value for the local machines.
//
void CNetObjEntity::SetOverridingLocalVisibility(bool bVisible, const char* BANK_ONLY(reason), bool bIncludeChildAttachments, bool BANK_ONLY(bLogScriptName))
{
	CEntity* pEntity = GetEntity();
	VALIDATE_GAME_OBJECT(pEntity);

	if (!m_LocalVisibilityOverride.m_Active && pEntity)
	{
		// back up the visibility state of the local object so we know how to restore it after it is overridden
		m_bIsVisible = pEntity->GetIsVisibleForModule( SETISVISIBLE_MODULE_SCRIPT );
	}

#if __BANK
	if (ms_logLocalVisibilityOverride && ((!m_LocalVisibilityOverride.m_Active && !m_LocalVisibilityOverride.m_SetLastFrame) || (m_LocalVisibilityOverride.m_Active && m_LocalVisibilityOverride.m_Visible != bVisible)))
	{
		NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "OVERRIDING_LOCAL_VISIBILITY", GetLogName());
		NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Visible", bVisible ? "true" : "false");
		NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Reason", reason);
		NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Cached visibility", m_bIsVisible ? "true" : "false");

		if (bLogScriptName)
		{
			NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Script", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
#endif

	m_LocalVisibilityOverride.m_Active					= true;
	m_LocalVisibilityOverride.m_Visible					= bVisible;
	m_LocalVisibilityOverride.m_Set						= true;
	m_LocalVisibilityOverride.m_SetLastFrame			= true;
	m_LocalVisibilityOverride.m_IncludeChildAttachments = bIncludeChildAttachments;
}

// name:		CNetObjEntity::SetOverridingLocalCollision
// description: Sets the override collision flag and override value for the local machines.
//
void CNetObjEntity::SetOverridingLocalCollision(bool bCollision, const char* BANK_ONLY(reason))
{
	CEntity* pEntity = GetEntity();
	VALIDATE_GAME_OBJECT(pEntity);

    if(!CanOverrideLocalCollision())
    {
        return;
    }

	if (!m_LocalCollisionOverride.m_Active && pEntity)
	{
		// back up the collision state of the local object so we know how to restore it after it is overridden
		m_bUsesCollision = GetCachedEntityCollidableState();
	}

#if __BANK
	if (ms_logLocalVisibilityOverride && ((!m_LocalCollisionOverride.m_Active && !m_LocalCollisionOverride.m_SetLastFrame) || (m_LocalCollisionOverride.m_Active && m_LocalCollisionOverride.m_Collision != bCollision)))
	{
		NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "OVERRIDING_LOCAL_COLLISION", GetLogName());
		NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Collision", bCollision ? "true" : "false");
		NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Reason", reason);
		NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Cached collision", "%s", m_bUsesCollision ? "true" : "false");
	}
#endif

	m_LocalCollisionOverride.m_Active		= true;
	m_LocalCollisionOverride.m_Collision	= bCollision;
	m_LocalCollisionOverride.m_Set			= true;
	m_LocalCollisionOverride.m_SetLastFrame = true;
}

void CNetObjEntity::ClearOverridingLocalCollisionImmediately(const char* BANK_ONLY(reason))
{
	if (m_LocalCollisionOverride.m_Active)
	{
		m_LocalCollisionOverride.m_Active = false;

		if (GetEntity())
		{
			// restore cached collision
			if (m_bUsesCollision)
			{
				if (CanEnableCollision())
				{
					GetEntity()->EnableCollision();
				}
				else
				{
					m_EnableCollisonPending = true;
				}
			}
			else
			{
				DisableCollision();
			}
		}
	}
#if __BANK
	if (ms_logLocalVisibilityOverride)
	{
		NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "CLEAR_OVERRIDING_LOCAL_COLLISION", GetLogName());
		NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Reason", reason);
		NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Cached collision", "%s", m_bUsesCollision ? "true" : "false");
		NetworkInterface::GetObjectManager().GetLog().WriteDataValue("CanEnableCollision()", "%s", CanEnableCollision() ? "true" : "false");
		NetworkInterface::GetObjectManager().GetLog().WriteDataValue("m_EnableCollisonPending", "%s", m_EnableCollisonPending ? "true" : "false");
	}
#endif

}

void CNetObjEntity::FlagForDeletion()
{
#if ENABLE_NETWORK_LOGGING
    netLoggingInterface *log = GetLog();

    if(log)
    {
		Displayf("%s FLAGGED FOR DELETION", GetLogName());
		sysStack::PrintStackTrace();
        NetworkLogUtils::WritePlayerText(*log, GetPhysicalPlayerIndex(), "FLAG_FOR_DELETION", GetLogName());
    }
#endif

    m_deletionTimer = DELETION_TIME;
}

// name:		CNetObjEntity::HideForCutscene
// description: hides an entity for a cutscene 
//
void CNetObjEntity::HideForCutscene()
{
	if (IsLocalFlagSet(CNetObjGame::LOCALFLAG_SHOWINCUTSCENE))
    {
        if (IsClone())
        {
            // make visible on our machine
            SetOverridingLocalVisibility(true, "Exposed for cutscene");
        }
        else if (!IsPendingOwnerChange() && !m_ShowRemotelyInCutsene)
        {
            // make invisible on remote machines
            SetOverridingRemoteVisibility(true, false, "Hidden for cutscene");
        }
    }
    else
	{
        // make invisible on our machine
        SetOverridingLocalVisibility(false, "Hidden for cutscene");
    }

	bool isCurrentlyVisibleOverNetwork = false;
	CEntity* entity = GetEntity();
	if(entity)
	{
		isCurrentlyVisibleOverNetwork = m_LocalVisibilityOverride.m_Active ? m_bIsVisible : entity->GetIsVisibleForModule( SETISVISIBLE_MODULE_SCRIPT );
	}

	if (m_ShowRemotelyInCutsene && !IsClone() && !IsPendingOwnerChange() && !isCurrentlyVisibleOverNetwork)
	{
		SetOverridingRemoteVisibility(true, true, "Expose remotely during cutscene");
	}
}

// name:		CNetObjEntity::ExposeAfterCutscene
// description: exposes an entity after a cutscene 
//
void CNetObjEntity::ExposeAfterCutscene()
{
	// some entities created during the cutscene may need to remain after it is finished. It is up to the script to delete them immediately if not.
	if (IsLocalFlagSet(CNetObjGame::LOCALFLAG_SHOWINCUTSCENE))
	{
		if(!m_KeepIsVisibleForCutsceneFlagSet)
		{
			SetLocalFlag(CNetObjGame::LOCALFLAG_SHOWINCUTSCENE, false);
		}
		SetLocalFlag(CNetObjGame::LOCALFLAG_NOCLONE, false);
	}
	else
	{
		// call this here so that it is called twice in the frame and the entity is made visible again by the end of the frame
		UpdateLocalVisibility();
	}

	// make visible on remote machines
	if (m_RemoteVisibilityOverride.m_Active && !IsClone() && !IsPendingOwnerChange())
	{
		SetOverridingRemoteVisibility(false, true, "Expose remotely after cutscene");
	}

	m_ShowRemotelyInCutsene = false;
}

// name:		ManageUpdateLevel
// description:	Decides what the default update level should be for this object
void CNetObjEntity::ManageUpdateLevel(const netPlayer& player)
{
	CEntity* pEntity = GetEntity();
	VALIDATE_GAME_OBJECT(pEntity);

	if (!pEntity)
		return;

    BANK_ONLY(NetworkDebug::AddManageUpdateLevelCall());

	u8 newUpdateLevel = CNetworkSyncDataULBase::UPDATE_LEVEL_VERY_LOW;

	float dist = (VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()) - NetworkInterface::GetPlayerCameraPosition(player)).XYMag2();

	if (IsImportant())
	{
		// important entities are updated at a higher rate than ambient entities
		bool isInCutscene = NetworkInterface::IsInMPCutscene() && pEntity->GetIsVisible();

		if(isInCutscene || dist < rage::square(GetScopeData().m_syncDistanceNear) || (NetworkDebug::UsingPlayerCameras() && IsVisibleToPlayer(player)))
		{
			newUpdateLevel = CNetworkSyncDataULBase::UPDATE_LEVEL_VERY_HIGH;
		}
		else if (dist < rage::square(GetScopeData().m_syncDistanceFar))
		{
			newUpdateLevel = CNetworkSyncDataULBase::UPDATE_LEVEL_HIGH;
		}
		else if(dist < rage::square(GetScopeData().m_scopeDistance))
		{
			newUpdateLevel = CNetworkSyncDataULBase::UPDATE_LEVEL_MEDIUM;
		}
		else
		{
			newUpdateLevel = CNetworkSyncDataULBase::UPDATE_LEVEL_VERY_LOW;
		}
	}
	else
	{
		// general ambient entity update levels
		if (dist < rage::square(GetScopeData().m_syncDistanceNear))
		{
			newUpdateLevel = CNetworkSyncDataULBase::UPDATE_LEVEL_HIGH;
		}
		else if (NetworkDebug::UsingPlayerCameras() && IsVisibleToPlayer(player))
		{
			newUpdateLevel = CNetworkSyncDataULBase::UPDATE_LEVEL_MEDIUM;
		}
		else if (dist < rage::square(GetScopeData().m_syncDistanceFar))
		{
			newUpdateLevel = CNetworkSyncDataULBase::UPDATE_LEVEL_LOW;
		}
		else
		{
			newUpdateLevel = CNetworkSyncDataULBase::UPDATE_LEVEL_VERY_LOW;
		}
	}

	u8 highestLevel = GetHighestAllowedUpdateLevel();
	u8 lowestLevel  = GetLowestAllowedUpdateLevel();

	if (highestLevel < newUpdateLevel)
	{
		newUpdateLevel = highestLevel;
	}
	else if (lowestLevel > newUpdateLevel)
	{
		newUpdateLevel = lowestLevel;
	}

	SetUpdateLevel(player.GetPhysicalPlayerIndex(), newUpdateLevel);
}

// name:        Update
// description: Called once a frame to do special network object related stuff
// Returns		:	True if the object wants to unregister itself
bool CNetObjEntity::Update()
{
	CEntity* pEntity = GetEntity();
	VALIDATE_GAME_OBJECT(pEntity);

	if (!pEntity)
		return false;

	NETWORK_DEBUG_BREAK_FOR_FOCUS(BREAK_TYPE_FOCUS_UPDATE, *this);

	if (m_deletionTimer > 0)
	{
		m_deletionTimer--;

		if (!IsClone())
		{
			if(!IsPendingOwnerChange())
			{
				if(CanDeleteWhenFlaggedForDeletion(m_deletionTimer == 0))
				{
					NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "OBJECT_FLAGGED_FOR_DELETION", GetLogName());
					return true;
				}
				else
				{
					NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "DELETION_FAILED", GetLogName());
					NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Reason", "!CanDeleteWhenFlaggedForDeletion()");
					NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Deletion Timer", "%d", m_deletionTimer);
				}
			}
			else
			{
				NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "DELETION_FAILED", GetLogName());
				NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Reason", "Pending owner change");
				NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Deletion Timer", "%d", m_deletionTimer);
			}
		}

		if(m_deletionTimer == 0)
		{
			NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "DELETION_FAILED", GetLogName());
			NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Reason", "Timer expired");
			NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Deletion Timer", "%d", m_deletionTimer);
		}
	}

	// possibly hide the entity if there is a cutscene
	if (NetworkInterface::IsInMPCutscene())
	{
		HideForCutscene();
	}

	if(!IsClone())
	{
		UpdateForceSendRequests();
	}

#if __ASSERT
	// catch entities that have not been added to the world properly. This is to help track down a bug with invisible parked cars
	if (GetObjectType() != NET_OBJ_TYPE_PICKUP && !pEntity->IsRetainedFlagSet() && !pEntity->GetOwnerEntityContainer())
	{
		if(!fwTimer::IsGamePaused())
		{
			m_notInWorldTimer += fwTimer::GetTimeStepInMilliseconds();
		}
		u32 notInWorldMax = 10000;
		if (pEntity->GetIsTypeVehicle())
		{
			CVehicle* veh = (CVehicle*)pEntity;
			if(veh->GetIsScheduled())
			{
				// Scheduled vehicles can sit around for a while, especially when MP is loading up
				// Give them more time before we complain
				notInWorldMax = 60000;
				
			}
			gnetAssert(!veh->GetIsInPopulationCache());	

			// JW: removing the problem vehicle for sub3 rather than just complaining about it.
			if ( (m_notInWorldTimer > notInWorldMax) && !veh->GetCurrentPhysicsInst()->IsInLevel())
			{
				FlagForDeletion();			// would like to find the cause of this in NG, eventually.
			}
		}	
	}
	else
	{
		m_notInWorldTimer = 0;
	}
#endif // __ASSERT

#if __BANK
	if (!PARAM_stopCloneFailChecks.Get() && IsScriptObject() && !IsClone())
	{
		unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
		const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

		netLoggingInterface &log = NetworkInterface::GetObjectManager().GetLog();

		bool bCloneFail = false;

		PlayerFlags clonedState, createState, removeState;
		GetClonedState(clonedState, createState, removeState);

		for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
		{
			const CNetGamePlayer *pPlayer = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);
			PhysicalPlayerIndex playerFlag = (1<<pPlayer->GetPhysicalPlayerIndex());

			if (!(clonedState & playerFlag))
			{
				bool pendingCloning = (createState & playerFlag) != 0;

				if (pendingCloning || IsInScope(*pPlayer)) 
				{
					bCloneFail = true;

					if (m_cloneCreateFailTimer >= 5000)
					{
						NetworkLogUtils::WriteLogEvent(log, "CLONE_CREATE_FAIL", GetLogName());
						log.WriteDataValue("Player", pPlayer->GetLogName());
						log.WriteDataValue("Pending cloning", pendingCloning ? "true" : "false");
					}
				}
			}
		}

		if (bCloneFail)
		{
			if (m_cloneCreateFailTimer < 5000)
			{
				m_cloneCreateFailTimer += fwTimer::GetTimeStepInMilliseconds();
			}
		}
		else
		{
			m_cloneCreateFailTimer = 0;
		}
	}
	else
	{
		m_cloneCreateFailTimer = 0;
	}
#endif // __BANK

	//If local Flag TELEPORT is set and teleport has finished then reset the flag
	if (!IsClone() && IsLocalFlagSet(CNetObjGame::LOCALFLAG_TELEPORT) && CGameLogic::HasTeleportPlayerFinished() && camInterface::IsFadedIn())
	{
		SetLocalFlag(CNetObjGame::LOCALFLAG_TELEPORT, false);
	}

	if (IsScriptObject())
	{
		if (!IsLocalFlagSet(LOCALFLAG_COUNTED_BY_RESERVATIONS))
		{
			CTheScripts::GetScriptHandlerMgr().AddToReservationCount(*this);
		}
	}
	else if (IsLocalFlagSet(LOCALFLAG_COUNTED_BY_RESERVATIONS))
	{
		CTheScripts::GetScriptHandlerMgr().RemoveFromReservationCount(*this);
	}

    return CNetObjProximityMigrateable::Update();
}

bool CNetObjEntity::CanDeleteWhenFlaggedForDeletion(bool UNUSED_PARAM(useLogging))
{
	CEntity* pEntity = GetEntity();
	VALIDATE_GAME_OBJECT(pEntity);

	if (!pEntity)
		return false;
	return pEntity->CanBeDeleted();
}

void CNetObjEntity::OnUnregistered()
{
	if (IsLocalFlagSet(LOCALFLAG_COUNTED_BY_RESERVATIONS) && !CNetwork::GetObjectManager().IsShuttingDown())
	{
		CTheScripts::GetScriptHandlerMgr().RemoveFromReservationCount(*this);
	}

	CNetObjProximityMigrateable::OnUnregistered();
}

bool CNetObjEntity::IsInScope(const netPlayer& player, unsigned* scopeReason) const
{
    CEntity *pEntity = GetEntity();

    // don't clone entities until they have been added to the game world, this causes
    // issues with car generators which create cars and don't add them to the world for
    // several frames later - but this sounds a sensible thing to do for all entity types
    if(pEntity && !CGameWorld::HasEntityBeenAdded(pEntity))
    {
		if (scopeReason)
		{
			*scopeReason = SCOPE_OUT_ENTITY_NOT_ADDED_TO_WORLD;
		}
        return false;
    }

    return CNetObjProximityMigrateable::IsInScope(player, scopeReason);
}

bool CNetObjEntity::CanPassControl(const netPlayer& player, eMigrationType migrationType, unsigned *resultCode) const
{
	CEntity* pEntity = GetEntity();
	VALIDATE_GAME_OBJECT(pEntity);

	if (m_deletionTimer > 0)
    {
        if(resultCode)
        {
            *resultCode = CPC_FAIL_DELETION_TIMER;
        }
		return false;
    }

    if(m_RemoteVisibilityOverride.m_Active || m_RemoteCollisionOverride.m_Active)
    {
        if(resultCode)
        {
            *resultCode = CPC_FAIL_OVERRIDING_CRITICAL_STATE;
        }
        return false;
    }

	// can't pass control if the entity is about to be destroyed
	if (pEntity && pEntity->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD))
    {
        if(resultCode)
        {
            *resultCode = CPC_FAIL_REMOVING_FROM_WORLD;
        }
		return false;
    }

    if(pEntity && !CGameWorld::HasEntityBeenAdded(pEntity))
    {
        if(resultCode)
        {
            *resultCode = CPC_FAIL_NOT_ADDED_TO_WORLD;
        }

        return false;
    }

	return CNetObjProximityMigrateable::CanPassControl(player, migrationType, resultCode);
}

bool CNetObjEntity::CanAcceptControl(const netPlayer& player, eMigrationType migrationType, unsigned *resultCode) const
{
    CEntity* pEntity = GetEntity();
    VALIDATE_GAME_OBJECT(pEntity);

    if(pEntity && !CGameWorld::HasEntityBeenAdded(pEntity))
    {
        if(resultCode)
        {
            *resultCode = CAC_FAIL_NOT_ADDED_TO_WORLD;
        }

        return false;
    }

    return CNetObjProximityMigrateable::CanAcceptControl(player, migrationType, resultCode);
}

// Name         :   ChangeOwner
// Purpose      :   Called when the player controlling this object changes
// Parameters   :   playerIndex - the id of the new player controlling this object
//					migrationType - how the entity migrated
void CNetObjEntity::ChangeOwner(const netPlayer& player, eMigrationType migrationType)
{
	CEntity* pEntity = GetEntity();
	VALIDATE_GAME_OBJECT(pEntity);

    bool bWasClone = IsClone();

	CNetObjProximityMigrateable::ChangeOwner(player, migrationType);

	if (!bWasClone && IsClone())
	{
		gnetAssert(!m_RemoteVisibilityOverride.m_Active && !m_RemoteCollisionOverride.m_Active);

		if (pEntity)
		{
			m_LastReceivedMatrix = MAT34V_TO_MATRIX34(pEntity->GetTransform().GetMatrix());
		}

		// make sure the cached collision state is correct
		m_bUsesCollision = GetCachedEntityCollidableState();

		NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Cached collision", m_bUsesCollision ? "true" : "false");
	}

	m_ForceSendEntityScriptGameState = false;
	m_FrameForceSendRequested =0;
}

//
// name:		CNetObjEntity::UpdateBeforePhysics
// description:	Possibly overrides the co
void CNetObjEntity::UpdateBeforePhysics()
{
	CEntity* pEntity = GetEntity();
	VALIDATE_GAME_OBJECT(pEntity);

	if (m_LocalCollisionOverride.m_Active)
	{
		if (m_LocalCollisionOverride.m_Set)
		{
			if (pEntity)
			{
				// possibly override local collision
				if (m_bUsesCollision != m_LocalCollisionOverride.m_Collision)
				{
					if (m_LocalCollisionOverride.m_Collision)
					{
						if (CanEnableCollision())
						{
							pEntity->EnableCollision();
						}
					}
					else
					{
						DisableCollision();
					}
				}
			}

			m_LocalCollisionOverride.m_Set = false;
		}
		else
		{
			m_LocalCollisionOverride.m_Active = false;

			// restore cached collision
			if (GetEntity())
			{
				if (m_bUsesCollision)
				{
					if (CanEnableCollision())
					{
						GetEntity()->EnableCollision();
					}
					else
					{
						m_EnableCollisonPending = true;
					}
				}
				else
				{
					DisableCollision();
				}
			}
		}
	}
	else 
	{
		if (m_LocalCollisionOverride.m_SetLastFrame)
		{
			NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "STOP_OVERRIDING_LOCAL_COLLISION", GetLogName());
			NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Cached collision", m_bUsesCollision ? "true" : "false");
			NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Can enable collision", CanEnableCollision() ? "true" : "false");
			NetworkInterface::GetObjectManager().GetLog().WriteDataValue("m_EnableCollisonPending", m_EnableCollisonPending ? "true" : "false");
			m_LocalCollisionOverride.m_SetLastFrame = false;
		}
	
		if (m_EnableCollisonPending && CanEnableCollision())
		{
			NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "RESTORE_CACHED_COLLISION", GetLogName());
			SetUsesCollision(m_bUsesCollision, "Restore cached state");
		}

#ifdef __ASSERT
		if (m_EnableCollisonPending)
		{
			gnetAssertf(m_bUsesCollision, "%s has m_EnableCollisonPending set but no cached collision", GetLogName());
		}
#endif
	}
}


void CNetObjEntity::UpdateAfterScripts()
{
	UpdateLocalVisibility();
}

//
// name:		CNetObjEntity::IsVisibleToPlayer
// description:	works out if the given player can currently see this entity
bool CNetObjEntity::IsVisibleToPlayer(const netPlayer& player, Vector3* pNewPos) const
{
	CEntity* pEntity = GetEntity();
	VALIDATE_GAME_OBJECT(pEntity);

	//Make sure the palyer has a player ped
	if(pEntity && ((CNetGamePlayer&)player).GetPlayerPed())
	{
		Vector3 entityPos;
		Vector3 centre;
		Vector3 diff;

		if (pNewPos)
			entityPos = *pNewPos;
		else
			entityPos = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());

		if (player.IsMyPlayer())
		{
			return pEntity->GetIsOnScreen();
		}

		pEntity->GetBoundCentre(centre);

		if (pNewPos)
		{
			diff = centre - VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
			centre = *pNewPos + diff;
		}

		// get viewport
		grcViewport *viewport = NetworkUtils::GetNetworkPlayerViewPort((CNetGamePlayer&)player);
		if (gnetVerify(viewport))
		{
			return viewport->IsSphereVisible(centre.x, centre.y, centre.z, pEntity->GetBoundRadius()) != cullOutside;
		}
	}

	return false;
}

//
// name:		IsVisibleToAnyRemotePlayer
// description:	works out if any player can currently see this entity
bool CNetObjEntity::IsVisibleToAnyRemotePlayer() const
{
	unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
    const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

    for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
    {
        const CNetGamePlayer *remotePlayer = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);

        if (IsVisibleToPlayer(*remotePlayer))
        {
	        return true;
        }
    }

    return false;
}

bool CNetObjEntity::IsVisibleToScript() const
{
	CEntity *pEntity = GetEntity();
	VALIDATE_GAME_OBJECT(pEntity);

	bool visible =true;
	
	if (pEntity)
	{
		visible = m_LocalVisibilityOverride.m_Active ? m_bIsVisible : pEntity->GetIsVisibleForModule( SETISVISIBLE_MODULE_SCRIPT );
	}

	return visible;
}


void CNetObjEntity::ForceSendOfPositionData()
{
	CEntitySyncTreeBase* entitySyncTree = SafeCast(CEntitySyncTreeBase, GetSyncTree());
	entitySyncTree->ForceSendOfPositionData(*this, GetActivationFlags());
}

// sync parser getter functions
void CNetObjEntity::GetEntityScriptInfo(bool& hasScriptInfo, CGameScriptObjInfo& scriptInfo) const
{
	const CGameScriptObjInfo* pInfo = static_cast<const CGameScriptObjInfo*>(GetScriptObjInfo());

	hasScriptInfo = (pInfo != NULL);

	if (pInfo)
	{
		scriptInfo = *pInfo;
	}
}

void CNetObjEntity::GetEntityScriptGameState(CEntityScriptGameStateDataNode& data)
{
    CEntity *pEntity = GetEntity();
	VALIDATE_GAME_OBJECT(pEntity);

	data.m_isFixed             = pEntity && pEntity->IsBaseFlagSet(fwEntity::IS_FIXED);
    data.m_usesCollision       = IsLocalEntityCollidableOverNetwork();
	data.m_disableCollisionCompletely = !data.m_usesCollision && pEntity && pEntity->GetIsCollisionCompletelyDisabled();
}

void CNetObjEntity::GetEntityOrientation(CEntityOrientationDataNode& data)
{
    CEntity *pEntity = GetEntity();
	VALIDATE_GAME_OBJECT(pEntity);

	if (pEntity)
	{
		data.m_orientation = MAT34V_TO_MATRIX34(pEntity->GetMatrix());
	}
	else
	{
		data.m_orientation.Identity();
	}
}

// sync parser setter functions
void CNetObjEntity::SetEntityScriptInfo(bool hasScriptInfo, CGameScriptObjInfo& scriptInfo)
{
	if (hasScriptInfo)
	{
		SetScriptObjInfo(&scriptInfo);
	}
	else if (GetScriptObjInfo())
	{
		SetScriptObjInfo(NULL);
	}
}

void CNetObjEntity::SetEntityScriptGameState(const CEntityScriptGameStateDataNode& data)
{
    SetIsFixed(data.m_isFixed);
    SetUsesCollision(data.m_usesCollision, "SetEntityScriptGameState");
	if(!data.m_usesCollision && data.m_disableCollisionCompletely)
	{
		m_DisableCollisionCompletely = true;
		if(GetEntity())
			GetEntity()->SetIsCollisionCompletelyDisabled();
	}
	else
	{
		m_DisableCollisionCompletely = false;
	}
}

void CNetObjEntity::SetIsFixed(bool isFixed)
{
    CEntity *pEntity = GetEntity();
	VALIDATE_GAME_OBJECT(pEntity);

	if (pEntity)
	{
		CPhysical* physical = pEntity->GetIsPhysical() ? static_cast<CPhysical *>(pEntity) : 0;

		// Some physics objects that can become networked can never be activated by the physics,
        // for these cases it is an error to unfix them, even though their fixed flag may be unset
        // on the remote machine, this will be called by CProjectSyncTree::ResetScriptState in this case
        bool canActivate  = physical                                                      && 
                            physical->GetCurrentPhysicsInst()                             &&
                            physical->GetCurrentPhysicsInst()->GetArchetype()             &&
                            physical->GetCurrentPhysicsInst()->GetArchetype()->GetBound() &&
                            physical->GetCurrentPhysicsInst()->GetArchetype()->GetBound()->CanBecomeActive();
        bool canApplyData = isFixed || canActivate;

        if (canApplyData)
		{
		    bool bWasFixed = pEntity->IsBaseFlagSet(fwEntity::IS_FIXED);

#if DR_ENABLED
			debugPlayback::RecordFixedStateChange(*pEntity, isFixed, false);
#endif // DR_ENABLED

		    pEntity->AssignBaseFlag(fwEntity::IS_FIXED, isFixed);

		    // we need to update the physics system for physical entities when their fixed flag changes
		    if(physical)
		    {
			    physical->DisablePhysicsIfFixed();

			    if(!pEntity->IsBaseFlagSet(fwEntity::IS_FIXED) && bWasFixed)
			    {
				    physical->SetFixedPhysics(false);
			    }
		    }

			//Set local flag LOCALFLAG_ENTITY_FIXED so that the entity is moved straight to target in CNetObjPhysical::PostSync()
            bool fixedStateChanged = (!bWasFixed && isFixed) || (bWasFixed && !isFixed);

            if(fixedStateChanged && GetNetBlender())
            {
				SetLocalFlag(CNetObjGame::LOCALFLAG_ENTITY_FIXED, true);
            }
        }
	}
}

void CNetObjEntity::SetIsFixedWaitingForCollision(bool isFixedWaitingForCollision)
{
    CEntity *pEntity = GetEntity();
	VALIDATE_GAME_OBJECT(pEntity);

	if (pEntity)
	{
		pEntity->SetIsFixedWaitingForCollision(isFixedWaitingForCollision);
	}
}

void CNetObjEntity::SetIsVisible(bool isVisible, const char* LOGGING_ONLY(reason), bool UNUSED_PARAM(bNetworkUpdate))
{
    CEntity *pEntity = GetEntity();
	VALIDATE_GAME_OBJECT(pEntity);

	bool bVisibilityAltered = false;

	if (pEntity)
	{
		if (m_LocalVisibilityOverride.m_Active)
		{
			if (m_bIsVisible != isVisible)
			{
				bVisibilityAltered = true;
				m_bIsVisible = isVisible;
			}
		}
		else if (!pEntity->GetIsVisibleForModule(SETISVISIBLE_MODULE_SCRIPT) && isVisible)
		{
			SetIsVisibleForModuleSafe( pEntity, SETISVISIBLE_MODULE_SCRIPT, true, true );

			bVisibilityAltered = true;
		}
		else if (pEntity->GetIsVisibleForModule(SETISVISIBLE_MODULE_SCRIPT) && !isVisible)
		{
			SetIsVisibleForModuleSafe( pEntity, SETISVISIBLE_MODULE_SCRIPT, false, true );
			bVisibilityAltered = true;
		}
	}

#if ENABLE_NETWORK_LOGGING
	if (bVisibilityAltered)
	{
		if (IsClone())
		{
			NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "SETTING_CLONE_VISIBILITY", GetLogName());
		}
		else
		{
			NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "SETTING_LOCAL_VISIBILITY", GetLogName());
		}
		NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Reason", reason);
		NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Visible", "%s", isVisible ? "true" : "false");
		NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Cached", "%s", m_LocalVisibilityOverride.m_Active ? "true" : "false");
	}
#endif // ENABLE_NETWORK_LOGGING
}

void CNetObjEntity::SetUsesCollision(bool usesCollision, const char* LOGGING_ONLY(reason))
{
	CEntity *pEntity = GetEntity();
	VALIDATE_GAME_OBJECT(pEntity);

	LOGGING_ONLY(bool bCollisionAltered = m_bUsesCollision != usesCollision);

	m_bUsesCollision = usesCollision;

	if (!usesCollision)
	{
		m_EnableCollisonPending = false;
	}

	if (pEntity && !m_LocalCollisionOverride.m_Active)
	{
		if (usesCollision)
		{
			if (CanEnableCollision())
			{
				pEntity->EnableCollision();
				m_EnableCollisonPending = false;
			}
			else
			{
				m_EnableCollisonPending = true;
			}
		}
		else
		{
			DisableCollision();
		}
	}

#if ENABLE_NETWORK_LOGGING
	if (bCollisionAltered)
	{
		if (IsClone())
		{
			NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "SETTING_CLONE_COLLISION", GetLogName());
		}
		else
		{
			NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "SETTING_LOCAL_COLLISION", GetLogName());
		}
		NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Reason", reason);
		NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Collision", "%s", m_bUsesCollision ? "true" : "false");
		NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Cached", "%s", m_LocalCollisionOverride.m_Active ? "true" : "false");
		NetworkInterface::GetObjectManager().GetLog().WriteDataValue("CanEnableCollision()", "%s", CanEnableCollision() ? "true" : "false");
		NetworkInterface::GetObjectManager().GetLog().WriteDataValue("m_EnableCollisonPending", "%s", m_EnableCollisonPending ? "true" : "false");
	}
#endif // ENABLE_NETWORK_LOGGING
}

//void SetSector(u8 sectorX, u8 sectorY, u8 sectorZ);
void CNetObjEntity::SetEntityOrientation(const CEntityOrientationDataNode& data)
{
    m_LastReceivedMatrix.Set3x3(data.m_orientation);
}

// name:		CNetObjEntity::IsLocalEntityVisibleOverNetwork
// description: Returns whether the entity should be visible on remote machines.
//
bool CNetObjEntity::IsLocalEntityVisibleOverNetwork()
{
	CEntity *pEntity = GetEntity();
	VALIDATE_GAME_OBJECT(pEntity);

	bool visibleOverNetwork = false;
	
	if (pEntity)
	{
		visibleOverNetwork = m_LocalVisibilityOverride.m_Active ? m_bIsVisible : pEntity->GetIsVisibleForModule( SETISVISIBLE_MODULE_SCRIPT );

#if __ASSERT
		if (visibleOverNetwork && !m_LocalVisibilityOverride.m_Active && !pEntity->IsBaseFlagSet(fwEntity::IS_VISIBLE) && GetObjectType() != NET_OBJ_TYPE_PLAYER)
		{	
			if (!pEntity->GetIsVisibleForModule(SETISVISIBLE_MODULE_DEBUG))
			{
				gnetAssertf(0, "%s has been set invisible from MODULE_DEBUG, this will cause visibility issues across the network for this entity", GetLogName());
			}
			else if (!pEntity->GetIsVisibleForModule(SETISVISIBLE_MODULE_CAMERA))
			{
				// this is only used when warping the local player and his vehicle so don't assert
			}
			else if (!pEntity->GetIsVisibleForModule(SETISVISIBLE_MODULE_CUTSCENE))
			{
				// cutscenes now run in MP on clones
			}
			else if (!pEntity->GetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY))
			{
				// this can be set on peds in vehicles being respotted, or in planes exploding so don't assert
			}
			else if (!pEntity->GetIsVisibleForModule(SETISVISIBLE_MODULE_FRONTEND))
			{
				gnetAssertf(0, "%s has been set invisible from MODULE_FRONTEND, this will cause visibility issues across the network for this entity", GetLogName());
			}
			else if (!pEntity->GetIsVisibleForModule(SETISVISIBLE_MODULE_VFX))
			{
				gnetAssertf(0, "%s has been set invisible from MODULE_VFX, this will cause visibility issues across the network for this entity", GetLogName());
			}
			else if (!pEntity->GetIsVisibleForModule(SETISVISIBLE_MODULE_NETWORK))
			{
				// this is only used when locally hiding a network object so don't assert
			}
			else if (!pEntity->GetIsVisibleForModule(SETISVISIBLE_MODULE_PHYSICS))
			{
				gnetAssertf(0, "%s has been set invisible from MODULE_PHYSICS, this will cause visibility issues across the network for this entity", GetLogName());
			}
			else if (!pEntity->GetIsVisibleForModule(SETISVISIBLE_MODULE_WORLD))
			{
				// this can be set on local entities by the game world so is valid
			}
			else if (!pEntity->GetIsVisibleForModule(SETISVISIBLE_MODULE_DUMMY_CONVERSION))
			{
				gnetAssertf(0, "%s has been set invisible from MODULE_DUMMY_CONVERSION, this will cause visibility issues across the network for this entity", GetLogName());
			}
			else if (!pEntity->GetIsVisibleForModule(SETISVISIBLE_MODULE_PLAYER))
			{
				gnetAssertf(0, "%s has been set invisible from MODULE_PLAYER, this will cause visibility issues across the network for this entity", GetLogName());
			}
			else if (!pEntity->GetIsVisibleForModule(SETISVISIBLE_MODULE_FIRST_PERSON))
			{
				gnetAssertf(0, "%s has been set invisible from FIRST_PERSON, this will cause visibility issues across the network for this entity", GetLogName());
			}
			else if (!pEntity->GetIsVisibleForModule(SETISVISIBLE_MODULE_PICKUP))
			{
				// pickups are a special case
			}
			else if (!pEntity->GetIsVisibleForModule(SETISVISIBLE_MODULE_VEHICLE_RESPOT))
			{
				// this is only used when respotting a vehicle so don't assert
			}
			else if (!pEntity->GetIsVisibleForModule(SETISVISIBLE_MODULE_RESPAWN))
			{
				// this is only used when respawning a ped so don't assert
			}
			else if (!pEntity->GetIsVisibleForModule(SETISVISIBLE_MODULE_REPLAY))
			{
				gnetAssertf(0, "%s has been set invisible from REPALY, this will cause visibility issues across the network for this entity", GetLogName());
			}
			else
			{
				gnetAssertf(0, "%s has been set invisible from an unrecognised module, this will cause visibility issues across the network for this entity", GetLogName());
			}
		}
#endif
	}

	if(m_RemoteVisibilityOverride.m_Active)
	{
		visibleOverNetwork = m_RemoteVisibilityOverride.m_Visible;
	}

	return visibleOverNetwork;
}

// name:		CNetObjEntity::IsLocalEntityCollidableOverNetwork
// description: Returns whether the entity collides on remote machines.
//
bool CNetObjEntity::IsLocalEntityCollidableOverNetwork()
{
	CEntity *pEntity = GetEntity();
	VALIDATE_GAME_OBJECT(pEntity);

	bool collidableOverNetwork = false;

	if (pEntity)
	{
		collidableOverNetwork = (m_LocalCollisionOverride.m_Active || m_EnableCollisonPending) ? m_bUsesCollision : pEntity->IsCollisionEnabled();

		if(m_RemoteCollisionOverride.m_Active)
		{
			collidableOverNetwork = m_RemoteCollisionOverride.m_Collision;
		}
	}

	return collidableOverNetwork;
}

bool CNetObjEntity::GetCachedEntityCollidableState()
{
	CEntity *pEntity = GetEntity();
	VALIDATE_GAME_OBJECT(pEntity);

	if (IsClone() || m_LocalCollisionOverride.m_Active || m_EnableCollisonPending)
	{
		return m_bUsesCollision;
	}
	else if (!pEntity->GetIsAddedToWorld())
	{
		return true;
	}
	else
	{
		return pEntity->IsCollisionEnabled();
	}
}

void CNetObjEntity::UpdateLocalVisibility()
{
	CEntity* pEntity = GetEntity();
	VALIDATE_GAME_OBJECT(pEntity);

	if (pEntity)
	{
		if (!pEntity->GetIsVisibleForModule(SETISVISIBLE_MODULE_NETWORK))
		{
			SetIsVisibleForModuleSafe(pEntity, SETISVISIBLE_MODULE_NETWORK, true, true);
		}

		if (m_LocalVisibilityOverride.m_Active)
		{
			if (m_LocalVisibilityOverride.m_Set)
			{
				// possibly override local visibility
				if (m_LocalVisibilityOverride.m_Visible)
				{
					SetIsVisibleForModuleSafe(pEntity, SETISVISIBLE_MODULE_SCRIPT, true, m_LocalVisibilityOverride.m_IncludeChildAttachments);
				}
				else
				{
					SetIsVisibleForModuleSafe(pEntity, SETISVISIBLE_MODULE_NETWORK, false, m_LocalVisibilityOverride.m_IncludeChildAttachments);
				}

				m_LocalVisibilityOverride.m_Set = false;
			}
			else
			{
				// reset the local visibility override 
				m_LocalVisibilityOverride.m_Active = false;

				// restore cached visibility 
				SetIsVisibleForModuleSafe(pEntity, SETISVISIBLE_MODULE_SCRIPT, m_bIsVisible, true);
			}
		}
		else if (m_LocalVisibilityOverride.m_SetLastFrame)
		{
			NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "STOP_OVERRIDING_LOCAL_VISIBILITY", GetLogName());
			NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Cached visibility", m_bIsVisible ? "true" : "false");
			m_LocalVisibilityOverride.m_SetLastFrame = false;
		}
	}
}

void CNetObjEntity::SetIsVisibleForModuleSafe(CEntity* pEntity, const eIsVisibleModule module, bool bIsVisible, bool bProcessChildAttachments)
{
	if (gnetVerify(pEntity))
	{
		// SetIsVisibleForModule can allocate or deallocate memory (the visibility extension), so make sure the main game heap is active
		sysMemAllocator *oldHeap = &sysMemAllocator::GetCurrent();

		sysMemAllocator::SetCurrent(sysMemAllocator::GetMaster());
		pEntity->SetIsVisibleForModule( module, bIsVisible, bProcessChildAttachments );
		sysMemAllocator::SetCurrent(*oldHeap);
	}
}

void CNetObjEntity::DisableCollision()
{
	CEntity *pEntity = GetEntity();
	
	if (pEntity)
	{
		pEntity->DisableCollision();
	}
}

void CNetObjEntity::UpdateForceSendRequests()
{
	CEntitySyncTreeBase* entitySyncTree = SafeCast(CEntitySyncTreeBase, GetSyncTree());

	if(entitySyncTree)
	{
		u8 frameCount = static_cast<u8>(fwTimer::GetSystemFrameCount());

		if(frameCount != m_FrameForceSendRequested)
		{
			if(m_ForceSendEntityScriptGameState)
			{
				m_ForceSendEntityScriptGameState = false;

				entitySyncTree->ForceSendOfNodeData(SERIALISEMODE_UPDATE, GetActivationFlags(), this, *entitySyncTree->GetScriptGameStateNode());
			}
		}
	}
}




// Name         :   DisplayNetworkInfo
// Purpose      :   Displays debug info above the network object, players always have their name displayed
//                  above them, if the debug flag is set then all network objects have debug info displayed
//                  above them
void CNetObjEntity::DisplayNetworkInfo()
{
#if __BANK
	const NetworkDebug::NetworkDisplaySettings &displaySettings = NetworkDebug::GetDebugDisplaySettings();

	if (!GetEntity() || !GetEntity()->GetBaseModelInfo())
		return;

	if (!FindPlayerPed())
		return;

	float radius = 0.0f;
	Vector3 centre = VEC3_ZERO;

	radius = GetEntity()->GetBoundRadius();
	GetEntity()->GetBoundCentre(centre);

	if (!camInterface::IsSphereVisibleInGameViewport(centre, radius))
		return;

	bool displayNetworkInfo = displaySettings.m_displayObjectInfo || displaySettings.m_displayObjectScriptInfo || (displaySettings.m_displayTargets.IsAnyFlagSet() && displaySettings.m_displayInformation.IsAnyFlagSet());

	if (!displayNetworkInfo && GetObjectType() != NET_OBJ_TYPE_PLAYER)
		return;

	CPed* pFollowPed = FindFollowPed();

	if (pFollowPed && GetObjectType() != NET_OBJ_TYPE_PLAYER && displaySettings.m_DoObjectInfoRoomChecks && !displaySettings.m_displayVisibilityInfo)
	{
		const bool playerIsOutside = !pFollowPed->GetIsInInterior();
		const bool entityIsOutside = !GetEntity()->GetIsInInterior();
		const bool inSameRoom = (playerIsOutside && entityIsOutside) || (!playerIsOutside && !entityIsOutside);
		if(!inSameRoom)
			return;
	}

    const unsigned DEBUG_STR_LEN = 200;
	char debugStr[DEBUG_STR_LEN];

	float   scale = 1.0f;
	Vector2 screenCoords;
	if(NetworkUtils::GetScreenCoordinatesForOHD(GetPosition(), screenCoords, scale))
	{
		DisplaySyncTreeInfoForObject();

		float DEFAULT_CHARACTER_HEIGHT = 0.065f; // normalised value, CText::GetDefaultCharacterHeight() returns coordinates relative to screen dimensions

		//If it is a player and is inside a flying vehicle.
		CVehicle* vehicle = ((NET_OBJ_TYPE_PLAYER == GetObjectType() && GetPlayerOwner() && GetPlayerOwner()->GetPlayerPed() && GetPlayerOwner()->GetPlayerPed()->GetIsInVehicle()) ? GetPlayerOwner()->GetPlayerPed()->GetMyVehicle() : NULL);
		if (vehicle && (vehicle->GetIsAircraft() || vehicle->GetIsRotaryAircraft()))
		{
			DEFAULT_CHARACTER_HEIGHT = -0.151f;
		}

		const float DEBUG_TEXT_Y_INCREMENT   = (DEFAULT_CHARACTER_HEIGHT * scale * 0.5f); // amount to adjust screen coordinates to move down a line
		screenCoords.y += (GetNetworkInfoStartYOffset() * DEBUG_TEXT_Y_INCREMENT);

		// draw the text over objects owned by a player in the default colour for the player
		// based on their ID. This is so we can distinguish between objects owned by different
		// players on the same team
		NetworkColours::NetworkColour colour = NetworkUtils::GetDebugColourForPlayer(GetPlayerOwner());
		Color32 playerColour = NetworkColours::GetNetworkColour(colour);

		// display any additional debug info
		if(ShouldDisplayAdditionalDebugInfo())
		{
			DisplayNetworkInfoForObject(playerColour, scale, screenCoords, DEBUG_TEXT_Y_INCREMENT);

			if(displaySettings.m_displayInformation.m_displayOrientation)
			{
				GetOrientationDisplayString(debugStr);
				DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, debugStr));
				screenCoords.y += DEBUG_TEXT_Y_INCREMENT;
			}

			if(displaySettings.m_displayInformation.m_displayUpdateRate && !IsClone())
			{
				const netPlayer* pPlayer = NetworkDebug::GetPlayerFromUpdateRateIndex(displaySettings.m_displayInformation.m_updateRatePlayer);

				if (pPlayer && pPlayer->IsValid())
				{
					u32 updateLevel = GetUpdateLevel(pPlayer->GetPhysicalPlayerIndex());

					snprintf(debugStr, DEBUG_STR_LEN, "%s", GetUpdateLevelString(updateLevel));

					DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, debugStr));
					screenCoords.y += DEBUG_TEXT_Y_INCREMENT;
				}
			}

			if(displaySettings.m_displayInformation.m_displayEstimatedUpdateLevel)
			{
				netBlender *blender = GetNetBlender();

				if(blender)
				{
                    DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, blender->GetEstimatedUpdateLevelName()));
					screenCoords.y += DEBUG_TEXT_Y_INCREMENT;
				}
			}

			if(displaySettings.m_displayInformation.m_displayPredictionInfo)
			{
				if(IsClone())
				{
                    DisplayBlenderInfo(screenCoords, playerColour, scale, DEBUG_TEXT_Y_INCREMENT);
				}
			}

			if (displaySettings.m_displayInformation.m_displayInteriorInfo || 
				displaySettings.m_displayVisibilityInfo)
			{
				fwInteriorLocation loc = GetEntity()->GetInteriorLocation();

				if (IsInInterior())
				{
					if (!loc.IsValid() || (CInteriorInst::GetInteriorForLocation( loc )==NULL))
					{
						snprintf(debugStr, DEBUG_STR_LEN, "Interior room: Is Not Valid");
					}
					else if (loc.IsAttachedToRoom() && CInteriorInst::GetInteriorForLocation( loc )->IsOfficeMLO())
					{
						snprintf(debugStr, DEBUG_STR_LEN, "Interior room: In Office_Room_%d",loc.GetRoomIndex());
					}
					else if (loc.IsAttachedToRoom() && CInteriorInst::GetInteriorForLocation( loc )->IsSubwayMLO())
					{
						snprintf(debugStr, DEBUG_STR_LEN, "Interior room: In Subway_Room_%d",loc.GetRoomIndex());
					}
					else if (loc.IsAttachedToPortal() && CInteriorInst::GetInteriorForLocation( loc )->IsOfficeMLO())
					{
						snprintf(debugStr, DEBUG_STR_LEN, "Interior room: In Office_Portal_%d",loc.GetPortalIndex());
					}
					else if (loc.IsAttachedToPortal() && CInteriorInst::GetInteriorForLocation( loc )->IsSubwayMLO())
					{
						snprintf(debugStr, DEBUG_STR_LEN, "Interior room: In Subway_Portal_%d",loc.GetPortalIndex());
					}
					else if (loc.IsAttachedToRoom())
					{
						snprintf(debugStr, DEBUG_STR_LEN, "Interior room: In Room_%d",loc.GetRoomIndex());
					}
					else if (loc.IsAttachedToPortal())
					{
						snprintf(debugStr, DEBUG_STR_LEN, "Interior room: In Portal_%d",loc.GetPortalIndex());
					}
					else
					{
						snprintf(debugStr, DEBUG_STR_LEN, "Interior room: IsValid_%s", loc.IsValid() ? "Yes" : "No");
					}

					DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, debugStr));
					screenCoords.y += DEBUG_TEXT_Y_INCREMENT;

					if (GetEntity()->GetIsDynamic())
					{
						u32 myInteriorProxyLoc = 0;

						if (GetEntity()->GetIsRetainedByInteriorProxy())
						{
							myInteriorProxyLoc = GetEntity()->GetRetainingInteriorProxy();
						} 
						else
						{
							myInteriorProxyLoc = GetEntity()->GetInteriorLocation().GetAsUint32();
						}

						snprintf(debugStr, DEBUG_STR_LEN, "Interior location: %u", myInteriorProxyLoc);

						DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, debugStr));
						screenCoords.y += DEBUG_TEXT_Y_INCREMENT;
					}
				}
				else
				{
					DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, "Interior location: Outside"));
					screenCoords.y += DEBUG_TEXT_Y_INCREMENT;
				}

				if (GetEntity()->GetAlpha() < 255)
				{
					snprintf(debugStr, DEBUG_STR_LEN, "Alpha: %u", GetEntity()->GetAlpha());

					DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, debugStr));
					screenCoords.y += DEBUG_TEXT_Y_INCREMENT;
				}

				if (!IsClone())
				{
					if (!IsLocalEntityVisibleOverNetwork())
					{
						snprintf(debugStr, DEBUG_STR_LEN, "IsLocalEntityVisibleOverNetwork: false");
						DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, debugStr));
						screenCoords.y += DEBUG_TEXT_Y_INCREMENT;

						bool bActive = m_LocalVisibilityOverride.m_Active ? m_bIsVisible : GetEntity()->GetIsVisibleForModule( SETISVISIBLE_MODULE_SCRIPT );

						if (bActive)
						{
							//display second reason
							snprintf(debugStr, DEBUG_STR_LEN, "  m_RemoteVisibilityOverride: m_Active, m_Visible: false");
							DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, debugStr));
							screenCoords.y += DEBUG_TEXT_Y_INCREMENT;
						}
						else
						{
							//display first reason
							if (m_LocalVisibilityOverride.m_Active)
							{
								snprintf(debugStr, DEBUG_STR_LEN, "  m_LocalVisibilityOverride.m_Active, m_bIsVisible: false");
								DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, debugStr));
								screenCoords.y += DEBUG_TEXT_Y_INCREMENT;
							}
							else
							{
								snprintf(debugStr, DEBUG_STR_LEN, "  !m_LocalVisibilityOverride.m_Active, MODULE_SCRIPT: false");
								DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, debugStr));
								screenCoords.y += DEBUG_TEXT_Y_INCREMENT;
							}
						}
					}
				}
			}

            if(displaySettings.m_displayInformation.m_displayCanPassControl)
            {
                const netPlayer *player = NetworkDebug::GetPlayerFromUpdateRateIndex(displaySettings.m_displayInformation.m_updateRatePlayer);

				if(player && player->IsValid())
				{
                    unsigned resultCode = 0;
                    CanPassControl(*player, MIGRATE_PROXIMITY, &resultCode);

                    if(resultCode == CPC_SUCCESS)
                    {
                        DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, "Can Pass Control") );
                    }
                    else
                    {
                        DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, GetCanPassControlErrorString(resultCode, MIGRATE_PROXIMITY)));
                    }

					screenCoords.y += DEBUG_TEXT_Y_INCREMENT;
                }
            }

            if(displaySettings.m_displayInformation.m_displayCanAcceptControl)
            {
                const netPlayer *player = NetworkDebug::GetPlayerFromUpdateRateIndex(displaySettings.m_displayInformation.m_updateRatePlayer);

				if(player && player->IsValid())
				{
                    unsigned resultCode = 0;
                    CanAcceptControl(*player, MIGRATE_PROXIMITY, &resultCode);

                    if(resultCode == CAC_SUCCESS)
                    {
                        DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, "Can Accept Control") );
                    }
                    else
                    {
                        DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, NetworkUtils::GetCanAcceptControlErrorString(resultCode)));
                    }

                    screenCoords.y += DEBUG_TEXT_Y_INCREMENT;
                }
            }

			if(displaySettings.m_displayInformation.m_displayStealthNoise)
			{
				const netPlayer *player = NetworkDebug::GetPlayerFromUpdateRateIndex(displaySettings.m_displayInformation.m_updateRatePlayer);

				if (player == GetPlayerOwner())
				{
					snprintf(debugStr, DEBUG_STR_LEN, "Stealth noise: %f", static_cast<const CNetGamePlayer*>(player)->GetPlayerInfo()->GetStealthNoise());
					DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, debugStr) );
					screenCoords.y += DEBUG_TEXT_Y_INCREMENT;
				}
			}
		}

		// display the network object names of objects if they should be displayed
		if (displaySettings.m_displayObjectInfo)
		{
			if (GetScriptObjInfo() && !displaySettings.m_displayObjectScriptInfo)
			{
				if (IsPendingOwnerChange())
				{
					snprintf(debugStr, DEBUG_STR_LEN, "%s* (%s)", GetLogName(), static_cast<const CGameScriptId&>(GetScriptObjInfo()->GetScriptId()).GetScriptName());
				}
				else
				{
					snprintf(debugStr, DEBUG_STR_LEN, "%s %s (%s)", GetLogName(), (!IsClone() && GetSyncData()) ? "(S)" : "", static_cast<const CGameScriptId&>(GetScriptObjInfo()->GetScriptId()).GetScriptName());
				}
			}
			else 
			{
				if (IsPendingOwnerChange())
				{
					snprintf(debugStr, DEBUG_STR_LEN, "%s*", GetLogName());
				}
				else
				{
					bool usingFPCamera = false;
					if (GetObjectType() == NET_OBJ_TYPE_PLAYER)
					{
						CPed* playerPed = static_cast<CNetObjPlayer*>(this)->GetPlayerPed();
						if (playerPed)
						{
							usingFPCamera = IsClone() ? static_cast<CNetObjPlayer*>(this)->IsUsingFirstPersonCamera() : playerPed->IsFirstPersonShooterModeEnabledForPlayer(false);
						}
						
					}

					snprintf(debugStr, DEBUG_STR_LEN, "%s %s %s", GetLogName(), (!IsClone() && GetSyncData()) ? "(S)" : "", usingFPCamera ? "(FP)" : "");
				}
			}

			DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, debugStr));
			screenCoords.y += DEBUG_TEXT_Y_INCREMENT;

			/*snprintf(debugStr, DEBUG_STR_LEN, "%s_%d", GetObjectTypeName(), GetObjectID());

			DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, debugStr));
			screenCoords.y += DEBUG_TEXT_Y_INCREMENT;*/
		}

		if (displaySettings.m_displayObjectScriptInfo && GetScriptObjInfo())
		{
			DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, GetScriptObjInfo()->GetScriptId().GetLogName()));
			screenCoords.y += DEBUG_TEXT_Y_INCREMENT;

			snprintf(debugStr, DEBUG_STR_LEN, "%d", GetScriptObjInfo()->GetObjectId());
			DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, debugStr));
			screenCoords.y += DEBUG_TEXT_Y_INCREMENT;
		}
	}
#endif // __BANK
}

#if __BANK

const char *CNetObjEntity::GetNetworkBlenderIsOverridenString(unsigned resultCode) const
{
    return NetworkUtils::GetNetworkBlenderOverriddenReason(resultCode);
}

void CNetObjEntity::DisplayBlenderInfo(Vector2 &screenCoords, Color32 playerColour, float scale, float debugYIncrement) const
{
    const unsigned DEBUG_STR_LEN = 200;
    char debugStr[DEBUG_STR_LEN];

    unsigned resultCode = CB_SUCCESS;

    netBlenderLinInterp *netBlender = static_cast<netBlenderLinInterp *>(GetNetBlender());
	
	if (netBlender)
	{
		snprintf(debugStr, DEBUG_STR_LEN, "%s", netBlender->GetBlenderDataName());
		DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, debugStr));
		screenCoords.y += debugYIncrement;
	}	

    DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, netBlender->GetBlenderModeName()));
    screenCoords.y += debugYIncrement;

    if(NetworkBlenderIsOverridden(&resultCode))
    {
        float error = (netBlender->GetCurrentPredictedPosition() - GetPosition()).Mag();
        snprintf(debugStr, DEBUG_STR_LEN, "Error: %.2f", error);
        DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, debugStr));
        screenCoords.y += debugYIncrement;

        snprintf(debugStr, DEBUG_STR_LEN, "Network Blender Overridden: %s", GetNetworkBlenderIsOverridenString(resultCode));
        DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, debugStr));
        screenCoords.y += debugYIncrement;
    }
    else if(!CanBlend(&resultCode))
    {
        snprintf(debugStr, DEBUG_STR_LEN, "Can't Blend: %s", NetworkUtils::GetCanBlendErrorString(resultCode));
        DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, debugStr));
        screenCoords.y += debugYIncrement;
    }
    else
    {
        bool  blendedTooFar = false;
        float positionDelta = netBlender->GetPositionDelta(blendedTooFar);
        snprintf(debugStr, DEBUG_STR_LEN, "Position Delta: %.2f%s", positionDelta, blendedTooFar ? " (TOO FAR)" : "");
        DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, debugStr));
        screenCoords.y += debugYIncrement;

        if(netBlender->IsUsingLogarithmicBlending())
        {
            DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, "LOGARITHMIC"));
	        screenCoords.y += debugYIncrement;
        }

        const char *blendRampingString = netBlender->GetIsBlendRamping() ? "Blend Ramping" : " ";
        DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, blendRampingString));
	    screenCoords.y += debugYIncrement;

	    u32 timeSinceLastPop = (fwTimer::GetTimeInMilliseconds() > netBlender->GetLastPopTime()) ? (fwTimer::GetTimeInMilliseconds() - netBlender->GetLastPopTime()) : 0;
	    snprintf(debugStr, DEBUG_STR_LEN, "Time since last pop: %d", timeSinceLastPop);
	    DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, debugStr));
	    screenCoords.y += debugYIncrement;

	    snprintf(debugStr, DEBUG_STR_LEN, "Prediction Error: %.2f (TBU %d)", netBlender->GetPredictionError(), netBlender->GetTimeBetweenUpdates());
	    DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, debugStr));
	    screenCoords.y += debugYIncrement;

        CPhysical *physical = (GetEntity() && GetEntity()->GetIsPhysical()) ? static_cast<CPhysical *>(GetEntity()) : 0;

        if(physical)
        {
            float targetSpeed = netBlender->GetLastVelocityReceived().Mag();
            float actualSpeed = physical->GetVelocity().Mag();
            snprintf(debugStr, DEBUG_STR_LEN, "Target Speed: %.2f Actual Speed: %.2f Diff: %.2f", targetSpeed, actualSpeed, actualSpeed - targetSpeed);
	        DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, debugStr));
	        screenCoords.y += debugYIncrement;
        }

        snprintf(debugStr, DEBUG_STR_LEN, "Velocity Change: %.2f", netBlender->GetCurrentVelChange());
	    DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, debugStr));
	    screenCoords.y += debugYIncrement;

        snprintf(debugStr, DEBUG_STR_LEN, "Velocity Error: %.2f (peak: %.2f)", netBlender->GetVelocityError(), netBlender->GetVelocityErrorPeak());
	    DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, debugStr));
	    screenCoords.y += debugYIncrement;

		if (netBlender->DisableZBlending())
		{
			snprintf(debugStr, DEBUG_STR_LEN, "Z blending disabled");
			DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, debugStr));
			screenCoords.y += debugYIncrement;
		}

		if(NetworkDebug::IsPredictionFocusEntityID(GetObjectID()))
        {
            u32 timeBetweenUpdates = NetworkDebug::GetPredictionFocusEntityUpdateAverageTime();
            snprintf(debugStr, DEBUG_STR_LEN, "Average time between updates: %d", timeBetweenUpdates);
	        DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, debugStr));
	        screenCoords.y += debugYIncrement;
        }
    }
}

#endif // __BANK

// Name         :   ValidateMigration
// Purpose      :   Called when the entity is migrating. Sanity checks that it is ok for it to be migrating
#if __ASSERT
void CNetObjEntity::ValidateMigration() const
{
	gnetAssertf(!m_RemoteVisibilityOverride.m_Active && !m_RemoteCollisionOverride.m_Active, "An entity is migrating with remote visibility or collision override set");
}
#endif // __ASSERT
