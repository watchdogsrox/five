// ==========================================================
// debug/textureviewer/textureviewerstreamingiteratortest.cpp
// (c) 2010 RockstarNorth
// ==========================================================

#include "debug/textureviewer/textureviewerprivate.h"

#if __BANK

#include "atl/string.h"
#include "grcore/edgeExtractgeomspu.h"
#include "grcore/image.h"
#include "grcore/image_dxt.h"
#include "grcore/texture.h"
#include "grcore/texturedebugflags.h"
#include "file/asset.h"
#include "file/default_paths.h"
#include "file/packfile.h"
#include "string/string.h"
#include "string/stringutil.h"
#include "system/memory.h"

#include "fragment/drawable.h"

#include "entity/archetypemanager.h"
#include "fwutil/xmacro.h"
#include "fwscene/stores/txdstore.h"
#include "fwscene/stores/dwdstore.h"
#include "fwscene/stores/drawablestore.h"
#include "fwscene/stores/fragmentstore.h"
#include "fwscene/stores/mapdatastore.h"
#include "streaming/streaming.h"
#include "streaming/streamingengine.h"
#include "streaming/packfilemanager.h"
#include "timecycle/tcbox.h"
#include "timecycle/tcmodifier.h"
#include "vfx/ptfx/ptfxasset.h"
#include "vfx/ptfx/ptfxmanager.h"

#include "core/game.h"
#include "debug/AssetAnalysis/AssetAnalysisUtil.h"
#include "debug/AssetAnalysis/AssetAnalysis.h"
#include "debug/DebugArchetypeProxy.h"
#include "modelinfo/MloModelInfo.h"
#include "modelinfo/modelinfo.h"
#include "modelinfo/pedmodelinfo.h"
#include "modelinfo/vehiclemodelinfo.h"
#include "renderer/GtaDrawable.h" // for gtaDrawable
#include "physics/gtaArchetype.h" // for gtaFragType
#include "scene/loader/MapData.h"
#include "shaders/CustomShaderEffectCable.h" // for CCustomShaderEffectCable::CheckDrawable
#include "timecycle/TimeCycle.h"

#include "vfx/vehicleglass/VehicleGlassManager.h" // for dumping water geometry // [WATERDRAW]

#include "debug/textureviewer/textureviewerutil.h" // for GetFriendlyTextureName, StringMatch
#include "debug/textureviewer/textureviewerstreamingiterator.h"
#include "debug/textureviewer/textureviewerstreamingiteratortest.h"

DEBUG_TEXTURE_VIEWER_OPTIMISATIONS()

// ================================================================================================

// these need to be consistent with TextureAnalysis settings, otherwise the crcs will not match
char RS_UNZIPPED[80] = RS_ASSETS "/unzipped";

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEST

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TXDCHILDREN
/*
class CDTVStreamingIteratorTest_TxdChildren
{
public:
	CDTVStreamingIteratorTest_TxdChildren() : m_slots_count(0) {}

	atArray<int> m_slots_TxdStore;
	atArray<int> m_slots_DwdStore;
	atArray<int> m_slots_DrawableStore;
	atArray<int> m_slots_FragmentStore;
	atArray<int> m_slots_ModelInfo;
	int          m_slots_count;
};

class CDTVStreamingIteratorTest_TxdChildrenMap
{
public:
	void SetParentTxdSlot_TxdStore      (int assetIndex, int parentTxdIndex);
	void SetParentTxdSlot_DwdStore      (int assetIndex, int parentTxdIndex);
	void SetParentTxdSlot_DrawableStore (int assetIndex, int parentTxdIndex);
	void SetParentTxdSlot_FragmentStore (int assetIndex, int parentTxdIndex);
	void SetParentTxdSlot_ModelInfo     (int assetIndex, int parentTxdIndex);
	void SetParentDwdSlot_ModelInfo     (int assetIndex, int parentDwdIndex);
	void SetParentDrawableSlot_ModelInfo(int assetIndex, int parentDrawableIndex);
	void SetParentFragmentSlot_ModelInfo(int assetIndex, int parentFragmentIndex);

	void IterateModelInfos();

	void Print() const;

	atMap<int,CDTVStreamingIteratorTest_TxdChildren> m_children;

	atMap<int,atArray<int> > m_dwdChildren;      // key = dwd      slot, data = model indices
	atMap<int,atArray<int> > m_drawableChildren; // key = drawable slot, data = model indices
	atMap<int,atArray<int> > m_fragmentChildren; // key = fragment slot, data = model indices
};

static CDTVStreamingIteratorTest_TxdChildrenMap gStreamingIteratorTest_TxdChildrenMap;

static __forceinline const char* _SafeGetModelInfoName(u32 modelInfoIndex)
{
	const CBaseModelInfo* modelInfo = CModelInfo::GetBaseModelInfo(fwModelId(modelInfoIndex));

	if (modelInfo)
	{
		return modelInfo->GetModelName();
	}

	return "[NULL]";
}

static __forceinline bool _PushUniqueSlow(atArray<int>& array, int index)
{
	if (index == INDEX_NONE)
	{
		return false;
	}

	for (int i = 0; i < array.size(); i++)
	{
		if (array[i] == index)
		{
			return false;
		}
	}

	array.PushAndGrow(index);

	return true;
}

void CDTVStreamingIteratorTest_TxdChildrenMap::SetParentTxdSlot_TxdStore(int assetIndex, int parentTxdIndex)
{
	if (parentTxdIndex != INDEX_NONE)
	{
		if (_PushUniqueSlow(m_children[parentTxdIndex].m_slots_TxdStore, assetIndex))
		{
			m_children[parentTxdIndex].m_slots_count++;
		}
	}
}

void CDTVStreamingIteratorTest_TxdChildrenMap::SetParentTxdSlot_DwdStore(int assetIndex, int parentTxdIndex)
{
	if (parentTxdIndex != INDEX_NONE)
	{
		if (_PushUniqueSlow(m_children[parentTxdIndex].m_slots_DwdStore, assetIndex))
		{
			m_children[parentTxdIndex].m_slots_count++;
		}
	}
}

void CDTVStreamingIteratorTest_TxdChildrenMap::SetParentTxdSlot_DrawableStore(int assetIndex, int parentTxdIndex)
{
	if (parentTxdIndex != INDEX_NONE)
	{
		if (_PushUniqueSlow(m_children[parentTxdIndex].m_slots_DrawableStore, assetIndex))
		{
			m_children[parentTxdIndex].m_slots_count++;
		}
	}
}

void CDTVStreamingIteratorTest_TxdChildrenMap::SetParentTxdSlot_FragmentStore(int assetIndex, int parentTxdIndex)
{
	if (parentTxdIndex != INDEX_NONE)
	{
		if (_PushUniqueSlow(m_children[parentTxdIndex].m_slots_FragmentStore, assetIndex))
		{
			m_children[parentTxdIndex].m_slots_count++;
		}
	}
}

void CDTVStreamingIteratorTest_TxdChildrenMap::SetParentTxdSlot_ModelInfo(int assetIndex, int parentTxdIndex)
{
	if (parentTxdIndex != INDEX_NONE)
	{
		if (_PushUniqueSlow(m_children[parentTxdIndex].m_slots_ModelInfo, assetIndex))
		{
			m_children[parentTxdIndex].m_slots_count++;
		}
	}
}

void CDTVStreamingIteratorTest_TxdChildrenMap::SetParentDwdSlot_ModelInfo(int assetIndex, int parentDwdIndex)
{
	if (parentDwdIndex != INDEX_NONE)
	{
		_PushUniqueSlow(m_dwdChildren[parentDwdIndex], assetIndex);
	}
}

void CDTVStreamingIteratorTest_TxdChildrenMap::SetParentDrawableSlot_ModelInfo(int assetIndex, int parentDrawableIndex)
{
	if (parentDrawableIndex != INDEX_NONE)
	{
		_PushUniqueSlow(m_drawableChildren[parentDrawableIndex], assetIndex);
	}
}

void CDTVStreamingIteratorTest_TxdChildrenMap::SetParentFragmentSlot_ModelInfo(int assetIndex, int parentFragmentIndex)
{
	if (parentFragmentIndex != INDEX_NONE)
	{
		_PushUniqueSlow(m_fragmentChildren[parentFragmentIndex], assetIndex);
	}
}

void CDTVStreamingIteratorTest_TxdChildrenMap::IterateModelInfos()
{
	for (u32 i = 0; i < CDebugArchetype::GetMaxDebugArchetypeProxies(); i++)
	{
		const CDebugArchetypeProxy* modelInfo = CDebugArchetype::GetDebugArchetypeProxy(i);

		if (modelInfo == NULL)
		{
			continue;
		}

		SetParentTxdSlot_ModelInfo     (i, modelInfo->GetAssetParentTxdIndex());
		SetParentDwdSlot_ModelInfo     (i, modelInfo->SafeGetDrawDictIndex  ());
		SetParentDrawableSlot_ModelInfo(i, modelInfo->SafeGetDrawableIndex  ());
		SetParentFragmentSlot_ModelInfo(i, modelInfo->SafeGetFragmentIndex  ());

		if (modelInfo->GetModelType() == MI_TYPE_PED)
		{
			const CDebugPedArchetypeProxy* pedModelInfo = (const CDebugPedArchetypeProxy*)modelInfo;

			SetParentTxdSlot_ModelInfo(i, pedModelInfo->GetHDTxdIndex());
			SetParentDwdSlot_ModelInfo(i, pedModelInfo->GetPedComponentFileIndex());
			SetParentDwdSlot_ModelInfo(i, pedModelInfo->GetPropsFileIndex());
		}
		else if (modelInfo->GetModelType() == MI_TYPE_VEHICLE)
		{
			const CDebugVehicleArchetypeProxy* vehModelInfo = (const CDebugVehicleArchetypeProxy*)modelInfo;

			SetParentTxdSlot_ModelInfo     (i, vehModelInfo->GetHDTxdIndex());
			SetParentFragmentSlot_ModelInfo(i, vehModelInfo->GetHDFragmentIndex());
		}
	}
}

void CDTVStreamingIteratorTest_TxdChildrenMap::Print() const
{
	for (int pass = 0; pass < 2; pass++) // first print multi-child relationships, then print single-child relationships
	{
		for (atMap<int,CDTVStreamingIteratorTest_TxdChildren>::ConstIterator iter = m_children.CreateIterator(); iter; ++iter)
		{
			const int parentTxdIndex = iter.GetKey();
			const CDTVStreamingIteratorTest_TxdChildren& children = iter.GetData();

			if (children.m_slots_count > 1)
			{
				if (pass == 0)
				{
					Displayf("Txd:%d(%s) inherited by ..", parentTxdIndex, g_TxdStore.GetName(parentTxdIndex));

					for (int i = 0; i < children.m_slots_TxdStore     .size(); i++) { Displayf("  Txd:%d(%s)"  , children.m_slots_TxdStore     [i], g_TxdStore     .GetName(children.m_slots_TxdStore     [i])); }
					for (int i = 0; i < children.m_slots_DwdStore     .size(); i++) { Displayf("  Dwd:%d(%s)"  , children.m_slots_DwdStore     [i], g_DwdStore     .GetName(children.m_slots_DwdStore     [i])); }
					for (int i = 0; i < children.m_slots_DrawableStore.size(); i++) { Displayf("  Draw:%d(%s)" , children.m_slots_DrawableStore[i], g_DrawableStore.GetName(children.m_slots_DrawableStore[i])); }
					for (int i = 0; i < children.m_slots_FragmentStore.size(); i++) { Displayf("  Frag:%d(%s)" , children.m_slots_FragmentStore[i], g_FragmentStore.GetName(children.m_slots_FragmentStore[i])); }
					for (int i = 0; i < children.m_slots_ModelInfo    .size(); i++) { Displayf("  Model:%d(%s)", children.m_slots_ModelInfo    [i], _SafeGetModelInfoName  (children.m_slots_ModelInfo    [i])); }
				}
			}
			else // assume 1 slot
			{
				if (pass == 1)
				{
					atVarString temp("Txd:%d(%s) inherited by ..", parentTxdIndex, g_TxdStore.GetName(parentTxdIndex));

					for (int i = 0; i < children.m_slots_TxdStore     .size(); i++) { temp += atVarString(" Txd:%d(%s)"  , children.m_slots_TxdStore     [i], g_TxdStore     .GetName(children.m_slots_TxdStore     [i])); }
					for (int i = 0; i < children.m_slots_DwdStore     .size(); i++) { temp += atVarString(" Dwd:%d(%s)"  , children.m_slots_DwdStore     [i], g_DwdStore     .GetName(children.m_slots_DwdStore     [i])); }
					for (int i = 0; i < children.m_slots_DrawableStore.size(); i++) { temp += atVarString(" Draw:%d(%s)" , children.m_slots_DrawableStore[i], g_DrawableStore.GetName(children.m_slots_DrawableStore[i])); }
					for (int i = 0; i < children.m_slots_FragmentStore.size(); i++) { temp += atVarString(" Frag:%d(%s)" , children.m_slots_FragmentStore[i], g_FragmentStore.GetName(children.m_slots_FragmentStore[i])); }
					for (int i = 0; i < children.m_slots_ModelInfo    .size(); i++) { temp += atVarString(" Model:%d(%s)", children.m_slots_ModelInfo    [i], _SafeGetModelInfoName  (children.m_slots_ModelInfo    [i])); }

					Displayf(temp);
				}
			}
		}

		for (atMap<int,atArray<int> >::ConstIterator iter = m_dwdChildren.CreateIterator(); iter; ++iter) // Dwd <- ModelInfo
		{
			const int parentDwdIndex = iter.GetKey();
			const atArray<int>& children = iter.GetData();

			if (children.size() > 1)
			{
				if (pass == 0)
				{
					Displayf("Dwd:%d(%s) inherited by ..", parentDwdIndex, g_DwdStore.GetName(parentDwdIndex));

					for (int i = 0; i < children.size(); i++)
					{
						Displayf("  Model:%d(%s)", children[i], _SafeGetModelInfoName(children[i]));
					}
				}
			}
			else // assume 1 slot
			{
				if (pass == 1)
				{
					atVarString temp("Dwd:%d(%s) inherited by ..", parentDwdIndex, g_DwdStore.GetName(parentDwdIndex));

					for (int i = 0; i < children.size(); i++)
					{
						temp += atVarString(" Model:%d(%s)", children[i], _SafeGetModelInfoName(children[i]));
					}

					Displayf(temp);
				}
			}
		}

		for (atMap<int,atArray<int> >::ConstIterator iter = m_drawableChildren.CreateIterator(); iter; ++iter) // Drawable <- ModelInfo
		{
			const int parentDrawableIndex = iter.GetKey();
			const atArray<int>& children = iter.GetData();

			if (children.size() > 1)
			{
				if (pass == 0)
				{
					Displayf("Draw:%d(%s) inherited by ..", parentDrawableIndex, g_DrawableStore.GetName(parentDrawableIndex));

					for (int i = 0; i < children.size(); i++)
					{
						Displayf("  Model:%d(%s)", children[i], _SafeGetModelInfoName(children[i]));
					}
				}
			}
			else // assume 1 slot
			{
				if (pass == 1)
				{
					atVarString temp("Draw:%d(%s) inherited by ..", parentDrawableIndex, g_DrawableStore.GetName(parentDrawableIndex));

					for (int i = 0; i < children.size(); i++)
					{
						temp += atVarString(" Model:%d(%s)", children[i], _SafeGetModelInfoName(children[i]));
					}

					Displayf(temp);
				}
			}
		}

		for (atMap<int,atArray<int> >::ConstIterator iter = m_fragmentChildren.CreateIterator(); iter; ++iter) // Fragment <- ModelInfo
		{
			const int parentFragmentIndex = iter.GetKey();
			const atArray<int>& children = iter.GetData();

			if (children.size() > 1)
			{
				if (pass == 0)
				{
					Displayf("Frag:%d(%s) inherited by ..", parentFragmentIndex, g_FragmentStore.GetName(parentFragmentIndex));

					for (int i = 0; i < children.size(); i++)
					{
						Displayf("  Model:%d(%s)", children[i], _SafeGetModelInfoName(children[i]));
					}
				}
			}
			else // assume 1 slot
			{
				if (pass == 1)
				{
					atVarString temp("Frag:%d(%s) inherited by ..", parentFragmentIndex, g_FragmentStore.GetName(parentFragmentIndex));

					for (int i = 0; i < children.size(); i++)
					{
						temp += atVarString(" Model:%d(%s)", children[i], _SafeGetModelInfoName(children[i]));
					}

					Displayf(temp);
				}
			}
		}
	}
}

void _CDTVStreamingIteratorTest_TxdChildrenMap_Print()
{
	gStreamingIteratorTest_TxdChildrenMap.IterateModelInfos();
	gStreamingIteratorTest_TxdChildrenMap.Print();
}
*/
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TXDCHILDREN

// ================================================================================================

template <typename T> const CDebugArchetypeProxy* CDTVStreamingIteratorTest_GetArchetypeProxyForDrawable(T& store, int slot, int index)
{
	for (u32 i = 0; i < CDebugArchetype::GetMaxDebugArchetypeProxies(); i++)
	{
		const CDebugArchetypeProxy* modelInfo = CDebugArchetype::GetDebugArchetypeProxy(i);

		if (modelInfo)
		{
			if ((const void*)&store == &g_DwdStore && modelInfo->SafeGetDrawDictIndex() == slot)
			{
				const Dwd* dwd = g_DwdStore.Get(slot);

				if (dwd && dwd->LookupLocalIndex(modelInfo->GetHashKey()) == index)
				{
					return modelInfo;
				}
			}

			if (((const void*)&store == &g_DrawableStore && modelInfo->SafeGetDrawableIndex() == slot) ||
				((const void*)&store == &g_FragmentStore && modelInfo->SafeGetFragmentIndex() == slot))
			{
				return modelInfo;
			}
		}
	}

	return NULL;
}

template <typename T> static const char* CDTVStreamingIteratorTest_GetRPFPathName(T& store, int slot)
{
	if (slot != INDEX_NONE && store.IsValidSlot(slot))
	{
		const strIndex          streamingIndex = store.GetStreamingIndex(slot);
		const strStreamingInfo* streamingInfo  = strStreamingEngine::GetInfo().GetStreamingInfo(streamingIndex);
		const strStreamingFile* streamingFile  = strPackfileManager::GetImageFileFromHandle(streamingInfo->GetHandle());

		if (streamingFile)
		{
			return streamingFile->m_name.GetCStr();
		}

		return "/$INVALID_FILE";
	}

	return "/$INVALID_SLOT";
}

template <typename T> atString CDTVStreamingIteratorTest_GetAssetDesc(T& store, int slot, int index);

template <> atString CDTVStreamingIteratorTest_GetAssetDesc<fwTxdStore>(fwTxdStore& store, int slot, int)
{
	return atVarString("Txd:%d(%s)", slot, store.GetName(slot));
}

template <> atString CDTVStreamingIteratorTest_GetAssetDesc<fwDwdStore>(fwDwdStore& store, int slot, int index)
{
	return atVarString("Dwd:%d_%d(%s)", slot, index, store.GetName(slot));
}

template <> atString CDTVStreamingIteratorTest_GetAssetDesc<fwDrawableStore>(fwDrawableStore& store, int slot, int)
{
	return atVarString("Draw:%d(%s)", slot, store.GetName(slot));
}

template <> atString CDTVStreamingIteratorTest_GetAssetDesc<fwFragmentStore>(fwFragmentStore& store, int slot, int index)
{
	return (index == INDEX_NONE)
		? atVarString("Frag:%d_COMMON(%s)"   , slot,        store.GetName(slot))
		: atVarString("Frag:%d_EXTRA[%d](%s)", slot, index, store.GetName(slot));
}

template <> atString CDTVStreamingIteratorTest_GetAssetDesc<ptfxAssetStore>(ptfxAssetStore& store, int slot, int index)
{
	return (index == INDEX_NONE)
		? atVarString("Part:%d(%s)"          , slot,        store.GetName(slot))
		: atVarString("Part:%d_MODEL[%d](%s)", slot, index, store.GetName(slot));
}

template <typename T> atString CDTVStreamingIteratorTest_GetAssetPath(T& store, int slot, int index);

template <typename T> static void CDTVStreamingIteratorTest_GetAssetPath_internal(char* out, T& store, int slot, const char* ext)
{
	const char* path = striskip(CDTVStreamingIteratorTest_GetRPFPathName(store, slot), "/$INVALID_FILE");
	const char* name = striskip(store.GetName(slot), "platform:/");

	if (path[0] != '\0')
	{
		strcat(out, striskip(path, "platform:")); // TODO -- not sure if we need to skip this
		if (strrchr(out, '.')) { strrchr(out, '.')[0] = '\0'; } // remove ".rpf" extension
	}
	else
	{
		if (((const void*)&store == &g_DwdStore      && stricmp(name, "models/plantsmgr"      ) == 0) ||
			((const void*)&store == &g_DwdStore      && stricmp(name, "models/skydome"        ) == 0) ||
			((const void*)&store == &g_DwdStore      && stricmp(name, "models/farlods"        ) == 0) ||
			((const void*)&store == &g_FragmentStore && stricmp(name, "models/z_z_fred"       ) == 0) ||
			((const void*)&store == &g_FragmentStore && stricmp(name, "models/z_z_fred_large" ) == 0) ||
			((const void*)&store == &g_FragmentStore && stricmp(name, "models/z_z_wilma"      ) == 0) ||
			((const void*)&store == &g_FragmentStore && stricmp(name, "models/z_z_wilma_large") == 0))
		{
			// these are in "/models" already, no need to warn
		}
		else if ((const void*)&store == &g_TxdStore && stristr(name, "textures/"))
		{
			// these are itd's in "/textures" already
		}
		else
		{
			strcat(out, "/$UNKNOWN_RPF");
		}
	}

	if (strstr(path, "componentpeds") ||
		strstr(path, "streamedpeds") ||
		strstr(path, "cutspeds"))
	{
		strcat(out, "/");
		strcat(out, name);

		if (strchr(name, '/')) // remove chars after '/', e.g. if name is "hc_driver/accs_000_u" we just want "hc_driver"
		{
			strrchr(out, '/')[0] = '\0';
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

	Assert(stristr(out, "platform:") == NULL); // TODO -- remove this check
}

template <> atString CDTVStreamingIteratorTest_GetAssetPath<fwTxdStore>(fwTxdStore& store, int slot, int)
{
	char path[1024] = "";
	strcpy(path, RS_UNZIPPED);

	if (slot != INDEX_NONE && store.IsValidSlot(slot))
	{
		CDTVStreamingIteratorTest_GetAssetPath_internal(path, store, slot, ".itd");
	}
	else
	{
		strcat(path, atVarString("/$INVALID_SLOT[%d]", slot).c_str());
	}

	strcat(path, "/");
	return atString(path);
}

atString CDTVStreamingIteratorTest_GetDwdEntryName(int slot, int index)
{
	const Dwd* dwd = NULL;

	if (index >= 0 && slot != INDEX_NONE && g_DwdStore.IsValidSlot(slot))
	{
		dwd = g_DwdStore.Get(slot);

		if (dwd && index < dwd->GetCount())
		{
			if (0) // try to get the name from the drawable .. this doesn't work
			{
				const Drawable* drawable = dwd->GetEntry(index);

				if (drawable)
				{
					char temp[256] = "";
					const char* name = drawable->GetDebugName(temp, sizeof(temp));

					if (name && strlen(name) > 4)
					{
						return atString(name);
					}
				}
			}

			if (1) // try to get the name from the archetype .. works for non-peds
			{
				const fwArchetype* a = fwArchetypeManager::GetArchetypeFromHashKey(dwd->GetCode(index), NULL);

				if (a)
				{
					return atString(a->GetModelName());
				}
			}

			if (1) // archetype might not have been streamed in .. try the debug archetype proxy
			{
				const CDebugArchetypeProxy* a = CDebugArchetype::GetDebugArchetypeProxyForHashKey(dwd->GetCode(index), NULL);

				if (a)
				{
					return atString(a->GetModelName());
				}
			}

			if (1) // try to get the name from the hash string .. this doesn't seem to work, but i'm desperate
			{
				const atHashString h(dwd->GetCode(index));
				const char* s = h.TryGetCStr();

				if (s)
				{
					// TODO -- i'm seeing cases where this function succeeds but there is still no archetype
					// for the dwd entry, so maybe it's getting the name from this codepath? so for now i'm
					// going to tag these with $DRAWABLE so i can catch them in the error report ..

					//return atString(s);
					return atVarString("$DRAWABLE_%d(%s)", index, s);
				}
			}
		}
	}

	return atVarString("$DRAWABLE_%d", index); // failed
}

template <> atString CDTVStreamingIteratorTest_GetAssetPath<fwDwdStore>(fwDwdStore& store, int slot, int index)
{
	char path[1024] = "";
	strcpy(path, RS_UNZIPPED);

	if (slot != INDEX_NONE && store.IsValidSlot(slot))
	{
		CDTVStreamingIteratorTest_GetAssetPath_internal(path, store, slot, ".idd");

		if (index != INDEX_NONE)
		{
			strcat(path, "/");
			strcat(path, CDTVStreamingIteratorTest_GetDwdEntryName(slot, index).c_str());
		}
	}
	else
	{
		strcat(path, atVarString("/$INVALID_SLOT[%d/%d]", slot, index).c_str());
	}

	strcat(path, "/");
	return atString(path);
}

template <> atString CDTVStreamingIteratorTest_GetAssetPath<fwDrawableStore>(fwDrawableStore& store, int slot, int)
{
	char path[1024] = "";
	strcpy(path, RS_UNZIPPED);

	if (slot != INDEX_NONE && store.IsValidSlot(slot))
	{
		CDTVStreamingIteratorTest_GetAssetPath_internal(path, store, slot, ".idr");
	}
	else
	{
		strcat(path, atVarString("/$INVALID_SLOT[%d]", slot).c_str());
	}

	strcat(path, "/");
	return atString(path);
}

template <> atString CDTVStreamingIteratorTest_GetAssetPath<fwFragmentStore>(fwFragmentStore& store, int slot, int index)
{
	char path[1024] = "";
	strcpy(path, RS_UNZIPPED);

	if (slot != INDEX_NONE && store.IsValidSlot(slot))
	{
		CDTVStreamingIteratorTest_GetAssetPath_internal(path, store, slot, ".ift");

		if (index != INDEX_NONE)
		{
			strcat(path, "/");

			const Fragment* fragment = store.Get(slot);
			const char* name = NULL;

			if (fragment && index < fragment->GetNumExtraDrawables())
			{
				name = fragment->GetExtraDrawableName(index);
			}

			if (name)
			{
				strcat(path, name);
			}
			else
			{
				strcat(path, atVarString("$DRAWABLE_%d", index).c_str()); // failed
			}
		}
	}
	else
	{
		strcat(path, atVarString("/$INVALID_SLOT[%d/%d]", slot, index).c_str());
	}

	strcat(path, "/");
	return atString(path);
}

template <> atString CDTVStreamingIteratorTest_GetAssetPath<ptfxAssetStore>(ptfxAssetStore& store, int slot, int index)
{
	char path[1024] = "";
	strcpy(path, RS_UNZIPPED);

	if (slot != INDEX_NONE && store.IsValidSlot(slot))
	{
		CDTVStreamingIteratorTest_GetAssetPath_internal(path, store, slot, ".ipt");

		if (index != INDEX_NONE)
		{
			strcat(path, "/");

			ptxFxList* ptx = store.Get(slot);
			Dwd* dwd = ptx->GetModelDictionary();
			const char* drawableName = NULL;

			if (ptx && dwd && index < dwd->GetCount())
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

								if (drawable == dwd->GetEntry(index))
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
				strcat(path, atVarString("models/$DRAWABLE_%d", index).c_str()); // failed
			}
		}
	}
	else
	{
		strcat(path, atVarString("/$INVALID_SLOT[%d/%d]", slot, index).c_str());
	}

	strcat(path, "/");
	return atString(path);
}

template <> atString CDTVStreamingIteratorTest_GetAssetPath<fwMapDataStore>(fwMapDataStore& store, int slot, int)
{
	char path[1024] = "";
	strcpy(path, RS_UNZIPPED);

	if (slot != INDEX_NONE && store.IsValidSlot(slot))
	{
		CDTVStreamingIteratorTest_GetAssetPath_internal(path, store, slot, "");
	}
	else
	{
		strcat(path, atVarString("/$INVALID_SLOT[%d]", slot).c_str());
	}

	strcat(path, "/");
	return atString(path);
}

template <typename T> int CDTVStreamingIteratorTest_FindSharedTxdSlot(T& store, int slot, const Drawable* drawable, const grcTexture*& texture, u32 textureKey)
{
	// first try drawable's txd
	{
		const fwTxd* txd = drawable->GetTexDictSafe();

		if (txd)
		{
			for (int i = 0; i < txd->GetCount(); i++)
			{
				if ((textureKey == 0 && txd->GetEntry(i) == texture) || (textureKey != 0 && txd->Lookup(textureKey)))
				{
					if (textureKey != 0)
					{
						texture = txd->Lookup(textureKey);
					}

					return INDEX_NONE;
				}
			}
		}
	}

	for (int parentTxdSlot = store.GetParentTxdForSlot(slot); parentTxdSlot != INDEX_NONE; parentTxdSlot = g_TxdStore.GetParentTxdSlot(parentTxdSlot))
	{
		const fwTxd* txd = g_TxdStore.Get(parentTxdSlot);

		if (txd)
		{
			for (int i = 0; i < txd->GetCount(); i++)
			{
				if ((textureKey == 0 && txd->GetEntry(i) == texture) || (textureKey != 0 && txd->Lookup(textureKey)))
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

	return -2; // not found
}

// not defined for particle assets
template <> int CDTVStreamingIteratorTest_FindSharedTxdSlot<ptfxAssetStore>(ptfxAssetStore& store, int slot, const Drawable* drawable, const grcTexture*& texture, u32 textureKey);

template <typename T> atString CDTVStreamingIteratorTest_FindTexturePath(T& store, int slot, int index, const Drawable* drawable, const grcTexture* texture, u32 textureKey, bool* bUsesNonPackedTextures)
{
	const int slotAndIndex = CDTVStreamingIteratorTest_FindSharedTxdSlot(store, slot, drawable, texture, textureKey);
	atString path = atString("");

	if (slotAndIndex == INDEX_NONE) // texture is in drawable's txd
	{
		path = CDTVStreamingIteratorTest_GetAssetPath(store, slot, index);
	}
	else if (slotAndIndex >= 0) // texture is in g_TxdStore
	{
		path = CDTVStreamingIteratorTest_GetAssetPath(g_TxdStore, slotAndIndex & 0x0000ffff, INDEX_NONE);

		if (bUsesNonPackedTextures)
		{
			*bUsesNonPackedTextures = true;
		}
	}
	else // ?
	{
		path = atVarString("%s/$UNKNOWN_TEXTURE/", RS_UNZIPPED);

		if (bUsesNonPackedTextures)
		{
			*bUsesNonPackedTextures = true;
		}
	}

	if (texture)
	{
		path += GetFriendlyTextureName(texture);
		path += ".dds";
	}

	return path;
}

template <> atString CDTVStreamingIteratorTest_FindTexturePath<ptfxAssetStore>(ptfxAssetStore& store, int slot, int index, const Drawable* drawable, const grcTexture* texture, u32 textureKey, bool* bUsesNonPackedTextures)
{
	bool bFound = false;
	atString path = atString("");

	if (drawable) // first try drawable's txd (it won't be in there, but we can try)
	{
		const fwTxd* txd = drawable->GetTexDictSafe();

		if (txd)
		{
			for (int i = 0; i < txd->GetCount(); i++)
			{
				if ((textureKey == 0 && txd->GetEntry(i) == texture) || (textureKey != 0 && txd->Lookup(textureKey)))
				{
					if (textureKey != 0)
					{
						texture = txd->Lookup(textureKey);
					}

					path = CDTVStreamingIteratorTest_GetAssetPath(store, slot, index);
					path += "textures/";
					bFound = true;
					break;
				}
			}
		}
	}

	if (!bFound) // try ptx's txd
	{
		const fwTxd* txd = store.Get(slot)->GetTextureDictionary();

		if (txd)
		{
			for (int i = 0; i < txd->GetCount(); i++)
			{
				if ((textureKey == 0 && txd->GetEntry(i) == texture) || (textureKey != 0 && txd->Lookup(textureKey)))
				{
					if (textureKey != 0)
					{
						texture = txd->Lookup(textureKey);
					}

					path = CDTVStreamingIteratorTest_GetAssetPath(store, slot, INDEX_NONE); // use INDEX_NONE so we won't append "models/" to the path
					path += "textures/";
					bFound = true;
					break;
				}
			}
		}
	}

	if (!bFound) // try ptx/core.ipt's txd
	{
		const int coreSlot = g_ParticleStore.FindSlotFromHashKey(atHashValue("core"));

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
						if ((textureKey == 0 && txd->GetEntry(i) == texture) || (textureKey != 0 && txd->Lookup(textureKey)))
						{
							if (bUsesNonPackedTextures)
							{
								*bUsesNonPackedTextures = true;
							}

							if (textureKey != 0)
							{
								texture = txd->Lookup(textureKey);
							}

							path = CDTVStreamingIteratorTest_GetAssetPath(store, coreSlot, INDEX_NONE); // use INDEX_NONE so we won't append "models/" to the path
							path += "textures/";
							bFound = true;
							break;
						}
					}
				}
			}
		}
	}

	if (!bFound) // ?
	{
		path = atVarString("%s/$UNKNOWN_TEXTURE/", RS_UNZIPPED);

		if (bUsesNonPackedTextures)
		{
			*bUsesNonPackedTextures = true;
		}
	}

	if (texture)
	{
		path += GetFriendlyTextureName(texture);
		path += ".dds";
	}

	return path;
}

atString CDTVStreamingIteratorTest_FindTexturePath(const CBaseModelInfo* model, const grcTexture* texture)
{
	const Drawable* drawable = model->GetDrawable();

	if (drawable)
	{
		switch ((int)model->GetDrawableType())
		{
		case fwArchetype::DT_FRAGMENT           : return CDTVStreamingIteratorTest_FindTexturePath(g_FragmentStore, model->GetFragmentIndex(), INDEX_NONE, drawable, texture, 0, NULL);
		case fwArchetype::DT_DRAWABLE           : return CDTVStreamingIteratorTest_FindTexturePath(g_DrawableStore, model->GetDrawableIndex(), INDEX_NONE, drawable, texture, 0, NULL);
		case fwArchetype::DT_DRAWABLEDICTIONARY : return CDTVStreamingIteratorTest_FindTexturePath(g_DwdStore     , model->GetDrawDictIndex(), model->GetDrawDictDrawableIndex(), drawable, texture, 0, NULL);
		}
	}

	return GetFriendlyTextureName(texture);
}

// ================================================================================================

static fiStream* gStreamingIteratorTest_LogFile = NULL;

static bool              g_siLogRecordErrors = false; // record log errors (currently this is only used for dumping water geometry)
static atArray<atString> g_siLogStrings;

void siLogOpen(const char* path)
{
	if (gStreamingIteratorTest_LogFile == NULL)
	{
		ASSET.PushFolder("assets:");
		gStreamingIteratorTest_LogFile = ASSET.Create(path, "log");
		ASSET.PopFolder();
	}
}

void siLog(const char* format, ...)
{
	char temp[4096] = "";
	va_list args;
	va_start(args, format);
	vsnprintf(temp, sizeof(temp), format, args);
	va_end(args);

	Displayf(temp);

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS_AUTO
	if (gStreamingIteratorTest_LogFile)
	{
		strcat(temp, "\n");
		gStreamingIteratorTest_LogFile->WriteByte(temp, istrlen(temp));
		gStreamingIteratorTest_LogFile->Flush();
	}
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS_AUTO

#if __PS3
	if (g_siLogRecordErrors && strstr(temp, "!ERROR:"))
	{
		g_siLogStrings.PushAndGrow(atString(temp));
	}
#endif // __PS3
}

// ================================================================================================

template <typename T> static void CDTVStreamingIteratorTest_CheckDrawableForAnimUVs(T& store, int slot, int index, const Drawable* drawable)
{
	if (drawable && drawable->GetShaderGroupPtr())
	{
		const atString desc = CDTVStreamingIteratorTest_GetAssetPath(store, slot, index);
		const grmShaderGroup* shaderGroup = drawable->GetShaderGroupPtr();

		for (int i = 0; i < shaderGroup->GetCount(); i++)
		{
			const grmShader* shader = shaderGroup->GetShaderPtr(i);

			const grcEffectVar shaderEffectVarUV0 = shader->LookupVar("globalAnimUV0", false);
			const grcEffectVar shaderEffectVarUV1 = shader->LookupVar("globalAnimUV1", false);

			if (shaderEffectVarUV0 != grcevNONE &&
				shaderEffectVarUV1 != grcevNONE)
			{
				const CDebugArchetypeProxy* modelInfo = CDTVStreamingIteratorTest_GetArchetypeProxyForDrawable(store, slot, index);

				if (modelInfo && modelInfo->GetHasAnimUV())
				{
					//Displayf("### %s using anim UVs with shader,\"%s\"", desc.c_str(), shader->GetName());
				}
			}
			else
			{
				Assert(shaderEffectVarUV0 == grcevNONE && shaderEffectVarUV1 == grcevNONE);
			}
		}
	}
}

template <typename T> static void CDTVStreamingIteratorTest_CheckAssetForUncompressedTextures(T& store, int slot, int index, const fwTxd* txd)
{
	if (txd)
	{
		for (int i = 0; i < txd->GetCount(); i++)
		{
			const grcTexture* texture = txd->GetEntry(i);

			if (texture && !grcImage::IsFormatDXTBlockCompressed((grcImage::Format)texture->GetImageFormat()))
			{
				if (strcmp(texture->GetName(), "cable") == 0 ||
					strstr(texture->GetName(), "hgdalphabet4096") != NULL ||
					strstr(texture->GetName(), "script_rt") != NULL)
				{
					// ignore these ..
				}
				else if (strstr(texture->GetName(), "_pal") != NULL && texture->GetWidth() == 256 && texture->GetHeight() <= 8)
				{
					// palette texture
				}
				else if (texture->GetWidth() == 256 && texture->GetHeight() == 1)
				{
					// scaleform palette texture
				}
				else
				{
					const atString assetPath = CDTVStreamingIteratorTest_GetAssetPath(store, slot, index);
					const atString textureName = GetFriendlyTextureName(texture);

					siLog(">> !ERROR: uncompressed texture %s%s,%s,%dx%d", assetPath.c_str(), textureName.c_str(), grcImage::GetFormatString((grcImage::Format)texture->GetImageFormat()), texture->GetWidth(), texture->GetHeight());
				}
			}
		}
	}
}

template <typename T> static void CDTVStreamingIteratorTest_CheckAssetForUncompressedTextures(T& store, int slot, int index, const Drawable* drawable)
{
	if (drawable)
	{
		CDTVStreamingIteratorTest_CheckAssetForUncompressedTextures(store, slot, index, drawable->GetTexDictSafe());
	}
}

// ugh wrapper ..
template <typename T> static bool IsTextureNameUsedByDrawable(T& store, int slot, int index, const Drawable* drawable, const char* textureName, atArray<const grcTexture*>* textureList = NULL);

template <> bool IsTextureNameUsedByDrawable<fwDwdStore     >(fwDwdStore     &, int slot, int index, const Drawable* drawable, const char* textureName, atArray<const grcTexture*>* textureList) { return IsTextureUsedByDrawable_Dwd     (slot, index, drawable, NULL, textureName, NULL, NULL, true, textureList); }
template <> bool IsTextureNameUsedByDrawable<fwDrawableStore>(fwDrawableStore&, int slot, int index, const Drawable* drawable, const char* textureName, atArray<const grcTexture*>* textureList) { return IsTextureUsedByDrawable_Drawable(slot, index, drawable, NULL, textureName, NULL, NULL, true, textureList); }
template <> bool IsTextureNameUsedByDrawable<fwFragmentStore>(fwFragmentStore&, int slot, int index, const Drawable* drawable, const char* textureName, atArray<const grcTexture*>* textureList) { return IsTextureUsedByDrawable_Fragment(slot, index, drawable, NULL, textureName, NULL, NULL, true, textureList); }

template <typename T> static void CDTVStreamingIteratorTest_CheckAssetForTexture(T& store, int slot, int index, const Drawable* drawable, const char* searchStr)
{
	if (drawable)
	{
		atArray<const grcTexture*> textureList;

		if (IsTextureNameUsedByDrawable(store, slot, index, drawable, searchStr, &textureList))
		{
			const atString desc = CDTVStreamingIteratorTest_GetAssetPath(store, slot, index);

			if (textureList.GetCount() == 1)
			{
				Displayf("### %s uses texture \"%s\":", desc.c_str(), CDTVStreamingIteratorTest_FindTexturePath(store, slot, index, drawable, textureList[0], 0, NULL).c_str());
			}
			else
			{
				Displayf("### %s uses textures:", desc.c_str());

				for (int i = 0; i < textureList.GetCount(); i++)
				{
					Displayf("  \"%s\"", CDTVStreamingIteratorTest_FindTexturePath(store, slot, index, drawable, textureList[i], 0, NULL).c_str());
				}
			}
		}
	}
}

template <> void CDTVStreamingIteratorTest_CheckAssetForTexture<fwTxdStore>(fwTxdStore& store, int slot, int, const Drawable*, const char* searchStr)
{
	if (!store.GetSlot(slot)->m_isDummy)
	{
		const fwTxd* txd = store.Get(slot);

		if (txd)
		{
			atArray<const grcTexture*> textureList;

			for (int i = 0; i < txd->GetCount(); i++)
			{
				const grcTexture* texture = txd->GetEntry(i);

				if (texture)
				{
					const atString textureName = GetFriendlyTextureName(texture);

					if (StringMatch(textureName.c_str(), searchStr))
					{
						textureList.PushAndGrow(texture);
					}
				}
			}

			if (textureList.GetCount() > 0)
			{
				const atString desc = CDTVStreamingIteratorTest_GetAssetPath(g_TxdStore, slot, INDEX_NONE);

				if (textureList.GetCount() == 1)
				{
					Displayf("### %s uses texture \"%s\":", desc.c_str(), GetFriendlyTextureName(textureList[0]).c_str());
				}
				else
				{
					Displayf("### %s uses textures:", desc.c_str());

					for (int i = 0; i < textureList.GetCount(); i++)
					{
						Displayf("  \"%s\"", GetFriendlyTextureName(textureList[i]).c_str());
					}
				}
			}
		}
	}
}

template <typename T> static void CDTVStreamingIteratorTest_CheckDrawableForShader(T& store, int slot, int index, const Drawable* drawable, const char* shaderName)
{
	if (drawable && drawable->GetShaderGroupPtr())
	{
		const atString desc = CDTVStreamingIteratorTest_GetAssetPath(store, slot, index);
		const grmShaderGroup* shaderGroup = drawable->GetShaderGroupPtr();

		for (int i = 0; i < shaderGroup->GetCount(); i++)
		{
			const grmShader& shader = shaderGroup->GetShader(i);

			if (StringMatch(shader.GetName(), shaderName))
			{
				Displayf("### %s uses shader \"%s\"", desc.c_str(), shader.GetName());
			}
			else // rare shaders
			{
				const char* rareShaderNames[] =
				{
					"default_um",
					"normal_spec_um",
					"normal_um",
					"spec_reflect_decal",
					"cutout_hard",
					"normal_spec_reflect_emissivenight",
					"normal_um_tnt",
					"normal_reflect_decal",
					"normal_spec_wrinkle",
					"vehicle_generic",
					"vehicle_track",
					"vehicle_detail",
					"vehicle_paint3_enveff",         
					"vehicle_rims2",
				};

				for (int j = 0; j < NELEM(rareShaderNames); j++)
				{
					if (strcmp(shader.GetName(), rareShaderNames[j]) == 0)
					{
						Displayf("### %s uses shader \"%s\"", desc.c_str(), shader.GetName());
						break;
					}
				}
			}
		}
	}
}

// ================================================================================================

#if DEBUG_TEXTURE_VIEWER_FIND_INEFFECTIVE_TEXTURES

template <typename DXT_BlockType> static void CDTVStreamingIteratorTest_FindIneffectiveTextures_GetRangeCompressed(u32 minRGBA[4], u32 maxRGBA[4], const grcTexture* texture, int mipIndex)
{
	grcTextureLock lock;
	texture->LockRect(0, mipIndex, lock, grcsRead | grcsAllowVRAMLock);

	const int w = (lock.Width  + 3)/4;
	const int h = (lock.Height + 3)/4;

	const DXT_BlockType* blocks = (const DXT_BlockType*)lock.Base;

	for (int y = 0; y < h; y++)
	{
		for (int x = 0; x < w; x++)
		{
			DXT::ARGB8888 blockPixels[4*4];
			blocks->Decompress(blockPixels);
			blocks++;

			for (int j = 0; j < 4*4; j++)
			{
				const DXT::ARGB8888 p = blockPixels[j];

				minRGBA[0] = Min<u32>(p.r, minRGBA[0]);
				minRGBA[1] = Min<u32>(p.g, minRGBA[1]);
				minRGBA[2] = Min<u32>(p.b, minRGBA[2]);
				minRGBA[3] = Min<u32>(p.a, minRGBA[3]);

				maxRGBA[0] = Max<u32>(p.r, maxRGBA[0]);
				maxRGBA[1] = Max<u32>(p.g, maxRGBA[1]);
				maxRGBA[2] = Max<u32>(p.b, maxRGBA[2]);
				maxRGBA[3] = Max<u32>(p.a, maxRGBA[3]);
			}                                      
		}
	}

	texture->UnlockRect(lock);
}

static void CDTVStreamingIteratorTest_FindIneffectiveTextures_GetRange(u32 minRGBA[4], u32 maxRGBA[4], const grcTexture* texture, int mipIndex)
{
	switch (texture->GetImageFormat())
	{
	case grcImage::DXT1: CDTVStreamingIteratorTest_FindIneffectiveTextures_GetRangeCompressed<DXT::DXT1_BLOCK>(minRGBA, maxRGBA, texture, mipIndex); break;
	case grcImage::DXT3: CDTVStreamingIteratorTest_FindIneffectiveTextures_GetRangeCompressed<DXT::DXT3_BLOCK>(minRGBA, maxRGBA, texture, mipIndex); break;
	case grcImage::DXT5: CDTVStreamingIteratorTest_FindIneffectiveTextures_GetRangeCompressed<DXT::DXT5_BLOCK>(minRGBA, maxRGBA, texture, mipIndex); break;
	case grcImage::A8R8G8B8:
		{
			grcTextureLock lock;
			texture->LockRect(0, mipIndex, lock, grcsRead | grcsAllowVRAMLock);

			const int w = lock.Width;
			const int h = lock.Height;

			const DXT::ARGB8888* blocks = (const DXT::ARGB8888*)lock.Base;

			for (int y = 0; y < h; y++)
			{
				for (int x = 0; x < w; x++)
				{
					const DXT::ARGB8888 p = *(blocks++);

					minRGBA[0] = Min<u32>(p.r, minRGBA[0]);
					minRGBA[1] = Min<u32>(p.g, minRGBA[1]);
					minRGBA[2] = Min<u32>(p.b, minRGBA[2]);
					minRGBA[3] = Min<u32>(p.a, minRGBA[3]);

					maxRGBA[0] = Max<u32>(p.r, maxRGBA[0]);
					maxRGBA[1] = Max<u32>(p.g, maxRGBA[1]);
					maxRGBA[2] = Max<u32>(p.b, maxRGBA[2]);
					maxRGBA[3] = Max<u32>(p.a, maxRGBA[3]);
				}
			}

			texture->UnlockRect(lock);
		}
		break;
	}
}

// more expensive version which swizzles internally and returns saturation
static u32 CDTVStreamingIteratorTest_FindIneffectiveTextures_GetSaturationRGB(u32 minRGBA[4], u32 maxRGBA[4], const grcTexture* texture, int mipIndex)
{
	DEBUG_TEXTURE_VIEWER_USE_DEBUG_MEMORY();

	u32 satRGB = 0;

	grcImage* image = rage_new grcImage();
	grcTextureLock lock;
	texture->LockRect(0, mipIndex, lock, grcsRead | grcsAllowVRAMLock);
	image->LoadAsTextureAlias(texture, &lock);

	for (int y = 0; (y + 3) < lock.Height; y += 4)
	{
		for (int x = 0; (x + 3) < lock.Width; x += 4)
		{
			Color32 block[4*4];
			image->GetPixelBlock(block, x, y);

			for (int yy = 0; yy < 4; yy++)
			{
				for (int xx = 0; xx < 4; xx++)
				{
					const Color32 colour = texture->Swizzle(block[xx + yy*4]);

					const int r = (int)colour.GetRed  ();
					const int g = (int)colour.GetGreen();
					const int b = (int)colour.GetBlue ();
					const int a = (int)colour.GetAlpha();

					minRGBA[0] = Min<u32>((u32)r, minRGBA[0]);
					minRGBA[1] = Min<u32>((u32)g, minRGBA[1]);
					minRGBA[2] = Min<u32>((u32)b, minRGBA[2]);
					minRGBA[3] = Min<u32>((u32)a, minRGBA[3]);

					maxRGBA[0] = Max<u32>((u32)r, maxRGBA[0]);
					maxRGBA[1] = Max<u32>((u32)g, maxRGBA[1]);
					maxRGBA[2] = Max<u32>((u32)b, maxRGBA[2]);
					maxRGBA[3] = Max<u32>((u32)a, maxRGBA[3]);

					satRGB = Max<u32>((u32)Abs<int>(r - g), satRGB);
					satRGB = Max<u32>((u32)Abs<int>(g - b), satRGB);
					satRGB = Max<u32>((u32)Abs<int>(b - r), satRGB);
				}
			}
		}
	}

	image->ReleaseAlias();
	texture->UnlockRect(lock);

	return satRGB;
}

template <typename T> static void CDTVStreamingIteratorTest_FindIneffectiveTextures(T& store, int slot, int index, const Drawable* drawable)
{
	const float varThreshold = 0.05f; // [0..1] threshold for shader vars
	const int   texThreshold = 5; // [0..255] threshold for texture data

	if (drawable && drawable->GetShaderGroupPtr())
	{
		const atString desc = CDTVStreamingIteratorTest_GetAssetPath(store, slot, index);
		const grmShaderGroup* shaderGroup = drawable->GetShaderGroupPtr();

		for (int i = 0; i < shaderGroup->GetCount(); i++)
		{
			const grmShader* shader = shaderGroup->GetShaderPtr(i);

			grcTexture* bumpTex = NULL;
			u32         bumpTexMaxDiff = 9999;
			float       bumpiness = 1.0f;

			grcTexture* specularTex = NULL;
			u32         specularTexMaxValue = 9999;
			float       specIntensity = 1.0f;
			Vector3     specIntensityMask(1.0f, 0.0f, 0.0f);

			for (int j = 0; j < shader->GetVarCount(); j++)
			{
				const grcEffectVar var = shader->GetVarByIndex(j);
				const char* name = NULL;
				grcEffect::VarType type;
				int annotationCount = 0;
				bool isGlobal = false;

				shader->GetVarDesc(var, name, type, annotationCount, isGlobal);

				if (!isGlobal)
				{
					if      (type == grcEffect::VT_TEXTURE && stricmp(name, "BumpTex"                 ) == 0) { shader->GetVar(var, bumpTex          ); }
					else if (type == grcEffect::VT_FLOAT   && stricmp(name, "Bumpiness"               ) == 0) { shader->GetVar(var, bumpiness        ); }
					else if (type == grcEffect::VT_TEXTURE && stricmp(name, "SpecularTex"             ) == 0) { shader->GetVar(var, specularTex      ); }
					else if (type == grcEffect::VT_FLOAT   && stricmp(name, "SpecularColor"           ) == 0) { shader->GetVar(var, specIntensity    ); }
					else if (type == grcEffect::VT_VECTOR3 && stricmp(name, "SpecularMapIntensityMask") == 0) { shader->GetVar(var, specIntensityMask); }
				}
			}

			if (dynamic_cast<grcRenderTarget*>(bumpTex    )) { bumpTex     = NULL; } // we don't want rendertargets ..
			if (dynamic_cast<grcRenderTarget*>(specularTex)) { specularTex = NULL; }

			if (gStreamingIteratorTest_FindIneffectiveTexturesCheckFlat)
			{
				if (bumpTex && gStreamingIteratorTest_FindIneffectiveTexturesBumpTex)
				{
					if (Max<int>(bumpTex->GetWidth(), bumpTex->GetHeight()) <= 4 || gStreamingIteratorTest_FindIneffectiveTexturesCheckFlatGT4x4) // assume flat textures have been replaced by 4x4's
					{
						grcTexture::eTextureSwizzle swizzle[4];
						bumpTex->GetTextureSwizzle(swizzle[0], swizzle[1], swizzle[2], swizzle[3]);

						u32 minRGBA01[6] = {255,255,255,255,0,255};
						u32 maxRGBA01[6] = {0,0,0,0        ,0,255};
						CDTVStreamingIteratorTest_FindIneffectiveTextures_GetRange(minRGBA01, maxRGBA01, bumpTex, 0);

						CompileTimeAssert(grcTexture::TEXTURE_SWIZZLE_R == 0);
						CompileTimeAssert(grcTexture::TEXTURE_SWIZZLE_G == 1);
						CompileTimeAssert(grcTexture::TEXTURE_SWIZZLE_B == 2);
						CompileTimeAssert(grcTexture::TEXTURE_SWIZZLE_A == 3);
						CompileTimeAssert(grcTexture::TEXTURE_SWIZZLE_0 == 4);
						CompileTimeAssert(grcTexture::TEXTURE_SWIZZLE_1 == 5);

						const u32 minX = minRGBA01[swizzle[0]];
						const u32 minY = minRGBA01[swizzle[1]];

						const u32 maxX = maxRGBA01[swizzle[0]];
						const u32 maxY = maxRGBA01[swizzle[1]];

						bumpTexMaxDiff = Max<u32>(maxX - minX, maxY - minY); // not really "flat", but constant slope .. i consider this flat
					}
				}

				if (specularTex && gStreamingIteratorTest_FindIneffectiveTexturesSpecularTex)
				{
					if (Max<int>(specularTex->GetWidth(), specularTex->GetHeight()) <= 4 || gStreamingIteratorTest_FindIneffectiveTexturesCheckFlatGT4x4) // assume flat textures have been replaced by 4x4's
					{
						grcTexture::eTextureSwizzle swizzle[4];
						specularTex->GetTextureSwizzle(swizzle[0], swizzle[1], swizzle[2], swizzle[3]);

						u32 minRGBA01[6] = {255,255,255,255,0,255};
						u32 maxRGBA01[6] = {0,0,0,0        ,0,255};
						CDTVStreamingIteratorTest_FindIneffectiveTextures_GetRange(minRGBA01, maxRGBA01, specularTex, 0);

						CompileTimeAssert(grcTexture::TEXTURE_SWIZZLE_R == 0);
						CompileTimeAssert(grcTexture::TEXTURE_SWIZZLE_G == 1);
						CompileTimeAssert(grcTexture::TEXTURE_SWIZZLE_B == 2);
						CompileTimeAssert(grcTexture::TEXTURE_SWIZZLE_A == 3);
						CompileTimeAssert(grcTexture::TEXTURE_SWIZZLE_0 == 4);
						CompileTimeAssert(grcTexture::TEXTURE_SWIZZLE_1 == 5);

						const u32 maxR = maxRGBA01[swizzle[0]];
						const u32 maxG = maxRGBA01[swizzle[1]];
						const u32 maxB = maxRGBA01[swizzle[2]];

						specularTexMaxValue = 0;

						if (specIntensity*specIntensityMask.x > varThreshold) { specularTexMaxValue = Max<u32>(maxR, specularTexMaxValue); }
						if (specIntensity*specIntensityMask.y > varThreshold) { specularTexMaxValue = Max<u32>(maxG, specularTexMaxValue); }
						if (specIntensity*specIntensityMask.z > varThreshold) { specularTexMaxValue = Max<u32>(maxB, specularTexMaxValue); }
					}
				}
			}

			bool bReportBumpTex = false;
			bool bReportSpecularTex = false;

			if (gStreamingIteratorTest_FindIneffectiveTexturesBumpTex)
			{
				if (bumpTex && (bumpTexMaxDiff <= texThreshold || bumpiness <= varThreshold))
				{
					bReportBumpTex = true;
				}
			}

			if (gStreamingIteratorTest_FindIneffectiveTexturesSpecularTex)
			{
				if (specularTex && (specularTexMaxValue <= texThreshold || specIntensity <= varThreshold || specIntensity*MaxElement(RCC_VEC3V(specIntensityMask)).Getf() <= varThreshold))
				{
					bReportSpecularTex = true;
				}
			}

			if (gStreamingIteratorTest_FindNonDefaultSpecularMasks)
			{
				if (MaxElement(Abs(RCC_VEC3V(specIntensityMask) - Vec3V(V_X_AXIS_WZERO))).Getf() > 0.0f)
				{
					bReportSpecularTex = true; // report even if there is no specular texture
				}
			}

			if (bReportBumpTex)
			{
				if (bumpTex)
				{
					Displayf(
						"BumpTex, %s, shadergroup=%d, %s, crc=0x%08X, %s (%dx%d mips=%d)%s, maxDiff=%03d, , used by shader \"%s\", Bumpiness=%f",
						desc.c_str(),
						i,
						CDTVStreamingIteratorTest_FindTexturePath(store, slot, index, drawable, bumpTex, 0, NULL).c_str(),
						GetTextureHash(bumpTex, false, -1),
						grcImage::GetFormatString((grcImage::Format)bumpTex->GetImageFormat()),
						bumpTex->GetWidth(),
						bumpTex->GetHeight(),
						bumpTex->GetMipMapCount(),
						IsTextureConstant(bumpTex) ? " - no variation" : "",
						bumpTexMaxDiff,
						shader->GetName(),
						bumpiness
					);
				}
				else // no bump texture?
				{
					Displayf(
						"BumpTex, %s, shadergroup=%d, NULL, , , , , used by shader \"%s\", Bumpiness=%f",
						desc.c_str(),
						i,
						shader->GetName(),
						bumpiness
					);
				}
			}

			if (bReportSpecularTex)
			{
				if (specularTex)
				{
					u32 minRGBA[4] = {255,255,255,255};
					u32 maxRGBA[4] = {0,0,0,0};

					const u32 satRGB = CDTVStreamingIteratorTest_FindIneffectiveTextures_GetSaturationRGB(minRGBA, maxRGBA, specularTex, 0);
					const u32 minRGB = Min<u32>(minRGBA[0], minRGBA[1], minRGBA[2]);
					const u32 maxRGB = Max<u32>(maxRGBA[0], maxRGBA[1], maxRGBA[2]);

					const u32 maxRGBDiff = Max<u32>(maxRGBA[0] - minRGBA[0], maxRGBA[1] - minRGBA[1], maxRGBA[2] - minRGBA[2]);

					Displayf(
						"SpecularTex, %s, shadergroup=%d, %s, crc=0x%08X, %s (%dx%d mips=%d), range=[%d..%d], sat=%03d (%.2f%%), used by shader \"%s\", SpecularColor=%f, SpecularMapIntensityMask=[%.3f %.3f %.3f]",
						desc.c_str(),
						i,
						CDTVStreamingIteratorTest_FindTexturePath(store, slot, index, drawable, specularTex, 0, NULL).c_str(),
						GetTextureHash(specularTex, false, -1),
						grcImage::GetFormatString((grcImage::Format)specularTex->GetImageFormat()),
						specularTex->GetWidth(),
						specularTex->GetHeight(),
						specularTex->GetMipMapCount(),
						minRGB,
						maxRGB,
						satRGB,
						maxRGBDiff > 0 ? 100.0f*(float)satRGB/(float)maxRGBDiff : 0.0f,
						shader->GetName(),
						specIntensity,
						specIntensityMask.x,
						specIntensityMask.y,
						specIntensityMask.z
					);
				}
				else // no specular texture?
				{
					Displayf(
						"SpecularTex, %s, shadergroup=%d, NULL, , , , , used by shader \"%s\", SpecularColor=%f, SpecularMapIntensityMask=[%.3f %.3f %.3f]",
						desc.c_str(),
						i,
						shader->GetName(),
						specIntensity,
						specIntensityMask.x,
						specIntensityMask.y,
						specIntensityMask.z
					);
				}
			}
		}
	}
}

#endif // DEBUG_TEXTURE_VIEWER_FIND_INEFFECTIVE_TEXTURES

// ================================================================================================

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS

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

__forceinline void crc32(unsigned char data, u32& crc)
{
	crc32_init();
	crc = g_crc32_table[(crc ^ data) & 0xff] ^ (crc >> 8);
}

// hack: non-inline version so we can extern it from VehicleGlass
void crc32_notinline(unsigned char data, u32& crc)
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
			crc32((unsigned char)tolower(*(str++)), x);
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

__forceinline void crc64(unsigned char data, u64& crc)
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
			crc64((unsigned char)tolower(*(str++)), x);
		}
	}

	return x;
}

static int                      gStreamingIteratorTest_BuildTextureUsageMapDATFileEntryIndex = 0;
static fiStream*                gStreamingIteratorTest_BuildTextureUsageMapDATFile = NULL;
static fiStream*                gStreamingIteratorTest_BuildTextureUsageMapCSVFile = NULL; // if this works well, consider using it for all streaming iterator codepaths
static atMap<u32,u8>            gStreamingIteratorTest_BuildTextureUsageMapFlags; // built during phase 0
static atMap<atString,atString> gStreamingIteratorTest_BuildTextureUsageMapDiffuseTex;
static atMap<atString,atString> gStreamingIteratorTest_BuildTextureUsageMapBumpTex;
static atMap<atString,atString> gStreamingIteratorTest_BuildTextureUsageMapSpecularTex;

void CDTVStreamingIteratorTest_BuildTextureUsageMapsReset()
{
	Displayf("CDTVStreamingIteratorTest_BuildTextureUsageMapsReset");

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS_AUTO
	if (gStreamingIteratorTest_LogFile)
	{
		gStreamingIteratorTest_LogFile->Close();
		gStreamingIteratorTest_LogFile = NULL;
	}
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS_AUTO

	if (gStreamingIteratorTest_BuildTextureUsageMapDATFile)
	{
		gStreamingIteratorTest_BuildTextureUsageMapDATFile->WriteInt(&gStreamingIteratorTest_BuildTextureUsageMapDATFileEntryIndex, 1);
		gStreamingIteratorTest_BuildTextureUsageMapDATFileEntryIndex = 0;
		gStreamingIteratorTest_BuildTextureUsageMapDATFile->Close();
		gStreamingIteratorTest_BuildTextureUsageMapDATFile = NULL;

		// signal to external program that the file is done by creating a proxy file
		{
			ASSET.PushFolder("assets:");
			fiStream* proxy = ASSET.Create("texture_usage", "proxy");
			ASSET.PopFolder();

			if (proxy)
			{
				proxy->Close();
			}
		}
	}

	if (gStreamingIteratorTest_BuildTextureUsageMapCSVFile)
	{
		gStreamingIteratorTest_BuildTextureUsageMapCSVFile->Close();
		gStreamingIteratorTest_BuildTextureUsageMapCSVFile = NULL;
	}

	gStreamingIteratorTest_BuildTextureUsageMapFlags      .Kill();
	gStreamingIteratorTest_BuildTextureUsageMapDiffuseTex .Kill();
	gStreamingIteratorTest_BuildTextureUsageMapBumpTex    .Kill();
	gStreamingIteratorTest_BuildTextureUsageMapSpecularTex.Kill();
}

static bool CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsDiffuseTex(const char* name)
{
	if (stristr(name, "DiffuseTex"    ) ||
		stristr(name, "DirtTex"       ) ||
		stristr(name, "SnowTex"       ) ||
		stristr(name, "DensityTex"    ) ||
		stristr(name, "Density2Tex"   ) ||
		stristr(name, "WrinkleMaskTex") ||
		stristr(name, "PlateBgTex"    ) ||
		stristr(name, "StubbleTex"    ) ||
		stristr(name, "EnvironmentTex")) // treat this as diffuse for now ..
	{
		return true;
	}

	return false;
}

static bool CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsBumpTex(const char* name)
{
	if (stristr(name, "BumpTex"       ) ||
		stristr(name, "DetailTex"     ) ||
		stristr(name, "NormalTex"     ) ||
		stristr(name, "Normal2Tex"    ) || // e.g. DetailNormal2Texture
		stristr(name, "NormalMap"     ) || // e.g. FontNormalMap
		stristr(name, "WrinkleTex"    ) ||
		stristr(name, "MirrorCrackTex"))
	{
		return true;
	}

	return false;
}

static bool CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsSpecularTex(const char* name)
{
	if (stristr(name, "SpecularTex"))
	{
		return true;
	}

	return false;
}

static bool CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsDistanceTex(const char* name)
{
	if (stricmp(name, "DistanceMap") == 0 ||
		stricmp(name, "FontTexture") == 0)
	{
		return true;
	}

	return false;
}

static bool CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsWaterTex(const char* name)
{
	if (stricmp(name, "FoamTexture" ) == 0 || // FoamOpacityMap.tcp
		stricmp(name, "FoamAnimTex" ) == 0 || // WaterFoamMap.tcp
		stricmp(name, "RiverFoamTex") == 0)   // WaterFogFoamMap.tcp
	{
		return true;
	}

	return false;
}

static bool CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsWaterFlowTex(const char* name)
{
	if (stricmp(name, "FlowTexture") == 0)
	{
		return true;
	}

	return false;
}

static bool CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsCableTex(const char* name)
{
	if (stricmp(name, "texture") == 0) // ack, should be "CableTex"
	{
		return true;
	}

	return false;
}

static bool CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsTintPalette(const char* name)
{
	if (stricmp(name, "TintPaletteTex") == 0)
	{
		return true;
	}

	return false;
}

static bool CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsLookupTex(const char* name)
{
	if (stricmp(name, "lookupTexture") == 0)
	{
		return true;
	}

	return false;
}

#define SAVE_STRINGS_IN_TEXTUREUSAGE (0) // naw, i'm rewriting the system ..

#if SAVE_STRINGS_IN_TEXTUREUSAGE
static void CTextureUsage_WriteString(fiStream* stream, const char* str)
{
	bool bSkipUnzipped = false;

	if (str && strstr(str, RS_UNZIPPED) == str)
	{
		str += istrlen(RS_UNZIPPED) + 1; // skip past RS_UNZIPPED and first '/'
		bSkipUnzipped = true;
	}

	const u16 length = str ? (u16)istrlen(str) : 0;

	stream->WriteShort(&length, 1);

	if (str)
	{
		if (bSkipUnzipped)
		{
			const char ch = '@'; // first chars "@" is a placeholder for RS_UNZIPPED + '/'
			stream->WriteByte(&ch, 1);
		}

		stream->WriteByte(str, length + 1); // includes '\0' terminator
	}
}
#endif // SAVE_STRINGS_IN_TEXTUREUSAGE

class CTextureUsage
{
public:
	CTextureUsage(
		const char* textureNameStr,
		const char* texturePathStr,
		const char* drawablePathStr,
		const char* textureUsageStr,
		char        textureUsage,
		char        drawableStoreType,
		char        drawableShaderId,
		u8          flags,
		Vec4V_In    scale,
		const char* shaderNameStr)
	{
		const u64 textureNameHash  = crc64_lower(textureNameStr);
		const u64 texturePathHash  = crc64_lower(texturePathStr);
		const u64 drawablePathHash = crc64_lower(drawablePathStr);
		const u32 textureUsageHash = crc32_lower(textureUsageStr);
		const u32 shaderNameHash   = crc32_lower(shaderNameStr);

		m_tag                 = 'TXSU';
		m_textureNameHash[0]  = (u32)(textureNameHash);
		m_textureNameHash[1]  = (u32)(textureNameHash >> 32);
		m_texturePathHash[0]  = (u32)(texturePathHash);
		m_texturePathHash[1]  = (u32)(texturePathHash >> 32);
		m_drawablePathHash[0] = (u32)(drawablePathHash);
		m_drawablePathHash[1] = (u32)(drawablePathHash >> 32);
		m_textureUsageHash    = textureUsageHash;
		m_textureUsage        = textureUsage;
		m_drawableStoreType   = drawableStoreType;
		m_drawableShaderId    = drawableShaderId;
		m_flags               = flags;
		m_scale[0]            = scale.GetXf();
		m_scale[1]            = scale.GetYf();
		m_scale[2]            = scale.GetZf();
		m_scale[3]            = scale.GetWf();
		m_shaderNameHash      = shaderNameHash;
		m_unused[0]           = 0;
		m_unused[1]           = 0;
		m_unused[2]           = 0;
		m_unused[3]           = 0;
#if SAVE_STRINGS_IN_TEXTUREUSAGE
		m_textureNameStr      = textureNameStr;
		m_texturePathStr      = texturePathStr;
		m_drawablePathStr     = drawablePathStr;
		m_textureUsageStr     = textureUsageStr;
		m_shaderNameStr       = shaderNameStr;
#endif // SAVE_STRINGS_IN_TEXTUREUSAGE
	}

	static fiStream* GetFileStream()
	{
		fiStream* stream = NULL;

		if (gStreamingIteratorTest_BuildTextureUsageMapDATOutput)
		{
			stream = gStreamingIteratorTest_BuildTextureUsageMapDATFile;

			if (stream == NULL)
			{
				ASSET.PushFolder("assets:");
				stream = ASSET.Create("texture_usage", "dat");
				ASSET.PopFolder();

				if (stream)
				{
					gStreamingIteratorTest_BuildTextureUsageMapDATFile = stream;
				}
				else
				{
					gStreamingIteratorTest_BuildTextureUsageMapDATOutput = false; // turn it off
				}
			}
		}

		return stream;
	}

	void Write() const
	{
		fiStream* stream = GetFileStream();

		if (stream)
		{
			stream->WriteInt(&m_tag, 1);
			stream->WriteInt(&m_textureNameHash[0], 2);
			stream->WriteInt(&m_texturePathHash[0], 2);
			stream->WriteInt(&m_drawablePathHash[0], 2);
			stream->WriteInt(&m_textureUsageHash, 1);
			stream->WriteByte(&m_textureUsage, 1);
			stream->WriteByte(&m_drawableStoreType, 1);
			stream->WriteByte(&m_drawableShaderId, 1);
			stream->WriteByte(&m_flags, 1);
			stream->WriteInt((const int*)&m_scale, 4);
			stream->WriteInt(&m_shaderNameHash, 1);
			stream->WriteByte(&m_unused[0], 4);
#if SAVE_STRINGS_IN_TEXTUREUSAGE
			CTextureUsage_WriteString(stream, m_textureNameStr);
			CTextureUsage_WriteString(stream, m_texturePathStr);
			CTextureUsage_WriteString(stream, m_drawablePathStr);
			CTextureUsage_WriteString(stream, m_textureUsageStr);
			CTextureUsage_WriteString(stream, m_shaderNameStr);
#endif // SAVE_STRINGS_IN_TEXTUREUSAGE

			if (gStreamingIteratorTest_BuildTextureUsageMapDATFlush)
			{
				stream->Flush();
			}

			gStreamingIteratorTest_BuildTextureUsageMapDATFileEntryIndex++;
		}
	}

	int   m_tag;
	u32   m_textureNameHash[2];
	u32   m_texturePathHash[2];
	u32   m_drawablePathHash[2];
	u32   m_textureUsageHash;
	char  m_textureUsage;
	char  m_drawableStoreType;
	char  m_drawableShaderId; // index into shadergroup shaders, not used currently
	u8    m_flags;
	float m_scale[4]; // bumpiness, or specular intensity + mask
	u32   m_shaderNameHash;
	char  m_unused[4];
#if SAVE_STRINGS_IN_TEXTUREUSAGE
	const char* m_textureNameStr;
	const char* m_texturePathStr;
	const char* m_drawablePathStr;
	const char* m_textureUsageStr;
	const char* m_shaderNameStr;
#endif // SAVE_STRINGS_IN_TEXTUREUSAGE
};

class CTextureUsage_Shader
{
public:
	CTextureUsage_Shader(
		const char* drawablePathStr,
		char        drawableStoreType,
		char        drawableShaderId,
		u8          flags,
		u8          drawBucket,
		const char* shaderNameStr,
		const char* diffTexturePathStr,
		const char* bumpTexturePathStr,
		const char* specTexturePathStr)
	{
		const u64 drawablePathHash    = crc64_lower(drawablePathStr);
		const u32 shaderNameHash      = crc32_lower(shaderNameStr);
		const u64 diffTexturePathHash = crc64_lower(diffTexturePathStr);
		const u64 bumpTexturePathHash = crc64_lower(bumpTexturePathStr);
		const u64 specTexturePathHash = crc64_lower(specTexturePathStr);

		m_tag                       = 'SHAD';
		m_drawablePathHash[0]       = (u32)(drawablePathHash);
		m_drawablePathHash[1]       = (u32)(drawablePathHash >> 32);
		m_drawableStoreType         = drawableStoreType;
		m_drawableShaderId          = drawableShaderId;
		m_flags                     = flags;
		m_drawBucket                = drawBucket;
		m_hasBeenCounted_DO_NOT_USE = 0;
		m_unused[0]                 = 0;
		m_unused[1]                 = 0;
		m_unused[2]                 = 0;
		m_unused[3]                 = 0;
		m_unused[4]                 = 0;
		m_unused[5]                 = 0;
		m_unused[6]                 = 0;
		m_shaderNameHash            = shaderNameHash;
		m_diffTexturePathHash[0]    = (u32)(diffTexturePathHash);
		m_diffTexturePathHash[1]    = (u32)(diffTexturePathHash >> 32);
		m_bumpTexturePathHash[0]    = (u32)(bumpTexturePathHash);
		m_bumpTexturePathHash[1]    = (u32)(bumpTexturePathHash >> 32);
		m_specTexturePathHash[0]    = (u32)(specTexturePathHash);
		m_specTexturePathHash[1]    = (u32)(specTexturePathHash >> 32);
#if SAVE_STRINGS_IN_TEXTUREUSAGE
		m_drawablePathStr           = drawablePathStr;
		m_shaderNameStr             = shaderNameStr;
		m_diffTexturePathStr        = diffTexturePathStr;
		m_bumpTexturePathStr        = bumpTexturePathStr;
		m_specTexturePathStr        = specTexturePathStr;
#endif // SAVE_STRINGS_IN_TEXTUREUSAGE
	}

	void Write() const
	{
		fiStream* stream = CTextureUsage::GetFileStream();

		if (stream)
		{
			stream->WriteInt(&m_tag, 1);
			stream->WriteInt(&m_drawablePathHash[0], 2);
			stream->WriteByte(&m_drawableStoreType, 1);
			stream->WriteByte(&m_drawableShaderId, 1);
			stream->WriteByte(&m_flags, 1);
			stream->WriteByte(&m_drawBucket, 1);
			stream->WriteByte(&m_hasBeenCounted_DO_NOT_USE, 1);
			stream->WriteByte(&m_unused[0], 7);
			stream->WriteInt(&m_shaderNameHash, 1);
			stream->WriteInt(&m_diffTexturePathHash[0], 2);
			stream->WriteInt(&m_bumpTexturePathHash[0], 2);
			stream->WriteInt(&m_specTexturePathHash[0], 2);
#if SAVE_STRINGS_IN_TEXTUREUSAGE
			CTextureUsage_WriteString(stream, m_drawablePathStr);
			CTextureUsage_WriteString(stream, m_shaderNameStr);
			CTextureUsage_WriteString(stream, m_diffTexturePathStr);
			CTextureUsage_WriteString(stream, m_bumpTexturePathStr);
			CTextureUsage_WriteString(stream, m_specTexturePathStr);
#endif // SAVE_STRINGS_IN_TEXTUREUSAGE

			if (gStreamingIteratorTest_BuildTextureUsageMapDATFlush)
			{
				stream->Flush();
			}

			gStreamingIteratorTest_BuildTextureUsageMapDATFileEntryIndex++;
		}
	}

	int  m_tag;
	u32  m_drawablePathHash[2];
	char m_drawableStoreType;
	char m_drawableShaderId; // index into shadergroup shaders, not used currently
	u8   m_flags;
	u8   m_drawBucket;
	char m_hasBeenCounted_DO_NOT_USE; // used internally by texture analysis, set to zero
	char m_unused[7];
	u32  m_shaderNameHash;
	u32  m_diffTexturePathHash[2];
	u32  m_bumpTexturePathHash[2];
	u32  m_specTexturePathHash[2];
#if SAVE_STRINGS_IN_TEXTUREUSAGE
	const char* m_drawablePathStr;
	const char* m_shaderNameStr;
	const char* m_diffTexturePathStr;
	const char* m_bumpTexturePathStr;
	const char* m_specTexturePathStr;
#endif // SAVE_STRINGS_IN_TEXTUREUSAGE
};

class CTextureUsage_Texture
{
public:
	CTextureUsage_Texture(
		const grcTexture* texture,
		const char*       textureNameStr,
		const char*       texturePathStr,
		const char*       drawablePathStr,
		char              drawableStoreType,
		u8                flags)
	{
		const u64 textureNameHash  = crc64_lower(textureNameStr);
		const u64 texturePathHash  = crc64_lower(texturePathStr);
		const u64 drawablePathHash = crc64_lower(drawablePathStr);

		m_tag                 = 'TXTR';
		m_textureNameHash[0]  = (u32)(textureNameHash);
		m_textureNameHash[1]  = (u32)(textureNameHash >> 32);
		m_texturePathHash[0]  = (u32)(texturePathHash);
		m_texturePathHash[1]  = (u32)(texturePathHash >> 32);
		m_drawablePathHash[0] = (u32)(drawablePathHash);
		m_drawablePathHash[1] = (u32)(drawablePathHash >> 32);
		m_drawableStoreType   = drawableStoreType;
		m_flags               = flags;
		m_textureWidth        = 0;
		m_textureHeight       = 0;
		m_textureMips         = 0;
		m_textureFormat       = grcImage::UNKNOWN;
		m_textureSwizzle[0]   = 'x';
		m_textureSwizzle[1]   = 'x';
		m_textureSwizzle[2]   = 'x';
		m_textureSwizzle[3]   = 'x';
		m_texturePixelHash    = 0;
#if SAVE_STRINGS_IN_TEXTUREUSAGE
		m_textureNameStr      = textureNameStr;
		m_texturePathStr      = texturePathStr;
		m_drawablePathStr     = drawablePathStr;
#endif // SAVE_STRINGS_IN_TEXTUREUSAGE

		if (texture)
		{
			m_textureWidth  = (u16)texture->GetWidth();
			m_textureHeight = (u16)texture->GetHeight();
			m_textureMips   = (u8)texture->GetMipMapCount();
			m_textureFormat = (u8)texture->GetImageFormat();

			grcTexture::eTextureSwizzle swizzle[4];
			texture->GetTextureSwizzle(swizzle[0], swizzle[1], swizzle[2], swizzle[3]);

			for (int i = 0; i < 4; i++)
			{
				switch (swizzle[i])
				{
				case grcTexture::TEXTURE_SWIZZLE_R: m_textureSwizzle[i] = 'R'; break;
				case grcTexture::TEXTURE_SWIZZLE_G: m_textureSwizzle[i] = 'G'; break;
				case grcTexture::TEXTURE_SWIZZLE_B: m_textureSwizzle[i] = 'B'; break;
				case grcTexture::TEXTURE_SWIZZLE_A: m_textureSwizzle[i] = 'A'; break;
				case grcTexture::TEXTURE_SWIZZLE_0: m_textureSwizzle[i] = '0'; break;
				case grcTexture::TEXTURE_SWIZZLE_1: m_textureSwizzle[i] = '1'; break;
				}
			}

			m_texturePixelHash = GetTextureHash(texture, false, 16); // hash up to 16 lines of top mip, along with w/h/format/mipcount
		}
	}

	void Write() const
	{
		fiStream* stream = CTextureUsage::GetFileStream();

		if (stream)
		{
			stream->WriteInt(&m_tag, 1);
			stream->WriteInt(&m_textureNameHash[0], 2);
			stream->WriteInt(&m_texturePathHash[0], 2);
			stream->WriteInt(&m_drawablePathHash[0], 2);
			stream->WriteByte(&m_drawableStoreType, 1);
			stream->WriteByte(&m_flags, 1);
			stream->WriteShort(&m_textureWidth, 1);
			stream->WriteShort(&m_textureHeight, 1);
			stream->WriteByte(&m_textureMips, 1);
			stream->WriteByte(&m_textureFormat, 1);
			stream->WriteByte(&m_textureSwizzle[0], 4);
			stream->WriteInt(&m_texturePixelHash, 1);
#if SAVE_STRINGS_IN_TEXTUREUSAGE
			CTextureUsage_WriteString(stream, m_textureNameStr);
			CTextureUsage_WriteString(stream, m_texturePathStr);
			CTextureUsage_WriteString(stream, m_drawablePathStr);
#endif // SAVE_STRINGS_IN_TEXTUREUSAGE

			if (gStreamingIteratorTest_BuildTextureUsageMapDATFlush)
			{
				stream->Flush();
			}

			gStreamingIteratorTest_BuildTextureUsageMapDATFileEntryIndex++;
		}
	}

	int  m_tag;
	u32  m_textureNameHash[2];
	u32  m_texturePathHash[2];
	u32  m_drawablePathHash[2];
	char m_drawableStoreType;
	u8   m_flags;
	u16  m_textureWidth;
	u16  m_textureHeight;
	u8   m_textureMips;
	u8   m_textureFormat; // grcImage::Format
	char m_textureSwizzle[4];
	u32  m_texturePixelHash;
#if SAVE_STRINGS_IN_TEXTUREUSAGE
	const char* m_textureNameStr;
	const char* m_texturePathStr;
	const char* m_drawablePathStr;
#endif // SAVE_STRINGS_IN_TEXTUREUSAGE
};

class CTextureUsage_Drawable
{
public:
	CTextureUsage_Drawable(
		const char* drawablePathStr,
		char        drawableStoreType,
		int         drawableIndex,
		const char* dwdPathStr,
		const char* parentTxdPathStr,
		float       maxSizeXY)
	{
		const u64 drawablePathHash  = crc64_lower(drawablePathStr);
		const u64 dwdPathHash       = crc64_lower(dwdPathStr);
		const u64 parentTxdPathHash = crc64_lower(parentTxdPathStr);

		m_tag                  = 'DRAW';
		m_drawablePathHash[0]  = (u32)(drawablePathHash);
		m_drawablePathHash[1]  = (u32)(drawablePathHash >> 32);
		m_drawableStoreType    = drawableStoreType;
		m_drawableIndex        = (char)drawableIndex;
		m_unused[0]            = 0;
		m_unused[1]            = 0;
		m_maxSizeXY            = maxSizeXY;
		m_dwdPathHash[0]       = (u32)(dwdPathHash);
		m_dwdPathHash[1]       = (u32)(dwdPathHash >> 32);
		m_parentTxdPathHash[0] = (u32)(parentTxdPathHash);
		m_parentTxdPathHash[1] = (u32)(parentTxdPathHash >> 32);
#if SAVE_STRINGS_IN_TEXTUREUSAGE
		m_drawablePathStr      = drawablePathStr;
		m_dwdPathStr           = dwdPathStr;
		m_parentTxdPathStr     = parentTxdPathStr;
#endif // SAVE_STRINGS_IN_TEXTUREUSAGE
	}

	void Write() const
	{
		fiStream* stream = CTextureUsage::GetFileStream();

		if (stream)
		{
			stream->WriteInt(&m_tag, 1);
			stream->WriteInt(&m_drawablePathHash[0], 2);
			stream->WriteByte(&m_drawableStoreType, 1);
			stream->WriteByte(&m_drawableIndex, 1);
			stream->WriteByte(&m_unused[0], 2);
			stream->WriteInt((const int*)&m_maxSizeXY, 1);
			stream->WriteInt(&m_dwdPathHash[0], 2);
			stream->WriteInt(&m_parentTxdPathHash[0], 2);
#if SAVE_STRINGS_IN_TEXTUREUSAGE
			CTextureUsage_WriteString(stream, m_drawablePathStr);
			CTextureUsage_WriteString(stream, m_dwdPathStr);
			CTextureUsage_WriteString(stream, m_parentTxdPathStr);
#endif // SAVE_STRINGS_IN_TEXTUREUSAGE

			if (gStreamingIteratorTest_BuildTextureUsageMapDATFlush)
			{
				stream->Flush();
			}

			gStreamingIteratorTest_BuildTextureUsageMapDATFileEntryIndex++;
		}
	}

	int   m_tag;
	u32   m_drawablePathHash[2];
	char  m_drawableStoreType;
	char  m_drawableIndex; // index into dwd, or 0xff
	char  m_unused[2];
	float m_maxSizeXY;
	u32   m_dwdPathHash[2];
	u32   m_parentTxdPathHash[2];
#if SAVE_STRINGS_IN_TEXTUREUSAGE
	const char* m_drawablePathStr;
	const char* m_dwdPathStr;
	const char* m_parentTxdPathStr;
#endif // SAVE_STRINGS_IN_TEXTUREUSAGE
};

class CTextureUsage_TextureDict
{
public:
	CTextureUsage_TextureDict(
		const char* txdPathStr,
		const char* parentTxdPathStr)
	{
		const u64 txdPathHash       = crc64_lower(txdPathStr);
		const u64 parentTxdPathHash = crc64_lower(parentTxdPathStr);

		m_tag                  = 'TXD ';
		m_txdPathHash[0]       = (u32)(txdPathHash);
		m_txdPathHash[1]       = (u32)(txdPathHash >> 32);
		m_parentTxdPathHash[0] = (u32)(parentTxdPathHash);
		m_parentTxdPathHash[1] = (u32)(parentTxdPathHash >> 32);
#if SAVE_STRINGS_IN_TEXTUREUSAGE
		m_txdPathStr           = txdPathStr;
		m_parentTxdPathStr     = parentTxdPathStr;
#endif // SAVE_STRINGS_IN_TEXTUREUSAGE
	}

	void Write() const
	{
		fiStream* stream = CTextureUsage::GetFileStream();

		if (stream)
		{
			stream->WriteInt(&m_tag, 1);
			stream->WriteInt(&m_txdPathHash[0], 2);
			stream->WriteInt(&m_parentTxdPathHash[0], 2);
#if SAVE_STRINGS_IN_TEXTUREUSAGE
			CTextureUsage_WriteString(stream, m_txdPathStr);
			CTextureUsage_WriteString(stream, m_parentTxdPathStr);
#endif // SAVE_STRINGS_IN_TEXTUREUSAGE

			if (gStreamingIteratorTest_BuildTextureUsageMapDATFlush)
			{
				stream->Flush();
			}

			gStreamingIteratorTest_BuildTextureUsageMapDATFileEntryIndex++;
		}
	}

	int m_tag;
	u32 m_txdPathHash[2];
	u32 m_parentTxdPathHash[2];
#if SAVE_STRINGS_IN_TEXTUREUSAGE
	const char* m_txdPathStr;
	const char* m_parentTxdPathStr;
#endif // SAVE_STRINGS_IN_TEXTUREUSAGE
};

static bool CDTVStreamingIteratorTest_IsUnknownTexturePath(u64 texturePathHash)
{
	if (0) // enable this to debug unknown texture paths
	{
		const u64 _unknownTexturePaths[] =
		{
			0x9A442F24BC873F83,
			0x3D79EC2263A57F8D,
			0x0E1EDEF29175E3FB,
			0x9FA0C7CCFD164CA1,
			0xDFD3F93C4BE0034F,
		};

		static atMap<u64,bool> unknownTexturePaths;
		static bool bInited = false;

		if (!bInited)
		{
			for (int i = 0; i < NELEM(_unknownTexturePaths); i++)
			{
				unknownTexturePaths[_unknownTexturePaths[i]] = true;
			}

			bInited = true;
		}

		if (unknownTexturePaths.Access(texturePathHash))
		{
			return true;
		}
	}

	return false;
}

static bool CDTVStreamingIteratorTest_IsUnknownDrawablePath(u64 drawablePathHash)
{
	if (0) // enable this to debug unknown drawable paths
	{
		const u64 _unknownDrawablePaths[] =
		{
			0x21E795FCF399FF2D,
			/*
			0x00338CE022CBBEE5,0x015A3FBB1B103335,0x016CE8D105EAB65F,0x01FAA0531EF7D531,0x02AF13DF2B197051,0x0481C4C19A4527A5,0x04BBCD074FB67D4F,0x04C40D172BCB5E51,
			0x058B069551ED5F0D,0x059A52F172153F91,0x0617C538AAC971C7,0x069A8B43E172F7C6,0x06B98A2CA517C994,0x06F506DC6927D2B0,0x07CFCC43BFC0A274,0x08410AAAE4F2A65A,
			0x0952F532E596561A,0x0A505EEEB0D63D30,0x0A677735C3D29E82,0x0B7CBF95CAC487FE,0x0C24E0A6FB702E85,0x0C53DBD8EF17A06A,0x0C930BFC942CE23F,0x0D9C02B63EF82065,
			0x0DBF674F76605F10,0x0DDB1CCCE61F2D22,0x0E2F3CCECBB3597C,0x0E650B3DDB1B65B5,0x0F8E380C67D0EE65,0x0FA24A3C4BF19649,0x0FACF70D12836177,0x0FC52FB436762033,
			0x10E66608B4F6045F,0x11216C60BE6851FA,0x1199B0103F7D70CC,0x119CE6C8826035D9,0x11EE62B402F7EDBC,0x12518B2F88397435,0x135B83FCA481A0AC,0x13A4F390B45132E5,
			0x13ADE5270C02E30E,0x1448BE07FE3E77E0,0x14B09D2043CBD35C,0x14E08F2A7B9C896D,0x17479A9D6424532E,0x17C12D6A1988B032,0x17EC2F32B6F3DCE1,0x186958FBD1966735,
			0x1970C8DCF203FDFD,0x19F66ECD22FD97B7,0x1B0F8647AEF4AFF6,0x1B247A6579C0C358,0x1B5308370D5062B2,0x1D0C96949D0B9EDF,0x1DE2D25C9B2F8275,0x1DE9951FBCFE931B,
			0x1DF7806989438F40,0x1E2FB45096FD2B65,0x1E47DE284A510C36,0x1EEED9C569F845AE,0x1FA5B2177AAFEB7C,0x1FCB394BDF991D76,0x214A5540A4375779,0x21AF56CB85C25ABD,
			0x22A400663DE31FC3,0x238DFA9FE6A5D4D0,0x23B262A86CC8E8FB,0x23E371C3439322DA,0x2408620E35F0D5B2,0x242F9B2FE8AAAE93,0x24984321670A4F60,0x25211D63752C45BD,
			0x2715CBE3346CAB14,0x2762B9B140FC0AFE,0x280E7DD3C702BE46,0x282382E0B07EF5F2,0x28483C4D03797859,0x28A64CFE42A040CB,0x28C7FA64E2B30331,0x28F65EF47AF71AFA,
			0x2C960361172652BA,0x2CA0A5DC8DCACDF9,0x2D236E1F54FAF5F2,0x2D3EC18135B95174,0x2D67AFB48754985C,0x2DD5E4A44B255F48,0x2DDA251CBB5CFC7F,0x2EAB79B4BD09BA00,
			0x3006FB4D1BCF0317,0x313D29532A6D232F,0x319DDF18DF23E484,0x31DAC16207C4E9C3,0x31F9A49B4F5C96B6,0x3269FF1AF28F90DA,0x33137FDAC419A15C,0x3383EC600F4AE995,
			0x3407C97EDDCE6FFC,0x344905E2EEC5511F,0x344905E2EEC5511F,0x351436E6DCAA9FBC,0x35AAA1FCFB59D3BB,0x35F25770746193F4,0x361225957F600E09,0x36169D3A89EAF496,
			0x3729F78B4EC22E31,0x382C403DD09715A1,0x38892108D07D2602,0x39237452AE52B587,0x39DC91254B29F637,0x3A5106EC93F5B861,0x3ADC4897D84E3E60,0x3AFF49F89C2B0032,
			0x3B890F9786FC6BD2,0x3B8A1AAD9DAB09FF,0x3C754F341BF77743,0x3C83C61B643EF9C4,0x3D1CFC6F222CFA93,0x3D2A2B053CD67FF9,0x3E6DB7941D3D16BA,0x3E820691B260FEE6,
			0x3FB9D48F83C2DEDE,0x4009F6B8274C7860,0x40A1C795A2EE86ED,0x40BC02A49C05C49A,0x4187D0BAADA7E4A2,0x41C18F48220D4381,0x4272CE50EE52937E,0x430EE5BEA3ED4585,
			0x432854F84558413F,0x43A32AD3CE1BF672,0x440BAE6A8E77B8D7,0x440F1F9AE826F552,0x45071B265E271CB1,0x454F3D5C08905C3B,0x4570C98263AD81C1,0x45759F5ADEB0C4D4,
			0x45C815F2E2B8A0F7,0x462E3B217CE52640,0x46F2824593F2E1EF,0x4900EC8DE02E6216,0x49432F3B90A164F7,0x49CCE912716B1F9F,0x49E5EF06C1DB6FD2,0x4B0BB2EB28B45A1A,
			0x4B1F6194A18AB3E9,0x4B2240D98349EC7B,0x4B4CCB85267F1A71,0x4C8021698D469638,0x4D5E1239CA68998B,0x4D82198F42FC6F33,0x4D99B14EAED30CF0,0x4DA05F7072D56800,
			0x4E271E09D00E1CD1,0x4FBA71A5518093BF,0x4FCD03F725103255,0x51489287139D1467,0x51627F070D3DAE00,0x51732B632EC5CE9C,0x524B41EE9093FF5A,0x5250F3BEF9C73899,
			0x5273F2D1BDA206CB,0x52FEBCAAF61980CA,0x54F2E1303F2E2700,0x5513D9C14227243C,0x55859143593A4752,0x55B3462947C0C238,0x561FAB9A2B31148B,0x574AB0B4618A4B3F,
			0x575C4C5F77321979,0x58CD9934A7A0DF88,0x5932655EBACFDC2F,0x59561EDD2AB0AE1D,0x59757B246228D168,0x5A8C72AF87CB94B8,0x5AC6455C9763A871,0x5B2C56266AA6D13E,
			0x5B458E9F4E53907A,0x5B4B33AE17216744,0x5B67419E3B001F68,0x5BECAE26CF937A40,0x5C42928C3442909E,0x5CA87338B8225757,0x5CE6BFA48B2969B4,0x5D071344162C9E02,
			0x5DBB8CA0B946A717,0x5E3BEA524BE0F30F,0x5EB9277CEC06CC3D,0x60A07C70B215A012,0x60EEB0EC811E9EF1,0x6142080355961672,0x6143D86EA78922B6,0x61FD4F74807A6EB1,
			0x62FFE4A8D53A059B,0x638525C84FBEBFDC,0x63EA2DFF46DF4139,0x64CE76D376464A07,0x6510DD09138C67BB,0x6533B8F05B1418CE,0x6539A63025D4AF10,0x656AB161AED8FF4C,
			0x6574A68A83F31589,0x65F5A4CD47E46A3F,0x66808688AE5F61D7,0x668D3246B58CD373,0x676A95F2539A1898,0x67FA064898C95051,0x681930BBD52ADBBC,0x689C36A64727864E,
			0x68E9F919A934C6A5,0x69551A157B1BED9A,0x69C3529760068EF4,0x69F585FD7EFC0B9E,0x6BF0ED5B4A6D7766,0x6C5E55D7BB46BAE8,0x6CC539AF8C47E4AC,0x6D0D3ECC63E1E927,
			0x6D35E8B717F9073A,0x6D6587C98AE49AD0,0x6D852475B56CB493,0x6DB189CA1470E0AA,0x6E16306AC0FBF13F,0x6E353105849ECF6D,0x6E6DCA26E3C2C13A,0x6EB87F7ECF25496C,
			0x6F450963A504C986,0x6F607605DA2C9ADF,0x70C6E2BDB47A5F9E,0x738BC0231C2CFBF3,0x73FCB27168BC5A19,0x74C8A80F6265DABB,0x75462F59D912ABB0,0x7562B39226EABB0E,
			0x75A32CD2F8E7A674,0x76C69C3B4DA8218B,0x76FD6822BC8FDA63,0x770A08511F43D3D7,0x7849DC4ED11A3CF4,0x79335C8EE78C0D72,0x798ED626DB846951,0x79CA178D082A04FF,
			0x79D7B8136969A079,0x7A420026E1D94B0D,0x7C1F27662627EBF7,0x7C4F356C1E70B1C6,0x7C6F2064F444E2BD,0x7CE704419BD24F4B,0x7D705A2BAD706595,0x7F7182392FA2368C,
			0x8124ADD0C356F8E0,0x8154AAD21135F1AA,0x8174BFDAFB01A2D1,0x81DC8EF77EA35C5C,0x81F171C409DF17E8,0x827B34AE36A3A335,0x8408D63802FD1E65,0x8427B9232D34CED9,
			0x842BFF40F6C936C1,0x84B55C903EF57A46,0x84DAE5CF92E23604,0x84F19D3BED5B17E8,0x84F5598435E8B1E9,0x857256F8346B2FE3,0x8637B4FEDC82D0C5,0x864A65568D326BDF,
			0x86F14424B191A466,0x87527A7632740A48,0x8771B748BC904BE7,0x87798A9004A8581A,0x887DA5EF3C63B8A7,0x888DE1D258D007BE,0x89F322B98714C9AC,0x8A1AE6143A2D9724,
			0x8A3182E7FA32C0C0,0x8A5F09BB5F0436CA,0x8A62D3CF578CBAEE,0x8AC543AC954FD013,0x8B5901D1662E9AD6,0x8B9931D05CBBD927,0x8BB4B5CCFD35559E,0x8BBB84A0166000D9,
			0x8BD0C5B4CA0173E2,0x8C624E9EAD816F71,0x8D4AB005DEABAD7A,0x8DD916EA09C1C57E,0x8DFD680B510B4C89,0x8EB04A95F95DE8E4,0x8EC738C78DCD490E,0x900E6201F288142D,
			0x90E6F53B3FCAE65D,0x912CD69BCFE6029B,0x925BFCB33F5D89C8,0x930EBBB361EFDC7A,0x932DBADC258AE228,0x9383F5C82A545A7B,0x9449EB66DF33E650,0x946E90A39E6AFE8D,
			0x94CE0F4B9B8D1889,0x94F8D82185779DE3,0x9565A7D16BCF3D76,0x956A8BEB1C1ABA28,0x95A7BC10A2569559,0x975745DE57667DDC,0x97BF44B0A49CF4A0,0x98083246BE650BD9,
			0x982B57BFF6FD74AC,0x984F2C3C6682069E,0x98E6CF22E8132F96,0x994D0421C368368A,0x99905F42D7041BC9,0x9A367ACCCB6CBDF5,0x9A511F44B6EB0B8F,0x9AAEE3563AE9DCB2,
			0x9AC18CFE7DB84346,0x9B567B5C5C81C825,0x9BBB0C3E4B2E72C0,0x9C7852D842F831A1,0x9CC17AF1E0B86AFF,0x9CC6C5C2650B7DA6,0x9D9BF6C65764B305,0x9DD53A5A646F8DE6,
			0x9DF62B70178FCB6D,0x9E094AAB300FC090,0x9E92777BEEC84303,0x9ED1A749A3AE522E,0x9F6DA7AC85D06DAA,0x9FC46E1E304B168C,0xA08006165C37B400,0xA193F98E5D534440,
			0xA19996317FA22EC8,0xA1DD35126E587AA3,0xA245D947EEC3E6B9,0xA374CFB73EBEB523,0xA382ADCA0977DF2A,0xA409EFE85FBECF38,0xA4170E9879485DE2,0xA44EF1928759C27F,
			0xA46D946BCFC1BD0A,0xA4D4F0C5C428E069,0xA5F6138242D1CC9F,0xA617DC908FD7C229,0xA61A2E5A557515FE,0xA6557CDDD49BA281,0xA65CCB28DE710C7F,0xA670B918F2507453,
			0xA67E0429AB22836D,0xA7FDCFEA7212BB66,0xA8285377A756372B,0xA865D712BB63AED2,0xA86A11647DCE3CC4,0xA888CC9FA2B1D12F,0xA8BE1BF5BC4B5445,0xA916A8C21AC24DA2,
			0xA9857F3C823C3D2C,0xAA1CF4DA5F06CB3D,0xAB9CD9689E1F6806,0xAC48A1D5CBB4DD8B,0xAC59F5B1E84CBD17,0xAD1041E0D5A8C090,0xAD7F01DF388A30BE,0xAE1D3F670661406E,
			0xAF48786758D315DC,0xAF6B79081CB62B8E,0xAF70CB5875E2EC4D,0xAFA22E34EC467DC7,0xAFC5361C136893DD,0xB0232D82C7FB148D,0xB0A59FC8A6E9AE5A,0xB1BBABDF6837852F,
			0xB281FB13B4F180A8,0xB2F68941C0612142,0xB31C94BF357F0FC6,0xB42BD206482B64DF,0xB43B663B055F7101,0xB46CCC7C90CC6998,0xB4DE65B024AA7CC5,0xB5AF5352E42B48F8,
			0xB5D85FDA8C4638A1,0xB619CA6F6638FF6C,0xB6774133C30E0966,0xB78312F03D6D41C7,0xB7DE61DDEC70973D,0xB7FD47742F5CC97F,0xB84B433486DC92D6,0xB84E15EC3BC1D7C3,
			0xB8B75EEFD467DE4E,0xB934952C0D57E645,0xB988DC983B59C45A,0xB9AA9A670B70C369,0xBB3F49443D9491BC,0xBB787F4F4269A642,0xBC6A9C3BDC21766C,0xBD327C0EC23D6B77,
			0xBD8E99D4AC711DB3,0xBD9A4D23479F95FA,0xBE28E3EAC1CF9110,0xBE396C7D53F55FDB,0xC011A1B3D9A76CEE,0xC02776D9C75DE984,0xC087E931C2BA0F80,0xC14EC582FE866454,
			0xC3017463074E826B,0xC3563D22F84C05AD,0xC4E71B93AE58E520,0xC54DE869B57C87C6,0xC593DBC70B5A3A24,0xC68D0AB3249EDE1E,0xC6B28521638D78C5,0xC6DD4EFA2F615FB5,
			0xC76A8C5A7684AB76,0xC7C4C34E795A1325,0xC82FBC5039DB8CAB,0xC8912B4A1E28C0AC,0xC8E2350028E8E838,0xC93C43C838BF7CEB,0xC9728F540BB44208,0xCA4A26C5C4647CFF,
			0xCA7B0EE9B218B20E,0xCAAA8579FBEC52BC,0xCB2D178C6C9BE781,0xCB90CF83589CF66E,0xCCA655AE3A52F793,0xCCC22E2DAA2D85A1,0xCCE14BD4E2B5FAD4,0xCCF86C85C7D9CD92,
			0xCD59A9C4273DF434,0xCE28F56C2168B24B,0xCE96CF9D1E00F2BE,0xCEB866D6EA3BFA82,0xCED1BE6FCECEBBC6,0xCEDF035E97BC4CF8,0xCEF3716EBB9D34D4,0xCF26FBA9D83C0EC9,
			0xCF5275AC17FE83CD,0xCF90E2E8059F3DD5,0xCFA6F6C1C9B94BE0,0xD018E4A9B18BE6E5,0xD0339C5DCE32C709,0xD05C250262258B4B,0xD0E1AFAA5E2DEF68,0xD0E4F972E330AA7D,
			0xD1099E8B3CA0E9E7,0xD15821916C1DBBB9,0xD19B2F6A68BBDEEE,0xD2115BAB9F86FA88,0xD3545A9AAD76AEBE,0xD35A469CB468D7CE,0xD390F3025878A917,0xD5180856550FE6E5,
			0xD535F7652273AD51,0xD59DC648A7D153DC,0xD5CDD4429F8609ED,0xD659D83A0C34CC83,0xD78DC83F12FF26E2,0xD8BE80895EAAFEE0,0xD91411990DDBBD84,0xD98552976F08DED8,
			0xD9A3C997827B5C77,0xD9FD674C02A9F312,0xDA2E4155D11DB803,0xDA593307A58D19E9,0xDA72CF2572B97547,0xDC68C384649A4579,0xDC934275771AE5BA,0xDC94DC7D60B349AA,
			0xDD1A5B2BDBC438A1,0xDEB6702903D4C7C7,0xDED8FB75A6E231CD,0xDF704842006B282A,0xE0371C22787A8DC8,0xE085B5EECC1C9895,0xE0A1C407B7CF0F42,0xE0D21FA9598F800C,
			0xE1DD55C50D2C92AF,0xE29E38A19FDEF86B,0xE2F0B3FD3AE80E61,0xE3143EE6738C3872,0xE39147F38A3D9070,0xE3A1FE6407FA2E71,0xE3C4ACDAFF7F70BB,0xE4D81C7EDCBA1275,
			0xE51A570322D39092,0xE552D24D34E77422,0xE61FF0D39CB1D04F,0xE63FDE82B4C25C97,0xE648D20ADCAF2CCE,0xE6688281E82171A5,0xE74E706EE9276A83,0xE7F5ED2D69AFFECB,
			0xE80DC8DCF633F2C0,0xE883E5A980F18761,0xE91046CF585C7DB4,0xE97334B11B4F64F7,0xE98A7E73E02DA37E,0xE9921E7537BC1BA3,0xE9DB059C9EED9A7A,0xEA0A0F7EFB58FEA6,
			0xEA6F5DC003DDA06C,0xEAFAA7DCFCF9A325,0xEB4196F3E75DB7D5,0xEB8D3DD77921C946,0xEC24E64C64B0C7B9,0xEC35941B7E6DD575,0xEC4C5741A2B1E7E9,0xEC7CCD08EA63E82D,
			0xECA23AA6DA0C63DB,0xECA76C7E671126CE,0xECD5E802E786FEAB,0xEDDDECBE51871748,0xEDDEED256B713BED,0xEDF8D5E93EEE468B,0xEF4ABFB6FE109A60,0xF084EDF993114C07,
			0xF08DD4DB3569C7ED,0xF0934EEF063AD115,0xF0A78800DB893372,0xF0CD24B9ACE820AF,0xF0E0967A036E3E35,0xF11F6A101E013D92,0xF154E7C642A2C64C,0xF2977DBBF7F27260,
			0xF299C08AAE80855E,0xF2B5B2BA82A1FD72,0xF2BC054F884B538C,0xF2FEA502D3073324,0xF314B6782EC24A6B,0xF4697F8400E7450D,0xF4A25F8E8745E2F1,0xF4D7E89E2714090A,
			0xF5344C8032888BAE,0xF570EFA32372DFC5,0xF57A801C0183B54D,0xF68790376295DC0B,0xF6C1EDFCCA71F415,0xF76BD45855A72E27,0xF87B5830A69B0C30,0xF8A1D84797642C86,
			0xF8B08C23B49C4C1A,0xF90B0EF0DF43E71F,0xF95DED4B021D5F08,0xF9F938728978319D,0xFAF446F55AB1B163,0xFB2C4F8E4FB862D0,0xFB82009A4066DA83,0xFBA101F50403E4D1,
			0xFBBF2908A14DBAE3,0xFC576267E09BA548,0xFC61B50DFE612022,0xFC85519070848D29,0xFCC12AE5FB86C626,0xFD63510004B95342,0xFDD4AA176AEF6D76,0xFDFFD1504612BCAF,
			0xFEFC40E18B7C02C9,0xFF8A6CA5801A3648,
			*/
		};

		static atMap<u64,bool> unknownDrawablePaths;
		static bool bInited = false;

		if (!bInited)
		{
			for (int i = 0; i < NELEM(_unknownDrawablePaths); i++)
			{
				unknownDrawablePaths[_unknownDrawablePaths[i]] = true;
			}

			bInited = true;
		}

		if (unknownDrawablePaths.Access(drawablePathHash))
		{
			return true;
		}
	}

	return false;
}

static bool CDTVStreamingIteratorTest_BuildTextureUsageMaps_DebugMatch(const atString* texturePath, const grmShader* shader)
{
	if (gStreamingIteratorTest_BuildTextureUsageMapTextureFilter[0] != '\0' && texturePath && StringMatch(texturePath->c_str(), gStreamingIteratorTest_BuildTextureUsageMapTextureFilter))
	{
		return true;
	}

	if (gStreamingIteratorTest_BuildTextureUsageMapShaderFilter[0] != '\0' && shader && StringMatch(shader->GetName(), gStreamingIteratorTest_BuildTextureUsageMapShaderFilter))
	{
		return true;
	}

	return false;
}

static bool CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsTxdParentChainEmpty(int slot)
{
	while (slot != INDEX_NONE)
	{
		if (!g_TxdStore.GetSlot(slot)->m_isDummy)
		{
			const fwTxd* txd = g_TxdStore.Get(slot);

			if (txd && txd->GetCount() > 0)
			{
				return false;
			}
		}

		slot = g_TxdStore.GetParentTxdSlot(slot);
	}

	return true;
}

template <typename T> static int CDTVStreamingIteratorTest_GetParentTxdForSlot(T& store, int slot)
{
	int parentTxdSlot = INDEX_NONE;

	if (store.IsValidSlot(slot))
	{
		parentTxdSlot = store.GetParentTxdForSlot(slot);

		while (parentTxdSlot != INDEX_NONE && g_TxdStore.GetSlot(parentTxdSlot)->m_isDummy)
		{
			parentTxdSlot = g_TxdStore.GetParentTxdSlot(parentTxdSlot);
		}
	}

	return parentTxdSlot;
}

template <> int CDTVStreamingIteratorTest_GetParentTxdForSlot<fwTxdStore>(fwTxdStore& store, int slot)
{
	int parentTxdSlot = INDEX_NONE;

	if (store.IsValidSlot(slot))
	{
		parentTxdSlot = store.GetParentTxdSlot(slot);

		while (parentTxdSlot != INDEX_NONE && g_TxdStore.GetSlot(parentTxdSlot)->m_isDummy)
		{
			parentTxdSlot = g_TxdStore.GetParentTxdSlot(parentTxdSlot);
		}
	}

	return parentTxdSlot;
}

template <> int CDTVStreamingIteratorTest_GetParentTxdForSlot<ptfxAssetStore>(ptfxAssetStore&, int)
{
	return INDEX_NONE;
}

static void CDTVStreamingIteratorTest_BuildTextureUsageMaps_TxdStore(int slot)
{
	if (gStreamingIteratorTest_BuildTextureUsageMapPhase == 0)
	{
		const fwTxd* txd = g_TxdStore.Get(slot);

		if (txd)
		{
			int  nonHDTxdSlot      = INDEX_NONE;
			bool nonHDTxdSlotValid = false;

			const atString path = CDTVStreamingIteratorTest_GetAssetPath(g_TxdStore, slot, INDEX_NONE);

			if (strstr(path.c_str(), "/$"))
			{
				siLog(">> !ERROR: failed to get txd path: %s, actual name is \"%s\"", path.c_str(), g_TxdStore.GetName(slot));
			}
			else
			{
				atString parentTxdPath("");

				const int parentTxdSlot = CDTVStreamingIteratorTest_GetParentTxdForSlot(g_TxdStore, slot);

				if (parentTxdSlot != INDEX_NONE)
				{
					parentTxdPath = CDTVStreamingIteratorTest_GetAssetPath(g_TxdStore, parentTxdSlot, INDEX_NONE);
				}

				if (gStreamingIteratorTest_BuildTextureUsageMapDATOutput)
				{
					CTextureUsage_TextureDict tu(
						path.c_str(),
						parentTxdPath.c_str()
					);

					tu.Write();
				}
			}

			for (int i = 0; i < txd->GetCount(); i++)
			{
				const grcTexture* texture = txd->GetEntry(i);

				if (texture)
				{
					const atString textureName = atVarString("%s.dds", GetFriendlyTextureName(texture).c_str());
					const atString texturePath = atVarString("%s%s", path.c_str(), textureName.c_str());

					const u64 texturePathHash = crc64_lower(texturePath.c_str());

					if (CDTVStreamingIteratorTest_IsUnknownTexturePath(texturePathHash))
					{
						Displayf("> 0x%08X%08X: %s", (u32)(texturePathHash >> 32), (u32)(texturePathHash), texturePath.c_str());
					}

					// search for duplicates in parents
					{
						const u32 textureNameHash = crc32_lower(texture->GetName());
						int parentTxdSlot = g_TxdStore.GetParentTxdSlot(slot);
						bool bFound = false;

						while (parentTxdSlot != INDEX_NONE)
						{
							if (!g_TxdStore.GetSlot(parentTxdSlot)->m_isDummy)
							{
								const fwTxd* parentTxd = g_TxdStore.Get(parentTxdSlot);

								if (parentTxd)
								{
									for (int j = 0; j < parentTxd->GetCount(); j++)
									{
										const grcTexture* parentTexture = parentTxd->GetEntry(j);

										if (parentTexture && textureNameHash == crc32_lower(parentTexture->GetName()))
										{
											if (!nonHDTxdSlotValid)
											{
												if (strstr(path.c_str(), "+hi"))
												{
													char temp[256] = "";
													const char* s = strstr(path.c_str(), "+hi");

													while (s > path.c_str() && s[-1] != '/')
													{
														s--;
													}

													strcpy(temp, s);
													strrchr(temp, '+')[0] = '\0';

													nonHDTxdSlot = g_TxdStore.FindSlot(temp);
												}

												nonHDTxdSlotValid = true;
											}

											if (parentTxdSlot != nonHDTxdSlot)
											{
												bFound = true;
												break;
											}
										}
									}
								}
							}

							if (bFound)
							{
								break;
							}

							parentTxdSlot = g_TxdStore.GetParentTxdSlot(parentTxdSlot);
						}

						if (bFound)
						{
							siLog(
								">> [3] \"%s\" found in parent txd \"%s\"",
								texturePath.c_str(),
								CDTVStreamingIteratorTest_GetAssetPath(g_TxdStore, parentTxdSlot, INDEX_NONE).c_str()
							);
						}
					}

					if (strstr(texturePath.c_str(), "/unzipped/textures/") == NULL &&
						strstr(texturePath.c_str(), "/scaleform") == NULL)
					{
						const int texW = texture->GetWidth();
						const int texH = texture->GetHeight();

						if ((texW&(texW - 1))|(texH&(texH - 1)))
						{
							siLog(
								">> [10] \"%s\" non-power-of-2 size \"%dx%d\"",
								texturePath.c_str(),
								texW,
								texH
							);
						}
					}

					u8 flags = 0;

					if (strstr(texturePath.c_str(), atVarString("/levels/%s/"                 , RS_PROJECT).c_str()) != NULL &&
						strstr(texturePath.c_str(), atVarString("/levels/%s/cloudhats/"       , RS_PROJECT).c_str()) == NULL &&
						strstr(texturePath.c_str(), atVarString("/levels/%s/vehicles_packed/" , RS_PROJECT).c_str()) == NULL &&
						strstr(texturePath.c_str(), atVarString("/levels/%s/generic/cutspeds/", RS_PROJECT).c_str()) == NULL)
					{
						flags |= BIT(0); // map/interior/prop/des
					}

					if (strstr(texturePath.c_str(), "+hi.itd"  ) != NULL ||
						strstr(texturePath.c_str(), "+hidr.itd") != NULL ||
						strstr(texturePath.c_str(), "+hidd.itd") != NULL ||
						strstr(texturePath.c_str(), "+hifr.itd") != NULL)
					{
						flags |= BIT(1); // HD
					}

					if (stristr(texturePath.c_str(), "/models/"                 ) != NULL &&
						stristr(texturePath.c_str(), "/models/cdimages/weapons/") == NULL)
					{
						flags |= BIT(2); // ped, also includes plantsmgr and skydome
					}
					else if (stristr(texturePath.c_str(), atVarString("/levels/%s/generic/cutspeds/", RS_PROJECT).c_str()))
					{
						flags |= BIT(2); // ped
					}

					if (strstr(texturePath.c_str(), atVarString("/levels/%s/vehicles_packed/", RS_PROJECT).c_str()))
					{
						flags |= BIT(3); // veh
					}

					if (strstr(texturePath.c_str(), "/$"))
					{
						flags |= BIT(7);
					}

					if (CDTVStreamingIteratorTest_BuildTextureUsageMaps_DebugMatch(&texturePath, NULL))
					{
						Displayf("TEXTURE USAGE 'TXTR'[%d]:", gStreamingIteratorTest_BuildTextureUsageMapDATFileEntryIndex);
						Displayf("  textureName       = \"%s\"", textureName.c_str());
						Displayf("  texturePath       = \"%s\"", texturePath.c_str());
						Displayf("  drawablePath      = NULL");
						Displayf("  drawableStoreType = 0");
						Displayf("  flags             = 0x%08x", flags);
					}

#define REPORT_UNCOMPRESSED_TEXTURES (0) // temporary

#if REPORT_UNCOMPRESSED_TEXTURES
					if (texture->GetImageFormat() != grcImage::DXT1 &&
						texture->GetImageFormat() != grcImage::DXT3 &&
						texture->GetImageFormat() != grcImage::DXT5 &&
						texture->GetImageFormat() != grcImage::A8 &&
						texture->GetImageFormat() != grcImage::L8)
					{
						if (strstr(texture->GetName(), "_pal") != NULL &&
							texture->GetWidth() == 256 &&
							texture->GetHeight() <= 8)
						{
							// ok, a palette texture
						}
						else if (texture->Get
						{
							siLog(">> !ERROR: uncompressed texture %s,%s,%dx%d", texturePath.c_str(), grcImage::GetFormatString((grcImage::Format)texture->GetImageFormat()), texture->GetWidth(), texture->GetHeight());
						}
					}
#endif // REPORT_UNCOMPRESSED_TEXTURES

					if (gStreamingIteratorTest_BuildTextureUsageMapDATOutput)
					{
						CTextureUsage_Texture tu(
							texture,
							textureName.c_str(),
							texturePath.c_str(),
							path.c_str(), // drawablePathHash (in this case because drawableStoreType == 0 we store the txd path hash)
							0, // drawableStoreType
							flags
						);

						tu.Write();
					}
				}
			}
		}
	}
}

template <typename T> static void CDTVStreamingIteratorTest_BuildTextureUsageMaps(T& store, int slot, int index, const Drawable* drawable, bool& bUsesNonPackedTextures)
{
	if (drawable && drawable->GetShaderGroupPtr())
	{
		const atString drawablePath     = CDTVStreamingIteratorTest_GetAssetPath(store, slot, index);
		const u64      drawablePathHash = crc64_lower(drawablePath.c_str());

		char drawableStoreType = 0;

		if      ((const void*)&store == &g_TxdStore     ) { drawableStoreType = 1; Assert(0); }
		else if ((const void*)&store == &g_DwdStore     ) { drawableStoreType = 2; }
		else if ((const void*)&store == &g_DrawableStore) { drawableStoreType = 3; }
		else if ((const void*)&store == &g_FragmentStore) { drawableStoreType = 4; }
		else if ((const void*)&store == &g_ParticleStore) { drawableStoreType = 5; }

		if (CDTVStreamingIteratorTest_IsUnknownDrawablePath(drawablePathHash))
		{
			Displayf("> 0x%08X%08X: %s", (u32)(drawablePathHash >> 32), (u32)(drawablePathHash), drawablePath.c_str());
		}

		if (strstr(drawablePath.c_str(), "/$")) // e.g. $DRAWABLE, $UNKNOWN_RPF, $UNKNOWN_TEXTURE, $INVALID_SLOT etc.
		{
			bool bIsPed = false;

			if (stristr(drawablePath.c_str(), "/models/"                 ) != NULL &&
				stristr(drawablePath.c_str(), "/models/cdimages/weapons/") == NULL)
			{
				bIsPed = true; // also includes plantsmgr and skydome
			}
			else if (stristr(drawablePath.c_str(), atVarString("/levels/%s/generic/cutspeds/", RS_PROJECT).c_str()))
			{
				bIsPed = true;
			}

			if (!bIsPed && strstr(drawablePath.c_str(), "/$"))
			{
				siLog(">> !ERROR: failed to get drawable path: %s", drawablePath.c_str());
			}
		}
		else
		{
			atString dwdPath("");
			atString parentTxdPath("");

			if ((const void*)&store == &g_DwdStore)
			{
				dwdPath = CDTVStreamingIteratorTest_GetAssetPath(store, slot, INDEX_NONE);
			}

			const int parentTxdSlot = CDTVStreamingIteratorTest_GetParentTxdForSlot(store, slot);

			if (parentTxdSlot != INDEX_NONE)
			{
				parentTxdPath = CDTVStreamingIteratorTest_GetAssetPath(g_TxdStore, parentTxdSlot, INDEX_NONE);
			}

			if (gStreamingIteratorTest_BuildTextureUsageMapDATOutput)
			{
				Vec3V bbmin = Vec3V(V_ZERO);
				Vec3V bbmax = Vec3V(V_ZERO);
				drawable->GetLodGroup().GetBoundingBox(RC_VECTOR3(bbmin), RC_VECTOR3(bbmax));
				const float maxSizeXY = MaxElement((bbmax - bbmin).GetXY()).Getf();

				CTextureUsage_Drawable tu(
					drawablePath.c_str(),
					drawableStoreType,
					index,
					dwdPath.c_str(),
					parentTxdPath.c_str(),
					maxSizeXY
				);

				tu.Write();
			}
		}

		float   bumpiness = 1.0f;
		bool    bumpinessFound = false;
		float   specIntensity = 1.0f;
		bool    specIntensityFound = false;
		Vector3 specIntensityMask(1.0f, 0.0f, 0.0f);
		bool    specIntensityMaskFound = false;

		const grmShaderGroup* shaderGroup = drawable->GetShaderGroupPtr();

		// get shader vars and textures so we can pack this information with the usage
		if (gStreamingIteratorTest_BuildTextureUsageMapPhase == 0)
		{
			for (int i = 0; i < shaderGroup->GetCount(); i++)
			{
				const grmShader* shader = shaderGroup->GetShaderPtr(i);
				const atVarString desc("%s%s(%d)", drawablePath.c_str(), shader->GetName(), i); // append shadergroup index to drawable path

				for (int j = 0; j < shader->GetVarCount(); j++)
				{
					const grcEffectVar var = shader->GetVarByIndex(j);
					const char* name = NULL;
					grcEffect::VarType type;
					int annotationCount = 0;
					bool isGlobal = false;

					shader->GetVarDesc(var, name, type, annotationCount, isGlobal);

					if (!isGlobal)
					{
						if (type == grcEffect::VT_FLOAT && stricmp(name, "Bumpiness") == 0)
						{
							shader->GetVar(var, bumpiness);
							bumpinessFound = true;
						}
						else if (type == grcEffect::VT_FLOAT && stricmp(name, "SpecularColor") == 0)
						{
							shader->GetVar(var, specIntensity);
							specIntensityFound = true;
						}
						else if (type == grcEffect::VT_VECTOR3 && stricmp(name, "SpecularMapIntensityMask") == 0)
						{
							shader->GetVar(var, specIntensityMask);
							specIntensityMaskFound = true;
						}
					}
				}
			}

			const fwTxd* txd = drawable->GetTexDictSafe();

			if (txd)
			{
				int  nonHDTxdSlot      = INDEX_NONE;
				bool nonHDTxdSlotValid = false;

				for (int i = 0; i < txd->GetCount(); i++)
				{
					const grcTexture* texture = txd->GetEntry(i);

					if (texture)
					{
						const atString textureName = atVarString("%s.dds", GetFriendlyTextureName(texture).c_str());
						const atString texturePath = CDTVStreamingIteratorTest_FindTexturePath(store, slot, index, drawable, texture, 0, NULL); // should be drawablePath + textureName

						const u64 texturePathHash = crc64_lower(texturePath.c_str());

						if (CDTVStreamingIteratorTest_IsUnknownTexturePath(texturePathHash))
						{
							Displayf("> 0x%08X%08X: %s", (u32)(texturePathHash >> 32), (u32)(texturePathHash), texturePath.c_str());
						}

						// search for duplicates in parents
						{
							const u32 textureNameHash = crc32_lower(texture->GetName());
							int parentTxdSlot = CDTVStreamingIteratorTest_GetParentTxdForSlot(store, slot);
							bool bFound = false;

							while (parentTxdSlot != INDEX_NONE)
							{
								if (!g_TxdStore.GetSlot(parentTxdSlot)->m_isDummy)
								{
									const fwTxd* parentTxd = g_TxdStore.Get(parentTxdSlot);

									if (parentTxd)
									{
										for (int j = 0; j < parentTxd->GetCount(); j++)
										{
											const grcTexture* parentTexture = parentTxd->GetEntry(j);

											if (parentTexture && textureNameHash == crc32_lower(parentTexture->GetName()))
											{
												if (!nonHDTxdSlotValid)
												{
													if (strstr(drawablePath.c_str(), "+hi"))
													{
														char temp[256] = "";
														const char* s = strstr(drawablePath.c_str(), "+hi");

														while (s > drawablePath.c_str() && s[-1] != '/')
														{
															s--;
														}

														strcpy(temp, s);
														strrchr(temp, '+')[0] = '\0';

														nonHDTxdSlot = g_TxdStore.FindSlot(temp);
													}

													nonHDTxdSlotValid = true;
												}

												if (parentTxdSlot != nonHDTxdSlot)
												{
													bFound = true;
													break;
												}
											}
										}
									}
								}

								if (bFound)
								{
									break;
								}

								parentTxdSlot = g_TxdStore.GetParentTxdSlot(parentTxdSlot);
							}

							if (bFound)
							{
								siLog(
									">> [3] \"%s\" found in parent txd \"%s\"",
									texturePath.c_str(),
									CDTVStreamingIteratorTest_GetAssetPath(g_TxdStore, parentTxdSlot, INDEX_NONE).c_str()
								);
							}
						}

						if (strstr(texturePath.c_str(), "/unzipped/textures/") == NULL &&
							strstr(texturePath.c_str(), "/scaleform") == NULL)
						{
							const int texW = texture->GetWidth();
							const int texH = texture->GetHeight();

							if ((texW&(texW - 1))|(texH&(texH - 1)))
							{
								siLog(
									">> [10] \"%s\" non-power-of-2 size \"%dx%d\"",
									texturePath.c_str(),
									texW,
									texH
								);
							}
						}

						u8 flags = 0;

						if (strstr(texturePath.c_str(), atVarString("/levels/%s/"                 , RS_PROJECT).c_str()) != NULL &&
							strstr(texturePath.c_str(), atVarString("/levels/%s/cloudhats/"       , RS_PROJECT).c_str()) == NULL &&
							strstr(texturePath.c_str(), atVarString("/levels/%s/vehicles_packed/" , RS_PROJECT).c_str()) == NULL &&
							strstr(texturePath.c_str(), atVarString("/levels/%s/generic/cutspeds/", RS_PROJECT).c_str()) == NULL)
						{
							flags |= BIT(0); // map/interior/prop/des
						}

						if (strstr(texturePath.c_str(), "+hi.itd"  ) != NULL ||
							strstr(texturePath.c_str(), "+hidr.itd") != NULL ||
							strstr(texturePath.c_str(), "+hidd.itd") != NULL ||
							strstr(texturePath.c_str(), "+hifr.itd") != NULL)
						{
							flags |= BIT(1); // HD
						}

						if (stristr(texturePath.c_str(), "/models/"                 ) != NULL &&
							stristr(texturePath.c_str(), "/models/cdimages/weapons/") == NULL)
						{
							flags |= BIT(2); // ped, also includes plantsmgr and skydome
						}
						else if (stristr(texturePath.c_str(), atVarString("/levels/%s/generic/cutspeds/", RS_PROJECT).c_str()))
						{
							flags |= BIT(2); // ped
						}

						if (strstr(texturePath.c_str(), atVarString("/levels/%s/vehicles_packed/", RS_PROJECT).c_str()))
						{
							flags |= BIT(3); // veh
						}

						if (strstr(texturePath.c_str(), "/$"))
						{
							flags |= BIT(7);
						}

						if (CDTVStreamingIteratorTest_BuildTextureUsageMaps_DebugMatch(&texturePath, NULL))
						{
							Displayf("TEXTURE USAGE 'TXTR'[%d]:", gStreamingIteratorTest_BuildTextureUsageMapDATFileEntryIndex);
							Displayf("  textureName       = \"%s\"", textureName.c_str());
							Displayf("  texturePath       = \"%s\"", texturePath.c_str());
							Displayf("  drawablePath      = \"%s\"", drawablePath.c_str());
							Displayf("  drawableStoreType = %d"    , (int)drawableStoreType);
							Displayf("  flags             = 0x%08x", flags);
						}

#if REPORT_UNCOMPRESSED_TEXTURES
						if (texture->GetImageFormat() != grcImage::DXT1 &&
							texture->GetImageFormat() != grcImage::DXT3 &&
							texture->GetImageFormat() != grcImage::DXT5 &&
							texture->GetImageFormat() != grcImage::A8 &&
							texture->GetImageFormat() != grcImage::L8)
						{
							if (strstr(texture->GetName(), "_pal") != NULL &&
								texture->GetWidth() == 256 &&
								texture->GetHeight() <= 8)
							{
								// ok, a palette texture
							}
							else
							{
								siLog(">> !ERROR: uncompressed texture %s,%s,%dx%d", texturePath.c_str(), grcImage::GetFormatString((grcImage::Format)texture->GetImageFormat()), texture->GetWidth(), texture->GetHeight());
							}
						}
#endif // REPORT_UNCOMPRESSED_TEXTURES

						if (gStreamingIteratorTest_BuildTextureUsageMapDATOutput)
						{
							CTextureUsage_Texture tu(
								texture,
								textureName.c_str(),
								texturePath.c_str(),
								drawablePath.c_str(),
								drawableStoreType,
								flags
							);

							tu.Write();
						}
					}
				}
			}
		}

		for (int i = 0; i < shaderGroup->GetCount(); i++)
		{
			const grmShader* shader = shaderGroup->GetShaderPtr(i);
			const atVarString desc("%s%s(%d)", drawablePath.c_str(), shader->GetName(), i); // append shadergroup index to drawable path

			grcTexture* diffTexture = NULL;
			grcTexture* bumpTexture = NULL;
			grcTexture* specTexture = NULL;

			if (gStreamingIteratorTest_BuildTextureUsageMapPhase == 0)
			{
				// first get three common textures so we can set compound usage 'D' and 'N'
				for (int j = 0; j < shader->GetVarCount(); j++)
				{
					const grcEffectVar var = shader->GetVarByIndex(j);
					const char* name = NULL;
					grcEffect::VarType type;
					int annotationCount = 0;
					bool isGlobal = false;

					shader->GetVarDesc(var, name, type, annotationCount, isGlobal);

					if (!isGlobal)
					{
						if (type == grcEffect::VT_TEXTURE)
						{
							grcTexture* texture = NULL;
							shader->GetVar(var, texture);

							if (texture && dynamic_cast<grcRenderTarget*>(texture) == NULL) // make sure this isn't a rendertarget
							{
								if      (stricmp(name, "DiffuseTex" ) == 0) { diffTexture = texture; }
								else if (stricmp(name, "BumpTex"    ) == 0) { bumpTexture = texture; }
								else if (stricmp(name, "SpecularTex") == 0) { specTexture = texture; }
							}
						}
					}
				}

				int drawBucket = shader->GetDrawBucket();

				if (drawBucket < 0 || drawBucket >= CRenderer::RB_NUM_BASE_BUCKETS)
				{
					drawBucket = CRenderer::RB_NUM_BASE_BUCKETS; // this will be "RB_UNKNOWN" in texture analysis
				}

				// TODO -- i'm trying to determine if the "_draw" or "_drawskinned" technique is needed here, but this logic
				// is not always doing the correct thing - in particular i'm seeing some frags which have skeleton data yet
				// they are using the _draw technique (in fact they are drawing the terrain shader, which doesn't even have
				// drawskinned techniques!)
				if (drawable->GetSkeletonData())
				{
					drawBucket |= 0x80;
				}

				bool bInconsistentSkinning = false;

				const bool bIsPedProp       = strstr(drawablePath.c_str(), "_p/p_") != NULL || strstr(drawablePath.c_str(), "/pedprops/") != NULL;
				const bool bIsPedShader     = strstr(shader->GetName(), "ped"    ) != NULL;
				const bool bIsVehicleShader = strstr(shader->GetName(), "vehicle") != NULL;
				const bool bIsWeaponShader  = strstr(shader->GetName(), "weapon" ) != NULL;
				const bool bIsTreeShader    = strstr(shader->GetName(), "tree"   ) != NULL;
				const bool bIsDecalShader   = strstr(shader->GetName(), "decal"  ) != NULL && strstr(shader->GetName(), "vehicle_decal") == NULL && strstr(shader->GetName(), "mirror_decal") == NULL;
				const bool bIsTerrainShader = strstr(shader->GetName(), "terrain") != NULL && strstr(shader->GetName(), "water_terrainfoam") == NULL;
				const bool bIsSkinned       = (drawBucket & 0x80) != 0;

				const CDebugArchetypeProxy* modelInfo = CDTVStreamingIteratorTest_GetArchetypeProxyForDrawable(store, slot, index);

				if (modelInfo)
				{
					bool bShaderIsOkForPed     = bIsPedShader;
					bool bShaderIsOkForVehicle = bIsVehicleShader;
					bool bShaderIsInconsistent = false;

					if (strcmp(shader->GetName(), "default") == 0)
					{
						bShaderIsOkForPed = true;
						bShaderIsOkForVehicle = true;
					}
					else if (strcmp(shader->GetName(), "cloth_default") == 0 || strcmp(shader->GetName(), "cloth_normal_spec") == 0)
					{
						bShaderIsOkForVehicle = true;
					}

					if ((bIsPedShader     && modelInfo->GetModelType() != MI_TYPE_PED    ) ||
						(bIsVehicleShader && modelInfo->GetModelType() != MI_TYPE_VEHICLE) ||
						(bIsWeaponShader  && modelInfo->GetModelType() != MI_TYPE_WEAPON ) ||
						(bIsTreeShader    && modelInfo->GetModelType() != MI_TYPE_BASE   ))
					{
						bShaderIsInconsistent = true;
					}

					if ((!bShaderIsOkForPed     && modelInfo->GetModelType() == MI_TYPE_PED    ) ||
						(!bShaderIsOkForVehicle && modelInfo->GetModelType() == MI_TYPE_VEHICLE))
					{
						bShaderIsInconsistent = true;
					}

					if (bShaderIsInconsistent)
					{
						const char* modelTypeStrings[] =
						{
							STRING(MI_TYPE_NONE     ),
							STRING(MI_TYPE_BASE     ),
							STRING(MI_TYPE_MLO      ),
							STRING(MI_TYPE_TIME     ),
							STRING(MI_TYPE_WEAPON   ),
							STRING(MI_TYPE_VEHICLE  ),
							STRING(MI_TYPE_PED      ),
							STRING(MI_TYPE_COMPOSITE),
						};
						CompileTimeAssert(MI_TYPE_NONE == 0);
						CompileTimeAssert(NELEM(modelTypeStrings) == MI_TYPE_COMPOSITE + 1);

						const char* modelTypeString = "UNKNOWN";

						if (modelInfo->GetIsTree())
						{
							modelTypeString = "TREE";
						}
						else if (modelInfo->GetModelType() < NELEM(modelTypeStrings))
						{
							modelTypeString = modelTypeStrings[modelInfo->GetModelType()];
						}

						siLog(
							">> [8] \"%s\" inconsistent with archetype \"%s (%s)\"",
							desc.c_str(),
							modelInfo->GetModelName(),
							modelTypeString
						);
					}
				}

				if ((bIsPedShader     && !bIsSkinned && !bIsPedProp) || // _p/p_ is a ped prop, these can be unskinned
					(bIsVehicleShader && !bIsSkinned && strstr(shader->GetName(), "vehicle_tire") == NULL)			||
					(bIsVehicleShader && !bIsSkinned && strstr(shader->GetName(), "vehicle_tire_emissive") == NULL) ||
					(bIsWeaponShader  && !bIsSkinned) ||
					(bIsTreeShader    &&  bIsSkinned) ||
					(bIsDecalShader   &&  bIsSkinned && false) || // lots of fragments using decal shaders .. need to figure out what this is
					(bIsTerrainShader &&  bIsSkinned))
				{
					if (0) // this warning seems to be happening a lot, not sure if it's still valid
					{
						siLog(
							">> [9] \"%s\" inconsistent with skinning \"%s\"",
							desc.c_str(),
							bIsSkinned ? "SKINNED" : "NOT-SKINNED"
						);
					}

					//bInconsistentSkinning = true;
				}

				if (CDTVStreamingIteratorTest_BuildTextureUsageMaps_DebugMatch(NULL, shader) || bInconsistentSkinning)
				{
					atString drawBucketStr("");

					switch (drawBucket & ~0x80)
					{
					case CRenderer::RB_OPAQUE      : drawBucketStr = STRING(RB_OPAQUE     ); break;
					case CRenderer::RB_ALPHA       : drawBucketStr = STRING(RB_ALPHA      ); break;
					case CRenderer::RB_DECAL       : drawBucketStr = STRING(RB_DECAL      ); break;
					case CRenderer::RB_CUTOUT      : drawBucketStr = STRING(RB_CUTOUT     ); break;
					case CRenderer::RB_NOSPLASH    : drawBucketStr = STRING(RB_NOSPLASH   ); break;
					case CRenderer::RB_NOWATER     : drawBucketStr = STRING(RB_NOWATER    ); break;
					case CRenderer::RB_WATER       : drawBucketStr = STRING(RB_WATER      ); break;
					case CRenderer::RB_DISPL_ALPHA : drawBucketStr = STRING(RB_DISPL_ALPHA); break;
					}

					if (drawBucket & 0x80)
					{
						drawBucketStr += atString("/drawskinned");
					}

					Displayf("TEXTURE USAGE 'SHAD'[%d]:", gStreamingIteratorTest_BuildTextureUsageMapDATFileEntryIndex);
					Displayf("  drawablePath      = \"%s\"", drawablePath.c_str());
					Displayf("  drawableStoreType = %d"    , (int)drawableStoreType);
					Displayf("  drawableShaderId  = %d"    , i);
					Displayf("  drawBucket        = %s"    , drawBucketStr.c_str());
					Displayf("  shaderName        = \"%s\"", shader->GetName());
				}

				if (gStreamingIteratorTest_BuildTextureUsageMapDATOutput)
				{
					atString diffTexturePath = atString("");
					atString bumpTexturePath = atString("");
					atString specTexturePath = atString("");

					if (diffTexture) { diffTexturePath = CDTVStreamingIteratorTest_FindTexturePath(store, slot, index, drawable, diffTexture, 0, NULL); }
					if (bumpTexture) { bumpTexturePath = CDTVStreamingIteratorTest_FindTexturePath(store, slot, index, drawable, bumpTexture, 0, NULL); }
					if (specTexture) { specTexturePath = CDTVStreamingIteratorTest_FindTexturePath(store, slot, index, drawable, specTexture, 0, NULL); }

					u8 flags = 0;

					if (strstr(diffTexturePath.c_str(), "/$")) { flags |= BIT(6); }
					if (strstr(bumpTexturePath.c_str(), "/$")) { flags |= BIT(6); }
					if (strstr(specTexturePath.c_str(), "/$")) { flags |= BIT(6); }
					if (strstr(drawablePath   .c_str(), "/$")) { flags |= BIT(7); }

					const CTextureUsage_Shader tu(
						drawablePath.c_str(),
						drawableStoreType,
						(char)i,
						flags,
						(u8)drawBucket,
						shader->GetName(),
						diffTexture ? diffTexturePath.c_str() : NULL,
						bumpTexture ? bumpTexturePath.c_str() : NULL,
						specTexture ? specTexturePath.c_str() : NULL
					);

					tu.Write();
				}
			}

			for (int j = 0; j < shader->GetVarCount(); j++)
			{
				const grcEffectVar var = shader->GetVarByIndex(j);
				const char* name = NULL;
				grcEffect::VarType type;
				int annotationCount = 0;
				bool isGlobal = false;

				shader->GetVarDesc(var, name, type, annotationCount, isGlobal);

				if (!isGlobal)
				{
					if (type == grcEffect::VT_TEXTURE)
					{
						grcTexture* texture = NULL;
						shader->GetVar(var, texture);

						if (texture && dynamic_cast<grcRenderTarget*>(texture) == NULL) // make sure this isn't a rendertarget
						{
							atString textureName = GetFriendlyTextureName(texture);

							// #######################################################
							if (gStreamingIteratorTest_BuildTextureUsageMapPhase == 0)
							{
								const atString textureName = atVarString("%s.dds", GetFriendlyTextureName(texture).c_str());
								const atString texturePath = CDTVStreamingIteratorTest_FindTexturePath(store, slot, index, drawable, texture, 0, &bUsesNonPackedTextures);

								if (gStreamingIteratorTest_BuildTextureUsageMapRuntimeTest)
								{
									atString textureName = GetFriendlyTextureName(texture);

									if (gStreamingIteratorTest_BuildTextureUsageMapTextureHashNumLines != 0)
									{
										textureName += atVarString(",0x%08X", GetTextureHash(texture, false, gStreamingIteratorTest_BuildTextureUsageMapTextureHashNumLines));
									}

									const u32 key = atStringHash(textureName.c_str());
									u8 flags = 0;

									if      (CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsDiffuseTex (name)) { flags = BIT(0); }
									else if (CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsBumpTex    (name)) { flags = BIT(1); }
									else if (CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsSpecularTex(name)) { flags = BIT(2); }

									if (gStreamingIteratorTest_BuildTextureUsageMapFlags.Access(key) == NULL)
									{
										gStreamingIteratorTest_BuildTextureUsageMapFlags[key] = flags;
									}
									else
									{
										gStreamingIteratorTest_BuildTextureUsageMapFlags[key] |= flags;
									}
								}

								//
								{
									char usage = '-';
									u8   flags = 0;

									if (0) {}
									else if (CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsDiffuseTex  (name)) { usage = 'd'; }
									else if (CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsBumpTex     (name)) { usage = 'n'; }
									else if (CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsSpecularTex (name)) { usage = 's'; }
									else if (CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsDistanceTex (name)) { usage = 'q'; }
									else if (CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsWaterTex    (name)) { usage = 'w'; }
									else if (CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsWaterFlowTex(name)) { usage = 'f'; }
									else if (CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsCableTex    (name)) { usage = 'c'; }
									else if (CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsTintPalette (name)) { usage = 'p'; }
									else if (CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsLookupTex   (name)) { usage = 'l'; }

									if (bumpinessFound            ) { flags |= BIT(0); }
									if (specIntensityFound        ) { flags |= BIT(1); }
									if (specIntensityMaskFound    ) { flags |= BIT(2); }
									if (diffTexture == texture    ) { flags |= BIT(3); }
									if (bumpTexture == texture    ) { flags |= BIT(4); }
									if (specTexture == texture    ) { flags |= BIT(5); }
									if (strstr(texturePath , "/$")) { flags |= BIT(6); }
									if (strstr(drawablePath, "/$")) { flags |= BIT(7); }

									Vec4V scale = Vec4V(V_ZERO);

									if (usage == 'n')
									{
										scale = Vec4V(ScalarV(bumpiness), Vec3V(V_ZERO));
									}
									else if (usage == 's')
									{
										scale = Vec4V(ScalarV(specIntensity), RCC_VEC3V(specIntensityMask));
									}

									if (CDTVStreamingIteratorTest_BuildTextureUsageMaps_DebugMatch(&texturePath, shader))
									{
										Displayf("TEXTURE USAGE 'TXSU'[%d]:", gStreamingIteratorTest_BuildTextureUsageMapDATFileEntryIndex);
										Displayf("  textureName       = \"%s\"", textureName.c_str());
										Displayf("  texturePath       = \"%s\"", texturePath.c_str());
										Displayf("  drawablePath      = \"%s\"", drawablePath.c_str());
										Displayf("  name              = \"%s\"", name);
										Displayf("  usage             = '%c'"  , usage);
										Displayf("  drawableStoreType = %d"    , (int)drawableStoreType);
										Displayf("  drawableShaderId  = %d"    , i);
										Displayf("  flags             = 0x%02X", (int)flags);
										Displayf("  scale             = %.2f,%.2f,%.2f,%.2f", VEC4V_ARGS(scale));
										Displayf("  shaderName        = \"%s\"", shader->GetName());
									}

									if (gStreamingIteratorTest_BuildTextureUsageMapDATOutput)
									{
										const CTextureUsage tu(
											textureName.c_str(),
											texturePath.c_str(),
											drawablePath.c_str(),
											name,
											usage,
											drawableStoreType,
											(char)i,
											flags,
											scale,
											shader->GetName()
										);

										tu.Write();
									}
								}
							}

							// #######################################################
							if (gStreamingIteratorTest_BuildTextureUsageMapPhase == 1 &&
								gStreamingIteratorTest_BuildTextureUsageMapRuntimeTest)
							{
								if (gStreamingIteratorTest_BuildTextureUsageMapTextureHashNumLines != 0)
								{
									textureName += atVarString(",0x%08X", GetTextureHash(texture, false, gStreamingIteratorTest_BuildTextureUsageMapTextureHashNumLines));
								}

								const u32 key = atStringHash(textureName.c_str());
								const u8 flags = gStreamingIteratorTest_BuildTextureUsageMapFlags[key];

								if (flags == 0 || (flags & (flags - 1)) == 0)
								{
									continue; // skip this, it's either already been reported or it only has one usage
								}

								u8 flagsCurrent = 0;

								if (gStreamingIteratorTest_BuildTextureUsageMapDiffuseTex .Access(textureName)) { flagsCurrent |= BIT(0); }
								if (gStreamingIteratorTest_BuildTextureUsageMapBumpTex    .Access(textureName)) { flagsCurrent |= BIT(1); }
								if (gStreamingIteratorTest_BuildTextureUsageMapSpecularTex.Access(textureName)) { flagsCurrent |= BIT(2); }

								if (CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsDiffuseTex(name))
								{
									if ((flagsCurrent & BIT(0)) == 0)
									{
										flagsCurrent |= BIT(0);
										gStreamingIteratorTest_BuildTextureUsageMapDiffuseTex[textureName] = desc;
									}
								}
								else if (CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsBumpTex(name))
								{
									if ((flagsCurrent & BIT(1)) == 0)
									{
										flagsCurrent |= BIT(1);
										gStreamingIteratorTest_BuildTextureUsageMapBumpTex[textureName] = desc;
									}
								}
								else if (CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsSpecularTex(name))
								{
									if ((flagsCurrent & BIT(2)) == 0)
									{
										flagsCurrent |= BIT(2);
										gStreamingIteratorTest_BuildTextureUsageMapSpecularTex[textureName] = desc;
									}
								}

								if (flagsCurrent == flags) // we've found examples of all the usages this texture has
								{
									const atString texturePath = CDTVStreamingIteratorTest_FindTexturePath(store, slot, index, drawable, texture, 0, NULL);

									atVarString str("%s, %s, used as", texturePath.c_str(), textureName.c_str());

									if (flags & BIT(0)) { str += " DiffuseTex" ; }
									if (flags & BIT(1)) { str += " BumpTex"    ; }
									if (flags & BIT(2)) { str += " SpecularTex"; }

									const atString* descDiffuseTex  = gStreamingIteratorTest_BuildTextureUsageMapDiffuseTex .Access(textureName);
									const atString* descBumpTex     = gStreamingIteratorTest_BuildTextureUsageMapBumpTex    .Access(textureName);
									const atString* descSpecularTex = gStreamingIteratorTest_BuildTextureUsageMapSpecularTex.Access(textureName);

									if (descDiffuseTex)
									{
										str += ", D";

										if (descBumpTex     && strcmp(descBumpTex    ->c_str(), descDiffuseTex->c_str()) == 0) { str += "B"; descBumpTex     = NULL; }
										if (descSpecularTex && strcmp(descSpecularTex->c_str(), descDiffuseTex->c_str()) == 0) { str += "S"; descSpecularTex = NULL; }

										str += "=";
										str += *descDiffuseTex;
									}

									if (descBumpTex)
									{
										str += ", B";

										if (descSpecularTex && strcmp(descSpecularTex->c_str(), descBumpTex->c_str()) == 0) { str += "S"; descSpecularTex = NULL; }

										str += "=";
										str += *descBumpTex;
									}

									if (descSpecularTex)
									{
										str += ", S=";
										str += *descSpecularTex;
									}

									Displayf(str.c_str());

									if (gStreamingIteratorTest_BuildTextureUsageMapCSVOutput) // dump to csv file
									{
										if (gStreamingIteratorTest_BuildTextureUsageMapCSVFile == NULL)
										{
											ASSET.PushFolder("assets:");
											gStreamingIteratorTest_BuildTextureUsageMapCSVFile = ASSET.Create("streamingiterator", "csv");
											ASSET.PopFolder();
										}

										if (gStreamingIteratorTest_BuildTextureUsageMapCSVFile)
										{
											str += "\n";
											gStreamingIteratorTest_BuildTextureUsageMapCSVFile->WriteByte(str.c_str(), str.length());

											if (gStreamingIteratorTest_BuildTextureUsageMapCSVFlush)
											{
												gStreamingIteratorTest_BuildTextureUsageMapCSVFile->Flush();
											}
										}
									}

									gStreamingIteratorTest_BuildTextureUsageMapFlags[key] = 0; // don't report this texture again
								}
							}
						}
					}
				}
			}
		}

		if (gStreamingIteratorTest_BuildTextureUsageMapPhase == 0)
		{
			const bool bReportAllLights = false; // enable this to get a report of all lights
			const gtaDrawable* gtaDraw = dynamic_cast<const gtaDrawable*>(drawable);

			if (gtaDraw)
			{
				for (int pass = 0; pass < 2; pass++)
				{
					int numLights = 0;
					int numTexturedLights = 0;

					for (int i = 0; i < gtaDraw->m_lights.GetCount(); i++)
					{
						const CLightAttr& light = gtaDraw->m_lights[i];

						if (light.m_projectedTextureKey != 0)
						{
							const atString texturePath = CDTVStreamingIteratorTest_FindTexturePath(store, slot, index, drawable, NULL, light.m_projectedTextureKey, &bUsesNonPackedTextures);

							if (pass == 0)
							{
								char textureName[256] = "";
								sprintf(textureName, "%s/$UNKNOWN_TEXTURE/", RS_UNZIPPED);
								const char* s = strrchr(texturePath.c_str(), '/');

								if (s)
								{
									strcpy(textureName, s + 1);
									char* end = strrchr(textureName, '.');

									if (end)
									{
										end[0] = '\0';
									}
								}

								char usage = 'd';
								u8   flags = 0;

								if (strstr(texturePath , "/$")) { flags |= BIT(6); }
								if (strstr(drawablePath, "/$")) { flags |= BIT(7); }

								if (gStreamingIteratorTest_BuildTextureUsageMapDATOutput)
								{
									const CTextureUsage tu(
										textureName,
										texturePath.c_str(),
										drawablePath.c_str(),
										"LightTexture",
										usage,
										drawableStoreType,
										(char)i,
										flags,
										Vec4V(V_ZERO),
										"light_draw" // shaderNameHash
									);

									tu.Write();
								}
							}
							else if (bReportAllLights) // pass == 1
							{
								Displayf("  %s", texturePath.c_str());								
							}

							numTexturedLights++;
						}

						numLights++;
					}

					if (pass == 0)
					{
						if (bReportAllLights)
						{
							if (numTexturedLights > 0 && numTexturedLights == numLights)
							{
								Displayf("GTADRAW: %s has %d textured lights", drawablePath.c_str(), numTexturedLights);
							}
							else if (numTexturedLights > 0)
							{
								Displayf("GTADRAW: %s has %d lights (%d textured)", drawablePath.c_str(), numLights, numTexturedLights);
							}
							else if (numLights > 0)
							{
								Displayf("GTADRAW: %s has %d lights", drawablePath.c_str(), numLights);
							}
						}

						if (numLights > 0)
						{
							siLog(">> [6] \"%s\" has lights \"%d\"", drawablePath.c_str(), numLights);
						}
					}
				}
			}

			if ((const void*)&store == &g_FragmentStore && index == INDEX_NONE)
			{
				const gtaFragType* gtaFrag = dynamic_cast<const gtaFragType*>(store.Get(slot));

				if (gtaFrag)
				{
					for (int pass = 0; pass < 2; pass++)
					{
						int numLights = 0;
						int numTexturedLights = 0;

						for (int i = 0; i < gtaFrag->m_lights.GetCount(); i++)
						{
							const CLightAttr& light = gtaFrag->m_lights[i];

							if (light.m_projectedTextureKey != 0)
							{
								const atString texturePath = CDTVStreamingIteratorTest_FindTexturePath(store, slot, index, drawable, NULL, light.m_projectedTextureKey, &bUsesNonPackedTextures);

								if (pass == 0)
								{
									char textureName[256] = "";
									sprintf(textureName, "%s/$UNKNOWN_TEXTURE/", RS_UNZIPPED);
									const char* s = strrchr(texturePath.c_str(), '/');

									if (s)
									{
										strcpy(textureName, s + 1);
										char* end = strrchr(textureName, '.');

										if (end)
										{
											end[0] = '\0';
										}
									}

									char usage = 'd';
									u8   flags = 0;

									if (strstr(texturePath , "/$")) { flags |= BIT(6); }
									if (strstr(drawablePath, "/$")) { flags |= BIT(7); }

									if (gStreamingIteratorTest_BuildTextureUsageMapDATOutput)
									{
										const CTextureUsage tu(
											textureName,
											texturePath.c_str(),
											drawablePath.c_str(),
											"LightTexture",
											usage,
											drawableStoreType,
											(char)i,
											flags,
											Vec4V(V_ZERO),
											"light_frag" // shaderNameHash
										);

										tu.Write();
									}
								}
								else if (bReportAllLights) // pass == 1
								{
									Displayf("  %s", texturePath.c_str());
								}

								numTexturedLights++;
							}

							numLights++;
						}

						if (pass == 0)
						{
							if (bReportAllLights)
							{
								if (numTexturedLights > 0 && numTexturedLights == numLights)
								{
									Displayf("GTAFRAG: %s has %d textured lights", drawablePath.c_str(), numTexturedLights);
								}
								else if (numTexturedLights > 0)
								{
									Displayf("GTAFRAG: %s has %d lights (%d textured)", drawablePath.c_str(), numLights, numTexturedLights);
								}
								else if (numLights > 0)
								{
									Displayf("GTAFRAG: %s has %d lights", drawablePath.c_str(), numLights);
								}
							}

							if (numLights > 0)
							{
								siLog(">> [6] \"%s\" has lights \"%d\"", drawablePath.c_str(), numLights);
							}
						}
					}
				}
			}
		}
	}
}

static void CDTVStreamingIteratorTest_BuildTextureUsageMaps_ParticleRule(int slot, int, ptxParticleRule* rule)
{
	if (rule)
	{
		const atString drawablePath     = atVarString("%sptxrules/%s/", CDTVStreamingIteratorTest_GetAssetPath(g_ParticleStore, slot, INDEX_NONE).c_str(), rule->GetName());
		const u64      drawablePathHash = crc64_lower(drawablePath.c_str());

		if (CDTVStreamingIteratorTest_IsUnknownDrawablePath(drawablePathHash))
		{
			Displayf("> 0x%08X%08X: %s", (u32)(drawablePathHash >> 32), (u32)(drawablePathHash), drawablePath.c_str());
		}

		if (gStreamingIteratorTest_BuildTextureUsageMapPhase == 0 &&
			rule->GetDrawType() != PTXPARTICLERULE_DRAWTYPE_MODEL)
		{
			const char drawableStoreType = 6;

			ptxShaderInst& si = rule->GetShaderInst();
			ptxInstVars& instVars = si.GetShaderVars();

			const grmShader* shader = si.GetGrmShader();

			// ..
			{
				if (CDTVStreamingIteratorTest_BuildTextureUsageMaps_DebugMatch(NULL, shader))
				{
					Displayf("TEXTURE USAGE 'SHAD'[%d]:", gStreamingIteratorTest_BuildTextureUsageMapDATFileEntryIndex);
					Displayf("  drawablePath      = \"%s\"", drawablePath.c_str());
					Displayf("  drawableStoreType = %d"    , (int)drawableStoreType);
					Displayf("  drawableShaderId  = -1");
					Displayf("  shaderName        = \"%s\"", shader->GetName());
				}

				if (gStreamingIteratorTest_BuildTextureUsageMapDATOutput)
				{
					const CTextureUsage_Shader tu(
						drawablePath.c_str(),
						drawableStoreType,
						-1,
						0,
						0,
						shader->GetName(),
						NULL,
						NULL,
						NULL
					);

					tu.Write();
				}
			}

			for (int j = 0; j < instVars.GetCount(); j++)
			{
				const ptxShaderVar* sv = instVars[j];

				if (sv->GetType() == PTXSHADERVAR_TEXTURE)
				{
					const ptxShaderVarTexture* svt = static_cast<const ptxShaderVarTexture*>(sv);
					const grcTexture* texture = svt->GetTexture();

					if (texture && dynamic_cast<const grcRenderTarget*>(texture) == NULL) // make sure this isn't a rendertarget
					{
						const char* name = atFinalHashString::TryGetString(sv->GetHashName());

						const atString textureName = atVarString("%s.dds", GetFriendlyTextureName(texture).c_str());
						const atString texturePath = CDTVStreamingIteratorTest_FindTexturePath(g_ParticleStore, slot, INDEX_NONE, NULL, texture, 0, NULL);

						char usage = '-';
						u8   flags = 0;

						if (0) {}
						else if (CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsDiffuseTex  (name)) { usage = 'd'; }
						else if (CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsBumpTex     (name)) { usage = 'n'; }
						else if (CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsSpecularTex (name)) { usage = 's'; }
						else if (CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsDistanceTex (name)) { usage = 'q'; }
						else if (CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsWaterTex    (name)) { usage = 'w'; }
						else if (CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsWaterFlowTex(name)) { usage = 'f'; }
						else if (CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsCableTex    (name)) { usage = 'c'; }
						else if (CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsTintPalette (name)) { usage = 'p'; }
						else if (CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsLookupTex   (name)) { usage = 'l'; }

						if (strstr(texturePath , "/$")) { flags |= BIT(6); }
						if (strstr(drawablePath, "/$")) { flags |= BIT(7); }

						if (CDTVStreamingIteratorTest_BuildTextureUsageMaps_DebugMatch(&texturePath, shader))
						{
							Displayf("TEXTURE USAGE 'TXSU'[%d]:", gStreamingIteratorTest_BuildTextureUsageMapDATFileEntryIndex);
							Displayf("  textureName       = \"%s\"", textureName.c_str());
							Displayf("  texturePath       = \"%s\"", texturePath.c_str());
							Displayf("  drawablePath      = \"%s\"", drawablePath.c_str());
							Displayf("  name              = \"%s\"", name);
							Displayf("  usage             = '%c'"  , usage);
							Displayf("  drawableStoreType = %d"    , (int)drawableStoreType);
							Displayf("  drawableShaderId  = %d"    , INDEX_NONE);
							Displayf("  flags             = 0x%02X", (int)flags);
							Displayf("  scale             = N/A");
							Displayf("  shaderName        = \"%s\"", shader->GetName());
						}

						if (gStreamingIteratorTest_BuildTextureUsageMapDATOutput)
						{
							const CTextureUsage tu(
								textureName.c_str(),
								texturePath.c_str(),
								drawablePath.c_str(),
								name,
								usage,
								drawableStoreType,
								INDEX_NONE,
								flags,
								Vec4V(V_ZERO),
								shader->GetName()
							);

							tu.Write();
						}
					}
				}
			}

			for (int j = 0; j < shader->GetVarCount(); j++)
			{
				const grcEffectVar var = shader->GetVarByIndex(j);
				const char* name = NULL;
				grcEffect::VarType type;
				int annotationCount = 0;
				bool isGlobal = false;

				shader->GetVarDesc(var, name, type, annotationCount, isGlobal);

				if (!isGlobal)
				{
					if (type == grcEffect::VT_TEXTURE)
					{
						grcTexture* texture = NULL;
						shader->GetVar(var, texture);

						if (texture && dynamic_cast<grcRenderTarget*>(texture) == NULL) // make sure this isn't a rendertarget
						{
							const atString textureName = atVarString("%s.dds", GetFriendlyTextureName(texture).c_str());
							const atString texturePath = CDTVStreamingIteratorTest_FindTexturePath(g_ParticleStore, slot, INDEX_NONE, NULL, texture, 0, NULL);

							char usage = '-';
							u8   flags = 0;

							if (0) {}
							else if (CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsDiffuseTex  (name)) { usage = 'd'; }
							else if (CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsBumpTex     (name)) { usage = 'n'; }
							else if (CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsSpecularTex (name)) { usage = 's'; }
							else if (CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsDistanceTex (name)) { usage = 'q'; }
							else if (CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsWaterTex    (name)) { usage = 'w'; }
							else if (CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsWaterFlowTex(name)) { usage = 'f'; }
							else if (CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsCableTex    (name)) { usage = 'c'; }
							else if (CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsTintPalette (name)) { usage = 'p'; }
							else if (CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsLookupTex   (name)) { usage = 'l'; }

							if (strstr(texturePath , "/$")) { flags |= BIT(6); }
							if (strstr(drawablePath, "/$")) { flags |= BIT(7); }

							if (CDTVStreamingIteratorTest_BuildTextureUsageMaps_DebugMatch(&texturePath, shader))
							{
								Displayf("TEXTURE USAGE 'TXSU'[%d]:", gStreamingIteratorTest_BuildTextureUsageMapDATFileEntryIndex);
								Displayf("  textureName       = \"%s\"", textureName.c_str());
								Displayf("  texturePath       = \"%s\"", texturePath.c_str());
								Displayf("  drawablePath      = \"%s\"", drawablePath.c_str());
								Displayf("  name              = \"%s\"", name);
								Displayf("  usage             = '%c'"  , usage);
								Displayf("  drawableStoreType = %d"    , (int)drawableStoreType);
								Displayf("  drawableShaderId  = %d"    , INDEX_NONE);
								Displayf("  flags             = 0x%02X", (int)flags);
								Displayf("  scale             = N/A");
								Displayf("  shaderName        = \"%s\"", shader->GetName());
							}

							if (gStreamingIteratorTest_BuildTextureUsageMapDATOutput)
							{
								const CTextureUsage tu(
									textureName.c_str(),
									texturePath.c_str(),
									drawablePath.c_str(),
									name,
									usage,
									drawableStoreType,
									INDEX_NONE,
									flags,
									Vec4V(V_ZERO),
									shader->GetName()
								);

								tu.Write();
							}
						}
					}
				}
			}
		}
	}
}

template <typename T> static bool CDTVStreamingIteratorTest_IsDummySlot(T&, int)
{
	return false;
}

template <> bool CDTVStreamingIteratorTest_IsDummySlot<fwTxdStore>(fwTxdStore& store, int slot)
{
	return store.GetSlot(slot)->m_isDummy;
}

template <typename T> static bool CDTVStreamingIteratorTest_IsSharedParentTxdSlot(T& store, int slot, int parentTxdSlot)
{
	for (int slot2 = 0; slot2 < store.GetSize(); slot2++)
	{
		if (slot2 != slot &&
			store.IsValidSlot(slot2) &&
			!CDTVStreamingIteratorTest_IsDummySlot(store, slot2) &&
			CDTVStreamingIteratorTest_GetParentTxdForSlot(store, slot2) == parentTxdSlot)
		{
			return true;
		}
	}

	return false;
}

template <typename T1, typename T2> static void CDTVStreamingIteratorTest_WarnAboutSharedByDifferentAssetTypes(T1& store1, T2& store2)
{
	DEBUG_TEXTURE_VIEWER_USE_DEBUG_MEMORY();

	if (!gStreamingIteratorTest_BuildTextureUsageMapDATOutput)
	{
		return;
	}

	u8* flags1 = rage_new u8[(g_TxdStore.GetSize() + 7)/8];
	u8* flags2 = rage_new u8[(g_TxdStore.GetSize() + 7)/8];

	sysMemSet(flags1, 0, (g_TxdStore.GetSize() + 7)/8);
	sysMemSet(flags2, 0, (g_TxdStore.GetSize() + 7)/8);

	for (int slot1 = 0; slot1 < store1.GetSize(); slot1++)
	{
		const int parentTxdSlot1 = CDTVStreamingIteratorTest_GetParentTxdForSlot(store1, slot1);

		if (parentTxdSlot1 != INDEX_NONE)
		{
			flags1[parentTxdSlot1/8] |= BIT(parentTxdSlot1%8);
		}
	}

	for (int slot2 = 0; slot2 < store2.GetSize(); slot2++)
	{
		const int parentTxdSlot2 = CDTVStreamingIteratorTest_GetParentTxdForSlot(store2, slot2);

		if (parentTxdSlot2 != INDEX_NONE)
		{
			flags2[parentTxdSlot2/8] |= BIT(parentTxdSlot2%8);
		}
	}

	for (int i = 0; i < g_TxdStore.GetSize(); i++)
	{
		if ((flags1[i/8] & BIT(i%8)) != 0 &&
			(flags2[i/8] & BIT(i%8)) != 0)
		{
			int slot1 = 0;
			int slot2 = 0;

			for (; slot1 < store1.GetSize(); slot1++)
			{
				if (CDTVStreamingIteratorTest_GetParentTxdForSlot(store1, slot1) == i)
				{
					break;
				}
			}

			for (; slot2 < store2.GetSize(); slot2++)
			{
				if (CDTVStreamingIteratorTest_GetParentTxdForSlot(store2, slot2) == i)
				{
					break;
				}
			}

			if (slot1 < store1.GetSize() &&
				slot2 < store2.GetSize())
			{
				siLog(
					">> different asset types \"%s\" and \"%s\" share parent txd \"%s\"",
					CDTVStreamingIteratorTest_GetAssetPath(store1, slot1, INDEX_NONE).c_str(),
					CDTVStreamingIteratorTest_GetAssetPath(store2, slot2, INDEX_NONE).c_str(),
					CDTVStreamingIteratorTest_GetAssetPath(g_TxdStore, i, INDEX_NONE).c_str()
				);
			}
			else
			{
				Assert(0); // should not happen
			}
		}
	}

	delete[] flags1;
	delete[] flags2;
}

template <typename T> static void CDTVStreamingIteratorTest_WarnAboutUniqueParentTxds(T& store, int slot)
{
	if (!gStreamingIteratorTest_BuildTextureUsageMapDATOutput)
	{
		return;
	}

	const int parentTxdSlot = CDTVStreamingIteratorTest_GetParentTxdForSlot(store, slot);

	if (parentTxdSlot != INDEX_NONE &&
		!CDTVStreamingIteratorTest_IsSharedParentTxdSlot(g_TxdStore     , (const void*)&store == &g_TxdStore      ? slot : INDEX_NONE, parentTxdSlot) &&
		!CDTVStreamingIteratorTest_IsSharedParentTxdSlot(g_DwdStore     , (const void*)&store == &g_DwdStore      ? slot : INDEX_NONE, parentTxdSlot) &&
		!CDTVStreamingIteratorTest_IsSharedParentTxdSlot(g_DrawableStore, (const void*)&store == &g_DrawableStore ? slot : INDEX_NONE, parentTxdSlot) &&
		!CDTVStreamingIteratorTest_IsSharedParentTxdSlot(g_FragmentStore, (const void*)&store == &g_FragmentStore ? slot : INDEX_NONE, parentTxdSlot))
	{
		siLog(
			">> [2] \"%s\" has unique parent txd \"%s\"",
			CDTVStreamingIteratorTest_GetAssetPath(store, slot, INDEX_NONE).c_str(),
			CDTVStreamingIteratorTest_GetAssetPath(g_TxdStore, parentTxdSlot, INDEX_NONE).c_str()
		);
	}
}

template <> void CDTVStreamingIteratorTest_WarnAboutUniqueParentTxds<fwDwdStore>(fwDwdStore& store, int slot)
{
	if (!gStreamingIteratorTest_BuildTextureUsageMapDATOutput)
	{
		return;
	}

	const int parentTxdSlot = CDTVStreamingIteratorTest_GetParentTxdForSlot(store, slot);

	if (parentTxdSlot != INDEX_NONE &&
		!CDTVStreamingIteratorTest_IsSharedParentTxdSlot(g_TxdStore     , (const void*)&store == &g_TxdStore      ? slot : INDEX_NONE, parentTxdSlot) &&
		!CDTVStreamingIteratorTest_IsSharedParentTxdSlot(g_DwdStore     , (const void*)&store == &g_DwdStore      ? slot : INDEX_NONE, parentTxdSlot) &&
		!CDTVStreamingIteratorTest_IsSharedParentTxdSlot(g_DrawableStore, (const void*)&store == &g_DrawableStore ? slot : INDEX_NONE, parentTxdSlot) &&
		!CDTVStreamingIteratorTest_IsSharedParentTxdSlot(g_FragmentStore, (const void*)&store == &g_FragmentStore ? slot : INDEX_NONE, parentTxdSlot))
	{
		// need to check that no drawable in dwd shares textures
		const Dwd* dwd = store.Get(slot);
		bool bSharedTextures = false;

		if (dwd)
		{
			for (int i1 =      0; i1 < dwd->GetCount() && !bSharedTextures; i1++)
			for (int i2 = i1 + 1; i2 < dwd->GetCount() && !bSharedTextures; i2++)
			{
				const Drawable* drawable1 = dwd->GetEntry(i1);
				const Drawable* drawable2 = dwd->GetEntry(i2);

				if (drawable1 == NULL || drawable2 == NULL)
				{
					continue;
				}

				const grmShaderGroup* shaderGroup1 = drawable1->GetShaderGroupPtr();
				const grmShaderGroup* shaderGroup2 = drawable2->GetShaderGroupPtr();

				if (shaderGroup1 == NULL || shaderGroup2 == NULL)
				{
					continue; // temporary workaround for NULL shadergroups (which shouldn't happen)
				}

				for (int j1 = 0; j1 < shaderGroup1->GetCount() && !bSharedTextures; j1++)
				for (int j2 = 0; j2 < shaderGroup2->GetCount() && !bSharedTextures; j2++)
				{
					const grmShader* shader1 = shaderGroup1->GetShaderPtr(j1);
					const grmShader* shader2 = shaderGroup2->GetShaderPtr(j2);

					for (int k1 = 0; k1 < shader1->GetVarCount() && !bSharedTextures; k1++)
					for (int k2 = 0; k2 < shader2->GetVarCount() && !bSharedTextures; k2++)
					{
						const grcEffectVar var1 = shader1->GetVarByIndex(k1);
						const grcEffectVar var2 = shader2->GetVarByIndex(k2);
						const char* name1 = NULL;
						const char* name2 = NULL;
						grcEffect::VarType type1;
						grcEffect::VarType type2;
						int annotationCount1 = 0;
						int annotationCount2 = 0;
						bool isGlobal1 = false;
						bool isGlobal2 = false;

						shader1->GetVarDesc(var1, name1, type1, annotationCount1, isGlobal1);
						shader2->GetVarDesc(var2, name2, type2, annotationCount2, isGlobal2);

						if (!isGlobal1 &&
							!isGlobal2)
						{
							if (type1 == grcEffect::VT_TEXTURE &&
								type2 == grcEffect::VT_TEXTURE)
							{
								grcTexture* texture1 = NULL;
								grcTexture* texture2 = NULL;
								shader1->GetVar(var1, texture1);
								shader2->GetVar(var2, texture2);

								if (texture1 && dynamic_cast<grcRenderTarget*>(texture1) == NULL &&
									texture2 && dynamic_cast<grcRenderTarget*>(texture2) == NULL)
								{
									// check that textures are in parent txd and not further up the chain
									const fwTxd* parentTxd = g_TxdStore.Get(parentTxdSlot);
									bool bInParent1 = false;
									bool bInParent2 = false;

									if (parentTxd)
									{
										for (int i = 0; i < parentTxd->GetCount(); i++)
										{
											if (parentTxd->GetEntry(i) == texture1) { bInParent1 = true; }
											if (parentTxd->GetEntry(i) == texture2) { bInParent2 = true; }
										}
									}

									if (bInParent1 && bInParent2 && texture1 == texture2)
									{
										bSharedTextures = true;
									}
								}
							}
						}
					}
				}
			}
		}

		if (!bSharedTextures)
		{
			siLog(
				">> [2] \"%s\" has unique parent txd \"%s\"",
				CDTVStreamingIteratorTest_GetAssetPath(store, slot, INDEX_NONE).c_str(),
				CDTVStreamingIteratorTest_GetAssetPath(g_TxdStore, parentTxdSlot, INDEX_NONE).c_str()
			);
		}
	}
}

static bool CDTVStreamingIteratorTest_DoesTxdHaveDrawableChildren(int slot)
{
	for (int i = 0; i < g_TxdStore.GetSize(); i++)
	{
		if (g_TxdStore.IsValidSlot(i) && g_TxdStore.GetParentTxdSlot(i) == slot) //&& !g_TxdStore.GetSlot(i)->m_isDummy)
		{
			return CDTVStreamingIteratorTest_DoesTxdHaveDrawableChildren(i);
		}
	}

	for (int i = 0; i < g_DwdStore.GetSize(); i++)
	{
		if (g_DwdStore.IsValidSlot(i) && g_DwdStore.GetParentTxdForSlot(i) == slot)
		{
			return true;
		}
	}

	for (int i = 0; i < g_DrawableStore.GetSize(); i++)
	{
		if (g_DrawableStore.IsValidSlot(i) && g_DrawableStore.GetParentTxdForSlot(i) == slot)
		{
			return true;
		}
	}

	for (int i = 0; i < g_FragmentStore.GetSize(); i++)
	{
		if (g_FragmentStore.IsValidSlot(i) && g_FragmentStore.GetParentTxdForSlot(i) == slot)
		{
			return true;
		}
	}

	return false;
}

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS_ONCE
static void CDTVStreamingIteratorTest_WarnAboutUniqueParentTxds()
{
	if (!gStreamingIteratorTest_BuildTextureUsageMapDATOutput)
	{
		return;
	}

	for (int slot = 0; slot < g_TxdStore.GetSize(); slot++)
	{
		if (g_TxdStore.IsValidSlot(slot) && !g_TxdStore.GetSlot(slot)->m_isDummy)
		{
			CDTVStreamingIteratorTest_WarnAboutUniqueParentTxds(g_TxdStore, slot);
		}
	}

	for (int slot = 0; slot < g_DrawableStore.GetSize(); slot++)
	{
		if (g_DrawableStore.IsValidSlot(slot))
		{
			CDTVStreamingIteratorTest_WarnAboutUniqueParentTxds(g_DrawableStore, slot);
		}
	}

	if (0) // check for txds shared by different asset types (this isn't necessarily bad, just interesting)
	{
		CDTVStreamingIteratorTest_WarnAboutSharedByDifferentAssetTypes(g_DrawableStore, g_TxdStore);
		CDTVStreamingIteratorTest_WarnAboutSharedByDifferentAssetTypes(g_DrawableStore, g_DwdStore);
		CDTVStreamingIteratorTest_WarnAboutSharedByDifferentAssetTypes(g_DrawableStore, g_FragmentStore);
		CDTVStreamingIteratorTest_WarnAboutSharedByDifferentAssetTypes(g_FragmentStore, g_DwdStore);
	}
}
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS_ONCE

static void CDTVStreamingIteratorTest_WarnAboutTxdHasNoDrawableChildren(int single_slot = INDEX_NONE)
{
	if (!gStreamingIteratorTest_BuildTextureUsageMapDATOutput)
	{
		return;
	}

	for (int slot = 0; slot < g_TxdStore.GetSize(); slot++)
	{
		if (single_slot != INDEX_NONE)
		{
			slot = single_slot;
		}

		if (g_TxdStore.IsValidSlot(slot))// && !g_TxdStore.GetSlot(slot)->m_isDummy)
		{
			const atString path = CDTVStreamingIteratorTest_GetAssetPath(g_TxdStore, slot, INDEX_NONE);
			bool bIsPed = false;
			bool bIsTextures = false;
			bool bIsCDImages = false;

			if (stristr(path.c_str(), "/models/"                 ) != NULL &&
				stristr(path.c_str(), "/models/cdimages/weapons/") == NULL)
			{
				bIsPed = true; // also includes plantsmgr and skydome
			}
			else if (stristr(path.c_str(), atVarString("/levels/%s/generic/cutspeds/", RS_PROJECT).c_str()))
			{
				bIsPed = true;
			}
			else if (stristr(path.c_str(), "/textures/"))
			{
				bIsTextures = true;
			}
			else if (stristr(path.c_str(), "/data/cdimages/"))
			{
				bIsCDImages = true;
			}

			if (!bIsPed && !bIsTextures && !bIsCDImages && !CDTVStreamingIteratorTest_DoesTxdHaveDrawableChildren(slot))
			{
				int nonHDSlot_txd      = INDEX_NONE;
				int nonHDSlot_dwd      = INDEX_NONE;
				int nonHDSlot_drawable = INDEX_NONE;
				int nonHDSlot_fragment = INDEX_NONE;

				if (strstr(path.c_str(), "+hi.itd/")) // HD txd that came from a txd
				{
					char temp[256] = "";
					strcpy(temp, g_TxdStore.GetName(slot));
					strrchr(temp, '+')[0] = '\0';

					nonHDSlot_txd = g_TxdStore.FindSlot(temp);
				}
				else if (strstr(path.c_str(), "+hidd.itd/")) // HD txd that came from a dwd
				{
					char temp[256] = "";
					strcpy(temp, g_TxdStore.GetName(slot));
					strrchr(temp, '+')[0] = '\0';

					nonHDSlot_dwd = g_DwdStore.FindSlot(temp);
				}
				else if (strstr(path.c_str(), "+hidr.itd/")) // HD txd that came from a drawable
				{
					char temp[256] = "";
					strcpy(temp, g_TxdStore.GetName(slot));
					strrchr(temp, '+')[0] = '\0';

					nonHDSlot_drawable = g_DrawableStore.FindSlot(temp);
				}
				else if (strstr(path.c_str(), "+hifr.itd/"))   // HD txd that came from a fragment
				{
					char temp[256] = "";
					strcpy(temp, g_TxdStore.GetName(slot));
					strrchr(temp, '+')[0] = '\0';

					nonHDSlot_fragment = g_FragmentStore.FindSlot(temp);
				}

				if (nonHDSlot_txd == INDEX_NONE || !CDTVStreamingIteratorTest_DoesTxdHaveDrawableChildren(nonHDSlot_txd))
				{
					if (nonHDSlot_dwd      == INDEX_NONE &&
						nonHDSlot_drawable == INDEX_NONE &&
						nonHDSlot_fragment == INDEX_NONE)
					{
						siLog(
							">> [4] \"%s\" has no drawable children \"...\"",
							CDTVStreamingIteratorTest_GetAssetPath(g_TxdStore, slot, INDEX_NONE).c_str()
						);
					}
				}
			}
		}

		if (single_slot != INDEX_NONE)
		{
			break;
		}
	}
}

template <typename T> static void CDTVStreamingIteratorTest_WarnAboutDrawableHasNoArchetype(T& store, int slot)
{
	if (!gStreamingIteratorTest_BuildTextureUsageMapDATOutput)
	{
		return;
	}

	const atString path = CDTVStreamingIteratorTest_GetAssetPath(store, slot, INDEX_NONE);

	const bool     bIsVehicleMod = strstr(path.c_str(), "/vehiclemods/") != NULL;
	const bool     bIsHD         = strstr(path.c_str(), "_hi.") != NULL;
	const bool     bIsPedProp    = strstr(path.c_str(), "_p/p_") != NULL || strstr(path.c_str(), "/pedprops/") != NULL;
	bool           bIsPed        = false;

	if (stristr(path.c_str(), "/models/"                 ) != NULL &&
		stristr(path.c_str(), "/models/cdimages/weapons/") == NULL)
	{
		bIsPed = true; // also includes plantsmgr and skydome
	}
	else if (stristr(path.c_str(), atVarString("/levels/%s/generic/cutspeds/", RS_PROJECT).c_str()))
	{
		bIsPed = true;
	}

	if (!bIsVehicleMod && !bIsHD && !bIsPed && !bIsPedProp)
	{
		bool bFound = false;

		for (u32 i = 0; i < CDebugArchetype::GetMaxDebugArchetypeProxies(); i++)
		{
			const CDebugArchetypeProxy* modelInfo = CDebugArchetype::GetDebugArchetypeProxy(i);

			if (modelInfo)
			{
				if (((const void*)&store == &g_DrawableStore && modelInfo->SafeGetDrawableIndex() == slot) ||
					((const void*)&store == &g_FragmentStore && modelInfo->SafeGetFragmentIndex() == slot))
				{
					bFound = true;
					break;
				}
				else if (modelInfo->GetModelType() == MI_TYPE_VEHICLE)
				{
					const CDebugVehicleArchetypeProxy* vehModelInfo = (const CDebugVehicleArchetypeProxy*)modelInfo;

					if ((const void*)&store == &g_FragmentStore && vehModelInfo->GetHDFragmentIndex() == slot)
					{
						bFound = true;
						break;
					}
				}
			}
		}

		if (!bFound)
		{
			siLog(
				">> [5] \"%s\" has no archetype \"...\"",
				CDTVStreamingIteratorTest_GetAssetPath(store, slot, INDEX_NONE).c_str()
			);
		}
	}
}

template <> void CDTVStreamingIteratorTest_WarnAboutDrawableHasNoArchetype<fwDwdStore>(fwDwdStore& store, int slot)
{
	if (!gStreamingIteratorTest_BuildTextureUsageMapDATOutput)
	{
		return;
	}

	const atString path = CDTVStreamingIteratorTest_GetAssetPath(store, slot, INDEX_NONE);

	const bool     bIsVehicleMod = strstr(path.c_str(), "/vehiclemods/") != NULL;
	const bool     bIsHD         = strstr(path.c_str(), "_hi.") != NULL;
	const bool     bIsPedProp    = strstr(path.c_str(), "_p/p_") != NULL || strstr(path.c_str(), "/pedprops/") != NULL;
	bool           bIsPed        = false;

	if (stristr(path.c_str(), "/models/"                 ) != NULL &&
		stristr(path.c_str(), "/models/cdimages/weapons/") == NULL)
	{
		bIsPed = true; // also includes plantsmgr and skydome
	}
	else if (stristr(path.c_str(), atVarString("/levels/%s/generic/cutspeds/", RS_PROJECT).c_str()))
	{
		bIsPed = true;
	}

	if (!bIsVehicleMod && !bIsHD && !bIsPed && !bIsPedProp)
	{
		const Dwd* dwd = store.Get(slot);

		if (dwd)
		{
			for (int j = 0; j < dwd->GetCount(); j++)
			{
				bool bFound = false;

				for (u32 i = 0; i < CDebugArchetype::GetMaxDebugArchetypeProxies(); i++)
				{
					const CDebugArchetypeProxy* modelInfo = CDebugArchetype::GetDebugArchetypeProxy(i);

					if (modelInfo && modelInfo->SafeGetDrawDictIndex() == slot && dwd->LookupLocalIndex(modelInfo->GetHashKey()) == j)
					{
						bFound = true;
						break;
					}
				}

				if (!bFound)
				{
					siLog(
						">> [5] \"%s\" has no archetype \"...\"",
						CDTVStreamingIteratorTest_GetAssetPath(store, slot, j).c_str()
					);
				}
			}
		}
	}
}

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS_ONCE
static void CDTVStreamingIteratorTest_WarnAboutDrawableHasNoArchetype()
{
	if (!gStreamingIteratorTest_BuildTextureUsageMapDATOutput)
	{
		return;
	}

	for (int slot = 0; slot < g_DrawableStore.GetSize(); slot++)
	{
		if (g_DrawableStore.IsValidSlot(slot))
		{
			CDTVStreamingIteratorTest_WarnAboutDrawableHasNoArchetype(g_DrawableStore, slot);
		}
	}

	for (int slot = 0; slot < g_FragmentStore.GetSize(); slot++)
	{
		if (g_FragmentStore.IsValidSlot(slot))
		{
			CDTVStreamingIteratorTest_WarnAboutDrawableHasNoArchetype(g_FragmentStore, slot);
		}
	}
}
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS_ONCE

#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS

// ================================================================================================

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_FIND_UNUSED

#define MAX_TEXTURES_PER_TXD_SLOT 700
#define MAX_TXD_SLOTS             10000

class CDTVStreamingIteratorTest_FindUnusedSharedTexturesData
{
public:
	CDTVStreamingIteratorTest_FindUnusedSharedTexturesData()
	{
		m_data = NULL;
	}

	void Reset()
	{
		if (m_data)
		{
			sysMemSet(m_data, 0, sizeof(u32)*((MAX_TEXTURES_PER_TXD_SLOT*MAX_TXD_SLOTS + 31)/32));
		}
	}

	void CheckTxdSlot(int slot)
	{
		DEBUG_TEXTURE_VIEWER_USE_DEBUG_MEMORY();

		if (m_data == NULL)
		{
			m_data = rage_new u32[(MAX_TEXTURES_PER_TXD_SLOT*MAX_TXD_SLOTS + 31)/32];
			Reset();
		}

		if (slot != INDEX_NONE)
		{
			const fwTxd* txd = g_TxdStore.Get(slot);

			Assertf(slot < MAX_TXD_SLOTS, "txd slot is %d, expected < %d", slot, MAX_TXD_SLOTS);

			if (txd && slot < MAX_TXD_SLOTS)
			{
				const atString desc = CDTVStreamingIteratorTest_GetAssetPath(g_TxdStore, slot, INDEX_NONE);

				Assertf(txd->GetCount() <= MAX_TEXTURES_PER_TXD_SLOT, "txd \"%s\" has %d textures, expected <= %d", g_TxdStore.GetName(slot), txd->GetCount(), MAX_TEXTURES_PER_TXD_SLOT);

				if (stristr(desc.c_str(), "/data/cdimages/scaleform_generic/"    ) != NULL || // skip scaleform txds
					stristr(desc.c_str(), "/data/cdimages/scaleform_minigames/"  ) != NULL ||
					stristr(desc.c_str(), "/data/cdimages/scaleform_web/"        ) != NULL ||
					stristr(desc.c_str(), "/platform:/textures/"                 ) != NULL || // skip all txds in /textures/ (e.g. plantsmgr, fxdecal, etc.)
					stristr(desc.c_str(), "/models/cdimages/componentpeds"       ) != NULL || // skip ped txds
					stristr(desc.c_str(), "/models/cdimages/pedprops/"           ) != NULL ||
					stristr(desc.c_str(), "/models/cdimages/ped_mp_overlay_txds/") != NULL ||
					stristr(desc.c_str(), "/models/cdimages/ped_mpheadov_txds/"  ) != NULL ||
					stristr(desc.c_str(), "/models/cdimages/ped_tattoo_txds/"    ) != NULL ||
					stristr(desc.c_str(), "/models/cdimages/streamedpeds/"       ) != NULL ||
					stristr(desc.c_str(), "/models/cdimages/streamedpedprops/"   ) != NULL ||
					stristr(desc.c_str(), atVarString("/levels/%s/generic/cutspeds/", RS_PROJECT).c_str()) != NULL ||
					stristr(desc.c_str(), "/textures/script_txds/"               ) != NULL || // skip script txds
					stristr(desc.c_str(), "+hi.itd/"                             ) != NULL) // skip HD txds for now
				{
					for (int k = 0; k < txd->GetCount(); k++)
					{
						const grcTexture* texture = txd->GetEntry(k);

						if (texture)
						{
							gStreamingIteratorTest_FindUnusedSharedTexturesMemoryNotCounted += (s64)texture->GetPhysicalSize();
							gStreamingIteratorTest_FindUnusedSharedTexturesMemoryNotCountedMB = (float)((double)gStreamingIteratorTest_FindUnusedSharedTexturesMemoryNotCounted/(1024.0*1024.0));
						}

						if (k < MAX_TEXTURES_PER_TXD_SLOT)
						{
							const int index = slot*MAX_TEXTURES_PER_TXD_SLOT + k;

							m_data[index/32] |= BIT(index%32);
						}
					}

					return;
				}

				if (txd->GetCount() > 0)
				{
					// first count how many are used
					int numTexturesUsed = 0;
					int numBytesUsed = 0;
					int numBytesTotal = 0;

					for (int k = 0; k < txd->GetCount() && k < MAX_TEXTURES_PER_TXD_SLOT; k++)
					{
						const int index = slot*MAX_TEXTURES_PER_TXD_SLOT + k;
						const grcTexture* texture = txd->GetEntry(k);

						if (texture == NULL)
						{
							texture = grcTexture::None; // this shouldn't happen, but i don't want to crash
						}

						if (m_data[index/32] & BIT(index%32))
						{
							numTexturesUsed++;
							numBytesUsed += texture->GetPhysicalSize();
						}

						numBytesTotal += texture->GetPhysicalSize();
					}

					Displayf("");
					Displayf(
						"> %s contains %d textures, %d used (%.2f%%), %.2fMB total, %.2fMB used (%.2f%%)",
						desc.c_str(),
						txd->GetCount(),
						numTexturesUsed,
						100.0f*(float)numTexturesUsed/(float)txd->GetCount(),
						(float)numBytesTotal/(1024.0f*1024.0f),
						(float)numBytesUsed/(1024.0f*1024.0f),
						100.0f*(float)numBytesUsed/(float)numBytesTotal
					);

					for (int k = 0; k < txd->GetCount() && k < MAX_TEXTURES_PER_TXD_SLOT; k++)
					{
						const int index = slot*MAX_TEXTURES_PER_TXD_SLOT + k;
						const grcTexture* texture = txd->GetEntry(k);

						if (texture == NULL)
						{
							texture = grcTexture::None; // this shouldn't happen, but i don't want to crash
						}

						if ((m_data[index/32] & BIT(index%32)) == 0)
						{
							Displayf(
								"- %s%s.dds (%s %dx%d) is not used by any drawable!",
								desc.c_str(),
								GetFriendlyTextureName(texture).c_str(),
								grcImage::GetFormatString((grcImage::Format)texture->GetImageFormat()),
								texture->GetWidth(),
								texture->GetHeight()
							);

							gStreamingIteratorTest_FindUnusedSharedTexturesMemoryUnused += (s64)texture->GetPhysicalSize();
							gStreamingIteratorTest_FindUnusedSharedTexturesMemoryUnusedMB = (float)((double)gStreamingIteratorTest_FindUnusedSharedTexturesMemoryUnused/(1024.0*1024.0));
						}
						else
						{
							Displayf(
								"+ %s%s.dds (%s %dx%d) is used",
								desc.c_str(),
								GetFriendlyTextureName(texture).c_str(),
								grcImage::GetFormatString((grcImage::Format)texture->GetImageFormat()),
								texture->GetWidth(),
								texture->GetHeight()
							);

							gStreamingIteratorTest_FindUnusedSharedTexturesMemoryUsed += (s64)texture->GetPhysicalSize();
							gStreamingIteratorTest_FindUnusedSharedTexturesMemoryUsedMB = (float)((double)gStreamingIteratorTest_FindUnusedSharedTexturesMemoryUsed/(1024.0*1024.0));
						}

						gStreamingIteratorTest_FindUnusedSharedTexturesMemoryTotal += (s64)texture->GetPhysicalSize();
						gStreamingIteratorTest_FindUnusedSharedTexturesMemoryTotalMB = (float)((double)gStreamingIteratorTest_FindUnusedSharedTexturesMemoryTotal/(1024.0*1024.0));
					}
				}
				else
				{
					Displayf("");
					Displayf("> %s contains 0 textures", desc.c_str());
				}
			}
		}
	}

	void MarkSharedTexture(int slot, int k)
	{
		DEBUG_TEXTURE_VIEWER_USE_DEBUG_MEMORY();

		if (m_data == NULL)
		{
			m_data = rage_new u32[(MAX_TEXTURES_PER_TXD_SLOT*MAX_TXD_SLOTS + 31)/32];
			Reset();
		}

		if (AssertVerify(slot < MAX_TXD_SLOTS && k < MAX_TEXTURES_PER_TXD_SLOT))
		{
			const int index = slot*MAX_TEXTURES_PER_TXD_SLOT + k;

			m_data[index/32] |= BIT(index%32);
		}
	}

	u32* m_data;
};

CDTVStreamingIteratorTest_FindUnusedSharedTexturesData gStreamingIteratorTest_FindUnusedSharedTexturesData;

// this is used to mark textures in the texture viewer
bool CDTVStreamingIteratorTest_IsTextureMarked(int slot, int k)
{
	if (gStreamingIteratorTest_FindUnusedSharedTexturesData.m_data == NULL)
	{
		return true; // hasn't run the test yet
	}
	else if (AssertVerify(slot >= 0 && slot < MAX_TXD_SLOTS && k >= 0 && k < MAX_TEXTURES_PER_TXD_SLOT))
	{
		const int index = slot*MAX_TEXTURES_PER_TXD_SLOT + k;

		return (gStreamingIteratorTest_FindUnusedSharedTexturesData.m_data[index/32] & BIT(index%32)) != 0;
	}

	return false;
}

void CDTVStreamingIteratorTest_FindUnusedSharedTexturesReset()
{
	gStreamingIteratorTest_FindUnusedSharedTexturesData.Reset();

	gStreamingIteratorTest_FindUnusedSharedTexturesMemoryUsed         = 0;
	gStreamingIteratorTest_FindUnusedSharedTexturesMemoryUsedMB       = 0.0f;
	gStreamingIteratorTest_FindUnusedSharedTexturesMemoryUnused       = 0;
	gStreamingIteratorTest_FindUnusedSharedTexturesMemoryUnusedMB     = 0.0f;
	gStreamingIteratorTest_FindUnusedSharedTexturesMemoryTotal        = 0;
	gStreamingIteratorTest_FindUnusedSharedTexturesMemoryTotalMB      = 0.0f;
	gStreamingIteratorTest_FindUnusedSharedTexturesMemoryNotCounted   = 0;
	gStreamingIteratorTest_FindUnusedSharedTexturesMemoryNotCountedMB = 0.0f;
}

template<typename T> static void CDTVStreamingIteratorTest_MarkSharedTextures(T& store, int slot, int, const Drawable* drawable)
{
	if (drawable && drawable->GetShaderGroupPtr())
	{
		const grmShaderGroup* shaderGroup = drawable->GetShaderGroupPtr();

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

				if (!isGlobal)
				{
					if (type == grcEffect::VT_TEXTURE)
					{
						grcTexture* texture = NULL;
						shader->GetVar(var, texture);

						if (texture && dynamic_cast<grcRenderTarget*>(texture) == NULL) // make sure this isn't a rendertarget
						{
							const grcTexture* _texture = texture;
							const int slotAndIndex = CDTVStreamingIteratorTest_FindSharedTxdSlot(store, slot, drawable, _texture, 0);

							if (slotAndIndex >= 0)
							{
								gStreamingIteratorTest_FindUnusedSharedTexturesData.MarkSharedTexture(slotAndIndex & 0x0000ffff, slotAndIndex >> 16);
							}
						}
					}
				}
			}
		}
	}
}

#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_FIND_UNUSED

// ================================================================================================

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEXTURE

static char TextureName_GetUsage(const char* name_, bool* bNameIsComposite = NULL)
{
	char usage = '-'; // unspecified (or maybe a diffuse map)

	char name[1024] = "";
	strcpy(name, name_);
	char* base = strrchr(name, '/');

	if (base && strchr(base, '_')) // is it a compound texture?
	{
		base++;

		// common case: <name> + <name> + "_a", e.g. "prop_micro_glass_01prop_micro_glass_01_a"
		// less common: <name1> + <suffix1> + <name2> + <suffix2>
		//
		// e.g.:
		// "ph_des_splinter_nph_des_splinter_a" --> "ph_des_splinter_" + "n" + "ph_des_splinter_" + "a"
		// "prop_chrome_03prop_green_glass_a"--> "prop_" + "chrome_03" + "prop_" + "green_glass_a"
		//
		// other examples:
		// "ph_shattered_glass_jewel_cab_dph_des_glass_shard2_a_"
		// "ph_shattered_glass_jewel_cab_sph_shattered_glass_jewel_cab_s"
		// "prop_bike_logo_1prop_bike_logo_1_a2"
		// "gz_v_bk_desklampgz_v_bk_desklamp_em"
		// "gz_v_cs_papertry01_sp_mi_tryppr_n2_aa_tm"
		// "v_mp_plastic_white_dmv_mp_light_diffuser_t"
		// "v_mp_grey_010_dmv_mp_p_alarm_detail_d"
		// "v_mp_socket_01_nv_mp_socket_01_t_02"

		char* s0 = base + (strlen(base)*1)/4;
		char* s1 = base + (strlen(base)*3)/4;

		char* bestStart = NULL;
		int   bestLength = 0;

		for (char* s = s0; s <= s1; s++)
		{
			for (char* ss = base; ss < s; ss++)
			{
				if (s[ss - base + 0] != ss[0])
				{
					const int len = ptrdiff_t_to_int(ss - base);

					if (bestLength <= len)
					{
						bestLength = len;
						bestStart = s;
					}

					break;
				}
			}
		}

		if (bestLength >= 6) // arbitrary min length
		{
			if (strchr(base, '_') <= base + bestLength) // base name must contains '_' char
			{
				bestStart[0] = '\0';

				if (bNameIsComposite)
				{
					*bNameIsComposite = true;
				}
			}
		}
	}

	int len = istrlen(name);
	char* end = name + len;

	// remove trailing "_lod"
	{
		const char* _lod = "_lod";
		const int x = istrlen(_lod);
		char* s = end - x;

		if (strcmp(s, _lod) == 0)
		{
			*s = '\0';
			end = s;
			len -= x;
		}
	}

	// remove trailing "_b<digit>"
	{
		if (len > 3 && end[-3] == '_' && end[-2] == 'b' && isdigit(end[-1]))
		{
			*(--end) = '\0';
			len--;
			*(--end) = '\0';
			len--;
			*(--end) = '\0';
			len--;
		}
	}

	while (end > name && isdigit(end[-1])) // remove trailing digits
	{
		*(--end) = '\0';
		len--;
	}

	while (end > name && end[-1] == '_') // remove trailing underscores
	{
		*(--end) = '\0';
		len--;
	}

	if (len > 2 && end[-2] == '_')
	{
		switch (end[-1])
		{
		case 'd': usage = 'd'; break;
		case 'n': usage = 'n'; break;
		case 's': usage = 's'; break;
		case 'e': usage = 'e'; break;
		}
	}
	else if (len > 3 && end[-3] == '_')
	{
		if      (strcmp(&end[-3], "_dm") == 0) { usage = 'd'; }
		else if (strcmp(&end[-3], "_nm") == 0) { usage = 'n'; }
		else if (strcmp(&end[-3], "_sm") == 0) { usage = 's'; }
		else if (strcmp(&end[-3], "_em") == 0) { usage = 'e'; }
		else if (strcmp(&end[-3], "_ap") == 0) { usage = 'a'; } // only used in cloudhat, not sure what this is for
	}
	else if (len > 4 && end[-4] == '_')
	{
		if      (strcmp(&end[-4], "_nrm") == 0) { usage = 'n'; }
		else if (strcmp(&end[-4], "_pal") == 0) { usage = 'p'; }
	}
	else if (len > 5 && end[-5] == '_')
	{
		if      (strcmp(&end[-5], "_diff") == 0) { usage = 'd'; }
		else if (strcmp(&end[-5], "_norm") == 0) { usage = 'n'; }
		else if (strcmp(&end[-5], "_bump") == 0) { usage = 'n'; }
		else if (strcmp(&end[-5], "_spec") == 0) { usage = 's'; }
	}
	else if (len > 6 && end[-6] == '_')
	{
		if (strcmp(&end[-6], "_alpha") == 0) { usage = 'a'; }
	}
	else if (len > 7 && end[-7] == '_')
	{
		if (strcmp(&end[-7], "_normal") == 0) { usage = 'n'; }
	}
	else if (len > 8 && end[-8] == '_')
	{
		if      (strcmp(&end[-8], "_diffuse") == 0) { usage = 'd'; }
		else if (strcmp(&end[-8], "_wrinkle") == 0) { usage = 'n'; }
	}

	if (usage == '-') // still unspecified? try searching for substrings ..
	{
		if      (strstr(name, "_diff_0"          )) { usage = 'd'; }
		else if (strstr(name, "_normal_0"        )) { usage = 'n'; }
		else if (strstr(name, "_wrinkle_0"       )) { usage = 'n'; }
		else if (strstr(name, "_wrinkle_a"       )) { usage = 'n'; }
		else if (strstr(name, "_wrinkle_b"       )) { usage = 'n'; }
		else if (strstr(name, "_wrinkle_mask_"   )) { usage = 'd'; }
		else if (strstr(name, "_spec_0"          )) { usage = 's'; }
	}

	if (usage == '-') // maybe a digit followed by a suffix (no underscore)
	{
		if (len > 2 && isdigit(end[-2]))
		{
			switch (end[-1])
			{
			case 'n': usage = 'n'; break;
			case 's': usage = 's'; break;
			}
		}
		else if (len > 5 && isdigit(end[-5]))
		{
			if (strcmp(&end[-4], "bump") == 0)
			{
				usage = 'n';
			}
		}
	}

	if (usage == '-' || usage == 'a') // specific normal map textures which don't correspond to any naming convention (incomplete list)
	{
		if (strrchr(name_, '/'))
		{
			name_ = strrchr(name_, '/') + 1;
		}

		if (0) {}
		else if (strcmp(name_, "normal_flat_small"                                      ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "barrelchair1"                                           ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "strip3chair4"                                           ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "clothnorm"                                              ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "prop_hatchings_01"                                      ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "wettest_nwettest_n_a"                                   ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "rsnos_tenniscourtwall"                                  ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "ah_goldsurroundn"                                       ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "bh1_45_windvain_b"                                      ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "ih_ch02_drivewayn"                                      ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "ih_ch102_rooftilesn"                                    ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "ih_concplatesn"                                         ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "ih_rsn_hibrickn"                                        ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "ih_drivemessn"                                          ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "ih_pavementn"                                           ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "ih_plasterdecaln"                                       ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "ih_waterprotectwoodn"                                   ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "ih_woodprimaryplankwhiten"                              ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "ih_woodplanksn"                                         ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "ih_woodprimarysupportsn"                                ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "ih_robliqourgooshn"                                     ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "dh_ch02_floorpanelstrailern"                            ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "id1_27_rntg_corr_0001_p"                                ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "id2_12_rntg_corr_0003_nsc1_04_rnmo_paintoverlay_a"      ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "ss_v_distort4_nrmnt_sew_conc16b_a"                      ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "os_mlgy_metal_bridge_0001_nos_mlcr_grime_bridges_0001_a") == 0) { usage = 'n'; }
		else if (strcmp(name_, "os_ml_bhroof_0002_n_highres"                            ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "og_maliburock2_dr"                                      ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "cs_rsn_sl_bchsand_04_normalt"                           ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "cs_rsn_sl_freewaycracks_001_b"                          ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "cs_rsn_og_ged_scrub_ndt1_21_rsd_dr_dirtmask_a"          ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "cs1_07_ih_rustedmetalsupportn"                          ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "cs6_06_slod_n_8a"                                       ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "ch_02_ih_tilewalln"                                     ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "ch_02_whitewoodn"                                       ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "ch1_1_reb_blend_01_b"                                   ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "ch1_02_ih_woodslatsn"                                   ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "ch1_09_claddingn"                                       ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "ch1_09_woodpaneln"                                      ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "ch2_09b_ih_pavement_hn"                                 ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "xj_v_backp2"                                            ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "xj_v_bpack02"                                           ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "xj_v_cloth02"                                           ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "xj_v_jers02"                                            ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "xj_v_vest02"                                            ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "xj_v_scuff01"                                           ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "e2_glasslumpsn"                                         ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "e2_gaypinkseatsn"                                       ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "gz_v_cars_floor_tiles"                                  ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "gz_v_cf_spills_nnt_sew_spills3"                         ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "gz_v_cf_spills_nnt_sew_ground01_opac"                   ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "gtav_michael_head_wm1_normal_map_001"                   ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "gtav_michael_head_wm2_normal_map_001"                   ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "sc1_21_rt_lowall_01_b"                                  ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "sc1_21_rt_resgrd_d_02_b"                                ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "sc1_22_rt_spdbump_01_b"                                 ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "sl_blockpave02_b"                                       ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "sm1_11_rsn_cs_blueribbedmetalsn"                        ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "ss1_14_ja_rn_chatfoyer_tile_b"                          ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "hw_1_9_carparkwall2_b"                                  ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "hw_1_9_ja_rn_officebits_b"                              ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "hw_1_9_ja_rn_officewin_b"                               ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "ja_rn_conc_wall_b"                                      ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "ja_rn_poured_conc_b"                                    ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "mc_fd_1nyp01_b"                                         ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "rb_marble01_b"                                          ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "sc1_rnad_concrete_01_n_resized"                         ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "id1_34_rustironwork_m"                                  ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "gm_id2_din8"                                            ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "gm_id2_din10"                                           ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "ss_nt_sew_rock2big_nrmnt_sew_rock2_opac"                ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "sc1_22_rt_cnbase_b_01"                                  ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "ih_rsn_shoprockgunkn"                                   ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "golf_ball_normals"                                      ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "bh1_15_dc_bbros5win_nfixscrsn"                          ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "bh1_15_dc_bbros5_nfixscrsn"                             ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "bh1_15_beer1_n__fixscrsn"                               ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "bh1_15_beer1_s__fixscrsn"                               ) == 0) { usage = 's'; } // specular map
		else if (strcmp(name_, "rnbj_wall_012_nv"                                       ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "rsnos_pirate_branding2_ns"                              ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "bh1_03_wall01_nmr"                                      ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "bh1_03_windows01_nmr"                                   ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "vb1_21_rsn_rb_wall5_nmr"                                ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "vb1_21_rsn_rb_brownwall_nmp"                            ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "p_mi_bbqfood_nmp_mi_bbqfood_tm"                         ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "vehicle_generic_worn_chrome_bmp"                        ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "gz_v_ss_brick06_osn"                                    ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "ap1_04_airboard1bump"                                   ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "ap1_04_canop4bump"                                      ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "ap1_04_nums1bump"                                       ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "ld_roof_brown_bmpet_garagedoor5_sdirtyrust512bump"      ) == 0) { usage = 'n'; }
		else if (strcmp(name_, "nt_tc_metdetails1_sp"                                   ) == 0) { usage = 's'; } // specular map
		else if (strcmp(name_, "ntmp_server01_sp"                                       ) == 0) { usage = 's'; } // specular map
		else if (strcmp(name_, "ntmp_metal01_sp"                                        ) == 0) { usage = 's'; } // specular map
		else if (strcmp(name_, "vehicle_generic_worn_chrome_sp"                         ) == 0) { usage = 's'; } // specular map
		else if (strcmp(name_, "vehicle_generic_worn_spc"                               ) == 0) { usage = 's'; } // specular map
		else if (strcmp(name_, "hw_1_4_ja_alleybits_spc"                                ) == 0) { usage = 's'; } // specular map
		else if (strcmp(name_, "rsn_am_window_glass_0001_sv"                            ) == 0) { usage = 's'; } // specular map
		else if (strcmp(name_, "sc1_15_rsn_ns_metal_0001"                               ) == 0) { usage = 's'; } // specular map
		else if (strcmp(name_, "dt1_22_os_cardwin_a"                                    ) == 0) { usage = 's'; } // specular map
		else if (strcmp(name_, "dt1_22_os_figedge"                                      ) == 0) { usage = 's'; } // specular map
		else if (strcmp(name_, "prop_glass_02_a3"                                       ) == 0) { usage = 's'; } // specular map
		else if (strcmp(name_, "prop_wire_01_a"                                         ) == 0) { usage = 's'; } // specular map
		else if (strcmp(name_, "prop_egg_alpha"                                         ) == 0) { usage = 's'; } // specular map
		else if (strcmp(name_, "prop_bin_01_b"                                          ) == 0) { usage = 's'; } // specular map
		else if (strcmp(name_, "prop_trolly_bars_a_t"                                   ) == 0) { usage = 's'; } // specular map
		else if (strcmp(name_, "prop_table_07"                                          ) == 0) { usage = 's'; } // specular map
		else if (strcmp(name_, "prop_metal_ridged02"                                    ) == 0) { usage = 's'; } // specular map
		else if (strcmp(name_, "prop_news_disp_02a_glass"                               ) == 0) { usage = 's'; } // specular map
		else if (strcmp(name_, "prop_busstop_1_glass"                                   ) == 0) { usage = 's'; } // specular map
		else if (strcmp(name_, "prop_busstop_2_glass"                                   ) == 0) { usage = 's'; } // specular map
		else if (strcmp(name_, "prop_busstop_3_glass"                                   ) == 0) { usage = 's'; } // specular map
		else if (strcmp(name_, "prop_busstop_4_glass"                                   ) == 0) { usage = 's'; } // specular map
	}

	return usage;
}

template <typename T> static void CDTVStreamingIteratorTest_Texture(T& store, int slot, int index, const grcTexture* texture)
{
	if (texture)
	{
		grcTexture::eTextureSwizzle swizzle[4];
		texture->GetTextureSwizzle(swizzle[0], swizzle[1], swizzle[2], swizzle[3]);
		const grcImage::Format format = (grcImage::Format)texture->GetImageFormat();
		const char* swizzleTable = "RGBA01";
		const atVarString desc(
			"%s%s%s %s-%c%c%c%c %dx%d mips=%d,", // use comma separator so list can be sorted by error message as csv file
			CDTVStreamingIteratorTest_GetRPFPathName<T>(store, slot),
			CDTVStreamingIteratorTest_GetAssetDesc<T>(store, slot, index).c_str(),
			GetFriendlyTextureName(texture).c_str(),
			grcImage::GetFormatString(format),
			swizzleTable[swizzle[0]],
			swizzleTable[swizzle[1]],
			swizzleTable[swizzle[2]],
			swizzleTable[swizzle[3]],
			texture->GetWidth(),
			texture->GetHeight(),
			texture->GetMipMapCount()
		);

		atString conversionFlagsStr;

		if (texture->GetConversionFlags() & TEXTURE_CONVERSION_FLAG_PROCESSED        ) { if (conversionFlagsStr.length() > 0) { conversionFlagsStr += "+"; } conversionFlagsStr += "PROCESSED"   ; }
		if (texture->GetConversionFlags() & TEXTURE_CONVERSION_FLAG_SKIPPED          ) { if (conversionFlagsStr.length() > 0) { conversionFlagsStr += "+"; } conversionFlagsStr += "SKIPPED"     ; }
		if (texture->GetConversionFlags() & TEXTURE_CONVERSION_FLAG_FAILED_PROCESSING) { if (conversionFlagsStr.length() > 0) { conversionFlagsStr += "+"; } conversionFlagsStr += "FAILED"      ; }
		if (texture->GetConversionFlags() & TEXTURE_CONVERSION_FLAG_INVALID_METADATA ) { if (conversionFlagsStr.length() > 0) { conversionFlagsStr += "+"; } conversionFlagsStr += "INVALID_META"; }
		if (texture->GetConversionFlags() & TEXTURE_CONVERSION_FLAG_OPTIMISED_DXT    ) { if (conversionFlagsStr.length() > 0) { conversionFlagsStr += "+"; } conversionFlagsStr += "OPTIMISED"   ; }
		if (texture->GetConversionFlags() & 0x07                                     ) { if (conversionFlagsStr.length() > 0) { conversionFlagsStr += "+"; } conversionFlagsStr += "UNKNOWN_FLAG"; }

		Vec4V colourMin =  Vec4V(V_FLT_MAX);
		Vec4V colourMax = -Vec4V(V_FLT_MAX);
		Vec4V colourAvg =  Vec4V(V_ZERO);

		Vec4V colourDiff_RG_GB_BA_AR = Vec4V(V_ZERO);
		Vec2V colourDiff_RB_GA       = Vec2V(V_ZERO);

		bool bTextureLooksLikeNormalMap = false;

		// only call GetTextureAverageColour if we need to ..
		if ((gStreamingIteratorTest_FindBadNormalMaps && gStreamingIteratorTest_FindBadNormalMapsCheckAvgColour) ||
			(gStreamingIteratorTest_FindUnusedChannels))
		{
			colourAvg = AssetAnalysis::GetTextureAverageColour(texture, &colourMin, &colourMax, &colourDiff_RG_GB_BA_AR, &colourDiff_RB_GA);

			const ScalarV tolerance(1.0f/16.0f);
			bTextureLooksLikeNormalMap = (IsLessThanOrEqualAll(MaxElement(Abs(colourAvg.GetXYZ() - Vec3V(0.5f, 0.5f, 1.0f))), tolerance) != 0);
		}

		if (gStreamingIteratorTest_FindBadNormalMaps)
		{
			const atString textureName = GetFriendlyTextureName(texture);
			bool bTextureSwizIsNormalMap = false;
			bool bTextureNameIsNormalMap = false;
			//bool bTextureNameIsSpecularMap = false;	// TODO: commented out as a compile fix ("unused local var"), uncomment if needed

			const char usage = TextureName_GetUsage(textureName.c_str());

			if (usage == 'n')
			{
				bTextureNameIsNormalMap = true;
			}
			else if (usage == 's')
			{
				//bTextureNameIsSpecularMap = true; // TODO -- check that format is DXT1-GGG1, unless it's a ped texture
			}

			if (swizzle[0] == grcTexture::TEXTURE_SWIZZLE_R &&
				swizzle[1] == grcTexture::TEXTURE_SWIZZLE_G &&
				swizzle[2] == grcTexture::TEXTURE_SWIZZLE_1 &&
				swizzle[3] == grcTexture::TEXTURE_SWIZZLE_1 &&
				format == grcImage::DXT1)
			{
				if (gStreamingIteratorTest_Verbose)
				{
					Displayf("  %s is a DXT1 normal map [%s]", desc.c_str(), conversionFlagsStr.c_str());
				}

				bTextureSwizIsNormalMap = true;
			}
			else
			if (swizzle[1] == grcTexture::TEXTURE_SWIZZLE_A && // assume either RA11 or GA11
				swizzle[2] == grcTexture::TEXTURE_SWIZZLE_1 &&
				swizzle[3] == grcTexture::TEXTURE_SWIZZLE_1 &&
				format == grcImage::DXT5)
			{
				if (gStreamingIteratorTest_Verbose)
				{
					Displayf("  %s is a DXT5 high quality normal map [%s]", desc.c_str(), conversionFlagsStr.c_str());
				}

				bTextureSwizIsNormalMap = true;
			}

			if (bTextureNameIsNormalMap)
			{
				if (!bTextureSwizIsNormalMap)
				{
					Displayf("! %s name appears to be a normal map but format is inconsistent [%s]", desc.c_str(), conversionFlagsStr.c_str());
				}

				if (gStreamingIteratorTest_FindBadNormalMapsCheckAvgColour && !bTextureLooksLikeNormalMap)
				{
					Displayf(
						"! %s name appears to be a normal map but but avg colour is {%.2f %.2f %.2f %.2f} [%s]",
						desc.c_str(),
						VEC4V_ARGS(colourAvg),
						conversionFlagsStr.c_str()
					);
				}
			}
			else
			{
				if (gStreamingIteratorTest_FindBadNormalMapsCheckAvgColour && bTextureLooksLikeNormalMap)
				{
					Displayf(
						"! %s name does not appear to be a normal map but but avg colour is {%.2f %.2f %.2f %.2f} [%s]",
						desc.c_str(),
						VEC4V_ARGS(colourAvg),
						conversionFlagsStr.c_str()
					);
				}
			}
		}

		if (gStreamingIteratorTest_FindUnusedChannels) // TODO -- add 'gStreamingIteratorTest_FindUnusedChannels'
		{
			const float threshold1 = 1.0f/99.0f; // threshold for black/white channels
			const float threshold2 = 1.0f/64.0f; // threshold for colour variation

			const float rMin = colourMin.GetXf();
			const float gMin = colourMin.GetYf();
			const float bMin = colourMin.GetZf();
			const float aMin = colourMin.GetWf();
			const float rMax = colourMax.GetXf();
			const float gMax = colourMax.GetYf();
			const float bMax = colourMax.GetZf();
			const float aMax = colourMax.GetWf();

			if (format == grcImage::DXT3 || format == grcImage::DXT5)
			{
				if (aMin >= 1.0f - threshold1)
				{
					Displayf("! %s does not require alpha channel - should be DXT1 [%s]", desc.c_str(), conversionFlagsStr.c_str());
				}

				if (MaxElement((colourMax - colourMin).GetXYZ()).Getf() <= threshold2)
				{
					Displayf(
						"! %s has very little colour variation - should be alpha-only? min={%.2f %.2f %.2f,%.2f} max={%.2f %.2f %.2f,%.2f} [%s]",
						desc.c_str(),
						rMin,gMin,bMin,aMin,
						rMax,gMax,bMax,aMax,
						conversionFlagsStr.c_str()
					);
				}
			}

			if (gStreamingIteratorTest_FindUnusedChannelsSwizzle) // how many channels could be handled by swizzle remapping? (this has too many false positives ..)
			{				
				const bool r0 = (rMax - rMin) <= threshold1 && Abs<float>(rMax + rMin - 1.0f) >= 1.0f - threshold1; // red is all nearly 0 or 1
				const bool g0 = (gMax - gMin) <= threshold1 && Abs<float>(gMax + gMin - 1.0f) >= 1.0f - threshold1; // green is all nearly 0 or 1
				const bool b0 = (bMax - bMin) <= threshold1 && Abs<float>(bMax + bMin - 1.0f) >= 1.0f - threshold1; // blue is all nearly 0 or 1
				const bool a0 = (aMax - aMin) <= threshold1 && Abs<float>(aMax + aMin - 1.0f) >= 1.0f - threshold1; // alpha is all nearly 0 or 1

				const bool rg = colourDiff_RG_GB_BA_AR.GetXf() <= threshold2; // red and green are nearly identical
				const bool rb = colourDiff_RB_GA      .GetXf() <= threshold2; // red and blue are nearly identical
				const bool ra = colourDiff_RG_GB_BA_AR.GetWf() <= threshold2; // red and alpha are nearly identical
				const bool gb = colourDiff_RG_GB_BA_AR.GetYf() <= threshold2; // green and blue are nearly identical
				const bool ga = colourDiff_RB_GA      .GetYf() <= threshold2; // green and alpha are nearly identical
				const bool ba = colourDiff_RG_GB_BA_AR.GetZf() <= threshold2; // blue and alpha are nearly identical

				int numUniqueChannels = 4;

				if (r0 || rg || rb || ra) { numUniqueChannels--; } // red channel is not unique
				if (g0 || gb || ga      ) { numUniqueChannels--; } // green channel is not unique
				if (b0 || ba            ) { numUniqueChannels--; } // blue channel is not unique
				if (a0                  ) { numUniqueChannels--; } // alpha channel is not unique

				int numTextureChannels = 0;

				if (0) // this doesn't actually work, since for example DXT5-GA11 is only using 2 channels but "DXT5" implies 4 channels ..
				{
					switch (format)
					{
					case grcImage::DXT1    : numTextureChannels = 3; break;
					case grcImage::DXT3    : numTextureChannels = 4; break;
					case grcImage::DXT5    : numTextureChannels = 4; break;
					case grcImage::CTX1    : numTextureChannels = 2; break;
					case grcImage::DXN     : numTextureChannels = 2; break;
					case grcImage::A8L8    : numTextureChannels = 2; break;
					case grcImage::R5G6B5  : numTextureChannels = 3; break;
					case grcImage::A1R5G5B5: numTextureChannels = 3; break;
					case grcImage::A4R4G4B4: numTextureChannels = 4; break;
					case grcImage::A8R8G8B8: numTextureChannels = 4; break;
					case grcImage::A8B8G8R8: numTextureChannels = 4; break;
					default                : numTextureChannels = 0; break; // other formats are uncommon, or only have one channel
					}
				}
				else // count the unique channels in the swizzle
				{
					for (int chan = grcTexture::TEXTURE_SWIZZLE_R; chan <= grcTexture::TEXTURE_SWIZZLE_A; chan++)
					{
						if (swizzle[0] == chan ||
							swizzle[1] == chan ||
							swizzle[2] == chan ||
							swizzle[3] == chan)
						{
							numTextureChannels++;
						}
					}
				}

				if (numUniqueChannels < numTextureChannels)
				{
					Displayf(
						"! %s has colour variation in %d channel%s only but texture uses %d channel%s [%s]",
						desc.c_str(),
						numUniqueChannels,
						numUniqueChannels == 1 ? "" : "s",
						numTextureChannels,
						numTextureChannels == 1 ? "" : "s",
						conversionFlagsStr.c_str()
					);					
				}
			}
		}

		if (gStreamingIteratorTest_FindUncompressedTextures)
		{
			if (!grcImage::IsFormatDXTBlockCompressed((grcImage::Format)texture->GetImageFormat()))
			{
				Displayf("! %s is not compressed [%s]", desc.c_str(), conversionFlagsStr.c_str());
			}
		}

		if (gStreamingIteratorTest_FindUnprocessedTextures)
		{
			if (texture->GetConversionFlags() != TEXTURE_CONVERSION_FLAG_PROCESSED)
			{
				Displayf("! %s was not processed normally [%s]", desc.c_str(), conversionFlagsStr.c_str());
			}
		}

		if (gStreamingIteratorTest_FindConstantTextures)
		{
			const int w = texture->GetWidth ();
			const int h = texture->GetHeight();

			if ((w > 16 || h > 16) && IsTextureConstant(texture))
			{
				Displayf("! %s has no variation in top-level mip [%s]", desc.c_str(), conversionFlagsStr.c_str());
			}
		}
	}
}

#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEXTURE

// ================================================================================================

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_REDUNDANT

static void CDTVStreamingIteratorTest_FindRedundantTextures(atMap<u32,atArray<atString> >& map, int parentTxdSlot)
{
	while (parentTxdSlot != INDEX_NONE)
	{
		const fwTxd*   parentTxd  = g_TxdStore.GetSafeFromIndex(parentTxdSlot);
		const atString parentDesc = CDTVStreamingIteratorTest_GetAssetPath(g_TxdStore, parentTxdSlot, INDEX_NONE);

		for (int k = 0; k < parentTxd->GetCount(); k++)
		{
			const grcTexture* texture = parentTxd->GetEntry(k);

			if (texture)
			{
				const u32 textureHash = GetTextureHash(texture, !gStreamingIteratorTest_FindRedundantTexturesByHash, gStreamingIteratorTest_FindRedundantTexturesByHash ? -1 : 0);
				atString  textureDesc = parentDesc;

				textureDesc += atVarString("%s.dds (%s %dx%d)", GetFriendlyTextureName(texture).c_str(), grcImage::GetFormatString((grcImage::Format)texture->GetImageFormat()), texture->GetWidth(), texture->GetHeight());

				if (map.Access(textureHash))
				{
					map[textureHash].PushAndGrow(textureDesc);
				}
			}
		}

		parentTxdSlot = g_TxdStore.GetParentTxdSlot(parentTxdSlot);
	}

	for (atMap<u32,atArray<atString> >::Iterator iter = map.CreateIterator(); iter; ++iter)
	{
		const atArray<atString>& data = iter.GetData();

		if (data.GetCount() > 1)
		{
			Displayf("multiple textures found with hash=0x%08X", iter.GetKey());

			for (int k = 0; k < data.GetCount(); k++)
			{
				Displayf("  %s", data[k].c_str());
			}
		}
	}
}

static void CDTVStreamingIteratorTest_FindRedundantTextures_TxdStore(int slot)
{
	const fwTxd* txd = g_TxdStore.Get(slot);
	atMap<u32,atArray<atString> > map;
	bool bHasTextures = false;

	const atString assetPath = CDTVStreamingIteratorTest_GetAssetPath(g_TxdStore, slot, INDEX_NONE);

	for (int k = 0; k < txd->GetCount(); k++)
	{
		const grcTexture* texture = txd->GetEntry(k);

		if (texture)
		{
			const u32 textureHash = GetTextureHash(texture, !gStreamingIteratorTest_FindRedundantTexturesByHash, gStreamingIteratorTest_FindRedundantTexturesByHash ? -1 : 0);
			atString  textureDesc = assetPath;

			textureDesc += atVarString("%s.dds (%s %dx%d)", GetFriendlyTextureName(texture).c_str(), grcImage::GetFormatString((grcImage::Format)texture->GetImageFormat()), texture->GetWidth(), texture->GetHeight());

			map[textureHash].PushAndGrow(textureDesc);
			bHasTextures = true;
		}
	}

	if (bHasTextures)
	{
		CDTVStreamingIteratorTest_FindRedundantTextures(map, g_TxdStore.GetParentTxdSlot(slot));
	}
}

static void CDTVStreamingIteratorTest_FindRedundantTextures_DwdStore(int slot)
{
	const Dwd* dwd = g_DwdStore.Get(slot);
	atMap<u32,atArray<atString> > map;
	bool bHasTextures = false;

	for (int i = 0; i < dwd->GetCount(); i++)
	{
		const atString assetPath = CDTVStreamingIteratorTest_GetAssetPath(g_DwdStore, slot, i);

		const fwTxd* txd = dwd->GetEntry(i)->GetTexDictSafe();

		if (txd)
		{
			for (int k = 0; k < txd->GetCount(); k++)
			{
				const grcTexture* texture = txd->GetEntry(k);

				if (texture)
				{
					const u32 textureHash = GetTextureHash(texture, !gStreamingIteratorTest_FindRedundantTexturesByHash, gStreamingIteratorTest_FindRedundantTexturesByHash ? -1 : 0);
					atString  textureDesc = assetPath;

					textureDesc += atVarString("%s.dds (%s %dx%d)", GetFriendlyTextureName(texture).c_str(), grcImage::GetFormatString((grcImage::Format)texture->GetImageFormat()), texture->GetWidth(), texture->GetHeight());

					map[textureHash].PushAndGrow(textureDesc);
					bHasTextures = true;
				}
			}
		}
	}

	if (bHasTextures)
	{
		CDTVStreamingIteratorTest_FindRedundantTextures(map, g_DwdStore.GetParentTxdForSlot(slot));
	}
}

static void CDTVStreamingIteratorTest_FindRedundantTextures_DrawableStore(int slot)
{
	const Drawable* drawable = g_DrawableStore.Get(slot);
	atMap<u32,atArray<atString> > map;
	bool bHasTextures = false;

	const fwTxd* txd = drawable->GetTexDictSafe();

	if (txd)
	{
		const atString assetPath = CDTVStreamingIteratorTest_GetAssetPath(g_DrawableStore, slot, INDEX_NONE);

		for (int k = 0; k < txd->GetCount(); k++)
		{
			const grcTexture* texture = txd->GetEntry(k);

			if (texture)
			{
				const u32 textureHash = GetTextureHash(texture, !gStreamingIteratorTest_FindRedundantTexturesByHash, gStreamingIteratorTest_FindRedundantTexturesByHash ? -1 : 0);
				atString  textureDesc = assetPath;

				textureDesc += atVarString("%s.dds (%s %dx%d)", GetFriendlyTextureName(texture).c_str(), grcImage::GetFormatString((grcImage::Format)texture->GetImageFormat()), texture->GetWidth(), texture->GetHeight());

				map[textureHash].PushAndGrow(textureDesc);
				bHasTextures = true;
			}
		}
	}

	if (bHasTextures)
	{
		CDTVStreamingIteratorTest_FindRedundantTextures(map, g_DrawableStore.GetParentTxdForSlot(slot));
	}
}

static void CDTVStreamingIteratorTest_FindRedundantTextures_FragmentStore(int slot)
{
	const Fragment* fragment = g_FragmentStore.Get(slot);
	atMap<u32,atArray<atString> > map;
	bool bHasTextures = false;

	const fwTxd* txd = fragment->GetCommonDrawable()->GetTexDictSafe();

	if (txd)
	{
		const atString assetPath = CDTVStreamingIteratorTest_GetAssetPath(g_DrawableStore, slot, INDEX_NONE);

		for (int k = 0; k < txd->GetCount(); k++)
		{
			const grcTexture* texture = txd->GetEntry(k);

			if (texture)
			{
				const u32 textureHash = GetTextureHash(texture, !gStreamingIteratorTest_FindRedundantTexturesByHash, gStreamingIteratorTest_FindRedundantTexturesByHash ? -1 : 0);
				atString  textureDesc = assetPath;

				textureDesc += atVarString("%s.dds (%s %dx%d)", GetFriendlyTextureName(texture).c_str(), grcImage::GetFormatString((grcImage::Format)texture->GetImageFormat()), texture->GetWidth(), texture->GetHeight());

				map[textureHash].PushAndGrow(textureDesc);
				bHasTextures = true;
			}
		}
	}

	if (bHasTextures)
	{
		CDTVStreamingIteratorTest_FindRedundantTextures(map, g_FragmentStore.GetParentTxdForSlot(slot));
	}
}

#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_REDUNDANT

// ================================================================================================

CDTVStreamingIteratorTest_TxdStore::CDTVStreamingIteratorTest_TxdStore()
{
	sysMemSet(this, 0, sizeof(*this));
}

void CDTVStreamingIteratorTest_TxdStore::IteratorFunc(fwTxdDef* def, bool, int slot, int frames)
{
	if (slot == -1) // update finished
	{
		return;
	}

	fwTxd* txd = def ? def->m_pObject : NULL;

	if (g_TxdStore.GetSlot(slot)->m_isDummy)
	{
		return;
	}

	const char* txdName = g_TxdStore.GetName(slot);

	if (stricmp(txdName, "startup") == 0 || // don't care about these ..
		stricmp(txdName, "introscrn") == 0)
	{
		return;
	}

	if (txd)
	{
		if (gStreamingIteratorTest_BuildAssetAnalysisData)
		{
			AssetAnalysis::ProcessTxdSlot(slot);
			return;
		}

		if (gStreamingIteratorTest_FindUncompressedTexturesNew)
		{
			CDTVStreamingIteratorTest_CheckAssetForUncompressedTextures(g_TxdStore, slot, INDEX_NONE, g_TxdStore.Get(slot));
			return;
		}

		if (gStreamingIteratorTest_FindTextureInDrawables[0] != '\0')
		{
			CDTVStreamingIteratorTest_CheckAssetForTexture(g_TxdStore, slot, INDEX_NONE, NULL, gStreamingIteratorTest_FindTextureInDrawables);
			return;
		}

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEXTURE
		for (int k = 0; k < txd->GetCount(); k++)
		{
			CDTVStreamingIteratorTest_Texture(g_TxdStore, slot, INDEX_NONE, txd->GetEntry(k));
		}
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEXTURE

		int parentTxdIndex = INDEX_NONE;
		int parentTxdSlot  = CDTVStreamingIteratorTest_GetParentTxdForSlot(g_TxdStore, slot);

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TXDCHILDREN
		if (gStreamingIteratorTest_TxdChildren)
		{
			gStreamingIteratorTest_TxdChildrenMap.SetParentTxdSlot_TxdStore(slot, parentTxdSlot);
		}
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TXDCHILDREN

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_REDUNDANT
		if (gStreamingIteratorTest_FindRedundantTextures)
		{
			CDTVStreamingIteratorTest_FindRedundantTextures_TxdStore(slot);
		}
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_REDUNDANT

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS
		if (gStreamingIteratorTest_BuildTextureUsageMap)
		{
#if !DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS_ONCE
			CDTVStreamingIteratorTest_WarnAboutUniqueParentTxds(g_TxdStore, slot);
			CDTVStreamingIteratorTest_WarnAboutTxdHasNoDrawableChildren(slot);
#endif // !DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS_ONCE

			CDTVStreamingIteratorTest_BuildTextureUsageMaps_TxdStore(slot);
		}
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_FIND_UNUSED
		if (gStreamingIteratorTest_FindUnusedSharedTextures)
		{
			gStreamingIteratorTest_FindUnusedSharedTexturesData.CheckTxdSlot(slot);
		}
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_FIND_UNUSED

		// find parent
		{
			const fwTxd* parentTxd = txd->GetParent();

			if (parentTxd)
			{
				for (int i = 0; i < g_TxdStore.GetSize(); i++)
				{
					if (g_TxdStore.GetSafeFromIndex(i) == parentTxd && !g_TxdStore.GetSlot(i)->m_isDummy)
					{
						parentTxdIndex = i;
						break;
					}
				}

				Assertf(parentTxdIndex != INDEX_NONE, "parent txd not found!");
			}
		}

		m_numTxds           += 1;
		m_numStreamed       += (frames > 0) ? 1 : 0;
		m_numTxdParents     += (parentTxdIndex != INDEX_NONE) ? 1 : 0;
		m_numTxdParentSlots += (parentTxdSlot  != INDEX_NONE) ? 1 : 0;
		m_numNonEmptyTxds   += (txd->GetCount() > 0) ? 1 : 0;
		m_numTextures       += txd->GetCount();
		m_maxTextures        = Max<int>(txd->GetCount(), m_maxTextures);

		if (gStreamingIteratorTest_Verbose)
		{
			atString temp = frames > 0 ? atVarString(" [STREAMED IN %d FRAMES]", frames) : "";
			atString parents;

			if (parentTxdIndex != INDEX_NONE)
			{
				if (parents.length() > 0)
				{
					parents += ",";
				}

				parents += atVarString("Txd:%d(%s)", parentTxdIndex, g_TxdStore.GetName(parentTxdIndex));
			}

			if (parentTxdSlot != INDEX_NONE && parentTxdSlot != parentTxdIndex)
			{
				if (parents.length() > 0)
				{
					parents += ",";
				}

				parents += atVarString("Txd_SLOT:%d(%s)", parentTxdSlot, g_TxdStore.GetName(parentTxdSlot));
			}

			if (parentTxdSlot != parentTxdIndex)
			{
				parents += "wtf?!"; // please no.
			}

			if (parents.length() > 0)
			{
				parents = atVarString(" [parents = %s]", parents.c_str());
			}
			else
			{
				parents = "";
			}

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_RPFTEST
			strIndex          streamingIndex = g_TxdStore.GetStreamingIndex(slot);
			strStreamingInfo* pInfo          = strStreamingEngine::GetInfo().GetStreamingInfo(streamingIndex);
			strStreamingFile* pFile          = strPackfileManager::GetImageFileFromHandle(pInfo->GetHandle());

			temp += atVarString(" [RPF=\"%s\"]", pFile ? pFile->m_packfile->GetDebugName() : "NULL");
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_RPFTEST

			siLog(
				">> g_TxdStore[%d] = \"%s\" has %d textures%s%s",
				slot,
				CDTVStreamingIteratorTest_GetAssetPath(g_TxdStore, slot, INDEX_NONE).c_str(),
				txd->GetCount(),
				parents.c_str(),
				temp.c_str()
			);
		}
	}
	else
	{
		if (frames < 0)
		{
			if (gStreamingIteratorTest_Verbose)
			{
				siLog(">> g_TxdStore[%d] = \"%s\" not in streaming image!", slot, CDTVStreamingIteratorTest_GetAssetPath(g_TxdStore, slot, INDEX_NONE).c_str());
			}

			m_numNotInImage++;
		}
		else
		{
			if (gStreamingIteratorTest_Verbose || gStreamingIteratorTest_VerboseErrors)
			{
				siLog(">> !ERROR: g_TxdStore[%d] = \"%s\" failed to stream in!", slot, CDTVStreamingIteratorTest_GetAssetPath(g_TxdStore, slot, INDEX_NONE).c_str());
			}

			m_numFailed++;
		}
	}
}

void CDTVStreamingIteratorTest_TxdStore::Reset()
{
	if (m_si)
	{
		m_si->Reset();
		delete m_si;
		m_si = NULL;
	}

	m_enabled           = false;
	m_numTxds           = 0;
	m_numStreamed       = 0;
	m_numTxdParents     = 0;
	m_numTxdParentSlots = 0;
	m_numNonEmptyTxds   = 0;
	m_numTextures       = 0;
	m_maxTextures       = 0;
	m_numFailed         = 0;
	m_numNotInImage     = 0;
}

void CDTVStreamingIteratorTest_TxdStore::Update()
{
	DEBUG_TEXTURE_VIEWER_USE_DEBUG_MEMORY();

	if (!m_enabled)
	{
		if (m_si)
		{
			Reset();
		}

		return;
	}

	if (m_si == NULL)
	{
		m_currentSlot = 0;
		m_si = rage_new CDTVStreamingIterator_TxdStore(this, m_currentSlot, g_TxdStore, _Func, gStreamingIteratorTest_SearchRPF, gStreamingIteratorTest_SearchName, gStreamingIteratorTest_SearchFlags);

		// TODO -- fix these broken assets
		m_si->m_brokenAssetNames.PushAndGrow(atString("Z_Z_HairTest"));

		m_numTxds           = 0;
		m_numStreamed       = 0;
		m_numTxdParents     = 0;
		m_numTxdParentSlots = 0;
		m_numNonEmptyTxds   = 0;
		m_numTextures       = 0;
		m_maxTextures       = 0;
		m_numFailed         = 0;
		m_numNotInImage     = 0;

		siLog(">> STREAMING ITERATOR STARTING .. %d/%d TXD SLOTS", g_TxdStore.GetNumUsedSlots(), g_TxdStore.GetSize());
	}

	if (m_si->Update())
	{
		siLog(">> STREAMING ITERATOR COMPLETE");
		siLog("   txds             : %d"            , m_numTxds                   );
		siLog("   streamed         : %d"            , m_numStreamed               );
		siLog("   txd parents      : %d"            , m_numTxdParents             );
		siLog("   txd parent slots : %d"            , m_numTxdParentSlots         );
		siLog("   txds (non empty) : %d"            , m_numNonEmptyTxds           );
		siLog("   textures         : num=%d, max=%d", m_numTextures, m_maxTextures);
		siLog("   failed           : %d/%d"         , m_numFailed, m_numNotInImage);
	}
}

// =====

CDTVStreamingIteratorTest_DwdStore::CDTVStreamingIteratorTest_DwdStore()
{
	sysMemSet(this, 0, sizeof(*this));
}

template <typename T> void CheckForNameCollisions(T& store, const char* storeName)
{
	const int count = store.GetCount();
	const u32 total = ((u32)count*((u32)count - 1))/2;
	u32 index = 0;
	u32 progress = 1;

	Displayf("checking for name collisions within %s (%d) ..", storeName, count);

	for (int slot1 = 0; slot1 < count; slot1++)
	{
		for (int slot2 = slot1 + 1; slot2 < count; slot2++)
		{
			if (store.IsValidSlot(slot1) &&
				store.IsValidSlot(slot2))
			{
				if (stricmp(store.GetName(slot1), store.GetName(slot2)) == 0)
				{
					siLog(">> !ERROR: %s[%d] \"%s\" is same name as %s[%d] \"%s\"!", storeName, slot1, CDTVStreamingIteratorTest_GetAssetPath(store, slot1, INDEX_NONE).c_str(), storeName, slot2, CDTVStreamingIteratorTest_GetAssetPath(store, slot2, INDEX_NONE).c_str());
				}
			}

			if (20.0f*(float)(++index)/(float)total > (float)progress)
			{
				progress++;

				Displayf("progress = %.2f%% (%d,%d)", 100.0f*(float)index/(float)total, slot1, slot2);
			}
		}
	}

	Displayf("done.");
}

static void CheckForNameCollisionsForAllAssets()
{
	CheckForNameCollisions(g_TxdStore, "g_TxdStore");
	CheckForNameCollisions(g_DwdStore, "g_DwdStore");
	CheckForNameCollisions(g_DrawableStore, "g_DrawableStore");
	CheckForNameCollisions(g_FragmentStore, "g_FragmentStore");
	CheckForNameCollisions(g_ParticleStore, "g_ParticleStore");

	// also check for name collisions between g_DrawableStore and g_FragmentStore, since both of these correspond to archetype names
	{
		const int count1 = g_DrawableStore.GetCount();
		const int count2 = g_FragmentStore.GetCount();
		const u32 total = (u32)count1*(u32)count2;
		u32 index = 0;
		u32 progress = 1;

		Displayf("checking for name collisions between g_DrawableStore (%d) and g_FragmentStore (%d) ..", count1, count2);

		for (u32 slot1 = 0; slot1 < count1; slot1++)
		{
			for (u32 slot2 = 0; slot2 < count2; slot2++)
			{
				if (g_DrawableStore.IsValidSlot(slot1) &&
					g_FragmentStore.IsValidSlot(slot2))
				{
					if (stricmp(g_DrawableStore.GetName(slot1), g_FragmentStore.GetName(slot2)) == 0)
					{
						siLog(">> !ERROR: g_DrawableStore[%d] \"%s\" is same name as g_FragmentStore[%d] \"%s\"!", slot1, CDTVStreamingIteratorTest_GetAssetPath(g_DrawableStore, slot1, INDEX_NONE).c_str(), slot2, CDTVStreamingIteratorTest_GetAssetPath(g_FragmentStore, slot2, INDEX_NONE).c_str());
					}
				}

				if (20.0f*(float)(++index)/(float)total > (float)progress)
				{
					progress++;

					Displayf("progress = %.2f%% (%d,%d)", 100.0f*(float)index/(float)total, slot1, slot2);
				}
			}
		}

		Displayf("done.");
	}
}

void CDTVStreamingIteratorTest_DwdStore::IteratorFunc(fwDwdDef* def, bool, strLocalIndex slot, int frames)
{
	if (slot == -1) // update finished
	{
		return;
	}

	Dwd* dwd = def ? def->m_pObject : NULL;
	//const char* dwdName = g_DwdStore.GetName(slot);

	if (dwd)
	{
		if (gStreamingIteratorTest_BuildAssetAnalysisData)
		{
			AssetAnalysis::ProcessDwdSlot(slot);
			return;
		}

		if (gStreamingIteratorTest_FindUncompressedTexturesNew)
		{
			for (int i = 0; i < dwd->GetCount(); i++)
			{
				CDTVStreamingIteratorTest_CheckAssetForUncompressedTextures(g_DwdStore, slot, i, dwd->GetEntry(i));
			}

			return;
		}

		if (gStreamingIteratorTest_FindShaderInDrawables[0] != '\0')
		{
			for (int i = 0; i < dwd->GetCount(); i++)
			{
				CDTVStreamingIteratorTest_CheckDrawableForShader(g_DwdStore, slot, i, dwd->GetEntry(i), gStreamingIteratorTest_FindShaderInDrawables);
			}

			return;
		}

		if (gStreamingIteratorTest_FindTextureInDrawables[0] != '\0')
		{
			for (int i = 0; i < dwd->GetCount(); i++)
			{
				CDTVStreamingIteratorTest_CheckAssetForTexture(g_DwdStore, slot, i, dwd->GetEntry(i), gStreamingIteratorTest_FindTextureInDrawables);
			}

			return;
		}

		// TEMPORARY
		{
			for (int i = 0; i < dwd->GetCount(); i++)
			{
				const Drawable* drawable = dwd->GetEntry(i);

				if (drawable && drawable->GetShaderGroupPtr() == NULL)
				{
					siLog(">> !ERROR: g_DwdStore[%d] entry %d \"%s\" has NULL shadergroup!", slot, i, CDTVStreamingIteratorTest_GetAssetPath(g_DwdStore, slot, i).c_str());
				}
			}

			for (int i = 0; i < dwd->GetCount(); i++)
			{
				CDTVStreamingIteratorTest_CheckDrawableForAnimUVs(g_DwdStore, slot, i, dwd->GetEntry(i));
			}

			// check for name collisions
			{
				static bool bOnce = true;

				if (0 && bOnce) // TODO -- this is kind of too slow .. could be made faster with hashes
				{
					CheckForNameCollisionsForAllAssets();
					bOnce = false;
				}

				for (int i = 0; i < dwd->GetCount(); i++)
				{
					const atString entryName = CDTVStreamingIteratorTest_GetDwdEntryName(slot, i);

					const int drawableSlot = g_DrawableStore.FindSlot(entryName.c_str());
					const int fragmentSlot = g_FragmentStore.FindSlot(entryName.c_str());

					if (drawableSlot != INDEX_NONE)
					{
						siLog(">> !ERROR: g_DwdStore[%d] entry %d \"%s\" is same name as g_DrawableStore[%d] \"%s\"!", slot, i, CDTVStreamingIteratorTest_GetAssetPath(g_DwdStore, slot, i).c_str(), drawableSlot, CDTVStreamingIteratorTest_GetAssetPath(g_DrawableStore, drawableSlot, INDEX_NONE).c_str());
					}

					if (fragmentSlot != INDEX_NONE)
					{
						siLog(">> !ERROR: g_DwdStore[%d] entry %d \"%s\" is same name as g_FragmentStore[%d] \"%s\"!", slot, i, CDTVStreamingIteratorTest_GetAssetPath(g_DwdStore, slot, i).c_str(), fragmentSlot, CDTVStreamingIteratorTest_GetAssetPath(g_FragmentStore, fragmentSlot, INDEX_NONE).c_str());
					}
				}
			}
		}

		int numDrawables    = dwd->GetCount();
		int numTxds         = 0;
		int numNonEmptyTxds = 0;
		int numTextures     = 0;
		int parentDwdIndex  = INDEX_NONE;
		int parentTxdSlot   = CDTVStreamingIteratorTest_GetParentTxdForSlot(g_DwdStore, slot); // i'm calling this a "slot" to differentiate it from parentTxdIndex in the TxdStore iterator test ..

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TXDCHILDREN
		if (gStreamingIteratorTest_TxdChildren)
		{
			gStreamingIteratorTest_TxdChildrenMap.SetParentTxdSlot_DwdStore(slot, parentTxdSlot);
		}
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TXDCHILDREN

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_REDUNDANT
		if (gStreamingIteratorTest_FindRedundantTextures)
		{
			CDTVStreamingIteratorTest_FindRedundantTextures_DwdStore(slot);
		}
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_REDUNDANT

#if DEBUG_TEXTURE_VIEWER_FIND_INEFFECTIVE_TEXTURES
		if (gStreamingIteratorTest_FindIneffectiveTexturesBumpTex ||
			gStreamingIteratorTest_FindIneffectiveTexturesSpecularTex ||
			gStreamingIteratorTest_FindNonDefaultSpecularMasks)
		{
			for (int i = 0; i < dwd->GetCount(); i++)
			{
				CDTVStreamingIteratorTest_FindIneffectiveTextures(g_DwdStore, slot, i, dwd->GetEntry(i));
			}
		}
#endif // DEBUG_TEXTURE_VIEWER_FIND_INEFFECTIVE_TEXTURES

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS
		if (gStreamingIteratorTest_BuildTextureUsageMap)
		{
			// warn about unique parent txds
			{
#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS_ONCE
				// for txd and drawable, we can check all assets at once
				static bool bOnce = true;

				if (bOnce)
				{
					bOnce = false;
					CDTVStreamingIteratorTest_WarnAboutDrawableHasNoArchetype();
					CDTVStreamingIteratorTest_WarnAboutTxdHasNoDrawableChildren();
					CDTVStreamingIteratorTest_WarnAboutUniqueParentTxds();
				}
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS_ONCE

				// .. but for dwd, we need to check each one individually and verify that textures are not shared between drawables
				CDTVStreamingIteratorTest_WarnAboutDrawableHasNoArchetype(g_DwdStore, slot);
				CDTVStreamingIteratorTest_WarnAboutUniqueParentTxds(g_DwdStore, slot);
			}

			bool bUsesNonPackedTextures = false;

			for (int i = 0; i < dwd->GetCount(); i++)
			{
				CDTVStreamingIteratorTest_BuildTextureUsageMaps(g_DwdStore, slot, i, dwd->GetEntry(i), bUsesNonPackedTextures);
			}

			if (parentTxdSlot != INDEX_NONE && !bUsesNonPackedTextures)
			{
				if (strstr(g_TxdStore.GetName(parentTxdSlot), "mapdetail") == NULL) // don't report unnecessary dependencies on mapdetail, since this gets hooked up globally
				{
					siLog(
						">> [1] \"%s\" only used packed textures but depends on \"%s\"%s",
						CDTVStreamingIteratorTest_GetAssetPath(g_DwdStore, slot, INDEX_NONE).c_str(),
						CDTVStreamingIteratorTest_GetAssetPath(g_TxdStore, parentTxdSlot, INDEX_NONE).c_str(),
						CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsTxdParentChainEmpty(parentTxdSlot) ? " (EMPTY)" : ""
					);
				}
			}
		}
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_FIND_UNUSED
		if (gStreamingIteratorTest_FindUnusedSharedTextures)
		{
			for (int i = 0; i < dwd->GetCount(); i++)
			{
				CDTVStreamingIteratorTest_MarkSharedTextures(g_DwdStore, slot, i, dwd->GetEntry(i));
			}
		}
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_FIND_UNUSED

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_FIND_UNUSED_DRAWABLES_IN_DWDS
		if (gStreamingIteratorTest_FindUnusedDrawablesInDwds)
		{
			u32 flags[1024/32];
			sysMemSet(flags, 0, sizeof(flags));

			for (u32 i = 0; i < CDebugArchetype::GetMaxDebugArchetypeProxies(); i++)
			{
				const CDebugArchetypeProxy* modelInfo = CDebugArchetype::GetDebugArchetypeProxy(i);

				if (modelInfo && modelInfo->SafeGetDrawDictIndex() == slot)
				{
					// since model hasn't necessarily loaded, we can't just call GetDrawDictDrawableIndex ..
					const int j = dwd->LookupLocalIndex(modelInfo->GetHashKey());

					if (j >= 0 && j < dwd->GetCount())
					{
						flags[j/32] |= BIT(j%32);
					}

					Assertf(!modelInfo->GetHasLoaded() || modelInfo->SafeGetDrawDictEntry_IfLoaded() == j, "model has dwd entry %d, LookupLocalIndex returned %d", modelInfo->SafeGetDrawDictEntry_IfLoaded(), j);
				}
			}

			int numDrawablesNotUsed = 0;
			int numBytesNotUsed = 0;

			for (int j = 0; j < dwd->GetCount(); j++)
			{
				if ((flags[j/32] & BIT(j%32)) == 0)
				{
					const Drawable* drawable = dwd->GetEntry(j);

					if (drawable)
					{
						const fwTxd* txd = drawable->GetTexDictSafe();

						if (txd)
						{
							for (int k = 0; k < txd->GetCount(); k++)
							{
								const grcTexture* texture = txd->GetEntry(k);

								if (texture)
								{
									numBytesNotUsed = texture->GetPhysicalSize();
								}
							}
						}
					}

					numDrawablesNotUsed++;
				}
			}

			if (numDrawablesNotUsed > 0)
			{
				Displayf(
					"g_DwdStore[%d] \"%s\" is using only %d of %d drawables! (wasting %.fMB of texture data)",
					slot,
					CDTVStreamingIteratorTest_GetAssetPath(g_DwdStore, slot, INDEX_NONE).c_str(),
					dwd->GetCount() - numDrawablesNotUsed,
					dwd->GetCount(),
					(float)numBytesNotUsed/(1024.0f*1024.0f)
				);

				for (int j = 0; j < dwd->GetCount(); j++)
				{
					if ((flags[j/32] & BIT(j%32)) == 0)
					{
						Displayf("   \"%s\" not used!", CDTVStreamingIteratorTest_GetDwdEntryName(slot, j).c_str());
					}
				}
			}
		}
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_FIND_UNUSED_DRAWABLES_IN_DWDS

		// find parent
		{
			const Dwd* parentDwd = dwd->GetParent();

			if (parentDwd)
			{
				for (int i = 0; i < g_DwdStore.GetSize(); i++)
				{
					if (g_DwdStore.GetSafeFromIndex(i) == parentDwd)
					{
						parentDwdIndex = i;
						break;
					}
				}

				Assertf(parentDwdIndex != INDEX_NONE, "parent dwd not found!");
			}
		}

		for (int i = 0; i < dwd->GetCount(); i++)
		{
			const fwTxd* txd = dwd->GetEntry(i)->GetTexDictSafe();

			if (txd)
			{
#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEXTURE
				for (int k = 0; k < txd->GetCount(); k++)
				{
					CDTVStreamingIteratorTest_Texture(g_DwdStore, slot, i, txd->GetEntry(k));
				}
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEXTURE

				numTxds++;
				numNonEmptyTxds += (txd->GetCount() > 0) ? 1 : 0;
				numTextures     += txd->GetCount();
			}
		}

		m_numDwds           += 1;
		m_numStreamed       += (frames > 0) ? 1 : 0;
		m_numDwdParents     += (parentDwdIndex != INDEX_NONE) ? 1 : 0;
		m_numTxdParentSlots += (parentTxdSlot  != INDEX_NONE) ? 1 : 0;
		m_numDrawables      += dwd->GetCount();
		m_maxDrawables       = Max<int>(numDrawables, m_maxDrawables), 
		m_numTxds           += numTxds;
		m_maxTxds            = Max<int>(numTxds, m_maxTxds);
		m_numNonEmptyTxds   += numNonEmptyTxds;
		m_maxNonEmptyTxds    = Max<int>(numNonEmptyTxds, m_maxNonEmptyTxds);
		m_numTextures       += numTextures;

		if (gStreamingIteratorTest_Verbose)
		{
			atString temp = frames > 0 ? atVarString(" [STREAMED IN %d FRAMES]", frames) : "";
			atString parents;
			
			if (parentDwdIndex != INDEX_NONE)
			{
				if (parents.length() > 0)
				{
					parents += ",";
				}

				parents += atVarString("Dwd:%d(%s)", parentDwdIndex, g_DwdStore.GetName(parentDwdIndex));
			}

			if (parentTxdSlot != INDEX_NONE)
			{
				if (parents.length() > 0)
				{
					parents += ",";
				}

				parents += atVarString("Txd_SLOT:%d(%s)", parentTxdSlot, g_TxdStore.GetName(parentTxdSlot));
			}

			if (parents.length() > 0)
			{
				parents = atVarString(" [parents = %s]", parents.c_str());
			}
			else
			{
				parents = "";
			}

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_RPFTEST
			strIndex          streamingIndex = g_DwdStore.GetStreamingIndex(slot);
			strStreamingInfo* pInfo          = strStreamingEngine::GetInfo().GetStreamingInfo(streamingIndex);
			strStreamingFile* pFile          = strPackfileManager::GetImageFileFromHandle(pInfo->GetHandle());

			temp += atVarString(" [RPF=\"%s\"]", pFile ? pFile->m_packfile->GetDebugName() : "NULL");
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_RPFTEST

			siLog(
				">> g_DwdStore[%d] = \"%s\" has %d drawables, %d/%d txds, %d textures%s%s",
				slot,
				CDTVStreamingIteratorTest_GetAssetPath(g_DwdStore, slot, INDEX_NONE).c_str(),
				numDrawables,
				numNonEmptyTxds,
				numTxds,
				numTextures,
				parents.c_str(),
				temp.c_str()
			);
		}
	}
	else
	{
		if (frames < 0)
		{
			if (gStreamingIteratorTest_Verbose)
			{
				siLog(">> g_DwdStore[%d] = \"%s\" not in streaming image!", slot, CDTVStreamingIteratorTest_GetAssetPath(g_DwdStore, slot, INDEX_NONE).c_str());
			}

			m_numNotInImage++;
		}
		else
		{
			if (gStreamingIteratorTest_Verbose || gStreamingIteratorTest_VerboseErrors)
			{
				siLog(">> !ERROR: g_DwdStore[%d] = \"%s\" failed to stream in!", slot, CDTVStreamingIteratorTest_GetAssetPath(g_DwdStore, slot, INDEX_NONE).c_str());
			}

			m_numFailed++;
		}
	}
}

void CDTVStreamingIteratorTest_DwdStore::Reset()
{
	if (m_si)
	{
		m_si->Reset();
		delete m_si;
		m_si = NULL;
	}

	m_enabled           = false;
	m_numDwds           = 0;
	m_numStreamed       = 0;
	m_numDwdParents     = 0;
	m_numTxdParentSlots = 0;
	m_numDrawables      = 0;
	m_maxDrawables      = 0;
	m_numTxds           = 0;
	m_maxTxds           = 0;
	m_numNonEmptyTxds   = 0;
	m_maxNonEmptyTxds   = 0;
	m_numTextures       = 0;
	m_numFailed         = 0;
	m_numNotInImage     = 0;
}

void CDTVStreamingIteratorTest_DwdStore::Update()
{
	DEBUG_TEXTURE_VIEWER_USE_DEBUG_MEMORY();

	if (!m_enabled)
	{
		if (m_si)
		{
			Reset();
		}

		return;
	}

	if (m_si == NULL)
	{
		m_currentSlot = 0;
		m_si = rage_new CDTVStreamingIterator_DwdStore(this, m_currentSlot, g_DwdStore, _Func, gStreamingIteratorTest_SearchRPF, gStreamingIteratorTest_SearchName, gStreamingIteratorTest_SearchFlags);

		// TODO -- fix these broken assets
		m_si->m_brokenAssetNames.PushAndGrow(atString("Z_Z_HairTest"));

		m_numDwds           = 0;
		m_numStreamed       = 0;
		m_numDwdParents     = 0;
		m_numTxdParentSlots = 0;
		m_numDrawables      = 0;
		m_maxDrawables      = 0;
		m_numTxds           = 0;
		m_maxTxds           = 0;
		m_numNonEmptyTxds   = 0;
		m_maxNonEmptyTxds   = 0;
		m_numTextures       = 0;
		m_numFailed         = 0;
		m_numNotInImage     = 0;

		siLog(">> STREAMING ITERATOR STARTING .. %d/%d DWD SLOTS", g_DwdStore.GetNumUsedSlots(), g_DwdStore.GetSize());
	}

	if (m_si->Update())
	{
		siLog(">> STREAMING ITERATOR COMPLETE");
		siLog("   dwds             : %d"            , m_numDwds                           );
		siLog("   streamed         : %d"            , m_numStreamed                       );
		siLog("   dwd parents      : %d"            , m_numDwdParents                     );
		siLog("   txd parent slots : %d"            , m_numTxdParentSlots                 );
		siLog("   drawables        : num=%d, max=%d", m_numDrawables   , m_maxDrawables   );
		siLog("   txds             : num=%d, max=%d", m_numTxds        , m_maxTxds        );
		siLog("   txds (non empty) : num=%d, max=%d", m_numNonEmptyTxds, m_maxNonEmptyTxds);
		siLog("   textures         : %d"            , m_numTextures                       );
		siLog("   failed           : %d/%d"         , m_numFailed      , m_numNotInImage  );
	}
}

// =====

CDTVStreamingIteratorTest_DrawableStore::CDTVStreamingIteratorTest_DrawableStore()
{
	sysMemSet(this, 0, sizeof(*this));
}

void CDTVStreamingIteratorTest_DrawableStore::IteratorFunc(fwDrawableDef* def, bool, int slot, int frames)
{
	if (slot == -1) // update finished
	{
		return;
	}

	Drawable* drawable = def ? def->m_pObject : NULL;
	//const char* drawableName = g_DrawableStore.GetName(slot);

	if (drawable)
	{
		if (gStreamingIteratorTest_BuildAssetAnalysisData)
		{
			AssetAnalysis::ProcessDrawableSlot(slot);
			return;
		}

		if (gStreamingIteratorTest_FindUncompressedTexturesNew)
		{
			CDTVStreamingIteratorTest_CheckAssetForUncompressedTextures(g_DrawableStore, slot, INDEX_NONE, drawable);
			return;
		}

		if (gStreamingIteratorTest_FindShaderInDrawables[0] != '\0')
		{
			CDTVStreamingIteratorTest_CheckDrawableForShader(g_DrawableStore, slot, INDEX_NONE, drawable, gStreamingIteratorTest_FindShaderInDrawables);
			return;
		}

		if (gStreamingIteratorTest_FindTextureInDrawables[0] != '\0')
		{
			CDTVStreamingIteratorTest_CheckAssetForTexture(g_DrawableStore, slot, INDEX_NONE, drawable, gStreamingIteratorTest_FindTextureInDrawables);
			return;
		}

		// TEMPORARY
		{
			const CDebugArchetypeProxy* modelInfo = CDTVStreamingIteratorTest_GetArchetypeProxyForDrawable(g_DrawableStore, slot, INDEX_NONE);

			if (modelInfo)
			{
				if (modelInfo->GetIsShadowProxy())
				{
					Displayf("### %s is shadow proxy", CDTVStreamingIteratorTest_GetAssetPath(g_DrawableStore, slot, INDEX_NONE).c_str());
				}
			}

			if (drawable->GetShaderGroupPtr() == NULL)
			{
				siLog(">> !ERROR: g_DrawableStore[%d] \"%s\" has NULL shadergroup!", slot, CDTVStreamingIteratorTest_GetAssetPath(g_DrawableStore, slot, INDEX_NONE).c_str());
			}

			CDTVStreamingIteratorTest_CheckDrawableForAnimUVs(g_DrawableStore, slot, INDEX_NONE, drawable);
		}

		if (gStreamingIteratorTest_CheckCableDrawables)
		{
			CCustomShaderEffectCable::CheckDrawable(drawable, g_DrawableStore.GetName(slot), gStreamingIteratorTest_CheckCableDrawablesSimple);
		}

		const int parentTxdSlot = CDTVStreamingIteratorTest_GetParentTxdForSlot(g_DrawableStore, slot);

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TXDCHILDREN
		if (gStreamingIteratorTest_TxdChildren)
		{
			gStreamingIteratorTest_TxdChildrenMap.SetParentTxdSlot_DrawableStore(slot, parentTxdSlot);
		}
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TXDCHILDREN

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_REDUNDANT
		if (gStreamingIteratorTest_FindRedundantTextures)
		{
			CDTVStreamingIteratorTest_FindRedundantTextures_DrawableStore(slot);
		}
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_REDUNDANT

#if DEBUG_TEXTURE_VIEWER_FIND_INEFFECTIVE_TEXTURES
		if (gStreamingIteratorTest_FindIneffectiveTexturesBumpTex ||
			gStreamingIteratorTest_FindIneffectiveTexturesSpecularTex ||
			gStreamingIteratorTest_FindNonDefaultSpecularMasks)
		{
			CDTVStreamingIteratorTest_FindIneffectiveTextures(g_DrawableStore, slot, INDEX_NONE, drawable);
		}
#endif // DEBUG_TEXTURE_VIEWER_FIND_INEFFECTIVE_TEXTURES

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS
		if (gStreamingIteratorTest_BuildTextureUsageMap)
		{
#if !DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS_ONCE
			CDTVStreamingIteratorTest_WarnAboutDrawableHasNoArchetype(g_DrawableStore, slot);
			CDTVStreamingIteratorTest_WarnAboutUniqueParentTxds      (g_DrawableStore, slot);
#endif // !DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS_ONCE

			bool bUsesNonPackedTextures = false;

			CDTVStreamingIteratorTest_BuildTextureUsageMaps(g_DrawableStore, slot, INDEX_NONE, drawable, bUsesNonPackedTextures);

			if (parentTxdSlot != INDEX_NONE && !bUsesNonPackedTextures)
			{
				if (strstr(g_TxdStore.GetName(parentTxdSlot), "mapdetail") == NULL) // don't report unnecessary dependencies on mapdetail, since this gets hooked up globally
				{
					siLog(
						">> [1] \"%s\" only used packed textures but depends on \"%s\"%s",
						CDTVStreamingIteratorTest_GetAssetPath(g_DrawableStore, slot, INDEX_NONE).c_str(),
						CDTVStreamingIteratorTest_GetAssetPath(g_TxdStore, parentTxdSlot, INDEX_NONE).c_str(),
						CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsTxdParentChainEmpty(parentTxdSlot) ? " (EMPTY)" : ""
					);
				}
			}
		}
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_FIND_UNUSED
		if (gStreamingIteratorTest_FindUnusedSharedTextures)
		{
			CDTVStreamingIteratorTest_MarkSharedTextures(g_DrawableStore, slot, INDEX_NONE, drawable);
		}
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_FIND_UNUSED

		const fwTxd* txd = drawable->GetTexDictSafe();

		if (txd)
		{
#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEXTURE
			for (int k = 0; k < txd->GetCount(); k++)
			{
				CDTVStreamingIteratorTest_Texture(g_DrawableStore, slot, INDEX_NONE, txd->GetEntry(k));
			}
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEXTURE
		}

		m_numDrawables      += 1;
		m_numStreamed       += (frames > 0) ? 1 : 0;
		m_numTxdParentSlots += (parentTxdSlot != INDEX_NONE) ? 1 : 0;
		m_numTxds           += txd ? 1 : 0;
		m_numNonEmptyTxds   += (txd && txd->GetCount() > 0) ? 1 : 0;
		m_numTextures       += txd ? txd->GetCount() : 0;
		m_maxTextures        = Max<int>(txd ? txd->GetCount() : 0, m_maxTextures);

		if (gStreamingIteratorTest_Verbose)
		{
			atString temp = frames > 0 ? atVarString(" [STREAMED IN %d FRAMES]", frames) : "";
			atString parents;

			if (parentTxdSlot != INDEX_NONE)
			{
				if (parents.length() > 0)
				{
					parents += ",";
				}

				parents += atVarString("Txd_SLOT:%d(%s)", parentTxdSlot, g_TxdStore.GetName(parentTxdSlot));
			}

			if (parents.length() > 0)
			{
				parents = atVarString(" [parents = %s]", parents.c_str());
			}
			else
			{
				parents = "";
			}

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_RPFTEST
			strIndex          streamingIndex = g_DrawableStore.GetStreamingIndex(slot);
			strStreamingInfo* pInfo          = strStreamingEngine::GetInfo().GetStreamingInfo(streamingIndex);
			strStreamingFile* pFile          = strPackfileManager::GetImageFileFromHandle(pInfo->GetHandle());

			temp += atVarString(" [RPF=\"%s\"]", pFile ? pFile->m_packfile->GetDebugName() : "NULL");
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_RPFTEST

			siLog(
				">> g_DrawableStore[%d] = \"%s\" has %d txds, %d textures%s%s",
				slot,
				CDTVStreamingIteratorTest_GetAssetPath(g_DrawableStore, slot, INDEX_NONE).c_str(),
				txd ? 1 : 0,
				txd ? txd->GetCount() : 0,
				parents.c_str(),
				temp.c_str()
			);
		}
	}
	else
	{
		if (frames < 0)
		{
			if (gStreamingIteratorTest_Verbose)
			{
				siLog(">> g_DrawableStore[%d] = \"%s\" not in streaming image!", slot, CDTVStreamingIteratorTest_GetAssetPath(g_DrawableStore, slot, INDEX_NONE).c_str());
			}

			m_numNotInImage++;
		}
		else
		{
			if (gStreamingIteratorTest_Verbose || gStreamingIteratorTest_VerboseErrors)
			{
				siLog(">> !ERROR: g_DrawableStore[%d] = \"%s\" failed to stream in!", slot, CDTVStreamingIteratorTest_GetAssetPath(g_DrawableStore, slot, INDEX_NONE).c_str());
			}

			m_numFailed++;
		}
	}
}

void CDTVStreamingIteratorTest_DrawableStore::Reset()
{
	if (m_si)
	{
		m_si->Reset();
		delete m_si;
		m_si = NULL;
	}

	m_enabled           = false;
	m_numDrawables      = 0;
	m_numStreamed       = 0;
	m_numTxdParentSlots = 0;
	m_numTxds           = 0;
	m_numNonEmptyTxds   = 0;
	m_numTextures       = 0;
	m_maxTextures       = 0;
	m_numFailed         = 0;
	m_numNotInImage     = 0;
}

void CDTVStreamingIteratorTest_DrawableStore::Update()
{
	DEBUG_TEXTURE_VIEWER_USE_DEBUG_MEMORY();

	if (!m_enabled)
	{
		if (m_si)
		{
			Reset();
		}

		return;
	}

	if (m_si == NULL)
	{
		m_currentSlot = 0;
		m_si = rage_new CDTVStreamingIterator_DrawableStore(this, m_currentSlot, g_DrawableStore, _Func, gStreamingIteratorTest_SearchRPF, gStreamingIteratorTest_SearchName, gStreamingIteratorTest_SearchFlags);

		m_numDrawables      = 0;
		m_numStreamed       = 0;
		m_numTxdParentSlots = 0;
		m_numTxds           = 0;
		m_numNonEmptyTxds   = 0;
		m_numTextures       = 0;
		m_maxTextures       = 0;
		m_numFailed         = 0;
		m_numNotInImage     = 0;

		siLog(">> STREAMING ITERATOR STARTING .. %d/%d DRAWABLE SLOTS", g_DrawableStore.GetNumUsedSlots(), g_DrawableStore.GetSize());
	}

	if (m_si->Update())
	{
		siLog(">> STREAMING ITERATOR COMPLETE");
		siLog("   drawables        : %d"            , m_numDrawables              );
		siLog("   streamed         : %d"            , m_numStreamed               );
		siLog("   txd parent slots : %d"            , m_numTxdParentSlots         );
		siLog("   txds             : %d"            , m_numTxds                   );
		siLog("   txds (non empty) : %d"            , m_numNonEmptyTxds           );
		siLog("   textures         : num=%d, max=%d", m_numTextures, m_maxTextures);
		siLog("   failed           : %d/%d"         , m_numFailed, m_numNotInImage);
	}
}

// =====

CDTVStreamingIteratorTest_FragmentStore::CDTVStreamingIteratorTest_FragmentStore()
{
	sysMemSet(this, 0, sizeof(*this));
}

void CDTVStreamingIteratorTest_FragmentStore::IteratorFunc(fwFragmentDef* def, bool, int slot, int frames)
{
	if (slot == -1) // update finished
	{
		return;
	}

	Fragment* fragment = def ? def->m_pObject : NULL;
	//const char* fragmentName = g_FragmentStore.GetName(slot);

	if (fragment)
	{
		if (gStreamingIteratorTest_BuildAssetAnalysisData)
		{
			AssetAnalysis::ProcessFragmentSlot(slot);
			return;
		}

		if (gStreamingIteratorTest_FindUncompressedTexturesNew)
		{
			CDTVStreamingIteratorTest_CheckAssetForUncompressedTextures(g_FragmentStore, slot, INDEX_NONE, fragment->GetCommonDrawable());
			CDTVStreamingIteratorTest_CheckAssetForUncompressedTextures(g_FragmentStore, slot, INDEX_NONE, fragment->GetClothDrawable());
			return;
		}

		if (gStreamingIteratorTest_FindShaderInDrawables[0] != '\0')
		{
			CDTVStreamingIteratorTest_CheckDrawableForShader(g_FragmentStore, slot, INDEX_NONE, fragment->GetCommonDrawable(), gStreamingIteratorTest_FindShaderInDrawables);
			CDTVStreamingIteratorTest_CheckDrawableForShader(g_FragmentStore, slot, INDEX_NONE, fragment->GetClothDrawable(), gStreamingIteratorTest_FindShaderInDrawables);
			return;
		}

		if (gStreamingIteratorTest_FindTextureInDrawables[0] != '\0')
		{
			CDTVStreamingIteratorTest_CheckAssetForTexture(g_FragmentStore, slot, INDEX_NONE, fragment->GetCommonDrawable(), gStreamingIteratorTest_FindTextureInDrawables);
			CDTVStreamingIteratorTest_CheckAssetForTexture(g_FragmentStore, slot, INDEX_NONE, fragment->GetClothDrawable(), gStreamingIteratorTest_FindTextureInDrawables);
			return;
		}

		// TEMPORARY
		{
			const CDebugArchetypeProxy* modelInfo = CDTVStreamingIteratorTest_GetArchetypeProxyForDrawable(g_FragmentStore, slot, INDEX_NONE);
			const Drawable* drawable = fragment->GetCommonDrawable();

			if (modelInfo && modelInfo->GetModelType() == MI_TYPE_VEHICLE && drawable) // check for vehicles with embedded textures
			{
				const fwTxd* txd = drawable->GetTexDictSafe();

				if (txd && txd->GetCount() > 0)
				{
					siLog(">> !ERROR: g_FragmentStore[%d] \"%s\" is a vehicle and has %d textures!", slot, CDTVStreamingIteratorTest_GetAssetPath(g_FragmentStore, slot, INDEX_NONE).c_str(), txd->GetCount());
				}
			}

			if (fragment->GetClothDrawable())
			{
				const fwTxd* txd = fragment->GetClothDrawable()->GetTexDictSafe();

				if (txd && txd->GetCount() > 0)
				{
					// actually in this case the textures are duplicated .. so this is a waste of memory
					siLog(">> !NOTE: g_FragmentStore[%d] \"%s\" has a cloth drawable with %d textures!", slot, CDTVStreamingIteratorTest_GetAssetPath(g_FragmentStore, slot, INDEX_NONE).c_str(), txd->GetCount());
				}
				else
				{
					siLog(">> !NOTE: g_FragmentStore[%d] \"%s\" has a cloth drawable!", slot, CDTVStreamingIteratorTest_GetAssetPath(g_FragmentStore, slot, INDEX_NONE).c_str());
				}
			}

			if (fragment->GetNumExtraDrawables() > 1)
			{
				siLog(">> !ERROR: g_FragmentStore[%d] \"%s\" has %d extra drawables!", slot, CDTVStreamingIteratorTest_GetAssetPath(g_FragmentStore, slot, INDEX_NONE).c_str(), fragment->GetNumExtraDrawables());
			}
			else if (fragment->GetNumExtraDrawables() == 1 && strcmp(fragment->GetExtraDrawableName(0), "damaged") != 0)
			{
				siLog(">> !ERROR: g_FragmentStore[%d] \"%s\" has an extra drawables named \"%s\"!", slot, CDTVStreamingIteratorTest_GetAssetPath(g_FragmentStore, slot, INDEX_NONE).c_str(), fragment->GetExtraDrawableName(0));
			}

			if (drawable == NULL)
			{
				Assert(0);
				siLog(">> !ERROR: g_FragmentStore[%d] \"%s\" has NULL common drawable!", slot, CDTVStreamingIteratorTest_GetAssetPath(g_FragmentStore, slot, INDEX_NONE).c_str());
			}
			else if (drawable->GetShaderGroupPtr() == NULL)
			{
				Assert(0);
				siLog(">> !ERROR: g_FragmentStore[%d] \"%s\" has common drawable with NULL shadergroup!", slot, CDTVStreamingIteratorTest_GetAssetPath(g_FragmentStore, slot, INDEX_NONE).c_str());
			}

			CDTVStreamingIteratorTest_CheckDrawableForAnimUVs(g_FragmentStore, slot, INDEX_NONE, fragment->GetCommonDrawable());
		}

		const int parentTxdSlot = CDTVStreamingIteratorTest_GetParentTxdForSlot(g_FragmentStore, slot);

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TXDCHILDREN
		if (gStreamingIteratorTest_TxdChildren)
		{
			gStreamingIteratorTest_TxdChildrenMap.SetParentTxdSlot_FragmentStore(slot, parentTxdSlot);
		}
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TXDCHILDREN

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_REDUNDANT
		if (gStreamingIteratorTest_FindRedundantTextures)
		{
			CDTVStreamingIteratorTest_FindRedundantTextures_FragmentStore(slot);
		}
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_REDUNDANT

#if DEBUG_TEXTURE_VIEWER_FIND_INEFFECTIVE_TEXTURES
		if (gStreamingIteratorTest_FindIneffectiveTexturesBumpTex ||
			gStreamingIteratorTest_FindIneffectiveTexturesSpecularTex ||
			gStreamingIteratorTest_FindNonDefaultSpecularMasks)
		{
			CDTVStreamingIteratorTest_FindIneffectiveTextures(g_FragmentStore, slot, INDEX_NONE, fragment->GetCommonDrawable());

			for (int i = 0; i < fragment->GetNumExtraDrawables(); i++)
			{
				CDTVStreamingIteratorTest_FindIneffectiveTextures(g_FragmentStore, slot, i, fragment->GetExtraDrawable(i));
			}
		}
#endif // DEBUG_TEXTURE_VIEWER_FIND_INEFFECTIVE_TEXTURES

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS
		if (gStreamingIteratorTest_BuildTextureUsageMap)
		{
#if !DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS_ONCE
			CDTVStreamingIteratorTest_WarnAboutDrawableHasNoArchetype(g_FragmentStore, slot);
			CDTVStreamingIteratorTest_WarnAboutUniqueParentTxds      (g_FragmentStore, slot);
#endif // !DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS_ONCE

			bool bUsesNonPackedTextures = false;

			CDTVStreamingIteratorTest_BuildTextureUsageMaps(g_FragmentStore, slot, INDEX_NONE, fragment->GetCommonDrawable(), bUsesNonPackedTextures);

			for (int i = 0; i < fragment->GetNumExtraDrawables(); i++)
			{
				CDTVStreamingIteratorTest_BuildTextureUsageMaps(g_FragmentStore, slot, i, fragment->GetExtraDrawable(i), bUsesNonPackedTextures);
			}

			if (parentTxdSlot != INDEX_NONE && !bUsesNonPackedTextures)
			{
				if (strstr(g_TxdStore.GetName(parentTxdSlot), "mapdetail") == NULL) // don't report unnecessary dependencies on mapdetail, since this gets hooked up globally
				{
					siLog(
						">> [1] \"%s\" only uses packed textures but depends on \"%s\"%s",
						CDTVStreamingIteratorTest_GetAssetPath(g_FragmentStore, slot, INDEX_NONE).c_str(),
						CDTVStreamingIteratorTest_GetAssetPath(g_TxdStore, parentTxdSlot, INDEX_NONE).c_str(),
						CDTVStreamingIteratorTest_BuildTextureUsageMaps_IsTxdParentChainEmpty(parentTxdSlot) ? " (EMPTY)" : ""
					);
				}
			}
		}
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_FIND_UNUSED
		if (gStreamingIteratorTest_FindUnusedSharedTextures)
		{
			CDTVStreamingIteratorTest_MarkSharedTextures(g_FragmentStore, slot, INDEX_NONE, fragment->GetCommonDrawable());

			for (int i = 0; i < fragment->GetNumExtraDrawables(); i++)
			{
				CDTVStreamingIteratorTest_MarkSharedTextures(g_FragmentStore, slot, i, fragment->GetExtraDrawable(i));
			}
		}
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_FIND_UNUSED

		atString extraDrawableNames;

		int numTextures = 0;
		int numTexturesInExtraDrawables = 0;

		// common drawable
		{
			const fwTxd* txd = fragment->GetCommonDrawable()->GetTexDictSafe();

			if (txd)
			{
#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEXTURE
				for (int k = 0; k < txd->GetCount(); k++)
				{
					CDTVStreamingIteratorTest_Texture(g_FragmentStore, slot, INDEX_NONE, txd->GetEntry(k));
				}
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEXTURE

				m_numTxds++;
				m_numNonEmptyTxds += (txd->GetCount() > 0) ? 1 : 0;
				m_numTextures     += txd->GetCount();
				m_maxTextures      = Max<int>(txd->GetCount(), m_maxTextures);

				numTextures += txd->GetCount();
			}
		}

		m_numFragments      += 1;
		m_numStreamed       += (frames > 0) ? 1 : 0;
		m_numTxdParentSlots += (parentTxdSlot != INDEX_NONE) ? 1 : 0;
		m_numExtraDrawables += fragment->GetNumExtraDrawables();
		m_maxExtraDrawables  = Max<int>(fragment->GetNumExtraDrawables(), m_maxExtraDrawables);

		for (int i = 0; i < fragment->GetNumExtraDrawables(); i++)
		{
			const fwTxd* txd = fragment->GetExtraDrawable(i)->GetTexDictSafe();

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEXTURE
			if (txd)
			{
				for (int k = 0; k < txd->GetCount(); k++)
				{
					CDTVStreamingIteratorTest_Texture(g_FragmentStore, slot, i, txd->GetEntry(k));
				}
			}
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEXTURE

			m_numTxds         += txd ? 1 : 0;
			m_numNonEmptyTxds += (txd && txd->GetCount() > 0) ? 1 : 0;
			m_numTextures     += txd ? txd->GetCount() : 0;
			m_maxTextures      = Max<int>(txd ? txd->GetCount() : 0, m_maxTextures);

			m_numExtraDrawablesWithTxds         += txd ? 1 : 0;
			m_numExtraDrawablesWithNonEmptyTxds += (txd && txd->GetCount() > 0) ? 1 : 0;

			numTexturesInExtraDrawables += txd ? txd->GetCount() : 0;

			extraDrawableNames += (i > 0) ? "," : "";
			extraDrawableNames += fragment->GetExtraDrawableName(i);
		}

		if (gStreamingIteratorTest_Verbose)// || fragment->GetNumExtraDrawables() > 0)
		{
			if (extraDrawableNames.length() > 0)
			{
				extraDrawableNames = atVarString("{%s}", extraDrawableNames.c_str());
			}

			atString temp = frames > 0 ? atVarString(" [STREAMED IN %d FRAMES]", frames) : "";
			atString parents;

			if (parentTxdSlot != INDEX_NONE)
			{
				if (parents.length() > 0)
				{
					parents += ",";
				}

				parents += atVarString("Txd_SLOT:%d(%s)", parentTxdSlot, g_TxdStore.GetName(parentTxdSlot));
			}

			if (parents.length() > 0)
			{
				parents = atVarString(" [parents = %s]", parents.c_str());
			}
			else
			{
				parents = "";
			}

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_RPFTEST
			strIndex          streamingIndex = g_FragmentStore.GetStreamingIndex(slot);
			strStreamingInfo* pInfo          = strStreamingEngine::GetInfo().GetStreamingInfo(streamingIndex);
			strStreamingFile* pFile          = strPackfileManager::GetImageFileFromHandle(pInfo->GetHandle());

			temp += atVarString(" [RPF=\"%s\"]", pFile ? pFile->m_packfile->GetDebugName() : "NULL");
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_RPFTEST

			if (fragment->GetNumExtraDrawables() > 0)
			{
				siLog(
					">> g_FragmentStore[%d] = \"%s\" has %d textures, %d extra drawables %s with %d textures%s%s",
					slot,
					CDTVStreamingIteratorTest_GetAssetPath(g_FragmentStore, slot, INDEX_NONE).c_str(),
					numTextures,
					fragment->GetNumExtraDrawables(),
					extraDrawableNames.c_str(),
					numTexturesInExtraDrawables,
					parents.c_str(),
					temp.c_str()
				);
			}
			else
			{
				siLog(
					">> g_FragmentStore[%d] = \"%s\" has %d textures%s%s",
					slot,
					CDTVStreamingIteratorTest_GetAssetPath(g_FragmentStore, slot, INDEX_NONE).c_str(),
					numTextures,
					parents.c_str(),
					temp.c_str()
				);
			}
		}
	}
	else
	{
		if (frames < 0)
		{
			if (gStreamingIteratorTest_Verbose)
			{
				siLog(">> g_FragmentStore[%d] = \"%s\" not in streaming image!", slot, CDTVStreamingIteratorTest_GetAssetPath(g_FragmentStore, slot, INDEX_NONE).c_str());
			}

			m_numNotInImage++;
		}
		else
		{
			if (gStreamingIteratorTest_Verbose || gStreamingIteratorTest_VerboseErrors)
			{
				siLog(">> !ERROR: g_FragmentStore[%d] = \"%s\" failed to stream in!", slot, CDTVStreamingIteratorTest_GetAssetPath(g_FragmentStore, slot, INDEX_NONE).c_str());
			}

			m_numFailed++;
		}
	}
}

void CDTVStreamingIteratorTest_FragmentStore::Reset()
{
	if (m_si)
	{
		m_si->Reset();
		delete m_si;
		m_si = NULL;
	}

	m_enabled                           = false;
	m_numFragments                      = 0;
	m_numStreamed                       = 0;
	m_numTxdParentSlots                 = 0;
	m_numTxds                           = 0;
	m_numNonEmptyTxds                   = 0;
	m_numTextures                       = 0;
	m_maxTextures                       = 0;
	m_numExtraDrawables                 = 0;
	m_maxExtraDrawables                 = 0;
	m_numExtraDrawablesWithTxds         = 0;
	m_numExtraDrawablesWithNonEmptyTxds = 0;
	m_numFailed                         = 0;
	m_numNotInImage                     = 0;
}

void CDTVStreamingIteratorTest_FragmentStore::Update()
{
	DEBUG_TEXTURE_VIEWER_USE_DEBUG_MEMORY();

	if (!m_enabled)
	{
		if (m_si)
		{
			Reset();
		}

		return;
	}

	if (m_si == NULL)
	{
		m_si = rage_new CDTVStreamingIterator_FragmentStore(this, m_currentSlot, g_FragmentStore, _Func, gStreamingIteratorTest_SearchRPF, gStreamingIteratorTest_SearchName, gStreamingIteratorTest_SearchFlags);

		m_si->m_brokenAssetNames.PushAndGrow(atString("z_z_fred"));
		m_si->m_brokenAssetNames.PushAndGrow(atString("z_z_fred_large"));
		m_si->m_brokenAssetNames.PushAndGrow(atString("z_z_wilma"));
		m_si->m_brokenAssetNames.PushAndGrow(atString("z_z_wilma_large"));
		m_si->m_brokenAssetNames.PushAndGrow(atString("z_z_skirttest"));

		// TODO -- these assets aren't necessarily broken, just trying to make it easier to debug
		m_si->m_brokenRPFNames.PushAndGrow(atString("vehiclemods"));
		//m_si->m_brokenRPFNames.PushAndGrow(atString("componentpeds")); // once s_m_m_movalien_01 is fixed we can remove this line

		m_numFragments                      = 0;
		m_numStreamed                       = 0;
		m_numTxdParentSlots                 = 0;
		m_numTxds                           = 0;
		m_numNonEmptyTxds                   = 0;
		m_numTextures                       = 0;
		m_maxTextures                       = 0;
		m_numExtraDrawables                 = 0;
		m_maxExtraDrawables                 = 0;
		m_numExtraDrawablesWithTxds         = 0;
		m_numExtraDrawablesWithNonEmptyTxds = 0;
		m_numFailed                         = 0;
		m_numNotInImage                     = 0;

		siLog(">> STREAMING ITERATOR STARTING .. %d/%d FRAGMENT SLOTS", g_FragmentStore.GetNumUsedSlots(), g_FragmentStore.GetSize());
	}

	if (m_si->Update())
	{
		siLog(">> STREAMING ITERATOR COMPLETE");
		siLog("   fragments                           : %d"            , m_numFragments                          );
		siLog("   streamed                            : %d"            , m_numStreamed                           );
		siLog("   txd parent slots                    : %d"            , m_numTxdParentSlots                     );
		siLog("   txds                                : %d"            , m_numTxds                               );
		siLog("   txds (non empty)                    : %d"            , m_numNonEmptyTxds                       );
		siLog("   textures                            : num=%d, max=%d", m_numTextures      , m_maxTextures      );
		siLog("   extra drawables                     : num=%d, max=%d", m_numExtraDrawables, m_maxExtraDrawables);
		siLog("   extra drawables w/ txds             : %d"            , m_numExtraDrawablesWithTxds             );
		siLog("   extra drawables w/ txds (non empty) : %d"            , m_numExtraDrawablesWithNonEmptyTxds     );
		siLog("   failed                              : %d/%d"         , m_numFailed        , m_numNotInImage    );
	}
}

// =====

CDTVStreamingIteratorTest_ParticleStore::CDTVStreamingIteratorTest_ParticleStore()
{
	sysMemSet(this, 0, sizeof(*this));
}

void CDTVStreamingIteratorTest_ParticleStore::IteratorFunc(ptxFxListDef* def, bool, int slot, int frames)
{
	if (slot == -1) // update finished
	{
		return;
	}

	ptxFxList* ptx = def ? def->m_pObject : NULL;
	//const char* ptxName = g_ParticleStore.GetName(slot);

	if (ptx)
	{
		if (gStreamingIteratorTest_BuildAssetAnalysisData)
		{
			AssetAnalysis::ProcessParticleSlot(slot);
			return;
		}

		const Dwd* ptxDwd = ptx->GetModelDictionary();

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TXDCHILDREN
		if (gStreamingIteratorTest_TxdChildren)
		{
			gStreamingIteratorTest_TxdChildrenMap.SetParentTxdSlot_ParticleStore(slot, parentTxdSlot);
		}
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TXDCHILDREN

#if DEBUG_TEXTURE_VIEWER_FIND_INEFFECTIVE_TEXTURES
		if (gStreamingIteratorTest_FindIneffectiveTexturesBumpTex ||
			gStreamingIteratorTest_FindIneffectiveTexturesSpecularTex ||
			gStreamingIteratorTest_FindNonDefaultSpecularMasks)
		{
			if (ptxDwd)
			{
				for (int i = 0; i < ptxDwd->GetCount(); i++)
				{
					CDTVStreamingIteratorTest_FindIneffectiveTextures(g_ParticleStore, slot, i, ptxDwd->GetEntry(i));
				}
			}
		}
#endif // DEBUG_TEXTURE_VIEWER_FIND_INEFFECTIVE_TEXTURES

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS
		if (gStreamingIteratorTest_BuildTextureUsageMap)
		{
			if (ptxDwd)
			{
				bool bUsesNonPackedTextures = false;

				for (int i = 0; i < ptxDwd->GetCount(); i++)
				{
					CDTVStreamingIteratorTest_BuildTextureUsageMaps(g_ParticleStore, slot, i, ptxDwd->GetEntry(i), bUsesNonPackedTextures);
				}
			}

			const ptxParticleRuleDictionary* prd = ptx->GetParticleRuleDictionary();

			if (prd)
			{
				for (int i = 0; i < prd->GetCount(); i++)
				{
					CDTVStreamingIteratorTest_BuildTextureUsageMaps_ParticleRule(slot, i, prd->GetEntry(i));
				}
			}
		}
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_FIND_UNUSED
		if (gStreamingIteratorTest_FindUnusedSharedTextures)
		{
			// ...
		}
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_FIND_UNUSED

		int numTextures = 0;
		int numTexturesInDrawables = 0;
		int numDrawables = 0;

		// ptx txd
		{
			const fwTxd* txd = ptx->GetTextureDictionary();

			if (txd)
			{
#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEXTURE
				for (int k = 0; k < txd->GetCount(); k++)
				{
					CDTVStreamingIteratorTest_Texture(g_ParticleStore, slot, INDEX_NONE, txd->GetEntry(k));
				}
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEXTURE

				m_numTxds++;
				m_numNonEmptyTxds += (txd->GetCount() > 0) ? 1 : 0;
				m_numTextures     += txd->GetCount();
				m_maxTextures      = Max<int>(txd->GetCount(), m_maxTextures);

				numTextures += txd->GetCount();
			}
		}

		m_numParticles += 1;
		m_numStreamed  += (frames > 0) ? 1 : 0;

		if (ptxDwd)
		{
			numDrawables = ptxDwd->GetCount();

			m_numDrawables += numDrawables;
			m_maxDrawables = Max<int>(numDrawables, m_maxDrawables);

			for (int i = 0; i < ptxDwd->GetCount(); i++)
			{
				const fwTxd* txd = ptxDwd->GetEntry(i)->GetTexDictSafe();

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEXTURE
				if (txd)
				{
					for (int k = 0; k < txd->GetCount(); k++)
					{
						CDTVStreamingIteratorTest_Texture(g_ParticleStore, slot, i, txd->GetEntry(k));
					}
				}
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEXTURE

				m_numTxds         += txd ? 1 : 0;
				m_numNonEmptyTxds += (txd && txd->GetCount() > 0) ? 1 : 0;
				m_numTextures     += txd ? txd->GetCount() : 0;
				m_maxTextures      = Max<int>(txd ? txd->GetCount() : 0, m_maxTextures);

				m_numDrawablesWithTxds         += txd ? 1 : 0;
				m_numDrawablesWithNonEmptyTxds += (txd && txd->GetCount() > 0) ? 1 : 0;

				numTexturesInDrawables += txd ? txd->GetCount() : 0;
			}
		}

		if (gStreamingIteratorTest_Verbose)
		{
			atString temp = frames > 0 ? atVarString(" [STREAMED IN %d FRAMES]", frames) : "";

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_RPFTEST
			strIndex          streamingIndex = g_ParticleStore.GetStreamingIndex(slot);
			strStreamingInfo* pInfo          = strStreamingEngine::GetInfo().GetStreamingInfo(streamingIndex);
			strStreamingFile* pFile          = strPackfileManager::GetImageFileFromHandle(pInfo->GetHandle());

			temp += atVarString(" [RPF=\"%s\"]", pFile ? pFile->m_packfile->GetDebugName() : "NULL");
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_RPFTEST

			if (numDrawables > 0)
			{
				siLog(
					">> g_ParticleStore[%d] = \"%s\" has %d textures, %d drawables with %d textures%s",
					slot,
					CDTVStreamingIteratorTest_GetAssetPath(g_ParticleStore, slot, INDEX_NONE).c_str(),
					numTextures,
					numDrawables,
					numTexturesInDrawables,
					temp.c_str()
				);
			}
			else
			{
				siLog(
					">> g_ParticleStore[%d] = \"%s\" has %d textures%s",
					slot,
					CDTVStreamingIteratorTest_GetAssetPath(g_ParticleStore, slot, INDEX_NONE).c_str(),
					numTextures,
					temp.c_str()
				);
			}
		}
	}
	else
	{
		if (frames < 0)
		{
			if (gStreamingIteratorTest_Verbose)
			{
				siLog(">> g_ParticleStore[%d] = \"%s\" not in streaming image!", slot, CDTVStreamingIteratorTest_GetAssetPath(g_ParticleStore, slot, INDEX_NONE).c_str());
			}

			m_numNotInImage++;
		}
		else
		{
			if (gStreamingIteratorTest_Verbose || gStreamingIteratorTest_VerboseErrors)
			{
				siLog(">> !ERROR: g_ParticleStore[%d] = \"%s\" failed to stream in!", slot, CDTVStreamingIteratorTest_GetAssetPath(g_ParticleStore, slot, INDEX_NONE).c_str());
			}

			m_numFailed++;
		}
	}
}

void CDTVStreamingIteratorTest_ParticleStore::Reset()
{
	if (m_si)
	{
		m_si->Reset();
		delete m_si;
		m_si = NULL;
	}

	m_enabled                      = false;
	m_numParticles                 = 0;
	m_numStreamed                  = 0;
	m_numTxds                      = 0;
	m_numNonEmptyTxds              = 0;
	m_numTextures                  = 0;
	m_maxTextures                  = 0;
	m_numDrawables                 = 0;
	m_maxDrawables                 = 0;
	m_numDrawablesWithTxds         = 0;
	m_numDrawablesWithNonEmptyTxds = 0;
	m_numFailed                    = 0;
	m_numNotInImage                = 0;
}

void CDTVStreamingIteratorTest_ParticleStore::Update()
{
	DEBUG_TEXTURE_VIEWER_USE_DEBUG_MEMORY();

	if (!m_enabled)
	{
		if (m_si)
		{
			Reset();
		}

		return;
	}

	if (m_si == NULL)
	{
		m_si = rage_new CDTVStreamingIterator_ParticleStore(this, m_currentSlot, g_ParticleStore, _Func, gStreamingIteratorTest_SearchRPF, gStreamingIteratorTest_SearchName, gStreamingIteratorTest_SearchFlags);

		m_numParticles                 = 0;
		m_numStreamed                  = 0;
		m_numTxds                      = 0;
		m_numNonEmptyTxds              = 0;
		m_numTextures                  = 0;
		m_maxTextures                  = 0;
		m_numDrawables                 = 0;
		m_maxDrawables                 = 0;
		m_numDrawablesWithTxds         = 0;
		m_numDrawablesWithNonEmptyTxds = 0;
		m_numFailed                    = 0;
		m_numNotInImage                = 0;

		siLog(">> STREAMING ITERATOR STARTING .. %d/%d PARTICLE SLOTS", g_ParticleStore.GetNumUsedSlots(), g_ParticleStore.GetSize());
	}

	if (m_si->Update())
	{
		siLog(">> STREAMING ITERATOR COMPLETE");
		siLog("   particles                     : %d"            , m_numParticles                );
		siLog("   streamed                      : %d"            , m_numStreamed                 );
		siLog("   txds                          : %d"            , m_numTxds                     );
		siLog("   txds (non empty)              : %d"            , m_numNonEmptyTxds             );
		siLog("   textures                      : num=%d, max=%d", m_numTextures, m_maxTextures  );
		siLog("   drawables                     : num=%d, max=%d", m_numDrawables, m_maxDrawables);
		siLog("   drawables w/ txds             : %d"            , m_numDrawablesWithTxds        );
		siLog("   drawables w/ txds (non empty) : %d"            , m_numDrawablesWithNonEmptyTxds);
		siLog("   failed                        : %d/%d"         , m_numFailed, m_numNotInImage  );
	}
}

// =====

static u8* gStreamingIteratorTest_ArchetypeEntityCounts = NULL; // how many entities (in map data) are used by each archetype

static void CDTVStreamingIteratorTest_AddModifierBoxGeometry(fiStream*& stream, const char* name, const char* mtlName, int groupId, int& quadCount, const spdAABB& box)
{
	if (stream == NULL)
	{
		char path[512] = "";
		sprintf(path, "assets:/non_final/geometry/timecycle/%s.obj", name);
		stream = fiStream::Create(path);

		if (Verifyf(stream, "failed to create \"%s\"", path))
		{
			fprintf(stream, "mtllib ../materials.mtl\n");
			fprintf(stream, "g %d\n", groupId);
			fprintf(stream, "usemtl %s\n", mtlName);
		}
	}

	if (stream)
	{
		const Vec3V bbmin = box.GetMin();
		const Vec3V bbmax = box.GetMax();
		const Vec3V corners[] =
		{
			bbmin,
			GetFromTwo<Vec::X2,Vec::Y1,Vec::Z1>(bbmin, bbmax),
			GetFromTwo<Vec::X1,Vec::Y2,Vec::Z1>(bbmin, bbmax),
			GetFromTwo<Vec::X2,Vec::Y2,Vec::Z1>(bbmin, bbmax),
			GetFromTwo<Vec::X1,Vec::Y1,Vec::Z2>(bbmin, bbmax),
			GetFromTwo<Vec::X2,Vec::Y1,Vec::Z2>(bbmin, bbmax),
			GetFromTwo<Vec::X1,Vec::Y2,Vec::Z2>(bbmin, bbmax),
			bbmax,
		};

		//   6---7
		//  /|  /|
		// 4---5 |
		// | | | |
		// | 2-|-3
		// |/  |/
		// 0---1

		const int quads[6][4] =
		{
			{1,5,4,0},
			{3,7,5,1},
			{2,6,7,3},
			{0,4,6,2},
			{2,3,1,0},
			{5,7,6,4},
		};

		for (int i = 0; i < NELEM(quads); i++)
		{
			fprintf(stream, "v %f %f %f\n", VEC3V_ARGS(corners[quads[i][0]]));
			fprintf(stream, "v %f %f %f\n", VEC3V_ARGS(corners[quads[i][1]]));
			fprintf(stream, "v %f %f %f\n", VEC3V_ARGS(corners[quads[i][2]]));
			fprintf(stream, "v %f %f %f\n", VEC3V_ARGS(corners[quads[i][3]]));
			quadCount++;
		}

		stream->Flush();
	}
}

CDTVStreamingIteratorTest_MapDataStore::CDTVStreamingIteratorTest_MapDataStore()
{
	sysMemSet(this, 0, sizeof(*this));
}

//template <typename T> static int GetIndexInArray(const T* element, const T* array)
//{
//	return (int)((size_t)element - (size_t)array)/sizeof(T);
//}

void CDTVStreamingIteratorTest_MapDataStore::IteratorFunc(fwMapDataDef* def, bool bIsParentDef, int slot, int frames)
{
	DEBUG_TEXTURE_VIEWER_USE_DEBUG_MEMORY();

	static fiStream* s_timecycleModifierBoxFiles[3] = {NULL,NULL,NULL};
	static int       s_timecycleModifierBoxQuadCounts[3] = {0,0,0};
	static int*      s_timecycleModifierBoxCounts = NULL;

	if (slot == -1) // update finished
	{
		if (gStreamingIteratorTest_MapDataDumpTCModBoxes)
		{
			Displayf("finalising mapdata timecycle modifiers ..");

			for (int i = 0; i < NELEM(s_timecycleModifierBoxFiles); i++)
			{
				if (s_timecycleModifierBoxFiles[i])
				{
					for (int j = 0; j < s_timecycleModifierBoxQuadCounts[i]; j++)
					{
						fprintf(s_timecycleModifierBoxFiles[i], "f %d %d %d %d\n", -(4 + j*4), -(3 + j*4), -(2 + j*4), -(1 + j*4));
					}

					s_timecycleModifierBoxFiles[i]->Close();
					s_timecycleModifierBoxFiles[i] = NULL;
				}

				s_timecycleModifierBoxQuadCounts[i] = 0;
			}

			if (s_timecycleModifierBoxCounts)
			{
				for (int i = 0; i < TCVAR_NUM; i++)
				{
					if (s_timecycleModifierBoxCounts[i] > 0)
					{
						Displayf("found %d modifiers affecting TCVAR %d (%s)", s_timecycleModifierBoxCounts[i], i, g_varInfos[i].name);
					}
				}

				delete[] s_timecycleModifierBoxCounts;
			}
		}

		if (gStreamingIteratorTest_MapDataCountInstances &&
			gStreamingIteratorTest_ArchetypeEntityCounts)
		{
			int numArchetypesWithNoInstances = 0;
			int numArchetypesWithOneInstance = 0;
			int numArchetypesWithMultipleInstances = 0;

			for (u32 i = 0; i < CDebugArchetype::GetMaxDebugArchetypeProxies(); i++)
			{
				const CDebugArchetypeProxy* modelInfo = CDebugArchetype::GetDebugArchetypeProxy(i);

				if (modelInfo)
				{
					bool bIsArchetypeFromMapData =
						modelInfo->GetModelType() == MI_TYPE_BASE ||
						modelInfo->GetModelType() == MI_TYPE_MLO  ||
						modelInfo->GetModelType() == MI_TYPE_TIME ||
						modelInfo->GetModelType() == MI_TYPE_COMPOSITE;

					const char* modelName = modelInfo->GetModelName();

					if (stristr(modelName, "_cs_") != NULL ||
						stristr(modelName, "des_") == modelName ||
						stristr(modelName, "_ilev_") != NULL)
					{
						bIsArchetypeFromMapData = false; // skip these ..
					}

					if (bIsArchetypeFromMapData)
					{
						const int count = (int)gStreamingIteratorTest_ArchetypeEntityCounts[i];

						if (count == 0)
						{
							const char* modelTypeStrings[] =
							{
								STRING(MI_TYPE_NONE     ),
								STRING(MI_TYPE_BASE     ),
								STRING(MI_TYPE_MLO      ),
								STRING(MI_TYPE_TIME     ),
								STRING(MI_TYPE_WEAPON   ),
								STRING(MI_TYPE_VEHICLE  ),
								STRING(MI_TYPE_PED      ),
								STRING(MI_TYPE_COMPOSITE),
							};
							CompileTimeAssert(NELEM(modelTypeStrings) == NUM_MI_TYPES);

							atString drawablePath("");

							switch ((int)modelInfo->GetDrawableType())
							{
							case fwArchetype::DT_DRAWABLE           : drawablePath = CDTVStreamingIteratorTest_GetAssetPath(g_DrawableStore, modelInfo->SafeGetDrawableIndex(), INDEX_NONE); break;
							case fwArchetype::DT_FRAGMENT           : drawablePath = CDTVStreamingIteratorTest_GetAssetPath(g_FragmentStore, modelInfo->SafeGetFragmentIndex(), INDEX_NONE); break;
							case fwArchetype::DT_DRAWABLEDICTIONARY : drawablePath = CDTVStreamingIteratorTest_GetAssetPath(g_DwdStore     , modelInfo->SafeGetDrawDictIndex(), INDEX_NONE); break;
							}

							const char* drawablePath2 = drawablePath.c_str();

							if (strstr(drawablePath2, "unzipped/"))
							{
								drawablePath2 = strstr(drawablePath2, "unzipped/") + strlen("unzipped/");
							}

							siLog(
								">> [7] \"%s\" (%s) not used by any map data entities (type=%s)",
								modelInfo->GetModelName(),
								drawablePath2,
								modelTypeStrings[modelInfo->GetModelType()]
							);

							numArchetypesWithNoInstances++;
						}
						else if (count == 1)
						{
							numArchetypesWithOneInstance++;
						}
						else if (count > 1)
						{
							numArchetypesWithMultipleInstances++;
						}
					}
				}
			}

			Displayf("### found %d archetypes with no instances (BASE/MLO/TIME/COMPOSITE)", numArchetypesWithNoInstances);
			Displayf("### found %d archetypes with one instance", numArchetypesWithOneInstance);
			Displayf("### found %d archetypes with multiple instances", numArchetypesWithMultipleInstances);
		}

		if (gStreamingIteratorTest_ArchetypeEntityCounts)
		{
			delete[] gStreamingIteratorTest_ArchetypeEntityCounts;
			gStreamingIteratorTest_ArchetypeEntityCounts = NULL;
		}

		return;
	}

	if (gStreamingIteratorTest_BuildAssetAnalysisData)
	{
		AssetAnalysis::ProcessMapDataSlot(slot);
		return;
	}

	if (gStreamingIteratorTest_MapDataProcessParents && def && def->GetParentDef()) // handle parents (we could do this for other store types too, in theory .. it would be a bit complex)
	{
		fwMapDataDef* parentDef  = def->GetParentDef();
		const int     parentSlot = g_MapDataStore.GetSlotIndex(parentDef);

		Assert(parentDef == g_MapDataStore.GetSlot(parentSlot));
		Assert((parentDef->GetContentFlags() & fwMapData::CONTENTFLAG_ENTITIES_HD) == 0); // parents can't be HD

		if (parentDef->GetIsValid() && !parentDef->GetIsScriptManaged())
		{
			IteratorFunc(parentDef, true, parentSlot, frames);
		}
	}

	fwMapDataContents* mapData = def ? def->m_pObject : NULL;
	const char* mapDataName = g_MapDataStore.GetName(slot);

	if (mapData)
	{
		if (gStreamingIteratorTest_MapDataProcessParents) // since we recursively process parents, we need to skip slots which have already been handled
		{
			if (m_slotsProcessed == NULL)
			{
				m_slotsProcessedSize = (g_MapDataStore.GetSize() + 7)/8;
				m_slotsProcessed = rage_new u8[m_slotsProcessedSize];
				sysMemSet(m_slotsProcessed, 0, m_slotsProcessedSize);
			}
			else if (m_slotsProcessed[slot/8] & BIT(slot%8))
			{
				return; // do nothing, we've loaded this slot by necessity (due to the hierarchy) but it's already been processed
			}

			m_slotsProcessed[slot/8] |= BIT(slot%8);
		}

		if (gStreamingIteratorTest_MapDataDumpTCModBoxes)
		{
			const fwPool<tcBox>* pPool = tcBox::GetPool();

			if (AssertVerify(pPool))
			{
				int numBoxes[3] = {0,0,0};

				for (int i = 0; i < pPool->GetSize(); i++)
				{
					const tcBox* pBox = pPool->GetSlot(i);

					if (pBox && pBox->GetIplId() == slot)
					{
						const tcModifier* pModifier = g_timeCycle.GetModifier(pBox->GetIndex());

						if (pModifier)
						{
							bool bHasModifierWaterReflection = false;
							bool bHasModifierWaterReflectionFarClip = false;

							for (int j = 0; j < pModifier->GetModDataCount(); j++)
							{
								const int varId = pModifier->GetModDataArray()[j].varId;

								if (varId == TCVAR_WATER_REFLECTION)
								{
									bHasModifierWaterReflection = true;
								}
								else if (varId == TCVAR_WATER_REFLECTION_FAR_CLIP)
								{
									const float valA = pModifier->GetModDataArray()[j].valA;
									const float valB = pModifier->GetModDataArray()[j].valB;

									if (valA == 0.0f &&
										valB == 0.0f)
									{
										bHasModifierWaterReflectionFarClip = true;
									}
									else // why would we change the far clip to something other than zero? let's warn if this happens ..
									{
										Displayf("### mapdata \"%s\" timecycle modifier box %d/%d sets TCVAR_WATER_REFLECTION_FAR_CLIP values = [%f,%f]", mapDataName, i, j, valA, valB);
									}
								}

								if (s_timecycleModifierBoxCounts == NULL)
								{
									s_timecycleModifierBoxCounts = rage_new int[TCVAR_NUM];
								}

								s_timecycleModifierBoxCounts[varId]++;

								//Displayf("mod %s ..", g_varInfos[varId].name);
							}

							if (1) // all
							{
								CDTVStreamingIteratorTest_AddModifierBoxGeometry(s_timecycleModifierBoxFiles[0], "modifiers[all]", "cyan", 1001, s_timecycleModifierBoxQuadCounts[0], pBox->GetBoundingBox());
								numBoxes[0]++;
							}

							if (bHasModifierWaterReflection)
							{
								CDTVStreamingIteratorTest_AddModifierBoxGeometry(s_timecycleModifierBoxFiles[1], "modifiers[WATER_REFLECTION]", "yellow", 1001, s_timecycleModifierBoxQuadCounts[1], pBox->GetBoundingBox());
								numBoxes[1]++;
							}

							if (bHasModifierWaterReflectionFarClip)
							{
								CDTVStreamingIteratorTest_AddModifierBoxGeometry(s_timecycleModifierBoxFiles[2], "modifiers[WATER_REFLECTION_FAR_CLIP]", "white", 1001, s_timecycleModifierBoxQuadCounts[2], pBox->GetBoundingBox());
								numBoxes[2]++;
							}
						}
					}
				}

				if (numBoxes[0] > 0)
				{
					Displayf("### mapdata \"%s\" has %d modifier boxes", mapDataName, numBoxes[0]);
				}

				if (numBoxes[1] > 0)
				{
					Displayf("### mapdata \"%s\" has %d boxes modifying TCVAR_WATER_REFLECTION", mapDataName, numBoxes[1]);
				}

				if (numBoxes[2] > 0)
				{
					Displayf("### mapdata \"%s\" has %d boxes modifying TCVAR_WATER_REFLECTION_FAR_CLIP", mapDataName, numBoxes[2]);
				}
			}
		}

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS
		if (gStreamingIteratorTest_BuildTextureUsageMap)
		{
			// ...
		}
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS

		m_numMapDatas++;
		m_numStreamed += (frames > 0) ? 1 : 0;

		if (gStreamingIteratorTest_Verbose)
		{
			atString temp = frames > 0 ? atVarString(" [STREAMED IN %d FRAMES]", frames) : "";

			if (1) // show flags
			{
				if (def->GetContentFlags() & fwMapData::CONTENTFLAG_ENTITIES_HD           ) { temp += atString(",HD"); }
				if (def->GetContentFlags() & fwMapData::CONTENTFLAG_ENTITIES_LOD          ) { temp += atString(",LOD"); }
				if (def->GetContentFlags() & fwMapData::CONTENTFLAG_ENTITIES_CONTAINERLOD ) { temp += atString(",CLOD"); }
				if (def->GetContentFlags() & fwMapData::CONTENTFLAG_MLO                   ) { temp += atString(",MLO"); }
				if (def->GetContentFlags() & fwMapData::CONTENTFLAG_BLOCKINFO             ) { temp += atString(",BLOCK"); }
				if (def->GetContentFlags() & fwMapData::CONTENTFLAG_OCCLUDER              ) { temp += atString(",OCC"); }
				if (def->GetContentFlags() & fwMapData::CONTENTFLAG_ENTITIES_USING_PHYSICS) { temp += atString(",PHYS"); }
				if (def->GetContentFlags() & fwMapData::CONTENTFLAG_LOD_LIGHTS            ) { temp += atString(",LODLIGHTS"); }
				if (def->GetContentFlags() & fwMapData::CONTENTFLAG_DISTANT_LOD_LIGHTS    ) { temp += atString(",DISTLODLIGHTS"); }
				if (def->GetContentFlags() & fwMapData::CONTENTFLAG_ENTITIES_CRITICAL     ) { temp += atString(",CRITICAL"); }
				if (def->GetContentFlags() & fwMapData::CONTENTFLAG_INSTANCE_LIST         ) { temp += atString(",INSTLIST"); }
			}

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_RPFTEST
			strIndex          streamingIndex = g_MapDataStore.GetStreamingIndex(slot);
			strStreamingInfo* pInfo          = strStreamingEngine::GetInfo().GetStreamingInfo(streamingIndex);
			strStreamingFile* pFile          = strPackfileManager::GetImageFileFromHandle(pInfo->GetHandle());

			temp += atVarString(" [RPF=\"%s\"]", pFile ? pFile->m_packfile->GetDebugName() : "NULL");
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_RPFTEST

			siLog(
				">> g_MapDataStore[%d] = \"%s\" has %d entities%s",
				slot,
				mapDataName,
				mapData->GetNumEntities(),
				temp.c_str()
			);
		}

		atArray<CEntity*> requests;

		for (int i = 0; i < mapData->GetNumEntities(); i++)
		{
			CEntity* entity = static_cast<const CEntity*>(mapData->GetEntities()[i]);

			if (entity == NULL)
			{
				continue;
			}

			CBaseModelInfo* modelInfo = entity->GetBaseModelInfo();

			if (modelInfo == NULL)
			{
				continue;
			}

			atString drawablePath = atString("");

			switch ((int)modelInfo->GetDrawableType())
			{
			case fwArchetype::DT_FRAGMENT           : drawablePath = CDTVStreamingIteratorTest_GetAssetPath(g_FragmentStore, modelInfo->GetFragmentIndex(), INDEX_NONE); break;
			case fwArchetype::DT_DRAWABLE           : drawablePath = CDTVStreamingIteratorTest_GetAssetPath(g_DrawableStore, modelInfo->GetDrawableIndex(), INDEX_NONE); break;
			case fwArchetype::DT_DRAWABLEDICTIONARY : drawablePath = CDTVStreamingIteratorTest_GetAssetPath(g_DwdStore     , modelInfo->GetDrawDictIndex(), modelInfo->GetDrawDictDrawableIndex()); break;
			}

			const Vec3V centre = entity->GetMatrix().GetCol3();

			if (gStreamingIteratorTest_MapDataShowAllEntities)
			{
				Displayf("    %f,%f,%f: %s - (%s)", VEC3V_ARGS(centre), entity->GetModelName(), drawablePath.c_str());
			}
			else if (gStreamingIteratorTest_FindArchetypeInMapData[0] != '\0')
			{
				if (StringMatch(modelInfo->GetModelName(), gStreamingIteratorTest_FindArchetypeInMapData))
				{
					Displayf("g_MapDataStore[%d] \"%s\" entry %d uses archetype \"%s\", position = %f,%f,%f", slot, mapDataName, i, modelInfo->GetModelName(), VEC3V_ARGS(centre));
				}
			}

			if (gStreamingIteratorTest_MapDataCountInstances)
			{
				int proxyIndex = -1;
				CDebugArchetype::GetDebugArchetypeProxyForModelInfo(modelInfo, &proxyIndex);

				if (proxyIndex != -1)
				{
					if (gStreamingIteratorTest_ArchetypeEntityCounts == NULL)
					{
						const int maxCount = CDebugArchetype::GetMaxDebugArchetypeProxies();
						gStreamingIteratorTest_ArchetypeEntityCounts = rage_new u8[maxCount];
						sysMemSet(gStreamingIteratorTest_ArchetypeEntityCounts, 0, maxCount*sizeof(u8));
					}

					u8& count = gStreamingIteratorTest_ArchetypeEntityCounts[proxyIndex];

					if (count < 0xff)
					{
						count++;
					}

					if (modelInfo->GetModelType() == MI_TYPE_MLO)
					{
						const atArray<fwEntityDef*>* interiorEntityDefs = &static_cast<CMloModelInfo*>(modelInfo)->GetEntities();

						if (interiorEntityDefs) // this can be NULL?
						{
							for (int j = 0; j < interiorEntityDefs->GetCount(); j++)
							{
								const fwEntityDef* interiorEntityDef = interiorEntityDefs->operator[](j);

								if (interiorEntityDef)
								{
									const u32 interiorModelIndex = CModelInfo::GetModelIdFromName(interiorEntityDef->m_archetypeName).GetModelIndex();
									const char* interiorModelName = interiorEntityDef->m_archetypeName.TryGetCStr();

									if (interiorModelName == NULL)
									{
										interiorModelName = "NULL";
									}

									if (interiorModelIndex != fwModelId::MI_INVALID)
									{
										const CBaseModelInfo* interiorModelInfo = CModelInfo::GetBaseModelInfo(fwModelId(interiorModelIndex));

										if (interiorModelInfo)
										{
											int interiorProxyIndex = -1;
											CDebugArchetype::GetDebugArchetypeProxyForModelInfo(interiorModelInfo, &interiorProxyIndex);

											if (interiorProxyIndex != -1)
											{
												u8& interiorCount = gStreamingIteratorTest_ArchetypeEntityCounts[interiorProxyIndex];

												if (interiorCount < 0xff)
												{
													interiorCount++;
												}
											}
											else
											{
												siLog(">> !ERROR: can't find archetype proxy for interior model \"%s\" in \"%s\"", interiorModelInfo->GetModelName(), modelInfo->GetModelName());
											}
										}
										else
										{
											siLog(">> !ERROR: CModelInfo::GetBaseModelInfo returned NULL for interior model \"%s\" in \"%s\"", interiorModelName, modelInfo->GetModelName());
										}
									}
									else
									{
										siLog(">> !ERROR: CModelInfo::GetModelIdFromName returned MI_INVALID for interior model \"%s\" in \"%s\"", interiorModelName, modelInfo->GetModelName());
									}
								}
							}
						}
					}
					else if (gStreamingIteratorTest_MapDataCreateEntities)
					{
						Assertf(0, "WARNING -- this codepath has never worked, use at your own peril");

						if (!entity->GetBaseModelInfo()->GetHasLoaded())
						{
							if (requests.GetCount() < 10) // for now, limit to 10 requests ..
							{
								CModelInfo::RequestAssets(entity->GetBaseModelInfo(), 0);
								requests.PushAndGrow(entity);
							}
						}
						else
						{
							if (entity->GetDrawHandlerPtr() == NULL)
							{
								entity->CreateDrawable();
							}

							if (entity->GetDrawHandlerPtr())
							{
								Displayf("ok for entity %s", entity->GetModelName());
							}
							else
							{
								Displayf("FAILED TO CREATE DRAWABLE FOR ENTITY %s!", entity->GetModelName());
							}
						}
					}
				}
				else
				{
					siLog(">> !ERROR: can't find archetype proxy for model \"%s\"", modelInfo->GetModelName());
				}
			}

			if (requests.GetCount() > 0)
			{
				CStreaming::LoadAllRequestedObjects();

				for (int i = 0; i < requests.GetCount(); i++)
				{
					CEntity* entity = requests[i];

					if (entity->GetBaseModelInfo()->GetHasLoaded())
					{
						if (entity->GetDrawHandlerPtr() == NULL)
						{
							entity->CreateDrawable();
						}

						if (entity->GetDrawHandlerPtr())
						{
							Displayf("ok for requested entity %s", entity->GetModelName());
						}
						else
						{
							Displayf("FAILED TO CREATE REQUESTED DRAWABLE FOR %s!", entity->GetModelName());
						}
					}
					else
					{
						Displayf("ENTITY FAILED TO LOAD REQUESTED ENTITY %s!", entity->GetModelName());
					}
				}
			}
		}

		if (!bIsParentDef && gStreamingIteratorTest_MapDataRemoveEntities)
		{
			if ((def->GetContentFlags() & fwMapData::CONTENTFLAG_ENTITIES_HD) != 0)
			{
				g_MapDataStore.SafeRemove(slot); // remove the entities
			}
			else
			{
				Assertf(0, "g_MapDataStore[%d] = \"%s\" is not a high detail map data! why are we trying to remove it?", slot, CDTVStreamingIteratorTest_GetAssetPath(g_MapDataStore, slot, INDEX_NONE).c_str());
			}
		}
	}
	else
	{
		if (frames < 0)
		{
			if (gStreamingIteratorTest_Verbose)
			{
				siLog(">> g_MapDataStore[%d] = \"%s\" not in streaming image!", slot, CDTVStreamingIteratorTest_GetAssetPath(g_MapDataStore, slot, INDEX_NONE).c_str());
			}

			m_numNotInImage++;
		}
		else
		{
			if (gStreamingIteratorTest_Verbose || gStreamingIteratorTest_VerboseErrors)
			{
				siLog(">> !ERROR: g_MapDataStore[%d] = \"%s\" failed to stream in!", slot, CDTVStreamingIteratorTest_GetAssetPath(g_MapDataStore, slot, INDEX_NONE).c_str());
			}

			m_numFailed++;
		}
	}
}

void CDTVStreamingIteratorTest_MapDataStore::Reset()
{
	if (m_si)
	{
		m_si->Reset();
		delete m_si;
		m_si = NULL;
	}

	m_enabled       = false;
	m_numMapDatas   = 0;
	m_numStreamed   = 0;
	m_numFailed     = 0;
	m_numNotInImage = 0;

	if (m_slotsProcessed)
	{
		sysMemSet(m_slotsProcessed, 0, m_slotsProcessedSize);
	}
}

void CDTVStreamingIteratorTest_MapDataStore::Update(int count, int maxStreamingSlots)
{
	DEBUG_TEXTURE_VIEWER_USE_DEBUG_MEMORY();

	if (!m_enabled)
	{
		if (m_si)
		{
			Reset();
		}

		return;
	}

	if (m_si == NULL)
	{
		m_si = rage_new CDTVStreamingIterator_MapDataStore(this, m_currentSlot, g_MapDataStore, _Func, gStreamingIteratorTest_SearchRPF, gStreamingIteratorTest_SearchName, gStreamingIteratorTest_SearchFlags);

		m_numMapDatas   = 0;
		m_numStreamed   = 0;
		m_numFailed     = 0;
		m_numNotInImage = 0;

		siLog(">> STREAMING ITERATOR STARTING .. %d/%d MAPDATA SLOTS", g_MapDataStore.GetNumUsedSlots(), g_MapDataStore.GetSize());
	}

	if (m_si->Update(count, maxStreamingSlots))
	{
		siLog(">> STREAMING ITERATOR COMPLETE");
		siLog("   mapdatas : %d"   , m_numMapDatas               );
		siLog("   streamed : %d"   , m_numStreamed               );
		siLog("   failed   : %d/%d", m_numFailed, m_numNotInImage);
	}
}

bool gStreamingIteratorTest_Verbose = false;
bool gStreamingIteratorTest_VerboseErrors = true;
char gStreamingIteratorTest_SearchRPF[80] = "";
char gStreamingIteratorTest_SearchName[80] = "";
u32  gStreamingIteratorTest_SearchFlags = 0;

bool gStreamingIteratorTest_FindUncompressedTexturesNew = false;
char gStreamingIteratorTest_FindShaderInDrawables[80] = "";
char gStreamingIteratorTest_FindTextureInDrawables[80] = "";
char gStreamingIteratorTest_FindArchetypeInMapData[80] = "";

bool gStreamingIteratorTest_CheckCableDrawables = false;
bool gStreamingIteratorTest_CheckCableDrawablesSimple = false;

bool gStreamingIteratorTest_MapDataCountInstances = false;
bool gStreamingIteratorTest_MapDataCreateEntities = false;
bool gStreamingIteratorTest_MapDataProcessParents = false;
bool gStreamingIteratorTest_MapDataShowAllEntities = false;
bool gStreamingIteratorTest_MapDataRemoveEntities = false; // TODO -- fix this
bool gStreamingIteratorTest_MapDataDumpTCModBoxes = false;

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TXDCHILDREN
bool gStreamingIteratorTest_TxdChildren = false;
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TXDCHILDREN

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_REDUNDANT
bool gStreamingIteratorTest_FindRedundantTextures = false;
bool gStreamingIteratorTest_FindRedundantTexturesByHash = false;
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_REDUNDANT

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEST && DEBUG_TEXTURE_VIEWER_FIND_INEFFECTIVE_TEXTURES
bool gStreamingIteratorTest_FindIneffectiveTexturesBumpTex = false;
bool gStreamingIteratorTest_FindIneffectiveTexturesSpecularTex = false;
bool gStreamingIteratorTest_FindIneffectiveTexturesCheckFlat = false;
bool gStreamingIteratorTest_FindIneffectiveTexturesCheckFlatGT4x4 = false;
bool gStreamingIteratorTest_FindNonDefaultSpecularMasks = false;
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEST && DEBUG_TEXTURE_VIEWER_FIND_INEFFECTIVE_TEXTURES

bool gStreamingIteratorTest_BuildAssetAnalysisData = false;
#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS
bool gStreamingIteratorTest_BuildTextureUsageMap = false;
int  gStreamingIteratorTest_BuildTextureUsageMapPhase = 0;
int  gStreamingIteratorTest_BuildTextureUsageMapTextureHashNumLines = 0;
char gStreamingIteratorTest_BuildTextureUsageMapShaderFilter[256] = "";
char gStreamingIteratorTest_BuildTextureUsageMapTextureFilter[256] = "";
bool gStreamingIteratorTest_BuildTextureUsageMapDATOutput = false;
bool gStreamingIteratorTest_BuildTextureUsageMapDATFlush = false;
bool gStreamingIteratorTest_BuildTextureUsageMapCSVOutput = false;
bool gStreamingIteratorTest_BuildTextureUsageMapCSVFlush = false;
bool gStreamingIteratorTest_BuildTextureUsageMapRuntimeTest = false; // too much memory i think
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_FIND_UNUSED
bool  gStreamingIteratorTest_FindUnusedSharedTextures = false;
s64   gStreamingIteratorTest_FindUnusedSharedTexturesMemoryUsed         = 0;
float gStreamingIteratorTest_FindUnusedSharedTexturesMemoryUsedMB       = 0.0f;
s64   gStreamingIteratorTest_FindUnusedSharedTexturesMemoryUnused       = 0;
float gStreamingIteratorTest_FindUnusedSharedTexturesMemoryUnusedMB     = 0.0f;
s64   gStreamingIteratorTest_FindUnusedSharedTexturesMemoryTotal        = 0;
float gStreamingIteratorTest_FindUnusedSharedTexturesMemoryTotalMB      = 0.0f;
s64   gStreamingIteratorTest_FindUnusedSharedTexturesMemoryNotCounted   = 0;
float gStreamingIteratorTest_FindUnusedSharedTexturesMemoryNotCountedMB = 0.0f;
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_FIND_UNUSED

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_FIND_UNUSED_DRAWABLES_IN_DWDS
bool gStreamingIteratorTest_FindUnusedDrawablesInDwds = false;
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_FIND_UNUSED_DRAWABLES_IN_DWDS

#if DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEXTURE
bool gStreamingIteratorTest_FindBadNormalMaps = false;
bool gStreamingIteratorTest_FindBadNormalMapsCheckAvgColour = false;
bool gStreamingIteratorTest_FindUnusedChannels = false;
bool gStreamingIteratorTest_FindUnusedChannelsSwizzle = false;
bool gStreamingIteratorTest_FindUncompressedTextures = false;
bool gStreamingIteratorTest_FindUnprocessedTextures = false;
bool gStreamingIteratorTest_FindConstantTextures = false; // find textures where the top mip is a solid colour (or identical DXT blocks)
#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEXTURE

// ================================================================================================

#if 0
namespace _StreamingIteratorTest {

static atString _StringRep(int n, const char* s) // stringutil.h
{
	atString result;

	for (int i = 0; i < n; i++)
	{
		result += s;
	}

	return result;
}

// copied from DebugTextureViewerUtil.cpp
static int _GetTextureSizeInBytes(const grcTexture* texture)
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

// copied from DebugTextureViewerUtil.cpp
static int _FindTxdSlot(const fwTxd* txd)
{
	if (txd)
	{
		for (int i = 0; i < g_TxdStore.GetSize(); i++)
		{
			if (g_TxdStore.GetSafeFromIndex(i) == txd && !g_TxdStore.GetSlot(i)->m_isDummy)
			{
				return i;
			}
		}
	}

	return INDEX_NONE;
}

CTxdNode::CTxdNode(const fwTxd* txd, int slot)
{
	m_slot        = slot;
	m_parentSlot  = _FindTxdSlot(txd->GetParent());

	if (m_parentSlot == INDEX_NONE && g_TxdStore.IsValidSlot(slot)) // txd's have special parents ..
	{
		m_parentSlot = g_TxdStore.GetParentTxdSlot(slot);
	}

	m_name        = g_TxdStore.GetName(slot);
	m_validSlot   = g_TxdStore.IsValidSlot(slot);
	m_streamIndex = g_TxdStore.GetStreamingIndex(slot).Get(); // need to check if m_validSlot is true first?
	m_streamImage = g_TxdStore.IsObjectInImage(slot);
	m_numTextures = txd->GetCount();
	m_maxTexDims  = 0;
	m_sizeInBytes = 0;

	for (int i = 0; i < txd->GetCount(); i++)
	{
		const grcTexture* texture = txd->GetEntry(i);

		if (texture)
		{
			m_maxTexDims = Max<int>(texture->GetWidth (), m_maxTexDims);
			m_maxTexDims = Max<int>(texture->GetHeight(), m_maxTexDims);

			m_sizeInBytes += _GetTextureSizeInBytes(texture);
		}
	}
}

bool CTxdNode::Insert(CTxdNode* node)
{
	if (m_slot == node->m_parentSlot)
	{
		m_children.PushAndGrow(node);
		return true;
	}

	for (int i = 0; i < m_children.size(); i++)
	{
		if (m_children[i]->Insert(node))
		{
			return true;
		}
	}

	return false;
}

void CTxdNode::Display(const char* prefix, int indent) const
{
	Displayf(atVarString("%s%s%s", prefix, _StringRep(indent*2, " ").c_str(), m_name.c_str()));

	for (int i = 0; i < m_children.size(); i++)
	{
		m_children[i]->Display(prefix, indent + 2);
	}
}

#if __WIN32
#pragma warning(push)
#pragma warning(disable:4355) // 'this' : used in base member initializer list
#endif

CTxdTree::CTxdTree() : CDTVStreamingIterator_TxdStore(this, m_currentSlot, g_TxdStore, _Func), m_currentSlot(0)
{}

#if __WIN32
#pragma warning(pop)
#endif

void CTxdTree::IteratorFunc(fwTxdDef* def, bool, int slot, int frames)
{
	DEBUG_TEXTURE_VIEWER_USE_DEBUG_MEMORY();

	if (slot == -1) // update finished
	{
		return;
	}

	fwTxd* txd = def ? def->m_pObject : NULL;

	UNUSED_VAR(frames);

	if (txd)
	{
		m_nodes.PushAndGrow(rage_new CTxdNode(txd, slot));
	}
}

void CTxdTree::Update()
{
	if (CDTVStreamingIterator_TxdStore::Update())
	{
		for (int i = 0; i < m_nodes.size(); i++) // O(N^2), yuck
		{
			CTxdNode* node = m_nodes[i];

			if (node->m_parentSlot != INDEX_NONE) // has a parent slot, should be able to insert it somewhere
			{
				bool bInserted = false;

				for (int j = 0; j < m_nodes.size(); j++)
				{
					if (m_nodes[j]->Insert(node))
					{
						CTxdNode* swap = m_nodes.Pop();

						if (i < m_nodes.size()) // move top node into the now empty array slot
						{
							m_nodes[i] = swap;
						}

						bInserted = true;
						break;
					}
				}

				if (!bInserted)
				{
					Displayf("CTxdTree::Update - failed to insert node \"%s\"!", node->m_name.c_str());
				}
			}
		}

		for (int i = 0; i < m_nodes.size(); i++)
		{
			m_nodes[i]->Display(">> ", 0);
		}
	}
}

} // namespace _StreamingIteratorTest
#endif // 0

#endif // DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEST
#endif // __BANK
