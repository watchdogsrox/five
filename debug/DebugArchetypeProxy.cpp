// =============================
// debug/DebugArchetypeProxy.cpp
// (c) 2012 RockstarNorth
// =============================

#include "debug/DebugArchetypeProxy.h"

#if USE_DEBUG_ARCHETYPE_PROXIES

#include "fwscene/stores/txdstore.h"
#include "fwscene/stores/dwdstore.h"
#include "fwscene/stores/drawablestore.h"
#include "fwscene/stores/fragmentstore.h"
#include "fwscene/stores/maptypesstore.h"
#include "streaming/packfilemanager.h"

#include "control/gamelogic.h"
#include "debug/AssetAnalysis/AssetAnalysisUtil.h"
#include "modelinfo/ModelInfo.h"
#include "scene/DataFileMgr.h"
#include "streaming/streaming.h"

static u8*  g_debugArchetypeProxyData = NULL;
static u32  g_debugArchetypeProxySize = 0;
static u32  g_debugArchetypeProxyCount = 0;
static u32  g_debugArchetypeProxyCountExtraRequired = 0;
static u32  g_debugArchetypeProxyCurrentIndex = 0; // used internally in CreateDebugArchetypeProxies
static int  g_debugArchetypeProxyCurrentMapTypeSlot = -1;
static bool g_debugArchetypeProxiesBeingCreated = false; // currently being created ..
static bool g_debugArchetypeProxiesCreatingStatic = false;
static bool g_debugArchetypeProxiesFinishedCreation = false; // all done
static bool g_debugArchetypeProxyVerbose = false;

#if 0
// wrapper to make std::map look like atMap
template <typename KeyType,typename DataType> class std_map : public std::map<KeyType,DataType>
{
public:
	inline const DataType* Access(const KeyType& key) const
	{
		typename std::map<KeyType,DataType>::const_iterator it = std::map<KeyType,DataType>::find(key);
		return (it != std::map<KeyType,DataType>::end()) ? &it->second : NULL;
	}

	inline DataType* Access(const KeyType& key)
	{
		typename std::map<KeyType,DataType>::iterator it = std::map<KeyType,DataType>::find(key);
		return (it != std::map<KeyType,DataType>::end()) ? &it->second : NULL;
	}
};
static std_map<u32,int> g_debugArchetypeProxyMap;
#else
static atMap<u32,int> g_debugArchetypeProxyMap;
#endif

bool CDebugArchetype::AreDebugArchetypeProxiesBeingCreated()
{
	return g_debugArchetypeProxiesBeingCreated;
}

void CDebugArchetype::CreateDebugArchetypeProxies()
{
	if (g_debugArchetypeProxyData)
	{
		return;
	}

	dlDrawListMgr::SuppressAddTypeFileReferencesBegin();

	g_debugArchetypeProxiesBeingCreated = true;

	g_debugArchetypeProxySize = Max<u32>(g_debugArchetypeProxySize, sizeof(CDebugArchetypeProxy));
	g_debugArchetypeProxySize = Max<u32>(g_debugArchetypeProxySize, sizeof(CDebugPedArchetypeProxy));
	g_debugArchetypeProxySize = Max<u32>(g_debugArchetypeProxySize, sizeof(CDebugVehicleArchetypeProxy));

	g_debugArchetypeProxyCount = 130000; // actual count was 126706

	{
		// Debug Heap
		sysMemAutoUseDebugMemory debug;
		g_debugArchetypeProxyData = rage_new u8[g_debugArchetypeProxySize*g_debugArchetypeProxyCount];

	}

	// create map with maximum number of slots
	{
		USE_DEBUG_MEMORY();
		g_debugArchetypeProxyMap.Create(ATL_MAP_MAX_SIZE, false);
	}

	if (g_debugArchetypeProxyVerbose)
	{
		Displayf("streaming in %d mapType slots..", g_MapTypesStore.GetSize());
	}
	else
	{
		Displayf("creating debug archetype proxies ... this may take a few seconds");
	}

	class CreateArchetypeProxy { public: static void func(fwArchetype* pArchetype)
	{
		const CBaseModelInfo* pMI = dynamic_cast<const CBaseModelInfo*>(pArchetype);

		if (pMI == NULL)
		{
			return; // there are shitty things in the factories which shouldn't be there ..
		}

		if (AssertVerify(g_debugArchetypeProxyCurrentIndex < g_debugArchetypeProxyCount))
		{
			if (g_debugArchetypeProxyVerbose)
			{
				Displayf("creating archetype proxy for \"%s\"", pArchetype->GetModelName());
			}

			const u32 proxyIndex = g_debugArchetypeProxyCurrentIndex++;
			CDebugArchetypeProxy* pDesc = reinterpret_cast<CDebugArchetypeProxy*>(&g_debugArchetypeProxyData[g_debugArchetypeProxySize*proxyIndex]);
			pDesc->Create(pMI, proxyIndex, g_debugArchetypeProxyCurrentMapTypeSlot, g_debugArchetypeProxiesCreatingStatic);
		}
		else
		{
			g_debugArchetypeProxyCountExtraRequired++;
		}
	}};

	const u32 N = 4;

	for (u32 i = 0; i < g_MapTypesStore.GetSize(); i += N)
	{
		u32 slots[N];
		u32 slotsLoaded = 0;
		u32 slotsWaiting = 0;

		for (u32 j = 0; j < N; j++)
		{
			const strLocalIndex slot = strLocalIndex(i + j);

			if (slot.Get() < g_MapTypesStore.GetSize() && g_MapTypesStore.IsValidSlot(slot))
			{
				const fwMapTypesDef* pDef = g_MapTypesStore.GetSlot(slot);

				if (pDef && !pDef->GetIsPermanent())
				{
					const fwMapTypesContents* pContents = g_MapTypesStore.Get(slot);

					slots[j] = slot.Get();

					if (pContents)
					{
						if (g_debugArchetypeProxyVerbose)
						{
							Displayf("maptype[%d] \"%s\" is already loaded", slot.Get(), g_MapTypesStore.GetName(slot));
						}

						slotsLoaded |= BIT(j);
						slotsWaiting |= BIT(j);
					}
					else if (g_MapTypesStore.IsObjectInImage(slot))
					{
						if (g_debugArchetypeProxyVerbose)
						{
							Displayf("maptype[%d] \"%s\" is requesting streaming ..", slot.Get(), g_MapTypesStore.GetName(slot));
						}

						g_MapTypesStore.StreamingRequest(slot, STRFLAG_DONTDELETE|STRFLAG_PRIORITY_LOAD|STRFLAG_FORCE_LOAD);
						slotsWaiting |= BIT(j);
					}
					else // not in streaming image? wtf.
					{
						Displayf("maptype[%d] \"%s\" is not in streaming image", slot.Get(), g_MapTypesStore.GetName(slot));
					}
				}
			}
		}

		CStreaming::LoadAllRequestedObjects();

		while (slotsWaiting)
		{
			sysIpcSleep(1);

			for (u32 j = 0; j < N; j++)
			{
				const strLocalIndex slot = strLocalIndex(i + j);
				fwMapTypesContents* pContents = NULL;

				if (pContents == NULL && (slotsLoaded & BIT(j)) != 0)
				{
					pContents = g_MapTypesStore.Get(slot);

					if (AssertVerify(pContents))
					{
						if (g_debugArchetypeProxyVerbose)
						{
							Displayf("maptype[%d] \"%s\" ready", slot.Get(), g_MapTypesStore.GetName(slot));
						}
					}
				}

				if (pContents == NULL && (slotsWaiting & BIT(j)) != 0)
				{
					pContents = g_MapTypesStore.Get(slot);

					if (pContents)
					{
						if (g_debugArchetypeProxyVerbose)
						{
							Displayf("maptype[%d] \"%s\" has been streamed in", slot.Get(), g_MapTypesStore.GetName(slot));
						}
					}
					else
					{
						// still waiting for it
					}
				}

				if (pContents)
				{
					g_debugArchetypeProxyCurrentMapTypeSlot = slot.Get();
					fwArchetypeManager::ForAllArchetypesInFile(slot.Get(), CreateArchetypeProxy::func);
					g_debugArchetypeProxyCurrentMapTypeSlot = -1;
					g_MapTypesStore.ClearRequiredFlag(slot.Get(), STRFLAG_DONTDELETE);

					slotsLoaded &= ~BIT(j); // we got it
					slotsWaiting &= ~BIT(j);
				}
				else if (g_debugArchetypeProxyVerbose && (slotsWaiting & BIT(j)) != 0)
				{
					Displayf("maptype[%d] \"%s\" still waiting ..", slot.Get(), g_MapTypesStore.GetName(slot));
				}
			}

			g_MapTypesStore.Update();
		}
	}

	// get static archetypes
	{
		g_debugArchetypeProxiesCreatingStatic = true;

		for (u32 j = CModelInfo::GetStartModelInfos(); j < CModelInfo::GetMaxModelInfos(); j++)
		{
			CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(j)));

			if (pModelInfo && !pModelInfo->IsStreamedArchetype())
			{
				CreateArchetypeProxy::func(pModelInfo);
			}
		}

		g_debugArchetypeProxiesCreatingStatic = false;
	}

#if __ASSERT
	if (CGameLogic::IsRunningGTA5Map())
	{
		const u32 numPages1 = (g_debugArchetypeProxySize*g_debugArchetypeProxyCount        + 64*1024 - 1)/(64*1024);
		const u32 numPages2 = (g_debugArchetypeProxySize*g_debugArchetypeProxyCurrentIndex + 64*1024 - 1)/(64*1024);

		const u32 numExtraBytesAllocated = (numPages1 - numPages2)*(64*1024);

		if (g_debugArchetypeProxyCountExtraRequired > 0)
		{
			Displayf(
				"CDebugArchetype::CreateDebugArchetypeProxies allocated %d archetypes, but needs total count of %d",
				g_debugArchetypeProxyCount,
				g_debugArchetypeProxyCount + g_debugArchetypeProxyCountExtraRequired);

			Assertf(0, "CDebugArchetype::CreateDebugArchetypeProxies ran out of memory, please increase g_debugArchetypeProxyCount");
		}
		else if (numExtraBytesAllocated >= 192*1024)
		{
			Displayf(
				"CDebugArchetype::CreateDebugArchetypeProxies allocated %.2fKB plus %.2fKB extra for total %d archetypes, actual count was %d",
				(float)numPages1*64.0f,
				(float)numExtraBytesAllocated/1024.0f,
				g_debugArchetypeProxyCount,
				g_debugArchetypeProxyCurrentIndex);
		}
		else
		{
			Displayf(
				"CDebugArchetype::CreateDebugArchetypeProxies allocated %.2fKB for total %d archetypes, actual count was %d",
				(float)numPages1*64.0f,
				g_debugArchetypeProxyCount,
				g_debugArchetypeProxyCurrentIndex);
		}
	}
#endif // __ASSERT

	g_debugArchetypeProxyCount = g_debugArchetypeProxyCurrentIndex;
	g_debugArchetypeProxiesBeingCreated = false;
	g_debugArchetypeProxiesFinishedCreation = true;

	dlDrawListMgr::SuppressAddTypeFileReferencesEnd();
}

void CDebugArchetype::CheckDebugArchetypeProxy(const CBaseModelInfo* pModelInfo)
{
	if (g_debugArchetypeProxiesFinishedCreation)
	{
		const CDebugArchetypeProxy* pProxy = CDebugArchetype::GetDebugArchetypeProxyForModelInfo(pModelInfo);

		if (pProxy)
		{
			if (pProxy->Check(pModelInfo))
			{
				//Displayf("### debug archetype proxy ok for %s", pModelInfo->GetModelName());
			}
		}
		else
		{
			Assertf(0, "### debug archetype proxy returned NULL for %s", pModelInfo->GetModelName() ? pModelInfo->GetModelName() : "NULL");
		}
	}
}

u32 CDebugArchetype::GetMaxDebugArchetypeProxies()
{
	CDebugArchetype::CreateDebugArchetypeProxies();
	return g_debugArchetypeProxyCount;
}

const CDebugArchetypeProxy* CDebugArchetype::GetDebugArchetypeProxy(u32 i)
{
	CDebugArchetype::CreateDebugArchetypeProxies();
	Assert(i < g_debugArchetypeProxyCount);
	return reinterpret_cast<const CDebugArchetypeProxy*>(&g_debugArchetypeProxyData[g_debugArchetypeProxySize*i]);
}

const CDebugArchetypeProxy* CDebugArchetype::GetDebugArchetypeProxyForHashKey(u32 hashKey, int* out_proxyIndex)
{
	CDebugArchetype::CreateDebugArchetypeProxies();
	const int proxyIndex = CDebugArchetypeProxy::FindProxyIndex(hashKey);

	if (out_proxyIndex)
	{
		*out_proxyIndex = proxyIndex;
	}

	if (proxyIndex != -1)
	{
		return GetDebugArchetypeProxy(proxyIndex);
	}

	return NULL;
}

const CDebugArchetypeProxy* CDebugArchetype::GetDebugArchetypeProxyForModelInfo(const CBaseModelInfo* pModelInfo, int* out_proxyIndex)
{
	if (pModelInfo)
	{
		return GetDebugArchetypeProxyForHashKey(pModelInfo->GetHashKey(), out_proxyIndex);
	}
	else if (out_proxyIndex)
	{
		*out_proxyIndex = -1;
	}

	return NULL;
}

CBaseModelInfo* CDebugArchetype::GetBaseModelInfoForProxy(const CDebugArchetypeProxy* pProxy)
{
	if (pProxy)
	{
		return CModelInfo::GetBaseModelInfoFromHashKey(pProxy->GetHashKey(), NULL);
	}

	return NULL;
}

#elif __BANK

u32 CDebugArchetype::GetMaxDebugArchetypeProxies()
{
	return CModelInfo::GetMaxModelInfos();
}

const CDebugArchetypeProxy* CDebugArchetype::GetDebugArchetypeProxy(u32 i)
{
	return static_cast<const CDebugArchetypeProxy*>(CModelInfo::GetBaseModelInfo(fwModelId(i)));
}

const CDebugArchetypeProxy* CDebugArchetype::GetDebugArchetypeProxyForModelInfo(const CBaseModelInfo* pModelInfo)
{
	return static_cast<const CDebugArchetypeProxy*>(pModelInfo);
}

#endif // __BANK

// ================================================================================================

#if USE_DEBUG_ARCHETYPE_PROXIES

void CDebugArchetypeProxy::Create(const CBaseModelInfo* pModelInfo, u32 proxyIndex, int mapTypeSlot, bool bStaticArchetype)
{
	Assert(pModelInfo);

#if __ASSERT
	if (g_debugArchetypeProxyMap.Access(pModelInfo->GetHashKey()))
	{
		const int prevProxyIndex = g_debugArchetypeProxyMap[pModelInfo->GetHashKey()];
		const CDebugArchetypeProxy* prevProxy = reinterpret_cast<const CDebugArchetypeProxy*>(&g_debugArchetypeProxyData[g_debugArchetypeProxySize*prevProxyIndex]);

		const char* currRpfPath = "static";
		const char* prevRpfPath = "static";

		if (!bStaticArchetype)
		{
			currRpfPath = AssetAnalysis::GetRPFPath(g_MapTypesStore, mapTypeSlot);
		}

		if (!prevProxy->GetIsStaticArchetype())
		{
			prevRpfPath = AssetAnalysis::GetRPFPath(g_MapTypesStore, prevProxy->GetMapTypeSlot());
		}

		Assertf(0, "CDebugArchetypeProxy::Create called on \"%s/%s\" (0x%08x) more than once, previous path was \"%s/%s\"", currRpfPath, pModelInfo->GetModelName(), pModelInfo->GetHashKey(), prevRpfPath, prevProxy->GetModelName());
	}
#endif // __ASSERT

	// make sure map allocations use debug memory
	{
		USE_DEBUG_MEMORY();
		g_debugArchetypeProxyMap[pModelInfo->GetHashKey()] = (int)proxyIndex;
	}

	m_modelName.SetFromString(pModelInfo->GetModelName());

	m_type          = pModelInfo->GetModelType();
	m_hashKey       = pModelInfo->GetHashKey();
	m_mapTypeSlot   = (s16)mapTypeSlot;
	m_proxyFlags    = 0;
	m_HDTexDistance = pModelInfo->GetHDTextureDistance();
	m_lodDistance   = pModelInfo->GetLodDistanceUnscaled();
	m_drawableType  = (u8)pModelInfo->GetDrawableType();
	m_drawableIndex = 0;
	m_ptfxAssetSlot = (s16)pModelInfo->GetPtFxAssetSlot();

	// set prop flag
	{
		strStreamingModule* module = NULL;
		strLocalIndex index = strLocalIndex(0xffff);

		switch (pModelInfo->GetDrawableType())
		{
		case fwArchetype::DT_FRAGMENT:
			module = &g_FragmentStore;
			index = pModelInfo->GetFragmentIndex();
			break;
		case fwArchetype::DT_DRAWABLE:
			module = &g_DrawableStore;
			index = pModelInfo->GetDrawableIndex();
			break;
		case fwArchetype::DT_DRAWABLEDICTIONARY:
			module = &g_DwdStore;
			index = pModelInfo->GetDrawDictIndex();
			break;
		default:
			break;
		}

		if (module && index != 0xffff)
		{
			strStreamingInfo* info = module->GetStreamingInfo(index);

			if (info)
			{
				strLocalIndex imageIndex = strPackfileManager::GetImageFileIndexFromHandle(info->GetHandle());

				if (imageIndex != -1)
				{
					strStreamingFile* file = strPackfileManager::GetImageFile(imageIndex.Get());

					if (file && file->m_contentsType == CDataFileMgr::CONTENTS_PROPS)
					{
						m_proxyFlags |= DEBUG_ARCHETYPE_PROXY_IS_PROP;
					}
				}
			}
		}
	}

	if (bStaticArchetype)
	{
		m_proxyFlags |= DEBUG_ARCHETYPE_PROXY_STATIC;
	}

//	if (pModelInfo->GetArchetypeFlags() & MODEL_IS_PROP          ) { m_proxyFlags |= DEBUG_ARCHETYPE_PROXY_IS_PROP          ; }
	if (pModelInfo->GetArchetypeFlags() & MODEL_IS_TREE          ) { m_proxyFlags |= DEBUG_ARCHETYPE_PROXY_IS_TREE          ; }
	if (pModelInfo->GetArchetypeFlags() & MODEL_DONT_CAST_SHADOWS) { m_proxyFlags |= DEBUG_ARCHETYPE_PROXY_DONT_CAST_SHADOWS; }
	if (pModelInfo->GetArchetypeFlags() & MODEL_SHADOW_PROXY     ) { m_proxyFlags |= DEBUG_ARCHETYPE_PROXY_SHADOW_PROXY     ; }

	if (pModelInfo->GetClipDictionaryIndex() != -1 && pModelInfo->GetHasUvAnimation())
	{
		m_proxyFlags |= DEBUG_ARCHETYPE_PROXY_HAS_UV_ANIMATION;
	}

	switch (pModelInfo->GetDrawableType())
	{
	case fwArchetype::DT_DRAWABLE          : m_drawableIndex = pModelInfo->GetDrawableIndex(); break;
	case fwArchetype::DT_DRAWABLEDICTIONARY: m_drawableIndex = pModelInfo->GetDrawDictIndex(); break;
	case fwArchetype::DT_FRAGMENT          : m_drawableIndex = pModelInfo->GetFragmentIndex(); break;
	default: break;
	}

	if (pModelInfo->GetModelType() == MI_TYPE_PED)
	{
		static_cast<CDebugPedArchetypeProxy*>(this)->Create(static_cast<const CPedModelInfo*>(pModelInfo));
	}
	else if (pModelInfo->GetModelType() == MI_TYPE_VEHICLE)
	{
#if !__SPU // for some reason CVehicleModelInfo is specifically !__SPU
		static_cast<CDebugVehicleArchetypeProxy*>(this)->Create(static_cast<const CVehicleModelInfo*>(pModelInfo));
#endif // !__SPU
	}
	else if (pModelInfo->GetModelType() == MI_TYPE_WEAPON)
	{
		static_cast<CDebugWeaponArchetypeProxy*>(this)->Create(static_cast<const CWeaponModelInfo*>(pModelInfo));
	}
}

void CDebugPedArchetypeProxy::Create(const CPedModelInfo* pPedModelInfo)
{
	Assert(pPedModelInfo);
	Assert(pPedModelInfo->GetHDTxdIndex()            < 32768);
	Assert(pPedModelInfo->GetPedComponentFileIndex() < 32768);
	Assert(pPedModelInfo->GetPropsFileIndex()        < 32768);

	m_HDtxdIdx           = (s16)pPedModelInfo->GetHDTxdIndex();
	m_pedCompFileIndex   = pPedModelInfo->GetPedComponentFileIndex();
	m_propsFileIndex     = pPedModelInfo->GetPropsFileIndex();
	m_isStreamedGraphics = (s16)pPedModelInfo->GetIsStreamedGfx();
	m_propStreamFolder   = pPedModelInfo->GetPropStreamFolder();
}

void CDebugVehicleArchetypeProxy::Create(const CVehicleModelInfo* pVehicleModelInfo)
{
	Assert(pVehicleModelInfo);
	Assert(pVehicleModelInfo->GetHDFragmentIndex() < 32768);
	Assert(pVehicleModelInfo->GetHDTxdIndex()      < 32768);

	m_HDfragIdx = pVehicleModelInfo->GetHDFragmentIndex();
	m_HDtxdIdx  = pVehicleModelInfo->GetHDTxdIndex();
}

void CDebugWeaponArchetypeProxy::Create(const CWeaponModelInfo* pWeaponModelInfo)
{
	Assert(pWeaponModelInfo);
	Assert(pWeaponModelInfo->GetHDDrawableIndex() < 32768);
	Assert(pWeaponModelInfo->GetHDTxdIndex()      < 32768);

	m_HDdrawIdx = pWeaponModelInfo->GetHDDrawableIndex();
	m_HDtxdIdx  = pWeaponModelInfo->GetHDTxdIndex();
}

template <typename T> static bool CDebugArchetypeProxy_Check(const char* ASSERT_ONLY(modelName), const char* ASSERT_ONLY(modelTypeName), const char* ASSERT_ONLY(paramName), T modelValue, T proxyValue)
{
	if (modelValue != proxyValue)
	{
		Assertf(0, "check failed for %s: %s = %d for %s but %d for proxy", modelName, paramName, (int)modelValue, modelTypeName, (int)proxyValue);
		return false;
	}

	return true;
}

template <> bool CDebugArchetypeProxy_Check<bool>(const char* ASSERT_ONLY(modelName), const char* ASSERT_ONLY(modelTypeName), const char* ASSERT_ONLY(paramName), bool modelValue, bool proxyValue)
{
	if (modelValue != proxyValue)
	{
		Assertf(0, "check failed for %s: %s = %s for %s but %s for proxy", modelName, paramName, modelValue ? "TRUE" : "FALSE", modelTypeName, proxyValue ? "TRUE" : "FALSE");
		return false;
	}

	return true;
}

template <> bool CDebugArchetypeProxy_Check<const char*>(const char* ASSERT_ONLY(modelName), const char* ASSERT_ONLY(modelTypeName), const char* ASSERT_ONLY(paramName), const char* modelValue, const char* proxyValue)
{
	if (strcmp(modelValue, proxyValue) != 0)
	{
		Assertf(0, "check failed for %s: %s = %s for %s but %s for proxy", modelName, paramName, modelValue, modelTypeName, proxyValue);
		return false;
	}

	return true;
}

bool CDebugArchetypeProxy::Check(const CBaseModelInfo* pModelInfo) const
{
	if (AssertVerify(pModelInfo && pModelInfo->GetModelName()))
	{
		if (!Verifyf(GetModelName(), "check failed for %s: proxy name is NULL", pModelInfo->GetModelName()))
		{
			return false;
		}

#define CDebugArchetypeProxy_CHECK(param) if (!CDebugArchetypeProxy_Check(pModelInfo->GetModelName(), "CBaseModelInfo", #param, pModelInfo->param(), param())) { return false; }
		CDebugArchetypeProxy_CHECK(GetModelName);
		CDebugArchetypeProxy_CHECK(GetModelType);
		CDebugArchetypeProxy_CHECK(GetHashKey);
	//	CDebugArchetypeProxy_CHECK(GetIsProp); -- damn, this is STILL broken, wtf.
		CDebugArchetypeProxy_CHECK(GetIsTree);
		CDebugArchetypeProxy_CHECK(GetDontCastShadows);
		CDebugArchetypeProxy_CHECK(GetIsShadowProxy);
	//	CDebugArchetypeProxy_CHECK(GetHasAnimUV);
		CDebugArchetypeProxy_CHECK(GetHDTextureDistance);
		CDebugArchetypeProxy_CHECK(GetLodDistanceUnscaled);
		CDebugArchetypeProxy_CHECK(GetDrawableType);

		CDebugArchetypeProxy_CHECK(SafeGetDrawableIndex);
		CDebugArchetypeProxy_CHECK(SafeGetFragmentIndex);
		CDebugArchetypeProxy_CHECK(SafeGetDrawDictIndex);
	//	CDebugArchetypeProxy_CHECK(SafeGetDrawDictEntry);

		CDebugArchetypeProxy_CHECK(GetAssetParentTxdIndex);
		CDebugArchetypeProxy_CHECK(GetPtFxAssetSlot);
#undef  CDebugArchetypeProxy_CHECK

		if (pModelInfo->GetHasLoaded() && pModelInfo->GetDrawableType() == fwArchetype::DT_DRAWABLEDICTIONARY)
		{
			if (!CDebugArchetypeProxy_Check(pModelInfo->GetModelName(), "CBaseModelInfo", "SafeGetDrawDictEntry", pModelInfo->SafeGetDrawDictEntry(), SafeGetDrawDictEntry_IfLoaded()))
			{
				return false;
			}
		}

		if (GetModelType() == MI_TYPE_PED)
		{
			if (!static_cast<const CDebugPedArchetypeProxy*>(this)->Check(static_cast<const CPedModelInfo*>(pModelInfo)))
			{
				return false;
			}
		}
		else if (GetModelType() == MI_TYPE_VEHICLE)
		{
			if (!static_cast<const CDebugVehicleArchetypeProxy*>(this)->Check(static_cast<const CVehicleModelInfo*>(pModelInfo)))
			{
				return false;
			}
		}

		return true;
	}

	return false;
}

bool CDebugPedArchetypeProxy::Check(const CPedModelInfo* pPedModelInfo) const
{
	if (GetModelType() == MI_TYPE_PED)
	{
#define CDebugArchetypeProxy_CHECK(param) if (!CDebugArchetypeProxy_Check(pPedModelInfo->GetModelName(), "CPedModelInfo", #param, pPedModelInfo->param(), param())) { return false; }
		CDebugArchetypeProxy_CHECK(GetHDTxdIndex);
		CDebugArchetypeProxy_CHECK(GetPedComponentFileIndex);
		CDebugArchetypeProxy_CHECK(GetPropsFileIndex);
		CDebugArchetypeProxy_CHECK(GetIsStreamedGfx);
#undef  CDebugArchetypeProxy_CHECK
	}
	else
	{
		Assertf(0, "check failed for %s: model type is %d, not MI_TYPE_PED", pPedModelInfo->GetModelName(), GetModelType());
		return false;
	}

	return true;
}

bool CDebugVehicleArchetypeProxy::Check(const CVehicleModelInfo* pVehicleModelInfo) const
{
	if (GetModelType() == MI_TYPE_VEHICLE)
	{
#define CDebugArchetypeProxy_CHECK(param) if (!CDebugArchetypeProxy_Check(pVehicleModelInfo->GetModelName(), "CVehicleModelInfo", #param, pVehicleModelInfo->param(), param())) { return false; }
		CDebugArchetypeProxy_CHECK(GetHDFragmentIndex);
		CDebugArchetypeProxy_CHECK(GetHDTxdIndex);
#undef  CDebugArchetypeProxy_CHECK
	}
	else
	{
		Assertf(0, "check failed for %s: model type is %d, not MI_TYPE_VEHICLE", pVehicleModelInfo->GetModelName(), GetModelType());
		return false;
	}

	return true;
}

bool CDebugWeaponArchetypeProxy::Check(const CWeaponModelInfo* pWeaponModelInfo) const
{
	if (GetModelType() == MI_TYPE_WEAPON)
	{
#define CDebugArchetypeProxy_CHECK(param) if (!CDebugArchetypeProxy_Check(pWeaponModelInfo->GetModelName(), "CVehicleModelInfo", #param, pWeaponModelInfo->param(), param())) { return false; }
		CDebugArchetypeProxy_CHECK(GetHDDrawableIndex);
		CDebugArchetypeProxy_CHECK(GetHDTxdIndex);
#undef  CDebugArchetypeProxy_CHECK
	}
	else
	{
		Assertf(0, "check failed for %s: model type is %d, not MI_TYPE_WEAPON", pWeaponModelInfo->GetModelName(), GetModelType());
		return false;
	}

	return true;
}

int CDebugArchetypeProxy::FindProxyIndex(u32 hashKey)
{
	const int* pIndex = g_debugArchetypeProxyMap.Access(hashKey);

	if (pIndex)
	{
		Assert(*pIndex >= 0);
		return *pIndex;
	}

	return -1;
}

bool CDebugArchetypeProxy::GetHasLoaded() const
{
	const CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey(m_hashKey, NULL);
	return pModelInfo ? pModelInfo->GetHasLoaded() : false;
}

s32 CDebugArchetypeProxy::SafeGetDrawableIndex() const { return (m_drawableType == fwArchetype::DT_DRAWABLE           && m_drawableIndex != INVALID_DRAWABLE_IDX) ? m_drawableIndex : -1; }
s32 CDebugArchetypeProxy::SafeGetDrawDictIndex() const { return (m_drawableType == fwArchetype::DT_DRAWABLEDICTIONARY && m_drawableIndex != INVALID_DRAWDICT_IDX) ? m_drawableIndex : -1; }
s32 CDebugArchetypeProxy::SafeGetFragmentIndex() const { return (m_drawableType == fwArchetype::DT_FRAGMENT           && m_drawableIndex != INVALID_FRAG_IDX    ) ? m_drawableIndex : -1; }

s32 CDebugArchetypeProxy::SafeGetDrawDictEntry_IfLoaded() const
{
	const CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey(m_hashKey, NULL);
	return (pModelInfo && pModelInfo->GetHasLoaded()) ? pModelInfo->SafeGetDrawDictEntry() : -1;
}

s32 CDebugArchetypeProxy::GetAssetParentTxdIndex() const
{
	switch (m_drawableType)
	{
	case fwArchetype::DT_ASSETLESS         : break;
	case fwArchetype::DT_DRAWABLE          : return g_DrawableStore.GetParentTxdForSlot(strLocalIndex(SafeGetDrawableIndex())).Get();
	case fwArchetype::DT_DRAWABLEDICTIONARY: return g_DwdStore     .GetParentTxdForSlot(strLocalIndex(SafeGetDrawDictIndex())).Get();
	case fwArchetype::DT_FRAGMENT          : return g_FragmentStore.GetParentTxdForSlot(strLocalIndex(SafeGetFragmentIndex())).Get();
	}

	return -1;
}

rmcDrawable* CDebugArchetypeProxy::GetDrawable() const
{
	const CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey(m_hashKey, NULL);
	return pModelInfo ? pModelInfo->GetDrawable() : NULL;
}

#elif __BANK

// ..

#endif // __BANK
