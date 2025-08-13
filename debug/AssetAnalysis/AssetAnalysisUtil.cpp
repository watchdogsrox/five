// =========================================
// debug/AssetAnalysis/AssetAnalysisUtil.cpp
// (c) 2013 RockstarNorth
// =========================================

#if __BANK

#include "atl/string.h"
#include "grcore/edgeExtractgeomspu.h"
#include "grcore/image.h"
#include "grcore/image_dxt.h"
#include "grcore/texture.h"
#include "file/asset.h"
#include "file/packfile.h"
#include "string/string.h"
#include "string/stringhash.h"
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
#include "fwutil/xmacro.h"
#include "streaming/packfilemanager.h"
#include "streaming/streaming.h"
#include "streaming/streamingengine.h"
#include "timecycle/tcbox.h"
#include "timecycle/tcmodifier.h"
#include "vfx/ptfx/ptfxasset.h"
#include "vfx/ptfx/ptfxmanager.h"

#include "core/game.h"
#include "debug/AssetAnalysis/AssetAnalysisCommon.h"
#include "debug/AssetAnalysis/AssetAnalysisUtil.h"
#include "debug/DebugArchetypeProxy.h"
#include "modelinfo/MloModelInfo.h"
#include "modelinfo/ModelInfo.h"
#include "modelinfo/PedModelInfo.h"
#include "modelinfo/VehicleModelInfo.h"
#include "physics/gtaArchetype.h" // for gtaFragType
#include "renderer/GtaDrawable.h" // for gtaDrawable
#include "scene/loader/MapData.h"

namespace AssetAnalysis {

template <typename T> static const char* GetRPFPath_T(T& store, int slot)
{
	if (slot != INDEX_NONE)
	{
		if (store.IsValidSlot(strLocalIndex(slot)))
		{
			const strIndex          streamingIndex = store.GetStreamingIndex(strLocalIndex(slot));
			const strStreamingInfo* streamingInfo  = strStreamingEngine::GetInfo().GetStreamingInfo(streamingIndex);
			const strStreamingFile* streamingFile  = strPackfileManager::GetImageFileFromHandle(streamingInfo->GetHandle());

			if (streamingFile)
			{
				const char* path = streamingFile->m_IsPatchfile ? streamingFile->m_mappedName.TryGetCStr() : streamingFile->m_name.TryGetCStr();

				if (path)
				{
					return striskip(path, "platform:/");
				}
				else
				{
					return "$NULL_RPF_NAME";
				}
			}
			else
			{
				return "$INVALID_RPF_FILE";
			}
		}
		else
		{
			return "$INVALID_RPF_SLOT";
		}
	}
	else
	{
		return "";
	}
}

const char* GetRPFPath(fwTxdStore     & store, int slot) { return GetRPFPath_T(store, slot); }
const char* GetRPFPath(fwDwdStore     & store, int slot) { return GetRPFPath_T(store, slot); }
const char* GetRPFPath(fwDrawableStore& store, int slot) { return GetRPFPath_T(store, slot); }
const char* GetRPFPath(fwFragmentStore& store, int slot) { return GetRPFPath_T(store, slot); }
const char* GetRPFPath(ptfxAssetStore & store, int slot) { return GetRPFPath_T(store, slot); }
const char* GetRPFPath(fwMapDataStore & store, int slot) { return GetRPFPath_T(store, slot); }
const char* GetRPFPath(fwMapTypesStore& store, int slot) { return GetRPFPath_T(store, slot); }

atString GetArchetypePath(const CDebugArchetypeProxy* arch)
{
	if (arch)
	{
		const char* rpfPath = "static";

		if (arch->GetMapTypeSlot() != INDEX_NONE)
		{
			rpfPath = GetRPFPath(g_MapTypesStore, arch->GetMapTypeSlot());
		}

		return atVarString("%s/%s", rpfPath, arch->GetModelName());
	}

	return atString("?");
}

atString GetDrawablePath(const CBaseModelInfo* modelInfo)
{
	if (modelInfo)
	{
		switch ((int)modelInfo->GetDrawableType())
		{
		case fwArchetype::DT_FRAGMENT           : return AssetAnalysis::GetAssetPath(g_FragmentStore, modelInfo->GetFragmentIndex(), INDEX_NONE);
		case fwArchetype::DT_DRAWABLE           : return AssetAnalysis::GetAssetPath(g_DrawableStore, modelInfo->GetDrawableIndex(), INDEX_NONE);
		case fwArchetype::DT_DRAWABLEDICTIONARY : return AssetAnalysis::GetAssetPath(g_DwdStore     , modelInfo->GetDrawDictIndex(), modelInfo->GetDrawDictDrawableIndex());
		}
	}

	return atString("?");
}

template <typename T> static void GetAssetPath_internal(char* out, T& store, int slot, const char* ext)
{
	const char* path = GetRPFPath_T(store, slot);
	const char* name = striskip(store.GetName(strLocalIndex(slot)), "platform:/");

	if (path[0] != '$')
	{
		strcpy(out, path);
		char* tmp = strrchr(out, '.');

		if (tmp && stricmp(tmp, ".rpf") == 0)
		{
			tmp[0] = '\0';
		}
	}
	else
	{
		if (((const void*)&store == &g_DwdStore      && stristr(name, "models/")) ||
			((const void*)&store == &g_FragmentStore && stristr(name, "models/")))
		{
			// these are in "/models" already, no need to warn about missing RPF path
			strcpy(out, "models");
			name = strchr(name, '/') + 1;
		}
		else if ((const void*)&store == &g_TxdStore && stristr(name, "textures/"))
		{
			// these are itd's in "/textures" already, no need to warn about missing RPF path
			strcpy(out, "textures");
			name = strchr(name, '/') + 1;
		}
		else
		{
			sprintf(out, "$UNKNOWN_RPF[%d]", store.GetStreamingIndex(strLocalIndex(slot)).Get());
		}
	}

	if (strstr(path, "componentpeds") ||
		strstr(path, "streamedpeds") ||
		strstr(path, "cutspeds"))
	{
		strcat(out, "/");
		strcat(out, name);

		if (strchr(out, '/')) // remove chars after '/', e.g. if name is "hc_driver/accs_000_u" we just want "hc_driver"
		{
			strrchr(out, '/')[0] = '\0';
		}

		char* hi = strrchr(out, '+');

		if (hi && strcmp(hi, "+hi") == 0)
		{
			hi[0] = '\0';
		}

		strcat(out, ".ped");
	}
	else if (strstr(path, "/vehicles") && strstr(path, "/vehicles") == strrchr(path, '/'))
	{
		strcat(out, "_packed");

		if (strstr(name, "vehshare"))
		{
			strcat(out, "/vehiclesshared");
		}
		else
		{
			strcat(out, "/");
			strcat(out, name);

			if (strrchr(name, '+'))
			{
				strrchr(out, '+')[0] = '\0'; // strip off "+hi" from name
			}

			strcat(out, "/sp"); // we only deal with single player assets
		}
	}

	strcat(out, "/");
	strcat(out, name);
	strcat(out, ext);
}

template <typename T> static atString GetAssetPath_T(T& store, int slot, int entry);

template <> atString GetAssetPath_T<fwTxdStore>(fwTxdStore& store, int slot, int)
{
	char path[1024] = "";

	if (slot != INDEX_NONE)
	{
		if (store.IsValidSlot(strLocalIndex(slot)))
		{
			GetAssetPath_internal(path, store, slot, ".itd");
		}
		else
		{
			strcpy(path, atVarString("$INVALID_SLOT[%d]", slot).c_str());
		}
	}

	return atString(path);
}

static atString GetDwdEntryName(int slot, int entry)
{
	const Dwd* dwd = NULL;

	if (entry >= 0 && g_DwdStore.IsValidSlot(strLocalIndex(slot)))
	{
		dwd = g_DwdStore.Get(strLocalIndex(slot));

		if (dwd && entry < dwd->GetCount())
		{
			// try to get the name from the archetype .. works for most non-peds
			{
				const fwArchetype* a = fwArchetypeManager::GetArchetypeFromHashKey(dwd->GetCode(entry), NULL);

				if (a)
				{
					return atString(a->GetModelName());
				}
			}

			// archetype might not have been streamed in .. try the debug archetype proxy
			{
				const CDebugArchetypeProxy* a = CDebugArchetype::GetDebugArchetypeProxyForHashKey(dwd->GetCode(entry), NULL);

				if (a)
				{
					return atString(a->GetModelName());
				}
			}

			// try to get the name from the hash string .. works for some ped props i think
			{
				const atHashString h(dwd->GetCode(entry));
				const char* s = h.TryGetCStr();

				if (s)
				{
					return atVarString("%s",s);
				}
			}
		}
	}

	return atVarString("$DRAWABLE_%d", entry); // failed
}

template <> atString GetAssetPath_T<fwDwdStore>(fwDwdStore& store, int slot, int entry)
{
	char path[1024] = "";

	if (slot != INDEX_NONE)
	{
		if (store.IsValidSlot(strLocalIndex(slot)))
		{
			GetAssetPath_internal(path, store, slot, ".idd");

			if (entry != INDEX_NONE)
			{
				strcat(path, "/");
				strcat(path, GetDwdEntryName(slot, entry).c_str());
			}
		}
		else
		{
			strcpy(path, atVarString("$INVALID_SLOT[%d/%d]", slot, entry).c_str());
		}
	}

	return atString(path);
}

template <> atString GetAssetPath_T<fwDrawableStore>(fwDrawableStore& store, int slot, int)
{
	char path[1024] = "";

	if (slot != INDEX_NONE)
	{
		if (store.IsValidSlot(strLocalIndex(slot)))
		{
			GetAssetPath_internal(path, store, slot, ".idr");
		}
		else
		{
			strcpy(path, atVarString("$INVALID_SLOT[%d]", slot).c_str());
		}
	}

	return atString(path);
}

template <> atString GetAssetPath_T<fwFragmentStore>(fwFragmentStore& store, int slot, int entry)
{
	char path[1024] = "";

	if (slot != INDEX_NONE)
	{
		if (store.IsValidSlot(strLocalIndex(slot)))
		{
			GetAssetPath_internal(path, store, slot, ".ift");

			if (entry != FRAGMENT_COMMON_DRAWABLE_ENTRY)
			{
				strcat(path, "/");

				const Fragment* fragment = store.Get(strLocalIndex(slot));
				const char* entryName = NULL;

				if (entry == FRAGMENT_CLOTH_DRAWABLE_ENTRY)
				{
					entryName = "cloth";
				}
				else if (fragment && entry >= 0 && entry < fragment->GetNumExtraDrawables())
				{
					entryName = fragment->GetExtraDrawableName(entry);
				}

				if (entryName)
				{
					strcat(path, entryName);
				}
				else
				{
					strcat(path, atVarString("$DRAWABLE_%d", entry).c_str()); // failed
				}
			}
		}
		else
		{
			strcpy(path, atVarString("$INVALID_SLOT[%d/%d]", slot, entry).c_str());
		}
	}

	return atString(path);
}

template <> atString GetAssetPath_T<ptfxAssetStore>(ptfxAssetStore& store, int slot, int entry)
{
	char path[1024] = "";

	if (slot != INDEX_NONE)
	{
		if (store.IsValidSlot(strLocalIndex(slot)))
		{
			GetAssetPath_internal(path, store, slot, ".ipt");

			if (entry != INDEX_NONE)
			{
				strcat(path, "/");

				ptxFxList* ptx = store.Get(strLocalIndex(slot));
				Dwd* dwd = ptx->GetModelDictionary();
				const char* drawableName = NULL;

				if (ptx && dwd && entry < dwd->GetCount())
				{
					ptxParticleRuleDictionary* prd = ptx->GetParticleRuleDictionary();

					if (prd)
					{
						for (int i = 0; i < prd->GetCount(); i++)
						{
							ptxParticleRule* rule = prd->GetEntry(i);

							if (rule)
							{
								for (int j = 0; j < rule->GetNumDrawables(); j++)
								{
									ptxDrawable& pd = rule->GetDrawable(j);
									Drawable* drawable = pd.GetDrawable();

									if (drawable == dwd->GetEntry(entry))
									{
										drawableName = pd.GetName();

										if (strrchr(drawableName, '/')) // for some reason, drawable names are stores as <NAME>/<NAME>
										{
											drawableName = strrchr(drawableName, '/') + 1;
										}

										break;
									}
								}
							}

							if (drawableName)
							{
								break;
							}
						}
					}
				}

				if (drawableName)
				{
					strcat(path, "models/");
					strcat(path, drawableName);
				}
				else
				{
					strcat(path, atVarString("models/$DRAWABLE_%d", entry).c_str()); // failed
				}
			}
		}
		else
		{
			strcpy(path, atVarString("$INVALID_SLOT[%d/%d]", slot, entry).c_str());
		}
	}

	return atString(path);
}

template <> atString GetAssetPath_T<fwMapDataStore>(fwMapDataStore& store, int slot, int)
{
	char path[1024] = "";

	if (slot != INDEX_NONE)
	{
		if (store.IsValidSlot(strLocalIndex(slot)))
		{
			GetAssetPath_internal(path, store, slot, "");
		}
		else
		{
			strcat(path, atVarString("$INVALID_SLOT[%d]", slot).c_str());
		}
	}

	return atString(path);
}

template <> atString GetAssetPath_T<fwMapTypesStore>(fwMapTypesStore& store, int slot, int)
{
	char path[1024] = "";

	if (slot != INDEX_NONE)
	{
		if (store.IsValidSlot(strLocalIndex(slot)))
		{
			GetAssetPath_internal(path, store, slot, "");
		}
		else
		{
			safecat(path, atVarString("$INVALID_SLOT[%d]", slot).c_str());
		}
	}

	return atString(path);
}

atString GetAssetPath(fwTxdStore     & store, int slot, int entry) { return GetAssetPath_T(store, slot, entry); }
atString GetAssetPath(fwDwdStore     & store, int slot, int entry) { return GetAssetPath_T(store, slot, entry); }
atString GetAssetPath(fwDrawableStore& store, int slot, int entry) { return GetAssetPath_T(store, slot, entry); }
atString GetAssetPath(fwFragmentStore& store, int slot, int entry) { return GetAssetPath_T(store, slot, entry); }
atString GetAssetPath(ptfxAssetStore & store, int slot, int entry) { return GetAssetPath_T(store, slot, entry); }
atString GetAssetPath(fwMapDataStore & store, int slot, int entry) { return GetAssetPath_T(store, slot, entry); }
atString GetAssetPath(fwMapTypesStore& store, int slot, int entry) { return GetAssetPath_T(store, slot, entry); }

template <typename T> static u32 GetAssetSize_T(T& store, int slot, eAssetSize as)
{
	if (store.IsValidSlot(strLocalIndex(slot)) && store.IsObjectInImage(strLocalIndex(slot)))
	{
		const strIndex index = store.GetStreamingIndex(strLocalIndex(slot));
		const strStreamingInfo& info = strStreamingEngine::GetInfo().GetStreamingInfoRef(index);

		switch (as)
		{
		case ASSET_SIZE_PHYSICAL : return info.ComputePhysicalSize(index, true);
		case ASSET_SIZE_VIRTUAL  : return info.ComputeVirtualSize(index, true);
		}
	}

	return 0;
}

u32 GetAssetSize(fwTxdStore     & store, int slot, eAssetSize as) { return GetAssetSize_T(store, slot, as); }
u32 GetAssetSize(fwDwdStore     & store, int slot, eAssetSize as) { return GetAssetSize_T(store, slot, as); }
u32 GetAssetSize(fwDrawableStore& store, int slot, eAssetSize as) { return GetAssetSize_T(store, slot, as); }
u32 GetAssetSize(fwFragmentStore& store, int slot, eAssetSize as) { return GetAssetSize_T(store, slot, as); }
u32 GetAssetSize(ptfxAssetStore & store, int slot, eAssetSize as) { return GetAssetSize_T(store, slot, as); }
u32 GetAssetSize(fwMapDataStore & store, int slot, eAssetSize as) { return GetAssetSize_T(store, slot, as); }

atString GetTextureName(const grcTexture* texture)
{
	char buffer[256] = "";
	const char* name = texture->GetName();

	if (name)
	{
		strcpy(buffer, striskip(name, "pack:/"));

		char* ext = strrchr(buffer, '.');

		if (ext && stricmp(ext, ".dds") == 0)
		{
			ext[0] = '\0';
		}
	}

	return atString(buffer);
}

#define FIND_SHARED_TXD_SLOT_NOT_FOUND -2

template <typename T> static int FindSharedTxdSlot_internal(T& store, int slot, const Drawable* drawable, const grcTexture*& texture, u32 textureKey)
{
	// first try drawable's txd
	{
		const fwTxd* txd = drawable->GetTexDictSafe();

		if (txd)
		{
			for (int i = 0; i < txd->GetCount(); i++)
			{
				if ((textureKey == 0 && txd->GetEntry(i) == texture) ||
					(textureKey != 0 && txd->Lookup(textureKey)))
				{
					if (textureKey != 0)
					{
						texture = txd->Lookup(textureKey);
					}

					return INDEX_NONE; // texture is in drawable's txd
				}
			}
		}
	}

	for (int parentTxdSlot = GetParentTxdSlot(store, slot); parentTxdSlot != INDEX_NONE; parentTxdSlot = g_TxdStore.GetParentTxdSlot(strLocalIndex(parentTxdSlot)).Get())
	{
		const fwTxd* txd = g_TxdStore.Get(strLocalIndex(parentTxdSlot));

		if (txd)
		{
			for (int i = 0; i < txd->GetCount(); i++)
			{
				if ((textureKey == 0 && txd->GetEntry(i) == texture) ||
					(textureKey != 0 && txd->Lookup(textureKey)))
				{
					if (textureKey != 0)
					{
						texture = txd->Lookup(textureKey);
					}

					return parentTxdSlot | (i<<16); // texture is in (potentially) shared txd
				}
			}
		}
	}

	// try mapdetail, for some reason this txd is not parented properly but its textures can still be used
	{
		const int parentTxdSlot = g_TxdStore.FindSlot("platform:/textures/mapdetail").Get();

		if (parentTxdSlot != INDEX_NONE)
		{
			const fwTxd* txd = g_TxdStore.Get(strLocalIndex(parentTxdSlot));

			if (txd)
			{
				for (int i = 0; i < txd->GetCount(); i++)
				{
					if ((textureKey == 0 && txd->GetEntry(i) == texture) ||
						(textureKey != 0 && txd->Lookup(textureKey)))
					{
						if (textureKey != 0)
						{
							texture = txd->Lookup(textureKey);
						}

						return parentTxdSlot | (i<<16);
					}
				}
			}
		}
	}

	return FIND_SHARED_TXD_SLOT_NOT_FOUND;
}

template <typename T> static atString GetTexturePath_T(T& store, int slot, int entry, const Drawable* drawable, const grcTexture* texture, u32 textureKey, bool* bUsesNonPackedTextures = NULL)
{
	if (texture == NULL)
	{
		return atString("");
	}

	const int slotAndIndex = FindSharedTxdSlot_internal(store, slot, drawable, texture, textureKey);
	atString path = atString("");

	const atString textureName = GetTextureName(texture);

	if (slotAndIndex == INDEX_NONE) // texture is in drawable's txd
	{
		path = GetAssetPath(store, slot, entry);
	}
	else if (slotAndIndex != FIND_SHARED_TXD_SLOT_NOT_FOUND) // texture is in g_TxdStore
	{
		path = GetAssetPath(g_TxdStore, slotAndIndex & 0x0000ffff);

		if (bUsesNonPackedTextures)
		{
			*bUsesNonPackedTextures = true;
		}
	}
	else if (strcmp(textureName.c_str(), "image") == 0) // code-generated textures will come through as "./image"
	{
		path = ".";
	}
	else // ?
	{
		path = "$UNKNOWN_TEXTURE";

		if (bUsesNonPackedTextures)
		{
			*bUsesNonPackedTextures = true;
		}
	}

	if (striskip(textureName.c_str(), "common:/data/") == textureName.c_str())
	{
		path += "/";
		path += textureName;
	}
	else // textures in common:/data just come through with their names directly (e.g. common:/data/ped_detail_atlas)
	{
		path = textureName;
	}

	return path;
}

template <> atString GetTexturePath_T<ptfxAssetStore>(ptfxAssetStore& store, int slot, int entry, const Drawable* drawable, const grcTexture* texture, u32 textureKey, bool* bUsesNonPackedTextures)
{
	if (texture == NULL)
	{
		return atString("");
	}

	atString path = atString("");
	bool bFound = false;

	if (drawable) // first try drawable's txd (it won't be in there, but we can try)
	{
		const fwTxd* txd = drawable->GetTexDictSafe();

		if (txd)
		{
			for (int i = 0; i < txd->GetCount(); i++)
			{
				if ((textureKey == 0 && txd->GetEntry(i) == texture) ||
					(textureKey != 0 && txd->Lookup(textureKey)))
				{
					if (textureKey != 0)
					{
						texture = txd->Lookup(textureKey);
					}

					path = GetAssetPath(store, slot, entry);
					path += "textures";
					bFound = true;
					break;
				}
			}
		}
	}

	if (!bFound) // try ptx's txd
	{
		const fwTxd* txd = store.Get(strLocalIndex(slot))->GetTextureDictionary();

		if (txd)
		{
			for (int i = 0; i < txd->GetCount(); i++)
			{
				if ((textureKey == 0 && txd->GetEntry(i) == texture) ||
					(textureKey != 0 && txd->Lookup(textureKey)))
				{
					if (textureKey != 0)
					{
						texture = txd->Lookup(textureKey);
					}

					path = GetAssetPath(store, slot);
					path += "/textures";
					bFound = true;
					break;
				}
			}
		}
	}

	if (!bFound) // try ptx/core.ipt's txd
	{
		const strLocalIndex coreSlot = strLocalIndex(g_ParticleStore.FindSlotFromHashKey(atHashValue("core")));

		if (AssertVerify(coreSlot != INDEX_NONE))
		{
			ptxFxList* core = store.Get(coreSlot);

			if (AssertVerify(core))
			{
				const fwTxd* txd = core->GetTextureDictionary();

				if (txd)
				{
					for (int i = 0; i < txd->GetCount(); i++)
					{
						if ((textureKey == 0 && txd->GetEntry(i) == texture) ||
							(textureKey != 0 && txd->Lookup(textureKey)))
						{
							if (bUsesNonPackedTextures)
							{
								*bUsesNonPackedTextures = true;
							}

							if (textureKey != 0)
							{
								texture = txd->Lookup(textureKey);
							}

							path = GetAssetPath(store, coreSlot.Get());
							path += "/textures";
							bFound = true;
							break;
						}
					}
				}
			}
		}
	}

	if (!bFound)
	{
		path = "$UNKNOWN_TEXTURE";

		if (bUsesNonPackedTextures)
		{
			*bUsesNonPackedTextures = true;
		}
	}

	path += "/";
	path += GetTextureName(texture);

	return path;
}

atString GetTexturePath(fwTxdStore     & store, int slot, int entry, const Drawable* drawable, const grcTexture* texture, u32 textureKey, bool* bUsesNonPackedTextures) { return GetTexturePath_T(store, slot, entry, drawable, texture, textureKey, bUsesNonPackedTextures); }
atString GetTexturePath(fwDwdStore     & store, int slot, int entry, const Drawable* drawable, const grcTexture* texture, u32 textureKey, bool* bUsesNonPackedTextures) { return GetTexturePath_T(store, slot, entry, drawable, texture, textureKey, bUsesNonPackedTextures); }
atString GetTexturePath(fwDrawableStore& store, int slot, int entry, const Drawable* drawable, const grcTexture* texture, u32 textureKey, bool* bUsesNonPackedTextures) { return GetTexturePath_T(store, slot, entry, drawable, texture, textureKey, bUsesNonPackedTextures); }
atString GetTexturePath(fwFragmentStore& store, int slot, int entry, const Drawable* drawable, const grcTexture* texture, u32 textureKey, bool* bUsesNonPackedTextures) { return GetTexturePath_T(store, slot, entry, drawable, texture, textureKey, bUsesNonPackedTextures); }
atString GetTexturePath(ptfxAssetStore & store, int slot, int entry, const Drawable* drawable, const grcTexture* texture, u32 textureKey, bool* bUsesNonPackedTextures) { return GetTexturePath_T(store, slot, entry, drawable, texture, textureKey, bUsesNonPackedTextures); }

atString GetTexturePath(const fwArchetype* archetype, const grcTexture* texture)
{
	const Drawable* drawable = archetype->GetDrawable();

	if (drawable)
	{
		switch ((int)archetype->GetDrawableType())
		{
		case fwArchetype::DT_FRAGMENT           : return GetTexturePath(g_FragmentStore, archetype->GetFragmentIndex(), INDEX_NONE, drawable, texture, 0, NULL);
		case fwArchetype::DT_DRAWABLE           : return GetTexturePath(g_DrawableStore, archetype->GetDrawableIndex(), INDEX_NONE, drawable, texture, 0, NULL);
		case fwArchetype::DT_DRAWABLEDICTIONARY : return GetTexturePath(g_DwdStore     , archetype->GetDrawDictIndex(), archetype->GetDrawDictDrawableIndex(), drawable, texture, 0, NULL);
		}
	}

	return atString("");
}

u32 GetTexturePixelDataHash(const grcTexture* texture, bool bHashName, int numLines)
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

template <typename T> static const CDebugArchetypeProxy* GetArchetypeProxyForDrawable_T(T& store, int slot, int entry)
{
	for (u32 i = 0; i < CDebugArchetype::GetMaxDebugArchetypeProxies(); i++)
	{
		const CDebugArchetypeProxy* arch = CDebugArchetype::GetDebugArchetypeProxy(i);

		if (arch)
		{
			if ((const void*)&store == &g_DwdStore && arch->SafeGetDrawDictIndex() == slot)
			{
				const Dwd* dwd = g_DwdStore.Get(strLocalIndex(slot));

				if (dwd && dwd->LookupLocalIndex(arch->GetHashKey()) == entry)
				{
					return arch;
				}
			}

			if (((const void*)&store == &g_DrawableStore && arch->SafeGetDrawableIndex() == slot) ||
				((const void*)&store == &g_FragmentStore && arch->SafeGetFragmentIndex() == slot))
			{
				return arch;
			}
		}
	}

	return NULL;
}

const CDebugArchetypeProxy* GetArchetypeProxyForDrawable(fwDwdStore     & store, int slot, int entry) { return GetArchetypeProxyForDrawable_T(store, slot, entry); }
const CDebugArchetypeProxy* GetArchetypeProxyForDrawable(fwDrawableStore& store, int slot, int entry) { return GetArchetypeProxyForDrawable_T(store, slot, entry); }
const CDebugArchetypeProxy* GetArchetypeProxyForDrawable(fwFragmentStore& store, int slot, int entry) { return GetArchetypeProxyForDrawable_T(store, slot, entry); }

const CDebugArchetypeProxy* GetArchetypeProxyForDrawable(ptfxAssetStore&, int, int)
{
	return NULL;
}

int GetParentTxdSlot(fwTxdStore     &, int slot) { return g_TxdStore     .GetParentTxdSlot   (strLocalIndex(slot)).Get(); }
int GetParentTxdSlot(fwDwdStore     &, int slot) { return g_DwdStore     .GetParentTxdForSlot(strLocalIndex(slot)).Get(); }
int GetParentTxdSlot(fwDrawableStore&, int slot) { return g_DrawableStore.GetParentTxdForSlot(strLocalIndex(slot)).Get(); }
int GetParentTxdSlot(fwFragmentStore&, int slot) { return g_FragmentStore.GetParentTxdForSlot(strLocalIndex(slot)).Get(); }
int GetParentTxdSlot(ptfxAssetStore &, int)      { return INDEX_NONE; }

// useful to check for textures which look like normal maps etc.
Vec4V_Out GetTextureAverageColour(const grcTexture* texture, Vec4V* colourMinOut, Vec4V* colourMaxOut, Vec4V* colourDiffOut_RG_GB_BA_AR, Vec2V* colourDiffOut_RB_GA)
{
	const int w = texture->GetWidth ();
	const int h = texture->GetHeight();

	grcTextureLock lock;
	texture->LockRect(0, 0, lock, grcsRead | grcsAllowVRAMLock);

	grcImage* image = rage_new grcImage();
	image->LoadAsTextureAlias(texture, &lock);

	Vec4V colourMin =  Vec4V(V_FLT_MAX);
	Vec4V colourMax = -Vec4V(V_FLT_MAX);
	Vec4V colourSum =  Vec4V(V_ZERO);

	Vec4V colourDiff_RG_GB_BA_AR = Vec4V(V_ZERO);
	Vec2V colourDiff_RB_GA       = Vec2V(V_ZERO);

	for (int y = 0; y <= h - 4; y += 4)
	{
		for (int x = 0; x <= w - 4; x += 4)
		{
			Vector4 block[4*4];

			image->GetPixelBlock(block, x, y);

			for (int yy = 0; yy < 4; yy++)
			{
				for (int xx = 0; xx < 4; xx++)
				{
					if (x + xx < w &&
						y + yy < h)
					{
						const Vec4V c = texture->Swizzle(RCC_VEC4V(block[xx + yy*4]));

						colourMin = Min(c, colourMin);
						colourMax = Max(c, colourMax);
						colourSum += c;

						colourDiff_RG_GB_BA_AR = Max(Abs(c - c.Get<Vec::Y,Vec::Z,Vec::W,Vec::X>()), colourDiff_RG_GB_BA_AR);
						colourDiff_RB_GA       = Max(Abs(c.Get<Vec::X,Vec::Z>() - c.Get<Vec::Y,Vec::W>()), colourDiff_RB_GA);
					}
				}
			}
		}
	}

	image->ReleaseAlias();
	texture->UnlockRect(lock);

	if (colourMinOut) { *colourMinOut = colourMin; }
	if (colourMaxOut) { *colourMaxOut = colourMax; }

	if (colourDiffOut_RG_GB_BA_AR) { *colourDiffOut_RG_GB_BA_AR = colourDiff_RG_GB_BA_AR; }
	if (colourDiffOut_RB_GA      ) { *colourDiffOut_RB_GA       = colourDiff_RB_GA      ; }

	return colourSum*ScalarV(1.0f/(float)(w*h));
}

// faster version which extracts raw values from DXT blocks, also constructs histograms
void GetTextureRGBMinMaxHistogramRaw(const grcTexture* texture, u16& rgbmin, u16& rgbmax, int* rhist32, int* ghist64, int* bhist32)
{
	u16 rmin = 0xffff;
	u16 gmin = 0xffff;
	u16 bmin = 0xffff;
	u16 rmax = 0x0000;
	u16 gmax = 0x0000;
	u16 bmax = 0x0000;

	if (grcImage::IsFormatDXTBlockCompressed((grcImage::Format)texture->GetImageFormat()))
	{
		const int w = texture->GetWidth () & ~3;
		const int h = texture->GetHeight() & ~3;

		grcTextureLock lock;
		texture->LockRect(0, 0, lock, grcsRead | grcsAllowVRAMLock);

		grcImage* image = rage_new grcImage();
		image->LoadAsTextureAlias(texture, &lock);

		#define GET_RGB_MIN_MAX_HISTOGRAM(block) \
		{ \
			DXT::RGB565 c0 = block->c[0]; \
			DXT::RGB565 c1 = block->c[1]; \
			PPU_ONLY(c0.ByteSwap()); \
			PPU_ONLY(c1.ByteSwap()); \
			rmin = Min<u16>(c0.r, c1.r, rmin); \
			gmin = Min<u16>(c0.g, c1.g, gmin); \
			bmin = Min<u16>(c0.b, c1.b, bmin); \
			rmax = Max<u16>(c0.r, c1.r, rmax); \
			gmax = Max<u16>(c0.g, c1.g, gmax); \
			bmax = Max<u16>(c0.b, c1.b, bmax); \
			if (rhist32) { rhist32[c0.r]++; rhist32[c1.r]++; } \
			if (ghist64) { ghist64[c0.g]++; ghist64[c1.g]++; } \
			if (bhist32) { bhist32[c0.b]++; bhist32[c1.b]++; } \
		}

		switch (texture->GetImageFormat())
		{
		case grcImage::DXT1:
			{
				for (int y = 0; y < h; y += 4) for (int x = 0; x < w; x += 4)
				{
					const DXT::DXT1_BLOCK* block = reinterpret_cast<const DXT::DXT1_BLOCK*>(image->GetPixelAddr(x, y));
					GET_RGB_MIN_MAX_HISTOGRAM(block);
				}
				break;
			}
		case grcImage::DXT3:
			{
				for (int y = 0; y < h; y += 4) for (int x = 0; x < w; x += 4)
				{
					const DXT::DXT1_BLOCK* block = &reinterpret_cast<const DXT::DXT3_BLOCK*>(image->GetPixelAddr(x, y))->m_colour;
					GET_RGB_MIN_MAX_HISTOGRAM(block);
				}
				break;
			}
		case grcImage::DXT5:
			{
				for (int y = 0; y < h; y += 4) for (int x = 0; x < w; x += 4)
				{
					const DXT::DXT1_BLOCK* block = &reinterpret_cast<const DXT::DXT5_BLOCK*>(image->GetPixelAddr(x, y))->m_colour;
					GET_RGB_MIN_MAX_HISTOGRAM(block);
				}
				break;
			}
		}

		#undef GET_RGB_MIN_MAX_HISTOGRAM

		image->ReleaseAlias();
		texture->UnlockRect(lock);
	}

	rgbmin = (rmin & 31) | ((gmin & 63) << 5) | ((bmin & 31) << 11);
	rgbmax = (rmax & 31) | ((gmax & 63) << 5) | ((bmax & 31) << 11);
}


} // namespace AssetAnalysis

#endif // __BANK
