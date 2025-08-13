// =====================================
// debug/AssetAnalysis/AssetAnalysis.cpp
// (c) 2013 RockstarNorth
// =====================================

#if __BANK

/*
TODO --
- create rag widgets for this
- remove dependency on streaming iterators, just copy the code over i guess
- commandline arg to start automatically (see si_auto)
- batch file to launch the game with the commandline
- batch file to zip up the data files and check them in
- particle assets
? mapdata assets
? texture stats - min/max RGBA values
? write rpf paths as separate strings, then use append to write asset paths
- stuff to report
	? unused archetypes
	? unused textures (name unused, path unused, etc.) - ignore HD textures
	- textures which are packed in the drawable but also exist in the parent txd
	- textures which are in a txd but also in a parent txd
	- textures which are in every sibling (should be moved up to a parent txd) - requires hierarchy information
	- textures which are only used by 1 child when there are multiple children (should be moved down) - requires hierarchy information
	? textures with unused alpha
	? flat (or nearly flat) textures
	? string crc's which did not resolve in the string table

- move water geometry dump system into this
- volume analysis (BS#1196726)
	...

- store template type in aaTextureRecord (need to get this checked into ragebuilder ASAP)

- not sure what i'd use these for yet ..
	- consider storing lod distances for archetypes (need to add this to debug archetype proxy then ..)
	- consider storing lod distances for drawables (and also available lod flags)
	- consider storing vertex/index counts for drawables and physical sizes for assets
	- consider storing bounding box and bounding sphere information
- currently i'm storing some lod distance stuff in the archetype instance (Entity), but this seems inefficient
  since it will be the same for multiple instances of the same archetype .. is there any way to store this
  (or some of it) at the archetype level? need to talk to ian about this

- unused shader report
- txd has no drawable children report (waste of slot)
- ask rick if we still need those collision reports
	- textures with different name but same crc
	- textures with same name but different crc
	- also should check for textures in gtxd that don't have unique name
	- also should check for textures in mapdetail which don't have unique name
- ask klaas if we need a texture coverage report of some sort ..
	- we could generate a low-res image for each texture showing where the camera needs to be in order to be using this texture
	- i assume we could do this by looking at the draw distances of the archetypes and where the archetypes are placed in the map ..
	- resolution would be something like 5x5 metres, and we would just store a 1-bit mask (RLE-compressed) so it shouldn't be too large
- copy streaming iterators into asset analysis system (call it CAssetAnalysisStreamingIterator and CAssetAnalysisStreamingIteratorTest until i think of better names)

- don't store texture names and archetype names, just texture paths and archetype paths (we can construct the names and name hashes from the paths)
*/

#include "atl/string.h"
#include "bank/bank.h"
#include "grcore/image.h"
#include "grcore/image_dxt.h"
#include "grcore/texture.h"
#if __XENON
#include "grcore/texturexenon.h"
#include "system/xtl.h"
#define DBG 0
#include <xgraphics.h>
#undef DBG
#endif // __XENON
#if __PS3
#include <cell/rtc.h>
#include "grcore/edgeExtractgeomspu.h"
#endif // __PS3
#include "grmodel/shadergroup.h"
#include "file/asset.h"
#include "file/packfile.h"
#include "file/stream.h"
#include "string/string.h"
#include "string/stringutil.h"
#include "system/memory.h"

#include "fragment/drawable.h"
#include "fragment/type.h"

#include "entity/archetypemanager.h"
#include "fwscene/stores/drawablestore.h"
#include "fwscene/stores/dwdstore.h"
#include "fwscene/stores/fragmentstore.h"
#include "fwscene/stores/mapdatastore.h"
#include "fwscene/stores/maptypesstore.h"
#include "fwscene/stores/txdstore.h"
#include "fwsys/timer.h"
#include "fwutil/xmacro.h"
#include "streaming/packfilemanager.h"
#include "streaming/streaming.h"
#include "streaming/streamingengine.h"
#include "timecycle/tcbox.h"
#include "timecycle/tcmodifier.h"
#include "vfx/ptfx/ptfxasset.h"
#include "vfx/ptfx/ptfxmanager.h"

#include "core/game.h"
#include "debug/AssetAnalysis/AssetAnalysis.h"
#include "debug/AssetAnalysis/AssetAnalysisCommon.h"
#include "debug/AssetAnalysis/AssetAnalysisUtil.h"
#include "debug/AssetAnalysis/StreamingIterator.h"
#include "debug/AssetAnalysis/StreamingIteratorManager.h"
#include "debug/DebugArchetypeProxy.h"
#include "debug/DebugGeometryUtil.h"
#include "modelinfo/MloModelInfo.h"
#include "modelinfo/ModelInfo.h"
#include "modelinfo/PedModelInfo.h"
#include "modelinfo/VehicleModelInfo.h"
#include "physics/gtaArchetype.h" // for gtaFragType
#include "renderer/GtaDrawable.h" // for gtaDrawable
#include "scene/loader/MapData.h"
#include "scene/lod/LodMgr.h"

namespace AssetAnalysis {

static bool      g_enabled                 = false;
static bool      g_verbose                 = false;
static bool      g_processDependencies     = false; // if false, txds will still be processed but only when their slot comes up
static bool      g_stringTableNoAppend     = false;
static bool      g_stringTableEmbeddedCrcs = true; // TODO -- consider removing this eventually
static bool      g_noArchetypeStream       = false;
static bool      g_streamsEnabled          = false;
static fiStream* g_streams[STREAM_TYPE_COUNT];
static int       g_streamsEntries[STREAM_TYPE_COUNT]; // number of entries per stream
static u32       g_stringTableOffset       = 0;

static atMap<u32,bool> g_uniqueStrings32;
static atMap<u64,bool> g_uniqueStrings64;

static void WriteToLogFile(const char* format, ...)
{
	fiStream*& stream = g_streams[STREAM_TYPE_LOG_FILE];

	if (stream == NULL)
	{
		stream = fiStream::Create(atVarString("assets:/non_final/aa/%s", GetStreamName(STREAM_TYPE_LOG_FILE)).c_str());
		g_streamsEntries[STREAM_TYPE_LOG_FILE] = 0;
	}

	if (stream)
	{
		char temp[4096] = "";
		va_list args;
		va_start(args, format);
		vsnprintf(temp, sizeof(temp), format, args);
		va_end(args);

		fprintf(stream, "%s\n", temp);
		stream->Flush();
	}
}

static fiStream* GetStream(eStreamType type, const char* assetPath = NULL)
{
	if (g_streams[type] == NULL)
	{
		g_streams[type] = fiStream::Create(atVarString("assets:/non_final/aa/%s", GetStreamName(type)).c_str());
		g_streamsEntries[type] = 0;

		Assertf(g_streams[type], "failed to create stream %s", GetStreamName(type));
	}

	if (g_verbose && assetPath)
	{
		char temp[80] = "";
		strcpy(temp, GetStreamName(type));
		strrchr(temp, '.')[0] = '\0';

		WriteToLogFile("writing %s[%d] \"%s\"", temp, g_streamsEntries[type], assetPath);
	}

	g_streamsEntries[type]++;
	return g_streams[type];
}

#define AA_DEBUG_STRING_HASHES (1 && __DEV)

#if AA_DEBUG_STRING_HASHES
static const u64 g_unknownStringHashes[] = // TODO -- debugging, these string hashes did not resolve in the string table, why?
{
	0x014281be0e83cab9ULL,
	0x01e61ff263a03f8bULL,
	0x02252571e55440faULL,
	0x03ef9419f50e21bfULL,
	0x0420b04041a46aa8ULL,
	0x0689e7ff213657faULL,
	0x07db5035b8a5aa2aULL,
	0x08acc7a08886c2e4ULL,
	0x08ca4de5a5f41a74ULL,
	0x0b8e002d5260bd36ULL,
	0x0ba0a1d1e0c3e5d0ULL,
	0x0ce81ec84d194589ULL,
	0x100845697e4e0607ULL,
	0x102feb015891f0beULL,
	0x1331c844b18a96acULL,
	0x1352f963bdce3bc9ULL,
	0x149f191fd84fc2d6ULL,
	0x153abe94b160bc32ULL,
	0x160b6ec5686207daULL,
	0x197e964e58480e6cULL,
	0x1986c32aa565d787ULL,
	0x198f257537d21ef4ULL,
	0x19a93329ef2a4dfdULL,
	0x1a0b3f8d70b36cbaULL,
	0x1ae0eed118c192f5ULL,
	0x1bf3f44a9fddc281ULL,
	0x1d7dba263d1e3f11ULL,
	0x1f271a85e40557dcULL,
	0x1f5a51e1317a8237ULL,
	0x1f91199816cdde7bULL,
	0x20c1fe54288ed5eeULL,
	0x20eb77d53acceadcULL,
	0x2229ed1ce06abe96ULL,
	0x2279940afab036d5ULL,
	0x22e9cd31cfe6c320ULL,
	0x231a60a23f072730ULL,
	0x236cbf1bb0f6f49cULL,
	0x2404b9279a8de6dfULL,
	0x2579b48f0d06c925ULL,
	0x258603e87b67cc60ULL,
	0x258cc0f84e2bb714ULL,
	0x2590e6dabfe4ac37ULL,
	0x26c41ac94625eb6aULL,
	0x287c9d8e855fa0f1ULL,
	0x29455ef68105e411ULL,
	0x297372d9d3dc3e4dULL,
	0x29cb85751d646bc9ULL,
	0x2c3a320cd65d3161ULL,
	0x2d9785a248ae51b0ULL,
	0x3016057f60baaa33ULL,
	0x32c343fffdb81f72ULL,
	0x331adeb616a57bc0ULL,
	0x332ffa7d45c2b1c1ULL,
	0x33e5a7129982ea82ULL,
	0x345e8de754d9296dULL,
	0x3466e0dde8f8a1fbULL,
	0x3585a02f285bc297ULL,
	0x375ef8c205e464e4ULL,
	0x37852e2abf4ae653ULL,
	0x38281d775cacfdadULL,
	0x3a19d52fc40d73abULL,
	0x3b04b57cf289860aULL,
	0x3bdc43450c45018cULL,
	0x3c53f523e61645deULL,
	0x3fa4e5d3d3ab11f9ULL,
	0x400e77096a946fd0ULL,
	0x40c8ec86b1664251ULL,
	0x41877e3defc4cb3fULL,
	0x42ae3b0a004b2670ULL,
	0x42e2bfd10d7699eeULL,
	0x441347a48e996912ULL,
	0x4443f747d5b03e8eULL,
	0x480cadf6e28c1781ULL,
	0x48d36d9859cec524ULL,
	0x4a3050f0b3996a12ULL,
	0x4adcae03cb6a3714ULL,
	0x4aeb254b8c2eae4cULL,
	0x4b3c1e4ccee65130ULL,
	0x4cc38e0e04f70a36ULL,
	0x4ce391ee86d9e65fULL,
	0x4d0fd921647fdbb0ULL,
	0x4e40359010c07fcbULL,
	0x4f6a40c912558a9bULL,
	0x4f76d2dd559f96ceULL,
	0x4ff2a4e1d077b5eeULL,
	0x511fc10c1c363ca1ULL,
	0x53b3e9992f8acfadULL,
	0x56b65645a9e4c76cULL,
	0x5b159632d4ed6168ULL,
	0x5b3044975b93ec60ULL,
	0x5b8532e8d287f6ddULL,
	0x5c7a54faf134f166ULL,
	0x5d704c8e4d0e8ab7ULL,
	0x5f9f74db6dbf6105ULL,
	0x60a5786cc184ff04ULL,
	0x6564d80dfa1029e6ULL,
	0x65712f387121cb04ULL,
	0x65b419fc9c384a52ULL,
	0x66da06a5a82d09f7ULL,
	0x66ed2ce63ad06fe0ULL,
	0x673cb0805128e9a9ULL,
	0x6bcccea7bd3bc6f3ULL,
	0x6bd039da7511d9a5ULL,
	0x6c7215597aa6b09bULL,
	0x6d59a285d84579d0ULL,
	0x6dfb27236e718b8dULL,
	0x6f78bd06ffdd6137ULL,
	0x6f9122d32a6888f2ULL,
	0x6fbd8c9cf6cdda51ULL,
	0x71f43d591fb0745aULL,
	0x74bad66162fb4395ULL,
	0x75928284c70fd43aULL,
	0x75e38b7c21fe2bc6ULL,
	0x763bf1ae1ef2ec19ULL,
	0x76b27e11af9a191fULL,
	0x773d118b55645057ULL,
	0x78ec4c4881de4f76ULL,
	0x794686badf657765ULL,
	0x79f71ea38297b9fbULL,
	0x7c3f50e03490fca7ULL,
	0x7c765882c5c037beULL,
	0x7e95658e02d329c6ULL,
	0x7eba4b258e0a7aaaULL,
	0x7ee423a139e41537ULL,
	0x7fb8f5d91f7a4a8dULL,
	0x81917a1178d6f1dfULL,
	0x822d496232d2e871ULL,
	0x82a0400dfbfb1f97ULL,
	0x8310af2d95a65700ULL,
	0x850fbb783ba03294ULL,
	0x855bff7a97a16fd3ULL,
	0x85ac601bb41bdb84ULL,
	0x876e7340cd46410cULL,
	0x8772b408562f2728ULL,
	0x8818907b6063d1f4ULL,
	0x881f9a311386eba4ULL,
	0x8907413de62d0567ULL,
	0x89add1ac3fbf4c0cULL,
	0x89d3709280a44665ULL,
	0x8bd66acb34cc3ea2ULL,
	0x8bf723fc3a66a2bbULL,
	0x8c192bbf66dff24bULL,
	0x8c422fe5aecd7d87ULL,
	0x8cc84a0128aa8002ULL,
	0x8ce053c8683bd727ULL,
	0x8db5fccac4f76361ULL,
	0x8e991dda4abb6eddULL,
	0x8ea793284cb441feULL,
	0x8f3809b95abfcba8ULL,
	0x90d2ac2a764cec5aULL,
	0x90e69a340991dabbULL,
	0x94a6ba402d3c539dULL,
	0x950997398175f579ULL,
	0x96a7da6232503b6dULL,
	0x98b1a0a12a000d0dULL,
	0x9a02d58ef6bcaa3dULL,
	0x9a3d5510ca89aa8eULL,
	0x9a57863e3fa0566cULL,
	0x9c09550cb59f2954ULL,
	0x9c0cd08f5696b51eULL,
	0x9c49006fc0ebc40aULL,
	0x9c4fe512e33fda88ULL,
	0x9d67f9bbbfbeebc4ULL,
	0x9d7df919b370b608ULL,
	0x9dac90b23cdc2862ULL,
	0x9e19071c875b76b5ULL,
	0x9f0b87715d98ebd2ULL,
	0x9f736556cb2d2699ULL,
	0xa0d1aad47677b17eULL,
	0xa161b0f55cfeb8c4ULL,
	0xa341002da1baf074ULL,
	0xa3acf5f0fe5c115cULL,
	0xa4be2976a52e32bdULL,
	0xa55155c1720b934bULL,
	0xa5baccc3b61e6053ULL,
	0xa68c6f6195aa4182ULL,
	0xa6dce32afdbf760fULL,
	0xa9462023a5c3c3d3ULL,
	0xaa91ecc00a473bdaULL,
	0xab1a9447e13639a8ULL,
	0xab30006b18746935ULL,
	0xb0b63a7a06daeffdULL,
	0xb244a11c2180182eULL,
	0xb25e0fa3273a37bbULL,
	0xb293428955be4712ULL,
	0xb3acd8221ae5e818ULL,
	0xb3c5528226a2eb91ULL,
	0xb4f62dea78ab00ecULL,
	0xb51e3bf01de15003ULL,
	0xb54e3d1c62a0fba8ULL,
	0xb7efddc21d781d2dULL,
	0xb89ef2f13ff85353ULL,
	0xb8ca163260cda378ULL,
	0xb8e1a4142759a615ULL,
	0xb93ad4cdd37d5fdbULL,
	0xb9555ba52b3a4482ULL,
	0xbac3d3c4b109dfe8ULL,
	0xbb60ed973db67ef9ULL,
	0xbd032a34027eb6e5ULL,
	0xbdcf4da3eccef198ULL,
	0xbe0ced04d50bcf4fULL,
	0xbe62880dfddc6adbULL,
	0xbe6a97566b2e6236ULL,
	0xbfa80ba51bf25f95ULL,
	0xc0f7e7921e41e460ULL,
	0xc3072ddb72869d43ULL,
	0xc3ae6d8777e4cef9ULL,
	0xc568bbdeaa751882ULL,
	0xc6660460ef094b2fULL,
	0xc692a56fab94c8ffULL,
	0xc6b3bc911704f661ULL,
	0xc75e90d4e641c698ULL,
	0xc89210a7d5d82d2cULL,
	0xc94558fe943b436fULL,
	0xcab0ead713c83a2cULL,
	0xcba90eff80ae337eULL,
	0xcc6a81afd73238efULL,
	0xcee21ebae3c60f30ULL,
	0xcfa117a4a7674708ULL,
	0xd0b57138a9452b00ULL,
	0xd136a431efc90f2cULL,
	0xd1f6dbe779e87f9fULL,
	0xd3502af99f243163ULL,
	0xd35e2eca1efbaa35ULL,
	0xd3fcf12ace20e2c1ULL,
	0xd44cd3ec6ad0ccc3ULL,
	0xd5252ada7a383fb6ULL,
	0xd7607e7ee44eb029ULL,
	0xd860ad920166899bULL,
	0xd8a9c38e0cdf8c80ULL,
	0xdaefff4aedec7341ULL,
	0xdb78a74b251924a1ULL,
	0xdce8111997d682a0ULL,
	0xde21989b18b92a82ULL,
	0xe03ba2eb11c1aa5eULL,
	0xe0b75a2f7212f884ULL,
	0xe1e865691282dbdfULL,
	0xe23e889dd940a4c8ULL,
	0xe3e897aa1fca72a6ULL,
	0xe45021c8605e6f84ULL,
	0xe483601c6703991eULL,
	0xe4abd766a50692aeULL,
	0xe4e58332d9edab00ULL,
	0xe556c74449c9afb9ULL,
	0xe6d22d9e6e49fbb1ULL,
	0xe721693ee5b5e275ULL,
	0xe7362583d364a6ebULL,
	0xe80f5848ccb3747eULL,
	0xe8219987b3cd9c4eULL,
	0xe8674916515abd69ULL,
	0xe94c36ce4a63d7c9ULL,
	0xe9c7117363e89af4ULL,
	0xe9c891e95dc5a0b4ULL,
	0xe9d6ac1a4d5bb885ULL,
	0xea172ee1a9896647ULL,
	0xec58682b2e430489ULL,
	0xedaef5aea98c1574ULL,
	0xeea1ccdbf475ec21ULL,
	0xeeebe7045d726a94ULL,
	0xf1934c5081ab5b6fULL,
	0xf2eab0374bca5e5dULL,
	0xf3a38e717d198e09ULL,
	0xf4284392147fd9cbULL,
	0xf557e7a264d61ac8ULL,
	0xf5b766a9cbe017f8ULL,
	0xf956f0fe207766d1ULL,
	0xfa28e4e0157f831aULL,
	0xfa31c7194242ff24ULL,
	0xfaf134c8f266239aULL,
	0xff5da571f95bc6bdULL,
	0xffbb5744bc4e1a81ULL,
};
#endif // AA_DEBUG_STRING_HASHES

#if __DEV

static u32 GetCrc32(const char* func, const char* param, const char* str)
{
	const u32 crc = crc32_lower(str);

	if (g_verbose && str[0])
	{
		WriteToLogFile("using string crc = 0x%08x \"%s\" in %s::%s", crc, str, func, param);
	}

	return crc;
}

static u64 GetCrc64(const char* func, const char* param, const char* str)
{
	const u64 crc = crc64_lower(str);

	if (g_verbose && str[0])
	{
		WriteToLogFile("using string crc = 0x%08x%08x \"%s\" in %s::%s", U64_ARGS(crc), str, func, param);
	}
#if AA_DEBUG_STRING_HASHES
	else if (str[0])
	{
		static atMap<u64,bool> s_debugStringHashMap;
		static bool            s_debugStringHashMapValid = false;

		if (!s_debugStringHashMapValid)
		{
			for (int i = 0; i < NELEM(g_unknownStringHashes); i++)
			{
				s_debugStringHashMap[g_unknownStringHashes[i]] = true;
			}

			s_debugStringHashMapValid = true;
		}

		if (s_debugStringHashMap.Access(crc))
		{
			WriteToLogFile("[DEBUG STRING HASH] using string crc = 0x%08x%08x \"%s\" in %s::%s", U64_ARGS(crc), str, func, param);
		}
	}
#endif // AA_DEBUG_STRING_HASHES

	return crc;
}

#define GET_CRC32(func,str) GetCrc32(func, #str, str)
#define GET_CRC64(func,str) GetCrc64(func, #str, str)
#else
#define GET_CRC32(func,str) crc32_lower(str)
#define GET_CRC64(func,str) crc64_lower(str)
#endif

static void WriteString(const char* str, int hashBits)
{
	if (g_verbose)
	{
		if (hashBits == 64)
		{
			const u64 crc = crc64_lower(str);
			WriteToLogFile("writing string[%d] crc = 0x%08x%08x \"%s\" @0x%08x", g_streamsEntries[STREAM_TYPE_STRING_TABLE], U64_ARGS(crc), str, g_stringTableOffset);
		}
		else
		{
			const u32 crc = crc32_lower(str);
			WriteToLogFile("writing string[%d] crc = 0x%08x \"%s\" @0x%08x", g_streamsEntries[STREAM_TYPE_STRING_TABLE], crc, str, g_stringTableOffset);
		}
	}

	fiStream* stream = GetStream(STREAM_TYPE_STRING_TABLE);

	if (stream)
	{
		const int length = str ? istrlen(str) : 0;
		const u16 tag = (u16)length | (hashBits == 64 ? STRING_FLAG_64BIT : 0) | (g_stringTableEmbeddedCrcs ? STRING_FLAG_EMBEDDED_CRC : 0);

		stream->WriteShort(&tag, 1);
		g_stringTableOffset += sizeof(tag);

		if (g_stringTableEmbeddedCrcs)
		{
			if (hashBits == 64)
			{
				const u64 crc = crc64_lower(str);
				stream->WriteLong(&crc, 1);
				g_stringTableOffset += sizeof(crc);
			}
			else
			{
				const u32 crc = crc32_lower(str);
				stream->WriteInt(&crc, 1);
				g_stringTableOffset += sizeof(crc);
			}
		}

		if (str)
		{
			stream->WriteByte(str, length + 1); // includes '\0' terminator
			g_stringTableOffset += length + 1;
		}
	}
}

static void WriteString32Unique(const char* str)
{
	const u32 hash = crc32_lower(str);

	if (g_uniqueStrings32.Access(hash) == NULL)
	{
		g_uniqueStrings32[hash] = true;
		WriteString(str, 32);
	}
}

static void WriteString64Unique(const char* str)
{
	const u64 hash = crc64_lower(str);

	if (g_uniqueStrings64.Access(hash) == NULL)
	{
		g_uniqueStrings64[hash] = true;
		WriteString(str, 64);
	}
}

static void WriteString64Append(const char* prependStr, const char* str)
{
	const char* appendStr = striskip(str, prependStr);

	if (appendStr == str || *appendStr != '/' || g_stringTableNoAppend)
	{
		WriteString(str, 64);
		return;
	}

	appendStr++;

	const u64 prependHash = crc64_lower(prependStr);

	if (g_verbose)
	{
		const u64 crc = crc64_lower(appendStr);
		WriteToLogFile("appending string[%d] to 0x%08x%08x crc = 0x%08x%08x \"%s\" @0x%08x", g_streamsEntries[STREAM_TYPE_STRING_TABLE], U64_ARGS(prependHash), U64_ARGS(crc), str, g_stringTableOffset);
	}

	fiStream* stream = GetStream(STREAM_TYPE_STRING_TABLE);

	if (stream)
	{
		const int length = appendStr ? istrlen(appendStr) : 0;
		const u16 tag = (u16)length | STRING_FLAG_64BIT | (g_stringTableEmbeddedCrcs ? STRING_FLAG_EMBEDDED_CRC : 0) | STRING_FLAG_APPEND;

		stream->WriteShort(&tag, 1);
		g_stringTableOffset += sizeof(tag);

		if (g_stringTableEmbeddedCrcs)
		{
			const u64 crc = crc64_lower(str);
			stream->WriteLong(&crc, 1);
			g_stringTableOffset += sizeof(crc);
		}

		stream->WriteLong(&prependHash, 1);
		g_stringTableOffset += sizeof(prependHash);

		if (appendStr)
		{
			stream->WriteByte(appendStr, length + 1); // includes '\0' terminator
			g_stringTableOffset += length + 1;
		}
	}
}

template <typename T> static void WriteString64AppendToRPFPath(T& store, int slot, const char* str)
{
	const char* rpfPath = GetRPFPath(store, slot);

	WriteString64Unique(rpfPath);
	WriteString64Append(rpfPath, str);
}

#define CAN_LOCK_TEXTURE_ON_CPU (!RSG_PC)

static void WriteTexture(const char* assetPath, const grcTexture* texture)
{
	if (texture)
	{
		const atString textureName = GetTextureName(texture);
		const atString texturePath = atVarString("%s/%s", assetPath, textureName.c_str());

		const u64 textureNameHash        = GET_CRC64("Texture", textureName.c_str());
		const u64 texturePathHash        = GET_CRC64("Texture", texturePath.c_str());
		const u32 texturePixelDataHash   = CAN_LOCK_TEXTURE_ON_CPU ? GetTexturePixelDataHash(texture, false, 32) : 0; // hash up to 32 lines of top mip, along with w/h/format/mipcount
		const u32 texturePhysicalSize    = (u32)texture->GetPhysicalSize();
		const u16 textureWidth           = (u16)texture->GetWidth();
		const u16 textureHeight          = (u16)texture->GetHeight();
		const u8  textureMips            = (u8)texture->GetMipMapCount();
		const u8  textureFormat          = (u8)texture->GetImageFormat();
		const u8  textureConversionFlags = texture->GetConversionFlags();
		const u16 textureTemplateType    = texture->GetTemplateType();

		const char* textureSwizzleTable = "RGBA01";
		char textureSwizzle[4];
		grcTexture::eTextureSwizzle temp[4];
		texture->GetTextureSwizzle(temp[0], temp[1], temp[2], temp[3]);
		textureSwizzle[0] = textureSwizzleTable[temp[0]];
		textureSwizzle[1] = textureSwizzleTable[temp[1]];
		textureSwizzle[2] = textureSwizzleTable[temp[2]];
		textureSwizzle[3] = textureSwizzleTable[temp[3]];

		u16 rgbmin = 0xffff;
		u16 rgbmax = 0x0000;
		Vec4V avg(V_ZERO);

#if CAN_LOCK_TEXTURE_ON_CPU
		GetTextureRGBMinMaxHistogramRaw(texture, rgbmin, rgbmax);
		avg = GetTextureAverageColour(texture); // kinda slow but we need this to find shifted normal maps
#endif // CAN_LOCK_TEXTURE_ON_CPU

		fiStream* stream = GetStream(STREAM_TYPE_TEXTURE, texturePath.c_str());

		if (stream)
		{
			// NOTE -- keep in sync with %RS_CODEBRANCH%\tools\AssetAnalysisTool\src\AssetAnalysisTool.h
			/*class aaTextureRecord
			{
			public:
				enum { TAG = 'TXTR' };

				u32   m_tag;
				u64   m_textureNameHash;
				u64   m_texturePathHash;
				u32   m_texturePixelDataHash;
				u32   m_texturePhysicalSize;
				u16   m_textureWidth;
				u16   m_textureHeight;
				u8    m_textureMips;
				u8    m_textureFormat;
				char  m_textureSwizzle[4];
				u8    m_textureConversionFlags;
				u16   m_textureTemplateType;
				u16   m_rgbmin;
				u16   m_rgbmax;
				float m_average[4];
			};*/
			const u32  tag = 'TXTR';
			stream->WriteInt  (&tag, 1);
			stream->WriteLong (&textureNameHash, 1);
			stream->WriteLong (&texturePathHash, 1);
			stream->WriteInt  (&texturePixelDataHash, 1);
			stream->WriteInt  (&texturePhysicalSize, 1);
			stream->WriteShort(&textureWidth, 1);
			stream->WriteShort(&textureHeight, 1);
			stream->WriteByte (&textureMips, 1);
			stream->WriteByte (&textureFormat, 1);
			stream->WriteByte (&textureSwizzle[0], 4);
			stream->WriteByte (&textureConversionFlags, 1);
			stream->WriteShort(&textureTemplateType, 1);
			stream->WriteShort(&rgbmin, 1);
			stream->WriteShort(&rgbmax, 1);
			stream->WriteFloat((const float*)&avg, 4);
		}

		WriteString(textureName.c_str(), 64); // not unique, but we don't want to store this many strings in-game .. so just write them all out i guess
		WriteString64Append(assetPath, texturePath.c_str());
	}
}

template <typename T> static void WriteTextureDictionary(T& store, int slot, int entry, const fwTxd* txd, u32 physicalSize, u32 virtualSize)
{
	if (txd)
	{
		const atString assetPath     = GetAssetPath(store, slot, entry);
		const atString parentTxdPath = GetAssetPath(g_TxdStore, GetParentTxdSlot(store, slot));

		atString txdPath = assetPath;

		if (GetAssetType(store) == ASSET_TYPE_PARTICLE)
		{
			txdPath += "/textures";
		}

		const u8  assetType         = (u8)GetAssetType(store);
		const u64 txdPathHash       = GET_CRC64("TextureDictionary", txdPath.c_str());
		const u64 parentTxdPathHash = GET_CRC64("TextureDictionary", parentTxdPath.c_str());

		const u16 numEntries = (u16)txd->GetCount();

		fiStream* stream = GetStream(STREAM_TYPE_TEXTURE_DICTIONARY, txdPath.c_str());

		if (stream)
		{
			// NOTE -- keep in sync with %RS_CODEBRANCH%\tools\AssetAnalysisTool\src\AssetAnalysisTool.h
			/*class aaTextureDictionaryRecord
			{
			public:
				enum { TAG = 'TXD ' };

				u32 m_tag;
				u32 m_physicalSize;
				u32 m_virtualSize;
				u16 m_numEntries;
				u8  m_assetType; // eAssetType
				u64 m_txdPathHash;
				u64 m_parentTxdPathHash;
			};*/
			const u32 tag = 'TXD ';
			stream->WriteInt  (&tag, 1);
			stream->WriteInt  (&physicalSize, 1);
			stream->WriteInt  (&virtualSize, 1);
			stream->WriteShort(&numEntries, 1);
			stream->WriteByte (&assetType, 1);
			stream->WriteLong (&txdPathHash, 1);
			stream->WriteLong (&parentTxdPathHash, 1);
		}

		if (GetAssetType(store) == ASSET_TYPE_TXD ||
			GetAssetType(store) == ASSET_TYPE_PARTICLE)
		{
			WriteString64AppendToRPFPath(store, slot, txdPath.c_str());
		}

		for (int k = 0; k < txd->GetCount(); k++)
		{
			WriteTexture(txdPath.c_str(), txd->GetEntry(k));
		}
	}
}

template <typename T> static u64 WriteMaterialTexture(T& store, int slot, int entry, const Drawable* drawable, const grcTexture* texture, u32 textureKey, u64 materialPathHash, const char* materialVarName)
{
	if ((texture && dynamic_cast<const grcRenderTarget*>(texture) == NULL) ||
		(texture == NULL && textureKey != 0))
	{
		const atString texturePath = GetTexturePath(store, slot, entry, drawable, texture, textureKey);

		const u32 materialVarNameHash = GET_CRC32("MaterialTexture", materialVarName);
		const u64 texturePathHash     = GET_CRC64("MaterialTexture", texturePath.c_str());

		fiStream* stream = GetStream(STREAM_TYPE_MATERIAL_TEXTURE_VAR, texturePath.c_str());

		if (stream)
		{
			// NOTE -- keep in sync with %RS_CODEBRANCH%\tools\AssetAnalysisTool\src\AssetAnalysisTool.h
			/*class aaMaterialTextureVarRecord
			{
			public:
				enum { TAG = 'MTEX' };

				u32 m_tag;
				u32 m_materialVarNameHash;
				u64 m_materialPathHash;
				u64 m_texturePathHash;
			};*/
			const u32 tag = 'MTEX';
			stream->WriteInt (&tag, 1);
			stream->WriteInt (&materialVarNameHash, 1);
			stream->WriteLong(&materialPathHash, 1);
			stream->WriteLong(&texturePathHash, 1);
		}

		WriteString32Unique(materialVarName);

		if (strchr(texturePath.c_str(), '$') != NULL || // hopefully this does not happen too often, but we want to store the strings in this case
			textureKey != 0) // .. and i'm also forcing light texture paths to write their full path, because for some reason they're not coming through normally
		{
			WriteString(texturePath.c_str(), 64);
		}

		return texturePathHash;
	}

	return 0;
}

static atString GetMaterialPath(const char* assetPath, int shaderIndex, const char* shaderName)
{
	return atVarString("%s/[%d]_%s", assetPath, shaderIndex, shaderName);
}

static atString GetLightTexturePath(const char* drawablePath, int lightIndexInDrawable)
{
	return atVarString("%s/light[%d]_texture", drawablePath, lightIndexInDrawable);
}

template <typename T> static void WriteMaterial(T& store, int slot, int entry, const char* assetPath, const Drawable* drawable, const grmShader* shader, int shaderIndex)
{
	// standard shader vars
	grcTexture* diffTex = NULL;
	grcTexture* bumpTex = NULL;
	float       bumpiness = 1.0f;
	grcTexture* specTex = NULL;
	float       specIntensity = 1.0f;
	Vector3     specIntensityMask(1.0f, 0.0f, 0.0f);

	for (int j = 0; j < shader->GetVarCount(); j++)
	{
		const grcEffectVar var = shader->GetVarByIndex(j);
		const char* materialVarName = NULL;
		grcEffect::VarType type;
		int annotationCount = 0;
		bool isGlobal = false;

		shader->GetVarDesc(var, materialVarName, type, annotationCount, isGlobal);

		if (!isGlobal)
		{
			if      (type == grcEffect::VT_TEXTURE && stricmp(materialVarName, "DiffuseTex"              ) == 0) { shader->GetVar(var, diffTex          ); }
			else if (type == grcEffect::VT_TEXTURE && stricmp(materialVarName, "BumpTex"                 ) == 0) { shader->GetVar(var, bumpTex          ); }
			else if (type == grcEffect::VT_FLOAT   && stricmp(materialVarName, "Bumpiness"               ) == 0) { shader->GetVar(var, bumpiness        ); }
			else if (type == grcEffect::VT_TEXTURE && stricmp(materialVarName, "SpecularTex"             ) == 0) { shader->GetVar(var, specTex          ); }
			else if (type == grcEffect::VT_FLOAT   && stricmp(materialVarName, "SpecularColor"           ) == 0) { shader->GetVar(var, specIntensity    ); }
			else if (type == grcEffect::VT_VECTOR3 && stricmp(materialVarName, "SpecularMapIntensityMask") == 0) { shader->GetVar(var, specIntensityMask); }
		}
	}

	if (dynamic_cast<grcRenderTarget*>(diffTex)) { diffTex = NULL; } // we don't want rendertargets ..
	if (dynamic_cast<grcRenderTarget*>(bumpTex)) { bumpTex = NULL; }
	if (dynamic_cast<grcRenderTarget*>(specTex)) { specTex = NULL; }

	const atString materialPath = GetMaterialPath(assetPath, shaderIndex, shader->GetName());
	const atString diffTexPath  = GetTexturePath(store, slot, entry, drawable, diffTex);
	const atString bumpTexPath  = GetTexturePath(store, slot, entry, drawable, bumpTex);
	const atString specTexPath  = GetTexturePath(store, slot, entry, drawable, specTex);

	const u64 materialPathHash = GET_CRC64("ShaderGroupMaterial", materialPath.c_str());
	const u64 diffTexPathHash  = GET_CRC64("ShaderGroupMaterial", diffTexPath.c_str());
	const u64 bumpTexPathHash  = GET_CRC64("ShaderGroupMaterial", bumpTexPath.c_str());
	const u64 specTexPathHash  = GET_CRC64("ShaderGroupMaterial", specTexPath.c_str());

	fiStream* stream = GetStream(STREAM_TYPE_MATERIAL, materialPath.c_str());

	if (stream)
	{
		// NOTE -- keep in sync with %RS_CODEBRANCH%\tools\AssetAnalysisTool\src\AssetAnalysisTool.h
		/*class aaMaterialRecord
		{
		public:
			enum { TAG = 'MAT ' };

			u32   m_tag;
			u64   m_materialPathHash;
			u64   m_diffTexPathHash;
			u64   m_bumpTexPathHash;
			float m_bumpiness;
			u64   m_specTexPathHash;
			float m_specIntensity;
			float m_specIntensityMask[3];
		};*/
		const u32 tag = 'MAT ';
		stream->WriteInt  (&tag, 1);
		stream->WriteLong (&materialPathHash, 1);
		stream->WriteLong (&diffTexPathHash, 1);
		stream->WriteLong (&bumpTexPathHash, 1);
		stream->WriteFloat(&bumpiness, 1);
		stream->WriteLong (&specTexPathHash, 1);
		stream->WriteFloat(&specIntensity, 1);
		stream->WriteFloat((const float*)&specIntensityMask, 3);
	}

	WriteString64Append(assetPath, materialPath.c_str());

	// material textures
	{
		for (int j = 0; j < shader->GetVarCount(); j++)
		{
			const grcEffectVar var = shader->GetVarByIndex(j);
			const char* materialVarName = NULL;
			grcEffect::VarType type;
			int annotationCount = 0;
			bool isGlobal = false;

			shader->GetVarDesc(var, materialVarName, type, annotationCount, isGlobal);

			if (!isGlobal && type == grcEffect::VT_TEXTURE)
			{
				grcTexture* texture = NULL;
				shader->GetVar(var, texture);

				WriteMaterialTexture(store, slot, entry, drawable, texture, 0, materialPathHash, materialVarName);
			}
		}
	}
}

static void WriteParticleRuleMaterial(int slot, int entry, const grmShader* shader)
{
	// TODO
	(void)slot;
	(void)entry;
	(void)shader;
}

template <typename T> static void WriteShaderGroupMaterials(T& store, int slot, int entry, const char* assetPath, const Drawable* drawable)
{
	const grmShaderGroup* shaderGroup = drawable->GetShaderGroupPtr();

	if (shaderGroup)
	{
		for (int i = 0; i < shaderGroup->GetCount(); i++)
		{
			WriteMaterial(store, slot, entry, assetPath, drawable, shaderGroup->GetShaderPtr(i), i);
		}
	}
}

template <typename T> static void WriteLight(T& store, int slot, int entry, const Drawable* drawable, const CLightAttr& light, int lightIndex, const char* drawablePath, u16& numLights, u16& numLightsWithTextures)
{
	const u64 drawablePathHash = GET_CRC64("Light", drawablePath);
	const u16 lightIndexInDrawable = (u16)lightIndex;

	Vec3V posn;
	light.GetPos(RC_VECTOR3(posn));

	u8 lightType = 0;

	switch (light.m_lightType)
	{
	case LIGHT_TYPE_NONE        : lightType = 0; break;
	case LIGHT_TYPE_POINT       : lightType = 1; break;
	case LIGHT_TYPE_SPOT        : lightType = 2; break;
	case LIGHT_TYPE_CAPSULE     : lightType = 3; break;
	case LIGHT_TYPE_DIRECTIONAL : lightType = 4; break;
	case LIGHT_TYPE_AO_VOLUME   : lightType = 5; break;
	}

	u64 projectedTexturePathHash = 0;

	if (light.m_projectedTextureKey != 0)
	{
		const char*    materialVarName  = "LightTexture";
		const atString materialPath     = GetLightTexturePath(drawablePath, lightIndex);
		const u64      materialPathHash = GET_CRC64("Light", materialPath.c_str());

		projectedTexturePathHash = WriteMaterialTexture(store, slot, entry, drawable, NULL, light.m_projectedTextureKey, materialPathHash, materialVarName);
		WriteString64Append(drawablePath, materialPath.c_str());

		numLightsWithTextures++;
	}

	fiStream* stream = GetStream(STREAM_TYPE_LIGHT, drawablePath);

	if (stream)
	{
		// NOTE -- keep in sync with %RS_CODEBRANCH%\tools\AssetAnalysisTool\src\AssetAnalysisTool.h
		/*class aaLightRecord
		{
		public:
			enum { TAG = 'LITE' };

			u32   m_tag;
			u64   m_drawablePathHash;
			u16   m_lightIndexInDrawable;
			u64   m_projectedTexturePathHash;

			float m_position[3];

			// General data
			u8    m_colour[3];
			u8    m_flashiness;
			float m_intensity;
			u32   m_flags;
			s16   m_boneTag;
			u8    m_lightType;
			u8    m_groupId;
			u32   m_timeFlags;
			float m_falloff;
			float m_falloffExponent;
			float m_cullingPlane[4];
			u8    m_shadowBlur;
 			u8    m_padding1;
 			s16   m_padding2;
			u32   m_padding3;

			// Volume data
			float m_volIntensity;
			float m_volSizeScale;
			u8    m_volOuterColour[3];
			u8    m_lightHash;
			float m_volOuterIntensity;

			// Corona data
			float m_coronaSize;
			float m_volOuterExponent;
			u8    m_lightFadeDistance;
			u8    m_shadowFadeDistance;
			u8    m_specularFadeDistance;
			u8    m_volumetricFadeDistance;
			float m_shadowNearClip;
			float m_coronaIntensity;
			float m_coronaZBias;

			// Spot data
			float m_direction[3];
			float m_tangent[3];
			float m_coneInnerAngle;
			float m_coneOuterAngle;

			// Line data
			float m_extents[3];

			// Texture data
			u32 m_projectedTextureKey;
		};*/
		const u32 tag = 'LITE';
		stream->WriteInt  (&tag, 1);
		stream->WriteLong (&drawablePathHash, 1);
		stream->WriteShort(&lightIndexInDrawable, 1);
		stream->WriteLong (&projectedTexturePathHash, 1);

		stream->WriteFloat((const float*)&posn, 3);

		// ==================================================================================
		// WARNING!! IF YOU CHANGE THE FIELDS HERE YOU MUST ALSO KEEP IN SYNC WITH:
		// 
		// CLightAttrDef - %RS_CODEBRANCH%\game\scene\loader\MapData_Extensions.psc
		// CLightAttr    - %RS_CODEBRANCH%\game\scene\2dEffect.h
		// CLightAttr    - %RS_CODEBRANCH%\rage\framework\tools\src\cli\ragebuilder\gta_res.h
		// WriteLight    - %RS_CODEBRANCH%\game\debug\AssetAnalysis\AssetAnalysis.cpp
		// WriteLights   - %RS_CODEBRANCH%\tools\AssetAnalysisTool\src\AssetAnalysisTool.cpp
		// aaLightRecord - %RS_CODEBRANCH%\tools\AssetAnalysisTool\src\AssetAnalysisTool.h
		// ==================================================================================

		// General data
		stream->WriteByte (&light.m_colour.red, 1);
		stream->WriteByte (&light.m_colour.green, 1);
		stream->WriteByte (&light.m_colour.blue, 1);
		stream->WriteByte (&light.m_flashiness, 1);
		stream->WriteFloat(&light.m_intensity, 1);
		stream->WriteInt  (&light.m_flags, 1);
		stream->WriteShort(&light.m_boneTag, 1);
		stream->WriteByte (&lightType, 1);
		stream->WriteByte (&light.m_groupId, 1);
		stream->WriteInt  (&light.m_timeFlags, 1);
		stream->WriteFloat(&light.m_falloff, 1);
		stream->WriteFloat(&light.m_falloffExponent, 1);
		stream->WriteFloat(&light.m_cullingPlane[0], 4);
		stream->WriteByte (&light.m_shadowBlur, 1);
		stream->WriteByte (&light.m_padding1, 1);
		stream->WriteShort(&light.m_padding2, 1);
		stream->WriteInt  (&light.m_padding3, 1);

		// Volume data
		stream->WriteFloat(&light.m_volIntensity, 1);
		stream->WriteFloat(&light.m_volSizeScale, 1);
		stream->WriteByte (&light.m_volOuterColour.red, 1);
		stream->WriteByte (&light.m_volOuterColour.green, 1);
		stream->WriteByte (&light.m_volOuterColour.blue, 1);
		stream->WriteByte (&light.m_lightHash, 1);
		stream->WriteFloat(&light.m_volOuterIntensity, 1);

		// Corona data
		stream->WriteFloat(&light.m_coronaSize, 1);
		stream->WriteFloat(&light.m_volOuterExponent, 1);
		stream->WriteByte (&light.m_lightFadeDistance, 1);
		stream->WriteByte (&light.m_shadowFadeDistance, 1);
		stream->WriteByte (&light.m_specularFadeDistance, 1);
		stream->WriteByte (&light.m_volumetricFadeDistance, 1);
		stream->WriteFloat(&light.m_shadowNearClip, 1);
		stream->WriteFloat(&light.m_coronaIntensity, 1);
		stream->WriteFloat(&light.m_coronaZBias, 1);

		// Spot data
		stream->WriteFloat(&light.m_direction.x, 1);
		stream->WriteFloat(&light.m_direction.y, 1);
		stream->WriteFloat(&light.m_direction.z, 1);
		stream->WriteFloat(&light.m_tangent.x, 1);
		stream->WriteFloat(&light.m_tangent.y, 1);
		stream->WriteFloat(&light.m_tangent.z, 1);
		stream->WriteFloat(&light.m_coneInnerAngle, 1);
		stream->WriteFloat(&light.m_coneOuterAngle, 1);

		// Line data
		stream->WriteFloat(&light.m_extents.x, 1);
		stream->WriteFloat(&light.m_extents.y, 1);
		stream->WriteFloat(&light.m_extents.z, 1);

		// Texture data
		stream->WriteInt  (&light.m_projectedTextureKey, 1);
	}

	numLights++;
}

template <typename T> static void WriteDrawable(T& store, int slot, int entry, const Drawable* drawable, u32 physicalSize, u32 virtualSize, u64 assetPathHash, u8 drawableType)
{
	if (drawable)
	{
		const atString drawablePath  = GetAssetPath(store, slot, entry);
		const atString parentTxdPath = GetAssetPath(g_TxdStore, GetParentTxdSlot(store, slot));

		const CDebugArchetypeProxy* arch = GetArchetypeProxyForDrawable(store, slot, entry);

		const u8  assetType         = (u8)GetAssetType(store);
		const u64 drawablePathHash  = GET_CRC64("Drawable", drawablePath.c_str());
		const u64 parentTxdPathHash = GET_CRC64("Drawable", parentTxdPath.c_str());
		const u64 archetypeNameHash = arch ? GET_CRC64("Drawable", arch->GetModelName()) : 0;

		const u16 numTriangles[LOD_COUNT] =
		{
			(u16)Min<int>(65535, GeometryUtil::CountTrianglesForDrawable(drawable, LOD_HIGH, drawablePath.c_str())),
			(u16)Min<int>(65535, GeometryUtil::CountTrianglesForDrawable(drawable, LOD_MED, drawablePath.c_str())),
			(u16)Min<int>(65535, GeometryUtil::CountTrianglesForDrawable(drawable, LOD_LOW, drawablePath.c_str())),
			(u16)Min<int>(65535, GeometryUtil::CountTrianglesForDrawable(drawable, LOD_VLOW, drawablePath.c_str())),
		};

		u16 tintDataSize          = 0;
		u16 numLights             = 0;
		u16 numLightsWithTextures = 0;

		if ((const void*)&store != &g_FragmentStore) // lights
		{
			const gtaDrawable* gtaDraw = dynamic_cast<const gtaDrawable*>(drawable);

			if (gtaDraw)
			{
				for (int i = 0; i < gtaDraw->m_lights.GetCount(); i++)
				{
					WriteLight(store, slot, entry, drawable, gtaDraw->m_lights[i], i, drawablePath.c_str(), numLights, numLightsWithTextures);
				}

				const atArray<u8>* pTintData = gtaDraw->m_pTintData;

				if (pTintData)
				{
					tintDataSize = (u16)pTintData->GetCount();
				}
			}
		}

		fiStream* stream = GetStream(STREAM_TYPE_DRAWABLE, drawablePath.c_str());

		if (stream)
		{
			// NOTE -- keep in sync with %RS_CODEBRANCH%\tools\AssetAnalysisTool\src\AssetAnalysisTool.h
			/*class aaDrawableRecord
			{
			public:
				enum { TAG = 'DRAW' };

				u32 m_tag;
				u32 m_physicalSize;
				u32 m_virtualSize;
				u8  m_assetType; // eAssetType
				u8  m_drawableType; // eDrawableType
				u64 m_drawablePathHash;
				u64 m_assetPathHash;
				u64 m_parentTxdPathHash;
				u64 m_archetypeNameHash;
				u16 m_numTriangles[4];
				u16 m_tintDataSize;
				u16 m_numLights;
				u16 m_numLightsWithTextures;
			};*/
			const u32 tag = 'DRAW';
			stream->WriteInt  (&tag, 1);
			stream->WriteInt  (&physicalSize, 1);
			stream->WriteInt  (&virtualSize, 1);
			stream->WriteByte (&assetType, 1);
			stream->WriteByte (&drawableType, 1);
			stream->WriteLong (&drawablePathHash, 1);
			stream->WriteLong (&assetPathHash, 1);
			stream->WriteLong (&parentTxdPathHash, 1);
			stream->WriteLong (&archetypeNameHash, 1);
			stream->WriteShort(&numTriangles[0], LOD_COUNT);
			stream->WriteShort(&tintDataSize, 1);
			stream->WriteShort(&numLights, 1);
			stream->WriteShort(&numLightsWithTextures, 1);
		}

		if (entry == INDEX_NONE)
		{
			WriteString64AppendToRPFPath(store, slot, drawablePath.c_str());
		}
		else
		{
			WriteString64Append(GetAssetPath(store, slot).c_str(), drawablePath.c_str());
		}

		WriteShaderGroupMaterials(store, slot, entry, drawablePath.c_str(), drawable);
		WriteTextureDictionary(store, slot, entry, drawable->GetTexDictSafe(), 0, 0);
	}
}

static void WriteDrawableDictionary(int slot)
{
	const Dwd* dwd = g_DwdStore.Get(GTA_ONLY(strLocalIndex)(slot));

	if (dwd)
	{
		const atString assetPath     = GetAssetPath(g_DwdStore, slot);
		const atString parentTxdPath = GetAssetPath(g_TxdStore, GetParentTxdSlot(g_DwdStore, slot));

		const u64 assetPathHash     = GET_CRC64("DrawableDictionary", assetPath.c_str());
		const u64 parentTxdPathHash = GET_CRC64("DrawableDictionary", parentTxdPath.c_str());

		const u32 physicalSize = GetAssetSize(g_DwdStore, slot, ASSET_SIZE_PHYSICAL);
		const u32 virtualSize  = GetAssetSize(g_DwdStore, slot, ASSET_SIZE_VIRTUAL);
		const u16 numEntries   = (u16)dwd->GetCount();

		fiStream* stream = GetStream(STREAM_TYPE_DRAWABLE_DICTIONARY, assetPath.c_str());

		if (stream)
		{
			// NOTE -- keep in sync with %RS_CODEBRANCH%\tools\AssetAnalysisTool\src\AssetAnalysisTool.h
			/*class aaDrawableDictionaryRecord
			{
			public:
				enum { TAG = 'DWD ' };

				u32 m_tag;
				u32 m_physicalSize;
				u32 m_virtualSize;
				u64 m_assetPathHash;
				u64 m_parentTxdPathHash;
			};*/
			const u32 tag = 'DWD ';
			stream->WriteInt  (&tag, 1);
			stream->WriteInt  (&physicalSize, 1);
			stream->WriteInt  (&virtualSize, 1);
			stream->WriteShort(&numEntries, 1);
			stream->WriteLong (&assetPathHash, 1);
			stream->WriteLong (&parentTxdPathHash, 1);
		}

		WriteString64AppendToRPFPath(g_DwdStore, slot, assetPath.c_str());

		for (int i = 0; i < dwd->GetCount(); i++)
		{
			WriteDrawable(g_DwdStore, slot, i, dwd->GetEntry(i), 0, 0, assetPathHash, DRAWABLE_TYPE_DWD_ENTRY);
		}
	}
}

static void WriteFragment(int slot)
{
	const Fragment* fragment = g_FragmentStore.Get(GTA_ONLY(strLocalIndex)(slot));

	if (fragment)
	{
		const atString assetPath     = GetAssetPath(g_FragmentStore, slot);
		const atString parentTxdPath = GetAssetPath(g_TxdStore, GetParentTxdSlot(g_FragmentStore, slot));

		const u64 assetPathHash     = GET_CRC64("Fragment", assetPath.c_str());
		const u64 parentTxdPathHash = GET_CRC64("Fragment", parentTxdPath.c_str());

		const u32 physicalSize = GetAssetSize(g_FragmentStore, slot, ASSET_SIZE_PHYSICAL);
		const u32 virtualSize  = GetAssetSize(g_FragmentStore, slot, ASSET_SIZE_VIRTUAL);

		u16 tintDataSize          = 0;
		u16 numLights             = 0;
		u16 numLightsWithTextures = 0;

		u8 numEnvCloths  = (u8)fragment->GetNumEnvCloths();
		u8 numCharCloths = (u8)fragment->GetNumCharCloths();
		u8 numGlassPanes = (u8)fragment->GetNumGlassPaneModelInfos();

		// lights
		{
			const gtaFragType* gtaFrag = dynamic_cast<const gtaFragType*>(fragment);

			if (gtaFrag)
			{
				for (int i = 0; i < gtaFrag->m_lights.GetCount(); i++)
				{
					WriteLight(g_FragmentStore, slot, FRAGMENT_COMMON_DRAWABLE_ENTRY, fragment->GetCommonDrawable(), gtaFrag->m_lights[i], i, assetPath.c_str(), numLights, numLightsWithTextures);
				}

				const atArray<u8>* pTintData = gtaFrag->m_pTintData;

				if (pTintData)
				{
					tintDataSize = (u16)pTintData->GetCount();
				}
			}
		}

		fiStream* stream = GetStream(STREAM_TYPE_FRAGMENT, assetPath.c_str());

		if (stream)
		{
			// NOTE -- keep in sync with %RS_CODEBRANCH%\tools\AssetAnalysisTool\src\AssetAnalysisTool.h
			/*class aaFragmentRecord
			{
			public:
				enum { TAG = 'FRAG' };

				u32 m_tag;
				u32 m_physicalSize;
				u32 m_virtualSize;
				u64 m_assetPathHash;
				u64 m_parentTxdPathHash;
				u16 m_tintDataSize;
				u16 m_numLights;
				u16 m_numLightsWithTextures;
				u8  m_numEnvCloths;
				u8  m_numCharCloths;
				u8  m_numGlassPanes;
			};*/
			const u32 tag = 'FRAG';
			stream->WriteInt  (&tag, 1);
			stream->WriteInt  (&physicalSize, 1);
			stream->WriteInt  (&virtualSize, 1);
			stream->WriteLong (&assetPathHash, 1);
			stream->WriteLong (&parentTxdPathHash, 1);
			stream->WriteShort(&tintDataSize, 1);
			stream->WriteShort(&numLights, 1);
			stream->WriteShort(&numLightsWithTextures, 1);
			stream->WriteByte (&numEnvCloths, 1);
			stream->WriteByte (&numCharCloths, 1);
			stream->WriteByte (&numGlassPanes, 1);
		}

		WriteString64AppendToRPFPath(g_FragmentStore, slot, assetPath.c_str());

		WriteDrawable(g_FragmentStore, slot, FRAGMENT_COMMON_DRAWABLE_ENTRY, fragment->GetCommonDrawable(), 0, 0, assetPathHash, DRAWABLE_TYPE_FRAGMENT_COMMON_DRAWABLE);
		WriteDrawable(g_FragmentStore, slot, FRAGMENT_CLOTH_DRAWABLE_ENTRY, fragment->GetClothDrawable(), 0, 0, assetPathHash, DRAWABLE_TYPE_FRAGMENT_CLOTH_DRAWABLE);

		for (int i = 0; i < fragment->GetNumExtraDrawables(); i++)
		{
			u8 fragmentDrawableType = DRAWABLE_TYPE_FRAGMENT_EXTRA_DRAWABLE;

			if (strcmp(fragment->GetExtraDrawableName(i), "damaged") == 0)
			{
				fragmentDrawableType = DRAWABLE_TYPE_FRAGMENT_DAMAGED_DRAWABLE;
			}

			WriteDrawable(g_FragmentStore, slot, i, fragment->GetExtraDrawable(i), 0, 0, assetPathHash, fragmentDrawableType);
		}
	}
}

static void WriteParticle(int slot)
{
	const ptxFxList* ptx = g_ParticleStore.Get(GTA_ONLY(strLocalIndex)(slot));

	if (ptx)
	{
		const atString assetPath     = GetAssetPath(g_ParticleStore, slot);
		const u64      assetPathHash = GET_CRC64("Particle", assetPath.c_str());

		const u32 physicalSize = GetAssetSize(g_ParticleStore, slot, ASSET_SIZE_PHYSICAL);
		const u32 virtualSize  = GetAssetSize(g_ParticleStore, slot, ASSET_SIZE_VIRTUAL);

		fiStream* stream = GetStream(STREAM_TYPE_PARTICLE, assetPath.c_str());

		if (stream)
		{
			// NOTE -- keep in sync with %RS_CODEBRANCH%\tools\AssetAnalysisTool\src\AssetAnalysisTool.h
			/*class aaParticleRecord
			{
			public:
				enum { TAG = 'PART' };

				u32 m_tag;
				u32 m_physicalSize;
				u32 m_virtualSize;
				u64 m_assetPathHash;
			};*/
			const u32 tag = 'PART';
			stream->WriteInt (&tag, 1);
			stream->WriteInt (&physicalSize, 1);
			stream->WriteInt (&virtualSize, 1);
			stream->WriteLong(&assetPathHash, 1);
		}

		WriteString64AppendToRPFPath(g_ParticleStore, slot, assetPath.c_str());
		WriteTextureDictionary(g_ParticleStore, slot, INDEX_NONE, ptx->GetTextureDictionary(), 0, 0);

		const Dwd* dwd = ptx->GetModelDictionary();

		if (dwd)
		{
			for (int i = 0; i < dwd->GetCount(); i++)
			{
				// note -- entries in the particle model dwd should not have packed textures
				WriteDrawable(g_ParticleStore, slot, i, dwd->GetEntry(i), 0, 0, assetPathHash, DRAWABLE_TYPE_PARTICLE_MODEL);
			}
		}

		const ptxParticleRuleDictionary* prd = ptx->GetParticleRuleDictionary();

		if (0 && prd) // TODO -- test this and enable it
		{
			for (int i = 0; i < prd->GetCount(); i++)
			{
				const ptxParticleRule* rule = prd->GetEntry(i);

				if (rule && rule->GetDrawType() != PTXPARTICLERULE_DRAWTYPE_MODEL)
				{
					const atString drawablePath     = atVarString("%s/ptxrules/%s", assetPath.c_str(), rule->GetName());
				//	const u64      drawablePathHash = crc64_lower(drawablePath.c_str());

					const ptxShaderInst& si = rule->GetShaderInst();
					const ptxInstVars& instVars = si.GetShaderVars();

					for (int j = 0; j < instVars.GetCount(); j++)
					{
						const ptxShaderVar* sv = instVars[j];

						if (sv->GetType() == PTXSHADERVAR_TEXTURE)
						{
							const ptxShaderVarTexture* svt = static_cast<const ptxShaderVarTexture*>(sv);
							const grcTexture* texture = svt->GetTexture();

							if (texture && dynamic_cast<const grcRenderTarget*>(texture) == NULL) // make sure this isn't a rendertarget
							{
								const char* materialVarName  = atFinalHashString::TryGetString(sv->GetHashName());
								const u64   materialPathHash = 0; // ?

								WriteMaterialTexture(g_ParticleStore, slot, i, NULL, texture, 0, materialPathHash, materialVarName); 
							}
						}
					}

					WriteParticleRuleMaterial(slot, i, si.GetGrmShader()); // TODO -- this isn't hooked up to anything yet
				}
			}
		}
	}
}

static u64 WriteArchetypeInstance(const CEntity* entity, const CBaseModelInfo* modelInfo, Mat34V_In matrix, u64 containingMLONameHash, u64 mapDataPathHash)
{
	if (modelInfo)
	{
		const CDebugArchetypeProxy* arch = CDebugArchetype::GetDebugArchetypeProxyForModelInfo(modelInfo);

		if (AssertVerify(arch))
		{
			const atString archetypeName = atString(arch->GetModelName());
			const atString archetypePath = GetArchetypePath(arch);

			const u64 archetypeNameHash = GET_CRC64("Archetype", archetypeName.c_str());
			const u64 archetypePathHash = GET_CRC64("Archetype", archetypePath.c_str());

			u64   lodArchetypeNameHash = 0;
			u16   entityLodDistance    = 0;
			float entityBoundRadius    = 0.0f;
			Vec3V lodPivot             = Vec3V(V_ZERO);
			float lodParentRatio       = 0.0f;
			float lodRadiusRatio       = 0.0f;
			u8    lodWarningFlags      = 0;

			if (entity)
			{
				const CEntity* lod = (const CEntity*)const_cast<CEntity*>(entity)->GetLod();

				if (lod)
				{
					const CDebugArchetypeProxy* lodProxy = CDebugArchetype::GetDebugArchetypeProxyForModelInfo(lod->GetBaseModelInfo());

					if (lodProxy)
					{
						lodArchetypeNameHash = crc64_lower(lodProxy->GetModelName());
					}
				}

				entityLodDistance = (u16)entity->GetLodDistance();
				entityBoundRadius = entity->GetBoundRadius();
				lodParentRatio    = lod ? (float)entityLodDistance/(float)lod->GetLodDistance() : 0.0f; // bad if > 1
				lodRadiusRatio    = entityBoundRadius/(float)entityLodDistance; // bad if > 1

				// check lod distance matches siblings
				if (lod && lod->GetLodData().GetChildLodDistance() != entity->GetLodDistance())
				{
					lodWarningFlags |= BIT(0);
				}

				// check if lod distance is too small when considering distance to parent’s pivot
				g_LodMgr.GetBasisPivot(const_cast<CEntity*>(entity), RC_VECTOR3(lodPivot));
				const spdSphere lodRangeSphere(lodPivot, ScalarV((float)entityLodDistance));
				const spdSphere entityBoundingSphere = entity->GetBoundSphere();

				if (!lodRangeSphere.ContainsSphere(entityBoundingSphere))
				{
					lodWarningFlags |= BIT(1);
				}
			}

			const Vec3V matrixCol0 = matrix.GetCol0();
			const Vec3V matrixCol1 = matrix.GetCol1();
			const Vec3V matrixCol2 = matrix.GetCol2();
			const Vec3V matrixCol3 = matrix.GetCol3();

			fiStream* stream = GetStream(STREAM_TYPE_ARCHETYPE_INSTANCE, archetypePath.c_str());

			if (stream)
			{
				// NOTE -- keep in sync with %RS_CODEBRANCH%\tools\AssetAnalysisTool\src\AssetAnalysisTool.h
				/*class aaArchetypeInstanceRecord
				{
				public:
					u32   m_tag;
					u64   m_archetypeNameHash;
					u64   m_archetypePathHash;
					u64   m_mapDataPathHash;
					u64   m_containingMLONameHash; // 0 if this is not an interior entity
					u64   m_lodArchetypeNameHash;
					u16   m_entityLodDistance;
					float m_entityBoundRadius;
					float m_lodPivot[3];
					float m_lodParentRatio;
					float m_lodRadiusRatio;
					u8    m_lodWarningFlags;
					float m_matrixCol0[3];
					float m_matrixCol1[3];
					float m_matrixCol2[3];
					float m_matrixCol3[3];
				};*/
				const u32 tag = 'INST';
				stream->WriteInt  (&tag, 1);
				stream->WriteLong (&archetypeNameHash, 1);
				stream->WriteLong (&archetypePathHash, 1);
				stream->WriteLong (&mapDataPathHash, 1);
				stream->WriteLong (&containingMLONameHash, 1);
				stream->WriteLong (&lodArchetypeNameHash, 1);
				stream->WriteShort(&entityLodDistance, 1);
				stream->WriteFloat(&entityBoundRadius, 1);
				stream->WriteFloat((const float*)&lodPivot, 3);
				stream->WriteFloat(&lodParentRatio, 1);
				stream->WriteFloat(&lodRadiusRatio, 1);
				stream->WriteByte (&lodWarningFlags, 1);
				stream->WriteFloat((const float*)&matrixCol0, 3);
				stream->WriteFloat((const float*)&matrixCol1, 3);
				stream->WriteFloat((const float*)&matrixCol2, 3);
				stream->WriteFloat((const float*)&matrixCol3, 3);
			}

			return archetypeNameHash;
		}
	}

	return 0;
}

static void WriteMapData(int slot)
{
	const fwMapDataContents* mapData = g_MapDataStore.Get(GTA_ONLY(strLocalIndex)(slot));

	if (mapData)
	{
		const fwMapDataDef* def = g_MapDataStore.GetSlot(GTA_ONLY(strLocalIndex)(slot));

		const atString mapDataPath     = GetAssetPath(g_MapDataStore, slot);
		const u64      mapDataPathHash = GET_CRC64("MapData", mapDataPath.c_str());

		const fwMapDataDef* parentDef      = def->GetParentDef();
		const int           parentSlot     = parentDef ? g_MapDataStore.GetSlotIndex(parentDef)GTA_ONLY(.Get()) : INDEX_NONE;
		const u64           parentPathHash = (parentSlot != INDEX_NONE) ? GET_CRC64("MapData", GetAssetPath(g_MapDataStore, parentSlot).c_str()) : 0;

		const u32 contentsFlags = g_MapDataStore.GetSlot(GTA_ONLY(strLocalIndex)(slot))->GetContentFlags();

		const u32 physicalSize = GetAssetSize(g_MapDataStore, slot, ASSET_SIZE_PHYSICAL);
		const u32 virtualSize  = GetAssetSize(g_MapDataStore, slot, ASSET_SIZE_VIRTUAL);

		fiStream* stream = GetStream(STREAM_TYPE_MAPDATA, mapDataPath.c_str());

		if (stream)
		{
			// NOTE -- keep in sync with %RS_CODEBRANCH%\tools\AssetAnalysisTool\src\AssetAnalysisTool.h
			/*class aaMapDataRecord
			{
			public:
				enum { TAG = 'MAPD' };

				u32 m_tag;
				u32 m_physicalSize;
				u32 m_virtualSize;
				u64 m_mapDataPathHash;
				u64 m_parentPathHash;
				u32 m_contentsFlags;
			};*/
			const u32 tag = 'MAPD';
			stream->WriteInt (&tag, 1);
			stream->WriteInt (&physicalSize, 1);
			stream->WriteInt (&virtualSize, 1);
			stream->WriteLong(&mapDataPathHash, 1);
			stream->WriteLong(&parentPathHash, 1);
			stream->WriteInt (&contentsFlags, 1);
		}

		WriteString64AppendToRPFPath(g_MapDataStore, slot, mapDataPath.c_str());

		for (int i = 0; i < mapData->GetNumEntities(); i++)
		{
			const CEntity* entity = static_cast<const CEntity*>(mapData->GetEntities()[i]);

			if (entity)
			{
				const CBaseModelInfo* modelInfo = entity->GetBaseModelInfo();

				if (modelInfo)
				{
					const u64 archetypeNameHash = WriteArchetypeInstance(entity, entity->GetBaseModelInfo(), entity->GetMatrix(), 0, mapDataPathHash);

					if (modelInfo && modelInfo->GetModelType() == MI_TYPE_MLO)
					{
						const atArray<fwEntityDef*>* interiorEntityDefs = &static_cast<CMloModelInfo*>(const_cast<CBaseModelInfo*>(modelInfo))->GetEntities();

						if (interiorEntityDefs) // this can be NULL?
						{
							for (int j = 0; j < interiorEntityDefs->GetCount(); j++)
							{
								const fwEntityDef* interiorEntityDef = interiorEntityDefs->operator[](j);

								if (interiorEntityDef)
								{
									const u32 interiorModelIndex = CModelInfo::GetModelIdFromName(interiorEntityDef->m_archetypeName).GetModelIndex();
									/*const char* interiorModelName = interiorEntityDef->m_archetypeName.TryGetCStr();

									if (interiorModelName == NULL)
									{
										interiorModelName = "NULL";
									}*/

									if (interiorModelIndex != fwModelId::MI_INVALID)
									{
										const CBaseModelInfo* interiorModelInfo = CModelInfo::GetBaseModelInfo(fwModelId(GTA_ONLY(strLocalIndex)(interiorModelIndex)));

										if (interiorModelInfo)
										{
											const QuatV localQuat(-interiorEntityDef->m_rotation.x, -interiorEntityDef->m_rotation.y, -interiorEntityDef->m_rotation.z, interiorEntityDef->m_rotation.w);
											const Vec3V localPos(interiorEntityDef->m_position);
											Mat34V localMatrix;
											Mat34VFromQuatV(localMatrix, localQuat, localPos);
											Mat34V interiorMatrix;
											Transform(interiorMatrix, localMatrix, entity->GetMatrix());

											WriteArchetypeInstance(NULL, interiorModelInfo, interiorMatrix, archetypeNameHash, mapDataPathHash);
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
}

static u8* g_TxdSlots      = NULL;
static u8* g_DwdSlots      = NULL;
static u8* g_DrawableSlots = NULL;
static u8* g_FragmentSlots = NULL;
static u8* g_ParticleSlots = NULL;
static u8* g_MapDataSlots  = NULL;

void AddWidgets(bkBank& bk)
{
	bk.PushGroup("Asset Analysis", false);
	{
		bk.AddToggle("Enabled"             , &g_enabled);
		bk.AddToggle("Verbose"             , &g_verbose);
		bk.AddToggle("Process Dependencies", &g_processDependencies);
		bk.AddToggle("No Archetypes"       , &g_noArchetypeStream);
	}
	bk.PopGroup();
}

bool ShouldLoadTxdSlot     (int slot) { return g_TxdSlots      == NULL || (g_TxdSlots     [slot/8] & BIT(slot%8)) == 0; }
bool ShouldLoadDwdSlot     (int slot) { return g_DwdSlots      == NULL || (g_DwdSlots     [slot/8] & BIT(slot%8)) == 0; }
bool ShouldLoadDrawableSlot(int slot) { return g_DrawableSlots == NULL || (g_DrawableSlots[slot/8] & BIT(slot%8)) == 0; }
bool ShouldLoadFragmentSlot(int slot) { return g_FragmentSlots == NULL || (g_FragmentSlots[slot/8] & BIT(slot%8)) == 0; }
bool ShouldLoadParticleSlot(int slot) { return g_ParticleSlots == NULL || (g_ParticleSlots[slot/8] & BIT(slot%8)) == 0; }
bool ShouldLoadMapDataSlot (int slot) { return g_MapDataSlots  == NULL || (g_MapDataSlots [slot/8] & BIT(slot%8)) == 0; }

bool IsWritingStreams_DEPRECATED()
{
	return g_streamsEnabled;
}

static void WriteTimestamp(const char* str)
{
	const u32 time0 = fwTimer::GetSystemTimeInMilliseconds();
	char dateTimeStr[256] = "";
#if __PS3
	CellRtcDateTime clk;
	cellRtcGetCurrentClockLocalTime(&clk);
	sprintf(dateTimeStr, "%04d/%02d/%02d %02d:%02d:%02d", clk.year, clk.month, clk.day, clk.hour, clk.minute, clk.second);
#elif __XENON
	SYSTEMTIME clk;
	GetLocalTime(&clk);
	sprintf(dateTimeStr, "%04d/%02d/%02d", clk.wYear, clk.wMonth, clk.wDay);
#endif // __XENON
	char headerStr[256] = "";
	sprintf(headerStr, "%s AT %s [%d] (version %04d.%02d built on %s)", str, dateTimeStr, time0, ASSET_ANALYSIS_VERSION, ASSET_ANALYSIS_SUBVERSION, __DATE__);
	WriteString(headerStr, 32);
}

void BeginWritingStreams_PRIVATE()
{
	if (g_streamsEnabled)
	{
		return;
	}

	g_streamsEnabled = true;

	WriteTimestamp("START");
	WriteString("static", 64); // for 'static' archetypes, this is the rpf path
	WriteString("./image", 64);
	WriteString("common:/data/ped_detail_atlas", 64);

	// write strings for z_z_* assets (which will be skipped)
	{
		for (int i = 0; i < g_TxdStore.GetCount(); i++)
		{
			const char* name = g_TxdStore.GetName(GTA_ONLY(strLocalIndex)(i));

			if (stristr(name, "z_z_"))
			{
				WriteString(AssetAnalysis::GetAssetPath(g_TxdStore, i).c_str(), 64);
			}
		}

		for (int i = 0; i < g_DwdStore.GetCount(); i++)
		{
			const char* name = g_DwdStore.GetName(GTA_ONLY(strLocalIndex)(i));

			if (stristr(name, "z_z_"))
			{
				WriteString(AssetAnalysis::GetAssetPath(g_DwdStore, i).c_str(), 64);
			}
		}

		for (int i = 0; i < g_DrawableStore.GetCount(); i++)
		{
			const char* name = g_DrawableStore.GetName(GTA_ONLY(strLocalIndex)(i));

			if (stristr(name, "z_z_"))
			{
				WriteString(AssetAnalysis::GetAssetPath(g_DrawableStore, i).c_str(), 64);
			}
		}

		for (int i = 0; i < g_FragmentStore.GetCount(); i++)
		{
			const char* name = g_FragmentStore.GetName(GTA_ONLY(strLocalIndex)(i));

			if (stristr(name, "z_z_"))
			{
				WriteString(AssetAnalysis::GetAssetPath(g_FragmentStore, i).c_str(), 64);
			}
		}
	}

	if (!g_noArchetypeStream)
	{
		for (u32 i = 0; i < CDebugArchetype::GetMaxDebugArchetypeProxies(); i++)
		{
			const CDebugArchetypeProxy* arch = CDebugArchetype::GetDebugArchetypeProxy(i);

			if (arch)
			{
				const atString archetypeName = atString(arch->GetModelName());
				const atString archetypePath = GetArchetypePath(arch);

				const u64 archetypeNameHash = GET_CRC64("Archetype", archetypeName.c_str());
				const u64 archetypePathHash = GET_CRC64("Archetype", archetypePath.c_str());

				u8 archetypeFlags = 0;

				if (arch->GetIsProp         ()) { archetypeFlags |= DEBUG_ARCHETYPE_PROXY_IS_PROP          ; }
				if (arch->GetIsTree         ()) { archetypeFlags |= DEBUG_ARCHETYPE_PROXY_IS_TREE          ; }
				if (arch->GetDontCastShadows()) { archetypeFlags |= DEBUG_ARCHETYPE_PROXY_DONT_CAST_SHADOWS; }
				if (arch->GetIsShadowProxy  ()) { archetypeFlags |= DEBUG_ARCHETYPE_PROXY_SHADOW_PROXY     ; }
				if (arch->GetHasAnimUV      ()) { archetypeFlags |= BIT(4); }

				const u8    modelInfoType = (u8)arch->GetModelType();
				const float HDTexDistance = arch->GetHDTextureDistance();
				const float lodDistance   = arch->GetLodDistanceUnscaled();

				const atString parentParticlePath     = GetAssetPath(g_ParticleStore, arch->GetPtFxAssetSlot());
				const u64      parentParticlePathHash = GET_CRC64("Archetype", parentParticlePath.c_str());

				fiStream* stream = GetStream(STREAM_TYPE_ARCHETYPE, archetypePath.c_str());

				if (stream)
				{
					// NOTE -- keep in sync with %RS_CODEBRANCH%\tools\AssetAnalysisTool\src\AssetAnalysisTool.h
					/*class aaArchetypeRecord
					{
					public:
						u32   m_tag;
						u64   m_archetypeNameHash;
						u64   m_archetypePathHash;
						u8    m_archetypeFlags;
						u8    m_modelInfoType;
						float m_HDTexDistance;
						float m_lodDistance;
						u64   m_parentParticlePathHash;
						u64   m_HDTxdPathHash; // peds and vehicles
						u64   m_HDDrawableOrFragmentPathHash; // vehicles and weapons
						u64   m_pedCompDwdPathHash; // peds only
						u64   m_pedPropDwdPathHash; // peds only
					};*/
					const u32 tag = 'ARCH';
					stream->WriteInt  (&tag, 1);
					stream->WriteLong (&archetypeNameHash, 1);
					stream->WriteLong (&archetypePathHash, 1);
					stream->WriteByte (&archetypeFlags, 1);
					stream->WriteByte (&modelInfoType, 1);
					stream->WriteFloat(&HDTexDistance, 1);
					stream->WriteFloat(&lodDistance, 1);
					stream->WriteLong (&parentParticlePathHash, 1);

					if (arch->GetModelType() == MI_TYPE_PED)
					{
						const CDebugPedArchetypeProxy* pedArch = static_cast<const CDebugPedArchetypeProxy*>(arch);

						const atString pedHDTxdPath     = GetAssetPath(g_TxdStore, pedArch->GetHDTxdIndex());
						const u64      pedHDTxdPathHash = GET_CRC64("Archetype", pedHDTxdPath.c_str());

						const atString pedCompDwdPath     = GetAssetPath(g_DwdStore, pedArch->GetPedComponentFileIndex());
						const u64      pedCompDwdPathHash = GET_CRC64("Archetype", pedCompDwdPath.c_str());

						const atString pedPropDwdPath     = GetAssetPath(g_DwdStore, pedArch->GetPropsFileIndex());
						const u64      pedPropDwdPathHash = GET_CRC64("Archetype", pedPropDwdPath.c_str());

						const u64 dummy = 0;

						stream->WriteLong(&pedHDTxdPathHash, 1);
						stream->WriteLong(&dummy, 1); // m_HDDrawableOrFragmentPathHash
						stream->WriteLong(&pedCompDwdPathHash, 1);
						stream->WriteLong(&pedPropDwdPathHash, 1);
					}
					else if (arch->GetModelType() == MI_TYPE_VEHICLE)
					{
						const CDebugVehicleArchetypeProxy* vehicleArch = static_cast<const CDebugVehicleArchetypeProxy*>(arch);

						const atString vehicleHDTxdPath     = GetAssetPath(g_TxdStore, vehicleArch->GetHDTxdIndex());
						const u64      vehicleHDTxdPathHash = GET_CRC64("Archetype", vehicleHDTxdPath.c_str());

						const atString vehicleHDFragmentPath     = GetAssetPath(g_FragmentStore, vehicleArch->GetHDFragmentIndex());
						const u64      vehicleHDFragmentPathHash = GET_CRC64("Archetype", vehicleHDFragmentPath.c_str());

						const u64 dummy = 0;

						stream->WriteLong(&vehicleHDTxdPathHash, 1);
						stream->WriteLong(&vehicleHDFragmentPathHash, 1);
						stream->WriteLong(&dummy, 1); // pedCompDwd
						stream->WriteLong(&dummy, 1); // pedPropDwd
					}
					else if (arch->GetModelType() == MI_TYPE_WEAPON)
					{
						const CDebugWeaponArchetypeProxy* weaponArch = static_cast<const CDebugWeaponArchetypeProxy*>(arch);

						const atString weaponHDTxdPath     = GetAssetPath(g_TxdStore, weaponArch->GetHDTxdIndex());
						const u64      weaponHDTxdPathHash = GET_CRC64("Archetype", weaponHDTxdPath.c_str());

						const atString weaponHDDrawablePath     = GetAssetPath(g_DrawableStore, weaponArch->GetHDDrawableIndex());
						const u64      weaponHDDrawablePathHash = GET_CRC64("Archetype", weaponHDDrawablePath.c_str());

						const u64 dummy = 0;

						stream->WriteLong(&weaponHDTxdPathHash, 1);
						stream->WriteLong(&weaponHDDrawablePathHash, 1);
						stream->WriteLong(&dummy, 1); // pedCompDwd
						stream->WriteLong(&dummy, 1); // pedPropDwd
					}
					else
					{
						const u64 dummy = 0;

						stream->WriteLong(&dummy, 1); // HDTxd
						stream->WriteLong(&dummy, 1); // HDDrawableOrFragmentPathHash
						stream->WriteLong(&dummy, 1); // pedCompDwd
						stream->WriteLong(&dummy, 1); // pedPropDwd
					}
				}

				// TODO -- CWeaponModelInfo also has HD txd and drawable (to store these, we need to create a debug archetype proxy for them)
				//		inline s32 GetHDDrawableIndex() const { return m_HDDrwbIdx; }
				//		inline s32 GetHDTxdIndex() const { return m_HDtxdIdx; }

				// TODO -- do we need to store both name and path? we can get the name from the path .. but we rely on name hashed in the tool
				// we could just construct the crc of the name from the path in the tool ..
				// .. and similarly we can do the same for constructing texture names from paths, no need to store both of these (and their crc's)
				// in the files
				WriteString(archetypeName.c_str(), 64);
				WriteString(archetypePath.c_str(), 64);
			}
		}
	}

	ProcessTxdSlot(g_TxdStore.FindSlot("platform:/textures/mapdetail")GTA_ONLY(.Get()));
}

template <typename T> static void FinishWritingFailedSlots(T& store, int assetType)
{
	const atArray<CStreamingIteratorSlot>* failedSlots = CStreamingIteratorManager::GetFailedSlots(assetType);

	if (failedSlots)
	{
		for (int i = 0; i < failedSlots->GetCount(); i++)
		{
			const atString path = AssetAnalysis::GetAssetPath(store, failedSlots->operator[](i).m_slot GTA_ONLY(.Get()));

			AssetAnalysis::WriteString(path.c_str(), 64);
			AssetAnalysis::WriteToLogFile("[ERROR] failed to stream in %s", path.c_str());
		}
	}
}

void FinishWritingStreams_PRIVATE()
{
	if (g_streamsEnabled)
	{
		WriteTimestamp("END");

		FinishWritingFailedSlots(g_TxdStore     , AssetAnalysis::ASSET_TYPE_TXD);
		FinishWritingFailedSlots(g_DwdStore     , AssetAnalysis::ASSET_TYPE_DWD);
		FinishWritingFailedSlots(g_DrawableStore, AssetAnalysis::ASSET_TYPE_DRAWABLE);
		FinishWritingFailedSlots(g_FragmentStore, AssetAnalysis::ASSET_TYPE_FRAGMENT);
		FinishWritingFailedSlots(g_ParticleStore, AssetAnalysis::ASSET_TYPE_PARTICLE);
		FinishWritingFailedSlots(g_MapDataStore , AssetAnalysis::ASSET_TYPE_MAPDATA);

		for (int i = 0; i < STREAM_TYPE_COUNT; i++)
		{
			if (g_streams[i])
			{
				if (i == STREAM_TYPE_LOG_FILE) // any text file
				{
					fprintf(g_streams[i], "%d lines\n", g_streamsEntries[i]);
					fprintf(g_streams[i], "EOF.\n");
				}
				else
				{
					const u32 tag = 'EOF.';
					g_streams[i]->WriteInt(&g_streamsEntries[i], 1);
					g_streams[i]->WriteInt(&tag, 1);
				}

				g_streams[i]->Close();
				g_streams[i] = NULL;
			}
		}

		{
			// Debug Heap
			sysMemAutoUseDebugMemory debug;

			if (g_TxdSlots)
			{
				delete [] g_TxdSlots;
				g_TxdSlots = NULL; 
			}

			if (g_DwdSlots)
			{
				delete [] g_DwdSlots;
				g_DwdSlots = NULL; 
			}

			if (g_DrawableSlots)
			{
				delete [] g_DrawableSlots;
				g_DrawableSlots = NULL; 
			}

			if (g_FragmentSlots)
			{
				delete [] g_FragmentSlots;
				g_FragmentSlots = NULL; 
			}

			if (g_ParticleSlots)
			{
				delete [] g_ParticleSlots;
				g_ParticleSlots = NULL; 
			}

			if (g_MapDataSlots)
			{
				delete [] g_MapDataSlots;
				g_MapDataSlots = NULL; 
			}
		}

		g_uniqueStrings32.Kill();
		g_uniqueStrings64.Kill();

		g_streamsEnabled = false;

		fiStream* finished = fiStream::Create("x:/sync.aa1");

		if (finished)
		{
			finished->Close();
		}
	}
}

void ProcessTxdSlot(int slot)
{
	if (g_enabled)
	{
		if (!g_TxdStore.IsValidSlot(GTA_ONLY(strLocalIndex)(slot)) || g_TxdStore.GetSlot(GTA_ONLY(strLocalIndex)(slot))->m_isDummy)
		{
			return;
		}

		if (!g_streamsEnabled)
		{
			BeginWritingStreams_PRIVATE();
		}

		if (g_TxdSlots == NULL)
		{
			const int numBytes = (g_TxdStore.GetNumUsedSlots() + 7)/8;

			{
				// Debug Heap
				sysMemAutoUseDebugMemory debug;
				g_TxdSlots = rage_new u8[numBytes];
			}
			
			sysMemSet(g_TxdSlots, 0, numBytes);
		}

		if ((g_TxdSlots[slot/8] & BIT(slot%8)) == 0)
		{
			const u32 physicalSize = GetAssetSize(g_TxdStore, slot, ASSET_SIZE_PHYSICAL);
			const u32 virtualSize  = GetAssetSize(g_TxdStore, slot, ASSET_SIZE_VIRTUAL);

			WriteTextureDictionary(g_TxdStore, slot, INDEX_NONE, g_TxdStore.Get(GTA_ONLY(strLocalIndex)(slot)), physicalSize, virtualSize);
			g_TxdSlots[slot/8] |= BIT(slot%8);
		}

		if (g_processDependencies)
		{
			ProcessTxdSlot(g_TxdStore.GetParentTxdSlot(GTA_ONLY(strLocalIndex)(slot))GTA_ONLY(.Get()));
		}
	}
}

void ProcessDwdSlot(int slot)
{
	if (g_enabled)
	{
		if (!g_DwdStore.IsValidSlot(GTA_ONLY(strLocalIndex)(slot)))
		{
			return;
		}

		if (!g_streamsEnabled)
		{
			BeginWritingStreams_PRIVATE();
		}

		if (g_DwdSlots == NULL)
		{
			const int numBytes = (g_DwdStore.GetNumUsedSlots() + 7)/8;

			{
				// Debug Heap
				sysMemAutoUseDebugMemory debug;
				g_DwdSlots = rage_new u8[numBytes];
			}

			sysMemSet(g_DwdSlots, 0, numBytes);
		}

		if ((g_DwdSlots[slot/8] & BIT(slot%8)) == 0)
		{
			WriteDrawableDictionary(slot);
			g_DwdSlots[slot/8] |= BIT(slot%8);
		}

		if (g_processDependencies)
		{
			ProcessTxdSlot(g_DwdStore.GetParentTxdForSlot(GTA_ONLY(strLocalIndex)(slot))GTA_ONLY(.Get()));
		}
	}
}

void ProcessDrawableSlot(int slot)
{
	if (g_enabled)
	{
		if (!g_DrawableStore.IsValidSlot(GTA_ONLY(strLocalIndex)(slot)))
		{
			return;
		}

		if (!g_streamsEnabled)
		{
			BeginWritingStreams_PRIVATE();
		}

		if (g_DrawableSlots == NULL)
		{
			const int numBytes = (g_DrawableStore.GetNumUsedSlots() + 7)/8;

			{
				// Debug Heap
				sysMemAutoUseDebugMemory debug;
				g_DrawableSlots = rage_new u8[numBytes];
			}

			sysMemSet(g_DrawableSlots, 0, numBytes);
		}

		if ((g_DrawableSlots[slot/8] & BIT(slot%8)) == 0)
		{
			const atString assetPath     = GetAssetPath(g_DrawableStore, slot);
			const u64      assetPathHash = GET_CRC64("DrawableSlot", assetPath);

			const u32 physicalSize = GetAssetSize(g_DrawableStore, slot, ASSET_SIZE_PHYSICAL);
			const u32 virtualSize  = GetAssetSize(g_DrawableStore, slot, ASSET_SIZE_VIRTUAL);

			WriteDrawable(g_DrawableStore, slot, INDEX_NONE, g_DrawableStore.Get(GTA_ONLY(strLocalIndex)(slot)), physicalSize, virtualSize, assetPathHash, DRAWABLE_TYPE_DRAWABLE);
			g_DrawableSlots[slot/8] |= BIT(slot%8);
		}

		if (g_processDependencies)
		{
			ProcessTxdSlot(g_DrawableStore.GetParentTxdForSlot(GTA_ONLY(strLocalIndex)(slot))GTA_ONLY(.Get()));
		}
	}
}

void ProcessFragmentSlot(int slot)
{
	if (g_enabled)
	{
		if (!g_FragmentStore.IsValidSlot(GTA_ONLY(strLocalIndex)(slot)))
		{
			return;
		}

		if (!g_streamsEnabled)
		{
			BeginWritingStreams_PRIVATE();
		}

		if (g_FragmentSlots == NULL)
		{
			const int numBytes = (g_FragmentStore.GetNumUsedSlots() + 7)/8;

			{
				// Debug Heap
				sysMemAutoUseDebugMemory debug;
				g_FragmentSlots = rage_new u8[numBytes];
			}
			
			sysMemSet(g_FragmentSlots, 0, numBytes);
		}

		if ((g_FragmentSlots[slot/8] & BIT(slot%8)) == 0)
		{
			WriteFragment(slot);
			g_FragmentSlots[slot/8] |= BIT(slot%8);
		}

		if (g_processDependencies)
		{
			ProcessTxdSlot(g_FragmentStore.GetParentTxdForSlot(GTA_ONLY(strLocalIndex)(slot))GTA_ONLY(.Get()));
		}
	}
}

void ProcessParticleSlot(int slot)
{
	if (g_enabled)
	{
		if (!g_ParticleStore.IsValidSlot(GTA_ONLY(strLocalIndex)(slot)))
		{
			return;
		}

		if (!g_streamsEnabled)
		{
			BeginWritingStreams_PRIVATE();
		}

		if (g_ParticleSlots == NULL)
		{
			const int numBytes = (g_ParticleStore.GetNumUsedSlots() + 7)/8;

			{
				// Debug Heap
				sysMemAutoUseDebugMemory debug;
				g_ParticleSlots = rage_new u8[numBytes];
			}

			sysMemSet(g_ParticleSlots, 0, numBytes);
		}

		if ((g_ParticleSlots[slot/8] & BIT(slot%8)) == 0)
		{
			WriteParticle(slot);
			g_ParticleSlots[slot/8] |= BIT(slot%8);
		}
	}
}

void ProcessMapDataSlot(int slot)
{
	if (g_enabled)
	{
		if (!g_MapDataStore.IsValidSlot(GTA_ONLY(strLocalIndex)(slot)))
		{
			return;
		}

		if (!g_streamsEnabled)
		{
			BeginWritingStreams_PRIVATE();
		}

		if (g_MapDataSlots == NULL)
		{
			const int numBytes = (g_MapDataStore.GetNumUsedSlots() + 7)/8;

			{
				// Debug Heap
				sysMemAutoUseDebugMemory debug;
				g_MapDataSlots = rage_new u8[numBytes];
			}

			sysMemSet(g_MapDataSlots, 0, numBytes);
		}

		if ((g_MapDataSlots[slot/8] & BIT(slot%8)) == 0)
		{
			WriteMapData(slot);
			g_MapDataSlots[slot/8] |= BIT(slot%8);
		}
	}
}

void Start()
{
	g_enabled = true;
}

void FinishAndReset()
{
	FinishWritingStreams_PRIVATE();
}

} // namespace AssetAnalysis

#endif // __BANK
