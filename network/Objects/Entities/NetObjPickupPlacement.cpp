//
// name:        NetObjPickupPlacement.cpp
// description: Derived from netObject, this class handles all pickup placement related network object
//              calls. See NetworkPickup.h for a full description of all the methods.
// written by:  John Gurney
//

#include "network/Objects/entities/NetObjPickupPlacement.h"

// game headers
#include "network/network.h"
#include "network/Debug/NetworkDebug.h"
#include "network/Events/NetworkEventTypes.h"
#include "Network/Objects/NetworkObjectMgr.h"
#include "network/objects/NetworkObjectTypes.h"
#include "Network/Players/NetGamePlayer.h"
#include "peds/ped.h"
#include "peds/ped.h"
#include "pickups/Pickup.h"
#include "pickups/PickupManager.h"
#include "script/Handlers/GameScriptIds.h"
#include "script/Handlers/GameScriptMgr.h"
#include "script/script.h"
#include "renderer/DrawLists/drawListNY.h"
#include "text/text.h"
#include "text/TextConversion.h"

NETWORK_OPTIMISATIONS()

FW_INSTANTIATE_CLASS_POOL(CNetObjPickupPlacement, MAX_NUM_NETOBJPICKUPPLACEMENTS, atHashString("CNetObjPickupPlacement",0xf120209c));
FW_INSTANTIATE_CLASS_POOL(CNetObjPickupPlacement::CPickupPlacementSyncData, MAX_NUM_NETOBJPICKUPPLACEMENTS, atHashString("CPickupPlacementSyncData",0x2bbe0773));

CPickupPlacementSyncTree                          *CNetObjPickupPlacement::ms_pickupPlacementSyncTree;
CNetObjPickupPlacement::CPickupPlacementScopeData  CNetObjPickupPlacement::ms_pickupPlacementScopeData;

// ================================================================================================================
// CNetObjPickupPlacement
// ================================================================================================================

CNetObjPickupPlacement::CNetObjPickupPlacement(CPickupPlacement				*placement,
                                               const NetworkObjectType		type,
                                               const ObjectId				objectID,
                                               const PhysicalPlayerIndex    playerIndex,
                                               const NetObjFlags			localFlags,
                                               const NetObjFlags			globalFlags)
: CNetObjProximityMigrateable(placement, type, objectID, playerIndex, localFlags, globalFlags)
, m_regenerationTime(0)
, m_collector(NETWORK_INVALID_OBJECT_ID)
, m_MapPickup(false)
//, m_CloneToPlayersList(0)
{
	if (placement && placement->GetIsMapPlacement())
	{
		m_MapPickup = true;
		//m_CloneToPlayersList = placement->GetPlayersAvailability();
	}
}

void CNetObjPickupPlacement::CreateSyncTree()
{
    ms_pickupPlacementSyncTree = rage_new CPickupPlacementSyncTree;
}

void CNetObjPickupPlacement::DestroySyncTree()
{
	ms_pickupPlacementSyncTree->ShutdownTree();
    delete ms_pickupPlacementSyncTree;
    ms_pickupPlacementSyncTree = 0;
}

netINodeDataAccessor *CNetObjPickupPlacement::GetDataAccessor(u32 dataAccessorType)
{
	netINodeDataAccessor *dataAccessor = 0;

	if(dataAccessorType == IPickupPlacementNodeDataAccessor::DATA_ACCESSOR_ID())
	{
		dataAccessor = (IPickupPlacementNodeDataAccessor *)this;
	}
	else
	{
		dataAccessor = CNetObjProximityMigrateable::GetDataAccessor(dataAccessorType);
	}

	return dataAccessor;
}


const scriptObjInfoBase*	CNetObjPickupPlacement::GetScriptObjInfo() const
{
	CPickupPlacement* pPlacement = GetPickupPlacement();

	if (pPlacement)
	{
		return pPlacement->GetScriptInfo();
	}

	return NULL;
}

void CNetObjPickupPlacement::SetScriptObjInfo(scriptObjInfoBase* info)
{
	CPickupPlacement* pPlacement = GetPickupPlacement();

	if (AssertVerify(pPlacement))
	{
		if (info)
		{
			scriptHandler* pHandler = CTheScripts::GetScriptHandlerMgr().GetScriptHandler(info->GetScriptId());

			if (pPlacement->GetScriptInfo())
			{
				gnetAssert(*info == *pPlacement->GetScriptInfo());

			}
			else
			{
				pPlacement->SetScriptInfo(*info);

				if (pHandler)
				{
					pHandler->RegisterExistingScriptObject(*pPlacement);
				}
			}
		}
		else if (pPlacement->GetScriptInfo())
		{
			gnetAssertf(0, "Pickup placement already has script info"); // This should never happen. Placements will be removed when the script that created them terminates
		}
	}
}

scriptHandlerObject* CNetObjPickupPlacement::GetScriptHandlerObject() const
{
	return static_cast<scriptHandlerObject*>(GetPickupPlacement());
}

bool CNetObjPickupPlacement::IsInInterior() const
{
	return (GetPickupPlacement() && GetPickupPlacement()->GetRoomHash() != 0);
}

Vector3 CNetObjPickupPlacement::GetPosition() const
{
	CPickupPlacement* pPlacement = GetPickupPlacement();

	if (pPlacement)
	{
		return pPlacement->GetPickupPosition();
	}

	gnetAssertf(0, "CNetObjPickupPlacement::GetPosition() - no pickup placement");

	return VEC3_ZERO;
}

void CNetObjPickupPlacement::SetPosition(const Vector3&)
{
	// this function should never be called
	gnetAssert(0);
}

bool CNetObjPickupPlacement::Update()
{
	CPickupPlacement* pPlacement = GetPickupPlacement();

	CNetObjProximityMigrateable::Update();

	if (pPlacement)
	{
		m_MapPickup = pPlacement->GetIsMapPlacement();
	}

	if (AssertVerify(pPlacement) && !IsClone())
	{
		// collected regenerating placements can't be unregistered until they regenerate
		if (pPlacement->GetIsCollected() && pPlacement->GetRegenerates())
		{
			return false;
		}

		// don't unregister placements with pickups that can't be deleted
		if (pPlacement->GetPickup() && pPlacement->GetPickup()->GetNetworkObject() &&
			!pPlacement->GetPickup()->GetNetworkObject()->CanDelete())
		{
			return false;
		}

		// remove any pickup placements that are no longer needed
		if (IsLocalFlagSet(LOCALFLAG_NOLONGERNEEDED) && !IsPendingOwnerChange())
		{
			// wait until the pickup is destroyed first
			if (pPlacement->GetPickup() && pPlacement->GetPickup()->GetNetworkObject())
			{
				pPlacement->GetPickup()->GetNetworkObject()->SetLocalFlag(LOCALFLAG_NOLONGERNEEDED, true);
			}
			else
			{
				return true;
			}
		}
	}

	return false;
}

float CNetObjPickupPlacement::GetScopeDistance(const netPlayer* pRelativePlayer) const
{
	static const float MAP_PLACEMENT_ADDITIONAL_RANGE = 150.0f;
	static const float MAP_PLACEMENT_VEHICLE_RANGE = 500.0f;

	// map placements need to clone before they can be generated on other machines, to avoid them appearing briefly before the
	// clone create is received
	CPickupPlacement* pPlacement = GetPickupPlacement();

	if (AssertVerify(pPlacement))
	{
		if (pPlacement->GetIsMapPlacement())
		{
			// vehicle pickups need to have a much larger scope as vehicles move quickly and they are generally blipped on the map
			if (pPlacement->GetPickupData() && (pPlacement->GetPickupData()->GetCollectableInCar() || (pPlacement->GetPickup() && pPlacement->GetPickup()->IsFlagSet(CPickup::PickupFlags::PF_AllowCollectionInVehicle))))
			{
				return MAP_PLACEMENT_VEHICLE_RANGE;
			}

			return pPlacement->GetGenerationRange() + MAP_PLACEMENT_ADDITIONAL_RANGE;
		}
	}

	return CNetObjProximityMigrateable::GetScopeDistance(pRelativePlayer);
}

bool CNetObjPickupPlacement::IsInScope(const netPlayer& player, unsigned *scopeReason) const
{
	CPickupPlacement* pPlacement = GetPickupPlacement();
	/*
	if (m_MapPickup && m_CloneToPlayersList != 0)
	{
		if((m_CloneToPlayersList & (1 << player.GetPhysicalPlayerIndex())) == 0)
		{
			if (scopeReason)
			{
				*scopeReason = SCOPE_OUT_NOT_ALLOWED_FOR_THIS_PLAYER;
			}
			return false;
		}
	}
	*/
	if (!AssertVerify(pPlacement) || !AssertVerify(!pPlacement->GetIsBeingDestroyed()))
	{
		if (scopeReason)
		{
			*scopeReason = SCOPE_OUT_PICKUP_PLACEMENT_DESTROYED;
		}

		return false;
	}

	if (pPlacement->GetDebugCreated())
	{
		if (scopeReason)
		{
			*scopeReason = SCOPE_IN_PICKUP_DEBUG_CREATED;
		}

		return true;
	}

	if (pPlacement->GetIsMapPlacement())
	{
		if (HasBeenCloned(player) && CNetObjGame::IsInScope(player))
		{
			// collected map pickups must remain in scope until they regenerate
			if (scopeReason)
			{
				*scopeReason = SCOPE_IN_PICKUP_PLACEMENT_REGENERATING;
			}
			return true;
		}
		else
		{
			return CNetObjProximityMigrateable::IsInScope(player, scopeReason);
		}
	}

	// other placements are always in of players running the script that created them
	if (AssertVerify(pPlacement) && pPlacement->GetScriptHandler())
	{
		if (AssertVerify(pPlacement->GetScriptHandler()->GetNetworkComponent()) &&
			pPlacement->GetScriptHandler()->GetNetworkComponent()->IsPlayerAParticipant(player))
		{
			if (scopeReason)
			{
				*scopeReason = SCOPE_IN_SCRIPT_PARTICIPANT;
			}
			return true;
		}
	}

	*scopeReason = SCOPE_OUT_NOT_SCRIPT_PARTICIPANT;

	return false;
}

void CNetObjPickupPlacement::CleanUpGameObject(bool bDestroyObject)
{
    CPickupPlacement* pPlacement = GetPickupPlacement();

	if (AssertVerify(pPlacement))
	{
		pPlacement->SetNetworkObject(NULL);
		SetGameObject(0);

		// map placements are not removed, they remain until collected
		if (bDestroyObject && !pPlacement->GetIsBeingDestroyed())
		{
			if (pPlacement->GetIsMapPlacement())
			{
				if (!IsLocalFlagSet(CNetObjGame::LOCALFLAG_OWNERSHIP_CONFLICT))
				{
					// make sure the map placement has regenerated
					if (pPlacement->GetIsCollected() && !pPlacement->GetIsBeingDestroyed())
					{
						// need to restore the game heap here as MigratePlacementOwnership can allocate memory
						sysMemAllocator *oldHeap = &sysMemAllocator::GetCurrent();
						sysMemAllocator::SetCurrent(sysMemAllocator::GetMaster());

						CPickupManager::MigratePlacementOwnership(pPlacement, m_regenerationTime);				

						sysMemAllocator::SetCurrent(*oldHeap);
					}
				}
			}
			else
			{
				// need to restore the game heap here in case the pickup placement constructor/destructor allocates from the heap
				sysMemAllocator *oldHeap = &sysMemAllocator::GetCurrent();
				sysMemAllocator::SetCurrent(sysMemAllocator::GetMaster());

				CPickupManager::RemovePickupPlacement(pPlacement);

				sysMemAllocator::SetCurrent(*oldHeap);
			}
		}
	}
}

bool CNetObjPickupPlacement::CanPassControl(const netPlayer& player, eMigrationType migrationType, unsigned *resultCode) const
{
	CPickupPlacement* pPlacement = GetPickupPlacement();

	if (CNetObjProximityMigrateable::CanPassControl(player, migrationType, resultCode) && AssertVerify(pPlacement))
	{
		if (pPlacement->GetIsBeingDestroyed())
		{
			if(resultCode)
			{
				*resultCode = CPC_FAIL_PICKUP_PLACEMENT_DESTROYED;
			}
			return false;
		}

		if  (pPlacement->GetPickup() &&
			pPlacement->GetPickup()->GetNetworkObject() &&
			AssertVerify(pPlacement->GetPickup()->GetNetworkObject()->GetPlayerOwner() == GetPlayerOwner()))
		{
			// can't pass control of the placement if the corresponding pickup object is not cloned on the same machines
			u32 clonedState, createState, removeState;
			GetClonedState(clonedState, createState, removeState);

			u32 clonedStatePickup, createStatePickup, removeStatePickup;
			pPlacement->GetPickup()->GetNetworkObject()->GetClonedState(clonedStatePickup, createStatePickup, removeStatePickup);

			if (clonedState != clonedStatePickup || createState != createStatePickup || removeState != removeStatePickup)
			{
                if(resultCode)
                {
                    *resultCode = CPC_FAIL_PICKUP_PLACEMENT;
                }
				return false;
			}
		}

		// can't pass control until the collected state is acked
		if (!IsSyncedWithAllPlayers())
		{
			return false;
		}

		return true;
	}

	return false;
}

bool CNetObjPickupPlacement::CanAcceptControl(const netPlayer& player, eMigrationType migrationType, unsigned *resultCode) const
{
	CPickupPlacement* pPlacement = GetPickupPlacement();

	bool bCanAcceptControl = CNetObjProximityMigrateable::CanAcceptControl(player, migrationType, resultCode) && AssertVerify(pPlacement);

	if (bCanAcceptControl)
	{
		if (pPlacement->GetIsCollected() && pPlacement->GetPickup())
		{
			if (resultCode)
			{
				*resultCode = CAC_FAIL_COLLECTED_WITH_PICKUP;
			}

			bCanAcceptControl = false;
		}
	}

	if (bCanAcceptControl)
	{
		// can't accept control of an object that is set to only migrate to other machines running the same script if we are not running that script.
		if (!GetScriptObjInfo() && (IsGlobalFlagSet(GLOBALFLAG_SCRIPT_MIGRATION) || IsGlobalFlagSet(GLOBALFLAG_CLONEONLY_SCRIPT)))
		{
			if(resultCode)
			{
				*resultCode = CAC_FAIL_NOT_SCRIPT_PARTICIPANT;
			}

			bCanAcceptControl = false;
		}
	}

	return bCanAcceptControl;
}

// Name			:	ChangeOwner
// Purpose		:	change ownership from one player to another
// Parameters   :   player            - the new owner
//					bProximityMigrate - ownership is being changed during proximity migration
// Returns      :   void
void CNetObjPickupPlacement::ChangeOwner(const netPlayer& player, eMigrationType migrationType)
{
	CPickupPlacement* pPlacement = GetPickupPlacement();
	netPlayer* pPreviousOwner = GetPlayerOwner();

	if (player.IsLocal())
	{
		CPickupManager::MigratePlacementOwnership(GetPickupPlacement(), m_regenerationTime);
	}

	CNetObjGame::ChangeOwner(player, migrationType);

	// the pickup that this placement generated always migrates with the placement
    netObject *pickUpNetworkObject = 0;

    if(AssertVerify(pPlacement) && pPlacement->GetPickup())
    {
        pickUpNetworkObject = pPlacement->GetPickup()->GetNetworkObject();
    }

	if(pickUpNetworkObject && AssertVerify(pickUpNetworkObject->GetPlayerOwner() == pPreviousOwner))
	{
        // don't change owner of pickups that are being reassigned, this will be done as part of the object
        // reassignment process
        if(!pickUpNetworkObject->IsLocalFlagSet(netObject::LOCALFLAG_BEINGREASSIGNED))
        {
		    u32 clonedState, createState, removeState;
		    GetClonedState(clonedState, createState, removeState);

		    pPlacement->GetPickup()->GetNetworkObject()->SetClonedState(clonedState, createState, removeState);

		    CNetwork::GetObjectManager().ChangeOwner(*pPlacement->GetPickup()->GetNetworkObject(), player, migrationType);
		    gnetAssert(pPlacement->GetPickup()->GetNetworkObject()->GetPlayerOwner() == GetPlayerOwner());
        }
        else
        {
            gnetAssertf(pPreviousOwner == 0, "Changing owner of a pickup placement with a pickup being reassigned outside of the reassignment process!");
        }
	}
}

void CNetObjPickupPlacement::LogAdditionalData(netLoggingInterface &UNUSED_PARAM(log)) const
{
    // Intentionally empty function to override LogAdditionalData for CNetObjProximityMigrateable,
    // which is not valid for pickup placement object
}

// Name         :   TryToPassControlOutOfScope
// Purpose      :   Tries to pass control of this entity when it is going out of scope with our players.
// Parameters   :   
// Returns      :   true if object is not in scope with any other players and can be deleted.
bool CNetObjPickupPlacement::TryToPassControlOutOfScope()
{
	CPickupPlacement* pPlacement = GetPickupPlacement();

	if (!NetworkInterface::IsGameInProgress())
		return true;

	gnetAssert(!IsClone());

	if (IsPendingOwnerChange() || !gnetVerify(pPlacement))
		return false;

	if(!HasBeenCloned())
	{
		return true;
	}

	if (IsGlobalFlagSet(GLOBALFLAG_PERSISTENTOWNER))
		return false;

	const netPlayer* pNearestPlayer = NULL;

	// find closest player and pass control to it
	Vector3 diff;
	float closestDist2 = -1.0f;

	unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
    const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

    for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
    {
        const CNetGamePlayer *pPlayer = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);

		if (HasBeenCloned(*pPlayer) && pPlayer->GetPlayerPed() && pPlacement->GetInScope(pPlayer->GetPlayerPed()))
		{
			diff = NetworkUtils::GetNetworkPlayerPosition(*pPlayer) - GetPosition();
			const float dist2 = diff.XYMag2();

			if (!pNearestPlayer || dist2 < closestDist2)
			{
				if (CanPassControl(*pPlayer, MIGRATE_OUT_OF_SCOPE) &&
					NetworkInterface::GetObjectManager().RemotePlayerCanAcceptObjects(*pPlayer, true))
				{
					pNearestPlayer = pPlayer;
					closestDist2 = dist2;
				}
			}
		}
	}

	if (pNearestPlayer)
	{
		CGiveControlEvent::Trigger(*pNearestPlayer, this, MIGRATE_OUT_OF_SCOPE);
	}

	return false;
}

void CNetObjPickupPlacement::ConvertToAmbientObject()
{
	// pickup placements are just removed when converted to ambient objects
	SetLocalFlag(CNetObjGame::LOCALFLAG_NOLONGERNEEDED, true);
}

bool CNetObjPickupPlacement::CanDelete(unsigned* LOGGING_ONLY(reason))
{
	CPickupPlacement* pPlacement = GetPickupPlacement();

	// map pickups can always be deleted as they are removed on all machines by the scripts
	if (pPlacement && pPlacement->GetIsMapPlacement())
	{
		return true;
	}

	return CNetObjProximityMigrateable::CanDelete(LOGGING_ONLY(reason));
}

// sets the collector of this placement
void  CNetObjPickupPlacement::SetCollector(CPed* pCollector)
{
	if (pCollector)
	{
		if (AssertVerify(pCollector->GetNetworkObject()))
		{
			m_collector = pCollector->GetNetworkObject()->GetObjectID();

			gnetAssert(m_collector != NETWORK_INVALID_OBJECT_ID);
		}
	}
	else
	{
		m_collector = NETWORK_INVALID_OBJECT_ID;
	}
}

// sync parser getter functions
void CNetObjPickupPlacement::GetPickupPlacementCreateData(CPickupPlacementCreationDataNode& data)  const
{
	CPickupPlacement* pPlacement = GetPickupPlacement();

	if (AssertVerify(pPlacement))
	{
		data.m_mapPlacement = pPlacement->GetIsMapPlacement();

		if (!data.m_mapPlacement)
		{
			data.m_pickupPosition		= pPlacement->GetPickupPosition();
			data.m_pickupOrientation	= pPlacement->GetPickupOrientation();
			data.m_pickupHash			= pPlacement->GetPickupHash();
			data.m_placementFlags		= pPlacement->GetFlags();
			data.m_amount				= pPlacement->GetAmount();
			data.m_customModelHash		= pPlacement->GetCustomModelHash();
			data.m_customRegenTime		= pPlacement->GetCustomRegenerationTime();
			data.m_teamPermits			= pPlacement->GetTeamPermits();

			// don't sync blipped placements, all blipping is now being done locally
			data.m_placementFlags &= ~CPickupPlacement::PLACEMENT_CREATION_FLAG_BLIPPED_COMPLEX;

			// don't sync internal flags
			data.m_placementFlags &= (1<<(CPickupPlacement::PLACEMENT_FLAG_NUM_CREATION_FLAGS))-1;
		}

		if (!pPlacement->GetDebugCreated() && gnetVerifyf(pPlacement->GetScriptInfo(), "Pickup placement at %f, %f, %f of type %s is not associated with a script!", pPlacement->GetPickupPosition().x, pPlacement->GetPickupPosition().y, pPlacement->GetPickupPosition().z, pPlacement->GetPickupData()->GetName()))
		{
			data.m_scriptObjInfo = *pPlacement->GetScriptInfo();
		}
	}
}

void CNetObjPickupPlacement::GetPickupPlacementStateData(CPickupPlacementStateDataNode& data) const
{
	CPickupPlacement* pPlacement = GetPickupPlacement();

	data.m_collected			= false;
	data.m_destroyed			= false;
	data.m_collector			= NETWORK_INVALID_OBJECT_ID;
	data.m_regenerates			= false;
	data.m_regenerationTime		= 0;

	gnetAssert(pPlacement || m_MapPickup);

	if (pPlacement)
	{
		if (pPlacement->GetIsCollected() && AssertVerify(m_collector != NETWORK_INVALID_OBJECT_ID))
		{
			data.m_collector	 = m_collector;
			data.m_collected	 = true;
		}

		data.m_destroyed = pPlacement->GetHasPickupBeenDestroyed();

		data.m_regenerates = pPlacement->GetRegenerates();

		if (data.m_regenerates && (data.m_collected || data.m_destroyed))
		{
			// this can be called on clones when writing the important state checksum after a reassignment
			if (IsClone())
			{
				data.m_regenerationTime = m_regenerationTime;
			}
			else
			{
				data.m_regenerationTime = CPickupManager::GetRegenerationTime(pPlacement);
			}
		}
	}
}

// sync parser setter functions
void CNetObjPickupPlacement::SetPickupPlacementCreateData(CPickupPlacementCreationDataNode& data)
{
	CPickupPlacement* pPlacement = NULL;

	if (data.m_mapPlacement)
	{
		scriptHandler* pHandler = CTheScripts::GetScriptHandlerMgr().GetScriptHandler(data.m_scriptObjInfo.GetScriptId());

		if (AssertVerify(pHandler))
		{
			// find the map placement for this pickup
			scriptHandlerObject* pScriptObj = pHandler->GetScriptObject(data.m_scriptObjInfo.GetObjectId());

			if (AssertVerify(pScriptObj) && AssertVerify(pScriptObj->GetType() == SCRIPT_HANDLER_OBJECT_TYPE_PICKUP))
			{
				pPlacement = SafeCast(CPickupPlacement, pScriptObj);
				
				if (AssertVerify(pPlacement))
				{
					gnetAssert(!pPlacement->GetNetworkObject());
					pPlacement->SetNetworkObject(this);
				}
			}
		}
	}
	else
	{
		u32 customModelIndex = fwModelId::MI_INVALID;

		if (data.m_customModelHash != 0)
		{
			fwModelId modelId;
			CModelInfo::GetBaseModelInfoFromHashKey(data.m_customModelHash, &modelId);

			if (gnetVerifyf(modelId.IsValid(), "Unrecognised custom model hash for pickup placement clone"))
			{
				customModelIndex = modelId.GetModelIndex();
			}
		}

		pPlacement = CPickupManager::RegisterPickupPlacement(data.m_pickupHash, data.m_pickupPosition, data.m_pickupOrientation, static_cast<PlacementFlags>(data.m_placementFlags), this, customModelIndex);

		if (pPlacement)
		{
			pPlacement->SetAmount(static_cast<u16>(data.m_amount));
			pPlacement->SetTeamPermits(static_cast<u8>(data.m_teamPermits));

			if (data.m_customRegenTime != 0)
			{
				pPlacement->SetRegenerationTime(static_cast<u32>(data.m_customRegenTime*1000));
			}
		}
	}

	if (pPlacement)
	{
		SetGameObject((void*) pPlacement);

		if (!pPlacement->GetDebugCreated())
		{
			SetScriptObjInfo(&data.m_scriptObjInfo);
		}
	}
}

void CNetObjPickupPlacement::SetPickupPlacementStateData(const CPickupPlacementStateDataNode& data)
{
	CPickupPlacement* pPlacement = GetPickupPlacement();

	if (AssertVerify(pPlacement))
	{
		if (data.m_collected)
		{
			m_collector = data.m_collector;

			gnetAssert(m_collector != NETWORK_INVALID_OBJECT_ID);

			if (!pPlacement->GetIsCollected())
			{
				netObject* pObj = NetworkInterface::GetObjectManager().GetNetworkObject(data.m_collector);

				CPed* pPed = NetworkUtils::GetPedFromNetworkObject(pObj);

				// if we are waiting for a collection response for this pickup, process it now
				if (pPlacement->GetPickup() && pPlacement->GetPickup()->GetIsPendingCollection() && pPed && pPed->IsLocalPlayer())
				{
					pPlacement->GetPickup()->ProcessCollectionResponse(NULL, true);
				}
				else
				{
					CPickupManager::RegisterPickupCollection(pPlacement, pPed);
				}
			}	

			if (pPlacement->GetRegenerates())
			{
				m_regenerationTime = data.m_regenerationTime;
			}
		}
		else if (pPlacement->GetIsCollected())
		{
			pPlacement->Regenerate();
		}
	}

	if (data.m_destroyed)
	{
		pPlacement->SetPickupHasBeenDestroyed(false);

		if (pPlacement->GetRegenerates())
		{
			m_regenerationTime = data.m_regenerationTime;
		}
	}
}

// Name         :   DisplayNetworkInfo
// Purpose      :   Displays debug info above the network object, players always have their name displayed
//                  above them, if the debug flag is set then all network objects have debug info displayed
//                  above them
void CNetObjPickupPlacement::DisplayNetworkInfo()
{
#if __BANK

	const NetworkDebug::NetworkDisplaySettings &displaySettings = NetworkDebug::GetDebugDisplaySettings();

	if (!displaySettings.m_displayObjectInfo)
		return;

	if (!GetPickupPlacement())
		return;

	float   scale = 1.0f;
	Vector2 screenCoords;
	if(NetworkUtils::GetScreenCoordinatesForOHD(GetPosition(), screenCoords, scale))
	{
		const float DEFAULT_CHARACTER_HEIGHT = 0.065f; // normalised value, CText::GetDefaultCharacterHeight() returns coordinates relative to screen dimensions
		const float DEBUG_TEXT_Y_INCREMENT   = (DEFAULT_CHARACTER_HEIGHT * scale * 0.5f); // amount to adjust screen coordinates to move down a line

		screenCoords.y += DEBUG_TEXT_Y_INCREMENT*4; // move below display info for pickup object

		// draw the text over objects owned by a player in the default colour for the player
		// based on their ID. This is so we can distinguish between objects owned by different
		// players on the same team
		NetworkColours::NetworkColour colour = NetworkUtils::GetDebugColourForPlayer(GetPlayerOwner());
		Color32 playerColour = NetworkColours::GetNetworkColour(colour);

		char str[100];
		sprintf(str, "%s%s", GetLogName(), IsPendingOwnerChange() ? "*" : "");
		DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, str));

		if(displaySettings.m_displayInformation.m_displayPickupRegeneration)
		{
			CPickupPlacement* pPlacement = GetPickupPlacement();

			char regenStr[100];

			if (pPlacement->GetPickup())
			{
				sprintf(regenStr, "Has pickup");
			}
			else if (!pPlacement->GetRegenerates())
			{
				sprintf(regenStr, "Non-regenerating");
			}
			else if (pPlacement->IsNetworkClone())
			{
				sprintf(regenStr, "Network clone");
			}
			else
			{
				if (CPickupManager::IsPlacementOnRegenerationList(pPlacement))
				{
					u32 regenTime = CPickupManager::GetRegenerationTime(pPlacement);
					sprintf(regenStr, "Reg time : %d. Time left : %0.2f", regenTime, static_cast<float>(regenTime-NetworkInterface::GetSyncedTimeInMilliseconds()) / 1000.0f);
				}
				else
				{
					sprintf(regenStr, "Not on regeneration list");
				}
			}
			
			screenCoords.y += DEBUG_TEXT_Y_INCREMENT;
			DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, regenStr));
		}
	}

#endif // __BANK
}
