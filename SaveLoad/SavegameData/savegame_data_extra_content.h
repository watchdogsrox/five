#ifndef SAVEGAME_DATA_EXTRA_CONTENT_H
#define SAVEGAME_DATA_EXTRA_CONTENT_H

// Rage headers
#include "atl/array.h"
#include "parser/macros.h"



class CExtraContentSaveStructure
{
public:

	class CInstalledPackagesStruct
	{
	public:

		CInstalledPackagesStruct()
		{
			m_nameHash = 0;
		}

		u32 m_nameHash;

		PAR_SIMPLE_PARSABLE
	};

	atArray<CInstalledPackagesStruct> m_InstalledPackages;

#if 0	//	__EXTRA_METADATA_CONTAINS_SCRIPT_SAVE_DATA_FLAGS
	class CBaseShopPedApparelScriptData
	{
	public:
		u32 m_ItemHash;
		u8 m_DataBits;

		PAR_SIMPLE_PARSABLE
	};

	atArray<CBaseShopPedApparelScriptData> m_pedComponentItemScriptData;
	atArray<CBaseShopPedApparelScriptData> m_pedPropItemScriptData;
#endif	//	__EXTRA_METADATA_CONTAINS_SCRIPT_SAVE_DATA_FLAGS

	CExtraContentSaveStructure();
	~CExtraContentSaveStructure();

	void PreSave();
	void PostLoad();

	PAR_SIMPLE_PARSABLE
};


#endif // SAVEGAME_DATA_EXTRA_CONTENT_H

