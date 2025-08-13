// ========================================================
// debug/textureviewer/textureviewerstreamingiteratortest.h
// (c) 2010 RockstarNorth
// ========================================================

#ifndef _DEBUG_TEXTUREVIEWER_STREAMINGITERATORTEST_H_
#define _DEBUG_TEXTUREVIEWER_STREAMINGITERATORTEST_H_

#if __BANK

#include "debug/textureviewer/textureviewerstreamingiterator.h"

// ================================================================================================

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEST

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TXDCHILDREN
void _CDTVStreamingIteratorTest_TxdChildrenMap_Print();
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TXDCHILDREN

class CDTVStreamingIteratorTest_TxdStore
{
public:
	CDTVStreamingIteratorTest_TxdStore();

	static void _Func(fwTxdDef* def, bool bIsParentDef, int slot, int frames, void* user) { static_cast<CDTVStreamingIteratorTest_TxdStore*>(user)->IteratorFunc(def, bIsParentDef, slot, frames); }
	void IteratorFunc(fwTxdDef* def, bool bIsParentDef, int slot, int frames);
	void Reset();
	void Update();

	CDTVStreamingIterator_TxdStore* m_si;

	bool m_enabled;
	int m_currentSlot;

private:
	int m_numTxds;
	int m_numStreamed;
	int m_numTxdParents;
	int m_numTxdParentSlots;
	int m_numNonEmptyTxds;
	int m_numTextures;
	int m_maxTextures;
	int m_numFailed; // failed to stream in (this is bad)
	int m_numNotInImage; // not in streaming image (probably not a big deal)
};

// =====

class CDTVStreamingIteratorTest_DwdStore
{
public:
	CDTVStreamingIteratorTest_DwdStore();

	static void _Func(fwDwdDef* def, bool bIsParentDef, strLocalIndex slot, int frames, void* user) { static_cast<CDTVStreamingIteratorTest_DwdStore*>(user)->IteratorFunc(def, bIsParentDef, slot, frames); }
	void IteratorFunc(fwDwdDef* def, bool bIsParentDef, strLocalIndex slot, int frames);
	void Reset();
	void Update();

	CDTVStreamingIterator_DwdStore* m_si;

	bool m_enabled;
	int m_currentSlot;

private:
	int m_numDwds;
	int m_numStreamed;
	int m_numDwdParents;
	int m_numTxdParentSlots;
	int m_numDrawables;
	int m_maxDrawables;
	int m_numTxds;
	int m_maxTxds;
	int m_numNonEmptyTxds;
	int m_maxNonEmptyTxds;
	int m_numTextures;
	int m_numFailed; // failed to stream in (this is bad)
	int m_numNotInImage; // not in streaming image (probably not a big deal)
};

// =====

class CDTVStreamingIteratorTest_DrawableStore
{
public:
	CDTVStreamingIteratorTest_DrawableStore();

	static void _Func(fwDrawableDef* def, bool bIsParentDef, int slot, int frames, void* user) { static_cast<CDTVStreamingIteratorTest_DrawableStore*>(user)->IteratorFunc(def, bIsParentDef, slot, frames); }
	void IteratorFunc(fwDrawableDef* def, bool bIsParentDef, int slot, int frames);
	void Reset();
	void Update();

	CDTVStreamingIterator_DrawableStore* m_si;

	bool m_enabled;
	int m_currentSlot;

private:
	int m_numDrawables;
	int m_numStreamed;
	int m_numTxdParentSlots;
	int m_numTxds;
	int m_numNonEmptyTxds;
	int m_numTextures;
	int m_maxTextures;
	int m_numFailed; // failed to stream in (this is bad)
	int m_numNotInImage; // not in streaming image (probably not a big deal)
};

// =====

class CDTVStreamingIteratorTest_FragmentStore
{
public:
	CDTVStreamingIteratorTest_FragmentStore();

	static void _Func(fwFragmentDef* def, bool bIsParentDef, int slot, int frames, void* user) { static_cast<CDTVStreamingIteratorTest_FragmentStore*>(user)->IteratorFunc(def, bIsParentDef, slot, frames); }
	void IteratorFunc(fwFragmentDef* def, bool bIsParentDef, int slot, int frames);
	void Reset();
	void Update();

	CDTVStreamingIterator_FragmentStore* m_si;

	bool m_enabled;
	int m_currentSlot;

private:
	int m_numFragments;
	int m_numStreamed;
	int m_numTxdParentSlots;
	int m_numTxds;
	int m_numNonEmptyTxds;
	int m_numTextures;
	int m_maxTextures;
	int m_numExtraDrawables;
	int m_maxExtraDrawables;
	int m_numExtraDrawablesWithTxds;
	int m_numExtraDrawablesWithNonEmptyTxds;
	int m_numFailed; // failed to stream in (this is bad)
	int m_numNotInImage; // not in streaming image (probably not a big deal)
};

// =====

class CDTVStreamingIteratorTest_ParticleStore
{
public:
	CDTVStreamingIteratorTest_ParticleStore();

	static void _Func(ptxFxListDef* def, bool bIsParentDef, int slot, int frames, void* user) { static_cast<CDTVStreamingIteratorTest_ParticleStore*>(user)->IteratorFunc(def, bIsParentDef, slot, frames); }
	void IteratorFunc(ptxFxListDef* def, bool bIsParentDef, int slot, int frames);
	void Reset();
	void Update();

	CDTVStreamingIterator_ParticleStore* m_si;

	bool m_enabled;
	int m_currentSlot;

private:
	int m_numParticles;
	int m_numStreamed;
	int m_numTxds;
	int m_numNonEmptyTxds;
	int m_numTextures;
	int m_maxTextures;
	int m_numDrawables;
	int m_maxDrawables;
	int m_numDrawablesWithTxds;
	int m_numDrawablesWithNonEmptyTxds;
	int m_numFailed; // failed to stream in (this is bad)
	int m_numNotInImage; // not in streaming image (probably not a big deal)
};

// =====

class CDTVStreamingIteratorTest_MapDataStore
{
public:
	CDTVStreamingIteratorTest_MapDataStore();

	static void _Func(fwMapDataDef* def, bool bIsParentDef, int slot, int frames, void* user) { static_cast<CDTVStreamingIteratorTest_MapDataStore*>(user)->IteratorFunc(def, bIsParentDef, slot, frames); }
	void IteratorFunc(fwMapDataDef* def, bool bIsParentDef, int slot, int frames);
	void Reset();
	void Update(int count, int maxStreamingSlots);

	CDTVStreamingIterator_MapDataStore* m_si;

	bool m_enabled;
	int m_currentSlot;

private:
	u8* m_slotsProcessed;
	int m_slotsProcessedSize;

	int m_numMapDatas;
	int m_numStreamed;
	int m_numFailed; // failed to stream in (this is bad)
	int m_numNotInImage; // not in streaming image (probably not a big deal)
};

extern bool gStreamingIteratorTest_Verbose;
extern bool gStreamingIteratorTest_VerboseErrors;
extern char gStreamingIteratorTest_SearchRPF[80];
extern char gStreamingIteratorTest_SearchName[80];
extern u32  gStreamingIteratorTest_SearchFlags;

extern bool gStreamingIteratorTest_FindUncompressedTexturesNew;
extern char gStreamingIteratorTest_FindShaderInDrawables[80];
extern char gStreamingIteratorTest_FindTextureInDrawables[80];
extern char gStreamingIteratorTest_FindArchetypeInMapData[80];

extern bool gStreamingIteratorTest_CheckCableDrawables;
extern bool gStreamingIteratorTest_CheckCableDrawablesSimple;

extern bool gStreamingIteratorTest_MapDataCountInstances;
extern bool gStreamingIteratorTest_MapDataCreateEntities;
extern bool gStreamingIteratorTest_MapDataProcessParents;
extern bool gStreamingIteratorTest_MapDataShowAllEntities;
extern bool gStreamingIteratorTest_MapDataRemoveEntities;
extern bool gStreamingIteratorTest_MapDataDumpTCModBoxes;

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TXDCHILDREN
extern bool gStreamingIteratorTest_TxdChildren;
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TXDCHILDREN

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_REDUNDANT
extern bool gStreamingIteratorTest_FindRedundantTextures;
extern bool gStreamingIteratorTest_FindRedundantTexturesByHash;
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_REDUNDANT

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEST && DEBUG_TEXTURE_VIEWER_FIND_INEFFECTIVE_TEXTURES
extern bool gStreamingIteratorTest_FindIneffectiveTexturesBumpTex;
extern bool gStreamingIteratorTest_FindIneffectiveTexturesSpecularTex;
extern bool gStreamingIteratorTest_FindIneffectiveTexturesCheckFlat;
extern bool gStreamingIteratorTest_FindIneffectiveTexturesCheckFlatGT4x4;
extern bool gStreamingIteratorTest_FindNonDefaultSpecularMasks;
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEST && DEBUG_TEXTURE_VIEWER_FIND_INEFFECTIVE_TEXTURES

extern bool gStreamingIteratorTest_BuildAssetAnalysisData; // new system hooked up to old system ..
#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS
extern bool gStreamingIteratorTest_BuildTextureUsageMap;
extern int  gStreamingIteratorTest_BuildTextureUsageMapPhase;
extern int  gStreamingIteratorTest_BuildTextureUsageMapTextureHashNumLines; // -1 for entire top mip, 0 for none
extern char gStreamingIteratorTest_BuildTextureUsageMapShaderFilter[256];
extern char gStreamingIteratorTest_BuildTextureUsageMapTextureFilter[256];
extern bool gStreamingIteratorTest_BuildTextureUsageMapDATOutput;
extern bool gStreamingIteratorTest_BuildTextureUsageMapDATFlush;
extern bool gStreamingIteratorTest_BuildTextureUsageMapCSVOutput;
extern bool gStreamingIteratorTest_BuildTextureUsageMapCSVFlush;
extern bool gStreamingIteratorTest_BuildTextureUsageMapRuntimeTest; // too much memory i think
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_FIND_UNUSED
extern bool  gStreamingIteratorTest_FindUnusedSharedTextures;
extern s64   gStreamingIteratorTest_FindUnusedSharedTexturesMemoryUsed;
extern float gStreamingIteratorTest_FindUnusedSharedTexturesMemoryUsedMB;
extern s64   gStreamingIteratorTest_FindUnusedSharedTexturesMemoryUnused;
extern float gStreamingIteratorTest_FindUnusedSharedTexturesMemoryUnusedMB;
extern s64   gStreamingIteratorTest_FindUnusedSharedTexturesMemoryTotal;
extern float gStreamingIteratorTest_FindUnusedSharedTexturesMemoryTotalMB;
extern s64   gStreamingIteratorTest_FindUnusedSharedTexturesMemoryNotCounted;
extern float gStreamingIteratorTest_FindUnusedSharedTexturesMemoryNotCountedMB;
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_FIND_UNUSED

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_FIND_UNUSED_DRAWABLES_IN_DWDS
extern bool gStreamingIteratorTest_FindUnusedDrawablesInDwds;
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_FIND_UNUSED_DRAWABLES_IN_DWDS

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEXTURE
extern bool gStreamingIteratorTest_FindBadNormalMaps;
extern bool gStreamingIteratorTest_FindBadNormalMapsCheckAvgColour;
extern bool gStreamingIteratorTest_FindUnusedChannels;
extern bool gStreamingIteratorTest_FindUnusedChannelsSwizzle;
extern bool gStreamingIteratorTest_FindUncompressedTextures;
extern bool gStreamingIteratorTest_FindUnprocessedTextures;
extern bool gStreamingIteratorTest_FindConstantTextures; // find textures where the top mip is a solid colour (or identical DXT blocks)
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEXTURE

// ================================================================================================

/*
This is a half-baked idea for creating hierarchies from the results of the streaming iterator. Part
of this involves displaying hierarchies as ASCII text, which I've implemented separately in another
project, so to make this actually useful I'd need to integrate the code from the other project into
this. Which *would* be kinda cool, I suppose.
*/

#if 0
namespace _StreamingIteratorTest {

class CTxdNode
{
public:
	CTxdNode() {}
	CTxdNode(const fwTxd* txd, int slot);

	bool Insert(CTxdNode* node);
	void Display(const char* prefix, int depth = 0) const;

	int                m_slot;
	int                m_parentSlot;
	atArray<CTxdNode*> m_children;

	atString           m_name;
	bool               m_validSlot;
	int                m_streamIndex;
	bool               m_streamImage;
	int                m_numTextures;
	int                m_sizeInBytes;
	int                m_maxTexDims;
};

class CTxdTree : public CDTVStreamingIterator_TxdStore
{
public:
	CTxdTree();

	static void _Func(fwTxdDef* def, bool bIsParentDef, int slot, int frames, void* user) { reinterpret_cast<CTxdTree*>(user)->IteratorFunc(def, bIsParentDef, slot, frames); }
	void IteratorFunc(fwTxdDef* def, bool bIsParentDef, int slot, int frames);
	void Update();

private:
	int m_currentSlot;
	atArray<CTxdNode*> m_nodes;
};

} // namespace _StreamingIteratorTest
#endif // 0

#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEST
#endif // __BANK
#endif // _DEBUG_TEXTUREVIEWER_STREAMINGITERATORTEST_H_
