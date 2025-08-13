//
// weapons/weaponinfomanager.h
//
// Copyright (C) 1999-2010 Rockstar North.  All Rights Reserved. 
//

#ifndef WEAPON_INFO_MANAGER_H
#define WEAPON_INFO_MANAGER_H

// Rage headers
#include "atl/array.h"
#include "data/base.h"

#include "parsercore/utils.h"

// Game headers
#include "Weapons/Info/ItemInfo.h"
#include "Weapons/Info/VehicleWeaponInfo.h"
#include "Weapons/Info/WeaponInfo.h"
#include "Weapons/WeaponChannel.h"

#include "scene/DataFileMgr.h"

////////////////////////////////////////////////////////////////////////////////
// sWeaponInfoList
////////////////////////////////////////////////////////////////////////////////

struct sWeaponInfoList
{
	typedef atArray<CItemInfo*> InfoList;
	InfoList Infos;
	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// sAimGunStrafingInfoList
////////////////////////////////////////////////////////////////////////////////

struct sAimingInfoList
{
	typedef atArray<CAimingInfo*> AimingInfoList;
	AimingInfoList AimingInfos;
	PAR_SIMPLE_PARSABLE;
};


////////////////////////////////////////////////////////////////////////////////
// CWeaponInfBlob
////////////////////////////////////////////////////////////////////////////////

class CWeaponInfoBlob : public datBase
{
public:
	// Categories of infos, for easier organising - if modifying, change the matching string array ms_InfoTypeNames
	enum InfoType
	{
		IT_Ammo = 0,
		IT_Weapon,
		IT_VehicleWeapon,
		IT_Damage,
		IT_Max
	};

	enum WeaponSlots
	{
		WS_Default = 0,
		WS_CnCCop,
		WS_Max
	};

	struct SlotEntry
	{
		int m_OrderNumber;
		atHashWithStringNotFinal m_Entry;

		SlotEntry() {}

		SlotEntry(atHashWithStringNotFinal entry)		{ m_OrderNumber = 0; m_Entry = entry; }

		u32 GetHash() const								{ return m_Entry.GetHash(); }

		operator u32()									{ return m_Entry.GetHash(); }

		bool operator ==(u32 hash)						{ return m_Entry.GetHash() == hash; }

		// This may be a bit deceptive, but we really don't care about the order number in real life.
		bool operator ==(const SlotEntry &o)			{ return o.m_Entry == m_Entry; }

#if __ASSERT
		void Validate();
#endif // __ASSERT

		static bool SortFunc(const SlotEntry &a, const SlotEntry &b)		{ return a.m_OrderNumber < b.m_OrderNumber; }

		PAR_SIMPLE_PARSABLE;
	};

	typedef atArray<SlotEntry> SlotList;

	struct sWeaponSlotList
	{
#if __ASSERT
		void Validate();
#endif // __ASSERT

		SlotList WeaponSlots;
		PAR_SIMPLE_PARSABLE;
	};

	// Armoured glass weapon group damage structure
	struct sWeaponGroupArmouredGlassDamage
	{
		atHashWithStringNotFinal GroupHash;
		float Damage;
		PAR_SIMPLE_PARSABLE;
	};

	CWeaponInfoBlob() :
		m_patchHash(0)
	{		
	}

	// Access a tint spec value
	const CWeaponTintSpecValues* GetWeaponTintSpecValue(atHashWithStringNotFinal uNameHash) const;

	// Access a firing pattern alias
	const CWeaponFiringPatternAliases* GetWeaponFiringPatternAlias(atHashWithStringNotFinal uNameHash) const;

	// Access a upper body expression data
	const CWeaponUpperBodyFixupExpressionData* GetWeaponUpperBodyFixupExpressionData(atHashWithStringNotFinal uNameHash) const;

	// Access a aiming info
	const CAimingInfo* GetAimingInfo(atHashWithStringNotFinal uNameHash) const;

	int GetAimingInfoCount() const								{ return m_AimingInfos.GetCount(); }

	const CAimingInfo *GetAimingInfoByIndex(int index) const	{ return m_AimingInfos[index]; }

	// Access a vehicle weapon info
	const CVehicleWeaponInfo* GetVehicleWeaponInfo(atHashWithStringNotFinal uNameHash) const;

	sWeaponInfoList &GetInfos(InfoType infoType)				{ return m_Infos[infoType]; }

	sWeaponSlotList &GetSlotNavigateOrder(WeaponSlots slot)		{ return m_SlotNavigateOrder[slot]; }

	int GetSlotNavigateOrderCount() const						{ return m_SlotNavigateOrder.GetCount(); }

	sWeaponSlotList &GetSlotBestOrder()							{ return m_SlotBestOrder; }

	const float GetArmouredGlassDamageForWeaponGroup(atHashWithStringNotFinal uGroupHash) const;

	// Delete the data
	void Reset();

#if __BANK
	void AddWidgets(bkBank &bank);

	// Add tuning widgets
	void AddTuningWidgets(bkBank& bank);

	// Add graphics widgets
	void AddGraphicsWidgets(bkBank& bank);

	// Add info widgets
	void AddInfoWidgets(bkBank& bank, InfoType type);

	// Add aiming info widgets
	void AddAimingInfoWidgets(bkBank& bank);

	// Save the data
	void Save();

	// Load the data
	void Load();

	atArray<CVehicleWeaponInfo*> &GetVehicleWeaponInfos()		{ return m_VehicleWeaponInfos; }

#endif // __BANK

	ConstString GetName(){return m_Name;}

	// Remember which filename we used to load this data - does not include the ".meta" extension.
	void SetFilename(const char *filename)						{ m_Filename = filename; }
	const char* GetFilename() const								{ return m_Filename; }

	void SetPatchHash(u32 hash)								{ m_patchHash = hash; }
	u32 GetPatchHash() const								{ return m_patchHash; }

	void Validate();

private:
	// Weapon tint data
	atArray<CWeaponTintSpecValues> m_TintSpecValues;

	// Weapon firing pattern alias data
	atArray<CWeaponFiringPatternAliases> m_FiringPatternAliases;

	// Weapon upper body expression data
	atArray<CWeaponUpperBodyFixupExpressionData> m_UpperBodyFixupExpressionData;

	// Aiming info storage
	atArray<CAimingInfo*> m_AimingInfos;

	// The vehicle weapon storage
	atArray<CVehicleWeaponInfo*> m_VehicleWeaponInfos;

	// The navigate slot list
	atFixedArray<sWeaponSlotList,WS_Max> m_SlotNavigateOrder;

	// The best slot list
	sWeaponSlotList m_SlotBestOrder;

	// The info storage
	sWeaponInfoList m_Infos[CWeaponInfoBlob::IT_Max];

	// Defines damage to apply to armoured vehicle glass based on weapon group
	atArray<sWeaponGroupArmouredGlassDamage> m_WeaponGroupDamageForArmouredVehicleGlass;

	// The name of this blob to identify which DLC it is from (if at all)
	ConstString m_Name;

	// The filename we loaded this data from
	ConstString m_Filename;

	u32			m_patchHash;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CWeaponInfoManager
////////////////////////////////////////////////////////////////////////////////

class CWeaponInfoManager
{
public:

	// Types
	typedef atArray<const char*> StringList;

	////////////////////////////////////////////////////////////////////////////////
	// sWeaponSlotList
	////////////////////////////////////////////////////////////////////////////////

	CWeaponInfoManager();

	// Initialise
	static void Init();

	// Shutdown
	static void Shutdown();

	// Check if the entity has been damaged by a general weapon type
	static bool CheckDamagedGeneralWeaponType(atHashWithStringNotFinal uDamagedByHash, s32 iGeneralWeaponType);

	// Access an item info
	static const CItemInfo* GetInfo(atHashWithStringNotFinal uNameHash ASSERT_ONLY(, bool bIgnoreAssert = false));
	static CItemInfo* GetInfoNonConst(atHashWithStringNotFinal uNameHash ASSERT_ONLY(, bool bIgnoreAssert = false));

	static const CItemInfo* GetInfoFromModelNameHash(atHashWithStringNotFinal uModelNameHash ASSERT_ONLY(, bool bIgnoreAssert = false));

	// Access an item info of a specified type - will assert if incorrect type
	template<typename _Info>
	static const _Info* GetInfo(atHashWithStringNotFinal uNameHash);
	template<typename _Info>
	static _Info* GetInfoNonConst(atHashWithStringNotFinal uNameHash);

	// Access an item info of a specified type - will assert if incorrect type - uses model name hash
	template<typename _Info>
	static const _Info* GetInfoFromModelNameHash(atHashWithStringNotFinal uModelNameHash ASSERT_ONLY(, bool bIgnoreAssert = false));

	// Returns the index in the weapon info list occupied by the given type
	static u32 GetInfoSlotForWeapon(atHashWithStringNotFinal uNameHash);

	// Access an item info from a index in the weapon info list
	static const CItemInfo* GetInfoFromSlot(u32 infoSlot);

	// Access an item info of a specified type - will assert if incorrect type
	template<typename _Info>
	static const _Info* GetInfoFromSlot(u32 infoSlot);

	// Access a tint spec value
	static const CWeaponTintSpecValues* GetWeaponTintSpecValue(atHashWithStringNotFinal uNameHash);

	// Access a firing pattern alias
	static const CWeaponFiringPatternAliases* GetWeaponFiringPatternAlias(atHashWithStringNotFinal uNameHash);

	// Access a upper body expression data
	static const CWeaponUpperBodyFixupExpressionData* GetWeaponUpperBodyFixupExpressionData(atHashWithStringNotFinal uNameHash);

	// Access a aiming info
	static const CAimingInfo* GetAimingInfo(atHashWithStringNotFinal uNameHash);

	// Access a vehicle weapon info
	static const CVehicleWeaponInfo* GetVehicleWeaponInfo(atHashWithStringNotFinal uNameHash);

	// Access armoured glass damage
	static const float GetArmouredGlassDamageForWeaponGroup(atHashWithStringNotFinal uGroupHash);

	// Order we iterate through the slots for weapon selection
	static const CWeaponInfoBlob::SlotList& GetSlotListNavigate(const CPed* pPed = NULL);

	// Order of best slots, for when selecting the best weapon
	static const CWeaponInfoBlob::SlotList& GetSlotListBest();

	// Create the list of sorted infos for binary searches from the current infos
	static void UpdateInfoPtrs();

	// Collate the information from all blobs
	static void CollateInfoBlobs();

	// Load one data file
	static void Load(const CDataFileMgr::DataFile & file);

	// Load one data file patch
	static void LoadPatch(const CDataFileMgr::DataFile & file);

	// Call this after loading one or more weapon info files
	static void PostLoad();
	
	static void Unload(const CDataFileMgr::DataFile & file);

	static void UnloadPatch(const CDataFileMgr::DataFile & file);

	static void ResetDamageModifiers();

	static CWeaponInfoBlob *FindWeaponInfoBlobByFileName(const char *fname);

	static u32 CalculateWeaponPatchCRC(u32 initValue);

#if __BANK
	// Widgets
	static atString GetWidgetGroupName();
	static void AddWidgetButton(bkBank& bank);
	static void AddWidgets(bkBank& bank);

	// Get a random weapon hash
	static u32 GetRandomOneHandedWeaponHash();
	static u32 GetRandomTwoHandedWeaponHash();
	static u32 GetRandomProjectileWeaponHash();
	static u32 GetRandomThrownWeaponHash();

	static bool *GetForceShaderUpdatePtr()			{ return &ms_bForceShaderUpdate; }

	// Get the list of names
	static StringList& GetNames(CWeaponInfoBlob::InfoType type);

	static const char *GetInfoTypeName(CWeaponInfoBlob::InfoType type)		{ return ms_InfoTypeNames[type]; }

	// Is force shader update set?
	static const bool GetForceShaderUpdate();
#endif // __BANK

private:

	// Delete the data
	static void Reset();

	// Load all registered data files
	static void Load();

#if !__FINAL
	// Validate the data
	static void Validate();
#endif // !__FINAL

#if __BANK
	// Add graphics widgets
	static void AddGraphicsWidgets(bkBank& bank);

	// Add info widgets
	static void AddInfoWidgets(bkBank& bank, CWeaponInfoBlob::InfoType type);

	// Add aiming info widgets
	static void AddAimingInfoWidgets(bkBank& bank);

	// Save example data structures
	static void SaveExamples();

	// Remove all references to infos on a load
	static void RemoveAllWeaponInfoReferences();

	// Set peds to default load outs
	static void GiveAllPedsDefaultItems();

	// Print weapon stats for all weapons
	static void DumpWeaponStats();

	// Print weapon stats for an individual weapon info
	static void DumpWeaponStats(const CWeaponInfo* pWeaponInfo, FileHandle fileHandle, fwClipSetRequestHelper& rAnimReqHelper);

	static StringList &GetInfoNames(CWeaponInfoBlob::InfoType infoType)		{ return ms_Instance.m_InfoNames[infoType]; }

#endif // __BANK

	//
	// Members
	//

	// The info storage
	sWeaponInfoList m_Infos[CWeaponInfoBlob::IT_Max];

	// Pointers to infos, sorted
	sWeaponInfoList m_InfoPtrs;

	atArray<CWeaponInfoBlob> m_WeaponInfoBlobs;

	// The navigate slot list
	atFixedArray<CWeaponInfoBlob::sWeaponSlotList,CWeaponInfoBlob::WS_Max> m_SlotNavigateOrder;

	// The best slot list
	CWeaponInfoBlob::sWeaponSlotList m_SlotBestOrder;

	// Static manager object
	static CWeaponInfoManager ms_Instance;

#if __BANK
	// Strings representing the loaded infos
	StringList m_InfoNames[CWeaponInfoBlob::IT_Max];

	// Strings corresponding to the enum InfoType - ensure this matches with InfoType enum
	static const char* ms_InfoTypeNames[CWeaponInfoBlob::IT_Max];

	// Strings representing the loaded infos
	StringList m_AimingInfoNames;

	// Force the shader update when set
	static bank_bool ms_bForceShaderUpdate;
#endif // __BANK
};

////////////////////////////////////////////////////////////////////////////////

inline const CItemInfo* CWeaponInfoManager::GetInfo(atHashWithStringNotFinal uNameHash ASSERT_ONLY(, bool bIgnoreAssert))
{
	if(uNameHash.GetHash() != 0)
	{
		s32 iLow = 0;
		s32 iHigh = ms_Instance.m_InfoPtrs.Infos.GetCount()-1;
		while(iLow <= iHigh)
		{
			s32 iMid = (iLow + iHigh) >> 1;
			if(uNameHash.GetHash() == ms_Instance.m_InfoPtrs.Infos[iMid]->GetHash())
			{
				return ms_Instance.m_InfoPtrs.Infos[iMid];
			}
			else if (uNameHash.GetHash() < ms_Instance.m_InfoPtrs.Infos[iMid]->GetHash())
			{
				iHigh = iMid-1;
			}
			else
			{
				iLow = iMid+1;
			}
		}
#if __ASSERT
		if(!bIgnoreAssert)
		{
			weaponAssertf(0, "CWeaponInfoManager item [%s][%d] not found in data", uNameHash.TryGetCStr(), uNameHash.GetHash());
		}
#endif
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////
inline CItemInfo* CWeaponInfoManager::GetInfoNonConst(atHashWithStringNotFinal uNameHash ASSERT_ONLY(, bool bIgnoreAssert))
{
	if(uNameHash.GetHash() != 0)
	{
		s32 iLow = 0;
		s32 iHigh = ms_Instance.m_InfoPtrs.Infos.GetCount()-1;
		while(iLow <= iHigh)
		{
			s32 iMid = (iLow + iHigh) >> 1;
			if(uNameHash.GetHash() == ms_Instance.m_InfoPtrs.Infos[iMid]->GetHash())
			{
				return ms_Instance.m_InfoPtrs.Infos[iMid];
			}
			else if (uNameHash.GetHash() < ms_Instance.m_InfoPtrs.Infos[iMid]->GetHash())
			{
				iHigh = iMid-1;
			}
			else
			{
				iLow = iMid+1;
			}
		}
#if __ASSERT
		if(!bIgnoreAssert)
		{
			weaponAssertf(0, "CWeaponInfoManager item [%s][%d] not found in data", uNameHash.TryGetCStr(), uNameHash.GetHash());
		}
#endif
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

inline const CItemInfo* CWeaponInfoManager::GetInfoFromModelNameHash(atHashWithStringNotFinal uModelNameHash ASSERT_ONLY(, bool bIgnoreAssert))
{
	if(uModelNameHash.GetHash() != 0)
	{
		const s32 sCount = ms_Instance.m_InfoPtrs.Infos.GetCount();
		for (s32 i=0; i < sCount; i++)
		{
			if(uModelNameHash.GetHash() == ms_Instance.m_InfoPtrs.Infos[i]->GetModelHash())
			{
				return ms_Instance.m_InfoPtrs.Infos[i];
			}
		}
	}

#if __ASSERT
	if(!bIgnoreAssert)
	{
		weaponAssertf(0, "CWeaponInfoManager item with model [%s][%d] not found in data", uModelNameHash.TryGetCStr(), uModelNameHash.GetHash());
	}
#endif

	return NULL;
}

////////////////////////////////////////////////////////////////////////////////
template<typename _Info>
inline const _Info* CWeaponInfoManager::GetInfo(atHashWithStringNotFinal uNameHash)
{
	const CItemInfo* pInfo = GetInfo(uNameHash);
	if(pInfo)
	{
		weaponFatalAssertf(pInfo->GetIsClassId(_Info::GetStaticClassId()), "Casting [%s][%s] to incorrect type [%s] - will crash!", pInfo->GetStaticClassId().TryGetCStr(), pInfo->GetName(), _Info::GetStaticClassId().TryGetCStr());
		return static_cast<const _Info*>(pInfo);
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////
template<typename _Info>
inline _Info* CWeaponInfoManager::GetInfoNonConst(atHashWithStringNotFinal uNameHash)
{
	CItemInfo* pInfo = GetInfoNonConst(uNameHash);
	if(pInfo)
	{
		weaponFatalAssertf(pInfo->GetIsClassId(_Info::GetStaticClassId()), "Casting [%s][%s] to incorrect type [%s] - will crash!", pInfo->GetStaticClassId().TryGetCStr(), pInfo->GetName(), _Info::GetStaticClassId().TryGetCStr());
		return static_cast<_Info*>(pInfo);
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////
template<typename _Info>
inline const _Info* CWeaponInfoManager::GetInfoFromModelNameHash(atHashWithStringNotFinal uModelNameHash ASSERT_ONLY(, bool bIgnoreAssert))
{
	if(uModelNameHash.GetHash() != 0)
	{
		const s32 sCount = ms_Instance.m_InfoPtrs.Infos.GetCount();
		for (s32 i=0; i < sCount; i++)
		{
			if(uModelNameHash.GetHash() == ms_Instance.m_InfoPtrs.Infos[i]->GetModelHash() && ms_Instance.m_InfoPtrs.Infos[i]->GetIsClassId(_Info::GetStaticClassId()))
			{
				return static_cast<const _Info*>(ms_Instance.m_InfoPtrs.Infos[i]);
			}
		}
	}

#if __ASSERT
	if(!bIgnoreAssert)
	{
		weaponAssertf(0, "CWeaponInfoManager item with model [%s][%d] and type [%s] not found in data", uModelNameHash.TryGetCStr(), uModelNameHash.GetHash(), _Info::GetStaticClassId().TryGetCStr());
	}
#endif

	return NULL;
}

////////////////////////////////////////////////////////////////////////////////
inline u32 CWeaponInfoManager::GetInfoSlotForWeapon(atHashWithStringNotFinal uNameHash)
{
	if(uNameHash.GetHash() != 0)
	{
		s32 iLow = 0;
		s32 iHigh = ms_Instance.m_InfoPtrs.Infos.GetCount()-1;
		while(iLow <= iHigh)
		{
			s32 iMid = (iLow + iHigh) >> 1;
			if(uNameHash.GetHash() == ms_Instance.m_InfoPtrs.Infos[iMid]->GetHash())
			{
				return iMid;
			}
			else if (uNameHash.GetHash() < ms_Instance.m_InfoPtrs.Infos[iMid]->GetHash())
			{
				iHigh = iMid-1;
			}
			else
			{
				iLow = iMid+1;
			}
		}
		weaponAssertf(0, "CWeaponInfoManager item [%s][%d] not found in data", parUtils::FindStringFromHash(atHashValue(uNameHash)), uNameHash.GetHash());
	}
	return 0;
}

////////////////////////////////////////////////////////////////////////////////

inline const CItemInfo* CWeaponInfoManager::GetInfoFromSlot(u32 slot)
{
	if (weaponVerifyf(slot < ms_Instance.m_InfoPtrs.Infos.GetCount(), "CWeaponInfoManager::GetInfoFromSlot - invalid slot %d", slot))
	{	
		return ms_Instance.m_InfoPtrs.Infos[slot];
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

template<typename _Info>
inline const _Info* CWeaponInfoManager::GetInfoFromSlot(u32 slot)
{
	const CItemInfo* pInfo = GetInfoFromSlot(slot);
	if(pInfo)
	{
		weaponFatalAssertf(pInfo->GetIsClassId(_Info::GetStaticClassId()), "Casting [%s][%s] to incorrect type [%s] - will crash!", pInfo->GetStaticClassId().TryGetCStr(), pInfo->GetName(), _Info::GetStaticClassId().TryGetCStr());
		return static_cast<const _Info*>(pInfo);
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

inline void CWeaponInfoManager::ResetDamageModifiers()
{
	sWeaponInfoList::InfoList& weaponList = ms_Instance.m_Infos[CWeaponInfoBlob::IT_Weapon].Infos;
	for(s32 i = 0; i < weaponList.GetCount(); i++)
	{
		if(weaponVerifyf(weaponList[i]->GetIsClassId(CWeaponInfo::GetStaticClassId()), "Info [%s] is not of type weapon, but is in CWeaponInfoBlob::IT_Weapon list", weaponList[i]->GetName()))
		{
			CWeaponInfo* pWeaponInfo = static_cast<CWeaponInfo*>(weaponList[i]);
			
			pWeaponInfo->SetWeaponDamageModifier(1.0f);
			pWeaponInfo->SetDamageFallOffRangeModifier(1.0f);
			pWeaponInfo->SetRecoilAccuracyToAllowHeadShotPlayerModifier(1.0f);
			pWeaponInfo->SetMinHeadShotDistancePlayerModifier(1.0f);
			pWeaponInfo->SetMaxHeadShotDistancePlayerModifier(1.0f);
			pWeaponInfo->SetHeadShotDamageModifierPlayerModifier(1.0f);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

inline const CWeaponTintSpecValues* CWeaponInfoBlob::GetWeaponTintSpecValue(atHashWithStringNotFinal uNameHash) const
{
	for(s32 i = 0; i < m_TintSpecValues.GetCount(); i++)
	{
		if(uNameHash.GetHash() == m_TintSpecValues[i].GetHash())
		{
			return &m_TintSpecValues[i];
		}
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

inline const CWeaponFiringPatternAliases* CWeaponInfoBlob::GetWeaponFiringPatternAlias(atHashWithStringNotFinal uNameHash) const
{
	for(s32 i = 0; i < m_FiringPatternAliases.GetCount(); i++)
	{
		if(uNameHash.GetHash() == m_FiringPatternAliases[i].GetHash())
		{
			return &m_FiringPatternAliases[i];
		}
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

inline const CWeaponUpperBodyFixupExpressionData* CWeaponInfoBlob::GetWeaponUpperBodyFixupExpressionData(atHashWithStringNotFinal uNameHash) const
{
	for(s32 i = 0; i < m_UpperBodyFixupExpressionData.GetCount(); i++)
	{
		if(uNameHash.GetHash() == m_UpperBodyFixupExpressionData[i].GetHash())
		{
			return &m_UpperBodyFixupExpressionData[i];
		}
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

inline const CAimingInfo* CWeaponInfoBlob::GetAimingInfo(atHashWithStringNotFinal uNameHash) const
{
	for(s32 i = 0; i < m_AimingInfos.GetCount(); i++)
	{
		if(uNameHash.GetHash() == m_AimingInfos[i]->GetHash())
		{
			return m_AimingInfos[i];
		}
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

inline const CVehicleWeaponInfo* CWeaponInfoBlob::GetVehicleWeaponInfo(atHashWithStringNotFinal uNameHash) const
{
	for(s32 i = 0; i < m_VehicleWeaponInfos.GetCount(); i++)
	{
		if(uNameHash.GetHash() == m_VehicleWeaponInfos[i]->GetHash())
		{
			return m_VehicleWeaponInfos[i];
		}
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

inline const float CWeaponInfoBlob::GetArmouredGlassDamageForWeaponGroup(atHashWithStringNotFinal uGroupHash) const
{
	for(s32 i = 0; i < m_WeaponGroupDamageForArmouredVehicleGlass.GetCount(); i++)
	{
		if(uGroupHash.GetHash() == m_WeaponGroupDamageForArmouredVehicleGlass[i].GroupHash)
		{
			return m_WeaponGroupDamageForArmouredVehicleGlass[i].Damage;
		}
	}
	
	return -1.0f;
}

////////////////////////////////////////////////////////////////////////////////

inline const CWeaponInfoBlob::SlotList& CWeaponInfoManager::GetSlotListBest()
{
	return ms_Instance.m_SlotBestOrder.WeaponSlots;
}

////////////////////////////////////////////////////////////////////////////////

#if __BANK
inline const bool CWeaponInfoManager::GetForceShaderUpdate()
{
	return ms_bForceShaderUpdate;
}
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////

#endif // WEAPON_INFO_MANAGER_H
