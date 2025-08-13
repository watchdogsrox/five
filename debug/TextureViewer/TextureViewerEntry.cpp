// ==========================================
// debug/textureviewer/textureviewerentry.cpp
// (c) 2010 RockstarNorth
// ==========================================

#include "debug/textureviewer/textureviewerprivate.h"

#if __BANK

// Rage headers
#include "grcore/image.h"
#include "grcore/texture.h"
#include "grcore/texturedebugflags.h"
#include "system/memops.h"

#include "fragment/drawable.h"

// Framework headers
#include "fwutil/xmacro.h"
#include "fwscene/stores/txdstore.h"
#include "fwscene/stores/dwdstore.h"
#include "fwscene/stores/drawablestore.h"
#include "fwscene/stores/fragmentstore.h"
#include "fwscene/stores/mapdatastore.h"

// Game headers
#include "core/game.h"
#include "modelinfo/modelinfo.h"
#include "modelinfo/pedmodelinfo.h"
#include "modelinfo/vehiclemodelinfo.h"
#include "shaders/ShaderEdit.h"

#include "debug/textureviewer/textureviewerutil.h"
#include "debug/textureviewer/textureviewerentry.h"
#include "debug/textureviewer/textureviewerwindow.h"
#include "scene/texLod.h"

DEBUG_TEXTURE_VIEWER_OPTIMISATIONS()

// ================================================================================================

PARAM(dtv,"dtv");

CDTVColumnFlags::CDTVColumnFlags()
{
	for (int i = 0; i < CID_COUNT; i++)
	{
		m_visible  [i] = true;
		m_columnIDs[i] = CID_NONE;
	}

	const bool bDev3 = false;                // dev level 3 = true never
	const bool bDev2 = PARAM_dtv.Get();      // dev level 2 = true if -dtv command line arg is present
	const bool bDev1 = __DEV ? true : bDev2; // dev level 1 = true in beta build
	const bool bDev0 = true;                 // dev level 0 = true always

	m_visible[CID_LineNumber        ] = bDev3;
	m_visible[CID_Name              ] = bDev0;
	m_visible[CID_RPFPathName       ] = bDev2;
//	m_visible[CID_SourceTemplate    ] = bDev2;
//	m_visible[CID_SourceImage       ] = bDev2;
	m_visible[CID_ShaderName        ] = bDev2;
	m_visible[CID_ShaderVar         ] = bDev2;
	m_visible[CID_ModelInfoIndex    ] = bDev3;
	m_visible[CID_ModelInfoType     ] = bDev3;
	m_visible[CID_ModelInfoFlag     ] = bDev1;
	m_visible[CID_AssetLoaded       ] = bDev1;
	m_visible[CID_AssetName         ] = bDev0;
	m_visible[CID_AssetRef          ] = bDev0;
	m_visible[CID_AssetStreamID     ] = bDev3;
	m_visible[CID_AssetSizeVirt     ] = bDev3;
	m_visible[CID_AssetSizePhys     ] = bDev0;
	m_visible[CID_TextureSizePhys   ] = __XENON ? bDev2 : bDev3;
	m_visible[CID_TextureSizeIdeal  ] = bDev0;
	m_visible[CID_TextureSizeWaste  ] = bDev2;
	m_visible[CID_NumTxds           ] = bDev0;
	m_visible[CID_NumTextures       ] = bDev0;
	m_visible[CID_Dimensions        ] = bDev0;
	m_visible[CID_DimensionMax      ] = bDev0;
	m_visible[CID_NumMips           ] = bDev0;
	m_visible[CID_TexFormatBPP      ] = bDev0;
	m_visible[CID_TexFormat         ] = bDev0;
	m_visible[CID_TexFormatSwizzle  ] = bDev2;
	m_visible[CID_TexConversionFlags] = bDev2;
	m_visible[CID_TexTemplateType   ] = bDev2;
	m_visible[CID_TexAddress        ] = bDev3;
	m_visible[CID_TexCRC            ] = bDev3;
	m_visible[CID_Info              ] = bDev1;
	m_visible[CID_IsHD              ] = bDev0 && !PARAM_dtv.Get(); // temporary hack

	m_columnMaxChars_Name      = GetColumnMaxChars(CID_Name     , NULL);
	m_columnMaxChars_AssetName = GetColumnMaxChars(CID_AssetName, NULL);
}

__COMMENT(static) const char* CDTVColumnFlags::GetColumnName(eColumnID columnID)
{
	switch ((int)columnID)
	{
	case CID_LineNumber         : return "line"        ;
	case CID_Name               : return "name"        ;
	case CID_RPFPathName        : return "RPF"         ;
//	case CID_SourceTemplate     : return "template"    ;
//	case CID_SourceImage        : return "image"       ;
	case CID_ShaderName         : return "shader"      ;
	case CID_ShaderVar          : return "var"         ;
	case CID_ModelInfoIndex     : return "index"       ;
	case CID_ModelInfoType      : return "type"        ;
	case CID_ModelInfoFlag      : return "?"           ;
	case CID_AssetLoaded        : return "loaded"      ;
	case CID_AssetName          : return "assetname"   ;
	case CID_AssetRef           : return "assetref"    ;
	case CID_AssetStreamID      : return "streamID"    ;
	case CID_AssetSizeVirt      : return "virtual"     ;
	case CID_AssetSizePhys      : return "physical"    ;
	case CID_TextureSizePhys    : return "texsizeph"   ;
	case CID_TextureSizeIdeal   : return "texsize"     ;
	case CID_TextureSizeWaste   : return "texwaste"    ;
	case CID_NumTxds            : return "#txd"        ;
	case CID_NumTextures        : return "#tex"        ;
	case CID_Dimensions         : return "dims"        ;
	case CID_DimensionMax       : return "maxdim"      ;
	case CID_NumMips            : return "mips"        ;
	case CID_TexFormatBPP       : return "bpp"         ;
	case CID_TexFormat          : return "format"      ;
	case CID_TexFormatSwizzle   : return "swiz"        ;
	case CID_TexConversionFlags : return "conversion"  ;
	case CID_TexTemplateType    : return "templatetype";
	case CID_TexAddress         : return "texaddr"     ;
	case CID_TexCRC             : return "crc"         ;
	case CID_Info               : return "info"        ;
	case CID_IsHD               : return "HD"          ;
	}

	return "";
}

__COMMENT(static) int CDTVColumnFlags::GetColumnMaxChars(eColumnID columnID, const CDTVColumnFlags* overrides)
{
	if (overrides)
	{
		if (columnID == CID_Name)
		{
			return Min<int>(overrides->m_columnNumChars[CID_Name], overrides->m_columnMaxChars_Name);
		}
		else if (columnID == CID_AssetName)
		{
			return Min<int>(overrides->m_columnNumChars[CID_AssetName], overrides->m_columnMaxChars_AssetName);
		}
		else
		{
			return overrides->m_columnNumChars[columnID];
		}
	}

	const bool bShowBothNumTxds = __DEV ? true : false;

	switch ((int)columnID)
	{
	case CID_LineNumber         : return  5;
	case CID_Name               : return 80;
	case CID_RPFPathName        : return 30;
//	case CID_SourceTemplate     : return 30;
//	case CID_SourceImage        : return 30;
	case CID_ShaderName         : return 20;
	case CID_ShaderVar          : return 20;
	case CID_ModelInfoIndex     : return  5;
	case CID_ModelInfoType      : return  4;
	case CID_ModelInfoFlag      : return  2; // maybe 3?
	case CID_AssetLoaded        : return  6;
	case CID_AssetName          : return 16;
	case CID_AssetRef           : return 12;
	case CID_AssetStreamID      : return  8;
	case CID_AssetSizeVirt      : return  9;
	case CID_AssetSizePhys      : return  9;
	case CID_TextureSizePhys    : return  9;
	case CID_TextureSizeIdeal   : return  9;
	case CID_TextureSizeWaste   : return  9;
	case CID_NumTxds            : return bShowBothNumTxds ? 5 : 4;
	case CID_NumTextures        : return  4;
	case CID_Dimensions         : return  9;
	case CID_DimensionMax       : return  6;
	case CID_NumMips            : return  4;
	case CID_TexFormatBPP       : return  3;
	case CID_TexFormat          : return 13;
	case CID_TexFormatSwizzle   : return  4;
	case CID_TexConversionFlags : return  7;
	case CID_TexTemplateType    : return 14;
	case CID_TexAddress         : return 10;
	case CID_TexCRC             : return 10;
	case CID_Info               : return 12;
	case CID_IsHD               : return  3;
	}

	return 0;
}

__COMMENT(static) bool CDTVColumnFlags::GetColumnTruncate(eColumnID columnID)
{
	if (columnID == CID_Name || columnID == CID_AssetName)
	{
		return true;
	}

	return false;
}

CDTVColumnFlags::eColumnID CDTVColumnFlags::GetColumnID(int column) const
{
	if (column >= 0 && column < CID_COUNT)
	{
		return m_columnIDs[column];
	}

	return CID_NONE;
}

// ================================================================================================

int CDTVEntry::SetupWindow(CDebugTextureViewerWindow* debugWindow, CDTVColumnFlags& cf) const
{
	const int charWidth   = 6; // uses 6x8 font
	const int spacing     = 6; // extra spacing between cells
	int       offset      = 1; // initial offset
	int       columnIndex = 0;

	debugWindow->DeleteColumns(); // necessary if we're resetting the columns without resetting the entire debugwindow

	for (int i = 0; i < CDTVColumnFlags::CID_COUNT; i++)
	{
		const CDTVColumnFlags::eColumnID id = (const CDTVColumnFlags::eColumnID)i;

		cf.m_columnIDs[id] = CDTVColumnFlags::CID_NONE;

		if (cf.m_visible[id] && IsColumnRelevant(id))
		{
			const char* columnName     = CDTVColumnFlags::GetColumnName    (id);
			const int   columnMaxChars = CDTVColumnFlags::GetColumnMaxChars(id, &cf);

			debugWindow->AddColumn(columnName, (float)offset);
			offset += spacing + charWidth*columnMaxChars;

			cf.m_columnIDs[columnIndex++] = id; // note that columnIndex <= id before ++
		}
	}

	debugWindow->SetWidth((float)(offset + spacing));

	return offset;
}

// ================================================================================================

CDTVEntry_Generic::CDTVEntry_Generic(const char* name) : CDTVEntry(name)
{
}

__COMMENT(virtual) float CDTVEntry_Generic::GetTextAndSortKey(char* result, CDTVColumnFlags::eColumnID columnID) const
{
	float sortKey = 0.0f;

	if (result)
	{
		result[0] = '\0';
	}

	if (columnID == CDTVColumnFlags::CID_LineNumber)
	{
		sortKey = (float)(-m_lineNumber);
		if (result) { sprintf(result, "%d", m_lineNumber); }
	}
	else if (columnID == CDTVColumnFlags::CID_Name)
	{
		if (result) { strcpy(result, m_name.c_str()); }
	}

	return sortKey;
}

// ================================================================================================

CDTVEntry_ModelInfo::CDTVEntry_ModelInfo(const char* name, u32 modelInfoIndex, const CEntity* entity) : CDTVEntry(name)
{
	m_modelInfo      = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelInfoIndex)));
	m_modelInfoIndex = modelInfoIndex;
	m_entity         = RegdEnt(const_cast<CEntity*>(entity));
}

__COMMENT(virtual) void CDTVEntry_ModelInfo::Clear()
{
	m_modelInfo      = NULL;
	m_modelInfoIndex = fwModelId::MI_INVALID; 
	m_entity         = NULL;

	CDTVEntry::Clear();
}

// this is sort of experimental, see if we can load a modelinfo's resources .. do we need to print some debug info?
static bool _LoadAsset_AddRef(eAssetType assetType, int assetIndex, const CAssetRefInterface& ari)
{
	if (assetIndex != INDEX_NONE)
	{
		CTxdRef ref(assetType, assetIndex, assetType == AST_DwdStore ? 0 : INDEX_NONE, "");
		const fwTxd* txd = ref.LoadTxd(ari.m_verbose);

		if (txd)
		{
			ref.AddRef(ari);
			return true;
		}
	}

	return false;
}

__COMMENT(virtual) void CDTVEntry_ModelInfo::Load(const CAssetRefInterface& ari)
{
	if (m_modelInfo)
	{
		_LoadAsset_AddRef(AST_TxdStore     , m_modelInfo->GetAssetParentTxdIndex(), ari);
		_LoadAsset_AddRef(AST_DwdStore     , m_modelInfo->SafeGetDrawDictIndex  (), ari);
		_LoadAsset_AddRef(AST_DrawableStore, m_modelInfo->SafeGetDrawableIndex  (), ari);
		_LoadAsset_AddRef(AST_FragmentStore, m_modelInfo->SafeGetFragmentIndex  (), ari);
		_LoadAsset_AddRef(AST_ParticleStore, m_modelInfo->GetPtFxAssetSlot      (), ari);

		if (m_modelInfo->GetModelType() == MI_TYPE_PED)
		{
			const CPedModelInfo* pedModelInfo = (const CPedModelInfo*)m_modelInfo;

			_LoadAsset_AddRef(AST_TxdStore, pedModelInfo->GetHDTxdIndex(), ari);

			const strLocalIndex pedCompDwdIndex = strLocalIndex(pedModelInfo->GetPedComponentFileIndex());
			const strLocalIndex pedPropDwdIndex = strLocalIndex(pedModelInfo->GetPropsFileIndex());

			if (pedCompDwdIndex != INDEX_NONE)
			{
				_LoadAsset_AddRef(AST_TxdStore, g_DwdStore.GetParentTxdForSlot(strLocalIndex(pedCompDwdIndex)).Get(), ari);
			}

			if (pedPropDwdIndex != INDEX_NONE)
			{
				_LoadAsset_AddRef(AST_TxdStore, g_DwdStore.GetParentTxdForSlot(strLocalIndex(pedPropDwdIndex)).Get(), ari);
			}
		}
		else if (m_modelInfo->GetModelType() == MI_TYPE_VEHICLE)
		{
			const CVehicleModelInfo* vehModelInfo = (const CVehicleModelInfo*)m_modelInfo;

			_LoadAsset_AddRef(AST_TxdStore     , vehModelInfo->GetHDTxdIndex     (), ari);
			_LoadAsset_AddRef(AST_FragmentStore, vehModelInfo->GetHDFragmentIndex(), ari);
		}
	}
}

//__COMMENT(virtual) void CDTVEntry_ModelInfo::Select(const CAssetRefInterface& ari);

__COMMENT(virtual) atString CDTVEntry_ModelInfo::GetDesc() const
{
	atString desc = CDTVEntry::GetDesc();

	if (m_modelInfo)
	{
		if (1) // show dependencies
		{
			if (desc.length() > 0)
			{
				desc += "\n";
			}

			desc += atVarString("-> %s\n", CAssetRef(AST_TxdStore     , m_modelInfo->GetAssetParentTxdIndex()).GetDesc().c_str());
			desc += atVarString("-> %s\n", CAssetRef(AST_DwdStore     , m_modelInfo->SafeGetDrawDictIndex  ()).GetDesc(m_modelInfo->SafeGetDrawDictEntry()).c_str());
			desc += atVarString("-> %s\n", CAssetRef(AST_DrawableStore, m_modelInfo->SafeGetDrawableIndex  ()).GetDesc().c_str());
			desc += atVarString("-> %s\n", CAssetRef(AST_FragmentStore, m_modelInfo->SafeGetFragmentIndex  ()).GetDesc().c_str());
			desc += atVarString("-> %s\n", CAssetRef(AST_ParticleStore, m_modelInfo->GetPtFxAssetSlot      ()).GetDesc().c_str());

			desc += "\n";

			desc += atVarString("HasLoaded = %s\n", m_modelInfo->GetHasLoaded() ? "TRUE" : "FALSE");
		}
	}

	return desc;
}

__COMMENT(virtual) const CEntity* CDTVEntry_ModelInfo::GetEntity() const
{
	return m_entity;
}

__COMMENT(virtual) bool CDTVEntry_ModelInfo::IsColumnRelevant(CDTVColumnFlags::eColumnID columnID) const
{
	if (CDTVEntry::IsColumnRelevant(columnID))
	{
		return true;
	}

	switch ((int)columnID)
	{
	case CDTVColumnFlags::CID_ModelInfoIndex : return true;
	case CDTVColumnFlags::CID_ModelInfoType  : return true;
	case CDTVColumnFlags::CID_ModelInfoFlag  : return true;
	case CDTVColumnFlags::CID_AssetLoaded    : return true;
	case CDTVColumnFlags::CID_NumTxds        : return true;
	case CDTVColumnFlags::CID_NumTextures    : return true;
	case CDTVColumnFlags::CID_DimensionMax   : return true;
	}

	return false;
}

__COMMENT(virtual) float CDTVEntry_ModelInfo::GetTextAndSortKey(char* result, CDTVColumnFlags::eColumnID columnID) const
{
	float sortKey = 0.0f;

	if (result)
	{
		result[0] = '\0';
	}

	if (columnID == CDTVColumnFlags::CID_LineNumber)
	{
		sortKey = (float)(-m_lineNumber);
		if (result) { sprintf(result, "%d", m_lineNumber); }
	}
	else if (columnID == CDTVColumnFlags::CID_Name)
	{
		if (result) { strcpy(result, m_name.c_str()); }
	}
	else if (columnID == CDTVColumnFlags::CID_ModelInfoIndex)
	{
		sortKey = (float)(-((s32)m_modelInfoIndex));
		if (result) { sprintf(result, "%d", m_modelInfoIndex); }
	}
	else if (columnID == CDTVColumnFlags::CID_ModelInfoType)
	{
		if (m_modelInfo)
		{
			const int type = (int)m_modelInfo->GetModelType();
			sortKey = (float)(-type);
			if (result) { strcpy(result, GetModelInfoTypeStr(type, true)); }
		}
	}
	else if (columnID == CDTVColumnFlags::CID_ModelInfoFlag)
	{
		if (m_modelInfo && result)
		{
			atString flagStr;

			if (m_modelInfo->GetAssetParentTxdIndex() != INDEX_NONE) { flagStr += "T"; }
			if (m_modelInfo->SafeGetDrawDictIndex  () != INDEX_NONE) { flagStr += "D"; }
			if (m_modelInfo->SafeGetDrawableIndex  () != INDEX_NONE) { flagStr += "d"; }
			if (m_modelInfo->SafeGetFragmentIndex  () != INDEX_NONE) { flagStr += "f"; }
			if (m_modelInfo->GetPtFxAssetSlot      () != INDEX_NONE) { flagStr += "X"; }

			if (flagStr.length() > 0)
			{
				strcpy(result, flagStr.c_str());
			}
		}
	}
	else if (columnID == CDTVColumnFlags::CID_AssetLoaded)
	{
		if (m_modelInfo)
		{
			const bool bLoaded = m_modelInfo->GetHasLoaded();
			sortKey = bLoaded ? 1.0f : 0.0f;
			if (result) { strcpy(result, bLoaded ? "Y" : "N"); }
		}
	}
	else if (m_modelInfo) // the rest of the columns require m_modelInfo to be valid
	{
		atArray<CTxdRef> refs; GetAssociatedTxds_ModelInfo(refs, m_modelInfo, m_entity, NULL); // no asp available here ..

		int numTxds         = 0;
		int numTxdsNonEmpty = 0;
		int numTextures     = 0;
		int textureMaxDim   = 0;

		for (int i = 0; i < refs.size(); i++)
		{
			const fwTxd* txd = refs[i].GetTxd();

			if (txd)
			{
				numTxds++;

				if (txd->GetCount() > 0)
				{
					numTxdsNonEmpty++;

					for (int j = 0; j < txd->GetCount(); j++)
					{
						numTextures++;

						const grcTexture* texture = txd->GetEntry(j);

						if (texture)
						{
							textureMaxDim = Max<int>(textureMaxDim, texture->GetWidth ());
							textureMaxDim = Max<int>(textureMaxDim, texture->GetHeight());
						}
					}
				}
			}
		}

		if (columnID == CDTVColumnFlags::CID_NumTxds)
		{
			const bool bShowBothNumTxds = __DEV ? true : false;

			if (bShowBothNumTxds) // show both numTxds and numTxdsNonEmpty
			{
				sortKey = (float)((numTxdsNonEmpty << 8) | numTxds);
				if (result) { sprintf(result, "%d/%d", numTxdsNonEmpty, numTxds); }
			}
			else // show only numTxdsNonEmpty
			{
				sortKey = (float)numTxdsNonEmpty;
				if (result) { sprintf(result, "%d", numTxdsNonEmpty); }
			}
		}
		else if (columnID == CDTVColumnFlags::CID_NumTextures)
		{
			sortKey = (float)numTextures;
			if (result) { sprintf(result, "%d", numTextures); }
		}
		else if (columnID == CDTVColumnFlags::CID_DimensionMax)
		{
			sortKey = (float)textureMaxDim;
			if (result) { sprintf(result, "%d", textureMaxDim); }
		}
	}

	return sortKey;
}

// ================================================================================================

CDTVEntry_Shader::CDTVEntry_Shader(const grmShader* shader) : CDTVEntry(shader->GetName())
{
	// TODO
}

// ================================================================================================

CDTVEntry_Txd::CDTVEntry_Txd(const CTxdRef& ref, const CAssetSearchParams& asp, bool bAllowFindDependencies) : CDTVEntry(ref.GetAssetName())
	, m_txd(NULL)
	, m_ref(ref )
{
	m_parentTxdIndex = ref.GetParentTxdIndex();

	if (bAllowFindDependencies && asp.m_findDependenciesWhilePopulating)
	{
		FindAssets_CurrentDependentModelInfos(m_dependentModelInfos, ref, "", asp);
	}
}

CDTVEntry_Txd::CDTVEntry_Txd(const CTxdRef& ref, const CAssetSearchParams& asp, bool bAllowFindDependencies, const CAssetRefInterface& ari) : CDTVEntry(ref.GetAssetName())
	, m_txd(NULL)
	, m_ref(ref )
{
	m_parentTxdIndex = ref.GetParentTxdIndex();
	Load(ari);

	if (bAllowFindDependencies && asp.m_findDependenciesWhilePopulating)
	{
		FindAssets_CurrentDependentModelInfos(m_dependentModelInfos, ref, "", asp);
	}
}

__COMMENT(virtual) void CDTVEntry_Txd::Clear()
{
	m_txd = NULL;
	m_ref.Clear();

	CDTVEntry::Clear();
}

__COMMENT(virtual) void CDTVEntry_Txd::Load(const CAssetRefInterface& ari)
{
	if (m_txd == NULL)
	{
		bool bStreamed = false;

		m_txd = m_ref.LoadTxd(ari.m_verbose, &bStreamed); // this calls m_ref.GetTxd() first, if the result is non-NULL then that's what we use

		if (m_txd || bStreamed)
		{
			m_ref.AddRef(ari);
		}
	}
}

//__COMMENT(virtual) void CDTVEntry_Txd::Select(const CAssetRefInterface& ari);

__COMMENT(virtual) atString CDTVEntry_Txd::GetDesc() const
{
	atString desc = CDTVEntry::GetDesc();

	if (m_parentTxdIndex != INDEX_NONE) // show parent txd
	{
		if (desc.length() > 0)
		{
			desc += "\n";
		}

		desc += atVarString("-> parent = %s", CAssetRef(AST_TxdStore, m_parentTxdIndex).GetDesc().c_str());
	}

	if (0) // show textures
	{
		const fwTxd* txd = GetCurrentTxd();

		if (txd)
		{
			if (desc.length() > 0)
			{
				desc += "\n\n";
			}

			for (int i = 0; i < txd->GetCount(); i++)
			{
				if (i > 0)
				{
					desc += "\n";
				}

				desc += "+ ";
				desc += GetFriendlyTextureName(txd->GetEntry(i));
			}
		}
	}

	if (1) // show dependents
	{
		if (desc.length() > 0)
		{
			desc += "\n\n";
		}

		for (int i = 0; i < m_dependentModelInfos.size(); i++)
		{
			if (i > 0)
			{
				desc += "\n";
			}

			desc += "<- ";
			desc += GetModelInfoDesc(m_dependentModelInfos[i], true);
		}
	}

	return desc;
}

__COMMENT(virtual) const fwTxd* CDTVEntry_Txd::GetPreviewTxd() const
{
	// Note that this does NOT stream the asset in (obviously), nor does it
	// call m_ref.GetTxd() because the txd might not be valid by the time
	// the render thread picks it up. Theoretically if m_ref.GetTxd() returned
	// a txd here we could addref it and immediately removeref when it is
	// finished rendering, but this would add complexity to an already complex
	// system ..
	return m_txd;
}

__COMMENT(virtual) const fwTxd* CDTVEntry_Txd::GetCurrentTxd() const
{
	if (m_txd) // static entry, assume it has been addref'd
	{
		return m_txd;
	}

	return m_ref.GetTxd();
}

__COMMENT(virtual) bool CDTVEntry_Txd::IsColumnRelevant(CDTVColumnFlags::eColumnID columnID) const
{
	if (CDTVEntry::IsColumnRelevant(columnID))
	{
		return true;
	}

	switch ((int)columnID)
	{
	case CDTVColumnFlags::CID_RPFPathName      : return true;
	case CDTVColumnFlags::CID_AssetLoaded      : return true;
	case CDTVColumnFlags::CID_AssetRef         : return true;
	case CDTVColumnFlags::CID_AssetStreamID    : return true;
	case CDTVColumnFlags::CID_AssetSizeVirt    : return true;
	case CDTVColumnFlags::CID_AssetSizePhys    : return true;
	case CDTVColumnFlags::CID_TextureSizePhys  : return true;
	case CDTVColumnFlags::CID_TextureSizeIdeal : return true;
	case CDTVColumnFlags::CID_TextureSizeWaste : return true;
	case CDTVColumnFlags::CID_NumTextures      : return true;
	case CDTVColumnFlags::CID_DimensionMax     : return true;
	case CDTVColumnFlags::CID_Info             : return true;
	case CDTVColumnFlags::CID_IsHD             : return true;
	}

	return false;
}

__COMMENT(virtual) float CDTVEntry_Txd::GetTextAndSortKey(char* result, CDTVColumnFlags::eColumnID columnID) const
{
	float sortKey = 0.0f;

	if (result)
	{
		result[0] = '\0';
	}

	if (columnID == CDTVColumnFlags::CID_LineNumber)
	{
		sortKey = (float)(-m_lineNumber);
		if (result) { sprintf(result, "%d", m_lineNumber); }
	}
	else if (columnID == CDTVColumnFlags::CID_Name)
	{
		if (result) { strcpy(result, m_name.c_str()); }
	}
	else if (columnID == CDTVColumnFlags::CID_RPFPathName)
	{
		if (result) { strcpy(result, m_ref.GetRPFPathName()); }
	}
	else if (columnID == CDTVColumnFlags::CID_AssetLoaded)
	{
		if (m_txd)
		{
			sortKey = -1.0f;
			if (result) { strcpy(result, "A"); }
		}
		else if (m_ref.IsAssetLoaded())//(m_ref.GetTxd())
		{
			sortKey = -2.0f;
			if (result) { strcpy(result, "L"); }
		}
		else
		{
			sortKey = -3.0f;
			if (result) { strcpy(result, "N"); }
		}
	}
	//else if (columnID == CDTVColumnFlags::CID_AssetName)
	//{
	//	if (result) { strcpy(result, m_ref.GetAssetName()); }
	//}
	else if (columnID == CDTVColumnFlags::CID_AssetRef)
	{
		sortKey = (float)(-m_ref.GetSortKey());
		if (result) { strcpy(result, m_ref.GetString().c_str()); }
	}
	else if (columnID == CDTVColumnFlags::CID_AssetStreamID)
	{
		sortKey = (float)(-(int)(m_ref.GetStreamingIndex().Get()));
		if (result) { sprintf(result, "%d", m_ref.GetStreamingIndex().Get()); }	// TODO: Use %x when using paged pools?
	}
	else if (columnID == CDTVColumnFlags::CID_AssetSizeVirt)
	{
		const int size = m_ref.GetStreamingAssetSize(true);
		sortKey = (float)size;
		if (result) { sprintf(result, "%.2fKB", (float)size/1024.0f); }
	}
	else if (columnID == CDTVColumnFlags::CID_AssetSizePhys)
	{
		const int size = m_ref.GetStreamingAssetSize(false);
		sortKey = (float)size;
		if (result) { sprintf(result, "%.2fKB", (float)size/1024.0f); }
	}
	else if (columnID == CDTVColumnFlags::CID_Info)
	{
		if (result) { strcpy(result, m_ref.m_info.c_str()); }
	}
	else // the rest of the columns rely on having a txd .. maybe we should store information when the entry is created instead?
	{
		const fwTxd* txd = GetCurrentTxd();

		if (txd)
		{
			if (columnID == CDTVColumnFlags::CID_TextureSizePhys)
			{
				const int size = GetTextureDictionarySizeInBytes(txd, true);
				sortKey = (float)size;
				if (result) { sprintf(result, "%.2fKB", (float)size/1024.0f); }
			}
			else if (columnID == CDTVColumnFlags::CID_TextureSizeIdeal)
			{
				const int size = GetTextureDictionarySizeInBytes(txd);
				sortKey = (float)size;
				if (result) { sprintf(result, "%.2fKB", (float)size/1024.0f); }
			}
			else if (columnID == CDTVColumnFlags::CID_TextureSizeWaste)
			{
				const int size = GetTextureDictionarySizeInBytes(txd, true) - GetTextureDictionarySizeInBytes(txd);
				sortKey = (float)size;
				if (result) { sprintf(result, "%.2fKB", (float)size/1024.0f); }
			}
			else if (columnID == CDTVColumnFlags::CID_NumTextures)
			{
				const int count = txd->GetCount();
				sortKey = (float)count;
				if (result) { sprintf(result, "%d", count); }
			}
			else if (columnID == CDTVColumnFlags::CID_DimensionMax)
			{
				const int count = txd->GetCount(); // maybe should call 'GetTextureDictionaryMaxDimension(txd)?
				int maxDim = 0;

				for (int i = 0; i < count; i++)
				{
					const grcTexture* texture = txd->GetEntry(i);

					if (texture)
					{
						maxDim = Max<int>(maxDim, texture->GetWidth ());
						maxDim = Max<int>(maxDim, texture->GetHeight());
					}
				}

				sortKey = (float)maxDim;
				if (result) { sprintf(result, "%d", maxDim); }
			}
			else if (columnID == CDTVColumnFlags::CID_IsHD)
			{
				const fwTxd *pfwTxd = m_ref.GetTxd();
				bool isHD = CTexLod::IsTxdUpgradedToHD(pfwTxd);
				if (result) { sprintf(result, "%s", isHD ? "Yes" : "No"); }
			}
		}
	}

	return sortKey;
}

// ================================================================================================

CDTVEntry_Texture::CDTVEntry_Texture(const CTxdRef& ref, const CAssetSearchParams& asp, bool bAllowFindDependencies, const grcTexture* texture) : CDTVEntry(GetFriendlyTextureName(texture).c_str())
{
	m_ref     = ref;
	m_texture = texture; // the txd where this texture came from must have been addref'd, so it is safe to store (but may be NULL)

	if (texture) // set texture information
	{
		m_texSizeInBytesPhys    = texture->GetPhysicalSize();
		m_texSizeInBytes        = GetTextureSizeInBytes(texture);
		m_texWidth              = texture->GetWidth();
		m_texHeight             = texture->GetHeight();
		m_texNumMips            = texture->GetMipMapCount();
		m_texFormat             = texture->GetImageFormat();
		m_texConversionFlags    = texture->GetConversionFlags();
		m_texTemplateType       = texture->GetTemplateType();
		m_texCRC                = asp.m_createTextureCRCs ? GetTextureHash(texture, false, 32) : 0;

		grcTexture::eTextureSwizzle temp[4]; // r,g,b,a
		texture->GetTextureSwizzle(temp[0], temp[1], temp[2], temp[3]);

		sysMemSet(m_texFormatSwizzle, 0, sizeof(m_texFormatSwizzle));

		for (int i = 0; i < 4; i++)
		{
			switch (temp[i])
			{
			case grcTexture::TEXTURE_SWIZZLE_R : m_texFormatSwizzle[i] = 'R'; break;
			case grcTexture::TEXTURE_SWIZZLE_G : m_texFormatSwizzle[i] = 'G'; break;
			case grcTexture::TEXTURE_SWIZZLE_B : m_texFormatSwizzle[i] = 'B'; break;
			case grcTexture::TEXTURE_SWIZZLE_A : m_texFormatSwizzle[i] = 'A'; break;
			case grcTexture::TEXTURE_SWIZZLE_0 : m_texFormatSwizzle[i] = '0'; break;
			case grcTexture::TEXTURE_SWIZZLE_1 : m_texFormatSwizzle[i] = '1'; break;
			}
		}

		if (bAllowFindDependencies && asp.m_findDependenciesWhilePopulating)
		{
			FindAssets_CurrentDependentModelInfos(m_dependentModelInfos, texture, "", asp);
			FindAssets_CurrentDependentTxdRefs   (m_dependentTxdRefs   , texture, asp.m_showEmptyTxds);
		}
	}
	else
	{
		m_texSizeInBytesPhys = 0;
		m_texSizeInBytes     = 0;
		m_texWidth           = 0;
		m_texHeight          = 0;
		m_texNumMips         = 0;
		m_texFormat          = grcImage::UNKNOWN;
		m_texConversionFlags = 0;
		m_texTemplateType    = 0;
		m_texCRC             = 0;

		sysMemSet(m_texFormatSwizzle, 0, sizeof(m_texFormatSwizzle));
	}
}

__COMMENT(virtual) void CDTVEntry_Texture::Clear()
{
	m_texture = NULL;
	m_ref.Clear();

	CDTVEntry::Clear();
}

__COMMENT(virtual) atString CDTVEntry_Texture::GetDesc() const
{
	atString desc = CDTVEntry::GetDesc();

	if (1) // show dependents (models)
	{
		if (desc.length() > 0)
		{
			desc += "\n\n";
		}

		for (int i = 0; i < m_dependentModelInfos.size(); i++)
		{
			if (i > 0)
			{
				desc += "\n";
			}

			desc += "<- ";
			desc += GetModelInfoDesc(m_dependentModelInfos[i], true);
		}
	}

	if (1) // show dependents (txdrefs)
	{
		if (desc.length() > 0)
		{
			desc += "\n\n";
		}

		for (int i = 0; i < m_dependentTxdRefs.size(); i++)
		{
			if (i > 0)
			{
				desc += "\n";
			}

			desc += "<- ";
			desc += m_dependentTxdRefs[i].GetDesc();
		}
	}

	return desc;
}

__COMMENT(virtual) const grcTexture* CDTVEntry_Texture::GetPreviewTexture() const
{
	return m_texture;
}

__COMMENT(virtual) bool CDTVEntry_Texture::IsColumnRelevant(CDTVColumnFlags::eColumnID columnID) const
{
	if (CDTVEntry::IsColumnRelevant(columnID))
	{
		return true;
	}

	switch ((int)columnID)
	{
	case CDTVColumnFlags::CID_RPFPathName        : return true;
//	case CDTVColumnFlags::CID_SourceTemplate     : return true;
//	case CDTVColumnFlags::CID_SourceImage        : return true;
	case CDTVColumnFlags::CID_ShaderName         : return false; // will be true for _TextureRef entries 
	case CDTVColumnFlags::CID_ShaderVar          : return false; // will be true for _TextureRef entries 
	case CDTVColumnFlags::CID_AssetLoaded        : return false; // will be true for _TextureRef entries
	case CDTVColumnFlags::CID_AssetName          : return false; // will be true for _TextureRef entries
	case CDTVColumnFlags::CID_AssetRef           : return false; // will be true for _TextureRef entries
	case CDTVColumnFlags::CID_TextureSizePhys    : return true;
	case CDTVColumnFlags::CID_TextureSizeIdeal   : return true;
	case CDTVColumnFlags::CID_TextureSizeWaste   : return true;
	case CDTVColumnFlags::CID_Dimensions         : return true;
	case CDTVColumnFlags::CID_NumMips            : return true;
	case CDTVColumnFlags::CID_TexFormatBPP       : return true;
	case CDTVColumnFlags::CID_TexFormat          : return true;
	case CDTVColumnFlags::CID_TexFormatSwizzle   : return true;
	case CDTVColumnFlags::CID_TexConversionFlags : return true;
	case CDTVColumnFlags::CID_TexTemplateType    : return true;
	case CDTVColumnFlags::CID_TexAddress         : return true;
	case CDTVColumnFlags::CID_TexCRC             : return true;
	case CDTVColumnFlags::CID_IsHD               : return true;
	}

	return false;
}

__COMMENT(virtual) float CDTVEntry_Texture::GetTextAndSortKey(char* result, CDTVColumnFlags::eColumnID columnID) const
{
	float sortKey = 0.0f;

	if (result)
	{
		result[0] = '\0';
	}

	if (columnID == CDTVColumnFlags::CID_LineNumber)
	{
		sortKey = (float)(-m_lineNumber);
		if (result) { sprintf(result, "%d", m_lineNumber); }
	}
	else if (columnID == CDTVColumnFlags::CID_Name)
	{
		if (result) { strcpy(result, m_name.c_str()); }
	}
	else if (columnID == CDTVColumnFlags::CID_RPFPathName)
	{
		if (result) { strcpy(result, m_ref.GetRPFPathName()); }
	}
	//else if (columnID == CDTVColumnFlags::CID_SourceTemplate)
	//{
	//	if (result) { strcpy(result, " -- TODO -- "); }
	//}
	//else if (columnID == CDTVColumnFlags::CID_SourceImage)
	//{
	//	if (result) { strcpy(result, " -- TODO -- "); }
	//}
	else if (columnID == CDTVColumnFlags::CID_ShaderName)
	{
		if (result && m_shaderVarName.length() > 0)
		{
			strcpy(result, m_shaderVarName.c_str());

			if (strchr(result, '/'))
			{
				strchr(result, '/')[0] = '\0';
			}
		}
	}
	else if (columnID == CDTVColumnFlags::CID_ShaderVar)
	{
		if (result && m_shaderVarName.length() > 0)
		{
			if (strchr(m_shaderVarName.c_str(), '/'))
			{
				strcpy(result, strchr(m_shaderVarName.c_str(), '/') + 1);
			}
			else
			{
				strcpy(result, m_shaderVarName.c_str());
			}
		}
	}
	else if (columnID == CDTVColumnFlags::CID_AssetLoaded)
	{
		if (m_texture)
		{
			sortKey = -1.0f;
			if (result) { strcpy(result, "A"); } // AddRef'ed (in-use)
		}
		else if (m_ref.GetTxd())
		{
			sortKey = -2.0f;
			if (result) { strcpy(result, "L"); } // Loaded but not referenced by texture viewer
		}
		else
		{
			sortKey = -3.0f;
			if (result) { strcpy(result, "N"); } // Not loaded
		}
	}
	else if (columnID == CDTVColumnFlags::CID_AssetName)
	{
		if (result) { strcpy(result, m_ref.GetAssetName()); }
	}
	else if (columnID == CDTVColumnFlags::CID_AssetRef)
	{
		sortKey = (float)(-m_ref.GetSortKey());
		if (result) { strcpy(result, m_ref.GetString().c_str()); }
	}
	else if (columnID == CDTVColumnFlags::CID_TextureSizePhys)
	{
		// might change if texture switches to or from HD
		const grcTexture* texture = GetCurrentTexture();
		const int sizeInBytesPhys = texture ? texture->GetPhysicalSize() : m_texSizeInBytesPhys;
		sortKey = (float)sizeInBytesPhys;
		if (result) { sprintf(result, "%.2fKB", (float)sizeInBytesPhys/1024.0f); }
	}
	else if (columnID == CDTVColumnFlags::CID_TextureSizeIdeal)
	{
		// might change if texture switches to or from HD
		const grcTexture* texture = GetCurrentTexture();
		const int sizeInBytes = texture ? GetTextureSizeInBytes(texture) : m_texSizeInBytes;
		sortKey = (float)sizeInBytes;
		if (result) { sprintf(result, "%.2fKB", (float)sizeInBytes/1024.0f); }
	}
	else if (columnID == CDTVColumnFlags::CID_TextureSizeWaste)
	{
		// might change if texture switches to or from HD
		const grcTexture* texture = GetCurrentTexture();
		const int sizeInBytes = texture ? (texture->GetPhysicalSize() - GetTextureSizeInBytes(texture)) : (m_texSizeInBytesPhys - m_texSizeInBytes);
		sortKey = (float)sizeInBytes;
		if (result) { sprintf(result, "%.2fKB", (float)sizeInBytes/1024.0f); }
	}
	else if (columnID == CDTVColumnFlags::CID_Dimensions)
	{
		// might change if texture switches to or from HD
		const grcTexture* texture = GetCurrentTexture();
		const int w = texture ? texture->GetWidth() : m_texWidth;
		const int h = texture ? texture->GetHeight() : m_texHeight;
		const int d = texture ? texture->GetDepth() : 1; // TODO -- store m_texDepth?
		char dims[64] = "";
		if (d > 1) { sprintf(dims, "%dx%dx%d", w, h, d); }
		else       { sprintf(dims, "%dx%d", w, h); }

		if (texture)
		{
			if (texture->GetLayerCount() > 1)
			{
				strcat(dims, atVarString("[%d]", texture->GetLayerCount()).c_str());
			}
		}
#if __DEV
		else
		{
			strcat(dims, "_NULL"); // lame, can't get layer count .. maybe we should store it?
		}
#endif
		sortKey = (float)((w<<20) + (h<<8) | d); // sort by width, then height, then depth
		if (result) { sprintf(result, "%s%s", dims, texture ? "" : "*"); }
	}
	else if (columnID == CDTVColumnFlags::CID_NumMips)
	{
		// might change if texture switches to or from HD
		const grcTexture* texture = GetCurrentTexture();
		const int numMips = texture ? texture->GetMipMapCount() : m_texNumMips;
		sortKey = (float)numMips;
		if (result) { sprintf(result, "%d", numMips); }
	}
	else if (columnID == CDTVColumnFlags::CID_TexFormatBPP)
	{
		const grcImage::Format format = (const grcImage::Format)m_texFormat;
		const int bpp = grcImage::GetFormatBitsPerPixel(format);
		sortKey = (float)bpp;
		if (result) { sprintf(result, "%d", bpp); }
	}
	else if (columnID == CDTVColumnFlags::CID_TexFormat)
	{
		const grcImage::Format format = (const grcImage::Format)m_texFormat;
		sortKey = (float)format;
		if (result) { sprintf(result, "%s", grcImage::GetFormatString(format)); }
	}
	else if (columnID == CDTVColumnFlags::CID_TexFormatSwizzle)
	{
		if (result) { strcpy(result, m_texFormatSwizzle); }
	}
	else if (columnID == CDTVColumnFlags::CID_TexConversionFlags)
	{
		if (result) { strcpy(result, GetTextureConversionFlagsString(m_texConversionFlags).c_str()); }
	}
	else if (columnID == CDTVColumnFlags::CID_TexTemplateType)
	{
		if (result) { strcpy(result, GetTextureTemplateTypeString(m_texTemplateType).c_str()); }
	}
	else if (columnID == CDTVColumnFlags::CID_TexAddress)
	{
		const grcTexture* texture = GetCurrentTexture();
		sortKey = (float)(~((size_t)texture >> 4));
		if (result) { sprintf(result, "%p", texture); }
	}
	else if (columnID == CDTVColumnFlags::CID_TexCRC)
	{
		sortKey = (float)m_texCRC;
		if (result) { sprintf(result, "0x%08x", m_texCRC); }
	}
	else if (columnID == CDTVColumnFlags::CID_IsHD)
	{
 		const grcTexture* texture = GetCurrentTexture();
		bool isHD = CTexLod::IsTextureUpgradedToHD(texture);
		if (result) { sprintf(result, "%s", isHD ? "Yes" : "No"); }
	}

	return sortKey;
}

// ================================================================================================

CDTVEntry_TextureRef::CDTVEntry_TextureRef(const CTextureRef& ref, const CAssetSearchParams& asp, bool bAllowFindDependencies, const char* shaderVarName) : CDTVEntry_Texture(ref, asp, bAllowFindDependencies, ref.m_tempTexture)
{
	m_texture   = NULL; // we constructed CDTVEntry_Texture with a valid texture so it would set up the information, now we must clear it
	m_txdEntry  = ref.m_txdEntry;
	m_shaderPtr = 0; // TODO

	if (shaderVarName)
	{
		m_shaderVarName = shaderVarName;
	}
}

__COMMENT(virtual) void CDTVEntry_TextureRef::Load(const CAssetRefInterface& ari)
{
	if (m_texture)
	{
		return; // already loaded
	}

	if (m_txdEntry >= 0)
	{
		const fwTxd* txd = m_ref.LoadTxd(ari.m_verbose);

		if (txd && m_txdEntry < txd->GetCount())
		{
			m_texture = txd->GetEntry(m_txdEntry);

			if (m_texture)
			{
				m_ref.AddRef(ari);
			}
		}
	}
}

__COMMENT(virtual) const grcTexture* CDTVEntry_TextureRef::GetCurrentTexture() const
{
	if (m_texture)
	{
		return m_texture;
	}

	if (m_txdEntry >= 0)
	{
		const fwTxd* txd = m_ref.GetTxd();

		if (txd && m_txdEntry < txd->GetCount())
		{
			return txd->GetEntry(m_txdEntry);
		}
	}

	return NULL;
}

__COMMENT(virtual) bool CDTVEntry_TextureRef::IsColumnRelevant(CDTVColumnFlags::eColumnID columnID) const
{
	if (CDTVEntry_Texture::IsColumnRelevant(columnID))
	{
		return true;
	}

	switch ((int)columnID)
	{
	case CDTVColumnFlags::CID_ShaderName  : return m_shaderVarName.length() > 0;
	case CDTVColumnFlags::CID_ShaderVar   : return m_shaderVarName.length() > 0;
	case CDTVColumnFlags::CID_AssetLoaded : return true;
	case CDTVColumnFlags::CID_AssetName   : return true;
	case CDTVColumnFlags::CID_AssetRef    : return true;
	}

	return false;
}

// ================================================================================================

#if DEBUG_TEXTURE_VIEWER_SHADER_EDIT_INTEGRATION

CDTVEntry_ShaderEditTexture::CDTVEntry_ShaderEditTexture(const grcTexture* texture, const char* name) : CDTVEntry(name), m_texture(texture)
{
	CShaderEdit::ActiveTextureRefAdd(const_cast<grcTexture*>(texture));
}

CDTVEntry_ShaderEditTexture::~CDTVEntry_ShaderEditTexture()
{
	Clear();
	CShaderEdit::ActiveTextureRefRemove(const_cast<grcTexture*>(m_texture));
}

__COMMENT(virtual) bool CDTVEntry_ShaderEditTexture::IsColumnRelevant(CDTVColumnFlags::eColumnID columnID) const
{
	if (CDTVEntry::IsColumnRelevant(columnID))
	{
		return true;
	}

	switch ((int)columnID)
	{
	case CDTVColumnFlags::CID_TextureSizePhys    : return true;
	case CDTVColumnFlags::CID_TextureSizeIdeal   : return true;
	case CDTVColumnFlags::CID_TextureSizeWaste   : return true;
	case CDTVColumnFlags::CID_Dimensions         : return true;
	case CDTVColumnFlags::CID_NumMips            : return true;
	case CDTVColumnFlags::CID_TexFormatBPP       : return true;
	case CDTVColumnFlags::CID_TexFormat          : return true;
	case CDTVColumnFlags::CID_TexFormatSwizzle   : return true;
	case CDTVColumnFlags::CID_TexConversionFlags : return true;
	case CDTVColumnFlags::CID_TexTemplateType    : return true;
	case CDTVColumnFlags::CID_TexAddress         : return true;
	case CDTVColumnFlags::CID_TexCRC             : return false;
	}

	return false;
}

__COMMENT(virtual) float CDTVEntry_ShaderEditTexture::GetTextAndSortKey(char* result, CDTVColumnFlags::eColumnID columnID) const
{
	float sortKey = 0.0f;

	if (result)
	{
		result[0] = '\0';
	}

	if (columnID == CDTVColumnFlags::CID_LineNumber)
	{
		sortKey = (float)(-m_lineNumber);
		if (result) { sprintf(result, "%d", m_lineNumber); }
	}
	else if (columnID == CDTVColumnFlags::CID_Name)
	{
		if (result) { strcpy(result, m_name.c_str()); }
	}
	else if (columnID == CDTVColumnFlags::CID_TextureSizePhys)
	{
		const int size = m_texture->GetPhysicalSize();
		sortKey = (float)size;
		if (result) { sprintf(result, "%.2fKB", (float)size/1024.0f); }
	}
	else if (columnID == CDTVColumnFlags::CID_TextureSizeIdeal)
	{
		const int size = GetTextureSizeInBytes(m_texture);
		sortKey = (float)size;
		if (result) { sprintf(result, "%.2fKB", (float)size/1024.0f); }
	}
	else if (columnID == CDTVColumnFlags::CID_TextureSizeWaste)
	{
		const int size = m_texture->GetPhysicalSize() - GetTextureSizeInBytes(m_texture);
		sortKey = (float)size;
		if (result) { sprintf(result, "%.2fKB", (float)size/1024.0f); }
	}
	else if (columnID == CDTVColumnFlags::CID_Dimensions)
	{
		const int w = m_texture->GetWidth();
		const int h = m_texture->GetHeight();
		const int d = m_texture->GetDepth();
		char dims[64] = "";
		if (d > 1) { sprintf(dims, "%dx%dx%d", w, h, d); }
		else       { sprintf(dims, "%dx%d", w, h); }

		if (m_texture->GetLayerCount() > 1)
		{
			strcat(dims, atVarString("[%d]", m_texture->GetLayerCount()).c_str());
		}

		sortKey = (float)((w<<20) + (h<<8) | d); // sort by width, then height, then depth
		if (result) { strcpy(result, dims); }
	}
	else if (columnID == CDTVColumnFlags::CID_NumMips)
	{
		const int numMips = m_texture->GetMipMapCount();
		sortKey = (float)numMips;
		if (result) { sprintf(result, "%d", numMips); }
	}
	else if (columnID == CDTVColumnFlags::CID_TexFormatBPP)
	{
		const grcImage::Format format = (const grcImage::Format)m_texture->GetImageFormat();
		const int bpp = grcImage::GetFormatBitsPerPixel(format);
		sortKey = (float)bpp;
		if (result) { sprintf(result, "%d", bpp); }
	}
	else if (columnID == CDTVColumnFlags::CID_TexFormat)
	{
		const grcImage::Format format = (const grcImage::Format)m_texture->GetImageFormat();
		sortKey = (float)format;
		if (result) { sprintf(result, "%s", grcImage::GetFormatString(format)); }
	}
	else if (columnID == CDTVColumnFlags::CID_TexFormatSwizzle)
	{
		if (result)
		{
			grcTexture::eTextureSwizzle swizzle[4];
			m_texture->GetTextureSwizzle(swizzle[0], swizzle[1], swizzle[2], swizzle[3]);
			const char swizzleTable[] = "RGBA01";
			result[0] = swizzleTable[swizzle[0]];
			result[1] = swizzleTable[swizzle[1]];
			result[2] = swizzleTable[swizzle[2]];
			result[3] = swizzleTable[swizzle[3]];
			result[4] = '\0';
		}
	}
	else if (columnID == CDTVColumnFlags::CID_TexConversionFlags)
	{
		if (result) { strcpy(result, GetTextureConversionFlagsString(m_texture->GetConversionFlags()).c_str()); }
	}
	else if (columnID == CDTVColumnFlags::CID_TexTemplateType)
	{
		if (result) { strcpy(result, GetTextureTemplateTypeString(m_texture->GetTemplateType()).c_str()); }
	}
	else if (columnID == CDTVColumnFlags::CID_TexAddress)
	{
		sortKey = (float)(~((size_t)m_texture >> 4));
		if (result) { sprintf(result, "%p", m_texture); }
	}

	return sortKey;
}

#endif // DEBUG_TEXTURE_VIEWER_SHADER_EDIT_INTEGRATION

#endif // __BANK

