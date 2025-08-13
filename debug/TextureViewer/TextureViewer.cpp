// =====================================
// debug/textureviewer/textureviewer.cpp
// (c) 2010 RockstarNorth
// =====================================

#include "debug/textureviewer/textureviewerprivate.h"

#if __BANK

#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "file/asset.h"
#include "grcore/setup.h"
#include "grcore/image.h"
#include "grcore/texture.h"
#include "grcore/texture_d3d11.h"
#include "grcore/texture_durango.h"
#include "grcore/texture_gnm.h"
#include "grcore/texturecontrol.h"
#include "grcore/texturedebugflags.h"
#include "grmodel/geometry.h"
#include "parser/manager.h"
#include "string/stringutil.h"
#include "system/memory.h"

#include "rmptfx/ptxfxlist.h"
#include "vfx/ptfx/ptfxmanager.h"

#include "file/default_paths.h"
#include "fwdebug/picker.h"
#include "fwmaths/geomutil.h"
#include "fwmaths/rect.h"
#include "fwnet/HttpQuery.h"
#include "fwscene/stores/txdstore.h"
#include "fwscene/stores/dwdstore.h"
#include "fwscene/stores/drawablestore.h"
#include "fwscene/stores/fragmentstore.h"
#include "fwscene/stores/mapdatastore.h"
#include "fwscene/world/InteriorLocation.h"
#include "fwsys/fileexts.h"
#include "fwtextureconversion/textureconversion.h"
#include "fwutil/popups.h"
#include "fwutil/xmacro.h"
#include "streaming/streaming.h"
#include "streaming/streamingengine.h"
#include "streaming/packfilemanager.h"

#include "core/game.h"
#include "camera/viewports/ViewportManager.h"
#include "debug/AssetAnalysis/AssetAnalysis.h"
#include "debug/AssetAnalysis/AssetAnalysisUtil.h"
#include "debug/DebugArchetypeProxy.h"
#include "frontend/HudTools.h"
#include "input/mouse.h"
#include "input/keyboard.h"
#include "input/keys.h"
#include "Objects/DummyObject.h"
#include "system/memory.h"
#include "system/controlmgr.h"
#include "modelinfo/MloModelInfo.h"
#include "objects/object.h"
#include "peds/ped.h"
#include "scene/animatedbuilding.h"
#include "scene/loader/MapData.h"
#include "scene/portals/InteriorInst.h"
#include "scene/world/gameworld.h"
#include "renderer/color.h"
#include "renderer/Debug/EntitySelect.h"
#include "renderer/RenderPhases/RenderPhaseDebugOverlay.h"
#include "renderer/RenderPhases/RenderPhaseReflection.h"
#include "renderer/rendertargets.h"
#include "renderer/Util/ShaderUtil.h"

#if RDR_VERSION // TEMP -- not integrating this over to V
#include "debug/TextureViewer/DXTDecompressionTable.h"
#endif
#include "debug/TextureViewer/TextureViewer.h"
#include "debug/TextureViewer/TextureViewerDebugging.h"
#include "debug/TextureViewer/TextureViewerDTQ.h"
#include "debug/TextureViewer/TextureViewerEntry.h"
#include "debug/TextureViewer/TextureViewerState.h"
#include "debug/TextureViewer/TextureViewerUtil.h"
#include "debug/TextureViewer/TextureViewerWindow.h"

// experimental
#include "debug/TextureViewer/TextureViewerStreamingIterator.h"
#include "debug/TextureViewer/TextureViewerStreamingIteratorTest.h"

#if DEBUG_TEXTURE_VIEWER_UIGADGET_DEMO
	#include "debug/UiGadget/UiGadgetBase.h"
	#include "debug/UiGadget/UiGadgetBox.h"
	#include "debug/UiGadget/UiGadgetWindow.h"
	#include "debug/UiGadget/UiGadgetList.h"
#endif // DEBUG_TEXTURE_VIEWER_UIGADGET_DEMO

DEBUG_TEXTURE_VIEWER_OPTIMISATIONS()

#define ENTITYSELECT_CHECK (0 && __DEV && ENTITYSELECT_ENABLED_BUILD) // enable this to help debug what might be turning off entity select

// ================================================================================================

class CDTVListState
{
public:
	CDTVListState() {}
	CDTVListState(const char* windowTitle, CDTVColumnFlags::eColumnID listSortColumnID, int listEntryOffset, int assetRefOffset)
		: m_windowTitle     (windowTitle     )
		, m_listSortColumnID(listSortColumnID)
		, m_listEntryOffset (listEntryOffset )
		, m_assetRefOffset  (assetRefOffset  )
	{}

	atString                   m_windowTitle;
	CDTVColumnFlags::eColumnID m_listSortColumnID;
	int                        m_listEntryOffset;
	int                        m_assetRefOffset;
};

class CDebugTextureViewer : public CDTVStateCommon
{
public:
	CDebugTextureViewer();

	// static methods for bank callbacks
#if __DEV
	static void _PrintAssets_ModelInfo    ();
	static void _PrintAssets_TxdStore     ();
	static void _PrintAssets_DwdStore     ();
	static void _PrintAssets_DrawableStore();
	static void _PrintAssets_FragmentStore();
	static void _PrintAssets_ParticleStore();
#endif // __DEV
	static void _Populate_Search              ();
	static void _Populate_SearchModels__broken();
	static void _Populate_SearchTxds          ();
	static void _Populate_SearchTextures      ();
	static void _Populate_SelectFromPicker    ();

	enum ePopulateSearchType
	{
		PST_SearchModels__broken,
		PST_SearchTxds,
		PST_SearchTextures,
		PST_COUNT
	};

	const char** GetUIStrings_ePopulateSearchType()
	{
		static const char* strings[] =
		{
			"Models -- broken",
			"Texture Dictionaries",
			"Textures",
			NULL
		};

		return strings;
	}

	void Populate_SearchModelClick    (); // modelinfo/entities under mouse cursor
	void Populate_Search              (); // search specified by search type
	void Populate_SearchModels__broken(); // modelinfo's by name
	void Populate_SearchTxds          (); // associated txd's of txd's, dwd's, drawables, fragments and particles by name
	void Populate_SearchTextures      (); // textures by name .. scans all asset stores
	void Populate_SelectFromPicker    ();
	void Populate_SelectModelInfo     (const char* name, u32 modelInfoIndex, const CEntity* entity, bool bNoEmptyTxds = false); // called by CDTVEntry_ModelInfo::Select
	void Populate_SelectModelInfoTex  (const char* name, u32 modelInfoIndex, const CEntity* entity); // called by CDTVEntry_ModelInfo::Select
	void Populate_SelectTxd           (const char* name, const CTxdRef& ref); // called by CDTVEntry_Txd::Select

	int        GetListEntryOffset() const;
	int        GetListNumEntries() const;
	CDTVEntry* GetListEntry(int index);
	CDTVEntry* GetSelectedListEntry();
	void       ClearSelectedListEntry();

	void Clear(bool bRemoveRefsImmediately, bool bAllLists);
	void Close(bool bRemoveRefsImmediately);
	void Back();

	void SetupWindow(const char* title, bool bPushListState);
	void SetupWindowFinish();

	void SetSortColumnID(CDTVColumnFlags::eColumnID columnID);

#if DEBUG_TEXTURE_VIEWER_EDIT_METADATA_IN_WORKBENCH
	static void _Edit_MetadataInWorkbench();
	void Edit_MetadataInWorkbench();
#endif // DEBUG_TEXTURE_VIEWER_EDIT_METADATA_IN_WORKBENCH

	static int  DebugWindow_GetNumEntries   ();
	static void DebugWindow_GetEntryText    (char*    result, int row, int column);
	static bool DebugWindow_GetEntryColour  (Color32* result, int row, int column);
	static bool DebugWindow_GetRowActive    (                 int row);
	static void DebugWindow_ClickCategory   (                 int row, int column);
	static void DebugWindow_ClickBack       ();
	static bool DebugWindow_ClickBackEnabled();
	static void DebugWindow_CloseWindow     ();
	static void DebugWindow_ResetPosition   ();
	static void DebugWindow_DumpListToCSV   (const char* path);

#if DEBUG_TEXTURE_VIEWER_UIGADGET_DEMO
	static void UiGadgetDemo();
#endif // DEBUG_TEXTURE_VIEWER_UIGADGET_DEMO

	static void LoadPreviewTexture();
	static void LoadPreviewTexture_Reload();
	static void LoadPreviewTexture_Update();
	static void LoadSandboxTxd();
	static void LoadSandboxTxd_Update();
#if DEBUG_TEXTURE_VIEWER_NEW_STREAMING_CODE
	static void RemoveSandboxTxds();
#endif // DEBUG_TEXTURE_VIEWER_NEW_STREAMING_CODE
	static void LoadAll();

	void InitWidgets(bkBank& bk);

	void Update();

	CDTVState& GetCurrentUpdateState();
	CDTVState& GetCurrentRenderState();

	void Render();
	void Synchronise();

	bool                       m_visible;
	bool                       m_enablePicking;
#if ENTITYSELECT_CHECK
	bool                       m_assertEntitySelectOn;
#endif // ENTITYSELECT_CHECK
	int                        m_hoverEntityShaderMode;
	bool                       m_hoverTextUseMiniFont;
#if DEBUG_TEXTURE_VIEWER_GET_WORLD_POS_UNDER_MOUSE
	bool                       m_showWorldPosUnderMouse;
	bool                       m_showWorldPosMarker;
	int                        m_showWorldPosExternalRefCount;
#endif // DEBUG_TEXTURE_VIEWER_GET_WORLD_POS_UNDER_MOUSE
	bool                       m_showDesc;
	bool                       m_showRefs;
	bool                       m_verbose;

#if DEBUG_TEXTURE_VIEWER_INVERT_TEXTURE_CHANNELS
	int                        m_invertTextureMipCount;
	bool                       m_invertTextureChannelR;
	bool                       m_invertTextureChannelG;
	bool                       m_invertTextureChannelB;
	bool                       m_invertTextureChannelA;
	bool                       m_invertTextureReverseBits;
	bool                       m_invertTextureVerbose;
	Color32                    m_solidTextureColour;
#endif // DEBUG_TEXTURE_VIEWER_INVERT_TEXTURE_CHANNELS

#if DEBUG_TEXTURE_VIEWER_FRAMEBUFFER_MAGNIFIER
	bool                       m_magnifierEnabled;
	bool                       m_magnifierPixelPicker; // only move while right mouse button is held down
//	int                        m_magnifierBuffer;
	int                        m_magnifierRes;
	int                        m_magnifierZoom;
	int                        m_magnifierOffsetX;
	int                        m_magnifierOffsetY;
	bool                       m_magnifierAttachedToMouse;
#endif // DEBUG_TEXTURE_VIEWER_FRAMEBUFFER_MAGNIFIER

#if DEBUG_TEXTURE_VIEWER_ENTITY_FLAGS
	u16                        m_entityVisPhaseFlags;
	u32                        m_entityFlags;
#endif // DEBUG_TEXTURE_VIEWER_ENTITY_FLAGS

#if DEBUG_TEXTURE_VIEWER_OVERRIDE_REFLECTION_TEX
	int                        m_overrideReflectionTex;
	int                        m_overrideReflectionCubeTex;
#endif // DEBUG_TEXTURE_VIEWER_OVERRIDE_REFLECTION_TEX

	ePopulateSearchType        m_searchType;
	char                       m_searchFilter[80];
	char                       m_searchRPF[80];
	char                       m_sandboxTxdName[80];
	char                       m_previewTextureName[80];
	char                       m_previewTextureTCP[80];
	bool                       m_previewTextureLinear;
	bool                       m_previewTextureSRGB;
	CAssetSearchParams         m_asp;
	bool                       m_selectModelTextures;
#if __DEV
	bool                       m_printShaderNamesForModelSearchResults; // print shader names for all models found through Populate_SearchModels__broken (useful if EntitySelect can't see the model)
#endif // __DEV
	CDTVColumnFlags            m_columnFlags;
	CDTVColumnFlags::eColumnID m_sortColumnID;
	bool                       m_findAssetsOnlyIfLoaded;
	int                        m_findAssetsMaxCount;
	int                        m_findTexturesMinSize;
	bool                       m_findTexturesConstantOnly;
	bool                       m_findTexturesRedundantOnly;
	bool                       m_findTexturesReportNonUniqueTextureMemory;
	u32                        m_findTexturesConversionFlagsRequired;
	u32                        m_findTexturesConversionFlagsSkipped;
	char                       m_findTexturesUsage[256];
	char                       m_findTexturesUsageExclusion[256];
	char                       m_findTexturesFormat[256];
	atString                   m_windowTitle;
	int                        m_windowNumListSlots;
	float                      m_windowScale;
	float                      m_windowBorder;
	float                      m_windowSpacing;
	u32                        m_doubleClickDelay;
	u32                        m_doubleClickWait;
#if DEBUG_TEXTURE_VIEWER_TRACKASSETHASH
	bool                       m_trackAssetHash;
#endif // DEBUG_TEXTURE_VIEWER_TRACKASSETHASH
#if DEBUG_TEXTURE_VIEWER_TEXTUREUSAGEANALYSER
	int                        m_textureUsageAnalyserMaxCount;
	bool                       m_textureUsageAnalyserBuildRefs;
	bool                       m_textureUsageAnalyserHashTextureData; // SLOW!
	bool                       m_textureUsageAnalyserShowTexturesNotUsedByShader;
	bool                       m_textureUsageAnalyserShowModelReferences;
	bool                       m_textureUsageAnalyserCSVOutput;
#endif // DEBUG_TEXTURE_VIEWER_TEXTUREUSAGEANALYSER

	CDebugTextureViewerWindow* m_debugWindow;
	atArray<CDTVListState>     m_listState;
	atArray<CDTVEntry*>        m_listEntries;
	atArray<CAssetRef>         m_assetRefs;
#if DEBUG_TEXTURE_VIEWER_NEW_STREAMING_CODE
	atArray<int>               m_sandboxTxdSlots; // these get removed in RemoveSandboxTxds
	bool                       m_sandboxTxdUseNewStreamingCode;
#endif // DEBUG_TEXTURE_VIEWER_NEW_STREAMING_CODE
#if DEBUG_TEXTURE_VIEWER_SELECTPREVIEW
#if 0
	pgRef<const fwTxd>         m_previewTxd; // crashes immediately, wtf?
#else
	const fwTxd*               m_previewTxd; // doesn't crash immediately, but crashes later ..
#endif
#endif // DEBUG_TEXTURE_VIEWER_SELECTPREVIEW
	pgRef<const grcTexture>    m_previewTexture;
	CDTVState                  m_state[2];
	int                        m_stateIndex;
};

CDebugTextureViewer gDebugTextureViewer;

#include "debug/textureviewer/textureviewerentry.inl"
#include "debug/textureviewer/textureviewerpopulate.inl"

CDebugTextureViewer::CDebugTextureViewer()
{
	sysMemSet((void*)this, 0, sizeof(*this));

	// default values (non-zero)
	{
		m_visible                          = true;
#if ENTITYSELECT_CHECK
		m_assertEntitySelectOn             = false;
#endif // ENTITYSELECT_CHECK
		m_hoverEntityShaderMode            = 0;
		m_hoverTextUseMiniFont             = true;//(RSG_PC || RSG_DURANGO || RSG_ORBIS) ? false : true;
#if DEBUG_TEXTURE_VIEWER_GET_WORLD_POS_UNDER_MOUSE
		m_showWorldPosUnderMouse           = false;
		m_showWorldPosMarker               = false;
		m_showWorldPosExternalRefCount     = 0;
#endif // DEBUG_TEXTURE_VIEWER_GET_WORLD_POS_UNDER_MOUSE
		m_showDesc                         = false;
		m_showRefs                         = false;
		m_verbose                          = false;

		m_searchType                       = PST_SearchTxds;

#if DEBUG_TEXTURE_VIEWER_INVERT_TEXTURE_CHANNELS
		m_invertTextureMipCount            = 12;
		m_invertTextureChannelR            = true;
		m_invertTextureChannelG            = true;
		m_invertTextureChannelB            = true;
		m_invertTextureChannelA            = true;
		m_invertTextureReverseBits         = false;
		m_invertTextureVerbose             = false;
		m_solidTextureColour               = Color32(255,0,0,255);
#endif // DEBUG_TEXTURE_VIEWER_INVERT_TEXTURE_CHANNELS

#if DEBUG_TEXTURE_VIEWER_FRAMEBUFFER_MAGNIFIER
		m_magnifierEnabled                 = false;
		m_magnifierPixelPicker             = false;
	//	m_magnifierBuffer                  = 0;
		m_magnifierRes                     = 24; // can't be higher than 24, i've hardcoded the m_pixels array for some reason
		m_magnifierZoom                    = 8;
		m_magnifierOffsetX                 = 60;
		m_magnifierOffsetY                 = 50;
		m_magnifierAttachedToMouse         = false;
#endif // DEBUG_TEXTURE_VIEWER_FRAMEBUFFER_MAGNIFIER

		m_asp                              = CAssetSearchParams();
		m_columnFlags                      = CDTVColumnFlags();
		m_sortColumnID                     = CDTVColumnFlags::CID_NONE;
		m_previewTxdGrid                   = 4;
		m_previewTxdGridOffset             = 0.0f;
		m_previewTextureViewMode           = DebugTextureViewerDTQ::eDTQVM_RGBA;
		m_previewTextureCubeForceOpaque    = true;
		m_previewTextureCubeCylinderAspect = 4.0f;
		m_previewTextureCubeFaceZoom       = 1.0f;
		m_previewTextureCubeFace           = DebugTextureViewerDTQ::eDTQCF_PY_FRONT;
		m_previewTextureCubeProjection     = DebugTextureViewerDTQ::eDTQCP_4x3Cross;
		m_previewTextureCubeProjClip       = true;
		m_previewTextureZoom               = 1.0f;
		m_previewTextureSpecularL.z        = 1.0f;
		m_previewTextureSpecularBumpiness  = 1.0f;
		m_previewTextureSpecularExponent   = 32.0f;
		m_previewTextureSpecularScale      = 1.0f;
#if DEBUG_TEXTURE_VIEWER_HISTOGRAM
		m_histogramScale                   = 1.0f;
		m_histogramBucketMin               = 1;
#endif // DEBUG_TEXTURE_VIEWER_HISTOGRAM
#if DEBUG_TEXTURE_VIEWER_GETPIXELTEST
		m_getPixelTestShowAlpha            = false;
		m_getPixelMipIndex                 = 0;
		m_getPixelTestW                    = 128;
		m_getPixelTestH                    = 128;
		m_getPixelTestSize                 = 1.0f;
#endif // DEBUG_TEXTURE_VIEWER_GETPIXELTEST
#if DEBUG_TEXTURE_VIEWER_DUMP_TEXTURE
		strcpy(m_dumpTextureDir, "c:/dump");
#endif // DEBUG_TEXTURE_VIEWER_DUMP_TEXTURE
		m_windowNumListSlots               = (int)ceilf(256.0f/CDebugTextureViewerWindow::GetDefaultListEntryHeight()) - 1; // at least 256 pixels high
		m_windowScale                      = 1.0f;
		m_windowBorder                     = 2.0f;
		m_windowSpacing                    = 2.0f;
		m_findAssetsOnlyIfLoaded           = false;
		m_findAssetsMaxCount               = 12000;
		m_findTexturesMinSize              = 0;
		m_findTexturesConstantOnly         = false;
		m_findTexturesRedundantOnly        = false;
		m_findTexturesReportNonUniqueTextureMemory = false;
		m_findTexturesConversionFlagsRequired = 0;
		m_findTexturesConversionFlagsSkipped  = 0;
		m_findTexturesUsage[0]             = '\0';
		m_findTexturesUsageExclusion[0]    = '\0';
		m_findTexturesFormat[0]            = '\0';
		m_doubleClickDelay                 = 250;
		m_doubleClickWait                  = 500;
#if DEBUG_TEXTURE_VIEWER_TEXTUREUSAGEANALYSER
		m_textureUsageAnalyserMaxCount     = 100;
		m_textureUsageAnalyserBuildRefs    = true;
#endif // DEBUG_TEXTURE_VIEWER_TEXTUREUSAGEANALYSER
#if DEBUG_TEXTURE_VIEWER_NEW_STREAMING_CODE
		m_sandboxTxdUseNewStreamingCode   = true;
#endif // DEBUG_TEXTURE_VIEWER_NEW_STREAMING_CODE

		strcpy(m_sandboxTxdName, atVarString("platform:/textures/sandbox/sandbox.%s", TXD_FILE_EXT));
	}
}

int CDebugTextureViewer::GetListEntryOffset() const
{
	return m_listState.size() > 0 ? m_listState.back().m_listEntryOffset : 0;
}

int CDebugTextureViewer::GetListNumEntries() const
{
	const int index0 = GetListEntryOffset();
	const int index1 = m_listEntries.size();

	Assert(index0 <= index1);
	return index1 - index0;
}

CDTVEntry* CDebugTextureViewer::GetListEntry(int index)
{
	Assert(index >= 0 && index < GetListNumEntries());
	return m_listEntries[GetListEntryOffset() + index];
}

CDTVEntry* CDebugTextureViewer::GetSelectedListEntry()
{
	if (m_debugWindow && m_visible)
	{
		const int selectedIndex = m_debugWindow->GetCurrentIndex();

		if (selectedIndex >= 0 && selectedIndex < GetListNumEntries())
		{
			return GetListEntry(selectedIndex);
		}
	}

	return NULL;
}

void CDebugTextureViewer::ClearSelectedListEntry()
{
	if (m_debugWindow)
	{
		m_debugWindow->SetSelectorEnabled(false);
	}
}

void CDebugTextureViewer::Clear(bool bRemoveRefsImmediately, bool bAllLists)
{
	// remove refs
	{
		const int index0 = bAllLists ? 0 : m_listState.back().m_assetRefOffset;
		const int index1 = m_assetRefs.size();

		for (int i = index0; i < index1; i++) // copy refs to update state .. they will be deref'd after renderthread is done with them
		{
			//if (bRemoveRefsImmediately)
			//{
			//	m_assetRefs[i].RemoveRefWithoutDelete(false);
			//}
			//else
			{
				GetCurrentUpdateState().m_assetRefs.PushAndGrow(m_assetRefs[i]);
			}
		}

		m_assetRefs.Resize(index0); // Resize(0) is the same as clear(), right?
	}

	// delete entries
	{
		const int index0 = bAllLists ? 0 : m_listState.back().m_listEntryOffset;
		const int index1 = m_listEntries.size();

		for (int i = index0; i < index1; i++)
		{
			delete m_listEntries[i];
		}

		m_listEntries.Resize(index0); // Resize(0) is the same as clear(), right?
	}

	if (bAllLists)
	{
		m_listState.clear();
	}
	else
	{
		m_listState.Pop();
	}

	if (bRemoveRefsImmediately)
	{
		GetCurrentUpdateState().Cleanup();
		GetCurrentRenderState().Cleanup(); // is this safe?
	}
}

void CDebugTextureViewer::Close(bool bRemoveRefsImmediately)
{
	Clear(bRemoveRefsImmediately, true);

	if (m_debugWindow)
	{
		m_debugWindow->Release();
		Assert(m_debugWindow == NULL);
	}

	m_enablePicking          = false;
	m_hoverEntityShaderMode  = 0;
#if DEBUG_TEXTURE_VIEWER_GET_WORLD_POS_UNDER_MOUSE
	m_showWorldPosUnderMouse = false;
	m_showWorldPosMarker     = false;
#endif // DEBUG_TEXTURE_VIEWER_GET_WORLD_POS_UNDER_MOUSE
}

void CDebugTextureViewer::Back()
{
	Clear(false, false);

	if (m_listState.size() > 0)
	{
		SetupWindowFinish();
	}
	else if (m_debugWindow)
	{
		m_debugWindow->Release();
		Assert(m_debugWindow == NULL);
	}
}

void CDebugTextureViewer::SetupWindow(const char* title, bool bPushListState)
{
	DEBUG_TEXTURE_VIEWER_USE_DEBUG_MEMORY();

#if ENTITYSELECT_ENABLED_BUILD
	CEntitySelect::SetEntitySelectPassEnabled();
#endif // ENTITYSELECT_ENABLED_BUILD

	if (!bPushListState)
	{
		Clear(false, true);
	}

	m_listState.PushAndGrow(CDTVListState(title, CDTVColumnFlags::CID_Name, m_listEntries.size(), m_assetRefs.size()));

	m_windowTitle    = title;
#if DEBUG_TEXTURE_VIEWER_SELECTPREVIEW
	m_previewTxd     = NULL;
#endif // DEBUG_TEXTURE_VIEWER_SELECTPREVIEW
	m_previewTexture = NULL; // should release it if it was created for the texture viewer?

	Vector2 windowPos = DEBUG_TEXTURE_VIEWER_WINDOW_POS;

	if (m_debugWindow == NULL)
	{
		const float listWidth    = 440.0f;
		const float previewWidth = (float)(m_windowNumListSlots + 1)*CDebugTextureViewerWindow::GetDefaultListEntryHeight(); // make it square

		m_debugWindow = rage_new CDebugTextureViewerWindow(
			windowPos.x,
			windowPos.y,
			listWidth,
			previewWidth,
			m_windowNumListSlots,
			DebugWindow_GetNumEntries,
			DebugWindow_GetEntryText
		);

		m_debugWindow->SetGetEntryColourCB  (DebugWindow_GetEntryColour  );
		m_debugWindow->SetGetRowActiveCB    (DebugWindow_GetRowActive    );
		m_debugWindow->SetClickCategoryCB   (DebugWindow_ClickCategory   );
		m_debugWindow->SetClickBackCB       (DebugWindow_ClickBack       );
		m_debugWindow->SetClickBackEnabledCB(DebugWindow_ClickBackEnabled);
		m_debugWindow->SetCloseWindowCB     (DebugWindow_CloseWindow     );
	}
	else
	{
		m_debugWindow->DeleteColumns();
	}

	// version 0 colour = Color32(255,0,0,255)
	// version 1 colour = Color32(80,80,120,255)
	m_debugWindow->SetColour   (CDebugTextureViewerWindow::DebugTextureViewerWindow_COLOUR_TITLE_BG, Color32(80,80,120,255));
	m_debugWindow->SetTitle    (title);
	m_debugWindow->SetIsVisible(true);
	m_debugWindow->SetIsActive (true);

#if DEBUG_TEXTURE_VIEWER_DISABLE_PLAYER
	CGameWorld::GetMainPlayerInfo()->DisableControlsDebug();
#endif // DEBUG_TEXTURE_VIEWER_DISABLE_PLAYER
}

static int _GetColumnMaxChars(CDebugTextureViewer* _this, CDTVColumnFlags::eColumnID columnID)
{
	int maxChars = istrlen(CDTVColumnFlags::GetColumnName(columnID));

	for (int i = 0; i < _this->GetListNumEntries(); i++)
	{
		char text[512] = "";
		_this->GetListEntry(i)->GetTextAndSortKey(text, columnID);
		maxChars = Max<int>(istrlen(text), maxChars);
	}

	return maxChars;
}

void CDebugTextureViewer::SetupWindowFinish()
{
	if (GetListNumEntries() > 0) // assume all entries use the same set of columns
	{
		GetListEntry(0)->SetupWindow(m_debugWindow, m_columnFlags);
	}

	m_windowTitle = m_listState.back().m_windowTitle.c_str(); // save it
	m_debugWindow->SetTitle(m_windowTitle);

	SetSortColumnID(m_listState.back().m_listSortColumnID);

	for (int i = 0; i < CDTVColumnFlags::CID_COUNT; i++)
	{
		if (1) // calculate max chars dynamically
		{
			m_columnFlags.m_columnNumChars[i] = _GetColumnMaxChars(this, (CDTVColumnFlags::eColumnID)i);
		}
		else // assume fixed max chars
		{
			m_columnFlags.m_columnNumChars[i] = CDTVColumnFlags::GetColumnMaxChars((CDTVColumnFlags::eColumnID)i, NULL);
		}
	}
}

void CDebugTextureViewer::SetSortColumnID(CDTVColumnFlags::eColumnID columnID)
{
	m_sortColumnID = columnID;

	if (columnID != CDTVColumnFlags::CID_NONE)
	{
		const int entryStart = GetListEntryOffset();
		const int entryCount = GetListNumEntries();

		m_listEntries.QSort(entryStart, entryCount, CDTVEntry::CompareFuncPtr);

		for (int i = 0; i < entryCount; i++)
		{
			m_listEntries[entryStart + i]->SetLineNumber(i + 1);
		}
	}
}

#if DEBUG_TEXTURE_VIEWER_EDIT_METADATA_IN_WORKBENCH

__COMMENT(static) void CDebugTextureViewer::_Edit_MetadataInWorkbench()
{
	gDebugTextureViewer.Edit_MetadataInWorkbench();
}

void CDebugTextureViewer::Edit_MetadataInWorkbench()
{
	DEBUG_TEXTURE_VIEWER_USE_DEBUG_MEMORY();

	CDTVEntry* selectedEntry = GetSelectedListEntry();
	if (selectedEntry != NULL)
	{
		// Make sure the selected entry is a texture
		CDTVEntry_Texture* textureEntry = dynamic_cast<CDTVEntry_Texture*>(selectedEntry);
		if (textureEntry != NULL)
		{
			// Get the txd ref
			const CTxdRef& ref = textureEntry->GetTxdRef();

			// Generate the tree that represents the xml to post
			parTree* pTree = rage_new parTree();
			parTreeNode* pRoot = rage_new parTreeNode("MetadataInfo");
			pTree->SetRoot(pRoot);

			parTreeNode* pAssetName = rage_new parTreeNode("AssetName");
			parTreeNode* pAssetType = rage_new parTreeNode("AssetType");
			parTreeNode* pRPF = rage_new parTreeNode("RPFFilename");
			parTreeNode* pTexture = rage_new parTreeNode("TextureName");

			// Start populating the data
			pAssetName->SetData(ref.GetAssetName(), ustrlen(ref.GetAssetName()));
			const char* assetTypeAbbreviation = CAssetRef::GetAssetTypeName(ref.GetAssetType(), true);
			pAssetType->SetData(assetTypeAbbreviation, ustrlen(assetTypeAbbreviation));
			pRPF->SetData(ref.GetRPFName(), ustrlen(ref.GetRPFName()));
			pTexture->SetData(textureEntry->GetName().c_str(), textureEntry->GetName().length());

			pAssetName->AppendAsChildOf(pRoot);
			pAssetType->AppendAsChildOf(pRoot);
			pRPF->AppendAsChildOf(pRoot);
			pTexture->AppendAsChildOf(pRoot);

			// Convert the tree to an xml string
			datGrowBuffer gb;
			gb.Init(sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_DEBUG_VIRTUAL), datGrowBuffer::NULL_TERMINATE);

			char gbName[RAGE_MAX_PATH];
			fiDevice::MakeGrowBufferFileName(gbName, RAGE_MAX_PATH, &gb);
			fiStream* stream = ASSET.Create(gbName, "");
			PARSER.SaveTree(stream, pTree, parManager::XML);
			stream->Flush();
			stream->Close();
			const char* pXml = (const char*)gb.GetBuffer();

			// Run the query
			char workbenchIPAddress[32];
			sprintf(workbenchIPAddress, "%s:12357", bkRemotePacket::GetRagSocketAddress());

			char workbenchRESTAddress[64];
			sprintf(workbenchRESTAddress, "http://%s/MetaDataService-1.0/", workbenchIPAddress);

			fwHttpQuery query(workbenchIPAddress, workbenchRESTAddress, true);
			query.InitPost("OpenFileFromGame", 60);
			query.AppendXmlContent(pXml, istrlen(pXml));

			bool querySucceeded = false;
			if(query.CommitPost())
			{
				while(query.Pending())
				{
					sysIpcSleep(100);
				}

				querySucceeded = query.Succeeded();
			}

			if (!querySucceeded)
			{
				// Tell the user that the query failed (this is most likely because the workbench isn't open)
				Assertf(false, "An error occurred while attempting to edit the metadata file.  This is most likely because the workbench isn't open.");
			}

			delete pTree;
		}
		else
		{
			// Tell the user he doesn't have a texture selected
			Assertf(false, "A texture needs to be selected in the texture viewer for the edit metadata button to work.");
		}
	}
	else
	{
		// Tell the user to select a texture
		Assertf(false, "A texture needs to be selected in the texture viewer for the edit metadata button to work.");
	}
}

#endif // DEBUG_TEXTURE_VIEWER_EDIT_METADATA_IN_WORKBENCH

__COMMENT(static) int CDebugTextureViewer::DebugWindow_GetNumEntries()
{
	CDebugTextureViewer* _this = &gDebugTextureViewer;

	return _this->GetListNumEntries();
}

__COMMENT(static) void CDebugTextureViewer::DebugWindow_GetEntryText(char* result, int row, int column)
{
	CDebugTextureViewer* _this = &gDebugTextureViewer;

	if (row >= 0 && row < _this->GetListNumEntries())
	{
		const CDTVColumnFlags::eColumnID columnID = _this->m_columnFlags.GetColumnID(column);

		if (columnID != CDTVColumnFlags::CID_NONE)
		{
			const int maxLen = CDTVColumnFlags::GetColumnMaxChars(columnID, &_this->m_columnFlags);
			char temp[512] = "";
			_this->GetListEntry(row)->GetTextAndSortKey(temp, columnID);

			if (CDTVColumnFlags::GetColumnTruncate(columnID))
			{
				strcpy(result, StringTruncate(temp, maxLen).c_str());
			}
			else
			{
				strcpy(result, temp);
			}
		}
		else
		{
			result[0] = '\0';
		}
	}
	else
	{
		result[0] = '\0';
	}
}

__COMMENT(static) bool CDebugTextureViewer::DebugWindow_GetEntryColour(Color32* result, int row, int column)
{
	CDebugTextureViewer* _this = &gDebugTextureViewer;

	if (_this->m_columnFlags.GetColumnID(column) == _this->m_sortColumnID)
	{
		if (row == INDEX_NONE) // category
		{
			*result = Color32(64,64,255,255);
		}
		else
		{
			*result = Color32(0,0,100,255);
		}
	}
	else // default colours
	{
		if (row == INDEX_NONE) // category
		{
			*result = Color32(0,0,100,255); // DEBUGWINDOW_DEFAULTCOLOUR_LIST_CATEGORY_TEXT
		}
		else
		{
			*result = Color32(0,0,0,255); // DEBUGWINDOW_DEFAULTCOLOUR_LIST_ENTRY_TEXT
		}
	}

	if (row >= 0 && row < _this->GetListNumEntries())
	{
		if (_this->GetListEntry(row)->GetErrorFlag())
		{
			*result = Color32(255,0,0,255);
		}

		if (!_this->GetListEntry(row)->GetActiveFlag())
		{
			result->SetAlpha(128); // dimmed
		}
	}

	return true;
}

__COMMENT(static) bool CDebugTextureViewer::DebugWindow_GetRowActive(int row)
{
	CDebugTextureViewer* _this = &gDebugTextureViewer;

	if (row >= 0 && row < _this->GetListNumEntries())
	{
		return _this->GetListEntry(row)->GetActiveFlag();
	}

	return false;
}

__COMMENT(static) void CDebugTextureViewer::DebugWindow_ClickCategory(int row, int column)
{
	CDebugTextureViewer* _this = &gDebugTextureViewer;

	if (row == INDEX_NONE) // category
	{
		_this->SetSortColumnID(_this->m_columnFlags.GetColumnID(column));
		_this->m_listState.back().m_listSortColumnID = _this->m_sortColumnID; // save it
	}
}

__COMMENT(static) void CDebugTextureViewer::DebugWindow_ClickBack()
{
	CDebugTextureViewer* _this = &gDebugTextureViewer;

	_this->Back();
}

__COMMENT(static) bool CDebugTextureViewer::DebugWindow_ClickBackEnabled()
{
	CDebugTextureViewer* _this = &gDebugTextureViewer;

	return _this->m_listState.size() > 1;
}

__COMMENT(static) void CDebugTextureViewer::DebugWindow_CloseWindow()
{
	CDebugTextureViewer* _this = &gDebugTextureViewer;

	_this->m_debugWindow = NULL;
	_this->Clear(false, true); // clear everything

#if DEBUG_TEXTURE_VIEWER_DISABLE_PLAYER
	CGameWorld::GetMainPlayerInfo()->EnableControlsDebug();
#endif // DEBUG_TEXTURE_VIEWER_DISABLE_PLAYER
}

__COMMENT(static) void CDebugTextureViewer::DebugWindow_ResetPosition()
{
	CDebugTextureViewer* _this = &gDebugTextureViewer;

	if (_this->m_debugWindow)
	{
		_this->m_debugWindow->SetPos(DEBUG_TEXTURE_VIEWER_WINDOW_POS);
	}
}

__COMMENT(static) void CDebugTextureViewer::DebugWindow_DumpListToCSV(const char* path)
{
	fiStream* csv = fiStream::Create(path);

	if (csv)
	{
		CDebugTextureViewer* _this = &gDebugTextureViewer;

		for (int row = 0; row < _this->GetListNumEntries(); row++)
		{
			bool bFirst = true;

			for (int column = 0; column < CDTVColumnFlags::CID_COUNT; column++)
			{
				const CDTVColumnFlags::eColumnID columnID = _this->m_columnFlags.GetColumnID(column);

				if (columnID != CDTVColumnFlags::CID_NONE)
				{
					char temp[512] = "";
					_this->GetListEntry(row)->GetTextAndSortKey(temp, columnID);

					for (char* s = temp; *s; s++)
					{
						if (*s == ',')
						{
							*s = ';'; // replace any commas with semicolons so csv columns are preserved
						}
					}

					fprintf(csv, "%s%s", bFirst ? "" : ",", temp);
					bFirst = false;
				}
			}

			fprintf(csv, "\n");
		}

		csv->Close();
	}
}

#if DEBUG_TEXTURE_VIEWER_UIGADGET_DEMO

static void UiGadgetDemo_GetEntry(CUiGadgetText* pText, u32 col, u32 row, void* UNUSED_PARAM(extraCallbackData) )
{
	char achTmp[100];
	sprintf(achTmp, "%d %d", col, row);
	pText->SetString(achTmp);
}

/*static*/ CUiGadgetWindow* gUiGadgetDemoWindow = NULL;
/*static*/ CUiGadgetList*   gUiGadgetDemoList   = NULL;

__COMMENT(static) void CDebugTextureViewer::UiGadgetDemo()
{
	DEBUG_TEXTURE_VIEWER_USE_DEBUG_MEMORY();

	static bool bOnce = true;

	if (bOnce)
	{
		bOnce = false;

		CUiColorScheme colorScheme;
		float afColumnOffsets[] = { 0.0f, 100.0f };
		float offset = 0.0f;

		for (int i = 0; i < 3; i++)
		{
			CUiGadgetWindow* pWindow = rage_new CUiGadgetWindow(200.0f + offset, 200.0f + offset, 400.0f, 98.0f, colorScheme);
			CUiGadgetList* pList = rage_new CUiGadgetList(202.0f + offset, 224.0f + offset, 196.0f, 6, 2, afColumnOffsets, colorScheme);
			pList->SetNumEntries(52);
			pList->SetUpdateCellCB(UiGadgetDemo_GetEntry);
			pWindow->AttachChild(pList);
			g_UiGadgetRoot.AttachChild(pWindow);

			offset += 30.0f;

			gUiGadgetDemoWindow = pWindow;
			gUiGadgetDemoList   = pList;
		}
	}
}

#endif // DEBUG_TEXTURE_VIEWER_UIGADGET_DEMO

__COMMENT(static) void CDebugTextureViewer::LoadPreviewTexture()
{
	CDebugTextureViewer* _this = &gDebugTextureViewer;

	if (BANKMGR.OpenFile(_this->m_previewTextureName, sizeof(_this->m_previewTextureName), "*.dds", false, "Open DDS"))
	{
		LoadPreviewTexture_Reload();
	}
}

static bool s_LoadPreviewTexture = false;

__COMMENT(static) void CDebugTextureViewer::LoadPreviewTexture_Reload()
{
	CDebugTextureViewer* _this = &gDebugTextureViewer;

	if (_this->m_verbose)
	{
		Displayf("LoadPreviewTexture_Reload(\"%s\")", _this->m_previewTextureName);
	}

	s_LoadPreviewTexture = true;
}

__COMMENT(static) void CDebugTextureViewer::LoadPreviewTexture_Update()
{
	CDebugTextureViewer* _this = &gDebugTextureViewer;

	if (s_LoadPreviewTexture)
	{
		s_LoadPreviewTexture = false;
	}
	else
	{
		return;
	}

	if (strlen(_this->m_previewTextureName) > 0)
	{
		char pathNoExt[RAGE_MAX_PATH];
		strcpy(pathNoExt, _this->m_previewTextureName);
		char* ext = strrchr(pathNoExt, '.');

		if (ext && (stricmp(ext, ".dds") == 0 || stricmp(ext, ".tif") == 0))
		{
			*ext = '\0';
		}

		const bool nopopups = ForceNoPopupsBegin(false);
		grcTexture* texture = NULL;
		grcImage* image = grcImage::Load(_this->m_previewTextureName);

		if (AssertVerify(image))
		{
			grcTextureFactory::TextureCreateParams params(GTA_ONLY(grcTextureFactory::TextureCreateParams::VIDEO, grcTextureFactory::TextureCreateParams::TILED));
			char swizzle[6] = "RGBA\0";
			u8 conversionFlags = 0;
			u16 templateType = 0;
			bool isProcessed = false;
			bool isOptimised = false;
			bool doNotOptimise = false;

			if (_this->m_previewTextureLinear)
			{
				params.Format = grcTextureFactory::TextureCreateParams::LINEAR;
			}

			if (_this->m_previewTextureTCP[0] && AssertVerify(image->GetType() == grcImage::STANDARD))
			{
				fwTextureConversionParams tcp;
#if 1
				char root[512] = "";
				char path[512] = "";
				strcpy(path, _this->m_previewTextureTCP);
				atArray<atString> parents;
				bool tcpValid = false;

				while (true) // traverse parents
				{
					for (char* s = path; *s; s++)
					{
						if (*s == '\\') { *s = '/'; }
					}

					if (!ASSET.Exists(path, NULL))
					{
						if (root[0]) // it might be local, try appending to previous root
						{
							char path2[512] = "";
							sprintf(path2, "%s/%s", root, path);

							if (ASSET.Exists(path2, NULL))
							{
								strcpy(path, path2);
							}
							else
							{
								Assertf(0, "could not find %s or %s", path, path2);
								break;
							}
						}
						else
						{
							Assertf(0, "could not find %s", path);
							break;
						}
					}

					if (strrchr(path, '/'))
					{
						strcpy(root, path);
						strrchr(root, '/')[0] = '\0';
					}
					else
					{
						root[0] = '\0';
					}

					parents.PushAndGrow(atString(path));
					parTree* tree = PARSER.LoadTree(path, "");
					path[0] = '\0';

					if (AssertVerify(tree))
					{
						const parTreeNode* parentValNode = tree->GetRoot()->FindFromXPath("parent");

						if (parentValNode)
						{
							strcpy(path, parentValNode->GetData());
						}

						delete tree;
					}

					if (path[0] == '\0')
					{
						break;
					}
				}

				char paths[4096] = "";

				for (int i = parents.GetCount() - 1; i >= 0; i--)
				{
					if (Verifyf(PARSER.LoadObject(parents[i].c_str(), NULL, tcp), "failed to load %s", parents[i].c_str()))
					{
						strcat(paths, atVarString("%sloaded %s", path[0] ? "\n" : "", parents[i].c_str()).c_str());
						tcpValid = true;
					}
				}

				Displayf("%s", paths);
#else
				if (AssertVerify(ASSET.Exists(_this->m_previewTextureTCP, NULL)) &&
					AssertVerify(PARSER.LoadObject(_this->m_previewTextureTCP, NULL, tcp)))
				{
					tcpValid = true;
				}
#endif
				if (tcpValid)
				{
					grcImage* processed = tcp.Process(image, _this->m_previewTextureName, swizzle, conversionFlags);

					if (processed)
					{
						image->Release();
						image = processed;
						templateType = fwTextureConversionParams::GetTemplateTypeFromTemplatePath(_this->m_previewTextureTCP);
						isProcessed = true;
					}

					doNotOptimise = tcp.m_doNotOptimise;
				}
			}
			
			if (!isProcessed && !doNotOptimise)
			{
				isOptimised = fwTextureConversionParams::OptimiseCompressed(image, swizzle, conversionFlags);
			}

			texture = grcTextureFactory::GetInstance().Create(image, &params);

			if (AssertVerify(texture))
			{
				if (isProcessed || isOptimised)
				{
#if RDR_VERSION
					texture->SetTextureSwizzleFromString(swizzle, true);
#endif // RDR_VERSION
				}

				texture->SetConversionFlags(conversionFlags);
				texture->SetTemplateType(templateType);
			}

			image->Release();
		}
		
		ForceNoPopupsEnd(nopopups);

		if (texture)
		{
			grcTexture::eTextureSwizzle s[4];
			texture->GetTextureSwizzle(s[0], s[1], s[2], s[3]);
			const char* sw = "RGBA01";
			const char swizzle[5] = { sw[s[0]], sw[s[1]], sw[s[2]], sw[s[3]] };

			char dims[64] = "";
			if (texture->GetDepth() > 1) { sprintf(dims, "%dx%dx%d", texture->GetWidth(), texture->GetHeight(), texture->GetDepth()); }
			else                         { sprintf(dims, "%dx%d", texture->GetWidth(), texture->GetHeight()); }

			_this->Clear(false, true);
			_this->SetupWindow(atVarString("TEXTURE VIEWER - \"%s\" - %s %s-%s", _this->m_previewTextureName, dims, grcImage::GetFormatString((grcImage::Format)texture->GetImageFormat()), swizzle).c_str(), false);

			_this->m_previewTexture = texture;
		}
	}
}

static bool s_LoadSandboxTxd = false;

__COMMENT(static) void CDebugTextureViewer::LoadSandboxTxd()
{
	s_LoadSandboxTxd = true;
}

__COMMENT(static) void CDebugTextureViewer::LoadSandboxTxd_Update()
{
	CDebugTextureViewer* _this = &gDebugTextureViewer;

	if (s_LoadSandboxTxd)
	{
		s_LoadSandboxTxd = false;
	}
	else
	{
		return;
	}

	if (ASSET.Exists(_this->m_sandboxTxdName, NULL))
	{
		if (_this->m_verbose)
		{
			Displayf("LoadSandboxTxd(\"%s\") ...", _this->m_sandboxTxdName);
		}
	}
	else
	{
		if (_this->m_verbose)
		{
			Displayf("LoadSandboxTxd(\"%s\") - doesn't exist!", _this->m_sandboxTxdName);
		}

		return;
	}

	char sandboxTxdName[RAGE_MAX_PATH] = "";
	strcpy(sandboxTxdName, _this->m_sandboxTxdName);
	if (strrchr(sandboxTxdName, '.')) { strrchr(sandboxTxdName, '.')[0] = '\0'; } // strip extension

	strLocalIndex slot = strLocalIndex(INDEX_NONE);

#if DEBUG_TEXTURE_VIEWER_NEW_STREAMING_CODE
	if (_this->m_sandboxTxdUseNewStreamingCode)
	{
		const strIndex si = strPackfileManager::RegisterIndividualFile(_this->m_sandboxTxdName);

		if (si.IsValid())
		{
			slot = g_TxdStore.FindSlot(sandboxTxdName); // can we use the si instead?

			if (slot != INDEX_NONE)
			{
				if (CStreaming::LoadObject(slot, g_TxdStore.GetStreamingModuleId()))
				{
					bool bFound = false;

					for (int i = 0; i < _this->m_sandboxTxdSlots.size(); i++)
					{
						if (slot == _this->m_sandboxTxdSlots[i])
						{
							bFound = true;
							break;
						}
					}

					if (!bFound)
					{
						_this->m_sandboxTxdSlots.PushAndGrow(slot.Get());
					}
				}
				else // CStreaming::LoadObject failed, should we remove the slot?
				{
					slot = INDEX_NONE;
				}
			}
		}
	}
	else
#endif // DEBUG_TEXTURE_VIEWER_NEW_STREAMING_CODE
	{
		static int current = 0;
		const char* names[3] = {"sandbox_itd_0000","sandbox_itd_0001","sandbox_itd_0002"}; // triple-buffered for maximum comfort

		if (1) // remove oldest slot
		{
			const strLocalIndex old = strLocalIndex(g_TxdStore.FindSlot(names[(current + 1)%3]));

			if (_this->m_verbose)
			{
				Displayf("  attempting to remove slot %d \"%s\" ..", old.Get(), names[(current + 1)%3]);
			}

			if (old != INDEX_NONE)
			{
				g_TxdStore.Remove(old);
			}
		}

		slot = g_TxdStore.FindSlot(names[current]); // we could just call g_TxdStore.Register(names[current]) instead i think ..

		if (_this->m_verbose)
		{
			Displayf("  attempting to add new slot %d \"%s\" ..", slot.Get(), names[current]);
		}

		if (slot == INDEX_NONE)
		{
			slot = g_TxdStore.AddSlot(names[current]);

			if (_this->m_verbose)
			{
				Displayf("  slot didn't exist .. adding slot %d ..", slot.Get());
			}
		}

		if (slot != INDEX_NONE)
		{
			//pgRscBuilder::sm_bAssertSuppressionSecret = true;
			const bool bLoaded = g_TxdStore.LoadFile(slot, _this->m_sandboxTxdName);
			//pgRscBuilder::sm_bAssertSuppressionSecret = false;

			if (bLoaded)
			{
				current = (current + 1)%3;
			}
			else
			{
				slot = strLocalIndex(INDEX_NONE);
			}
		}
	}

	if (slot.Get() != INDEX_NONE)
	{
		const CTxdRef ref(AST_TxdStore, slot.Get(), INDEX_NONE, "SANDBOX");

		_this->Clear(false, true);
		_this->Populate_SelectTxd(_this->m_sandboxTxdName, ref);

#if DEBUG_TEXTURE_VIEWER_SELECTPREVIEW
		if (0) // this is causing problems, sandbox textures do not look right in preview mode .. not sure why
		{
			const fwTxd* previewTxd = g_TxdStore.GetSafeFromIndex(slot);

			if (previewTxd)
			{
				_this->m_previewTxd = previewTxd;
			}

			_this->ClearSelectedListEntry();
		}
#endif // DEBUG_TEXTURE_VIEWER_SELECTPREVIEW

		ref.AddRef(CAssetRefInterface(_this->m_assetRefs, _this->m_verbose));
	}
}

#if DEBUG_TEXTURE_VIEWER_NEW_STREAMING_CODE

__COMMENT(static) void CDebugTextureViewer::RemoveSandboxTxds()
{
	CDebugTextureViewer* _this = &gDebugTextureViewer;

	for (int i = 0; i < _this->m_sandboxTxdSlots.size(); i++)
	{
		const strLocalIndex slot = strLocalIndex(_this->m_sandboxTxdSlots[i]);

		if (g_TxdStore.GetNumRefs(slot) == 0)
		{
			if (_this->m_verbose)
			{
				Displayf("CDebugTextureViewer::RemoveSandboxTxds: removing \"%s\" from slot %d", g_TxdStore.GetName(slot), slot.Get());
			}

			CStreaming::ClearRequiredFlag(slot, g_TxdStore.GetStreamingModuleId(), STRFLAG_DONTDELETE);
			CStreaming::RemoveObject     (slot, g_TxdStore.GetStreamingModuleId());

			if (i < _this->m_sandboxTxdSlots.size() - 1)
			{
				_this->m_sandboxTxdSlots[i--] = _this->m_sandboxTxdSlots.Pop();
			}
		}
	}

	if (_this->m_verbose)
	{
		Displayf("CDebugTextureViewer::RemoveSandboxTxds: %d slots remaining", _this->m_sandboxTxdSlots.size());
	}
}

#endif // DEBUG_TEXTURE_VIEWER_NEW_STREAMING_CODE

__COMMENT(static) void CDebugTextureViewer::LoadAll()
{
	CDebugTextureViewer* _this = &gDebugTextureViewer;

	for (int i = 0; i < _this->GetListNumEntries(); i++)
	{
		_this->GetListEntry(i)->Load(CAssetRefInterface(_this->m_assetRefs, _this->m_verbose));
	}
}

// ================================================================================================

#if DEBUG_TEXTURE_VIEWER_GET_WORLD_POS_UNDER_MOUSE

#if __XENON
__forceinline float ConvertD24F(u32 raw)
{
	// assume raw is 20e4, so we want to compute 1.mmmmmmmmmmmmmmmmmmmm * 2^(e - 16)
	// see "Floating-Point Depth Buffers" in 360 docs

	const u32 e = raw >> 20;
	const u32 m = raw & 0x000fffff;

	return (1.0f + (float)m/(float)0x00100000)/(float)(1<<(16 - e));
}
#endif // __XENON

class CDTVGetWorldPosUnderMouse
{
public:
	static void Update();
	static void Render();

	static Vec4V m_worldPos; // w is linear z in viewspace (these should really be double-buffered .. let's see if it matters)
	static Vec3V m_worldNormal;
	static int   m_stencil;
	static int   m_mouseX;
	static int   m_mouseY;
	static bool  m_mouseMoved;
};

__COMMENT(static) Vec4V CDTVGetWorldPosUnderMouse::m_worldPos    = Vec4V(V_ZERO);
__COMMENT(static) Vec3V CDTVGetWorldPosUnderMouse::m_worldNormal = Vec3V(V_ZERO);
__COMMENT(static) int   CDTVGetWorldPosUnderMouse::m_stencil     = 0;
__COMMENT(static) int   CDTVGetWorldPosUnderMouse::m_mouseX      = 0;
__COMMENT(static) int   CDTVGetWorldPosUnderMouse::m_mouseY      = 0;
__COMMENT(static) bool  CDTVGetWorldPosUnderMouse::m_mouseMoved  = false;

__COMMENT(static) void CDTVGetWorldPosUnderMouse::Update()
{
#if ENTITYSELECT_CHECK
	if (gDebugTextureViewer.m_assertEntitySelectOn)
	{
		Assert(CEntitySelect::IsEntitySelectPassEnabled());
	}

	grcDebugDraw::AddDebugOutput(Color32(255,255,255,255), "entity select is %s", CEntitySelect::IsEntitySelectPassEnabled() ? "ON" : "OFF");
#endif // ENTITYSELECT_CHECK

	if (gDebugTextureViewer.m_showWorldPosUnderMouse || gDebugTextureViewer.m_showWorldPosExternalRefCount > 0)
	{
		if (ioMouse::GetButtons() & ioMouse::MOUSE_LEFT)
		{
			const int x = m_mouseX;
			const int y = m_mouseY;

			m_mouseX = ioMouse::GetX();
			m_mouseY = ioMouse::GetY();

			m_mouseMoved = (m_mouseX != x || m_mouseY != y);
		}
	}

	if (gDebugTextureViewer.m_showWorldPosMarker)
	{
		grcDebugDraw::Sphere(m_worldPos.GetXYZ(), 0.03f, Color32(255,0,0,255), false);
		grcDebugDraw::Line(m_worldPos.GetXYZ(), m_worldPos.GetXYZ() + m_worldNormal, Color32(0,0,255,255));

#if __XENON // TODO -- this isn't working on 360, so i've added some extra lines to help debug it
		grcDebugDraw::Line(m_worldPos.GetXYZ() + Vec3V(-6.0f,-6.0f, 0.0f), m_worldPos.GetXYZ() - Vec3V(-6.0f,-6.0f, 0.0f), Color32(255,0,255,255));
		grcDebugDraw::Line(m_worldPos.GetXYZ() + Vec3V(+6.0f,-6.0f, 0.0f), m_worldPos.GetXYZ() - Vec3V(+6.0f,-6.0f, 0.0f), Color32(255,0,255,255));
		grcDebugDraw::Line(m_worldPos.GetXYZ() + Vec3V(-6.0f, 0.0f,-6.0f), m_worldPos.GetXYZ() - Vec3V(-6.0f, 0.0f,-6.0f), Color32(255,0,255,255));
		grcDebugDraw::Line(m_worldPos.GetXYZ() + Vec3V(+6.0f, 0.0f,-6.0f), m_worldPos.GetXYZ() - Vec3V(+6.0f, 0.0f,-6.0f), Color32(255,0,255,255));
		grcDebugDraw::Line(m_worldPos.GetXYZ() + Vec3V( 0.0f,-6.0f,-6.0f), m_worldPos.GetXYZ() - Vec3V( 0.0f,-6.0f,-6.0f), Color32(255,0,255,255));
		grcDebugDraw::Line(m_worldPos.GetXYZ() + Vec3V( 0.0f,+6.0f,-6.0f), m_worldPos.GetXYZ() - Vec3V( 0.0f,+6.0f,-6.0f), Color32(255,0,255,255));
#endif // __XENON
	}
}

// note: element pointed to by ptr must not cross a 16-byte boundary
template <typename T> __forceinline T GetFromLocalMemory(const T* ptr, int i = 0)
{
#if __XENON || __PS3
	const u32 addr = reinterpret_cast<const u32>(ptr + i);
	const u32 base = addr & ~15; // aligned

	ALIGNAS(16) u8 temp[16] ;

#if __XENON
	*reinterpret_cast<Vec::Vector_4V_uint*>(temp) = __lvx(reinterpret_cast<const u32*>(base), 0);
#else
	*reinterpret_cast<Vec::Vector_4V_uint*>(temp) = vec_ld(0, reinterpret_cast<const u32*>(base));
#endif
	return *reinterpret_cast<const T*>(temp + (addr & 15));
#else
	return ptr[i];	
#endif
}

static __forceinline int GetLocalMemoryAddress(int x, int y, int w, int pitch)
{
#if __XENON
	UNUSED_VAR(pitch);
	Assertf(pitch == w*(int)sizeof(u32), "grcTexture::LockRect pitch = %d, expected %d", pitch, w*sizeof(u32));
	return XGAddress2DTiledOffset(x, y, w, sizeof(u32));
#else
	UNUSED_VAR(w);
	return x + y*(pitch/sizeof(u32));
#endif
}

__COMMENT(static) void CDTVGetWorldPosUnderMouse::Render()
{
	bool bRender = (gDebugTextureViewer.m_showWorldPosExternalRefCount > 0);

	if (gDebugTextureViewer.m_showWorldPosUnderMouse)
	{
		if (m_mouseMoved) // we could update continuously .. maybe add a flag for this?
		{
			if (gDebugTextureViewer.m_showWorldPosUnderMouse ||
				gDebugTextureViewer.m_showWorldPosMarker)
			{
				bRender = true;
			}
		}
	}

	if (bRender)
	{
		const grcViewport* vp = gVpMan.GetRenderGameGrcViewport();

		grcRenderTarget* rt = GBuffer::GetDepthTarget();
		const int w = rt->GetWidth ();
		const int h = rt->GetHeight();
		const int x = Clamp<int>(m_mouseX, 0, w - 1);
		const int y = Clamp<int>(m_mouseY, 0, h - 1);

		grcTextureLock lock;

		if (AssertVerify(rt->LockRect(0, 0, lock, grcsRead | grcsAllowVRAMLock)))
		{
			const int   addr = GetLocalMemoryAddress(x, y, w, lock.Pitch);
			const u32   r    = GetFromLocalMemory((const u32*)lock.Base, addr); // raw data from depth buffer
			const float d    = XENON_SWITCH(ConvertD24F(r >> 8), (float)(r >> 8)/(float)((1<<24) - 1));
			const float z    = ShaderUtil::GetLinearDepth(vp, ScalarV(d)).Getf();

			rt->UnlockRect(lock);

			const Mat34V  localToWorld = vp->GetCameraMtx();
			const ScalarV tanHFOV      = ScalarV(vp->GetTanHFOV());
			const ScalarV tanVFOV      = ScalarV(vp->GetTanVFOV());
			const ScalarV screenX      = ScalarV(-1.0f + 2.0f*(float)x/(float)w);
			const ScalarV screenY      = ScalarV(+1.0f - 2.0f*(float)y/(float)h); // flip y
			const ScalarV screenZ      = ScalarV(z);

			m_worldPos = Vec4V(Transform(localToWorld, Vec3V(tanHFOV*screenX, tanVFOV*screenY, ScalarV(V_NEGONE))*screenZ), screenZ);
			m_stencil  = (int)(r & 0xff);
		}

		// get normal from GBuffer
		{
			rt = GBuffer::GetTarget(GBUFFER_RT_1);

			if (AssertVerify(rt->LockRect(0, 0, lock, grcsRead | grcsAllowVRAMLock)))
			{
				const int addr = GetLocalMemoryAddress(x, y, w, lock.Pitch);
				const u32 r    = GetFromLocalMemory((const u32*)lock.Base, addr); // raw data
#if __XENON
				const float nx = (float)((r >> (8*2))&0xff)/127.5f - 1.0f;
				const float ny = (float)((r >> (8*1))&0xff)/127.5f - 1.0f;
				const float nz = (float)((r >> (8*0))&0xff)/127.5f - 1.0f;
#else
				const float nx = (float)((r >> (8*0))&0xff)/127.5f - 1.0f;
				const float ny = (float)((r >> (8*1))&0xff)/127.5f - 1.0f;
				const float nz = (float)((r >> (8*2))&0xff)/127.5f - 1.0f;
#endif
				rt->UnlockRect(lock);

				m_worldNormal = Normalize(Vec3V(nx, ny, nz));
			}
		}
	}
}

#endif // DEBUG_TEXTURE_VIEWER_GET_WORLD_POS_UNDER_MOUSE

// ================================================================================================

#if DEBUG_TEXTURE_VIEWER_FRAMEBUFFER_MAGNIFIER

class CDTVMagnifier
{
public:
	enum eMagnifierBuffers
	{
		MAGNIFIER_BUFFER_FRONTBUFFER,
#if __PS3 || __WIN32PC
		MAGNIFIER_BUFFER_BACKBUFFER,
#endif // __PS3 || __WIN32PC
		MAGNIFIER_BUFFER_ALBEDO,
		MAGNIFIER_BUFFER_NORMALS,
		MAGNIFIER_BUFFER_SPECULAR,
		MAGNIFIER_BUFFER_SHADOWS,
		MAGNIFIER_BUFFER_AMBIENT,
		MAGNIFIER_BUFFER_DEPTH,
		MAGNIFIER_BUFFER_COUNT
	};

	static void Update();
	static void Render();

	static void EnabledCallback(); // called when switched on or off

	static Color32 m_pixels[24*24];
	static int     m_mouseX;
	static int     m_mouseY;
	static bool    m_mouseMoved;
};

__COMMENT(static) Color32 CDTVMagnifier::m_pixels[24*24];
__COMMENT(static) int     CDTVMagnifier::m_mouseX     = -1; // don't display until this is >= 0
__COMMENT(static) int     CDTVMagnifier::m_mouseY     = 0;
__COMMENT(static) bool    CDTVMagnifier::m_mouseMoved = false;

__COMMENT(static) void CDTVMagnifier::Update()
{
	if (gDebugTextureViewer.m_magnifierEnabled)
	{
		if ((ioMouse::GetButtons() & ioMouse::MOUSE_RIGHT) != 0 || !gDebugTextureViewer.m_magnifierPixelPicker)
		{
			const int x = m_mouseX;
			const int y = m_mouseY;

			m_mouseX = ioMouse::GetX();
			m_mouseY = ioMouse::GetY();

			m_mouseMoved = (m_mouseX != x || m_mouseY != y);
		}
	}
}

__COMMENT(static) void CDTVMagnifier::Render()
{
	if (gDebugTextureViewer.m_magnifierEnabled && m_mouseX != -1)
	{
		grcImage* pScreenImage = GRCDEVICE.CaptureScreenShot(NULL); // TODO -- this is pretty inefficient .. would be better to just grab the pixels we need

		if (AssertVerify(pScreenImage))
		{
			const int w = pScreenImage->GetWidth();
			const int h = pScreenImage->GetHeight();
			const int x = Clamp<int>(m_mouseX - gDebugTextureViewer.m_magnifierRes/2, 0, w - gDebugTextureViewer.m_magnifierRes);
			const int y = Clamp<int>(m_mouseY - gDebugTextureViewer.m_magnifierRes/2, 0, h - gDebugTextureViewer.m_magnifierRes);

			for (int yy = 0; yy < gDebugTextureViewer.m_magnifierRes; yy++)
			{
				for (int xx = 0; xx < gDebugTextureViewer.m_magnifierRes; xx++)
				{
					if (x + xx >= 0 && x + xx < w - 1 &&
						y + yy >= 0 && y + yy < h - 1)
					{
						m_pixels[xx + yy*24] = ((const Color32*)pScreenImage->GetBits())[(x + xx) + (y + yy)*w];
					}
					else
					{
						m_pixels[xx + yy*24] = Color32(0);
					}

					m_pixels[xx + yy*24].SetAlpha(255);
				}
			}

			pScreenImage->Release();
		}

		/*grcRenderTarget* rt = NULL;

		switch (gDebugTextureViewer.m_magnifierBuffer)
		{
#if __WIN32PC
		case MAGNIFIER_BUFFER_BACKBUFFER  :
		case MAGNIFIER_BUFFER_FRONTBUFFER : rt = grcTextureFactory::GetInstance().GetBackBuffer(); break;
#else
		case MAGNIFIER_BUFFER_FRONTBUFFER : rt = grcTextureFactory::GetInstance().GetFrontBuffer(); break;
#endif // __WIN32PC
#if __PS3
		case MAGNIFIER_BUFFER_BACKBUFFER  : rt = CRenderTargets::GetBackBuffer(); break; // this target is 64 bpp on 360 and PC ..
#endif // __PS3 || __WIN32PC
		case MAGNIFIER_BUFFER_ALBEDO      : rt = GBuffer::GetTarget(GBUFFER_RT_0); break;
		case MAGNIFIER_BUFFER_NORMALS     : rt = GBuffer::GetTarget(GBUFFER_RT_1); break;
		case MAGNIFIER_BUFFER_SPECULAR    : rt = GBuffer::GetTarget(GBUFFER_RT_2); break;
		case MAGNIFIER_BUFFER_SHADOWS     : rt = GBuffer::GetTarget(GBUFFER_RT_2); break; // alpha
		case MAGNIFIER_BUFFER_AMBIENT     : rt = GBuffer::GetTarget(GBUFFER_RT_3); break;
		case MAGNIFIER_BUFFER_DEPTH       : rt = GBuffer::GetDepthTarget(); break;
		}

		const int w = rt->GetWidth ();
		const int h = rt->GetHeight();
		const int x = Clamp<int>(m_mouseX - gDebugTextureViewer.m_magnifierRes/2, 0, w - gDebugTextureViewer.m_magnifierRes);
		const int y = Clamp<int>(m_mouseY - gDebugTextureViewer.m_magnifierRes/2, 0, h - gDebugTextureViewer.m_magnifierRes);

		grcTextureLock lock;

		if (rt->LockRect(0, 0, lock, grcsRead | grcsAllowVRAMLock))
		{
			if (gDebugTextureViewer.m_magnifierBuffer == MAGNIFIER_BUFFER_DEPTH)
			{
				const grcViewport* vp = gVpMan.GetRenderGameGrcViewport();

				float zmin = 0.0f;
				float zmax = 0.0f;
				float zdat[24*24];

				for (int yy = 0; yy < gDebugTextureViewer.m_magnifierRes; yy++)
				{
					for (int xx = 0; xx < gDebugTextureViewer.m_magnifierRes; xx++)
					{
						const int     addr = GetLocalMemoryAddress(x+xx, y+yy, w, lock.Pitch);
						const Color32 c    = GetFromLocalMemory((const Color32*)lock.Base, addr);
						const u32     r    = c.GetColor(); // raw data from depth buffer
						const float   d    = XENON_SWITCH(ConvertD24F(r >> 8), (float)(r >> 8)/(float)((1<<24) - 1));
						const float   z    = ShaderUtil::GetLinearDepth(vp, ScalarV(d)).Getf();

						if (xx == 0 && yy == 0)
						{
							zmin = z;
							zmax = z;
						}
						else
						{
							zmin = Min<float>(z, zmin);
							zmax = Max<float>(z, zmax);
						}

						zdat[xx + yy*24] = z;
					}
				}

				if (zmin < zmax)
				{
					for (int yy = 0; yy < gDebugTextureViewer.m_magnifierRes; yy++)
					{
						for (int xx = 0; xx < gDebugTextureViewer.m_magnifierRes; xx++)
						{
							const float z = (zdat[xx + yy*24] - zmin)/(zmax - zmin);
							const int zi = (int)(z*255.0f + 0.5f);

							m_pixels[xx + yy*24] = Color32(zi, zi, zi, 255);
						}
					}
				}
			}
			else // not depth buffer
			{
				for (int yy = 0; yy < gDebugTextureViewer.m_magnifierRes; yy++)
				{
					for (int xx = 0; xx < gDebugTextureViewer.m_magnifierRes; xx++)
					{
						const int     addr = GetLocalMemoryAddress(x + xx, y + yy, w, lock.Pitch);
						const Color32 c    = Color32(GetFromLocalMemory((const Color32*)lock.Base, addr).GetDeviceColor());
						const int     r    = c.GetRed();
						const int     g    = c.GetGreen();
					//	const int     b    = c.GetBlue();
						const int     a    = c.GetAlpha();

						if (gDebugTextureViewer.m_magnifierBuffer == MAGNIFIER_BUFFER_SHADOWS)
						{
							m_pixels[xx + yy*24] = Color32(255 - a, 255 - a, 255 - a, 255);
						}
						else if (gDebugTextureViewer.m_magnifierBuffer == MAGNIFIER_BUFFER_FRONTBUFFER)
						{
							m_pixels[xx + yy*24] = Color32(g, r, a, 255); // frontbuffer has a strange format ..
						}
					}
				}
			}

			rt->UnlockRect(lock);
		}*/
	}
}

__COMMENT(static) void CDTVMagnifier::EnabledCallback()
{
#if MOUSE_RENDERING_SUPPORT
	if (gDebugTextureViewer.m_magnifierEnabled && !gDebugTextureViewer.m_magnifierPixelPicker)
	{
		grcSetup::m_DisableMousePointerRefCount = 1;
	}
	else
	{
		grcSetup::m_DisableMousePointerRefCount = 0;
	}
#endif
}

#endif // DEBUG_TEXTURE_VIEWER_FRAMEBUFFER_MAGNIFIER

// ================================================================================================

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEST

static CDTVStreamingIteratorTest_TxdStore      g_si_TxdStore     ;
static CDTVStreamingIteratorTest_DwdStore      g_si_DwdStore     ;
static CDTVStreamingIteratorTest_DrawableStore g_si_DrawableStore;
static CDTVStreamingIteratorTest_FragmentStore g_si_FragmentStore;
static CDTVStreamingIteratorTest_ParticleStore g_si_ParticleStore;
static CDTVStreamingIteratorTest_MapDataStore  g_si_MapDataStore ;

static void StreamingIteratorsReset()
{
	g_si_TxdStore     .Reset();
	g_si_DwdStore     .Reset();
	g_si_DrawableStore.Reset();
	g_si_FragmentStore.Reset();
	g_si_ParticleStore.Reset();
	g_si_MapDataStore .Reset();

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS
	extern void CDTVStreamingIteratorTest_BuildTextureUsageMapsReset();
	CDTVStreamingIteratorTest_BuildTextureUsageMapsReset();
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_FIND_UNUSED
	extern void CDTVStreamingIteratorTest_FindUnusedSharedTexturesReset();
	CDTVStreamingIteratorTest_FindUnusedSharedTexturesReset();
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_FIND_UNUSED

	if (gStreamingIteratorTest_BuildAssetAnalysisData)
	{
		AssetAnalysis::FinishWritingStreams_PRIVATE();
		return;
	}
}

// for now, these just iterate over pools which contain CDynamicEntity's and report frag children
static bool _EntityIterator_CAnimatedBuilding(void* item, void*) { PrintFragChildrenDesc(static_cast<CAnimatedBuilding*>(item)); return true; }
static bool _EntityIterator_CObject          (void* item, void*) { PrintFragChildrenDesc(static_cast<CObject*>(item)); return true; }
static bool _EntityIterator_CPed             (void* item, void*) { PrintFragChildrenDesc(static_cast<CPed*>(item)); return true; }
static bool _EntityIterator_CVehicle         (void* item, void*) { PrintFragChildrenDesc(static_cast<CVehicle*>(item)); return true; }
static void _EntityIterator()
{
	Displayf("scanning CAnimatedBuilding pool .."); CAnimatedBuilding::GetPool()->ForAll(_EntityIterator_CAnimatedBuilding, NULL);
	Displayf("scanning CObject pool .."          ); CObject          ::GetPool()->ForAll(_EntityIterator_CObject          , NULL);
	Displayf("scanning CPed pool .."             ); CPed             ::GetPool()->ForAll(_EntityIterator_CPed             , NULL);
	Displayf("scanning CVehicle pool .."         ); CVehicle         ::GetPool()->ForAll(_EntityIterator_CVehicle         , NULL);
}

class CModelRef
{
public:
	CModelRef() {}
	CModelRef(u32 modelInfoIndex)
	{
		m_modelInfoIndex = modelInfoIndex;
		m_nameHash       = atStringHash(CModelInfo::GetBaseModelInfoName(fwModelId(strLocalIndex(modelInfoIndex))));
	}

	static s32 CompareFunc   (CModelRef  const* a, CModelRef  const* b) { if (a->m_nameHash > b->m_nameHash) return +1; else if (a->m_nameHash < b->m_nameHash) return -1; return 0; }
	static s32 CompareFuncPtr(CModelRef* const* a, CModelRef* const* b) { return CompareFunc(*a, *b); }

	u32 m_modelInfoIndex;
	u32 m_nameHash;
};

// this returns the same txdIndex as GetAssetParentTxdIndex(), unless the model is broken ..
template <typename T> static int _fwArchetype_GetDrawableParentTxdIndex(const T* archetype)
{
	switch (archetype->GetDrawableType())
	{
	case fwArchetype::DT_DRAWABLEDICTIONARY : { const int slot = archetype->SafeGetDrawDictIndex(); if (g_DwdStore     .IsValidSlot(slot)) { return g_DwdStore     .GetParentTxdForSlot(slot); } } break;
	case fwArchetype::DT_DRAWABLE           : { const int slot = archetype->SafeGetDrawableIndex(); if (g_DrawableStore.IsValidSlot(slot)) { return g_DrawableStore.GetParentTxdForSlot(slot); } } break;
	case fwArchetype::DT_FRAGMENT           : { const int slot = archetype->SafeGetFragmentIndex(); if (g_FragmentStore.IsValidSlot(slot)) { return g_FragmentStore.GetParentTxdForSlot(slot); } } break;
	default                                 : break;
	}

	return -1;
}

static void _ScanModelsForBullshit()
{
	atArray<CModelRef> modelRefs;
	bool bOk = true;

	for (u32 i = 0; i < CDebugArchetype::GetMaxDebugArchetypeProxies(); i++)
	{
		const CDebugArchetypeProxy* modelInfo = CDebugArchetype::GetDebugArchetypeProxy(i);

		if (modelInfo == NULL) // this can happen, it seems .. not sure why
		{
			continue;
		}

		modelRefs.PushAndGrow(CModelRef(i));

		const int txdIndex1 = modelInfo->GetAssetParentTxdIndex();
		const int txdIndex2 = _fwArchetype_GetDrawableParentTxdIndex(modelInfo);

		if (txdIndex1 != INDEX_NONE &&
			txdIndex2 != INDEX_NONE &&
			txdIndex2 != txdIndex1)
		{
			Displayf(
				"Model \"%s\" inherits Txd:%d(%s) directly, and Txd:%d(%s) through drawable!",
				modelInfo->GetModelName(),
				txdIndex1,
				g_TxdStore.GetName(txdIndex1),
				txdIndex2,
				g_TxdStore.GetName(txdIndex2)
			);

			bOk = false;
		}

		// expect that if the object has packed textures, the drawable will have a txd .. otherwise the modelinfo will have a txd, but not both
		const Drawable* drawable = modelInfo->GetDrawable();

		if (drawable)
		{
			const fwTxd* drawableTxd = drawable->GetTexDictSafe();
			const fwTxd* parentTxd   = g_TxdStore.GetSafeFromIndex(txdIndex1);

			if (drawableTxd && drawableTxd->GetCount() > 1 && parentTxd && parentTxd->GetCount() > 0)
			{
				// this actually isn't an error .. all unique textures (not referenced by any other drawables) will be packed into the drawable, not just one
				if (0)
				{
					Displayf(
						"Model \"%s\" has %d textures in its drawable, and non-empty shared parent Txd:%d(%s)!",
						modelInfo->GetModelName(),
						drawableTxd->GetCount(),
						txdIndex1,
						g_TxdStore.GetName(txdIndex1)
					);

					bOk = false;
				}
			}

			if (drawableTxd && drawableTxd->GetCount() == 0)
			{
				Displayf("Model \"%s\" has an empty txd in its drawable!", modelInfo->GetModelName());
				bOk = false;
			}
		}
		else
		{
			// model hasn't loaded, shit
		}
	}

	modelRefs.QSort(0, -1, CModelRef::CompareFunc);

	for (int i = 0; i < modelRefs.size(); i++)
	{
		int i2 = i + 1;

		while (i2 < modelRefs.size() && modelRefs[i2].m_nameHash == modelRefs[i].m_nameHash)
		{
			i2++;
		}

		if (i2 - i > 1)
		{
			Displayf("model \"%s\" has %d name collisions!", CModelInfo::GetBaseModelInfoName(fwModelId(modelRefs[i].m_modelInfoIndex)), i2 - i - 1);
			bOk = false;
		}

		i = i2 - 1;
	}

	if (bOk)
	{
		Displayf("no BS.");
	}
}

#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEST

#if DEBUG_TEXTURE_VIEWER_ADVANCED

static void _PrintTxdChildren()
{
	PrintTxdChildren(g_TxdStore.FindSlot(gDebugTextureViewer.m_searchFilter));
}

#endif // DEBUG_TEXTURE_VIEWER_ADVANCED

// ================================================================================================

#if DEBUG_TEXTURE_VIEWER_TEXTUREUSAGEANALYSER

class CTextureUsageEntry;
class CTextureUsageCount;
class CTextureUsageRefs;

class CTextureUsageEntry
{
public:
	CTextureUsageEntry() {}
	CTextureUsageEntry(const grcTexture* texture, bool bHashTextureData) : m_texture(texture), m_hash(0)
	{
		if (bHashTextureData)
		{
			const int w = texture->GetWidth ();
			const int h = texture->GetHeight();

			grcTextureLock lock;
			texture->LockRect(0, 0, lock, grcsRead | grcsAllowVRAMLock);
			m_hash = atDataHash((const char*)lock.Base, grcImage::GetFormatBitsPerPixel((grcImage::Format)texture->GetImageFormat())*w*h/8, 0); // hash base level (no mipmaps)
			texture->UnlockRect(lock);
		}
	}

	operator int() const
	{
		return atStringHash(m_texture->GetName(), m_hash);
	}

	const grcTexture* m_texture;
	u32               m_hash;
};

class CTextureUsageCount : public CTextureUsageEntry
{
public:
	CTextureUsageCount() {}
	CTextureUsageCount(const CTextureUsageEntry& entry, int count) : CTextureUsageEntry(entry), m_count(count) {}

	static s32 CompareFunc(CTextureUsageCount const* a, CTextureUsageCount const* b)
	{
		if (b->m_count != a->m_count)
		{
			return b->m_count - a->m_count;
		}

		return b->m_texture->GetPhysicalSize() - a->m_texture->GetPhysicalSize(); // sort by memory
	}

	int m_count;
};

class CTextureUsageRefs
{
public:
	atArray<u16> m_modelIndices;
	atArray<u16> m_modelIndices_NotUsedByShaders;
	atArray<CTxdRef> m_txdRefs;

	void SortTxdRefs()
	{
		m_txdRefs.QSort(0, -1, CompareTxdRefsFunc);
	}

private:
	static s32 CompareTxdRefsFunc(CTxdRef const* a, CTxdRef const* b)
	{
		return (a->operator int()) - (b->operator int());
	}
};

static void TextureUsageAddEntry(
	atMap<CTextureUsageEntry,int>& map,
	atMap<const fwTxd*,bool>&      set,
	const CTxdRef&                 _txdRef,
	bool                           bHashTextureData
	)
{
	CTxdRef txdRef = _txdRef;

	while (txdRef.GetTxd() && set.Access(txdRef.GetTxd()) == NULL)
	{
		const fwTxd* txd = txdRef.GetTxd();

		set[txd] = true;

		for (int k = 0; k < txd->GetCount(); k++)
		{
			const grcTexture* texture = txd->GetEntry(k);

			if (texture)
			{
				const CTextureUsageEntry key(texture, bHashTextureData);
				map[key]++;
			}
		}

		txdRef = CTxdRef(AST_TxdStore, txdRef.GetParentTxdIndex(), INDEX_NONE, "");
	}
}

static void TextureUsageAddModelRefs(
	atMap<CTextureUsageEntry,int>&               map,
	atMap<CTextureUsageEntry,CTextureUsageRefs>& refs,
	const CTxdRef&                               _txdRef,
	strLocalIndex                                modelIndex,
	bool                                         bHashTextureData,
	bool                                         bShowTexturesNotUsedByShader,
	bool                                         bShowModelReferences
	)
{
	CTxdRef txdRef = _txdRef;

	while (txdRef.GetTxd())
	{
		const fwTxd* txd = txdRef.GetTxd();

		for (int k = 0; k < txd->GetCount(); k++)
		{
			const grcTexture* texture = txd->GetEntry(k);

			if (texture)
			{
				const CTextureUsageEntry key(texture, bHashTextureData);

				if (map[key] > 0) // worst offenders
				{
					const CBaseModelInfo* modelInfo = CModelInfo::GetBaseModelInfo(fwModelId(modelIndex));
					CTextureUsageRefs& data = refs[key];

					if (bShowModelReferences)
					{
						bool bUniqueModelIndex = true;

						for (int i = 0; i < data.m_modelIndices.size(); i++)
						{
							if (data.m_modelIndices[i] == modelIndex.Get())
							{
								bUniqueModelIndex = false;
								break;
							}
						}

						if (bUniqueModelIndex)
						{
							for (int i = 0; i < data.m_modelIndices_NotUsedByShaders.size(); i++)
							{
								if (data.m_modelIndices_NotUsedByShaders[i] == modelIndex.Get())
								{
									bUniqueModelIndex = false;
									break;
								}
							}
						}

						if (bUniqueModelIndex)
						{
							if (IsTextureUsedByModel(texture, NULL, modelInfo, NULL, NULL, NULL, true))
							{
								data.m_modelIndices.PushAndGrow((u16)modelIndex.Get());
							}
							else if (bShowTexturesNotUsedByShader)
							{
								data.m_modelIndices_NotUsedByShaders.PushAndGrow((u16)modelIndex.Get());
							}
						}
					}

					if (1) // show txd refs
					{
						bool bUniqueTxdRef = true;

						for (int i = 0; i < data.m_txdRefs.size(); i++)
						{
							if (data.m_txdRefs[i] == txdRef)
							{
								bUniqueTxdRef = false;
								break;
							}
						}

						if (bUniqueTxdRef)
						{
							data.m_txdRefs.PushAndGrow(txdRef);
						}
					}
				}
			}
		}

		txdRef = CTxdRef(AST_TxdStore, txdRef.GetParentTxdIndex(), INDEX_NONE, "");
	}
}

static void _TextureUsageAnalyser()
{
	CDebugTextureViewer* _this = &gDebugTextureViewer;

	Displayf("");
	Displayf("======================");
	Displayf("TEXTURE USAGE ANALYSER");
	Displayf("======================");

	atMap<CTextureUsageEntry,int> map; // may be large ..
	atMap<CTextureUsageEntry,CTextureUsageRefs> refs; // worst offenders
	atArray<CTextureUsageCount> list; // for sorting map [key,data] pairs, this is dumb .. there should be a better way to do this
	atArray<u32> modelIndices;

	FindAssets_ModelInfo(modelIndices, _this->m_searchFilter, "", _this->m_asp, _this->m_findAssetsMaxCount);

	// build map of entries
	{
		atMap<const fwTxd*,bool> set; // prevent TXDs from being scanned more than once

		for (int i = 0 ; i < modelIndices.size(); i++)
		{
			strLocalIndex modelIndex = strLocalIndex(modelIndices[i]);
			atArray<CTxdRef> temp; GetAssociatedTxds_ModelInfo(temp, CModelInfo::GetBaseModelInfo(fwModelId(modelIndex)), NULL, NULL);

			for (int j = 0; j < temp.size(); j++)
			{
				TextureUsageAddEntry(map, set, temp[j], _this->m_textureUsageAnalyserHashTextureData);
			}
		}
	}

	// build list and sort
	{
		for (atMap<CTextureUsageEntry,int>::Iterator iter = map.CreateIterator(); iter; ++iter)
		{
			list.PushAndGrow(CTextureUsageCount(iter.GetKey(), iter.GetData()));
		}

		if (list.size() > 0)
		{
			list.QSort(0, -1, CTextureUsageCount::CompareFunc);

			int count = Min<int>(list.size(), _this->m_textureUsageAnalyserMaxCount);

			if (1) // skip entries with less than 2 references
			{
				for (int i = 0; i < count; i++)
				{
					if (list[i].m_count < 2)
					{
						count = i;
						break;
					}
				}
			}

			if (list.size() > count)
			{
				for (int i = count; i < list.size(); i++)
				{
					map[list[i]] = 0; // disable entry (not worst offender)
				}

				list.Resize(count);
			}
		}
	}

	if (_this->m_textureUsageAnalyserBuildRefs)
	{
		for (int i = 0 ; i < modelIndices.size(); i++)
		{
			const strLocalIndex modelIndex = strLocalIndex(modelIndices[i]);
			atArray<CTxdRef> temp; GetAssociatedTxds_ModelInfo(temp, CModelInfo::GetBaseModelInfo(fwModelId(modelIndex)), NULL, NULL);

			for (int j = 0; j < temp.size(); j++)
			{
				TextureUsageAddModelRefs(
					map,
					refs,
					temp[j],
					modelIndex,
					_this->m_textureUsageAnalyserHashTextureData,
					_this->m_textureUsageAnalyserShowTexturesNotUsedByShader,
					_this->m_textureUsageAnalyserShowModelReferences
				);
			}
		}

		for (int i = 0; i < list.size(); i++)
		{
			const atString textureName = GetFriendlyTextureName(list[i].m_texture);
			const int      textureSize = list[i].m_texture->GetPhysicalSize();

			if (!_this->m_textureUsageAnalyserCSVOutput)
			{
				Displayf("  %d. %s is in %d TXDs (x %.2fKB each)", i, textureName.c_str(), list[i].m_count, (float)textureSize/1024.0f);
			}

			CTextureUsageRefs& data = refs[list[i]];

			data.SortTxdRefs();

			for (int j = 0; j < data.m_txdRefs.size(); j++)
			{
				if (!_this->m_textureUsageAnalyserCSVOutput)
				{
					Displayf("    %s", data.m_txdRefs[j].GetDesc().c_str());
				}
				else
				{
					char assetTypeStr[32] = "";
					strcpy(assetTypeStr, CAssetRef::GetAssetTypeName(data.m_txdRefs[j].GetAssetType(), true));

					for (char* s = &assetTypeStr[0]; *s; s++) // capitalise
					{
						*s = char( toupper(*s) );
					}

					Displayf("%s,%.2fKB,%s:%s", textureName.c_str(), (float)textureSize/1024.0f, assetTypeStr, data.m_txdRefs[j].GetAssetName());
				}
			}

			for (int j = 0; j < data.m_modelIndices.size(); j++)
			{
				const strLocalIndex modelIndex = strLocalIndex(data.m_modelIndices[j]);

				if (!_this->m_textureUsageAnalyserCSVOutput)
				{
					Displayf("    Model:%d(%s)", modelIndex.Get(), CModelInfo::GetBaseModelInfoName(fwModelId(modelIndex)));
				}
				else
				{
					Displayf("%s,%.2fKB,MODEL:%s", textureName.c_str(), (float)textureSize/1024.0f, CModelInfo::GetBaseModelInfoName(fwModelId(modelIndex)));
				}
			}

			for (int j = 0; j < data.m_modelIndices_NotUsedByShaders.size(); j++)
			{
				const strLocalIndex modelIndex = strLocalIndex(data.m_modelIndices_NotUsedByShaders[j]);

				if (!_this->m_textureUsageAnalyserCSVOutput)
				{
					Displayf("    Model:%d(%s) - NOT USED BY SHADERS", modelIndex.Get(), CModelInfo::GetBaseModelInfoName(fwModelId(modelIndex)));
				}
				else
				{
					// no CSV output
				}
			}
		}
	}
	else
	{
		for (int i = 0; i < list.size(); i++)
		{
			const atString textureName = GetFriendlyTextureName(list[i].m_texture);
			const int      textureSize = list[i].m_texture->GetPhysicalSize();

			if (!_this->m_textureUsageAnalyserCSVOutput)
			{
				Displayf("  %d. %s is in %d TXDs (x %.2fKB each)", i, textureName.c_str(), list[i].m_count, (float)textureSize/1024.0f);
			}
			else
			{
				Displayf("%s,%.2fKB,count=%d", textureName.c_str(), (float)textureSize/1024.0f, list[i].m_count);
			}
		}
	}
}

#endif // DEBUG_TEXTURE_VIEWER_TEXTUREUSAGEANALYSER

// ================================================================================================

static void _ToggleWireframe()
{
	if (!CRenderPhaseDebugOverlayInterface::DrawEnabledRef())
	{
		CRenderPhaseDebugOverlayInterface::DrawEnabledRef() = true;
		CRenderPhaseDebugOverlayInterface::SetColourMode(DOCM_TEXTURE_VIEWER);
	}
	else
	{
		CRenderPhaseDebugOverlayInterface::DrawEnabledRef() = false;
		CRenderPhaseDebugOverlayInterface::SetColourMode(DOCM_NONE);
	}
}

#if __DEV

static void _PrintModelTextures()
{
	const CDTVEntry* entry = gDebugTextureViewer.GetSelectedListEntry();

	if (entry)
	{
		const CEntity* entity = entry->GetEntity();

		if (entity)
		{
			Displayf("PrintTexturesUsedByEntity(%s)", entity->GetModelName());
			PrintTexturesUsedByEntity(entity);
		}
		else
		{
			const CBaseModelInfo* modelInfo = entry->GetModelInfo();

			if (modelInfo)
			{
				Displayf("PrintTexturesUsedByModel(%s)", modelInfo->GetModelName());
				PrintTexturesUsedByModel(modelInfo);
			}
		}
	}
}

#endif // __DEV

#if DEBUG_TEXTURE_VIEWER_MESH_DUMP_TEST

static void _PrintEntityMeshData()
{
	const CDTVEntry* entry = gDebugTextureViewer.GetSelectedListEntry();

	if (entry)
	{
		const CEntity* entity = entry->GetEntity();

		if (entity)
		{
			Displayf("PrintEntityMeshData(%s)", entity->GetModelName());
			PrintEntityMeshData(entity);
		}
		else
		{
			const CBaseModelInfo* modelInfo = entry->GetModelInfo();

			if (modelInfo)
			{
				Displayf("PrintModelMeshData(%s)", modelInfo->GetModelName());
				PrintModelMeshData(modelInfo);
			}
		}
	}
}

#endif // DEBUG_TEXTURE_VIEWER_MESH_DUMP_TEST

#if DEBUG_TEXTURE_VIEWER_INVERT_TEXTURE_CHANNELS

template <bool bMipOnly> static void InvertTextureChannels_button()
{
	const CDTVEntry* entry = gDebugTextureViewer.GetSelectedListEntry();

	if (entry)
	{
		const grcTexture* texture = entry->GetCurrentTexture();

		if (texture)
		{
			int mipIndex = bMipOnly ? gDebugTextureViewer.m_invertTextureMipCount : 0;
			int mipCount = bMipOnly ? 1 : gDebugTextureViewer.m_invertTextureMipCount;

			if (mipCount == 0)
			{
				mipIndex = gDebugTextureViewer.m_previewTextureLOD;
				mipCount = 1;
			}

			InvertTextureChannels(
				texture,
				mipIndex,
				mipCount,
				gDebugTextureViewer.m_invertTextureChannelR,
				gDebugTextureViewer.m_invertTextureChannelG,
				gDebugTextureViewer.m_invertTextureChannelB,
				gDebugTextureViewer.m_invertTextureChannelA,
				gDebugTextureViewer.m_invertTextureReverseBits,
				gDebugTextureViewer.m_invertTextureVerbose
			);
		}
	}
}

template <bool bMipOnly> static void SetSolidTextureColour_button()
{
	const CDTVEntry* entry = gDebugTextureViewer.GetSelectedListEntry();

	if (entry)
	{
		const grcTexture* texture = entry->GetCurrentTexture();

		if (texture)
		{
			int mipIndex = bMipOnly ? gDebugTextureViewer.m_invertTextureMipCount : 0;
			int mipCount = bMipOnly ? 1 : gDebugTextureViewer.m_invertTextureMipCount;

			if (mipCount == 0)
			{
				mipIndex = gDebugTextureViewer.m_previewTextureLOD;
				mipCount = 1;
			}

			SetSolidTextureColour(
				texture,
				mipIndex,
				mipCount,
				gDebugTextureViewer.m_solidTextureColour,
				gDebugTextureViewer.m_invertTextureVerbose
			);
		}
	}
}

static void ReverseBitsForAllErrorTextures_button()
{
	for (int i = 0; i < gDebugTextureViewer.m_listEntries.GetCount(); i++)
	{
		CDTVEntry_Texture* textureEntry = dynamic_cast<CDTVEntry_Texture*>(gDebugTextureViewer.m_listEntries[i]);

		if (textureEntry && textureEntry->GetErrorFlag())
		{
			const grcTexture* texture = textureEntry->GetCurrentTexture_Public();

			if (texture)
			{
				InvertTextureChannels(texture, 0, 9999, false, false, false, false, true, false);
			}
		}
	}
}

// this is a "fast" version which only evaluates a histogram on the DXT block endpoint colours, not the interpolated colours
static void DumpFastDXTHistogram_button()
{
	const CDTVEntry* entry = gDebugTextureViewer.GetSelectedListEntry();

	if (entry)
	{
		const grcTexture* texture = entry->GetCurrentTexture();

		if (texture)
		{
			u16 rgbmin = 0xffff;
			u16 rgbmax = 0x0000;
			int rhist32[32];
			int ghist64[64];
			int bhist32[32];
			sysMemSet(rhist32, 0, sizeof(rhist32));
			sysMemSet(ghist64, 0, sizeof(ghist64));
			sysMemSet(bhist32, 0, sizeof(bhist32));
			AssetAnalysis::GetTextureRGBMinMaxHistogramRaw(texture, rgbmin, rgbmax, rhist32, ghist64, bhist32);

			char rhistStr[512] = "";
			char ghistStr[512] = "";
			char bhistStr[512] = "";

			for (int i = 0; i < 32; i++)
			{
				if (i > 0)
				{
					strcat(rhistStr, ",");
					strcat(bhistStr, ",");

					if (i == 32/2)
					{
						strcat(rhistStr, " ");
						strcat(bhistStr, " ");
					}
				}

				strcat(rhistStr, atVarString("%d", rhist32[i]).c_str());
				strcat(bhistStr, atVarString("%d", bhist32[i]).c_str());
			}

			for (int i = 0; i < 64; i++)
			{
				if (i > 0)
				{
					strcat(ghistStr, ",");

					if (i == 64/2)
					{
						strcat(ghistStr, " ");
					}
				}

				strcat(ghistStr, atVarString("%d", ghist64[i]).c_str());
			}

			Displayf("rhist = %s", rhistStr);
			Displayf("ghist = %s", ghistStr);
			Displayf("bhist = %s", bhistStr);

			const Vec4V avg = AssetAnalysis::GetTextureAverageColour(texture);

			Displayf("avg = %f,%f,%f,%f", VEC4V_ARGS(avg));
		}
	}
}

#endif // DEBUG_TEXTURE_VIEWER_INVERT_TEXTURE_CHANNELS

void TextureViewer_GetRenderBucketInfoStrings(const Drawable* pDrawable, u16 phaseVisMask, char* renderBucketMaskStr, int renderBucketMaskStrMaxLen, char* renderBucketWarning, int renderBucketWarningMaxLen)
{
	const grmShaderGroup* pShaderGroup = pDrawable->GetShaderGroupPtr();

	if (pShaderGroup)
	{
		const rmcLodGroup& lodGroup = pDrawable->GetLodGroup();

		const u32 maskDefault    = CRenderer::GenerateSubBucketMask(CRenderer::RB_MODEL_DEFAULT   );
		const u32 maskShadow     = CRenderer::GenerateSubBucketMask(CRenderer::RB_MODEL_SHADOW    );
		const u32 maskReflection = CRenderer::GenerateSubBucketMask(CRenderer::RB_MODEL_REFLECTION);
		const u32 maskMirror     = CRenderer::GenerateSubBucketMask(CRenderer::RB_MODEL_MIRROR    );
		const u32 maskWater      = CRenderer::GenerateSubBucketMask(CRenderer::RB_MODEL_WATER     );

		u32 maskAnyOn = 0x00000000;
		u32 maskAllOn = 0xffffffff;
		int modelCount = 0;
		int numValidLods = 0;
		int maxSubmeshesPerLod = 0;

		strncat(renderBucketMaskStr, "flags=", renderBucketMaskStrMaxLen);

		for (int lodIndex = 0; lodIndex < LOD_COUNT; lodIndex++)
		{
			if (lodGroup.ContainsLod(lodIndex))
			{
				const rmcLod& lod = lodGroup.GetLod(lodIndex);

				for (int i = 0; i < lod.GetCount(); i++)
				{
					const u32 mask = CRenderer::GetSubBucketMask(lod.GetModel(i)->ComputeBucketMask(*pShaderGroup));

					maskAnyOn |= mask;
					maskAllOn &= mask;
					modelCount++;
				}

				numValidLods++;
				maxSubmeshesPerLod = Max<int>(lod.GetCount(), maxSubmeshesPerLod);
			}
		}

		if (modelCount > 0)
		{
			const u32 maskAllOff = ~maskAnyOn;

			strncat(renderBucketMaskStr, (maskAllOff & maskDefault   ) ? "-" : (maskAllOn & maskDefault   ) ? "G" : "g", renderBucketMaskStrMaxLen);
			strncat(renderBucketMaskStr, (maskAllOff & maskShadow    ) ? "-" : (maskAllOn & maskShadow    ) ? "S" : "s", renderBucketMaskStrMaxLen);
			strncat(renderBucketMaskStr, (maskAllOff & maskReflection) ? "-" : (maskAllOn & maskReflection) ? "R" : "r", renderBucketMaskStrMaxLen);
			strncat(renderBucketMaskStr, (maskAllOff & maskMirror    ) ? "-" : (maskAllOn & maskMirror    ) ? "M" : "m", renderBucketMaskStrMaxLen);
			strncat(renderBucketMaskStr, (maskAllOff & maskWater     ) ? "-" : (maskAllOn & maskWater     ) ? "W" : "w", renderBucketMaskStrMaxLen);

			if (modelCount > 1) // let's write out a full set of flags for each submesh, if they are different
			{
				char submeshMaskStr[2048] = "";
				u32  firstSubmeshMask = 0;
				bool firstSubmesh = true;
				bool allSubmeshesHaveTheSameMask = true;

				for (int lodIndex = 0; lodIndex < LOD_COUNT; lodIndex++)
				{
					if (lodGroup.ContainsLod(lodIndex))
					{
						const rmcLod& lod = lodGroup.GetLod(lodIndex);

						for (int i = 0; i < lod.GetCount(); i++)
						{
							const u32 mask = CRenderer::GetSubBucketMask(lod.GetModel(i)->ComputeBucketMask(*pShaderGroup));

							if (firstSubmesh)
							{
								firstSubmesh = false;
								firstSubmeshMask = mask;
							}
							else if (mask != firstSubmeshMask)
							{
								allSubmeshesHaveTheSameMask = false;
							}

							strncat(submeshMaskStr, "[", renderBucketMaskStrMaxLen);

							if (numValidLods > 1)
							{
								strncat(submeshMaskStr, atVarString("lod=%d", lodIndex).c_str(), renderBucketMaskStrMaxLen);
							}

							if (maxSubmeshesPerLod > 1)
							{
								strncat(submeshMaskStr, atVarString("%ssubmesh=%d", numValidLods > 1 ? "/" : "", i).c_str(), renderBucketMaskStrMaxLen);
							}

							strncat(submeshMaskStr, ":", renderBucketMaskStrMaxLen);
							strncat(submeshMaskStr, (mask & maskDefault   ) ? "g" : "-", renderBucketMaskStrMaxLen);
							strncat(submeshMaskStr, (mask & maskShadow    ) ? "s" : "-", renderBucketMaskStrMaxLen);
							strncat(submeshMaskStr, (mask & maskReflection) ? "r" : "-", renderBucketMaskStrMaxLen);
							strncat(submeshMaskStr, (mask & maskMirror    ) ? "m" : "-", renderBucketMaskStrMaxLen);
							strncat(submeshMaskStr, (mask & maskWater     ) ? "w" : "-", renderBucketMaskStrMaxLen);
							strncat(submeshMaskStr, "]", renderBucketMaskStrMaxLen);
						}
					}
				}

				if (allSubmeshesHaveTheSameMask)
				{
					strncat(renderBucketMaskStr, " [", renderBucketMaskStrMaxLen);

					if (numValidLods > 1)
					{
						strncat(renderBucketMaskStr, atVarString("numlods=%d", numValidLods).c_str(), renderBucketMaskStrMaxLen);
					}

					if (maxSubmeshesPerLod > 1)
					{
						strncat(renderBucketMaskStr, atVarString("%smaxsubmeshesperlod=%d", numValidLods > 1 ? "/" : "", maxSubmeshesPerLod).c_str(), renderBucketMaskStrMaxLen);
					}

					strncat(renderBucketMaskStr, "]", renderBucketMaskStrMaxLen);
				}
				else
				{
					strncat(renderBucketMaskStr, " ", renderBucketMaskStrMaxLen);
					strncat(renderBucketMaskStr, submeshMaskStr, renderBucketMaskStrMaxLen);
				}
			}

			if (maskAnyOn)
			{
				if ((maskAllOff & maskDefault) && (phaseVisMask & VIS_PHASE_MASK_GBUF))
				{
					strncat(renderBucketWarning, atVarString("%sdefault", renderBucketWarning[0] ? "" : "/").c_str(), renderBucketWarningMaxLen);
				}

				if ((maskAllOff & maskShadow) && (phaseVisMask & VIS_PHASE_MASK_SHADOWS))
				{
					strncat(renderBucketWarning, atVarString("%sshadow", renderBucketWarning[0] ? "" : "/").c_str(), renderBucketWarningMaxLen);
				}

				if ((maskAllOff & maskReflection) && (phaseVisMask & VIS_PHASE_MASK_PARABOLOID_REFLECTION))
				{
					strncat(renderBucketWarning, atVarString("%sreflection", renderBucketWarning[0] ? "" : "/").c_str(), renderBucketWarningMaxLen);
				}

				if ((maskAllOff & maskMirror) && (phaseVisMask & VIS_PHASE_MASK_MIRROR_REFLECTION))
				{
					strncat(renderBucketWarning, atVarString("%smirror", renderBucketWarning[0] ? "" : "/").c_str(), renderBucketWarningMaxLen);
				}

				if ((maskAllOff & maskWater) && (phaseVisMask & VIS_PHASE_MASK_WATER_REFLECTION))
				{
					strncat(renderBucketWarning, atVarString("%swater", renderBucketWarning[0] ? "" : "/").c_str(), renderBucketWarningMaxLen);
				}
			}
		}
		else
		{
			strcpy(renderBucketWarning, "no models");
		}
	}
	else
	{
		strcpy(renderBucketWarning, "NULL shadergroup");
	}

	if (renderBucketMaskStr[0] == '\0') { strcpy(renderBucketMaskStr, "-"); }
	if (renderBucketWarning[0] == '\0') { strcpy(renderBucketWarning, "-"); }
}

static float g_DumpEntityPhaseVisMasksSearchRadius = 16000.0f;
static void DumpEntityPhaseVisMasks_button()
{
	static fiStream* csv = NULL;

	csv = fiStream::Create("assets:/non_final/pvm.csv");

	if (csv)
	{
		class p { public: static bool func(void* item, void*)
		{
			if (item)
			{
				const CEntity* pEntity = static_cast<CEntity*>(item);
				const CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();

				if (pEntity->GetModelName() == NULL ||
					pEntity->GetModelName()[0] == '\0' ||
					pModelInfo == NULL)
				{
					return true; // skip models with no name or modelinfo .. not sure what these are
				}

				if (Mag(pEntity->GetMatrix().GetCol3() - gVpMan.GetUpdateGameGrcViewport()->GetCameraPosition()).Getf() > g_DumpEntityPhaseVisMasksSearchRadius)
				{
					return true;
				}

				const u16 phaseVisMask = pEntity->GetRenderPhaseVisibilityMask();
				char renderBucketMaskStr[2048] = "";
				char renderBucketWarning[256] = "";

				const Drawable* pDrawable = pEntity->GetDrawable();

				if (pDrawable)
				{
					TextureViewer_GetRenderBucketInfoStrings(pDrawable, phaseVisMask, renderBucketMaskStr, (int)sizeof(renderBucketMaskStr), renderBucketWarning, (int)sizeof(renderBucketWarning));
				}

				const CEntity* pEntityLodParent = (const CEntity*)const_cast<CEntity*>(pEntity)->GetLod();

				fprintf(csv, "%s,%s,%f,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,[%f %f %f]\n",
					pEntity->GetModelName(),
					pEntity->GetLodData().GetLodTypeName(),
					pEntity->GetLodDistance(),
					pEntityLodParent ? pEntityLodParent->GetModelName() : "-",
					pEntityLodParent ? atVarString("%f", (float)pEntityLodParent->GetLodDistance()).c_str() : "-",
					renderBucketMaskStr,
					renderBucketWarning,
					(phaseVisMask & VIS_PHASE_MASK_OTHER                 ) ? "other" : "-",
					(phaseVisMask & VIS_PHASE_MASK_GBUF                  ) ? "gbuf" : "-",
					(phaseVisMask & VIS_PHASE_MASK_CASCADE_SHADOWS       ) ? "shadow(cascade)" : "-",
					(phaseVisMask & VIS_PHASE_MASK_PARABOLOID_SHADOWS    ) ? "shadow(parab)" : "-",
					(phaseVisMask & VIS_PHASE_MASK_PARABOLOID_REFLECTION ) ? "reflection" : "-",
					(phaseVisMask & VIS_PHASE_MASK_WATER_REFLECTION      ) ? "water" : "-",
					(phaseVisMask & VIS_PHASE_MASK_MIRROR_REFLECTION     ) ? "mirror" : "-",
					(phaseVisMask & VIS_PHASE_MASK_RAIN_COLLISION_MAP    ) ? "rain" : "-",
					(phaseVisMask & VIS_PHASE_MASK_SEETHROUGH_MAP        ) ? "seethrough" : "-",
					(phaseVisMask & VIS_PHASE_MASK_STREAMING             ) ? "streaming" : "-",
					(phaseVisMask & VIS_PHASE_MASK_SHADOW_PROXY          ) ? "{shadowproxy}" : "-",
					(phaseVisMask & VIS_PHASE_MASK_WATER_REFLECTION_PROXY) ? "{watrefproxy}" : "-",
					VEC3V_ARGS(pEntity->GetTransform().GetPosition())
				);
			}

			return true;
		}};

		CBuilding        ::GetPool()->ForAll(p::func, NULL);
		CAnimatedBuilding::GetPool()->ForAll(p::func, NULL);
		CDummyObject     ::GetPool()->ForAll(p::func, NULL);
		CObject          ::GetPool()->ForAll(p::func, NULL);
	//	CPed             ::GetPool()->ForAll(p::func, NULL);
	//	CVehicle         ::GetPool()->ForAll(p::func, NULL);
	//	CEntityBatch     ::GetPool()->ForAll(p::func, NULL);
	//	CGrassBatch      ::GetPool()->ForAll(p::func, NULL);

		csv->Close();
		csv = NULL;
	}
}

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEST

// ugly extern templates .. should move these to common header
extern atString CDTVStreamingIteratorTest_GetDwdEntryName(int slot, int index);

template <typename T> extern atString CDTVStreamingIteratorTest_GetAssetPath(T& store, int slot, int index);

template <> atString CDTVStreamingIteratorTest_GetAssetPath<fwTxdStore     >(fwTxdStore     & store, int slot, int index);
template <> atString CDTVStreamingIteratorTest_GetAssetPath<fwDwdStore     >(fwDwdStore     & store, int slot, int index);
template <> atString CDTVStreamingIteratorTest_GetAssetPath<fwDrawableStore>(fwDrawableStore& store, int slot, int index);
template <> atString CDTVStreamingIteratorTest_GetAssetPath<fwFragmentStore>(fwFragmentStore& store, int slot, int index);
template <> atString CDTVStreamingIteratorTest_GetAssetPath<ptfxAssetStore >(ptfxAssetStore & store, int slot, int index);

template <typename T> extern atString CDTVStreamingIteratorTest_FindTexturePath(T& store, int slot, int index, const Drawable* drawable, const grcTexture* texture, u32 textureKey, bool* bUsesNonPackedTextures);

extern atString CDTVStreamingIteratorTest_FindTexturePath(const CBaseModelInfo* model, const grcTexture* texture);

template <typename T> static int FindShadersUsingThisTexture_Report(T& store, int slot, int index, const Drawable* drawable, const grcTexture* texture, bool bCompareNameOnly)
{
	int count = 0;

	if (drawable && texture)
	{
		const atString desc0 = CDTVStreamingIteratorTest_GetAssetPath(store, slot, index);
		const grmShaderGroup* shaderGroup = &drawable->GetShaderGroup();

		for (int i = 0; i < shaderGroup->GetCount(); i++)
		{
			const grmShader* shader = shaderGroup->GetShaderPtr(i);

			for (int j = 0; j < shader->GetVarCount(); j++)
			{
				const grcEffectVar var = shader->GetVarByIndex(j);
				const char* name = NULL;
				grcEffect::VarType type;
				int annotationCount = 0;
				bool isGlobal = false;

				shader->GetVarDesc(var, name, type, annotationCount, isGlobal);

				if (!isGlobal && type == grcEffect::VT_TEXTURE)
				{
					grcTexture* t = NULL;
					shader->GetVar(var, t);

					if (t == texture || (bCompareNameOnly && t && stricmp(t->GetName(), texture->GetName()) == 0))
					{
						const atVarString desc("%s%s(%d)", desc0.c_str(), shader->GetName(), i); // append shadergroup index to drawable path
						atString archetypeStr = atString("none");

						for (u32 i = 0; i < CDebugArchetype::GetMaxDebugArchetypeProxies(); i++)
						{
							const CDebugArchetypeProxy* modelInfo = CDebugArchetype::GetDebugArchetypeProxy(i);

							if (modelInfo)
							{
								if (modelInfo->GetDrawable() == drawable)
								{
									archetypeStr = modelInfo->GetModelName();
									break;
								}
								else
								{
									const int ptxSlot = modelInfo->GetPtFxAssetSlot();
									
									if (g_ParticleStore.IsValidSlot(ptxSlot))
									{
										ptxFxList* ptx = g_ParticleStore.Get(ptxSlot);

										if (ptx)
										{
											const Dwd* ptxDwd = ptx->GetModelDictionary();
											int ptxDwdIndex = INDEX_NONE;

											if (ptxDwd)
											{
												for (int jj = 0; jj < ptxDwd->GetCount(); jj++)
												{
													if (ptxDwd->GetEntry(jj) == drawable)
													{
														ptxDwdIndex = jj;
														break;
													}
												}
											}

											if (ptxDwdIndex != INDEX_NONE)
											{
												archetypeStr = modelInfo->GetModelName();
												archetypeStr += atVarString("/PTX[%d]", ptxDwdIndex);
												break;
											}
										}
									}
								}
							}
						}

						Displayf("  used as %s by %s (%s)%s", name, desc.c_str(), archetypeStr.c_str(), (t == texture) ? "" : " - NAME ONLY");
						count++;
					}
				}
			}
		}
	}

	return count;
}

static int FindShadersUsingThisTexture_internal(const grcTexture* texture, bool bCompareNameOnly)
{
	int count = 0;

	if (texture)
	{
		Displayf("finding shaders using %s [%p]..", GetFriendlyTextureName(texture).c_str(), texture);

		for (int i = 0; i < g_DwdStore.GetSize(); i++)
		{
			if (g_DwdStore.IsValidSlot(i))
			{
				const Dwd* dwd = g_DwdStore.Get(i);

				if (dwd)
				{
					for (int j = 0; j < dwd->GetCount(); j++)
					{
						count += FindShadersUsingThisTexture_Report(g_DwdStore, i, j, dwd->GetEntry(j), texture, bCompareNameOnly);
					}
				}
			}
		}

		for (int i = 0; i < g_DrawableStore.GetSize(); i++)
		{
			if (g_DrawableStore.IsValidSlot(i))
			{
				count += FindShadersUsingThisTexture_Report(g_DrawableStore, i, INDEX_NONE, g_DrawableStore.Get(i), texture, bCompareNameOnly);
			}
		}

		for (int i = 0; i < g_FragmentStore.GetSize(); i++)
		{
			if (g_FragmentStore.IsValidSlot(i))
			{
				const Fragment* fragment = g_FragmentStore.Get(i);

				if (fragment)
				{
					count += FindShadersUsingThisTexture_Report(g_FragmentStore, i, INDEX_NONE, fragment->GetCommonDrawable(), texture, bCompareNameOnly);
					count += FindShadersUsingThisTexture_Report(g_FragmentStore, i, INDEX_NONE, fragment->GetClothDrawable(), texture, bCompareNameOnly);

					for (int j = 0; j < fragment->GetNumExtraDrawables(); j++)
					{
						count += FindShadersUsingThisTexture_Report(g_FragmentStore, i, j, fragment->GetExtraDrawable(j), texture, bCompareNameOnly);
					}
				}
			}
		}

		for (int i = 0; i < g_ParticleStore.GetSize(); i++)
		{
			if (g_ParticleStore.IsValidSlot(i))
			{
				ptxFxList* ptx = g_ParticleStore.Get(i);

				if (ptx)
				{
					const Dwd* ptxDwd = ptx->GetModelDictionary();

					if (ptxDwd)
					{
						for (int j = 0; j < ptxDwd->GetCount(); j++)
						{
							count += FindShadersUsingThisTexture_Report(g_ParticleStore, i, j, ptxDwd->GetEntry(j), texture, bCompareNameOnly);
						}
					}
				}
			}
		}
	}

	return count;
}

template <bool bCompareNameOnly> static void FindShadersUsingThisTexture_button()
{
	const grcTexture* texture = NULL;
	const CDTVEntry_Texture* entry = dynamic_cast<const CDTVEntry_Texture*>(gDebugTextureViewer.GetSelectedListEntry());

	if (entry)
	{
		texture = entry->GetCurrentTexture_Public();
	}
#if DEBUG_TEXTURE_VIEWER_SHADER_EDIT_INTEGRATION
	else
	{
		const CDTVEntry_ShaderEditTexture* entry2 = dynamic_cast<const CDTVEntry_ShaderEditTexture*>(gDebugTextureViewer.GetSelectedListEntry());

		if (entry2)
		{
			texture = entry2->GetTexture_Public();
		}
	}
#endif // DEBUG_TEXTURE_VIEWER_SHADER_EDIT_INTEGRATION

	if (FindShadersUsingThisTexture_internal(texture, bCompareNameOnly) == 0)
	{
		Displayf("  none found");
	}
}

template <bool bCompareNameOnly> static void FindShadersUsingTextures_button()
{
	const int numEntries = gDebugTextureViewer.GetListNumEntries();

	for (int i = 0; i < numEntries; i++)
	{
		const grcTexture* texture = NULL;
		const CDTVEntry_Texture* entry = dynamic_cast<const CDTVEntry_Texture*>(gDebugTextureViewer.GetListEntry(i));

		if (entry)
		{
			texture = entry->GetCurrentTexture_Public();
		}
#if DEBUG_TEXTURE_VIEWER_SHADER_EDIT_INTEGRATION
		else
		{
			const CDTVEntry_ShaderEditTexture* entry2 = dynamic_cast<const CDTVEntry_ShaderEditTexture*>(gDebugTextureViewer.GetListEntry(i));

			if (entry2)
			{
				texture = entry2->GetTexture_Public();
			}
		}
#endif // DEBUG_TEXTURE_VIEWER_SHADER_EDIT_INTEGRATION

		if (FindShadersUsingThisTexture_internal(texture, bCompareNameOnly) == 0)
		{
			Displayf("  none found");
		}
	}
}

static void FindTexturesUsedByThisDrawable_button()
{
	const CDTVEntry_Txd* entry = dynamic_cast<const CDTVEntry_Txd*>(gDebugTextureViewer.GetSelectedListEntry());

	if (entry)
	{
		atArray<const Drawable*> drawables;

		if (entry->GetTxdRef().GetAssetType() == AST_DwdStore)
		{
			const Dwd* dwd = g_DwdStore.Get(entry->GetTxdRef().GetAssetIndex());

			if (dwd && entry->GetTxdRef().m_assetEntry >= 0 && entry->GetTxdRef().m_assetEntry < dwd->GetCount())
			{
				const Drawable* drawable = dwd->GetEntry(entry->GetTxdRef().m_assetEntry);

				if (drawable)
				{
					drawables.PushAndGrow(drawable);
				}
			}
		}
		else if (entry->GetTxdRef().GetAssetType() == AST_DrawableStore)
		{
			drawables.PushAndGrow(g_DrawableStore.Get(entry->GetTxdRef().GetAssetIndex()));
		}
		else if (entry->GetTxdRef().GetAssetType() == AST_FragmentStore)
		{
			const Fragment* fragment = g_FragmentStore.Get(entry->GetTxdRef().GetAssetIndex());
			const Drawable* clothDrawable = fragment->GetClothDrawable();

			drawables.PushAndGrow(fragment->GetCommonDrawable());

			if (clothDrawable)
			{
				Displayf("%s has cloth drawable", CDTVStreamingIteratorTest_GetAssetPath(g_FragmentStore, entry->GetTxdRef().GetAssetIndex(), INDEX_NONE).c_str());
				drawables.PushAndGrow(clothDrawable);
			}
		}

		for (int k = 0; k < drawables.GetCount(); k++)
		{
			const Drawable* drawable = drawables[k];
			const grmShaderGroup* shaderGroup = drawable->GetShaderGroupPtr();

			for (int i = 0; i < shaderGroup->GetCount(); i++)
			{
				const grmShader* shader = shaderGroup->GetShaderPtr(i);
				bool bFoundTextures = false;

				for (int j = 0; j < shader->GetVarCount(); j++)
				{
					const grcEffectVar var = shader->GetVarByIndex(j);
					const char* name = NULL;
					grcEffect::VarType type;
					int annotationCount = 0;
					bool isGlobal = false;

					shader->GetVarDesc(var, name, type, annotationCount, isGlobal);

					//if (!isGlobal)
					{
						if (type == grcEffect::VT_TEXTURE)
						{
							grcTexture* texture = NULL;
							shader->GetVar(var, texture);

							if (texture)
							{
								atString texturePath = atString("");

								if (entry->GetTxdRef().GetAssetType() == AST_DwdStore)
								{
									texturePath = CDTVStreamingIteratorTest_FindTexturePath(g_DwdStore, entry->GetTxdRef().GetAssetIndex(), entry->GetTxdRef().m_assetEntry, drawable, texture, 0, NULL);
								}
								else if (entry->GetTxdRef().GetAssetType() == AST_DrawableStore)
								{
									texturePath = CDTVStreamingIteratorTest_FindTexturePath(g_DrawableStore, entry->GetTxdRef().GetAssetIndex(), INDEX_NONE, drawable, texture, 0, NULL);
								}
								else if (entry->GetTxdRef().GetAssetType() == AST_FragmentStore)
								{
									texturePath = CDTVStreamingIteratorTest_FindTexturePath(g_FragmentStore, entry->GetTxdRef().GetAssetIndex(), INDEX_NONE, drawable, texture, 0, NULL);
								}

								Displayf("  %d. shader '%s': %s = \"%s\"", i, shader->GetName(), name, texturePath.c_str());
								bFoundTextures = true;
							}
							else
							{
								Displayf("  %d. shader '%s': %s = NULL", i, shader->GetName(), name);
								bFoundTextures = true;
							}
						}
					}
				}

				if (!bFoundTextures)
				{
					Displayf("  %d. shader '%s': no textures", i, shader->GetName());
				}
			}
		}
	}
}

static void FindDependentAssets_internal(int txdSlot, int depth, bool bAddToList, bool bShowArchetypes, bool bImmediateOnly)
{
	DEBUG_TEXTURE_VIEWER_USE_DEBUG_MEMORY();

	if (g_TxdStore.IsValidSlot(txdSlot))
	{
		atString tabStr("");

		for (int j = 0; j < depth; j++)
		{
			tabStr += "  ";
		}

		for (int i = 0; i < g_TxdStore.GetSize(); i++)
		{
			if (g_TxdStore.IsValidSlot(i) && g_TxdStore.GetParentTxdSlot(i) == txdSlot)// && !g_TxdStore.GetSlot(i)->m_isDummy)
			{
				if (bAddToList)
				{
					gDebugTextureViewer.m_listEntries.PushAndGrow(rage_new CDTVEntry_Txd(CTxdRef(AST_TxdStore, i, INDEX_NONE, ""), gDebugTextureViewer.m_asp, false, CAssetRefInterface(gDebugTextureViewer.m_assetRefs, false)));
				}
				else
				{
					Displayf("%s<- %s%s", tabStr.c_str(), CDTVStreamingIteratorTest_GetAssetPath(g_TxdStore, i, INDEX_NONE).c_str(), g_TxdStore.GetSlot(i)->m_isDummy ? " (DUMMY)" : "");

					if (bShowArchetypes)
					{
						for (u32 j = 0; j < CDebugArchetype::GetMaxDebugArchetypeProxies(); j++)
						{
							const CDebugArchetypeProxy* modelInfo = CDebugArchetype::GetDebugArchetypeProxy(j);

							if (modelInfo)
							{
								if (modelInfo->GetModelType() == MI_TYPE_PED)
								{
									const CDebugPedArchetypeProxy* pedModelInfo = (const CDebugPedArchetypeProxy*)modelInfo;

									if (pedModelInfo->GetHDTxdIndex() == i)
									{
										Displayf("  %s<- archetype(%d) \"%s\" PED_HD_TXD", tabStr.c_str(), j, modelInfo->GetModelName());
									}
								}
								else if (modelInfo->GetModelType() == MI_TYPE_VEHICLE)
								{
									const CDebugVehicleArchetypeProxy* vehModelInfo = (const CDebugVehicleArchetypeProxy*)modelInfo;

									if (vehModelInfo->GetHDTxdIndex() == i)
									{
										Displayf("  %s<- archetype(%d) \"%s\" VEH_HD_TXD", tabStr.c_str(), j, modelInfo->GetModelName());
									}
								}
							}
						}
					}
				}

				if (!bImmediateOnly)
				{
					FindDependentAssets_internal(i, depth + 1, bAddToList, bShowArchetypes, bImmediateOnly);
				}
			}
		}

		for (int i = 0; i < g_DwdStore.GetSize(); i++)
		{
			if (g_DwdStore.IsValidSlot(i) && g_DwdStore.GetParentTxdForSlot(strLocalIndex(i)) == txdSlot)
			{
				if (bAddToList)
				{
					const Dwd* dwd = g_DwdStore.Get(i);

					if (dwd)
					{
						for (int j = 0; j < dwd->GetCount(); j++)
						{
							gDebugTextureViewer.m_listEntries.PushAndGrow(rage_new CDTVEntry_Txd(CTxdRef(AST_DwdStore, i, j, ""), gDebugTextureViewer.m_asp, false, CAssetRefInterface(gDebugTextureViewer.m_assetRefs, false)));
						}
					}
				}
				else
				{
					Displayf("%s<- %s", tabStr.c_str(), CDTVStreamingIteratorTest_GetAssetPath(g_DwdStore, i, INDEX_NONE).c_str());

					if (bShowArchetypes)
					{
						const Dwd* dwd = g_DwdStore.Get(i);

						if (dwd)
						{
							for (u32 j = 0; j < CDebugArchetype::GetMaxDebugArchetypeProxies(); j++)
							{
								const CDebugArchetypeProxy* modelInfo = CDebugArchetype::GetDebugArchetypeProxy(j);

								if (modelInfo && modelInfo->SafeGetDrawDictIndex() == i)
								{
									const int entryIndex = dwd->LookupLocalIndex(modelInfo->GetHashKey());

									Displayf("  %s<- archetype(%d) \"%s\" (index=%d)", tabStr.c_str(), j, modelInfo->GetModelName(), entryIndex);
								}
							}
						}
					}
				}
			}
		}

		for (int i = 0; i < g_DrawableStore.GetSize(); i++)
		{
			if (g_DrawableStore.IsValidSlot(i) && g_DrawableStore.GetParentTxdForSlot(i) == txdSlot)
			{
				if (bAddToList)
				{
					gDebugTextureViewer.m_listEntries.PushAndGrow(rage_new CDTVEntry_Txd(CTxdRef(AST_DrawableStore, i, INDEX_NONE, ""), gDebugTextureViewer.m_asp, false, CAssetRefInterface(gDebugTextureViewer.m_assetRefs, false)));
				}
				else
				{
					Displayf("%s<- %s", tabStr.c_str(), CDTVStreamingIteratorTest_GetAssetPath(g_DrawableStore, i, INDEX_NONE).c_str());

					if (bShowArchetypes)
					{
						for (u32 j = 0; j < CDebugArchetype::GetMaxDebugArchetypeProxies(); j++)
						{
							const CDebugArchetypeProxy* modelInfo = CDebugArchetype::GetDebugArchetypeProxy(j);

							if (modelInfo && modelInfo->SafeGetDrawableIndex() == i)
							{
								Displayf("  %s<- archetype(%d) \"%s\"", tabStr.c_str(), j, modelInfo->GetModelName());
							}
						}
					}
				}
			}
		}

		for (int i = 0; i < g_FragmentStore.GetSize(); i++)
		{
			if (g_FragmentStore.IsValidSlot(i) && g_FragmentStore.GetParentTxdForSlot(i) == txdSlot)
			{
				if (bAddToList)
				{
					gDebugTextureViewer.m_listEntries.PushAndGrow(rage_new CDTVEntry_Txd(CTxdRef(AST_FragmentStore, i, INDEX_NONE, ""), gDebugTextureViewer.m_asp, false, CAssetRefInterface(gDebugTextureViewer.m_assetRefs, false)));
				}
				else
				{
					Displayf("%s<- %s", tabStr.c_str(), CDTVStreamingIteratorTest_GetAssetPath(g_FragmentStore, i, INDEX_NONE).c_str());

					if (bShowArchetypes)
					{
						for (u32 j = 0; j < CDebugArchetype::GetMaxDebugArchetypeProxies(); j++)
						{
							const CDebugArchetypeProxy* modelInfo = CDebugArchetype::GetDebugArchetypeProxy(j);

							if (modelInfo && modelInfo->SafeGetFragmentIndex() == i)
							{
								Displayf("  %s<- archetype(%d) \"%s\"", tabStr.c_str(), j, modelInfo->GetModelName());
							}
						}
					}
				}
			}
		}
	}
}

template <bool bAddToList, bool bShowArchetypes, bool bImmediateOnly> static void FindDependentAssets_button()
{
	const CDTVEntry_Txd* entry = dynamic_cast<const CDTVEntry_Txd*>(gDebugTextureViewer.GetSelectedListEntry());

	if (entry)
	{
		const CTxdRef& ref = entry->GetTxdRef();

		if (ref.GetAssetType() == AST_TxdStore)
		{
			const int i = ref.GetAssetIndex();

			if (g_TxdStore.IsValidSlot(i))
			{
				if (!bAddToList)
				{
					Displayf("%s", CDTVStreamingIteratorTest_GetAssetPath(g_TxdStore, i, INDEX_NONE).c_str());

					if (bShowArchetypes)
					{
						for (u32 j = 0; j < CDebugArchetype::GetMaxDebugArchetypeProxies(); j++)
						{
							const CDebugArchetypeProxy* modelInfo = CDebugArchetype::GetDebugArchetypeProxy(j);

							if (modelInfo)
							{
								if (modelInfo->GetModelType() == MI_TYPE_PED)
								{
									const CDebugPedArchetypeProxy* pedModelInfo = (const CDebugPedArchetypeProxy*)modelInfo;

									if (pedModelInfo->GetHDTxdIndex() == i)
									{
										Displayf("  <- archetype(%d) \"%s\" PED_HD_TXD", j, modelInfo->GetModelName());
									}
								}
								else if (modelInfo->GetModelType() == MI_TYPE_VEHICLE)
								{
									const CDebugVehicleArchetypeProxy* vehModelInfo = (const CDebugVehicleArchetypeProxy*)modelInfo;

									if (vehModelInfo->GetHDTxdIndex() == i)
									{
										Displayf("  <- archetype(%d) \"%s\" VEH_HD_TXD", j, modelInfo->GetModelName());
									}
								}
							}
						}
					}
				}

				FindDependentAssets_internal(i, 1, bAddToList, bShowArchetypes, bImmediateOnly);
			}
		}
		else if (!bAddToList)
		{
			if (ref.GetAssetType() == AST_DwdStore)
			{
				const int i = ref.GetAssetIndex();

				if (g_DwdStore.IsValidSlot(i))
				{
					Displayf("%s", CDTVStreamingIteratorTest_GetAssetPath(g_DwdStore, i, INDEX_NONE).c_str());

					if (bShowArchetypes)
					{
						const Dwd* dwd = g_DwdStore.Get(i);

						if (dwd)
						{
							for (u32 j = 0; j < CDebugArchetype::GetMaxDebugArchetypeProxies(); j++)
							{
								const CDebugArchetypeProxy* modelInfo = CDebugArchetype::GetDebugArchetypeProxy(j);

								if (modelInfo)
								{
									if (modelInfo->SafeGetDrawDictIndex() == i)
									{
										const int entryIndex = dwd->LookupLocalIndex(modelInfo->GetHashKey());

										Displayf("  <- archetype(%d) \"%s\" (index=%d)", j, modelInfo->GetModelName(), entryIndex);
									}
									else if (modelInfo->GetModelType() == MI_TYPE_PED)
									{
										const CDebugPedArchetypeProxy* pedModelInfo = (const CDebugPedArchetypeProxy*)modelInfo;

										if (pedModelInfo->GetPedComponentFileIndex() == i)
										{
											Displayf("  <- archetype(%d) \"%s\" PED_COMP", j, modelInfo->GetModelName());
										}

										if (pedModelInfo->GetPropsFileIndex() == i)
										{
											Displayf("  <- archetype(%d) \"%s\" PED_PROP", j, modelInfo->GetModelName());
										}
									}
								}
							}
						}
					}
				}
			}
			else if (ref.GetAssetType() == AST_DrawableStore)
			{
				const int i = ref.GetAssetIndex();

				if (g_DrawableStore.IsValidSlot(i))
				{
					Displayf("%s", CDTVStreamingIteratorTest_GetAssetPath(g_DrawableStore, i, INDEX_NONE).c_str());

					if (bShowArchetypes)
					{
						for (u32 j = 0; j < CDebugArchetype::GetMaxDebugArchetypeProxies(); j++)
						{
							const CDebugArchetypeProxy* modelInfo = CDebugArchetype::GetDebugArchetypeProxy(j);

							if (modelInfo && modelInfo->SafeGetDrawableIndex() == i)
							{
								Displayf("  <- archetype(%d) \"%s\"", j, modelInfo->GetModelName());
							}
						}
					}
				}
			}
			else if (ref.GetAssetType() == AST_FragmentStore)
			{
				const int i = ref.GetAssetIndex();

				if (g_FragmentStore.IsValidSlot(i))
				{
					Displayf("%s", CDTVStreamingIteratorTest_GetAssetPath(g_FragmentStore, i, INDEX_NONE).c_str());

					if (bShowArchetypes)
					{
						for (u32 j = 0; j < CDebugArchetype::GetMaxDebugArchetypeProxies(); j++)
						{
							const CDebugArchetypeProxy* modelInfo = CDebugArchetype::GetDebugArchetypeProxy(j);

							if (modelInfo)
							{
								if (modelInfo->SafeGetFragmentIndex() == i)
								{
									Displayf("  <- archetype(%d) \"%s\"", j, modelInfo->GetModelName());
								}
								else if (modelInfo->GetModelType() == MI_TYPE_VEHICLE)
								{
									const CDebugVehicleArchetypeProxy* vehModelInfo = (const CDebugVehicleArchetypeProxy*)modelInfo;

									if (vehModelInfo->GetHDFragmentIndex() == i)
									{
										Displayf("  <- archetype(%d) \"%s\" VEH_HD_FRAG", j, modelInfo->GetModelName());
									}
								}
							}
						}
					}
				}
			}
			else if (ref.GetAssetType() == AST_ParticleStore)
			{
				const int i = ref.GetAssetIndex();

				if (g_ParticleStore.IsValidSlot(i))
				{
					Displayf("%s", CDTVStreamingIteratorTest_GetAssetPath(g_ParticleStore, i, INDEX_NONE).c_str());

					if (bShowArchetypes)
					{
						for (u32 j = 0; j < CModelInfo::GetMaxModelInfos(); j++)
						{
							const CDebugArchetypeProxy* modelInfo = CDebugArchetype::GetDebugArchetypeProxy(j);

							if (modelInfo && modelInfo->GetPtFxAssetSlot() == i)
							{
								Displayf("  <- archetype(%d) \"%s\"", j, modelInfo->GetModelName());
							}
						}
					}
				}
			}
		}
	}
}

static bool FindAllUnreferencedAssets_ShouldSkipPath(const char* path)
{
	if (stristr(path, "/platform:/textures/"                    ) ||
		stristr(path, "/platform:/models/"                      ) ||
		stristr(path, "/textures/script_txds/"                  ) ||
		stristr(path, "/data/cdimages/scaleform_generic/"       ) ||
		stristr(path, "/data/cdimages/scaleform_web/"           ) ||
		stristr(path, "/data/cdimages/scaleform_minigames/"     ) ||
		stristr(path, "/models/cdimages/streamedpeds/"          ) ||
		stristr(path, "/models/cdimages/componentpeds"          ) ||
		stristr(path, "/levels/" RS_PROJECT "/generic/cutspeds/") ||
		stristr(path, "/levels/" RS_PROJECT "/vehiclemods/"     ) ||
		stristr(path, "_hi.ift/"                                ) ||
		stristr(path, "+hi.itd/"                                ) ||
		stristr(path, "+hidd.itd/"                              ) ||
		stristr(path, "+hidr.itd/"                              ) ||
		stristr(path, "+hifr.itd/"                              ))
	{
		return true;
	}

	return false;
}

static void FindAllUnreferencedAssets_button()
{
	DEBUG_TEXTURE_VIEWER_USE_DEBUG_MEMORY();

	const int numFlagWords_Txd  = (g_TxdStore     .GetSize() + 31)/32;
	const int numFlagWords_Dwd  = (g_DwdStore     .GetSize() + 31)/32;
	const int numFlagWords_Draw = (g_DrawableStore.GetSize() + 31)/32;
	const int numFlagWords_Frag = (g_FragmentStore.GetSize() + 31)/32;

	u32* flags_Txd  = rage_new u32[numFlagWords_Txd ];
	u32* flags_Dwd  = rage_new u32[numFlagWords_Dwd ];
	u32* flags_Draw = rage_new u32[numFlagWords_Draw];
	u32* flags_Frag = rage_new u32[numFlagWords_Frag];

	sysMemSet(flags_Txd , 0, numFlagWords_Txd *sizeof(u32));
	sysMemSet(flags_Dwd , 0, numFlagWords_Dwd *sizeof(u32));
	sysMemSet(flags_Draw, 0, numFlagWords_Draw*sizeof(u32));
	sysMemSet(flags_Frag, 0, numFlagWords_Frag*sizeof(u32));

	for (u32 i = 0; i < CDebugArchetype::GetMaxDebugArchetypeProxies(); i++)
	{
		const CDebugArchetypeProxy* modelInfo = CDebugArchetype::GetDebugArchetypeProxy(i);

		if (modelInfo == NULL)
		{
			continue;
		}

		// drawable
		{
			const int slot_Dwd  = modelInfo->SafeGetDrawDictIndex();
			const int slot_Draw = modelInfo->SafeGetDrawableIndex();
			const int slot_Frag = modelInfo->SafeGetFragmentIndex();

			if (slot_Dwd  != INDEX_NONE) { flags_Dwd [slot_Dwd /32] |= BIT(slot_Dwd %32); }
			if (slot_Draw != INDEX_NONE) { flags_Draw[slot_Draw/32] |= BIT(slot_Draw%32); }
			if (slot_Frag != INDEX_NONE) { flags_Frag[slot_Frag/32] |= BIT(slot_Frag%32); }
		}

		if (modelInfo->GetModelType() == MI_TYPE_PED)
		{
			const CDebugPedArchetypeProxy* pedModelInfo = (const CDebugPedArchetypeProxy*)modelInfo;

			const int slot_HDTxd  = pedModelInfo->GetHDTxdIndex();
			const int slot_Dwd1   = pedModelInfo->GetPedComponentFileIndex();
			const int slot_Dwd2   = pedModelInfo->GetPropsFileIndex();

			if (slot_HDTxd != INDEX_NONE) { flags_Txd[slot_HDTxd/32] |= BIT(slot_HDTxd%32); }
			if (slot_Dwd1  != INDEX_NONE) { flags_Dwd[slot_Dwd1 /32] |= BIT(slot_Dwd1 %32); }
			if (slot_Dwd2  != INDEX_NONE) { flags_Dwd[slot_Dwd2 /32] |= BIT(slot_Dwd2 %32); }
		}
		else if (modelInfo->GetModelType() == MI_TYPE_VEHICLE)
		{
			const CDebugVehicleArchetypeProxy* vehModelInfo = (const CDebugVehicleArchetypeProxy*)modelInfo;

			const int slot_HDTxd  = vehModelInfo->GetHDTxdIndex();
			const int slot_HDFrag = vehModelInfo->GetHDFragmentIndex();

			if (slot_HDTxd  != INDEX_NONE) { flags_Txd [slot_HDTxd /32] |= BIT(slot_HDTxd %32); }
			if (slot_HDFrag != INDEX_NONE) { flags_Frag[slot_HDFrag/32] |= BIT(slot_HDFrag%32); }
		}
	}

	for (int i = 0; i < g_TxdStore.GetSize(); i++)
	{
		if (g_TxdStore.IsValidSlot(i))// && !g_TxdStore.GetSlot(i)->m_isDummy)
		{
			const int parentTxdSlot = g_TxdStore.GetParentTxdSlot(i);

			if (parentTxdSlot >= 0)
			{
				flags_Txd[parentTxdSlot/32] |= BIT(parentTxdSlot%32);
			}
		}
	}

	for (int i = 0; i < g_DwdStore.GetSize(); i++)
	{
		if (g_DwdStore.IsValidSlot(i))
		{
			const int parentTxdSlot = g_DwdStore.GetParentTxdForSlot(strLocalIndex(i));

			if (parentTxdSlot >= 0)
			{
				flags_Txd[parentTxdSlot/32] |= BIT(parentTxdSlot%32);
			}
		}
	}

	for (int i = 0; i < g_DrawableStore.GetSize(); i++)
	{
		if (g_DrawableStore.IsValidSlot(i))
		{
			const int parentTxdSlot = g_DrawableStore.GetParentTxdForSlot(i);

			if (parentTxdSlot >= 0)
			{
				flags_Txd[parentTxdSlot/32] |= BIT(parentTxdSlot%32);
			}
		}
	}

	for (int i = 0; i < g_FragmentStore.GetSize(); i++)
	{
		if (g_FragmentStore.IsValidSlot(i))
		{
			const int parentTxdSlot = g_FragmentStore.GetParentTxdForSlot(i);

			if (parentTxdSlot >= 0)
			{
				flags_Txd[parentTxdSlot/32] |= BIT(parentTxdSlot%32);
			}
		}
	}

	// ===========================================

	for (int i = 0; i < g_TxdStore.GetSize(); i++)
	{
		if (g_TxdStore.IsValidSlot(i))// && !g_TxdStore.GetSlot(i)->m_isDummy)
		{
			if ((flags_Txd[i/32] & BIT(i%32)) == 0)
			{
				const atString desc = CDTVStreamingIteratorTest_GetAssetPath(g_TxdStore, i, INDEX_NONE);

				if (!FindAllUnreferencedAssets_ShouldSkipPath(desc.c_str()))
				{
					Displayf("g_TxdStore[%d] \"%s\" is not used!", i, desc.c_str());
				}
			}
		}
	}

	for (int i = 0; i < g_DwdStore.GetSize(); i++)
	{
		if (g_DwdStore.IsValidSlot(i))
		{
			if ((flags_Dwd[i/32] & BIT(i%32)) == 0)
			{
				const atString desc = CDTVStreamingIteratorTest_GetAssetPath(g_DwdStore, i, INDEX_NONE);

				if (!FindAllUnreferencedAssets_ShouldSkipPath(desc.c_str()))
				{
					Displayf("g_DwdStore[%d] \"%s\" is not used!", i, desc.c_str());
				}
			}
		}
	}

	for (int i = 0; i < g_DrawableStore.GetSize(); i++)
	{
		if (g_DrawableStore.IsValidSlot(i))
		{
			if ((flags_Draw[i/32] & BIT(i%32)) == 0)
			{
				const atString desc = CDTVStreamingIteratorTest_GetAssetPath(g_DrawableStore, i, INDEX_NONE);

				if (!FindAllUnreferencedAssets_ShouldSkipPath(desc.c_str()))
				{
					Displayf("g_DrawableStore[%d] \"%s\" is not used!", i, desc.c_str());
				}
			}
		}
	}

	for (int i = 0; i < g_FragmentStore.GetSize(); i++)
	{
		if (g_FragmentStore.IsValidSlot(i))
		{
			if ((flags_Frag[i/32] & BIT(i%32)) == 0)
			{
				const atString desc = CDTVStreamingIteratorTest_GetAssetPath(g_FragmentStore, i, INDEX_NONE);

				if (!FindAllUnreferencedAssets_ShouldSkipPath(desc.c_str()))
				{
					Displayf("g_FragmentStore[%d] \"%s\" is not used!", i, desc.c_str());
				}
			}
		}
	}

	delete[] flags_Txd;
	delete[] flags_Dwd;
	delete[] flags_Draw;
	delete[] flags_Frag;
}

static void FindAllDummyTxds_button()
{
	int count = 0;

	for (int i = 0; i < g_TxdStore.GetSize(); i++)
	{
		if (g_TxdStore.IsValidSlot(i) && g_TxdStore.GetSlot(i)->m_isDummy)
		{
			const int parentTxdSlot = g_TxdStore.GetParentTxdSlot(i);
			const bool parentIsDummy = (parentTxdSlot != INDEX_NONE && g_TxdStore.GetSlot(parentTxdSlot)->m_isDummy);

			Displayf("g_TxdStore[%d] \"%s\" is a dummy (parent is \"%s\"%s)", i, g_TxdStore.GetName(i), parentTxdSlot != INDEX_NONE ? g_TxdStore.GetName(parentTxdSlot) : "NULL", parentIsDummy ? "(DUMMY)" : "");

			for (int childTxdSlot = 0; childTxdSlot < g_TxdStore.GetSize(); childTxdSlot++)
			{
				if (g_TxdStore.IsValidSlot(childTxdSlot) && g_TxdStore.GetParentTxdSlot(childTxdSlot) == i)
				{
					Displayf("  child is \"%s\"%s", g_TxdStore.GetName(i), g_TxdStore.GetSlot(childTxdSlot)->m_isDummy ? "(DUMMY)" : "");
				}
			}

			count++;
		}
	}

	Displayf("found %d dummy txds", count);
}

static void ReportUnusedTextures_MarkTexturesUsedByDrawable(atMap<const grcTexture*,int>& shared, atMap<u32,int>& names, const Drawable* drawable)
{
	if (drawable)
	{
		const grmShaderGroup* shaderGroup = &drawable->GetShaderGroup();

		for (int i = 0; i < shaderGroup->GetCount(); i++)
		{
			const grmShader* shader = shaderGroup->GetShaderPtr(i);

			for (int j = 0; j < shader->GetVarCount(); j++)
			{
				const grcEffectVar var = shader->GetVarByIndex(j);
				const char* name = NULL;
				grcEffect::VarType type;
				int annotationCount = 0;
				bool isGlobal = false;

				shader->GetVarDesc(var, name, type, annotationCount, isGlobal);

				if (!isGlobal)
				{
					if (type == grcEffect::VT_TEXTURE)
					{
						grcTexture* texture = NULL;
						shader->GetVar(var, texture);

						if (texture)
						{
							int* data = shared.Access(texture);

							if (data)
							{
								(*data)++;
							}
							else
							{
								int* data2 = names.Access(atStringHash(texture->GetName()));

								if (data2)
								{
									(*data2)++;
								}
							}
						}
					}
				}
			}
		}
	}
}

template <bool bAutoLoad> static void ReportUnusedTextures_button()
{
	CDTVEntry_Txd* entry = dynamic_cast<CDTVEntry_Txd*>(gDebugTextureViewer.GetSelectedListEntry());

	if (entry)
	{
		const CTxdRef& ref = entry->GetTxdRef();

		if (1 || ref.GetAssetType() == AST_TxdStore)
		{
			const fwTxd* txd = ref.GetTxd();

			if (bAutoLoad && txd == NULL) // try loading it ..
			{
				gDebugTextureViewer.GetSelectedListEntry()->Load(CAssetRefInterface(gDebugTextureViewer.m_assetRefs, false));
				txd = ref.GetTxd();
			}

			if (txd)
			{
				atMap<const grcTexture*,int> shared;
				atMap<u32,int> names;

				Displayf("");

				switch ((int)ref.GetAssetType())
				{
				case AST_TxdStore      : Displayf("> scanning textures in %s ..", CDTVStreamingIteratorTest_GetAssetPath(g_TxdStore     , ref.GetAssetIndex(), INDEX_NONE).c_str()); break;
				case AST_DwdStore      : Displayf("> scanning textures in %s ..", CDTVStreamingIteratorTest_GetAssetPath(g_DwdStore     , ref.GetAssetIndex(), INDEX_NONE).c_str()); break;
				case AST_DrawableStore : Displayf("> scanning textures in %s ..", CDTVStreamingIteratorTest_GetAssetPath(g_DrawableStore, ref.GetAssetIndex(), INDEX_NONE).c_str()); break;
				case AST_FragmentStore : Displayf("> scanning textures in %s ..", CDTVStreamingIteratorTest_GetAssetPath(g_FragmentStore, ref.GetAssetIndex(), INDEX_NONE).c_str()); break;
				case AST_ParticleStore : Displayf("> scanning textures in %s ..", CDTVStreamingIteratorTest_GetAssetPath(g_ParticleStore, ref.GetAssetIndex(), INDEX_NONE).c_str()); break;
				}

				FindDependentAssets_button<true,false,false>();

				if (bAutoLoad)
				{
					CDebugTextureViewer::LoadAll();
				}

				for (int k = 0; k < txd->GetCount(); k++)
				{
					const grcTexture* texture = txd->GetEntry(k);

					if (texture)
					{
						shared[texture] = 0;
						names[atStringHash(texture->GetName())] = 0;
					}
				}

				for (int i = 0; i < g_DwdStore.GetSize(); i++)
				{
					if (g_DwdStore.IsValidSlot(i))
					{
						const Dwd* dwd = g_DwdStore.Get(i);

						if (dwd)
						{
							for (int j = 0; j < dwd->GetCount(); j++)
							{
								ReportUnusedTextures_MarkTexturesUsedByDrawable(shared, names, dwd->GetEntry(j));
							}
						}
					}
				}

				for (int i = 0; i < g_DrawableStore.GetSize(); i++)
				{
					if (g_DrawableStore.IsValidSlot(i))
					{
						ReportUnusedTextures_MarkTexturesUsedByDrawable(shared, names, g_DrawableStore.Get(i));
					}
				}

				for (int i = 0; i < g_FragmentStore.GetSize(); i++)
				{
					if (g_FragmentStore.IsValidSlot(i))
					{
						const Fragment* fragment = g_FragmentStore.Get(i);

						if (fragment)
						{
							ReportUnusedTextures_MarkTexturesUsedByDrawable(shared, names, fragment->GetCommonDrawable());
							ReportUnusedTextures_MarkTexturesUsedByDrawable(shared, names, fragment->GetClothDrawable());

							for (int j = 0; j < fragment->GetNumExtraDrawables(); j++)
							{
								ReportUnusedTextures_MarkTexturesUsedByDrawable(shared, names, fragment->GetExtraDrawable(j));
							}
						}
					}
				}

				for (int i = 0; i < g_ParticleStore.GetSize(); i++)
				{
					if (g_ParticleStore.IsValidSlot(i))
					{
						ptxFxList* ptx = g_ParticleStore.Get(i);

						if (ptx)
						{
							const Dwd* ptxDwd = ptx->GetModelDictionary();

							if (ptxDwd)
							{
								for (int j = 0; j < ptxDwd->GetCount(); j++)
								{
									ReportUnusedTextures_MarkTexturesUsedByDrawable(shared, names, ptxDwd->GetEntry(j));
								}
							}
						}
					}
				}

				int numTexturesUsed  = 0;
				int numTexturesTotal = 0;
				int numBytesUsed     = 0;
				int numBytesTotal    = 0;

				for (atMap<const grcTexture*,int>::Iterator iter = shared.CreateIterator(); iter; ++iter)
				{
					const grcTexture* texture = iter.GetKey();
					const int n = iter.GetData();

					if (n > 0)
					{
						numTexturesUsed++;
						numBytesUsed += texture->GetPhysicalSize();
					}

					numTexturesTotal++;
					numBytesTotal += texture->GetPhysicalSize();
				}

				for (int pass = 0; pass < 2; pass++)
				{
					for (atMap<const grcTexture*,int>::Iterator iter = shared.CreateIterator(); iter; ++iter)
					{
						const grcTexture* texture = iter.GetKey();
						const int n = iter.GetData();

						if (pass == 0 && n > 0)
						{
							Displayf(
								"+ %s (%s %dx%d) used by %d drawables",
								GetFriendlyTextureName(texture).c_str(),
								grcImage::GetFormatString((grcImage::Format)texture->GetImageFormat()),
								texture->GetWidth(),
								texture->GetHeight(),
								n
							);
						}
						else if (pass == 1 && n == 0)
						{
							const int* n2 = names.Access(atStringHash(texture->GetName()));

							if (n2 && *n2 > 0)
							{
								Displayf(
									"- %s (%s %dx%d) not used, but the name is used by %d drawables",
									GetFriendlyTextureName(texture).c_str(),
									grcImage::GetFormatString((grcImage::Format)texture->GetImageFormat()),
									texture->GetWidth(),
									texture->GetHeight(),
									*n2
								);
							}
							else
							{
								Displayf(
									"- %s (%s %dx%d) not used!",
									GetFriendlyTextureName(texture).c_str(),
									grcImage::GetFormatString((grcImage::Format)texture->GetImageFormat()),
									texture->GetWidth(),
									texture->GetHeight()
								);
							}
						}
					}
				}

				Displayf(
					"> %d textures, %d used (%.2f%%), %.2fKB data, %.2fKB used (%.2f%%)",
					numTexturesTotal,
					numTexturesUsed,
					100.0f*(float)numTexturesUsed/(float)numTexturesTotal,
					(float)numBytesTotal/1024.0f,
					(float)numBytesUsed/1024.0f,
					100.0f*(float)numBytesUsed/(float)numBytesTotal
				);

				if (bAutoLoad) // select txd and mark textures that are not used
				{
					entry->Select_Public(CAssetRefInterface(gDebugTextureViewer.m_assetRefs, false));

					for (int i = 0; i < gDebugTextureViewer.m_listEntries.GetCount(); i++)
					{
						CDTVEntry_Texture* textureEntry = dynamic_cast<CDTVEntry_Texture*>(gDebugTextureViewer.m_listEntries[i]);

						if (textureEntry)
						{
							const grcTexture* texture = textureEntry->GetCurrentTexture_Public();

							if (texture)
							{
								for (atMap<const grcTexture*,int>::Iterator iter = shared.CreateIterator(); iter; ++iter)
								{
									if (iter.GetKey() == texture)
									{
										if (iter.GetData() == 0)
										{
											textureEntry->SetErrorFlag(true);
										}

										break;
									}
								}
							}
						}
					}
				}
			}
		}
	}
}

class CDTVDuplicateTextureGroup
{
public:
	u32 m_CRC;
	int m_textureSize;
	int m_wastedBytes;
	atString m_textureName; // first texture name
	atArray<atString> m_texturePaths;

	bool operator <(const CDTVDuplicateTextureGroup& rhs) const { return m_wastedBytes < rhs.m_wastedBytes; }
	bool operator >(const CDTVDuplicateTextureGroup& rhs) const { return m_wastedBytes > rhs.m_wastedBytes; }

	static s32 CompareFunc   (CDTVDuplicateTextureGroup  const* a, CDTVDuplicateTextureGroup  const* b) { if ((*a) < (*b)) { return +1; } else if ((*a) > (*b)) { return -1; } else { return 0; }}
	static s32 CompareFuncPtr(CDTVDuplicateTextureGroup* const* a, CDTVDuplicateTextureGroup* const* b) { return CompareFunc(*a, *b); }
};

static char gDumpDuplicateTexturesPath[64] = "x:/duptex.txt";

static void DumpDuplicateTextures_button()
{
	fiStream* f = fiStream::Create(gDumpDuplicateTexturesPath);

	if (f)
	{
		// auto setup
		{
			gDebugTextureViewer.m_columnFlags.m_visible[CDTVColumnFlags::CID_TexCRC] = true;
			gDebugTextureViewer.m_asp.m_createTextureCRCs = true;
			gDebugTextureViewer.Populate_SearchTextures();
			gDebugTextureViewer.SetSortColumnID(CDTVColumnFlags::CID_TexCRC);
		}

		atArray<CDTVDuplicateTextureGroup*> groups;

		for (int i = 0; i < gDebugTextureViewer.GetListNumEntries(); i++)
		{
			const CDTVEntry_Texture* entry0 = dynamic_cast<const CDTVEntry_Texture*>(gDebugTextureViewer.GetListEntry(i));

			if (entry0)
			{
				const int firstEntry = i;
				int lastEntry = i;
				int wastedBytes = 0;
				int numDuplicates = 0;

				for (int j = i + 1; j < gDebugTextureViewer.GetListNumEntries(); j++)
				{
					const CDTVEntry_Texture* entry1 = dynamic_cast<const CDTVEntry_Texture*>(gDebugTextureViewer.GetListEntry(j));

					if (entry1 && entry1->GetTexCRC_Public() == entry0->GetTexCRC_Public())
					{
						wastedBytes += entry1->GetTexSizeInBytes_Public();
						numDuplicates++;
						lastEntry = j;
						i = j; // so we'll continue with the next entry (j+1)
					}
					else
					{
						break;
					}
				}

				if (numDuplicates > 0)
				{
					if (stristr(gDumpDuplicateTexturesPath, ".csv"))
					{
						// name, CRC, size, number of duplicates, total wasted bytes
						fprintf(f, "%s,0x%08x,%d,%d,%d\n", entry0->GetName().c_str(), entry0->GetTexCRC_Public(), entry0->GetTexSizeInBytes_Public(), numDuplicates, wastedBytes);
					}
					else
					{
						CDTVDuplicateTextureGroup* group = rage_new CDTVDuplicateTextureGroup;

						group->m_CRC = entry0->GetTexCRC_Public();
						group->m_textureSize = entry0->GetTexSizeInBytes_Public();
						group->m_wastedBytes = wastedBytes;
						group->m_textureName = entry0->GetName();

						for (int j = firstEntry; j <= lastEntry; j++)
						{
							const CDTVEntry_Texture* entry1 = dynamic_cast<const CDTVEntry_Texture*>(gDebugTextureViewer.GetListEntry(j));

							atString path("UNKNOWN");

							switch ((int)entry1->GetTxdRef().GetAssetType())
							{
							case AST_TxdStore      : path = CDTVStreamingIteratorTest_GetAssetPath(g_TxdStore     , entry1->GetTxdRef().GetAssetIndex(), INDEX_NONE); break;
							case AST_DwdStore      : path = CDTVStreamingIteratorTest_GetAssetPath(g_DwdStore     , entry1->GetTxdRef().GetAssetIndex(), entry1->GetTxdRef().m_assetEntry); break;
							case AST_DrawableStore : path = CDTVStreamingIteratorTest_GetAssetPath(g_DrawableStore, entry1->GetTxdRef().GetAssetIndex(), INDEX_NONE); break;
							case AST_FragmentStore : path = CDTVStreamingIteratorTest_GetAssetPath(g_FragmentStore, entry1->GetTxdRef().GetAssetIndex(), INDEX_NONE); break;
							case AST_ParticleStore : path = CDTVStreamingIteratorTest_GetAssetPath(g_ParticleStore, entry1->GetTxdRef().GetAssetIndex(), INDEX_NONE); break;
							}

							path += entry1->GetName();

							group->m_texturePaths.PushAndGrow(path);
						}

						groups.PushAndGrow(group);
					}
				}
			}
		}

		if (groups.GetCount() > 0)
		{
			groups.QSort(0, -1, CDTVDuplicateTextureGroup::CompareFuncPtr);

			for (int i = 0; i < groups.GetCount(); i++)
			{
				char line[1024] = "";

				sprintf(line, "== %.2fK wasted by %d duplicates of %s (%.2fK each) ==", (float)groups[i]->m_wastedBytes/1024.0f, groups[i]->m_texturePaths.GetCount() - 1, groups[i]->m_textureName.c_str(), (float)groups[i]->m_textureSize/1024.0f);
				fprintf(f, "%s\n", line);
				Displayf(line);

				for (int j = 0; j < groups[i]->m_texturePaths.GetCount(); j++)
				{
					char path[1024] = "";
					strcpy(path, groups[i]->m_texturePaths[j].c_str());

					if (strstr(path, "unzipped/"))
					{
						strcpy(line, strstr(path, "unzipped/") + strlen("unzipped/"));
					}
					else
					{
						strcpy(line, path);
					}

					fprintf(f, "%s\n", line);
					Displayf(line);
				}

				fprintf(f, "\n");
				Displayf("");

				delete groups[i];
			}
		}

		f->Close();
	}
}

#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEST

#if __DEV && EFFECT_PRESERVE_STRINGS

static void DumpShaderVarsForModel_button()
{
	const CDTVEntry* entry = gDebugTextureViewer.GetSelectedListEntry();

	if (entry)
	{
		const CBaseModelInfo* modelInfo = entry->GetModelInfo();

		if (modelInfo)
		{
			Displayf("model \"%s\"", modelInfo->GetModelName());

			const Drawable* drawable = modelInfo->GetDrawable();

			if (drawable)
			{
				const grmShaderGroup* shaderGroup = &drawable->GetShaderGroup();

				for (int i = 0; i < shaderGroup->GetCount(); i++)
				{
					const grmShader* shader = shaderGroup->GetShaderPtr(i);

					Displayf("  shader_%d(%s)", i, shader->GetName());

					for (int j = 0; j < shader->GetVarCount(); j++)
					{
						atString value;

						const grcEffectVar var = shader->GetVarByIndex(j);
						const char* name = NULL;
						grcEffect::VarType type;
						int annotationCount = 0;
						bool isGlobal = false;

						shader->GetVarDesc(var, name, type, annotationCount, isGlobal);

						if (!isGlobal)
						{
							if (name == NULL)
							{
								name = "NULL";
							}

							if (type == grcEffect::VT_TEXTURE)
							{
								grcTexture* texture = NULL;
								shader->GetVar(var, texture);

								if (texture)
								{
									atString textureName = GetFriendlyTextureName(texture);
#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEST
									textureName = CDTVStreamingIteratorTest_FindTexturePath(modelInfo, texture);
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEST
									Displayf("    %s = \"%s\"", name, textureName.c_str());
								}
								else
								{
									Displayf("    %s = NULL texture", name);
								}
							}
							else if (type == grcEffect::VT_FLOAT)
							{
								float f = 0.0f;
								shader->GetVar(var, f);

								Displayf("    %s = %f", name, f);
							}
							else if (type == grcEffect::VT_VECTOR2)
							{
								Vector2 v = Vector2(0.0f, 0.0f);
								shader->GetVar(var, v);

								Displayf("    %s = %f,%f", name, v.x, v.y);
							}
							else if (type == grcEffect::VT_VECTOR3)
							{
								Vector3 v = Vector3(0.0f, 0.0f, 0.0f);
								shader->GetVar(var, v);

								Displayf("    %s = %f,%f,%f", name, v.x, v.y, v.z);
							}
							else if (type == grcEffect::VT_VECTOR4)
							{
								Vector4 v = Vector4(0.0f, 0.0f, 0.0f, 0.0f);
								shader->GetVar(var, v);

								Displayf("    %s = %f,%f,%f,%f", name, v.x, v.y, v.z, v.w);
							}
							else
							{
								Displayf("    %s = UNKNOWN TYPE %d", name, type);
							}
						}
					}
				}
			}
		}
	}
}

static void DumpGeometriesForModel_button()
{
	const CDTVEntry* entry = gDebugTextureViewer.GetSelectedListEntry();
	const CBaseModelInfo* modelInfo = NULL;

	if (entry)
	{
		modelInfo = entry->GetModelInfo();
	}
	else
	{
		const CEntity* entity = static_cast<const CEntity*>(g_PickerManager.GetSelectedEntity());

		if (entity)
		{
			modelInfo = entity->GetBaseModelInfo();
		}
	}

	if (modelInfo)
	{
		Displayf("model \"%s\"", modelInfo->GetModelName());

		const Drawable* drawable = modelInfo->GetDrawable();

		if (drawable)
		{
			const rmcLodGroup& lodGroup = drawable->GetLodGroup();

			for (int lodIndex = 0; lodIndex < LOD_COUNT; lodIndex++)
			{
				if (lodGroup.ContainsLod(lodIndex))
				{
					const rmcLod& lod = lodGroup.GetLod(lodIndex);

					for (int lodModelIndex = 0; lodModelIndex < lod.GetCount(); lodModelIndex++)
					{
						const grmModel* pModel = lod.GetModel(lodModelIndex);

						if (pModel)
						{
							const grmModel& model = *pModel;
							const grmShaderGroup& shaderGroup = drawable->GetShaderGroup();
#if RAGE_SUPPORT_TESSELLATION_TECHNIQUES
							Displayf("    lod %d model %d has %d tessellated and %d untessellated geometries", lodIndex, lodModelIndex, model.GetTessellatedGeometryCount(), model.GetUntessellatedGeometryCount());
#endif // RAGE_SUPPORT_TESSELLATION_TECHNIQUES
							for (int geomIndex = 0; geomIndex < model.GetGeometryCount(); geomIndex++)
							{
								const grmGeometry& geom = model.GetGeometry(geomIndex);
								const int shaderIndex = model.GetShaderIndex(geomIndex);
								const grmShader& shader = shaderGroup.GetShader(shaderIndex);

								Displayf("    lod %d model %d geom %d has %d verts, using shader (%d) \"%s\"", lodIndex, lodModelIndex, geomIndex, geom.GetVertexCount(), model.GetShaderIndex(geomIndex), shader.GetName());
							}
						}
					}
				}
			}
		}
	}
}

// temporary hack until i fix the Search Models properly ..
static void SearchModelsTempHack_button()
{
	Displayf("searching for \"%s\" ...", gDebugTextureViewer.m_searchFilter);

	for (int i = 0; i < CDebugArchetype::GetMaxDebugArchetypeProxies(); i++)
	{
		const char* modelName = CDebugArchetype::GetDebugArchetypeProxy(i)->GetModelName();

		if (StringMatch(modelName, gDebugTextureViewer.m_searchFilter))
		{
			Displayf("  %s", modelName);
		}
	}
}

#endif // __DEV && EFFECT_PRESERVE_STRINGS

#if DEBUG_TEXTURE_VIEWER_ENTITY_FLAGS

static void EntityFlags_GetEntityVisPhaseFlags_button()
{
	const CDTVEntry* entry = gDebugTextureViewer.GetSelectedListEntry();

	if (entry)
	{
		const CEntity* entity = entry->GetEntity();

		if (entity)
		{
			gDebugTextureViewer.m_entityVisPhaseFlags = entity->GetRenderPhaseVisibilityMask();
		}
	}
}

static void EntityFlags_SetEntityVisPhaseFlags_button()
{
	const CDTVEntry* entry = gDebugTextureViewer.GetSelectedListEntry();

	if (entry)
	{
		const CEntity* entity = entry->GetEntity();

		if (entity)
		{
			const_cast<CEntity*>(entity)->SetRenderPhaseVisibilityMask(gDebugTextureViewer.m_entityVisPhaseFlags);
			const_cast<CEntity*>(entity)->RequestUpdateInWorld();
		}
	}
}

static void EntityFlags_GetEntityFlags_button()
{
	const CDTVEntry* entry = gDebugTextureViewer.GetSelectedListEntry();

	if (entry)
	{
		const CEntity* entity = entry->GetEntity();

		if (entity)
		{
			gDebugTextureViewer.m_entityFlags = entity->GetBaseFlags();
		}
	}
}

static void EntityFlags_SetEntityFlags_button()
{
	const CDTVEntry* entry = gDebugTextureViewer.GetSelectedListEntry();

	if (entry)
	{
		const CEntity* entity = entry->GetEntity();

		if (entity)
		{
			const_cast<CEntity*>(entity)->SetBaseFlag((fwEntity::flags)gDebugTextureViewer.m_entityFlags);
			const_cast<CEntity*>(entity)->RequestUpdateInWorld();
		}
	}
}

#endif // DEBUG_TEXTURE_VIEWER_ENTITY_FLAGS

/*
static bkGroup* g_TestWidgets_group = NULL;
static char     g_TestWidgets_name[64] = "";
static float    g_TestWidgets_valueStart = 0.0f;
static float    g_TestWidgets_valueMin = 0.0f;
static float    g_TestWidgets_valueMax = 10.0f;
static float    g_TestWidgets_step = 1.0f;

template <typename T> static void AddTestSlider()
{
	T* value = rage_new T;
	*value = (T)g_TestWidgets_valueStart;
	g_TestWidgets_group->AddSlider(g_TestWidgets_name, value, (T)g_TestWidgets_valueMin, (T)g_TestWidgets_valueMax, (float)g_TestWidgets_step);
}

template <> static void AddTestSlider<Vec3V>()
{
	Vec3V* value = rage_new Vec3V;
	*value = Vec3VFromF32(g_TestWidgets_valueStart);
	g_TestWidgets_group->AddSlider(g_TestWidgets_name, (Vector3*)value, g_TestWidgets_valueMin, g_TestWidgets_valueMax, g_TestWidgets_step);
}

static void AddTestVector()
{
	Vec3V* value = rage_new Vec3V;
	*value = Vec3VFromF32(g_TestWidgets_valueStart);
	g_TestWidgets_group->AddVector(g_TestWidgets_name, value, g_TestWidgets_valueMin, g_TestWidgets_valueMax, g_TestWidgets_step);
}

static void AddTestAngle()
{
	float* value = rage_new float;
	*value = g_TestWidgets_valueStart;
	g_TestWidgets_group->AddAngle(g_TestWidgets_name, value, bkAngleType::DEGREES, g_TestWidgets_valueMin, g_TestWidgets_valueMax);
}

static void AddTestMatrix()
{
	Mat33V* value = rage_new Mat33V;
	value->GetCol0Ref() = Vec3VFromF32(g_TestWidgets_valueStart);
	value->GetCol1Ref() = Vec3VFromF32(g_TestWidgets_valueStart);
	value->GetCol2Ref() = Vec3VFromF32(g_TestWidgets_valueStart);
	g_TestWidgets_group->AddMatrix(g_TestWidgets_name, (Matrix33*)value, g_TestWidgets_valueMin, g_TestWidgets_valueMax, g_TestWidgets_step);
}

static void TestWidgets(bkBank& bk)
{
	g_TestWidgets_group = bk.AddGroup("Test Widgets");
	g_TestWidgets_group->AddText("NAME", &g_TestWidgets_name[0], sizeof(g_TestWidgets_name), false);
	g_TestWidgets_group->AddSlider("CURRENT VALUE", &g_TestWidgets_valueStart, -100.0f, 100.0f, 0.001f);
	g_TestWidgets_group->AddSlider("MIN VALUE", &g_TestWidgets_valueMin, -100.0f, 100.0f, 0.001f);
	g_TestWidgets_group->AddSlider("MAX VALUE", &g_TestWidgets_valueMax, -100.0f, 100.0f, 0.001f);
	g_TestWidgets_group->AddSlider("STEP", &g_TestWidgets_step, -100.0f, 100.0f, 0.001f);
	g_TestWidgets_group->AddButton("TEST INT SLIDER", AddTestSlider<int>);
	g_TestWidgets_group->AddButton("TEST FLOAT SLIDER", AddTestSlider<float>);
	g_TestWidgets_group->AddButton("TEST VEC3V SLIDER", AddTestSlider<Vec3V>);
	g_TestWidgets_group->AddButton("TEST VEC3V VECTOR", AddTestVector);
	g_TestWidgets_group->AddButton("TEST ANGLE", AddTestAngle);
	g_TestWidgets_group->AddButton("TEST MATRIX", AddTestMatrix);
	g_TestWidgets_group->AddSeparator();
}
*/

#define USE_FLIPBOOK_ANIM_TEST (1 && __DEV)

#if USE_FLIPBOOK_ANIM_TEST

static bool      g_flipbookUpdateEnabled     = false;
static bool      g_flipbookSequenceEnabled   = true;
static int       g_flipbookSequenceIndex     = 0;
static float     g_flipbookSequenceFreqScale = 1.0f;
static grcImage* g_flipbookSequenceImage     = NULL;
static float     g_flipbookTime              = 0.0f;
static float     g_flipbookTimeScale         = 1.0f;
// these are exposed as shader params
static float     g_flipbookShaderFreq[2]     = {30.0f, 30.0f};
static float     g_flipbookShaderPhase       = 0.0f;
// these are exposed as per-vertex data
static int       g_flipbookVertexPhaseOffset = 0;
static int       g_flipbookVertexFreqInterp  = 0;

static void UpdateFlipbookAnim(int& frame)
{
	if (g_flipbookUpdateEnabled)
	{
		const grcTexture* texture = gDebugTextureViewer.m_previewTexture;

		if (texture == NULL)
		{
			const CDTVEntry_Texture* entry = dynamic_cast<const CDTVEntry_Texture*>(gDebugTextureViewer.GetSelectedListEntry());

			if (entry)
			{
				texture = entry->GetCurrentTexture_Public();
			}
		}

		if (texture)
		{
			const int frameCount = texture->GetLayerCount();
			const float dt = 1.0f/30.0f;

			g_flipbookTime += g_flipbookTimeScale*dt;

			// see megashader.fxh - CalculateFlipbookIndex
			{
				const float cpv_phaseOffset = (float)g_flipbookVertexPhaseOffset/255.0f; // phase offset (frames)
				const float cpv_freqInterp  = (float)g_flipbookVertexFreqInterp/255.0f; // base interpolation between freq.x and freq.y (constant)

				const float t     = g_flipbookTime; // time in seconds
				const float freq  = g_flipbookShaderFreq[0] + (g_flipbookShaderFreq[1] - g_flipbookShaderFreq[0])*cpv_freqInterp; // frequency (frames/sec)
				const float phase = g_flipbookShaderPhase + cpv_phaseOffset*frameCount; // phase (frames)
				const float x     = phase + t*freq;

				if (g_flipbookSequenceEnabled &&
					g_flipbookSequenceImage)
				{
					const int w = g_flipbookSequenceImage->GetWidth();
					const int h = g_flipbookSequenceImage->GetHeight();
					const u8* row = ((const u8*)g_flipbookSequenceImage->GetBits()) + Clamp<int>(g_flipbookSequenceIndex, 0, h - 1)*w;

					int i = (int)fmodf(x*g_flipbookSequenceFreqScale, (float)w);

					// testing
					/*static bool alt1 = false;
					if (alt1)
						i = (int)(x*g_flipbookSequenceFreqScale)%w;
					static bool alt2 = false;
					if (alt2) {
						static int i_ = 0;
						i = i_;
						if (++i_ >= w)
							i_ = 0;
					}*/

					frame = (((int)row[i])*frameCount)/256;

					// testing
					/*static bool alt3 = false;
					if (alt3)
						Displayf("i=%d, row[i]=%u, count=%d, frame=%d", i, row[i], frameCount, frame);
					static bool alt4 = false;
					if (alt4) {
						char temp[4096] = "";
						for (int i = 0; i < w && strlen(temp) < sizeof(temp) - 10; i++)
							strcat(temp, atVarString("%d,", row[i]).c_str());
						Displayf("%s", temp);
						alt4 = false;
					}*/
				}
				else
				{
					frame = (int)fmodf(x, (float)frameCount);
				}
			}
		}
	}
}

static void AddFlipbookAnimWidgets_LoadSequenceImage()
{
	static char path[256] = "";

	if (BANKMGR.OpenFile(path, sizeof(path), "*.dds", false, "Open DDS"))
	{
		if (g_flipbookSequenceImage)
		{
			g_flipbookSequenceImage->Release();
		}

		g_flipbookSequenceImage = grcImage::LoadDDS(path);

		if (g_flipbookSequenceImage)
		{
			g_flipbookSequenceEnabled = true;
		}
	}
}

static void AddFlipbookAnimWidgets(bkBank& bk)
{
	bk.PushGroup("Tex Array Flipbook Anim", false);
	{
		bk.AddToggle("Update Enabled", &g_flipbookUpdateEnabled);
		bk.AddToggle("Sequence Enabled", &g_flipbookSequenceEnabled);
		bk.AddSlider("Sequence Index", &g_flipbookSequenceIndex, 0, 63, 1);
		bk.AddSlider("Sequence Freq Scale", &g_flipbookSequenceFreqScale, 0.0f, 60.0f, 1.0f/32.0f);
		bk.AddButton("Load Sequence Image", AddFlipbookAnimWidgets_LoadSequenceImage);
		bk.AddSlider("Time Scale", &g_flipbookTimeScale, -10.0f, 10.0f, 1.0f/32.0f);
		bk.AddSeparator();
		bk.AddSlider("Shader - Freq.x", &g_flipbookShaderFreq[0], -256.0f, 256.0f, 1.0f/32.0f);
		bk.AddSlider("Shader - Freq.y", &g_flipbookShaderFreq[1], -256.0f, 256.0f, 1.0f/32.0f);
		bk.AddSlider("Shader - Phase", &g_flipbookShaderPhase, -256.0f, 256.0f, 1.0f/32.0f);
		bk.AddSeparator();
		bk.AddSlider("Vertex - Phase Offset", &g_flipbookVertexPhaseOffset, 0, 255, 1);
		bk.AddSlider("Vertex - Freq Interp", &g_flipbookVertexFreqInterp, 0, 255, 1);
	}
	bk.PopGroup();
}

#endif // USE_FLIPBOOK_ANIM_TEST

void CDebugTextureViewer::InitWidgets(bkBank& bk)
{
	//TestWidgets(bk);
#if RDR_VERSION // TEMP -- not integrating this over to V
	DXTDecompressionTable::AddWidgets(bk);
#endif
	// reinit column flags, as they depend on a command line arg which apparently can't be accessed during the static constructor of CDebugWindow
	{
		m_columnFlags = CDTVColumnFlags();
		m_asp         = CAssetSearchParams();
	}

	const char* hoverEntityShaderModeStrings[] =
	{
		"Off",
		"Unique Shader Names",
		"All Shader Names + Hover Shader",
		"All Shader Names + Hover Shader Expand Vars",
		"Hover Shader Only",
		"Hover Shader Only + Show Texture Paths",
	};

	bk.AddToggle("Visible"                   , &m_visible);
	bk.AddToggle("Enable Picking"            , &m_enablePicking ENTITYSELECT_ONLY(COMMA CEntitySelect::SetEntitySelectPassEnabled));
#if ENTITYSELECT_CHECK
	bk.AddToggle("Assert Entity Select On"   , &m_assertEntitySelectOn);
#endif // ENTITYSELECT_CHECK
	bk.AddCombo ("Hover Entity Shader Mode"  , &m_hoverEntityShaderMode, NELEM(hoverEntityShaderModeStrings), hoverEntityShaderModeStrings);
	bk.AddToggle("Hover Text Use Minifont"   , &m_hoverTextUseMiniFont);
#if DEBUG_TEXTURE_VIEWER_GET_WORLD_POS_UNDER_MOUSE
	bk.AddToggle("Show World Pos Under Mouse", &m_showWorldPosUnderMouse);
	bk.AddToggle("Show World Pos Marker"     , &m_showWorldPosMarker);
#endif // DEBUG_TEXTURE_VIEWER_GET_WORLD_POS_UNDER_MOUSE
	bk.AddToggle("Show Description"          , &m_showDesc);
	bk.AddToggle("Show References"           , &m_showRefs);
#if __DEV
	bk.AddToggle("Verbose"                   , &m_verbose );
#endif // __DEV
	bk.AddButton("Wireframe On/Off"          , _ToggleWireframe);

	bk.AddSeparator();

	bk.AddCombo ("Search Type"       , (int*)&m_searchType, PST_COUNT, GetUIStrings_ePopulateSearchType());
	bk.AddText  ("Search Filter"     , &m_searchFilter[0], sizeof(m_searchFilter), false);
	bk.AddText  ("Search RPF"        , &m_searchRPF[0], sizeof(m_searchRPF), false);
	bk.AddButton("Search"            , &_Populate_Search);
	bk.AddButton("Select From Picker", &_Populate_SelectFromPicker);

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEST
	// alias to button in Utilities below
	bk.AddButton("Report Unused Textures (Auto-Load)", ReportUnusedTextures_button<true>);
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEST

	bk.AddSeparator();

	const char* volumeCoordSwapStrings[] =
	{
		"xyz",
		"zyx",
		"xzy",
	};

	const char* cubeFaceStrings[] =
	{
		"+z (up)",
		"-z (down)",
		"-x (left)",
		"+x (right)",
		"+y (front)",
		"-y (back)",
	};
	CompileTimeAssert(NELEM(cubeFaceStrings) == DebugTextureViewerDTQ::eDTQCF_Count);

	const char* cubeProjectionStrings[] =
	{
		"faces",
		"4x3 cross",
		"panorama",
		"cylinder",
		"mirror ball",
		"spherical",
		"spherical square",
		"paraboloid",
		"paraboloid square",
	};
	CompileTimeAssert(NELEM(cubeProjectionStrings) == DebugTextureViewerDTQ::eDTQCP_Count);

	bk.AddCombo ("Channel View Mode"   , &m_previewTextureViewMode, DebugTextureViewerDTQ::eDTQVM_COUNT, DebugTextureViewerDTQ::GetUIStrings_eDTQViewMode());
	bk.AddToggle("Show Mips"           , &m_previewTextureShowMips);
	bk.AddToggle("Filtered"            , &m_previewTextureFiltered);
	bk.AddSlider("Mip Level"           , &m_previewTextureLOD, 0, 16, 1);
	bk.AddSlider("Tex Array Layer"     , &m_previewTextureLayer, 0, 255, 1);
	bk.AddSlider("Volume Slice"        , &m_previewTextureVolumeSlice, 0.0f, 1.0f, 1.0f/64.0f);
//	bk.AddSlider("Volume Slice Numer"  , &m_previewTextureVolumeSliceNumer, 0, 65536, 1);
//	bk.AddSlider("Volume Slice Denom"  , &m_previewTextureVolumeSliceDenom, 0, 65536, 1);
	bk.AddCombo ("Volume Coord"        , &m_previewTextureVolumeCoordSwap, NELEM(volumeCoordSwapStrings), volumeCoordSwapStrings);
	bk.AddToggle("Cube Force Opaque"   , &m_previewTextureCubeForceOpaque);
	bk.AddToggle("Cube Sample All Mips", &m_previewTextureCubeSampleAllMips);
	bk.AddToggle("Cube Reflection Map" , &m_previewTextureCubeReflectionMap);
	bk.AddSlider("Cube Cylinder Aspect", &m_previewTextureCubeCylinderAspect, 1.0f/16.0f, 16.0f, 1.0f/64.0f);
	bk.AddSlider("Cube Face Zoom"      , &m_previewTextureCubeFaceZoom, 1.0f/16.0f, 64.0f, 1.0f/64.0f);
	bk.AddCombo ("Cube Face"           , &m_previewTextureCubeFace, NELEM(cubeFaceStrings), cubeFaceStrings);
	bk.AddVector("Cube Rotation"       , &m_previewTextureCubeRotationYPR, 0.0f, 360.0f, 1.0f);
	bk.AddCombo ("Cube Projection"     , &m_previewTextureCubeProjection, NELEM(cubeProjectionStrings), cubeProjectionStrings);
	bk.AddToggle("Cube Projection Clip", &m_previewTextureCubeProjClip);
	bk.AddToggle("Cube Negate"         , &m_previewTextureCubeNegate);
	bk.AddSlider("Cube Reference Dots" , &m_previewTextureCubeRefDots, 0.0f, 1.0f, 1.0f/32.0f);
	bk.AddSlider("Texture Offset X"    , &m_previewTextureOffsetX, -512.0f, 512.0f, 1.0f/8.0f);
	bk.AddSlider("Texture Offset Y"    , &m_previewTextureOffsetY, -512.0f, 512.0f, 1.0f/8.0f);
	bk.AddSlider("Texture Zoom"        , &m_previewTextureZoom, 1.0f/16.0f, 256.0f, 1.0f/64.0f);
	bk.AddSlider("Texture Brightness"  , &m_previewTextureBrightness, -6.0f, 6.0f, 1.0f/64.0f);
	bk.AddToggle("Stretch to Fit"      , &m_previewTextureStretchToFit);

#if DEBUG_TEXTURE_VIEWER_EDIT_METADATA_IN_WORKBENCH
	bk.AddButton("Edit Metadata File (SHIFT-W)", &_Edit_MetadataInWorkbench);
#endif // DEBUG_TEXTURE_VIEWER_EDIT_METADATA_IN_WORKBENCH

	bk.AddSeparator();

#if USE_FLIPBOOK_ANIM_TEST
	AddFlipbookAnimWidgets(bk);
#endif // USE_FLIPBOOK_ANIM_TEST

	bk.PushGroup("Search Params", false);
	{
		bk.AddToggle("Select Model Textures", &m_selectModelTextures);
#if __DEV
		bk.AddToggle("Print Shader Names for Model Search Results", &m_printShaderNamesForModelSearchResults);
#endif // __DEV

		bk.AddSeparator();

		bk.AddToggle("Exact Name Match" , &m_asp.m_exactNameMatch);
#if __DEV
		bk.AddToggle("Show Empty Txds"  , &m_asp.m_showEmptyTxds );
		bk.AddToggle("Show Streamable"  , &m_asp.m_showStreamable);
		bk.AddToggle("Find Dependencies", &m_asp.m_findDependenciesWhilePopulating);
#endif // __DEV

		bk.AddSeparator();

#if DEBUG_TEXTURE_VIEWER_ENTITY_FILTERING
		bk.AddCombo("Scan Entity LODs", &m_asp.m_entityLODFilter, ELODSP_COUNT, GetUIStrings_eEntityLODSearchParams());

		bk.AddSeparator();

		for (int i = 0; i < ENTITY_TYPE_TOTAL; i++)
		{
			bk.AddToggle(atVarString("Scan Entity Type %s", GetEntityTypeStr(i, false) + strlen("ENTITY_TYPE_")), &m_asp.m_entityTypeFlags[i]);
		}

		bk.AddSeparator();
#endif // DEBUG_TEXTURE_VIEWER_ENTITY_FILTERING

		bk.AddToggle("Scan Loaded Models Only", &m_asp.m_modelInfoShowLoadedModelsOnly);

		bk.AddSeparator();

		for (int i = 0; i < CAssetSearchParams::_MI_TYPE_COUNT; i++)
		{
			bk.AddToggle(atVarString("Scan Model Type %s", GetModelInfoTypeStr(i, false) + strlen("MI_TYPE_")), &m_asp.m_modelInfoTypeFlags[i]);
		}

		bk.AddSeparator();

		bk.AddCombo("Scan Props", &m_asp.m_modelInfoShowProps, MISP_COUNT, GetUIStrings_eModelInfoShowProps());

		bk.AddSeparator();

		bk.AddToggle("Scan TxdStore"     , &m_asp.m_scanTxdStore     );
		bk.AddToggle("Scan DwdStore"     , &m_asp.m_scanDwdStore     );
		bk.AddToggle("Scan DrawableStore", &m_asp.m_scanDrawableStore);
		bk.AddToggle("Scan FragmentStore", &m_asp.m_scanFragmentStore);
		bk.AddToggle("Scan ParticleStore", &m_asp.m_scanParticleStore);

		bk.AddToggle("Create Texture CRCs", &m_asp.m_createTextureCRCs);

#if __DEV
		bk.PushGroup("Advanced Search", false);
		{
			bk.AddToggle("Asset Range Enabled", &m_asp.m_scanAssetIndexRangeEnabled);
			bk.AddSlider("Asset Range Min"    , &m_asp.m_scanAssetIndexRange[0], 0, 99999, 1);
			bk.AddSlider("Asset Range Max"    , &m_asp.m_scanAssetIndexRange[1], 0, 99999, 1);

			bk.AddSeparator();

			// these are specific to FindAssets_, i.e. "which models use this particular asset?"
			static const char* eAssetType_UIStrings[6] =
			{
				"None",
				"Txd",
				"Dwd",
				"Drawable",
				"Fragment",
				"Particle",
			};
			bk.AddCombo ("Filter Asset Type"       , &m_asp.m_filterAssetType, 6, eAssetType_UIStrings);
			bk.AddText  ("Filter Asset Name"       , &m_asp.m_filterAssetName[0], sizeof(m_asp.m_filterAssetName), false);
			bk.AddToggle("Filter Dwd Entry Enabled", &m_asp.m_filterDwdEntryEnabled);
			bk.AddSlider("Filter Dwd Entry"        , &m_asp.m_filterDwdEntry, 0, 16, 1);

			bk.AddSeparator();

			bk.AddToggle("Find Assets Only if Loaded"  , &m_findAssetsOnlyIfLoaded);
			bk.AddSlider("Find Assets Max Count"       , &m_findAssetsMaxCount, 5, 99999, 1);
			bk.AddSlider("Find Textures Min Size"      , &m_findTexturesMinSize, 0, 4096, 1);
			bk.AddToggle("Find Textures Constant Only" , &m_findTexturesConstantOnly);
			bk.AddToggle("Find Textures Redundant Only", &m_findTexturesRedundantOnly);

			bk.AddToggle("Find Textures Report Non-Unique Texture Memory", &m_findTexturesReportNonUniqueTextureMemory);

			bk.AddTitle("Find Textures Private Flags (required)");
			bk.AddToggle("TEXTUREFLAG_PROCESSED"        , &m_findTexturesConversionFlagsRequired, TEXTURE_CONVERSION_FLAG_PROCESSED);
			bk.AddToggle("TEXTUREFLAG_SKIPPED"          , &m_findTexturesConversionFlagsRequired, TEXTURE_CONVERSION_FLAG_SKIPPED);
			bk.AddToggle("TEXTUREFLAG_FAILED_PROCESSING", &m_findTexturesConversionFlagsRequired, TEXTURE_CONVERSION_FLAG_FAILED_PROCESSING);
			bk.AddToggle("TEXTUREFLAG_INVALID_METADATA" , &m_findTexturesConversionFlagsRequired, TEXTURE_CONVERSION_FLAG_INVALID_METADATA);
			bk.AddToggle("TEXTUREFLAG_OPTIMISED_DXT"    , &m_findTexturesConversionFlagsRequired, TEXTURE_CONVERSION_FLAG_OPTIMISED_DXT);

			bk.AddTitle("Find Textures Private Flags (skipped)");
			bk.AddToggle("TEXTUREFLAG_PROCESSED"        , &m_findTexturesConversionFlagsSkipped, TEXTURE_CONVERSION_FLAG_PROCESSED);
			bk.AddToggle("TEXTUREFLAG_SKIPPED"          , &m_findTexturesConversionFlagsSkipped, TEXTURE_CONVERSION_FLAG_SKIPPED);
			bk.AddToggle("TEXTUREFLAG_FAILED_PROCESSING", &m_findTexturesConversionFlagsSkipped, TEXTURE_CONVERSION_FLAG_FAILED_PROCESSING);
			bk.AddToggle("TEXTUREFLAG_INVALID_METADATA" , &m_findTexturesConversionFlagsSkipped, TEXTURE_CONVERSION_FLAG_INVALID_METADATA);
			bk.AddToggle("TEXTUREFLAG_OPTIMISED_DXT"    , &m_findTexturesConversionFlagsSkipped, TEXTURE_CONVERSION_FLAG_OPTIMISED_DXT);

			bk.AddSeparator();

			bk.AddText("Find Textures Usage"          , &m_findTexturesUsage         [0], sizeof(m_findTexturesUsage         ), false); // e.g. "BumpTex"
			bk.AddText("Find Textures Usage Exclusion", &m_findTexturesUsageExclusion[0], sizeof(m_findTexturesUsageExclusion), false); // e.g. "BumpTex"
			bk.AddText("Find Textures Format"         , &m_findTexturesFormat        [0], sizeof(m_findTexturesFormat        ), false); // e.g. "DXT5"
		}
		bk.PopGroup();
#endif // __DEV
	}
	bk.PopGroup();

	bk.PushGroup("Column Properties", false);
	{
		for (int i = 0; i < CDTVColumnFlags::CID_COUNT; i++)
		{
			const CDTVColumnFlags::eColumnID id = (const CDTVColumnFlags::eColumnID)i;

			bk.AddToggle(atVarString("Show Column \"%s\"", CDTVColumnFlags::GetColumnName(id)), &m_columnFlags.m_visible[id]);
		}

		bk.AddSlider(atVarString("Max Chars \"%s\"", CDTVColumnFlags::GetColumnName(CDTVColumnFlags::CID_Name     )), &m_columnFlags.m_columnMaxChars_Name     , 3, 80, 1);
		bk.AddSlider(atVarString("Max Chars \"%s\"", CDTVColumnFlags::GetColumnName(CDTVColumnFlags::CID_AssetName)), &m_columnFlags.m_columnMaxChars_AssetName, 3, 40, 1);
	}
	bk.PopGroup();

	bk.PushGroup("Window Properties", false);
	{
		bk.AddSlider("Window Height"        , &m_windowNumListSlots, 0, 32, 1);
		bk.AddSlider("Window Scale"         , &m_windowScale, 1.0f/4.0f, 4.0f, 1.0f/64.0f);
		bk.AddSlider("Window Border"        , &m_windowBorder, 0.0f, 16.0f, 1.0f/64.0f);
		bk.AddSlider("Window Spacing"       , &m_windowSpacing, 0.0f, 16.0f, 1.0f/64.0f);
		bk.AddButton("Reset Window Position", &DebugWindow_ResetPosition);

		static const char* s_previewTxdGrid_UIStrings[8] =
		{
			"1x1",
			"2x2",
			"3x3",
			"4x4",
			"5x5",
			"6x6",
			"7x7",
			"8x8",
		};
		bk.AddCombo("Preview Txd Grid"        , &m_previewTxdGrid, 8, s_previewTxdGrid_UIStrings);
		bk.AddSlider("Preview Txd Grid Offset", &m_previewTxdGridOffset, 0.0f, 1.0f, 1.0f/64.0f);
	}
	bk.PopGroup();

	bk.PushGroup("Load Texture/Txd", false);
	{
		bk.AddButton("Load Preview Texture ..."              , &LoadPreviewTexture);
		bk.AddText  ("Preview Texture Name"                  , &m_previewTextureName[0], sizeof(m_previewTextureName), false);
		bk.AddText  ("Preview Texture TCP"                   , &m_previewTextureTCP[0], sizeof(m_previewTextureTCP), false);
		bk.AddToggle("Preview Texture Linear"                , &m_previewTextureLinear);
	//	bk.AddToggle("Preview Texture sRGB"                  , &m_previewTextureSRGB);
		bk.AddButton("Reload Preview Texture"                , &LoadPreviewTexture_Reload);
		bk.AddText  ("Sandbox"                               , &m_sandboxTxdName[0], sizeof(m_sandboxTxdName), false);
		bk.AddButton("Load Sandbox Txd"                      , &LoadSandboxTxd);
#if DEBUG_TEXTURE_VIEWER_NEW_STREAMING_CODE
		bk.AddButton("Remove Sandbox Txds"                   , &RemoveSandboxTxds);
		bk.AddToggle("Use New Streaming Code for Sandbox Txd", &m_sandboxTxdUseNewStreamingCode);
#endif // DEBUG_TEXTURE_VIEWER_NEW_STREAMING_CODE
		bk.AddButton("Load All List Entries"                 , &LoadAll);
	}
	bk.PopGroup();

	bk.PushGroup("Specular Test", false);
	{
		bk.AddSlider("L"                , &m_previewTextureSpecularL        , -4.0f,    4.0f, 1.0f/64.0f);
		bk.AddSlider("Bumpiness"        , &m_previewTextureSpecularBumpiness, -4.0f,    4.0f, 1.0f/64.0f);
		bk.AddSlider("Specular Exponent", &m_previewTextureSpecularExponent ,  0.5f, 1024.0f, 1.0f/64.0f);
		bk.AddSlider("Specular Scale"   , &m_previewTextureSpecularScale    ,  1.0f,    4.0f, 1.0f/64.0f);
	}
	bk.PopGroup();

#if DEBUG_TEXTURE_VIEWER_HISTOGRAM
	bk.PushGroup("Histogram", false);
	{
		bk.AddToggle("Show Histogram" , &m_histogramEnabled);
		bk.AddSlider("Histogram Shift", &m_histogramShift, 0, 4, 1);
		bk.AddSlider("Histogram Scale", &m_histogramScale, 1.0f, 10.0f, 1.0f/64.0f);
		bk.AddSlider("Min Alpha Value", &m_histogramAlphaMin, 0, 255, 1);
		bk.AddSlider("Min Bucket Size", &m_histogramBucketMin, 0, 1024, 1);
		bk.AddSlider("Outliers"       , &m_histogramOutliers, 0, 65536, 1);
	}
	bk.PopGroup();
#endif // DEBUG_TEXTURE_VIEWER_HISTOGRAM

#if DEBUG_TEXTURE_VIEWER_TEXTUREUSAGEANALYSER
	bk.PushGroup("Texture Usage Analyser", false);
	{
		bk.AddSlider("Max Texture Count"               , &m_textureUsageAnalyserMaxCount, 10, 5000, 1);
		bk.AddToggle("Build References"                , &m_textureUsageAnalyserBuildRefs);
		bk.AddToggle("Hash Texture Data (SLOW!)"       , &m_textureUsageAnalyserHashTextureData);
		bk.AddToggle("Show Textures Not Used by Shader", &m_textureUsageAnalyserShowTexturesNotUsedByShader);
		bk.AddToggle("Show Model References"           , &m_textureUsageAnalyserShowModelReferences);
		bk.AddToggle("CSV Output"                      , &m_textureUsageAnalyserCSVOutput);

		bk.AddButton("Texture Usage Analyser", &_TextureUsageAnalyser);
	}
	bk.PopGroup();
#endif // DEBUG_TEXTURE_VIEWER_TEXTUREUSAGEANALYSER

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEST
	bk.PushGroup("Streaming Iterator (Prototype)", false);
	{
		class SuppressTextureReferenceWarning_button { public: static void func()
		{
			Displayf("suppressing texture reference warnings");
			grcTexture::SetSuppressReferenceWarning();
		}};

		class OpenStreamingIteratorLogFile_button { public: static void func()
		{
			Displayf("opening log file \"assets:/si.log\"");
			extern void siLogOpen(const char* path);
			siLogOpen("si");
		}};

		bk.AddButton("Suppress Texture Reference Warning"     , SuppressTextureReferenceWarning_button::func); // these warnings are really annoying, make them go away
		bk.AddButton("Open Streaming Iterator Log File"       , OpenStreamingIteratorLogFile_button::func);
		bk.AddToggle("Streaming Iterator Test Verbose"        , &gStreamingIteratorTest_Verbose);
		bk.AddToggle("Streaming Iterator Test Verbose Errors" , &gStreamingIteratorTest_VerboseErrors);
		bk.AddText  ("Search RPF"                             , &gStreamingIteratorTest_SearchRPF[0], sizeof(gStreamingIteratorTest_SearchRPF), false);
		bk.AddText  ("Search Name"                            , &gStreamingIteratorTest_SearchName[0], sizeof(gStreamingIteratorTest_SearchName), false);
		bk.AddToggle("Search Flags - Skip Vehicles"           , &gStreamingIteratorTest_SearchFlags, SI_SEARCH_FLAG_SKIP_VEHICLES);
		bk.AddToggle("Search Flags - Skip Peds"               , &gStreamingIteratorTest_SearchFlags, SI_SEARCH_FLAG_SKIP_PEDS);
		bk.AddToggle("Search Flags - Skip Props"              , &gStreamingIteratorTest_SearchFlags, SI_SEARCH_FLAG_SKIP_PROPS);
		bk.AddToggle("Search Flags - Don't Skip"              , &gStreamingIteratorTest_SearchFlags, SI_SEARCH_FLAG_DONT_SKIP);
		bk.AddSeparator();
		bk.AddToggle("Find Uncompressed Textures"             , &gStreamingIteratorTest_FindUncompressedTexturesNew);
		bk.AddText  ("Find Shader In Drawables"               , &gStreamingIteratorTest_FindShaderInDrawables[0], sizeof(gStreamingIteratorTest_FindShaderInDrawables), false);
		bk.AddText  ("Find Texture In Drawables"              , &gStreamingIteratorTest_FindTextureInDrawables[0], sizeof(gStreamingIteratorTest_FindTextureInDrawables), false);
		bk.AddText  ("Find Archetype In MapData"              , &gStreamingIteratorTest_FindArchetypeInMapData[0], sizeof(gStreamingIteratorTest_FindArchetypeInMapData), false);
		bk.AddSeparator();
		bk.AddToggle("Check Cable Drawables"                  , &gStreamingIteratorTest_CheckCableDrawables);
		bk.AddToggle("Check Cable Drawables Simple"           , &gStreamingIteratorTest_CheckCableDrawablesSimple);
		bk.AddSeparator();
		bk.AddToggle("Map Data Load Exterior"                 , &gStreamingIterator_LoadMapDataExterior);
		bk.AddToggle("Map Data Load Interior"                 , &gStreamingIterator_LoadMapDataInterior);
		bk.AddToggle("Map Data Load Non-HD"                   , &gStreamingIterator_LoadMapDataNonHD);
		bk.AddToggle("Map Data Count Instances"               , &gStreamingIteratorTest_MapDataCountInstances);
		bk.AddToggle("Map Data Create Entities (BROKEN)"      , &gStreamingIteratorTest_MapDataCreateEntities);
		bk.AddToggle("Map Data Process Parents"               , &gStreamingIteratorTest_MapDataProcessParents);
		bk.AddToggle("Map Data Show All Entities"             , &gStreamingIteratorTest_MapDataShowAllEntities);
		bk.AddToggle("Map Data Remove Entities (TODO FIXME)"  , &gStreamingIteratorTest_MapDataRemoveEntities);
		bk.AddToggle("Map Data Dump Timecycle Modifier Boxes" , &gStreamingIteratorTest_MapDataDumpTCModBoxes);

		bk.AddSeparator();
		bk.AddToggle("Streaming Iterator (TxdStore)"          , &g_si_TxdStore.m_enabled);
		bk.AddSlider("Streaming Iterator (TxdStore)"          , &g_si_TxdStore.m_currentSlot, -1, g_TxdStore.GetSize(), 1);
		bk.AddToggle("Streaming Iterator (DwdStore)"          , &g_si_DwdStore.m_enabled);
		bk.AddSlider("Streaming Iterator (DwdStore)"          , &g_si_DwdStore.m_currentSlot, -1, g_DwdStore.GetSize(), 1);
		bk.AddToggle("Streaming Iterator (DrawableStore)"     , &g_si_DrawableStore.m_enabled);
		bk.AddSlider("Streaming Iterator (DrawableStore)"     , &g_si_DrawableStore.m_currentSlot, -1, g_DrawableStore.GetSize(), 1);
		bk.AddToggle("Streaming Iterator (FragmentStore)"     , &g_si_FragmentStore.m_enabled);
		bk.AddSlider("Streaming Iterator (FragmentStore)"     , &g_si_FragmentStore.m_currentSlot, -1, g_FragmentStore.GetSize(), 1);
		bk.AddToggle("Streaming Iterator (ParticleStore)"     , &g_si_ParticleStore.m_enabled);
		bk.AddSlider("Streaming Iterator (ParticleStore)"     , &g_si_ParticleStore.m_currentSlot, -1, g_ParticleStore.GetSize(), 1);
		bk.AddToggle("Streaming Iterator (MapDataStore)"      , &g_si_MapDataStore.m_enabled);
		bk.AddSlider("Streaming Iterator (MapDataStore)"      , &g_si_MapDataStore.m_currentSlot, -1, g_MapDataStore.GetSize(), 1);
#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TXDCHILDREN
		bk.AddSeparator();
		bk.AddToggle("Build Txd Children Hierarchy"           , &gStreamingIteratorTest_TxdChildren);
		bk.AddButton("Streaming Iterator PrintTxdChildren"    , &_CDTVStreamingIteratorTest_TxdChildrenMap_Print);
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TXDCHILDREN
#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_REDUNDANT
		bk.AddSeparator();
		bk.AddToggle("Find Redundant Textures in Parent Txds" , &gStreamingIteratorTest_FindRedundantTextures);
		bk.AddToggle("Find Redundant Textures by Hash (slow)" , &gStreamingIteratorTest_FindRedundantTexturesByHash);
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_REDUNDANT
#if DEBUG_TEXTURE_VIEWER_FIND_INEFFECTIVE_TEXTURES
		bk.AddSeparator();
		bk.AddToggle("Find Ineffective Textures - BumpTex"                , &gStreamingIteratorTest_FindIneffectiveTexturesBumpTex);
		bk.AddToggle("Find Ineffective Textures - SpecularTex"            , &gStreamingIteratorTest_FindIneffectiveTexturesSpecularTex);
		bk.AddToggle("Find Ineffective Textures - Check Flat"             , &gStreamingIteratorTest_FindIneffectiveTexturesCheckFlat);
		bk.AddToggle("Find Ineffective Textures - Check Flat > 4x4 (slow)", &gStreamingIteratorTest_FindIneffectiveTexturesCheckFlatGT4x4);
		bk.AddToggle("Find Non-Default Specular Masks"                    , &gStreamingIteratorTest_FindNonDefaultSpecularMasks);
#endif // DEBUG_TEXTURE_VIEWER_FIND_INEFFECTIVE_TEXTURES
		bk.AddSeparator();
		bk.AddToggle("Build Asset Analysis Data"               , &gStreamingIteratorTest_BuildAssetAnalysisData);
#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS
		bk.AddSeparator();
		bk.AddToggle("Build Texture Usage Map"                 , &gStreamingIteratorTest_BuildTextureUsageMap);
		bk.AddSlider("Build Texture Usage Map - Phase"         , &gStreamingIteratorTest_BuildTextureUsageMapPhase, 0, 1, 1);
		bk.AddSlider("Build Texture Usage Map - Num Lines"     , &gStreamingIteratorTest_BuildTextureUsageMapTextureHashNumLines, -1, 16, 1);
		bk.AddText  (                        "- Shader Filter" , &gStreamingIteratorTest_BuildTextureUsageMapShaderFilter[0], sizeof(gStreamingIteratorTest_BuildTextureUsageMapShaderFilter), false);
		bk.AddText  (                        "- Texture Filter", &gStreamingIteratorTest_BuildTextureUsageMapTextureFilter[0], sizeof(gStreamingIteratorTest_BuildTextureUsageMapTextureFilter), false);
		bk.AddToggle("Build Texture Usage Map - DAT Output"    , &gStreamingIteratorTest_BuildTextureUsageMapDATOutput);
		bk.AddToggle("Build Texture Usage Map - DAT Flush"     , &gStreamingIteratorTest_BuildTextureUsageMapDATFlush);
		bk.AddToggle("Build Texture Usage Map - CSV Output"    , &gStreamingIteratorTest_BuildTextureUsageMapCSVOutput);
		bk.AddToggle("Build Texture Usage Map - CSV Flush"     , &gStreamingIteratorTest_BuildTextureUsageMapCSVFlush);
		bk.AddToggle("Build Texture Usage Map - Runtime Test"  , &gStreamingIteratorTest_BuildTextureUsageMapRuntimeTest);
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS
#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_FIND_UNUSED
		bk.AddSeparator();
		bk.AddToggle("Find Unused Shared Textures", &gStreamingIteratorTest_FindUnusedSharedTextures);
		bk.AddSlider(".. Memory Used (MB)"        , &gStreamingIteratorTest_FindUnusedSharedTexturesMemoryUsedMB      , 0.0f, 4096.0f, 0.0f);
		bk.AddSlider(".. Memory Unused (MB)"      , &gStreamingIteratorTest_FindUnusedSharedTexturesMemoryUnusedMB    , 0.0f, 4096.0f, 0.0f);
		bk.AddSlider(".. Memory Total (MB)"       , &gStreamingIteratorTest_FindUnusedSharedTexturesMemoryTotalMB     , 0.0f, 4096.0f, 0.0f);
		bk.AddSlider(".. Memory Not Counted (MB)" , &gStreamingIteratorTest_FindUnusedSharedTexturesMemoryNotCountedMB, 0.0f, 4096.0f, 0.0f);
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_FIND_UNUSED
#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_FIND_UNUSED_DRAWABLES_IN_DWDS
		bk.AddSeparator();
		bk.AddToggle("Find Unused Drawables in Dwds", &gStreamingIteratorTest_FindUnusedDrawablesInDwds);
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_FIND_UNUSED_DRAWABLES_IN_DWDS
#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEXTURE
		bk.AddSeparator();
		bk.AddToggle("Streaming Iterator Texture - Find Bad Normal Maps"                   , &gStreamingIteratorTest_FindBadNormalMaps);
		bk.AddToggle("Streaming Iterator Texture - Find Bad Normal Maps - Check Avg (slow)", &gStreamingIteratorTest_FindBadNormalMapsCheckAvgColour);
		bk.AddToggle("Streaming Iterator Texture - Find Unused Texture Channels (slow)"    , &gStreamingIteratorTest_FindUnusedChannels);
		bk.AddToggle("Streaming Iterator Texture - Find Unused Texture Channels - Swizzle" , &gStreamingIteratorTest_FindUnusedChannelsSwizzle);
		bk.AddToggle("Streaming Iterator Texture - Find Uncompressed Textures"             , &gStreamingIteratorTest_FindUncompressedTextures);
		bk.AddToggle("Streaming Iterator Texture - Find Unprocessed Textures"              , &gStreamingIteratorTest_FindUnprocessedTextures);
		bk.AddToggle("Streaming Iterator Texture - Find Constant Textures"                 , &gStreamingIteratorTest_FindConstantTextures);
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEXTURE
		bk.AddButton("Streaming Iterators Reset", &StreamingIteratorsReset);
		bk.AddButton("Entity Iterator", &_EntityIterator);
		bk.AddButton("Scan Models BS", &_ScanModelsForBullshit);
	}
	bk.PopGroup();
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEST

#if DEBUG_TEXTURE_VIEWER_GETPIXELTEST
	bk.PushGroup("Get Pixel Test", false);
	{
		bk.AddToggle("Enabled"     , &m_getPixelTest);
		bk.AddToggle("Use Old Code", &m_getPixelTestUseOldCode);
		bk.AddToggle("Show Alpha"  , &m_getPixelTestShowAlpha);
		bk.AddSlider("Mip Index"   , &m_getPixelMipIndex, 0, 12, 1);
		bk.AddSlider("X"           , &m_getPixelTestX, 0, 256, 1);
		bk.AddSlider("Y"           , &m_getPixelTestY, 0, 256, 1);
		bk.AddSlider("W"           , &m_getPixelTestW, 0, 256, 1);
		bk.AddSlider("H"           , &m_getPixelTestH, 0, 256, 1);
		bk.AddSlider("Size"        , &m_getPixelTestSize, 1.0f/4.0f, 16.0f, 1.0f/64.0f);

		bk.AddSeparator();

		bk.AddToggle("Selected Pixel Enabled", &m_getPixelTestSelectedPixelEnabled);
	}
	bk.PopGroup();
#endif // DEBUG_TEXTURE_VIEWER_GETPIXELTEST

#if DEBUG_TEXTURE_VIEWER_DUMP_TEXTURE
	bk.PushGroup("Dump Texture", false);
	{
		bk.AddText  ("Dump Texture Dir", &m_dumpTextureDir[0], sizeof(m_dumpTextureDir), false);
		bk.AddButton("Dump Texture"    , datCallback(MFA(CDTVStateCommon::StartDumpTexture), this));
		bk.AddButton("Dump Texture Raw", datCallback(MFA(CDTVStateCommon::StartDumpTextureRaw), this));
		bk.AddButton("Dump Txd Raw"    , datCallback(MFA(CDTVStateCommon::StartDumpTxdRaw), this));
	}
	bk.PopGroup();
#endif // DEBUG_TEXTURE_VIEWER_DUMP_TEXTURE

	bk.PushGroup("Utilities", false);
	{
#if DEBUG_TEXTURE_VIEWER_OVERRIDE_REFLECTION_TEX
		const char* overrideReflectionTexStrings[] =
		{
			"None",
			"Current Texture",
			"White",
			"Black",
		};
		bk.AddCombo("Override Reflection Tex", &m_overrideReflectionTex, NELEM(overrideReflectionTexStrings), overrideReflectionTexStrings);
		bk.AddCombo("Override Reflection Cube Tex", &m_overrideReflectionCubeTex, NELEM(overrideReflectionTexStrings), overrideReflectionTexStrings);
#endif // DEBUG_TEXTURE_VIEWER_OVERRIDE_REFLECTION_TEX

		static char s_dumpListToCSVPath[80] = "assets:/non_final/texture_viewer_list.csv";

		class DumpWindowListToCSV_button { public: static void func()
		{
			gDebugTextureViewer.DebugWindow_DumpListToCSV(s_dumpListToCSVPath);
		}};

		bk.AddText  ("Dump List Path", &s_dumpListToCSVPath[0], sizeof(s_dumpListToCSVPath), false);
		bk.AddButton("Dump List To CSV", DumpWindowListToCSV_button::func);

		class CreateDebugArchetypeProxies_button { public: static void func()
		{
			const int n = CDebugArchetype::GetMaxDebugArchetypeProxies();
			Displayf("%d.", n);
		}};

		bk.AddButton("Create Debug Archetype Proxies", CreateDebugArchetypeProxies_button::func);

#if DEBUG_TEXTURE_VIEWER_INVERT_TEXTURE_CHANNELS
		bk.AddTitle("Invert Texture");
		bk.AddSlider("Mip Count"                      , &m_invertTextureMipCount, 0, 12, 1);
		bk.AddToggle("Red Channel"                    , &m_invertTextureChannelR);
		bk.AddToggle("Green Channel"                  , &m_invertTextureChannelG);
		bk.AddToggle("Blue Channel"                   , &m_invertTextureChannelB);
		bk.AddToggle("Alpha Channel"                  , &m_invertTextureChannelA);
		bk.AddToggle("Reverse Bits"                   , &m_invertTextureReverseBits);
		bk.AddToggle("Invert Tex Verbose"             , &m_invertTextureVerbose);
		bk.AddButton("Invert Texture"                 , InvertTextureChannels_button<false>);
		bk.AddButton("Invert Mip"                     , InvertTextureChannels_button<true>);
		bk.AddColor ("Solid Texture Colour"           , &m_solidTextureColour);
		bk.AddButton("Set Solid Texture Colour"       , SetSolidTextureColour_button<false>);
		bk.AddButton("Set Solid Mip Colour"           , SetSolidTextureColour_button<true>);
		bk.AddButton("Reverse Bits for Error Textures", ReverseBitsForAllErrorTextures_button); // for testing results of ReportUnusedTextures
		bk.AddButton("Dump Fast DXT Histogram (TEST)" , DumpFastDXTHistogram_button);
		bk.AddSeparator();
#endif // DEBUG_TEXTURE_VIEWER_INVERT_TEXTURE_CHANNELS
		bk.AddSlider("Dump Entity Search Radius", &g_DumpEntityPhaseVisMasksSearchRadius, 0.0f, 16000.0f, 1.0f);
		bk.AddButton("Dump Entity Phase Vis Masks", DumpEntityPhaseVisMasks_button);
#if __DEV && EFFECT_PRESERVE_STRINGS
		bk.AddButton("Dump Shader Vars For Model", DumpShaderVarsForModel_button);
		bk.AddButton("Dump Geometries For Model", DumpGeometriesForModel_button);
		bk.AddButton("Search Models (TEMPORARY HACK)", SearchModelsTempHack_button);
#endif // __DEV && EFFECT_PRESERVE_STRINGS
#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEST
		bk.AddButton("Find Shaders Using This Texture ..."     , FindShadersUsingThisTexture_button<false>);
		bk.AddButton("Find Shaders Using This Texture Name ...", FindShadersUsingThisTexture_button<true>);
		bk.AddButton("Find Shaders Using Textures (All)"       , FindShadersUsingTextures_button<false>);
		bk.AddButton("Find Textures Used By This Drawable ..." , FindTexturesUsedByThisDrawable_button);
		bk.AddButton("Find Dependent Assets ..."               , FindDependentAssets_button<false,false,false>);
		bk.AddButton("Find Dependent Assets and Archetypes ...", FindDependentAssets_button<false,true,false>);
		bk.AddButton("Find Dependent Assets Immediate ..."     , FindDependentAssets_button<false,true,true>);
		bk.AddButton("Find All Unreferenced Assets"            , FindAllUnreferencedAssets_button);
		bk.AddButton("Find All Dummy Txds"                     , FindAllDummyTxds_button);
		bk.AddButton("Report Unused Textures ..."              , ReportUnusedTextures_button<false>);
		bk.AddButton("Report Unused Textures (Auto-Load)"      , ReportUnusedTextures_button<true>);
		bk.AddText  ("Dump Duplicate Textures Path"            , &gDumpDuplicateTexturesPath[0], sizeof(gDumpDuplicateTexturesPath), false);
		bk.AddButton("Dump Duplicate Textures"                 , DumpDuplicateTextures_button);
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEST
	}
	bk.PopGroup();

#if DEBUG_TEXTURE_VIEWER_FRAMEBUFFER_MAGNIFIER
	bk.PushGroup("Magnifier", false);
	{
		/*const char* magnifierBufferNames[] =
		{
			"Frontbuffer",
#if __PS3 || __WIN32PC
			"Backbuffer",
#endif // __PS3 || __WIN32PC
			"Albedo (GBuffer[0] RGB)",
			"Normals (GBuffer[1] RGB)",
			"Specular (GBuffer[2] RGB)",
			"Shadows (GBuffer[2] ALPHA)",
			"Ambient (GBuffer[3] RGB)",
			"Depth",
		};
		CompileTimeAssert(NELEM(magnifierBufferNames) == CDTVMagnifier::MAGNIFIER_BUFFER_COUNT);*/

		bk.AddToggle("Magnifier Enabled"           , &m_magnifierEnabled, CDTVMagnifier::EnabledCallback);
		bk.AddToggle("Magnifier Pixel Picker"      , &m_magnifierPixelPicker, CDTVMagnifier::EnabledCallback);
	//	bk.AddCombo ("Magnifier Buffer"            , &m_magnifierBuffer, NELEM(magnifierBufferNames), magnifierBufferNames);
		bk.AddSlider("Magnifier Res"               , &m_magnifierRes, 6, 24, 1);
		bk.AddSlider("Magnifier Zoom"              , &m_magnifierZoom, 4, 16, 1);
		bk.AddSlider("Magnifier Offset X"          , &m_magnifierOffsetX, 0, 1280, 1);
		bk.AddSlider("Magnifier Offset Y"          , &m_magnifierOffsetY, 0, 720, 1);
	//	bk.AddToggle("Magnifier Attached to Mouse" , &m_magnifierAttachedToMouse); -- this doesn't really work
	}
	bk.PopGroup();
#endif // DEBUG_TEXTURE_VIEWER_FRAMEBUFFER_MAGNIFIER

#if DEBUG_TEXTURE_VIEWER_ENTITY_FLAGS
	bk.PushGroup("Entity Flags", false);
	{
		bk.AddTitle("Entity Vis Phase Mask");

		bk.AddToggle("VIS_PHASE_MASK_OTHER"                , &m_entityVisPhaseFlags, VIS_PHASE_MASK_OTHER                );
		bk.AddToggle("VIS_PHASE_MASK_GBUF"                 , &m_entityVisPhaseFlags, VIS_PHASE_MASK_GBUF                 );
		bk.AddToggle("VIS_PHASE_MASK_CASCADE_SHADOWS"      , &m_entityVisPhaseFlags, VIS_PHASE_MASK_CASCADE_SHADOWS      );
		bk.AddToggle("VIS_PHASE_MASK_PARABOLOID_SHADOWS"   , &m_entityVisPhaseFlags, VIS_PHASE_MASK_PARABOLOID_SHADOWS   );
		bk.AddToggle("VIS_PHASE_MASK_PARABOLOID_REFLECTION", &m_entityVisPhaseFlags, VIS_PHASE_MASK_PARABOLOID_REFLECTION);
		bk.AddToggle("VIS_PHASE_MASK_WATER_REFLECTION"     , &m_entityVisPhaseFlags, VIS_PHASE_MASK_WATER_REFLECTION     );
		bk.AddToggle("VIS_PHASE_MASK_MIRROR_REFLECTION"    , &m_entityVisPhaseFlags, VIS_PHASE_MASK_MIRROR_REFLECTION    );
		bk.AddToggle("VIS_PHASE_MASK_RAIN_COLLISION_MAP"   , &m_entityVisPhaseFlags, VIS_PHASE_MASK_RAIN_COLLISION_MAP   );
#if RDR_VERSION // TEMP 
		bk.AddToggle("VIS_PHASE_MASK_BOUNCE_MAP"           , &m_entityVisPhaseFlags, VIS_PHASE_MASK_BOUNCE_MAP           );
#endif
		bk.AddToggle("VIS_PHASE_MASK_SEETHROUGH_MAP"       , &m_entityVisPhaseFlags, VIS_PHASE_MASK_SEETHROUGH_MAP       );
		bk.AddToggle("VIS_PHASE_MASK_STREAMING"            , &m_entityVisPhaseFlags, VIS_PHASE_MASK_STREAMING            );
		bk.AddButton("<< GET ENTITY VIS PHASE FLAGS", EntityFlags_GetEntityVisPhaseFlags_button);
		bk.AddButton(">> SET ENTITY VIS PHASE FLAGS", EntityFlags_SetEntityVisPhaseFlags_button);

		bk.AddTitle("Entity Flags");

		bk.AddToggle("IS_VISIBLE         ", &m_entityFlags, fwEntity::IS_VISIBLE         );
		bk.AddToggle("IS_SEARCHABLE      ", &m_entityFlags, fwEntity::IS_SEARCHABLE      );
		bk.AddToggle("HAS_ALPHA          ", &m_entityFlags, fwEntity::HAS_ALPHA          );
		bk.AddToggle("HAS_DECAL          ", &m_entityFlags, fwEntity::HAS_DECAL          );
		bk.AddToggle("HAS_CUTOUT         ", &m_entityFlags, fwEntity::HAS_CUTOUT         );
		bk.AddToggle("HAS_WATER          ", &m_entityFlags, fwEntity::HAS_WATER          );
		bk.AddToggle("HAS_DISPL_ALPHA    ", &m_entityFlags, fwEntity::HAS_DISPL_ALPHA    );
		bk.AddToggle("HAS_PRERENDER      ", &m_entityFlags, fwEntity::HAS_PRERENDER      );
		bk.AddToggle("IS_LIGHT           ", &m_entityFlags, fwEntity::IS_LIGHT           );
		bk.AddToggle("USE_SCREENDOOR     ", &m_entityFlags, fwEntity::USE_SCREENDOOR     );
		bk.AddToggle("SUPPRESS_ALPHA     ", &m_entityFlags, fwEntity::SUPPRESS_ALPHA     );
		bk.AddToggle("RENDER_SMALL_SHADOW", &m_entityFlags, fwEntity::RENDER_SMALL_SHADOW);
		bk.AddToggle("OK_TO_PRERENDER    ", &m_entityFlags, fwEntity::OK_TO_PRERENDER    );
		bk.AddToggle("IS_DYNAMIC         ", &m_entityFlags, fwEntity::IS_DYNAMIC         );
		bk.AddToggle("IS_FIXED           ", &m_entityFlags, fwEntity::IS_FIXED           );
		bk.AddToggle("IS_FIXED_BY_NETWORK", &m_entityFlags, fwEntity::IS_FIXED_BY_NETWORK);
		bk.AddToggle("SPAWN_PHYS_ACTIVE  ", &m_entityFlags, fwEntity::SPAWN_PHYS_ACTIVE  );
		bk.AddToggle("DONT_STREAM        ", &m_entityFlags, fwEntity::DONT_STREAM        );
		bk.AddToggle("LOW_PRIORITY_STREAM", &m_entityFlags, fwEntity::LOW_PRIORITY_STREAM);
		bk.AddToggle("HAS_PHYSICS_DICT   ", &m_entityFlags, fwEntity::HAS_PHYSICS_DICT   );
		bk.AddToggle("REMOVE_FROM_WORLD  ", &m_entityFlags, fwEntity::REMOVE_FROM_WORLD  );
		bk.AddToggle("FORCED_ZERO_ALPHA",	&m_entityFlags, fwEntity::FORCED_ZERO_ALPHA  );
		bk.AddToggle("DRAW_LAST          ", &m_entityFlags, fwEntity::DRAW_LAST          );
		bk.AddToggle("DRAW_FIRST_SORTED  ", &m_entityFlags, fwEntity::DRAW_FIRST_SORTED  );
		bk.AddToggle("NO_INSTANCED_PHYS  ", &m_entityFlags, fwEntity::NO_INSTANCED_PHYS  );
		bk.AddToggle("HAS_HD_TEX_DIST    ", &m_entityFlags, fwEntity::HAS_HD_TEX_DIST    );
		bk.AddButton("<< GET ENTITY FLAGS", EntityFlags_GetEntityFlags_button);
		bk.AddButton(">> SET ENTITY FLAGS", EntityFlags_SetEntityFlags_button);
	}
	bk.PopGroup();
#endif // DEBUG_TEXTURE_VIEWER_ENTITY_FLAGS

#if __BANK_TEXTURE_CONTROL
	gTextureControl.AddWidgets(bk);
#endif // __BANK_TEXTURE_CONTROL

#if DEBUG_TEXTURE_VIEWER_ADVANCED
	bk.PushGroup("Advanced", false);
	{
		bk.AddToggle("m_selectModelUseShowTxdFlags", &m_asp.m_selectModelUseShowTxdFlags);

		// these simply call FindAssets_ and display the results
		bk.AddButton("Print Assets (ModelInfo)"    , &_PrintAssets_ModelInfo    );
		bk.AddButton("Print Assets (TxdStore)"     , &_PrintAssets_TxdStore     );
		bk.AddButton("Print Assets (DwdStore)"     , &_PrintAssets_DwdStore     );
		bk.AddButton("Print Assets (DrawableStore)", &_PrintAssets_DrawableStore);
		bk.AddButton("Print Assets (FragmentStore)", &_PrintAssets_FragmentStore);
		bk.AddButton("Print Assets (ParticleStore)", &_PrintAssets_ParticleStore);

		bk.AddSeparator();

		ADD_WIDGET(bk, Slider, m_findAssetsMaxCount, 5, 99999, 1);
		ADD_WIDGET(bk, Slider, m_doubleClickDelay, 0, 1000, 1);
		ADD_WIDGET(bk, Slider, m_doubleClickWait , 0, 1000, 1);
#if DEBUG_TEXTURE_VIEWER_TRACKASSETHASH
		ADD_WIDGET(bk, Toggle, m_trackAssetHash);
#endif // DEBUG_TEXTURE_VIEWER_TRACKASSETHASH

		bk.AddSeparator();

		ADD_WIDGET(bk, Button, PrintTextureDictionaries   );
		ADD_WIDGET(bk, Button, PrintTextureDictionarySizes);
		ADD_WIDGET(bk, Button, PrintModelInfoStuff        );
		ADD_WIDGET(bk, Button, _PrintTxdChildren          );

		bk.AddSeparator();

#if DEBUG_TEXTURE_VIEWER_UIGADGET_DEMO
		ADD_WIDGET(bk, Button, UiGadgetDemo);
#endif // DEBUG_TEXTURE_VIEWER_UIGADGET_DEMO
	}
	bk.PopGroup();
#endif // DEBUG_TEXTURE_VIEWER_ADVANCED

#if __DEV
	bk.AddButton("Print Model Textures", &_PrintModelTextures);
#endif // __DEV

#if DEBUG_TEXTURE_VIEWER_MESH_DUMP_TEST
	bk.AddButton("Print Entity Mesh Data", &_PrintEntityMeshData);
#endif // DEBUG_TEXTURE_VIEWER_MESH_DUMP_TEST

#if defined(DEBUG_MTHRANDOM)
	bk.AddToggle("mthRandom Debug Num Calls", &g_DrawRand.m_DebugNumCallsEnabled);
#endif // defined(DEBUG_MTHRANDOM)
}

static void ShowHoverInfo(const char* info, int& line, Vector2& pos, const Color32& col = CRGBA_White())
{
	if (info[0] != '\0')
	{
		grcDebugDraw::TextFontPush(gDebugTextureViewer.m_hoverTextUseMiniFont ? grcSetup::GetMiniFixedWidthFont() : grcSetup::GetFixedWidthFont());
		grcDebugDraw::Text(pos, DD_ePCS_Pixels, col, info, true, 1.0f, 1.0f);
		grcDebugDraw::TextFontPop();
	}

	if (++line == 1)
	{
		pos.x += 20.0f; // to the right of the mouse cursor
	}

	pos.y += 9.0f; // offset by font height + 1
}

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS_AUTO

enum eStreamingIteratorAutoPhase
{
	SI_AUTO_PHASE_NONE,
	SI_AUTO_PHASE_STARTING,
	SI_AUTO_PHASE_RUNNING,
	SI_AUTO_PHASE_DONE,
	SI_AUTO_PHASE_COUNT
};

static eStreamingIteratorAutoPhase g_si_autoPhase = SI_AUTO_PHASE_NONE;
PARAM(si_auto,"si_auto"); // also run with -rag, -texlite and -debugstart=0,0,75,0,-1,0,0,1,0,0,0,0,0,1,1000,18.000000

extern void siLogOpen(const char* path);
extern void siLog(const char* format, ...);

#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS_AUTO

void CDebugTextureViewer::Update()
{
#if defined(DEBUG_MTHRANDOM)
	if (g_DrawRand.m_DebugNumCallsEnabled)
	{
		grcDebugDraw::AddDebugOutput("g_DrawRand num calls = %d", g_DrawRand.m_DebugNumCalls);
		g_DrawRand.m_DebugNumCalls = 0;
	}
#endif // defined(DEBUG_MTHRANDOM)

	// catch deferred update
	{
		CDebugTextureViewerInterface::SelectModelInfo(0, NULL, false, false, true);
		CDebugTextureViewerInterface::SelectTxd(CTxdRef(), false, false, false, true);
		CDebugTextureViewerInterface::SelectPreviewTexture(NULL, NULL, false, true);
	}

#if USE_FLIPBOOK_ANIM_TEST
	UpdateFlipbookAnim(m_previewTextureLayer);
#endif // USE_FLIPBOOK_ANIM_TEST

#if DEBUG_TEXTURE_VIEWER_TRACKASSETHASH
	CAssetStoreHashCheck::Update(m_trackAssetHash);
#endif // DEBUG_TEXTURE_VIEWER_TRACKASSETHASH

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEST
#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS_AUTO
	// auto reset
	if (gStreamingIteratorTest_BuildAssetAnalysisData &&
		AssetAnalysis::IsWritingStreams_DEPRECATED())
	{
		if (g_si_TxdStore     .m_currentSlot <= 0 &&
			g_si_DwdStore     .m_currentSlot <= 0 &&
			g_si_DrawableStore.m_currentSlot <= 0 &&
			g_si_FragmentStore.m_currentSlot <= 0 &&
			g_si_ParticleStore.m_currentSlot <= 0 &&
			g_si_MapDataStore .m_currentSlot <= 0)
		{
			StreamingIteratorsReset();
		}
	}

	if (PARAM_si_auto.Get())
	{
		const char* logFileName = "texture_usage";
		static u32 autoTimeStart = 0;

		if (g_si_autoPhase == SI_AUTO_PHASE_NONE)
		{
			if (autoTimeStart == 0)
			{
				autoTimeStart = fwTimer::GetSystemTimeInMilliseconds();

				char dateTimeStr[256] = "";
#if __PS3
				CellRtcDateTime clk;
				cellRtcGetCurrentClockLocalTime(&clk);
				sprintf(dateTimeStr, "%04d/%02d/%02d %02d:%02d:%02d", clk.year, clk.month, clk.day, clk.hour, clk.minute, clk.second);
#elif __XENON
				SYSTEMTIME clk;
				GetLocalTime(&clk);
				sprintf(dateTimeStr, "%04d/%02d/%02d", clk.wYear, clk.wMonth, clk.wDay);
#endif // __XENON

				siLogOpen(logFileName);
				siLog(">> STREAMING ITERATOR AUTO START AT %s (built on %s)", dateTimeStr, __DATE__);
			}
			else if (fwTimer::GetSystemTimeInMilliseconds() - autoTimeStart > 1000*30) // delay 30 seconds to allow streaming system to "catch up"
			{
				g_si_autoPhase = SI_AUTO_PHASE_STARTING;
			}
		}
		else if (g_si_autoPhase == SI_AUTO_PHASE_STARTING)
		{
			fwTimer::StartUserPause();

			// turn off these systems because there's some bad txd's that they create, which cause g_TxdStore.GetName() to return NULL ...
			//CGameStreamMgr::GetGameStream()->EnableFeedType(GAMESTREAM_TYPE_DLC, false);
			//CGameStreamMgr::GetGameStream()->EnableFeedType(GAMESTREAM_TYPE_SOCIALCLUB, false);

			const char* si_rpf = NULL;

			if (PARAM_si_auto.Get(si_rpf))
			{
				if (si_rpf && si_rpf[0] != '\0')
				{
					strcpy(gStreamingIteratorTest_SearchRPF, si_rpf);
				}
			}

			siLog(">> STREAMING ITERATOR AUTO PHASE RUNNING (logging to \"%s\")", logFileName);

			g_si_autoPhase = SI_AUTO_PHASE_RUNNING;

			// enable all the iterators (except the map data store)
			g_si_TxdStore     .m_enabled = true;
			g_si_DwdStore     .m_enabled = true;
			g_si_DrawableStore.m_enabled = true;
			g_si_FragmentStore.m_enabled = true;
			g_si_ParticleStore.m_enabled = true;

			gStreamingIteratorTest_BuildTextureUsageMap          = true;
			gStreamingIteratorTest_BuildTextureUsageMapDATOutput = true;
		}
		else if (g_si_autoPhase == SI_AUTO_PHASE_RUNNING)
		{
			if (g_si_TxdStore     .m_currentSlot <= 0 &&
				g_si_DwdStore     .m_currentSlot <= 0 &&
				g_si_DrawableStore.m_currentSlot <= 0 &&
				g_si_FragmentStore.m_currentSlot <= 0 &&
				g_si_ParticleStore.m_currentSlot <= 0)
			{
				siLog(">> STREAMING ITERATOR AUTO PHASE DONE (%.2f mins)", (float)(fwTimer::GetSystemTimeInMilliseconds() - autoTimeStart)/(1000.0f*60.0f));

				StreamingIteratorsReset();

				g_si_autoPhase = SI_AUTO_PHASE_DONE;

				g_si_TxdStore     .m_enabled = false;
				g_si_DwdStore     .m_enabled = false;
				g_si_DrawableStore.m_enabled = false;
				g_si_FragmentStore.m_enabled = false;
				g_si_ParticleStore.m_enabled = false;

				gStreamingIteratorTest_BuildTextureUsageMap          = false;
				gStreamingIteratorTest_BuildTextureUsageMapDATOutput = false;

				fwTimer::EndUserPause();
			}
		}
	}
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS_AUTO
	g_si_TxdStore     .Update();
	g_si_DwdStore     .Update();
	g_si_DrawableStore.Update();
	g_si_FragmentStore.Update();
	g_si_ParticleStore.Update();
	g_si_MapDataStore .Update(3, 4); // having problems with N4rage17fwMatrixTransformE pool, so i'm reducing pressure on this iterator
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEST

#if DEBUG_TEXTURE_VIEWER_INVERT_TEXTURE_CHANNELS
	//if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_I, KEYBOARD_MODE_DEBUG_ALT))
	//{
	//	InvertTextureChannels_button();
	//}
#endif // DEBUG_TEXTURE_VIEWER_INVERT_TEXTURE_CHANNELS

#if DEBUG_TEXTURE_VIEWER_FRAMEBUFFER_MAGNIFIER
	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_M, KEYBOARD_MODE_DEBUG_ALT))
	{
		m_magnifierEnabled = !m_magnifierEnabled;
		CDTVMagnifier::EnabledCallback();
	}

	if (m_magnifierEnabled && CDTVMagnifier::m_mouseX != -1)
	{
		const float offsetX = (float)(m_magnifierAttachedToMouse ? (CDTVMagnifier::m_mouseX - (m_magnifierRes*m_magnifierZoom)/2) : m_magnifierOffsetX);
		const float offsetY = (float)(m_magnifierAttachedToMouse ? (CDTVMagnifier::m_mouseY - (m_magnifierRes*m_magnifierZoom)/2) : m_magnifierOffsetY);

		grcDebugDraw::RectAxisAligned(
			Vec2V(
				((float)(CDTVMagnifier::m_mouseX - m_magnifierRes/2) - 1.5f)/(float)GRCDEVICE.GetWidth(),
				((float)(CDTVMagnifier::m_mouseY - m_magnifierRes/2) - 1.5f)/(float)GRCDEVICE.GetHeight()
			),
			Vec2V(
				((float)(CDTVMagnifier::m_mouseX + m_magnifierRes/2) + 1.5f)/(float)GRCDEVICE.GetWidth(),
				((float)(CDTVMagnifier::m_mouseY + m_magnifierRes/2) + 1.5f)/(float)GRCDEVICE.GetHeight()
			),
			Color32(255,0,0,255),
			false
		);

		const Vec2V magnifierViewMin(
			(offsetX - 1.5f)/(float)GRCDEVICE.GetWidth(),
			(offsetY - 1.5f)/(float)GRCDEVICE.GetHeight()
		);
		const Vec2V magnifierViewMax(
			(offsetX + 1.5f + (float)(m_magnifierRes*m_magnifierZoom))/(float)GRCDEVICE.GetWidth(),
			(offsetY + 1.5f + (float)(m_magnifierRes*m_magnifierZoom))/(float)GRCDEVICE.GetHeight()
		);

		grcDebugDraw::RectAxisAligned(magnifierViewMin, magnifierViewMax, Color32(255,0,0,255), false);

		const Vec2V mousePos(
			(float)ioMouse::GetX()/(float)GRCDEVICE.GetWidth(),
			(float)ioMouse::GetY()/(float)GRCDEVICE.GetHeight()
		);

		Vec2V   pickedPixelMin(V_ZERO);
		Vec2V   pickedPixelMax(V_ZERO);
		Color32 pickedPixel(0);
		bool    pickedPixelValid = false;
		Vec4V   sumColour(V_ZERO);

		for (int y = 0; y < m_magnifierRes; y++)
		{
			for (int x = 0; x < m_magnifierRes; x++)
			{
				const Vec2V pixelMin(
					(offsetX + (float)((x + 0)*m_magnifierZoom))/(float)GRCDEVICE.GetWidth(),
					(offsetY + (float)((y + 0)*m_magnifierZoom))/(float)GRCDEVICE.GetHeight()
				);
				const Vec2V pixelMax(
					(offsetX + (float)((x + 1)*m_magnifierZoom))/(float)GRCDEVICE.GetWidth(),
					(offsetY + (float)((y + 1)*m_magnifierZoom))/(float)GRCDEVICE.GetHeight()
				);

				grcDebugDraw::RectAxisAligned(pixelMin, pixelMax, CDTVMagnifier::m_pixels[x + y*24], true);

				if (IsGreaterThanOrEqualAll(mousePos, pixelMin) &
					IsGreaterThanOrEqualAll(pixelMax, mousePos))
				{
					pickedPixelMin   = pixelMin - Vec2V(ScalarV(1.0f/(float)GRCDEVICE.GetWidth()));
					pickedPixelMax   = pixelMax + Vec2V(ScalarV(1.0f/(float)GRCDEVICE.GetWidth()));
					pickedPixel      = CDTVMagnifier::m_pixels[x + y*24];
					pickedPixelValid = true;
				}

				sumColour += CDTVMagnifier::m_pixels[x + y*24].GetRGBA();
			}
		}

		if (m_magnifierPixelPicker)
		{
#if MOUSE_RENDERING_SUPPORT
			if (ioMouse::GetPressedButtons() & ioMouse::MOUSE_RIGHT)
			{
				grcSetup::m_DisableMousePointerRefCount = 1; // hide mouse cursor while dragging
			}
			else if (ioMouse::GetReleasedButtons() & ioMouse::MOUSE_RIGHT)
			{
				grcSetup::m_DisableMousePointerRefCount = 0;
			}
#endif

			const Color32 avgPixel(sumColour/ScalarV((float)(m_magnifierRes*m_magnifierRes)));

			const float textPosX0 = magnifierViewMin.GetXf();
			const float textPosY0 = magnifierViewMax.GetYf() + 10.0f/(float)GRCDEVICE.GetHeight();
			const float textPosX1 = textPosX0;
			const float textPosY1 = magnifierViewMax.GetYf() + 21.0f/(float)GRCDEVICE.GetHeight();
			const float textPosX2 = textPosX0;
			const float textPosY2 = magnifierViewMax.GetYf() + 32.0f/(float)GRCDEVICE.GetHeight();

			grcDebugDraw::RectAxisAligned(pickedPixelMin, pickedPixelMax, Color32(255,255,255,255), false);
			grcDebugDraw::Text(Vec2V(textPosX0, textPosY0), DD_ePCS_NormalisedZeroToOne, Color32(255,255,255,255), atVarString("rgb=%d,%d,%d", pickedPixel.GetRed(), pickedPixel.GetGreen(), pickedPixel.GetBlue()).c_str(), true);
			grcDebugDraw::Text(Vec2V(textPosX1, textPosY1), DD_ePCS_NormalisedZeroToOne, Color32(255,255,255,255), atVarString("avg=%d,%d,%d", avgPixel.GetRed(), avgPixel.GetGreen(), avgPixel.GetBlue()).c_str(), true);

			if (DebugDeferred::m_GBufferIndex == 2) // normal
			{
				const float nx = -1.0f + 2.0f*(float)pickedPixel.GetRed  ()/255.0f;
				const float ny = -1.0f + 2.0f*(float)pickedPixel.GetGreen()/255.0f;
				const float nz = -1.0f + 2.0f*(float)pickedPixel.GetBlue ()/255.0f;

				const Vec3V n = Normalize(Vec3V(nx, ny, nz));
				const float m = Mag(Vec3V(nx, ny, nz)).Getf();

				grcDebugDraw::Text(Vec2V(textPosX2, textPosY2), DD_ePCS_NormalisedZeroToOne, Color32(255,255,255,255), atVarString("normal=%.4f,%.4f,%.4f, 1-mag=%f", VEC3V_ARGS(n), 1.0f - m).c_str(), true);
			}
			else if (DebugDeferred::m_GBufferIndex == 3) // specular
			{
				class UnpackSpecFromGloss { public: static float func(float x)
				{
					// these must match the tcp which created the gloss map
					const float specExpMin = 1.0f;
					const float specExpMax = 8192.0f;
					return specExpMin*pow(specExpMax/specExpMin, Clamp<float>(x, 0.0f, 1.0f));
				}};

				class UnpackSpecOld { public: static float func(float x)
				{
					x *= 512.0f;

					// Adjust range (0..500 -> 0..1500 and 500..512 -> 1500..8196)
					const float expandRange = Max<float>(0.0f, x - 500.0f);
					return (x - expandRange) * 3.0f + expandRange * 558.0f;
				}};

				const float g = (float)pickedPixel.GetGreen()/255.0f;
				const float k0 = UnpackSpecOld::func(g);
				const float k1 = UnpackSpecFromGloss::func(g);

				const float g_m1 = (float)(Max<int>(0, pickedPixel.GetGreen() - 1))/255.0f;
				const float k0_m1 = UnpackSpecOld::func(g_m1);
				const float k1_m1 = UnpackSpecFromGloss::func(g_m1);
				const float g_p1 = (float)(Min<int>(255, pickedPixel.GetGreen() + 1))/255.0f;
				const float k0_p1 = UnpackSpecOld::func(g_p1);
				const float k1_p1 = UnpackSpecFromGloss::func(g_p1);
				const float err0 = Max<float>(k0 - k0_m1, k0_p1 - k0);
				const float err1 = Max<float>(k1 - k1_m1, k1_p1 - k1);

				grcDebugDraw::Text(Vec2V(textPosX2, textPosY2), DD_ePCS_NormalisedZeroToOne, Color32(255,255,255,255), atVarString("specexp=%.4f/%.4f, err=+/-%f/%f", k0, k1, err0, err1).c_str(), true);
			}
		}
	}
#endif // DEBUG_TEXTURE_VIEWER_FRAMEBUFFER_MAGNIFIER

#if DEBUG_TEXTURE_VIEWER_EDIT_METADATA_IN_WORKBENCH
	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_W, KEYBOARD_MODE_DEBUG_SHIFT))
	{
		Edit_MetadataInWorkbench();
	}
#endif // DEBUG_TEXTURE_VIEWER_EDIT_METADATA_IN_WORKBENCH

	// I was having rag crash after loading the sandbox txd several times (> 5 times), which Ian
	// thinks might be due to loading in the callback as opposed to the update. So I've hacked
	// it so that the callback simply sets a flag which gets picked up in the update (here), and
	// this seems to fix the problem. I'm doing the same thing for the preview texture ..
	{
		LoadPreviewTexture_Update();
		LoadSandboxTxd_Update();
	}

	bool bClick = false;

	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_T, KEYBOARD_MODE_DEBUG_ALT))
	{
		m_enablePicking = !m_enablePicking;

		if (m_enablePicking)
		{
			bClick = true; // pseudoclick
		}
		else
		{
			Close(false);
			return;
		}
	}

	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_H, KEYBOARD_MODE_DEBUG_ALT))
	{
		if (m_hoverEntityShaderMode != 5)
		{
#if ENTITYSELECT_ENABLED_BUILD
			CEntitySelect::SetEntitySelectPassEnabled();
#endif // ENTITYSELECT_ENABLED_BUILD
			m_hoverEntityShaderMode = 5;
		}
		else
		{
			m_hoverEntityShaderMode = 0;
		}
	}

	CDTVState& state = GetCurrentUpdateState();

	state.m_selectedModelInfo  = NULL; // will be set later if the window is active etc.
	state.m_selectedShaderPtr  = 0;
	state.m_selectedShaderName = "";

	Vector2          hoverInfoPos = Vector2((float)ioMouse::GetX(), (float)(ioMouse::GetY() - 8));
	int              hoverInfoLine = 0;
	CEntity*         hoverEntity = NULL;
	entitySelectID   hoverEntityID = 0;
	int              hoverEntityShaderIndex = INDEX_NONE;
	const grmShader* hoverEntityShader = NULL;
	const char*      hoverEntityShaderName = "";

	if (m_enablePicking)
	{
		if (ioMouse::GetPressedButtons() & ioMouse::MOUSE_RIGHT)
		{
			bClick = true; // right-click
		}
		else if (ioMouse::GetPressedButtons() & ioMouse::MOUSE_LEFT)
		{
			if (m_debugWindow)
			{
				const fwBox2D windowBounds = m_debugWindow->GetWindowBounds();
				const float mouseX = (float)ioMouse::GetX();
				const float mouseY = (float)ioMouse::GetY();

				if (mouseX < windowBounds.x0 || mouseY < windowBounds.y0 ||
					mouseX > windowBounds.x1 || mouseY > windowBounds.y1)
				{
					bClick = true; // left-click outside of window
				}
			}
			else
			{
				bClick = true; // left-click and there is no window currently
			}
		}

#if DEBUG_TEXTURE_VIEWER_SHADER_EDIT_INTEGRATION
		if (m_windowTitle.length() > 0 && stristr(m_windowTitle.c_str(), "SHADER EDIT"))
		{
			bClick = false; // don't handle clicking when viewing shader edit textures, we don't want to window to get reset
		}
#endif // DEBUG_TEXTURE_VIEWER_SHADER_EDIT_INTEGRATION

		if (bClick && !CDebugTextureViewerWindowMgr::GetLeftMouseButtonHandled())
		{
			Populate_SearchModelClick();
		}
	}

	if (m_enablePicking || m_hoverEntityShaderMode > 0)
	{
#if ENTITYSELECT_ENABLED_BUILD
		hoverEntity            = (CEntity*)CEntitySelect::GetSelectedEntity();
		hoverEntityID          =           CEntitySelect::GetSelectedEntityId();
		hoverEntityShaderIndex =           CEntitySelect::GetSelectedEntityShaderIndex();
		hoverEntityShader      =           CEntitySelect::GetSelectedEntityShader();
		hoverEntityShaderName  =           CEntitySelect::GetSelectedEntityShaderName();
#endif // ENTITYSELECT_ENABLED_BUILD

		if (hoverEntity)
		{
#if RDR_VERSION // TEMP
			atString hoverArchetypePath;

			if (CDebugArchetype::HaveDebugArchetypeProxiesBeenCreated())
			{
				hoverArchetypePath = AssetAnalysis::GetArchetypePath(CDebugArchetype::GetDebugArchetypeProxyForModelInfo(hoverEntity->GetBaseModelInfo()));
			}
			else
			{
				hoverArchetypePath = hoverEntity->GetModelName();
			}

			ShowHoverInfo(atVarString("%s (entityID=0x%08x)", hoverArchetypePath.c_str(), hoverEntityID).c_str(), hoverInfoLine, hoverInfoPos);
			ShowHoverInfo(AssetAnalysis::GetDrawablePath(hoverEntity->GetBaseModelInfo()).c_str(), hoverInfoLine, hoverInfoPos);
#else
			ShowHoverInfo(atVarString("%s (entityID=0x%08" SIZETFMT "x)", hoverEntity->GetModelName(), hoverEntityID).c_str(), hoverInfoLine, hoverInfoPos);
#endif
			hoverInfoPos.y += 1.0f;

			// show interior location
			{
				const fwInteriorLocation location = hoverEntity->GetInteriorLocation();

				if (location.IsValid())
				{
					CInteriorProxy* pInteriorProxy = CInteriorProxy::GetFromLocation(location);
					Assert(pInteriorProxy);
					CInteriorInst* pInteriorInst = pInteriorProxy->GetInteriorInst();
					CMloModelInfo* pMloModelInfo = static_cast<CMloModelInfo*>(pInteriorInst->GetBaseModelInfo());

					if (location.IsAttachedToRoom())
					{
						ShowHoverInfo(atVarString("interior: %s (%s id=%d)", pMloModelInfo->GetModelName(), pMloModelInfo->GetRoomName(location), location.GetRoomIndex()), hoverInfoLine, hoverInfoPos);
						hoverInfoPos.y += 1.0f;
					}
					else if (location.IsAttachedToPortal())
					{
						ShowHoverInfo(atVarString("interior: %s (PORTAL id=%d)", pMloModelInfo->GetModelName(), location.GetPortalIndex()), hoverInfoLine, hoverInfoPos);
						hoverInfoPos.y += 1.0f;
					}
				}
			}

			if (hoverEntityShaderName && hoverEntityShaderName[0] != '\0') // show shader
			{
				ShowHoverInfo(atVarString("shader=%s (%d)", hoverEntityShaderName, hoverEntityShaderIndex), hoverInfoLine, hoverInfoPos);
			}
		}
		else
		{
			ShowHoverInfo(atVarString("NULL (entityID=0x%08" SIZETFMT "x)", hoverEntityID), hoverInfoLine, hoverInfoPos);
			hoverInfoPos.y += 1.0f;
		}

#if DEBUG_TEXTURE_VIEWER_GET_WORLD_POS_UNDER_MOUSE
		if (m_showWorldPosUnderMouse)
		{
			const float px = CDTVGetWorldPosUnderMouse::m_worldPos.GetXf();
			const float py = CDTVGetWorldPosUnderMouse::m_worldPos.GetYf();
			const float pz = CDTVGetWorldPosUnderMouse::m_worldPos.GetZf();
			const float pd = CDTVGetWorldPosUnderMouse::m_worldPos.GetWf();
			const int   ps = CDTVGetWorldPosUnderMouse::m_stencil;
			const float pi = CDTVGetWorldPosUnderMouse::m_worldNormal.GetXf();
			const float pj = CDTVGetWorldPosUnderMouse::m_worldNormal.GetYf();
			const float pk = CDTVGetWorldPosUnderMouse::m_worldNormal.GetZf();

			ShowHoverInfo(atVarString("wpos=%.3f,%.3f,%.3f", px, py, pz), hoverInfoLine, hoverInfoPos);
			ShowHoverInfo(atVarString("norm=%.3f,%.3f,%.3f", pi, pj, pk), hoverInfoLine, hoverInfoPos);
			ShowHoverInfo(atVarString("mouse=%d,%d", CDTVGetWorldPosUnderMouse::m_mouseX, CDTVGetWorldPosUnderMouse::m_mouseY), hoverInfoLine, hoverInfoPos);
			ShowHoverInfo(atVarString("depth=%f", pd), hoverInfoLine, hoverInfoPos);
			ShowHoverInfo(atVarString("stencil=0x%02X", ps), hoverInfoLine, hoverInfoPos);

			hoverInfoPos.y += 1.0f;
		}
#endif // DEBUG_TEXTURE_VIEWER_GET_WORLD_POS_UNDER_MOUSE

		if (hoverEntity != NULL && m_hoverEntityShaderMode > 0 && !hoverEntity->GetIsTypePed())
		{
			atArray<const grmShader*> shaders;
			GetShadersUsedByEntity(shaders, hoverEntity, m_hoverEntityShaderMode == 1); // mode 1 = unique names only

			// TODO -- most of this functionality belongs in DebugTextureViewerUtil.cpp
			for (int i = 0; i < shaders.size(); i++)
			{
				if ((hoverEntityShader == NULL || hoverEntityShader == shaders[i]) && m_hoverEntityShaderMode >= 2) // mode 2 = show hover shader
				{
					const Color32 colour(255,255,255,255); // TODO -- grcDrawLabel enforces background colour is inverted text colour, need to force background colour to black
					//const Color32 colour(128,128,255,255);

					if (m_hoverEntityShaderMode < 4 || hoverEntityShader == NULL) // mode 4+ = show hover shader only
					{
						ShowHoverInfo(shaders[i]->GetName(), hoverInfoLine, hoverInfoPos, colour);
					}

#if EFFECT_PRESERVE_STRINGS
					if (m_hoverEntityShaderMode >= 3) // mode 3 = expand hover shader vars
					{
						hoverInfoPos.x += 5.0f;

						bool bIneffectiveBumpTex = false;
						bool bIneffectiveSpecTex = false;

						if (1)
						{
							const float threshold = 0.02f;

							float bumpiness = 1.0f;
							float specIntensity = 1.0f;
							Vec3V specIntensityMask = Vec3V(V_X_AXIS_WZERO);

							for (int j = 0; j < shaders[i]->GetVarCount(); j++)
							{
								const grcEffectVar var = shaders[i]->GetVarByIndex(j);
								const char* name = NULL;
								grcEffect::VarType type;
								int annotationCount = 0;
								bool isGlobal = false;

								shaders[i]->GetVarDesc(var, name, type, annotationCount, isGlobal);

								if (!isGlobal)
								{
									if (type == grcEffect::VT_FLOAT && stricmp(name, "Bumpiness") == 0)
									{
										shaders[i]->GetVar(var, bumpiness);
									}
									else if (type == grcEffect::VT_FLOAT && stricmp(name, "SpecularColor") == 0)
									{
										shaders[i]->GetVar(var, specIntensity);
									}
									else if (type == grcEffect::VT_VECTOR3 && stricmp(name, "SpecularMapIntensityMask") == 0)
									{
										shaders[i]->GetVar(var, RC_VECTOR3(specIntensityMask));
									}
								}
							}

							if (bumpiness <= threshold)
							{
								bIneffectiveBumpTex = true;
							}

							if (specIntensity*MaxElement(specIntensityMask).Getf() <= threshold)
							{
								bIneffectiveSpecTex = true;
							}
						}

						for (int j = 0; j < shaders[i]->GetVarCount(); j++)
						{
							const grcEffectVar var = shaders[i]->GetVarByIndex(j);
							const char* name = NULL;
							grcEffect::VarType type;
							int annotationCount = 0;
							bool isGlobal = false;

							shaders[i]->GetVarDesc(var, name, type, annotationCount, isGlobal);

							if (!isGlobal)
							{
								if (type == grcEffect::VT_TEXTURE)
								{
									grcTexture* texture = NULL;
									shaders[i]->GetVar(var, texture);

									if (texture)
									{
										grcTexture::eTextureSwizzle swizzle[4];
										texture->GetTextureSwizzle(swizzle[0], swizzle[1], swizzle[2], swizzle[3]);
										const char* swizzleTable = "RGBA01";

										bool bIneffective = false;
										bool bUnusedAlpha = false;

										if (stricmp(name, "BumpTex"    ) == 0 ||
											stricmp(name, "SpecularTex") == 0)
										{
											if      (bIneffectiveBumpTex && stricmp(name, "BumpTex"    ) == 0) { bIneffective = true; }
											else if (bIneffectiveSpecTex && stricmp(name, "SpecularTex") == 0) { bIneffective = true; }

											if (texture->GetImageFormat() == grcImage::DXT3 ||
												texture->GetImageFormat() == grcImage::DXT5)
											{
												if (swizzle[0] != grcTexture::TEXTURE_SWIZZLE_A && // maybe it's a "high quality" texture
													swizzle[1] != grcTexture::TEXTURE_SWIZZLE_A &&
													swizzle[2] != grcTexture::TEXTURE_SWIZZLE_A)
												{
													bUnusedAlpha = true;
												}
											}
										}

										ShowHoverInfo(
											atVarString(
												"%s=%s (%s-%c%c%c%c %dx%d mips=%d)",
												name,
												GetFriendlyTextureName(texture).c_str(),
												grcImage::GetFormatString((grcImage::Format)texture->GetImageFormat()),
												swizzleTable[swizzle[0]],
												swizzleTable[swizzle[1]],
												swizzleTable[swizzle[2]],
												swizzleTable[swizzle[3]],
												texture->GetWidth(),
												texture->GetHeight(),
												texture->GetMipMapCount()
											).c_str(),
											hoverInfoLine,
											hoverInfoPos,
											bIneffective ? Color32(255,0,0,255) : bUnusedAlpha ? Color32(220,255,0,255) : colour
										);

										if (m_hoverEntityShaderMode >= 5) // mode 5+ = show texture path
										{
											const CBaseModelInfo* hoverModel = hoverEntity->GetBaseModelInfo();

											if (hoverModel)
											{
												const atString path = AssetAnalysis::GetTexturePath(hoverModel, texture);

												if (path.length() > 0)
												{
													ShowHoverInfo(atVarString("  %s", path.c_str()).c_str(), hoverInfoLine, hoverInfoPos, colour);
												}
											}
										}
									}
									else
									{
										ShowHoverInfo(atVarString("%s=NULL", name).c_str(), hoverInfoLine, hoverInfoPos, colour);
									}
								}
								else if (type == grcEffect::VT_FLOAT)
								{
									float f = 0.0f;
									shaders[i]->GetVar(var, f);

									if (stricmp(name, "HardAlphaBlend") != 0 || f != 1.0f)
									{
										ShowHoverInfo(atVarString("%s=%f", name, f).c_str(), hoverInfoLine, hoverInfoPos, colour);
									}
								}
								else if (type == grcEffect::VT_VECTOR2)
								{
									Vector2 v(0.0f, 0.0f);
									shaders[i]->GetVar(var, v);

									if (stricmp(name, "gShadowRes") != 0) // isn't this global?
									{
										ShowHoverInfo(atVarString("%s=%f,%f", name, v.x, v.y).c_str(), hoverInfoLine, hoverInfoPos, colour);
									}
								}
								else if (type == grcEffect::VT_VECTOR3)
								{
									Vector3 v(0.0f, 0.0f, 0.0f);
									shaders[i]->GetVar(var, v);

									if ((stricmp(name, "globalAnimUV0") != 0 || IsLessThanOrEqualAll(Abs(RCC_VEC3V(v) - Vec3V(V_X_AXIS_WZERO)), Vec3V(V_FLT_SMALL_4)) == 0) &&
										(stricmp(name, "globalAnimUV1") != 0 || IsLessThanOrEqualAll(Abs(RCC_VEC3V(v) - Vec3V(V_Y_AXIS_WZERO)), Vec3V(V_FLT_SMALL_4)) == 0))
									{
										ShowHoverInfo(atVarString("%s=%f,%f,%f", name, v.x, v.y, v.z).c_str(), hoverInfoLine, hoverInfoPos, colour);
									}
								}
								else if (type == grcEffect::VT_VECTOR4)
								{
									Vector4 v(0.0f, 0.0f, 0.0f, 0.0f);
									shaders[i]->GetVar(var, v);

									if (stricmp(name, "MaterialColorScale") != 0 || IsEqualAll(RCC_VEC4V(v), Vec4V(V_X_AXIS_WONE)) == 0)
									{
										ShowHoverInfo(atVarString("%s=%f,%f,%f,%f", name, v.x, v.y, v.z, v.w).c_str(), hoverInfoLine, hoverInfoPos, colour);
									}
								}
							}
						}

						hoverInfoPos.x -= 5.0f;
					}
#endif // EFFECT_PRESERVE_STRINGS
				}
				else if (m_hoverEntityShaderMode < 4) // mode 4+ = show hover shader only
				{
					ShowHoverInfo(shaders[i]->GetName(), hoverInfoLine, hoverInfoPos);
				}
			}

			if (shaders.size() > 0)
			{
				hoverInfoPos.y += 1.0f;
			}
		}
	}

	if (m_debugWindow)
	{
		const fwTxd*      previewTxd     = NULL;
		const grcTexture* previewTexture = NULL;
		bool              bSelect        = false;

		CDTVEntry* selectedEntry = GetSelectedListEntry();

		if (m_visible)
		{
			if (selectedEntry)
			{
				// update selected model
				{
					const CEntity* entity = selectedEntry->GetEntity();

					if (entity)
					{
						state.m_selectedModelInfo = entity->GetBaseModelInfo();
					}
					else
					{
						state.m_selectedModelInfo = selectedEntry->GetModelInfo();
					}
				}

				// update selected shader
				{
					char shaderName[256] = "";
					strcpy(shaderName, selectedEntry->GetShaderVarName());

					if (strchr(shaderName, '/'))
					{
						strchr(shaderName, '/')[0] = '\0';
					}

					state.m_selectedShaderPtr  = selectedEntry->GetShaderPtr();
					state.m_selectedShaderName = shaderName;
				}

				if (m_debugWindow->GetIsActive())
				{
					// flash selected entity
					//{
					//	const CEntity* entity = selectedEntry->GetEntity();
					//	if (entity)
					//	{
					//		entity->SetAlpha                ((u8)(m_entityAOScale*255.0f + 0.5f));
					//		entity->SetAmbientOcclusionScale((u8)(m_entityAOScale*255.0f + 0.5f));
					//		entity->SetEmissiveBounceScale  ((u8)(m_entityEBScale*255.0f + 0.5f));
					//	}
					//}

					if (ioMapper::DebugKeyPressed(KEY_RETURN))
					{
						bSelect = true;
					}

					// handle double-click
					{
						static u32 s_lastSingleClickTime = 0;
						static u32 s_lastDoubleClickTime = 0;

						if (ioMouse::GetPressedButtons() & ioMouse::MOUSE_LEFT) // handle double-click
						{
							u32 singleClickTime = fwTimer::GetSystemTimeInMilliseconds();

							if (singleClickTime < s_lastSingleClickTime + m_doubleClickDelay)
							{
								u32 doubleClickTime = singleClickTime;

								if (doubleClickTime < s_lastDoubleClickTime + m_doubleClickWait)
								{
									// absorb it
									singleClickTime = 0;
									doubleClickTime = 0;
								}
								else
								{
									bSelect = true;
								}

								s_lastDoubleClickTime = doubleClickTime;
							}

							s_lastSingleClickTime = singleClickTime;
						}
					}

					if (m_showDesc)
					{
						const Vector2 showDescOrigin(48.0f, 48.0f);
						const bool bLameTextEffect = true;

						grcDebugDraw::TextFontPush(grcSetup::GetMiniFixedWidthFont());

						if (bLameTextEffect) // lame way to darken the edges of the text .. quite slow
						{
							for (int dy = -1; dy <= 1; dy++)
							{
								for (int dx = -1; dx <= 1; dx++)
								{
									if (dx|dy)
									{
										grcDebugDraw::Text(showDescOrigin + Vector2((float)dx, (float)dy), DD_ePCS_Pixels, Color32(0,0,0,128), selectedEntry->GetDesc().c_str(), false, 1.0f, 1.0f);
									}
								}
							}
						}

						grcDebugDraw::Text(showDescOrigin, DD_ePCS_Pixels, CRGBA_White(), selectedEntry->GetDesc().c_str(), !bLameTextEffect, 1.0f, 1.0f);
						grcDebugDraw::TextFontPop();
					}

					if (m_showRefs)
					{
						const Vector2 showDescOrigin(48.0f, 48.0f);

						grcDebugDraw::TextFontPush(grcSetup::GetMiniFixedWidthFont());

						for (int i = 0; i < m_assetRefs.size(); i++)
						{
							const atVarString temp("%02d - %s", m_assetRefs[i].GetRefCount(), m_assetRefs[i].GetDesc().c_str());
							const Color32 colour = i < m_listState.back().m_assetRefOffset ? Color32(255,255,0,255) : Color32(255,0,0,255);

							grcDebugDraw::Text(showDescOrigin + Vector2(801.0f, (float)(i*8 + 1)), DD_ePCS_Pixels, CRGBA_Black(), temp, false, 1.0f, 1.0f);
							grcDebugDraw::Text(showDescOrigin + Vector2(800.0f, (float)(i*8 + 0)), DD_ePCS_Pixels, colour       , temp, false, 1.0f, 1.0f);
						}

						grcDebugDraw::TextFontPop();
					}
				}

				previewTxd     = selectedEntry->GetPreviewTxd    ();
				previewTexture = selectedEntry->GetPreviewTexture();
			}

			if (m_previewTexture) // override with explicit texture
			{
				previewTexture = m_previewTexture;
			}
		}

		if (m_visible && m_debugWindow->GetIsActive()) // joypad
		{
			CPad* pad = CControlMgr::IsDebugPadOn() ? &CControlMgr::GetDebugPad() : CControlMgr::GetPlayerPad();

			if (pad)
			{
				if (pad->RightShoulder1JustDown() != 0)
				{
					bSelect = true;
				}
				else if (pad->GetLeftShoulder1() == 0) // only if L1 is *not* held down, otherwise we might accidentally mess up stuff while autoscrolling
				{
					if (pad->GetRightShoulder2() != 0)
					{
						const int enumCount = DebugTextureViewerDTQ::eDTQVM_COUNT;

						if (pad->DPadLeftJustDown ()) { m_previewTextureViewMode = ((m_previewTextureViewMode - 1 + enumCount)%enumCount); }
						if (pad->DPadRightJustDown()) { m_previewTextureViewMode = ((m_previewTextureViewMode + 1            )%enumCount); }
					}
					else // mip index
					{
						const int numMips = previewTexture ? previewTexture->GetMipMapCount() : 16;

						if (pad->DPadLeftJustDown ()) { m_previewTextureLOD = ((m_previewTextureLOD - 1 + numMips)%numMips); }
						if (pad->DPadRightJustDown()) { m_previewTextureLOD = ((m_previewTextureLOD + 1          )%numMips); }
					}
				}
			}

			if (selectedEntry && pad->ButtonTriangleJustDown())
			{
				selectedEntry->Load(CAssetRefInterface(m_assetRefs, m_verbose));
			}
			else if (selectedEntry && pad->ButtonSquareJustDown())
			{
				selectedEntry->SetActiveFlag(!selectedEntry->GetActiveFlag());
			}
			else if (pad->LeftShoulder1JustDown())
			{
				bSelect = false; // hopefully they're not button-mashing, but who knows
				Back();

				if (m_debugWindow == NULL) // pressing back when there is only one list state will kill the window (i think this is ok?)
				{
#if DEBUG_TEXTURE_VIEWER_DISABLE_PLAYER
					CGameWorld::GetMainPlayerInfo()->EnableControlsDebug();
#endif // DEBUG_TEXTURE_VIEWER_DISABLE_PLAYER
					return;
				}
			}
		}

		if (bSelect && selectedEntry)
		{
			selectedEntry->Select(CAssetRefInterface(m_assetRefs, m_verbose));
		}

		// update debug window
		{
			m_debugWindow->SetIsVisible(m_visible);
			m_debugWindow->SetNumSlots (m_windowNumListSlots);
			m_debugWindow->SetScale    (m_windowScale);
			m_debugWindow->SetBorder   (m_windowBorder);
			m_debugWindow->SetSpacing  (m_windowSpacing);
			m_debugWindow->SetTitle    (atVarString("%s (%d/%d)", m_windowTitle.c_str(), m_debugWindow->GetCurrentIndex() + 1, GetListNumEntries()));

			if (GetListNumEntries() > 0) // technically we could do this only when the column flags change ..
			{
				GetListEntry(0)->SetupWindow(m_debugWindow, m_columnFlags);
			}

			if (GetListNumEntries() == 0 || GetListEntry(0)->IsPreviewRelevant())
			{
				m_debugWindow->SetPreviewWidth((float)(m_windowNumListSlots + 1)*CDebugTextureViewerWindow::GetDefaultListEntryHeight()); // make it square
			}
			else
			{
				m_debugWindow->SetPreviewWidth(0.0f);
			}
		}

#if DEBUG_TEXTURE_VIEWER_DISABLE_PLAYER
		if (m_visible)
		{
			CGameWorld::GetMainPlayerInfo()->DisableControlsDebug();
		}
		else
		{
			CGameWorld::GetMainPlayerInfo()->EnableControlsDebug();
		}
#endif // DEBUG_TEXTURE_VIEWER_DISABLE_PLAYER

		state.m_verbose        = m_verbose;
		state.m_previewTxd     = previewTxd;
		state.m_previewTexture = previewTexture;

#if DEBUG_TEXTURE_VIEWER_SELECTPREVIEW
		if (state.m_previewTxd == NULL && m_previewTxd && m_debugWindow->GetCurrentIndex() == INDEX_NONE)
		{
			state.m_previewTxd     = m_previewTxd;
			state.m_previewTexture = NULL;
		}
#endif // DEBUG_TEXTURE_VIEWER_SELECTPREVIEW

		state.UpdateCommon(this);

#if DEBUG_TEXTURE_VIEWER_GETPIXELTEST
		state.m_getPixelTestMousePosX = ioMouse::GetX();
		state.m_getPixelTestMousePosY = ioMouse::GetY();
#endif // DEBUG_TEXTURE_VIEWER_GETPIXELTEST

#if DEBUG_TEXTURE_VIEWER_DUMP_TEXTURE
		m_dumpTexture = false; // one-shot
		m_dumpTextureRaw = false; // one-shot
		m_dumpTxdRaw = false; // one-shot
#endif // DEBUG_TEXTURE_VIEWER_DUMP_TEXTURE

		const grcTexture* texture = state.m_previewTexture;

		if (m_previewTextureCubeReflectionMap)
		{
			texture = CRenderPhaseReflection::ms_renderTargetCube;
		}

		if (texture)
		{
			const float w = (float)texture->GetWidth();
			const float h = (float)texture->GetHeight();

			state.m_previewSrcBounds = fwBox2D(0.0f, 0.0f, w, h);
			state.m_previewDstBounds = m_debugWindow->GetPreviewBounds();

			state.m_previewSrcBounds.x0 += m_previewTextureOffsetX;
			state.m_previewSrcBounds.y0 += m_previewTextureOffsetY;
			state.m_previewSrcBounds.x1 += m_previewTextureOffsetX;
			state.m_previewSrcBounds.y1 += m_previewTextureOffsetY;

			grcImage::ImageType imageType = grcImage::STANDARD;

			if (dynamic_cast<const grcRenderTarget*>(texture))
			{
				if (texture == CRenderPhaseReflection::ms_renderTargetCube)
				{
					imageType = grcImage::CUBE;
				}
				else
				{
					// TODO -- support render targets
				}
			}
			else
			{
#if RSG_ORBIS
				imageType = (grcImage::ImageType)static_cast<const grcTextureGNM*>(texture)->GetImageType();
#elif RSG_DURANGO
				imageType = (grcImage::ImageType)static_cast<const grcTextureDurango*>(texture)->GetImageType();
#elif RSG_PC
				imageType = (grcImage::ImageType)static_cast<const grcTextureDX11*>(texture)->GetImageType();
#endif
			}

			if (!m_previewTextureStretchToFit || imageType == grcImage::CUBE)
			{
				float zoomX = m_previewTextureZoom;
				float zoomY = m_previewTextureZoom;

				if (imageType == grcImage::CUBE && m_previewTextureCubeProjection != DebugTextureViewerDTQ::eDTQCP_Faces)
				{
					zoomX *= 4.0f;
					zoomY *= 4.0f;

					if (m_previewTextureCubeProjection == DebugTextureViewerDTQ::eDTQCP_Panorama ||
						m_previewTextureCubeProjection == DebugTextureViewerDTQ::eDTQCP_Paraboloid ||
						m_previewTextureCubeProjection == DebugTextureViewerDTQ::eDTQCP_ParaboloidSquare)
					{
						zoomY *= 0.5f;
					}
					else if (m_previewTextureCubeProjection == DebugTextureViewerDTQ::eDTQCP_4x3Cross)
					{
						zoomY *= 3.0f/4.0f;
					}
					else if (m_previewTextureCubeProjection == DebugTextureViewerDTQ::eDTQCP_Cylinder)
					{
						zoomY /= m_previewTextureCubeCylinderAspect;
					}

					if (texture == CRenderPhaseReflection::ms_renderTargetCube)
					{
						const float maxZoomOver = Max<float>(zoomX*w/(float)(GRCDEVICE.GetWidth() - 200), zoomY*h/(float)(GRCDEVICE.GetHeight() - 100));

						if (maxZoomOver > 1.0f)
						{
							zoomX /= maxZoomOver;
							zoomY /= maxZoomOver;
						}
					}
				}

				state.m_previewDstBounds.x1 = state.m_previewDstBounds.x0 + zoomX*w;
				state.m_previewDstBounds.y1 = state.m_previewDstBounds.y0 + zoomY*h;
			}
			else
			if (m_previewTextureZoom != 0.0f && // not particularly efficient code, this is written for clarity ..
				m_previewTextureZoom != 1.0f)
			{
				const float ooz = 1.0f/m_previewTextureZoom;

				const float cx = (state.m_previewSrcBounds.x1 + state.m_previewSrcBounds.x0)*0.5f;
				const float cy = (state.m_previewSrcBounds.y1 + state.m_previewSrcBounds.y0)*0.5f;
				const float ex = (state.m_previewSrcBounds.x1 - state.m_previewSrcBounds.x0)*0.5f;
				const float ey = (state.m_previewSrcBounds.y1 - state.m_previewSrcBounds.y0)*0.5f;

				state.m_previewSrcBounds.x0 = cx - ooz*ex;
				state.m_previewSrcBounds.y0 = cy - ooz*ey;
				state.m_previewSrcBounds.x1 = cx + ooz*ex;
				state.m_previewSrcBounds.y1 = cy + ooz*ey;
			}
		}
		else if (state.m_previewTxd)
		{
			state.m_previewSrcBounds       = fwBox2D(0.0f, 0.0f, 0.0f, 0.0f);
			state.m_previewDstBounds       = m_debugWindow->GetPreviewBounds();
			state.m_previewTextureShowMips = false; // this would cause issues with preview txd ..
		}
	}
}

CDTVState& CDebugTextureViewer::GetCurrentUpdateState()
{
	return m_state[m_stateIndex];
}

CDTVState& CDebugTextureViewer::GetCurrentRenderState()
{
	return m_state[m_stateIndex ^ 1];
}

void CDebugTextureViewer::Render()
{
	GetCurrentRenderState().Render();
}

void CDebugTextureViewer::Synchronise()
{
	GetCurrentRenderState().Cleanup();

	if (m_debugWindow && m_debugWindow->GetIsVisible())
	{
		// ok
	}
	else
	{
#if DEBUG_TEXTURE_VIEWER_SELECTPREVIEW
		GetCurrentUpdateState().m_previewTxd     = NULL;
#endif // DEBUG_TEXTURE_VIEWER_SELECTPREVIEW
		GetCurrentUpdateState().m_previewTexture = NULL;
	}

	m_stateIndex ^= 1;
}


// ================================================================================================
// ================================================================================================
// ================================================================================================


//
//
//
//
class COverlayImageViewer
{
public:
	COverlayImageViewer();
	~COverlayImageViewer();

	void InitWidgets(bkBank& bk);

	void Update();
	void Render();
	void Synchronise();


	static void LoadPreviewTexture();
	static void LoadPreviewTexture_Reload();
	static void LoadPreviewTexture_Update();
	static void LoadPreviewTexture_Unload();

private:
	pgRef<const grcTexture>		m_previewTexture;
	char						m_previewTextureName[80];

	Color32						m_renderColor;		// rgba

	Vector2						m_renderStart;		// xy: <0; 1>
	Vector2						m_renderSize;		// width,height: <0; 1>
	float						m_renderRotation;	// rotation: <0; 359> 
	bool						m_renderFitToScreen;
	bool						m_renderFlipX;
	bool						m_renderFlipY;
};

COverlayImageViewer gOverlayImageViewer;

static grcDepthStencilStateHandle sm_rstateDepth = grcStateBlock::DSS_Invalid;
static grcBlendStateHandle        sm_rstateBlend = grcStateBlock::BS_Invalid;
static grcRasterizerStateHandle   sm_rstateRaster= grcStateBlock::RS_Invalid;

COverlayImageViewer::COverlayImageViewer()
{
	sysMemSet((void*)this, 0x00, sizeof(*this));

	m_previewTexture	= NULL;
	m_renderColor		= Color32(255,255,255,255);
	m_renderStart		= Vector2(0.0f, 0.0f);
	m_renderSize		= Vector2(0.5f, 0.5f);
	m_renderRotation	= 0.0f;
	m_renderFitToScreen = false;
	m_renderFlipX		= false;
	m_renderFlipY		= false;
}

COverlayImageViewer::~COverlayImageViewer()
{
	m_previewTexture = NULL;
}

//
//
//
//
void COverlayImageViewer::InitWidgets(bkBank& bk)
{
	bk.PushGroup("Load Texture", false);
	{
		bk.AddButton("Load Overlay Texture ...", &LoadPreviewTexture);
		bk.AddText("Overlay Texture Name", &m_previewTextureName[0], sizeof(m_previewTextureName), false);
		bk.AddButton("Reload Overlay Texture", &LoadPreviewTexture_Reload);
		bk.AddButton("Unload Overlay Texture", &LoadPreviewTexture_Unload);
	}
	bk.PopGroup();
	
	bk.PushGroup("Overlay Rendering", false);
	{
		bk.AddSlider("Screen Pos",		&m_renderStart,Vector2(-1,-1), Vector2(1,1), Vector2(0.001f, 0.001f)); 
		bk.AddSlider("Size",			&m_renderSize, Vector2(0,0), Vector2(1,1), Vector2(0.001f, 0.001f));
		bk.AddToggle("Fit to Screen",	&m_renderFitToScreen);
		bk.AddToggle("Flip X",			&m_renderFlipX);
		bk.AddToggle("Flip Y",			&m_renderFlipY);
		bk.AddSlider("Rotation",		&m_renderRotation, -360.0f, 360.0f, 0.1f);
		bk.AddColor("Tint/Transparency",&m_renderColor);
	}
	bk.PopGroup();


	// init:
grcDepthStencilStateDesc descD;
grcBlendStateDesc        descB;
grcRasterizerStateDesc   descR;

	descD.DepthEnable		= false;
	descD.DepthWriteMask	= false;
	descD.DepthFunc			= grcRSV::CMP_LESS;

	descB.BlendRTDesc[0].BlendEnable= true;
	descB.BlendRTDesc[0].SrcBlend	= grcRSV::BLEND_SRCALPHA;
	descB.BlendRTDesc[0].DestBlend	= grcRSV::BLEND_INVSRCALPHA;

	descR.CullMode = grcRSV::CULL_NONE;

	sm_rstateDepth = grcStateBlock::CreateDepthStencilState(descD);
	sm_rstateBlend = grcStateBlock::CreateBlendState(descB);
	sm_rstateRaster = grcStateBlock::CreateRasterizerState(descR);
}

//
//
//
//
void COverlayImageViewer::Update()
{
	LoadPreviewTexture_Update();
}

//
//
//
//
void COverlayImageViewer::Render()
{
	const grcTexture* texture = m_previewTexture;

	if(texture)
	{
		grcStateBlock::SetDepthStencilState(sm_rstateDepth);
		grcStateBlock::SetBlendState(sm_rstateBlend);
		grcStateBlock::SetRasterizerState(sm_rstateRaster);


		grcBindTexture(texture);
		
		Vector2 _p0(0, 0);
		Vector2 size(0.5f, 0.5f);

		Vector2 uv0(0.0f, 0.0f);
		Vector2 uv1(1.0f, 0.0f);
		Vector2 uv2(0.0f, 1.0f);
		Vector2 uv3(1.0f, 1.0f);



		if(m_renderFitToScreen)
		{
			m_renderFitToScreen = false;
			m_renderStart = Vector2(0.0f,0.0f);
			m_renderSize = Vector2(1.0f,1.0f);
		}

		if(m_renderFlipX)
		{
			uv0.x = 1.0f;
			uv1.x = 0.0f;
			uv2.x = 1.0f;
			uv3.x = 0.0f;
		}
		else
		{
			uv0.x = 0.0f;
			uv1.x = 1.0f;
			uv2.x = 0.0f;
			uv3.x = 1.0f;
		}

		if(m_renderFlipY)
		{
			uv0.y = 1.0f;
			uv1.y = 1.0f;
			uv2.y = 0.0f;
			uv3.y = 0.0f;
		}
		else
		{
			uv0.y = 0.0f;
			uv1.y = 0.0f;
			uv2.y = 1.0f;
			uv3.y = 1.0f;
		}

		Color32 color(m_renderColor);

		_p0 = m_renderStart;
		size = m_renderSize;

		Vector2 p0 = _p0 + Vector2(0,0);
		Vector2 p1 = _p0 + Vector2(size.x, 0);
		Vector2 p2 = _p0 + Vector2(0, size.y);
		Vector2 p3 = _p0 + Vector2(size.x, size.y);

		if(m_renderRotation != 0.0f)
		{
			float rotation = m_renderRotation;

			while(rotation < 0.0f)
			{
				rotation += 360.0f;
			}

			float aspectRatio = 1.0f;
			float invAspectRatio = 1.0f;

			static bool bFixAspectRatio = true;
			if(bFixAspectRatio)
			{
				aspectRatio		= CHudTools::GetAspectRatio(false);
				invAspectRatio	= 1.0f / aspectRatio;
			}

			const float CentreX = (m_renderStart.x + size.x/2.0f) * aspectRatio;
			const float CentreY = m_renderStart.y + size.y/2.0f;
			const float Width	= (size.x/2.0f) * aspectRatio;
			const float Height	= size.y/2.0f;

			const float rotD = DtoR * rotation;
			const float cos_float = rage::Cosf(rotD);
			const float sin_float = rage::Sinf(rotD);

			const float WidthCos = Width * cos_float;
			const float WidthSin = Width * sin_float;
			const float HeightCos = Height * cos_float;
			const float HeightSin = Height * sin_float;

			p0.Set((CentreX - WidthCos + HeightSin) * invAspectRatio, CentreY - WidthSin - HeightCos);
			p1.Set((CentreX + WidthCos + HeightSin) * invAspectRatio, CentreY + WidthSin - HeightCos);
			p2.Set((CentreX - WidthCos - HeightSin) * invAspectRatio, CentreY - WidthSin + HeightCos);
			p3.Set((CentreX + WidthCos - HeightSin) * invAspectRatio, CentreY + WidthSin + HeightCos);
		}

		CSprite2d::Draw(
			p0, p1, p2, p3,
			uv0, uv1, uv2, uv3,
			color);

		grcBindTexture(NULL);
	}//if(texture)...
}

//
//
//
//
void COverlayImageViewer::Synchronise()
{
	// todo
}



//
//
//
//
void COverlayImageViewer::LoadPreviewTexture()
{
	COverlayImageViewer* _this = &gOverlayImageViewer;

	if (BANKMGR.OpenFile(_this->m_previewTextureName, sizeof(_this->m_previewTextureName), "*.dds", false, "Open DDS"))
	{
		LoadPreviewTexture_Reload();
	}
}

static bool s_LoadOverlayTexture = false;
static bool s_UnloadOverlayTexture = false;

void COverlayImageViewer::LoadPreviewTexture_Reload()
{
	s_LoadOverlayTexture = true;
}

void COverlayImageViewer::LoadPreviewTexture_Unload()
{
	s_UnloadOverlayTexture = true;
}


void COverlayImageViewer::LoadPreviewTexture_Update()
{
	COverlayImageViewer* _this = &gOverlayImageViewer;

	if(s_UnloadOverlayTexture)
	{
		s_UnloadOverlayTexture = false;
		_this->m_previewTexture = NULL;
		_this->m_previewTextureName[0] = 0x00;
		return;
	}

	if(s_LoadOverlayTexture)
	{
		s_LoadOverlayTexture = false;
	}
	else
	{
		return;
	}

	if (strlen(_this->m_previewTextureName) > 0)
	{
		char pathNoExt[RAGE_MAX_PATH];
		strcpy(pathNoExt, _this->m_previewTextureName);
		char* ext = strrchr(pathNoExt, '.');

		if (ext && (stricmp(ext, ".dds") == 0 || stricmp(ext, ".tif") == 0))
		{
			*ext = '\0';
		}

		const bool nopopups = ForceNoPopupsBegin(false);
		grcTexture* texture = NULL;
		grcImage* image = grcImage::Load(_this->m_previewTextureName);

		if(AssertVerify(image))
		{
			grcTextureFactory::TextureCreateParams params(GTA_ONLY(grcTextureFactory::TextureCreateParams::VIDEO, grcTextureFactory::TextureCreateParams::TILED));
			char swizzle[6] = "RGBA\0";
			u8 conversionFlags = 0;
			u16 templateType = 0;
			bool isProcessed = false;
			bool isOptimised = false;
			bool doNotOptimise = true;

			if (!isProcessed && !doNotOptimise)
			{
				isOptimised = fwTextureConversionParams::OptimiseCompressed(image, swizzle, conversionFlags);
			}

			texture = grcTextureFactory::GetInstance().Create(image, &params);

			if (AssertVerify(texture))
			{
				texture->SetConversionFlags(conversionFlags);
				texture->SetTemplateType(templateType);
			}

			image->Release();
		}

		ForceNoPopupsEnd(nopopups);

		if (texture)
		{
			_this->m_previewTexture = texture;
		}
	}
}

// ================================================================================================
// ================================================================================================
// ================================================================================================

__COMMENT(static) void CDebugTextureViewerInterface::Init(unsigned int mode)
{
	UNUSED_VAR(mode);
#if !__XENON && !__PS3
	// current gen do on the fly init, to preserve memory
	DebugTextureViewerDTQ::DTQInit();
#endif // !__XENON && !__PS3
}

__COMMENT(static) void CDebugTextureViewerInterface::Shutdown(unsigned int mode)
{
	UNUSED_VAR(mode);
}

__COMMENT(static) void CDebugTextureViewerInterface::InitWidgets()
{
	DEBUG_TEXTURE_VIEWER_USE_DEBUG_MEMORY();

	gDebugTextureViewer.InitWidgets(BANKMGR.CreateBank("Texture Viewer"));
	gOverlayImageViewer.InitWidgets(BANKMGR.CreateBank("Overlay Image"));
}

__COMMENT(static) bool CDebugTextureViewerInterface::IsEnabled()
{
	return gDebugTextureViewer.m_enablePicking;
}

__COMMENT(static) bool CDebugTextureViewerInterface::IsEntitySelectRequired()
{
#if ENTITYSELECT_ENABLED_BUILD
	if (gDebugTextureViewer.m_enablePicking ||
		gDebugTextureViewer.m_hoverEntityShaderMode > 0)
	{
		return true;
	}
#endif // ENTITYSELECT_ENABLED_BUILD

	return false;
}

__COMMENT(static) const grcTexture* CDebugTextureViewerInterface::GetReflectionOverrideTex()
{
#if DEBUG_TEXTURE_VIEWER_OVERRIDE_REFLECTION_TEX
	if (gDebugTextureViewer.m_overrideReflectionTex == 1)
	{
		const CDTVEntry* entry = gDebugTextureViewer.GetSelectedListEntry();

		if (entry)
		{
			return entry->GetCurrentTexture();
		}
	}
	else if (gDebugTextureViewer.m_overrideReflectionTex == 2) { return grcTexture::NoneWhite; }
	else if (gDebugTextureViewer.m_overrideReflectionTex == 3) { return grcTexture::NoneBlack; }
#endif // DEBUG_TEXTURE_VIEWER_OVERRIDE_REFLECTION_TEX
	return NULL;
}

__COMMENT(static) const grcTexture* CDebugTextureViewerInterface::GetReflectionOverrideCubeTex()
{
#if DEBUG_TEXTURE_VIEWER_OVERRIDE_REFLECTION_TEX
	if (gDebugTextureViewer.m_overrideReflectionCubeTex == 1)
	{
		const CDTVEntry* entry = gDebugTextureViewer.GetSelectedListEntry();

		if (entry)
		{
			return entry->GetCurrentTexture();
		}
	}
	else if (gDebugTextureViewer.m_overrideReflectionCubeTex == 2) { return grcTexture::NoneWhite; }
	else if (gDebugTextureViewer.m_overrideReflectionCubeTex == 3) { return grcTexture::NoneBlack; }
#endif // DEBUG_TEXTURE_VIEWER_OVERRIDE_REFLECTION_TEX
	return NULL;
}

__COMMENT(static) void CDebugTextureViewerInterface::Update()
{
	//DEBUG_TEXTURE_VIEWER_USE_DEBUG_MEMORY();

	CDebugTextureViewerWindowMgr::GetMgr().Update();
	gDebugTextureViewer.Update();
	gOverlayImageViewer.Update();
#if DEBUG_TEXTURE_VIEWER_GET_WORLD_POS_UNDER_MOUSE
	CDTVGetWorldPosUnderMouse::Update();
#endif // DEBUG_TEXTURE_VIEWER_GET_WORLD_POS_UNDER_MOUSE
#if DEBUG_TEXTURE_VIEWER_FRAMEBUFFER_MAGNIFIER
	CDTVMagnifier::Update();
#endif // DEBUG_TEXTURE_VIEWER_FRAMEBUFFER_MAGNIFIER
}

__COMMENT(static) void CDebugTextureViewerInterface::Render()
{
	//DEBUG_TEXTURE_VIEWER_USE_DEBUG_MEMORY();

	gDebugTextureViewer.Render();
	gOverlayImageViewer.Render();
#if DEBUG_TEXTURE_VIEWER_GET_WORLD_POS_UNDER_MOUSE
	CDTVGetWorldPosUnderMouse::Render();
#endif // DEBUG_TEXTURE_VIEWER_GET_WORLD_POS_UNDER_MOUSE
#if DEBUG_TEXTURE_VIEWER_FRAMEBUFFER_MAGNIFIER
	CDTVMagnifier::Render();
#endif // DEBUG_TEXTURE_VIEWER_FRAMEBUFFER_MAGNIFIER
}

__COMMENT(static) void CDebugTextureViewerInterface::Synchronise()
{
	gDebugTextureViewer.Synchronise();
	gOverlayImageViewer.Synchronise();
}

static const CBaseModelInfo* s_prevSelectModelInfo = NULL; // track previous inputs, don't populate 
static const CEntity*        s_prevSelectEntity    = NULL;
static CTxdRef               s_prevSelectTxdRef    = CTxdRef(AST_None, INDEX_NONE, INDEX_NONE, "");

__COMMENT(static) void CDebugTextureViewerInterface::SelectModelInfo(u32 modelInfoIndex, const CEntity* entity, bool bActivate, bool bNoEmptyTxds, bool bImmediate)
{
	static u32     s_deferredSelectModelInfo_modelInfoIndex = ~0U;
	static RegdEnt s_deferredSelectModelInfo_entity;
	static bool    s_deferredSelectModelInfo_bActivate = false;
	static bool    s_deferredSelectModelInfo_bNoEmptyTxds = false;

	if (bImmediate)
	{
		if (s_deferredSelectModelInfo_modelInfoIndex != ~0U)
		{
			modelInfoIndex = s_deferredSelectModelInfo_modelInfoIndex;
			entity         = s_deferredSelectModelInfo_entity.Get();
			bActivate      = s_deferredSelectModelInfo_bActivate;
			bNoEmptyTxds   = s_deferredSelectModelInfo_bNoEmptyTxds;

			DEBUG_TEXTURE_VIEWER_USE_DEBUG_MEMORY();

			CDebugTextureViewer* _this = &gDebugTextureViewer;
			const CBaseModelInfo* modelInfo = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelInfoIndex)));

			if (modelInfo)
			{
				_this->m_visible = true;

				if (s_prevSelectModelInfo != modelInfo ||
					s_prevSelectEntity    != entity)
				{
					s_prevSelectModelInfo = modelInfo;
					s_prevSelectEntity    = entity;
					s_prevSelectTxdRef    = CTxdRef(AST_None, INDEX_NONE, INDEX_NONE, "");

					_this->Populate_SelectModelInfo(modelInfo->GetModelName(), modelInfoIndex, entity, bNoEmptyTxds);

					if (_this->m_debugWindow && !bActivate)
					{
						_this->m_debugWindow->SetIsActive(false);
					}

					if (0)
					{
						_this->ClearSelectedListEntry();
					}
				}
			}
		}

		s_deferredSelectModelInfo_modelInfoIndex = ~0U;
		s_deferredSelectModelInfo_entity         = NULL;
		s_deferredSelectModelInfo_bActivate      = false;
		s_deferredSelectModelInfo_bNoEmptyTxds   = false;
	}
	else
	{
		s_deferredSelectModelInfo_modelInfoIndex = modelInfoIndex;
		s_deferredSelectModelInfo_entity         = const_cast<CEntity*>(entity);
		s_deferredSelectModelInfo_bActivate      = bActivate;
		s_deferredSelectModelInfo_bNoEmptyTxds   = bNoEmptyTxds;
	}
}

__COMMENT(static) void CDebugTextureViewerInterface::SelectTxd(const CTxdRef& in_ref, bool bActivate, bool bPreviewTxd, bool bMakeVisible, bool bImmediate)
{
	static eAssetType s_deferredSelectTxd_ref_m_assetType  = AST_None;
	static int        s_deferredSelectTxd_ref_m_assetIndex = INDEX_NONE;
	static int        s_deferredSelectTxd_ref_m_assetEntry = INDEX_NONE;
	static bool       s_deferredSelectTxd_bActivate        = false;
	static bool       s_deferredSelectTxd_bPreviewTxd      = false;
	static bool       s_deferredSelectTxd_bMakeVisible     = false;

	if (bImmediate)
	{
		if (s_deferredSelectTxd_ref_m_assetType != AST_None)
		{
			const CTxdRef ref
			(
				s_deferredSelectTxd_ref_m_assetType,
				s_deferredSelectTxd_ref_m_assetIndex,
				s_deferredSelectTxd_ref_m_assetEntry,
				""
			);
			bActivate    = s_deferredSelectTxd_bActivate;
			bPreviewTxd  = s_deferredSelectTxd_bPreviewTxd;
			bMakeVisible = s_deferredSelectTxd_bMakeVisible;

			DEBUG_TEXTURE_VIEWER_USE_DEBUG_MEMORY();

			CDebugTextureViewer* _this = &gDebugTextureViewer;

			if (ref.IsValidAssetSlot())
			{
				_this->m_visible = bMakeVisible || _this->m_visible;

				if (s_prevSelectTxdRef != ref)
				{
					s_prevSelectModelInfo = NULL;
					s_prevSelectEntity    = NULL;
					s_prevSelectTxdRef    = ref;

					_this->Populate_SelectTxd(ref.GetAssetName(), ref);

					if (_this->m_debugWindow && !bActivate)
					{
						_this->m_debugWindow->SetIsActive(false);
					}

					(void)bPreviewTxd;

#if DEBUG_TEXTURE_VIEWER_SELECTPREVIEW
					if (bPreviewTxd)
					{
						const fwTxd* previewTxd = ref.GetTxd();

						if (previewTxd)
						{
							_this->m_previewTxd = previewTxd;
						}

						_this->ClearSelectedListEntry();
					}
#endif // DEBUG_TEXTURE_VIEWER_SELECTPREVIEW
				}
			}
		}

		s_deferredSelectTxd_ref_m_assetType  = AST_None;
		s_deferredSelectTxd_ref_m_assetIndex = INDEX_NONE;
		s_deferredSelectTxd_ref_m_assetEntry = INDEX_NONE;
		s_deferredSelectTxd_bActivate        = false;
		s_deferredSelectTxd_bPreviewTxd      = false;
		s_deferredSelectTxd_bMakeVisible     = false;
	}
	else
	{
		s_deferredSelectTxd_ref_m_assetType  = in_ref.GetAssetType();
		s_deferredSelectTxd_ref_m_assetIndex = in_ref.GetAssetIndex().Get();
		s_deferredSelectTxd_ref_m_assetEntry = in_ref.m_assetEntry;
		s_deferredSelectTxd_bActivate        = bActivate;
		s_deferredSelectTxd_bPreviewTxd      = bPreviewTxd;
		s_deferredSelectTxd_bMakeVisible     = bMakeVisible;
	}
}

__COMMENT(static) void CDebugTextureViewerInterface::SelectPreviewTexture(const grcTexture* texture, const char* name, bool bActivate, bool bImmediate)
{
	static const grcTexture* s_deferredSelectPreviewTexture_texture   = NULL;
	static char              s_deferredSelectPreviewTexture_name[128] = "";
	static bool              s_deferredSelectPreviewTexture_bActivate = false;

	if (bImmediate)
	{
		if (s_deferredSelectPreviewTexture_texture)
		{
			texture   = s_deferredSelectPreviewTexture_texture;
			name      = s_deferredSelectPreviewTexture_name;
			bActivate = s_deferredSelectPreviewTexture_bActivate;

			DEBUG_TEXTURE_VIEWER_USE_DEBUG_MEMORY();

			CDebugTextureViewer* _this = &gDebugTextureViewer;

			if (texture)
			{
				_this->m_visible = true;

				_this->Clear(false, true);
				_this->SetupWindow(atVarString("TEXTURE VIEWER - \"%s\"", name), false);

				_this->m_previewTexture = texture;

				if (_this->m_debugWindow && !bActivate)
				{
					_this->m_debugWindow->SetIsActive(false);
				}

				s_prevSelectModelInfo = NULL;
				s_prevSelectEntity    = NULL;
				s_prevSelectTxdRef    = CTxdRef(AST_None, INDEX_NONE, INDEX_NONE, "");
			}
		}

		s_deferredSelectPreviewTexture_texture   = NULL;
		s_deferredSelectPreviewTexture_name[0]   = '\0';
		s_deferredSelectPreviewTexture_bActivate = false;
	}
	else
	{
		s_deferredSelectPreviewTexture_texture   = texture; // is this safe to store a texture pointer directly?
		s_deferredSelectPreviewTexture_bActivate = bActivate;

		strcpy(s_deferredSelectPreviewTexture_name, name);
	}
}

#if DEBUG_TEXTURE_VIEWER_SHADER_EDIT_INTEGRATION_OLD

extern void SetShaderEditHackModelInfo(const CBaseModelInfo* modelInfo);
extern void SetShaderEditHackTxdSlot(int slot);

#endif // DEBUG_TEXTURE_VIEWER_SHADER_EDIT_INTEGRATION_OLD

__COMMENT(static) bool CDebugTextureViewerInterface::UpdateTextureForShaderEdit(const grcTexture* texture, const char* name, u32 modelIndex)
{
	DEBUG_TEXTURE_VIEWER_USE_DEBUG_MEMORY();

	CDebugTextureViewer* _this = &gDebugTextureViewer;

	if (_this->m_verbose)
	{
		Displayf("CDebugTextureViewerInterface::UpdateTextureForShaderEdit(..,name=\"%s\",Model:%d(%s))", name, modelIndex, CModelInfo::GetBaseModelInfoName(fwModelId(strLocalIndex(modelIndex))));
	}

	if (texture)
	{
#if DEBUG_TEXTURE_VIEWER_SHADER_EDIT_INTEGRATION_OLD
		const char* txdName = "[ShaderEditTemp]";
		int slot = g_TxdStore.FindSlot(txdName);

		if (slot == INDEX_NONE)
		{
			slot = g_TxdStore.AddSlot(txdName); // this is permanent, right?

			if (slot != INDEX_NONE)
			{
				g_TxdStore.Set(slot, rage_new fwTxd(0));
				SetShaderEditHackTxdSlot(slot);
			}

			if (_this->m_verbose)
			{
				Displayf("  created new slot %d", slot);
			}
		}

		fwTxd* txd = g_TxdStore.GetSafeFromIndex(slot);

		if (txd)
		{
			bool bFound = false;

			for (int k = 0; k < txd->GetCount(); k++)
			{
				grcTexture* existing = txd->GetEntry(k);

				if (existing && stricmp(texture->GetName(), existing->GetName()) == 0)
				{
					if (_this->m_verbose)
					{
						Displayf("  replacing existing texture at entry %d ..", k);
					}

					existing->Release();

					txd->SetEntryUnsafe(k, const_cast<grcTexture*>(texture));
					bFound = true;
					break;
				}
			}

			if (!bFound)
			{
				if (_this->m_verbose)
				{
					Displayf("  adding new texture at entry %d", txd->GetCount());
				}

				txd->AddNewEntry(GetFriendlyTextureName(texture).c_str(), const_cast<grcTexture*>(texture));
			}

			if (modelIndex != fwModelId::MI_INVALID)
			{
				if (_this->m_verbose)
				{
					Displayf("  marking model %s as hacked for Shader Edit", CModelInfo::GetBaseModelInfoName(fwModelId(modelIndex)));
				}

				SetShaderEditHackModelInfo(CModelInfo::GetBaseModelInfo(fwModelId(modelIndex)));
			}

			return true;
		}
#endif // DEBUG_TEXTURE_VIEWER_SHADER_EDIT_INTEGRATION_OLD

#if DEBUG_TEXTURE_VIEWER_SHADER_EDIT_INTEGRATION
		if (_this->m_debugWindow && _this->m_visible)
		{
			const bool bAddEntriesToBottom = false; // doesn't work, dammit ..

			if (_this->m_listEntries.GetCount() == 0 || dynamic_cast<const CDTVEntry_ShaderEditTexture*>(_this->m_listEntries[0]) == NULL)
			{
				_this->Clear(false, true);
				_this->SetupWindow("TEXTURE VIEWER - SHADER EDIT", false);
			}

			if (_this->m_listState.GetCount() > 0)
			{
				_this->m_listState.Top().m_listSortColumnID = bAddEntriesToBottom ? CDTVColumnFlags::CID_NONE : CDTVColumnFlags::CID_LineNumber;
			}

			const char* name = texture->GetName();

			if (strrchr(name, '/')) // strip path
			{
				name = strrchr(name, '/') + 1;
			}

			_this->m_listEntries.PushAndGrow(rage_new CDTVEntry_ShaderEditTexture(texture, name));
			_this->SetupWindowFinish();

			if (bAddEntriesToBottom)
			{
				_this->m_debugWindow->SetCurrentIndex(99999); // select the last entry in the list, which should be the one we just added
			}

			s_prevSelectModelInfo = NULL;
			s_prevSelectEntity    = NULL;
			s_prevSelectTxdRef    = CTxdRef(AST_None, INDEX_NONE, INDEX_NONE, "");

			return true;
		}
#endif // DEBUG_TEXTURE_VIEWER_SHADER_EDIT_INTEGRATION
	}

	return false;
}

__COMMENT(static) void CDebugTextureViewerInterface::Hide()
{
	gDebugTextureViewer.m_visible = false;
}

__COMMENT(static) void CDebugTextureViewerInterface::Close(bool bRemoveRefsImmediately)
{
	gDebugTextureViewer.Close(bRemoveRefsImmediately);
}

__COMMENT(static) void CDebugTextureViewerInterface::GetWorldPosUnderMouseAddRef()
{
#if DEBUG_TEXTURE_VIEWER_GET_WORLD_POS_UNDER_MOUSE
	gDebugTextureViewer.m_showWorldPosExternalRefCount++;
#endif // DEBUG_TEXTURE_VIEWER_GET_WORLD_POS_UNDER_MOUSE
}

__COMMENT(static) void CDebugTextureViewerInterface::GetWorldPosUnderMouseRemoveRef()
{
#if DEBUG_TEXTURE_VIEWER_GET_WORLD_POS_UNDER_MOUSE
	gDebugTextureViewer.m_showWorldPosExternalRefCount--;
#endif // DEBUG_TEXTURE_VIEWER_GET_WORLD_POS_UNDER_MOUSE
}

__COMMENT(static) void CDebugTextureViewerInterface::GetWorldPosUnderMouse(Vec4V* pos)
{
#if DEBUG_TEXTURE_VIEWER_GET_WORLD_POS_UNDER_MOUSE
	*pos = CDTVGetWorldPosUnderMouse::m_worldPos;
#else
	*pos = Vec4V(V_ZERO);
#endif
}

__COMMENT(static) const CBaseModelInfo* CDebugTextureViewerInterface::GetSelectedModelInfo_renderthread()
{
	return gDebugTextureViewer.GetCurrentRenderState().m_selectedModelInfo;
}

__COMMENT(static) u32 CDebugTextureViewerInterface::GetSelectedShaderPtr_renderthread()
{
	return gDebugTextureViewer.GetCurrentRenderState().m_selectedShaderPtr;
}

__COMMENT(static) const char* CDebugTextureViewerInterface::GetSelectedShaderName_renderthread()
{
	return gDebugTextureViewer.GetCurrentRenderState().m_selectedShaderName;
}

__COMMENT(static) const void* CDebugTextureViewerInterface::GetSelectedTxd()
{
	const CDTVEntry_Txd* entry = dynamic_cast<const CDTVEntry_Txd*>(gDebugTextureViewer.GetSelectedListEntry());

	if (entry)
	{
		return entry->GetTxdRef().GetTxd();
	}
	else
	{
		const CDTVEntry_Texture* entry2 = dynamic_cast<const CDTVEntry_Texture*>(gDebugTextureViewer.GetSelectedListEntry());

		if (entry2)
		{
			return entry2->GetTxdRef().GetTxd();
		}
	}

	return NULL;
}

#endif // __BANK
