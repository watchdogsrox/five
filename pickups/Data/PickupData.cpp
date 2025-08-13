// File header
#include "Pickups/Data/PickupData.h"

// Rage headers
#include "audioengine/engine.h"

// Game headers
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "Pickups/Data/PickupDataManager.h"
#include "Pickups/PickupRewards.h"
#include "streaming/streaming.h"

WEAPON_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CItemInfo
//////////////////////////////////////////////////////////////////////////

FW_INSTANTIATE_CLASS_POOL(CPickupData, CPickupData::MAX_STORAGE, atHashString("CPickupData",0x8cc4f9df));

// Get the model index from the model hash
u32 CPickupData::GetModelIndex() const
{
	if(GetModelHash() == 0)
	{
		return fwModelId::MI_INVALID;
	}

	fwModelId iModelId;
	CModelInfo::GetBaseModelInfoFromHashKey(GetModelHash(), &iModelId);
	Assertf(iModelId.IsValid(), "Invalid model ID for pickup: %s",GetModelName());
	return iModelId.GetModelIndex();
}

u32 CPickupData::GetFirstWeaponReward() const
{
	for (u32 i=0; i<Rewards.GetCount(); i++)
	{
		const CPickupRewardData* pReward = GetReward(i);
		if (pReward && pReward->GetType() == PICKUP_REWARD_TYPE_WEAPON)
		{
			return static_cast<const CPickupRewardWeapon*>(pReward)->GetWeaponHash();
		}
	}
	return 0;
}

const CPickupActionData* CPickupData::GetOnFootAction(u32 index) const
{
	return CPickupDataManager::GetPickupActionData(OnFootPickupActions[index].GetHash());
}

const CPickupActionData* CPickupData::GetInCarAction(u32 index) const
{
	return CPickupDataManager::GetPickupActionData(InCarPickupActions[index].GetHash());
}

const CPickupActionData* CPickupData::GetOnShotAction(u32 index) const
{
	return CPickupDataManager::GetPickupActionData(OnShotPickupActions[index].GetHash());
}

const CPickupRewardData* CPickupData::GetReward(u32 index) const
{
	return CPickupDataManager::GetPickupRewardData(Rewards[index].GetHash());
}
