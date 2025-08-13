//
// Vehicles/VehicleMetadataManager.cpp
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

// Rage headers
#include "fwanimation/clipsets.h"
#include "fwanimation/animdirector.h"
#include "parser/manager.h"
#include "parser/restparserservices.h"

// Game headers
#include "animation/debug/AnimViewer.h"
#include "file/device.h"
#include "modelinfo/PedModelInfo.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Vehicles/vehicle_channel.h"
#include "Vehicles/Metadata/VehicleDebug.h"
#include "Vehicles/Metadata/VehicleMetadataManager.h"
#include "Vehicles/Metadata/VehicleLayoutInfo.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"
#include "Vehicles/Metadata/VehicleEntryPointInfo.h"
#include "Vehicles/Metadata/VehicleExplosionInfo.h"
#include "Vehicles/Metadata/VehicleScenarioLayoutInfo.h"
#include "Vehicles/VehicleGadgets.h"

// Parser Header
#include "Vehicles/Metadata/VehicleMetadataManager_parser.h"

////////////////////////////////////////////////////////////////////////////////

#define VEHICLE_LAYOUTS_BASE_FILE "commoncrc:/data/ai/vehiclelayouts"
#define VEHICLE_LAYOUTS_BASE_FILE_WITH_EXT "commoncrc:/data/ai/vehiclelayouts.meta"

////////////////////////////////////////////////////////////////////////////////

AI_OPTIMISATIONS()

CVehicleMetadataMgr CVehicleMetadataMgr::ms_Instance;
CVehicleMetadataMgr *CVehicleMetadataMgr::ms_InstanceExtra = NULL;

#if __BANK
atArray<atHashString> CVehicleMetadataMgr::ms_VehicleLayoutFiles;
int CVehicleMetadataMgr::ms_CurrentSelection = 0;	// init combo box selection index
bkButton* CVehicleMetadataMgr::ms_pCreateButton = NULL;
bkGroup* CVehicleMetadataMgr::ms_pAnimGroup = NULL;
#endif //__BANK


class CVehicleMetadataFileMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile & file)
	{
		CVehicleMetadataMgr::AppendExtra(file.m_filename);
		return true;
	}

	virtual void UnloadDataFile(const CDataFileMgr::DataFile & file) 
	{
		CVehicleMetadataMgr::RemoveExtra(file.m_filename);
	}

} g_vehicleMetadataFileMounter;

////////////////////////////////////////////////////////////////////////////////

const CAnimRateSet* CVehicleMetadataMgr::GetAnimRateSet(u32 uNameHash)
{
	if (uNameHash == 0)
	{
		return ms_Instance.m_AnimRateSets[0];
	}

	const CAnimRateSet* pEntry = ms_Instance.GetAnimRateSetByHash(uNameHash);

	if(pEntry)
		return pEntry;

	if(ms_InstanceExtra)
		pEntry = ms_InstanceExtra->GetAnimRateSetByHash(uNameHash);

	return pEntry;
}


////////////////////////////////////////////////////////////////////////////////

const CClipSetMap* CVehicleMetadataMgr::GetClipSetMap(u32 uNameHash)
{
	const CClipSetMap* pEntry = ms_Instance.GetClipSetMapByHash(uNameHash);

	if(pEntry)
		return pEntry;

	if(ms_InstanceExtra)
		pEntry = ms_InstanceExtra->GetClipSetMapByHash(uNameHash);

	return pEntry;
}

////////////////////////////////////////////////////////////////////////////////

const CBicycleInfo* CVehicleMetadataMgr::GetBicycleInfo(u32 uNameHash)
{
	const CBicycleInfo* pEntry = ms_Instance.GetBicycleInfoByHash(uNameHash);

	if(pEntry)
		return pEntry;

	if(ms_InstanceExtra)
		pEntry = ms_InstanceExtra->GetBicycleInfoByHash(uNameHash);

	return pEntry;
}


////////////////////////////////////////////////////////////////////////////////

const CPOVTuningInfo* CVehicleMetadataMgr::GetPOVTuningInfo(u32 uNameHash)
{
	const CPOVTuningInfo* pEntry = ms_Instance.GetPOVTuningInfoByHash(uNameHash);

	if(pEntry)
		return pEntry;

	if(ms_InstanceExtra)
		pEntry = ms_InstanceExtra->GetPOVTuningInfoByHash(uNameHash);

	return pEntry;
}

////////////////////////////////////////////////////////////////////////////////

const CVehicleCoverBoundOffsetInfo* CVehicleMetadataMgr::GetVehicleCoverBoundOffsetInfo(u32 uNameHash)
{
	const CVehicleCoverBoundOffsetInfo* pEntry = ms_Instance.GetVehicleCoverBoundOffsetInfoByHash(uNameHash);

	if(pEntry)
		return pEntry;

	if(ms_InstanceExtra)
		pEntry = ms_InstanceExtra->GetVehicleCoverBoundOffsetInfoByHash(uNameHash);

	return pEntry;
}

////////////////////////////////////////////////////////////////////////////////

const CVehicleLayoutInfo* CVehicleMetadataMgr::GetVehicleLayoutInfo(u32 uNameHash)
{
	const CVehicleLayoutInfo* pEntry = ms_Instance.GetVehicleLayoutInfoByHash(uNameHash);

	if(pEntry)
		return pEntry;

	if(ms_InstanceExtra)
		pEntry = ms_InstanceExtra->GetVehicleLayoutInfoByHash(uNameHash);

	return pEntry;
}

////////////////////////////////////////////////////////////////////////////////

const CVehicleSeatInfo* CVehicleMetadataMgr::GetVehicleSeatInfo(u32 uNameHash)
{
	const CVehicleSeatInfo* pEntry = ms_Instance.GetVehicleSeatInfoByHash(uNameHash);

	if(pEntry)
		return pEntry;

	if(ms_InstanceExtra)
		pEntry = ms_InstanceExtra->GetVehicleSeatInfoByHash(uNameHash);

	return pEntry;
}

////////////////////////////////////////////////////////////////////////////////

const CVehicleSeatAnimInfo* CVehicleMetadataMgr::GetVehicleSeatAnimInfo(u32 uNameHash)
{
	const CVehicleSeatAnimInfo* pEntry = ms_Instance.GetVehicleSeatAnimInfoByHash(uNameHash);

	if(pEntry)
		return pEntry;

	if(ms_InstanceExtra)
		pEntry = ms_InstanceExtra->GetVehicleSeatAnimInfoByHash(uNameHash);

	return pEntry;
}

////////////////////////////////////////////////////////////////////////////////

const CSeatOverrideAnimInfo* CVehicleMetadataMgr::GetSeatOverrideAnimInfo(u32 uNameHash)
{
	const CSeatOverrideAnimInfo* pEntry = ms_Instance.GetSeatOverrideAnimInfoByHash(uNameHash);

	if(pEntry)
		return pEntry;

	if(ms_InstanceExtra)
		pEntry = ms_InstanceExtra->GetSeatOverrideAnimInfoByHash(uNameHash);

	return pEntry;
}

////////////////////////////////////////////////////////////////////////////////

const CInVehicleOverrideInfo* CVehicleMetadataMgr::GetInVehicleOverrideInfo(u32 uNameHash)
{
	const CInVehicleOverrideInfo* pEntry = ms_Instance.GetInVehicleOverrideInfoByHash(uNameHash);

	if(pEntry)
		return pEntry;

	if(ms_InstanceExtra)
		pEntry = ms_InstanceExtra->GetInVehicleOverrideInfoByHash(uNameHash);

	return pEntry;
}





////////////////////////////////////////////////////////////////////////////////

const CAnimRateSet* CVehicleMetadataMgr::GetAnimRateSetByHash(u32 uNameHash)
{
	if (uNameHash == 0)
	{
		return m_AnimRateSets[0];
	}

	for (s32 i = 0; i < m_AnimRateSets.GetCount(); i++)
	{
		if (uNameHash == m_AnimRateSets[i]->GetName().GetHash())
		{
			return m_AnimRateSets[i];
		}
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

const CClipSetMap* CVehicleMetadataMgr::GetClipSetMapByHash(u32 uNameHash)
{
	for (s32 i = 0; i < m_ClipSetMaps.GetCount(); i++)
	{
		if (uNameHash == m_ClipSetMaps[i]->GetName().GetHash())
		{
			return m_ClipSetMaps[i];
		}
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

const CBicycleInfo* CVehicleMetadataMgr::GetBicycleInfoByHash(u32 uNameHash)
{
	for (s32 i = 0; i < m_BicycleInfos.GetCount(); i++)
	{
		if (uNameHash == m_BicycleInfos[i]->GetName().GetHash())
		{
			return m_BicycleInfos[i];
		}
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

const CPOVTuningInfo* CVehicleMetadataMgr::GetPOVTuningInfoByHash(u32 uNameHash)
{
	for (s32 i = 0; i < m_POVTuningInfos.GetCount(); i++)
	{
		if (uNameHash == m_POVTuningInfos[i]->GetName().GetHash())
		{
			return m_POVTuningInfos[i];
		}
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

const CVehicleCoverBoundOffsetInfo* CVehicleMetadataMgr::GetVehicleCoverBoundOffsetInfoByHash(u32 uNameHash)
{
	for (s32 i = 0; i < m_VehicleCoverBoundOffsetInfos.GetCount(); i++)
	{
		if (uNameHash == m_VehicleCoverBoundOffsetInfos[i]->GetName().GetHash())
		{
			return m_VehicleCoverBoundOffsetInfos[i];
		}
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

const CVehicleLayoutInfo* CVehicleMetadataMgr::GetVehicleLayoutInfoByHash(u32 uNameHash)
{
	for (s32 i = 0; i < m_VehicleLayoutInfos.GetCount(); i++)
	{
		if (uNameHash == m_VehicleLayoutInfos[i]->GetName().GetHash())
		{
			return m_VehicleLayoutInfos[i];
		}
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

const CVehicleSeatInfo* CVehicleMetadataMgr::GetVehicleSeatInfoByHash(u32 uNameHash)
{
	for (s32 i = 0; i < m_VehicleSeatInfos.GetCount(); i++)
	{
		if (uNameHash == m_VehicleSeatInfos[i]->GetName().GetHash())
		{
			return m_VehicleSeatInfos[i];
		}
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

const CVehicleSeatAnimInfo* CVehicleMetadataMgr::GetVehicleSeatAnimInfoByHash(u32 uNameHash)
{
	for (s32 i = 0; i < m_VehicleSeatAnimInfos.GetCount(); i++)
	{
		const CVehicleSeatAnimInfo* pSeatAnimInfo = m_VehicleSeatAnimInfos[i];
		if (pSeatAnimInfo && uNameHash == pSeatAnimInfo->GetName().GetHash())
		{
			return pSeatAnimInfo;
		}
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

const CSeatOverrideAnimInfo* CVehicleMetadataMgr::GetSeatOverrideAnimInfoByHash(u32 uNameHash)
{
	for (s32 i = 0; i < m_SeatOverrideAnimInfos.GetCount(); i++)
	{
		if (uNameHash == m_SeatOverrideAnimInfos[i]->GetName().GetHash())
		{
			return m_SeatOverrideAnimInfos[i];
		}
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

const CInVehicleOverrideInfo* CVehicleMetadataMgr::GetInVehicleOverrideInfoByHash(u32 uNameHash)
{
	for (s32 i = 0; i < m_InVehicleOverrideInfos.GetCount(); i++)
	{
		if (uNameHash == m_InVehicleOverrideInfos[i]->GetName().GetHash())
		{
			return m_InVehicleOverrideInfos[i];
		}
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

const CVehicleSeatAnimInfo*	CVehicleMetadataMgr::GetSeatAnimInfoFromPed(const CPed *pPed)
{
	CPed* myMount = pPed->GetMyMount();
	CVehicle* pVehicle = pPed->GetVehiclePedInside();
	const CVehicleSeatAnimInfo* pSeatAnimInfo = NULL;
	if (myMount) 
	{		
		pSeatAnimInfo = myMount->GetPedModelInfo()->GetLayoutInfo()->GetSeatAnimationInfo(myMount->GetSeatManager()->GetPedsSeatIndex(pPed));
	} 
	else if( pVehicle )
	{
		s32 seatPedOccupies = pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
		if (seatPedOccupies == -1)
		{
			return NULL;
		}

		// I don't know why we're only allowing combination driveby / vehicle weapons for the driver seat...
		if (seatPedOccupies != 0 && pVehicle->GetSeatHasWeaponsOrTurrets(seatPedOccupies))
		{
			bool bIsMulePassenger = MI_CAR_MULE4.IsValid() && pVehicle->GetModelIndex() == MI_CAR_MULE4 && seatPedOccupies == 1;
			bool bIsPounderPassenger = MI_CAR_POUNDER2.IsValid() && pVehicle->GetModelIndex() == MI_CAR_POUNDER2 && seatPedOccupies == 1;
			if (!bIsMulePassenger && !bIsPounderPassenger)
			{
				return NULL;
			}
		}
		// Look up vehicle seat info
		pSeatAnimInfo = pVehicle->GetSeatAnimationInfo(seatPedOccupies);
	}
	return pSeatAnimInfo;
}

////////////////////////////////////////////////////////////////////////////////

const CVehicleSeatAnimInfo* CVehicleMetadataMgr::GetSeatAnimInfoFromSeatIndex(const CEntity* pEntity, s32 iSeatIndex)
{
	const CVehicleSeatAnimInfo* pSeatAnimInfo = NULL;
	if (iSeatIndex >= 0)
	{
		if (pEntity->GetIsTypePed()) 
		{		
			pSeatAnimInfo = static_cast<const CPed*>(pEntity)->GetPedModelInfo()->GetLayoutInfo()->GetSeatAnimationInfo(iSeatIndex);
		} 
		else 
		{
			pSeatAnimInfo = static_cast<const CVehicle*>(pEntity)->GetSeatAnimationInfo(iSeatIndex);
		}
	}
	return pSeatAnimInfo;
}

////////////////////////////////////////////////////////////////////////////////

const CDrivebyWeaponGroup* CVehicleMetadataMgr::GetDrivebyWeaponGroup(u32 uNameHash)
{
	const CDrivebyWeaponGroup* pEntry = ms_Instance.GetDrivebyWeaponGroupByHash(uNameHash);

	if(pEntry)
		return pEntry;

	if(ms_InstanceExtra)
		pEntry = ms_InstanceExtra->GetDrivebyWeaponGroupByHash(uNameHash);

	return pEntry;
}

////////////////////////////////////////////////////////////////////////////////

const CVehicleDriveByInfo* CVehicleMetadataMgr::GetVehicleDriveByInfo(u32 uNameHash)
{
	const CVehicleDriveByInfo* pEntry = ms_Instance.GetVehicleDriveByInfoByHash(uNameHash);

	if(pEntry)
		return pEntry;

	if(ms_InstanceExtra)
		pEntry = ms_InstanceExtra->GetVehicleDriveByInfoByHash(uNameHash);

	return pEntry;
}

////////////////////////////////////////////////////////////////////////////////

const CVehicleDriveByAnimInfo* CVehicleMetadataMgr::GetVehicleDriveByAnimInfo(u32 uNameHash)
{
	const CVehicleDriveByAnimInfo* pEntry = ms_Instance.GetVehicleDriveByAnimInfoByHash(uNameHash);

	if(pEntry)
		return pEntry;

	if(ms_InstanceExtra)
		pEntry = ms_InstanceExtra->GetVehicleDriveByAnimInfoByHash(uNameHash);

	return pEntry;
}

////////////////////////////////////////////////////////////////////////////////

const CVehicleEntryPointInfo* CVehicleMetadataMgr::GetVehicleEntryPointInfo(u32 uNameHash)
{
	const CVehicleEntryPointInfo* pEntry = ms_Instance.GetVehicleEntryPointInfoByHash(uNameHash);

	if(pEntry)
		return pEntry;

	if(ms_InstanceExtra)
		pEntry = ms_InstanceExtra->GetVehicleEntryPointInfoByHash(uNameHash);

	return pEntry;
}

////////////////////////////////////////////////////////////////////////////////

const CVehicleEntryPointAnimInfo* CVehicleMetadataMgr::GetVehicleEntryPointAnimInfo(u32 uNameHash)
{
	const CVehicleEntryPointAnimInfo* pEntry = ms_Instance.GetVehicleEntryPointAnimInfoByHash(uNameHash);

	if(pEntry)
		return pEntry;

	if(ms_InstanceExtra)
		pEntry = ms_InstanceExtra->GetVehicleEntryPointAnimInfoByHash(uNameHash);

	return pEntry;
}

////////////////////////////////////////////////////////////////////////////////

const CVehicleExplosionInfo* CVehicleMetadataMgr::GetVehicleExplosionInfo(u32 uNameHash)
{
	const CVehicleExplosionInfo* pEntry = ms_Instance.GetVehicleExplosionInfoByHash(uNameHash);

	if(pEntry)
		return pEntry;

	if(ms_InstanceExtra)
		pEntry = ms_InstanceExtra->GetVehicleExplosionInfoByHash(uNameHash);

	return pEntry;
}

////////////////////////////////////////////////////////////////////////////////

const CVehicleScenarioLayoutInfo* CVehicleMetadataMgr::GetVehicleScenarioLayoutInfo(u32 uNameHash)
{
	const CVehicleScenarioLayoutInfo* pEntry = ms_Instance.GetVehicleScenarioLayoutInfoByHash(uNameHash);

	if(pEntry)
		return pEntry;

	if(ms_InstanceExtra)
		pEntry = ms_InstanceExtra->GetVehicleScenarioLayoutInfoByHash(uNameHash);

	return pEntry;
}

////////////////////////////////////////////////////////////////////////////////

const CVehicleExtraPointsInfo* CVehicleMetadataMgr::GetVehicleExtraPointsInfo(u32 uNameHash)
{
	const CVehicleExtraPointsInfo* pEntry = ms_Instance.GetVehicleExtraPointsInfoByHash(uNameHash);

	if(pEntry)
		return pEntry;

	if(ms_InstanceExtra)
		pEntry = ms_InstanceExtra->GetVehicleExtraPointsInfoByHash(uNameHash);

	return pEntry;
}

////////////////////////////////////////////////////////////////////////////////

const CEntryAnimVariations* CVehicleMetadataMgr::GetEntryAnimVariations(u32 uNameHash)
{
	const CEntryAnimVariations* pEntry = ms_Instance.GetEntryAnimVariationsByHash(uNameHash);

	if(pEntry)
		return pEntry;

	if(ms_InstanceExtra)
		pEntry = ms_InstanceExtra->GetEntryAnimVariationsByHash(uNameHash);

	return pEntry;
}

////////////////////////////////////////////////////////////////////////////////

s32 CVehicleMetadataMgr::GetVehicleLayoutInfoCount()
{
	return ms_Instance.m_VehicleLayoutInfos.GetCount();
}

////////////////////////////////////////////////////////////////////////////////

CVehicleLayoutInfo* CVehicleMetadataMgr::GetVehicleLayoutInfoAtIndex(s32 iIndex)
{
	return ms_Instance.m_VehicleLayoutInfos[iIndex];
}

////////////////////////////////////////////////////////////////////////////////


const CDrivebyWeaponGroup* CVehicleMetadataMgr::GetDrivebyWeaponGroupByHash(u32 uNameHash)
{
	for (s32 i = 0; i < m_DrivebyWeaponGroups.GetCount(); i++)
	{
		if (uNameHash == m_DrivebyWeaponGroups[i]->GetName().GetHash())
		{
			return m_DrivebyWeaponGroups[i];
		}
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

const CVehicleDriveByInfo* CVehicleMetadataMgr::GetVehicleDriveByInfoByHash(u32 uNameHash)
{
	for (s32 i = 0; i < m_VehicleDriveByInfos.GetCount(); i++)
	{
		if (uNameHash == m_VehicleDriveByInfos[i]->GetName().GetHash())
		{
			return m_VehicleDriveByInfos[i];
		}
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

const CVehicleDriveByAnimInfo* CVehicleMetadataMgr::GetVehicleDriveByAnimInfoByHash(u32 uNameHash)
{
	for (s32 i = 0; i < m_VehicleDriveByAnimInfos.GetCount(); i++)
	{
		if (uNameHash == m_VehicleDriveByAnimInfos[i]->GetName().GetHash())
		{
			return m_VehicleDriveByAnimInfos[i];
		}
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

const CVehicleEntryPointInfo* CVehicleMetadataMgr::GetVehicleEntryPointInfoByHash(u32 uNameHash)
{
	for (s32 i = 0; i < m_VehicleEntryPointInfos.GetCount(); i++)
	{
		if (uNameHash == m_VehicleEntryPointInfos[i]->GetName().GetHash())
		{
			return m_VehicleEntryPointInfos[i];
		}
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

const CVehicleEntryPointAnimInfo* CVehicleMetadataMgr::GetVehicleEntryPointAnimInfoByHash(u32 uNameHash)
{
	for (s32 i = 0; i < m_VehicleEntryPointAnimInfos.GetCount(); i++)
	{
		if (uNameHash == m_VehicleEntryPointAnimInfos[i]->GetName().GetHash())
		{
			return m_VehicleEntryPointAnimInfos[i];
		}
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

const CVehicleExplosionInfo* CVehicleMetadataMgr::GetVehicleExplosionInfoByHash(u32 uNameHash)
{
	for (s32 i = 0; i < m_VehicleExplosionInfos.GetCount(); i++)
	{
		if (uNameHash == m_VehicleExplosionInfos[i]->GetName().GetHash())
		{
			return m_VehicleExplosionInfos[i];
		}
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

const CVehicleScenarioLayoutInfo* CVehicleMetadataMgr::GetVehicleScenarioLayoutInfoByHash(u32 uNameHash)
{
	for (s32 i = 0; i < m_VehicleScenarioLayoutInfos.GetCount(); i++)
	{
		if (uNameHash == m_VehicleScenarioLayoutInfos[i]->GetName().GetHash())
		{
			return m_VehicleScenarioLayoutInfos[i];
		}
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

const CVehicleExtraPointsInfo* CVehicleMetadataMgr::GetVehicleExtraPointsInfoByHash(u32 uNameHash)
{
	for (s32 i = 0; i < m_VehicleExtraPointsInfos.GetCount(); i++)
	{
		if (uNameHash == m_VehicleExtraPointsInfos[i]->GetName().GetHash())
		{
			return m_VehicleExtraPointsInfos[i];
		}
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

const CEntryAnimVariations* CVehicleMetadataMgr::GetEntryAnimVariationsByHash(u32 uNameHash)
{
	for (s32 i = 0; i < m_EntryAnimVariations.GetCount(); i++)
	{
		if (uNameHash == m_EntryAnimVariations[i]->GetHash())
		{
			return m_EntryAnimVariations[i];
		}
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

const CVehicleDriveByInfo* CVehicleMetadataMgr::GetVehicleDriveByInfoFromPed(const CPed* pPed)
{	
	vehicleFatalAssertf(pPed, "Ped pointer was NULL");
	CVehicle* myVehicle = pPed->GetMyVehicle();
	CPed* myMount = pPed->GetMyMount();
	if (!myVehicle && !myMount && !pPed->GetIsParachuting() && !pPed->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack))
	{
		vehicleAssertf(false, "Ped wasn't in a vehicle");
		return NULL;
	}

	if (pPed->GetIsParachuting())
	{
		static atHashWithStringNotFinal s_uName("DRIVEBY_PARACHUTE", 0x9e643cb0);
		return GetVehicleDriveByInfo(s_uName.GetHash());
	}
	else if(pPed->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack))
	{
		static atHashWithStringNotFinal s_uName("DRIVEBY_JETPACK", 0x0b89a084);
		return GetVehicleDriveByInfo(s_uName.GetHash());
	}
	else
	{
		const CVehicleSeatAnimInfo* pSeatAnimInfo = NULL;
		if (myMount) 
		{		
			pSeatAnimInfo = myMount->GetPedModelInfo()->GetLayoutInfo()->GetSeatAnimationInfo(myMount->GetSeatManager()->GetPedsSeatIndex(pPed));
		} 
		else if (myVehicle)
		{
			pSeatAnimInfo = myVehicle->GetSeatAnimationInfo(pPed);
		}

		if (pSeatAnimInfo)
		{
			return pSeatAnimInfo->GetDriveByInfo();
		}
		else
		{
			return NULL;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

const CVehicleDriveByAnimInfo* CVehicleMetadataMgr::GetDriveByAnimInfoForWeaponGroup(const CPed* pPed, u32 uWeaponGroup)
{
	if (uWeaponGroup == WEAPONGROUP_UNARMED && !pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanTauntInVehicle))
	{
		return NULL;
	}

	CPed* myMount = pPed->GetMyMount();
	const CVehicleSeatAnimInfo* pSeatAnimInfo = NULL;

	vehicleAssertf(pPed, "Ped pointer was NULL");
	vehicleAssertf(pPed->GetMyVehicle() || myMount, "Ped wasn't in a vehicle");
	vehicleAssertf(uWeaponGroup != 0, "Weapon group hash was invalid");

	if (myMount) 
	{		
		pSeatAnimInfo = myMount->GetPedModelInfo()->GetLayoutInfo()->GetSeatAnimationInfo(myMount->GetSeatManager()->GetPedsSeatIndex(pPed));
	} 
	else 
	{
		pSeatAnimInfo = pPed->GetMyVehicle()->GetSeatAnimationInfo(pPed);
	}
	if (pSeatAnimInfo)
	{
		return GetDriveByAnimInfoForWeaponGroup(pSeatAnimInfo, uWeaponGroup);
	}

	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

const CVehicleDriveByAnimInfo* CVehicleMetadataMgr::GetDriveByAnimInfoForWeaponGroup(const CVehicleSeatAnimInfo* pSeatAnimInfo, u32 uWeaponGroup)
{
	vehicleAssertf(pSeatAnimInfo, "NULL seat anim info");
	const CVehicleDriveByInfo* pDriveByInfo = pSeatAnimInfo->GetDriveByInfo();
	if (pDriveByInfo)
	{
		// Go through each driveby anim info and check if the equipped weapon slot can be used with it
		const atArray<CVehicleDriveByAnimInfo*>& driveByAnimInfos = pDriveByInfo->GetDriveByAnimInfos();
		for (s32 i=0; i<driveByAnimInfos.GetCount(); i++)
		{
			const CVehicleDriveByAnimInfo* pAnimInfo = driveByAnimInfos[i];
			if (vehicleVerifyf(pAnimInfo, "NULL vehicle driveby anim info"))
			{
				if (pAnimInfo->ContainsWeaponGroup(uWeaponGroup))
				{
					return pAnimInfo;
				}
			}
		}
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

const CVehicleDriveByAnimInfo* CVehicleMetadataMgr::GetDriveByAnimInfoForWeapon(const CPed* pPed, u32 uWeapon)
{
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(uWeapon);
	if ( (!pWeaponInfo || (pWeaponInfo->GetIsUnarmed() && !pWeaponInfo->GetIsVehicleWeapon())) && 
		(!pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanTauntInVehicle) ||
		pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableUnarmedDrivebys)))
	{
		return NULL;
	}

	CPed* myMount = pPed->GetMyMount();
	const CVehicleSeatAnimInfo* pSeatAnimInfo = NULL;
	vehicleAssertf(pPed, "Ped pointer was NULL");
	vehicleAssertf(pPed->GetMyVehicle() || myMount || pPed->GetIsParachuting() || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack), "Ped wasn't in a vehicle");
	vehicleAssertf(uWeapon != 0, "Weapon hash is invalid");

	if (pPed->GetIsParachuting() || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack))
	{
		const CVehicleDriveByInfo* pDriveByInfo = GetVehicleDriveByInfoFromPed(pPed);
		vehicleAssert(pDriveByInfo);
		return GetDriveByAnimInfoForWeapon(pDriveByInfo, uWeapon);
	}
	else if (myMount) 
	{		
		pSeatAnimInfo = myMount->GetPedModelInfo()->GetLayoutInfo()->GetSeatAnimationInfo(myMount->GetSeatManager()->GetPedsSeatIndex(pPed));
	} 
	else 
	{
		pSeatAnimInfo = pPed->GetMyVehicle()->GetSeatAnimationInfo(pPed);
	}	
	if (pSeatAnimInfo)
	{
		return GetDriveByAnimInfoForWeapon(pSeatAnimInfo, uWeapon);
	}

	return NULL;
}


////////////////////////////////////////////////////////////////////////////////

const CVehicleDriveByAnimInfo* CVehicleMetadataMgr::GetDriveByAnimInfoForWeapon(const CVehicleSeatAnimInfo* pSeatAnimInfo, u32 uWeapon)
{
	vehicleAssertf(pSeatAnimInfo, "NULL seat anim info");
	const CVehicleDriveByInfo* pDriveByInfo = pSeatAnimInfo->GetDriveByInfo();
	if (pDriveByInfo)
	{
		return GetDriveByAnimInfoForWeapon(pDriveByInfo, uWeapon);
	}
	
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

const CVehicleDriveByAnimInfo* CVehicleMetadataMgr::GetDriveByAnimInfoForWeapon(const CVehicleDriveByInfo* pDriveByInfo, u32 uWeapon)
{
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(uWeapon);
	if (vehicleVerifyf(pWeaponInfo, "NULL weapon with hash %d", uWeapon))
	{
		vehicleAssertf(pDriveByInfo, "NULL driveby info");
		if (pDriveByInfo)
		{
			// Go through each driveby anim info and check if the equipped weapon slot can be used with it
			const atArray<CVehicleDriveByAnimInfo*>& driveByAnimInfos = pDriveByInfo->GetDriveByAnimInfos();
			for (s32 i=0; i<driveByAnimInfos.GetCount(); i++)
			{
				const CVehicleDriveByAnimInfo* pAnimInfo = driveByAnimInfos[i];
				if (vehicleVerifyf(pAnimInfo, "NULL vehicle driveby anim info"))
				{
					if (pAnimInfo->ContainsWeaponGroup(pWeaponInfo->GetGroup()) || pAnimInfo->ContainsWeaponType(pWeaponInfo->GetHash()))
					{
						return pAnimInfo;
					}
				}
			}
		}
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

bool CVehicleMetadataMgr::CanPedDoDriveByWithEquipedWeapon(const CPed* pPed) 
{
	vehicleAssert(pPed);
	vehicleAssert(pPed->GetWeaponManager());
	u32 uHash = pPed->GetWeaponManager()->GetEquippedWeaponHash();
	if( uHash == 0 )
	{
		const CVehicleWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedVehicleWeapon();
		if( pWeapon )
		{
			uHash = pWeapon->GetHash();
			return uHash !=0;
		}
	}

	return (uHash && GetDriveByAnimInfoForWeapon(pPed, uHash));
}

////////////////////////////////////////////////////////////////////////////////

const CFirstPersonDriveByLookAroundData* CVehicleMetadataMgr::GetFirstPersonDriveByLookAroundData(u32 uNameHash)
{
	const CFirstPersonDriveByLookAroundData* pData = ms_Instance.GetFirstPersonDriveByLookAroundDataByHash(uNameHash);

	if(pData)
		return pData;

	if(ms_InstanceExtra)
		pData = ms_InstanceExtra->GetFirstPersonDriveByLookAroundDataByHash(uNameHash);

	return pData;
}

////////////////////////////////////////////////////////////////////////////////

const CFirstPersonDriveByLookAroundData* CVehicleMetadataMgr::GetFirstPersonDriveByLookAroundDataByHash(u32 uNameHash)
{
	for (s32 i = 0; i < m_FirstPersonDriveByLookAroundData.GetCount(); i++)
	{
		if (uNameHash == m_FirstPersonDriveByLookAroundData[i]->GetName().GetHash())
		{
			return m_FirstPersonDriveByLookAroundData[i];
		}
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

u32 CVehicleMetadataMgr::GetTotalNumFirstPersonDriveByLookAroundData()
{
	u32 nNum = ms_Instance.GetNumFirstPersonDriveByLookAroundData();

	if(ms_InstanceExtra)
		nNum += ms_InstanceExtra->GetNumFirstPersonDriveByLookAroundData();

	return nNum;
}

////////////////////////////////////////////////////////////////////////////////

u32 CVehicleMetadataMgr::GetNumFirstPersonDriveByLookAroundData()
{
	return m_FirstPersonDriveByLookAroundData.GetCount();
}

////////////////////////////////////////////////////////////////////////////////

const CFirstPersonDriveByLookAroundData* CVehicleMetadataMgr::GetFirstPersonDriveByLookAroundDataAtIndex(u32 iIndex)
{
	const CFirstPersonDriveByLookAroundData* pData = NULL;

	if(iIndex < ms_Instance.GetNumFirstPersonDriveByLookAroundData())
	{
		pData = ms_Instance.GetFirstPersonDriveByLookAroundDataAtIndexInternal(iIndex);
	}
	else
	{
		iIndex -= ms_Instance.GetNumFirstPersonDriveByLookAroundData();
		if(ms_InstanceExtra)
			pData = ms_InstanceExtra->GetFirstPersonDriveByLookAroundDataAtIndexInternal(iIndex);
	}

	return pData;
}

////////////////////////////////////////////////////////////////////////////////

const CFirstPersonDriveByLookAroundData* CVehicleMetadataMgr::GetFirstPersonDriveByLookAroundDataAtIndexInternal(u32 iIndex)
{
	if(iIndex < m_FirstPersonDriveByLookAroundData.GetCount())
	{
		return m_FirstPersonDriveByLookAroundData[iIndex];
	}

	return NULL;
}



////////////////////////////////////////////////////////////////////////////////

void CVehicleMetadataMgr::ValidateData()
{
#if __DEV
	for (s32 i=0; i<m_VehicleSeatAnimInfos.GetCount(); i++)
	{
		if (m_VehicleSeatAnimInfos[i])
		{
			vehicleFatalAssertf(CLIP_SET_ID_INVALID != m_VehicleSeatAnimInfos[i]->GetSeatClipSetId(), "Couldn't find valid seat clipset for (%s)", m_VehicleSeatAnimInfos[i]->GetName().GetCStr());
			
			fwMvClipSetId panicClipSetId = m_VehicleSeatAnimInfos[i]->GetPanicClipSet();
			if (panicClipSetId != CLIP_SET_ID_INVALID)
			{
				fwClipSet* pClipSet = fwClipSetManager::GetClipSet(panicClipSetId);
				if (pClipSet)
				{
					// Also check seat clipset fallback is the same as the panic clipset fallback.
					fwClipSet *pFallBackClipSet = fwClipSetManager::GetClipSet(m_VehicleSeatAnimInfos[i]->GetSeatClipSetId());
					fwMvClipSetId fallBackClipSetId = CLIP_SET_ID_INVALID;
					if (pFallBackClipSet)
					{
						fallBackClipSetId = pFallBackClipSet->GetFallbackId();
					}
					
					vehicleFatalAssertf(((pClipSet->GetFallbackId() == m_VehicleSeatAnimInfos[i]->GetSeatClipSetId()) || (fallBackClipSetId != CLIP_SET_ID_INVALID && pClipSet->GetFallbackId() == fallBackClipSetId)), "PanicClipSet (%s) should fallback to SeatClipSet (%s)", panicClipSetId.GetCStr(), m_VehicleSeatAnimInfos[i]->GetSeatClipSetId().GetCStr());
				}
			}
		}
	}

	for (s32 i=0; i<m_VehicleDriveByAnimInfos.GetCount(); i++)
	{
		if (m_VehicleDriveByAnimInfos[i])
		{
			vehicleFatalAssertf(m_VehicleDriveByAnimInfos[i]->GetWeaponGroup()!=NULL, "Driveby weapon group is not defined for DriveByAnimGroup (%s) in the metadata", m_VehicleDriveByAnimInfos[i]->GetName().GetCStr());
		}
	}

	for (s32 i=0; i<m_VehicleEntryPointAnimInfos.GetCount(); i++)
	{
		if (m_VehicleEntryPointAnimInfos[i])
		{
			fwMvClipSetId commonOnVehicleClipSetId = m_VehicleEntryPointAnimInfos[i]->GetCommonOnVehicleClipSet();
			if (commonOnVehicleClipSetId != CLIP_SET_ID_INVALID)
			{
				fwClipSet* pClipSet = fwClipSetManager::GetClipSet(commonOnVehicleClipSetId);
				if (pClipSet)
				{
					fwMvClipSetId commonClipSetId = m_VehicleEntryPointAnimInfos[i]->GetCommonClipSetId();
					vehicleFatalAssertf(pClipSet->GetFallbackId() == commonClipSetId, "CommonOnVehicleClipSet (%s) should fallback to CommonClipSetId (%s)", commonOnVehicleClipSetId.GetCStr(), commonClipSetId.GetCStr());
				}
			}

			fwMvClipSetId altClipSetId = m_VehicleEntryPointAnimInfos[i]->GetAlternateEntryPointClipSet();
			if (altClipSetId != CLIP_SET_ID_INVALID)
			{
				fwClipSet* pClipSet = fwClipSetManager::GetClipSet(altClipSetId);
				if (pClipSet)
				{
					vehicleFatalAssertf(pClipSet->GetFallbackId() == m_VehicleEntryPointAnimInfos[i]->GetEntryPointClipSetId(), "AlternateEntryPointClipSet should fallback to EntryPointClipSet");
				}
			}
		}
	}

	for(s32 i=0; i<m_VehicleExplosionInfos.GetCount(); i++)
	{
		const CVehicleExplosionInfo* pVehicleExplosionData = m_VehicleExplosionInfos[i];
		if(pVehicleExplosionData)
		{
			Assertf(pVehicleExplosionData->GetAdditionalPartVelocityMinAngle() <= pVehicleExplosionData->GetAdditionalPartVelocityMaxAngle(), "Additional part velocity angle min is greater than the max for VehicleExplosionInfo '%s' in vehiclelayouts.meta",pVehicleExplosionData->GetName().GetCStr());
			Assertf(pVehicleExplosionData->GetAdditionalPartVelocityMinMagnitude() <= pVehicleExplosionData->GetAdditionalPartVelocityMaxMagnitude(), "Additional part velocity magnitude min is greater than the max for VehicleExplosionInfo '%s' in vehiclelayouts.meta",pVehicleExplosionData->GetName().GetCStr());
			int numVehicleExplosionLODs = pVehicleExplosionData->GetNumVehicleExplosionLODs();
			if(Verifyf(numVehicleExplosionLODs > 0, "No explosion LODs given for VehicleExplosionInfo '%s' in vehiclelayouts.meta",pVehicleExplosionData->GetName().GetCStr()))
			{
				Assertf(pVehicleExplosionData->GetVehicleExplosionLOD(0).GetRadius() == 0.0f, "Explosion LOD 0 has a non-zero radius on VehicleExplosionInfo '%s' in vehiclelayouts.meta.",pVehicleExplosionData->GetName().GetCStr());
				for(int vehicleExplosionLODIndex = 0; vehicleExplosionLODIndex < numVehicleExplosionLODs - 1; ++vehicleExplosionLODIndex)
				{
					Assertf(pVehicleExplosionData->GetVehicleExplosionLOD(vehicleExplosionLODIndex).GetRadius() < pVehicleExplosionData->GetVehicleExplosionLOD(vehicleExplosionLODIndex+1).GetRadius(), "Explosion LOD %i has equal or larger radius than the next LOD on VehicleExplosionInfo '%s' in vehiclelayouts.meta.",vehicleExplosionLODIndex,pVehicleExplosionData->GetName().GetCStr());
				}
			}
		}
	}
#endif // __DEV
}

////////////////////////////////////////////////////////////////////////////////

CVehicleMetadataMgr::CVehicleMetadataMgr()
{
	m_bIsInitialised = false;
}

////////////////////////////////////////////////////////////////////////////////

void CVehicleMetadataMgr::Init(unsigned UNUSED_PARAM(initMode))
{
	// Load
	Load();
	ms_Instance.m_bIsInitialised = true;

	CDataFileMount::RegisterMountInterface(CDataFileMgr::VEHICLE_LAYOUTS_FILE, &g_vehicleMetadataFileMounter, eDFMI_UnloadLast);
}

////////////////////////////////////////////////////////////////////////////////

void CVehicleMetadataMgr::Shutdown(unsigned UNUSED_PARAM(shutdownMode))
{
	Reset();

	ms_Instance.m_bIsInitialised = false;
}

////////////////////////////////////////////////////////////////////////////////

void CVehicleMetadataMgr::Reset()
{
	// Delete each entry from each array
	for (s32 i=0; i<ms_Instance.m_EntryAnimVariations.GetCount(); i++)
	{
		if (ms_Instance.m_EntryAnimVariations[i])
			delete ms_Instance.m_EntryAnimVariations[i];
	}
	ms_Instance.m_EntryAnimVariations.Reset();

	for (s32 i=0; i<ms_Instance.m_VehicleExtraPointsInfos.GetCount(); i++)
	{
		if (ms_Instance.m_VehicleExtraPointsInfos[i])
			delete ms_Instance.m_VehicleExtraPointsInfos[i];
	}
	ms_Instance.m_VehicleExtraPointsInfos.Reset();

	for (s32 i=0; i<ms_Instance.m_VehicleLayoutInfos.GetCount(); i++)
	{
		if (ms_Instance.m_VehicleLayoutInfos[i])
			delete ms_Instance.m_VehicleLayoutInfos[i];
	}
	ms_Instance.m_VehicleLayoutInfos.Reset();

	for (s32 i=0; i<ms_Instance.m_VehicleSeatInfos.GetCount(); i++)
	{
		if (ms_Instance.m_VehicleSeatInfos[i])
			delete ms_Instance.m_VehicleSeatInfos[i];
	}
	ms_Instance.m_VehicleSeatInfos.Reset();

	for (s32 i=0; i<ms_Instance.m_VehicleSeatAnimInfos.GetCount(); i++)
	{
		if (ms_Instance.m_VehicleSeatAnimInfos[i])
			delete ms_Instance.m_VehicleSeatAnimInfos[i];
	}
	ms_Instance.m_VehicleSeatAnimInfos.Reset();

	for (s32 i=0; i<ms_Instance.m_VehicleDriveByInfos.GetCount(); i++)
	{
		if (ms_Instance.m_VehicleDriveByInfos[i])
			delete ms_Instance.m_VehicleDriveByInfos[i];
	}
	ms_Instance.m_VehicleDriveByInfos.Reset();

	for (s32 i=0; i<ms_Instance.m_VehicleDriveByAnimInfos.GetCount(); i++)
	{
		if (ms_Instance.m_VehicleDriveByAnimInfos[i])
			delete ms_Instance.m_VehicleDriveByAnimInfos[i];
	}
	ms_Instance.m_VehicleDriveByAnimInfos.Reset();

	for (s32 i=0; i<ms_Instance.m_DrivebyWeaponGroups.GetCount(); i++)
	{
		if (ms_Instance.m_DrivebyWeaponGroups[i])
			delete ms_Instance.m_DrivebyWeaponGroups[i];
	}
	ms_Instance.m_DrivebyWeaponGroups.Reset();

	for (s32 i=0; i<ms_Instance.m_VehicleEntryPointInfos.GetCount(); i++)
	{
		if (ms_Instance.m_VehicleEntryPointInfos[i])
			delete ms_Instance.m_VehicleEntryPointInfos[i];
	}
	ms_Instance.m_VehicleEntryPointInfos.Reset();

	for (s32 i=0; i<ms_Instance.m_VehicleEntryPointAnimInfos.GetCount(); i++)
	{
		if (ms_Instance.m_VehicleEntryPointAnimInfos[i])
			delete ms_Instance.m_VehicleEntryPointAnimInfos[i];
	}
	ms_Instance.m_VehicleEntryPointAnimInfos.Reset();

	for(s32 i=0; i<ms_Instance.m_VehicleExplosionInfos.GetCount(); i++)
	{
		delete ms_Instance.m_VehicleExplosionInfos[i];
	}
	ms_Instance.m_VehicleExplosionInfos.Reset();

	for (s32 i=0; i<ms_Instance.m_VehicleScenarioLayoutInfos.GetCount(); i++)
	{
		if (ms_Instance.m_VehicleScenarioLayoutInfos[i])
			delete ms_Instance.m_VehicleScenarioLayoutInfos[i];
	}
	ms_Instance.m_VehicleScenarioLayoutInfos.Reset();

	for (s32 i=0; i<ms_Instance.m_FirstPersonDriveByLookAroundData.GetCount(); i++)
	{
		if (ms_Instance.m_FirstPersonDriveByLookAroundData[i])
			delete ms_Instance.m_FirstPersonDriveByLookAroundData[i];
	}
	ms_Instance.m_FirstPersonDriveByLookAroundData.Reset();

#if __BANK
	COwnershipInfo<CAnimRateSet*>::Reset();
	COwnershipInfo<CClipSetMap*>::Reset();
	COwnershipInfo<CVehicleCoverBoundOffsetInfo*>::Reset();
	COwnershipInfo<CBicycleInfo*>::Reset();
	COwnershipInfo<CPOVTuningInfo*>::Reset();
	COwnershipInfo<CEntryAnimVariations*>::Reset();
	COwnershipInfo<CVehicleExtraPointsInfo*>::Reset();
	COwnershipInfo<CVehicleLayoutInfo*>::Reset();
	COwnershipInfo<CVehicleSeatInfo*>::Reset();
	COwnershipInfo<CVehicleSeatAnimInfo*>::Reset();
	COwnershipInfo<CDrivebyWeaponGroup*>::Reset();
	COwnershipInfo<CVehicleDriveByAnimInfo*>::Reset();
	COwnershipInfo<CVehicleDriveByInfo*>::Reset();
	COwnershipInfo<CVehicleEntryPointInfo*>::Reset();
	COwnershipInfo<CVehicleEntryPointAnimInfo*>::Reset();
	COwnershipInfo<CVehicleExplosionInfo*>::Reset();
	COwnershipInfo<CVehicleScenarioLayoutInfo*>::Reset();
	COwnershipInfo<CSeatOverrideAnimInfo*>::Reset();
	COwnershipInfo<CInVehicleOverrideInfo*>::Reset();
	COwnershipInfo<CFirstPersonDriveByLookAroundData*>::Reset();
#endif // __BANK
}

////////////////////////////////////////////////////////////////////////////////

template <class T> void CheckArray( T &arrayToCheck )
{
	for(s32 i = 0; i < arrayToCheck.GetCount(); i++)
	{
		if (arrayToCheck[i] == NULL)
		{
#if !RSG_PC
			Quitf("vehicleLayouts data contains a bad ptr (at idx : %d)", i);
#else
			Quitf(0, "vehicleLayouts data contains a bad ptr (at idx : %d)", i);
#endif
		}
	}
}

void CVehicleMetadataMgr::ValidateDataForFinal(void)
{
	// validation of loaded data

	// top level data
	CheckArray(ms_Instance.m_AnimRateSets);
	CheckArray(ms_Instance.m_ClipSetMaps);
	CheckArray(ms_Instance.m_VehicleCoverBoundOffsetInfos);
	CheckArray(ms_Instance.m_BicycleInfos);
	CheckArray(ms_Instance.m_EntryAnimVariations);
	CheckArray(ms_Instance.m_VehicleExtraPointsInfos);
	CheckArray(ms_Instance.m_VehicleLayoutInfos);
	CheckArray(ms_Instance.m_VehicleSeatInfos);
	CheckArray(ms_Instance.m_VehicleSeatAnimInfos);
	CheckArray(ms_Instance.m_DrivebyWeaponGroups);
	CheckArray(ms_Instance.m_VehicleDriveByAnimInfos);
	CheckArray(ms_Instance.m_VehicleDriveByInfos);
	CheckArray(ms_Instance.m_VehicleEntryPointInfos);
	CheckArray(ms_Instance.m_VehicleEntryPointAnimInfos);
	CheckArray(ms_Instance.m_VehicleExplosionInfos);
	CheckArray(ms_Instance.m_VehicleScenarioLayoutInfos);
	CheckArray(ms_Instance.m_SeatOverrideAnimInfos);
	CheckArray(ms_Instance.m_InVehicleOverrideInfos);

	for(s32 i = 0; i < ms_Instance.m_VehicleLayoutInfos.GetCount(); i++)
	{
		if (ms_Instance.m_VehicleLayoutInfos[i])
		{
			if (!ms_Instance.m_VehicleLayoutInfos[i]->ValidateEntryPoints())
			{
#if !RSG_PC
				Quitf("vehicle layouts contains bad entry points (at idx : %d)", i);
#else
				Quitf(0, "vehicle layouts contains bad entry points (at idx : %d)", i);
#endif
			}
		}
	}

	// end of validation check
}

void CVehicleMetadataMgr::Load()
{
	// Delete any existing data
	Reset();

	// Load
	PARSER.LoadObject(VEHICLE_LAYOUTS_BASE_FILE, "meta", ms_Instance);
	
	parRestRegisterSingleton("Vehicles/Layouts", ms_Instance, VEHICLE_LAYOUTS_BASE_FILE_WITH_EXT); // Make this object browsable / editable via rest services (i.e. the web)

	//Fix up seat shuffle links
	for(s32 i = 0; i < ms_Instance.m_VehicleSeatInfos.GetCount(); i++)
	{
		ms_Instance.m_VehicleSeatInfos[i]->LoadLinkPtrs();		
	}

	ValidateDataForFinal();

#if __BANK
	// Init vehicle debug widgets
	CVehicleDebug::InitBank();

	// Add ownership info stuff.
	CreateOwnershipInfos(ms_Instance, VEHICLE_LAYOUTS_BASE_FILE);
	AddVehicleLayoutFile(VEHICLE_LAYOUTS_BASE_FILE);
#endif
}

void CVehicleMetadataMgr::AppendExtra(const char *fname)
{
	// Append

	CVehicleMetadataMgr tempInst;
	ms_InstanceExtra = &tempInst;	

	PARSER.LoadObject(fname, "meta", tempInst);
	
	//Fix up seat shuffle links
	for(s32 i = 0; i < tempInst.m_VehicleSeatInfos.GetCount(); i++)
	{
		tempInst.m_VehicleSeatInfos[i]->LoadLinkPtrs();		
	}

	ms_InstanceExtra = NULL;

	AppendArray(ms_Instance.m_AnimRateSets, tempInst.m_AnimRateSets BANK_ONLY(, fname));
	AppendArray(ms_Instance.m_ClipSetMaps, tempInst.m_ClipSetMaps BANK_ONLY(, fname));
	AppendArray(ms_Instance.m_VehicleCoverBoundOffsetInfos, tempInst.m_VehicleCoverBoundOffsetInfos BANK_ONLY(, fname));
	AppendArray(ms_Instance.m_BicycleInfos, tempInst.m_BicycleInfos BANK_ONLY(, fname));
	AppendArray(ms_Instance.m_POVTuningInfos, tempInst.m_POVTuningInfos BANK_ONLY(, fname));
	AppendArray(ms_Instance.m_EntryAnimVariations, tempInst.m_EntryAnimVariations BANK_ONLY(, fname));
	AppendArray(ms_Instance.m_VehicleExtraPointsInfos, tempInst.m_VehicleExtraPointsInfos BANK_ONLY(, fname));
	AppendArray(ms_Instance.m_VehicleLayoutInfos, tempInst.m_VehicleLayoutInfos BANK_ONLY(, fname));
	AppendArray(ms_Instance.m_VehicleSeatInfos, tempInst.m_VehicleSeatInfos BANK_ONLY(, fname));
	AppendArray(ms_Instance.m_VehicleSeatAnimInfos, tempInst.m_VehicleSeatAnimInfos BANK_ONLY(, fname));
	AppendArray(ms_Instance.m_DrivebyWeaponGroups, tempInst.m_DrivebyWeaponGroups BANK_ONLY(, fname));
	AppendArray(ms_Instance.m_VehicleDriveByAnimInfos, tempInst.m_VehicleDriveByAnimInfos BANK_ONLY(, fname));
	AppendArray(ms_Instance.m_VehicleDriveByInfos, tempInst.m_VehicleDriveByInfos BANK_ONLY(, fname));
	AppendArray(ms_Instance.m_VehicleEntryPointInfos, tempInst.m_VehicleEntryPointInfos BANK_ONLY(, fname));
	AppendArray(ms_Instance.m_VehicleEntryPointAnimInfos, tempInst.m_VehicleEntryPointAnimInfos BANK_ONLY(, fname));
	AppendArray(ms_Instance.m_VehicleExplosionInfos, tempInst.m_VehicleExplosionInfos BANK_ONLY(, fname));
	AppendArray(ms_Instance.m_VehicleScenarioLayoutInfos, tempInst.m_VehicleScenarioLayoutInfos BANK_ONLY(, fname));
	AppendArray(ms_Instance.m_SeatOverrideAnimInfos, tempInst.m_SeatOverrideAnimInfos BANK_ONLY(, fname));
	AppendArray(ms_Instance.m_InVehicleOverrideInfos, tempInst.m_InVehicleOverrideInfos BANK_ONLY(, fname));
	AppendArray(ms_Instance.m_FirstPersonDriveByLookAroundData, tempInst.m_FirstPersonDriveByLookAroundData BANK_ONLY(, fname));
	ValidateDataForFinal();

#if __BANK
	// For ownership info
	AddVehicleLayoutFile(fname);
#endif // __BANK
}

void CVehicleMetadataMgr::RemoveExtra(const char *fname)
{
	// Remove

	CVehicleMetadataMgr tempInst;

	ms_InstanceExtra = &tempInst;

	PARSER.LoadObject(fname, "meta", tempInst);

	ms_InstanceExtra = NULL;

	RemoveArray(ms_Instance.m_AnimRateSets, tempInst.m_AnimRateSets);
	RemoveArray(ms_Instance.m_ClipSetMaps, tempInst.m_ClipSetMaps);
	RemoveArray(ms_Instance.m_VehicleCoverBoundOffsetInfos, tempInst.m_VehicleCoverBoundOffsetInfos);
	RemoveArray(ms_Instance.m_BicycleInfos, tempInst.m_BicycleInfos);
	RemoveArray(ms_Instance.m_POVTuningInfos, tempInst.m_POVTuningInfos);
	RemoveArray(ms_Instance.m_EntryAnimVariations, tempInst.m_EntryAnimVariations);
	RemoveArray(ms_Instance.m_VehicleExtraPointsInfos, tempInst.m_VehicleExtraPointsInfos);
	RemoveArray(ms_Instance.m_VehicleLayoutInfos, tempInst.m_VehicleLayoutInfos);
	RemoveArray(ms_Instance.m_VehicleSeatInfos, tempInst.m_VehicleSeatInfos);
	RemoveArray(ms_Instance.m_VehicleSeatAnimInfos, tempInst.m_VehicleSeatAnimInfos);
	RemoveArray(ms_Instance.m_DrivebyWeaponGroups, tempInst.m_DrivebyWeaponGroups);
	RemoveArray(ms_Instance.m_VehicleDriveByAnimInfos, tempInst.m_VehicleDriveByAnimInfos);
	RemoveArray(ms_Instance.m_VehicleDriveByInfos, tempInst.m_VehicleDriveByInfos);
	RemoveArray(ms_Instance.m_VehicleEntryPointInfos, tempInst.m_VehicleEntryPointInfos);
	RemoveArray(ms_Instance.m_VehicleEntryPointAnimInfos, tempInst.m_VehicleEntryPointAnimInfos);
	RemoveArray(ms_Instance.m_VehicleExplosionInfos, tempInst.m_VehicleExplosionInfos);
	RemoveArray(ms_Instance.m_VehicleScenarioLayoutInfos, tempInst.m_VehicleScenarioLayoutInfos);
	RemoveArray(ms_Instance.m_SeatOverrideAnimInfos, tempInst.m_SeatOverrideAnimInfos);
	RemoveArray(ms_Instance.m_InVehicleOverrideInfos, tempInst.m_InVehicleOverrideInfos);
	RemoveArray(ms_Instance.m_FirstPersonDriveByLookAroundData, tempInst.m_FirstPersonDriveByLookAroundData);
	ValidateDataForFinal();
}

////////////////////////////////////////////////////////////////////////////////

#if __BANK
void CVehicleMetadataMgr::InitWidgets(bkBank& bank)
{
	ms_pCreateButton = bank.AddButton("Create ped vehicle anim metadata widgets", datCallback(CFA1(AddWidgets), &bank));
}

void CVehicleMetadataMgr::AddWidgets(bkBank& bank)
{
	if (!ms_pCreateButton)
		return;

	ms_pCreateButton->Destroy();
	ms_pCreateButton = NULL;

	ms_pAnimGroup = bank.PushGroup("Ped Vehicle Anim Metadata");

	// Load/Save
	const char* stringArray[64];
	for(int i=0; i<ms_VehicleLayoutFiles.GetCount(); ++i)
	{
		stringArray[i] = ms_VehicleLayoutFiles[i].TryGetCStr();
	}
	bank.AddCombo("Vehicle layout file", &ms_CurrentSelection, ms_VehicleLayoutFiles.GetCount(), stringArray, datCallback(CFA1(RefreshAnimLayout), &bank));
	bank.AddButton("Save", bank_Save);
	bank.AddSeparator("sep1");
	bank.AddButton("Save All", bank_SaveAll);
	bank.AddButton("Generate Entry Offset Info", GenerateEntryOffsetInfo); 
	bank.AddSeparator("sep2");
	RefreshAnimLayout(bank);

	bank.PopGroup(); // "Ped Vehicle Anim Metadata"
}

void CVehicleMetadataMgr::RefreshAnimLayout(bkBank& bank)
{
	if (!ms_pAnimGroup)
		return;

	bkGroup* group = static_cast<bkGroup*>(bank.GetCurrentGroup());
	if (group != &bank)
		bank.UnSetCurrentGroup(*group);
	bank.SetCurrentGroup(*ms_pAnimGroup);
	{
		AddCategory(bank, ms_Instance.m_AnimRateSets, "Anim Rate Sets", ms_VehicleLayoutFiles[ms_CurrentSelection]);
		AddCategory(bank, ms_Instance.m_EntryAnimVariations, "Entry Anim Variations", ms_VehicleLayoutFiles[ms_CurrentSelection]);
		AddCategory(bank, ms_Instance.m_VehicleExtraPointsInfos, "Vehicle Extra Points Infos", ms_VehicleLayoutFiles[ms_CurrentSelection]);
		AddCategory(bank, ms_Instance.m_VehicleLayoutInfos, "Vehicle Layout Infos", ms_VehicleLayoutFiles[ms_CurrentSelection]);
		AddCategory(bank, ms_Instance.m_VehicleDriveByInfos, "Vehicle Driveby Infos", ms_VehicleLayoutFiles[ms_CurrentSelection]);
		AddCategory(bank, ms_Instance.m_VehicleDriveByAnimInfos, "Vehicle Driveby Anim Infos", ms_VehicleLayoutFiles[ms_CurrentSelection]);
		AddCategory(bank, ms_Instance.m_DrivebyWeaponGroups, "Vehicle Driveby Weapon Slot Groups", ms_VehicleLayoutFiles[ms_CurrentSelection]);
		AddCategory(bank, ms_Instance.m_VehicleSeatInfos, "Vehicle Seat Infos", ms_VehicleLayoutFiles[ms_CurrentSelection]);
		AddCategory(bank, ms_Instance.m_VehicleSeatAnimInfos, "Vehicle Seat Anim Infos", ms_VehicleLayoutFiles[ms_CurrentSelection]);
		AddCategory(bank, ms_Instance.m_VehicleEntryPointInfos, "Vehicle Entry Point Infos", ms_VehicleLayoutFiles[ms_CurrentSelection]);
		AddCategory(bank, ms_Instance.m_VehicleEntryPointAnimInfos, "Vehicle Entry Point Anim Infos", ms_VehicleLayoutFiles[ms_CurrentSelection]);
		AddCategory(bank, ms_Instance.m_VehicleExplosionInfos, "Vehicle Explosion Infos", ms_VehicleLayoutFiles[ms_CurrentSelection]);
		AddCategory(bank, ms_Instance.m_VehicleScenarioLayoutInfos, "Vehicle Scenario Layout Infos", ms_VehicleLayoutFiles[ms_CurrentSelection]);
		AddCategory(bank, ms_Instance.m_VehicleCoverBoundOffsetInfos, "Vehicle Cover Bound Offset Infos", ms_VehicleLayoutFiles[ms_CurrentSelection]);
		AddCategory(bank, ms_Instance.m_BicycleInfos, "Bicycle Infos", ms_VehicleLayoutFiles[ms_CurrentSelection]);
		AddCategory(bank, ms_Instance.m_POVTuningInfos, "POV Tuning Infos", ms_VehicleLayoutFiles[ms_CurrentSelection]);
		AddCategory(bank, ms_Instance.m_SeatOverrideAnimInfos, "Seat Override Contexts", ms_VehicleLayoutFiles[ms_CurrentSelection]);
		AddCategory(bank, ms_Instance.m_InVehicleOverrideInfos, "In Vehicle Override Infos", ms_VehicleLayoutFiles[ms_CurrentSelection]);
		AddCategory(bank, ms_Instance.m_ClipSetMaps, "CClipSetMaps", ms_VehicleLayoutFiles[ms_CurrentSelection]);
		AddCategory(bank, ms_Instance.m_FirstPersonDriveByLookAroundData, "First Person Driveby Data", ms_VehicleLayoutFiles[ms_CurrentSelection]);
	}
	bank.UnSetCurrentGroup(*ms_pAnimGroup);
	if (group != &bank)
		bank.SetCurrentGroup(*group);
}

void CVehicleMetadataMgr::AddVehicleExplosionWidgets(bkBank& bank)
{
	bank.PushGroup("Vehicle Explosion Infos");
	bank.AddToggle("Draw Vehicle Explosion Info Names", &CVehicleExplosionInfo::ms_DrawVehicleExplosionInfoNames);
	bank.AddToggle("Draw Vehicle Explosion LOD Spheres", &CVehicleExplosionInfo::ms_DrawVehicleExplosionInfoLodSpheres);
	bank.AddToggle("Enable Part Deletion Chance Override", &CVehicleExplosionLOD::ms_EnablePartDeletionChanceOverride);
	bank.AddSlider("Part Deletion Chance Override", &CVehicleExplosionLOD::ms_PartDeletionChanceOverride, 0.0f, 1.0f, 0.05f);
	bank.AddToggle("Enable Additional Part Velocity Override", &CVehicleExplosionInfo::ms_EnableAdditionalPartVelocityOverride);
	bank.AddSlider("Additional Part Velocity Angle Override", &CVehicleExplosionInfo::ms_AdditionalPartVelocityAngleOverride,0.0f,90.0f,0.1f);
	bank.AddSlider("Additional Part Velocity Magnitude Override", &CVehicleExplosionInfo::ms_AdditionalPartVelocityMagnitudeOverride,0.0f,100.0f,1.0f);
	for(s32 i = 0; i < ms_Instance.m_VehicleExplosionInfos.GetCount(); i++)
	{
		bank.PushGroup(ms_Instance.m_VehicleExplosionInfos[i]->GetName().GetCStr());
		PARSER.AddWidgets(bank, ms_Instance.m_VehicleExplosionInfos[i]);
		bank.PopGroup();
	}
	bank.PopGroup(); // "Vehicle Explosion Infos"
}

////////////////////////////////////////////////////////////////////////////////

void CVehicleMetadataMgr::GenerateEntryOffsetInfo()
{


// 	for (s32 i=0; i<ms_Instance.m_VehicleEntryPointAnimInfos.GetCount(); i++)
// 	{
// 		CVehicleEntryPointAnimInfo* pEntryPointAnimInfo = ms_Instance.m_VehicleEntryPointAnimInfos[i];
// 		if (pEntryPointAnimInfo)
// 		{
			fwMvClipSetId clipSetId = CAnimViewer::m_animSelector.GetSelectedClipSetId();

			fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetId);
			if (pClipSet)
			{
				//if (pClipSet->StreamIn())
				{
					TUNE_BOOL(DO_JACK, false);
					TUNE_BOOL(DO_QUICK_GET_ON, true);
					const crClip* pClip = DO_JACK ? pClipSet->GetClip(fwMvClipId("jack_base_perp",0x8501100C)) : pClipSet->GetClip(fwMvClipId("get_in",0x1B922298));
					if (DO_QUICK_GET_ON)
					{
						pClip = pClipSet->GetClip(fwMvClipId("get_on_quick",0xD806BD43));
					}
					if (pClip)
					{
						Vector3 vOffset = fwAnimHelpers::GetMoverTrackTranslationDiff(*pClip, 1.f, 0.f);
 						Quaternion qStartRotation = fwAnimHelpers::GetMoverTrackRotation(*pClip, 0.f);
						const float fInitialHeadingOffset = qStartRotation.TwistAngle(ZAXIS);
						// Account for any initial z rotation
						vOffset.RotateZ(-fInitialHeadingOffset);
						// Get the total z axis rotation over the course of the get_in anim excluding the initial rotation
						Quaternion qRotTotal = fwAnimHelpers::GetMoverTrackRotationDiff(*pClip, 1.f, 0.f);	
						const float fTotalHeadingRotation = qRotTotal.TwistAngle(ZAXIS);
						vOffset.RotateZ(fTotalHeadingRotation);
						Displayf("Initial Z Axis Rotation = %.4f", fInitialHeadingOffset);
						Displayf("<EntryTranslation x=\"%.4f\" y=\"%.4f\" z=\"%.4f\" />", vOffset.x, vOffset.y, vOffset.z);
						Displayf("<EntryHeadingChange value=\"%.4f\" />", fTotalHeadingRotation);
						//pEntryPointAnimInfo->SetEntryTranslation(vOffset);
						//pEntryPointAnimInfo->SetEntryHeadingChange(fTotalHeadingRotation);
					}
				}
			}
// 		}
// 	}
}

////////////////////////////////////////////////////////////////////////////////

void CVehicleMetadataMgr::Save(int index)
{
	CVehicleMetadataMgr tempInst;
	ms_InstanceExtra = &tempInst;

	ExtractTempMetaData(ms_Instance.m_AnimRateSets, ms_InstanceExtra->m_AnimRateSets, ms_VehicleLayoutFiles[index].TryGetCStr());
	ExtractTempMetaData(ms_Instance.m_ClipSetMaps, ms_InstanceExtra->m_ClipSetMaps, ms_VehicleLayoutFiles[index].TryGetCStr());
	ExtractTempMetaData(ms_Instance.m_VehicleCoverBoundOffsetInfos, ms_InstanceExtra->m_VehicleCoverBoundOffsetInfos, ms_VehicleLayoutFiles[index].TryGetCStr());
	ExtractTempMetaData(ms_Instance.m_BicycleInfos, ms_InstanceExtra->m_BicycleInfos, ms_VehicleLayoutFiles[index].TryGetCStr());
	ExtractTempMetaData(ms_Instance.m_POVTuningInfos, ms_InstanceExtra->m_POVTuningInfos, ms_VehicleLayoutFiles[index].TryGetCStr());
	ExtractTempMetaData(ms_Instance.m_EntryAnimVariations, ms_InstanceExtra->m_EntryAnimVariations, ms_VehicleLayoutFiles[index].TryGetCStr());
	ExtractTempMetaData(ms_Instance.m_VehicleExtraPointsInfos, ms_InstanceExtra->m_VehicleExtraPointsInfos, ms_VehicleLayoutFiles[index].TryGetCStr());
	ExtractTempMetaData(ms_Instance.m_VehicleLayoutInfos, ms_InstanceExtra->m_VehicleLayoutInfos, ms_VehicleLayoutFiles[index].TryGetCStr());
	ExtractTempMetaData(ms_Instance.m_VehicleSeatInfos, ms_InstanceExtra->m_VehicleSeatInfos, ms_VehicleLayoutFiles[index].TryGetCStr());
	ExtractTempMetaData(ms_Instance.m_VehicleSeatAnimInfos, ms_InstanceExtra->m_VehicleSeatAnimInfos, ms_VehicleLayoutFiles[index].TryGetCStr());
	ExtractTempMetaData(ms_Instance.m_DrivebyWeaponGroups, ms_InstanceExtra->m_DrivebyWeaponGroups, ms_VehicleLayoutFiles[index].TryGetCStr());
	ExtractTempMetaData(ms_Instance.m_VehicleDriveByAnimInfos, ms_InstanceExtra->m_VehicleDriveByAnimInfos, ms_VehicleLayoutFiles[index].TryGetCStr());
	ExtractTempMetaData(ms_Instance.m_VehicleDriveByInfos, ms_InstanceExtra->m_VehicleDriveByInfos, ms_VehicleLayoutFiles[index].TryGetCStr());
	ExtractTempMetaData(ms_Instance.m_VehicleEntryPointInfos, ms_InstanceExtra->m_VehicleEntryPointInfos, ms_VehicleLayoutFiles[index].TryGetCStr());
	ExtractTempMetaData(ms_Instance.m_VehicleEntryPointAnimInfos, ms_InstanceExtra->m_VehicleEntryPointAnimInfos, ms_VehicleLayoutFiles[index].TryGetCStr());
	ExtractTempMetaData(ms_Instance.m_VehicleExplosionInfos, ms_InstanceExtra->m_VehicleExplosionInfos, ms_VehicleLayoutFiles[index].TryGetCStr());
	ExtractTempMetaData(ms_Instance.m_VehicleScenarioLayoutInfos, ms_InstanceExtra->m_VehicleScenarioLayoutInfos, ms_VehicleLayoutFiles[index].TryGetCStr());
	ExtractTempMetaData(ms_Instance.m_SeatOverrideAnimInfos, ms_InstanceExtra->m_SeatOverrideAnimInfos, ms_VehicleLayoutFiles[index].TryGetCStr());
	ExtractTempMetaData(ms_Instance.m_InVehicleOverrideInfos, ms_InstanceExtra->m_InVehicleOverrideInfos, ms_VehicleLayoutFiles[index].TryGetCStr());
	ExtractTempMetaData(ms_Instance.m_FirstPersonDriveByLookAroundData, ms_InstanceExtra->m_FirstPersonDriveByLookAroundData, ms_VehicleLayoutFiles[index].TryGetCStr());

	const fiDevice *device = fiDevice::GetDevice(ms_VehicleLayoutFiles[index].TryGetCStr());
	if(Verifyf(device, "Couldn't get device for %s", ms_VehicleLayoutFiles[index].TryGetCStr()))
	{
		char path[RAGE_MAX_PATH];
		device->FixRelativeName(path, RAGE_MAX_PATH, ms_VehicleLayoutFiles[index].TryGetCStr());
		vehicleVerifyf(PARSER.SaveObject(path, "meta", ms_InstanceExtra, parManager::XML), "Failed to save vehicle layouts");
	}

	ms_InstanceExtra = NULL;
}

void CVehicleMetadataMgr::CreateOwnershipInfos(CVehicleMetadataMgr& anInstance, const char* fileName)
{
	COwnershipInfo<CAnimRateSet*>::AddArray(anInstance.m_AnimRateSets, fileName);
	COwnershipInfo<CClipSetMap*>::AddArray(anInstance.m_ClipSetMaps, fileName);
	COwnershipInfo<CVehicleCoverBoundOffsetInfo*>::AddArray(anInstance.m_VehicleCoverBoundOffsetInfos, fileName);
	COwnershipInfo<CBicycleInfo*>::AddArray(anInstance.m_BicycleInfos, fileName);
	COwnershipInfo<CPOVTuningInfo*>::AddArray(anInstance.m_POVTuningInfos, fileName);
	COwnershipInfo<CEntryAnimVariations*>::AddArray(anInstance.m_EntryAnimVariations, fileName);
	COwnershipInfo<CVehicleExtraPointsInfo*>::AddArray(anInstance.m_VehicleExtraPointsInfos, fileName);
	COwnershipInfo<CVehicleLayoutInfo*>::AddArray(anInstance.m_VehicleLayoutInfos, fileName);
	COwnershipInfo<CVehicleSeatInfo*>::AddArray(anInstance.m_VehicleSeatInfos, fileName);
	COwnershipInfo<CVehicleSeatAnimInfo*>::AddArray(anInstance.m_VehicleSeatAnimInfos, fileName);
	COwnershipInfo<CDrivebyWeaponGroup*>::AddArray(anInstance.m_DrivebyWeaponGroups, fileName);
	COwnershipInfo<CVehicleDriveByAnimInfo*>::AddArray(anInstance.m_VehicleDriveByAnimInfos, fileName);
	COwnershipInfo<CVehicleDriveByInfo*>::AddArray(anInstance.m_VehicleDriveByInfos, fileName);
	COwnershipInfo<CVehicleEntryPointInfo*>::AddArray(anInstance.m_VehicleEntryPointInfos, fileName);
	COwnershipInfo<CVehicleEntryPointAnimInfo*>::AddArray(anInstance.m_VehicleEntryPointAnimInfos, fileName);
	COwnershipInfo<CVehicleExplosionInfo*>::AddArray(anInstance.m_VehicleExplosionInfos, fileName);
	COwnershipInfo<CVehicleScenarioLayoutInfo*>::AddArray(anInstance.m_VehicleScenarioLayoutInfos, fileName);
	COwnershipInfo<CSeatOverrideAnimInfo*>::AddArray(anInstance.m_SeatOverrideAnimInfos, fileName);
	COwnershipInfo<CInVehicleOverrideInfo*>::AddArray(anInstance.m_InVehicleOverrideInfos, fileName);
	COwnershipInfo<CFirstPersonDriveByLookAroundData*>::AddArray(anInstance.m_FirstPersonDriveByLookAroundData, fileName);
}

void CVehicleMetadataMgr::AddVehicleLayoutFile(const char* fileName)
{
	if(ms_VehicleLayoutFiles.Find(fileName) < 0)
		ms_VehicleLayoutFiles.PushAndGrow(fileName);
}

void CVehicleMetadataMgr::bank_Save()
{
	Save(ms_CurrentSelection);
}


void CVehicleMetadataMgr::bank_SaveAll()
{
	for(int i=0; i<ms_VehicleLayoutFiles.GetCount(); ++i)
	{
		Save(i);
	}
}
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////
