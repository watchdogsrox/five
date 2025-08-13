// =======================================
// debug/AssetAnalysis/AssetAnalysisUtil.h
// (c) 2013 RockstarNorth
// =======================================

#ifndef _DEBUG_ASSETANALYSIS_ASSETANALYSISUTIL_H_
#define _DEBUG_ASSETANALYSIS_ASSETANALYSISUTIL_H_

#if __BANK

#include "atl/string.h"
#include "vectormath/vec4v.h"

#include "debug/AssetAnalysis/AssetAnalysisCommon.h"

namespace rage { class fwTxdStore; class fwTxdDef; class grcTexture; }
namespace rage { class fwDwdStore; class DwdDef; }
namespace rage { class fwDrawableStore; class rmcDrawable; typedef rmcDrawable Drawable; class fwDrawableDef; }
namespace rage { class fwFragmentStore; class fragType; typedef fragType Fragment; class fwFragmentDef; }
namespace rage { class ptfxAssetStore; class ptxFxList; class ptxFxListDef; }
namespace rage { class fwMapDataStore; class fwMapDataContents; class fwMapDataDef; }
namespace rage { class fwMapTypesStore; class fwMapTypesContents; class fwMapTypesDef; }

namespace rage { template <typename> class pgDictionary; typedef pgDictionary<grcTexture> fwTxd; }
namespace rage { template <typename> class pgDictionary; typedef pgDictionary<Drawable> Dwd; }

namespace rage { class grcTexture; }

class CDebugArchetypeProxy;
class CBaseModelInfo;

namespace AssetAnalysis {

inline eAssetType GetAssetType(fwTxdStore     &) { return ASSET_TYPE_TXD     ; }
inline eAssetType GetAssetType(fwDwdStore     &) { return ASSET_TYPE_DWD     ; }
inline eAssetType GetAssetType(fwDrawableStore&) { return ASSET_TYPE_DRAWABLE; }
inline eAssetType GetAssetType(fwFragmentStore&) { return ASSET_TYPE_FRAGMENT; }
inline eAssetType GetAssetType(ptfxAssetStore &) { return ASSET_TYPE_PARTICLE; }
inline eAssetType GetAssetType(fwMapDataStore &) { return ASSET_TYPE_MAPDATA ; }

const char* GetRPFPath(fwTxdStore     & store, int slot);
const char* GetRPFPath(fwDwdStore     & store, int slot);
const char* GetRPFPath(fwDrawableStore& store, int slot);
const char* GetRPFPath(fwFragmentStore& store, int slot);
const char* GetRPFPath(ptfxAssetStore & store, int slot);
const char* GetRPFPath(fwMapDataStore & store, int slot);
const char* GetRPFPath(fwMapTypesStore& store, int slot);

atString GetArchetypePath(const CDebugArchetypeProxy* arch);

atString GetDrawablePath(const CBaseModelInfo* modelInfo);

enum
{
	FRAGMENT_COMMON_DRAWABLE_ENTRY = -1,
	FRAGMENT_CLOTH_DRAWABLE_ENTRY  = -2,
};

atString GetAssetPath(fwTxdStore     & store, int slot, int entry = -1);
atString GetAssetPath(fwDwdStore     & store, int slot, int entry = -1);
atString GetAssetPath(fwDrawableStore& store, int slot, int entry = -1);
atString GetAssetPath(fwFragmentStore& store, int slot, int entry = -1);
atString GetAssetPath(ptfxAssetStore & store, int slot, int entry = -1);
atString GetAssetPath(fwMapDataStore & store, int slot, int entry = -1);
atString GetAssetPath(fwMapTypesStore& store, int slot, int entry = -1);

enum eAssetSize
{
	ASSET_SIZE_PHYSICAL = 0,
	ASSET_SIZE_VIRTUAL  = 1,
};

u32 GetAssetSize(fwTxdStore     & store, int slot, eAssetSize as);
u32 GetAssetSize(fwDwdStore     & store, int slot, eAssetSize as);
u32 GetAssetSize(fwDrawableStore& store, int slot, eAssetSize as);
u32 GetAssetSize(fwFragmentStore& store, int slot, eAssetSize as);
u32 GetAssetSize(ptfxAssetStore & store, int slot, eAssetSize as);
u32 GetAssetSize(fwMapDataStore & store, int slot, eAssetSize as);

atString GetTextureName(const grcTexture* texture);

atString GetTexturePath(fwTxdStore     & store, int slot, int entry, const Drawable* drawable, const grcTexture* texture, u32 textureKey = 0, bool* bUsesNonPackedTextures = NULL);
atString GetTexturePath(fwDwdStore     & store, int slot, int entry, const Drawable* drawable, const grcTexture* texture, u32 textureKey = 0, bool* bUsesNonPackedTextures = NULL);
atString GetTexturePath(fwDrawableStore& store, int slot, int entry, const Drawable* drawable, const grcTexture* texture, u32 textureKey = 0, bool* bUsesNonPackedTextures = NULL);
atString GetTexturePath(fwFragmentStore& store, int slot, int entry, const Drawable* drawable, const grcTexture* texture, u32 textureKey = 0, bool* bUsesNonPackedTextures = NULL);
atString GetTexturePath(ptfxAssetStore & store, int slot, int entry, const Drawable* drawable, const grcTexture* texture, u32 textureKey = 0, bool* bUsesNonPackedTextures = NULL);

atString GetTexturePath(const fwArchetype* archetype, const grcTexture* texture);

u32 GetTexturePixelDataHash(const grcTexture* texture, bool bHashName, int numLines);

const CDebugArchetypeProxy* GetArchetypeProxyForDrawable(fwDwdStore     & store, int slot, int entry = -1);
const CDebugArchetypeProxy* GetArchetypeProxyForDrawable(fwDrawableStore& store, int slot, int entry = -1);
const CDebugArchetypeProxy* GetArchetypeProxyForDrawable(fwFragmentStore& store, int slot, int entry = -1);
const CDebugArchetypeProxy* GetArchetypeProxyForDrawable(ptfxAssetStore & store, int slot, int entry = -1);

int GetParentTxdSlot(fwTxdStore     &, int slot);
int GetParentTxdSlot(fwDwdStore     &, int slot);
int GetParentTxdSlot(fwDrawableStore&, int slot);
int GetParentTxdSlot(fwFragmentStore&, int slot);
int GetParentTxdSlot(ptfxAssetStore &, int slot);

Vec4V_Out GetTextureAverageColour(const grcTexture* texture, Vec4V* colourMinOut = NULL, Vec4V* colourMaxOut = NULL, Vec4V* colourDiffOut_RG_GB_BA_AR = NULL, Vec2V* colourDiffOut_RB_GA = NULL);
void GetTextureRGBMinMaxHistogramRaw(const grcTexture* texture, u16& rgbmin, u16& rgbmax, int* rhist32 = NULL, int* ghist64 = NULL, int* bhist32 = NULL);

} // namespace AssetAnalysis

#endif // __BANK
#endif // _DEBUG_ANALYSIS_ASSETANALYSISUTIL_H_
