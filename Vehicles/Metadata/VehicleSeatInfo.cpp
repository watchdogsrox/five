//
// Vehicles/Metadata/VehicleSeatInfo.cpp
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

#include "Vehicles/Metadata/VehicleSeatInfo.h"

#include "fwanimation/animmanager.h"
#include "fwanimation/clipsets.h"
#include "Peds/Ped.h" 
#include "Vehicles/vehicle_channel.h"
#include "Vehicles/Metadata/VehicleMetadataManager.h"
#include "Weapons/WeaponTypes.h"

// Parser files
#include "VehicleSeatInfo_parser.h"

////////////////////////////////////////////////////////////////////////////////

AI_OPTIMISATIONS()


////////////////////////////////////////////////////////////////////////////////

const char* CDrivebyWeaponGroup::GetNameFromInfo(const CDrivebyWeaponGroup* NOTFINAL_ONLY(pInfo))
{
#if !__FINAL
	if(pInfo)
	{
		return pInfo->GetName().GetCStr();
	}
#endif // !__FINAL
	return "";
}

////////////////////////////////////////////////////////////////////////////////

const CDrivebyWeaponGroup* CDrivebyWeaponGroup::GetInfoFromName(const char* name)
{
	const CDrivebyWeaponGroup* pInfo = CVehicleMetadataMgr::GetDrivebyWeaponGroup(atStringHash(name));
	vehicleAssertf(pInfo, "CDrivebyWeaponGroup [%s] doesn't exist in data", name);
	return pInfo;
}


////////////////////////////////////////////////////////////////////////////////

const char* CVehicleDriveByAnimInfo::GetNameFromInfo(const CVehicleDriveByAnimInfo* NOTFINAL_ONLY(pInfo))
{
#if !__FINAL
	if(pInfo)
	{
		return pInfo->GetName().GetCStr();
	}
#endif // !__FINAL
	return "";
}

////////////////////////////////////////////////////////////////////////////////

const CVehicleDriveByAnimInfo* CVehicleDriveByAnimInfo::GetInfoFromName(const char* name)
{
	const CVehicleDriveByAnimInfo* pInfo = CVehicleMetadataMgr::GetVehicleDriveByAnimInfo(atStringHash(name));
	vehicleAssertf(pInfo, "VehicleDriveByAnimInfo [%s] doesn't exist in data", name);
	return pInfo;
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CVehicleDriveByAnimInfo::GetFirstPersonClipSet(u32 uWeaponHash) const
{
	if (uWeaponHash != 0)
	{
		for(s32 i = 0; i < m_AltFirstPersonDriveByClipSets.GetCount(); i++)
		{
			const CVehicleDriveByAnimInfo::sAltClips& ac = m_AltFirstPersonDriveByClipSets[i];
			for(s32 j = 0; j < ac.m_Weapons.GetCount(); j++)
			{
				if(ac.m_Weapons[j].GetHash() == uWeaponHash)
				{
					return fwMvClipSetId(ac.m_ClipSet);
				}
			}
		}
	}

	return fwMvClipSetId(m_FirstPersonDriveByClipSet);
}

////////////////////////////////////////////////////////////////////////////////

bool CVehicleDriveByAnimInfo::ContainsWeaponGroup(u32 uHash) const
{	
	if (vehicleVerifyf(m_WeaponGroup, "NULL weapon group"))
	{
		const atArray<atHashWithStringNotFinal>& weaponGroupNames = m_WeaponGroup->GetWeaponGroupNames();
		for (s32 i=0; i<weaponGroupNames.GetCount(); i++)
		{
			if (weaponGroupNames[i].GetHash() == uHash)
			{
				return true;
			}
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CVehicleDriveByAnimInfo::ContainsWeaponType(u32 uHash) const
{	
	if (vehicleVerifyf(m_WeaponGroup, "NULL weapon slot group"))
	{
		const atArray<atHashWithStringNotFinal>& weaponTypeNames = m_WeaponGroup->GetWeaponTypeNames();
		for (s32 i=0; i<weaponTypeNames.GetCount(); i++)
		{
			if (weaponTypeNames[i].GetHash() == uHash)
			{
				return true;
			}
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

const char* CVehicleDriveByInfo::GetNameFromInfo(const CVehicleDriveByInfo* NOTFINAL_ONLY(pInfo))
{
#if !__FINAL
	if(pInfo)
	{
		return pInfo->GetName().GetCStr();
	}
#endif // !__FINAL
	return "";
}

////////////////////////////////////////////////////////////////////////////////

const CVehicleDriveByInfo* CVehicleDriveByInfo::GetInfoFromName(const char* name)
{
	const CVehicleDriveByInfo* pInfo = CVehicleMetadataMgr::GetVehicleDriveByInfo(atStringHash(name));
	vehicleAssertf(pInfo, "VehicleDriveByInfo [%s] doesn't exist in data", name);
	return pInfo;
}

////////////////////////////////////////////////////////////////////////////////

bool CVehicleDriveByInfo::CanUseWeaponGroup(u32 uGroupHash) const
{
	for (s32 i=0; i<m_DriveByAnimInfos.GetCount(); i++)
	{
		const CVehicleDriveByAnimInfo* pDriveByAnimInfo = m_DriveByAnimInfos[i];
		if (pDriveByAnimInfo && pDriveByAnimInfo->ContainsWeaponGroup(uGroupHash))
		{
			return true;
		}
	}
	return false;
}


////////////////////////////////////////////////////////////////////////////////

bool CVehicleDriveByInfo::CanUseWeaponType(u32 uTypeHash) const
{
	for (s32 i=0; i<m_DriveByAnimInfos.GetCount(); i++)
	{
		const CVehicleDriveByAnimInfo* pDriveByAnimInfo = m_DriveByAnimInfos[i];
		if (pDriveByAnimInfo && pDriveByAnimInfo->ContainsWeaponType(uTypeHash))
		{
			return true;
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

float CVehicleDriveByInfo::GetMinAimSweepHeadingAngleDegs(const CPed* pPed) const
{
	u32 uWeaponHash = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponHash() : 0;

	if (uWeaponHash != 0)
	{
		const CVehicleDriveByAnimInfo* pAnimInfo = CVehicleMetadataMgr::GetDriveByAnimInfoForWeapon(pPed, uWeaponHash);
		if (pAnimInfo)
		{
			bool bDontUseOverride = pAnimInfo->GetOverrideAnglesInThirdPersonOnly() && pPed->IsInFirstPersonVehicleCamera();
			if (pAnimInfo->GetUseOverrideAngles() && !bDontUseOverride)
			{
				return pAnimInfo->GetOverrideMinAimAngle();
			}
		}
	}
	return m_MinAimSweepHeadingAngleDegs;
}

////////////////////////////////////////////////////////////////////////////////

float CVehicleDriveByInfo::GetMaxAimSweepHeadingAngleDegs(const CPed* pPed) const
{
	u32 uWeaponHash = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponHash() : 0;

	if (uWeaponHash != 0)
	{
		const CVehicleDriveByAnimInfo* pAnimInfo = CVehicleMetadataMgr::GetDriveByAnimInfoForWeapon(pPed, uWeaponHash);
		if (pAnimInfo)
		{
			bool bDontUseOverride = pAnimInfo->GetOverrideAnglesInThirdPersonOnly() && pPed->IsInFirstPersonVehicleCamera();
			if (pAnimInfo->GetUseOverrideAngles() && !bDontUseOverride)
			{
				return pAnimInfo->GetOverrideMaxAimAngle();
			}
		}
	}
	return m_MaxAimSweepHeadingAngleDegs;
}

////////////////////////////////////////////////////////////////////////////////

float CVehicleDriveByInfo::GetMinRestrictedAimSweepHeadingAngleDegs(const CPed* pPed) const
{
	u32 uWeaponHash = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponHash() : 0;

	if (uWeaponHash != 0)
	{
		const CVehicleDriveByAnimInfo* pAnimInfo = CVehicleMetadataMgr::GetDriveByAnimInfoForWeapon(pPed, uWeaponHash);
		if (pAnimInfo)
		{
			bool bDontUseOverride = pAnimInfo->GetOverrideAnglesInThirdPersonOnly() && pPed->IsInFirstPersonVehicleCamera();
			if (pAnimInfo->GetUseOverrideAngles() && !bDontUseOverride)
			{
				return pAnimInfo->GetOverrideMinRestrictedAimAngle();
			}
		}
	}
	return m_MinRestrictedAimSweepHeadingAngleDegs;
}

////////////////////////////////////////////////////////////////////////////////

float CVehicleDriveByInfo::GetMaxRestrictedAimSweepHeadingAngleDegs(const CPed* pPed) const
{
	u32 uWeaponHash = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponHash() : 0;

	if (uWeaponHash != 0)
	{
		const CVehicleDriveByAnimInfo* pAnimInfo = CVehicleMetadataMgr::GetDriveByAnimInfoForWeapon(pPed, uWeaponHash);
		if (pAnimInfo)
		{
			bool bDontUseOverride = pAnimInfo->GetOverrideAnglesInThirdPersonOnly() && pPed->IsInFirstPersonVehicleCamera();
			if (pAnimInfo->GetUseOverrideAngles() && !bDontUseOverride)
			{
				return pAnimInfo->GetOverrideMaxRestrictedAimAngle();
			}
		}
	}
	return m_MaxRestrictedAimSweepHeadingAngleDegs;
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipId CVehicleSeatAnimInfo::ms_DeadClipId("Die",0x866EE326);

////////////////////////////////////////////////////////////////////////////////

const char* CVehicleSeatAnimInfo::GetNameFromInfo(const CVehicleSeatAnimInfo* NOTFINAL_ONLY(pInfo))
{
#if !__FINAL
	if(pInfo)
	{
		return pInfo->GetName().GetCStr();
	}
#endif // !__FINAL
	return "";
}

////////////////////////////////////////////////////////////////////////////////

const CVehicleSeatAnimInfo* CVehicleSeatAnimInfo::GetInfoFromName(const char* name)
{
	const CVehicleSeatAnimInfo* pInfo = CVehicleMetadataMgr::GetVehicleSeatAnimInfo(atStringHash(name));
	vehicleAssertf(pInfo, "VehicleSeatAnimInfo [%s] doesn't exist in data", name);
	return pInfo;
}

////////////////////////////////////////////////////////////////////////////////

bool CVehicleSeatAnimInfo::FindAnim(eSeatAnimType nAnimType, s32& iDictIndex, u32& iAnimHash, bool bFirstPerson) const
{
	bool bFoundAnim = false;

	if (nAnimType == DEAD)
	{
		bFoundAnim = FindAnimInClipSet(m_InsideClipSetMap->GetMaps()[0].m_ClipSetId, ms_DeadClipId.GetHash(), iDictIndex, iAnimHash);
	}
	else if (nAnimType == SMASH_WINDOW)
	{
		if (GetDriveByInfo())
		{
			const u32 smashWindowAnimHash = ATSTRINGHASH("smash_window", 0x0cf4502dc);
			const CVehicleDriveByAnimInfo* pDriveByAnimInfo = CVehicleMetadataMgr::GetDriveByAnimInfoForWeaponGroup(this, WEAPONGROUP_PISTOL.GetHash());	
			if(pDriveByAnimInfo)
			{
				fwMvClipSetId smashWindowClipset = (bFirstPerson && pDriveByAnimInfo->GetFirstPersonClipSet() != CLIP_SET_ID_INVALID) ? pDriveByAnimInfo->GetFirstPersonClipSet() : pDriveByAnimInfo->GetClipSet();
				bFoundAnim = pDriveByAnimInfo ? FindAnimInClipSet(smashWindowClipset, smashWindowAnimHash, iDictIndex, iAnimHash) : false;
			}
		}
	}
	else if (nAnimType == SMASH_WINDSCREEN)
	{
		if (GetDriveByInfo())
		{
			const u32 smashWindScreenAnimHash = ATSTRINGHASH("smash_windscreen", 0x062f1e5dd);
			const CVehicleDriveByAnimInfo* pDriveByAnimInfo = CVehicleMetadataMgr::GetDriveByAnimInfoForWeaponGroup(this, WEAPONGROUP_PISTOL.GetHash());
			if(pDriveByAnimInfo)
			{
				fwMvClipSetId smashWindScreenClipset = (bFirstPerson && pDriveByAnimInfo->GetFirstPersonClipSet() != CLIP_SET_ID_INVALID) ? pDriveByAnimInfo->GetFirstPersonClipSet() : pDriveByAnimInfo->GetClipSet();
				bFoundAnim = pDriveByAnimInfo ? FindAnimInClipSet(smashWindScreenClipset, smashWindScreenAnimHash, iDictIndex, iAnimHash) : false;
			}
		}
	}
	else
	{
		vehicleAssertf(0, "Invalid anim type");
	}

	// Found something
	return bFoundAnim;
}

////////////////////////////////////////////////////////////////////////////////

bool CVehicleSeatAnimInfo::FindAnimInClipSet(const fwMvClipSetId &clipSetIdToSearch, u32 iDesiredAnimHash, s32& iDictIndex, u32& iAnimHash)
{
	const fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetIdToSearch);
	if (!vehicleVerifyf(pClipSet, "Couldn't find clipset for %s", clipSetIdToSearch != CLIP_SET_ID_INVALID ? clipSetIdToSearch.GetCStr() : "INVALID"))
	{
		return false;
	}

	iDictIndex = pClipSet->GetClipDictionaryIndex().Get();

	iAnimHash = iDesiredAnimHash;

	static u32 MAX_NUM_FALLBACKS = 10;
	u32 uFallbackCount = 0; 

	if (!fwAnimManager::GetClipIfExistsByDictIndex(iDictIndex, iDesiredAnimHash))
	{
		while (pClipSet && vehicleVerifyf(uFallbackCount < MAX_NUM_FALLBACKS, "More than %u fallback clipsets, possible infinite loop detected", uFallbackCount))
		{
			const fwClipSet* pFallbackClipSet = fwClipSetManager::GetClipSet(pClipSet->GetFallbackId());
			if (pFallbackClipSet)
			{
				iDictIndex = pFallbackClipSet->GetClipDictionaryIndex().Get();

				if (fwAnimManager::GetClipIfExistsByDictIndex(iDictIndex, iDesiredAnimHash))
				{
					return true; 
				}
				pClipSet = pFallbackClipSet;
			}
			else
			{
				pClipSet = NULL;
			}
			++uFallbackCount;
		}
	}
	else
	{
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

const char* CVehicleSeatInfo::GetNameFromInfo(const CVehicleSeatInfo* NOTFINAL_ONLY(pInfo))
{
#if !__FINAL
	if(pInfo)
	{
		return pInfo->GetName().GetCStr();
	}
#endif // !__FINAL
	return "";
}

////////////////////////////////////////////////////////////////////////////////

const CVehicleSeatInfo* CVehicleSeatInfo::GetInfoFromName(const char* name)
{
	const CVehicleSeatInfo* pInfo = CVehicleMetadataMgr::GetVehicleSeatInfo(atStringHash(name));
	vehicleAssertf(pInfo, "VehicleSeatInfo [%s] doesn't exist in data", name);
	return pInfo;
}

////////////////////////////////////////////////////////////////////////////////

void CVehicleSeatInfo::LoadLinkPtrs() 
{
	u32 uShuffleHash = m_ShuffleLink.GetHash();
	m_ShuffleLinkPtr = (uShuffleHash != 0) ? CVehicleMetadataMgr::GetVehicleSeatInfo(uShuffleHash) : NULL;
	u32 uRearHash = m_RearSeatLink.GetHash();
	m_RearSeatLinkPtr = (uRearHash != 0) ? CVehicleMetadataMgr::GetVehicleSeatInfo(uRearHash) : NULL;
	u32 uShuffle2Hash = m_ShuffleLink2.GetHash();
	m_ShuffleLink2Ptr = (uShuffle2Hash != 0) ? CVehicleMetadataMgr::GetVehicleSeatInfo(uShuffle2Hash) : NULL;
}

const bool CVehicleSeatInfo::ShouldRemoveHeadProp(CPed& rPed) const
{
	if (m_SeatFlags.IsFlagSet(DisallowHeadProp))
		return true;

	// only remove props if they are flagged as "not in car"
	if (CPedPropsMgr::CheckPropFlags(&rPed, ANCHOR_HEAD, PV_FLAG_NOT_IN_CAR) && !rPed.GetPedResetFlag(CPED_RESET_FLAG_AllowHeadPropInVehicle))
		return true;

	return false;
}

////////////////////////////////////////////////////////////////////////////////

const char* CSeatOverrideAnimInfo::GetNameFromInfo(const CSeatOverrideAnimInfo* NOTFINAL_ONLY(pInfo))
{
#if !__FINAL
	if(pInfo)
	{
		return pInfo->GetName().GetCStr();
	}
#endif // !__FINAL
	return "";
}

////////////////////////////////////////////////////////////////////////////////

const CSeatOverrideAnimInfo* CSeatOverrideAnimInfo::GetInfoFromName(const char* name)
{
	const CSeatOverrideAnimInfo* pInfo = CVehicleMetadataMgr::GetSeatOverrideAnimInfo(atStringHash(name));
	vehicleAssertf(pInfo, "SeatOverrideAnimInfo [%s] doesn't exist in data", name);
	return pInfo;
}

////////////////////////////////////////////////////////////////////////////////

const char* CInVehicleOverrideInfo::GetNameFromInfo(const CInVehicleOverrideInfo* NOTFINAL_ONLY(pInfo))
{
#if !__FINAL
	if(pInfo)
	{
		return pInfo->GetName().GetCStr();
	}
#endif // !__FINAL
	return "";
}

////////////////////////////////////////////////////////////////////////////////

const CInVehicleOverrideInfo* CInVehicleOverrideInfo::GetInfoFromName(const char* name)
{
	const CInVehicleOverrideInfo* pInfo = CVehicleMetadataMgr::GetInVehicleOverrideInfo(atStringHash(name));
	vehicleAssertf(pInfo, "InVehicleOverrideInfo [%s] doesn't exist in data", name);
	return pInfo;
}

////////////////////////////////////////////////////////////////////////////////

const CSeatOverrideAnimInfo* CInVehicleOverrideInfo::FindSeatOverrideAnimInfoFromSeatAnimInfo(const CVehicleSeatAnimInfo* pSeatAnimInfo) const
{
	for (s32 i=0; i<m_SeatOverrideInfos.GetCount(); ++i)
	{
		if (m_SeatOverrideInfos[i]->GetSeatAnimInfo() == pSeatAnimInfo)
		{
			return m_SeatOverrideInfos[i]->GetSeatOverrideAnimInfo();
		}
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////
