// =======================================
// debug/AssetAnalysis/StreamingIterator.h
// (c) 2013 RockstarNorth
// =======================================

#ifndef _DEBUG_ASSETANALYSIS_STREAMINGITERATOR_H_
#define _DEBUG_ASSETANALYSIS_STREAMINGITERATOR_H_

#if __BANK

#include "atl/array.h"

#include "fwscene/stores/mapdatastore.h"

namespace rage { class fwTxdStore; class fwTxdDef; class grcTexture; }
namespace rage { class fwDwdStore; class DwdDef; }
namespace rage { class fwDrawableStore; class rmcDrawable; typedef rmcDrawable Drawable; class fwDrawableDef; }
namespace rage { class fwFragmentStore; class fragType; typedef fragType Fragment; class fwFragmentDef; }
namespace rage { class ptfxAssetStore; class ptxFxList; class ptxFxListDef; }
namespace rage { class fwMapDataStore; class fwMapDataContents; class fwMapDataDef; }
namespace rage { class fwMapTypesStore; class fwMapTypesContents; class fwMapTypesDef; }

namespace rage { template <typename> class pgDictionary; typedef pgDictionary<grcTexture> fwTxd; }
namespace rage { template <typename> class pgDictionary; typedef pgDictionary<Drawable> Dwd; }

enum eStreamingIteratorState
{
	STREAMING_ITERATOR_OFF      = BIT(0),
	STREAMING_ITERATOR_RUNNING  = BIT(1),
	STREAMING_ITERATOR_COMPLETE = BIT(3),
};

class CStreamingIteratorSlot
{
public:
	CStreamingIteratorSlot() {}
	CStreamingIteratorSlot(int slot, int frames) : m_slot(slot), m_frames(frames) {}

	strLocalIndex m_slot;
	int m_frames;
};

template <typename AssetStoreType, typename AssetType, typename AssetDefType> class CStreamingIterator
{
public:
	CStreamingIterator(AssetStoreType& store, void* userData);

	bool& GetEnabledRef() { return m_enabled; }
	int& GetCurrentSlotRef() { return m_currentSlot; }

	typedef bool (*FilterFuncType)(const AssetDefType* def, const char* rpfPath, const char* assetName, int slot, void* userData);
	typedef void (*IteratorFuncType)(const AssetDefType* def, int slot, int frames, void* userData);
	typedef void (*CompleteFuncType)(const atArray<CStreamingIteratorSlot>& failedSlots, void* userData);

	void SetFilterFunc(FilterFuncType func) { m_filterFunc = func; }
	void SetIteratorFunc(IteratorFuncType func) { m_iteratorFunc = func; }
	void SetCompleteFunc(CompleteFuncType func) { m_completeFunc = func; }

	eStreamingIteratorState Update();
	void Reset();

	void SetSlotsStreamedAndProcessedBits(u8* bits);

	const atArray<CStreamingIteratorSlot>* GetFailedSlots() const { return m_failedSlots.GetCount() > 0 ? &m_failedSlots : NULL; }

private:
	AssetStoreType& m_store;

	bool  m_enabled;
	void* m_userData;
	int   m_currentSlot;
	int   m_maxNewRequests; // max slots requested in one update
	int   m_maxStreamingSlots; // max in-flight streaming slots
	int   m_maxFramesToWait;

	FilterFuncType   m_filterFunc;
	IteratorFuncType m_iteratorFunc;
	CompleteFuncType m_completeFunc;

	u8* m_slotsStreamedAndProcessed;

	atArray<CStreamingIteratorSlot> m_streamingSlots; // in-flight slots waiting to be streamed in
	atArray<CStreamingIteratorSlot> m_failedSlots;
};

extern CStreamingIterator<fwTxdStore     ,fwTxd            ,fwTxdDef     > g_TxdIterator     ;
extern CStreamingIterator<fwDwdStore     ,Dwd              ,DwdDef       > g_DwdIterator     ;
extern CStreamingIterator<fwDrawableStore,Drawable         ,fwDrawableDef> g_DrawableIterator;
extern CStreamingIterator<fwFragmentStore,Fragment         ,fwFragmentDef> g_FragmentIterator;
extern CStreamingIterator<ptfxAssetStore ,ptxFxList        ,ptxFxListDef > g_ParticleIterator;
extern CStreamingIterator<fwMapDataStore ,fwMapDataContents,fwMapDataDef > g_MapDataIterator ;

#define STRFLAG_STREAMING_ITERATOR (STRFLAG_DONTDELETE|STRFLAG_CUTSCENE_REQUIRED)

#define SI_SCRIPT_MAPDATA_USE_COMPENTITY (0) // TODO -- this makes shit really slow, but without it it will crash (unless you set -numStreamedSlots=50000)

template <typename AssetStoreType, typename AssetType, typename AssetDefType> CStreamingIterator<AssetStoreType,AssetType,AssetDefType>::CStreamingIterator(AssetStoreType& store, void* userData) : m_store(store)
{
	m_enabled           = false;
	m_userData          = userData;
	m_currentSlot       = 0;
	m_maxNewRequests    = 5;
	m_maxStreamingSlots = 8;
	m_maxFramesToWait   = 512;
	m_filterFunc        = NULL;
	m_iteratorFunc      = NULL;

	if ((const void*)&m_store == &g_MapDataStore) // mapdata streams in one slot at a time
	{
		m_maxNewRequests    = 1;
		m_maxStreamingSlots = 1;
	}

	m_slotsStreamedAndProcessed = NULL;
}

template <typename AssetStoreType, typename AssetType, typename AssetDefType> eStreamingIteratorState CStreamingIterator<AssetStoreType,AssetType,AssetDefType>::Update()
{
	if (m_enabled)
	{
		for (int i = 0; i < m_streamingSlots.size(); i++) // handle streaming slots
		{
			CStreamingIteratorSlot& sis = m_streamingSlots[i];
			const AssetType* asset = m_store.GetSafeFromIndex(sis.m_slot);
			sis.m_frames++;

			if (asset != NULL || sis.m_frames >= m_maxFramesToWait)
			{
				if (m_iteratorFunc)
				{
					m_iteratorFunc(m_store.GetSlot(sis.m_slot), sis.m_slot.Get(), sis.m_frames, m_userData);
				}

				if (asset == NULL)
				{
					m_failedSlots.PushAndGrow(sis);
				}

#if SI_SCRIPT_MAPDATA_USE_COMPENTITY
				if ((const void*)&m_store == &g_MapDataStore && g_MapDataStore.GetSlot(sis.m_slot)->GetIsScriptManaged())
				{
					// see RemoveIpl
					CCompEntity::UpdateCompEntitiesUsingGroup(sis.m_slot, CE_STATE_ABANDON);
					g_MapDataStore.RemoveGroup(sis.m_slot, g_MapDataStore.GetName(sis.m_slot));
				}
				else
#endif // SI_SCRIPT_MAPDATA_USE_COMPENTITY
				{
					m_store.ClearRequiredFlag(sis.m_slot.Get(), STRFLAG_STREAMING_ITERATOR);
				}

				if (m_slotsStreamedAndProcessed)
				{
					m_slotsStreamedAndProcessed[sis.m_slot.Get()/8] |= BIT(sis.m_slot.Get()%8);
				}

				const CStreamingIteratorSlot sis2 = m_streamingSlots.Pop();

				if (i < m_streamingSlots.size())
				{
					m_streamingSlots[i] = sis2;
				}
			}
		}

		for (int i = 0; i < m_maxNewRequests && m_currentSlot < m_store.GetSize(); i++)
		{
			bool bSkipSlot = false;

			if (!m_store.IsValidSlot(strLocalIndex(strLocalIndex(m_currentSlot))))
			{
				bSkipSlot = true;
			}
			else if ((const void*)&m_store == &g_TxdStore && g_TxdStore.GetSlot(strLocalIndex(m_currentSlot))->m_isDummy)
			{
				bSkipSlot = true;
			}
			else if ((const void*)&m_store == &g_MapDataStore && !g_MapDataStore.GetSlot(strLocalIndex(m_currentSlot))->GetIsValid())
			{
				bSkipSlot = true;
			}
			else if (m_filterFunc)
			{
				const strIndex          streamingIndex = m_store.GetStreamingIndex(strLocalIndex(m_currentSlot));
				const strStreamingInfo* streamingInfo  = strStreamingEngine::GetInfo().GetStreamingInfo(streamingIndex);
				const strStreamingFile* streamingFile  = strPackfileManager::GetImageFileFromHandle(streamingInfo->GetHandle());
				const char*             rpfPath        = streamingFile ? streamingFile->m_name.GetCStr() : "";
				const char*             assetName      = m_store.GetName(strLocalIndex(m_currentSlot));

				if (!m_filterFunc(m_store.GetSlot(strLocalIndex(m_currentSlot)), rpfPath, assetName, m_currentSlot, m_userData))
				{
					bSkipSlot = true;
				}
			}

			if (bSkipSlot)
			{
				i--;
				m_currentSlot++;
				continue;
			}

			const bool bIsInStreamingImage = m_store.IsObjectInImage(strLocalIndex(m_currentSlot));
			const AssetType* asset = m_store.GetSafeFromIndex(strLocalIndex(m_currentSlot));

			if (asset) // it's already loaded, no need to issue a streaming request
			{
				if (m_iteratorFunc)
				{
					m_iteratorFunc(m_store.GetSlot(strLocalIndex(m_currentSlot)), m_currentSlot, 0, m_userData);
				}

				m_currentSlot++;
			}
			else if (bIsInStreamingImage && m_maxStreamingSlots > 0)
			{
				if (m_streamingSlots.size() < m_maxStreamingSlots)
				{
#if SI_SCRIPT_MAPDATA_USE_COMPENTITY
					if ((const void*)&m_store == &g_MapDataStore && g_MapDataStore.GetSlot(m_currentSlot)->GetIsScriptManaged())
					{
						// see RequestIpl
						g_MapDataStore.RequestGroup(m_currentSlot, g_MapDataStore.GetName(m_currentSlot));
						CCompEntity::UpdateCompEntitiesUsingGroup(m_currentSlot, CE_STATE_INIT);
					}
					else
#endif // SI_SCRIPT_MAPDATA_USE_COMPENTITY
					{
						m_store.StreamingRequest(strLocalIndex(m_currentSlot), STRFLAG_PRIORITY_LOAD | STRFLAG_FORCE_LOAD | STRFLAG_STREAMING_ITERATOR);
					}

					m_streamingSlots.PushAndGrow(CStreamingIteratorSlot(m_currentSlot, 0));
					m_currentSlot++;
				}
				else // too many streaming slots .. give up for now (we'll try again next update)
				{
					break;
				}
			}
			else // not loaded, but not in streaming image (or we're not handling streaming) .. fail with frames=-1 (maybe it's a file?)
			{
				if (m_iteratorFunc)
				{
					m_iteratorFunc(NULL, m_currentSlot, -1, m_userData);
				}

				m_failedSlots.PushAndGrow(CStreamingIteratorSlot(m_currentSlot, -1));
				m_currentSlot++;
			}
		}

		if (m_currentSlot >= m_store.GetSize() && m_streamingSlots.size() == 0)
		{
			if (m_completeFunc)
			{
				m_completeFunc(m_failedSlots, m_userData);
			}

			m_enabled     = false;
			m_currentSlot = 0;

			return STREAMING_ITERATOR_COMPLETE;
		}

		return STREAMING_ITERATOR_RUNNING;
	}

	return STREAMING_ITERATOR_OFF;
}

template <typename AssetStoreType, typename AssetType, typename AssetDefType> void CStreamingIterator<AssetStoreType,AssetType,AssetDefType>::Reset()
{
	for (int i = 0; i < m_streamingSlots.size(); i++) // handle streaming slots
	{
		const CStreamingIteratorSlot& sis = m_streamingSlots[i];

		if (m_iteratorFunc)
		{
			m_iteratorFunc(NULL, sis.m_slot.Get(), sis.m_frames, m_userData);
		}

		m_failedSlots.PushAndGrow(sis);

#if SI_SCRIPT_MAPDATA_USE_COMPENTITY
		if ((const void*)&m_store == &g_MapDataStore && g_MapDataStore.GetSlot(sis.m_slot)->GetIsScriptManaged())
		{
			// see RemoveIpl
			CCompEntity::UpdateCompEntitiesUsingGroup(sis.m_slot, CE_STATE_ABANDON);
			g_MapDataStore.RemoveGroup(sis.m_slot, g_MapDataStore.GetName(sis.m_slot));
		}
		else
#endif // SI_SCRIPT_MAPDATA_USE_COMPENTITY
		{
			m_store.ClearRequiredFlag(sis.m_slot.Get(), STRFLAG_STREAMING_ITERATOR);
		}
	}

	if (m_completeFunc)
	{
		m_completeFunc(m_failedSlots, m_userData);
	}

	m_streamingSlots.clear();
	m_failedSlots.clear();

	m_enabled     = false;
	m_currentSlot = 0;

	// TODO -- memset m_slotsStreamedAndProcessed to zero
}

template <typename AssetStoreType, typename AssetType, typename AssetDefType> void CStreamingIterator<AssetStoreType,AssetType,AssetDefType>::SetSlotsStreamedAndProcessedBits(u8* bits)
{
	m_slotsStreamedAndProcessed = bits;
}

#endif // __BANK
#endif // _DEBUG_ASSETANALYSIS_STREAMINGITERATOR_H_
