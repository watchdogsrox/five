// =========================================
// debug/textureviewer/textureviewersearch.h
// (c) 2010 RockstarNorth
// =========================================

#ifndef _DEBUG_TEXTUREVIEWER_TEXTUREVIEWERSEARCH_H_
#define _DEBUG_TEXTUREVIEWER_TEXTUREVIEWERSEARCH_H_

#include "debug/textureviewer/textureviewerprivate.h"

#if __BANK

#include "atl/string.h"
#include "fwutil/xmacro.h"
#include "scene/EntityTypes.h"
namespace rage { template <typename,int,typename> class atArray; }
namespace rage { template <typename> class pgDictionary; typedef pgDictionary<grcTexture> fwTxd; }
class CDebugArchetypeProxy;
class CBaseModelInfo;
class CEntity;

// ================================================================================================

enum eAssetType
{
	AST_None      = 0, // NULL
	AST_TxdStore     , // g_TxdStore
	AST_DwdStore     , // g_DwdStore
	AST_DrawableStore, // g_DrawableStore
	AST_FragmentStore, // g_FragmentStore
	AST_ParticleStore, // g_ParticleStore
};

class CAssetRef
{
public:
	CAssetRef(eAssetType assetType = AST_None, int assetIndex = INDEX_NONE)
		: m_assetType  (assetType )
		, m_assetIndex_(assetIndex)
	{
		m_assetName = GetAssetName();
	}

	void Clear() { m_assetType = AST_None; m_assetName = ""; m_assetIndex_ = INDEX_NONE; }

	bool IsNULL() const { return m_assetType == AST_None || m_assetIndex_ == INDEX_NONE; }

	static const char* GetAssetTypeName(eAssetType assetType, bool bAbbrev = false);
	const char*        GetAssetName () const;
	eAssetType         GetAssetType () const;
	strLocalIndex      GetAssetIndex() const; // calls FindSlot to get the index from the name

	int      GetSortKey(int assetEntry = INDEX_NONE) const;
	atString GetString (int assetEntry = INDEX_NONE) const;
	atString GetDesc   (int assetEntry = INDEX_NONE) const; // appends GetString() and GetAssetName()

	int  GetRefCount() const;
	void AddRef(const class CAssetRefInterface& ari) const;
	void RemoveRefWithoutDelete(bool bVerbose) const;

	static bool        IsValidAssetSlot  (eAssetType assetType, strLocalIndex assetIndex);
	static bool        IsInStreamingImage(eAssetType assetType, strLocalIndex assetIndex);
	static strIndex    GetStreamingIndex (eAssetType assetType, strLocalIndex assetIndex); // whatever that means?
	static int         GetParentTxdIndex (eAssetType assetType, strLocalIndex assetIndex);
	static const char* GetRPFName        (eAssetType assetType, strLocalIndex assetIndex);
	static const char* GetRPFPathName    (eAssetType assetType, strLocalIndex assetIndex);

	bool        IsValidAssetSlot  () const { return IsValidAssetSlot  (m_assetType, GetAssetIndex()); }
	bool        IsInStreamingImage() const { return IsInStreamingImage(m_assetType, GetAssetIndex()); }
	strIndex    GetStreamingIndex () const { return GetStreamingIndex (m_assetType, GetAssetIndex()); }
	int         GetParentTxdIndex () const { return GetParentTxdIndex (m_assetType, GetAssetIndex()); }
	const char* GetRPFName        () const { return GetRPFName        (m_assetType, GetAssetIndex()); }
	const char* GetRPFPathName    () const { return GetRPFPathName    (m_assetType, GetAssetIndex()); }

	bool IsAssetLoaded() const;

	int GetStreamingAssetSize(bool bVirtual) const;

	bool operator ==(const CAssetRef& rhs) const { return m_assetType == rhs.m_assetType && m_assetIndex_ == rhs.m_assetIndex_; }
	bool operator !=(const CAssetRef& rhs) const { return m_assetType != rhs.m_assetType || m_assetIndex_ != rhs.m_assetIndex_; }

protected:
	static void Stream_TxdStore     (int assetIndex, bool* bStreamed = NULL);
	static void Stream_DwdStore     (int assetIndex, bool* bStreamed = NULL);
	static void Stream_DrawableStore(int assetIndex, bool* bStreamed = NULL);
	static void Stream_FragmentStore(int assetIndex, bool* bStreamed = NULL);
	static void Stream_ParticleStore(int assetIndex, bool* bStreamed = NULL);

	// NOTE -- m_assetType,m_assetName (or m_assetIndex_) is enough information to specify a
	// particular asset .. however to get a txd from an asset that is itself NOT a txd, then
	// m_assetEntry indicates which drawable or fragment contains the txd.

	eAssetType m_assetType;
	atString   m_assetName;   // use with GetSafeFromName()
	strLocalIndex       m_assetIndex_; // probably valid, but not necessarily .. use m_assetName instead (maybe get rid of this member?)
};

class CAssetRefInterface
{
public:
	CAssetRefInterface(atArray<CAssetRef>& assetRefs, bool bVerbose);

	atArray<CAssetRef>& m_assetRefs;
	bool                m_verbose;
};

class CTxdRef : public CAssetRef
{
public:
	CTxdRef() {}
	CTxdRef(eAssetType assetType, int assetIndex, int assetEntry, const char* info);

	int      GetSortKey() const;
	atString GetString () const;
	atString GetDesc   () const;

	const fwTxd* GetTxd() const; // note this does NOT stream the asset in ..
	const fwTxd* LoadTxd(bool bVerbose, bool* bStreamed = NULL); // need to addref if we store this

	// note that we specifically disregard m_info when comparing CTxdRef's
	bool operator ==(const CTxdRef& rhs) const { return m_assetType == rhs.m_assetType && m_assetIndex_ == rhs.m_assetIndex_ && m_assetEntry == rhs.m_assetEntry; }
	bool operator !=(const CTxdRef& rhs) const { return m_assetType != rhs.m_assetType || m_assetIndex_ != rhs.m_assetIndex_ || m_assetEntry != rhs.m_assetEntry; }

	// required for use as key in atMap .. alternatively we could define an atHash function for this class
	operator int() const { return (((int)m_assetType) << 24) | ((m_assetIndex_.Get() & 0xffff) << 8) | (m_assetEntry & 0xff); }

	int      m_assetEntry; // dwd entry, or fragment extra drawable index (INDEX_NONE for common drawable)
	atString m_info;
};

class CTextureRef : public CTxdRef
{
public:
	CTextureRef() {}
	CTextureRef(const CTxdRef& ref, int txdEntry, const grcTexture* tempTexture);
	CTextureRef(eAssetType assetType, int assetIndex, int assetEntry, const char* info, int txdEntry, const grcTexture* tempTexture);

	int               m_txdEntry;    // which entry in the txd this texture is .. so it can be loaded
	const grcTexture* m_tempTexture; // temporary pointer, do not store this!
};

// ================================================================================================

#if DEBUG_TEXTURE_VIEWER_ENTITY_FILTERING

enum eEntityLODSearchParams
{
	ELODSP_ALL    , // everything
	ELODSP_HD_ONLY, // LODTYPES_DEPTH_HD, LODTYPES_DEPTH_ORPHANHD
	ELODSP_HD_NONE, // LODTYPES_DEPTH_LOD, LODTYPES_DEPTH_SLOD1, LODTYPES_DEPTH_SLOD2, LODTYPES_DEPTH_SLOD3, LODTYPES_DEPTH_SLOD4
	ELODSP_COUNT
};

static inline const char** GetUIStrings_eEntityLODSearchParams()
{
	static const char* strings[] =
	{
		STRING(ALL    ),
		STRING(HD_ONLY),
		STRING(HD_NONE),
		NULL
	};
	return strings;
}

#endif // DEBUG_TEXTURE_VIEWER_ENTITY_FILTERING

enum eModelInfoShowProps
{
	MISP_ALL          ,
	MISP_PROPS_ONLY   ,
	MISP_NONPROPS_ONLY,
	MISP_COUNT
};

static inline const char** GetUIStrings_eModelInfoShowProps()
{
	static const char* strings[] =
	{
		STRING(ALL          ),
		STRING(PROPS_ONLY   ),
		STRING(NONPROPS_ONLY),
		NULL
	};
	return strings;
}

class CAssetSearchParams
{
public:
	enum { _MI_TYPE_COUNT = 9 }; // so we don't need to #include modelinfo.h here

	CAssetSearchParams();

	bool m_exactNameMatch; // name must match exactly
	bool m_showEmptyTxds;  // show txd's which have no textures in them (why do they exist?)
	bool m_showStreamable; // show assets which are not loaded but are potentially streamable (i.e. loadable)
	bool m_selectModelUseShowTxdFlags; // ugh .. controls whether the preceding three flags are used in Populate_SelectModelInfo
	bool m_findDependenciesWhilePopulating; // this can be quite slow, so it's disabled by default

#if DEBUG_TEXTURE_VIEWER_ENTITY_FILTERING
	int  m_entityLODFilter; // eEntityLODSearchParams
	bool m_entityTypeFlags[ENTITY_TYPE_TOTAL];
#endif // DEBUG_TEXTURE_VIEWER_ENTITY_FILTERING

	// flags for FindAssets_ModelInfos (does not affect Populate_SearchModelClick)
	bool m_modelInfoShowLoadedModelsOnly;
	bool m_modelInfoTypeFlags[_MI_TYPE_COUNT];
	int  m_modelInfoShowProps; // eModelInfoShowProps

	// optional flags for FindTextures .. also Populate_SearchTxds (controls what asset stores m_searchFilter applies to, does NOT filter associations)
	bool m_scanTxdStore;
	bool m_scanDwdStore;
	bool m_scanDrawableStore;
	bool m_scanFragmentStore;
	bool m_scanParticleStore;

	// optional ranges for FindAssets_ (does NOT filter associations)
	bool m_scanAssetIndexRangeEnabled;
	int  m_scanAssetIndexRange[2]; // min,max

	// optional filter for FindAssets_, mostly useful for FindAssets_ModelInfo
	int  m_filterAssetType; // eAssetType, AST_None to disable the filter
	char m_filterAssetName[80];
	bool m_filterDwdEntryEnabled;
	int  m_filterDwdEntry; // when searching for models and m_filterAssetType == AST_Dwd

	/*
	To search for models which use a particular drawable entry in a particular dwd, set
	m_searchFilter to "" (empty string), set m_filterAssetType to "Dwd", set m_filterAssetName to
	the name of the dwd you're searching for, set m_filterDwdEntryEnabled to true and set
	m_filterDwdEntry to the entry index of the dwd you're interested in ..
	
	It is also possible to search for models which use any entry of a particular dwd, just set
	m_filterDwdEntryEnabled to false.
	
	It is also possible to search for models which use a particular txd, drawable or fragment by
	setting m_filterAssetType and m_filterAssetName appropriately (keep m_searchFilter = "").
	
	It is also possible to search for txd's which refer to a particular txd as their parent, but
	keep in mind this only finds txd's which are currently loaded. It might be useful to try
	streaming them in using CDTVStreamingIterator .. eventually I'd like to do this.
	
	Similarly is it possible to search for dwd's which refer to a particular txd as their parent,
	and in this case streaming is not an issue since dwd's txd parent can be determined from
	g_DwdStore itself.
	*/

	bool m_createTextureCRCs;
};

// Note that GetAssociatedTxds_ModelInfo takes an 'optional' search params, the logic is a bit
// strange here and might be due for a cleanup at some point. If optional is NULL then it will
// be ignored, and the behaviour will be the same as before. I added this to support searching
// for txd's and textures that are currently loaded and associated with specific model types.
void GetAssociatedTxds_ModelInfo(atArray<CTxdRef>& refs, const CDebugArchetypeProxy* modelInfo, const CEntity* entity, const CAssetSearchParams* optional);
void GetAssociatedTxds_ModelInfo(atArray<CTxdRef>& refs, const CBaseModelInfo      * modelInfo, const CEntity* entity, const CAssetSearchParams* optional);

void GetAssociatedTxds_TxdStore     (atArray<CTxdRef>& refs, int assetIndex, const CAssetSearchParams& asp, const char* info = "", bool bIgnoreParentTxds = false);
void GetAssociatedTxds_DwdStore     (atArray<CTxdRef>& refs, strLocalIndex assetIndex, const CAssetSearchParams& asp, const char* info = "", bool bIgnoreParentTxds = false);
void GetAssociatedTxds_DrawableStore(atArray<CTxdRef>& refs, int assetIndex, const CAssetSearchParams& asp, const char* info = "", bool bIgnoreParentTxds = false);
void GetAssociatedTxds_FragmentStore(atArray<CTxdRef>& refs, int assetIndex, const CAssetSearchParams& asp, const char* info = "", bool bIgnoreParentTxds = false);
void GetAssociatedTxds_ParticleStore(atArray<CTxdRef>& refs, int assetIndex, const CAssetSearchParams& asp, const char* info = "", bool bIgnoreParentTxds = false);

void FindTextures(
	atArray<CTextureRef>& refs,
	const char* searchStr,
	const CAssetSearchParams& asp,
	int maxCount,
	int minSize,
	bool bConstantOnly,
	bool bRedundantOnly,
	bool bReportNonUniqueTextureMemory,
	u32 conversionFlagsRequired,
	u32 conversionFlagsSkipped,
	const char* usage, // e.g. "BumpTex,NormalTexture"
	const char* usageExclusion,
	const char* format, // e.g. "DXT5", etc.
	bool bVerbose
);

void FindAssets_ModelInfo    (atArray<u32>& slots, const char* searchStr, const char* rpf, const CAssetSearchParams& asp, int maxCount);
void FindAssets_TxdStore     (atArray<int>& slots, const char* searchStr, const char* rpf, const CAssetSearchParams& asp, int maxCount, bool bOnlyIfLoaded);
void FindAssets_DwdStore     (atArray<int>& slots, const char* searchStr, const char* rpf, const CAssetSearchParams& asp, int maxCount, bool bOnlyIfLoaded);
void FindAssets_DrawableStore(atArray<int>& slots, const char* searchStr, const char* rpf, const CAssetSearchParams& asp, int maxCount, bool bOnlyIfLoaded);
void FindAssets_FragmentStore(atArray<int>& slots, const char* searchStr, const char* rpf, const CAssetSearchParams& asp, int maxCount, bool bOnlyIfLoaded);
void FindAssets_ParticleStore(atArray<int>& slots, const char* searchStr, const char* rpf, const CAssetSearchParams& asp, int maxCount, bool bOnlyIfLoaded);

void FindAssets_CurrentDependentModelInfos(atArray<u32>& modelInfos, const CTxdRef&    ref    , const char* searchStr, const CAssetSearchParams& asp);
void FindAssets_CurrentDependentModelInfos(atArray<u32>& modelInfos, const grcTexture* texture, const char* searchStr, const CAssetSearchParams& asp);
void FindAssets_CurrentDependentTxdRefs   (atArray<CTxdRef>& refs  , const grcTexture* texture, bool bShowEmptyTxds);

#if __DEV

void PrintAssets_ModelInfo    (const char* searchStr, const CAssetSearchParams& asp);
void PrintAssets_TxdStore     (const char* searchStr, const CAssetSearchParams& asp);
void PrintAssets_DwdStore     (const char* searchStr, const CAssetSearchParams& asp);
void PrintAssets_DrawableStore(const char* searchStr, const CAssetSearchParams& asp);
void PrintAssets_FragmentStore(const char* searchStr, const CAssetSearchParams& asp);
void PrintAssets_ParticleStore(const char* searchStr, const CAssetSearchParams& asp);

#endif // __DEV
#endif // __BANK
#endif // _DEBUG_TEXTUREVIEWER_TEXTUREVIEWERSEARCH_H_
