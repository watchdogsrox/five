//
// weapons/weaponhelpers.cpp
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

// File header
#include "Weapons/Helpers/WeaponHelpers.h"

// Rage headers
#include "crskeleton/skeletondata.h"
#include "parser/treenode.h"

// Game headers
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "physics/gtaArchetype.h"

// Parser header
#include "WeaponHelpers_parser.h"

WEAPON_OPTIMISATIONS()


////////////////////////////////////////////////////////////////////////////////

CWeaponBoneId::CWeaponBoneId()
: m_BoneId(0)
{
}

////////////////////////////////////////////////////////////////////////////////

s32 CWeaponBoneId::GetBoneIndex(const u32 uModelIndex) const
{
	s32 iIdx = 0;
	if(CModelInfo::IsValidModelInfo(uModelIndex))
	{
		const CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(uModelIndex)));
		if(pModelInfo)
		{
			iIdx = GetBoneIndex(pModelInfo);
		}
	}

	return iIdx;
}

////////////////////////////////////////////////////////////////////////////////

s32 CWeaponBoneId::GetBoneIndex(const CBaseModelInfo* pModelInfo) const
{
	Assert(pModelInfo);
	s32 iIdx = 0;

	const crSkeletonData* pSkeletonData = NULL;
	const gtaFragType* pFragType = pModelInfo->GetFragType();
	if(pFragType)
	{
		Assertf(pFragType->GetCommonDrawable(), "ERROR : CWeaponBoneId::GetBoneIndex : Model %s FragType Has No Common Drawable?!", pModelInfo->GetModelName());
		if(pFragType->GetCommonDrawable())
		{
			pSkeletonData = pFragType->GetCommonDrawable()->GetSkeletonData();
		}
	}
	else 
	{
		Assertf(pModelInfo->GetDrawable(), "ERROR : CWeaponBoneId::GetBoneIndex : Model %s ModelInfo Has No Drawable?!", pModelInfo->GetModelName());
		if(pModelInfo->GetDrawable())
		{
			pSkeletonData = pModelInfo->GetDrawable()->GetSkeletonData();
		}
	}

	if(pSkeletonData)
	{
		if(!pSkeletonData->ConvertBoneIdToIndex(m_BoneId, iIdx))
		{
			// Assume the root bone
			iIdx = 0;
		}
	}
	
	return iIdx;
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponBoneId::OnPreLoad(parTreeNode* pNode)
{
	const char* name = pNode->GetData();
	if(name)
	{
		m_BoneId = crSkeletonData::ConvertBoneNameToId(name);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponBoneId::OnPostSave(parTreeNode* NOTFINAL_ONLY(pNode))
{
#if !__FINAL
	const char* name = crSkeletonData::DebugConvertBoneIdToName(m_BoneId);
	if(name)
	{
		pNode->SetData(name, ustrlen(name));
	}
#endif // !__FINAL
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponBoneId::OnPostAddWidgets(bkBank& BANK_ONLY(bank))
{
#if __BANK
	const char* name = crSkeletonData::DebugConvertBoneIdToName(m_BoneId);
	if(name)
	{
		bank.AddTitle(name);
	}
#endif // __BANK
}

////////////////////////////////////////////////////////////////////////////////
