// ===========================================
// debug/AssetAnalysis/AssetAnalysisCommon.cpp
// (c) 2013 RockstarNorth
// ===========================================

#if __BANK || defined(ASSET_ANALYSIS_TOOL)

#if !defined(ASSET_ANALYSIS_TOOL)
#include "atl/string.h"
#include "system/nelem.h"

#include "fwscene/stores/drawablestore.h"
#include "fwscene/stores/dwdstore.h"
#include "fwscene/stores/fragmentstore.h"
#include "fwscene/stores/mapdatastore.h"
#include "fwscene/stores/txdstore.h"
#include "fwutil/xmacro.h"
#endif // !defined(ASSET_ANALYSIS_TOOL)

#include "AssetAnalysisCommon.h"

namespace AssetAnalysis {

const char* GetStreamName(eStreamType type)
{
	static const char* names[] =
	{
		"aa_string_table.dat",
		"aa_archetype.dat",
		"aa_archetype_instance.dat",
		"aa_mapdata.dat",
		"aa_texture.dat",
		"aa_texture_dictionary.dat",
		"aa_drawable.dat",
		"aa_drawable_dictionary.dat",
		"aa_fragment.dat",
		"aa_light.dat",
		"aa_particle.dat",
		"aa_material.dat",
		"aa_material_texture_var.dat",
		"aa.log",
	};
	CompileTimeAssert(NELEM(names) == STREAM_TYPE_COUNT);

	return names[type];
}

static u32  g_crc32_table[256] = {0};
static bool g_crc32_tableInited = false;

const void crc32_init()
{
	if (!g_crc32_tableInited)
	{
		u32 polynomial = 0x1EDC6F41UL; // CRC-32-C (Castagnoli93)

		// reverse bits
		{
			u32 a = polynomial;

			{ const u32 mask = 0x55555555UL; a = ((a >> 0x01) & mask) | ((a & mask) << 0x01); }
			{ const u32 mask = 0x33333333UL; a = ((a >> 0x02) & mask) | ((a & mask) << 0x02); }
			{ const u32 mask = 0x0f0f0f0fUL; a = ((a >> 0x04) & mask) | ((a & mask) << 0x04); }
			{ const u32 mask = 0x00ff00ffUL; a = ((a >> 0x08) & mask) | ((a & mask) << 0x08); }

			polynomial = (a >> 0x10) | (a << 0x10);
		}

		for (int i = 0; i < 256; i++)
		{
			u32 x = (u32)i;

			for (int j = 0; j < 8; j++)
			{
				if (x & 1) { x = (x >> 1) ^ polynomial; }
				else       { x = (x >> 1); }
			}

			g_crc32_table[i] = x;
		}

		g_crc32_tableInited = true;
	}
}

void crc32_char(unsigned char data, u32& crc)
{
	crc32_init();
	crc = g_crc32_table[(crc ^ data) & 0xff] ^ (crc >> 8);
}

u32 crc32_lower(const char* str)
{
	u32 x = 0;

	if (str)
	{
		while (*str)
		{
			crc32_char((unsigned char)tolower(*(str++)), x);
		}
	}

	return x;
}

static u64  g_crc64_table[256] = {0};
static bool g_crc64_tableInited = false;

const void crc64_init()
{
	if (!g_crc64_tableInited)
	{
		u64 polynomial = 0x42F0E1EBA9EA3693ULL; // CRC-64-ECMA-182

		// reverse bits
		{
			u64 a = polynomial;

			{ const u64 mask = 0x5555555555555555ULL; a = ((a >> 0x01) & mask) | ((a & mask) << 0x01); }
			{ const u64 mask = 0x3333333333333333ULL; a = ((a >> 0x02) & mask) | ((a & mask) << 0x02); }
			{ const u64 mask = 0x0f0f0f0f0f0f0f0fULL; a = ((a >> 0x04) & mask) | ((a & mask) << 0x04); }
			{ const u64 mask = 0x00ff00ff00ff00ffULL; a = ((a >> 0x08) & mask) | ((a & mask) << 0x08); }
			{ const u64 mask = 0x0000ffff0000ffffULL; a = ((a >> 0x10) & mask) | ((a & mask) << 0x10); }

			polynomial = (a >> 0x20) | (a << 0x20);
		}

		for (int i = 0; i < 256; i++)
		{
			u64 x = (u64)i;

			for (int j = 0; j < 8; j++)
			{
				if (x & 1) { x = (x >> 1) ^ polynomial; }
				else       { x = (x >> 1); }
			}

			g_crc64_table[i] = x;
		}

		g_crc64_tableInited = true;
	}
}

void crc64_char(unsigned char data, u64& crc)
{
	crc64_init();
	crc = g_crc64_table[(crc ^ data) & 0xff] ^ (crc >> 8);
}

u64 crc64_lower(const char* str)
{
	u64 x = 0;

	if (str)
	{
		while (*str)
		{
			crc64_char((unsigned char)tolower(*(str++)), x);
		}
	}

	return x;
}

} // namespace AssetAnalysis

#endif // __BANK
