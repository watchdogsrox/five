//
// Vehicles/Metadata/VehicleMetadataManager.h
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

#ifndef INC_VEHICLE_METADATA_MANAGER_H
#define INC_VEHICLE_METADATA_MANAGER_H

////////////////////////////////////////////////////////////////////////////////

// Rage headers
#include "parser/macros.h"

// Game headers
#include "scene/ExtraContent.h"

////////////////////////////////////////////////////////////////////////////////

// Forward declarations
class CAnimRateSet;
class CVehicleCoverBoundOffsetInfo;
class CVehicleLayoutInfo;
class CVehicleSeatInfo;
class CVehicleSeatAnimInfo;
class CDrivebyWeaponGroup;
class CVehicleDriveByInfo;
class CVehicleDriveByAnimInfo;
class CVehicleEntryPointInfo;
class CVehicleEntryPointAnimInfo;
class CVehicleExplosionInfo;
class CVehicleScenarioLayoutInfo;
class CVehicleExtraPointsInfo;
class CEntryAnimVariations;
class CSeatOverrideAnimInfo;
class CInVehicleOverrideInfo;
class CBicycleInfo;
class CPOVTuningInfo;
class CClipSetMap;
class CPed;
class CFirstPersonDriveByLookAroundData;

////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------
// PURPOSE: Class which manages access to vehicle metadata
//-------------------------------------------------------------------------
class CVehicleMetadataMgr
{
public:

	////////////////////////////////////////////////////////////////////////////////

	// Static functions
	static void		Init(unsigned initMode);
	static void		Shutdown(unsigned shutdownMode);

	// Info accessors
	static const CAnimRateSet*					GetAnimRateSet(u32 uHash);
	static const CClipSetMap*					GetClipSetMap(u32 uHash);
	static const CBicycleInfo*					GetBicycleInfo(u32 uHash);
	static const CPOVTuningInfo*				GetPOVTuningInfo(u32 uHash);
	static const CVehicleCoverBoundOffsetInfo*  GetVehicleCoverBoundOffsetInfo(u32 uHash);
	static const CVehicleLayoutInfo*			GetVehicleLayoutInfo(u32 uHash);
	static const CVehicleSeatInfo*				GetVehicleSeatInfo(u32 uHash);
	static const CVehicleSeatAnimInfo*			GetVehicleSeatAnimInfo(u32 uHash); 
	static const CSeatOverrideAnimInfo*			GetSeatOverrideAnimInfo(u32 uHash); 
	static const CInVehicleOverrideInfo*		GetInVehicleOverrideInfo(u32 uHash); 
	static const CVehicleSeatAnimInfo*			GetSeatAnimInfoFromPed(const CPed *pPed);
	static const CVehicleSeatAnimInfo*			GetSeatAnimInfoFromSeatIndex(const CEntity* pEntity, s32 iSeatIndex);
	static const CDrivebyWeaponGroup*			GetDrivebyWeaponGroup(u32 uHash);
	static const CVehicleDriveByInfo*			GetVehicleDriveByInfo(u32 uHash);
	static const CVehicleDriveByAnimInfo*		GetVehicleDriveByAnimInfo(u32 uHash);
	static const CVehicleEntryPointInfo*		GetVehicleEntryPointInfo(u32 uHash);
	static const CVehicleEntryPointAnimInfo*	GetVehicleEntryPointAnimInfo(u32 uHash);
	static const CVehicleExplosionInfo*			GetVehicleExplosionInfo(u32 uHash);
	static const CVehicleScenarioLayoutInfo*	GetVehicleScenarioLayoutInfo(u32 uHash);
	static const CVehicleExtraPointsInfo*		GetVehicleExtraPointsInfo(u32 uHash);
	static const CEntryAnimVariations*			GetEntryAnimVariations(u32 uHash);
	static s32									GetVehicleLayoutInfoCount();
	static CVehicleLayoutInfo*					GetVehicleLayoutInfoAtIndex(s32 iIndex);
	static const CFirstPersonDriveByLookAroundData*	GetFirstPersonDriveByLookAroundData(u32 uHash);
	static const CFirstPersonDriveByLookAroundData*	GetFirstPersonDriveByLookAroundDataAtIndex(u32 iIndex);
	static u32									GetTotalNumFirstPersonDriveByLookAroundData();

	const CAnimRateSet*							GetAnimRateSetByHash(u32 uHash);
	const CClipSetMap*							GetClipSetMapByHash(u32 uHash);
	const CBicycleInfo*							GetBicycleInfoByHash(u32 uHash);
	const CPOVTuningInfo*						GetPOVTuningInfoByHash(u32 uHash);
	const CVehicleCoverBoundOffsetInfo*			GetVehicleCoverBoundOffsetInfoByHash(u32 uHash);
	const CVehicleLayoutInfo*					GetVehicleLayoutInfoByHash(u32 uHash);
	const CVehicleSeatInfo*						GetVehicleSeatInfoByHash(u32 uHash);
	const CVehicleSeatAnimInfo*					GetVehicleSeatAnimInfoByHash(u32 uHash); 
	const CSeatOverrideAnimInfo*				GetSeatOverrideAnimInfoByHash(u32 uHash); 
	const CInVehicleOverrideInfo*				GetInVehicleOverrideInfoByHash(u32 uHash); 
	const CDrivebyWeaponGroup*					GetDrivebyWeaponGroupByHash(u32 uHash);
	const CVehicleDriveByInfo*					GetVehicleDriveByInfoByHash(u32 uHash);
	const CVehicleDriveByAnimInfo*				GetVehicleDriveByAnimInfoByHash(u32 uHash);
	const CVehicleEntryPointInfo*				GetVehicleEntryPointInfoByHash(u32 uHash);
	const CVehicleEntryPointAnimInfo*			GetVehicleEntryPointAnimInfoByHash(u32 uHash);
	const CVehicleExplosionInfo*				GetVehicleExplosionInfoByHash(u32 uHash);
	const CVehicleScenarioLayoutInfo*			GetVehicleScenarioLayoutInfoByHash(u32 uHash);
	const CVehicleExtraPointsInfo*				GetVehicleExtraPointsInfoByHash(u32 uHash);
	const CEntryAnimVariations*					GetEntryAnimVariationsByHash(u32 uHash);
	const CFirstPersonDriveByLookAroundData*	GetFirstPersonDriveByLookAroundDataByHash(u32 uHash);
	const CFirstPersonDriveByLookAroundData*	GetFirstPersonDriveByLookAroundDataAtIndexInternal(u32 iIndex);
	u32											GetNumFirstPersonDriveByLookAroundData();

	static const CVehicleDriveByInfo*			GetVehicleDriveByInfoFromPed(const CPed* pPed);
	static const CVehicleDriveByAnimInfo*		GetDriveByAnimInfoForWeaponGroup(const CPed* pPed, u32 uWeaponGroup);
	static const CVehicleDriveByAnimInfo*		GetDriveByAnimInfoForWeaponGroup(const CVehicleSeatAnimInfo* pSeatAnimInfo, u32 uWeaponGroup);
	static const CVehicleDriveByAnimInfo*		GetDriveByAnimInfoForWeapon(const CPed* pPed, u32 uWeapon);
	static const CVehicleDriveByAnimInfo*		GetDriveByAnimInfoForWeapon(const CVehicleSeatAnimInfo* pSeatAnimInfo, u32 uWeapon);
	static const CVehicleDriveByAnimInfo*		GetDriveByAnimInfoForWeapon(const CVehicleDriveByInfo* pDriveByInfo, u32 uWeapon);
	static bool									CanPedDoDriveByWithEquipedWeapon(const CPed* pPed);

	void ValidateData();

	CVehicleMetadataMgr();

	bool GetIsInitialised() const { return m_bIsInitialised; }

	static CVehicleMetadataMgr& GetInstance() { return ms_Instance; }

	static void AppendExtra(const char *fname);
	static void RemoveExtra(const char *fname);

private:

	////////////////////////////////////////////////////////////////////////////////
	
	static void Reset();
	static void Load();

	static void ValidateDataForFinal();

	template <typename T>
	static bool EntryExists(const rage::atArray<T> &from, const T &entry)
	{
		for(int i = 0; i < from.GetCount(); i++)
		{
			if(from[i]->GetName().GetHash() == entry->GetName().GetHash())
			{
				return true;
			}
		}

		return false;
	}

	template <typename T>
	static void AppendArray(rage::atArray<T> &to, const rage::atArray<T> &from BANK_ONLY(, const char* fileName=NULL))
	{
		for(int i = 0; i < from.GetCount(); i++)
		{
			if(!EntryExists(to, from[i]))
			{
				to.PushAndGrow(from[i]);

#if __BANK
				if(fileName)
					COwnershipInfo<T>::Add(from[i], fileName);
#endif // __BANK
			}
		}
	}

	template <typename T>
	static void RemoveEntry(rage::atArray<T> &from, T const entry)
	{
		for(int i = 0; i < from.GetCount(); i++)
		{
			if(from[i]->GetName().GetHash() == entry->GetName().GetHash())
			{
				delete from[i];
				from.DeleteFast(i);
				return;
			}
		}

		Assertf(false, "CVehicleMetadataMgr::RemoveEntry entry with name %s doesn't exist!", entry->GetName().GetCStr());
	}

	template <typename T>
	static void RemoveArray(rage::atArray<T> &from, rage::atArray<T> &entriesToRemove)
	{
		for(int i = 0; i < entriesToRemove.GetCount(); i++)
		{
			RemoveEntry(from, entriesToRemove[i]);
			delete entriesToRemove[i];

#if __BANK
			COwnershipInfo<T>::Remove(entriesToRemove[i]);
#endif // __BANK
		}

		entriesToRemove.Reset();
	}

#if __BANK
	template <typename T>
	static void ExtractTempMetaData(atArray<T>& mainInstArray, atArray<T>& tempInstArray, const char* fileName)
	{
		for(int i=0; i<mainInstArray.GetCount(); ++i)
		{
			if(!stricmp(COwnershipInfo<T>::Find(mainInstArray[i]), fileName))
			{
				tempInstArray.PushAndGrow(mainInstArray[i]);
			}
		}
	}

	template <typename T>
	static void AddCategory(bkBank& bank, atArray<T>& theArray, const char* title, atHashString& fileHash)
	{
		bkGroup* oldEntry = static_cast<bkGroup*>(bank.GetCurrentGroup()->FindChild(title));
		if (oldEntry)
		{
			bank.DeleteGroup(*oldEntry);
		}

		// Bail early if curent content file has no entries
		if(!COwnershipInfo<T>::HasEntryWithHash(theArray, fileHash))
			return;

		bank.PushGroup(title);
		for(s32 i=0; i < theArray.GetCount(); i++)
		{
			if(COwnershipInfo<T>::Found(theArray[i], fileHash))
			{
				bank.PushGroup(theArray[i]->GetName().GetCStr());
				PARSER.AddWidgets(bank, theArray[i]);
				bank.PopGroup();
			}
		}
		bank.PopGroup();
	}

	static void CreateOwnershipInfos(CVehicleMetadataMgr& anInstance, const char* fileName);

	static void AddVehicleLayoutFile(const char* fileName);
#endif // __BANK

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Static instance of the metadata manager
	static CVehicleMetadataMgr					ms_Instance;
	static CVehicleMetadataMgr					*ms_InstanceExtra;

#if __BANK
	// PURPOSE: array of vehicle layout file hash strings
	static atArray<atHashString> ms_VehicleLayoutFiles;
	static int ms_CurrentSelection;
	static bkButton* ms_pCreateButton;
	static bkGroup* ms_pAnimGroup;
#endif // __BANK

	// PURPOSE: Arrays of pointers to our infos
	atArray<CAnimRateSet*>						m_AnimRateSets;
	atArray<CClipSetMap*>						m_ClipSetMaps;
	atArray<CVehicleCoverBoundOffsetInfo*>		m_VehicleCoverBoundOffsetInfos;
	atArray<CBicycleInfo*>						m_BicycleInfos;
	atArray<CPOVTuningInfo*>					m_POVTuningInfos;
	atArray<CEntryAnimVariations*>				m_EntryAnimVariations;
	atArray<CVehicleExtraPointsInfo*>			m_VehicleExtraPointsInfos;
	atArray<CVehicleLayoutInfo*>				m_VehicleLayoutInfos;
	atArray<CVehicleSeatInfo*>					m_VehicleSeatInfos;
	atArray<CVehicleSeatAnimInfo*>				m_VehicleSeatAnimInfos;
	atArray<CDrivebyWeaponGroup*>				m_DrivebyWeaponGroups;
	atArray<CVehicleDriveByAnimInfo*>			m_VehicleDriveByAnimInfos;
	atArray<CVehicleDriveByInfo*>				m_VehicleDriveByInfos;
	atArray<CVehicleEntryPointInfo*>			m_VehicleEntryPointInfos;
	atArray<CVehicleEntryPointAnimInfo*>		m_VehicleEntryPointAnimInfos;
	atArray<CVehicleExplosionInfo*>				m_VehicleExplosionInfos;
	atArray<CVehicleScenarioLayoutInfo*>		m_VehicleScenarioLayoutInfos;
	atArray<CSeatOverrideAnimInfo*>				m_SeatOverrideAnimInfos;
	atArray<CInVehicleOverrideInfo*>			m_InVehicleOverrideInfos;
	atArray<CFirstPersonDriveByLookAroundData*>	m_FirstPersonDriveByLookAroundData;

	// PURPOSE: has the metadata manager been initialised
	bool m_bIsInitialised;

	PAR_SIMPLE_PARSABLE

	////////////////////////////////////////////////////////////////////////////////

public:
#if __BANK
	// Add widgets
	static void InitWidgets(bkBank& bank);
	static void AddWidgets(bkBank& bank);
	static void RefreshAnimLayout(bkBank& bank);
	static void GenerateEntryOffsetInfo();
	static void Save(int index);
	static void bank_Save();
	static void bank_SaveAll();
	static void bank_ReloadAll();

	static void AddVehicleExplosionWidgets(bkBank& bank);
#endif // __BANK

	////////////////////////////////////////////////////////////////////////////////
};

#endif // INC_VEHICLE_METADATA_MANAGER_H