// ====================================================
// debug/textureviewer/textureviewerstreamingiterator.h
// (c) 2010 RockstarNorth
// ====================================================

#ifndef _DEBUG_TEXTUREVIEWER_STREAMINGITERATOR_H_
#define _DEBUG_TEXTUREVIEWER_STREAMINGITERATOR_H_

#include "debug/textureviewer/textureviewerprivate.h"

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR

#include "atl/array.h"
#include "atl/string.h"

#define STREAMING_ITERATOR_NO_ACCESS (0) // debugging

enum
{
	SI_SEARCH_FLAG_SKIP_VEHICLES = BIT(0),
	SI_SEARCH_FLAG_SKIP_PEDS     = BIT(1),
	SI_SEARCH_FLAG_SKIP_PROPS    = BIT(2),
	SI_SEARCH_FLAG_DONT_SKIP     = BIT(3), // flip the skip test
};

void CDTVStreamingIterator_Flush();

extern bool gStreamingIterator_LoadMapDataExterior;
extern bool gStreamingIterator_LoadMapDataInterior;
extern bool gStreamingIterator_LoadMapDataNonHD;

class CDTVStreamingIteratorSlot
{
public:
	CDTVStreamingIteratorSlot() {}
	CDTVStreamingIteratorSlot(int slot) : m_slot(slot), m_frames(0) {}

	int m_slot;
	int m_frames;
};

template <typename AssetStoreType, typename AssetType, typename AssetDefType> class CDTVStreamingIterator
{
public:
	typedef void (*IteratorFuncType)(AssetDefType* def, bool bIsParentDef, int slot, int frames, void* user); // if not streamed, frames will be 0

	CDTVStreamingIterator(void* user, int& currentSlot, AssetStoreType& store, IteratorFuncType func, const char* searchRPF, const char* searchName, u32& searchFlags, int maxFrames = 512);

	void Reset();
	bool Update(int count = 5, int maxStreamingSlots = 8); // returns true when done

	bool                            m_enabled;
	void*                           m_user;
	IteratorFuncType                m_func; // gets called on each asset once it's been streamed in
	AssetStoreType&                 m_store;
	int&                            m_currentSlot;
	bool                            m_immediate; // calls LoadAllRequestedObjects immediately after streaming req. (NOT IMPLEMENTED)
	bool                            m_forceLoad; // streaming request flag STRFLAG_FORCE_LOAD
	const char*                     m_searchRPF;
	const char*                     m_searchName;
	u32&                            m_searchFlags;
	int                             m_maxFrames; // max frames to wait for streaming requests
	atArray<CDTVStreamingIteratorSlot> m_streamingSlots; // should be a ringbuffer ..
	atArray<atString>               m_brokenAssetNames; // don't try to load these or stream them in
	atArray<atString>               m_brokenRPFNames;
};

namespace rage { class fwTxdStore; class fwTxdDef; class grcTexture; }
namespace rage { class fwDwdStore; class DwdDef; }
namespace rage { class fwDrawableStore; class rmcDrawable; typedef rmcDrawable Drawable; class fwDrawableDef; }
namespace rage { class fwFragmentStore; class fragType; typedef fragType Fragment; class fwFragmentDef; }
namespace rage { class ptfxAssetStore; class ptxFxList; class ptxFxListDef; }
namespace rage { class fwMapDataStore; class fwMapDataContents; class fwMapDataDef; }

namespace rage { template <typename> class pgDictionary; typedef pgDictionary<grcTexture> fwTxd; }
namespace rage { template <typename> class pgDictionary; typedef pgDictionary<Drawable> Dwd; }

// instantiate these types of streaming iterators
typedef CDTVStreamingIterator<fwTxdStore     ,fwTxd            ,fwTxdDef     > CDTVStreamingIterator_TxdStore     ;
typedef CDTVStreamingIterator<fwDwdStore     ,Dwd              ,DwdDef       > CDTVStreamingIterator_DwdStore     ;
typedef CDTVStreamingIterator<fwDrawableStore,Drawable         ,fwDrawableDef> CDTVStreamingIterator_DrawableStore;
typedef CDTVStreamingIterator<fwFragmentStore,Fragment         ,fwFragmentDef> CDTVStreamingIterator_FragmentStore;
typedef CDTVStreamingIterator<ptfxAssetStore ,ptxFxList        ,ptxFxListDef > CDTVStreamingIterator_ParticleStore;
typedef CDTVStreamingIterator<fwMapDataStore ,fwMapDataContents,fwMapDataDef > CDTVStreamingIterator_MapDataStore ;

#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR
#endif // _DEBUG_TEXTUREVIEWER_STREAMINGITERATOR_H_
