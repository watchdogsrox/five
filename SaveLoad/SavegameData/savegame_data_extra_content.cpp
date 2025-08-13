
#include "savegame_data_extra_content.h"
#include "savegame_data_extra_content_parser.h"


// Rage headers
#include "grcore/config.h"

// Game headers
#include "SaveLoad/GenericGameStorage.h"
#include "scene/ExtraMetadataMgr.h"



CExtraContentSaveStructure::CExtraContentSaveStructure()
{
	m_InstalledPackages.Reset();

#if __EXTRA_METADATA_CONTAINS_SCRIPT_SAVE_DATA_FLAGS
	m_pedComponentItemScriptData.Reset();
	m_pedPropItemScriptData.Reset();
#endif	//	__EXTRA_METADATA_CONTAINS_SCRIPT_SAVE_DATA_FLAGS
}

CExtraContentSaveStructure::~CExtraContentSaveStructure()
{
	m_InstalledPackages.Reset();

#if __EXTRA_METADATA_CONTAINS_SCRIPT_SAVE_DATA_FLAGS
	m_pedComponentItemScriptData.Reset();
	m_pedPropItemScriptData.Reset();
#endif	//	__EXTRA_METADATA_CONTAINS_SCRIPT_SAVE_DATA_FLAGS
}

void CExtraContentSaveStructure::PreSave()
{
	if (!CGenericGameStorage::AllowPreSaveAndPostLoad("CExtraContentSaveStructure::PreSave"))
	{
		return;
	}

	for (u32 i = 0; i < EXTRACONTENT.GetContentCount(); i++)
	{
		CInstalledPackagesStruct InstalledPackage;

		InstalledPackage.m_nameHash = EXTRACONTENT.GetContentHash(i);

		m_InstalledPackages.PushAndGrow(InstalledPackage);
	}

#if __EXTRA_METADATA_CONTAINS_SCRIPT_SAVE_DATA_FLAGS
	const atArray<ShopPedApparel*>& shopPedApparel = EXTRAMETADATAMGR.GetShopPedApparelArray();

	for (u32 i = 0; i < shopPedApparel.GetCount(); i++)
	{
		const ShopPedApparel& currShopPedApparel = *shopPedApparel[i];

		for (u32 j = 0; j < currShopPedApparel.m_pedComponents.GetCount(); j++)
		{
			const ShopPedComponent& item = currShopPedApparel.m_pedComponents[j];
			CBaseShopPedApparelScriptData saveItem;

			saveItem.m_ItemHash = item.m_uniqueNameHash.GetHash();
			saveItem.m_DataBits = item.m_scriptSaveData;

			m_pedComponentItemScriptData.PushAndGrow(saveItem, (u16)currShopPedApparel.m_pedComponents.GetCount());
		}

		for (u32 j = 0; j < currShopPedApparel.m_pedProps.GetCount(); j++)
		{
			const ShopPedProp& item = currShopPedApparel.m_pedProps[j];
			CBaseShopPedApparelScriptData saveItem;

			saveItem.m_ItemHash = item.m_uniqueNameHash.GetHash();
			saveItem.m_DataBits = item.m_scriptSaveData;

			m_pedPropItemScriptData.PushAndGrow(saveItem, (u16)currShopPedApparel.m_pedProps.GetCount());
		}
	}
#endif	//	__EXTRA_METADATA_CONTAINS_SCRIPT_SAVE_DATA_FLAGS
}

void CExtraContentSaveStructure::PostLoad()
{
	if (!CGenericGameStorage::AllowPreSaveAndPostLoad("CExtraContentSaveStructure::PostLoad"))
	{
		return;
	}

	EXTRACONTENT.ResetSaveGameInstalledPackagesInfo();

	for (u32 i = 0; i < m_InstalledPackages.GetCount(); i++)
	{
		EXTRACONTENT.SetSaveGameInstalledPackage(m_InstalledPackages[i].m_nameHash);
	}

#if __EXTRA_METADATA_CONTAINS_SCRIPT_SAVE_DATA_FLAGS
	for (int i = 0; i < m_pedComponentItemScriptData.GetCount(); i++)
		EXTRAMETADATAMGR.SetShopPedApparelScriptData(m_pedComponentItemScriptData[i].m_ItemHash, m_pedComponentItemScriptData[i].m_DataBits);

	for (int i = 0; i < m_pedPropItemScriptData.GetCount(); i++)
		EXTRAMETADATAMGR.SetShopPedApparelScriptData(m_pedPropItemScriptData[i].m_ItemHash, m_pedPropItemScriptData[i].m_DataBits);
#endif	//	__EXTRA_METADATA_CONTAINS_SCRIPT_SAVE_DATA_FLAGS
}
