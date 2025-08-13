#include "IslandHopper.h"

#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "fwscene/stores/mapdatastore.h"
#include "renderer/HorizonObjects.h"
#include "scene/entities/compEntity.h"
#include "scene/ipl/IplCullBox.h"
#include "scene/LoadScene.h"
#include "scene/world/GameWorldHeightMap.h"
#include "system/nelem.h"
#include "vfx/misc/DistantLightsCommon.h"
#include "vfx/misc/DistantLights2.h"
#include "vfx/misc/FogVolume.h"
#include "vfx/misc/LODLightManager.h"

#include "islandhopperdata_parser.h"

CIslandHopper CIslandHopper::sm_islandHopper;
bool CIslandHopper::ms_tunabledSkipLoadSceneWait = false;

static atHashString const HeistIslandName = ATSTRINGHASH("HeistIsland", 0x114611d6);

#if !__FINAL
PARAM(disableIslandHopperWaitForLoadScene, "");
#endif

class CIslandHopperFileMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile& file)
	{
		CIslandHopper::GetInstance().LoadFile(file.m_filename);

		return true;
	}

	virtual void UnloadDataFile(const CDataFileMgr::DataFile& file) 
	{
		CIslandHopper::GetInstance().UnloadFile(file.m_filename);
	}

} g_IslandHopperFileMounter;


//////////////////////////////////////////////////////////////////////////
void CIslandHopper::Init()
{
	CDataFileMount::RegisterMountInterface(CDataFileMgr::ISLAND_HOPPER_FILE, &g_IslandHopperFileMounter);

	m_allIplsActiveDebug = false;
	m_requestEnableIsland = false;
	m_waitForLoadscene = true;
}


//////////////////////////////////////////////////////////////////////////
void CIslandHopper::InitTunables()
{
	ms_tunabledSkipLoadSceneWait = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("HEIST4_DISABLE_ISLANDHOPPER_LOADSCENE_WAIT", 0xd5fe95b9), ms_tunabledSkipLoadSceneWait);
}


//////////////////////////////////////////////////////////////////////////
void CIslandHopper::LoadFile(char const* filename)
{
	CIslandHopperData* newFile = nullptr;
	if (Verifyf(PARSER.LoadObjectPtr(filename, "", newFile), "Failed to load %s", filename))
	{
		m_islands.PushAndGrow(newFile);
	}
}


//////////////////////////////////////////////////////////////////////////
void CIslandHopper::UnloadFile(char const* filename)
{
	atHashString filenameHash(filename);
	int toDelete = -1;
	for(int i = 0; i < m_islands.GetCount(); ++i)
	{
		if(m_islands[i]->Name == filenameHash)
		{
			toDelete = i;
			break;
		}
	}

	if (Verifyf(toDelete != -1, "Failed to unload %s", filename))
	{
		delete m_islands[toDelete];
		m_islands.DeleteFast(toDelete);
	}

}


//////////////////////////////////////////////////////////////////////////
void CIslandHopper::Update()
{
	if(m_requestedIsland != ATSTRINGHASH("", 0))
	{
		if(g_LoadScene.IsActive())
		{
			if(!m_waitForLoadscene)
			{
				Displayf("IslandHopper : Request is pending (%s, %s) but a LoadScene is currently active... however wait for loadscene was disabled!", m_requestedIsland.GetCStr(), m_requestEnableIsland ? "Enable" : "Disable");
			}
			else
			{
				Displayf("IslandHopper : Request is pending (%s, %s) but a LoadScene is currently active...", m_requestedIsland.GetCStr(), m_requestEnableIsland ? "Enable" : "Disable");
				return;
			}
		}

		ToggleIsland(m_requestedIsland, m_requestEnableIsland);
		m_requestedIsland = ATSTRINGHASH("", 0);
	}

	
}


//////////////////////////////////////////////////////////////////////////
void CIslandHopper::RequestToggleIsland(atHashString islandName, bool enable, bool waitForLoadscene)
{
	Displayf("IslandHopper : Requesting %s %s %s", islandName.GetCStr(), enable ? "Enable" : "Disable", waitForLoadscene ? "Wait For Loadscene" : "No wait for Loadscene");
	if(m_requestedIsland != ATSTRINGHASH("", 0))
	{
		Displayf("IslandHopper : Request made when one is already present (%s, %s)", m_requestedIsland.GetCStr(), m_requestEnableIsland ? "Enable" : "Disable");
		return;
	}

	if(m_currentIslandEnabled != ATSTRINGHASH("", 0))
	{
		if(m_currentIslandEnabled != islandName)
		{
			Displayf("IslandHopper : Request made when a different island is already enabled (%s)", m_currentIslandEnabled.GetCStr());
			return;
		}
		else
		{
			if(enable)
			{
				Displayf("IslandHopper : Request made when the same island is already enabled (%s)", m_currentIslandEnabled.GetCStr());
				return;
			}
		}
	}

	m_requestEnableIsland = enable;
	m_requestedIsland = islandName;
	m_waitForLoadscene = !ms_tunabledSkipLoadSceneWait && waitForLoadscene;
#if !__FINAL
	m_waitForLoadscene &= !PARAM_disableIslandHopperWaitForLoadScene.Get();
#endif
}


//////////////////////////////////////////////////////////////////////////
#if __BANK
void CIslandHopper::InitWidgets(bkBank& bank)
{
	bank.PushGroup("Island Hopper");
	
	bank.AddButton("Activate Heist Island", datCallback(MFA(CIslandHopper::ActivateHeistIsland), this));
	bank.AddButton("Deactivate Heist Island", datCallback(MFA(CIslandHopper::DeactivateHeistIsland), this));
	bank.AddButton("Toggle Heist Island", datCallback(MFA(CIslandHopper::DebugToggleHeistIsland), this));
	bank.AddButton("Enable IMAPs", datCallback(MFA(CIslandHopper::EnableHeistIslandImaps), this));
	bank.AddButton("Disable IMAPs", datCallback(MFA(CIslandHopper::DisableHeistIslandImaps), this));
	bank.AddButton("Print Grass", datCallback(MFA(CIslandHopper::PrintGrass), this));
	bank.AddToggle("Are Ipls Active", &m_allIplsActiveDebug);
	bank.AddButton("Check Ipls Active", datCallback(MFA(CIslandHopper::CheckIPLsActive), this));


	bank.PopGroup();
}

#endif // __BANK


//////////////////////////////////////////////////////////////////////////
void CIslandHopper::DebugToggleHeistIsland()
{
	static bool enabled = false;
	RequestToggleIsland(HeistIslandName, !enabled);
	enabled = !enabled;
}


//////////////////////////////////////////////////////////////////////////
void CIslandHopper::ToggleIsland(atHashString islandName, bool enable)
{
	if(enable)
	{
		if(m_currentIslandEnabled == islandName)
		{
			// Already enabled so bail
			return;
		}

		if(Verifyf(m_currentIslandEnabled.GetHash() == 0, "Island %s is already enabled!", m_currentIslandEnabled.GetCStr()))
		{
			if(islandName == HeistIslandName)
			{
				ActivateHeistIsland();
			}
		}
	}
	else
	{
		if(Verifyf(m_currentIslandEnabled == islandName, "Island %s is already enabled instead of %s you're trying to disable", m_currentIslandEnabled.GetCStr(), islandName.GetCStr()))
		{
			DeactivateHeistIsland();
		}
	}
}


//////////////////////////////////////////////////////////////////////////
bool CIslandHopper::AreCurrentIslandIPLsActive() const
{
	// If we have an island request pending then return not loaded
	if(m_requestedIsland != ATSTRINGHASH("", 0))
	{
		return false;
	}

	CIslandHopperData const* pIslandData = FindIslandData(m_currentIslandEnabled);
	if(pIslandData)
	{
		for(int i = 0; i < pIslandData->IPLsToEnable.GetCount(); ++i)
		{
			s32 index = INSTANCE_STORE.FindSlot(atFinalHashString(pIslandData->IPLsToEnable[i].GetHash())).Get();
			if(!INSTANCE_STORE.GetIsStreamable(strLocalIndex(index)))
			{
				return false;
			}
		}

		return true;
	}

	return false;
}


//////////////////////////////////////////////////////////////////////////
atHashString CIslandHopper::GetCurrentActiveIsland() const
{
	return m_currentIslandEnabled;
}


//////////////////////////////////////////////////////////////////////////
bool CIslandHopper::IsAnyIslandActive() const
{
	return GetCurrentActiveIsland() != ATSTRINGHASH("", 0);
}


//////////////////////////////////////////////////////////////////////////
CIslandHopperData const* CIslandHopper::FindIslandData(atHashString const& Name) const
{
	for(int i = 0; i < m_islands.GetCount(); ++i)
	{
		if(m_islands[i]->Name == Name)
			return m_islands[i];
	}

	return nullptr;
}


//////////////////////////////////////////////////////////////////////////
void CIslandHopper::ActivateIsland(CIslandHopperData const& data)
{
	Displayf("================================================");
	Displayf("IslandHopper: Activating %s", data.Name.GetCStr());
	Assertf(!g_LoadScene.IsActive(), "Don't modify Ipl groups whilst load scene is active! (Load scene triggered by %s)", g_LoadScene.WasStartedByScript() ? g_LoadScene.GetScriptName().c_str() : "CODE");

	for(int i = 0; i < data.IPLsToEnable.GetCount(); ++i)
	{
		RequestIpl(data.IPLsToEnable[i]);
	}

	EnableImaps(data.IMAPsToPreempt.GetElements(), data.IMAPsToPreempt.GetCount());
	SetMapDataCullBoxEnabled(data.Cullbox, true);
	CGameWorldHeightMap::EnableHeightmap(data.HeightMap, true);
	CHorizonObjects::CullEverything(true);
	g_fogVolumeMgr.SetDisableFogVolumeRender(true);

	CLODLightManager::CacheLoadedLODLightImapIDX();
	gRenderThreadInterface.Flush();
	CLODLightManager::UnloadLODLightImapIDXCache();

	strLocalIndex index = INSTANCE_STORE.FindSlot(data.LodLights);
	INSTANCE_STORE.RequestGroup(index, atStringHash(data.LodLights));

	Water::RequestGlobalWaterXmlFile(Water::WATERXML_ISLANDHEIST);

#if ENABLE_DISTANT_CARS
	g_distantLights.SetDistantCarsEnabled(false);
#endif

	Displayf("================================================");
}


//////////////////////////////////////////////////////////////////////////
void CIslandHopper::DeactivateIsland(CIslandHopperData const& data)
{
	Displayf("================================================");
	Displayf("IslandHopper: Deactivating %s", data.Name.GetCStr());
	Assertf(!g_LoadScene.IsActive(), "Don't modify Ipl groups whilst load scene is active! (Load scene triggered by %s)", g_LoadScene.WasStartedByScript() ? g_LoadScene.GetScriptName().c_str() : "CODE");

	gRenderThreadInterface.Flush();
	for(int i = 0; i < data.IPLsToEnable.GetCount(); ++i)
	{
		RemoveIpl(data.IPLsToEnable[i]);
	}

	DisableImaps(data.IMAPsToPreempt.GetElements(), data.IMAPsToPreempt.GetCount());
	SetMapDataCullBoxEnabled(data.Cullbox, false);
	CGameWorldHeightMap::EnableHeightmap(data.HeightMap, false);
	CHorizonObjects::CullEverything(false);
	g_fogVolumeMgr.SetDisableFogVolumeRender(false);

	strLocalIndex index = INSTANCE_STORE.FindSlot(data.LodLights);
	fwMapDataStore::GetStore().RemoveGroup(index, atStringHash(data.LodLights));

	CLODLightManager::LoadLODLightImapIDXCache();

	Water::RequestGlobalWaterXmlFile(Water::WATERXML_V_DEFAULT);

#if ENABLE_DISTANT_CARS
	g_distantLights.SetDistantCarsEnabled(true);
#endif

	Displayf("================================================");
}


//////////////////////////////////////////////////////////////////////////
void CIslandHopper::ActivateHeistIsland()
{
	if(CIslandHopperData const* pData = FindIslandData(HeistIslandName))
	{
		ActivateIsland(*pData);
		SetHeistIslandInteriors(true);
		m_currentIslandEnabled = HeistIslandName;
	}
}


//////////////////////////////////////////////////////////////////////////
void CIslandHopper::DeactivateHeistIsland()
{
	if(CIslandHopperData const* pData = FindIslandData(HeistIslandName))
	{
		DeactivateIsland(*pData);
		SetHeistIslandInteriors(false);
		m_currentIslandEnabled = ATSTRINGHASH("", 0);
	}
}


//////////////////////////////////////////////////////////////////////////
// Disable all Imaps that we want to forcibly disable when entering MP
// Currently just the heist island but could be added to in the future
void CIslandHopper::DisableAllPreemptedImaps()
{
	Displayf("IslandHopper: Disable all proscribed Imaps");
	for(int i = 0; i < m_islands.GetCount(); ++i)
	{
		DisableImaps(m_islands[i]->IMAPsToPreempt.GetElements(), m_islands[i]->IMAPsToPreempt.GetCount());
	}

	SetHeistIslandInteriors(false);
}


//////////////////////////////////////////////////////////////////////////
void CIslandHopper::EnableHeistIslandImaps()
{
	if(CIslandHopperData const* pData = FindIslandData(HeistIslandName))
	{
		EnableImaps(pData->IMAPsToPreempt.GetElements(), pData->IMAPsToPreempt.GetCount());
	}
}


//////////////////////////////////////////////////////////////////////////
void CIslandHopper::DisableHeistIslandImaps()
{
	if(CIslandHopperData const* pData = FindIslandData(HeistIslandName))
	{
		DisableImaps(pData->IMAPsToPreempt.GetElements(), pData->IMAPsToPreempt.GetCount());
	}
}


//////////////////////////////////////////////////////////////////////////
void CIslandHopper::CheckIPLsActive()
{
	m_allIplsActiveDebug = AreCurrentIslandIPLsActive();
}


//////////////////////////////////////////////////////////////////////////
void CIslandHopper::RequestIpl(atHashString iplName)
{
	Displayf(" IslandHopper: Requesting Ipl: %s", iplName.GetCStr());
	s32 index = INSTANCE_STORE.FindSlot(atFinalHashString(iplName.GetHash())).Get();

	if(Verifyf(index != -1, "IslandHopper: IPL group does not exist - %s", iplName.GetCStr()))
	{
		INSTANCE_STORE.RequestGroup(strLocalIndex(index), iplName.GetHash());
		CCompEntity::UpdateCompEntitiesUsingGroup(index, CE_STATE_INIT);
	}
}


//////////////////////////////////////////////////////////////////////////
void CIslandHopper::RemoveIpl(atHashString iplName)
{
	Displayf(" IslandHopper: Removing Ipl: %s", iplName.GetCStr());
	strLocalIndex index = strLocalIndex(INSTANCE_STORE.FindSlot(atFinalHashString(iplName.GetHash())));

	if(Verifyf(index != -1, "IslandHopper: IPL group does not exist - %s", iplName.GetCStr()))
	{
		CCompEntity::UpdateCompEntitiesUsingGroup(index.Get(), CE_STATE_ABANDON);
		INSTANCE_STORE.RemoveGroup(index, iplName.GetHash());
	}
}


//////////////////////////////////////////////////////////////////////////
void CIslandHopper::SetMapDataCullBoxEnabled(const char* boxName, bool enabled)
{
	Displayf(" IslandHopper: Setting IplCullBox %s to %s", boxName, enabled ? "Enabled" : "Disabled");

	u32 nameHash = atStringHash(boxName);

	// If the cullbox isn't already enabled and it's to be enabled then regenerate the cull list
	// as the map data may have changed
	if(enabled && !CIplCullBox::IsBoxEnabled(nameHash))
	{
		INSTANCE_STORE.CreateChildrenCache();
		CIplCullBox::GenerateCullListForBox(nameHash);
#if !__BANK
		INSTANCE_STORE.DestroyChildrenCache(); // used by cull box editor
#endif // !__BANK
	}

	CIplCullBox::SetBoxEnabled(nameHash, enabled);
}


//////////////////////////////////////////////////////////////////////////
void CIslandHopper::EnableImaps(atHashString const* imaps, int imapCount)
{
	Displayf(" IslandHopper: Enabling Imaps...");

	for(int i = 0; i < imapCount; ++i)
	{
		Displayf("  Enabling Imap %s", imaps[i].GetCStr());
		
		strLocalIndex index = strLocalIndex(g_MapDataStore.FindSlotFromHashKey(imaps[i]));
		if(index.IsValid())
		{
			fwMapDataStore::GetStore().SetStreamable(strLocalIndex(index), true);
		}
		else
		{
			Displayf("    - Invalid slot?");
		}
	}
}


//////////////////////////////////////////////////////////////////////////
void CIslandHopper::DisableImaps(atHashString const* imaps, int imapCount)
{
	Displayf(" IslandHopper: Disabling Imaps...");
	gRenderThreadInterface.Flush();

	for(int i = 0; i < imapCount; ++i)
	{
		Displayf("  Disabling Imap %s", imaps[i].GetCStr());

		strLocalIndex index = strLocalIndex(g_MapDataStore.FindSlotFromHashKey(imaps[i]));
		if(index.IsValid())
		{
			if(fwMapDataStore::GetStore().HasObjectLoaded(strLocalIndex(index)))
			{
				fwMapDataStore::GetStore().ClearRequiredFlag(index.Get(), STRFLAG_DONTDELETE);
				fwMapDataStore::GetStore().SafeRemove(strLocalIndex(index));
			}

			fwMapDataStore::GetStore().SetStreamable(strLocalIndex(index), false);
		}
		else
		{
			Displayf("    - Invalid slot?");
		}
	}
}


//////////////////////////////////////////////////////////////////////////
void CIslandHopper::SetHeistIslandInteriors(bool interiorsEnabled)
{
	const u32 heistIslandInteriors[] = {
		0xfb8ab286, /*"h4_airstrip_hanger"*/
	};

	for(int i = 0; i < NELEM(heistIslandInteriors); ++i)
	{
		for(int j = 0; j < CInteriorProxy::GetPool()->GetSize(); ++j)
		{
			CInteriorProxy* pProxy = CInteriorProxy::GetPool()->GetSlot(j);
			if(pProxy && pProxy->GetNameHash() == heistIslandInteriors[i])
			{
				if(!interiorsEnabled)
				{
					pProxy->CleanupInteriorImmediately();
				}
				pProxy->SetIsDisabledByCode(!interiorsEnabled);
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
// Utility function used to print out grass imaps...kept around for future similar use
void CIslandHopper::PrintGrass()
{
	s32 storeSize = g_MapDataStore.GetSize();
	for(int i = 0; i < storeSize; ++i)
	{
		fwMapDataDef *pDef = g_MapDataStore.GetSlot(strLocalIndex(i));
		if(pDef && (pDef->GetContentFlags() & fwMapData::CONTENTFLAG_INSTANCE_LIST) > 0)
		{
			strLocalIndex index = strLocalIndex(g_MapDataStore.FindSlotFromHashKey(pDef->m_name));

			strStreamingInfo* info = strStreamingEngine::GetInfo().GetStreamingInfo(g_MapDataStore.GetStreamingIndex(index));
			strStreamingFile* pFile = strPackfileManager::GetImageFileFromHandle(info->GetHandle());
			if(pFile)
				Displayf("GRASS %s %s", pDef->m_name.GetCStr(), pFile->m_name.GetCStr());
			else
				Displayf("GRASS %s Unknown", pDef->m_name.GetCStr());
		}
	}
}