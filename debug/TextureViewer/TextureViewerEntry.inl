// ==========================================
// debug/textureviewer/textureviewerentry.inl
// (c) 2010 RockstarNorth
// ==========================================

__COMMENT(static) s32 CDTVEntry::CompareFunc(CDTVEntry const* a, CDTVEntry const* b)
{
	const CDebugTextureViewer* _this = &gDebugTextureViewer;

	const float sortKey1 = a->GetTextAndSortKey(NULL, _this->m_sortColumnID);
	const float sortKey2 = b->GetTextAndSortKey(NULL, _this->m_sortColumnID);

	if      (sortKey1 < sortKey2) { return +1; } // sort highest to lowest
	else if (sortKey1 > sortKey2) { return -1; } // sort highest to lowest

	if (sortKey1 == 0.0f && sortKey2 == 0.0f) // sort by column text if sort keys are both 0.0f
	{
		char text1[512] = "";
		char text2[512] = "";

		a->GetTextAndSortKey(text1, _this->m_sortColumnID);
		b->GetTextAndSortKey(text2, _this->m_sortColumnID);

		return strcmp(text1, text2);
	}

	return strcmp(a->m_name.c_str(), b->m_name.c_str());
}

__COMMENT(virtual) void CDTVEntry_ModelInfo::Select(const CAssetRefInterface& ari)
{
	CDebugTextureViewer* _this = &gDebugTextureViewer;

	UNUSED_VAR(ari);

	if (m_modelInfo)
	{
		if (_this->m_selectModelTextures)
		{
			_this->Populate_SelectModelInfoTex(m_name.c_str(), m_modelInfoIndex, m_entity);
		}
		else
		{
			_this->Populate_SelectModelInfo(m_name.c_str(), m_modelInfoIndex, m_entity);
		}
	}
}

__COMMENT(virtual) void CDTVEntry_Txd::Select(const CAssetRefInterface& ari)
{
	CDebugTextureViewer* _this = &gDebugTextureViewer;

	if (m_txd == NULL) // dynamic entry, try to load it
	{
		m_txd = m_ref.LoadTxd(ari.m_verbose);
	}

#if DEBUG_TEXTURE_VIEWER_SELECTPREVIEW
	if (0 && m_txd) // testing SelectTxd functionality ..
	{
		CDebugTextureViewerInterface::SelectTxd(m_ref.GetAssetIndex(), true);
	}
	else
#endif // DEBUG_TEXTURE_VIEWER_SELECTPREVIEW
	{
		if (m_txd)
		{
			const CTxdRef ref = m_ref; // copy the ref, as Populate_* will clear the entries so 'this' will no longer be valid

			_this->Populate_SelectTxd(m_name.c_str(), ref);
			ref.AddRef(ari);
		}
	}
}
