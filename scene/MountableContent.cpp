//
// filename: MountableContent.cpp
// description: Contains all the required information for mounting a content package
//
#include "debug/Debug.h"
#include "MountableContent.h"
#include "system/device_xcontent.h"
#include "system/xtl.h"
#include "streaming/streamingengine.h"
#include "file/asset.h"
#include "file/device.h"
#include "file/device_relative.h"
#include "system/filemgr.h"
#include "scene/DataFileMgr.h"
#include "scene/dlc_channel.h"
#include "script/streamedscripts.h"
#include "text/TextFile.h"
#include "script/script.h"
#include "scene/loader/mapFileMgr.h"
#include "scene/ipl/IplCullBox.h"
#include "fwscene/stores/staticboundsstore.h"
#include "fwscene/stores/maptypesstore.h"
#include "scene/texLod.h"
#include "scene/WarpManager.h"
#include "scene/ExtraContent.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "streaming/CacheLoader.h"
#include "loader/ManagedImapGroup.h"

#if __BANK
#include "scene/ExtraMetadataMgr.h"
#endif

PARAM(disableDLCcacheLoader, "Disables DLC cache file load/save, main game cache load/save will remain active");

#define TEMP_DEVICE	"extra:/"
#define SETUP_FILE_NAME "setup2.xml"
#define CCS_DATA_FILE_ONLY ((u32)(1 << CDataFileMgr::ChangeSetAction::CHANGE_SET_ACTION_DATA_FILE))

bool CMountableContent::sm_mapChangeDirty = false;
atBinaryMap<atDelegate<bool(atHashString)>*,atHashString> CMountableContent::sm_genericConditionChecks;
SExtraTitleUpdateData			CMountableContent::sm_ExtraTitleUpdateData;
SExtraFolderMountData			CMountableContent::sm_ExtraFolderMountData;
atMap<u32, CMountableContent*>	CMountableContent::sm_changeSetLookup;

CMountableContent::CMountableContent() : m_pDevice(NULL)
{
	Reset();
}

CMountableContent::~CMountableContent()
{
	// CMountableContent lives in an array in ExtraContent so dtor will be called when it is resized...
}

void CMountableContent::Reset()
{
	Assertf(!m_pDevice, "CMountableContent::Reset - Device was not NULL!");

	m_status = CS_NONE;
	m_packFileECIndex = -1;
	m_setupData.Reset();
	m_filename.Clear();
	m_pDevice = NULL;
	m_pDeviceTU = NULL;
	m_pCrcDevice = NULL;
	m_pCrcDeviceTU = NULL;
	m_activeChangeSets.Reset();
	m_usesPackfile = false;
	m_datFileLoaded = false;
	m_deviceType = DT_INVALID;
	m_permanent = false;
	m_isShippedContent = false;

#if RSG_ORBIS
	memset(&m_ps4MountPoint, 0, sizeof(SceAppContentMountPoint));
	memset(&m_ps4EntitlementLab, 0, sizeof(SceNpUnifiedEntitlementLabel));
#endif

#if __PPU
	m_pSecondaryDevice = NULL;
#endif

#if __XENON
	m_xContentData = NULL;
#endif

#if __BANK
	m_bankShouldExecute = false;
#endif
}

bool CMountableContent::Setup(s32 userIndex)
{
	if( (m_status == CS_SETUP_READ) || 
		(m_status == CS_FULLY_MOUNTED) )
	{
		return true;
	}

	if (Verifyf(m_status == CS_NONE, "CMountableContent::Setup content is in wrong state : %s", GetContentStatusName(GetStatus())))
	{
		dlcDebugf2("CMountableContent::Setup - %s", GetFilename());

		eMountResult r = MountContent(userIndex, TEMP_DEVICE);

		if(r == MR_MOUNTED)
		{
			ASSET.PushFolder(TEMP_DEVICE);
			{
				const char* setupFileName = GetSetupFileName();

				// If the setup file exists, lets see how loading it goes, otherwise lets just assume this isn't a DLC package
				if (ASSET.Exists(setupFileName, NULL))
				{

					if(LoadSetupXML(SETUP_FILE_NAME))
						m_status = CS_SETUP_READ;
					else
						m_status = CS_INVALID_SETUP;
				}
				else
					m_status = CS_SETUP_MISSING;
			}
			ASSET.PopFolder();
		}
		else 
		{	
			Errorf("CMountableContent::Mount failed to mount content %s, mount error code = %d [%s]", GetFilename(), r, GetTimeStamp());

			if(r == MR_CONTENT_CORRUPTED)	
			{
				m_status = CS_CORRUPTED;
			}
		}

		UnmountContent(TEMP_DEVICE);
	}

	if (m_status == CS_SETUP_READ)
	{
		PatchSetup();

		// Compare required version against current
		if ( UpdateRequired(CDebug::GetVersionNumber(), m_setupData.m_requiredVersion.c_str()) )
		{
			m_status = CS_UPDATE_REQUIRED;
			Assertf(0, "DLC Pack %s requires version %s, but your current version is %s.", m_setupData.m_nameHash.GetCStr(), m_setupData.m_requiredVersion.c_str(), CDebug::GetVersionNumber());
		}

		dlcDebugf3("CMountableContent::Setup - Successfully setup %s [%s] ", GetFilename(), GetTimeStamp());

		// check for DLC that unlocks network
		if (m_setupData.m_nameHash == ATSTRINGHASH("dlc_online_pass", 0x876AA600))
		{
			CNetwork::SetMultiplayerLocked(false);
		}
	}

	return (m_status == CS_SETUP_READ);
}

void CMountableContent::PatchSetup()
{
	if(const SExtraTitleUpdateMount *tuMount = FindExtraTitleUpdateMount(GetDeviceName())) 
	{
		fiDeviceRelative tmpDevice;
		tmpDevice.Init(tuMount->m_path);
		if(tmpDevice.MountAs(TEMP_DEVICE))
		{
			ASSET.PushFolder(TEMP_DEVICE);
			{
				if (ASSET.Exists(GetSetupFileName(), NULL))
				{
					m_setupData.Reset();

					if(Verifyf(LoadSetupXML(SETUP_FILE_NAME), "Failed to load %s from %s", SETUP_FILE_NAME, tuMount->m_path.c_str()))
					{
						dlcDebugf1("Loaded %s from %s", SETUP_FILE_NAME, tuMount->m_path.c_str());
					}
				}
			}
			ASSET.PopFolder();

			tmpDevice.Unmount(TEMP_DEVICE);
		}
	}
}

bool CMountableContent::Mount(s32 userIndex)
{
	if(m_status == CS_FULLY_MOUNTED)
		return true;

	if (Verifyf(m_status == CS_SETUP_READ, "CMountableContent::Mount content is in wrong state : %s", GetContentStatusName(GetStatus())))
	{
		eMountResult r = MountContent(userIndex, GetDeviceName());

		if(r == MR_MOUNTED)
		{
			m_status = CS_FULLY_MOUNTED;

			dlcDebugf3("CMountableContent::Mount - Successfully mounted %s [%s] ", GetFilename(), GetTimeStamp());
		}
		else
			Errorf("CMountableContent::Mount failed to mount content %s, mount error code = %d, timeStamp=%s", GetFilename(), r, GetTimeStamp());
	}

	return (m_status == CS_FULLY_MOUNTED);
}

void CMountableContent::ShutdownContent()
{
	if (m_status == CS_NONE)
		return;

	// check for DLC that unlocks network
	if (m_setupData.m_nameHash == ATSTRINGHASH("dlc_online_pass", 0x876AA600))
		CNetwork::SetMultiplayerLocked(true);

	EndRevertActiveChangeSets();
	UnloadDatFile();
	UnregisterContentChangeSets();

	// Don't bother unmounting packs that are setup only or if the setup file was invalid
	if (!IsSetupOnly() && m_status != CS_INVALID_SETUP && m_status != CS_SETUP_MISSING)
		UnmountContent(GetDeviceName());

	UnmountPlatform();
	Reset();
}

void CMountableContent::UnmountPlatform()
{
#if RSG_ORBIS
	if (strlen(m_ps4MountPoint.data) > 0)
	{
		s32 errValue = sceAppContentAddcontUnmount(&m_ps4MountPoint);

		if (errValue == SCE_OK)
			dlcDebugf3("UnmountPlatform - Successfully unmounted %s", GetName());
		else
			Errorf("UnmountPlatform - Failed to unmount package %s! %i", GetName(), errValue);
	}
#endif
}

void CMountableContent::EnsureDatFileLoaded()
{
	if (!m_datFileLoaded && m_status == CS_FULLY_MOUNTED)
	{
		m_datFileLoaded = true;
		
		char contentDevice[RAGE_MAX_PATH] = {0};
		strncpy(contentDevice,GetDeviceName(), static_cast<int>(strcspn(GetDeviceName(),":")));
		if(m_setupData.m_minorOrder <= 0)
		{
			strcat(contentDevice,"CRC:/");
		}
		else
		{
			strcat(contentDevice,":/");
		}

		// set extra content id so we know where img files are coming from
		strPackfileManager::SetExtraContentIndex(m_packFileECIndex);
		{
			dlcDebugf1("CMountableContent::Mount - Adding new data files from DLC %s (via %s)", GetName(), GetDatFileName());

			if (GetIsDatFileValid())
			{
				DATAFILEMGR.ResetChangeSetCollisions();
				{
					strcat(contentDevice, GetDatFileName());
					DATAFILEMGR.Load(contentDevice);

					const atArray<atHashString>& collisions = DATAFILEMGR.GetChangeSetCollisions();

					// If any of the CCS we introduced collided with another existing one forget about it...
					for (u32 i = 0; i < collisions.GetCount(); i++)
					{
						for (u32 j = 0; j < m_setupData.m_contentChangeSets.GetCount(); j++)
						{
							if (collisions[i] == m_setupData.m_contentChangeSets[j].GetHash())
							{
								dlcDebugf1("CMountableContent::Mount - Pack %s deleting CCS %s due to collision", m_setupData.m_nameHash.TryGetCStr(), m_setupData.m_contentChangeSets[j].TryGetCStr());

								m_setupData.m_contentChangeSets.Delete(j);
								j--;
							}
						}

						for (u32 groupIndex = 0; groupIndex < m_setupData.m_contentChangeSetGroups.GetCount(); groupIndex++)
						{
							ContentChangeSetGroup &group = m_setupData.m_contentChangeSetGroups[groupIndex];

							for (u32 j = 0; j < group.m_ContentChangeSets.GetCount(); j++)
							{
								if (collisions[i] == group.m_ContentChangeSets[j].GetHash())
								{
									dlcDebugf1("CMountableContent::Mount - Pack %s deleting CCS %s due to collision", m_setupData.m_nameHash.TryGetCStr(), group.m_ContentChangeSets[j].TryGetCStr());

									group.m_ContentChangeSets.Delete(j);
									j--;
								}
							}
						}
					}
				}
				DATAFILEMGR.ResetChangeSetCollisions();

				RegisterContentChangeSets();
			}
		}
		strPackfileManager::SetExtraContentIndex(0);
	}
}

void CMountableContent::UnloadDatFile()
{
	if (m_datFileLoaded && m_status == CS_FULLY_MOUNTED)
	{
		ASSET.PushFolder(GetDeviceName());
		{
			strPackfileManager::SetExtraContentIndex(m_packFileECIndex);
			{
				if (GetIsDatFileValid())
					DATAFILEMGR.Unload(GetDatFileName());
			}
			strPackfileManager::SetExtraContentIndex(0);
		}
		ASSET.PopFolder();
	}
}

void CMountableContent::RegisterContentChangeSets()
{
	for (u32 j = 0; j < m_setupData.m_contentChangeSets.GetCount(); j++)
	{
		sm_changeSetLookup[m_setupData.m_contentChangeSets[j].GetHash()] = this;
	}

	for (u32 groupIndex = 0; groupIndex < m_setupData.m_contentChangeSetGroups.GetCount(); groupIndex++)
	{
		ContentChangeSetGroup &group = m_setupData.m_contentChangeSetGroups[groupIndex];

		for (u32 j = 0; j < group.m_ContentChangeSets.GetCount(); j++)
		{
			sm_changeSetLookup[group.m_ContentChangeSets[j].GetHash()] = this;
		}
	}
}

void CMountableContent::UnregisterContentChangeSets()
{
	for (u32 j = 0; j < m_setupData.m_contentChangeSets.GetCount(); j++)
	{
		sm_changeSetLookup.Delete(m_setupData.m_contentChangeSets[j].GetHash());
	}

	for (u32 groupIndex = 0; groupIndex < m_setupData.m_contentChangeSetGroups.GetCount(); groupIndex++)
	{
		ContentChangeSetGroup &group = m_setupData.m_contentChangeSetGroups[groupIndex];

		for (u32 j = 0; j < group.m_ContentChangeSets.GetCount(); j++)
		{
			sm_changeSetLookup.Delete(group.m_ContentChangeSets[j].GetHash());
		}
	}
}

void CMountableContent::RegisterExecutionCheck(atDelegate<bool(atHashString)>* callback, atHashString controlString)
{
	sm_genericConditionChecks.SafeInsert(controlString, callback);
	sm_genericConditionChecks.FinishInsertion();
}

u32 CMountableContent::GetChangeSetGroup(u32 changeSet)
{
	for (u32 groupIndex = 0; groupIndex < m_setupData.m_contentChangeSetGroups.GetCount(); groupIndex++)
	{
		ContentChangeSetGroup &group = m_setupData.m_contentChangeSetGroups[groupIndex];

		for (u32 j = 0; j < group.m_ContentChangeSets.GetCount(); j++)
		{
			if(group.m_ContentChangeSets[j].GetHash() == changeSet)
				return group.m_NameHash;
		}
	}

	return 0;
}

void CMountableContent::GetChangeSetHashesForGroup(u32 changeSetGroup, atArray<atHashString>& setNames)
{
	const ContentChangeSetGroup* groupCCSs = FindContentChangeSetGroup(changeSetGroup);

	if (groupCCSs)
	{
		for (u32 i = 0; i < groupCCSs->m_ContentChangeSets.GetCount(); i++)
			setNames.PushAndGrow(groupCCSs->m_ContentChangeSets[i]);
	}
}

void CMountableContent::StartRevertActiveChangeSets()
{
	// Only revert meta files that have been safely implemented to be unloaded
	CDataFileMount::SetOverrideUnload(false);
	{
		for (s32 i = (m_activeChangeSets.GetCount() - 1); i >= 0; i--)
		{
			RevertContentChangeSet(m_activeChangeSets[i].m_group, m_activeChangeSets[i].m_changeSetName, CCS_DATA_FILE_ONLY);
		}
	}
	CDataFileMount::SetOverrideUnload(true);
}

// Revert everything except data/meta files here...
void CMountableContent::EndRevertActiveChangeSets()
{
	for (s32 i = (m_activeChangeSets.GetCount() - 1); i >= 0; i--)
	{
		RevertContentChangeSet(m_activeChangeSets[i].m_group, m_activeChangeSets[i].m_changeSetName, ~CCS_DATA_FILE_ONLY);
	}

	m_activeChangeSets.ResetCount();
}

bool CMountableContent::CheckCondition(atHashString registeredCallHash, atHashString control,bool negate)
{
	if(atDelegate<bool(atHashString)>** ppDelegate = sm_genericConditionChecks.Access(registeredCallHash))
	{
		if(atDelegate<bool(atHashString)>* pDelegate = *ppDelegate) 
		{
			if(pDelegate->IsBound())
				return pDelegate->Invoke(control)!=negate;
		}
	}
	else
	{
		Assertf(0,"Variable supplied in the generic condition is not registered : %s",registeredCallHash.TryGetCStr());
	}

	return true!=negate;
}

bool CMountableContent::ParseExpression(const char* expr)
{
	if(!expr)
	{
		return true;
	}
	atHashString registeredCheck;
	atHashString condition;
	char control[RAGE_MAX_PATH]={0},var[RAGE_MAX_PATH]={0};
	const char* current = expr;
	memset(control,0,RAGE_MAX_PATH);
	memset(var,0,RAGE_MAX_PATH);
	size_t len = strlen(expr);
	size_t next= 0;
	bool result = false;
	bool intermediate = false;

	while(len!=strcspn(current,"$&=!"))
	{
		size_t oper = strcspn(current,"$&=!|");
		switch(current[oper])
		{
		case '$':
			current = &current[oper+1];
			memset(control,0,RAGE_MAX_PATH);
			next = strcspn(current,"$&=!|");
			strncpy(control,current,next);
			registeredCheck.SetFromString(control);
			Displayf("Check if var %s meets",control);
			break;
		case '=':
			current = &current[oper+1];
			memset(var,0,RAGE_MAX_PATH);
			next = strcspn(current,"$&=!|");
			strncpy(var,current,next);
			condition.SetFromString(var);
			Displayf("%s",var);
			result = CheckCondition(registeredCheck,condition,false);
			Displayf("%s",result?"true":"false");
			break;
		case '&':
			current = &current[oper+1];
			Displayf("and");
			intermediate = ParseExpression(current);
			return result && intermediate ;
			break;
		case '!':
			current = &current[oper+1];
			memset(var,0,RAGE_MAX_PATH);
			next = strcspn(current,"$&=!|");
			strncpy(var,current,next);
			condition.SetFromString(var);
			Displayf("not %s",var);
			result = CheckCondition(registeredCheck,condition,true);
			Displayf("%s",result?"true":"false");
			break;
		case '|':
			Displayf("or");
			current = &current[oper+1];
			return result || ParseExpression(current);
			break;
		}
		len=strlen(current);
	}
	return result;

}

bool CMountableContent::CheckContentChangeSetConditions(const ExecutionConditions* conditions)
{
	const atArray<ExecutionCondition>& changesets =  conditions->m_activeChangesetConditions;
	for(int i=0;i<changesets.GetCount();i++)
	{
		const CDataFileMgr::ContentChangeSet* ccs = DATAFILEMGR.GetContentChangeSet(changesets[i].m_name);
		if((ccs!=0) != (changesets[i].m_condition))
		{
			return false;
		}
	}
	return ParseExpression(conditions->m_genericConditions.GetCStr());
}

void CMountableContent::ExecuteContentChangeSet(atHashString groupNameHash, atHashString changeSetName)
{
	EnsureDatFileLoaded();

	const ContentChangeSetGroup *group = FindContentChangeSetGroup(groupNameHash);

	if(!group || !group->Contains(changeSetName))
		return;
	
	strPackfileManager::SetExtraContentIndex(GetECPackFileIndex());

	if (!GetChangeSetActive(groupNameHash.GetHash(), changeSetName.GetHash()))
	{
		const CDataFileMgr::ContentChangeSet* changeSet = DATAFILEMGR.GetContentChangeSet(changeSetName);

		if (Verifyf(changeSet, "CMountableContent::ExecuteContentChangeSet - Invalid changeset '%s' for DLC %s", changeSetName.TryGetCStr(), GetName()))
		{
			const ExecutionConditions& conds = changeSet->m_executionConditions;
			if(!CheckContentChangeSetConditions(&conds))
			{
				Displayf("Conditions not met, skipping CCS");
				return;
			}

			if (groupNameHash == CCS_GROUP_MAP || groupNameHash == CCS_GROUP_MAP_SP)
			{
				eMapChangeStates currMapChangeState = EXTRACONTENT.GetMapChangeState();

				if (currMapChangeState != MCS_UPDATE && !EXTRACONTENT.GetAllowMapChangeAnytime())
				{
					dlcErrorf("CMountableContent::ExecuteContentChangeSet - Invalid map change state %i", currMapChangeState);
					return;
				}

				const u32 MAX_CACHE_FILE_NAME_LEN = 256;
				char cacheFileName[MAX_CACHE_FILE_NAME_LEN] = { 0 };
				char fmtDeviceAndPath[MAX_CACHE_FILE_NAME_LEN] = { 0 };
				char expDeviceAndPath[MAX_CACHE_FILE_NAME_LEN] = { 0 };

				formatf(cacheFileName, "%s_%u", GetName(), changeSetName.GetHash());
				formatf(fmtDeviceAndPath, "%s%%PLATFORM%%/data/cacheLoaderData_DLC%s/", GetDeviceName(), __BANK ? "_BANK" : "");

				DATAFILEMGR.ExpandFilename(fmtDeviceAndPath, expDeviceAndPath, sizeof(expDeviceAndPath));

				sm_mapChangeDirty = true;
				EXTRACONTENT.SetMapChangeState(MCS_UPDATE);
				CMapFileMgr::GetInstance().Reset();
				{
					// Set this value here so all interiors added in this frame are marked as DLC
					CInteriorProxy::ms_currExecutingCS = changeSetName.GetHash();
					{
						pgStreamer::Drain();
						strStreamingEngine::GetLoader().Flush();
						gRenderThreadInterface.Flush();

						CPathServer::UnloadAllMeshes(true); // We have to remove all navmeshes prior to switching out DLC rpf
						CPathServer::InvalidateTrackers(); // Invalidate ped trackers since they might reference navmesh:poly pairs no longer in existence

						atDelegate<void (strLocalIndex, bool)> setProxyDisabledCB;
						atDelegate<bool (strLocalIndex)> getProxyDisabledCB;

						setProxyDisabledCB.Bind(CInteriorProxy::SetIsDisabledDLC);
						getProxyDisabledCB.Bind(CInteriorProxy::GetIsDisabledForSlot);

						g_StaticBoundsStore.GetBoxStreamer().SetLocked(false);
						DATAFILEMGR.CreateDependentsGraph(changeSetName, true);
						INSTANCE_STORE.SetGetProxyDisabledCB(&getProxyDisabledCB);
						INSTANCE_STORE.SetSetProxyDisabledCB(&setProxyDisabledCB);
						INSTANCE_STORE.CreateChildrenCache();
						INSTANCE_STORE.SaveScriptImapStates();
						{
							CTexLod::ShutdownSession(SHUTDOWN_SESSION);
							CTexLod::InitSession(INIT_SESSION);
							gRenderThreadInterface.Flush(); // Flush here because CTexLod adds refs when releasing
							CFileLoader::ExecuteContentChangeSet(*changeSet);
						}
						INSTANCE_STORE.RestoreScriptImapStates();
						INSTANCE_STORE.DestroyChildrenCache();
						INSTANCE_STORE.SetGetProxyDisabledCB(NULL);
						INSTANCE_STORE.SetSetProxyDisabledCB(NULL);
						DATAFILEMGR.DestroyDependentsGraph();
						
						CMapFileMgr::GetInstance().SetMapFileDependencies();

						// Load new stuff and check if we can make use of cache loader
						bool useCacheLoader = changeSet->m_useCacheLoader && !PARAM_disableDLCcacheLoader.Get();
						bool prevReadyOnly = strCacheLoader::GetReadyOnly();
						atMap<s32, bool> dlcArchives;

						DATAFILEMGR.GetCCSArchivesToEnable(changeSetName, true, dlcArchives);

						if (useCacheLoader)
						{
							strCacheLoader::Init(cacheFileName, expDeviceAndPath, SCL_DLC, dlcArchives);
							strCacheLoader::Load();
						}
						
						INSTANCE_STORE.FixupAllEntriesAfterRegistration(false);
						CManagedImapGroupMgr::MarkManagedImapFiles();

						// If we can't use the cache or we've never loaded it or the files changed, then make sure we request and remove
						if (!useCacheLoader || strCacheLoader::GetFilesChanged() || (!strCacheLoader::GetLoadCache() && !strCacheLoader::HasLoadedCache()))
							g_StaticBoundsStore.RequestAndRemove(dlcArchives);

						g_StaticBoundsStore.PostRegistration();

						CInteriorProxy::AddAllDummyBounds(changeSetName.GetHash());

						if (useCacheLoader)
						{
							strCacheLoader::Save();
							strCacheLoader::Shutdown();
							strCacheLoader::SetReadOnly(prevReadyOnly);
						}

					#if SCRATCH_ALLOCATOR
						sysMemManager::GetInstance().GetScratchAllocator()->Reset();
					#endif
						INSTANCE_STORE.GetBoxStreamer().PostProcessPending();

					#if SCRATCH_ALLOCATOR
						sysMemManager::GetInstance().GetScratchAllocator()->Reset();
					#endif
						g_StaticBoundsStore.GetBoxStreamer().PostProcess();

					#if SCRATCH_ALLOCATOR
						sysMemManager::GetInstance().GetScratchAllocator()->Reset();
					#endif
					}
					CInteriorProxy::ms_currExecutingCS = 0;
				}
				EXTRACONTENT.SetMapChangeState(currMapChangeState);
			}
			else
			{
				CFileLoader::ExecuteContentChangeSet(*changeSet);
			}
			
			CFileLoader::RequestDLCItypFiles();
			SetChangeSetActive(groupNameHash.GetHash(), changeSetName.GetHash(), true);

		#if __BANK
			EXTRACONTENT.Bank_UpdateContentDisplay();
			EXTRAMETADATAMGR.RefreshBankWidgets();
		#endif
		}
	}
	else
		dlcDebugf2("CMountableContent::ExecuteContentChangeSet - DLC content change on content %s already active", GetName());

	strPackfileManager::SetExtraContentIndex(0);
}

void CMountableContent::RevertContentChangeSet(atHashString groupNameHash, atHashString changeSetName, u32 actionMask)
{
	const ContentChangeSetGroup *group = FindContentChangeSetGroup(groupNameHash);

	if(!group || !group->Contains(changeSetName))
		return;

	if (GetChangeSetActive(groupNameHash.GetHash(), changeSetName.GetHash()))
	{
		dlcDebugf1("CMountableContent::RevertContentChangeSet - Reverting DLC content change set group %s", GetName());
		
		const CDataFileMgr::ContentChangeSet* changeSet = DATAFILEMGR.GetContentChangeSet(changeSetName);

		if (Verifyf(changeSet, "CMountableContent::RevertContentChangeSet - Invalid changeset '%s' for DLC %s", changeSetName.TryGetCStr(), GetName()))
		{
			if (groupNameHash == CCS_GROUP_MAP || groupNameHash == CCS_GROUP_MAP_SP)
			{
				eMapChangeStates currMapChangeState = EXTRACONTENT.GetMapChangeState();

				if (currMapChangeState != MCS_UPDATE && !EXTRACONTENT.GetAllowMapChangeAnytime())
				{
					dlcErrorf("CMountableContent::RevertMapContentChangeSet - Invalid map change state %i", currMapChangeState);
					return;
				}

				EXTRACONTENT.SetMapChangeState(MCS_UPDATE);
				{
					pgStreamer::Drain();
					strStreamingEngine::GetLoader().Flush();
					gRenderThreadInterface.Flush();
					CPathServer::UnloadAllMeshes(true); // We have to remove all navmeshes prior to switching out DLC rpf
					CPathServer::InvalidateTrackers(); // Invalidate ped trackers since they might reference navmesh:poly pairs no longer in existence

					// If the player is in a DLC interior we are about to remove, nudge them now.
					if (CPed* pPlayer = CGameWorld::FindFollowPlayer())
					{
						if (CInteriorInst* pPlayerIntInst = pPlayer->GetPortalTracker()->GetInteriorInst())
						{
							CInteriorProxy* pProxy = pPlayerIntInst->GetProxy();

							if (pProxy && pProxy->GetChangeSetSource() == changeSetName.GetHash() && 
								!CWarpManager::IsActive() && !g_PlayerSwitch.IsActive())
							{
								Vec3V proxyPos;

								pProxy->GetPosition(proxyPos);
								CWarpManager::SetWarp(Vector3(proxyPos.GetXf(), proxyPos.GetYf(), proxyPos.GetZf()), VEC3_ZERO, 0.0, true, true, 600.0f);
								CWarpManager::FadeOutAtStart(false);
								CWarpManager::FadeInWhenComplete(false);
								CWarpManager::FadeOutPlayer(false);
							}
						}
					}

					atDelegate<void (strLocalIndex, bool)> setProxyDisabledCB;
					atDelegate<bool (strLocalIndex)> getProxyDisabledCB;

					setProxyDisabledCB.Bind(CInteriorProxy::SetIsDisabledDLC);
					getProxyDisabledCB.Bind(CInteriorProxy::GetIsDisabledForSlot);

					DATAFILEMGR.CreateDependentsGraph(changeSetName, true);
					INSTANCE_STORE.SetGetProxyDisabledCB(&getProxyDisabledCB);
					INSTANCE_STORE.SetSetProxyDisabledCB(&setProxyDisabledCB);
					INSTANCE_STORE.CreateChildrenCache();
					INSTANCE_STORE.SaveScriptImapStates();
					{
						CTexLod::ShutdownSession(SHUTDOWN_SESSION);
						CTexLod::InitSession(INIT_SESSION);
						gRenderThreadInterface.Flush(); // Flush here because CTexLod adds refs when releasing
						CFileLoader::RevertContentChangeSet(*changeSet, actionMask);
					}
					INSTANCE_STORE.RestoreScriptImapStates();
					INSTANCE_STORE.DestroyChildrenCache();
					INSTANCE_STORE.SetGetProxyDisabledCB(NULL);
					INSTANCE_STORE.SetSetProxyDisabledCB(NULL);
					DATAFILEMGR.DestroyDependentsGraph();
				}
				EXTRACONTENT.SetMapChangeState(currMapChangeState);
			}
			else
			{
				CFileLoader::RevertContentChangeSet(*changeSet, actionMask);
			}

			if (actionMask == CDataFileMgr::ChangeSetAction::CCS_ALL_ACTIONS)
				SetChangeSetActive(groupNameHash.GetHash(), changeSetName.GetHash(), false);

		#if __BANK
			EXTRACONTENT.Bank_UpdateContentDisplay();
			EXTRAMETADATAMGR.RefreshBankWidgets();
		#endif
		}
	}
	else
		dlcDebugf2("CMountableContent::RevertContentChangeSet - DLC content change on content %s is not active so cannot be reverted", GetName());
}

void CMountableContent::CleanupAfterMapChange()
{
	if (sm_mapChangeDirty)
	{
		g_MapTypesStore.VerifyItypFiles();
		CMapFileMgr::GetInstance().Cleanup();
		sm_mapChangeDirty = false;
	}
}

CMountableContent::eMountResult CMountableContent::MountContent(s32 XENON_ONLY(userIndex), const char* deviceName)
{
	if(m_pDevice PPU_ONLY( || m_pSecondaryDevice) )
		return MR_MOUNTED;

	dlcDebugf3("CMountableContent::MountContent - Device name: %s [%i]", deviceName, GetPrimaryDeviceType());

#if __XENON
	if (GetPrimaryDeviceType() == CMountableContent::DT_XCONTENT)
	{
		dlcDebugf3("CMountableContent::MountContent - This is an XCONTENT package");
		fiDeviceXContent* pXContent;
		if(m_pDevice == NULL)
		{
			m_pDevice = rage_new fiDeviceXContent;
		}
		pXContent = (fiDeviceXContent*)m_pDevice;

		if(!pXContent->Init( userIndex, m_xContentData ))
		{
			DeleteDevice();
			return MR_INIT_FAILED;
		}

		DWORD r = pXContent->MountAs( deviceName );

		if( r != ERROR_SUCCESS )
		{
			dlcErrorf("CMountableContent::MountContent - Mounting DLC package failed error code = 0x%x", r);
			DeleteDevice();

			if(r == ERROR_FILE_CORRUPT)
			{
				return MR_CONTENT_CORRUPTED;
			}

			return MR_MOUNT_FAILED;
		}
		InitCrcDevice(deviceName,pXContent);
	}
#endif // __XENON
		
	if (m_deviceType == CMountableContent::DT_PACKFILE)
	{
		dlcDebugf3("CMountableContent::MountContent - This is a PACKFILE package, filename %s", m_filename.c_str());
		fiPackfile* pPackfile = NULL;

		if(m_pDevice == NULL)
		{
			m_pDevice = rage_new fiPackfile;
			pPackfile = (fiPackfile*)m_pDevice;
		}

		if (!AssertVerify(pPackfile->Init(m_filename, true, fiPackfile::CACHE_NONE)))
		{
			DeleteDevice();
			return MR_INIT_FAILED;
		}

		if (!AssertVerify(pPackfile->MountAs(deviceName)))
		{
			DeleteDevice();
			return MR_MOUNT_FAILED;
		}

		if(InitSubPacks(deviceName) != MR_MOUNTED)
		{
			DeleteDevice();
			return MR_MOUNT_FAILED;			
		}

		InitCrcDevice(deviceName,pPackfile);
	}

	if (m_deviceType == CMountableContent::DT_FOLDER)
	{
		dlcDebugf3("CMountableContent::MountContent - This is a FOLDER package, filename %s", m_filename.c_str());
		fiDeviceRelative* pRelative;
		if(m_pDevice == NULL)	
		{
			m_pDevice = rage_new fiDeviceRelative;
		}
		pRelative = (fiDeviceRelative*)m_pDevice;
		pRelative->Init( m_filename, true );

		if (!AssertVerify( pRelative->MountAs( deviceName ) ))
		{
			DeleteDevice();
			return MR_MOUNT_FAILED;
		}
		InitCrcDevice(deviceName,pRelative);

		MountTitleUpdateDevice(deviceName);

		return MR_MOUNTED;
	}

#if __PPU	

	const u8 E1_k_licensee[16] = { 0x12,0x34,0x56,0x78,0x12,0x34,0x56,0x78,0x12,0x34,0x56,0x78,0x12,0x34,0x56,0x78 };
	if(m_pDevice == NULL)	
	{
		dlcDebugf3("CMountableContent::MountContent - Creating new device for mount");
		m_pDevice = rage_new fiDeviceRelative;
	}
#if !__FINAL
	if (!m_usesPackfile)
	{
		dlcDebugf3("CMountableContent::MountContent - Not using pack file - adding %s", m_filename.c_str());
		m_pDevice->Init( m_filename.c_str(), false );	//	why isn't the readOnly flag set to true?
	}
	else
#endif // !__FINAL
	{
		dlcDebugf3("CMountableContent::MountContent - Using packfile - adding %s", m_filename.c_str());
		m_pDevice->Init( m_filename.c_str(), true );	//	Always readOnly in final build (and in non-final if using packfiles)
	}

	if(!AssertVerify( m_pDevice->MountAs( deviceName ) ))
	{
		DeleteDevice();
		return MR_MOUNT_FAILED;
	}

	bool bPackfileSuccessfullyMounted = true;	//	Set this to true initially so that Unmount isn't called if m_usesPackfile is false in non-FINAL builds
#if !__FINAL
	if(m_usesPackfile)
#endif // !__FINAL
	{
		dlcDebugf3("CMountableContent::MountContent - Adding secondary packfile");
		if(m_pSecondaryDevice == NULL)
		{
			m_pSecondaryDevice = rage_new fiPackfile;
		}

		char NameOfRpfFile[RAGE_MAX_PATH];
		safecpy(NameOfRpfFile, m_filename.c_str());
		m_pSecondaryDevice->SetDrmKey(&E1_k_licensee);
		safecat(NameOfRpfFile, "/DLC.edat");
		dlcDebugf3("CMountableContent::MountContent - Mounting secondary at %s", NameOfRpfFile);

		bPackfileSuccessfullyMounted = m_pSecondaryDevice->Init( NameOfRpfFile, true, fiPackfile::CACHE_NONE );

		if(!Verifyf(bPackfileSuccessfullyMounted, "CMountableContent::MountContent - Failed to Init DLC archive %s", NameOfRpfFile))
		{
			DeleteSecondaryDevice();
			return MR_INIT_FAILED;
		}

		if(!AssertVerify( m_pSecondaryDevice->MountAs( deviceName ) ))
		{
			DeleteSecondaryDevice();
			return MR_MOUNT_FAILED;
		}
		InitCrcDevice(deviceName,m_pSecondaryDevice);
	}

	//	Call this after m_mounted is set to true so that Unmount doesn't return immediately
	if (!bPackfileSuccessfullyMounted)
	{
		UnmountContent(deviceName);	
		return MR_MOUNT_FAILED;
	}
#else // __PPU
	(void)deviceName;
#endif

	MountTitleUpdateDevice(deviceName);

	return MR_MOUNTED;
}

void CMountableContent::UnmountContent(const char* deviceName)
{
	if (!m_pDevice)
		return;

	char crcDevice[RAGE_MAX_PATH] = {0};
	strncpy(crcDevice,deviceName, static_cast<int>(strcspn(deviceName,":")));
	strcat(crcDevice,"CRC:/");

	dlcDebugf3("CMountableContent::UnmountContent - Device name: %s [%i]", deviceName, GetPrimaryDeviceType());

	if( !Verifyf(fiDevice::Unmount( crcDevice )&&fiDevice::Unmount( deviceName ), "Failed to unmount the pack with device name = %s", m_pCrcDevice->GetDebugName()) )
	{
		Errorf( "CMountableContent::UnmountContent - Failed to unmount the pack" );

		DeleteCrcDevice();
		DeleteDevice();
	#if __PPU
		DeleteSecondaryDevice();
	#endif // __PPU
		ShutdownSubPacks();

		return;
	}
	if (m_deviceType == CMountableContent::DT_XCONTENT)
	{
	#if __XENON
		((fiDeviceXContent*)m_pDevice)->Shutdown();
	#endif //__XENON
	}
	else if (m_deviceType == CMountableContent::DT_PACKFILE)
	{
		((fiPackfile*)m_pDevice)->Shutdown();
	}
#if __PPU
	else if (m_deviceType == CMountableContent::DT_DLC_DISC)
	{
		// shutdown the packfile device (folder device doesn't need shutdown)
		((fiPackfile*)m_pSecondaryDevice)->Shutdown();
	}

#if !__FINAL
	if(m_usesPackfile)
#endif
	{
		Assert(m_pSecondaryDevice);
		if (m_pSecondaryDevice)
		{	// shutdown the packfile device
			strPackfileManager::UnregisterDevice(m_pSecondaryDevice);
			DeleteSecondaryDevice();
		}
	}
#endif
	Assert(m_pCrcDevice);
	if (m_pCrcDevice)
	{
		strPackfileManager::UnregisterDevice(m_pCrcDevice);
		DeleteCrcDevice();
	}

	//	I'm going to try deleting and NULLing the primary device here and see if it causes any problems
	if (Verifyf(m_pDevice, "UnmountContent - Device is NULL!"))
	{
		strPackfileManager::UnregisterDevice(m_pDevice);
		DeleteDevice();
	}

	ShutdownSubPacks();

	UnmountTitleUpdateDevice();
}

void CMountableContent::InitCrcDevice(const char* deviceName,  fiDevice* parentDevice, bool titleUpdate)
{
	char crcDevice[RAGE_MAX_PATH] = {0};
	strncpy(crcDevice,deviceName, static_cast<int>(strcspn(deviceName,":")));
	strcat(crcDevice,"CRC:/");
	if(titleUpdate)
	{
		if(m_pCrcDeviceTU== NULL)
		{
			m_pCrcDeviceTU = rage_new fiDeviceCrc;
		}
		m_pCrcDeviceTU->Init(deviceName,true,parentDevice);
		m_pCrcDeviceTU->MountAs(crcDevice);
	}
	else
	{
		if(m_pCrcDevice== NULL)
		{
			m_pCrcDevice = rage_new fiDeviceCrc;
		}
		m_pCrcDevice->Init(deviceName,true,parentDevice);
		m_pCrcDevice->MountAs(crcDevice);
	}
}

void CMountableContent::DeleteCrcDevice()
{
	if(m_pCrcDevice)
		delete m_pCrcDevice;
	m_pCrcDevice = NULL;
}

void CMountableContent::DeleteDevice()
{
	if(m_pDevice)
		delete m_pDevice;
	m_pDevice = NULL;
}

#if __PPU

void CMountableContent::DeleteSecondaryDevice()
{
	if(m_pSecondaryDevice)
		delete m_pSecondaryDevice;

	m_pSecondaryDevice = NULL;
}

#endif // __PPU

const SExtraTitleUpdateMount *CMountableContent::FindExtraTitleUpdateMount(const char *deviceName)
{
	for(int i = 0; i < sm_ExtraTitleUpdateData.m_Mounts.GetCount(); i++)
	{
		if(!strnicmp(sm_ExtraTitleUpdateData.m_Mounts[i].m_deviceName, deviceName, RAGE_MAX_PATH))
		{
			return &sm_ExtraTitleUpdateData.m_Mounts[i];
		}
	}

	return NULL;
}

void CMountableContent::MountTitleUpdateDevice(const char *deviceName)
{
	Assert(!m_pDeviceTU);	

	if(const SExtraTitleUpdateMount *tuMount = FindExtraTitleUpdateMount(deviceName))
	{
		m_pDeviceTU = rage_new fiDeviceRelative;
		m_pDeviceTU->Init( tuMount->m_path.c_str(), true );

		if (!Verifyf( m_pDeviceTU->MountAs( deviceName ), "Failed to mount TU DLC patch %s to %s", tuMount->m_path.c_str(), deviceName ))
		{
			delete m_pDeviceTU;
			m_pDeviceTU = NULL;
			return;
		}
		if(!m_pCrcDeviceTU)
			InitCrcDevice(deviceName,m_pDeviceTU,true);
	}
}

void CMountableContent::UnmountTitleUpdateDevice()
{
	if(m_pDeviceTU)
	{
		strPackfileManager::UnregisterDevice(m_pDeviceTU);
		delete m_pDeviceTU;
		m_pDeviceTU = NULL;
	}
	if(m_pCrcDeviceTU)
	{
		strPackfileManager::UnregisterDevice(m_pCrcDeviceTU);
		delete m_pCrcDeviceTU;
		m_pCrcDeviceTU = NULL;
	}
}

bool CMountableContent::LoadSetupXML(const char* path)
{
	bool retVal = false;

	if (!ASSET.Exists(path, NULL))
		return false;

	// Try to load it using parcodegen format, if that fails, just try the original format
	parSettings settings = PARSER.Settings();
	settings.SetFlag(parSettings::CULL_OTHER_PLATFORM_DATA,true);
	if (PARSER.LoadObject(path, NULL, m_setupData,&settings))
	{
		m_setupData.m_deviceName += ":/";
		if(m_setupData.m_isLevelPack)
		{
			m_setupData.m_type = EXTRACONTENT_LEVEL_PACK;
		}
		retVal = true;
	}
	else
	{
		// ****************************************
		// TODO: Remove this section... very soon.
		// This section is completely depreciated we only support this since we can't update the data on the dev store at the moment...
		parTree* pTree = PARSER.LoadTree(path, "");

		if (pTree)
		{
			parTreeNode* pRootNode= pTree->GetRoot();
			parTreeNode* pContentNode = NULL;
			parTreeNode* pNode;

			retVal = true;

		#if __FINAL
			if(!pRootNode->FindChildWithName("testmarketplace"))
		#endif // __FINAL
			{
				// Get device name
				pNode = pRootNode->FindChildWithName("device");

				if (pNode)
				{
					m_setupData.m_deviceName = pNode->GetData();
					m_setupData.m_deviceName += ":/";
				}

				// Get content
				while((pContentNode = pRootNode->FindChildWithName("content", pContentNode)) != NULL)
				{
					pNode = pContentNode->FindChildWithName("name");

					if (pNode)
						m_setupData.m_nameHash = pNode->GetData();
					pNode = pContentNode->FindChildWithName("datfile");
					if(pNode)
						m_setupData.m_datFile = pNode->GetData();

					pNode = pContentNode->FindChildWithName("contentchangesets");
					if(pNode)
					{
						parTreeNode::ChildNodeIterator it = pNode->BeginChildren();

						while (it != pNode->EndChildren())
						{
							m_setupData.m_contentChangeSets.Grow() = (*it)->GetData();
							++it;
						}
					}

					pNode = pContentNode->FindChildWithName("script");
					if( pNode )
					{
						if( const parTreeNode* pStartupNode = pNode->FindChildWithName("startup"))
						{
							char tmp[32];
							strncpy(tmp, pStartupNode->GetData(), sizeof(tmp));
							m_setupData.m_startupScript.SetFromString(tmp);
						}

						if( const parTreeNode* pStackSizeNode = pNode->FindChildWithName("stackSize"))
						{
							m_setupData.m_scriptCallstackSize = (s32)atoi(pStackSizeNode->GetData());
						}
					}
				}
			}
		}

		delete pTree;
		// TODO: Remove this section... very soon.
		// This section is completely depreciated we only support this since we can't update the data on the dev store at the moment...
		// ****************************************
	}

	Assertf(retVal, "CMountableContent does not have a valid setup file! %s", GetFilename());

	return retVal;
}

void CMountableContent::SetChangeSetActive(atHashString groupName, atHashString hashName, bool active)
{
	if (active)
	{
		for (u32 i = 0; i < m_activeChangeSets.GetCount(); i++)
		{
			if (m_activeChangeSets[i].m_group == groupName && m_activeChangeSets[i].m_changeSetName == hashName)
				return;
		}

		m_activeChangeSets.PushAndGrow(SActiveChangeSet(groupName, hashName), 1);
	}
	else
	{
		for (u32 i = 0; i < m_activeChangeSets.GetCount(); i++)
		{
			if (m_activeChangeSets[i].m_group == groupName && m_activeChangeSets[i].m_changeSetName == hashName)
			{
				m_activeChangeSets.Delete(i);
				i--;
				return;
			}
		}

		Assertf(0, "CMountableContent::SetChangeSetActive - Couldn't find change set to deactivate! [%s] [%s]", groupName.GetCStr(), hashName.GetCStr());
	}
}

bool CMountableContent::GetChangeSetActive(atHashString groupName, atHashString hashName)
{
	for (u32 i = 0; i < m_activeChangeSets.GetCount(); i++)
	{
		if (m_activeChangeSets[i].m_group == groupName && m_activeChangeSets[i].m_changeSetName == hashName)
			return true;
	}

	return false;
}

const char *CMountableContent::GetContentStatusName(eContentStatus status)
{
	static const char *name[] = 
	{
		"CS_NONE",
		"CS_SETUP_READ",
		"CS_FULLY_MOUNTED",
		"CS_CORRUPTED",
		"CS_INVALID_SETUP",
		"CS_SETUP_MISSING",
		"CS_UPDATE_REQUIRED"
	};

	CompileTimeAssert( COUNTOF(name) == CS_COUNT); 	

	return name[status];
};

const char *CMountableContent::GetSetupFileName()
{
	return SETUP_FILE_NAME;
}

void CMountableContent::LoadExtraTitleUpdateData(const char *fname)
{
	if(Verifyf(PARSER.LoadObject(fname, "meta", sm_ExtraTitleUpdateData), 
		"CMountableContent::LoadExtraTitleUpdateData couldn't load %s.", fname))
	{
		Displayf("Loaded ExtraTitleUpdateData from %s", fname);
	}
}

void CMountableContent::LoadExtraFolderMountData(const char *fname)
{
	SExtraFolderMountData data;
	parSettings settings = PARSER.Settings();
	settings.SetFlag(parSettings::CULL_OTHER_PLATFORM_DATA,true);
	if(Verifyf(PARSER.LoadObject(fname, NULL, data,&settings), 
		"CMountableContent::LoadExtraFolderMountData couldn't load %s.", fname))
	{
		Displayf("Loaded ExtraFolderMountData from %s", fname);

		for(int i = 0; i < data.m_FolderMounts.GetCount(); i++)
		{
			Displayf("ExtraContent folder %s mountAs %s", 
				data.m_FolderMounts[i].m_path.c_str(), data.m_FolderMounts[i].m_mountAs.c_str());
			AddExtraFolderMount(data.m_FolderMounts[i]);
		}
	}
}

void CMountableContent::UnloadExtraFolderMountData(const char *fname)
{
	SExtraFolderMountData data;

	if(Verifyf(PARSER.LoadObject(fname, NULL, data), 
		"CMountableContent::UnloadExtraFolderMountData couldn't load %s.", fname))
	{
		Displayf("Unloading ExtraFolderMountData from %s", fname);

		for(int i = 0; i < data.m_FolderMounts.GetCount(); i++)
		{
			Displayf("Removing ExtraContent folder %s mounted as %s", 
				data.m_FolderMounts[i].m_path.c_str(), data.m_FolderMounts[i].m_mountAs.c_str());
			RemoveExtraFolderMount(data.m_FolderMounts[i]);
		}
	}
}

bool CMountableContent::AddExtraFolderMount(const SExtraFolderMount &mountData)
{
	for(int i = 0; i < sm_ExtraFolderMountData.m_FolderMounts.GetCount(); i++)
	{
		if(strnicmp(sm_ExtraFolderMountData.m_FolderMounts[i].m_path.c_str(), mountData.m_path.c_str(), RAGE_MAX_PATH) == 0)
		{
			Errorf("ExtraFolderMount %s -> %s already exists!", 
				sm_ExtraFolderMountData.m_FolderMounts[i].m_path.c_str(), 
				sm_ExtraFolderMountData.m_FolderMounts[i].m_mountAs.c_str());

			return false;			
		}
	}

	SExtraFolderMount newMount = mountData;

	if(newMount.Init())
	{
		sm_ExtraFolderMountData.m_FolderMounts.PushAndGrow(newMount);	
	}

	return true;
}

bool CMountableContent::RemoveExtraFolderMount(const SExtraFolderMount &mountData)
{
	for(int i = 0; i < sm_ExtraFolderMountData.m_FolderMounts.GetCount(); i++)
	{
		SExtraFolderMount &m = sm_ExtraFolderMountData.m_FolderMounts[i];

		if(strnicmp(m.m_path.c_str(), mountData.m_path.c_str(), RAGE_MAX_PATH) == 0)
		{
			m.Shutdown();
			sm_ExtraFolderMountData.m_FolderMounts.DeleteFast(i);
			return true;			
		}
	}

	return false;
}

CMountableContent *CMountableContent::GetContentForChangeSet(u32 hash)
{
	if(CMountableContent **content = sm_changeSetLookup.Access(hash))
	{
		return *content;
	}

	return NULL;
}

#if __BANK
bool CMountableContent::HasAnyChangesets()
{
	for (u32 i = 0; i < m_setupData.m_contentChangeSetGroups.GetCount(); i++)
	{
		if (m_setupData.m_contentChangeSetGroups[i].m_ContentChangeSets.GetCount() > 0)
			return true;
	}

	return false;
}

void CMountableContent::GetChangesetGroups(atArray<const char*>& groupNames)
{
	for (u32 i = 0; i < m_setupData.m_contentChangeSetGroups.GetCount(); i++)
		groupNames.PushAndGrow(m_setupData.m_contentChangeSetGroups[i].m_NameHash.GetCStr(), 1);
}

void CMountableContent::GetChangeSetsForGroup(u32 changeSetGroup, atArray<const char*>& setNames)
{
	const ContentChangeSetGroup* groupCCSs = FindContentChangeSetGroup(changeSetGroup);

	if (groupCCSs)
	{
		for (u32 i = 0; i < groupCCSs->m_ContentChangeSets.GetCount(); i++)
			setNames.PushAndGrow(groupCCSs->m_ContentChangeSets[i].GetCStr());
	}
}

u32 CMountableContent::GetTotalContentChangeSetCount() const
{
	u32 totalCount = 0;

	for(int i = 0; i < m_setupData.m_contentChangeSetGroups.GetCount(); i++)
	{
		totalCount += m_setupData.m_contentChangeSetGroups[i].m_ContentChangeSets.GetCount();
	}

	return totalCount;
}
#endif

void CMountableContent::GetMapChangeHashes(atArray<u32>& mapChangeHashes) const
{
	for (u32 i = 0; i < m_activeChangeSets.GetCount(); i++)
	{
		if (m_activeChangeSets[i].m_group == CCS_GROUP_MAP ||
			m_activeChangeSets[i].m_group == CCS_GROUP_MAP_SP)
		{
			mapChangeHashes.PushAndGrow(m_activeChangeSets[i].m_changeSetName, 1);
		}
	}
}

bool CMountableContent::UpdateRequired(const char* currentVer, const char* requiredVer) const
{
	if(!requiredVer)
		return false;
	if(!requiredVer[0]) 
		return false;

	// CURRENT
	s32 cMaj, cMin;
	this->ParseVersionString(currentVer,  cMaj, cMin);

	// REQUIRED
	s32 rMaj, rMin;
	this->ParseVersionString(requiredVer, rMaj, rMin);

	if(cMaj < rMaj)  return true;
	if(cMaj == rMaj) return (cMin < rMin);
	if(cMaj > rMaj)  return false;

	return true;
}

void CMountableContent::ParseVersionString(const char* cc, s32& majVer, s32& minVer) const
{
	atString str(cc);
	
	// Cut off unnecessary characters from the end of the string.
	for(u32 i=0; i<str.length(); ++i)
	{
		if( (str[i] < '0'|| str[i] > '9') && str[i] != '.' )
		{
			str.Truncate(i);
			break;
		}
	}
	
	// Find if the version includes a minor and major part.
	if( str.IndexOf('.') == -1)
	{ 
		minVer = 0;
		majVer = (s32)atoi(str.c_str());
	}
	else
	{
		atString lStr;
		atString rStr;
		str.Split(lStr, rStr, '.');

		minVer = (s32)atoi(rStr.c_str());
		majVer = (s32)atoi(lStr.c_str());
	}
}

CMountableContent::eMountResult CMountableContent::InitSubPacks(const char* deviceName)
{
	if(m_setupData.m_subPackCount <= 0)
		return MR_MOUNTED;

	if(!m_subPacks.empty())
		return MR_MOUNTED;

	char path[RAGE_MAX_PATH] = { 0 };
	strncpy(path, m_filename.c_str(), RAGE_MAX_PATH);
	char *fname = strrchr(path, '/');

	if(Verifyf(fname != NULL, "Failed to find dlc rpf file name in path %s", path))
	{
		fname++;

		for(int i = 0; i < m_setupData.m_subPackCount; i++)
		{
			sprintf(fname, "dlc%d.rpf", i + 1);

			if(!ASSET.Exists(path, ""))
				break;

			fiPackfile *pPackfile = rage_new fiPackfile;

			if (!AssertVerify(pPackfile->Init(path, true, fiPackfile::CACHE_NONE)))
			{
				delete pPackfile;
				break;
			}

			if (!AssertVerify(pPackfile->MountAs(deviceName)))
			{
				delete pPackfile;
				break;
			}

			m_subPacks.PushAndGrow(pPackfile);
		}
	}

	if(m_subPacks.GetCount() != m_setupData.m_subPackCount)
	{
		Assertf(m_subPacks.GetCount() == m_setupData.m_subPackCount, "Failed to mount %d additional rpfs for dlc pack %s, found (%d). Please check that the subPackCount value in build/dev_ng/setup2.xml for this DLC matches the number of additional DLC rpfs (dlc1.rpf, dlc2.rpf, etc.)", 
			m_setupData.m_subPackCount, m_filename.c_str(), m_subPacks.GetCount());

		ShutdownSubPacks();
		return MR_MOUNT_FAILED;
	}

	return MR_MOUNTED;
}

void CMountableContent::ShutdownSubPacks()
{
	for(int i = 0; i < m_subPacks.GetCount(); i++)
	{
		m_subPacks[i]->Shutdown();
		delete m_subPacks[i];
	}

	m_subPacks.Reset();
}

bool SExtraFolderMount::Init()
{
	Assert(!m_device);

	if(IsRPF())
	{
		fiPackfile *device = rage_new fiPackfile();
		device->Init(m_path.c_str(), true, fiPackfile::CACHE_NONE);
		if(!Verifyf( device->MountAs(m_mountAs.c_str()), "Failed to mount ExtraFolderMount %s -> %s", 
			m_path.c_str(), m_mountAs.c_str()))
		{
			delete device;
			return false;
		}

		m_device = device;
	}
	else
	{
		fiDeviceRelative *device = rage_new fiDeviceRelative();
		char expanded[RAGE_MAX_PATH]={0};
		DATAFILEMGR.ExpandFilename(m_path.c_str(),expanded,sizeof(expanded));
		device->Init(expanded, fiDevice::GetIsReadOnly(m_mountAs.c_str()), NULL);

		if(!Verifyf( device->MountAs(m_mountAs.c_str()), "Failed to mount ExtraFolderMount %s -> %s", 
			expanded, m_mountAs.c_str()))
		{
			delete device;
			return false;
		}

		m_device = device;
	}

	return true;
}

void SExtraFolderMount::Shutdown()
{
	Assert(m_device);

	fiDevice::Unmount(*m_device);

	if(IsRPF())
	{
		strPackfileManager::UnregisterDevice(m_device);
	}

	delete m_device;
	m_device = NULL;
}


