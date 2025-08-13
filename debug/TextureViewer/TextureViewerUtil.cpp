// =========================================
// debug/textureviewer/textureviewerutil.cpp
// (c) 2010 RockstarNorth
// =========================================

#include "debug/textureviewer/textureviewerprivate.h"

#if __BANK

// Rage headers
#include "grcore/image.h"
#include "grcore/image_dxt.h"
#include "grcore/texture.h"
#include "grcore/fvf.h"
#include "grcore/vertexbuffer.h"
#include "grcore/indexbuffer.h"
#include "fragment/drawable.h"
#include "system/memory.h"

// Framework headers
#include "fwutil/xmacro.h"
#include "fwscene/stores/txdstore.h"
#include "fwscene/stores/dwdstore.h"
#include "fwscene/stores/drawablestore.h"
#include "fwscene/stores/fragmentstore.h"
#include "fwscene/stores/mapdatastore.h"

// Game headers
#include "modelinfo/modelinfo.h"
#include "modelinfo/pedmodelinfo.h"
#include "modelinfo/vehiclemodelinfo.h"
#include "peds/rendering/pedvariationstream.h"
#include "peds/ped.h"

#include "debug/textureviewer/textureviewerutil.h"

DEBUG_TEXTURE_VIEWER_OPTIMISATIONS()

// ================================================================================================

static atString StringToLower(const char* str)
{
	atString result(str);

	for (char* ptr = const_cast<char*>(result.c_str()); *ptr; ptr++)
	{
		*ptr = char( tolower(*ptr) );
	}

	return result;
}

#define MULTI_STRING_SEARCH (1)

bool StringMatch(const char* str_, const char* searchStr_, bool bExactMatch)
{
	if (searchStr_[0] == '*') // wildcard
	{
		return true;
	}

	if (searchStr_[0] == '\0' && !bExactMatch)
	{
		return true;
	}

	// TODO -- when passing bExactMatch=true this breaks, need to fix this ..
	// for now i'm implementing a simpler version using quotes
	{
		bExactMatch = false;

		if (searchStr_[0] == '\"' && searchStr_[1] != '\0')
		{
			return stricmp(str_, searchStr_ + 1) == 0;
		}
	}

#if MULTI_STRING_SEARCH
	char temp[512] = "";
	strcpy(temp, StringToLower(searchStr_).c_str());

	const char* searchStr = strtok(temp, ",");
#else
	const atString searchStr = StringToLower(searchStr_);
#endif
	const atString str = StringToLower(str_);

	bool bFoundExclusive = false; // will be true if any exclusive search strings are found
	bool bFoundInclusive = false; // will be true if any inclusive search strings are found
	bool bHasInclusive   = false; // will be true if there are any inclusive search strings

#if MULTI_STRING_SEARCH
	while (searchStr)
#endif
	{
		if (bExactMatch)
		{
			if (strcmp(str, searchStr) == 0)
			{
				return true;
			}
		}
		else
		{
			if (searchStr[0] == '!') // exclusive
			{
				if (strstr(str, searchStr + 1))
				{
					bFoundExclusive = true;
				}
			}
			else // inclusive
			{
				bHasInclusive = true;

				if (strstr(str, searchStr))
				{
					bFoundInclusive = true;
				}
			}
		}

#if MULTI_STRING_SEARCH
		searchStr = strtok(NULL, ",");
#endif
	}

	return (bFoundInclusive || !bHasInclusive) && !bFoundExclusive;
}

#undef MULTI_STRING_SEARCH

atString StringTruncate(const char* str, int maxLength, const char* trailing)
{
	const int len = istrlen(str);
	char temp[2048] = "";
	strcpy(temp, str);

	if (len > maxLength)
	{
		strcpy(temp + maxLength - strlen(trailing), trailing);
	}

	return atString(temp);
}

atString StringPadded(const char* str, int paddedLength, char padding, bool bAlignRight)
{
	const int len = istrlen(str);

	if (len < paddedLength)
	{
		atString temp(str);

		if (bAlignRight)
		{
			while (temp.length() < paddedLength) // slow, but this shouldn't matter
			{
				temp = atVarString("%c%s", padding, temp.c_str());
			}
		}
		else // align left
		{
			while (temp.length() < paddedLength) // slow, but this shouldn't matter
			{
				temp = atVarString("%s%c", temp.c_str(), padding);
			}
		}

		return temp;
	}

	return StringTruncate(str, paddedLength, "");
}

atString GetFriendlyTextureName(const grcTexture* texture)
{
	char buffer[256] = "";

	if (!texture)
	{
		strcpy(buffer, "NULL_TEXTURE");
	}
	else
	{
		const char* name = texture->GetName();

		if (name)
		{
			if (strstr(name, "pack:/") == name)
			{
				strcpy(buffer, name + strlen("pack:/"));
			}
			else
			{
				strcpy(buffer, name);
			}

			char* ext = strrchr(buffer, '.');

			if (ext && stricmp(ext, ".dds") == 0)
			{
				ext[0] = '\0';
			}
		}
		else
		{
			strcpy(buffer, "NO_NAME");
		}
	}

	return atString(buffer);
}

const char* GetEntityTypeStr(int type, bool bAbbrev) // type comes from CEntity::GetType()
{
	UNUSED_VAR(bAbbrev);

	switch (type)
	{
#define SWITCH_CASE(name) case name: return #name
		SWITCH_CASE(ENTITY_TYPE_NOTHING          );
		SWITCH_CASE(ENTITY_TYPE_BUILDING         );
		SWITCH_CASE(ENTITY_TYPE_ANIMATED_BUILDING);
		SWITCH_CASE(ENTITY_TYPE_VEHICLE          );
		SWITCH_CASE(ENTITY_TYPE_PED              );
		SWITCH_CASE(ENTITY_TYPE_OBJECT           );
		SWITCH_CASE(ENTITY_TYPE_DUMMY_OBJECT     );
		SWITCH_CASE(ENTITY_TYPE_PORTAL           );
		SWITCH_CASE(ENTITY_TYPE_MLO              );
		SWITCH_CASE(ENTITY_TYPE_NOTINPOOLS       );
		SWITCH_CASE(ENTITY_TYPE_PARTICLESYSTEM   );
		SWITCH_CASE(ENTITY_TYPE_LIGHT            );
		SWITCH_CASE(ENTITY_TYPE_COMPOSITE        );
#undef  SWITCH_CASE
	}

	return "ENTITY_TYPE_UNKNOWN";
}

const char* GetModelInfoTypeStr(int type, bool bAbbrev) // type comes from CModelInfo::GetModelType()
{
	if (bAbbrev)
	{
		switch (type)
		{
		case MI_TYPE_NONE      : return "NONE";
		case MI_TYPE_BASE      : return "BASE";
		case MI_TYPE_MLO       : return "MLO" ;
		case MI_TYPE_TIME      : return "TIME";
		case MI_TYPE_WEAPON    : return "WEAP";
		case MI_TYPE_VEHICLE   : return "VEH" ;
		case MI_TYPE_PED       : return "PED" ;
		case MI_TYPE_COMPOSITE : return "COMP";
		}

		return "???";
	}

	switch (type)
	{
#define SWITCH_CASE(name) case name: return #name
		SWITCH_CASE(MI_TYPE_NONE     );
		SWITCH_CASE(MI_TYPE_BASE     );
		SWITCH_CASE(MI_TYPE_MLO      );
		SWITCH_CASE(MI_TYPE_TIME     );
		SWITCH_CASE(MI_TYPE_WEAPON   );
		SWITCH_CASE(MI_TYPE_VEHICLE  );
		SWITCH_CASE(MI_TYPE_PED      );
		SWITCH_CASE(MI_TYPE_COMPOSITE);
#undef  SWITCH_CASE
	}

	return "MI_TYPE_UNKNOWN";
}

atString GetModelInfoDesc(u32 modelInfoIndex, bool bNewFormat)
{
	const CBaseModelInfo* modelInfo        = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelInfoIndex)));
	const char*           modelInfoName    = modelInfo->GetModelName();
	const char*           modelInfoTypeStr = GetModelInfoTypeStr(modelInfo->GetModelType(), true);

	if (bNewFormat)
	{
		return atVarString("Model/%s:%d(%s)", modelInfoTypeStr, modelInfoIndex, modelInfoName);
	}

	return atVarString("[%d] %s \"%s\"", modelInfoIndex, modelInfoTypeStr, modelInfoName);
}

int GetTextureSizeInBytes(const grcTexture* texture)
{
	const grcImage::Format format = (grcImage::Format)texture->GetImageFormat();

	int w           = texture->GetWidth();
	int h           = texture->GetHeight();
	int mips        = texture->GetMipMapCount();
	int layers      = texture->GetLayerCount();
	int bpp         = grcImage::GetFormatBitsPerPixel(format); // because i don't think texture->GetBitsPerPixel() always works ..
	int blockSize   = grcImage::IsFormatDXTBlockCompressed(format) ? 4 : 1;
	int sizeInBytes = 0;

	while (mips > 0)
	{
		sizeInBytes += (w*h*layers*bpp)/8;

		w = Max<int>(blockSize, w/2);
		h = Max<int>(blockSize, h/2);

		// if this is a volume texture, also divide layers by 2 ..

		mips--;
	}

	return sizeInBytes;
}

// (untested code)
// TODO -- replace internal code in AddTextureHashEntry
// TODO -- replace c'tor for CTextureUsageEntry (also note that operator int() does the hash, whereas we could do the hash in the c'tor)
// texture is always hashed by width,height,format,numMips
// if numLines != 0, then texture is hashed via pixel data as well
u32 GetTextureHash(const grcTexture* texture, bool bHashName, int numLines)
{
	u32 hash = 0;

	int w = texture->GetWidth      ();
	int h = texture->GetHeight     ();
	int n = texture->GetMipMapCount();
	int f = texture->GetImageFormat();

	hash = atDataHash((const char*)&w, sizeof(w), hash);
	hash = atDataHash((const char*)&h, sizeof(h), hash);
	hash = atDataHash((const char*)&n, sizeof(n), hash);
	hash = atDataHash((const char*)&f, sizeof(f), hash);

	if (bHashName)
	{
		hash = atStringHash(texture->GetName(), hash);
	}

	if (numLines != 0)
	{
		grcTextureLock lock;
		texture->LockRect(0, 0, lock, grcsRead | grcsAllowVRAMLock);

		int bpp = grcImage::GetFormatBitsPerPixel((grcImage::Format)f);

		if (grcImage::IsFormatDXTBlockCompressed((grcImage::Format)f))
		{
			w = (w + 3)/4;
			h = (h + 3)/4;
			bpp *= (4*4)/8; // bytes per block
		}
		else
		{
			bpp /= 8; // bytes per pixel
		}

		if (numLines <= -1 || numLines >= h) // hash all the pixel data (of the top mipmap), slow
		{
			hash = atDataHash((const char*)lock.Base, bpp*w*h, hash);
		}
		else // hash only some of the pixel data, should be much faster
		{
			for (int j = 0; j < numLines; j++)
			{
				const int y = j*(h - 1)/(numLines - 1);

				hash = atDataHash((const char*)lock.Base + bpp*w*y, bpp*w, hash);
			}
		}

		texture->UnlockRect(lock);
	}

	return hash;
}

int GetTextureDictionarySizeInBytes(const fwTxd* txd, bool bPhysical)
{
	int sizeInBytes = 0;

	for (int i = 0; i < txd->GetCount(); i++)
	{
		const grcTexture* texture = txd->GetEntry(i);

		if (texture)
		{
			if (bPhysical)
			{
				sizeInBytes += texture->GetPhysicalSize();
			}
			else
			{
				sizeInBytes += GetTextureSizeInBytes(texture);
			}
		}
	}

	return sizeInBytes;
}

int FindTxdSlot(const fwTxd* txd)
{
	if (txd)
	{
		for (int i = 0; i < g_TxdStore.GetSize(); i++)
		{
			if (g_TxdStore.GetSafeFromIndex(strLocalIndex(i)) == txd && !g_TxdStore.GetSlot(strLocalIndex(i))->m_isDummy)
			{
				return i;
			}
		}
	}

	return INDEX_NONE;
}

// ================================================================================================

// trying to understand "frag children" ..
// example of a modelinfo which contains damaged and undamaged frag children: "Prop_GasCyl_01A"
void PrintFragChildrenDesc(const CDynamicEntity* entity)
{
	const fragInst* pFragInst = entity->GetFragInst();

	if (pFragInst)
	{
		const int numChildren = pFragInst->GetTypePhysics()->GetNumChildren();

		Displayf("\"%s\" pFragInst num children = %d", entity->GetModelName(), numChildren);

		for (int i = 0; i < numChildren; i++)
		{
			const fragTypeChild* pChildType = pFragInst->GetTypePhysics()->GetChild(i);
			const Drawable* pDrawable0 = pChildType->GetDamagedEntity();
			const Drawable* pDrawable1 = pChildType->GetUndamagedEntity();

			if (pDrawable0) // damaged
			{
				const fwTxd* txd = pDrawable0->GetTexDictSafe();

				Displayf(
					"  child[%d] damaged drawable = %p (%d textures)%s",
					i,
					pDrawable0,
					txd ? txd->GetCount() : 0,
					txd && txd->GetCount() > 0 ? " [TEX]" : ""
				);
			}

			if (pDrawable1) // undamaged
			{
				const fwTxd* txd = pDrawable1->GetTexDictSafe();

				Displayf(
					"  child[%d] undamaged drawable = %p (%d textures)%s",
					i,
					pDrawable1,
					txd ? txd->GetCount() : 0,
					txd && txd->GetCount() > 0 ? " [TEX]" : ""
				);
			}
		}
	}
}

// ================================================================================================

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEST
// ugly extern templates .. should move these to common header
template <typename T> extern atString CDTVStreamingIteratorTest_GetAssetPath(T& store, int slot, int index);

template <> atString CDTVStreamingIteratorTest_GetAssetPath<fwTxdStore     >(fwTxdStore     & store, int slot, int index);
template <> atString CDTVStreamingIteratorTest_GetAssetPath<fwDwdStore     >(fwDwdStore     & store, int slot, int index);
template <> atString CDTVStreamingIteratorTest_GetAssetPath<fwDrawableStore>(fwDrawableStore& store, int slot, int index);
template <> atString CDTVStreamingIteratorTest_GetAssetPath<fwFragmentStore>(fwFragmentStore& store, int slot, int index);

#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEST

template <typename T> static bool IsTextureUsedByDrawable(T& store, int slot, int index, const Drawable* drawable, const grcTexture* texture, const char* textureName, const char** shaderName, const char** shaderVarName, bool bCheckNameOnly, atArray<const grcTexture*>* textureList = NULL)
{
#if EFFECT_PRESERVE_STRINGS
	if (drawable)
	{
		const grmShaderGroup* shaderGroup = &drawable->GetShaderGroup();

		for (int i = 0; i < shaderGroup->GetCount(); i++)
		{
			const grmShader* shader = shaderGroup->GetShaderPtr(i);

			for (int j = 0; j < shader->GetVarCount(); j++)
			{
				const grcEffectVar var = shader->GetVarByIndex(j);
				const char* name = NULL;
				grcEffect::VarType type;
				int annotationCount = 0;
				bool isGlobal = false;

				shader->GetVarDesc(var, name, type, annotationCount, isGlobal);

				if (type == grcEffect::VT_TEXTURE)
				{
					grcTexture* temp = NULL;
					shader->GetVar(var, temp);

					if (temp)
					{
						bool bMatch = false;

						if (texture)
						{
							if (temp == texture || (bCheckNameOnly && temp && stricmp(temp->GetName(), texture->GetName()) == 0))
							{
								bMatch = true;
							}
						}
						else if (textureName)
						{
							atString tempName = GetFriendlyTextureName(temp);
#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEST
							const atString assetPath = CDTVStreamingIteratorTest_GetAssetPath(store, slot, index);
							const char* unzipped = "unzipped/";
							const char* assetPathStart = strstr(assetPath.c_str(), unzipped);

							if (assetPathStart)
							{
								tempName = atVarString("%s%s", assetPathStart + strlen(unzipped), tempName.c_str());
							}
#else
							UNUSED_VAR(store);
							UNUSED_VAR(slot);
							UNUSED_VAR(index);
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEST

							if (StringMatch(tempName.c_str(), textureName))
							{
								bMatch = true;
							}
						}

						if (bMatch)
						{
							if (shaderName)
							{
								*shaderName = shader->GetName();
							}

							if (shaderVarName)
							{
								*shaderVarName = name;
							}

							if (textureList)
							{
								textureList->PushAndGrow(temp);
							}
							else
							{
								return true;
							}
						}
					}
				}
			}
		}

		if (textureList && textureList->GetCount() > 0)
		{
			return true;
		}
	}
#else // not EFFECT_PRESERVE_STRINGS
	UNUSED_VAR(store);
	UNUSED_VAR(slot);
	UNUSED_VAR(index);
	UNUSED_VAR(drawable);
	UNUSED_VAR(texture);
	UNUSED_VAR(textureName);
	UNUSED_VAR(shaderName);
	UNUSED_VAR(shaderVarName);
	UNUSED_VAR(bCheckNameOnly);
	UNUSED_VAR(textureList);
#endif // not EFFECT_PRESERVE_STRINGS
	return false;
}

static bool IsTextureUsedByFragment(const grcTexture* texture, const char* textureName, int fragmentIndex, const char** shaderName, const char** shaderVarName, bool bCheckNameOnly)
{
	const Fragment* fragment = g_FragmentStore.GetSafeFromIndex(strLocalIndex(fragmentIndex));

	if (fragment)
	{
		if (IsTextureUsedByDrawable(g_FragmentStore, fragmentIndex, INDEX_NONE, fragment->GetCommonDrawable(), texture, textureName, shaderName, shaderVarName, bCheckNameOnly))
		{
			return true;
		}

		if (IsTextureUsedByDrawable(g_FragmentStore, fragmentIndex, INDEX_NONE, fragment->GetClothDrawable(), texture, textureName, shaderName, shaderVarName, bCheckNameOnly))
		{
			return true;
		}

		for (int i = 0; i < fragment->GetNumExtraDrawables(); i++)
		{
			if (IsTextureUsedByDrawable(g_FragmentStore, fragmentIndex, i, fragment->GetExtraDrawable(i), texture, textureName, shaderName, shaderVarName, bCheckNameOnly))
			{
				return true;
			}
		}
	}

	return false;
}

static bool IsTextureUsedByDwd(const grcTexture* texture, const char* textureName, int dwdIndex, const char** shaderName, const char** shaderVarName, bool bCheckNameOnly)
{
	const Dwd* dwd = g_DwdStore.GetSafeFromIndex(strLocalIndex(dwdIndex));

	if (dwd)
	{
		for (int i = 0; i < dwd->GetCount(); i++)
		{
			if (IsTextureUsedByDrawable(g_DwdStore, dwdIndex, i, dwd->GetEntry(i), texture, textureName, shaderName, shaderVarName, bCheckNameOnly))
			{
				return true;
			}
		}
	}

	return false;
}

bool IsTextureUsedByModel(const grcTexture* texture, const char* textureName, const CBaseModelInfo* modelInfo, const CEntity* entity, const char** shaderName, const char** shaderVarName, bool bCheckNameOnly)
{
	if (texture == NULL)
	{
		return false;
	}

	if (modelInfo->GetModelType() == MI_TYPE_PED)
	{
		const CPedModelInfo* pedModelInfo = (const CPedModelInfo*)modelInfo;

		if (entity && pedModelInfo->GetIsStreamedGfx())
		{
			const CPedStreamRenderGfx* gfx = ((CPed*)entity)->GetPedDrawHandler().GetPedRenderGfx();

			if (gfx)
			{
				for (int i = 0; i < PV_MAX_COMP; i++)
				{
					if (IsTextureUsedByDwd(texture, textureName, gfx->m_dwdIdx[i], shaderName, shaderVarName, bCheckNameOnly))
					{
						return true;
					}
				}
			}
		}

		if (IsTextureUsedByDwd(texture, textureName, pedModelInfo->GetPedComponentFileIndex(), shaderName, shaderVarName, bCheckNameOnly) ||
			IsTextureUsedByDwd(texture, textureName, pedModelInfo->GetPropsFileIndex       (), shaderName, shaderVarName, bCheckNameOnly))
		{
			return true;
		}
	}
	else if (modelInfo->GetModelType() == MI_TYPE_VEHICLE)
	{
		const CVehicleModelInfo* vehModelInfo = (const CVehicleModelInfo*)modelInfo;

		if (IsTextureUsedByFragment(texture, textureName, vehModelInfo->GetHDFragmentIndex(), shaderName, shaderVarName, bCheckNameOnly))
		{
			return true;
		}
	}

	switch ((int)modelInfo->GetDrawableType())
	{
	case fwArchetype::DT_DRAWABLE:
		return IsTextureUsedByDrawable(g_DrawableStore, modelInfo->GetDrawableIndex(), INDEX_NONE, modelInfo->GetDrawable(), texture, textureName, shaderName, shaderVarName, bCheckNameOnly);
	case fwArchetype::DT_DRAWABLEDICTIONARY:
		return IsTextureUsedByDrawable(g_DwdStore, modelInfo->GetDrawDictIndex(), modelInfo->GetDrawDictDrawableIndex(), modelInfo->GetDrawable(), texture, textureName, shaderName, shaderVarName, bCheckNameOnly);
	case fwArchetype::DT_FRAGMENT:
		return IsTextureUsedByDrawable(g_FragmentStore, modelInfo->GetFragmentIndex(), INDEX_NONE, modelInfo->GetDrawable(), texture, textureName, shaderName, shaderVarName, bCheckNameOnly);
	}

	return false;
}

bool IsTextureUsedByDrawable_Dwd(int slot, int index, const rmcDrawable* drawable, const grcTexture* texture, const char* textureName, const char** shaderName, const char** shaderVarName, bool bCheckNameOnly, atArray<const grcTexture*>* textureList)
{
	return IsTextureUsedByDrawable(g_DwdStore, slot, index, drawable, texture, textureName, shaderName, shaderVarName, bCheckNameOnly, textureList);
}

bool IsTextureUsedByDrawable_Drawable(int slot, int, const rmcDrawable* drawable, const grcTexture* texture, const char* textureName, const char** shaderName, const char** shaderVarName, bool bCheckNameOnly, atArray<const grcTexture*>* textureList)
{
	return IsTextureUsedByDrawable(g_DrawableStore, slot, INDEX_NONE, drawable, texture, textureName, shaderName, shaderVarName, bCheckNameOnly, textureList);
}

bool IsTextureUsedByDrawable_Fragment(int slot, int index, const rmcDrawable* drawable, const grcTexture* texture, const char* textureName, const char** shaderName, const char** shaderVarName, bool bCheckNameOnly, atArray<const grcTexture*>* textureList)
{
	return IsTextureUsedByDrawable(g_FragmentStore, slot, index, drawable, texture, textureName, shaderName, shaderVarName, bCheckNameOnly, textureList);
}

// ================================================================================================

static int GetShadersUsedByDrawable(atArray<const grmShader*>& shaders, const Drawable* drawable     , const char* path, bool bUniqueNamesOnly);
static int GetShadersUsedByFragment(atArray<const grmShader*>& shaders, int             fragmentIndex, const char* path, bool bUniqueNamesOnly);
static int GetShadersUsedByDwd     (atArray<const grmShader*>& shaders, int             dwdIndex     , const char* path, bool bUniqueNamesOnly);

static int GetShadersUsedByDrawable(atArray<const grmShader*>& shaders, const Drawable* drawable, const char*, bool bUniqueNamesOnly)
{
	int count = 0;

	if (drawable)
	{
		const grmShaderGroup* shaderGroup = &drawable->GetShaderGroup();

		for (int i = 0; i < shaderGroup->GetCount(); i++)
		{
			const grmShader* shader = shaderGroup->GetShaderPtr(i);
			bool bUnique = true;

			if (bUniqueNamesOnly)
			{
				for (int j = 0; j < shaders.size(); j++)
				{
					if (stricmp(shaders[j]->GetName(), shader->GetName()) == 0)
					{
						bUnique = false;
						break;
					}
				}
			}

			if (bUnique)
			{
				shaders.PushAndGrow(shader);
			}

			count++;
		}
	}

	return count;
}

static int GetShadersUsedByFragment(atArray<const grmShader*>& shaders, int fragmentIndex, const char* path, bool bUniqueNamesOnly)
{
	const Fragment* fragment = g_FragmentStore.GetSafeFromIndex(strLocalIndex(fragmentIndex));
	int count = 0;

	if (fragment)
	{
		count += GetShadersUsedByDrawable(shaders, fragment->GetCommonDrawable(), atVarString("%s/FRAG:%d(%s)", path, fragmentIndex, g_FragmentStore.GetName(strLocalIndex(fragmentIndex))), bUniqueNamesOnly);
		count += GetShadersUsedByDrawable(shaders, fragment->GetClothDrawable(), atVarString("%s/FRAG:%d(%s)_CLOTH", path, fragmentIndex, g_FragmentStore.GetName(strLocalIndex(fragmentIndex))), bUniqueNamesOnly);

		for (int i = 0; i < fragment->GetNumExtraDrawables(); i++)
		{
			count += GetShadersUsedByDrawable(shaders, fragment->GetExtraDrawable(i), atVarString("%s/FRAG:%d(%s)_EXTRA[%d]", path, fragmentIndex, g_FragmentStore.GetName(strLocalIndex(fragmentIndex)), i), bUniqueNamesOnly);
		}
	}

	return count;
}

static int GetShadersUsedByDwd(atArray<const grmShader*>& shaders, int dwdIndex, const char* path, bool bUniqueNamesOnly)
{
	const Dwd* dwd = g_DwdStore.GetSafeFromIndex(strLocalIndex(dwdIndex));
	int count = 0;

	if (dwd)
	{
		for (int i = 0; i < dwd->GetCount(); i++)
		{
			count += GetShadersUsedByDrawable(shaders, dwd->GetEntry(i), atVarString("%s/DWD:%d(%s)_%d", path, dwdIndex, g_DwdStore.GetName(strLocalIndex(dwdIndex)), i), bUniqueNamesOnly);
		}
	}

	return count;
}

int GetShadersUsedByModel(atArray<const grmShader*>& shaders, const CBaseModelInfo* modelInfo, bool bUniqueNamesOnly)
{
	int count = 0;

	if (modelInfo)
	{
		count += GetShadersUsedByDrawable(shaders, modelInfo->GetDrawable(), "DRAWABLE", bUniqueNamesOnly);

		if (modelInfo->GetModelType() == MI_TYPE_PED)
		{
			const CPedModelInfo* pedModelInfo = (const CPedModelInfo*)modelInfo;

			count += GetShadersUsedByDwd(shaders, pedModelInfo->GetPedComponentFileIndex(), "PED_COMP", bUniqueNamesOnly);
			count += GetShadersUsedByDwd(shaders, pedModelInfo->GetPropsFileIndex       (), "PED_PROP", bUniqueNamesOnly);
		}
		else if (modelInfo->GetModelType() == MI_TYPE_VEHICLE)
		{
			const CVehicleModelInfo* vehModelInfo = (const CVehicleModelInfo*)modelInfo;

			count += GetShadersUsedByFragment(shaders, vehModelInfo->GetHDFragmentIndex(), "VEH_HD", bUniqueNamesOnly);
		}
	}

	return count;
}

int GetShadersUsedByEntity(atArray<const grmShader*>& shaders, const CEntity* entity, bool bUniqueNamesOnly)
{
	const u32 modelIndex = entity->GetModelIndex();
	int count = 0;

	if (CModelInfo::IsValidModelInfo(modelIndex))
	{
		const CBaseModelInfo* modelInfo = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelIndex)));

		if (modelInfo)
		{
			count += GetShadersUsedByModel(shaders, modelInfo, bUniqueNamesOnly);

			if (modelInfo->GetModelType() == MI_TYPE_PED)
			{
				const CPedModelInfo* pedModelInfo = (const CPedModelInfo*)modelInfo;

				if (entity && pedModelInfo->GetIsStreamedGfx())
				{
					const CPedStreamRenderGfx* gfx = ((CPed*)entity)->GetPedDrawHandler().GetPedRenderGfx();

					if (gfx)
					{
						for (int i = 0; i < PV_MAX_COMP; i++)
						{
							count += GetShadersUsedByDwd(shaders, gfx->m_dwdIdx[i], atVarString("PED_GFX[%d]", i), bUniqueNamesOnly);
						}
					}
				}
			}
		}
	}

	return count;
}

// ================================================================================================

#if __DEV

static void PrintTexturesUsedByDrawable(const Drawable* drawable     , const char* path);
static void PrintTexturesUsedByFragment(int             fragmentIndex, const char* path);
static void PrintTexturesUsedByDwd     (int             dwdIndex     , const char* path);

static void PrintTexturesUsedByDrawable(const Drawable* drawable, const char* path)
{
#if EFFECT_PRESERVE_STRINGS
	if (drawable)
	{
		const grmShaderGroup* shaderGroup = &drawable->GetShaderGroup();

		for (int i = 0; i < shaderGroup->GetCount(); i++)
		{
			const grmShader* shader = shaderGroup->GetShaderPtr(i);

			for (int j = 0; j < shader->GetVarCount(); j++)
			{
				const grcEffectVar var = shader->GetVarByIndex(j);
				const char* name = NULL;
				grcEffect::VarType type;
				int annotationCount = 0;
				bool isGlobal = false;

				shader->GetVarDesc(var, name, type, annotationCount, isGlobal);

				if (type == grcEffect::VT_TEXTURE)
				{
					grcTexture* texture = NULL;
					shader->GetVar(var, texture);

					if (texture)
					{
						Displayf("%s -- shader_%d(%s):%s <- %s", path, i, shader->GetName(), name, texture->GetName());
					}
				}
			}
		}
	}
#else // not EFFECT_PRESERVE_STRINGS
	UNUSED_VAR(drawable);
	UNUSED_VAR(path);
#endif // not EFFECT_PRESERVE_STRINGS
}

static void PrintTexturesUsedByFragment(int fragmentIndex, const char* path)
{
	const Fragment* fragment = g_FragmentStore.GetSafeFromIndex(strLocalIndex(fragmentIndex));

	if (fragment)
	{
		PrintTexturesUsedByDrawable(fragment->GetCommonDrawable(), atVarString("%s/FRAG:%d(%s)", path, fragmentIndex, g_FragmentStore.GetName(strLocalIndex(fragmentIndex))));
		PrintTexturesUsedByDrawable(fragment->GetClothDrawable(), atVarString("%s/FRAG:%d(%s)_CLOTH", path, fragmentIndex, g_FragmentStore.GetName(strLocalIndex(fragmentIndex))));

		for (int i = 0; i < fragment->GetNumExtraDrawables(); i++)
		{
			PrintTexturesUsedByDrawable(fragment->GetExtraDrawable(i), atVarString("%s/FRAG:%d(%s)_EXTRA[%d]", path, fragmentIndex, g_FragmentStore.GetName(strLocalIndex(fragmentIndex)), i));
		}
	}
}

static void PrintTexturesUsedByDwd(int dwdIndex, const char* path)
{
	const Dwd* dwd = g_DwdStore.GetSafeFromIndex(strLocalIndex(dwdIndex));

	if (dwd)
	{
		for (int i = 0; i < dwd->GetCount(); i++)
		{
			PrintTexturesUsedByDrawable(dwd->GetEntry(i), atVarString("%s/DWD:%d(%s)_%d", path, dwdIndex, g_DwdStore.GetName(strLocalIndex(dwdIndex)), i));
		}
	}
}

/*
e.g.:

PrintTexturesUsedByEntity(tailgater)
  VEH_HD/FRAG:315(tailgater_hi) -- shader_0(vehicle_paint1):DiffuseTex <- pack:/vehicle_generic_smallspecmap.dds
  VEH_HD/FRAG:315(tailgater_hi) -- shader_0(vehicle_paint1):DirtTex <- pack:/vehicle_genericmud_car.dds
  VEH_HD/FRAG:315(tailgater_hi) -- shader_0(vehicle_paint1):SpecularTex <- pack:/vehicle_generic_alloy_silver_spec.dds
  VEH_HD/FRAG:315(tailgater_hi) -- shader_1(vehicle_mesh):DiffuseTex <- pack:/vehicle_generic_detail2.dds
  VEH_HD/FRAG:315(tailgater_hi) -- shader_1(vehicle_mesh):DirtTex <- pack:/vehicle_genericmud_car.dds
  VEH_HD/FRAG:315(tailgater_hi) -- shader_1(vehicle_mesh):BumpTex <- pack:/vehicle_generic_detail2_normal.dds
  VEH_HD/FRAG:315(tailgater_hi) -- shader_1(vehicle_mesh):SpecularTex <- pack:/vehicle_generic_detail_spec.dds
  VEH_HD/FRAG:315(tailgater_hi) -- shader_2(vehicle_vehglass_inner):DiffuseTex <- pack:/vehicle_generic_glasswindows2.dds
  VEH_HD/FRAG:315(tailgater_hi) -- shader_2(vehicle_vehglass_inner):DirtTex <- pack:/vehicle_generic_glassdirt.dds
  VEH_HD/FRAG:315(tailgater_hi) -- shader_2(vehicle_vehglass_inner):SpecularTex <- pack:/vehicle_generic_smallspecmap.dds
  VEH_HD/FRAG:315(tailgater_hi) -- shader_2(vehicle_vehglass_inner):EnvironmentTex <- REFLECTION_MAP_COLOUR
  VEH_HD/FRAG:315(tailgater_hi) -- shader_3(vehicle_tire):DiffuseTex <- pack:/vehicle_generic_tyrewallblack.dds
  VEH_HD/FRAG:315(tailgater_hi) -- shader_3(vehicle_tire):DirtTex <- pack:/vehicle_generic_tyrewall_dirt.dds
  VEH_HD/FRAG:315(tailgater_hi) -- shader_3(vehicle_tire):BumpTex <- pack:/vehicle_generic_tyrewall_normal.dds
  VEH_HD/FRAG:315(tailgater_hi) -- shader_3(vehicle_tire):SpecularTex <- pack:/vehicle_generic_tyrewall_spec.dds
  VEH_HD/FRAG:315(tailgater_hi) -- shader_4(vehicle_interior2):DiffuseTex <- pack:/schafter_interior.dds
  VEH_HD/FRAG:315(tailgater_hi) -- shader_4(vehicle_interior2):SpecularTex <- pack:/vehicle_generic_smallspecmap.dds
  VEH_HD/FRAG:315(tailgater_hi) -- shader_5(vehicle_shuts):DiffuseTex <- pack:/vehicle_generic_doorshut.dds
  VEH_HD/FRAG:315(tailgater_hi) -- shader_5(vehicle_shuts):BumpTex <- pack:/vehicle_generic_doorshut_normal.dds
  VEH_HD/FRAG:315(tailgater_hi) -- shader_5(vehicle_shuts):SpecularTex <- pack:/vehicle_generic_smallspecmap.dds
  VEH_HD/FRAG:315(tailgater_hi) -- shader_6(vehicle_lightsemissive):DiffuseTex <- pack:/tailgater_lights_glass.dds
  VEH_HD/FRAG:315(tailgater_hi) -- shader_6(vehicle_lightsemissive):DirtTex <- pack:/vehicle_genericmud_car.dds
  VEH_HD/FRAG:315(tailgater_hi) -- shader_6(vehicle_lightsemissive):SpecularTex <- pack:/vehicle_generic_smallspecmap.dds
  VEH_HD/FRAG:315(tailgater_hi) -- shader_6(vehicle_lightsemissive):EnvironmentTex <- REFLECTION_MAP_COLOUR
  VEH_HD/FRAG:315(tailgater_hi) -- shader_7(vehicle_vehglass):DiffuseTex <- pack:/vehicle_generic_glasswindows2.dds
  VEH_HD/FRAG:315(tailgater_hi) -- shader_7(vehicle_vehglass):DirtTex <- pack:/vehicle_generic_glassdirt.dds
  VEH_HD/FRAG:315(tailgater_hi) -- shader_7(vehicle_vehglass):SpecularTex <- pack:/vehicle_generic_smallspecmap.dds
  VEH_HD/FRAG:315(tailgater_hi) -- shader_7(vehicle_vehglass):EnvironmentTex <- REFLECTION_MAP_COLOUR
  VEH_HD/FRAG:315(tailgater_hi) -- shader_8(vehicle_licenseplate):PlateBgTex <- pack:/plate04.dds
  VEH_HD/FRAG:315(tailgater_hi) -- shader_8(vehicle_licenseplate):PlateBgBumpTex <- pack:/plate04_n.dds
  VEH_HD/FRAG:315(tailgater_hi) -- shader_8(vehicle_licenseplate):FontTexture <- pack:/vehicle_generic_plate_font.dds
  VEH_HD/FRAG:315(tailgater_hi) -- shader_8(vehicle_licenseplate):FontNormalMap <- pack:/vehicle_generic_plate_font_n.dds
  DRAWABLE -- shader_0(vehicle_tire):DiffuseTex <- pack:/vehicle_generic_tyrewallblack.dds
  DRAWABLE -- shader_0(vehicle_tire):DirtTex <- pack:/vehicle_generic_tyrewall_dirt.dds
  DRAWABLE -- shader_0(vehicle_tire):BumpTex <- pack:/vehicle_generic_tyrewall_normal.dds
  DRAWABLE -- shader_0(vehicle_tire):SpecularTex <- pack:/vehicle_generic_tyrewall_spec.dds
  DRAWABLE -- shader_1(vehicle_paint1):DiffuseTex <- pack:/vehicle_generic_smallspecmap.dds
  DRAWABLE -- shader_1(vehicle_paint1):DirtTex <- pack:/vehicle_genericmud_car.dds
  DRAWABLE -- shader_1(vehicle_paint1):SpecularTex <- pack:/vehicle_generic_alloy_silver_spec.dds
  DRAWABLE -- shader_2(vehicle_mesh):DiffuseTex <- pack:/vehicle_generic_detail2.dds
  DRAWABLE -- shader_2(vehicle_mesh):DirtTex <- pack:/vehicle_genericmud_car.dds
  DRAWABLE -- shader_2(vehicle_mesh):BumpTex <- pack:/vehicle_generic_detail2_normal.dds
  DRAWABLE -- shader_2(vehicle_mesh):SpecularTex <- pack:/vehicle_generic_detail_spec.dds
  DRAWABLE -- shader_3(vehicle_vehglass_inner):DiffuseTex <- pack:/vehicle_generic_glasswindows2.dds
  DRAWABLE -- shader_3(vehicle_vehglass_inner):DirtTex <- pack:/vehicle_generic_glassdirt.dds
  DRAWABLE -- shader_3(vehicle_vehglass_inner):SpecularTex <- pack:/vehicle_generic_smallspecmap.dds
  DRAWABLE -- shader_3(vehicle_vehglass_inner):EnvironmentTex <- REFLECTION_MAP_COLOUR
  DRAWABLE -- shader_4(vehicle_interior2):DiffuseTex <- pack:/schafter_interior.dds
  DRAWABLE -- shader_4(vehicle_interior2):SpecularTex <- pack:/vehicle_generic_smallspecmap.dds
  DRAWABLE -- shader_5(vehicle_shuts):DiffuseTex <- pack:/vehicle_generic_doorshut.dds
  DRAWABLE -- shader_5(vehicle_shuts):BumpTex <- pack:/vehicle_generic_doorshut_normal.dds
  DRAWABLE -- shader_5(vehicle_shuts):SpecularTex <- pack:/vehicle_generic_smallspecmap.dds
  DRAWABLE -- shader_6(vehicle_lightsemissive):DiffuseTex <- pack:/tailgater_lights_glass.dds
  DRAWABLE -- shader_6(vehicle_lightsemissive):DirtTex <- pack:/vehicle_genericmud_car.dds
  DRAWABLE -- shader_6(vehicle_lightsemissive):SpecularTex <- pack:/vehicle_generic_smallspecmap.dds
  DRAWABLE -- shader_6(vehicle_lightsemissive):EnvironmentTex <- REFLECTION_MAP_COLOUR
  DRAWABLE -- shader_7(vehicle_vehglass):DiffuseTex <- pack:/vehicle_generic_glasswindows2.dds
  DRAWABLE -- shader_7(vehicle_vehglass):DirtTex <- pack:/vehicle_generic_glassdirt.dds
  DRAWABLE -- shader_7(vehicle_vehglass):SpecularTex <- pack:/vehicle_generic_smallspecmap.dds
  DRAWABLE -- shader_7(vehicle_vehglass):EnvironmentTex <- REFLECTION_MAP_COLOUR

PrintTexturesUsedByEntity(player_zero)
  PED_GFX[0]/DWD:101(player_zero/head_000_r)_0 -- shader_0(ped):VolumeTex <- common:/data/ped_detail_atlas
  PED_GFX[0]/DWD:101(player_zero/head_000_r)_0 -- shader_0(ped):BumpTex <- head_normal_000
  PED_GFX[0]/DWD:101(player_zero/head_000_r)_0 -- shader_0(ped):SpecularTex <- head_spec_000
  PED_GFX[0]/DWD:101(player_zero/head_000_r)_0 -- shader_0(ped):pedBloodTarget <- image
  PED_GFX[0]/DWD:101(player_zero/head_000_r)_0 -- shader_0(ped):pedTattooTarget <- image
  PED_GFX[2]/DWD:142(player_zero/hair_000_u)_0 -- shader_0(ped):VolumeTex <- common:/data/ped_detail_atlas
  PED_GFX[2]/DWD:142(player_zero/hair_000_u)_0 -- shader_0(ped):pedBloodTarget <- image
  PED_GFX[2]/DWD:142(player_zero/hair_000_u)_0 -- shader_0(ped):pedTattooTarget <- image
  PED_GFX[3]/DWD:106(player_zero/uppr_000_u)_0 -- shader_0(ped_wrinkle):VolumeTex <- common:/data/ped_detail_atlas
  PED_GFX[3]/DWD:106(player_zero/uppr_000_u)_0 -- shader_0(ped_wrinkle):BumpTex <- uppr_normal_000
  PED_GFX[3]/DWD:106(player_zero/uppr_000_u)_0 -- shader_0(ped_wrinkle):SpecularTex <- uppr_spec_000
  PED_GFX[3]/DWD:106(player_zero/uppr_000_u)_0 -- shader_0(ped_wrinkle):WrinkleMaskTexture_0 <- uppr_normal_000_ma1
  PED_GFX[3]/DWD:106(player_zero/uppr_000_u)_0 -- shader_0(ped_wrinkle):WrinkleMaskTexture_1 <- uppr_normal_000_ma2
  PED_GFX[3]/DWD:106(player_zero/uppr_000_u)_0 -- shader_0(ped_wrinkle):WrinkleMaskTexture_2 <- uppr_normal_000_mb1
  PED_GFX[3]/DWD:106(player_zero/uppr_000_u)_0 -- shader_0(ped_wrinkle):WrinkleMaskTexture_3 <- uppr_normal_000_mb2
  PED_GFX[3]/DWD:106(player_zero/uppr_000_u)_0 -- shader_0(ped_wrinkle):WrinkleTexture_A <- uppr_normal_000_wa
  PED_GFX[3]/DWD:106(player_zero/uppr_000_u)_0 -- shader_0(ped_wrinkle):WrinkleTexture_B <- uppr_normal_000_wb
  PED_GFX[3]/DWD:106(player_zero/uppr_000_u)_0 -- shader_0(ped_wrinkle):pedBloodTarget <- image
  PED_GFX[3]/DWD:106(player_zero/uppr_000_u)_0 -- shader_0(ped_wrinkle):pedTattooTarget <- image
  PED_GFX[4]/DWD:113(player_zero/lowr_000_u)_0 -- shader_0(ped_wrinkle):DiffuseTex <- pack:/lowr_diff_000_a_uni.dds
  PED_GFX[4]/DWD:113(player_zero/lowr_000_u)_0 -- shader_0(ped_wrinkle):VolumeTex <- common:/data/ped_detail_atlas
  PED_GFX[4]/DWD:113(player_zero/lowr_000_u)_0 -- shader_0(ped_wrinkle):BumpTex <- lowr_normal_000
  PED_GFX[4]/DWD:113(player_zero/lowr_000_u)_0 -- shader_0(ped_wrinkle):SpecularTex <- lowr_spec_000
  PED_GFX[4]/DWD:113(player_zero/lowr_000_u)_0 -- shader_0(ped_wrinkle):WrinkleMaskTexture_0 <- lowr_normal_000_ma1
  PED_GFX[4]/DWD:113(player_zero/lowr_000_u)_0 -- shader_0(ped_wrinkle):WrinkleMaskTexture_1 <- lowr_normal_000_ma2
  PED_GFX[4]/DWD:113(player_zero/lowr_000_u)_0 -- shader_0(ped_wrinkle):WrinkleMaskTexture_2 <- lowr_normal_000_mb1
  PED_GFX[4]/DWD:113(player_zero/lowr_000_u)_0 -- shader_0(ped_wrinkle):WrinkleMaskTexture_3 <- lowr_normal_000_mb2
  PED_GFX[4]/DWD:113(player_zero/lowr_000_u)_0 -- shader_0(ped_wrinkle):WrinkleTexture_A <- lowr_normal_000_wa
  PED_GFX[4]/DWD:113(player_zero/lowr_000_u)_0 -- shader_0(ped_wrinkle):WrinkleTexture_B <- lowr_normal_000_wb
  PED_GFX[4]/DWD:113(player_zero/lowr_000_u)_0 -- shader_0(ped_wrinkle):pedBloodTarget <- image
  PED_GFX[4]/DWD:113(player_zero/lowr_000_u)_0 -- shader_0(ped_wrinkle):pedTattooTarget <- image
  PED_GFX[5]/DWD:140(player_zero/hand_000_r)_0 -- shader_0(ped):DiffuseTex <- pack:/hand_diff_000_a_whi.dds
  PED_GFX[5]/DWD:140(player_zero/hand_000_r)_0 -- shader_0(ped):VolumeTex <- common:/data/ped_detail_atlas
  PED_GFX[5]/DWD:140(player_zero/hand_000_r)_0 -- shader_0(ped):BumpTex <- hand_normal_000
  PED_GFX[5]/DWD:140(player_zero/hand_000_r)_0 -- shader_0(ped):SpecularTex <- hand_spec_000
  PED_GFX[5]/DWD:140(player_zero/hand_000_r)_0 -- shader_0(ped):pedBloodTarget <- image
  PED_GFX[5]/DWD:140(player_zero/hand_000_r)_0 -- shader_0(ped):pedTattooTarget <- image
  PED_GFX[6]/DWD:126(player_zero/feet_000_u)_0 -- shader_0(ped):VolumeTex <- common:/data/ped_detail_atlas
  PED_GFX[6]/DWD:126(player_zero/feet_000_u)_0 -- shader_0(ped):BumpTex <- feet_normal_000
  PED_GFX[6]/DWD:126(player_zero/feet_000_u)_0 -- shader_0(ped):SpecularTex <- feet_spec_000
  PED_GFX[6]/DWD:126(player_zero/feet_000_u)_0 -- shader_0(ped):pedBloodTarget <- image
  PED_GFX[6]/DWD:126(player_zero/feet_000_u)_0 -- shader_0(ped):pedTattooTarget <- image
  PED_GFX[8]/DWD:90(player_zero/accs_000_u)_0 -- shader_0(ped):VolumeTex <- common:/data/ped_detail_atlas
  PED_GFX[8]/DWD:90(player_zero/accs_000_u)_0 -- shader_0(ped):BumpTex <- accs_normal_000
  PED_GFX[8]/DWD:90(player_zero/accs_000_u)_0 -- shader_0(ped):SpecularTex <- accs_spec_000
  PED_GFX[8]/DWD:90(player_zero/accs_000_u)_0 -- shader_0(ped):pedBloodTarget <- image
  PED_GFX[8]/DWD:90(player_zero/accs_000_u)_0 -- shader_0(ped):pedTattooTarget <- image
  PED_GFX[9]/DWD:124(player_zero/task_000_u)_0 -- shader_0(ped):VolumeTex <- common:/data/ped_detail_atlas
  PED_GFX[9]/DWD:124(player_zero/task_000_u)_0 -- shader_0(ped):pedBloodTarget <- image
  PED_GFX[9]/DWD:124(player_zero/task_000_u)_0 -- shader_0(ped):pedTattooTarget <- image
  PED_GFX[10]/DWD:125(player_zero/decl_000_u)_0 -- shader_0(ped):DiffuseTex <- pack:/decl_diff_000_a_uni.dds
  PED_GFX[10]/DWD:125(player_zero/decl_000_u)_0 -- shader_0(ped):VolumeTex <- common:/data/ped_detail_atlas
  PED_GFX[10]/DWD:125(player_zero/decl_000_u)_0 -- shader_0(ped):pedBloodTarget <- image
  PED_GFX[10]/DWD:125(player_zero/decl_000_u)_0 -- shader_0(ped):pedTattooTarget <- image
  PED_PROP/DWD:463(Player_Zero_p)_0 -- shader_0(ped):DiffuseTex <- pack:/p_head_diff_003_a.dds
  PED_PROP/DWD:463(Player_Zero_p)_0 -- shader_0(ped):VolumeTex <- common:/data/ped_detail_atlas
  PED_PROP/DWD:463(Player_Zero_p)_0 -- shader_0(ped):BumpTex <- pack:/p_head_normal_003.dds
  PED_PROP/DWD:463(Player_Zero_p)_0 -- shader_0(ped):SpecularTex <- pack:/p_head_spec_003.dds
  PED_PROP/DWD:463(Player_Zero_p)_1 -- shader_0(ped):DiffuseTex <- pack:/p_head_diff_002_a.dds
  PED_PROP/DWD:463(Player_Zero_p)_1 -- shader_0(ped):VolumeTex <- common:/data/ped_detail_atlas
  PED_PROP/DWD:463(Player_Zero_p)_1 -- shader_0(ped):BumpTex <- pack:/p_head_normal_002.dds
  PED_PROP/DWD:463(Player_Zero_p)_1 -- shader_0(ped):SpecularTex <- pack:/p_head_spec_002.dds
  PED_PROP/DWD:463(Player_Zero_p)_2 -- shader_0(ped):DiffuseTex <- pack:/p_head_diff_001_a.dds
  PED_PROP/DWD:463(Player_Zero_p)_2 -- shader_0(ped):VolumeTex <- common:/data/ped_detail_atlas
  PED_PROP/DWD:463(Player_Zero_p)_2 -- shader_0(ped):BumpTex <- pack:/p_head_normal_001.dds
  PED_PROP/DWD:463(Player_Zero_p)_2 -- shader_0(ped):SpecularTex <- pack:/p_head_spec_001.dds
  PED_PROP/DWD:463(Player_Zero_p)_3 -- shader_0(ped):DiffuseTex <- pack:/p_eyes_diff_002_a.dds
  PED_PROP/DWD:463(Player_Zero_p)_3 -- shader_0(ped):VolumeTex <- common:/data/ped_detail_atlas
  PED_PROP/DWD:463(Player_Zero_p)_3 -- shader_0(ped):BumpTex <- pack:/p_eyes_normal_002.dds
  PED_PROP/DWD:463(Player_Zero_p)_3 -- shader_0(ped):SpecularTex <- pack:/p_eyes_spec_002.dds
  PED_PROP/DWD:463(Player_Zero_p)_3 -- shader_1(ped):DiffuseTex <- pack:/p_eyes_diff_002_a.dds
  PED_PROP/DWD:463(Player_Zero_p)_3 -- shader_1(ped):VolumeTex <- common:/data/ped_detail_atlas
  PED_PROP/DWD:463(Player_Zero_p)_3 -- shader_1(ped):BumpTex <- pack:/p_eyes_normal_002.dds
  PED_PROP/DWD:463(Player_Zero_p)_3 -- shader_1(ped):SpecularTex <- pack:/p_eyes_spec_002.dds
  PED_PROP/DWD:463(Player_Zero_p)_4 -- shader_0(ped):DiffuseTex <- pack:/p_eyes_diff_000_a.dds
  PED_PROP/DWD:463(Player_Zero_p)_4 -- shader_0(ped):VolumeTex <- common:/data/ped_detail_atlas
  PED_PROP/DWD:463(Player_Zero_p)_4 -- shader_0(ped):BumpTex <- pack:/p_eyes_normal_000.dds
  PED_PROP/DWD:463(Player_Zero_p)_4 -- shader_0(ped):SpecularTex <- pack:/p_eyes_spec_000.dds
  PED_PROP/DWD:463(Player_Zero_p)_4 -- shader_1(ped):DiffuseTex <- pack:/p_eyes_diff_000_a.dds
  PED_PROP/DWD:463(Player_Zero_p)_4 -- shader_1(ped):VolumeTex <- common:/data/ped_detail_atlas
  PED_PROP/DWD:463(Player_Zero_p)_4 -- shader_1(ped):BumpTex <- pack:/p_eyes_normal_000.dds
  PED_PROP/DWD:463(Player_Zero_p)_4 -- shader_1(ped):SpecularTex <- pack:/p_eyes_spec_000.dds
  PED_PROP/DWD:463(Player_Zero_p)_5 -- shader_0(ped_enveff):DiffuseTex <- pack:/p_head_diff_000_a.dds
  PED_PROP/DWD:463(Player_Zero_p)_5 -- shader_0(ped_enveff):VolumeTex <- common:/data/ped_detail_atlas
  PED_PROP/DWD:463(Player_Zero_p)_5 -- shader_0(ped_enveff):BumpTex <- pack:/p_head_normal_000.dds
  PED_PROP/DWD:463(Player_Zero_p)_5 -- shader_0(ped_enveff):SpecularTex <- pack:/p_head_spec_000.dds
  PED_PROP/DWD:463(Player_Zero_p)_5 -- shader_0(ped_enveff):SnowTex <- pack:/soot_uppr_enveff.dds
  PED_PROP/DWD:463(Player_Zero_p)_6 -- shader_0(ped):DiffuseTex <- pack:/p_eyes_diff_001_a.dds
  PED_PROP/DWD:463(Player_Zero_p)_6 -- shader_0(ped):VolumeTex <- common:/data/ped_detail_atlas
  PED_PROP/DWD:463(Player_Zero_p)_6 -- shader_0(ped):BumpTex <- pack:/p_eyes_normal_001.dds
  PED_PROP/DWD:463(Player_Zero_p)_6 -- shader_0(ped):SpecularTex <- pack:/p_eyes_spec_001.dds
  PED_PROP/DWD:463(Player_Zero_p)_6 -- shader_1(ped):DiffuseTex <- pack:/p_eyes_diff_001_a.dds
  PED_PROP/DWD:463(Player_Zero_p)_6 -- shader_1(ped):VolumeTex <- common:/data/ped_detail_atlas
  PED_PROP/DWD:463(Player_Zero_p)_6 -- shader_1(ped):BumpTex <- pack:/p_eyes_normal_001.dds
  PED_PROP/DWD:463(Player_Zero_p)_6 -- shader_1(ped):SpecularTex <- pack:/p_eyes_spec_001.dds
  PED_PROP/DWD:463(Player_Zero_p)_7 -- shader_0(ped):DiffuseTex <- pack:/p_head_diff_004_a.dds
  PED_PROP/DWD:463(Player_Zero_p)_7 -- shader_0(ped):VolumeTex <- common:/data/ped_detail_atlas
  PED_PROP/DWD:463(Player_Zero_p)_7 -- shader_0(ped):BumpTex <- pack:/p_head_normal_004.dds
  PED_PROP/DWD:463(Player_Zero_p)_7 -- shader_0(ped):SpecularTex <- pack:/p_head_spec_004.dds
  DRAWABLE -- shader_0(ped):VolumeTex <- common:/data/ped_detail_atlas
*/

void PrintTexturesUsedByModel(const CBaseModelInfo* modelInfo)
{
	if (modelInfo->GetModelType() == MI_TYPE_PED)
	{
		const CPedModelInfo* pedModelInfo = (const CPedModelInfo*)modelInfo;

		PrintTexturesUsedByDwd(pedModelInfo->GetPedComponentFileIndex(), "  PED_COMP");
		PrintTexturesUsedByDwd(pedModelInfo->GetPropsFileIndex       (), "  PED_PROP");
	}
	else if (modelInfo->GetModelType() == MI_TYPE_VEHICLE)
	{
		const CVehicleModelInfo* vehModelInfo = (const CVehicleModelInfo*)modelInfo;

		PrintTexturesUsedByFragment(vehModelInfo->GetHDFragmentIndex(), "  VEH_HD");
	}

	PrintTexturesUsedByDrawable(modelInfo->GetDrawable(), "  DRAWABLE");
}

void PrintTexturesUsedByEntity(const CEntity* entity)
{
	const u32 modelIndex = entity->GetModelIndex();

	if (CModelInfo::IsValidModelInfo(modelIndex))
	{
		const CBaseModelInfo* modelInfo = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelIndex)));

		if (modelInfo)
		{
			PrintTexturesUsedByModel(modelInfo);

			if (modelInfo->GetModelType() == MI_TYPE_PED)
			{
				const CPedModelInfo* pedModelInfo = (const CPedModelInfo*)modelInfo;

				if (entity && pedModelInfo->GetIsStreamedGfx())
				{
					const CPedStreamRenderGfx* gfx = ((CPed*)entity)->GetPedDrawHandler().GetPedRenderGfx();

					if (gfx)
					{
						for (int i = 0; i < PV_MAX_COMP; i++)
						{
							PrintTexturesUsedByDwd(gfx->m_dwdIdx[i], atVarString("  PED_GFX[%d]", i));
						}
					}
				}
			}
		}
	}
}

#endif // __DEV

#if DEBUG_TEXTURE_VIEWER_MESH_DUMP_TEST

namespace rage { extern u32 g_AllowVertexBufferVramLocks; }

void PrintDrawableMeshData(const Drawable* drawable, Mat34V_In mat)
{
	const rmcLodGroup& lodGroup = drawable->GetLodGroup();

	Vec3V bmin = Vec3V(V_FLT_MAX);
	Vec3V bmax = Vec3V(V_NEG_FLT_MAX);

	const u32 bAllowVBLocks = g_AllowVertexBufferVramLocks; // save
	g_AllowVertexBufferVramLocks = 1;

	for (int lodIndex = 0; lodIndex < LOD_COUNT; lodIndex++)
	{
		if (lodGroup.ContainsLod(lodIndex))
		{
			const rmcLod& lod = lodGroup.GetLod(lodIndex);
			Displayf("  lodIndex=%d", lodIndex);

			for (int lodModelIndex = 0; lodModelIndex < lod.GetCount(); lodModelIndex++)
			{
				const grmModel* pModel = lod.GetModel(lodModelIndex);

				if (pModel)
				{
					const grmModel& model = *pModel;
					Displayf("    lodModelIndex=%d", lodModelIndex);

					for (int geomIndex = 0; geomIndex < model.GetGeometryCount(); geomIndex++)
					{
						grmGeometry& geom = model.GetGeometry(geomIndex);

						if (geom.GetType() == grmGeometry::GEOMETRYQB)
						{
							Displayf("      geomIndex=%d (GEOMETRYQB)", geomIndex);

							grcVertexBuffer* vb = geom.GetVertexBuffer();
							grcIndexBuffer*  ib = geom.GetIndexBuffer();

							const grcFvf* fvf = vb->GetFvf();

							// dump vertex formats
							{
								const char* grcfcStrings[] =
								{
									"grcfcPosition ",
									"grcfcWeight   ",
									"grcfcBinding  ",
									"grcfcNormal   ",
									"grcfcDiffuse  ",
									"grcfcSpecular ",
									"grcfcTexture0 ",
									"grcfcTexture1 ",
									"grcfcTexture2 ",
									"grcfcTexture3 ",
									"grcfcTexture4 ",
									"grcfcTexture5 ",
									"grcfcTexture6 ",
									"grcfcTexture7 ",
									"grcfcTangent0 ",
									"grcfcTangent1 ",
									"grcfcBinormal0",
									"grcfcBinormal1",
								};
								CompileTimeAssert(NELEM(grcfcStrings) == grcFvf::grcfcCount);

								const char* grcdsStrings[] =
								{
									"grcdsHalf",
									"grcdsHalf2",
									"grcdsHalf3",
									"grcdsHalf4",

									"grcdsFloat",
									"grcdsFloat2",
									"grcdsFloat3",
									"grcdsFloat4",

									"grcdsUBYTE4",
									"grcdsColor",
									"grcdsPackedNormal",

									"grcdsEDGE0",
									"grcdsEDGE1",
									"grcdsEDGE2",

									"grcdsShort2",
									"grcdsShort4",
								};
								CompileTimeAssert(NELEM(grcdsStrings) == grcFvf::grcdsCount);

								Displayf("        VERTEX BUFFER  - stride=%d, count=%d (%.2fkb)", vb->GetVertexStride(), vb->GetVertexCount(), (float)(vb->GetVertexStride()*vb->GetVertexCount())/1024.0f);

								for (int i = 0; i < grcFvf::grcfcCount; i++)
								{
									const grcFvf::grcFvfChannels channel = (grcFvf::grcFvfChannels)i;

									if (fvf->IsChannelActive(channel))
									{
										const grcFvf::grcDataSize format = fvf->GetDataSizeType(channel);
										Assert((int)format >= 0 && format < grcFvf::grcdsCount);

										Displayf("        %s - size=%d, offset=%d, format=%s", grcfcStrings[channel], fvf->GetSize(channel), fvf->GetOffset(channel), grcdsStrings[format]);
									}
								}
							}

							 // dump vertex buffer
							if (fvf->IsChannelActive(grcFvf::grcfcPosition) &&
								fvf->IsChannelActive(grcFvf::grcfcNormal) &&
								fvf->IsChannelActive(grcFvf::grcfcTexture0) &&
								fvf->IsChannelActive(grcFvf::grcfcDiffuse))
							{
								grcVertexBufferEditor vbe(vb, true, true); // lock=true, readOnly=true

								for (int i = 0; i < vb->GetVertexCount(); i++)
								{
									const Vec3V   pos = Transform(mat, VECTOR3_TO_VEC3V(vbe.GetPosition(i)));
									const Vec3V   nrm = VECTOR3_TO_VEC3V(vbe.GetNormal(i));
									const Vec2V   tex = Vec2V(vbe.GetUV(i, 0).x, vbe.GetUV(i, 0).y);
									const Color32 col = vbe.GetDiffuse(i);

									Displayf(
										"        VERTEX %d: pos=%f,%f,%f, nrm=%f,%f,%f, tex=%f,%f, col=%d,%d,%d,%d",
										i,
										VEC3V_ARGS(pos),
										VEC3V_ARGS(nrm),
										tex.GetXf(),
										1.0f - tex.GetYf(), // grcVertexBufferEditor::GetUV inverts texcoord.y, so let's display the actual value
										col.GetRed(),
										col.GetGreen(),
										col.GetBlue(),
										col.GetAlpha()
									);

									bmin = Min(pos, bmin);
									bmax = Max(pos, bmax);
								}
							}
							else if (fvf->IsChannelActive(grcFvf::grcfcPosition)) // .. at least dump the position channel
							{
								grcVertexBufferEditor vbe(vb, true, true); // lock=true, readOnly=true

								for (int i = 0; i < vb->GetVertexCount(); i++)
								{
									const Vec3V pos = Transform(mat, VECTOR3_TO_VEC3V(vbe.GetPosition(i)));

									Displayf("        VERTEX %d: pos=%f,%f,%f", i, VEC3V_ARGS(pos));

									bmin = Min(pos, bmin);
									bmax = Max(pos, bmax);
								}
							}

							// dump index buffer
							{
								const u16* indices = ib->LockRO();
								const int numIndices = ib->GetIndexCount();

								if (geom.GetPrimitiveType() == drawTris)
								{
									for (int i = 0; i < numIndices; i += 3)
									{
										bool bDegenerate = false;

										if (indices[i + 0] == indices[i + 1]) { bDegenerate = true; }
										if (indices[i + 1] == indices[i + 2]) { bDegenerate = true; }
										if (indices[i + 2] == indices[i + 0]) { bDegenerate = true; }

										Displayf(
											"        TRIANGLE %d: indices=%d,%d,%d%s",
											i/3,
											indices[i + 0],
											indices[i + 1],
											indices[i + 2],
											bDegenerate ? " (DEGENERATE)" : ""
										);
									}
								}
								else if (geom.GetPrimitiveType() == drawTriStrip)
								{
									for (int i = 0; i < numIndices - 2; i++)
									{
										bool bDegenerate = false;

										if (indices[i + 0] == indices[i + 1]) { bDegenerate = true; }
										if (indices[i + 1] == indices[i + 2]) { bDegenerate = true; }
										if (indices[i + 2] == indices[i + 0]) { bDegenerate = true; }

										Displayf(
											"        TRIANGLESTRIP %d: indices=%d,%d,%d%s",
											i,
											indices[i + 0],
											indices[i + 1 + (i&1)], // flip
											indices[i + 2 - (i&1)], // flip
											bDegenerate ? " (DEGENERATE)" : ""
										);
									}
								}
							}
						}
						else if (geom.GetType() == grmGeometry::GEOMETRYEDGE)
						{
							Displayf("      geomIndex=%d (GEOMETRYEDGE) - can't get vertex data", geomIndex);
						}
						else
						{
							Displayf("      geomIndex=%d (UNKNOWN TYPE %d) - can't get vertex data", geomIndex, geom.GetType());
						}
					}
				}
			}
		}
	}

	g_AllowVertexBufferVramLocks = bAllowVBLocks; // restore

	if (IsLessThanOrEqualAll(bmin, bmax) != 0)
	{
		Displayf("bmin=%f,%f,%f", VEC3V_ARGS(bmin));
		Displayf("bmax=%f,%f,%f", VEC3V_ARGS(bmax));
	}
	else
	{
		Displayf("no geometry found.");
	}
}

void PrintModelMeshData(const CBaseModelInfo* modelInfo)
{
	const Drawable* drawable = modelInfo->GetDrawable();

	if (drawable)
	{
		PrintDrawableMeshData(drawable, Mat34V(V_IDENTITY));
	}
}

void PrintEntityMeshData(const CEntity* entity)
{
	const Drawable* drawable = entity->GetDrawable();

	if (drawable)
	{
		PrintDrawableMeshData(drawable, entity->GetMatrix());
	}
}

#endif // DEBUG_TEXTURE_VIEWER_MESH_DUMP_TEST

bool IsTextureConstant(const grcTexture* texture)
{
	const grcImage::Format format = (grcImage::Format)texture->GetImageFormat();
	int bitsPerPixel = grcImage::GetFormatBitsPerPixel(format);

	grcTextureLock lock;
	texture->LockRect(0, 0, lock, grcsRead | grcsAllowVRAMLock);

	const void* end = (const u8*)lock.Base + (texture->GetWidth()*texture->GetHeight()*bitsPerPixel)/8;

	if (grcImage::IsFormatDXTBlockCompressed(format))
	{
		bitsPerPixel *= (4*4); // bits per block
	}

	bool bIsConst = true;

	switch (bitsPerPixel)
	{
	case   8: { const  u8* p = (const  u8*)lock.Base; const  u8 p0 = *(p++); while ((void*)p < end) { if (*(p++) != p0) { bIsConst = false; break; } } } break;
	case  16: { const u16* p = (const u16*)lock.Base; const u16 p0 = *(p++); while ((void*)p < end) { if (*(p++) != p0) { bIsConst = false; break; } } } break;
	case  32: { const u32* p = (const u32*)lock.Base; const u32 p0 = *(p++); while ((void*)p < end) { if (*(p++) != p0) { bIsConst = false; break; } } } break;
	case  64: { const u64* p = (const u64*)lock.Base; const u64 p0 = *(p++); while ((void*)p < end) { if (*(p++) != p0) { bIsConst = false; break; } } } break;
	case 128:
		{
			const u64* p = (const u64*)lock.Base;
			const u64 p0 = *(p++);
			const u64 p1 = *(p++);
			while ((void*)p < end)
			{
				if (*(p++) != p0) { bIsConst = false; break; }
				if (*(p++) != p1) { bIsConst = false; break; }
			}
			break;
		}
	default: bIsConst = false;
	}

	texture->UnlockRect(lock);

	return bIsConst;
}

static void InvertTextureChannels_DXT1_BLOCK(DXT::DXT1_BLOCK& block, u16 rgbMask)
{
	if (rgbMask == 0xffff)
	{
		block.InvertRGB();
	}
	else if (rgbMask != 0)
	{
		PPU_ONLY(block.c[0].ByteSwap());
		PPU_ONLY(block.c[1].ByteSwap());

		if (block.c[0].rgb > block.c[1].rgb) // no alpha
		{
			block.c[0].rgb ^= rgbMask;
			block.c[1].rgb ^= rgbMask;

			if (block.c[0].rgb < block.c[1].rgb) // changed order: index 3 = alpha 0
			{
				const DXT::RGB565 tmp = block.c[0];
				block.c[0] = block.c[1];
				block.c[1] = tmp;
				block.InvertIndices(); // {0,1,2,3} -> {1,0,3,2}
			}
		}
		else // index 3 = alpha 0
		{
			block.c[0].rgb ^= rgbMask;
			block.c[1].rgb ^= rgbMask;

			if (block.c[0].rgb > block.c[1].rgb) // changed order: no alpha
			{
				const DXT::RGB565 tmp = block.c[0];
				block.c[0] = block.c[1];
				block.c[1] = tmp;
				block.InvertIndices(); // {0,1,2,3} -> {1,0,2,3}
			}
		}

		PPU_ONLY(block.c[0].ByteSwap());
		PPU_ONLY(block.c[1].ByteSwap());
	}
}

static void InvertTextureChannels_DXT3_ALPHA(DXT::DXT3_ALPHA& block, u8 alphaMask)
{
	if (alphaMask != 0)
	{
		alphaMask = (alphaMask >> 4)|(alphaMask << 4); // we only have 4-bit alpha, but we can invert two-at-a-time

		for (int i = 0; i < 8; i++)
		{
			block.i[i] ^= alphaMask;
		}
	}
}

static void InvertTextureChannels_DXT5_ALPHA(DXT::DXT5_ALPHA& block, u8 alphaMask)
{
	if (alphaMask != 0)
	{
		if (block.a[0] > block.a[1])
		{
			block.a[0] ^= alphaMask;
			block.a[1] ^= alphaMask;

			if (block.a[0] < block.a[1]) // changed order
			{
				const u8 tmp = block.a[0];
				block.a[0] = block.a[1];
				block.a[1] = tmp;	

				// swap {0,1}, reverse {2,3,4,5,6,7}
				{
					int indices[4*4];
					block.GetIndices(indices);

					for (int i = 0; i < 4*4; i++)
					{
						const int remap[] = {1,0,7,6,5,4,3,2};
						indices[i] = remap[indices[i]];
					}

					block.SetIndices(indices);
				}
			}
		}
		else
		{
			block.a[0] ^= alphaMask;
			block.a[1] ^= alphaMask;

			if (block.a[0] > block.a[1]) // changed order
			{
				const u8 tmp = block.a[0];
				block.a[0] = block.a[1];
				block.a[1] = tmp;	

				// swap {0,1}, reverse {2,3,4,5}, swap {6,7}
				{
					int indices[4*4];
					block.GetIndices(indices);

					for (int i = 0; i < 4*4; i++)
					{
						const int remap[] = {1,0,5,4,3,2,7,6};
						indices[i] = remap[indices[i]];
					}

					block.SetIndices(indices);
				}
			}
		}
	}
}

// non-destructive channel inversion, useful for visualising textures
void InvertTextureChannels(const grcTexture* texture, int mipIndex, int mipCount, bool bInvertRed, bool bInvertGreen, bool bInvertBlue, bool bInvertAlpha, bool bReverseBits, bool bVerbose)
{
	const grcImage::Format format = (grcImage::Format)texture->GetImageFormat();

	if (!bReverseBits)
	{
		if (format != grcImage::DXT1 &&
			format != grcImage::DXT3 &&
			format != grcImage::DXT5 &&
			format != grcImage::A8R8G8B8)
		{
			return; // unsupported format
		}
		else if (!bInvertRed && !bInvertGreen && !bInvertBlue)
		{
			if (format == grcImage::DXT1 || !bInvertAlpha)
			{
				return; // nothing to invert
			}
		}
	}

	if (bVerbose)
	{
		Displayf(
			"inverting texture \"%s\" fmt=%s, w=%d, h=%d, mips=%d, addr=0x%p",
			texture->GetName(),
			grcImage::GetFormatString((grcImage::Format)texture->GetImageFormat()),
			texture->GetWidth(),
			texture->GetHeight(),
			texture->GetMipMapCount(),
			texture
		);
	}

	for (int mip = mipIndex; mip < mipIndex + mipCount; mip++)
	{
		if (mip >= texture->GetMipMapCount())
		{
			break;
		}

		grcTextureLock lock;
		texture->LockRect(0, mip, lock, grcsWrite | grcsAllowVRAMLock);

		if (bVerbose)
		{
			Displayf("locked mip %d: w=%d, h=%d, pitch=%d, base=0x%p", mip, lock.Width, lock.Height, lock.Pitch, lock.Base);
		}

		const void* end = ((const u8*)lock.Base) + (lock.Pitch*lock.Height)/(grcImage::IsFormatDXTBlockCompressed(format) ? 4 : 1);

		if (bReverseBits)
		{
			for (u8* dst = (u8*)lock.Base; dst < end; dst++)
			{
				unsigned char a = *dst;

				a = ((a >> 1) & 0x55) | ((a & 0x55) << 1);
				a = ((a >> 2) & 0x33) | ((a & 0x33) << 2);

				*dst = (a >> 4) | (a << 4);
			}
		}

		if (format == grcImage::A8R8G8B8)
		{
			const u32 rgbaMask = Color32(bInvertRed ? 255 : 0, bInvertGreen ? 255 : 0, bInvertBlue ? 255 : 0, bInvertAlpha ? 255 : 0).GetColor();

			for (u32* dst = reinterpret_cast<u32*>(lock.Base); dst < end; dst++)
			{
				*dst ^= rgbaMask;
			}
		}
		else // format is compressed
		{
			const u16 rgbMask = DXT::RGB565(bInvertRed ? 31 : 0, bInvertGreen ? 63 : 0, bInvertBlue ? 31 : 0).rgb;
			const u8 alphaMask = bInvertAlpha ? 255 : 0;

			if (format == grcImage::DXT1)
			{
				for (DXT::DXT1_BLOCK* dst = reinterpret_cast<DXT::DXT1_BLOCK*>(lock.Base); dst < end; dst++)
				{
					InvertTextureChannels_DXT1_BLOCK(*dst, rgbMask);
				}
			}
			else if (format == grcImage::DXT3)
			{
				for (DXT::DXT3_BLOCK* dst = reinterpret_cast<DXT::DXT3_BLOCK*>(lock.Base); dst < end; dst++)
				{
					InvertTextureChannels_DXT3_ALPHA(dst->m_alpha, alphaMask);
					InvertTextureChannels_DXT1_BLOCK(dst->m_colour, rgbMask);
				}
			}
			else if (format == grcImage::DXT5)
			{
				for (DXT::DXT5_BLOCK* dst = reinterpret_cast<DXT::DXT5_BLOCK*>(lock.Base); dst < end; dst++)
				{
					InvertTextureChannels_DXT5_ALPHA(dst->m_alpha, alphaMask);
					InvertTextureChannels_DXT1_BLOCK(dst->m_colour, rgbMask);
				}
			}
		}

		texture->UnlockRect(lock);
	}
}

void SetSolidTextureColour(const grcTexture* texture, int mipIndex, int mipCount, Color32 colour, bool bVerbose)
{
	const grcImage::Format format = (grcImage::Format)texture->GetImageFormat();

	if (format != grcImage::DXT1 &&
		format != grcImage::DXT3 &&
		format != grcImage::DXT5 &&
		format != grcImage::A8R8G8B8)
	{
		return; // unsupported format
	}

	if (bVerbose)
	{
		Displayf(
			"setting solid colour \"%s\" fmt=%s, w=%d, h=%d, mips=%d, addr=0x%p",
			texture->GetName(),
			grcImage::GetFormatString((grcImage::Format)texture->GetImageFormat()),
			texture->GetWidth(),
			texture->GetHeight(),
			texture->GetMipMapCount(),
			texture
		);
	}

	for (int mip = mipIndex; mip < mipIndex + mipCount; mip++)
	{
		if (mip >= texture->GetMipMapCount())
		{
			break;
		}

		grcTextureLock lock;
		texture->LockRect(0, mip, lock, grcsWrite | grcsAllowVRAMLock);

		if (bVerbose)
		{
			Displayf("locked mip %d: w=%d, h=%d, pitch=%d, base=0x%p", mip, lock.Width, lock.Height, lock.Pitch, lock.Base);
		}

		const void* end = ((const u8*)lock.Base) + (lock.Pitch*lock.Height)/(grcImage::IsFormatDXTBlockCompressed(format) ? 4 : 1);

		if (format == grcImage::A8R8G8B8)
		{
			for (Color32* dst = reinterpret_cast<Color32*>(lock.Base); dst < end; dst++)
			{
				*dst = colour;
			}
		}
		else // format is compressed
		{
			const DXT::ARGB8888 rgba(colour.GetRed(), colour.GetGreen(), colour.GetBlue(), colour.GetAlpha());

			if (format == grcImage::DXT1)
			{
				for (DXT::DXT1_BLOCK* dst = reinterpret_cast<DXT::DXT1_BLOCK*>(lock.Base); dst < end; dst++)
				{
					dst->SetColour(rgba);
				}
			}
			else if (format == grcImage::DXT3)
			{
				for (DXT::DXT3_BLOCK* dst = reinterpret_cast<DXT::DXT3_BLOCK*>(lock.Base); dst < end; dst++)
				{
					dst->m_colour.SetColour(rgba);
				}
			}
			else if (format == grcImage::DXT5)
			{
				for (DXT::DXT5_BLOCK* dst = reinterpret_cast<DXT::DXT5_BLOCK*>(lock.Base); dst < end; dst++)
				{
					dst->m_colour.SetColour(rgba);
				}
			}
		}

		texture->UnlockRect(lock);
	}
}

#endif // __BANK
