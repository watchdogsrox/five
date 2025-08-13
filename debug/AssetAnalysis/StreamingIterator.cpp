// =========================================
// debug/AssetAnalysis/StreamingIterator.cpp
// (c) 2013 RockstarNorth
// =========================================

#if __BANK

#include "fwscene/stores/txdstore.h"
#include "fwscene/stores/dwdstore.h"
#include "fwscene/stores/drawablestore.h"
#include "fwscene/stores/fragmentstore.h"
#include "fwscene/stores/mapdatastore.h"
#include "vfx/ptfx/ptfxmanager.h"
#include "streaming/packfilemanager.h"

#include "debug/AssetAnalysis/StreamingIterator.h"

CStreamingIterator<fwTxdStore     ,fwTxd            ,fwTxdDef     > g_TxdIterator     (g_TxdStore     , NULL);
CStreamingIterator<fwDwdStore     ,Dwd              ,DwdDef       > g_DwdIterator     (g_DwdStore     , NULL);
CStreamingIterator<fwDrawableStore,Drawable         ,fwDrawableDef> g_DrawableIterator(g_DrawableStore, NULL);
CStreamingIterator<fwFragmentStore,Fragment         ,fwFragmentDef> g_FragmentIterator(g_FragmentStore, NULL);
CStreamingIterator<ptfxAssetStore ,ptxFxList        ,ptxFxListDef > g_ParticleIterator(g_ParticleStore, NULL);
CStreamingIterator<fwMapDataStore ,fwMapDataContents,fwMapDataDef > g_MapDataIterator (g_MapDataStore , NULL);

#endif // __BANK
