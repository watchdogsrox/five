//
// weapons/weaponcomponentmanager.h
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

#ifndef WEAPON_COMPONENT_MANAGER_H
#define WEAPON_COMPONENT_MANAGER_H

// Game headers
#include "Weapons/Components/WeaponComponent.h"

// Forward declarations
class CWeaponComponentData;

////////////////////////////////////////////////////////////////////////////////

struct CWeaponComponentInfoBlob
{
	typedef atArray<CWeaponComponentData*> DataList;
	typedef atFixedArray<CWeaponComponentInfo*, CWeaponComponentInfo::MAX_STORAGE> InfoList;


	void Reset();



	// PURPOSE: Component datas
	DataList m_Data;

	// PURPOSE: Component infos
	InfoList m_Infos;

	// Name of this blob - used to identify DLC
	atString m_InfoBlobName;


#if __BANK
	void AddWidgets(bkBank &bank);

	void Save();

	// File that this was loaded from
	atString m_Filename;
#endif // __BANK


	PAR_SIMPLE_PARSABLE;
};

class CWeaponComponentManager
{
public:
	typedef atFixedArray<const char*, CWeaponComponentInfo::MAX_STORAGE> StringList;


	// PURPOSE: Initialise
	static void Init();

	// PURPOSE: Shutdown
	static void Shutdown();

	// PURPOSE: Access a component info
	static const CWeaponComponentInfo* GetInfo(atHashWithStringNotFinal nameHash);

	// PUROSE: Access an item info of a specified type - will assert if incorrect type
	template<typename _Info>
	static const _Info* GetInfo(atHashWithStringNotFinal nameHash);

	// PURPOSE: Used by the PARSER.  Gets a name from a pointer
	static const char* GetNameFromData(const CWeaponComponentData* pData);

	// PURPOSE: Used by the PARSER.  Gets a pointer from a name
	static const CWeaponComponentData* GetDataFromName(const char* name);

	// PURPOSE: Used by the PARSER.  Gets a name from a pointer
	static const char* GetNameFromInfo(const CWeaponComponentInfo* pInfo);

	// PURPOSE: Used by the PARSER.  Gets a pointer from a name
	static const CWeaponComponentInfo* GetInfoFromName(const char* name);

	// PURPOSE: Load one data file
	static void Load(const char *file);

	// PURPOSE: Preform housekeeping after loading one or more blobs
	static void PostLoad();

#if __BANK
	// Widgets
	static atString GetWidgetGroupName();
	static void AddWidgetButton(bkBank& bank);
	static void AddWidgets(bkBank& bank);

	// Get the list of names
	static StringList& GetNames();

	static void ReloadInfoBlob(CWeaponComponentInfoBlob *blob);
#endif // __BANK

private:

	// PURPOSE: Delete the data
	static void Reset();

	// PURPOSE: Load the data
	static void Load();

#if __BANK
	// Save the data
	static void LoadBlob(void *blob);
#endif // __BANK

	// PURPOSE: Create the list of sorted infos for binary searches from the current infos
	static void UpdateInfoPtrs();

	// PURPOSE: Function to sort the array so we can use a binary search
	static s32 ComponentSort(CWeaponComponentInfo* const* a, CWeaponComponentInfo* const* b);

	//
	// Members
	//

	// PURPOSE: Pointers to infos, sorted
	CWeaponComponentInfoBlob::InfoList m_InfoPtrs;

	atArray<CWeaponComponentInfoBlob *> m_InfoBlobs;

#if __BANK
	// PURPOSE: Strings representing the loaded infos
	StringList m_InfoNames;
#endif // __BANK

	// PURPOSE: Static manager object
	static CWeaponComponentManager ms_Instance;
};

////////////////////////////////////////////////////////////////////////////////

inline const CWeaponComponentInfo* CWeaponComponentManager::GetInfo(atHashWithStringNotFinal nameHash)
{
	if(nameHash.GetHash() != 0)
	{
		s32 iLow = 0;
		s32 iHigh = ms_Instance.m_InfoPtrs.GetCount()-1;
		while(iLow <= iHigh)
		{
			s32 iMid = (iLow + iHigh) >> 1;
			if(nameHash.GetHash() == ms_Instance.m_InfoPtrs[iMid]->GetHash())
			{
				return ms_Instance.m_InfoPtrs[iMid];
			}
			else if (nameHash.GetHash() < ms_Instance.m_InfoPtrs[iMid]->GetHash())
			{
				iHigh = iMid-1;
			}
			else
			{
				iLow = iMid+1;
			}
		}
		weaponAssertf(0, "Weapon component [%s][%d] not found in data", nameHash.GetCStr(), nameHash.GetHash());
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

template<typename _Info>
inline const _Info* CWeaponComponentManager::GetInfo(atHashWithStringNotFinal nameHash)
{
	const CWeaponComponentInfo* pInfo = GetInfo(nameHash);
	if(pInfo)
	{
		weaponFatalAssertf(pInfo->GetIsClassId(_Info::GetStaticClassId()), "Casting to incorrect type - will crash!");
		return static_cast<const _Info*>(pInfo);
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

#if __BANK
inline CWeaponComponentManager::StringList& CWeaponComponentManager::GetNames()
{
	return ms_Instance.m_InfoNames;
}
#endif

////////////////////////////////////////////////////////////////////////////////

inline s32 CWeaponComponentManager::ComponentSort(CWeaponComponentInfo* const* a, CWeaponComponentInfo* const* b)
{
	return ((*a)->GetHash() < (*b)->GetHash() ? -1 : 1);
}

////////////////////////////////////////////////////////////////////////////////

#endif // WEAPON_COMPONENT_MANAGER_H
