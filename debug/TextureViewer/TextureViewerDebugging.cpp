// ==============================================
// debug/textureviewer/textureviewerdebugging.cpp
// (c) 2010 RockstarNorth
// ==============================================

#include "debug/textureviewer/textureviewerprivate.h"

#if __BANK

// Rage headers
#include "grcore/image.h"

// Framework headers
#include "fwutil/xmacro.h"
#include "fwsys/fileexts.h"
#include "fwscene/stores/txdstore.h"
#include "fwscene/stores/dwdstore.h"
#include "fwscene/stores/drawablestore.h"
#include "fwscene/stores/fragmentstore.h"
#include "fwscene/stores/mapdatastore.h"

// Game headers
#include "core/game.h"
#include "debug/DebugArchetypeProxy.h"
#include "modelinfo/modelinfo.h"
#include "scene/loader/MapData.h"

#include "debug/textureviewer/textureviewersearch.h"
#include "debug/textureviewer/textureviewerutil.h"
#include "debug/textureviewer/textureviewerdebugging.h"

DEBUG_TEXTURE_VIEWER_OPTIMISATIONS()

// ================================================================================================

#if DEBUG_TEXTURE_VIEWER_TRACKASSETHASH

CAssetStoreHashCheck::CAssetStoreHashCheck()
{
	m_count = 0;
	m_hash  = 0;
}

void CAssetStoreHashCheck::Reset()
{
	m_count = 0;
	m_hash  = 0;
}

template <typename AssetStoreType> void CAssetStoreHashCheck::Check(AssetStoreType& store, const char* name)
{
	if (m_count == 0)
	{
		m_count = store.GetSize();
	}
	else if (m_count*2 < store.GetSize()) // count has doubled? reset ..
	{
		m_count = store.GetSize();
		m_hash  = 0;
	}

	Assert(m_count <= store.GetSize()); // when this decreases, we need to reset?

	u32 hash = 0;

	for (int i = 0; i < m_count; i++)
	{
		hash = atLiteralStringHash(store.GetName(i), hash);
	}

	if (m_hash == 0)
	{
		Displayf("%s: hash=0x%08x, count=%d", name, hash, m_count);
	}
	else
	{
		Assertf(hash == m_hash, "%s: hash has changed!", name);	 
	}

	m_hash = hash;
}

__COMMENT(static) void CAssetStoreHashCheck::Update(bool bTrack)
{
	static int s_frameIndex = 0;

	static CAssetStoreHashCheck s_TxdStoreHashCheck;
	static CAssetStoreHashCheck s_DwdStoreHashCheck;
	static CAssetStoreHashCheck s_DrawableStoreHashCheck;
	static CAssetStoreHashCheck s_FragmentStoreHashCheck;

	if (bTrack)
	{
		if (++s_frameIndex >= 10) // every 10 frames
		{
			s_frameIndex = 0;

			s_TxdStoreHashCheck.Check(g_TxdStore, "g_TxdStore");
			s_DwdStoreHashCheck.Check(g_DwdStore, "g_DwdStore");
			s_DrawableStoreHashCheck.Check(g_DrawableStore, "g_DrawableStore");
			s_FragmentStoreHashCheck.Check(g_FragmentStore, "g_FragmentStore");
		}
	}
	else // reset
	{
		s_TxdStoreHashCheck.Reset();
		s_DwdStoreHashCheck.Reset();
		s_DrawableStoreHashCheck.Reset();
		s_FragmentStoreHashCheck.Reset();
	}
}

#endif // DEBUG_TEXTURE_VIEWER_TRACKASSETHASH

// ================================================================================================

void PrintTextureDictionaries()
{
	const bool bReportNonLoadedDictionaries = false;

	Displayf("searching texture dictionaries (g_TxdStore) ...");

	for (int i = 0; i < g_TxdStore.GetSize(); i++) // iterate over all texture dictionaries that are NOT inside drawables ..
	{
		const fwTxd* txd = g_TxdStore.GetSafeFromIndex(strLocalIndex(i));

		if (txd == NULL)
		{
			if (bReportNonLoadedDictionaries)
			{
				Displayf("  %04d. dictionary [0x00000000] \"%s\" is not loaded!", i, g_TxdStore.GetName(strLocalIndex(i)));
			}

			continue;
		}

		Displayf("  %04d. dictionary [0x%p] \"%s\" contains %d textures .. ", i, txd, g_TxdStore.GetName(strLocalIndex(i)), txd->GetCount());

		if (txd->GetParent())
		{
			Displayf("    parent -> [0x%p]", txd->GetParent());
		}

		for (int j = 0; j < txd->GetCount(); j++)
		{
			const grcTexture* texture = txd->GetEntry(j);

			if (texture)
			{
				Displayf("    %04d. [0x%p] wh=%d,%d, bpp=%d, mips=%d, layers=%d, size=%.2fKB, name=\"%s\"",
					j,
					texture,
					texture->GetWidth       (),
					texture->GetHeight      (),
					texture->GetBitsPerPixel(),
					texture->GetMipMapCount (),
					texture->GetLayerCount  (),
					(float)GetTextureSizeInBytes(texture)/1024.0f,
					GetFriendlyTextureName(texture).c_str()
				);
			}
		}
	}

	Displayf("");

	Displayf("searching drawable dictionaries (g_DwdStore) ...");

	for (int i = 0; i < g_DwdStore.GetSize(); i++)
	{
		const Dwd* dwd = g_DwdStore.GetSafeFromIndex(strLocalIndex(i));

		if (dwd == NULL)
		{
			if (bReportNonLoadedDictionaries)
			{
				Displayf("  %04d. dictionary [0x00000000] \"%s\" is not loaded!", i, g_DwdStore.GetName(strLocalIndex(i)));
			}

			continue;
		}

		Displayf("  %04d. drawable dictionary [0x%p] contains %d drawables ..", i, dwd, dwd->GetCount());

		for (int j = 0; j < dwd->GetCount(); j++)
		{
			const Drawable*       drawable            = dwd->GetEntry(j);
			const grmShaderGroup* drawableShaderGroup = drawable ? &drawable->GetShaderGroup() : NULL;
			const fwTxd*          txd                 = drawableShaderGroup ? drawableShaderGroup->GetTexDict() : NULL;

			if (txd)
			{
				Displayf("    %04d. dictionary [0x%p] contains %d textures .. ", j, txd, txd->GetCount());

				if (txd->GetParent())
				{
					Displayf("        parent -> [0x%p]", txd->GetParent());
				}

				for (int j = 0; j < txd->GetCount(); j++)
				{
					const grcTexture* texture = txd->GetEntry(j);

					if (texture)
					{
						Displayf("      %04d. [0x%p] wh=%d,%d, bpp=%d, mips=%d, layers=%d, size=%.2fKB, name=\"%s\"",
							j,
							texture,
							texture->GetWidth       (),
							texture->GetHeight      (),
							texture->GetBitsPerPixel(),
							texture->GetMipMapCount (),
							texture->GetLayerCount  (),
							(float)GetTextureSizeInBytes(texture)/1024.0f,
							GetFriendlyTextureName(texture).c_str()
						);
					}
				}
			}
			else
			{
				Displayf("    %04d. dictionary [0x00000000] .. ", j);
			}
		}
	}

	Displayf("");
}

// this function is a hack, and is not safe!
static const char* GetPaddedString(char buffer[256], const char* str, int width, char pad = ' ')
{
	strcpy(buffer, str);
	int len = istrlen(str);
	char* p = &buffer[len];

	while (len < width)
	{
		p[0] = pad;
		p[1] = '\0';
		p++;
		len++;
	}

	return buffer;
}

void PrintTextureDictionarySizes()
{
	const bool bReportNonLoadedDictionaries = false;
	const int maxDictNameLen = 32;
	char buffer[256] = "";

	Displayf("searching texture dictionaries (g_TxdStore) ...");

	for (int i = 0; i < g_TxdStore.GetSize(); i++) // iterate over all texture dictionaries that are NOT inside drawables ..
	{
		const fwTxd* txd = g_TxdStore.GetSafeFromIndex(strLocalIndex(i));

		if (txd == NULL)
		{
			if (bReportNonLoadedDictionaries)
			{
				Displayf("  %04d. texture dictionary %s is not loaded!", i, GetPaddedString(buffer, g_TxdStore.GetName(strLocalIndex(i)), maxDictNameLen));
			}

			continue;
		}

		const int sizeInBytes = GetTextureDictionarySizeInBytes(txd);

		if (sizeInBytes > 0 || bReportNonLoadedDictionaries)
		{
			Displayf("  %04d. texture dictionary %s contains %3d textures .. texture data = %.2fKB",
				i,
				GetPaddedString(buffer, g_TxdStore.GetName(strLocalIndex(i)), maxDictNameLen),
				txd->GetCount(),
				(float)sizeInBytes/1024.0f
			);
		}

		//if (txd->GetParent())
		//{
		//	Displayf("    parent -> [0x%p]", txd->GetParent());
		//}
	}

	Displayf("");

	Displayf("searching drawable dictionaries (g_DwdStore) ...");

	for (int i = 0; i < g_DwdStore.GetSize(); i++)
	{
		const Dwd* dwd = g_DwdStore.GetSafeFromIndex(strLocalIndex(i));

		if (dwd == NULL)
		{
			if (bReportNonLoadedDictionaries)
			{
				Displayf("  %04d. drawable dictionary %s is not loaded!", i, GetPaddedString(buffer, g_DwdStore.GetName(strLocalIndex(i)), maxDictNameLen));
			}

			continue;
		}

		int sizeInBytes = 0;
		int numTextures = 0;

		for (int j = 0; j < dwd->GetCount(); j++)
		{
			const Drawable*       drawable            = dwd->GetEntry(j);
			const grmShaderGroup* drawableShaderGroup = drawable ? &drawable->GetShaderGroup() : NULL;
			const fwTxd*          txd                 = drawableShaderGroup ? drawableShaderGroup->GetTexDict() : NULL;

			if (txd)
			{
				sizeInBytes += GetTextureDictionarySizeInBytes(txd);
				numTextures += txd->GetCount();
			}
		}

		if (sizeInBytes > 0 || bReportNonLoadedDictionaries)
		{
			Displayf("  %04d. drawable dictionary %s contains %2d textures in %2d drawables .. texture data = %.2fKB",
				i,
				GetPaddedString(buffer, g_DwdStore.GetName(strLocalIndex(i)), maxDictNameLen),
				numTextures,
				dwd->GetCount(),
				(float)sizeInBytes/1024.0f
			);
		}
	}

	Displayf("");
}

void PrintModelInfoStuff()
{
	for (u32 i = 0; i < CDebugArchetype::GetMaxDebugArchetypeProxies(); i++)
	{
		const CDebugArchetypeProxy* modelInfo = CDebugArchetype::GetDebugArchetypeProxy(i);

		if (modelInfo == NULL) // this can happen, it seems .. not sure why
		{
			continue;
		}

		//if (!modelInfo->GetHasLoaded())
		//{
		//	continue;
		//}

		Displayf("%s", GetModelInfoDesc(i).c_str());

		atArray<CTxdRef> refs; GetAssociatedTxds_ModelInfo(refs, modelInfo, NULL, NULL);

		for (int j = 0; j < refs.size(); j++)
		{
			Displayf("  %s", refs[j].GetDesc().c_str());
		}
	}
}

// ================================================================================================

/*
Ok I'm confused ..

g_TxdStore, g_DwdStore, g_DrawableStore and g_FragmentStore (but not g_ParticleStore) implement a
GetParentTxdForSlot() method which returns the g_TxdStore slot for the parent texture dictionary
for the particular asset slot. So in effect any asset can have a parent txd asset. If the asset is
not currently loaded then GetParentTxdForSlot() may return -1 even though this slot does have a
parent txd (yes I have verified this), so using the streaming iterator may be necessary to get the
full child list for a particular txd. However I'm not sure if it's safe to assume that once the
asset is loaded then the GetParentTxdForSlot() is ready to be called, because those slots are
stored in a separate pool.

I've been using fwTxd's GetParent() method to get the parent txd's for txd assets, because I
wasn't aware that g_TxdStore had a GetParentTxdForSlot() method. Does this always return the same
result for txd's? Why are there two separate ways to get parent txd's for txd assets?

I also wasn't aware there was a GetParent() method for Dwd assets, but apparently there is.

So first of all, is a fwTxd's GetParent() ever different from g_TxdStore.GetParentTxdForSlot()? I
can check this in the streaming iterator ..

Second, what is Dwd's GetParent() actually used for? If a modelinfo refers to a Dwd then it also
refers to a specific entry, so I don't see how the parent relationship would actually come into
play here. Unless the Dwd's GetParent() has a parent txd somehow? That seems absurd. For now I'll
call GetParent() in the streaming iterator and report what GetParent() returns .. 

[UPDATE] - I've run some tests using CDTVStreamingIterator, and it seems that fwTxd::GetParent()
always agrees with g_TxdStore.GetParentTxdSlot() -- note that g_TxdStore has a "GetParentTxdSlot"
whereas the other asset stores have "GetParentTxd_FOR_Slot", but anyway -- and Dwd::GetParent()
always returns NULL, at least for cptestbed. For the time being I'll continue to use
fwTxd::GetParent() rather than g_TxdStore.GetParentTxdSlot, but either way appears to work.

[UPDATE] - Talked with Klaas about this issue briefly, seems that the reason for the two methods
for accessing fwTxd's parent txd slot is due to the pgDictionary's m_Parent being a rage-thing
and fwTxdDef's parentId being a framework-thing, so while it might be possible to remove the
parentId it would not be possible to remove m_Parent from pgDictionary. Of course I'd have to
convince Adam that the m_Parent is always available when parentId is available, which I'm only
about 70% certain of. Maybe some other time ..
*/

static void _FindTxdChildren_TxdStore(atArray<int>& slots, int parentTxdIndex)
{
	if (parentTxdIndex == INDEX_NONE)
	{
		return;
	}

	const fwTxd* parentTxd = g_TxdStore.GetSafeFromIndex(strLocalIndex(parentTxdIndex));

	if (parentTxd) // TODO -- need to stream this in if it's NULL?
	{
		for (int i = 0; i < g_TxdStore.GetSize(); i++)
		{
			if (g_TxdStore.IsValidSlot(strLocalIndex(i)))
			{
				const fwTxd* txd = g_TxdStore.GetSafeFromIndex(strLocalIndex(i));

				if (txd && txd->GetParent() == parentTxd)
				{
					slots.PushAndGrow(i);
				}
			}
		}
	}
}

static void _FindTxdChildren_DwdStore(atArray<int>& slots, int parentTxdIndex)
{
	if (parentTxdIndex == INDEX_NONE)
	{
		return;
	}

	for (int i = 0; i < g_DwdStore.GetSize(); i++)
	{
		if (CAssetRef(AST_DwdStore, i).GetParentTxdIndex() == parentTxdIndex)
		{
			slots.PushAndGrow(i);
		}
	}
}

static void _FindTxdChildren_DrawableStore(atArray<int>& slots, int parentTxdIndex)
{
	if (parentTxdIndex == INDEX_NONE)
	{
		return;
	}

	for (int i = 0; i < g_DrawableStore.GetSize(); i++)
	{
		if (CAssetRef(AST_DrawableStore, i).GetParentTxdIndex() == parentTxdIndex)
		{
			slots.PushAndGrow(i);
		}
	}
}

static void _FindTxdChildren_FragmentStore(atArray<int>& slots, int parentTxdIndex)
{
	if (parentTxdIndex == INDEX_NONE)
	{
		return;
	}

	for (int i = 0; i < g_FragmentStore.GetSize(); i++)
	{
		if (CAssetRef(AST_FragmentStore, i).GetParentTxdIndex() == parentTxdIndex)
		{
			slots.PushAndGrow(i);
		}
	}
}

void PrintTxdChildren(int parentTxdIndex)
{
	Displayf("txd children for %s ..", CAssetRef(AST_TxdStore, parentTxdIndex).GetDesc().c_str());

	// TxdStore
	{
		atArray<int> slots; _FindTxdChildren_TxdStore(slots, parentTxdIndex);

		for (int i = 0; i < slots.size(); i++)
		{
			Displayf("  %s", CAssetRef(AST_TxdStore, slots[i]).GetDesc().c_str());
		}
	}

	// DwdStore
	{
		atArray<int> slots; _FindTxdChildren_DwdStore(slots, parentTxdIndex);

		for (int i = 0; i < slots.size(); i++)
		{
			Displayf("  %s", CAssetRef(AST_DwdStore, slots[i]).GetDesc().c_str());
		}
	}

	// DrawableStore
	{
		atArray<int> slots; _FindTxdChildren_DrawableStore(slots, parentTxdIndex);

		for (int i = 0; i < slots.size(); i++)
		{
			Displayf("  %s", CAssetRef(AST_DrawableStore, slots[i]).GetDesc().c_str());
		}
	}

	// FragmentStore
	{
		atArray<int> slots; _FindTxdChildren_FragmentStore(slots, parentTxdIndex);

		for (int i = 0; i < slots.size(); i++)
		{
			Displayf("  %s", CAssetRef(AST_FragmentStore, slots[i]).GetDesc().c_str());
		}
	}
}

#endif // __BANK
