// =========================================
// debug/AssetAnalysis/AssetAnalysisCommon.h
// (c) 2013 RockstarNorth
// =========================================

#ifndef _DEBUG_ASSETANALYSIS_ASSETANALYSISCOMMON_H_
#define _DEBUG_ASSETANALYSIS_ASSETANALYSISCOMMON_H_

#if __BANK || defined(ASSET_ANALYSIS_TOOL)

#define ASSET_ANALYSIS_VERSION    (1)
#define ASSET_ANALYSIS_SUBVERSION (1)

#if defined(ASSET_ANALYSIS_TOOL)
#include "../../../tools/common/common.h"
#endif // defined(ASSET_ANALYSIS_TOOL)

namespace AssetAnalysis {

enum eAssetType
{
	ASSET_TYPE_NONE    , // none
	ASSET_TYPE_TXD     , // g_TxdStore
	ASSET_TYPE_DWD     , // g_DwdStore
	ASSET_TYPE_DRAWABLE, // g_DrawableStore
	ASSET_TYPE_FRAGMENT, // g_FragmentStore
	ASSET_TYPE_PARTICLE, // g_ParticleStore
	ASSET_TYPE_MAPDATA , // g_MapDataStore
	ASSET_TYPE_COUNT
};

enum eStreamType
{
	STREAM_TYPE_STRING_TABLE        , // string table
	STREAM_TYPE_ARCHETYPE           , // fwArchetype
	STREAM_TYPE_ARCHETYPE_INSTANCE  , // fwEntity placed in MapData (also includes interior entities)
	STREAM_TYPE_MAPDATA             , // fwMapData (container of archetype instances)
	STREAM_TYPE_TEXTURE             , // grcTexture in a txd
	STREAM_TYPE_TEXTURE_DICTIONARY  , // txd in g_TxdStore or in a drawable
	STREAM_TYPE_DRAWABLE            , // drawable in g_DrawableStore, g_DwdStore, g_FragmentStore
	STREAM_TYPE_DRAWABLE_DICTIONARY , // contains one or more drawables
	STREAM_TYPE_FRAGMENT            , // contains one or more drawables
	STREAM_TYPE_LIGHT               , // CLightAttr in gtaDrawable or gtaFragType
	STREAM_TYPE_PARTICLE            , // 
	STREAM_TYPE_MATERIAL            , // material instance (geometry in a drawable)
	STREAM_TYPE_MATERIAL_TEXTURE_VAR, // texture usage in material
	STREAM_TYPE_LOG_FILE            , // log file
	STREAM_TYPE_COUNT
};

const char* GetStreamName(eStreamType type);

enum eDrawableType
{
	DRAWABLE_TYPE_NONE                     , // 
	DRAWABLE_TYPE_DWD_ENTRY                , // in g_DwdStore (entry)
	DRAWABLE_TYPE_DRAWABLE                 , // in g_DrawableStore
	DRAWABLE_TYPE_FRAGMENT_COMMON_DRAWABLE , // in g_FragmentStore (common drawable)
	DRAWABLE_TYPE_FRAGMENT_DAMAGED_DRAWABLE, // extra drawable name "damaged"
	DRAWABLE_TYPE_FRAGMENT_EXTRA_DRAWABLE  , // some other extra drawable
	DRAWABLE_TYPE_FRAGMENT_CLOTH_DRAWABLE  , // 
	DRAWABLE_TYPE_PARTICLE_MODEL           , // 
	DRAWABLE_TYPE_COUNT
};

#define STRING_FLAG_64BIT        0x8000
#define STRING_FLAG_APPEND       0x4000
#define STRING_FLAG_EMBEDDED_CRC 0x2000

void crc32_char(unsigned char data, u32& crc);
u32 crc32_lower(const char* str);
void crc64_char(unsigned char data, u64& crc);
u64 crc64_lower(const char* str);

#define U64_ARGS(x) (u32)((x) >> 32), (u32)(x)

} // namespace AssetAnalysis

#endif // __BANK
#endif // _DEBUG_ASSETANALYSIS_ASSETANALYSISCOMMON_H_
