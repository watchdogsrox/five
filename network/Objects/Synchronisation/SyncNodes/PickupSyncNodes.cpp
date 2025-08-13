//
// name:        PickupSyncNodes.cpp
// description: Network sync nodes used by CNetObjPickups
// written by:    John Gurney
//

#include "network/objects/synchronisation/syncnodes/PickupSyncNodes.h"

#include "fwnet/netlog.h"

#include "network/Network.h"
#include "network/general/NetworkStreamingRequestManager.h"
#include "pickups/Data/PickupDataManager.h"
#include "pickups/Pickup.h"
#include "pickups/PickupManager.h"
#include "script/script.h"
#include "script/Handlers/GameScriptHandler.h"
#include "script/Handlers/GameScriptMgr.h"
#include "streaming/streaming.h"
#include "weapons/components/WeaponComponentManager.h"
#include "Weapons/Components/WeaponComponentVariantModel.h"

NETWORK_OPTIMISATIONS()

DATA_ACCESSOR_ID_IMPL(IPickupNodeDataAccessor);

bool CPickupCreationDataNode::CanApplyData(netSyncTreeTargetObject* pObj) const
{
	bool bSuccess = true;

	u32 pickupHash = 0;

	if (m_bHasPlacement)
	{
		CGameScriptHandler* pHandler = static_cast<CGameScriptHandler*>(CTheScripts::GetScriptHandlerMgr().GetScriptHandler(m_placementInfo.GetScriptId()));

		if (pHandler)
		{
			CPickupPlacement* pPlacement = static_cast<CPickupPlacement*>(pHandler->GetScriptObject(m_placementInfo.GetObjectId()));

			if (pPlacement)
			{
				// If the pickup already has a network object, two machines have registered a placement simultaneously.
				// The placement creates will sort out the ownership conflict so just ignore this create for the time being.
				if (pPlacement->GetPickup())
				{
					gnetAssert(pPlacement->GetPickup()->GetNetworkObject());
					gnetAssert(pPlacement->GetPickup()->GetNetworkObject()->IsClone());

					CNetwork::GetMessageLog().Log("\tREJECT: Ownership conflict\r\n");

					bSuccess = false;
				}
				else if (!pPlacement->GetNetworkObject())
				{
					CNetwork::GetMessageLog().Log("\tREJECT: Placement has no network object\r\n");

					bSuccess = false;
				}
				else if (pPlacement->GetNetworkObject()->GetPlayerOwner() != static_cast<netObject*>(pObj)->GetPlayerOwner())
				{
					CNetwork::GetMessageLog().Log("\tREJECT: The placement for this pickup is owned by a different player (%s)\r\n", pPlacement->GetNetworkObject()->GetPlayerOwner() ? pPlacement->GetNetworkObject()->GetPlayerOwner()->GetLogName() : "-NONE-");

					bSuccess = false;
				}
				else if (pPlacement->GetIsCollected())
				{
					// we need to wait for the sync update from the placement verifying it is collected before allowing the pickup to be collected
					CNetwork::GetMessageLog().Log("\tREJECT: Placement is still flagged as collected\r\n");

					bSuccess = false;
				}
				else
				{
					pickupHash = pPlacement->GetPickupHash();
				}
			}
			else
			{
				CNetwork::GetMessageLog().Log("\tREJECT: No corresponding placement\r\n");

				// no placement yet
				bSuccess = false;
			}
		}
		else
		{
			CNetwork::GetMessageLog().Log("\tREJECT: No corresponding script handler\r\n");

			// no script handler yet
			bSuccess = false;
		}
	}
	else
	{
		if(!CPickupManager::IsNetworkedMoneyPickupAllowed() && CPickupManager::PickupHashIsMoneyType(m_pickupHash))
		{
			CNetwork::GetMessageLog().Log("\tREJECT: Money pickup cloning is disabled\r\n");
			return false;
		}

		// always leave a few slots clear for important pickups
		static const unsigned IMPORTANT_PICKUP_SLOTS = 4;

		bool bImportantPickup = static_cast<CNetObjGame*>(pObj)->IsScriptObject() || m_bPlayerGift;

		u32 maxFreeSpaces = bImportantPickup ? 1 : IMPORTANT_PICKUP_SLOTS;

		if (CPickup::GetPool()->GetNoOfFreeSpaces() < maxFreeSpaces || CNetObjPickup::GetPool()->GetNoOfFreeSpaces() < maxFreeSpaces)
		{
			CPickupManager::ClearSpaceInPickupPool(true);

			if (CPickup::GetPool()->GetNoOfFreeSpaces() < maxFreeSpaces)
			{
				CNetwork::GetMessageLog().Log("\tREJECT: No space left in pickup pool\r\n");
				bSuccess = false;				
			}
			else if (CNetObjPickup::GetPool()->GetNoOfFreeSpaces() < maxFreeSpaces)
			{
				CNetwork::GetMessageLog().Log("\tREJECT: No space left in CNetObjPickup pool\r\n");
				bSuccess = false;
			}
		}

		pickupHash = m_pickupHash;
	}

	if (bSuccess)
	{
		// ensure pickup model is streamed in
		const CPickupData* pPickupData = CPickupDataManager::GetPickupData(pickupHash);

		if (gnetVerifyf(pPickupData, "Trying to create a clone pickup with no corresponding pickup metadata"))
		{
			u32 modelIndex = pPickupData->GetModelIndex();
			fwModelId modelId((strLocalIndex(modelIndex)));

			if (m_customModelHash != 0)
			{
				fwModelId modelId;
				CModelInfo::GetBaseModelInfoFromHashKey(m_customModelHash, &modelId);

				if (gnetVerifyf(modelId.IsValid(), "Unrecognised custom model hash for pickup clone"))
				{
					modelIndex = modelId.GetModelIndex();
				}
			}

			if(!CNetworkStreamingRequestManager::RequestModel(modelId))
			{
				CNetwork::GetMessageLog().Log("\tREJECT: Model is not streamed in\r\n");
				bSuccess = false;
			}
		}

		// check to see if a varmod exists
		const CWeaponComponentVariantModelInfo* pVarModInfo = NULL;
		for (u32 i = 0; i < m_numWeaponComponents; i++)
		{
			const CWeaponComponentInfo* pComponentInfo = CWeaponComponentManager::GetInfo(m_weaponComponents[i]);
			if (pComponentInfo->GetClassId() == WCT_VariantModel)
			{
				pVarModInfo = static_cast<const CWeaponComponentVariantModelInfo*>(pComponentInfo);
				break;
			}
		}

		// ensure the component models are streamed in
		for (u32 i=0; i<m_numWeaponComponents; i++)
		{
			const CWeaponComponentInfo* pComponentInfo = CWeaponComponentManager::GetInfo(m_weaponComponents[i]);

			if (AssertVerify(pComponentInfo))
			{
				u32 uModelHash = pComponentInfo->GetModelHash();

				// check varmod to see if we're using an alternate model
				if (pVarModInfo)
				{
					u32 uVarModelHash = pVarModInfo->GetExtraComponentModel(pComponentInfo->GetHash());
					if (uVarModelHash != 0)
					{
						uModelHash = uVarModelHash;
					}
				}

				fwModelId componentModelId;
				const CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey(uModelHash, &componentModelId);

				if (AssertVerify(pModelInfo) && !pModelInfo->GetHasLoaded())
				{
					if(!CNetworkStreamingRequestManager::RequestModel(componentModelId))
					{
						CNetwork::GetMessageLog().Log("\tREJECT: Component model is not streamed in\r\n");
						bSuccess = false;
					}
				}
			}
		}
	}

	return bSuccess;
}

void CPickupCreationDataNode::ExtractDataForSerialising(IPickupNodeDataAccessor &pickupNodeData)
{
	pickupNodeData.GetPickupCreateData(*this);
}

void CPickupCreationDataNode::ApplyDataFromSerialising(IPickupNodeDataAccessor &pickupNodeData)
{
	pickupNodeData.SetPickupCreateData(*this);
}

void CPickupCreationDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned int SIZEOF_PICKUP_HASH	= 32;
	static const unsigned int SIZEOF_AMOUNT			= 32;
	static const unsigned int SIZEOF_LIFETIME		= 18;
	static const unsigned int SIZEOF_NUM_WEAPON_COMPONENTS  = datBitsNeeded<IPedNodeDataAccessor::MAX_SYNCED_WEAPON_COMPONENTS>::COUNT;
	static const unsigned int SIZEOF_WEAPON_COMPONENT_HASH  = 32;
	static const unsigned int SIZEOF_TINT_INDEX				= 3;
	static const unsigned int SIZEOF_LOD_DISTANCE			= 11;
	static const unsigned int SIZEOF_BLOCKED_PLAYERS		= sizeof(PlayerFlags)<<3;

	SERIALISE_BOOL(serialiser, m_bHasPlacement, "Has placement");

	if (m_bHasPlacement)
		m_placementInfo.Serialise(serialiser);
	else
		SERIALISE_UNSIGNED(serialiser, m_pickupHash, SIZEOF_PICKUP_HASH, "Pickup hash");

	bool hasAmount = (m_amount > 0);

	SERIALISE_BOOL(serialiser, hasAmount);

	if (hasAmount)
	{
		SERIALISE_UNSIGNED(serialiser, m_amount, SIZEOF_AMOUNT, "Amount");
	}
	else
	{
		m_amount = 0;
	}

	bool hasCustomModel = m_customModelHash != 0;

	SERIALISE_BOOL(serialiser, hasCustomModel);

	if (hasCustomModel || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_MODELHASH(serialiser, m_customModelHash, "Custom model");
	}
	else
	{
		m_customModelHash = 0;
	}

	if (!m_bHasPlacement)
	{
		bool hasLifeTime = (m_lifeTime > 0);

		SERIALISE_BOOL(serialiser, hasLifeTime);

		if (hasLifeTime || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_UNSIGNED(serialiser, m_lifeTime, SIZEOF_LIFETIME, "Life Time");
		}
		else
		{
			m_lifeTime = 0;
		}

		if(serialiser.GetIsMaximumSizeSerialiser())
		{
			m_numWeaponComponents = IPedNodeDataAccessor::MAX_SYNCED_WEAPON_COMPONENTS;
		}

		bool bHasWeaponMods = m_numWeaponComponents > 0 || m_weaponTintIndex != 0;

		SERIALISE_BOOL(serialiser, bHasWeaponMods);

		if (bHasWeaponMods)
		{
			SERIALISE_UNSIGNED(serialiser, m_numWeaponComponents, SIZEOF_NUM_WEAPON_COMPONENTS, "Num Weapon Components");

			for(u32 index = 0; index < m_numWeaponComponents; index++)
			{
				SERIALISE_UNSIGNED(serialiser, m_weaponComponents[index], SIZEOF_WEAPON_COMPONENT_HASH, "Weapon component Hash");
			}

			SERIALISE_UNSIGNED(serialiser, m_weaponTintIndex, SIZEOF_TINT_INDEX, "Tint Index");
		}
		else
		{
			m_numWeaponComponents = 0;
			m_weaponTintIndex = 0;
		}

		SERIALISE_BOOL(serialiser, m_bPlayerGift, "Player Gift");
	}
	else
	{
		m_numWeaponComponents = 0;
		m_weaponTintIndex = 0;
	}

	SERIALISE_BOOL(serialiser, m_bHasPlayersBlockingList, "Has List Of Players To Block From Creation");
	if(m_bHasPlayersBlockingList || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_BITFIELD(serialiser, m_PlayersToBlockList, SIZEOF_BLOCKED_PLAYERS, "Hidden From Players List");
	}

	bool hasLODdistance = m_LODdistance != 0;
	SERIALISE_BOOL(serialiser, hasLODdistance, "Has Modified LOD Distance");
	if(hasLODdistance || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_LODdistance, SIZEOF_LOD_DISTANCE, "LOD Distance");
	}
	else
	{
		m_LODdistance = 0;
	}

	SERIALISE_BOOL(serialiser, m_includeProjectiles, "Include Projectiles");
}

PlayerFlags CPickupSectorPosNode::StopSend(const netSyncTreeTargetObject* pObj, SerialiseModeFlags UNUSED_PARAM(serMode)) const
{
	PlayerFlags playerFlags = GetIsAttachedForPlayers(pObj);

	return playerFlags;
}

void CPickupSectorPosNode::ExtractDataForSerialising(IPickupNodeDataAccessor &nodeData)
{
	nodeData.GetPickupSectorPosData(*this);
}

void CPickupSectorPosNode::ApplyDataFromSerialising(IPickupNodeDataAccessor &nodeData)
{
	nodeData.SetPickupSectorPosData(*this);
}

void CPickupSectorPosNode::Serialise(CSyncDataBase& serialiser)
{
	const unsigned SIZEOF_OBJECT_SECTORPOS_HIGH_PRECISION = 20;
	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_SectorPosX, (float)WORLD_WIDTHOFSECTOR_NETWORK,  SIZEOF_OBJECT_SECTORPOS_HIGH_PRECISION, "Sector Pos X (high precision)");
	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_SectorPosY, (float)WORLD_DEPTHOFSECTOR_NETWORK,  SIZEOF_OBJECT_SECTORPOS_HIGH_PRECISION, "Sector Pos Y (high precision)");
	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_SectorPosZ, (float)WORLD_HEIGHTOFSECTOR_NETWORK, SIZEOF_OBJECT_SECTORPOS_HIGH_PRECISION, "Sector Pos Z (high precision)");
}

void CPickupScriptGameStateNode::ExtractDataForSerialising(IPickupNodeDataAccessor &pickupNodeData)
{
	pickupNodeData.GetPickupScriptGameStateData(*this);

	m_teamPermits &= (1<<SIZEOF_TEAM_PERMITS)-1;
}

void CPickupScriptGameStateNode::ApplyDataFromSerialising(IPickupNodeDataAccessor &pickupNodeData)
{
	pickupNodeData.SetPickupScriptGameStateData(*this);
}

const unsigned int CPickupScriptGameStateNode::SIZEOF_TEAM_PERMITS	= MAX_NUM_TEAMS;

void CPickupScriptGameStateNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned int SIZEOF_FLAGS			= CPickup::NUM_SYNCED_PICKUP_FLAGS;
	static const unsigned int SIZEOF_OFFSET_GLOW	= 8;

	SERIALISE_UNSIGNED(serialiser, m_flags, SIZEOF_FLAGS, "Flags");

	SERIALISE_BOOL(serialiser, m_bFloating, "Floating");

	if ((m_flags & CPickup::PF_TeamPermitsSet) || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_teamPermits, SIZEOF_TEAM_PERMITS, "Team Permits");
	}

	if ((m_flags & CPickup::PF_OffsetGlow) || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_PACKED_FLOAT(serialiser, m_offsetGlow, 1.0f, SIZEOF_OFFSET_GLOW, "Offset Glow");
	}

	SERIALISE_BOOL(serialiser, m_portable);

	if (m_portable || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_BOOL(serialiser, m_inAccessible, "Inaccessible");
		SERIALISE_POSITION(serialiser, m_lastAccessibleLoc, "Last accessible location");
		SERIALISE_BOOL(serialiser, m_lastAccessibleLocHasValidGround, "LAL has valid ground");
	}

	SERIALISE_BOOL(serialiser, m_allowNonScriptParticipantCollect, "Allow Non Script Participant Collection");
}