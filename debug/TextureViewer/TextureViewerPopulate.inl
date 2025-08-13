// =============================================
// debug/textureviewer/textureviewerpopulate.inl
// (c) 2010 RockstarNorth
// =============================================

#if __DEV

__COMMENT(static) void CDebugTextureViewer::_PrintAssets_ModelInfo    () { const CDebugTextureViewer* _this = &gDebugTextureViewer; PrintAssets_ModelInfo    (_this->m_searchFilter, _this->m_asp); }
__COMMENT(static) void CDebugTextureViewer::_PrintAssets_TxdStore     () { const CDebugTextureViewer* _this = &gDebugTextureViewer; PrintAssets_TxdStore     (_this->m_searchFilter, _this->m_asp); }
__COMMENT(static) void CDebugTextureViewer::_PrintAssets_DwdStore     () { const CDebugTextureViewer* _this = &gDebugTextureViewer; PrintAssets_DwdStore     (_this->m_searchFilter, _this->m_asp); }
__COMMENT(static) void CDebugTextureViewer::_PrintAssets_DrawableStore() { const CDebugTextureViewer* _this = &gDebugTextureViewer; PrintAssets_DrawableStore(_this->m_searchFilter, _this->m_asp); }
__COMMENT(static) void CDebugTextureViewer::_PrintAssets_FragmentStore() { const CDebugTextureViewer* _this = &gDebugTextureViewer; PrintAssets_FragmentStore(_this->m_searchFilter, _this->m_asp); }
__COMMENT(static) void CDebugTextureViewer::_PrintAssets_ParticleStore() { const CDebugTextureViewer* _this = &gDebugTextureViewer; PrintAssets_ParticleStore(_this->m_searchFilter, _this->m_asp); }

#endif // __DEV

__COMMENT(static) void CDebugTextureViewer::_Populate_Search              () { CDebugTextureViewer* _this = &gDebugTextureViewer; _this->Populate_Search              (); }
__COMMENT(static) void CDebugTextureViewer::_Populate_SearchModels__broken() { CDebugTextureViewer* _this = &gDebugTextureViewer; _this->Populate_SearchModels__broken(); }
__COMMENT(static) void CDebugTextureViewer::_Populate_SearchTxds          () { CDebugTextureViewer* _this = &gDebugTextureViewer; _this->Populate_SearchTxds          (); }
__COMMENT(static) void CDebugTextureViewer::_Populate_SearchTextures      () { CDebugTextureViewer* _this = &gDebugTextureViewer; _this->Populate_SearchTextures      (); }
__COMMENT(static) void CDebugTextureViewer::_Populate_SelectFromPicker    () { CDebugTextureViewer* _this = &gDebugTextureViewer; _this->Populate_SelectFromPicker    (); }

void CDebugTextureViewer::Populate_SearchModelClick()
{
	DEBUG_TEXTURE_VIEWER_USE_DEBUG_MEMORY();

	if (m_verbose) { Displayf("CDebugTextureViewer::Populate_SearchModelClick"); }
	SetupWindow("TEXTURE VIEWER - CLICK", false);

	// populate
	{
#if ENTITYSELECT_ENABLED_BUILD
		CEntity* entity = (CEntity*)CEntitySelect::GetSelectedEntity();

		if (entity)
		{
			m_listEntries.PushAndGrow(rage_new CDTVEntry_ModelInfo(entity->GetModelName(), entity->GetModelIndex(), entity));
		}
#endif // ENTITYSELECT_ENABLED_BUILD
	}

	SetupWindowFinish();
}

void CDebugTextureViewer::Populate_Search()
{
	switch ((int)m_searchType)
	{
	case PST_SearchModels__broken : Populate_SearchModels__broken(); break;
	case PST_SearchTxds           : Populate_SearchTxds          (); break;
	case PST_SearchTextures       : Populate_SearchTextures      (); break;
	}
}

void CDebugTextureViewer::Populate_SearchModels__broken()
{
	DEBUG_TEXTURE_VIEWER_USE_DEBUG_MEMORY();

	const char* name = m_searchFilter;
	const atString nameStr = strlen(name) > 0 ? atVarString(": \"%s\"", name) : "";
	if (m_verbose) { Displayf("CDebugTextureViewer::Populate_SearchModels__broken(\"%s\")", name); }
	SetupWindow(atVarString("TEXTURE VIEWER - SEARCH MODEL%s", nameStr.c_str()), false);

	if (1)
	{
		Assertf(0, "\"Search Models\" is currently broken, aborting search");
	}
	else // populate
	{
		atArray<u32> slots; FindAssets_ModelInfo(slots, name, m_searchRPF, m_asp, m_findAssetsMaxCount);

		for (int i = 0; i < slots.size(); i++)
		{
			const u32   modelInfoIndex = slots[i];
			const CBaseModelInfo* modelInfo      = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelInfoIndex)));
			const char*           modelInfoName  = modelInfo->GetModelName();

#if __DEV
			if (m_printShaderNamesForModelSearchResults)
			{
				atArray<const grmShader*> shaders;
				GetShadersUsedByModel(shaders, modelInfo, true);

				Displayf("model \"%s\" uses %d unique shaders:", modelInfoName, shaders.size());

				for (int j = 0; j < shaders.size(); j++)
				{
					Displayf("  \"%s\"", shaders[j]->GetName());
				}
			}
#endif // __DEV

			m_listEntries.PushAndGrow(rage_new CDTVEntry_ModelInfo(modelInfoName, modelInfoIndex, NULL));
		}
	}

	SetupWindowFinish();
}

void CDebugTextureViewer::Populate_SearchTxds()
{
	DEBUG_TEXTURE_VIEWER_USE_DEBUG_MEMORY();

	const char* name = m_searchFilter;
	const atString nameStr = strlen(name) > 0 ? atVarString(": \"%s\"", name) : "";
	if (m_verbose) { Displayf("CDebugTextureViewer::Populate_SearchTxds(\"%s\")", name); }
	SetupWindow(atVarString("TEXTURE VIEWER - SEARCH TXD%s", nameStr.c_str()), false);

	const bool bOnlyIfLoaded = m_findAssetsOnlyIfLoaded || strlen(m_searchFilter) == 0; // if we're searching for "all", then only show the loaded assets
	const bool bIgnoreParentTxds = true;

	// populate
	{
		atArray<CTxdRef> refs;

		if (m_asp.m_scanTxdStore)
		{
			if (m_verbose) { Displayf("CDebugTextureViewer::Populate_SearchTxds: scanning g_TxdStore .."); }
			atArray<int> slots; FindAssets_TxdStore(slots, name, m_searchRPF, m_asp, m_findAssetsMaxCount, bOnlyIfLoaded);
			if (m_verbose) { Displayf("CDebugTextureViewer::Populate_SearchTxds: found %d assets, populating ..", slots.GetCount()); }

			for (int i = 0; i < slots.size(); i++)
			{
				GetAssociatedTxds_TxdStore(refs, slots[i], m_asp, "", bIgnoreParentTxds);
			}

			if (m_verbose) { Displayf("CDebugTextureViewer::Populate_SearchTxds: found %d refs so far", refs.GetCount()); }
		}

		if (m_asp.m_scanDwdStore)
		{
			if (m_verbose) { Displayf("CDebugTextureViewer::Populate_SearchTxds: scanning g_DwdStore .."); }
			atArray<int> slots; FindAssets_DwdStore(slots, name, m_searchRPF, m_asp, m_findAssetsMaxCount, bOnlyIfLoaded);
			if (m_verbose) { Displayf("CDebugTextureViewer::Populate_SearchTxds: found %d assets, populating ..", slots.GetCount()); }

			for (int i = 0; i < slots.size(); i++)
			{
				GetAssociatedTxds_DwdStore(refs, strLocalIndex(slots[i]), m_asp, "", bIgnoreParentTxds);
			}

			if (m_verbose) { Displayf("CDebugTextureViewer::Populate_SearchTxds: found %d refs so far", refs.GetCount()); }
		}

		if (m_asp.m_scanDrawableStore)
		{
			if (m_verbose) { Displayf("CDebugTextureViewer::Populate_SearchTxds: scanning g_DrawableStore .."); }
			atArray<int> slots; FindAssets_DrawableStore(slots, name, m_searchRPF, m_asp, m_findAssetsMaxCount, bOnlyIfLoaded);
			if (m_verbose) { Displayf("CDebugTextureViewer::Populate_SearchTxds: found %d assets, populating ..", slots.GetCount()); }

			for (int i = 0; i < slots.size(); i++)
			{
				GetAssociatedTxds_DrawableStore(refs, slots[i], m_asp, "", bIgnoreParentTxds);
			}
		}

		if (m_asp.m_scanFragmentStore)
		{
			if (m_verbose) { Displayf("CDebugTextureViewer::Populate_SearchTxds: scanning g_FragmentStore .."); }
			atArray<int> slots; FindAssets_FragmentStore(slots, name, m_searchRPF, m_asp, m_findAssetsMaxCount, bOnlyIfLoaded);
			if (m_verbose) { Displayf("CDebugTextureViewer::Populate_SearchTxds: found %d assets, populating ..", slots.GetCount()); }

			for (int i = 0; i < slots.size(); i++)
			{
				GetAssociatedTxds_FragmentStore(refs, slots[i], m_asp, "", bIgnoreParentTxds);
			}

			if (m_verbose) { Displayf("CDebugTextureViewer::Populate_SearchTxds: found %d refs so far", refs.GetCount()); }
		}

		if (m_asp.m_scanParticleStore)
		{
			if (m_verbose) { Displayf("CDebugTextureViewer::Populate_SearchTxds: scanning g_ParticleStore .."); }
			atArray<int> slots; FindAssets_ParticleStore(slots, name, m_searchRPF, m_asp, m_findAssetsMaxCount, bOnlyIfLoaded);
			if (m_verbose) { Displayf("CDebugTextureViewer::Populate_SearchTxds: found %d assets, populating ..", slots.GetCount()); }

			for (int i = 0; i < slots.size(); i++)
			{
				GetAssociatedTxds_ParticleStore(refs, slots[i], m_asp, "", bIgnoreParentTxds);
			}

			if (m_verbose) { Displayf("CDebugTextureViewer::Populate_SearchTxds: found %d refs so far", refs.GetCount()); }
		}

		for (int i = 0; i < refs.size(); i++)
		{
			const bool bAllowFindDependencies = true; // why not .. as long as it's allowed in m_asp
			bool bDuplicate = false;

			for (int j = 0; j < i; j++)
			{
				if (refs[j] == refs[i])
				{
					bDuplicate = true;
					break;
				}
			}

			if (!bDuplicate)
			{
				m_listEntries.PushAndGrow(rage_new CDTVEntry_Txd(refs[i], m_asp, bAllowFindDependencies));
			}
		}
	}

	if (m_verbose) { Displayf("CDebugTextureViewer::Populate_SearchTxds: finishing .."); }

	SetupWindowFinish();

	if (m_verbose) { Displayf("CDebugTextureViewer::Populate_SearchTxds: done."); }
}

void CDebugTextureViewer::Populate_SearchTextures()
{
	DEBUG_TEXTURE_VIEWER_USE_DEBUG_MEMORY();

	const char* name = m_searchFilter;
	const atString nameStr = strlen(name) > 0 ? atVarString(": \"%s\"", name) : "";
	if (m_verbose) { Displayf("CDebugTextureViewer::Populate_SearchTextures(\"%s\")", name); }
	SetupWindow(atVarString("TEXTURE VIEWER - SEARCH TEXTURE%s", nameStr.c_str()), false);

	// populate
	{
		atArray<CTextureRef> refs;
		FindTextures(
			refs,
			name,
			m_asp,
			m_findAssetsMaxCount,
			m_findTexturesMinSize,
			m_findTexturesConstantOnly,
			m_findTexturesRedundantOnly,
			m_findTexturesReportNonUniqueTextureMemory,
			m_findTexturesConversionFlagsRequired,
			m_findTexturesConversionFlagsSkipped,
			m_findTexturesUsage,
			m_findTexturesUsageExclusion,
			m_findTexturesFormat,
			m_verbose
		);

		if (m_verbose) { Displayf("CDebugTextureViewer::Populate_SearchTextures: populating .."); }

		for (int i = 0 ; i < refs.size(); i++)
		{
			const bool bAllowFindDependencies = false; // this would be far too slow, don't do it!

			if (m_searchRPF[0] != '\0')
			{
				const char* rpfPath = refs[i].GetRPFPathName();

				if (rpfPath[0] == '\0' || !StringMatch(rpfPath, m_searchRPF))
				{
					continue;
				}
			}

			m_listEntries.PushAndGrow(rage_new CDTVEntry_TextureRef(refs[i], m_asp, bAllowFindDependencies));
		}
	}

	if (m_verbose) { Displayf("CDebugTextureViewer::Populate_SearchTextures: finishing .."); }

	SetupWindowFinish();

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_FIND_UNUSED
	if (gStreamingIteratorTest_FindUnusedSharedTextures)
	{
		int numTexturesTotal = 0;
		int numTexturesUsed  = 0;
		int numBytesTotal    = 0;
		int numBytesUsed     = 0;

		for (int i = 0; i < m_listEntries.GetCount(); i++)
		{
			CDTVEntry_Texture* entry = dynamic_cast<CDTVEntry_Texture*>(m_listEntries[i]);

			if (entry && entry->GetTxdRef().GetAssetType() == AST_TxdStore)
			{
				const grcTexture* texture = entry->GetCurrentTexture_Public();
				const fwTxd*      txd     = entry->GetTxdRef().GetTxd();

				int k = INDEX_NONE; // ugh, we don't store the texture index in the txd in CDTVEntry_Texture for some reason, so we have to search for it

				if (texture && txd)
				{
					for (int kk = 0; kk < txd->GetCount(); kk++)
					{
						if (txd->GetEntry(kk) == texture)
						{
							k = kk;
							break;
						}
					}
				}

				if (k != INDEX_NONE)
				{
					const int slot = entry->GetTxdRef().GetAssetIndex();

					extern bool CDTVStreamingIteratorTest_IsTextureMarked(int slot, int k);

					if (!CDTVStreamingIteratorTest_IsTextureMarked(slot, k))
					{
						entry->SetErrorFlag(true);
					}
					else if (texture)
					{
						numTexturesUsed++;
						numBytesUsed += texture->GetPhysicalSize();
					}

					if (texture)
					{
						numTexturesTotal++;
						numBytesTotal += texture->GetPhysicalSize();
					}
				}
			}
		}

		Displayf(
			"> search found %d textures in g_TxdStore, %d used (%.2f%%), %.2fMB total, %.2fMB used (%.2f%%)",
			numTexturesTotal,
			numTexturesUsed,
			100.0f*(float)numTexturesUsed/(float)numTexturesTotal,
			(float)numBytesTotal/(1024.0f*1024.0f),
			(float)numBytesUsed/(1024.0f*1024.0f),
			100.0f*(float)numBytesUsed/(float)numBytesTotal
		);
	}
	else
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_FIND_UNUSED
	{
		int numBytes = 0; // just report total memory

		for (int i = 0; i < m_listEntries.GetCount(); i++)
		{
			CDTVEntry_Texture* entry = dynamic_cast<CDTVEntry_Texture*>(m_listEntries[i]);

			if (entry)
			{
				const grcTexture* texture = entry->GetCurrentTexture_Public();

				if (texture)
				{
					numBytes += texture->GetPhysicalSize();
				}
			}
		}

		Displayf(
			"> search found %.2fMB total",
			(float)numBytes/(1024.0f*1024.0f)
		);
	}

	if (m_verbose) { Displayf("CDebugTextureViewer::Populate_SearchTextures: done."); }
}

void CDebugTextureViewer::Populate_SelectFromPicker()
{
	DEBUG_TEXTURE_VIEWER_USE_DEBUG_MEMORY();

	if (m_verbose) { Displayf("CDebugTextureViewer::Populate_SelectFromPicker"); }
	SetupWindow("TEXTURE VIEWER - SELECT FROM PICKER", false);

	// populate
	{
		CEntity* entity = (CEntity*)g_PickerManager.GetSelectedEntity();

		if (entity)
		{
			m_listEntries.PushAndGrow(rage_new CDTVEntry_ModelInfo(entity->GetModelName(), entity->GetModelIndex(), entity));
		}
	}

	SetupWindowFinish();
}

void CDebugTextureViewer::Populate_SelectModelInfo(const char* name, u32 modelInfoIndex, const CEntity* entity, bool bNoEmptyTxds)
{
	DEBUG_TEXTURE_VIEWER_USE_DEBUG_MEMORY();

	if (m_verbose) { Displayf("CDebugTextureViewer::Populate_SelectModelInfo(\"%s\")", name); }
	SetupWindow(atVarString("TEXTURE VIEWER - SELECT MODEL: \"%s\"", name), true);

	const CBaseModelInfo* modelInfo = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelInfoIndex)));

	// populate
	{
		if (modelInfo)
		{
			atArray<CTxdRef> refs; GetAssociatedTxds_ModelInfo(refs, modelInfo, entity, m_asp.m_selectModelUseShowTxdFlags ? &m_asp : NULL);

			for (int i = 0; i < refs.size(); i++)
			{
				if (bNoEmptyTxds)
				{
					const fwTxd* txd = refs[i].GetTxd();

					if (txd == NULL || txd->GetCount() == 0)
					{
						continue;
					}
				}

				const bool bAllowFindDependencies = true; // why not .. as long as it's allowed in m_asp

				m_listEntries.PushAndGrow(rage_new CDTVEntry_Txd(refs[i], m_asp, bAllowFindDependencies, CAssetRefInterface(m_assetRefs, m_verbose)));
			}
		}
	}

	SetupWindowFinish();
}

void CDebugTextureViewer::Populate_SelectModelInfoTex(const char* name, u32 modelInfoIndex, const CEntity* entity)
{
	DEBUG_TEXTURE_VIEWER_USE_DEBUG_MEMORY();

	if (m_verbose) { Displayf("CDebugTextureViewer::Populate_SelectModelInfoTex(\"%s\")", name); }
	SetupWindow(atVarString("TEXTURE VIEWER - SELECT MODEL TEXTURES: \"%s\"", name), true);

	const CBaseModelInfo* modelInfo = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelInfoIndex)));

	// populate
	{
		if (modelInfo)
		{
			atArray<CTxdRef> refs; GetAssociatedTxds_ModelInfo(refs, modelInfo, entity, NULL);

			for (int i = 0; i < refs.size(); i++)
			{
				const fwTxd* txd = refs[i].GetTxd();

				if (txd)
				{
					for (int k = 0; k < txd->GetCount(); k++)
					{
						const grcTexture* texture = txd->GetEntry(k);

						if (texture)
						{
							const char* shaderName    = NULL;
							const char* shaderVarName = NULL;

							if (IsTextureUsedByModel(texture, NULL, modelInfo, entity, &shaderName, &shaderVarName, false))
							{
								const bool bAllowFindDependencies = true; // why not .. as long as it's allowed in m_asp

								// TODO -- only push if unique
								m_listEntries.PushAndGrow(rage_new CDTVEntry_TextureRef(CTextureRef(refs[i], k, texture), m_asp, bAllowFindDependencies, atVarString("%s/%s", shaderName, shaderVarName)));
							}
						}
					}
				}
			}

			LoadAll();
		}
	}

	SetupWindowFinish();
}

void CDebugTextureViewer::Populate_SelectTxd(const char* name, const CTxdRef& ref)
{
	DEBUG_TEXTURE_VIEWER_USE_DEBUG_MEMORY();

	if (m_verbose) { Displayf("CDebugTextureViewer::Populate_SelectTxd(\"%s\")", name); }
	const fwTxd* txd = ref.GetTxd();
	const atString info = txd ? atVarString(" (physical %.2fKB)", (float)ref.GetStreamingAssetSize(false)/1024.0f) : "";
	SetupWindow(atVarString("TEXTURE VIEWER - SELECT TXD: \"%s\"%s", name, info.c_str()), true);

	// populate
	{
		if (txd)
		{
			for (int i = 0; i < txd->GetCount(); i++)
			{
				const grcTexture* texture = txd->GetEntry(i);

				if (texture)
				{
					if (0 && __PS3) // i was having problems with 16+ bit/component textures on PS3 ..
					{
						const grcImage::Format format = (grcImage::Format)texture->GetImageFormat();

						if (grcImage::GetFormatBitsPerPixel(format) > 32 ||
							format == grcImage::L16 ||
							format == grcImage::G16R16 ||
							format == grcImage::G16R16F)
						{
							continue;
						}
					}

					const bool bAllowFindDependencies = true; // why not .. as long as it's allowed in m_asp

					m_listEntries.PushAndGrow(rage_new CDTVEntry_Texture(ref, m_asp, bAllowFindDependencies, texture));
				}
			}
		}
	}

	SetupWindowFinish();
}
