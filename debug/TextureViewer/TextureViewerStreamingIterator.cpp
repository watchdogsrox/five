// ======================================================
// debug/textureviewer/textureviewerstreamingiterator.cpp
// (c) 2010 RockstarNorth
// ======================================================

#include "debug/textureviewer/textureviewerstreamingiterator.h"

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR

#include "grcore/image.h"
#include "grcore/texture.h"
#include "string/stringutil.h"

#include "fragment/drawable.h"

#include "fwscene/stores/txdstore.h"
#include "fwscene/stores/dwdstore.h"
#include "fwscene/stores/drawablestore.h"
#include "fwscene/stores/fragmentstore.h"
#include "fwscene/stores/mapdatastore.h"
#include "fwutil/xmacro.h"
#include "vfx/ptfx/ptfxasset.h"

#include "streaming/streaming.h"
#include "streaming/streamingengine.h"
#include "streaming/packfilemanager.h"

#include "debug/textureviewer/textureviewerutil.h" // for StringMatch

RENDER_OPTIMISATIONS() // what?

// ================================================================================================

extern bool gStreamingIteratorTest_BuildAssetAnalysisData;

// TODO -- when starting the streaming iterator, call this (and disable scene streaming, Ian will provide a mechanism for this ..)
void CDTVStreamingIterator_Flush()
{
	CStreaming::PurgeRequestList();
	CStreaming::GetCleanup().DeleteAllDrawableReferences();
}

template <typename AssetStoreType, typename AssetType, typename AssetDefType> CDTVStreamingIterator<AssetStoreType,AssetType,AssetDefType>::CDTVStreamingIterator(void* user, int& currentSlot, AssetStoreType& store, IteratorFuncType func, const char* searchRPF, const char* searchName, u32& searchFlags, int maxFrames)
	: m_enabled    (true       )
	, m_user       (user       )
	, m_func       (func       )
	, m_store      (store      )
	, m_currentSlot(currentSlot)
	, m_immediate  (false      )
	, m_forceLoad  (true       )
	, m_searchRPF  (searchRPF  )
	, m_searchName (searchName )
	, m_searchFlags(searchFlags)
	, m_maxFrames  (maxFrames  )
{}

template <typename AssetStoreType, typename AssetType, typename AssetDefType> void CDTVStreamingIterator<AssetStoreType,AssetType,AssetDefType>::Reset()
{
	for (int i = 0; i < m_streamingSlots.size(); i++) // handle streaming slots
	{
		const CDTVStreamingIteratorSlot& sis = m_streamingSlots[i];

		m_func(NULL, false, sis.m_slot, sis.m_frames, m_user);
		m_store.ClearRequiredFlag(sis.m_slot, STRFLAG_DONTDELETE);

		//if (m_store.CheckObjectFlag(sis.m_slot, STRFLAG_CUTSCENE_REQUIRED))
		{
			//m_store.StreamingRemove(sis.m_slot); // we're done with the asset
		}
	}

	m_streamingSlots.clear();

	m_enabled     = true;
	m_currentSlot = 0;
}

// TODO -- share this with CDTVStreamingIteratorTest_GetRPFPathName
template <typename T> static const char* CDTVStreamingIterator_GetRPFPathName(T& store, int slot)
{
	if (slot != INDEX_NONE && store.IsValidSlot(slot))
	{
		const strIndex          streamingIndex = store.GetStreamingIndex(slot);
		const strStreamingInfo* streamingInfo  = strStreamingEngine::GetInfo().GetStreamingInfo(streamingIndex);
		const strStreamingFile* streamingFile  = strPackfileManager::GetImageFileFromHandle(streamingInfo->GetHandle());

		if (streamingFile)
		{
			return streamingFile->m_name.GetCStr();
		}

		return "/$INVALID_FILE";
	}

	return "/$INVALID_SLOT";
}

bool gStreamingIterator_LoadMapDataExterior = true;
bool gStreamingIterator_LoadMapDataInterior = true;
bool gStreamingIterator_LoadMapDataNonHD    = true;

template <typename AssetStoreType> static bool CDTVStreamingIterator_ShouldLoadSlot(AssetStoreType& store, int slot)
{
	return store.IsValidSlot(slot);
}

template <> bool CDTVStreamingIterator_ShouldLoadSlot<fwMapDataStore>(fwMapDataStore& store, int slot)
{
	if (store.IsValidSlot(slot))
	{
		const fwMapDataDef* def = store.GetSlot(slot);

		const bool bIsValid  = def->GetIsValid();
		const bool bIsHD     = (def->GetContentFlags() & fwMapData::CONTENTFLAG_ENTITIES_HD) != 0;
	//	const bool bIsScript = def->GetIsScriptManaged();

		if (1) // let's see what these are ..
		{
			if (!bIsValid) { Displayf(">> g_MapDataStore[%d] \"%s\" is %s", slot, store.GetName(slot), def->GetNumEntities() ? "invalid" : "empty"); }
		//	if (bIsScript) { Displayf(">> g_MapDataStore[%d] \"%s\" is script managed (%d entities)", slot, store.GetName(slot), def->GetNumEntities()); }
		}

		if (def->GetIsMLOInstanceDef()) { if (!gStreamingIterator_LoadMapDataInterior) { return false; }}
		else                            { if (!gStreamingIterator_LoadMapDataExterior) { return false; }}

		if (!bIsHD && !gStreamingIterator_LoadMapDataNonHD) { return false; }

		if (bIsValid)// && bIsHD)// && !bIsScript)
		{
			return true;
		}
	}

	return false;
}

template <typename AssetStoreType> static bool CDTVStreamingIterator_IsMapDataStore(AssetStoreType&)
{
	return false;
}

template <> bool CDTVStreamingIterator_IsMapDataStore<fwMapDataStore>(fwMapDataStore&)
{
	return true;
}

template <typename AssetStoreType, typename AssetType, typename AssetDefType> bool CDTVStreamingIterator<AssetStoreType,AssetType,AssetDefType>::Update(int count, int maxStreamingSlots)
{
	if (!m_enabled)
	{
		return false;
	}

	if (CDTVStreamingIterator_IsMapDataStore(m_store)) // force mapdata store to handle one per frame
	{
		count = 1;
		maxStreamingSlots = 1;
	}

	for (int i = 0; i < m_streamingSlots.size(); i++) // handle streaming slots
	{
		CDTVStreamingIteratorSlot& sis = m_streamingSlots[i];
#if STREAMING_ITERATOR_NO_ACCESS
		AssetType* asset = NULL;
#else
		AssetType* asset = m_store.GetSafeFromIndex(sis.m_slot);
#endif
		sis.m_frames++;

#if !STREAMING_ITERATOR_NO_ACCESS
		if (asset != NULL || sis.m_frames >= m_maxFrames)
#endif
		{
			m_func(m_store.GetSlot(sis.m_slot), false, sis.m_slot, sis.m_frames, m_user);
			m_store.ClearRequiredFlag(sis.m_slot, STRFLAG_CUTSCENE_REQUIRED);//STRFLAG_DONTDELETE);

			const CDTVStreamingIteratorSlot sis2 = m_streamingSlots.Pop();

			if (i < m_streamingSlots.size())
			{
				m_streamingSlots[i] = sis2;
			}
		}
	}

	for (int i = 0; i < count && m_currentSlot < m_store.GetSize(); i++)
	{
		if (!CDTVStreamingIterator_ShouldLoadSlot(m_store, m_currentSlot)) // invalid slot, skip it ..
		{
			i--;
			m_currentSlot++;
			continue;
		}

		const char* assetName = m_store.GetName(m_currentSlot);

		// skip broken assets, they crash for some reason
		{
			bool bSkip = false;

			for (int j = 0; j < m_brokenAssetNames.size(); j++)
			{
				if (stricmp(m_brokenAssetNames[j].c_str(), assetName) == 0)
				{
					bSkip = true;
					break;
				}
			}

			if (!bSkip && m_brokenRPFNames.size() > 0)
			{
				const char* rpfPath = CDTVStreamingIterator_GetRPFPathName(m_store, m_currentSlot);
			//	const char* rpfName = strrchr(rpfPath, '/');

			//	if (rpfName)
			//	{
			//		rpfName++;
			//	}
			//	else
			//	{
			//		rpfName = rpfPath;
			//	}

				for (int j = 0; j < m_brokenRPFNames.size(); j++)
				{
				//	if (stricmp(rpfName, m_brokenRPFNames[j].c_str()) == 0)
					if (stristr(rpfPath, m_brokenRPFNames[j].c_str()) != NULL)
					{
						bSkip = true;
						break;
					}
				}
			}

			if (bSkip)
			{
				i--;
				m_currentSlot++;
				continue;
			}
		}

		const strIndex          streamingIndex    = m_store.GetStreamingIndex(m_currentSlot);
		const strStreamingInfo* streamingInfo     = strStreamingEngine::GetInfo().GetStreamingInfo(streamingIndex);
		const strStreamingFile* streamingFile     = strPackfileManager::GetImageFileFromHandle(streamingInfo->GetHandle());
		const char*             streamingFileName = streamingFile ? streamingFile->m_name.GetCStr() : "";

		if (m_searchRPF && m_searchRPF[0] != '\0')
		{
			const bool bIsGlobalTxd = strstr(streamingFileName, "gtxd") != NULL || strstr(streamingFileName, "textures") != NULL;

			if (!(bIsGlobalTxd && !gStreamingIteratorTest_BuildAssetAnalysisData))
			{
				if (!StringMatch(streamingFileName, m_searchRPF))
				{
					i--;
					m_currentSlot++;
					continue;
				}
			}
		}

		if (m_searchName && m_searchName[0] != '\0')
		{
			if (!StringMatch(m_store.GetName(m_currentSlot), m_searchName))
			{
				i--;
				m_currentSlot++;
				continue;
			}
		}

		bool bSkip = false;

		if (m_searchFlags & SI_SEARCH_FLAG_SKIP_VEHICLES)
		{
			if (strstr(streamingFileName, "/vehicles"))
			{
				bSkip = true;
			}
		}

		if (m_searchFlags & SI_SEARCH_FLAG_SKIP_PEDS)
		{
			if (strstr(streamingFileName, "/componentpeds") ||
				strstr(streamingFileName, "/streamedpeds") ||
				strstr(streamingFileName, "/cutspeds"))
			{
				bSkip = true;
			}
		}

		if (m_searchFlags & SI_SEARCH_FLAG_SKIP_PROPS)
		{
			if (strstr(streamingFileName, "/props/"))
			{
				bSkip = true;
			}
		}

		if (m_searchFlags & SI_SEARCH_FLAG_DONT_SKIP)
		{
			bSkip = !bSkip;
		}

		if (bSkip)
		{
			i--;
			m_currentSlot++;
			continue;
		}

		const bool bIsInStreamingImage = m_store.IsObjectInImage(m_currentSlot);

#if STREAMING_ITERATOR_NO_ACCESS
		AssetType* asset = NULL;
#else
		AssetType* asset = m_store.GetSafeFromIndex(m_currentSlot);
#endif
		if (asset)
		{
			m_func(m_store.GetSlot(m_currentSlot), false, m_currentSlot, 0, m_user);

			//if (m_store.CheckObjectFlag(m_currentSlot, STRFLAG_CUTSCENE_REQUIRED))
			{
				//m_store.StreamingRemove(m_currentSlot); // we're done with the asset
			}

			m_currentSlot++;
		}
		else if (bIsInStreamingImage && maxStreamingSlots > 0)
		{
			if (m_streamingSlots.size() < maxStreamingSlots)
			{
				s32 flags = STRFLAG_PRIORITY_LOAD|STRFLAG_CUTSCENE_REQUIRED;//|STRFLAG_DONTDELETE;

				if (m_forceLoad)
				{
					flags |= STRFLAG_FORCE_LOAD; // not sure what this does ..
				}

				m_store.StreamingRequest(m_currentSlot, flags);
				m_streamingSlots.PushAndGrow(CDTVStreamingIteratorSlot(m_currentSlot++));
			}
			else // too many streaming slots .. give up for now
			{
				break;
			}
		}
		else // not loaded, but not in streaming image (or we're not handling streaming) .. fail with frames=-1 (maybe it's a file?)
		{
			m_func(NULL, false, m_currentSlot++, -1, m_user);
		}
	}

	if (m_currentSlot >= m_store.GetSize() && m_streamingSlots.size() == 0)
	{
		m_enabled     = false;
		m_currentSlot = 0;

		m_func(NULL, false, -1, -1, m_user); // update finished

		return true; // done
	}

	return false; // not done yet
}

// instantiate these types of streaming iterators
template class CDTVStreamingIterator<fwTxdStore     ,fwTxd            ,fwTxdDef     >;
template class CDTVStreamingIterator<fwDwdStore     ,Dwd              ,fwDwdDef     >;
template class CDTVStreamingIterator<fwDrawableStore,Drawable         ,fwDrawableDef>;
template class CDTVStreamingIterator<fwFragmentStore,Fragment         ,fwFragmentDef>;
template class CDTVStreamingIterator<ptfxAssetStore ,ptxFxList        ,ptxFxListDef >;
template class CDTVStreamingIterator<fwMapDataStore ,fwMapDataContents,fwMapDataDef >;

#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR
