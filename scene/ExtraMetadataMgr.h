//
// Filename:	ExtraMetadataMgr.h
// Description:	Wrapper for any type of generic extra dlc metadata needed after game release
//
//

#ifndef __INC_EXTRA_METADATA_MGR_H__
#define __INC_EXTRA_METADATA_MGR_H__

// game includes
#include "scene/ExtraMetadataDefs.h"
#include "atl/array.h"
#include "atl/binmap.h"

#if __BANK && !__SPU
#include "bank/list.h"
#endif	// __BANK && !__SPU

#define __EXTRA_METADATA_CONTAINS_SCRIPT_SAVE_DATA_FLAGS	(0)

// Keep in sync with definitions in commands_extrametadata.sch
enum eContentRightsTypes { CRT_TATTOOS = 0, CRT_PED_APPAREL, CRT_WEAPONS, CRT_VEHICLES, CRT_MAX };

struct SShopContentRightData
{
	SShopContentRightData() : m_onLockChangedCBIdx(0), m_rightsChanged(false) { }

	s32 m_onLockChangedCBIdx : 16;
	bool m_rightsChanged : 1;
};

struct SPedDLCPackMetaFiles
{
	SPedDLCPackMetaFiles() { Reset(); }

	void Reset()
	{
		m_dlcNameHash = 0;
		m_varInfoIndices.Reset();
		m_creatureIndices.Reset();
	}

	u32 m_dlcNameHash;
	atArray<s32> m_varInfoIndices;
	atArray<s32> m_creatureIndices;
};

struct SPedDLCMetaFiles
{
	SPedDLCMetaFiles() { Reset(); }

	void Reset()
	{
		m_pedName.Reset();
		m_pedDLCPackMetaFiles.Reset();
	}

	atString m_pedName;
	atArray<SPedDLCPackMetaFiles> m_pedDLCPackMetaFiles;
};

struct SPedDLCMetaFileQueryData
{
	s32 m_storeIndex;
	u32 m_dlcNameHash;
};

static const int COLLECTION_GROW_STEP = 4;
//////////////////////////////////////////////////////////////////////////
// CExtraMetadataMgr
//////////////////////////////////////////////////////////////////////////
class CExtraMetadataMgr
{
public:
	
	CExtraMetadataMgr();
	~CExtraMetadataMgr();
	void Reset();

	static void ClassInit(unsigned initMode);
	static void ClassShutdown(unsigned initMode);
	static void ShutdownDLCMetaFiles(u32 mode);
	static CExtraMetadataMgr& GetInstance() { return *smp_Instance; }

	void ResetShopContentRightsChanged(eContentRightsTypes type) { m_contentRightData[type].m_rightsChanged = false; }
	bool GetShopContentRightsChanged(eContentRightsTypes type) const { return m_contentRightData[type].m_rightsChanged; }

	// == TATTOOS == //
	int GetNumTattooShopItemsInFaction(eTattooFaction faction) const;
	const TattooShopItem* GetTattooShopItem(eTattooFaction faction, int idx) const;
	s32 GetTattooShopItemIndex(eTattooFaction faction, s32 collectionHash, s32 presetHash) const;

	void AddTattooShopItemsCollection(const char* pFilename);
	void RemoveTattooShopItemsCollection(const char* pFilename);
	// == TATTOOS == //

	// == PED APPAREL == //
	void AddShopPedApparel(const char* pFilename);
	void RemoveShopPedApparel(const char* pFilename);

	void FixupGlobalIndices(const CPedModelInfo* pedModelInfo, const CPedVariationInfo* newInfo, const CPedVariationInfoCollection* varInfoCollection);
	bool DoesShopPedApparelHaveRestrictionTag(u32 nameHash, u32 tagHash);
	s32 SetupShopPedComponentQuery(eScrCharacters character, eShopEnum shop, s32 locate, ePedVarComp componentType);
	s32 SetupShopPedPropQuery(eScrCharacters character, eShopEnum shop, s32 locate, eAnchorPoints anchorPoint);
	s32 SetupShopPedOutfitQuery(eScrCharacters character, eShopEnum shop, s32 locate);

#if __EXTRA_METADATA_CONTAINS_SCRIPT_SAVE_DATA_FLAGS
	void SetBitShopPedApparel(int nameHash, int bitFlag);
	bool IsBitSetShopPedApparel(int nameHash, int bitFlag);
	void ClearBitShopPedApparel(int nameHash, int bitFlag);
	void SetShopPedApparelScriptData(u32 nameHash, u8 bitFlags);
#endif	//	__EXTRA_METADATA_CONTAINS_SCRIPT_SAVE_DATA_FLAGS

	u32 GetHashNameForComponent(s32 pedIndex, ePedVarComp componentType, u32 drawableIndex, u32 textureIndex);
	u32 GetHashNameForProp(s32 pedIndex, eAnchorPoints anchorPoint, u32 propIndex, u32 textureIndex);
	u32 GetHashNameForProp(const CPed* pPed, eAnchorPoints anchorPoint, u32 propIndex, u32 textureIndex);
	s32 GetShopPedApparelForcedPropsCount(u32 nameHash);
	s32 GetShopPedApparelForcedComponentsCount(u32 nameHash);
	void GetForcedProp(u32 nameHash, u32 forcedPropIndex, s32& forcedPropNameHash, s32& forcedPropEnumValue, s32& forcedPropAnchor);
	void GetForcedComponent(u32 nameHash, u32 forcedComponentIndex, s32& forcedComponentNameHash, s32& forcedComponentEnumValue, s32& forcedComponentType);
	s32 GetShopPedApparelVariantPropsCount(u32 nameHash);
	s32 GetShopPedApparelVariantComponentsCount(u32 nameHash);
	void GetVariantProp(u32 nameHash, u32 variantPropIndex, s32& variantPropNameHash, s32& variantPropEnumValue, s32& variantPropAnchor);
	void GetVariantComponent(u32 nameHash, u32 variantComponentIndex, s32& variantComponentNameHash, s32& variantComponentEnumValue, s32& variantComponentType);
	ShopPedComponent* GetShopPedQueryComponent(u32 index);
	ShopPedProp* GetShopPedQueryProp(u32 index);
	ShopPedOutfit* GetShopPedQueryOutfit(u32 index);

	s32 GetShopPedQueryComponentIndex(u32 nameHash);
	s32 GetShopPedQueryPropIndex(u32 nameHash);

	BaseShopPedApparel* GetShopPedApparel(u32 nameHash);
	ShopPedComponent* GetShopPedComponent(u32 nameHash);
	ShopPedProp* GetShopPedProp(u32 nameHash);
	ShopPedOutfit* GetShopPedOutfit(u32 nameHash);
	eScrCharacters GetShopPedApparelCharacter(u32 nameHash);
	const atArray<ShopPedApparel*>& GetShopPedApparelArray() { return m_shopPedApparel; }
	void GetCreatureMetaDataIndices(CPedModelInfo* lookupModelInfo, atFixedArray<SPedDLCMetaFileQueryData, STREAMING_MAX_DEPENDENCIES>& targetArray);
	// == PED APPAREL == //

	// == VEHICLES ==//
	void AddVehiclesCollection(const char* pFilename);
	void RemoveVehiclesCollection(const char* pFilename);
	void BuildVehicleLookup();
	int  GetNumDLCVehicles() const { return m_vehiclesLookup.GetCount(); }
	const ShopVehicleData* GetShopVehicleData(int idx) const;
	int  GetShopVehicleModelNameHash(int idx) const;
	// == VEHICLES ==//
	
	// == VEHICLE MODS ==//
	int	GetNumDLCVehicleMods(int dlcIdx)const;
	const ShopVehicleMod* GetShopVehicleModData(int dlcIndex, int modIndex)const;
	const atArray<ShopVehicleMod*>& GetGenericVehicleModList();

	atArray<ShopVehicleMod*> m_shopNSVehicleModQuery;
	// == VEHICLE MODS ==//

	// == WEAPONS == //
    void AddWeaponShopItemsCollection(const char* pFilename);
    void RemoveWeaponShopItemsCollection(const char* pFilename);
	void BuildWeaponLookup();
    int GetNumDLCWeapons()const {return m_weaponShopItemsLookup.GetCount();}
	int GetNumDLCWeaponsSP()const;
	s32 GetIndexForSPWeapons(int index) const;
    const WeaponShopItem* GetDLCWeaponShopItem(int idx) const;
	const ShopWeaponComponent* GetShopWeaponComponentData(int dlcIdx, int componentIdx) const;
	const int GetNumDLCWeaponComponents(int idx) const;
	const int GetNumDLCWeaponComponentsSP(int idx) const;
    // == WEAPONS == //

#if __BANK && !__SPU
	static void CreateBankWidgets(bkBank& bank);
	void RefreshBankWidgets();
#endif

private:

    template <typename ARRAY_TYPE>
    int FindMetaItemsCollectionIndex(const ARRAY_TYPE & dataArray, const char* collectionName) const
    {
        atHashString collectionNameHash(collectionName);
        for (int i=0; i<dataArray.GetCount(); ++i)
        {
            if (dataArray[i].m_nameHash == collectionNameHash)
            {
                return i;
            }
        }
        return -1;
    }

    template <typename ARRAY_TYPE>
    typename ARRAY_TYPE::value_type * AddMetaItem(ARRAY_TYPE & dataArray, const char* pFileName, int * index = NULL)
    {
        if( FindMetaItemsCollectionIndex(dataArray,pFileName) < 0 )
        {
            typename ARRAY_TYPE::value_type & newItem = dataArray.Grow(COLLECTION_GROW_STEP);
            if( PARSER.LoadObject(pFileName, NULL, newItem) ) 
            {
                if (index)
                    *index = dataArray.GetCount()-1;
                newItem.m_nameHash.SetFromString(pFileName);
                return &newItem;
            }
            else
            {
                dataArray.Pop();
                Warningf("CExtraMetadataMgr::AddMetaItem: failed to load objects file: %s",pFileName);
            }
        }
        return NULL;
    }

    template <typename ARRAY_TYPE>
    bool RemoveMetaItemPtr(ARRAY_TYPE & dataArray, const char* pFileName)
    {
        const int collectionIdx = FindMetaItemsCollectionIndexPtr(dataArray,pFileName);
        if( collectionIdx >= 0 )
        {
			delete dataArray[collectionIdx];
            dataArray.DeleteFast(collectionIdx);
            return true;
        }
        return false;
    }

	template <typename ARRAY_TYPE>
	int FindMetaItemsCollectionIndexPtr(const ARRAY_TYPE & dataArray, const char* collectionName) const
	{
		atHashString collectionNameHash(collectionName);
		for (int i=0; i<dataArray.GetCount(); ++i)
		{
			if (dataArray[i]->m_nameHash == collectionNameHash)
			{
				return i;
			}
		}
		return -1;
	}

	template <class T>
	T* AddMetaItemPtr(atArray<T*>& dataArray, const char* pFileName, int * index = NULL)
	{
		if( FindMetaItemsCollectionIndexPtr(dataArray,pFileName) < 0 )
		{
			T* newItem = rage_new T();

			dataArray.PushAndGrow(newItem, COLLECTION_GROW_STEP);

			if( PARSER.LoadObject(pFileName, NULL, *newItem) ) 
			{
				if (index)
					*index = dataArray.GetCount()-1;
				newItem->m_nameHash.SetFromString(pFileName);
				return newItem;
			}
			else
			{
				delete newItem;
				dataArray.Pop();
				Warningf("CExtraMetadataMgr::AddMetaItemPtr: failed to load objects file: %s",pFileName);
			}
		}
		return NULL;
	}
            
    void OnTattooContentLockChangedCB(u32 hash, bool value);
	void OnVehicleContentLockChangedCB(u32 hash, bool value);
	void OnPedApparelContentLockChangedCB(u32 hash, bool value);
	void OnWeaponContentLockChangedCB(u32 hash, bool value);

	// == TATTOOS == //
	void BuildTattooShopItemsLookup();

	struct tattooLookupEntry
	{
		u16 collection;
		u16 index;
	};

	atRangeArray< atArray<tattooLookupEntry>, eTattooFaction_NUM_ENUMS > m_tattooShopItemsLookup;	//< Lookup table to easily allow looping tattoos by faction
	atArray< TattooShopItemArray > m_tattooShopsItems;
	// == TATTOOS == //

	// == PED APPAREL == //
	void RebuildApparelLookup();

	struct ShopPedApparelLookupEntry
	{
		ShopPedApparelLookupEntry() { }
		ShopPedApparelLookupEntry(eShopPedApparel apparelType, u32 apparelIndex, u32 typeIndex) : 
								m_apparelType((s8)apparelType), m_shopPedApparelIndex(apparelIndex), m_shopPedTypeIndex(typeIndex) {	}

		s32 m_apparelType : 8;
		u32 m_shopPedApparelIndex : 16;
		u32 m_shopPedTypeIndex : 16;
	};

	atArray<ShopPedApparel*> m_shopPedApparel;
	atBinaryMap<ShopPedApparelLookupEntry, u32> m_shopPedApparelLookup;
	atArray<ShopPedApparelLookupEntry> m_shopPedApparelCompQuery;
	atArray<ShopPedApparelLookupEntry> m_shopPedApparelPropQuery;
	atArray<ShopPedApparelLookupEntry> m_shopPedApparelOutfitQuery;
	// == PED APPAREL == //

	// == PED APPAREL == //
	atArray<SPedDLCMetaFiles> m_pedDLCMetaFiles;

	SPedDLCMetaFiles* FindPedDLCMetaFiles(const char* pedName);
	SPedDLCPackMetaFiles* FindPedDLCPackMetaFiles(SPedDLCMetaFiles* parentInfo, u32 dlcNameHash);

	void AddPedDLCMetaFiles(const char* pedName, u32 dlcNameHash, u32 varInfoNameHash, u32 creatureMetaNameHash);
	void RemovePedDLCMetaFiles(const char* pedName, u32 dlcNameHash, u32 varInfoNameHash, u32 creatureMetaNameHash);
	void RemoveAllPedDLCMetaFiles();
	bool IsModelInMemory(fwModelId modelInfo);
	// == PED APPAREL == //

	// == VEHICLES == //
	atArray< ShopVehicleDataArray > m_vehicles;
	atArray< ShopVehicleData* >	m_vehiclesLookup;
	atArray< ShopVehicleMod* >	m_vehicleGenericModsLookup;
	// == VEHICLES == //
	
	// == WEAPONS == //
    atArray< WeaponShopItem* > m_weaponShopItemsLookup;
    atArray< WeaponShopItemArray > m_weaponShopItems;
	// == WEAPONS == //

	SShopContentRightData m_contentRightData[CRT_MAX];
	static CExtraMetadataMgr* smp_Instance;

#if __BANK && !__SPU
	void WarpToDLCInteriorCB();
	void InteriorRefreshCB();
	void WeaponRefreshCB();
	void VehicleRefreshCB();
	void TattooRefreshCB();
	void DebugPrintTattooShopItems() const;
	void TattooDblClickCB(s32 idx);
	void TattooClearCB();
	void ComponentDblClickCB(s32 nameHash);
	void PropDblClickCB(s32 nameHash);
	void OutfitDblClickCB(s32 nameHash);
	void WeaponEquipCB();
	void WeaponComboCB();
	void VehicleComboCB();	
	void VehicleCreateCB();
	void BuildDLCNamesArray(atArray<const char*>& targetArray, s32 characterIdx);
	void CompCharComboCB();
	void CompDLCNameComboCB();
	void PropCharComboCB();
	void PropDLCNameComboCB();
	void OutfitCharComboCB();
	void OutfitDLCNameComboCB();
	static void ShowPreviousComponentPage();
	static void ShowNextComponentPage();
	static void ShowPreviousPropPage();
	static void ShowNextPropPage();
	static void ShowPreviousOutfitPage();
	static void ShowNextOutfitPage();
	void ClickProxyInFile(s32 index);
	void ClickProxyInScratch(s32 index);
	void CheckoutProxyFile();
	void CheckOutFile(const char* currOrderDataPath);
	void EditProxyFromIndex();
	void ProxyIndexComboCB();
	void MoveProxyFromFileCB();
	void MoveProxyToFileCB();
	void MoveAllProxiesToFileCB();
	void LookupShopPedComponent();
	void LookupShopPedProp();
	void LookupShopPedOutfit();
	void AddComponentToBankList(ShopPedComponent* currentItem, s32 index);
	void AddPropToBankList(ShopPedProp* currentItem, s32 index);
	void AddOutfitToBankList(ShopPedOutfit* currentItem, s32 index);
	void DisplayPedComponents(s32 nextPage, u32 lookupHash, bool buildDLCNames);
	void DisplayPedProps(s32 nextPage, u32 lookupHash, bool buildDLCNames);
	void DisplayPedOutfits(s32 nextPage, u32 lookupHash, bool buildDLCNames);
	void DisplayTattoos(s32 nextPage, u32 lookupHash);
	void PopulateVehicleNames();
	void PopulateWeaponNames();
	void SetupShopPedApparelWidgets(bkBank& bank);
	void SetupShopTattooWidgets(bkBank& bank);
	void SetupShopWeaponWidgets(bkBank& bank);
	void SetupShopVehicleWidgets(bkBank& bank);
	void SetupInteriorWidgets(bkBank& bank);

	static s32 m_currentOutfitPg;
	static s32 m_currentComponentPg;
	static s32 m_currentPropPg;
	static bkList* m_bankComponentList;
	static bkList* m_bankTattooList;
	static bkList* m_bankVehicleModList;
	static bkList* m_bankGenericVehicleModList;
	static bkList* m_bankTattooLookupList;
	static bkList* m_bankPropList;
	static bkList* m_bankOutfitList;
	static bkCombo* m_bankVehicleCombo;
	static char m_shopPedComponentLookup[RAGE_MAX_PATH];
	static char m_shopPedComponentCounts[RAGE_MAX_PATH];
	static char m_shopPedPropLookup[RAGE_MAX_PATH];
	static char m_shopPedPropCounts[RAGE_MAX_PATH];
	static char m_shopPedOutfitLookup[RAGE_MAX_PATH];
	static char m_shopPedOutfitCounts[RAGE_MAX_PATH];
	static char m_proxyFromIndex[RAGE_MAX_PATH];
	static char m_proxyToIndex[RAGE_MAX_PATH];
	static s32	m_sWeaponListIdx;
	static s32	m_sVehicleComboIdx;
	static s32	m_sVehicleComboPrevIdx;
	static s32	m_sNumOfListedMods;
	static s32	m_sNumOfListedGenericMods;
	atArray<const char*> m_vehicleNames;
	atArray<const char*> m_weaponNames;
	static bkText* m_proxyFromIdxTxt;
	static bkText* m_proxyToIdxTxt;
	static s32 m_clickedProxyInFileHash;
	static s32 m_clickedProxyInScratchHash;
	static bkCombo* m_bankProxyIndicesMetaCombo;
	static bkList* m_bankProxyIndicesInFileList;
	static bkList* m_bankProxyIndicesInScratchList;
	static s32 ms_ProxyIndexComboIdx;
	static atBinaryMap<const char*, u32> m_proxyOrderLookup;
	static bkCombo* m_bankWeaponCombo;
	static s32	m_sWeaponComboIdx;
	static s32	m_sNumOfListedWeaponComponents;
	static s32  m_sNumOfListedTattoos;
	static bkList* m_bankWeaponComponentsList; 
	static bkCombo* m_compCharCombo;
	static s32 m_compCharComboIdx;
	static bkCombo* m_compDLCNameCombo;
	static s32 m_compDLCNameComboIdx;
	static bkCombo* m_propCharCombo;
	static s32 m_propCharComboIdx;
	static bkCombo* m_propDLCNameCombo;
	static s32 m_propDLCNameComboIdx;
	static bkCombo* m_outfitCharCombo;
	static s32 m_outfitCharComboIdx;
	static bkCombo* m_outfitDLCNameCombo;
	static s32 m_outfitDLCNameComboIdx;
	static atArray<const char*> ms_outfitDLCNames;
	static atArray<const char*> ms_propDLCNames;
	static atArray<const char*> ms_compDLCNames;
	static bkCombo* ms_bankInteriorCombo;
	static s32 ms_dlcInteriorComboIdx;
	static atArray<s32> ms_dlcInteriorMapSlots;
#endif
};

#define EXTRAMETADATAMGR CExtraMetadataMgr::GetInstance()

#endif // __INC_EXTRA_METADATA_MGR_H__
