//
// name:        NetObjPickup.cpp
// description: Derived from CNetworkPickup, this class handles all pickup-related network Pickup
//              calls. See NetworkPickup.h for a full description of all the methods.
// written by:  John Gurney
//

#include "network/Objects/entities/NetObjPickup.h"

// game headers
#include "Network/Debug/NetworkDebug.h"
#include "Network/Events/NetworkEventTypes.h"
#include "Network/Objects/NetworkObjectMgr.h"
#include "network/objects/prediction/NetBlenderPhysical.h"
#include "peds/Ped.h"
#include "pickups/Data/PickupDataManager.h"
#include "pickups/Pickup.h"
#include "pickups/PickupManager.h"
#include "pickups/PickupRewards.h"
#include "renderer/DrawLists/drawListNY.h"
#include "script/Handlers/GameScriptIds.h"
#include "script/Handlers/GameScriptMgr.h"
#include "script/script.h"
#include "streaming/streaming.h"
#include "weapons/WeaponFactory.h"

NETWORK_OPTIMISATIONS()

FW_INSTANTIATE_CLASS_POOL(CNetObjPickup, MAX_NUM_NETOBJPICKUPS, atHashString("CNetObjPickup",0x1cbce31c));
FW_INSTANTIATE_CLASS_POOL(CNetObjPickup::CPickupSyncData, MAX_NUM_NETOBJPICKUPS, atHashString("CPickupSyncData",0x5944f8a2));

const float CNetObjPickup::CPickupScopeData::SCOPE_DIST_EXTENDER_COLLIDER_SIZE	= 0.2f;
const float CNetObjPickup::CPickupScopeData::SCOPE_DIST_EXTENDER_MULTIPLIER		= 2.0f;

CPickupSyncTree				   *CNetObjPickup::ms_pickupSyncTree;
CNetObjPickup::CPickupScopeData	CNetObjPickup::ms_pickupScopeData;

// ================================================================================================================
// CNetObjPickup
// ================================================================================================================

CNetObjPickup::CNetObjPickup(CPickup					*pickup,
                             const NetworkObjectType	type,
                             const ObjectId				objectID,
                             const PhysicalPlayerIndex  playerIndex,
                             const NetObjFlags			localFlags,
                             const NetObjFlags			globalFlags)
: CNetObjPhysical(pickup, type, objectID, playerIndex, localFlags, globalFlags)
, m_bDestroyNextUpdate(false), m_AllowedPlayersList(0)
#if ENABLE_NETWORK_LOGGING
, m_lastPassControlToCarrierFailReason(MAX_UINT32)
#endif // ENABLE_NETWORK_LOGGING
{
	m_bCompletedInteriorCheck = false;
}

void CNetObjPickup::CreateSyncTree()
{
    ms_pickupSyncTree = rage_new CPickupSyncTree(PICKUP_SYNC_DATA_BUFFER_SIZE);
}

void CNetObjPickup::DestroySyncTree()
{
	ms_pickupSyncTree->ShutdownTree();
    delete ms_pickupSyncTree;
    ms_pickupSyncTree = 0;
}

netINodeDataAccessor *CNetObjPickup::GetDataAccessor(u32 dataAccessorType)
{
	netINodeDataAccessor *dataAccessor = 0;

	if(dataAccessorType == IPickupNodeDataAccessor::DATA_ACCESSOR_ID())
	{
		dataAccessor = (IPickupNodeDataAccessor *)this;
	}
	else
	{
		dataAccessor = CNetObjPhysical::GetDataAccessor(dataAccessorType);
	}

	return dataAccessor;
}

float CNetObjPickup::GetScopeDistance(const netPlayer* pRelativePlayer) const
{
	CPickup* pPickup = GetPickup();
	
	float result = 0.0f;
	// pickups generated from placements have the same scope distance as the placement
	if (AssertVerify(pPickup) && pPickup->GetPlacement() && AssertVerify(pPickup->GetPlacement()->GetNetworkObject()))
	{
		result = static_cast<CNetObjProximityMigrateable*>(pPickup->GetPlacement()->GetNetworkObject())->GetScopeDistance();
	}
	else
	{
		result = CNetObjProximityMigrateable::GetScopeDistance(pRelativePlayer);
	}

	// Extend the scoping distance for large pickups.
	if(pPickup->GetHeightOffGround() > CPickupScopeData::SCOPE_DIST_EXTENDER_COLLIDER_SIZE)
	{
		result *= CPickupScopeData::SCOPE_DIST_EXTENDER_MULTIPLIER;
	}

	return result;
}

bool CNetObjPickup::Update()
{
	CPickup* pPickup = GetPickup();

	if (AssertVerify(pPickup) && !IsClone())
	{
		// remove any pickups that are no longer needed
		if (m_bDestroyNextUpdate && !IsPendingOwnerChange())
		{
			return true;
		}

		// forcibly pass a portable pickup to the owner of the ped it is attached to
		if (pPickup->IsFlagSet(CPickup::PF_Portable) && pPickup->GetIsAttached())
		{
			CEntity* pEntity = static_cast<CEntity*>(pPickup->GetAttachParent());

			if (pEntity->GetIsTypePed())
			{
				netObject* pNetObj = static_cast<CPed*>(pEntity)->GetNetworkObject();

				if (pNetObj && pNetObj->IsClone())
				{
					CNetGamePlayer* pOwner = static_cast<CNetGamePlayer*>(pNetObj->GetPlayerOwner());

					unsigned resultCode = 0;
					if (pOwner && pOwner->CanAcceptMigratingObjects() && CanPassControl(*pOwner, MIGRATE_FORCED, &resultCode))
					{
						if(NetworkDebug::IsOwnershipChangeAllowed())
						{
#if ENABLE_NETWORK_LOGGING
							m_lastPassControlToCarrierFailReason = MAX_UINT32;
#endif //ENABLE_NETWORK_LOGGING
							CGiveControlEvent::Trigger(*pOwner, this, MIGRATE_FORCED);
						}
					}
#if ENABLE_NETWORK_LOGGING
					else
					{
						if(m_lastPassControlToCarrierFailReason != resultCode)
						{
							m_lastPassControlToCarrierFailReason = resultCode;
							
							netLoggingInterface &log = NetworkInterface::GetObjectManager().GetLog();
							NetworkLogUtils::WriteLogEvent(log, "FAILED_TO_PASS_TO_CARRIER_PLAYER", GetLogName());
							log.WriteDataValue("Carrier", pNetObj->GetLogName());
							log.WriteDataValue("Reason", NetworkUtils::GetCanPassControlErrorString(resultCode));
						}
					}
#endif //ENABLE_NETWORK_LOGGING
					
				}
			}
		}
	}

#if __DEV
	// sanity check that the pickup and its placement are always owned by the same player
	if (AssertVerify(pPickup) && pPickup->GetPlacement() && !pPickup->GetPlacement()->GetIsBeingDestroyed() && AssertVerify(pPickup->GetPlacement()->GetNetworkObject()))
	{
		gnetAssert(pPickup->GetPlacement()->GetNetworkObject()->GetPlayerOwner() == GetPlayerOwner());
	}
#endif

	if(!m_bCompletedInteriorCheck)
	{
		InteriorCheck();
	}	

	// detach any pickups about to migrate due to the local player not running the script that created them
	if (pPickup && pPickup->IsFlagSet(CPickup::PF_Portable) && pPickup->GetIsAttached() && !IsClone() && !IsPendingOwnerChange())
	{
		CEntity* pAttachParent = static_cast<CEntity*>(pPickup->GetAttachParent());

		if (pAttachParent && pAttachParent->GetIsTypePed())
		{
			CPed* pAttachPed = static_cast<CPed*>(pAttachParent);

			if (pAttachPed->IsLocalPlayer())
			{
				CScriptEntityExtension* pScriptExtension = pPickup->GetExtension<CScriptEntityExtension>();

				if (pScriptExtension)
				{
					scriptHandler* pScriptHandler = pScriptExtension->GetScriptHandler();

					if (!pPickup->GetAllowNonScriptParticipantCollect()
						&& (!pScriptHandler || 
						!pScriptHandler->GetNetworkComponent() ||
						pScriptHandler->GetNetworkComponent()->GetState() != scriptHandlerNetComponent::NETSCRIPT_PLAYING ||
						!pScriptHandler->GetNetworkComponent()->IsPlayerAParticipant(*NetworkInterface::GetLocalPlayer())))
					{
						pPickup->DetachPortablePickupFromPed("CNetObjPickup::Update() - Local player not a script participant");
					}			
				}
			}
		}
	}

	bool result = CNetObjPhysical::Update();
	if(result && pPickup->GetForceWaitForFullySyncBeforeDestruction())
	{
		if(IsSyncedWithAllPlayers())
		{
			pPickup->SetForceWaitForFullySyncBeforeDestruction(false);
		}
		else
		{
			result = false;
		}
	}

	return result;
}


//For non-scripted clone pickups checks if the portal tracker has been run and once confirmed then waits for any existing movement
//to complete before forcing it to run portal tracker update again. 
//When m_bCompletedInteriorCheck is true this indicates this has been done or is not required. See B* 1568094
void CNetObjPickup::InteriorCheck()
{
	CPickup* pPickup = GetPickup();

	if (AssertVerify(pPickup) &&
		pPickup->GetOwnedBy() != ENTITY_OWNEDBY_SCRIPT &&
		IsClone() &&
		!pPickup->GetIsAttached() &&
		pPickup->GetPortalTracker()  )
	{
		bool portalUpdated = false;  //wait until the portal has a position set near to the pickup position
		Vector3 currentPosition			= VEC3V_TO_VECTOR3(pPickup->GetTransform().GetPosition());
		Vector3 portalCurrPos	= pPickup->GetPortalTracker()->GetCurrentPos();

		Vector3 diffPos = currentPosition - portalCurrPos;

		static const float PORTAL_UPDATED_RANGE_SQUARED = 10.0f*10.0f;

		if(diffPos.Mag2()< PORTAL_UPDATED_RANGE_SQUARED)
		{
			portalUpdated = true;
		}

		static const float SPEED_THRESHOLD = 0.01f;
		if ( portalUpdated && pPickup->GetVelocity().Mag2() <= SPEED_THRESHOLD)
		{   //pickup is at rest so recalculate portal status
			pPickup->GetPortalTracker()->RequestRescanNextUpdate();
			pPickup->GetPortalTracker()->Update(currentPosition);
			m_bCompletedInteriorCheck = true;
		}
	}
	else
	{
		m_bCompletedInteriorCheck = true;
	}

}

u32 CNetObjPickup::CalcReassignPriority() const
{
    u32 reassignPriority = CNetObjPhysical::CalcReassignPriority();

    CPickup          *pPickup    = GetPickup();
    CPickupPlacement *pPlacement = pPickup ? pPickup->GetPlacement() : 0;

    // pickups should migrate with their associated placements, so we want them to always have the same
    // reassign priority to ensure they are reassigned to the same machine
	if (pPlacement && gnetVerify(pPlacement->GetNetworkObject()))
	{
		reassignPriority = pPlacement->GetNetworkObject()->CalcReassignPriority();
	}

    return reassignPriority;
}

bool CNetObjPickup::CanClone(const netPlayer& player, unsigned *resultCode) const
{
	CPickup* pPickup = GetPickup();
	// Don't allow cloning of money pickups
	if(pPickup && !CPickupManager::IsNetworkedMoneyPickupAllowed() && CPickupManager::PickupHashIsMoneyType(pPickup->GetPickupHash()))
	{
		if (resultCode)
		{
			*resultCode = CC_MONEY_PICKUP;
		}
		return false;
	}

	return CNetObjPhysical::CanClone(player, resultCode);
}

bool CNetObjPickup::IsInScope(const netPlayer& player, unsigned *scopeReason) const
{
	CPickup* pPickup = GetPickup();

	// pickup isn't in scope until it has been initialised properly (got it's custom archetype and added to world correctly)
	if (AssertVerify(pPickup))
	{
		// Don't allow cloning of money pickups
		if(!CPickupManager::IsNetworkedMoneyPickupAllowed() && CPickupManager::PickupHashIsMoneyType(pPickup->GetPickupHash()))
		{
			if (scopeReason)
			{
				*scopeReason = SCOPE_OUT_PICKUP_IS_MONEY;
			}
			return false;
		}

		if (!pPickup->IsFlagSet(CPickup::PF_Initialised) && !HasBeenCloned(player))
		{
			if (scopeReason)
			{
				*scopeReason = SCOPE_OUT_PICKUP_UNINITIALISED;
			}
			return false;
		}

		if (pPickup->IsFlagSet(CPickup::PF_DebugCreated))
		{
			if (scopeReason)
			{
				*scopeReason = SCOPE_IN_PICKUP_DEBUG_CREATED;
			}
			return true;
		}

		if(pPickup && pPickup->GetIsAttached() && pPickup->GetAttachParent() && ((CEntity*)pPickup->GetAttachParent())->GetIsTypeVehicle())
		{
			netObject* pNetObj = NetworkUtils::GetNetworkObjectFromEntity((CEntity*)pPickup->GetAttachParent());

			if(pNetObj && pNetObj->IsInScope(player))
			{
				if (scopeReason)
				{
					*scopeReason = SCOPE_IN_OBJECT_ATTACH_PARENT_IN_SCOPE;
				}
				return true;
			}
		}

		CPickupPlacement* pPlacement = pPickup->GetPlacement();

		// pickups have the same scope as their placements if they have been created by the script. This is so that we can migrate
		// the placement to another player running the script at any time (placements must only migrate to players running the script
		// and they can't if their pickup hasn't been cloned on those machines)
		if (pPlacement && gnetVerify(pPlacement->GetNetworkObject()))
		{
			if (scopeReason)
			{
				*scopeReason = SCOPE_PICKUP_USE_PLACEMENT_SCOPE;
			}

			return pPlacement->GetNetworkObject()->IsInScope(player);
		}

		// if another networked entity is attached to this one, and cloned on this player's machine, we need to make this entity in scope too
		if (pPickup->GetChildAttachment() && !((CEntity*)pPickup->GetChildAttachment())->GetIsTypePed())
		{
			netObject* pNetObj = NetworkUtils::GetNetworkObjectFromEntity((CEntity*)pPickup->GetChildAttachment());

			if (pNetObj && pNetObj->IsInScope(player))
			{
				if (scopeReason)
				{
					*scopeReason = SCOPE_IN_OBJECT_ATTACH_CHILD_IN_SCOPE;
				}
				return true;
			}
		}
	}

	return CNetObjPhysical::IsInScope(player, scopeReason);
}

bool CNetObjPickup::CanPassControl(const netPlayer& player, eMigrationType migrationType, unsigned *resultCode) const
{
	CPickup* pPickup = GetPickup();

	if (pPickup)
	{
		if (pPickup->IsFlagSet(CPickup::PF_WarpToAccessibleLocation))
		{
			if (resultCode)
			{
				*resultCode = CPC_PICKUP_WARPING;
			}
			return false;
		}

		// pickups with a corresponding placement cannot migrate on their own
		if (pPickup->GetPlacement())
		{
			if (resultCode)
			{
				*resultCode = CPC_PICKUP_HAS_PLACEMENT;
			}
			return false;
		}

		if (migrationType == MIGRATE_PROXIMITY)
		{
			// don't let ambient pickups migrate until they have come to rest
			if(pPickup->GetOwnedBy() != ENTITY_OWNEDBY_SCRIPT)
			{
				static const float SPEED_THRESHOLD = 0.01f;
				if (pPickup->GetVelocity().Mag2() > SPEED_THRESHOLD)
				{
					if (resultCode)
					{
						*resultCode = CPC_PICKUP_COMING_TO_REST;
					}
					return false;
				}
			}

			// don't allow proximity migrations of pickups attached to a ped
			if (migrationType == MIGRATE_PROXIMITY && pPickup->IsFlagSet(CPickup::PF_Portable) && pPickup->GetIsAttached())
			{
				CEntity* pEntity = static_cast<CEntity*>(pPickup->GetAttachParent());

				if (pEntity && pEntity->GetIsTypePed())
				{
					CPed* pPed = SafeCast(CPed, pEntity);

					if (!pPed->IsNetworkClone())
					{
						if (resultCode)
						{
							*resultCode = CPC_PICKUP_ATTACHED_TO_LOCAL_PED;
						}
						return false;
					}
				}
			}
		}
	}

	return CNetObjPhysical::CanPassControl(player, migrationType, resultCode);
}

bool CNetObjPickup::CanDelete(unsigned* LOGGING_ONLY(reason))
{
	CPickup* pPickup = GetPickup();

	if (AssertVerify(pPickup))
	{
		// pickups pending collection are those we are trying to collect and waiting on approval by the owner of the pickup.
		// We must wait for this reply before we can remove the pickup.
		if (pPickup && pPickup->GetIsPendingCollection() && !m_bDestroyNextUpdate)
		{
#if ENABLE_NETWORK_LOGGING
			if(reason)
				*reason = CTD_PICKUP_PENDING_COLLECTION;
#endif // ENABLE_NETWORK_LOGGING
			return false;
		}

		if(pPickup->GetForceWaitForFullySyncBeforeDestruction() && !IsClone())
		{
			if(!IsCriticalStateSyncedWithAllPlayers())
			{
#if ENABLE_NETWORK_LOGGING
				if(reason)
					*reason = CTD_PICKUP_WAITING_TO_SYNC;
#endif // ENABLE_NETWORK_LOGGING

				return false;
			}
		}
	}

	return CNetObjPhysical::CanDelete(LOGGING_ONLY(reason));
}

void CNetObjPickup::PostCreate()
{
    if(IsClone())
    {
        BANK_ONLY(NetworkDebug::AddOtherCloneCreate());
    }

    CNetObjPhysical::PostCreate();
}

bool CNetObjPickup::CanBlend(unsigned *resultCode) const
{
	CPickup *pickup = GetPickup();

	if(pickup)
	{
		if (pickup->IsAlwaysFixed())
		{
			if(resultCode)
			{
				*resultCode = CB_FAIL_FIXED;
			}

			return false;
		}
	}

	return CNetObjPhysical::CanBlend(resultCode);
}


// name:        ProcessControl
// description: Called from CGameWorld::Process, called in the same place as the local entity process controls
bool CNetObjPickup::ProcessControl()
{
	CPickup* pPickup = GetPickup();

	if (AssertVerify(pPickup))
	{
		CNetObjPhysical::ProcessControl();

		if (IsClone())
		{
			if(pPickup->GetObjectIntelligence())
			{
				pPickup->GetObjectIntelligence()->GetTaskManager()->Process(fwTimer::GetTimeStep());
			}

			if (pPickup->RequiresProcessControl())
			{
				pPickup->ProcessControl();
			}
		}
	}

	return true;
}

//
// name:        CleanUpGameObject
// description: Cleans up the pointers between this network object and the
//              game object which owns it
void CNetObjPickup::CleanUpGameObject(bool bDestroyObject)
{
	CPickup* pPickup = GetPickup();

	if (AssertVerify(pPickup))
	{
		// need to restore the game heap here because CTheScripts::UnregisterEntity can destroy a script extension and CPickup::Destroy can allocate 
		// a visibility extension from the heap
		sysMemAllocator *oldHeap = &sysMemAllocator::GetCurrent();
		sysMemAllocator::SetCurrent(sysMemAllocator::GetMaster());

		const bool bPickupShouldBeDestroyed = (bDestroyObject && !GetEntityLeftAfterCleanup());
		// unregister the pickup with the scripts here when the object still has a network object ptr and the reservations can be adjusted
		CTheScripts::UnregisterEntity(pPickup, !bPickupShouldBeDestroyed);

		pPickup->SetNetworkObject(NULL);
		SetGameObject(0);

		if (bPickupShouldBeDestroyed)
		{
			pPickup->Destroy();
		}

        if(IsClone())
        {
            BANK_ONLY(NetworkDebug::AddOtherCloneRemove());
        }

		sysMemAllocator::SetCurrent(*oldHeap);
	}
}

bool CNetObjPickup::OnReassigned()
{
	CPickup* pPickup = GetPickup();

	// if we have an object reassigned to our machine, and we are not running the script that created it then we need to clean up the script state
	// if this object must only exist on machines running that script (if it has been reassigned to our machine this means that no other machines
	// are running the script)
	if (!IsClone() && AssertVerify(pPickup))
	{
		CPickupPlacement* pPlacement = pPickup->GetPlacement();

		// if this pickup has a placement that has been unregistered after reassignment or is about to be unregistered then we need to unregister the
		// pickup here too
		if (pPlacement && (!pPlacement->GetNetworkObject() || pPlacement->GetNetworkObject()->OnReassigned()))
		{
			return true;
		}

		if (IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_SCRIPT_MIGRATION))
		{
			scriptHandlerObject* scriptObj = GetScriptHandlerObject();

			if (gnetVerify(scriptObj))
			{
				if (!scriptObj->GetScriptHandler() ||
					!scriptObj->GetScriptHandler()->GetNetworkComponent() ||
					scriptObj->GetScriptHandler()->GetNetworkComponent()->GetState() != scriptHandlerNetComponent::NETSCRIPT_PLAYING)
				{
					return true;
				}
			}
		}

		// we may need to warp an inaccessible pickup if the player who was carrying left in an inaccessible spot
		if (pPickup && !pPickup->GetIsAttached() && pPickup->IsFlagSet(CPickup::PF_Portable) && pPickup->IsFlagSet(CPickup::PF_Inaccessible))
		{
			NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "INACCESSIBLE_PICKUP", GetLogName());

			pPickup->MoveToAccessibleLocation();
		}
	}

	return CNetObjPhysical::OnReassigned();
}


void CNetObjPickup::OnConversionToAmbientObject()
{
	CPickup *pickup = GetPickup();

	CNetObjPhysical::OnConversionToAmbientObject();

	if (pickup && pickup->IsFlagSet(CPickup::PF_Portable) && !pickup->IsFlagSet(CPickup::PF_KeepPortablePickupAtDropoff))
	{
		// portable pickups are just removed when converted to ambient objects
		m_bDestroyNextUpdate = true;
	}
}

bool CNetObjPickup::NetworkBlenderIsOverridden(unsigned *resultCode) const
{
    CPickup *pickup = GetPickup();

	if (pickup)
	{
		if(pickup->GetTask() && (pickup->GetTask()->GetTaskType() == CTaskTypes::TASK_SYNCHRONIZED_SCENE))
		{
			if (resultCode)
			{
				*resultCode = NBO_SYNCED_SCENE;
			}

			return true;
		}

		if(GetCurrentAttachmentEntity())
		{
			if (resultCode)
			{
				*resultCode = NBO_ATTACHED_PICKUP;
			}

			return true;
		}
		else
		{
			TUNE_FLOAT(MIN_PICKUP_BLEND_VELOCITY, 1.0f, 0.0f, 5.0f, 0.02f);
			TUNE_FLOAT(MIN_PICKUP_BLEND_POSITION, 0.7f, 0.0f, 2.0f, 0.02f);

			// stop blending when the pickup gets close to its target. This is to allow pickups in a pile to move around locally without the blender
			// getting in the way and possibly forcing pickups through the map
			netBlenderLinInterp* pNetBlender = static_cast<netBlenderLinInterp*>(GetNetBlender());

			if (pNetBlender)
			{
				Vector3 velocity = pickup->GetVelocity();
				Vector3 predictedVel = pNetBlender->GetLastVelocityReceived();

				float minPickupBlendVel = MIN_PICKUP_BLEND_VELOCITY*MIN_PICKUP_BLEND_VELOCITY;

				if (velocity.XYMag2() < minPickupBlendVel && predictedVel.XYMag2() < minPickupBlendVel)
				{
					Vector3 predictedPos = pNetBlender->GetCurrentPredictedPosition();
					Vector3 currentPosition = VEC3V_TO_VECTOR3(pickup->GetTransform().GetPosition());
					Vector3 diff = predictedPos - currentPosition;

					if (diff.Mag2() < MIN_PICKUP_BLEND_POSITION*MIN_PICKUP_BLEND_POSITION)
					{
						if(resultCode)
						{
							*resultCode = NBO_PICKUP_CLOSE_TO_TARGET;
						}

						return false;
					}
				}
			}
		}
	}

	return CNetObjPhysical::NetworkBlenderIsOverridden(resultCode);
}

bool CNetObjPickup::CanSynchronise(bool bOnRegistration) const
{
	CPickup* pPickup = GetPickup();

	if (pPickup && !bOnRegistration && !IsScriptObject() && pPickup->IsAsleep() && !pPickup->IsFlagSet(CPickup::PF_Portable))
	{
		// stop syncing ambient pickups that have come to rest
		return false;
	}

	return CNetObjPhysical::CanSynchronise(bOnRegistration);
}

void CNetObjPickup::SetIsVisible(bool isVisible, const char* reason, bool bNetworkUpdate)
{
	CPickup* pPickup = GetPickup();

	if (pPickup && !bNetworkUpdate)
	{
		if (isVisible)
		{
			pPickup->ClearFlag(CPickup::PF_HiddenByScript);
		}
		else
		{
			pPickup->SetFlag(CPickup::PF_HiddenByScript);
		}
	}

	CNetObjPhysical::SetIsVisible(isVisible, reason, bNetworkUpdate);
}

bool CNetObjPickup::HasCollisionLoadedUnderEntity() const
{
	CPickup* pPickup = GetPickup();

	// prevent pickups being unfixed by CNetObjPhysical::ProcessFixedByNetwork if they have not been placed on the ground yet - they will be intersecting the map and may fall through it
	if (pPickup && pPickup->IsFlagSet(CPickup::PF_PlaceOnGround) && !pPickup->IsFlagSet(CPickup::PF_PlacedOnGround))
	{
		return false;
	}

	return CNetObjPhysical::HasCollisionLoadedUnderEntity();
}

void CNetObjPickup::OverrideBlenderUntilAcceptingObjects()
{
	CPickup* pPickup = GetPickup();

	// don't prevent blending on portable pickups as they are often warped away by the script after player death
	if (!pPickup || !pPickup->IsFlagSet(CPickup::PF_Portable))
	{
		CNetObjPhysical::OverrideBlenderUntilAcceptingObjects();
	}
}

bool CNetObjPickup::CanEnableCollision() const
{
	CPickup* pPickup = GetPickup();

	if (pPickup && !pPickup->IsFlagSet(CPickup::PF_Initialised))
	{
		return false;
	}

	return CNetObjPhysical::CanEnableCollision();
}

void CNetObjPickup::SetPosition( const Vector3& pos )
{
	CPickup* pPickup = GetPickup();

	// whenever fixed pickups on the ground are moved, we need to place them on the ground again
	if (pPickup && pPickup->IsFlagSet(CPickup::PF_PlaceOnGround) && pPickup->IsAlwaysFixed() && !pPickup->GetIsAttached())
	{
		Vector3 pickupPos = VEC3V_TO_VECTOR3(pPickup->GetTransform().GetPosition());

		if (!pickupPos.IsClose(pos, 0.1f))
		{
			CNetObjPhysical::SetPosition(pos);

			pPickup->SetPlaceOnGround(pPickup->IsFlagSet(CPickup::PF_OrientToGround), pPickup->IsFlagSet(CPickup::PF_UprightOnGround));
		}
		else
		{
			CNetObjPhysical::SetPosition(pickupPos);
		}
	}
	else
	{
		CNetObjPhysical::SetPosition(pos);
	}
}


// Name         :   CheckPlayerHasAuthorityOverObject
// Purpose      :   Checks whether a player is allowed to be sending sync data for the object - check current ownership and ownership changes
//					**This must be called after the data from the player has been read into the sync tree**
bool CNetObjPickup::CheckPlayerHasAuthorityOverObject(const netPlayer& player)
{
	CPickup* pPickup = GetPickup();

	// pickups generated from placements migrate with their placement, so stop migration due to ownership token
	if (pPickup && pPickup->GetPlacement() && GetPhysicalPlayerIndex() != player.GetPhysicalPlayerIndex())
	{
		return false;
	}

	return CNetObjProximityMigrateable::CheckPlayerHasAuthorityOverObject(player);
}

void CNetObjPickup::TryToPassControlProximity()
{
	CPickup* pPickup = GetPickup();

	// only try to pass control for pickups with no placements (otherwise migration is handled by the placement)
	if (AssertVerify(pPickup) && !pPickup->GetPlacement())
	{
		CNetObjPhysical::TryToPassControlProximity();
	}
}

bool CNetObjPickup::TryToPassControlOutOfScope()
{
	CPickup* pPickup = GetPickup();

	// only try to pass control for pickups with no placements (otherwise migration is handled by the placement)
	if (AssertVerify(pPickup) && !pPickup->GetPlacement())
	{
		return CNetObjPhysical::TryToPassControlOutOfScope();
	}

	return false;
}

void CNetObjPickup::HideForCutscene()
{
	// just make pickups invisible during a cutscene
	CNetObjEntity::HideForCutscene();
}

void CNetObjPickup::ExposeAfterCutscene()
{
	// just make pickups invisible during a cutscene
	CNetObjEntity::ExposeAfterCutscene();
}

bool CNetObjPickup::IsAttachmentStateSentOverNetwork() const
{
    return IsScriptObject() || IsGlobalFlagSet(GLOBALFLAG_WAS_SCRIPTOBJECT);
}

void CNetObjPickup::AttemptPendingDetachment(CPhysical* pEntityToAttachTo)
{
    CNetObjPhysical::AttemptPendingDetachment(pEntityToAttachTo);

    CPickup *pPickup = GetPickup();

    if(pPickup && pPickup->IsAlwaysFixed())
    {
        if (!m_pendingDetachmentActivatePhysics && pPickup->GetCurrentPhysicsInst())
	    {
		    pPickup->GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_NEVER_ACTIVATE,true);
		}
    }
}

void CNetObjPickup::SetIsFixed(bool isFixed)
{
	CPickup *pPickup = GetPickup();

	if (pPickup && !isFixed && pPickup->IsFlagSet(CPickup::PF_Portable) && pPickup->IsAlwaysFixed())
	{
		NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "UNFIX_PORTABLE_PICKUP_NETOBJ", GetLogName());

		if (!IsClone())
		{
			gnetAssertf(0, "Trying to unfix local %s", GetLogName());
			return;
		}
	}

	CNetObjPhysical::SetIsFixed(isFixed);
}

bool CNetObjPickup::AllowFixByNetwork() const
{
	bool bAllow = true;

	CPickup* pPickup = GetPickup();

	if (pPickup && pPickup->IsAlwaysFixed())
	{
		bAllow = false;
	}

	return bAllow;
}

bool CNetObjPickup::UseBoxStreamerForFixByNetworkCheck() const
{
    return IsImportant();
}

#if __BANK
// Name         :   DisplayNetworkInfoForObject
// Purpose      :   Displays debug info above the network object, players always have their name displayed
//                  above them, if the debug flag is set then all network objects have debug info displayed
//                  above them
void CNetObjPickup::DisplayNetworkInfoForObject(const Color32 &col, float scale, Vector2 &screenCoords, const float debugTextYIncrement)
{
	CNetObjPhysical::DisplayNetworkInfoForObject(col, scale, screenCoords, debugTextYIncrement);

	const NetworkDebug::NetworkDisplaySettings &displaySettings = NetworkDebug::GetDebugDisplaySettings();

	const unsigned int MAX_STRING = 50;
	char str[MAX_STRING];

	CPickup* pPickup = GetPickup();

	if(!pPickup)
		return;

	if(displaySettings.m_displayPortablePickupInfo && pPickup->IsFlagSet(CPickup::PF_Portable))
	{
		sprintf(str, "Accessible: %s", !pPickup->IsFlagSet(CPickup::PF_Inaccessible) ? "true" : "false");
		DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, col, scale, str));
		screenCoords.y += debugTextYIncrement;

		const Vector3 lastInaccessibleLocation = pPickup->GetLastAccessibleLocation();

		sprintf(str, "Last accessible location %.2f, %.2f, %.2f", lastInaccessibleLocation.x, lastInaccessibleLocation.y, lastInaccessibleLocation.z);
		DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, col, scale, str));
		screenCoords.y += debugTextYIncrement;

		sprintf(str, "Valid nav mesh: %s", pPickup->IsFlagSet(CPickup::PF_LastAccessiblePosHasValidGround) ? "true" : "false");

		DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, col, scale, str));
		screenCoords.y += debugTextYIncrement;

		Matrix34 mat;
		mat.Identity();
		mat.d = lastInaccessibleLocation;
		grcDebugDraw::Axis(mat, 1.0f);

		if (pPickup->IsFlagSet(CPickup::PF_LyingOnFixedObject))
		{
			sprintf(str, "On fixed object: true");
			DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, col, scale, str));
			screenCoords.y += debugTextYIncrement;
		}

		if (pPickup->IsFlagSet(CPickup::PF_LyingOnUnFixedObject))
		{
			sprintf(str, "On unfixed object: true");
			DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, col, scale, str));
			screenCoords.y += debugTextYIncrement;
		}
	}
}
#endif // __BANK

// sync parser create functions
void CNetObjPickup::GetPickupCreateData(CPickupCreationDataNode& data)  const
{
	CPickup* pPickup = GetPickup();

	if (AssertVerify(pPickup))
	{
		data.m_bHasPlacement = pPickup->GetPlacement() != NULL;

		data.m_pickupHash = pPickup->GetPickupHash();
		data.m_amount = pPickup->GetAmount();

		if (data.m_bHasPlacement)
		{
			if (AssertVerify(pPickup->GetPlacement()->GetNetworkObject()) && AssertVerify(pPickup->GetPlacement()->GetNetworkObject()->GetScriptObjInfo()))
			{
				data.m_placementInfo = *pPickup->GetPlacement()->GetScriptInfo();
			}
		}

		data.m_customModelHash = 0;

		if (pPickup->GetPickupData()->GetModelIndex() != pPickup->GetModelIndex())
		{
			CBaseModelInfo *modelInfo = pPickup->GetBaseModelInfo();
			if(AssertVerify(modelInfo))
			{
				data.m_customModelHash = modelInfo->GetHashKey();
			}
		}

		if (!data.m_bHasPlacement)
		{
			data.m_lifeTime = pPickup->GetLifeTime();

			if (data.m_lifeTime > CPickup::AMBIENT_PICKUP_LIFETIME)
			{
				data.m_lifeTime = CPickup::AMBIENT_PICKUP_LIFETIME;
			}

			data.m_numWeaponComponents = 0;
			data.m_weaponTintIndex = 0;

			if (pPickup->GetWeapon())
			{
				const CWeapon::Components& components = pPickup->GetWeapon()->GetComponents();
				for(s32 i = 0; i < components.GetCount(); i++)
				{
					if (components[i]->GetDrawable() || components[i]->GetIsClassId(CWeaponComponentVariantModel::GetStaticClassId()))
					{
						data.m_weaponComponents[data.m_numWeaponComponents++] = components[i]->GetInfo()->GetHash();
					}
				}

				data.m_weaponTintIndex = pPickup->GetWeapon()->GetTintIndex();
			}

			data.m_bPlayerGift = pPickup->IsPlayerGift();
		}

		data.m_bHasPlayersBlockingList = m_AllowedPlayersList != 0;
		if(data.m_bHasPlayersBlockingList)
		{
			data.m_PlayersToBlockList = m_AllowedPlayersList;
		}
		else
		{
			data.m_PlayersToBlockList = 0;
		}

		data.m_LODdistance = pPickup->GetLodDistance();

		data.m_includeProjectiles = pPickup->GetIncludeProjectileFlag();
	}
}

void CNetObjPickup::GetPickupSectorPosData(CPickupSectorPosNode& data) const
{
	GetSectorPosition(data.m_SectorPosX, data.m_SectorPosY, data.m_SectorPosZ);
}

void CNetObjPickup::GetPickupScriptGameStateData(CPickupScriptGameStateNode& data) const
{
	CPickup* pPickup = GetPickup();

	if (AssertVerify(pPickup))
	{
		data.m_flags							= pPickup->GetSyncedFlags();
		data.m_bFloating						= pPickup->m_nObjectFlags.bFloater;
		data.m_teamPermits						= pPickup->GetTeamPermits();
		data.m_offsetGlow						= pPickup->GetGlowOffset();
		data.m_portable							= pPickup->IsFlagSet(CPickup::PF_Portable);
		data.m_allowNonScriptParticipantCollect = pPickup->GetAllowNonScriptParticipantCollect();

		if (data.m_portable)
		{
			data.m_inAccessible						= pPickup->IsFlagSet(CPickup::PF_Inaccessible);
			data.m_lastAccessibleLoc				= pPickup->GetLastAccessibleLocation();
			data.m_lastAccessibleLocHasValidGround	= pPickup->IsFlagSet(CPickup::PF_LastAccessiblePosHasValidGround);
		}
	}
}

void CNetObjPickup::SetPickupCreateData(const CPickupCreationDataNode& data)
{
	if (data.m_bHasPlacement)
	{
		scriptHandler* pHandler = CTheScripts::GetScriptHandlerMgr().GetScriptHandler(data.m_placementInfo.GetScriptId());

		if (gnetVerifyf(pHandler, "Trying to create a clone pickup with no corresponding script handler"))
		{
			CPickupPlacement* pPlacement = static_cast<CPickupPlacement*>(pHandler->GetScriptObject(data.m_placementInfo.GetObjectId()));

			if (gnetVerifyf(pPlacement, "Trying to create a clone pickup with no corresponding placement") && gnetVerify(pPlacement->GetPickupData()))
			{
				strLocalIndex modelIndex = strLocalIndex(pPlacement->GetPickupData()->GetModelIndex());
				fwModelId modelId(modelIndex);

				if (data.m_customModelHash != 0)
				{
					fwModelId modelId;
					CModelInfo::GetBaseModelInfoFromHashKey(data.m_customModelHash, &modelId);

					if (gnetVerifyf(modelId.IsValid(), "Unrecognised custom model hash for pickup clone"))
					{
						modelIndex = modelId.GetModelIndex();
					}
				}

				// ensure that the model is loaded
				if (AssertVerify(CModelInfo::HaveAssetsLoaded(modelId)))
				{
					pPlacement->CreatePickup(this);
				}

				if (AssertVerify(pPlacement->GetPickup()))
				{
					SetGameObject((void*) pPlacement->GetPickup());

					pPlacement->GetPickup()->SetAmount(data.m_amount);
				}
			}
		}
	}
	else
	{
		const CPickupData* pPickupData = CPickupDataManager::GetPickupData(data.m_pickupHash);

		if (gnetVerifyf(pPickupData, "Trying to create a clone pickup with no corresponding pickup metadata"))
		{
			strLocalIndex modelIndex = strLocalIndex(pPickupData->GetModelIndex());
			fwModelId modelId(modelIndex);
			bool hasCustomModel = false;

			if (data.m_customModelHash != 0)
			{
				fwModelId modelId;
				CModelInfo::GetBaseModelInfoFromHashKey(data.m_customModelHash, &modelId);

				if (gnetVerifyf(modelId.IsValid(), "Unrecognised custom model hash for pickup clone"))
				{
					modelIndex = modelId.GetModelIndex();
					hasCustomModel = true;
				}
			}

			// ensure that the model is loaded
			if (AssertVerify(CModelInfo::HaveAssetsLoaded(modelId)))
			{
				Matrix34 tempMat;
				tempMat.Identity();

				CPickup* pPickup = CPickupManager::CreatePickup(data.m_pickupHash, tempMat, this, true, hasCustomModel ? modelIndex.Get() : fwModelId::MI_INVALID);

				if (AssertVerify(pPickup))
				{
					SetGameObject((void*) pPickup);

					pPickup->SetAmount(data.m_amount);
					pPickup->SetLifeTime(data.m_lifeTime);

					if (data.m_weaponTintIndex != 0 || data.m_numWeaponComponents > 0)
					{
						u32 weaponHash = 0;

						for (u32 i=0; i<pPickup->GetPickupData()->GetNumRewards(); i++)
						{
							const CPickupRewardData* pReward = pPickupData->GetReward(i);
							if (pReward && pReward->GetType() == PICKUP_REWARD_TYPE_WEAPON)
							{
								weaponHash = SafeCast(const CPickupRewardWeapon, pReward)->GetWeaponHash();
								break;
							}
						}

						if (AssertVerify(weaponHash != 0))
						{
							pPickup->SetWeapon(WeaponFactory::Create(weaponHash, data.m_amount, pPickup, "CNetObjPickup::SetPickupCreateData"));
		
							if (AssertVerify(pPickup->GetWeapon()))
							{
								for( s32 i = 0; i < data.m_numWeaponComponents; i++ )
								{
									CPedEquippedWeapon::CreateWeaponComponent( data.m_weaponComponents[i], pPickup );
								}

								// Set the correct rendering flags now whole hierarchy has been made (so attachments are set).
								pPickup->SetForceAlphaAndUseAmbientScale();
								// Set up the glow too.
								pPickup->SetUseLightOverrideForGlow();

								if (data.m_weaponTintIndex != 0)
								{
									pPickup->GetWeapon()->UpdateShaderVariables( (u8) data.m_weaponTintIndex );
								}
							}

							pPickup->ClearFlag(CPickup::PF_CreateDefaultWeaponAttachments);
						}
					}

					if (data.m_bPlayerGift)
					{
						pPickup->SetPlayerGift();
					}

					pPickup->SetIncludeProjectileFlag(data.m_includeProjectiles);
				}
			}

			if(data.m_bHasPlayersBlockingList)
			{
				m_AllowedPlayersList = data.m_PlayersToBlockList;
			}

			GetEntity()->SetLodDistance(data.m_LODdistance);
		}
	}
}

void CNetObjPickup::SetPickupSectorPosData(const CPickupSectorPosNode& data)
{
	SetSectorPosition(data.m_SectorPosX, data.m_SectorPosY, data.m_SectorPosZ);
}

void CNetObjPickup::SetPickupScriptGameStateData(const CPickupScriptGameStateNode& data)
{
	CPickup* pPickup = GetPickup();

	if (AssertVerify(pPickup))
	{
		const bool bWasPortable = pPickup->IsFlagSet(CPickup::PF_Portable);
		const bool bDroppedInWater = pPickup->IsFlagSet(CPickup::PF_DroppedInWater);

		pPickup->SetSyncedFlags(data.m_flags);

		pPickup->m_nObjectFlags.bFloater = data.m_bFloating;

		if ((data.m_flags & (1<<CPickup::PF_Portable)) && !bWasPortable)
		{
			pPickup->SetPortable();
		}

		if (data.m_flags & (1<<CPickup::PF_TeamPermitsSet))
		{
			pPickup->SetTeamPermits(static_cast<u8>(data.m_teamPermits));
		}

		pPickup->SetGlowOffset(data.m_offsetGlow);

		if (data.m_flags & (1<<CPickup::PF_DroppedInWater))
		{
			if (!pPickup->GetIsAttached())
			{
				pPickup->DropInWater(false);
			}
			else
			{
				pPickup->SetDroppedInWater();
			}
		}
		else if (bDroppedInWater)
		{
			pPickup->ClearDroppedInWater();
		}

		gnetAssert(data.m_portable == pPickup->IsFlagSet(CPickup::PF_Portable));

		if (data.m_portable)
		{
			pPickup->SetClonePortablePickupData(data.m_inAccessible, data.m_lastAccessibleLoc, data.m_lastAccessibleLocHasValidGround);
		}

		pPickup->SetAllowNonScriptParticipantCollect(data.m_allowNonScriptParticipantCollect);
	}
}

#if ENABLE_NETWORK_LOGGING
const char *CNetObjPickup::GetCannotDeleteReason(unsigned reason)
{
	switch(reason)
	{
	case CTD_PICKUP_PENDING_COLLECTION:
		return "Pickup Pending Collection";
	case CTD_PICKUP_WAITING_TO_SYNC:
		return "Pickup is not fully synced with all players";
	default:
		return CNetObjGame::GetCannotDeleteReason(reason);
	}
}
#endif // ENABLE_NETWORK_LOGGING

void CNetObjPickup::PortablePickupLocallyAttached()
{
	CPickup* pPickup = GetPickup();

	if (AssertVerify(pPickup) && AssertVerify(pPickup->GetAttachParent()))
	{
		bool bAttached = false;

		// grab the pending attachment data from the new attachment so that the pickup is not immediately detached by CNetObjPhysical::ProcessPendingAttachment()
        bool isCargoVehicle = false;
		GetPhysicalAttachmentData(bAttached, 
									m_pendingAttachmentObjectID, 
									m_pendingAttachOffset,
									m_pendingAttachQuat,
									m_pendingAttachParentOffset,
									m_pendingOtherAttachBone,
									m_pendingMyAttachBone,
									m_pendingAttachFlags,
									m_pendingAllowInitialSeparation,
									m_pendingInvMassScaleA,
									m_pendingInvMassScaleB,
									m_pendingDetachmentActivatePhysics,
                                    isCargoVehicle);

		gnetAssert(bAttached);
	}
}

bool CNetObjPickup::IsAllowedForLocalPlayer()
{
	if(m_AllowedPlayersList != 0)
		return (m_AllowedPlayersList & (1 << netInterface::GetLocalPhysicalPlayerIndex())) != 0;

	return true;
}