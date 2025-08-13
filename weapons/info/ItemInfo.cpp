//
// weapons/iteminfo.cpp
//
// Copyright (C) 1999-2010 Rockstar North.  All Rights Reserved. 
//

// File header
#include "Weapons/Info/ItemInfo.h"

// Parser files
#include "ItemInfo_parser.h"

// Game headers
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "Streaming/Streaming.h"
#include "Weapons/Info/WeaponInfoManager.h"

// Macro to disable optimisations if set
WEAPON_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////

// Simple rtti implementation
INSTANTIATE_RTTI_CLASS(CItemInfo,0x4DD17280);

////////////////////////////////////////////////////////////////////////////////

CItemInfo::CItemInfo()
{
}

////////////////////////////////////////////////////////////////////////////////

CItemInfo::CItemInfo(u32 uNameHash)
{
	m_Name.SetHash(uNameHash);
}

////////////////////////////////////////////////////////////////////////////////

CItemInfo::~CItemInfo()
{
}

////////////////////////////////////////////////////////////////////////////////

u32 CItemInfo::GetModelIndex() const
{
	if(GetModelHash() == 0)
	{
		return fwModelId::MI_INVALID;
	}

	fwModelId iModelId;
	CModelInfo::GetBaseModelInfoFromHashKey(GetModelHash(), &iModelId);
	return iModelId.GetModelIndex();
}

////////////////////////////////////////////////////////////////////////////////

u32 CItemInfo::GetModelIndex()
{
	if(GetModelHash() == 0)
	{
		return fwModelId::MI_INVALID;
	}

	fwModelId iModelId;
	CModelInfo::GetBaseModelInfoFromHashKey(GetModelHash(), &iModelId);
	return iModelId.GetModelIndex();
}

////////////////////////////////////////////////////////////////////////////////

const CBaseModelInfo* CItemInfo::GetModelInfo() const
{
	if(GetModelHash() == 0)
	{
		return NULL;
	}

	return CModelInfo::GetBaseModelInfoFromHashKey(GetModelHash(), NULL);
}

////////////////////////////////////////////////////////////////////////////////

CBaseModelInfo* CItemInfo::GetModelInfo()
{
	if(GetModelHash() == 0)
	{
		return NULL;
	}

	return CModelInfo::GetBaseModelInfoFromHashKey(GetModelHash(), NULL);
}

////////////////////////////////////////////////////////////////////////////////

bool CItemInfo::GetIsModelStreamedIn() const
{
	u32 iModelIndex = GetModelIndex();
	fwModelId iModelId((strLocalIndex(iModelIndex)));
	if(iModelId.IsValid())
	{
		return CModelInfo::HaveAssetsLoaded(iModelId);
	}

	// No valid model, return true
	return true;
}

////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
bool CItemInfo::Validate() const
{
	// Validate model
	if(!weaponVerifyf(GetModelHash() == 0 || GetModelInfo(), "%s: ModelInfo for model [%s] doesn't exist", GetName(), m_Model.GetCStr()))
	{
		return false;
	}

	// Validate slot
// 	if(m_Slot.GetHash() != 0)
// 	{
// 		bool bValidSlot = false;
// 		const CWeaponInfoManager::SlotList& slotList = CWeaponInfoManager::GetSlotListNavigate();
// 		for(s32 i = 0; i < slotList.GetCount(); i++)
// 		{
// 			if(slotList[i].GetHash() == GetSlot())
// 			{
// 				bValidSlot = true;
// 				break;
// 			}
// 		}
// 
// 		if(!weaponVerifyf(bValidSlot, "%s: Slot [%s] doesn't exist", GetName(), m_Slot.GetCStr()))
// 		{
// 			return false;
// 		}
// 	}

	// Tests passed
	return true;
}
#endif // !__FINAL

////////////////////////////////////////////////////////////////////////////////

void CItemInfo::OnPostLoad()
{
}
