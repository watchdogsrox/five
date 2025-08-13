//
// Filename:	ExtraMetadataMgr.h
// Description: Wrapper for any type of generic extra dlc metadata needed after game release
//
//

#include "scene/ExtraMetadataMgr.h"
#include "scene/DataFileMgr.h"

// rage headers
#include "parser/manager.h"
#include "string/stringhash.h"
#include "fwsys/metadatastore.h"

// game headers
#include "scene/DataFileMgr.h"
#include "scene/dlc_channel.h"
#include "modelinfo/modelinfo.h"
#include "modelinfo/pedmodelinfo.h"
#include "scene/ExtraContent.h"
#include "script/script.h"
#include "scene/WarpManager.h"
#include "modelinfo/VehicleModelInfoEnums.h"

static const int TATTOO_COLLECTION_INITIAL_SIZE = 4;
static const int TATTOO_ITEM_LOOKUP_INITIAL_SIZE = 4;
static const int TATTOO_ITEM_LOOKUP_GROW = 4;

#if __BANK && !__SPU
#include "peds/rendering/PedVariationDS.h"
#include "peds/ped.h"
#include "peds/rendering/PedOverlay.h"
#include "scene/world/GameWorld.h"
#include "Vehicles/VehicleFactory.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "modelInfo/vehicleModelInfo.h"
#include "modelinfo/VehicleModelInfoColors.h"
#include "weapons/components/WeaponComponent.h"
#include "weapons/components/WeaponComponentManager.h"
#include "system/exec.h"

#define MAX_APPAREL_ITEMS_PER_PAGE 10

s32 CExtraMetadataMgr::m_currentOutfitPg = 0;
s32 CExtraMetadataMgr::m_currentComponentPg = 0;
s32 CExtraMetadataMgr::m_currentPropPg = 0;
s32 CExtraMetadataMgr::m_sWeaponListIdx = -1;
s32	CExtraMetadataMgr::m_sVehicleComboIdx = 0;
s32	CExtraMetadataMgr::m_sWeaponComboIdx = 0;
s32 CExtraMetadataMgr::m_sNumOfListedMods = 0;
s32 CExtraMetadataMgr::m_sNumOfListedGenericMods = 0;
s32 CExtraMetadataMgr::m_sNumOfListedWeaponComponents = 0;
s32 CExtraMetadataMgr::m_sNumOfListedTattoos = 0;
bkList* CExtraMetadataMgr::m_bankComponentList = NULL;
bkList* CExtraMetadataMgr::m_bankTattooList = NULL;
bkList* CExtraMetadataMgr::m_bankTattooLookupList = NULL;
bkList* CExtraMetadataMgr::m_bankPropList = NULL;
bkList* CExtraMetadataMgr::m_bankOutfitList = NULL;
bkList* CExtraMetadataMgr::m_bankWeaponComponentsList = NULL;
bkList* CExtraMetadataMgr::m_bankVehicleModList = NULL;
bkList* CExtraMetadataMgr::m_bankGenericVehicleModList = NULL;
bkCombo* CExtraMetadataMgr::m_bankVehicleCombo = NULL;
bkCombo* CExtraMetadataMgr::m_bankWeaponCombo = NULL;
bkCombo* CExtraMetadataMgr::m_compCharCombo = NULL;
s32 CExtraMetadataMgr::m_compCharComboIdx = 0;
bkCombo* CExtraMetadataMgr::m_compDLCNameCombo = NULL;
s32 CExtraMetadataMgr::m_compDLCNameComboIdx = 0;
bkCombo* CExtraMetadataMgr::m_propCharCombo = NULL;
s32 CExtraMetadataMgr::m_propCharComboIdx = 0;
bkCombo* CExtraMetadataMgr::m_propDLCNameCombo = NULL;
s32 CExtraMetadataMgr::m_propDLCNameComboIdx = 0;
bkCombo* CExtraMetadataMgr::m_outfitCharCombo = NULL;
s32 CExtraMetadataMgr::m_outfitCharComboIdx = 0;
bkCombo* CExtraMetadataMgr::m_outfitDLCNameCombo = NULL;
s32 CExtraMetadataMgr::m_outfitDLCNameComboIdx = 0;
atArray<const char*> CExtraMetadataMgr::ms_outfitDLCNames;
atArray<const char*> CExtraMetadataMgr::ms_propDLCNames;
atArray<const char*> CExtraMetadataMgr::ms_compDLCNames;
char CExtraMetadataMgr::m_shopPedComponentLookup[RAGE_MAX_PATH] = { 0 };
char CExtraMetadataMgr::m_shopPedPropLookup[RAGE_MAX_PATH] = { 0 };
char CExtraMetadataMgr::m_shopPedComponentCounts[RAGE_MAX_PATH] = { 0 };
char CExtraMetadataMgr::m_shopPedPropCounts[RAGE_MAX_PATH] = { 0 };
char CExtraMetadataMgr::m_shopPedOutfitLookup[RAGE_MAX_PATH] = { 0 };
char CExtraMetadataMgr::m_shopPedOutfitCounts[RAGE_MAX_PATH] = { 0 };
char CExtraMetadataMgr::m_proxyFromIndex[RAGE_MAX_PATH] = { 0 };
char CExtraMetadataMgr::m_proxyToIndex[RAGE_MAX_PATH] = { 0 };
bkCombo* CExtraMetadataMgr::ms_bankInteriorCombo;
s32 CExtraMetadataMgr::ms_dlcInteriorComboIdx = 0;
atArray<s32> CExtraMetadataMgr::ms_dlcInteriorMapSlots;
bkCombo* CExtraMetadataMgr::m_bankProxyIndicesMetaCombo = NULL;
bkList* CExtraMetadataMgr::m_bankProxyIndicesInFileList = NULL;
bkList* CExtraMetadataMgr::m_bankProxyIndicesInScratchList = NULL;
s32 CExtraMetadataMgr::ms_ProxyIndexComboIdx = 0;
atBinaryMap<const char*, u32> CExtraMetadataMgr::m_proxyOrderLookup;
s32 CExtraMetadataMgr::m_clickedProxyInFileHash = 0;
s32 CExtraMetadataMgr::m_clickedProxyInScratchHash = 0;
bkText* CExtraMetadataMgr::m_proxyFromIdxTxt = NULL;
bkText* CExtraMetadataMgr::m_proxyToIdxTxt = NULL;
#endif //__BANK && !__SPU

////////////////////////////////////////////////////////////////////////////////
// Data file loader interface
////////////////////////////////////////////////////////////////////////////////
class CExtraMetaDataFileMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile & file)
	{
        dlcDebugf3 ("CExtraMetaDataFileMounter::LoadDataFile: %s type = %d",file.m_filename, file.m_fileType);
        switch(file.m_fileType)
        {
            case CDataFileMgr::TATTOO_SHOP_DLC_FILE: EXTRAMETADATAMGR.AddTattooShopItemsCollection(file.m_filename);  break;
			case CDataFileMgr::WEAPON_SHOP_INFO_METADATA_FILE: EXTRAMETADATAMGR.AddWeaponShopItemsCollection(file.m_filename);  break;
			case CDataFileMgr::SHOP_PED_APPAREL_META_FILE: EXTRAMETADATAMGR.AddShopPedApparel(file.m_filename); break;
			case CDataFileMgr::VEHICLE_SHOP_DLC_FILE: EXTRAMETADATAMGR.AddVehiclesCollection(file.m_filename); break;
            default: return false;
        }
		return true;
    }

	virtual void UnloadDataFile(const CDataFileMgr::DataFile & file) 
	{
        dlcDebugf3 ("CExtraMetaDataFileMounter::UnloadDataFile %s type = %d",file.m_filename, file.m_fileType);
        switch(file.m_fileType)
        {
            case CDataFileMgr::TATTOO_SHOP_DLC_FILE: EXTRAMETADATAMGR.RemoveTattooShopItemsCollection(file.m_filename);  break;
            case CDataFileMgr::WEAPON_SHOP_INFO_METADATA_FILE: EXTRAMETADATAMGR.RemoveWeaponShopItemsCollection(file.m_filename);  break;
			case CDataFileMgr::SHOP_PED_APPAREL_META_FILE: EXTRAMETADATAMGR.RemoveShopPedApparel(file.m_filename); break;
			case CDataFileMgr::VEHICLE_SHOP_DLC_FILE: EXTRAMETADATAMGR.RemoveVehiclesCollection(file.m_filename); break;
            default: break;
        }

	}

} g_ExtraMetaDataFileMounter;

//////////////////////////////////////////////////////////////////////////
// CExtraMetadataMgr
//////////////////////////////////////////////////////////////////////////
CExtraMetadataMgr* CExtraMetadataMgr::smp_Instance = NULL;

CExtraMetadataMgr::CExtraMetadataMgr()
{
	m_tattooShopsItems.Reserve(TATTOO_COLLECTION_INITIAL_SIZE);

	for (int i=0; i < m_tattooShopItemsLookup.size(); ++i)
	{
		m_tattooShopItemsLookup[i].Reserve(TATTOO_ITEM_LOOKUP_INITIAL_SIZE);
	}

	atDelegate<void (u32, bool)> onTattooContentLockChangedCB;
	atDelegate<void (u32, bool)> onWeaponContentLockChangedCB;
	atDelegate<void (u32, bool)> onPedApparelContentLockChangedCB;
	atDelegate<void (u32, bool)> onVehicleContentLockChangedCB;

	onTattooContentLockChangedCB.Bind(this, &CExtraMetadataMgr::OnTattooContentLockChangedCB);
	m_contentRightData[CRT_TATTOOS].m_onLockChangedCBIdx = EXTRACONTENT.AddOnContentItemChangedCB(onTattooContentLockChangedCB);

	onWeaponContentLockChangedCB.Bind(this, &CExtraMetadataMgr::OnWeaponContentLockChangedCB);
	m_contentRightData[CRT_WEAPONS].m_onLockChangedCBIdx = EXTRACONTENT.AddOnContentItemChangedCB(onWeaponContentLockChangedCB);

	onPedApparelContentLockChangedCB.Bind(this, &CExtraMetadataMgr::OnPedApparelContentLockChangedCB);
	m_contentRightData[CRT_PED_APPAREL].m_onLockChangedCBIdx = EXTRACONTENT.AddOnContentItemChangedCB(onPedApparelContentLockChangedCB);

	onVehicleContentLockChangedCB.Bind(this, &CExtraMetadataMgr::OnVehicleContentLockChangedCB);
	m_contentRightData[CRT_VEHICLES].m_onLockChangedCBIdx = EXTRACONTENT.AddOnContentItemChangedCB(onVehicleContentLockChangedCB);
}

CExtraMetadataMgr::~CExtraMetadataMgr()
{
	// Clean up any arrays with pointers in...
	Reset();
}

void CExtraMetadataMgr::ClassInit(unsigned initMode)
{
	if (initMode == INIT_CORE && !smp_Instance)
	{
		smp_Instance = rage_new CExtraMetadataMgr();
		Assert(smp_Instance);
		CDataFileMount::RegisterMountInterface(CDataFileMgr::TATTOO_SHOP_DLC_FILE, &g_ExtraMetaDataFileMounter);
		CDataFileMount::RegisterMountInterface(CDataFileMgr::SHOP_PED_APPAREL_META_FILE, &g_ExtraMetaDataFileMounter,eDFMI_UnloadLast);
		CDataFileMount::RegisterMountInterface(CDataFileMgr::WEAPON_SHOP_INFO_METADATA_FILE, &g_ExtraMetaDataFileMounter);
	}
}

void CExtraMetadataMgr::ClassShutdown(unsigned shutdownMode)
{
	if (shutdownMode == SHUTDOWN_CORE)
	{
		delete smp_Instance;
		smp_Instance = NULL;
	}
	else if (shutdownMode == SHUTDOWN_SESSION)
		EXTRAMETADATAMGR.Reset();
}

void CExtraMetadataMgr::ShutdownDLCMetaFiles(u32 /*mode*/)
{
	if (smp_Instance)
		EXTRAMETADATAMGR.RemoveAllPedDLCMetaFiles();
}

// Cleans up the contents without deleting the manager itself
void CExtraMetadataMgr::Reset()
{
	for (u32 i = 0; i < m_tattooShopItemsLookup.size(); i++)
		m_tattooShopItemsLookup[i].Reset();

	m_tattooShopsItems.Reset();

	for (u32 i = 0; i < m_shopPedApparel.size(); i++)
		delete m_shopPedApparel[i];

	m_shopPedApparel.Reset();
	m_shopPedApparelLookup.Reset();
	m_shopPedApparelCompQuery.Reset();
	m_shopPedApparelPropQuery.Reset();
	m_shopPedApparelOutfitQuery.Reset();

	m_vehicles.Reset();
	m_vehiclesLookup.Reset();

	m_weaponShopItemsLookup.Reset();
	m_weaponShopItems.Reset();

	m_vehicleGenericModsLookup.Reset();

	memset(m_contentRightData, 0, sizeof(m_contentRightData));
}

// Content lock changed callbacks...
void CExtraMetadataMgr::OnTattooContentLockChangedCB(u32 /*hash*/, bool /*value*/)
{
	m_contentRightData[CRT_TATTOOS].m_rightsChanged = true;
}

void CExtraMetadataMgr::OnWeaponContentLockChangedCB(u32 /*hash*/, bool /*value*/)
{
	m_contentRightData[CRT_WEAPONS].m_rightsChanged = true;
}

void CExtraMetadataMgr::OnPedApparelContentLockChangedCB(u32 /*hash*/, bool /*value*/)
{
	m_contentRightData[CRT_PED_APPAREL].m_rightsChanged = true;
}

void CExtraMetadataMgr::OnVehicleContentLockChangedCB(u32 /*hash*/, bool /*value*/)
{
	m_contentRightData[CRT_VEHICLES].m_rightsChanged = true;
}

// VEHICLE Metadata helpers
void CExtraMetadataMgr::AddVehiclesCollection(const char* pFileName)
{
	if (ShopVehicleDataArray* vehiclesArray = AddMetaItem(m_vehicles, pFileName))
	{
		vehiclesArray->m_nameHash.SetFromString(pFileName);
		for(int i=0; i< vehiclesArray->m_Vehicles.GetCount(); ++i)
		{
			atHashString &parent = vehiclesArray->m_Vehicles[i].m_lockHash;
			atArray<ShopVehicleMod> &modsArray = vehiclesArray->m_Vehicles[i].m_vehicleMods;
			for(int j=0; j<modsArray.GetCount();j++)
			{
				if(modsArray[j].m_lockHash.GetHash() == 0)
					modsArray[j].m_lockHash = parent;
			}
		}

		BuildVehicleLookup();
	}
}

void CExtraMetadataMgr::RemoveVehiclesCollection(const char* pFilename)
{
	s32 index = FindMetaItemsCollectionIndex(m_vehicles, pFilename);
	
	if (index != -1)
	{
		for (u32 i = 0; i < m_vehicles[index].m_Vehicles.GetCount(); i++)
			EXTRACONTENT.RemoveContentLock(m_vehicles[index].m_Vehicles[i].m_lockHash);

		m_vehicles.Delete(index);
		BuildVehicleLookup();
	}
}

void CExtraMetadataMgr::BuildVehicleLookup()
{
	ShopVehicleDataArray* vehiclesArray = NULL;

	m_vehiclesLookup.ResetCount();
	m_vehicleGenericModsLookup.ResetCount();

	for (u32 vehCount = 0; vehCount < m_vehicles.GetCount(); vehCount++)
	{
		vehiclesArray = &m_vehicles[vehCount];

		if (vehiclesArray)
		{
			int l = vehiclesArray->m_Vehicles.GetCount();

			for (int i = 0 ; i < l ; i++)
			{
				ShopVehicleData & item = vehiclesArray->m_Vehicles[i];

				EXTRACONTENT.InsertContentLock(item.m_lockHash, m_contentRightData[CRT_VEHICLES].m_onLockChangedCBIdx, true);

				for (u32 vehModCount = 0; vehModCount < item.m_vehicleMods.GetCount(); vehModCount++)
					EXTRACONTENT.InsertContentLock(item.m_vehicleMods[vehModCount].m_lockHash, m_contentRightData[CRT_VEHICLES].m_onLockChangedCBIdx, true);

				m_vehiclesLookup.Grow() = &item;
			}
			l = vehiclesArray->m_VehicleMods.GetCount();

			for (int i = 0 ; i < l ; i++)
			{
				ShopVehicleMod & item = vehiclesArray->m_VehicleMods[i];

				EXTRACONTENT.InsertContentLock(item.m_lockHash, m_contentRightData[CRT_VEHICLES].m_onLockChangedCBIdx, true);
				m_vehicleGenericModsLookup.Grow() = &item;
			}
		}
	}
}

const ShopVehicleData* CExtraMetadataMgr::GetShopVehicleData(int idx) const
{
	if((u32)idx < (u32)(m_vehiclesLookup.GetCount()))
	{
		return m_vehiclesLookup[idx];
	}
	return NULL;
}

int CExtraMetadataMgr::GetShopVehicleModelNameHash(int idx) const
{
	const ShopVehicleData* vehicleData = GetShopVehicleData(idx);

	if(vehicleData)
		return vehicleData->m_modelNameHash;

	return 0;
}

int CExtraMetadataMgr::GetNumDLCVehicleMods(int dlcIdx) const
{
	if((u32)(dlcIdx) < (u32)(m_vehiclesLookup.GetCount()))
	{
		return m_vehiclesLookup[dlcIdx]->m_vehicleMods.GetCount();
	}	
	return -1;
}

const ShopVehicleMod* CExtraMetadataMgr::GetShopVehicleModData(int dlcIndex, int modIndex) const
{
	if((u32)(dlcIndex) < (u32)(m_vehiclesLookup.GetCount()))
	{
		if((u32)(modIndex) < (u32)(m_vehiclesLookup[dlcIndex]->m_vehicleMods.GetCount()))
		{
			return &(m_vehiclesLookup[dlcIndex]->m_vehicleMods[modIndex]);
		}
	}
	return NULL;
}

const atArray<ShopVehicleMod*>& CExtraMetadataMgr::GetGenericVehicleModList()
{
	return m_vehicleGenericModsLookup;
}

// WEAPON Metadata helpers
void CExtraMetadataMgr::AddWeaponShopItemsCollection(const char* pFileName)
{
    if (WeaponShopItemArray* weapons = AddMetaItem(m_weaponShopItems, pFileName))
	{
		for(int i=0; i< weapons->m_weaponShopItems.GetCount(); ++i)
		{
			atHashString &parent = weapons->m_weaponShopItems[i].m_lockHash;
			atArray<ShopWeaponComponent> &componentsArray = weapons->m_weaponShopItems[i].m_weaponComponents;
			for(int j=0; j<componentsArray.GetCount();j++)
			{
				if(componentsArray[j].m_lockHash.GetHash() == 0)
					componentsArray[j].m_lockHash = parent;
			}
		}
		BuildWeaponLookup();
	}
}

void CExtraMetadataMgr::RemoveWeaponShopItemsCollection(const char* pFileName)
{
	const int collectionIdx = FindMetaItemsCollectionIndex(m_weaponShopItems, pFileName);

	if (collectionIdx >= 0)
	{
		for (u32 i = 0 ; i < m_weaponShopItems[collectionIdx].m_weaponShopItems.GetCount() ; i++)
			EXTRACONTENT.RemoveContentLock(m_weaponShopItems[collectionIdx].m_weaponShopItems[i].m_lockHash);

		m_weaponShopItems.DeleteFast(collectionIdx);
	}

	BuildWeaponLookup();
}

void CExtraMetadataMgr::BuildWeaponLookup()
{
	m_weaponShopItemsLookup.ResetCount();

	for (u32 i = 0; i < m_weaponShopItems.GetCount(); i++)
	{
		for (u32 j = 0 ; j < m_weaponShopItems[i].m_weaponShopItems.GetCount() ; j++)
		{
			WeaponShopItem & item = m_weaponShopItems[i].m_weaponShopItems[j];

			EXTRACONTENT.InsertContentLock(item.m_lockHash, m_contentRightData[CRT_WEAPONS].m_onLockChangedCBIdx, true);

			for (u32 wepCompCount = 0; wepCompCount < item.m_weaponComponents.GetCount(); wepCompCount++)
				EXTRACONTENT.InsertContentLock(item.m_weaponComponents[wepCompCount].m_lockHash, m_contentRightData[CRT_VEHICLES].m_onLockChangedCBIdx, true);

			m_weaponShopItemsLookup.Grow() = &item;
		}
	}
}

int CExtraMetadataMgr::GetNumDLCWeaponsSP()const
{
	int iNumSPWeapons = 0;
	for (int i = 0; i < m_weaponShopItemsLookup.GetCount(); i++)
	{
		if (m_weaponShopItemsLookup[i]->m_availableInSP)
		{
			iNumSPWeapons++;
		}
	}
	return iNumSPWeapons;
}
s32 CExtraMetadataMgr::GetIndexForSPWeapons(int index) const
{
	u32 uNumOfSPWeaponsFound = 0;
	for (int i = 0; i < m_weaponShopItemsLookup.GetCount(); i++)
	{
		if (m_weaponShopItemsLookup[i]->m_availableInSP)
		{
			if (uNumOfSPWeaponsFound == (u32)index)
			{
				return i;
			}
			uNumOfSPWeaponsFound++;
		}
	}
	return -1;
}

const WeaponShopItem* CExtraMetadataMgr::GetDLCWeaponShopItem(int idx) const
{
	if((u32)(idx) < (u32)(m_weaponShopItemsLookup.GetCount()))
		return m_weaponShopItemsLookup[idx];
    return NULL;
}

const ShopWeaponComponent* CExtraMetadataMgr::GetShopWeaponComponentData(int dlcIdx, int componentIdx) const
{
	if((u32)(dlcIdx) < (u32)(m_weaponShopItemsLookup.GetCount()))
	{
		if((u32)(componentIdx) < (u32)(m_weaponShopItemsLookup[dlcIdx]->m_weaponComponents.GetCount()))
		{
			return &(m_weaponShopItemsLookup[dlcIdx]->m_weaponComponents[componentIdx]);
		}
	}
	return NULL;
}

const int CExtraMetadataMgr::GetNumDLCWeaponComponents(int idx) const
{
	if((u32)(idx) < (u32)(m_weaponShopItemsLookup.GetCount()))
	{
		return m_weaponShopItemsLookup[idx]->m_weaponComponents.GetCount();
	}
	return -1;
}

const int CExtraMetadataMgr::GetNumDLCWeaponComponentsSP(int idx) const
{
	if ((u32)(idx) < (u32)(m_weaponShopItemsLookup.GetCount()))
	{
		if (m_weaponShopItemsLookup[idx]->m_availableInSP)
		{
			return m_weaponShopItemsLookup[idx]->m_weaponComponents.GetCount();
		}
	}
	return -1;
}

//TATTOO Metadata helpers
void CExtraMetadataMgr::AddTattooShopItemsCollection(const char* pFileName)
{
	if (TattooShopItemArray* tattoos = AddMetaItem(m_tattooShopsItems, pFileName))
	{
		// If everything was alright, add new entries to the faction lookup. No need to rebuild in here: other indexes didn't get messed
		const int collectionIdx = m_tattooShopsItems.GetCount() - 1;

		for (int i=0; i < tattoos->TattooShopItems.GetCount(); ++i)
		{
			if( dlcVerifyf(tattoos->TattooShopItems[i].m_eFaction<m_tattooShopItemsLookup.size(), "CExtraMetadataMgr::AddTattooShopItemsCollection trying to add item for faction out of bounds. BAD DATA! (%s)", pFileName) )
			{
				tattooLookupEntry& appended = m_tattooShopItemsLookup[tattoos->TattooShopItems[i].m_eFaction].Grow(TATTOO_ITEM_LOOKUP_GROW);
				appended.collection = (u16)collectionIdx;
				appended.index = (u16)i;
			}

			EXTRACONTENT.InsertContentLock(tattoos->TattooShopItems[i].m_lockHash, m_contentRightData[CRT_TATTOOS].m_onLockChangedCBIdx, true);
		}

		// Also cache this collection hash
		tattoos->m_nameHash.SetFromString(pFileName);
	}
}

void CExtraMetadataMgr::RemoveTattooShopItemsCollection(const char* pFileName)
{
	const int collectionIdx = FindMetaItemsCollectionIndex(m_tattooShopsItems, pFileName);

	if( collectionIdx >= 0 )
	{
		for (u32 i = 0; i < m_tattooShopsItems[collectionIdx].TattooShopItems.GetCount(); i++)
			EXTRACONTENT.RemoveContentLock(m_tattooShopsItems[collectionIdx].TattooShopItems[i].m_lockHash);

		m_tattooShopsItems.Delete(collectionIdx);

		// Rebuild whole lookup table. This is not that expensive, even if happens N times in same frame because of removing multiple collections.
		// This also doesn't seem likely to get triggered: compatibility packs have all extra data and always need to be there. Maybe just on SP-only dlcs unmounting?
		BuildTattooShopItemsLookup();
		BANK_ONLY(TattooRefreshCB();)
	}
}

void CExtraMetadataMgr::BuildTattooShopItemsLookup()
{
	// Clean lookup tables first. Do not need to reclaim memory, just free their slots.
	for (int iFaction=0; iFaction < m_tattooShopItemsLookup.size(); ++iFaction)
	{
		m_tattooShopItemsLookup[iFaction].Resize(0);
	}

	for(int iColl=0; iColl<m_tattooShopsItems.GetCount(); ++iColl)
	{
		const TattooShopItemArray& shopItemArray = m_tattooShopsItems[iColl];
		for(int jItem=0; jItem<shopItemArray.TattooShopItems.GetCount(); ++jItem)
		{
			if( dlcVerifyf(shopItemArray.TattooShopItems[jItem].m_eFaction<m_tattooShopItemsLookup.size(), "CExtraMetadataMgr::BuildTattooShopLookup trying to add item for faction out of bounds. BAD DATA!") )
			{
				tattooLookupEntry& appended = m_tattooShopItemsLookup[shopItemArray.TattooShopItems[jItem].m_eFaction].Grow(TATTOO_ITEM_LOOKUP_GROW);
				appended.collection = (u16)iColl;
				appended.index = (u16)jItem;
			}
		}
	}
}

void CExtraMetadataMgr::FixupGlobalIndices(const CPedModelInfo* pedModelInfo, const CPedVariationInfo* newInfo, const CPedVariationInfoCollection* varInfoCollection)
{
	if (pedModelInfo && newInfo && varInfoCollection && newInfo->GetDlcNameHash() != 0)
	{
		for (u32 i = 0; i < m_shopPedApparel.GetCount(); i++)
		{
			ShopPedApparel& pedApparel = *m_shopPedApparel[i];
			u32 pedNameHash = atStringHash(pedApparel.m_pedName.c_str());
			u32 dlcNameHash = atStringHash(pedApparel.m_dlcName.c_str());

			if (pedNameHash == pedModelInfo->GetModelNameHash() && dlcNameHash == newInfo->GetDlcNameHash())
			{
				for (u32 j = 0; j < pedApparel.m_pedComponents.GetCount(); j++)
				{
					ShopPedComponent& item = pedApparel.m_pedComponents[j];
					u32 globalIndex = varInfoCollection->GetGlobalDrawableIndex(item.m_localDrawableIndex, (ePedVarComp)item.m_eCompType, dlcNameHash);

					dlcDebugf3("FixupGlobalIndices - Globalizing drawable index from %i -> %i [%s_%s]", item.m_localDrawableIndex, globalIndex, pedApparel.m_pedName.c_str(), pedApparel.m_dlcName.c_str());

					item.m_drawableIndex = globalIndex;
				}

				for (u32 j = 0; j < pedApparel.m_pedProps.GetCount(); j++)
				{
					ShopPedProp& item = pedApparel.m_pedProps[j];
					u32 globalIndex = varInfoCollection->GetGlobalPropIndex(item.m_localPropIndex, (eAnchorPoints)item.m_eAnchorPoint, dlcNameHash);

					dlcDebugf3("FixupGlobalIndices - Globalizing prop index from %i -> %i [%s_%s]", item.m_localPropIndex, globalIndex, pedApparel.m_pedName.c_str(), pedApparel.m_dlcName.c_str());

					Assertf(globalIndex < 255, "FixupGlobalIndices[Prop] - Global index is large and likely to cause issues");

					item.m_propIndex = (u8)globalIndex;
				}
			}
		}

	#if __BANK && !__SPU
		RefreshBankWidgets();
	#endif
	}
}

void CExtraMetadataMgr::RebuildApparelLookup()
{
	m_shopPedApparelLookup.ResetCount();

	for (u32 i = 0; i < m_shopPedApparel.GetCount(); i++)
	{
		ShopPedApparel& currApparelData = *m_shopPedApparel[i];

		for (u32 compIdx = 0; compIdx < currApparelData.m_pedComponents.GetCount(); compIdx++)
			m_shopPedApparelLookup.Insert(currApparelData.m_pedComponents[compIdx].m_uniqueNameHash, ShopPedApparelLookupEntry(SHOP_PED_COMPONENT, i, compIdx));

		for (u32 propIdx = 0; propIdx < currApparelData.m_pedProps.GetCount(); propIdx++)
			m_shopPedApparelLookup.Insert(currApparelData.m_pedProps[propIdx].m_uniqueNameHash, ShopPedApparelLookupEntry(SHOP_PED_PROP, i, propIdx));

		for (u32 outfitIdx = 0; outfitIdx < currApparelData.m_pedOutfits.GetCount(); outfitIdx++)
			m_shopPedApparelLookup.Insert(currApparelData.m_pedOutfits[outfitIdx].m_uniqueNameHash, ShopPedApparelLookupEntry(SHOP_PED_OUTFIT, i, outfitIdx));
	}

	m_shopPedApparelLookup.FinishInsertion();
}

void CExtraMetadataMgr::AddShopPedApparel(const char* pFilename)
{
	// Reset any current queries
	m_shopPedApparelCompQuery.ResetCount();
	m_shopPedApparelPropQuery.ResetCount();
	m_shopPedApparelOutfitQuery.ResetCount();

	if (ShopPedApparel* newShopApparel = AddMetaItemPtr<ShopPedApparel>(m_shopPedApparel, pFilename))
	{
		RebuildApparelLookup();

		// Insert content locks
		for (u32 i = 0; i < newShopApparel->m_pedComponents.GetCount(); i++)
			EXTRACONTENT.InsertContentLock(newShopApparel->m_pedComponents[i].m_lockHash, m_contentRightData[CRT_PED_APPAREL].m_onLockChangedCBIdx, true);			

		for (u32 i = 0; i < newShopApparel->m_pedProps.GetCount(); i++)
			EXTRACONTENT.InsertContentLock(newShopApparel->m_pedProps[i].m_lockHash, m_contentRightData[CRT_PED_APPAREL].m_onLockChangedCBIdx, true);

		for (u32 i = 0; i < newShopApparel->m_pedOutfits.GetCount(); i++)
			EXTRACONTENT.InsertContentLock(newShopApparel->m_pedOutfits[i].m_lockHash, m_contentRightData[CRT_PED_APPAREL].m_onLockChangedCBIdx, true);

		AddPedDLCMetaFiles(newShopApparel->m_pedName.c_str(), atStringHash(newShopApparel->m_dlcName), 
			newShopApparel->m_fullDlcName, newShopApparel->m_creatureMetaData);
	}

#if __BANK && !__SPU
	RefreshBankWidgets();
#endif
}

void CExtraMetadataMgr::RemoveShopPedApparel(const char* pFilename)
{
	// Reset any current queries
	m_shopPedApparelCompQuery.ResetCount();
	m_shopPedApparelPropQuery.ResetCount();
	m_shopPedApparelOutfitQuery.ResetCount();

	s32 itemIdx = FindMetaItemsCollectionIndexPtr(m_shopPedApparel, pFilename);

	if (itemIdx >= 0)
	{
		ShopPedApparel& shopApparelToRemove = *m_shopPedApparel[itemIdx];

		// Remove content locks
		for (u32 i = 0; i < shopApparelToRemove.m_pedComponents.GetCount(); i++)
			EXTRACONTENT.RemoveContentLock(shopApparelToRemove.m_pedComponents[i].m_lockHash);

		for (u32 i = 0; i < shopApparelToRemove.m_pedProps.GetCount(); i++)
			EXTRACONTENT.RemoveContentLock(shopApparelToRemove.m_pedProps[i].m_lockHash);

		for (u32 i = 0; i < shopApparelToRemove.m_pedOutfits.GetCount(); i++)
			EXTRACONTENT.RemoveContentLock(shopApparelToRemove.m_pedOutfits[i].m_lockHash);

		RemovePedDLCMetaFiles(shopApparelToRemove.m_pedName.c_str(), atStringHash(shopApparelToRemove.m_dlcName), 
			shopApparelToRemove.m_fullDlcName, shopApparelToRemove.m_creatureMetaData);

		delete m_shopPedApparel[itemIdx];
		m_shopPedApparel.DeleteFast(itemIdx);
		RebuildApparelLookup();
	}

#if __BANK && !__SPU
	RefreshBankWidgets();
#endif
}

SPedDLCMetaFiles* CExtraMetadataMgr::FindPedDLCMetaFiles(const char* pedName)
{
	for (u32 i = 0; i < m_pedDLCMetaFiles.GetCount(); i++)
	{
		if (m_pedDLCMetaFiles[i].m_pedName == pedName)
			return &m_pedDLCMetaFiles[i];
	}

	return NULL;
}

SPedDLCPackMetaFiles* CExtraMetadataMgr::FindPedDLCPackMetaFiles(SPedDLCMetaFiles* parentInfo, u32 dlcNameHash)
{
	if (parentInfo)
	{
		for (u32 i = 0; i < parentInfo->m_pedDLCPackMetaFiles.GetCount(); i++)
		{
			if (parentInfo->m_pedDLCPackMetaFiles[i].m_dlcNameHash == dlcNameHash)
				return &parentInfo->m_pedDLCPackMetaFiles[i];
		}
	}

	return NULL;
}

void CExtraMetadataMgr::AddPedDLCMetaFiles(const char* pedName, u32 dlcNameHash, u32 varInfoNameHash, u32 creatureMetaNameHash)
{
	fwModelId modelInfo;
	CPedModelInfo* pedModelInfo = (CPedModelInfo*)CModelInfo::GetBaseModelInfoFromName(pedName, &modelInfo);

	if (Verifyf(pedModelInfo && modelInfo.IsValid(), "CExtraMetadataMgr::AddPedDLCMetaFiles - Invalid model info! %s", pedName))
	{
		if (Verifyf(!IsModelInMemory(modelInfo), "CExtraMetadataMgr::AddPedDLCMetaFiles - Ped is already in memory! Ped Name: %s", pedName))
		{
			SPedDLCMetaFiles* dlcMetaFiles = FindPedDLCMetaFiles(pedName);
			SPedDLCPackMetaFiles* dlcPackMetaFiles = FindPedDLCPackMetaFiles(dlcMetaFiles, dlcNameHash);

			// Find or create the information about this DLC meta file for the given ped
			if (!dlcMetaFiles)
			{
				dlcMetaFiles = &m_pedDLCMetaFiles.Grow(1);
				dlcMetaFiles->Reset();
				dlcMetaFiles->m_pedName = pedName;
			}

			if (!dlcPackMetaFiles)
			{
				dlcPackMetaFiles = &dlcMetaFiles->m_pedDLCPackMetaFiles.Grow(1);
				dlcPackMetaFiles->Reset();
				dlcPackMetaFiles->m_dlcNameHash = dlcNameHash;
			}

			s32 varInfoIndex = g_fwMetaDataStore.FindSlotFromHashKey(varInfoNameHash).Get();
			if (varInfoIndex != -1)
			{
				pedModelInfo->SetPedMetaDataFile(varInfoIndex);
				dlcPackMetaFiles->m_varInfoIndices.PushAndGrow(varInfoIndex, 1);
			}

			s32 createMetaIndex = g_fwMetaDataStore.FindSlotFromHashKey(creatureMetaNameHash).Get();
			if (createMetaIndex != -1)
			{
				dlcPackMetaFiles->m_creatureIndices.PushAndGrow(createMetaIndex, 1);
			}
			strLocalIndex expressionSlot = g_ExpressionDictionaryStore.FindSlotFromHashKey(dlcNameHash);
			if(expressionSlot.IsValid())
			{
				strIndex index = g_ExpressionDictionaryStore.GetStreamingIndex(expressionSlot);
				strStreamingEngine::GetInfo().RequestObject(index,STRFLAG_DONTDELETE|STRFLAG_FORCE_LOAD);
			}
		}
	}
}

void CExtraMetadataMgr::RemovePedDLCMetaFiles(const char* pedName, u32 dlcNameHash, u32 varInfoNameHash, u32 creatureMetaNameHash)
{
	fwModelId modelInfo;
	CPedModelInfo* pedModelInfo = (CPedModelInfo*)CModelInfo::GetBaseModelInfoFromName(pedName, &modelInfo);
	strLocalIndex expressionSlot = g_ExpressionDictionaryStore.FindSlotFromHashKey(dlcNameHash);
	if(expressionSlot.IsValid())
	{
		strIndex index = g_ExpressionDictionaryStore.GetStreamingIndex(expressionSlot);
		strStreamingEngine::GetInfo().ClearRequiredFlag(index, STRFLAG_FORCE_LOAD|STRFLAG_DONTDELETE);
		strStreamingEngine::GetInfo().RemoveObject(index);
	}
	if (Verifyf(pedModelInfo && modelInfo.IsValid(), "CExtraMetadataMgr::AddPedDLCMetaFiles - Invalid model info! %s", pedName))
	{
		if (Verifyf(!IsModelInMemory(modelInfo), "CExtraMetadataMgr::RemovePedDLCMetaFiles - Ped is already in memory! Ped Name: %s", pedName))
		{
			SPedDLCMetaFiles* dlcMetaFiles = FindPedDLCMetaFiles(pedName);
			SPedDLCPackMetaFiles* dlcPackMetaFiles = FindPedDLCPackMetaFiles(dlcMetaFiles, dlcNameHash);

			if (Verifyf(dlcPackMetaFiles, "CExtraMetadataMgr::RemovePedDLCMetaFiles - Could not find DLC pack meta files! %s", pedName))
			{
				s32 varInfoIndex = g_fwMetaDataStore.FindSlotFromHashKey(varInfoNameHash).Get();
				if (varInfoIndex != -1)
				{
					pedModelInfo->RemovePedMetaDataFile(varInfoIndex);
					dlcPackMetaFiles->m_varInfoIndices.DeleteMatches(varInfoIndex);
				}

				s32 createMetaIndex = g_fwMetaDataStore.FindSlotFromHashKey(creatureMetaNameHash).Get();
				if (createMetaIndex != -1)
				{
					dlcPackMetaFiles->m_creatureIndices.DeleteMatches(createMetaIndex);
				}
			}
		}
	}
}

bool CExtraMetadataMgr::IsModelInMemory(fwModelId modelInfo)
{
	return (CModelInfo::AreAssetsLoading(modelInfo) || CModelInfo::AreAssetsRequested(modelInfo) || CModelInfo::HaveAssetsLoaded(modelInfo));
}

void CExtraMetadataMgr::RemoveAllPedDLCMetaFiles()
{
	fwModelId modelInfo;
	CPedModelInfo* pedModelInfo = NULL;

	for (u32 i = 0; i < m_pedDLCMetaFiles.GetCount(); i++)
	{
		SPedDLCMetaFiles& currPedDLCMetaFiles = m_pedDLCMetaFiles[i];
		pedModelInfo = (CPedModelInfo*)CModelInfo::GetBaseModelInfoFromName(currPedDLCMetaFiles.m_pedName.c_str(), &modelInfo);

		if (pedModelInfo && modelInfo.IsValid())
		{
			if (Verifyf(!IsModelInMemory(modelInfo), "CExtraMetadataMgr::RemoveAllPedDLCMetaFiles - Ped is already in memory! Ped Name: %s", currPedDLCMetaFiles.m_pedName.c_str()))
			{
				for (u32 j = 0; j < currPedDLCMetaFiles.m_pedDLCPackMetaFiles.GetCount(); j++)
				{
					SPedDLCPackMetaFiles& currPedDLCPackMetaFiles = currPedDLCMetaFiles.m_pedDLCPackMetaFiles[j];

					for (u32 k = 0; k < currPedDLCPackMetaFiles.m_varInfoIndices.GetCount(); k++)
					{
						pedModelInfo->RemovePedMetaDataFile(currPedDLCPackMetaFiles.m_varInfoIndices[k]);
						currPedDLCPackMetaFiles.m_varInfoIndices.DeleteMatches(currPedDLCPackMetaFiles.m_varInfoIndices[k]);
					}

					for (u32 k = 0; k < currPedDLCPackMetaFiles.m_creatureIndices.GetCount(); k++)
						currPedDLCPackMetaFiles.m_creatureIndices.DeleteMatches(currPedDLCPackMetaFiles.m_creatureIndices[k]);
				}
			}
		}
	}

	m_pedDLCMetaFiles.Reset();
}

void CExtraMetadataMgr::GetCreatureMetaDataIndices(CPedModelInfo* lookupModelInfo, atFixedArray<SPedDLCMetaFileQueryData, STREAMING_MAX_DEPENDENCIES>& targetArray)
{
	fwModelId modelInfo;
	CPedModelInfo* currPedModelInfo = NULL;
	s32 diskStoreIndex = lookupModelInfo->GetCreatureMetadataFileIndex().Get();

	targetArray.Reset();

	if (diskStoreIndex >= 0)
	{
		SPedDLCMetaFileQueryData& diskData = targetArray.Append();

		diskData.m_dlcNameHash = 0;
		diskData.m_storeIndex = diskStoreIndex;
	}

	for (u32 i = 0; i < m_pedDLCMetaFiles.GetCount(); i++)
	{
		SPedDLCMetaFiles& currDLCMetaFiles = m_pedDLCMetaFiles[i];

		currPedModelInfo = (CPedModelInfo*)CModelInfo::GetBaseModelInfoFromName(currDLCMetaFiles.m_pedName.c_str(), &modelInfo);

		if (currPedModelInfo == lookupModelInfo)
		{
			for (u32 j = 0; j < currDLCMetaFiles.m_pedDLCPackMetaFiles.GetCount(); j++)
			{
				SPedDLCPackMetaFiles& currDLCPackMetaFiles = currDLCMetaFiles.m_pedDLCPackMetaFiles[j];

				for (u32 k = 0; k < currDLCPackMetaFiles.m_creatureIndices.GetCount(); k++)
				{
					SPedDLCMetaFileQueryData& dlcData = targetArray.Append();

					dlcData.m_dlcNameHash = currDLCPackMetaFiles.m_dlcNameHash;
					dlcData.m_storeIndex = currDLCPackMetaFiles.m_creatureIndices[k];
				}
			}
		}
	}
}

u32 CExtraMetadataMgr::GetHashNameForComponent(s32 pedIndex, ePedVarComp componentType, u32 drawableIndex, u32 textureIndex)
{
	if (const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(pedIndex))
	{
		if (CPedModelInfo* pedModelInfo = pPed->GetPedModelInfo())
		{
			if (CPedVariationInfoCollection* pedVarInfoCollection = pedModelInfo->GetVarInfo())
			{
				if (const char* dlcName = pedVarInfoCollection->GetDlcNameFromDrawableIdx(componentType, drawableIndex))
				{
					for (u32 i = 0; i < m_shopPedApparel.GetCount(); i++)
					{
						ShopPedApparel& currPedApparel = *m_shopPedApparel[i];

						// Try to find the shop data with this DLC name
						if (stricmp(dlcName, currPedApparel.m_dlcName.c_str()) == 0)
						{
							// Try to find the component in the shop data with the specified drawable/texture indices
							for (u32 j = 0; j < currPedApparel.m_pedComponents.GetCount(); j++)
							{
								ShopPedComponent& currComponent = currPedApparel.m_pedComponents[j];

								if (currComponent.m_drawableIndex == drawableIndex && 
									currComponent.m_textureIndex == textureIndex && 
									currComponent.m_eCompType == componentType)
									return currComponent.m_uniqueNameHash;
							}
						}
					}
				}				
			}
		}
	}

	return 0;
}

u32 CExtraMetadataMgr::GetHashNameForProp(s32 pedIndex, eAnchorPoints anchorPoint, u32 propIndex, u32 textureIndex)
{
	if (const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(pedIndex))
	{
		return GetHashNameForProp(pPed, anchorPoint, propIndex, textureIndex);
	}

	return 0;
}

u32 CExtraMetadataMgr::GetHashNameForProp(const CPed* pPed, eAnchorPoints anchorPoint, u32 propIndex, u32 textureIndex)
{
	if (pPed)
	{
		if (CPedModelInfo* pedModelInfo = pPed->GetPedModelInfo())
		{
			if (CPedVariationInfoCollection* pedVarInfoCollection = pedModelInfo->GetVarInfo())
			{
				if (const char* dlcName = pedVarInfoCollection->GetDlcNameFromPropIdx(anchorPoint, propIndex))
				{
					for (u32 i = 0; i < m_shopPedApparel.GetCount(); i++)
					{
						ShopPedApparel& currPedApparel = *m_shopPedApparel[i];

						// Try to find the shop data with this DLC name
						if (stricmp(dlcName, currPedApparel.m_dlcName.c_str()) == 0)
						{
							// Try to find the component in the shop data with the specified prop/texture indices
							for (u32 j = 0; j < currPedApparel.m_pedProps.GetCount(); j++)
							{
								ShopPedProp& currProp = currPedApparel.m_pedProps[j];

								if (currProp.m_propIndex == propIndex && 
									currProp.m_textureIndex == textureIndex &&
									currProp.m_eAnchorPoint == anchorPoint)
									return currProp.m_uniqueNameHash;
							}
						}
					}
				}				
			}
		}
	}

	return 0;
}

s32 CExtraMetadataMgr::GetShopPedApparelVariantPropsCount(u32 nameHash)
{
	BaseShopPedApparel* baseShopItem = GetShopPedApparel(nameHash);

	if (baseShopItem)
		return baseShopItem->m_variantProps.GetCount();

	return 0;
}

s32 CExtraMetadataMgr::GetShopPedApparelVariantComponentsCount(u32 nameHash)
{
	BaseShopPedApparel* baseShopItem = GetShopPedApparel(nameHash);

	if (baseShopItem)
		return baseShopItem->m_variantComponents.GetCount();

	return 0;
}

void CExtraMetadataMgr::GetVariantProp(u32 nameHash, u32 variantPropIndex, s32& variantPropNameHash, s32& variantPropEnumValue, s32& variantPropAnchor)
{
	BaseShopPedApparel* baseShopItem = GetShopPedApparel(nameHash);

	if (baseShopItem)
	{
		if (Verifyf(variantPropIndex < baseShopItem->m_variantProps.GetCount(), 
			"CExtraMetadataMgr::GetVariantProp - Invalid index supplied! %u/%u", variantPropIndex, baseShopItem->m_variantProps.GetCount()))
		{
			variantPropNameHash = baseShopItem->m_variantProps[variantPropIndex].m_nameHash;
			variantPropEnumValue = (s32)baseShopItem->m_variantProps[variantPropIndex].m_enumValue;
			variantPropAnchor = (s32)baseShopItem->m_variantProps[variantPropIndex].m_eAnchorPoint;
		}
	}
}

void CExtraMetadataMgr::GetVariantComponent(u32 nameHash, u32 variantComponentIndex, s32& variantComponentNameHash, s32& variantComponentEnumValue, s32& variantComponentType)
{
	BaseShopPedApparel* baseShopItem = GetShopPedApparel(nameHash);

	if (baseShopItem)
	{
		if (Verifyf(variantComponentIndex < baseShopItem->m_variantComponents.GetCount(), 
			"CExtraMetadataMgr::GetVariantComponent - Invalid index supplied! %u/%u", variantComponentIndex, baseShopItem->m_variantComponents.GetCount()))
		{
			variantComponentNameHash = baseShopItem->m_variantComponents[variantComponentIndex].m_nameHash;
			variantComponentEnumValue = (s32)baseShopItem->m_variantComponents[variantComponentIndex].m_enumValue;
			variantComponentType = (s32)baseShopItem->m_variantComponents[variantComponentIndex].m_eCompType;
		}
	}
}

s32 CExtraMetadataMgr::GetShopPedApparelForcedPropsCount(u32 nameHash)
{
	BaseShopPedApparel* baseShopItem = GetShopPedApparel(nameHash);

	if (baseShopItem)
		return baseShopItem->m_forcedProps.GetCount();

	return 0;
}

s32 CExtraMetadataMgr::GetShopPedApparelForcedComponentsCount(u32 nameHash)
{
	BaseShopPedApparel* baseShopItem = GetShopPedApparel(nameHash);

	if (baseShopItem)
		return baseShopItem->m_forcedComponents.GetCount();

	return 0;
}

void CExtraMetadataMgr::GetForcedProp(u32 nameHash, u32 forcedPropIndex, s32& forcedPropNameHash, s32& forcedPropEnumValue, s32& forcedPropAnchor)
{
	BaseShopPedApparel* baseShopItem = GetShopPedApparel(nameHash);

	if (baseShopItem)
	{
		if (Verifyf(forcedPropIndex < baseShopItem->m_forcedProps.GetCount(), 
			"CExtraMetadataMgr::GetForcedProp - Invalid index supplied! %u/%u", forcedPropIndex, baseShopItem->m_forcedProps.GetCount()))
		{
			forcedPropNameHash = baseShopItem->m_forcedProps[forcedPropIndex].m_nameHash;
			forcedPropEnumValue = (s32)baseShopItem->m_forcedProps[forcedPropIndex].m_enumValue;
			forcedPropAnchor = (s32)baseShopItem->m_forcedProps[forcedPropIndex].m_eAnchorPoint;
		}
	}
}

void CExtraMetadataMgr::GetForcedComponent(u32 nameHash, u32 forcedComponentIndex, s32& forcedComponentNameHash, s32& forcedComponentEnumValue, s32& forcedComponentType)
{
	BaseShopPedApparel* baseShopItem = GetShopPedApparel(nameHash);

	if (baseShopItem)
	{
		if (Verifyf(forcedComponentIndex < baseShopItem->m_forcedComponents.GetCount(), 
			"CExtraMetadataMgr::GetForcedComponent - Invalid index supplied! %u/%u", forcedComponentIndex, baseShopItem->m_forcedComponents.GetCount()))
		{
			forcedComponentNameHash = baseShopItem->m_forcedComponents[forcedComponentIndex].m_nameHash;
			forcedComponentEnumValue = (s32)baseShopItem->m_forcedComponents[forcedComponentIndex].m_enumValue;
			forcedComponentType = (s32)baseShopItem->m_forcedComponents[forcedComponentIndex].m_eCompType;
		}
	}
}

bool CExtraMetadataMgr::DoesShopPedApparelHaveRestrictionTag(u32 nameHash, u32 tagHash)
{
	BaseShopPedApparel* baseShopItem = GetShopPedApparel(nameHash);

	if (baseShopItem)
	{
		for (int i = 0; i < baseShopItem->m_restrictionTags.GetCount(); i++)
		{
			if (baseShopItem->m_restrictionTags[i].m_tagNameHash == tagHash)
				return true;
		}
	}

	return false;
}

s32 CExtraMetadataMgr::SetupShopPedComponentQuery(eScrCharacters character, eShopEnum shop, s32 locate, ePedVarComp componentType)
{
	m_shopPedApparelCompQuery.ResetCount();

	for (u32 i = 0; i < m_shopPedApparel.GetCount(); i++)
	{
		ShopPedApparel& currPedApparelArray = *m_shopPedApparel[i];
		
		if (character == SCR_CHAR_ANY || character == currPedApparelArray.m_eCharacter)
		{
			for (u32 j = 0; j < currPedApparelArray.m_pedComponents.GetCount(); j++)
			{
				ShopPedComponent* currentItem = &currPedApparelArray.m_pedComponents[j];

				if ((shop == CLO_SHOP_NONE || shop == currentItem->m_eShopEnum) && 
					(locate == LOC_ANY || locate == currentItem->m_locate) &&
					(componentType == PV_COMP_INVALID || componentType == currentItem->m_eCompType))
					m_shopPedApparelCompQuery.PushAndGrow(ShopPedApparelLookupEntry(SHOP_PED_COMPONENT, i, j), 1);
			}
		}
	}

	return m_shopPedApparelCompQuery.GetCount();
}

s32 CExtraMetadataMgr::SetupShopPedPropQuery(eScrCharacters character, eShopEnum shop, s32 locate, eAnchorPoints anchorPoint)
{
	m_shopPedApparelPropQuery.ResetCount();

	for (u32 i = 0; i < m_shopPedApparel.GetCount(); i++)
	{
		ShopPedApparel& currPedApparelArray = *m_shopPedApparel[i];

		if (character == SCR_CHAR_ANY || character == currPedApparelArray.m_eCharacter)
		{
			for (u32 j = 0; j < currPedApparelArray.m_pedProps.GetCount(); j++)
			{
				ShopPedProp* currentItem = &currPedApparelArray.m_pedProps[j];

				if ((shop == CLO_SHOP_NONE || shop == currentItem->m_eShopEnum) && 
					(locate == LOC_ANY || locate == currentItem->m_locate) &&
					(anchorPoint == ANCHOR_NONE || anchorPoint == currentItem->m_eAnchorPoint))
					m_shopPedApparelPropQuery.PushAndGrow(ShopPedApparelLookupEntry(SHOP_PED_PROP, i, j), 1);
			}
		}
	}

	return m_shopPedApparelPropQuery.GetCount();
}

s32 CExtraMetadataMgr::SetupShopPedOutfitQuery(eScrCharacters character, eShopEnum shop, s32 locate)
{
	m_shopPedApparelOutfitQuery.ResetCount();

	for (u32 i = 0; i < m_shopPedApparel.GetCount(); i++)
	{
		ShopPedApparel& currPedApparelArray = *m_shopPedApparel[i];

		if (character == SCR_CHAR_ANY || character == currPedApparelArray.m_eCharacter)
		{
			for (u32 j = 0; j < currPedApparelArray.m_pedOutfits.GetCount(); j++)
			{
				ShopPedOutfit* currentItem = &currPedApparelArray.m_pedOutfits[j];

				if ((shop == CLO_SHOP_NONE || shop == currentItem->m_eShopEnum) && 
					(locate == LOC_ANY || locate == currentItem->m_locate))
					m_shopPedApparelOutfitQuery.PushAndGrow(ShopPedApparelLookupEntry(SHOP_PED_OUTFIT, i, j), 1);
			}
		}
	}

	return m_shopPedApparelOutfitQuery.GetCount();
}

#if __EXTRA_METADATA_CONTAINS_SCRIPT_SAVE_DATA_FLAGS
void CExtraMetadataMgr::SetShopPedApparelScriptData(u32 nameHash, u8 bitFlags)
{
	BaseShopPedApparel* item = GetShopPedApparel(nameHash);

	if (item)
		item->m_scriptSaveData = bitFlags;
}
#endif	//	__EXTRA_METADATA_CONTAINS_SCRIPT_SAVE_DATA_FLAGS

ShopPedComponent* CExtraMetadataMgr::GetShopPedQueryComponent(u32 index)
{
	if (index < m_shopPedApparelCompQuery.GetCount())
	{
		ShopPedApparelLookupEntry& luEntry = m_shopPedApparelCompQuery[index];

		return &m_shopPedApparel[luEntry.m_shopPedApparelIndex]->m_pedComponents[luEntry.m_shopPedTypeIndex];
	}

	return NULL;
}

ShopPedProp* CExtraMetadataMgr::GetShopPedQueryProp(u32 index)
{
	if (index < m_shopPedApparelPropQuery.GetCount())
	{
		ShopPedApparelLookupEntry& luEntry = m_shopPedApparelPropQuery[index];

		return &m_shopPedApparel[luEntry.m_shopPedApparelIndex]->m_pedProps[luEntry.m_shopPedTypeIndex];
	}

	return NULL;
}

ShopPedOutfit* CExtraMetadataMgr::GetShopPedQueryOutfit(u32 index)
{
	if (index < m_shopPedApparelOutfitQuery.GetCount())
	{
		ShopPedApparelLookupEntry& luEntry = m_shopPedApparelOutfitQuery[index];

		return &m_shopPedApparel[luEntry.m_shopPedApparelIndex]->m_pedOutfits[luEntry.m_shopPedTypeIndex];
	}

	return NULL;
}

s32 CExtraMetadataMgr::GetShopPedQueryComponentIndex(u32 nameHash)
{
	const s32 numberOfQueryResults = m_shopPedApparelCompQuery.GetCount();
	for (s32 loop = 0; loop < numberOfQueryResults; loop++)
	{
		ShopPedApparelLookupEntry& luEntry = m_shopPedApparelCompQuery[loop];
		if (m_shopPedApparel[luEntry.m_shopPedApparelIndex]->m_pedComponents[luEntry.m_shopPedTypeIndex].m_uniqueNameHash.GetHash() == nameHash)
		{
			return loop;
		}
	}

	return -1;
}

s32 CExtraMetadataMgr::GetShopPedQueryPropIndex(u32 nameHash)
{
	const s32 numberOfQueryResults = m_shopPedApparelPropQuery.GetCount();
	for (s32 loop = 0; loop < numberOfQueryResults; loop++)
	{
		ShopPedApparelLookupEntry& luEntry = m_shopPedApparelPropQuery[loop];
		if (m_shopPedApparel[luEntry.m_shopPedApparelIndex]->m_pedProps[luEntry.m_shopPedTypeIndex].m_uniqueNameHash.GetHash() == nameHash)
		{
			return loop;
		}
	}

	return -1;
}


BaseShopPedApparel* CExtraMetadataMgr::GetShopPedApparel(u32 nameHash)
{
	if (ShopPedApparelLookupEntry* luEntry = m_shopPedApparelLookup.SafeGet(nameHash))
	{
		if (luEntry->m_apparelType == (s8)SHOP_PED_COMPONENT)
		{
			return (BaseShopPedApparel*)&m_shopPedApparel[luEntry->m_shopPedApparelIndex]->m_pedComponents[luEntry->m_shopPedTypeIndex];
		}
		else if (luEntry->m_apparelType == (s8)SHOP_PED_PROP)
		{
			return (BaseShopPedApparel*)&m_shopPedApparel[luEntry->m_shopPedApparelIndex]->m_pedProps[luEntry->m_shopPedTypeIndex];
		}
		else if (luEntry->m_apparelType == (s8)SHOP_PED_OUTFIT)
		{
			return (BaseShopPedApparel*)&m_shopPedApparel[luEntry->m_shopPedApparelIndex]->m_pedOutfits[luEntry->m_shopPedTypeIndex];
		}
	}

	return NULL;
}

#if __EXTRA_METADATA_CONTAINS_SCRIPT_SAVE_DATA_FLAGS
void CExtraMetadataMgr::SetBitShopPedApparel(int nameHash, int bitFlag)
{
	BaseShopPedApparel* baseShopItem = GetShopPedApparel(nameHash);
	
	if (baseShopItem)
		baseShopItem->m_scriptSaveData |= (1 << bitFlag);
}

bool CExtraMetadataMgr::IsBitSetShopPedApparel(int nameHash, int bitFlag)
{
	BaseShopPedApparel* baseShopItem = GetShopPedApparel(nameHash);
	
	if (baseShopItem)
		return (baseShopItem) ? (((baseShopItem->m_scriptSaveData >> bitFlag) & 1) != 0) : false;

	return false;
}

void CExtraMetadataMgr::ClearBitShopPedApparel(int nameHash, int bitFlag)
{
	BaseShopPedApparel* baseShopItem = GetShopPedApparel(nameHash);
	
	if (baseShopItem)
		baseShopItem->m_scriptSaveData &= ~(1 << bitFlag);
}
#endif	//	__EXTRA_METADATA_CONTAINS_SCRIPT_SAVE_DATA_FLAGS

ShopPedComponent* CExtraMetadataMgr::GetShopPedComponent(u32 nameHash)
{
	if (ShopPedApparelLookupEntry* luEntry = m_shopPedApparelLookup.SafeGet(nameHash))
	{
		if (luEntry->m_apparelType == (s8)SHOP_PED_COMPONENT)
		{
			return &m_shopPedApparel[luEntry->m_shopPedApparelIndex]->m_pedComponents[luEntry->m_shopPedTypeIndex];
		}
	}

	return NULL;
}

ShopPedProp* CExtraMetadataMgr::GetShopPedProp(u32 nameHash)
{
	if (ShopPedApparelLookupEntry* luEntry = m_shopPedApparelLookup.SafeGet(nameHash))
	{
		if (luEntry->m_apparelType == (s8)SHOP_PED_PROP)
		{
			return &m_shopPedApparel[luEntry->m_shopPedApparelIndex]->m_pedProps[luEntry->m_shopPedTypeIndex];
		}
	}

	return NULL;
}

ShopPedOutfit* CExtraMetadataMgr::GetShopPedOutfit(u32 nameHash)
{
	if (ShopPedApparelLookupEntry* luEntry = m_shopPedApparelLookup.SafeGet(nameHash))
	{
		if (luEntry->m_apparelType == (s8)SHOP_PED_OUTFIT)
		{
			return &m_shopPedApparel[luEntry->m_shopPedApparelIndex]->m_pedOutfits[luEntry->m_shopPedTypeIndex];
		}
	}

	return NULL;
}

eScrCharacters CExtraMetadataMgr::GetShopPedApparelCharacter(u32 nameHash)
{
	if (ShopPedApparelLookupEntry* luEntry = m_shopPedApparelLookup.SafeGet(nameHash))
		return m_shopPedApparel[luEntry->m_shopPedApparelIndex]->m_eCharacter;

	return SCR_CHAR_ANY;
}

#if __BANK && !__SPU
void CExtraMetadataMgr::CreateBankWidgets(bkBank& bank)
{
	bank.PushGroup("Extra metadata manager widgets", false);
	{
		EXTRAMETADATAMGR.SetupShopTattooWidgets(bank);
		EXTRAMETADATAMGR.SetupShopPedApparelWidgets(bank);
		EXTRAMETADATAMGR.SetupShopWeaponWidgets(bank);
		EXTRAMETADATAMGR.SetupShopVehicleWidgets(bank);
		EXTRAMETADATAMGR.SetupInteriorWidgets(bank);
	}	
	bank.PopGroup();
}

void CExtraMetadataMgr::RefreshBankWidgets()
{
	TattooRefreshCB();
	VehicleRefreshCB();
	WeaponRefreshCB();
	InteriorRefreshCB();
	DisplayPedComponents(m_currentComponentPg, 0, false);
	DisplayPedProps(m_currentPropPg, 0, false);
	DisplayPedOutfits(m_currentOutfitPg, 0, false);
}

void CExtraMetadataMgr::TattooRefreshCB()
{
	if(m_bankTattooList)
	{
		for( int i= 0; i<m_sNumOfListedTattoos; i++)
		{
			m_bankTattooList->RemoveItem(i);
		}
		DisplayTattoos(0,0);
	}
}

void CExtraMetadataMgr::WeaponRefreshCB()
{
	if(m_bankWeaponCombo && m_bankWeaponComponentsList)
	{
		PopulateWeaponNames();
		if(m_weaponNames.GetCount()>0)
		{
			m_bankWeaponCombo->UpdateCombo("Weapon Name", &m_sWeaponComboIdx,m_weaponNames.GetCount(), &m_weaponNames[0],datCallback(MFA(CExtraMetadataMgr::WeaponComboCB),(datBase*)this));
		}
		else
		{
			m_sWeaponComboIdx = 0;
			m_bankWeaponCombo->UpdateCombo("Weapon Name", &m_sWeaponComboIdx, 1, NULL);
			m_bankWeaponCombo->SetString(0,"No DLC weapons loaded, hit refresh upon loading");
		}

		WeaponComboCB();
	}	
}

void CExtraMetadataMgr::VehicleRefreshCB()
{
	if(m_bankVehicleCombo && m_bankVehicleModList && m_bankGenericVehicleModList)
	{
		PopulateVehicleNames();
		if(m_vehicleNames.GetCount()>0)
		{
			m_bankVehicleCombo->UpdateCombo("Vehicle Name", &m_sVehicleComboIdx,m_vehicleNames.GetCount(), &m_vehicleNames[0],datCallback(MFA(CExtraMetadataMgr::VehicleComboCB),(datBase*)this));
		}
		else
		{
			m_sVehicleComboIdx = 0;
			m_bankVehicleCombo->UpdateCombo("Vehicle Name", &m_sVehicleComboIdx, 1, NULL);
			m_bankVehicleCombo->SetString(0,"No DLC vehicles loaded, hit refresh upon loading");
		}

		VehicleComboCB();
	}	
}



void CExtraMetadataMgr::WeaponEquipCB()
{
	dlcDebugf3("Attempting to equip weapon at index %d ", m_sWeaponComboIdx);
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if(pPlayer && m_sWeaponComboIdx>=0 && m_sWeaponComboIdx < m_weaponShopItemsLookup.GetCount())
	{
		const WeaponShopItem* currentItem = m_weaponShopItemsLookup[m_sWeaponComboIdx];
		dlcDebugf3("Equipping weapon :%p", m_weaponShopItemsLookup[m_sWeaponComboIdx]);
		pPlayer->GetInventory()->AddWeaponAndAmmo(currentItem->m_nameHash, 100);
		pPlayer->GetWeaponManager()->EquipWeapon(currentItem->m_nameHash,true);
	}	
}

void CExtraMetadataMgr::TattooDblClickCB(s32 idx)
{
	extern const char* parser_eTattooFaction_Strings[];
	extern const char* parser_eTattooFacing_Strings[];
	int ct=0;
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	TattooShopItem* currentItem = NULL;
	for(int i=0;i<m_tattooShopItemsLookup.GetMaxCount();i++)
	{
		for(int j= 0; j<m_tattooShopItemsLookup[i].GetCount(); j++)
		{
			if(ct==idx)
			{
				const tattooLookupEntry* entry = &(m_tattooShopItemsLookup[i][j]);
				currentItem = &(m_tattooShopsItems[entry->collection].TattooShopItems[entry->index]);
				break;
			}
			ct++;			
		}
	}
	dlcDebugf3("Tattoo idx: %d clicked, faction: %s , id= %d   ",idx,parser_eTattooFaction_Strings[currentItem->m_eFaction],currentItem->m_id);
	PEDDECORATIONMGR.AddPedDecoration(pPlayer,currentItem->m_collection,currentItem->m_preset);
}
void CExtraMetadataMgr::TattooClearCB()
{
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	PEDDECORATIONMGR.ClearPedDecorations(pPlayer);
}

void CExtraMetadataMgr::VehicleCreateCB()
{
	if(m_vehiclesLookup.GetCount()>0)
	{
		fwModelId test;
		CModelInfo::GetBaseModelInfoFromName(m_vehiclesLookup[m_sVehicleComboIdx]->m_modelNameHash.GetCStr(),&test);
		CVehicleFactory::CreateCar(test.GetModelIndex());
	}
}

void CExtraMetadataMgr::VehicleComboCB()
{
	for(int i = 0; i<m_sNumOfListedMods;i++)
	{
		m_bankVehicleModList->RemoveItem(i);
	}
	for(int i = 0; i<m_sNumOfListedGenericMods;i++)
	{
		m_bankGenericVehicleModList->RemoveItem(i);
	}
	
	if(m_vehiclesLookup.GetCount()>0)
	if(m_vehiclesLookup[m_sVehicleComboIdx])
	{
		m_sNumOfListedMods = m_vehiclesLookup[m_sVehicleComboIdx]->m_vehicleMods.GetCount();
		for(int i=0;i<m_vehiclesLookup[m_sVehicleComboIdx]->m_vehicleMods.GetCount();++i)
		{
			const ShopVehicleMod* currentItem = &(m_vehiclesLookup[m_sVehicleComboIdx]->m_vehicleMods[i]);
			if(currentItem)
			{	
				m_bankVehicleModList->AddItem(i,0,currentItem->m_nameHash.GetCStr());//Hash
				m_bankVehicleModList->AddItem(i,1,currentItem->m_cost);//Cost
				m_bankVehicleModList->AddItem(i,2,m_vehiclesLookup[m_sVehicleComboIdx]->m_lockHash.TryGetCStr());//lock hash
				m_bankVehicleModList->AddItem(i,3,currentItem->m_lockHash.TryGetCStr());//lock hash
			}
		}
	}
	if(m_vehicleGenericModsLookup.GetCount()>0)
	{
		m_sNumOfListedGenericMods= m_vehicleGenericModsLookup.GetCount();
		for(int i=0;i<m_vehicleGenericModsLookup.GetCount();++i)
		{
			const ShopVehicleMod* currentItem = (m_vehicleGenericModsLookup[i]);
			if(currentItem)
			{	
				m_bankGenericVehicleModList->AddItem(i,0,currentItem->m_nameHash.GetCStr());//Hash
				m_bankGenericVehicleModList->AddItem(i,1,currentItem->m_cost);//Cost
				m_bankGenericVehicleModList->AddItem(i,2,currentItem->m_lockHash.TryGetCStr());//lock hash
			}
		}
	}
}

void CExtraMetadataMgr::WeaponComboCB()
{
	for(int i = 0; i<m_sNumOfListedWeaponComponents;i++)
	{
		m_bankWeaponComponentsList->RemoveItem(i);
	}
	if(m_weaponShopItemsLookup.GetCount()>0)
		if(m_weaponShopItemsLookup[m_sWeaponComboIdx])
		{
			WeaponShopItem* curWeapon = m_weaponShopItemsLookup[m_sWeaponComboIdx];
			m_sNumOfListedWeaponComponents = curWeapon->m_weaponComponents.GetCount();
			for(int i=0;i<curWeapon->m_weaponComponents.GetCount();++i)
			{
				int col=0;
				const ShopWeaponComponent* currentItem = &(curWeapon->m_weaponComponents[i]);
				if(currentItem)
				{	
					const CWeaponInfo* info = CWeaponInfoManager::GetInfo<CWeaponInfo>(curWeapon->m_nameHash);
					const CWeaponComponentInfo* compInfo =  CWeaponComponentManager::GetInfo<CWeaponComponentInfo>(currentItem->m_componentName);

					const CWeaponComponentPoint* cPoint = info? info->GetAttachPoint(compInfo) : NULL;
					m_bankWeaponComponentsList->AddItem(i,col++,cPoint?cPoint->GetAttachBoneName():""); //parser_eWeaponModType_Strings[currentItem->m_eModType]);//Hash
					m_bankWeaponComponentsList->AddItem(i,col++,currentItem->m_componentName.GetCStr());//Hash
					m_bankWeaponComponentsList->AddItem(i,col++,currentItem->m_cost);//Cost
					m_bankWeaponComponentsList->AddItem(i,col++,curWeapon->m_ammoCost);//ammo Cost
					const CAmmoInfo* aInfo = info ? info->GetAmmoInfo() : NULL;
					m_bankWeaponComponentsList->AddItem(i,col++,aInfo ? aInfo->GetName():"");//ammo type;
					m_bankWeaponComponentsList->AddItem(i,col++,info ? (info->GetIsWeaponComponentDefault(currentItem->m_componentName)?"YES":"NO"):"ERROR");
					m_bankWeaponComponentsList->AddItem(i,col++, info ? info->GetClipSize() : 0 );//ammo type;
					m_bankWeaponComponentsList->AddItem(i,col++, TheText.DoesTextLabelExist(currentItem->m_textLabel)? TheText.Get(currentItem->m_textLabel) : currentItem->m_textLabel);
					m_bankWeaponComponentsList->AddItem(i,col++, TheText.DoesTextLabelExist(currentItem->m_componentDesc)? TheText.Get(currentItem->m_componentDesc) : currentItem->m_componentDesc);
					m_bankWeaponComponentsList->AddItem(i,col++, TheText.DoesTextLabelExist(curWeapon->m_weaponDesc)? TheText.Get(curWeapon->m_weaponDesc) : curWeapon->m_weaponDesc);
					m_bankWeaponComponentsList->AddItem(i,col++, TheText.DoesTextLabelExist(curWeapon->m_weaponTT)? TheText.Get(curWeapon->m_weaponTT) : curWeapon->m_weaponTT);
					m_bankWeaponComponentsList->AddItem(i,col++, TheText.DoesTextLabelExist(curWeapon->m_weaponUppercase)? TheText.Get(curWeapon->m_weaponUppercase) : curWeapon->m_weaponUppercase);
					m_bankWeaponComponentsList->AddItem(i,col++, curWeapon->m_lockHash.TryGetCStr());//lock hash
					m_bankWeaponComponentsList->AddItem(i,col++, currentItem->m_lockHash.TryGetCStr());//lock hash
				}
			}
		}
}

void CExtraMetadataMgr::SetupShopVehicleWidgets(bkBank& bank)
{
	bank.PushGroup("Shop Vehicles", false);
	{
		PopulateVehicleNames();
		if(m_vehicleNames.GetCount()>0)
		{
			m_bankVehicleCombo = bank.AddCombo("Vehicle Name",&m_sVehicleComboIdx,m_vehicleNames.GetCount(),&m_vehicleNames[0],datCallback(MFA(CExtraMetadataMgr::VehicleComboCB),(datBase*)this));
		}
		else
		{
			m_bankVehicleCombo = bank.AddCombo("Vehicle Name", &m_sVehicleComboIdx,0,NULL,rage::NullCallback);
			m_bankVehicleCombo->SetString(0,"No DLC vehicles loaded, hit refresh upon loading");
		}
		m_bankVehicleModList = bank.AddList("Vehicle mods");
		m_bankVehicleModList->AddColumnHeader(0,"Hash",bkList::STRING);
		m_bankVehicleModList->AddColumnHeader(1,"Cost",bkList::INT);
		m_bankVehicleModList->AddColumnHeader(2,"Vehicle Lock hash",bkList::STRING);
		m_bankVehicleModList->AddColumnHeader(3,"Vehmod Lock hash",bkList::STRING);
		m_bankGenericVehicleModList = bank.AddList("Generic Vehicle mods");
		m_bankGenericVehicleModList->AddColumnHeader(0,"Hash",bkList::STRING);
		m_bankGenericVehicleModList->AddColumnHeader(1,"Cost",bkList::INT);
		m_bankGenericVehicleModList->AddColumnHeader(2,"Mod Lock hash",bkList::STRING);
		bank.AddButton("Refresh",datCallback(MFA(CExtraMetadataMgr::VehicleRefreshCB),(datBase*)this));
		bank.AddButton("Create selected vehicle",datCallback(MFA(CExtraMetadataMgr::VehicleCreateCB),(datBase*)this));
		VehicleComboCB();		
	}
	bank.PopGroup();
}

void CExtraMetadataMgr::InteriorRefreshCB()
{
	CInteriorProxy::Pool* proxyPool = CInteriorProxy::GetPool();

	if (m_bankProxyIndicesMetaCombo)
	{
		static atArray<const char*> s_dlcProxyPathsPtr;
		const char* currOrderDataPath = m_bankProxyIndicesMetaCombo->GetString(ms_ProxyIndexComboIdx);

		ms_ProxyIndexComboIdx = 0;
		s_dlcProxyPathsPtr.ResetCount();
		m_proxyOrderLookup.ResetCount();

		const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::INTERIOR_PROXY_ORDER_FILE);
		s32 currComboIdx = 0;
		while (DATAFILEMGR.IsValid(pData))
		{
			SInteriorOrderData currOrderData;

			// Add proxy orders on file into the lookup table
			if (PARSER.LoadObject(pData->m_filename, NULL, currOrderData))
			{
				for (u32 i = 0; i < currOrderData.m_proxies.GetCount(); i++)
					m_proxyOrderLookup.SafeInsert(currOrderData.m_proxies[i].GetHash(), pData->m_filename);
			}

			s_dlcProxyPathsPtr.PushAndGrow(pData->m_filename);

			// Try to preserve what was already selected
			if (currOrderDataPath && strcmp(currOrderDataPath, pData->m_filename) == 0)
				ms_ProxyIndexComboIdx = currComboIdx;

			pData = DATAFILEMGR.GetNextFile(pData);
			currComboIdx++;
		}

		// Finalise the map
		m_proxyOrderLookup.FinishInsertion();

		if (s_dlcProxyPathsPtr.GetCount() <= 0)
			s_dlcProxyPathsPtr.PushAndGrow("No DLC proxies loaded, will refresh when some are loaded.", 1);

		ms_ProxyIndexComboIdx = rage::Clamp(ms_ProxyIndexComboIdx, 0, s_dlcProxyPathsPtr.GetCount() - 1);

		m_bankProxyIndicesMetaCombo->UpdateCombo("Proxy Index File: ", &ms_ProxyIndexComboIdx, s_dlcProxyPathsPtr.GetCount(), 
			s_dlcProxyPathsPtr.GetElements(), datCallback(MFA(CExtraMetadataMgr::ProxyIndexComboCB), (datBase*)this));

		ProxyIndexComboCB();
	}

	if (ms_bankInteriorCombo && proxyPool)
	{
		const u32 maxIntNameLen = 80;
		const u32 maxNumIntNames = 512;
		static atFixedArray<char[maxIntNameLen], maxNumIntNames> s_dlcIntDisplayNames;
		static atArray<const char*> s_dlcIntDisplayNamesPtr;
		CInteriorProxy* pProxy = NULL;
		CInteriorProxy* pPrevSelectedProxy = NULL;
		int numProxies = proxyPool->GetSize();
		int defaultIndex = 0;

		// Try to keep the old proxy during the refresh...
		if (ms_dlcInteriorComboIdx >= 0 && ms_dlcInteriorComboIdx < ms_dlcInteriorMapSlots.GetCount())
			pPrevSelectedProxy = CInteriorProxy::FindProxy(ms_dlcInteriorMapSlots[ms_dlcInteriorComboIdx]);

		s_dlcIntDisplayNames.Reset();
		s_dlcIntDisplayNamesPtr.ResetCount();
		ms_dlcInteriorMapSlots.ResetCount();

		for (u32 i = 0; i < numProxies; i++)
		{
			pProxy = proxyPool->GetSlot(i);

			if (pProxy && proxyPool->IsValidPtr(pProxy) && pProxy->GetChangeSetSource() != 0)
			{
				s32 currIntIndex = ms_dlcInteriorMapSlots.GetCount();

				if Verifyf(currIntIndex < maxNumIntNames, "CExtraMetadataMgr::InteriorRefreshCB - Reached maximum number of RAG DLC interior names")
				{
					if (pPrevSelectedProxy == pProxy)
						defaultIndex = currIntIndex;

					ms_dlcInteriorMapSlots.PushAndGrow(pProxy->GetMapDataSlotIndex().Get(), 1);

					char enableChar = '_';
					enableChar = pProxy->GetIsDisabled() ? 'D' : enableChar;
					enableChar = pProxy->GetIsCappedAtPartial() ? 'C' :enableChar;

					char* newValue = s_dlcIntDisplayNames.Append();

					if (pProxy->GetGroupId() == 0)
					{
						formatf(newValue, maxIntNameLen, "%03d %c:%c   %-30.25s    (id:%03d)", 
							i, (pProxy->IsContainingImapActive() ? ' ' : '!'), enableChar, pProxy->GetModelName(), proxyPool->GetJustIndex(pProxy));
					}
					else 
					{
						formatf(newValue, maxIntNameLen, "%03d %c:%c   %-30.25s    (id:%03d   grp:%02d)", 
							i, (pProxy->IsContainingImapActive() ? ' ' : '!'), enableChar, pProxy->GetModelName(), proxyPool->GetJustIndex(pProxy), pProxy->GetGroupId());
					}

					s_dlcIntDisplayNamesPtr.PushAndGrow(newValue, 1);
				}
			}
		}

		ms_dlcInteriorComboIdx = defaultIndex;

		if (s_dlcIntDisplayNamesPtr.GetCount() <= 0)
			s_dlcIntDisplayNamesPtr.PushAndGrow("No DLC interiors loaded, will refresh when some are loaded.", 1);

		ms_dlcInteriorComboIdx = rage::Clamp(ms_dlcInteriorComboIdx, 0, s_dlcIntDisplayNamesPtr.GetCount() - 1);

		ms_bankInteriorCombo->UpdateCombo("Interiors: ", &ms_dlcInteriorComboIdx, 
			s_dlcIntDisplayNamesPtr.GetCount(), s_dlcIntDisplayNamesPtr.GetElements());
	}
}

void CExtraMetadataMgr::WarpToDLCInteriorCB()
{
	if (ms_dlcInteriorComboIdx >= 0 && ms_dlcInteriorComboIdx < ms_dlcInteriorMapSlots.GetCount())
	{
		if (CInteriorProxy* targetProxy = CInteriorProxy::FindProxy(ms_dlcInteriorMapSlots[ms_dlcInteriorComboIdx]))
		{
			Vec3V proxyPos;

			targetProxy->GetPosition(proxyPos);
			CWarpManager::SetWarp(Vector3(proxyPos.GetXf(), proxyPos.GetYf(), proxyPos.GetZf()), VEC3_ZERO, 0.0, true, true, 600.0f);
		}
	}
}

void CExtraMetadataMgr::ProxyIndexComboCB()
{
	static atArray<atHashString> prevDisplayedIndiciesInFile;
	static atArray<atHashString> prevDisplayedIndiciesInScratch;
	SInteriorOrderData currOrderData;
	const char* currOrderDataPath = m_bankProxyIndicesMetaCombo->GetString(ms_ProxyIndexComboIdx);

	m_proxyFromIndex[0] = 0;
	m_proxyToIndex[0] = 0;

	if (ASSET.Exists(currOrderDataPath, NULL) && PARSER.LoadObject(currOrderDataPath, NULL, currOrderData))
	{
		formatf(m_proxyFromIndex, "%i", currOrderData.m_startFrom);
		formatf(m_proxyToIndex, "%i", currOrderData.m_startFrom + currOrderData.m_proxies.GetCount());

		// Indices in file
		for (u32 i = 0; i < prevDisplayedIndiciesInFile.GetCount(); i++)
			m_bankProxyIndicesInFileList->RemoveItem(prevDisplayedIndiciesInFile[i]);

		prevDisplayedIndiciesInFile.ResetCount();

		for (u32 i = 0; i < currOrderData.m_proxies.GetCount(); i++)
		{
			u32 col = 0;
			atHashString currStr = currOrderData.m_proxies[i];

			m_bankProxyIndicesInFileList->AddItem(currStr.GetHash(), col++, currStr.GetCStr());
			prevDisplayedIndiciesInFile.PushAndGrow(currStr);
		}

		// Indices in scratch
		for (u32 i = 0; i < prevDisplayedIndiciesInScratch.GetCount(); i++)
			m_bankProxyIndicesInScratchList->RemoveItem(prevDisplayedIndiciesInScratch[i]);

		prevDisplayedIndiciesInScratch.ResetCount();

		const atArray<atHashString>& unorderedProxies = CInteriorProxy::GetUnorderedProxies();

		for (s32 i = 0; i < unorderedProxies.GetCount(); i++)
		{
			u32 col = 0;
			atHashString currStr = unorderedProxies[i];

			m_bankProxyIndicesInScratchList->AddItem(currStr.GetHash(), col++, currStr.GetCStr());
			prevDisplayedIndiciesInScratch.PushAndGrow(currStr);
		}
	}
}

void CExtraMetadataMgr::MoveProxyFromFileCB()
{
	if (m_clickedProxyInFileHash != 0)
	{
		if (const char** orderFilePathPtr = m_proxyOrderLookup.Access(m_clickedProxyInFileHash))
		{
			const char* orderFilePath = *orderFilePathPtr;
			SInteriorOrderData currOrderData;

			if (ASSET.Exists(orderFilePath, NULL) && PARSER.LoadObject(orderFilePath, NULL, currOrderData))
			{
				u32 attrs = ASSET.GetAttributes(orderFilePath, NULL);

				if (!(attrs & FILE_ATTRIBUTE_READONLY))
				{
					s32 foundIdx = currOrderData.m_proxies.Find(m_clickedProxyInFileHash);

					if (foundIdx >= 0)
						currOrderData.m_proxies.Delete(foundIdx);

					PARSER.SaveObject(orderFilePath, NULL, &currOrderData);

					CInteriorProxy::AddUnorderedProxy(m_clickedProxyInFileHash);
				}
				else
					Assertf(false, "%s is read only, please make sure it's checked out!", orderFilePath);
			}
		}
	}

	InteriorRefreshCB();
}

void CExtraMetadataMgr::MoveAllProxiesToFileCB()
{
	const atArray<atHashString>& unorderedProxies = CInteriorProxy::GetUnorderedProxies();
	s32 prevSize = unorderedProxies.GetCount();

	for (s32 i = 0; i < unorderedProxies.GetCount(); i++)
	{
		m_clickedProxyInScratchHash = unorderedProxies[i];
		MoveProxyToFileCB();

		// We removed something
		if (prevSize != unorderedProxies.GetCount())
		{
			i--;
			prevSize = unorderedProxies.GetCount();
		}
	}

	InteriorRefreshCB();
	m_clickedProxyInScratchHash = 0;
}

void CExtraMetadataMgr::MoveProxyToFileCB()
{
	if (m_clickedProxyInScratchHash != 0)
	{
		SInteriorOrderData currOrderData;
		const char* currOrderDataPath = m_bankProxyIndicesMetaCombo->GetString(ms_ProxyIndexComboIdx);

		if (ASSET.Exists(currOrderDataPath, NULL) && PARSER.LoadObject(currOrderDataPath, NULL, currOrderData))
		{
			u32 attrs = ASSET.GetAttributes(currOrderDataPath, NULL);

			if (!(attrs & FILE_ATTRIBUTE_READONLY))
			{
				if (currOrderData.m_proxies.Find(m_clickedProxyInScratchHash) == -1)
				{
					currOrderData.m_proxies.PushAndGrow(m_clickedProxyInScratchHash);

					CInteriorProxy::RemoveUnorderedProxy(m_clickedProxyInScratchHash);
				}

				PARSER.SaveObject(currOrderDataPath, NULL, &currOrderData);
			}
			else
				Assertf(false, "%s is read only, please make sure it's checked out!", currOrderDataPath);
		}
	}

	InteriorRefreshCB();
}

void CExtraMetadataMgr::ClickProxyInFile(s32 index)
{
	m_clickedProxyInFileHash = index;
}

void CExtraMetadataMgr::ClickProxyInScratch(s32 index)
{
	m_clickedProxyInScratchHash = index;
}

void CExtraMetadataMgr::EditProxyFromIndex()
{
	SInteriorOrderData currOrderData;
	const char* currOrderDataPath = m_bankProxyIndicesMetaCombo->GetString(ms_ProxyIndexComboIdx);

	if (ASSET.Exists(currOrderDataPath, NULL) && PARSER.LoadObject(currOrderDataPath, NULL, currOrderData))
	{
		u32 attrs = ASSET.GetAttributes(currOrderDataPath, NULL);

		if (!(attrs & FILE_ATTRIBUTE_READONLY))
		{
			currOrderData.m_startFrom = atoi(m_proxyFromIndex);
			PARSER.SaveObject(currOrderDataPath, NULL, &currOrderData);
		}
		else
			Assertf(false, "%s is read only, please make sure it's checked out!", currOrderDataPath);
	}

	InteriorRefreshCB();
}

void CExtraMetadataMgr::CheckOutFile(const char* currOrderDataPath)
{
	char remoteCmd[256] = { 0 };
	char physicalPath[256] = { 0 };
	const fiDevice *device = fiDevice::GetDevice(currOrderDataPath);

	if (device)
	{
		device->FixRelativeName(physicalPath, sizeof(physicalPath), currOrderDataPath);
	}

	formatf(remoteCmd, "p4 edit %s", physicalPath);

	sysExec(remoteCmd);
}

void CExtraMetadataMgr::CheckoutProxyFile()
{
	const char* currOrderDataPath = m_bankProxyIndicesMetaCombo->GetString(ms_ProxyIndexComboIdx);

	if (ASSET.Exists(currOrderDataPath, NULL))
	{
		CheckOutFile(currOrderDataPath);
	}
}

void CExtraMetadataMgr::SetupInteriorWidgets(bkBank& bank)
{
	bank.PushGroup("DLC Interiors", false);
	{
		// DLC interior list
		bank.PushGroup("DLC Warp List", false);
		{
			ms_bankInteriorCombo = bank.AddCombo("Interiors: ", &ms_dlcInteriorComboIdx, 0, NULL, NullCB);
			bank.AddButton("Warp", datCallback(MFA(CExtraMetadataMgr::WarpToDLCInteriorCB),(datBase*)this));
		}
		bank.PopGroup();

		// DLC interior index tool
		bank.PushGroup("DLC Proxy Indexing tool", false);
		{
			m_bankProxyIndicesMetaCombo = bank.AddCombo("Proxy Index File: ", &ms_ProxyIndexComboIdx, 0, NULL, rage::NullCallback);

			m_proxyFromIdxTxt = bank.AddText("From Index:", m_proxyFromIndex, sizeof(m_proxyFromIndex), 
				false, datCallback(MFA(CExtraMetadataMgr::EditProxyFromIndex), (datBase*)this));

			m_proxyToIdxTxt = bank.AddText("To Index:", m_proxyToIndex, sizeof(m_proxyToIndex), true, rage::NullCallback);
			bank.AddButton("Checkout file", datCallback(MFA(CExtraMetadataMgr::CheckoutProxyFile),(datBase*)this));

			bkList::ClickItemFuncType clickProxyInFileCB;
			clickProxyInFileCB.Reset<CExtraMetadataMgr, &CExtraMetadataMgr::ClickProxyInFile>(this);

			m_bankProxyIndicesInFileList = bank.AddList("Proxies in File");
			m_bankProxyIndicesInFileList->AddColumnHeader(0, "Name", bkList::STRING);
			m_bankProxyIndicesInFileList->SetSingleClickItemFunc(clickProxyInFileCB);

			bank.AddButton("/\\/\\ - (All Up)", datCallback(MFA(CExtraMetadataMgr::MoveAllProxiesToFileCB),(datBase*)this));
			bank.AddButton("/\\ - (One Up)", datCallback(MFA(CExtraMetadataMgr::MoveProxyToFileCB),(datBase*)this));
			bank.AddButton("\\/ - (One down)", datCallback(MFA(CExtraMetadataMgr::MoveProxyFromFileCB),(datBase*)this));

			bkList::ClickItemFuncType clickProxyInScratchCB;
			clickProxyInScratchCB.Reset<CExtraMetadataMgr, &CExtraMetadataMgr::ClickProxyInScratch>(this);

			m_bankProxyIndicesInScratchList = bank.AddList("Proxies in Scratch");
			m_bankProxyIndicesInScratchList->AddColumnHeader(0, "Name", bkList::STRING);
			m_bankProxyIndicesInScratchList->SetSingleClickItemFunc(clickProxyInScratchCB);
		}
		bank.PopGroup();

		InteriorRefreshCB();
	}
	bank.PopGroup();
}

void CExtraMetadataMgr::SetupShopWeaponWidgets(bkBank& bank)
{
	bank.PushGroup("Shop Weapons", false);
	{
		PopulateWeaponNames();
		if(m_weaponNames.GetCount()>0)
		{
			m_bankWeaponCombo = bank.AddCombo("Weapon Name",&m_sWeaponComboIdx,m_weaponNames.GetCount(),&m_weaponNames[0],datCallback(MFA(CExtraMetadataMgr::WeaponComboCB),(datBase*)this));
		}
		else
		{
			m_bankWeaponCombo = bank.AddCombo("Weapon Name", &m_sWeaponComboIdx,0,NULL,rage::NullCallback);
			m_bankWeaponCombo->SetString(0,"No DLC weapons loaded, hit refresh upon loading");
		}
		m_bankWeaponComponentsList = bank.AddList("Weapon Components");
		m_bankWeaponComponentsList->AddColumnHeader(0,"Component Type",bkList::STRING);
		m_bankWeaponComponentsList->AddColumnHeader(1,"Hash",bkList::STRING);
		m_bankWeaponComponentsList->AddColumnHeader(2,"Cost",bkList::INT);
		m_bankWeaponComponentsList->AddColumnHeader(3,"Ammo cost",bkList::INT);
		m_bankWeaponComponentsList->AddColumnHeader(4,"Ammo type",bkList::STRING);
		m_bankWeaponComponentsList->AddColumnHeader(5,"Is Default",bkList::STRING);
		m_bankWeaponComponentsList->AddColumnHeader(6,"Default clip size",bkList::INT);
		m_bankWeaponComponentsList->AddColumnHeader(7,"Component Name",bkList::STRING);
		m_bankWeaponComponentsList->AddColumnHeader(8,"Component Desc",bkList::STRING);
		m_bankWeaponComponentsList->AddColumnHeader(9,"Weapon Desc",bkList::STRING);
		m_bankWeaponComponentsList->AddColumnHeader(10,"Weapon The Name",bkList::STRING);
		m_bankWeaponComponentsList->AddColumnHeader(11,"Weapon Uppercase",bkList::STRING);
		m_bankWeaponComponentsList->AddColumnHeader(12,"Weapon Lock hash",bkList::STRING);
		m_bankWeaponComponentsList->AddColumnHeader(13,"Component Lock hash",bkList::STRING);
		bank.AddButton("Refresh",datCallback(MFA(CExtraMetadataMgr::WeaponRefreshCB),(datBase*)this));
		bank.AddButton("Create selected Weapon",datCallback(MFA(CExtraMetadataMgr::WeaponEquipCB),(datBase*)this));
		WeaponRefreshCB();
		WeaponComboCB();		
	}
	bank.PopGroup();
}

void CExtraMetadataMgr::SetupShopTattooWidgets(bkBank& bank)
{
	bank.PushGroup("Shop Tattoo", false);
	{
		bank.AddButton("Print tattoo debug", datCallback(MFA(CExtraMetadataMgr::DebugPrintTattooShopItems),(datBase*)this));
		bank.AddButton("Clear tattoos", datCallback(MFA(CExtraMetadataMgr::TattooClearCB),(datBase*)this));
		m_bankTattooList = bank.AddList("Tattoo Shop Items (double click to add tattoos on the player)");
		bkList::ClickItemFuncType tattooClickCB;
		tattooClickCB.Reset<CExtraMetadataMgr, &CExtraMetadataMgr::TattooDblClickCB>(this);
		m_bankTattooList->SetDoubleClickItemFunc(tattooClickCB);
		m_bankTattooList->AddColumnHeader(0, "ID", bkList::INT);
		m_bankTattooList->AddColumnHeader(1, "Faction", bkList::STRING);
		m_bankTattooList->AddColumnHeader(2, "Cost", bkList::INT);
		m_bankTattooList->AddColumnHeader(3, "Facing", bkList::STRING);
		m_bankTattooList->AddColumnHeader(4, "DLC Label;", bkList::STRING);
		m_bankTattooList->AddColumnHeader(5, "Preset", bkList::STRING);
		m_bankTattooList->AddColumnHeader(6, "Collection", bkList::STRING);
		DisplayTattoos(0,0);
		bank.AddButton("Refresh",datCallback(MFA(CExtraMetadataMgr::TattooRefreshCB),(datBase*)this));
	}
	bank.PopGroup();
}

void CExtraMetadataMgr::PopulateVehicleNames()
{
	m_vehicleNames.ResetCount();
	for(int i=0; i<m_vehiclesLookup.GetCount();i++)
	{
		const ShopVehicleData* currentItem = m_vehiclesLookup[i];
		if(currentItem)
		{
			m_vehicleNames.PushAndGrow(TheText.DoesTextLabelExist(currentItem->m_textLabel)? TheText.Get(currentItem->m_textLabel) : currentItem->m_textLabel);			
		}
	}
}

void CExtraMetadataMgr::PopulateWeaponNames()
{
	m_weaponNames.ResetCount();
	for(int i=0; i<m_weaponShopItemsLookup.GetCount();i++)
	{
		const WeaponShopItem* currentItem = m_weaponShopItemsLookup[i];
		if(currentItem)
		{
			m_weaponNames.PushAndGrow(TheText.DoesTextLabelExist(currentItem->m_textLabel)? TheText.Get(currentItem->m_textLabel) : currentItem->m_textLabel);			
		}
	}
}



void CExtraMetadataMgr::DisplayTattoos(s32 /*nextPage*/, u32 /*lookupHash*/)
{
	m_sNumOfListedTattoos = 0;
	extern const char* parser_eTattooFaction_Strings[];
	extern const char* parser_eTattooFacing_Strings[];
	if(m_bankTattooList)
	{
		for(int i=0;i<m_tattooShopItemsLookup.GetMaxCount();i++)
		{
			for(int j= 0; j<m_tattooShopItemsLookup[i].GetCount(); j++)
			{
				const tattooLookupEntry* entry = &(m_tattooShopItemsLookup[i][j]);
				const TattooShopItem* currentItem = &(m_tattooShopsItems[entry->collection].TattooShopItems[entry->index]);
				if(currentItem)
				{
					m_bankTattooList->AddItem(m_sNumOfListedTattoos,0,currentItem->m_id);
					m_bankTattooList->AddItem(m_sNumOfListedTattoos,1,parser_eTattooFaction_Strings[currentItem->m_eFaction]);
					m_bankTattooList->AddItem(m_sNumOfListedTattoos,2,currentItem->m_cost);
					m_bankTattooList->AddItem(m_sNumOfListedTattoos,3,parser_eTattooFacing_Strings[currentItem->m_eFacing]);
					m_bankTattooList->AddItem(m_sNumOfListedTattoos,4,currentItem->m_textLabel);
					m_bankTattooList->AddItem(m_sNumOfListedTattoos,5,currentItem->m_preset.GetCStr());
					m_bankTattooList->AddItem(m_sNumOfListedTattoos,6,currentItem->m_collection.GetCStr());
					m_sNumOfListedTattoos++;
				}
			}
		}
	}
}
void CExtraMetadataMgr::SetupShopPedApparelWidgets(bkBank& bank)
{
	extern const char* parser_eScrCharacters_Strings[];

	bank.PushGroup("Shop Ped Apparel", false);
	{
		bank.PushGroup("Shop Ped Components", false);
		{
			bkList::ClickItemFuncType pedComponentblClickCB;
			const char* emptyList = "";

			bank.AddText("Lookup Component:", m_shopPedComponentLookup, sizeof(m_shopPedComponentLookup), false, datCallback(MFA(CExtraMetadataMgr::LookupShopPedComponent), (datBase*)this));
			bank.AddText("Component Counts:", m_shopPedComponentCounts, sizeof(m_shopPedComponentCounts), true, NullCallback);			

			m_compCharCombo = bank.AddCombo("Character:", &m_compCharComboIdx, eScrCharacters_NUM_ENUMS, parser_eScrCharacters_Strings, datCallback(MFA(CExtraMetadataMgr::CompCharComboCB),(datBase*)this));
			m_compDLCNameCombo = bank.AddCombo("DLC Name:", &m_compDLCNameComboIdx, 0, &emptyList, datCallback(MFA(CExtraMetadataMgr::CompDLCNameComboCB),(datBase*)this));

			u32 count = 0;
			pedComponentblClickCB.Reset<CExtraMetadataMgr, &CExtraMetadataMgr::ComponentDblClickCB>(this);
			m_bankComponentList = bank.AddList("Component List (double click item to set variation)");
			m_bankComponentList->SetDoubleClickItemFunc(pedComponentblClickCB);
			m_bankComponentList->AddColumnHeader(count++, "Name", bkList::STRING);
			m_bankComponentList->AddColumnHeader(count++, "Text", bkList::STRING);
			m_bankComponentList->AddColumnHeader(count++, "Shop", bkList::INT);
			m_bankComponentList->AddColumnHeader(count++, "Locate", bkList::INT);
			m_bankComponentList->AddColumnHeader(count++, "Local Drawable Index", bkList::INT);
			m_bankComponentList->AddColumnHeader(count++, "Global Drawable Index", bkList::INT);
			m_bankComponentList->AddColumnHeader(count++, "Texture", bkList::INT);
			m_bankComponentList->AddColumnHeader(count++, "Script Comp Type", bkList::INT);
			m_bankComponentList->AddColumnHeader(count++, "Cost", bkList::INT);
			m_bankComponentList->AddColumnHeader(count++, "In Outfit", bkList::STRING);

			bank.AddButton("<< Previous Page", &ShowPreviousComponentPage);
			bank.AddButton("Next Page >>", &ShowNextComponentPage);

			DisplayPedComponents(0, 0, true);
		}
		bank.PopGroup();

		bank.PushGroup("Shop Ped Props", false);
		{
			bkList::ClickItemFuncType pedPropDblClickCB;
			const char* emptyList = "";

			bank.AddText("Lookup Prop:", m_shopPedPropLookup, sizeof(m_shopPedPropLookup), false, datCallback(MFA(CExtraMetadataMgr::LookupShopPedProp), (datBase*)this));
			bank.AddText("Prop Counts:", m_shopPedPropCounts, sizeof(m_shopPedPropCounts), true, NullCallback);

			m_propCharCombo = bank.AddCombo("Character:", &m_propCharComboIdx, eScrCharacters_NUM_ENUMS, parser_eScrCharacters_Strings, datCallback(MFA(CExtraMetadataMgr::PropCharComboCB),(datBase*)this));
			m_propDLCNameCombo = bank.AddCombo("DLC Name:", &m_propDLCNameComboIdx, 0, &emptyList, datCallback(MFA(CExtraMetadataMgr::PropDLCNameComboCB),(datBase*)this));			

			u32 count = 0;
			pedPropDblClickCB.Reset<CExtraMetadataMgr, &CExtraMetadataMgr::PropDblClickCB>(this);
			m_bankPropList = bank.AddList("Prop List (double click item to set prop)");
			m_bankPropList->SetDoubleClickItemFunc(pedPropDblClickCB);
			m_bankPropList->AddColumnHeader(count++, "Name", bkList::STRING);
			m_bankPropList->AddColumnHeader(count++, "Text", bkList::STRING);
			m_bankPropList->AddColumnHeader(count++, "Shop", bkList::INT);
			m_bankPropList->AddColumnHeader(count++, "Locate", bkList::INT);
			m_bankPropList->AddColumnHeader(count++, "Local Prop Index", bkList::INT);
			m_bankPropList->AddColumnHeader(count++, "Global Prop Index", bkList::INT);
			m_bankPropList->AddColumnHeader(count++, "Texture", bkList::INT);
			m_bankPropList->AddColumnHeader(count++, "Anchor Point", bkList::INT);
			m_bankPropList->AddColumnHeader(count++, "Cost", bkList::INT);
			m_bankPropList->AddColumnHeader(count++, "In Outfit", bkList::STRING);

			bank.AddButton("<< Previous Page", &ShowPreviousPropPage);
			bank.AddButton("Next Page >>", &ShowNextPropPage);

			DisplayPedProps(0, 0, true);
		}
		bank.PopGroup();

		bank.PushGroup("Shop Ped Outfits", false);
		{
			bkList::ClickItemFuncType pedPrOutfitDblClickCB;
			const char* emptyList = "";

			bank.AddText("Lookup Outfit:", m_shopPedOutfitLookup, sizeof(m_shopPedOutfitLookup), false, datCallback(MFA(CExtraMetadataMgr::LookupShopPedOutfit), (datBase*)this));
			bank.AddText("Outfit Counts:", m_shopPedOutfitCounts, sizeof(m_shopPedOutfitCounts), true, NullCallback);

			m_outfitCharCombo = bank.AddCombo("Character:", &m_outfitCharComboIdx, eScrCharacters_NUM_ENUMS, parser_eScrCharacters_Strings, datCallback(MFA(CExtraMetadataMgr::OutfitCharComboCB),(datBase*)this));
			m_outfitDLCNameCombo = bank.AddCombo("DLC Name:", &m_outfitDLCNameComboIdx, 0, &emptyList, datCallback(MFA(CExtraMetadataMgr::OutfitDLCNameComboCB),(datBase*)this));

			u32 count = 0;
			pedPrOutfitDblClickCB.Reset<CExtraMetadataMgr, &CExtraMetadataMgr::OutfitDblClickCB>(this);
			m_bankOutfitList = bank.AddList("Outfit List (double click to set outfit)");
			m_bankOutfitList->SetDoubleClickItemFunc(pedPrOutfitDblClickCB);
			m_bankOutfitList->AddColumnHeader(count++,"Name", bkList::STRING);
			m_bankOutfitList->AddColumnHeader(count++,"Cost", bkList::INT);
			m_bankOutfitList->AddColumnHeader(count++,"Text Label", bkList::STRING);
			m_bankOutfitList->AddColumnHeader(count++,"Number of components", bkList::INT);
			m_bankOutfitList->AddColumnHeader(count++,"Number of props", bkList::INT);

			bank.AddButton("<< Previous Page", &ShowPreviousOutfitPage);
			bank.AddButton("Next Page >>", &ShowNextOutfitPage);

			DisplayPedOutfits(0, 0, true);

		}
		bank.PopGroup();
	}
	bank.PopGroup();
}

void CExtraMetadataMgr::BuildDLCNamesArray(atArray<const char*>& targetArray, s32 characterIdx)
{
	targetArray.ResetCount();

	targetArray.PushAndGrow("All", 1);

	for (u32 i = 0; i < m_shopPedApparel.GetCount(); i++)
	{
		ShopPedApparel& currApparel = *m_shopPedApparel[i];

		if (characterIdx == SCR_CHAR_ANY || currApparel.m_eCharacter == characterIdx)
			targetArray.PushAndGrow(currApparel.m_dlcName.c_str(), 1);
	}

	Assertf(targetArray.GetCount() > 0, "CExtraMetadataMgr::BuildDLCNamesArray - Target array must have at least one entry!");
}

void CExtraMetadataMgr::OutfitCharComboCB()
{
	EXTRAMETADATAMGR.DisplayPedOutfits(m_currentOutfitPg, 0, true);
}

void CExtraMetadataMgr::OutfitDLCNameComboCB()
{
	EXTRAMETADATAMGR.DisplayPedOutfits(m_currentOutfitPg, 0, false);
}

void CExtraMetadataMgr::CompCharComboCB()
{
	EXTRAMETADATAMGR.DisplayPedComponents(m_currentComponentPg, 0, true);
}

void CExtraMetadataMgr::CompDLCNameComboCB()
{
	EXTRAMETADATAMGR.DisplayPedComponents(m_currentComponentPg, 0, false);
}

void CExtraMetadataMgr::PropCharComboCB()
{
	EXTRAMETADATAMGR.DisplayPedProps(m_currentPropPg, 0, true);
}

void CExtraMetadataMgr::PropDLCNameComboCB()
{
	EXTRAMETADATAMGR.DisplayPedProps(m_currentPropPg, 0, false);
}

void CExtraMetadataMgr::ShowPreviousComponentPage()
{
	EXTRAMETADATAMGR.DisplayPedComponents(m_currentComponentPg - 1, 0, false);
}

void CExtraMetadataMgr::ShowNextComponentPage()
{
	EXTRAMETADATAMGR.DisplayPedComponents(m_currentComponentPg + 1, 0, false);
}

void CExtraMetadataMgr::ShowPreviousPropPage()
{
	EXTRAMETADATAMGR.DisplayPedProps(m_currentPropPg - 1, 0, false);
}

void CExtraMetadataMgr::ShowNextPropPage()
{
	EXTRAMETADATAMGR.DisplayPedProps(m_currentPropPg + 1, 0, false);
}

void CExtraMetadataMgr::ShowPreviousOutfitPage()
{
	EXTRAMETADATAMGR.DisplayPedProps(m_currentOutfitPg - 1, 0, false);
}

void CExtraMetadataMgr::ShowNextOutfitPage()
{
	EXTRAMETADATAMGR.DisplayPedProps(m_currentOutfitPg + 1, 0, false);
}

static u32 CURR_COMP_LIST[MAX_APPAREL_ITEMS_PER_PAGE] = { 0 };

void CExtraMetadataMgr::AddComponentToBankList(ShopPedComponent* currentItem, s32 index)
{
	if (currentItem)
	{
		u32 count = 0;
		m_bankComponentList->AddItem(currentItem->m_uniqueNameHash, count++, currentItem->m_uniqueNameHash.TryGetCStr());
		m_bankComponentList->AddItem(currentItem->m_uniqueNameHash, count++, currentItem->m_textLabel);
		m_bankComponentList->AddItem(currentItem->m_uniqueNameHash, count++, currentItem->m_eShopEnum);
		m_bankComponentList->AddItem(currentItem->m_uniqueNameHash, count++, currentItem->m_locate);
		m_bankComponentList->AddItem(currentItem->m_uniqueNameHash, count++, (s32)currentItem->m_localDrawableIndex);
		m_bankComponentList->AddItem(currentItem->m_uniqueNameHash, count++, (s32)currentItem->m_drawableIndex);
		m_bankComponentList->AddItem(currentItem->m_uniqueNameHash, count++, currentItem->m_textureIndex);
		m_bankComponentList->AddItem(currentItem->m_uniqueNameHash, count++, currentItem->m_eCompType);
		m_bankComponentList->AddItem(currentItem->m_uniqueNameHash, count++, currentItem->m_cost);
		m_bankComponentList->AddItem(currentItem->m_uniqueNameHash, count++, currentItem->m_isInOutfit?"TRUE":"FALSE");

		if (Verifyf(index < MAX_APPAREL_ITEMS_PER_PAGE, "CExtraMetadataMgr::AddComponentToBankList - Index out of range! %u / %u", index, MAX_APPAREL_ITEMS_PER_PAGE))
			CURR_COMP_LIST[index] = currentItem->m_uniqueNameHash;
	}
}

void CExtraMetadataMgr::DisplayPedComponents(s32 nextPage, u32 lookupHash, bool buildDLCNames)
{
	if (buildDLCNames)
	{
		BuildDLCNamesArray(ms_compDLCNames, m_compCharComboIdx);

		m_compDLCNameComboIdx = 0;
		m_compDLCNameCombo->UpdateCombo("DLC Name:", &m_compDLCNameComboIdx, ms_compDLCNames.GetCount(), ms_compDLCNames.GetElements(), datCallback(MFA(CExtraMetadataMgr::CompDLCNameComboCB),(datBase*)this), NULL);
	}

	if (m_bankComponentList)
	{
		const char* currCompDLCName = m_compDLCNameCombo->GetString(m_compDLCNameComboIdx);
		atArray<ShopPedComponent*> validComponents;
		s32 maxPage = 1;

		// remove all the items first...
		for (int i = 0; i < MAX_APPAREL_ITEMS_PER_PAGE; i++)
		{
			if (CURR_COMP_LIST[i] != 0)
				m_bankComponentList->RemoveItem(CURR_COMP_LIST[i]);
		}

		memset(CURR_COMP_LIST, 0, sizeof(CURR_COMP_LIST));

		for (u32 i = 0; i < m_shopPedApparel.GetCount(); i++)
		{
			ShopPedApparel& currApparel = *m_shopPedApparel[i];

			if ((m_compCharComboIdx == SCR_CHAR_ANY || currApparel.m_eCharacter == m_compCharComboIdx) &&
				(stricmp(currApparel.m_dlcName.c_str(), currCompDLCName) == 0 || stricmp("All", currCompDLCName) == 0))
			{
				for (u32 j = 0; j < currApparel.m_pedComponents.GetCount(); j++)
				{
					validComponents.PushAndGrow(&currApparel.m_pedComponents[j]);
				}
			}
		}

		if (validComponents.GetCount() > 0)
			maxPage = ((validComponents.GetCount() - 1) / MAX_APPAREL_ITEMS_PER_PAGE) + 1;

		m_currentComponentPg = nextPage;
		m_currentComponentPg = rage::Clamp(m_currentComponentPg, 0, maxPage - 1);
		

		formatf(m_shopPedComponentCounts, "Count: [%i] // Page: [%i / %i]", validComponents.GetCount(), (m_currentComponentPg + 1),  maxPage);

		if (lookupHash == 0)
		{
			int startIndex = m_currentComponentPg * MAX_APPAREL_ITEMS_PER_PAGE;

			for (s32 i = startIndex; (i < (startIndex + MAX_APPAREL_ITEMS_PER_PAGE)) && (i < validComponents.GetCount()); i++)
				AddComponentToBankList(validComponents[i], i - startIndex);
		}
		else
			AddComponentToBankList(GetShopPedComponent(lookupHash), 0);
	}
}

static u32 CURR_PROP_LIST[MAX_APPAREL_ITEMS_PER_PAGE] = { 0 };

void CExtraMetadataMgr::AddPropToBankList(ShopPedProp* currentItem, s32 index)
{
	if (currentItem)
	{
		u32 count = 0;
		m_bankPropList->AddItem(currentItem->m_uniqueNameHash, count++, currentItem->m_uniqueNameHash.TryGetCStr());
		m_bankPropList->AddItem(currentItem->m_uniqueNameHash, count++, currentItem->m_textLabel);
		m_bankPropList->AddItem(currentItem->m_uniqueNameHash, count++, currentItem->m_eShopEnum);
		m_bankPropList->AddItem(currentItem->m_uniqueNameHash, count++, currentItem->m_locate);
		m_bankPropList->AddItem(currentItem->m_uniqueNameHash, count++, (s32)currentItem->m_localPropIndex);
		m_bankPropList->AddItem(currentItem->m_uniqueNameHash, count++, (s32)currentItem->m_propIndex);
		m_bankPropList->AddItem(currentItem->m_uniqueNameHash, count++, currentItem->m_textureIndex);
		m_bankPropList->AddItem(currentItem->m_uniqueNameHash, count++, currentItem->m_eAnchorPoint);
		m_bankPropList->AddItem(currentItem->m_uniqueNameHash, count++, currentItem->m_cost);
		m_bankPropList->AddItem(currentItem->m_uniqueNameHash, count++, currentItem->m_isInOutfit?"TRUE":"FALSE");

		if (Verifyf(index < MAX_APPAREL_ITEMS_PER_PAGE, "CExtraMetadataMgr::AddPropToBankList - Index out of range! %u / %u", index, MAX_APPAREL_ITEMS_PER_PAGE))
			CURR_PROP_LIST[index] = currentItem->m_uniqueNameHash;
	}
}

void CExtraMetadataMgr::DisplayPedProps(s32 nextPage, u32 lookupHash, bool buildDLCNames)
{
	if (buildDLCNames)
	{
		BuildDLCNamesArray(ms_propDLCNames, m_propCharComboIdx);

		m_propDLCNameComboIdx = 0;
		m_propDLCNameCombo->UpdateCombo("DLC Name:", &m_propDLCNameComboIdx, ms_propDLCNames.GetCount(), ms_propDLCNames.GetElements(), datCallback(MFA(CExtraMetadataMgr::PropDLCNameComboCB),(datBase*)this), NULL);
	}

	if (m_bankPropList)
	{
		const char* currPropDLCName = m_propDLCNameCombo->GetString(m_propDLCNameComboIdx);
		atArray<ShopPedProp*> validProps;
		s32 maxPage = 1;

		// remove all the items first...
		for (u32 i = 0; i < MAX_APPAREL_ITEMS_PER_PAGE; i++)
		{
			if (CURR_PROP_LIST[i] != 0)
				m_bankPropList->RemoveItem(CURR_PROP_LIST[i]);
		}

		memset(CURR_PROP_LIST, 0, sizeof(CURR_PROP_LIST));

		for (u32 i = 0; i < m_shopPedApparel.GetCount(); i++)
		{
			ShopPedApparel& currApparel = *m_shopPedApparel[i];

			if ((m_propCharComboIdx == SCR_CHAR_ANY || currApparel.m_eCharacter == m_propCharComboIdx) &&
				(stricmp(currApparel.m_dlcName.c_str(), currPropDLCName) == 0 || stricmp("All", currPropDLCName) == 0))
			{
				for (u32 j = 0; j < currApparel.m_pedProps.GetCount(); j++)
				{
					validProps.PushAndGrow(&currApparel.m_pedProps[j]);
				}
			}
		}

		if (validProps.GetCount() > 0)
			maxPage = ((validProps.GetCount() - 1) / MAX_APPAREL_ITEMS_PER_PAGE) + 1;

		m_currentPropPg = nextPage;
		m_currentPropPg = rage::Clamp(m_currentPropPg, 0, maxPage - 1);

		formatf(m_shopPedPropCounts, "Count: [%i] // Page: [%i / %i]", validProps.GetCount(), (m_currentPropPg + 1),  maxPage);

		if (lookupHash == 0)
		{
			int startIndex = m_currentPropPg * MAX_APPAREL_ITEMS_PER_PAGE;

			for (int i = startIndex; (i < (startIndex + MAX_APPAREL_ITEMS_PER_PAGE)) && (i < validProps.GetCount()); i++)
				AddPropToBankList(validProps[i], i - startIndex);
		}
		else
			AddPropToBankList(GetShopPedProp(lookupHash), 0);
	}
}

static u32 CURR_OUTFIT_LIST[MAX_APPAREL_ITEMS_PER_PAGE] = { 0 };

void CExtraMetadataMgr::AddOutfitToBankList(ShopPedOutfit* currentItem, s32 index)
{
	if (currentItem)
	{
		u32 count = 0;
		m_bankOutfitList->RemoveItem(currentItem->m_uniqueNameHash);
		m_bankOutfitList->AddItem(currentItem->m_uniqueNameHash,count++,currentItem->m_uniqueNameHash.TryGetCStr());
		m_bankOutfitList->AddItem(currentItem->m_uniqueNameHash,count++,currentItem->m_cost);
		m_bankOutfitList->AddItem(currentItem->m_uniqueNameHash,count++,currentItem->m_textLabel);
		m_bankOutfitList->AddItem(currentItem->m_uniqueNameHash,count++,currentItem->m_includedPedComponents.GetCount());
		m_bankOutfitList->AddItem(currentItem->m_uniqueNameHash,count++,currentItem->m_includedPedProps.GetCount());

		if (Verifyf(index < MAX_APPAREL_ITEMS_PER_PAGE, "CExtraMetadataMgr::AddOutfitToBankList - Index out of range! %u / %u", index, MAX_APPAREL_ITEMS_PER_PAGE))
			CURR_OUTFIT_LIST[index] = currentItem->m_uniqueNameHash;
	}
}

void CExtraMetadataMgr::DisplayPedOutfits(s32 nextPage, u32 lookupHash, bool buildDLCNames)
{
	if (buildDLCNames)
	{
		BuildDLCNamesArray(ms_outfitDLCNames, m_outfitCharComboIdx);

		m_outfitDLCNameComboIdx = 0;
		m_outfitDLCNameCombo->UpdateCombo("DLC Name:", &m_outfitDLCNameComboIdx, ms_outfitDLCNames.GetCount(), ms_outfitDLCNames.GetElements(), datCallback(MFA(CExtraMetadataMgr::OutfitDLCNameComboCB),(datBase*)this), NULL);
	}

	if (m_bankOutfitList)
	{
		const char* currOutfitDLCName = m_outfitDLCNameCombo->GetString(m_outfitDLCNameComboIdx);
		atArray<ShopPedOutfit*> validOutfits;
		s32 maxPage = 1;

		// remove all the items first...
		for (u32 i = 0; i < MAX_APPAREL_ITEMS_PER_PAGE; i++)
		{
			if (CURR_OUTFIT_LIST[i] != 0)
				m_bankOutfitList->RemoveItem(CURR_OUTFIT_LIST[i]);
		}

		memset(CURR_OUTFIT_LIST, 0, sizeof(CURR_OUTFIT_LIST));

		for (u32 i = 0; i < m_shopPedApparel.GetCount(); i++)
		{
			ShopPedApparel& currApparel = *m_shopPedApparel[i];

			if ((m_outfitCharComboIdx == SCR_CHAR_ANY || currApparel.m_eCharacter == m_outfitCharComboIdx) && 
				(stricmp(currApparel.m_dlcName.c_str(), currOutfitDLCName) == 0 || stricmp("All", currOutfitDLCName) == 0))
			{
				for (u32 j = 0; j < currApparel.m_pedOutfits.GetCount(); j++)
				{
					validOutfits.PushAndGrow(&currApparel.m_pedOutfits[j]);
				}
			}
		}

		if (validOutfits.GetCount() > 0)
			maxPage = ((validOutfits.GetCount() - 1) / MAX_APPAREL_ITEMS_PER_PAGE) + 1;

		m_currentOutfitPg = nextPage;
		m_currentOutfitPg = rage::Clamp(m_currentOutfitPg, 0, maxPage - 1);

		formatf(m_shopPedOutfitCounts, "Count: [%i] // Page: [%i / %i]", validOutfits.GetCount(), (m_currentOutfitPg + 1),  maxPage);

		if (lookupHash == 0)
		{
			int startIndex = m_currentOutfitPg * MAX_APPAREL_ITEMS_PER_PAGE;

			for (int i = startIndex; (i < (startIndex + MAX_APPAREL_ITEMS_PER_PAGE)) && (i < validOutfits.GetCount()); i++)
				AddOutfitToBankList(validOutfits[i], i - startIndex);
		}
		else
			AddOutfitToBankList(GetShopPedOutfit(lookupHash), 0);
	}
}

void CExtraMetadataMgr::ComponentDblClickCB(s32 nameHash)
{
	ShopPedComponent* clickedItem = GetShopPedComponent((u32)nameHash);
	CPed* pPlayer = CGameWorld::FindLocalPlayer();

	if (pPlayer && clickedItem)
		pPlayer->SetVariation(clickedItem->m_eCompType, clickedItem->m_drawableIndex, 0, clickedItem->m_textureIndex, 0, 0, false);
}

void CExtraMetadataMgr::PropDblClickCB(s32 nameHash)
{
	ShopPedProp* clickedItem = GetShopPedProp((u32)nameHash);
	CPed* pPlayer = CGameWorld::FindLocalPlayer();

	if (pPlayer && clickedItem)
	{		
		if (CPedPropsMgr::IsPropValid(pPlayer, (eAnchorPoints)clickedItem->m_eAnchorPoint, clickedItem->m_propIndex, clickedItem->m_textureIndex))
			CPedPropsMgr::SetPedProp(pPlayer, (eAnchorPoints)clickedItem->m_eAnchorPoint, clickedItem->m_propIndex, clickedItem->m_textureIndex, ANCHOR_NONE, NULL, NULL);
	}
}

void CExtraMetadataMgr::OutfitDblClickCB(s32 nameHash)
{
	ShopPedOutfit* clickedItem = GetShopPedOutfit((u32)nameHash);
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if(pPlayer && clickedItem)
	{
		for(int i=0;i<clickedItem->m_includedPedComponents.GetCount();i++)
		{
			ComponentDblClickCB(clickedItem->m_includedPedComponents[i].m_nameHash);
		}
		for(int i=0;i<clickedItem->m_includedPedProps.GetCount();i++)
		{
			PropDblClickCB(clickedItem->m_includedPedProps[i].m_nameHash);
		}
	}
}

void CExtraMetadataMgr::LookupShopPedComponent()
{
	DisplayPedComponents(m_currentComponentPg, atStringHash(m_shopPedComponentLookup), false);
}

void CExtraMetadataMgr::LookupShopPedProp()
{
	DisplayPedProps(m_currentPropPg, atStringHash(m_shopPedPropLookup), false);
}

void CExtraMetadataMgr::LookupShopPedOutfit()
{
	DisplayPedOutfits(m_currentOutfitPg, atStringHash(m_shopPedOutfitLookup), false);
}

void CExtraMetadataMgr::DebugPrintTattooShopItems() const
{
	extern const char* parser_eTattooFaction_Strings[];
	extern const char* parser_eTattooFacing_Strings[];
	Displayf("--------------------------------------------");
	Displayf("------------ TATTOO SHOP ITEMS -------------");
	Displayf("---------------- (UNSORTED) ----------------");

	for(int i=0; i<m_tattooShopsItems.GetCount(); ++i)
	{
		Displayf("---- DLC Collection Hash 0x%X", m_tattooShopsItems[i].m_nameHash.GetHash());
		for(int j=0; j<m_tattooShopsItems[i].TattooShopItems.GetCount(); ++j)
		{
			Displayf("- %d: 0x%d 0x%X %s %d %s 0x%X \"%s\"", 
				m_tattooShopsItems[i].TattooShopItems[j].m_id, 
				m_tattooShopsItems[i].TattooShopItems[j].m_collection.GetHash(), 
				m_tattooShopsItems[i].TattooShopItems[j].m_preset.GetHash(),  
				parser_eTattooFaction_Strings[m_tattooShopsItems[i].TattooShopItems[j].m_eFaction],
				m_tattooShopsItems[i].TattooShopItems[j].m_cost, 
				parser_eTattooFacing_Strings[m_tattooShopsItems[i].TattooShopItems[j].m_eFacing], 
				m_tattooShopsItems[i].TattooShopItems[j].m_updateGroup.GetHash(), 
				m_tattooShopsItems[i].TattooShopItems[j].m_textLabel.c_str() ); 
		}
		Displayf("");
	}

	Displayf("--------------------------------------------");
	Displayf("------------ TATTOO SHOP ITEMS -------------");
	Displayf("----------------- (LOOKUP) -----------------");
	for(int iFaction=0; iFaction<m_tattooShopItemsLookup.size(); ++iFaction)
	{
		Displayf("---- Faction %s", parser_eTattooFaction_Strings[iFaction]);
		for(int j=0; j<m_tattooShopItemsLookup[iFaction].GetCount(); ++j)
		{
			const TattooShopItem* pTattooItem = GetTattooShopItem((eTattooFaction)iFaction, j);
			Displayf("- %d: 0x%X 0x%X %s %d %s 0x%X \"%s\"", 
				pTattooItem->m_id, 
				pTattooItem->m_collection.GetHash(), 
				pTattooItem->m_preset.GetHash(),  
				parser_eTattooFaction_Strings[pTattooItem->m_eFaction],
				pTattooItem->m_cost, 
				parser_eTattooFacing_Strings[pTattooItem->m_eFacing], 
				pTattooItem->m_updateGroup.GetHash(), 
				pTattooItem->m_textLabel.c_str() ); 
		}
		Displayf("");
	}
}
#endif	// __BANK && !__SPU

int CExtraMetadataMgr::GetNumTattooShopItemsInFaction(eTattooFaction faction) const
{
	if((u32)faction < (u32)m_tattooShopItemsLookup.size())
	{
		return m_tattooShopItemsLookup[faction].GetCount();
	}

	return 0;
}

const TattooShopItem* CExtraMetadataMgr::GetTattooShopItem(eTattooFaction faction, int idx) const
{
	if((u32)faction < (u32)m_tattooShopItemsLookup.size()
		&& (u32)idx < (u32)m_tattooShopItemsLookup[faction].GetCount())
	{
		const tattooLookupEntry& slotReferences = m_tattooShopItemsLookup[faction][idx];
		Assert(slotReferences.collection < m_tattooShopsItems.GetCount() && slotReferences.index < m_tattooShopsItems[slotReferences.collection].TattooShopItems.GetCount());
		return &m_tattooShopsItems[slotReferences.collection].TattooShopItems[slotReferences.index];
	}

	return NULL;
}

s32 CExtraMetadataMgr::GetTattooShopItemIndex(eTattooFaction faction, s32 collectionHash, s32 presetHash) const
{
	const s32 numberOfTattoosForThisFaction = GetNumTattooShopItemsInFaction(faction);

	bool bIgnoreCollectionHash = false;
	if (collectionHash == -1)
	{
		bIgnoreCollectionHash = true;
	}

	u32 unsignedCollectionHash = static_cast<u32>(collectionHash);
	u32 unsignedPresetHash = static_cast<u32>(presetHash);

	for (s32 loop = 0; loop < numberOfTattoosForThisFaction; loop++)
	{
		const TattooShopItem *pTattoo = GetTattooShopItem(faction, loop);
		if (pTattoo)
		{
			if ( (bIgnoreCollectionHash || (pTattoo->m_collection.GetHash() == unsignedCollectionHash) )
				&& (pTattoo->m_preset.GetHash() == unsignedPresetHash) )
			{
				return loop;
			}				
		}
	}

	return -1;
}

