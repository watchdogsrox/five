//
// filename: ExtraContent.cpp
// description:	Class controlling the loading of extra content once game has been released
//

#include "ExtraContent.h"

// Rage headers
#include "bank/bkmgr.h"
#include "file/asset.h"
#include "file/cachepartition.h"
#include "file/default_paths.h"
#include "file/device.h"
#include "file/device_relative.h"
#include "file/device_crc.h"
#include "parser/manager.h"
#include "parser/tree.h"
#include "rline/rlpresence.h"
#include "fwanimation/clipsets.h"
#include "fwanimation/expressionsets.h"
#include "fwanimation/facialclipsetgroups.h"
#include "fwcommerce/CommerceConsumable.h"
#include "string/stringutil.h"
#include "system/appcontent.h"
#include "system/messagequeue.h"
#include "fragment/tune.h"
#include "system/platform.h"

// Game headers
#include "control/Gen9ExclusiveAssets.h"
#include "peds/PlayerSpecialAbility.h"
#include "rline/cloud/rlcloud.h"
#include "streaming/packfilemanager.h"
#include "network/network.h"
#include "network/Cloud/Tunables.h"
#include "frontend/WarningScreen.h"
#include "scene/dlc_channel.h"
#include "scene/DownloadableTextureManager.h"
#include "scene/fileloader.h"
#include "scene/loader/mapFileMgr.h"
#include "scene/portals/LayoutManager.h"
#include "scene/loader/mapFileMgr.h"
#include "script/script.h"
#include "streaming/streaming.h"
#include "streaming/streamingengine.h"
#include "streaming/streaminginstall_psn.h"
#include "task/system/Tuning.h"
#include "system/controlmgr.h"
#include "system/filemgr.h"
#include "system/device_xcontent.h"
#include "system/xtl.h"
#include "task/physics/BlendFromNMData.h"
#include "text/TextConversion.h"
#include "ExtraContent_parser.h"
#include "scene/ExtraMetadataMgr.h"
#include "scene/scene.h"
#include "file/device_installer.h"
#include "frontend/loadingscreens.h"
#include "Network/commerce/CommerceManager.h"
#include "camera/CamInterface.h"
#include "network/Live/NetworkTelemetry.h"
#include "network/General/NetworkAssetVerifier.h"
#include "renderer/MeshBlendManager.h"
#include "Cutscene/CutSceneManagerNew.h"
#include "Stats/StatsInterface.h"
#include "weapons/inventory/PedInventoryLoadOut.h"
#include "audio/northaudioengine.h"


// No need for the final block around these, they turn into stripped_param in FINAL
PARAM(ignorePacks, "Ignores a given set of packs based on their nameHash from setup2, delimit with ;");
PARAM(extracontent, "Extra content development path");
PARAM(extracloudmanifest, "specify path to local ExtracontentManager cloud manifest file wich will be used instead of real one");
PARAM(disableExtraCloudContent, "Disables extra cloud content for !final builds");
PARAM(netSessionIgnoreECHash, "Ignore the CExtraContentManager::GetCRC data hash");
PARAM(disablecompatpackcheck, "Disables compatibility pack configuration check for MP (non FINAL only)");
PARAM(delayDLCLoad, "Don't automatically load DLC");
PARAM(useDebugCloudPatch, "Use debug suffix for the cloud patch");
PARAM(enableCloudTestWidgets, "Force cloud/manifest check results through widgets");
PARAM(christmas, "set the xmas special trigger by default");
PARAM(usecompatpacks, "mount all available compatibility packs");
PARAM(disablepackordercheck, "Disables compatibility packs order check");
PARAM(loadMapDLCOnStart, "Automatically loads both SP and MP map changes at startup");
PARAM(enableMapCCS, "Automatically loads specified CCS at session init");
PARAM(enableSPMapCCS, "Automatically loads specified SP map CCS at session init");
PARAM(ignoreDeployedPacks, "Ignore deployed packs");
PARAM(buildDLCCacheData, "Build cache data for all SP & MP map data currently installed");
XPARAM(checkUnusedFiles);


#if RSG_ORBIS
PARAM(disableEntitlementCheck, "Disables entitlement check on PS4");
#endif

RAGE_DEFINE_CHANNEL(dlc, DIAG_SEVERITY_DISPLAY, DIAG_SEVERITY_DISPLAY, DIAG_SEVERITY_ASSERT)

#if __PPU || __XENON
#define TITLE_UPDATE_RPF_PATH "update:/update.rpf"
#define TITLE_UPDATE2_RPF_PATH "update:/update2.rpf"
#else // NG
#if __FINAL
#define TITLE_UPDATE_RPF_PATH "update/update.rpf"
#define TITLE_UPDATE2_RPF_PATH "update/update2.rpf"
#else
#define TITLE_UPDATE_RPF_PATH "update/update_debug.rpf"
#define TITLE_UPDATE2_RPF_PATH "update/update2_debug.rpf"
#endif	// __FINAL
#endif 

#define TITLE_UPDATE_MOUNT_PATH "update:/"

#define EC_REBOOT_ID ATSTRINGHASH("R*EC", 0xA579A802)
#define MAP_CHANGE_FADE_TIME 600
#define COMPATPACKS_SET_PATH "common:/data/debug/compatpacks.xml"
#define COMPATPACKS_DEPLOYED_PATH "common:/data/dlclist.xml"
#define DLC_PACK_MOUNT_TIMEOUT (20.0f)
#define DLC_PACKS_PATH "platform:/dlcPacks/"

#if RSG_XENON
	#define LOAD_SCR_PACK_THRESHOLD 10
#endif

#undef DEFINE_CCS_GROUP
#define DEFINE_CCS_GROUP(enumName, stringName, hash) static atHashString gs_##enumName = atHashString(stringName);

// Macro magic to make sure all group names are registered with the string hash map
CCS_TITLES

static rlPresence::Delegate g_PresenceDlgt(CExtraContentManager::OnPresenceEvent);
static ServiceDelegate g_addContentServiceDelegate;

#if RSG_PS3
#include <stddef.h>
#include "fwutil/KeyGen.h"
extern const unsigned char __start__Ztext[];
extern const unsigned char __stop__Ztext[];
#elif RSG_XENON
#pragma const_seg("rodata1")
const unsigned char __start__Ztext = 0;
#pragma const_seg()
#pragma bss_seg("arsbss")
unsigned char __stop__Ztext;
#pragma bss_seg()
#endif

namespace rage
{
	NOSTRIP_XPARAM(audiofolder);
}

#if RSG_ORBIS
sysIpcThreadId s_contentPollThread = sysIpcThreadIdInvalid;
#endif

sysIpcThreadId s_codeCheckThread = sysIpcThreadIdInvalid;
bool s_codeCheckRun = true;
bool s_codeCompromised = false;

#if __BANK 
namespace rage {
	extern atBinaryMap<atString, u32> g_RequestedFiles;
	extern atBinaryMap<atString, u32> g_RequestedDevices;
}

enum {
	SHOW_FILES_PER_PAGE = 256
};

static atBinaryMap<atString, u32> s_loadedFilesAbsolute;
static atBinaryMap<atString, u32> s_unusedFilesAbsolute;
static u32 s_loadedFilesPageNumber = 0;
static u32 s_loadedFilesPageCount = 0;
static u32 s_unusedFilesPageNumber = 0;
static u32 s_unusedFilesPageCount = 0;

// This is probably really, really slow. So much iterating and recursing! Gets all the files inside dev:/path,
// recursing into any directories
static void GetDeviceContentsRecursive(atBinaryMap<atString, u32> &map, const fiDevice *dev, const char *path) {
	fiFindData data;
	fiHandle handle = dev->FindFileBegin(path, data);
	bool validHandle = fiIsValidHandle(handle);

	while (validHandle) {
		char currentPath[RAGE_MAX_PATH];

		if (data.m_Name[0] != '.') { // skip over ., .., and UNIX-style hidden files
			// Found a directory, so deal with it
			if ((data.m_Attributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) {
				formatf(currentPath, sizeof(currentPath), "%s\\%s", path, data.m_Name);

				GetDeviceContentsRecursive(map, dev, currentPath);
			} else {
				// Build the new pathname to use
				formatf(currentPath, "%s\\%s", path, data.m_Name);

				atString currentPathString(currentPath);
				atHashString currentPathHash = atStringHash(currentPath);

				map.Insert(currentPathHash, currentPathString);
			}
		}

		validHandle = dev->FindFileNext(handle, data);
	}

	dev->FindFileEnd(handle);
}

// Convenience form of above.
// Expects name in format of "device:"
static void GetDeviceContentsRecursive(atBinaryMap<atString, u32> &map, const char *deviceName) {
	// append '/'
	char fixedDeviceName[RAGE_MAX_PATH] = { 0 };
	strncpy(fixedDeviceName, deviceName, RAGE_MAX_PATH);
	fixedDeviceName[strlen(deviceName)] = '/';

	// find device
	if (const fiDevice *device = fiDevice::GetDevice(fixedDeviceName)) {
		return GetDeviceContentsRecursive(map, device, fixedDeviceName);
	}
}

#endif

const char * GetDeviceTypeString (CMountableContent::eDeviceType deviceType);

class CExtraContentFileMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile& file)
	{
		bool retValue = true;

		dlcDebugf3("CExtraContentFileMounter::LoadDataFile - Found file %s", file.m_filename);

		switch (file.m_fileType)
		{
			case CDataFileMgr::GTXD_PARENTING_DATA:
				{
					CMapFileMgr::GetInstance().ProcessGlobalTxdParentsFile(file.m_filename);
				}
			break;
			case CDataFileMgr::OVERLAY_INFO_FILE:
				EXTRACONTENT.LoadOverlayInfo(file.m_filename);
			break;
			case CDataFileMgr::CONTENT_UNLOCKING_META_FILE:
			{
				SContentUnlocks newUnlocks;

				if (PARSER.LoadObject(file.m_filename, NULL, newUnlocks))
				{
					dlcDebugf3("CExtraContentFileMounter::LoadDataFile - Found %i new content unlocks from %s", newUnlocks.m_listOfUnlocks.GetCount(), file.m_filename);

					for (int i = 0; i < newUnlocks.m_listOfUnlocks.GetCount(); i++)
						EXTRACONTENT.ModifyContentLockState(newUnlocks.m_listOfUnlocks[i].GetHash(), false);
				}
			}
			break;

			case CDataFileMgr::CLIP_SETS_FILE:
			{
				fwClipSetManager::PatchClipSets(file.m_filename);
			}
			break;

			case CDataFileMgr::EXPRESSION_SETS_FILE:
			{
				fwExpressionSetManager::Append(file.m_filename);
			}
			break;

			case CDataFileMgr::FACIAL_CLIPSET_GROUPS_FILE:
			{
				fwFacialClipSetGroupManager::Append(file.m_filename);
			}
			break;

			case CDataFileMgr::NM_BLEND_OUT_SETS_FILE:
			{
				CNmBlendOutSetManager::Append(file.m_filename);
			}
			break;

			case CDataFileMgr::NM_TUNING_FILE:
			{
				CTuningManager::AppendPhysicsTasks(file.m_filename);
			}
			break;
			case CDataFileMgr::TEXTFILE_METAFILE:
			{
				TheText.AddExtracontentTextFromMetafile(file.m_filename);
			}
			break;

			case CDataFileMgr::MOVE_NETWORK_DEFS:
			{
				fwAnimDirector::LoadNetworkDefinitions(file.m_filename);
			}
			break;

			case CDataFileMgr::EXTRA_TITLE_UPDATE_DATA:
			{
				CMountableContent::LoadExtraTitleUpdateData(file.m_filename);
			}
			break;

			case CDataFileMgr::WORLD_HEIGHTMAP_FILE:
			{
				CGameWorldHeightMap::LoadExtraFile(file.m_filename);
			}
			break;

			case CDataFileMgr::WATER_FILE:
			{
				// do nothing
			}
			break;

			case CDataFileMgr::EXTRA_FOLDER_MOUNT_DATA:
			{
				CMountableContent::LoadExtraFolderMountData(file.m_filename);
			}
			break;

			case CDataFileMgr::VEHICLE_SHOP_DLC_FILE:
			{
				EXTRAMETADATAMGR.AddVehiclesCollection(file.m_filename);
			}
			break;

			case CDataFileMgr::LEVEL_STREAMING_FILE:
			{
				CStreaming::SetStreamingFile(file.m_filename);
			}
			break;

			case CDataFileMgr::LOADOUTS_FILE:
			{
				CPedInventoryLoadOutManager::Append(file.m_filename);
			}
			break;

			case CDataFileMgr::CARCOLS_GEN9_FILE:
			{
				CVehicleModelInfo::AppendVehicleColorsGen9(file.m_filename);
			}
			break;

			case CDataFileMgr::CARMODCOLS_GEN9_FILE:
			{
				CVehicleModelInfo::LoadVehicleModColorsGen9(file.m_filename);
			}
			break;

			case CDataFileMgr::GEN9_EXCLUSIVE_ASSETS_VEHICLES_FILE :
			{
				CGen9ExclusiveAssets::LoadGen9ExclusiveAssetsVehiclesData(file.m_filename);
			}
			break;

			case CDataFileMgr::GEN9_EXCLUSIVE_ASSETS_PEDS_FILE :
			{
				CGen9ExclusiveAssets::LoadGen9ExclusiveAssetsPedsData(file.m_filename);
			}
			break;

			default: Errorf("CExtraContentFileMounter::LoadDataFile Invalid file type! %s [%i]", file.m_filename, file.m_fileType); break;
		}

		return retValue;
	}

	virtual void UnloadDataFile(const CDataFileMgr::DataFile& file) 
	{
		dlcDebugf3("CExtraContentFileMounter::UnloadDataFile - Found file %s", file.m_filename);

		switch (file.m_fileType)
		{
			case CDataFileMgr::OVERLAY_INFO_FILE:
			EXTRACONTENT.UnloadOverlayInfo(file.m_filename);
			break;
			case CDataFileMgr::TEXTFILE_METAFILE:
			{
				TheText.RemoveExtracontentTextFromMetafile(file.m_filename);
			}
			break;
			case CDataFileMgr::NM_TUNING_FILE:
			{
				CTuningManager::RevertPhysicsTasks(file.m_filename);
			}
			break;

			case CDataFileMgr::EXTRA_FOLDER_MOUNT_DATA:
			{
				CMountableContent::UnloadExtraFolderMountData(file.m_filename);
			}
			break;

			case CDataFileMgr::VEHICLE_SHOP_DLC_FILE:
			{
				EXTRAMETADATAMGR.RemoveVehiclesCollection(file.m_filename);
			}
			break;

 			case CDataFileMgr::MOVE_NETWORK_DEFS:
			{
 				fwAnimDirector::UnloadNetworkDefinitions(file.m_filename);
 			}
			break;

			default: Errorf("CExtraContentFileMounter::UnloadDataFile Invalid file type! %s [%i]", file.m_filename, file.m_fileType); break;
		}
	}
} g_extraContentFileMounter, g_extraContentFileMounterToRegisterUnload;

void CExtraContentWrapper::Init(u32 initMode)
{
	if (initMode == INIT_CORE && !CExtraContentManagerSingleton::IsInstantiated())
		CExtraContentManagerSingleton::Instantiate();

	if (CExtraContentManagerSingleton::IsInstantiated())
		EXTRACONTENT.Init(initMode);
}

void CExtraContentWrapper::Shutdown(u32 shutdownMode)
{
	if (CExtraContentManagerSingleton::IsInstantiated())
	{
		EXTRACONTENT.ShutdownMetafiles(shutdownMode, eDFMI_UnloadLast);
		EXTRACONTENT.Shutdown(shutdownMode);

		if (shutdownMode == SHUTDOWN_CORE)
			CExtraContentManagerSingleton::Destroy();
	}
}

void CExtraContentWrapper::ShutdownStart(u32 shutdownMode)
{
	if (CExtraContentManagerSingleton::IsInstantiated())
	{
		EXTRACONTENT.ShutdownMetafiles(shutdownMode, eDFMI_UnloadFirst);
		EXTRACONTENT.ShutdownMapChanges(shutdownMode);
	}
}

void CExtraContentWrapper::Update()
{
	PF_START_TIMEBAR("extra content");
	{
		EXTRACONTENT.Update();
		DOWNLOADABLETEXTUREMGR.Update();
	}
}

void CExtraContentManager::LoadOverlayInfo(const char* fileName)
{
	sOverlayInfos infos;
	if(PARSER.LoadObject(fileName,"",infos))
	{
		for(int idx=0;idx<infos.m_overlayInfos.GetCount();idx++)
		{
			dlcDebugf3("overlay info: %s %s", infos.m_overlayInfos[idx].m_nameId.TryGetCStr(), infos.m_overlayInfos[idx].m_changeSet.TryGetCStr());
			UpdateOverlayInfo(infos.m_overlayInfos[idx],fileName);
		}
	}
}

void CExtraContentManager::UnloadOverlayInfo(const char* fileName)
{
	sOverlayInfos infos;
	if(PARSER.LoadObject(fileName,"",infos))
	{
		for(int idx=0;idx<infos.m_overlayInfos.GetCount();idx++)
		{
			dlcDebugf3("overlay info: %s", infos.m_overlayInfos[idx].m_changeSet.TryGetCStr());
			RemoveOverlayInfo(infos.m_overlayInfos[idx]);
		}
	}
}

void CExtraContentManager::RemoveOverlayInfo(sOverlayInfo &overlayInfo)
{
	sOverlayInfo* toStart = NULL;
	sOverlayInfo* currentOverlay = NULL;
	for(int i=0;i<m_overlayInfo.GetCount();i++)
	{
		currentOverlay = m_overlayInfo[i];
		if(currentOverlay->m_nameId == overlayInfo.m_nameId)
		{
			if(currentOverlay->m_changeSet == overlayInfo.m_changeSet)
			{
				if (currentOverlay->m_state == sOverlayInfo::ACTIVE)
				{
					CMountableContent* content = GetContentByHash(currentOverlay->m_content);
					Assertf(content, "Mountable content doesn't exist!");
					RevertContentChangeSet(content->GetNameHash(),currentOverlay->m_changeSetGroupToExecuteWith,currentOverlay->m_changeSet);
				}
				m_overlayInfo.Delete(i--);
				delete currentOverlay;
			}
			else
			{
				if(toStart)
				{
					toStart = toStart->m_version < currentOverlay->m_version ? currentOverlay : toStart; 
				}
				else
				{
					toStart = currentOverlay;
				}
			}
		}
	}
	if(toStart && toStart->m_state != sOverlayInfo::ACTIVE)
	{
		toStart->m_state = sOverlayInfo::WILL_ACTIVATE;
	}

}

void CExtraContentManager::UpdateOverlayInfo(sOverlayInfo &overlayInfo, const char* fileName)
{
	char device[RAGE_MAX_PATH];
	memset(device,0,RAGE_MAX_PATH);
	sOverlayInfo* highestVersion = &overlayInfo;
	for(int i=0;i<m_overlayInfo.GetCount();i++)
	{
		sOverlayInfo*& currentInfo = m_overlayInfo[i];
		if(currentInfo->m_nameId == overlayInfo.m_nameId)
		{
			if(highestVersion->m_version>currentInfo->m_version)
			{
				CMountableContent* content = GetContentByHash(currentInfo->m_content);
				Assertf(content, "Mountable content doesn't exist!");
				if(currentInfo->m_state == sOverlayInfo::ACTIVE)
				{
					RevertContentChangeSet(content->GetNameHash(),currentInfo->m_changeSetGroupToExecuteWith,currentInfo->m_changeSet);
				}
				currentInfo->m_state = sOverlayInfo::INACTIVE;
			}
			else
			{
				highestVersion = m_overlayInfo[i];
			}
		}
	}
	if(highestVersion->m_state != sOverlayInfo::ACTIVE)
		highestVersion->m_state = sOverlayInfo::WILL_ACTIVATE;
	strncpy(device,fileName,static_cast<int>(strcspn(fileName,"/"))+1);
	overlayInfo.m_content = GetContentByDevice(device)->GetNameHash();
	Assertf(overlayInfo.m_content!=0,"Content not found for overlaid file!");
	dlcDisplayf("New overlayinfo, versioned changeset: %s, content changeset: %s, content: %s, version: %d", overlayInfo.m_nameId.TryGetCStr(), overlayInfo.m_changeSet.TryGetCStr(), GetContentByHash(overlayInfo.m_content)->GetName(),overlayInfo.m_version);
	dlcDisplayf("Highest version of the changeset for versioned changeset: %s,  content changeset: %s, content: %s, version: %d", overlayInfo.m_nameId.TryGetCStr(), overlayInfo.m_changeSet.TryGetCStr(), GetContentByHash(overlayInfo.m_content)->GetName(),overlayInfo.m_version);
	m_overlayInfo.PushAndGrow(rage_new sOverlayInfo(overlayInfo));	
}
void CExtraContentManager::ExecuteContentChangeSetGroupInternal(CMountableContent* content, atHashString changeSetGroup)
{
	if(content)
	{
		dlcDisplayf("Executing changeset group %s for %s",changeSetGroup.TryGetCStr(),content->GetName());
		atArray<atHashString> changeSets;
		content->GetChangeSetHashesForGroup(changeSetGroup,changeSets);
		for(int i=0;i<changeSets.GetCount();i++)
		{
			ExecuteContentChangeSetInternal(content,changeSetGroup,changeSets[i], ECCS_FLAG_USE_LATEST_VERSION | ECCS_FLAG_USE_LOADING_SCREEN);
		}

		CMountableContent::CleanupAfterMapChange();
	}
}
void CExtraContentManager::RevertContentChangeSetGroupInternal(CMountableContent* content, atHashString changeSetGroup, u32 actionMask)
{
	if(content)
	{
		dlcDisplayf("Reverting changeset group %s for %s",changeSetGroup.TryGetCStr(),content->GetName());
		atArray<atHashString> changeSets;
		content->GetChangeSetHashesForGroup(changeSetGroup,changeSets);
		for(int i=0;i<changeSets.GetCount();i++)
		{
			RevertContentChangeSetInternal(content,changeSetGroup,changeSets[i],actionMask, ECCS_FLAG_USE_LATEST_VERSION | ECCS_FLAG_USE_LOADING_SCREEN);
		}
	}
}

void CExtraContentManager::TriggerLoadingScreen(LoadingScreenContext loadingContext=LOADINGSCREEN_CONTEXT_MAPCHANGE)
{
	if (!m_loadingScreenState.m_modified)
	{
		m_loadingScreenState.m_modified = true;

		if (!CLoadingScreens::AreActive() DURANGO_ONLY(&& gRenderThreadInterface.IsUsingDefaultRenderFunction()) REPLAY_ONLY(&& !CVideoEditorPlayback::IsLoading()))
		{
			// Catch edge case where movie is still active.
			CLoadingScreens::Shutdown(SHUTDOWN_CORE);
			CLoadingScreens::Update();

			CLoadingScreens::Init(loadingContext, 0);
			CLoadingScreens::CommitToMpSp();
			strStreamingEngine::GetLoader().CallKeepAliveCallback();
			m_loadingScreenState.m_displayed = true;

			gRenderThreadInterface.Flush(true);
		}
	}
}

void CExtraContentManager::CloseLoadingScreen()
{
	if (m_loadingScreenState.m_modified)
	{
		if (m_loadingScreenState.m_displayed)
		{
			// Give the game one frame to gather streaming requests from the load
			if (m_loadingScreenState.m_doneAPostFrame)
			{
				if (strStreamingEngine::GetInfo().GetNumberObjectsRequested() <= 0)
				{
					CLoadingScreens::Shutdown(SHUTDOWN_CORE);
					m_loadingScreenState.Reset();
				}
			}
			else
				m_loadingScreenState.m_doneAPostFrame = true;
		}
		else
			m_loadingScreenState.Reset();
	}
}

void CExtraContentManager::ExecuteContentChangeSetInternal(
	CMountableContent* content, atHashString changeSetGroup, atHashString changeSetName, u32 flags DURANGO_ONLY(, bool addPending/*=true*/))
{
	BANK_ONLY(bool tempValue = strStreamingInfoManager::ms_bValidDlcOverlay;)
	sOverlayInfo * info = NULL;
	if(flags & ECCS_FLAG_USE_LATEST_VERSION)
	{
		atHashString versionedChange = GetVersionedChangeSetName(m_overlayInfo,changeSetName);
		Displayf("Executing changeset %s in group %s for %s",changeSetName.TryGetCStr(),changeSetGroup.TryGetCStr(),content->GetName());
		if(versionedChange != 0)
		{
			Displayf("This is a versioned changeset: %s",versionedChange.TryGetCStr());
			info = GetHighestOverlayInfoForVersionedChange(m_overlayInfo,versionedChange);
			if(info->m_state != sOverlayInfo::ACTIVE)
			{
				content = GetContentByHash(info->m_content);
				changeSetName = info->m_changeSet;
				changeSetGroup = info->m_changeSetGroupToExecuteWith;
				BANK_ONLY(strStreamingInfoManager::ms_bValidDlcOverlay = true;)
					info->m_state = sOverlayInfo::ACTIVE;
				Displayf("Executing highest version of the changeset %s in group %s in %s",changeSetName.TryGetCStr(),changeSetGroup.TryGetCStr(),content->GetName());
			}
			else
			{
				//already executed
				return;
			}
		}
		else if(changeSetGroup == CCS_GROUP_MAP || changeSetGroup == CCS_GROUP_MAP_SP)
		{
			Assertf(0, "Map changesets should be versioned, Content: %s", content->GetName());
#if !__NO_OUTPUT
			Quitf("Map changesets should be versioned, Content: %s", content->GetName());
#endif // !__NO_OUTPUT
		}
	}

#if RSG_DURANGO
	// Because XB1 doesn't have a persistent back buffer, we need to wait for it to copy out of ESRAM
	if (addPending && (changeSetGroup == CCS_GROUP_MAP || changeSetGroup == CCS_GROUP_MAP_SP))
	{
		if (const CDataFileMgr::ContentChangeSet* csPtr = DATAFILEMGR.GetContentChangeSet(changeSetName))
		{
			// If we need a loading screen and we're not already showing one then prep for last frame
			if (csPtr->m_requiresLoadingScreen && csPtr->m_loadingScreenContext == LOADINGSCREEN_CONTEXT_LAST_FRAME &&
				gRenderThreadInterface.IsUsingDefaultRenderFunction() && !CLoadingScreens::AreActive() REPLAY_ONLY(&& !CVideoEditorPlayback::IsLoading()))
			{
				m_pendingActions.PushAndGrow(SPendingCSAction(content->GetNameHash(), changeSetGroup, changeSetName, flags, 0, true, true));

				if (!m_waitingForBBCopy)
				{
					CPauseMenu::TogglePauseRenderPhases(false, OWNER_LOADING_SCR, __FUNCTION__ );
					m_waitingForBBCopy = true;
				}

				if (info)
					info->m_state = sOverlayInfo::INACTIVE;

				return;
			}
		}
	}
#endif

	if(flags & ECCS_FLAG_USE_LOADING_SCREEN)
	{
		// Check to see if we should trigger the loading screen
		if (const CDataFileMgr::ContentChangeSet* csPtr = DATAFILEMGR.GetContentChangeSet(changeSetName))
		{
			if (csPtr->m_requiresLoadingScreen)
				TriggerLoadingScreen(csPtr->m_loadingScreenContext);
		}
	}
	content->ExecuteContentChangeSet(changeSetGroup,changeSetName);

	if (flags & ECCS_FLAG_MAP_CLEANUP)
	{
		CMountableContent::CleanupAfterMapChange();
	}

	BANK_ONLY(strStreamingInfoManager::ms_bValidDlcOverlay = tempValue;)
}
void CExtraContentManager::RevertContentChangeSetInternal(
	CMountableContent* content, atHashString changeSetGroup, atHashString changeSetName, u32 actionMask, u32 flags DURANGO_ONLY(, bool addPending/*=true*/))
{
	BANK_ONLY(bool tempValue = strStreamingInfoManager::ms_bValidDlcOverlay;)
	sOverlayInfo * info = NULL;
	if(flags & ECCS_FLAG_USE_LATEST_VERSION)
	{
		atHashString versionedChange = GetVersionedChangeSetName(m_overlayInfo,changeSetName);
		Displayf("Reverting changeset %s in group %s for %s",changeSetName.TryGetCStr(),changeSetGroup.TryGetCStr(),content->GetName());
		if(versionedChange != 0)
		{
			Displayf("This is a versioned changeset: %s",versionedChange.TryGetCStr());
			info = GetHighestOverlayInfoForVersionedChange(m_overlayInfo,versionedChange);
			if(info->m_state == sOverlayInfo::ACTIVE)
			{
				content = GetContentByHash(info->m_content);
				changeSetName = info->m_changeSet;
				changeSetGroup = info->m_changeSetGroupToExecuteWith;
				BANK_ONLY(strStreamingInfoManager::ms_bValidDlcOverlay = true;)
					info->m_state = sOverlayInfo::INACTIVE;
				Displayf("Reverting highest version of the changeset %s in group %s in %s",changeSetName.TryGetCStr(),changeSetGroup.TryGetCStr(),content->GetName());
			}
			else
			{
				//already reverted
				return;
			}
		}
		else if(changeSetGroup == CCS_GROUP_MAP || changeSetGroup == CCS_GROUP_MAP_SP)
		{
			Assertf(0, "Map changesets should be versioned, Content: %s", content->GetName());
#if !__NO_OUTPUT
			Quitf("Map changesets should be versioned, Content: %s", content->GetName());
#endif // !__NO_OUTPUT
		}
	}

#if RSG_DURANGO
	// Because XB1 doesn't have a persistent back buffer, we need to wait for it to copy out of ESRAM
	if (addPending && (changeSetGroup == CCS_GROUP_MAP || changeSetGroup == CCS_GROUP_MAP_SP))
	{
		if (const CDataFileMgr::ContentChangeSet* csPtr = DATAFILEMGR.GetContentChangeSet(changeSetName))
		{
			// If we need a loading screen and we're not already showing one then prep for last frame
			if (csPtr->m_requiresLoadingScreen && csPtr->m_loadingScreenContext == LOADINGSCREEN_CONTEXT_LAST_FRAME &&
				gRenderThreadInterface.IsUsingDefaultRenderFunction() && !CLoadingScreens::AreActive() REPLAY_ONLY(&& !CVideoEditorPlayback::IsLoading()))
			{
				m_pendingActions.PushAndGrow(SPendingCSAction(content->GetNameHash(), changeSetGroup, changeSetName, flags, actionMask, false, true));

				if (!m_waitingForBBCopy)
				{
					CPauseMenu::TogglePauseRenderPhases(false, OWNER_LOADING_SCR, __FUNCTION__ );
					m_waitingForBBCopy = true;
				}

				if (info)
					info->m_state = sOverlayInfo::ACTIVE;

				return;
			}
		}
	}
#endif

	if(flags & ECCS_FLAG_USE_LOADING_SCREEN)
	{
		if ((changeSetGroup == CCS_GROUP_MAP) || (changeSetGroup == CCS_GROUP_MAP_SP))
		{
			audNorthAudioEngine::PurgeInteriors();
		}

		// Check to see if we should trigger the loading screen
		if (const CDataFileMgr::ContentChangeSet* csPtr = DATAFILEMGR.GetContentChangeSet(changeSetName))
		{
			if (csPtr->m_requiresLoadingScreen)
				TriggerLoadingScreen(csPtr->m_loadingScreenContext);
		}
	}

	content->RevertContentChangeSet(changeSetGroup,changeSetName,actionMask);

	BANK_ONLY(strStreamingInfoManager::ms_bValidDlcOverlay = tempValue; )
}

bool CExtraContentManager::AreAnyCCSPending()
{
	// If we have pending actions, or if we're still showing a loading screen for some actions, we're pending
	return DURANGO_ONLY(m_pendingActions.GetCount() > 0 || ) (m_loadingScreenState.m_displayed && CLoadingScreens::AreActive());
}

atHashString CExtraContentManager::GetVersionedChangeSetName(atArray<sOverlayInfo*>& overlayInfos, atHashString changeSetName)
{	
	for(int i=0;i<overlayInfos.GetCount();i++)
	{
		if(changeSetName == overlayInfos[i]->m_changeSet)
		{
			return overlayInfos[i]->m_nameId;
		}
	}
	return atHashString::Null();
}
sOverlayInfo* CExtraContentManager::GetHighestOverlayInfoForVersionedChange(atArray<sOverlayInfo*>& overlayInfos, atHashString versionedChange)
{
	sOverlayInfo* retval = NULL;
	for(int i=0;i<overlayInfos.GetCount();i++)
	{
		if(overlayInfos[i]->m_nameId == versionedChange)
		{
			if(retval)
			{
				if(overlayInfos[i]->m_version > retval->m_version)
				{
					retval = overlayInfos[i];					
				}
			}
			else
			{
				retval = overlayInfos[i];
			}
		}
	}
	return retval;
}
void CExtraContentManager::ExecutePendingOverlays()
{
	BANK_ONLY(bool tempValue = strStreamingInfoManager::ms_bValidDlcOverlay;)
	BANK_ONLY(strStreamingInfoManager::ms_bValidDlcOverlay = true;)
	for(int i=0;i<m_overlayInfo.GetCount();i++)
	{
		sOverlayInfo*& overlayInfo = m_overlayInfo[i];
		CMountableContent* content = GetContentByHash(overlayInfo->m_content);
		if(Verifyf(content,"Content not found"))
		{
			if(overlayInfo->m_changeSetGroupToExecuteWith == CCS_GROUP_ON_DEMAND)
			{
				if(overlayInfo->m_state==sOverlayInfo::WILL_ACTIVATE )
				{
					Assertf(content, "Mountable content doesn't exist!");
					dlcDisplayf("[%s]Activating %s : %s VERSION: %d", content->GetName(),overlayInfo->m_changeSet.TryGetCStr(),overlayInfo->m_nameId.TryGetCStr(), overlayInfo->m_version);
					if(overlayInfo->m_content)
					{
						ExecuteContentChangeSet(content->GetNameHash(),(u32)CCS_GROUP_ON_DEMAND,overlayInfo->m_changeSet);
						overlayInfo->m_state = sOverlayInfo::ACTIVE;
					}
				}
			#if __BANK
				else
				{
					dlcDisplayf("Changesets execution suppressed for %s in content %s, a higher version is available/executed ",overlayInfo->m_nameId.TryGetCStr(), content->GetName() );
				}
			#endif // __BANK
			}
		}
	}
	BANK_ONLY(strStreamingInfoManager::ms_bValidDlcOverlay = tempValue;)
}
#if EC_CLOUD_MANIFEST
CExtraContentCloudListener::CExtraContentCloudListener()
	: m_manifestReqID(INVALID_CLOUD_REQUEST_ID)
	//, m_bParsingCloud(false)
{	
}
CExtraContentCloudListener::~CExtraContentCloudListener()
{
	CloudManager::GetInstance().RemoveListener(this);
}

void CExtraContentCloudListener::Init()
{
	//CloudManager::GetInstance().AddListener(this);
}

void CExtraContentCloudListener::Shutdown()
{
	//CloudManager::GetInstance().RemoveListener(this);
}

bool CExtraContentCloudListener::DoManifestRequest()
{
	if(CloudManager::GetInstance().IsRequestActive(m_manifestReqID))
	{
		return false;
	}
	dlcDebugf3("DLC: Requesting cloud manifest");

	const char *manifestPath;
	if(sysAppContent::IsJapaneseBuild())
		manifestPath = "extraContent/ExtraContentManifest_jpn.xml";
	else // all other regions
		manifestPath = "extraContent/ExtraContentManifest.xml";

	EXTRACONTENT.SetCloudManifestState(CLOUDFILESTATE_CACHING);
	m_manifestReqID = CloudManager::GetInstance().RequestGetTitleFile(manifestPath,10,eRequest_CacheAddAndEncrypt|eRequest_AlwaysQueue|eRequest_Critical);
	return true;
}

void CExtraContentManager::BeginEnumerateCloudContent()
{
	// We'll first check the master file that knows about all available content.
	m_cloudTimedOut = false;
#if !__FINAL
	if(PARAM_extracloudmanifest.Get())
		LoadCloudManifestFromCommandLine();
	else
		if(!PARAM_disableExtraCloudContent.Get())
#endif // !__FINAL
		{
			m_cloudListener.DoManifestRequest();
		}

		return;
}

void CExtraContentManager::UpdateCloudStorage()
{
	if (!NetworkInterface::IsCloudAvailable())
	{
		//if the cloud isn't available clear the TRANSFER_ERROR state. lavalley / bektas
		if (m_cloudData.m_CloudManifest.m_State == CLOUDFILESTATE_TRANSFER_ERROR)
		{
			m_cloudData.m_CloudManifest.m_State = CLOUDFILESTATE_NOT_FOUND;
		}

		return;
	}

#if !__FINAL
	if(!PARAM_disableExtraCloudContent.Get())
#endif
	{
		if(m_cloudTimedOut)
		{	
			if(m_cloudListener.DoManifestRequest())
			{
				m_cloudTimedOut = false;
			}
		}
	}

	// TODO: Move somewhere else.
	UpdateCloudCacher();
}

#if !__FINAL

void CExtraContentManager::DebugDumpCloudManifestContents()
{
	// Let's see what files there are.

	for (int i = 0; i < m_cloudData.m_CloudManifest.m_Files.GetCount(); i++)
	{
		dlcDisplayf("ExtraContent CloudManifest: FILE: %s", m_cloudData.m_CloudManifest.m_Files[i].m_File.c_str());
	}

	for (int i = 0; i < m_cloudData.m_CloudManifest.m_CompatibilityPacks.GetCount(); i++)
	{
		const CExtraContentCloudPackDescriptor &packDesc = m_cloudData.m_CloudManifest.m_CompatibilityPacks[i];
		dlcDisplayf("ExtraContent CloudManifest: Compatibility pack: '%s', product id: '%s'", packDesc.m_Name.c_str(), packDesc.m_ProductId.c_str());
	}

	for (int i = 0; i < m_cloudData.m_CloudManifest.m_PaidPacks.GetCount(); i++)
	{
		const CExtraContentCloudPackDescriptor &packDesc = m_cloudData.m_CloudManifest.m_PaidPacks[i];
		dlcDisplayf("ExtraContent CloudManifest: Paid pack: '%s', product id: '%s'", packDesc.m_Name.c_str(), packDesc.m_ProductId.c_str());
	}
}

#endif // !__FINAL

void CExtraContentManager::UpdateCloudCacher()
{
	if (!NetworkInterface::IsCloudAvailable())
		return;
	if (Verifyf(fiCachePartition::IsAvailable(), "No cache partition available - we will not be able to use data from the cloud"))
	{
		for (int x=0; x < m_cloudData.m_CloudManifest.m_Files.GetCount(); x++)
		{
			CCloudStorageFile &file = m_cloudData.m_CloudManifest.m_Files[x];
			switch (file.m_State)
			{
			case CLOUDFILESTATE_UNCACHED:
				// If there's nothing else going on, let's start a new transfer.
				if (m_cloudData.m_CloudTransfers == 0)
				{
					dlcDebugf1("Beginning cloud transfer of %s to the cache partition", file.m_File.c_str());

					// Create our cache directory, in case it doesn't exist already.
					char targetDirectory[RAGE_MAX_PATH];
					formatf(targetDirectory, "%stitleStorage", fiCachePartition::GetCachePrefix());

					const fiDevice *targetDevice = fiDevice::GetDevice(targetDirectory, false);

					if (Verifyf(targetDevice, "Can't find device to manage cache partition at %s", targetDirectory))
					{
						// Create the directory - don't check the return value, the directory is likely to exist already.
						targetDevice->MakeDirectory(targetDirectory);

						// Create the target file.
						char targetFilename[RAGE_MAX_PATH];
						formatf(targetFilename, "%s/%s.rpf", targetDirectory, file.m_File.c_str());

						file.m_LocalPath = targetFilename;

						char titlePath[RAGE_MAX_PATH];
#if !__FINAL
						formatf(titlePath, "extraContent/%s%s.rpf", file.m_File.c_str(), !PARAM_useDebugCloudPatch.Get()? "" :file.m_File!=m_cloudData.m_CloudManifest.m_ScriptPatchName ? "" : "_DBG");
#else // !__FINAL
						formatf(titlePath, "extraContent/%s.rpf", file.m_File.c_str());
#endif

						m_cloudData.m_CloudTransferHandle = targetDevice->Create(targetFilename);

						if (Verifyf(fiIsValidHandle(m_cloudData.m_CloudTransferHandle), "Cannot create cached file %s", targetFilename))
						{
							dlcDebugf1("Begin transfer to %s", targetFilename);

							if (Verifyf(rlCloud::GetTitleFile(titlePath,
								RLROS_SECURITY_DEFAULT,
								0,  //ifModifiedSincePosixTime
								targetDevice,
								m_cloudData.m_CloudTransferHandle,
								NULL,   //rlCloudFileInfo
								NULL,   //allocator
								&m_cloudData.m_CloudTransferStatus),
								"Error requesting cloud transfer from %s to %s", titlePath, targetFilename))
							{
								m_cloudData.m_CloudTransfers++;
								file.m_State = CLOUDFILESTATE_CACHING;
								file.m_lastRequestTime = fwTimer::GetSystemTimeInMilliseconds();
							}
							else
							{
								targetDevice->Close(m_cloudData.m_CloudTransferHandle);
								file.m_State =CLOUDFILESTATE_TRANSFER_ERROR;
							}
						}
					}
				}
				break;

			case CLOUDFILESTATE_CACHING:
				// Are we done yet?
				{
					netStatus::StatusCode status = m_cloudData.m_CloudTransferStatus.GetStatus();
					if (status != netStatus::NET_STATUS_PENDING && status != netStatus::NET_STATUS_NONE)
					{
						dlcDebugf1("Cloud transfer for %s finished, result=%d", file.m_File.c_str(), status);

						// We are.
						m_cloudData.m_CloudTransfers--;

						const fiDevice *device = fiDevice::GetDevice(file.m_LocalPath.c_str(), false);
						device->Close(m_cloudData.m_CloudTransferHandle);

						if (status == netStatus::NET_STATUS_SUCCEEDED)
						{
							// And it worked!
							file.m_State = CLOUDFILESTATE_CACHED;

							// Next up - mount it and make it available to the game.
							file.m_Packfile = rage_new fiPackfile();

							if (Verifyf(file.m_MountPoint.c_str(), "Cloud DLC %s doesn't specify a mount point in the manifest file", file.m_File.c_str()))
							{
								if (Verifyf(file.m_Packfile->Init(file.m_LocalPath.c_str(), true, fiPackfile::CACHE_NONE), "Error mounting cloud RPF file %s", file.m_LocalPath.c_str()))
								{
									if (Verifyf(file.m_Packfile->MountAs(file.m_MountPoint.c_str()), "Error mounting %s to %s", file.m_LocalPath.c_str(), file.m_MountPoint.c_str()))
									{
										if (!IsContentFilenamePresent(file.m_MountPoint.c_str()))
										{
											// Now add it to our official list of DLC.
											CMountableContent mount;

											mount.SetFilename(file.m_MountPoint.c_str());
											mount.SetNameHash(file.m_File.c_str());
											mount.SetPrimaryDeviceType(CMountableContent::DT_FOLDER);

											AddContent(mount);
											LoadContent(false, false);
										}
									}
								}
							}
						}
						else
						{
							// An error occurred.
							int result = m_cloudData.m_CloudTransferStatus.GetResultCode();
							if(result == 404)
							{
								dlcWarningf("Could not find the file on the cloud : %s", file.m_File.c_str());
								file.m_State = CLOUDFILESTATE_NOT_FOUND;
							}
							else
							{
								dlcWarningf("ERROR: %d , Could not download %s from the cloud - giving up",result, file.m_File.c_str());
								file.m_State = CLOUDFILESTATE_TRANSFER_ERROR;
							}
						}
					}
				}
				break;

			case CLOUDFILESTATE_CACHED:
				// It's cached. That's cool.
				break;

			case CLOUDFILESTATE_NOT_FOUND:
				break;
			case CLOUDFILESTATE_TRANSFER_ERROR:
				// We couldn't download this file. Let's not try again.
				// TODO: We could have some retry logic here, where we first try again after a few weconds, then after a few minutes, or
				// whatever. But for now, let's sit still.
				for (int x=0; x < m_cloudData.m_CloudManifest.m_Files.GetCount(); x++)
				{
					CCloudStorageFile& file = m_cloudData.m_CloudManifest.m_Files[x];
					if(fwTimer::GetSystemTimeInMilliseconds() - file.m_lastRequestTime > 15000)
					{
						file.m_State = CLOUDFILESTATE_UNCACHED;
					}
				}
				break;
			}
		}
	}
}

void CExtraContentManager::OnReceivedExtraContentManifest()
{

}

void CExtraContentCloudListener::OnCloudEvent( const CloudEvent* pEvent )
{	
	if(!pEvent)
	{
		return;
	}
	switch(pEvent->GetType())
	{
		case CloudEvent::EVENT_REQUEST_FINISHED:
		{
			const CloudEvent::sRequestFinishedEvent* pEventData = pEvent->GetRequestFinishedData();
			if( !pEventData )
			{
				return;
			}

			if(pEventData->nRequestID == m_manifestReqID)
			{
				// clear the manifest ID
				m_manifestReqID = INVALID_CLOUD_REQUEST_ID;

				if(pEventData->bDidSucceed)
				{
					if(pEventData->pData)
					{
						//handle manifest
						dlcDebugf3("Extra Content manifest successfully downloaded!");
						const void* const & data = pEventData->pData;
						u32 bSize = pEventData->nDataSize;
						Displayf("%s loaded ", pEventData->szFileName);
						BANK_ONLY(if(!PARAM_extracloudmanifest.Get()))
							EXTRACONTENT.SetCloudManifest(data,bSize);
					}
					else
					{
						Displayf("MANIFEST REQUEST SUCCEEDED - No data!");
						EXTRACONTENT.SetCloudManifestState(CLOUDFILESTATE_TRANSFER_ERROR);
					}
				}
				else
				{
					Displayf("MANIFEST REQUEST UNSUCCESSFUL, Result code :%d", pEventData->nResultCode);

					//handle manifest
					if(pEventData->nResultCode == 404)
					{
						Displayf("Manifest state set to CLOUDFILESTATE_NOT_FOUND");
						EXTRACONTENT.SetCloudManifestState(CLOUDFILESTATE_NOT_FOUND);
					}
					else
					{
						Displayf("Manifest state set to CLOUDFILESTATE_TRANSFER_ERROR");
						EXTRACONTENT.SetCloudManifestState(CLOUDFILESTATE_TRANSFER_ERROR);
					}
				}
			}
		}
		break;
		case CloudEvent::EVENT_AVAILABILITY_CHANGED:
		{
			const CloudEvent::sAvailabilityChangedEvent* pEventData = pEvent->GetAvailabilityChangedData();

			// if we now have cloud and don't have the cloud file, request it
			if(pEventData->bIsAvailable BANK_ONLY(&& !PARAM_extracloudmanifest.Get()))
			{
				dlcDebugf1("Availability Changed - Requesting Cloud File");
				DoManifestRequest(); 
			}
		}
		break;
	}
}


bool CExtraContentManager::LoadCloudManifestFromCommandLine()
{
	if(PARAM_extracloudmanifest.Get())
	{	
		const char* pPath=NULL;
		PARAM_extracloudmanifest.Get(pPath);

		if(PARSER.LoadObject(pPath, "xml", m_cloudData.m_CloudManifest))
		{
			m_cloudData.m_CloudManifest.m_State = CLOUDFILESTATE_CACHED ;
#if !__FINAL
			DebugDumpCloudManifestContents();
#endif
			return true;
		}
		else
		{
			Errorf("Can't load %s manifest specified in -extracloudmanifest", pPath);
			m_cloudData.m_CloudManifest.m_State = CLOUDFILESTATE_NOT_FOUND;
			return false;
		}
	}
	return false;
}

bool CExtraContentManager::HasCloudContentSuccessfullyLoaded()
{
#if __BANK
	if(EXTRACONTENT.BANK_EnableCloudWidgetsOverride())
	{
		return EXTRACONTENT.BANK_GetForceCloudFileState();
	}
#endif // __BANK

#if !__FINAL	
	if(!PARAM_disableExtraCloudContent.Get())
#endif
	{
		if(m_cloudData.m_CloudManifest.m_State == CLOUDFILESTATE_NOT_FOUND )
		{
			return true;
		}
		if(m_cloudData.m_CloudManifest.m_State == CLOUDFILESTATE_CACHED)
		{
			for(int i=0; i<m_cloudData.m_CloudManifest.m_Files.GetCount();++i)
			{
				eCloudFileState& fileState = m_cloudData.m_CloudManifest.m_Files[i].m_State;
				Displayf("File state: %d",fileState);
				//Handle transfer error?
				if(fileState!=CLOUDFILESTATE_CACHED&&fileState!=CLOUDFILESTATE_NOT_FOUND)
					return false;
			}
		}
		else
		{	
			return false;
		}
	}
	return true;
}

eDlcCloudResult CExtraContentManager::GetCloudFileState(eCloudFileState state)
{
	switch (state)
	{
	case CLOUDFILESTATE_CACHED:
	case CLOUDFILESTATE_NOT_FOUND:
		return DLCRESULT_OK;
		break;
	case CLOUDFILESTATE_CACHING:
	case CLOUDFILESTATE_UNCACHED:
		return DLCRESULT_NOT_READY;
		break;
	case CLOUDFILESTATE_TRANSFER_ERROR:
		return DLCRESULT_CONNECTION_ERROR;			
		break;
	default:
		return DLCRESULT_OK;
	}
}

eDlcCloudResult CExtraContentManager::GetCloudContentResult()
{
	eDlcCloudResult state = DLCRESULT_OK; 
#if !__FINAL
	if(!PARAM_disableExtraCloudContent.Get())
#endif //!__FINAL
	{
		eCloudFileState& manifestState = m_cloudData.m_CloudManifest.m_State;
		if(m_cloudTimedOut)
			return DLCRESULT_TIMEOUT;
		state = GetCloudFileState(manifestState);
		if(state==DLCRESULT_OK)
		{
			for(int i=0;i<m_cloudData.m_CloudManifest.m_Files.GetCount();i++)
			{
				eDlcCloudResult fileState = GetCloudFileState(m_cloudData.m_CloudManifest.m_Files[i].m_State);
				if(fileState != DLCRESULT_OK)
				{
					return fileState;
				}
			}
		}	
	}
	return state;
}

bool CExtraContentManager::GetCloudContentState(int &bTimedOut, int uWaitDuration)
{
	u32 currentTime = sysTimer::GetSystemMsTime();
	bTimedOut = (int)false;
	bool result = GetCloudContentRequestsFinished();
	if(result)
	{
		dlcDebugf1("EXTRACONTENTMANIFEST: State request finished in %d ms", m_cloudStatusTimer);
		m_cloudStatusTimer = 0;
		return result;
	}
	if(m_cloudStatusTimer == 0)
	{
		m_cloudStatusTimer = currentTime;
		dlcDebugf1("EXTRACONTENTMANIFEST: Requesting state");
	}
	if(currentTime - m_cloudStatusTimer > (uWaitDuration > 0 ? uWaitDuration : 30000)  )
	{
		m_cloudStatusTimer = 0;
		bTimedOut = (int)true;
		m_cloudTimedOut = true;
		dlcDebugf1("EXTRACONTENTMANIFEST: Request timed out");
	}

	return result;
}

void CExtraContentManager::EnsureManifestLoaded()
{
	if(GetCloudContentResult()==DLCRESULT_CONNECTION_ERROR)
	{
		m_cloudListener.DoManifestRequest();
	}
}

bool CExtraContentManager::GetCloudContentRequestsFinished()
{
#if __BANK
	if(BANK_EnableCloudWidgetsOverride())
	{
		return BANK_GetForceManifestFileState();
	}
#endif //__BANK
#if !__FINAL
	if(!PARAM_disableExtraCloudContent.Get())
#endif //!__FINAL
	{
		eDlcCloudResult currentState = GetCloudFileState(m_cloudData.m_CloudManifest.m_State);
		if (currentState == DLCRESULT_NOT_READY)
		{
			Displayf("Manifest file not ready!");
			return false;
		}
		for (int x=0; x < m_cloudData.m_CloudManifest.m_Files.GetCount(); x++)
		{
			CCloudStorageFile &file = m_cloudData.m_CloudManifest.m_Files[x];
			currentState = GetCloudFileState(file.m_State);
			Displayf("File state: %d", file.m_State );
			if(currentState == DLCRESULT_NOT_READY)
			{
				Displayf("File not ready! %s",file.m_File.c_str());
				return false;
			}
		}	
	}
	return true;
}

void CExtraContentManager::SetCloudManifestState(eCloudFileState state)
{
	m_cloudData.m_CloudManifest.m_State = state;
}
void CExtraContentManager::SetCloudManifest(const void* const & data, u32 size)
{
	char fileName[RAGE_MAX_PATH];
	fiDevice::MakeMemoryFileName(fileName, sizeof(fileName), data , size, false, "CloudExtraContent");

	if(PARSER.LoadObject(fileName, "", m_cloudData.m_CloudManifest) )
	{
#if !__FINAL
		DebugDumpCloudManifestContents();
#endif
		m_cloudData.m_CloudManifest.m_State = CLOUDFILESTATE_CACHED;
		//RequestCloudFiles();
	}
	else
    {
		dlcDebugf3("Can't load manifest!");

        // match the state change for the command line version
        m_cloudData.m_CloudManifest.m_State = CLOUDFILESTATE_NOT_FOUND;
    }

}
#endif // EC_DO_MANIFEST_CHECKS

void CExtraContentManager::InitialiseSpecialTriggers(void)
{
	m_specialTriggerTunablesTested = false;
	m_specialTriggers = 0;

#if !__FINAL
	if (PARAM_christmas.Get()) 
	{ 
		SetSpecialTrigger(ST_XMAS, true); 
	}
#endif //!__FINAL
}

void CExtraContentManager::UpdateSpecialTriggers(void)
{
	if (m_specialTriggerTunablesTested == false)
	{
		if (Tunables::GetInstance().HasCloudRequestFinished())
		{
			if (!GetSpecialTrigger(ST_XMAS))
			{
				bool bValue =	Tunables::GetInstance().TryAccess(BASE_GLOBALS_HASH, ATSTRINGHASH("TURN_SNOW_ON_OFF", 0xbba8f5b0), false) ||
								Tunables::GetInstance().TryAccess(MP_GLOBAL_HASH, ATSTRINGHASH("TURN_SNOW_ON_OFF", 0xbba8f5b0), false);
				SetSpecialTrigger(ST_XMAS, bValue);
			}

			m_specialTriggerTunablesTested = true;
		}
	}
}

bool CExtraContentManager::GenericChangesetChecks(atHashString condition)
{
	if(atHashString("jpn_build",0xBE885BB1)==condition)
	{
		return sysAppContent::IsJapaneseBuild();
	}
	return true;
}

bool CExtraContentManager::LevelChecks(atHashString condition)
{
    return CScene::GetCurrentLevelNameHash()==condition;

}



void CExtraContentManager::Init(u32 initMode)
{
#if EC_CLOUD_MANIFEST
	m_cloudStatusTimer = 0;
	m_cloudTimedOut = false;
#endif // EC_DO_MANIFEST_CHECKS
	m_everHadBadPackOrder = false;
	m_loadingScreenState.Reset();

#if !__FINAL
	m_enumerateCommandLines = true;
#endif

#if GTA_REPLAY
	ResetReplayState();
#endif // GTA_REPLAY

	if (initMode == INIT_CORE)
	{
#if RSG_DURANGO
		m_waitingForBBCopy = false;
		InitDurangoContent();
#endif

	#if RSG_ORBIS
		Assert(sysAppContent::IsInitialized());

		if (s_contentPollThread == sysIpcThreadIdInvalid)
			s_contentPollThread = sysIpcCreateThread(&CExtraContentManager::ContentPoll, NULL, sysIpcMinThreadStackSize, PRIO_LOWEST, "OrbisAddContPoll");
	#endif

#if RSG_PS3 || RSG_XENON
        if (s_codeCheckThread == sysIpcThreadIdInvalid)
			s_codeCheckThread = sysIpcCreateThread(&CExtraContentManager::CodeCheck, NULL, sysIpcMinThreadStackSize, PRIO_LOWEST, "CodeCheck", 4, "CodeCheck");
#endif

		m_overlayInfo.Reset();
#if EC_CLOUD_MANIFEST
		m_cloudListener.Init();
		m_cloudData.m_CloudManifestFileRequest = NULL;
		m_cloudData.m_CloudTransfers = 0;
#endif // EC_DO_MANIFEST_CHECKS
		m_currentGamerId.Clear();
		m_localGamerIndex = -1;
		m_enumerating = false;
		m_enumerateOnUpdate = false;
		m_currMapChangeState = MCS_NONE;
		m_allowMapChangeAnyTime = true;
		atDelegate<bool (atHashString)>* defaultChecks = rage_new atDelegate<bool (atHashString)>();
		defaultChecks->Bind(this,&CExtraContentManager::GenericChangesetChecks);
		atDelegate<bool (atHashString)>* levelChecks = rage_new atDelegate<bool (atHashString)>();
		levelChecks->Bind(this,&CExtraContentManager::LevelChecks);
		CMountableContent::RegisterExecutionCheck(defaultChecks,atHashString("build",0x59859136));
		CMountableContent::RegisterExecutionCheck(levelChecks,atHashString("level",0xF66D3B99));
		fiCachePartition::Init();
		CDownloadableTextureManager::InitClass();

	#if __XENON
		m_deviceEnumerating = false;
		fiDeviceXContent::InitClass();
		fiDeviceXContent::SetDeviceChangeCallback(MakeFunctor(*this, &CExtraContentManager::DeviceChanged));
		m_downloadContentListener = XNotifyCreateListener(XNOTIFY_LIVE); // create listener to check for content being installed
	#endif

		rlPresence::AddDelegate(&g_PresenceDlgt);

		g_addContentServiceDelegate.Bind(&OnServiceEvent);
		g_SysService.AddDelegate(&g_addContentServiceDelegate);

// CDataFileMount::RegisterMountInterface doesn't make a copy of the CDataFileMountInterface that is passed to it. It only stores a pointer to the CDataFileMountInterface in an array.
//	It also modifies m_registerUnload within the CDataFileMountInterface.
//	For that reason, if you want to call RegisterMountInterface with registerUnload=false then use g_extraContentFileMounter.
//	If you want to call RegisterMountInterface with registerUnload=true then use g_extraContentFileMounterToRegisterUnload.
		CDataFileMount::RegisterMountInterface(CDataFileMgr::CONTENT_UNLOCKING_META_FILE, &g_extraContentFileMounter);
		CDataFileMount::RegisterMountInterface(CDataFileMgr::OVERLAY_INFO_FILE, &g_extraContentFileMounter);
		CDataFileMount::RegisterMountInterface(CDataFileMgr::CLIP_SETS_FILE, &g_extraContentFileMounter);
		CDataFileMount::RegisterMountInterface(CDataFileMgr::EXPRESSION_SETS_FILE, &g_extraContentFileMounter);
		CDataFileMount::RegisterMountInterface(CDataFileMgr::FACIAL_CLIPSET_GROUPS_FILE, &g_extraContentFileMounter);
		CDataFileMount::RegisterMountInterface(CDataFileMgr::NM_BLEND_OUT_SETS_FILE, &g_extraContentFileMounter);
		CDataFileMount::RegisterMountInterface(CDataFileMgr::NM_TUNING_FILE, &g_extraContentFileMounter);
		CDataFileMount::RegisterMountInterface(CDataFileMgr::EXTRA_TITLE_UPDATE_DATA, &g_extraContentFileMounter);
		CDataFileMount::RegisterMountInterface(CDataFileMgr::VEHICLE_SHOP_DLC_FILE, &g_extraContentFileMounter);
		CDataFileMount::RegisterMountInterface(CDataFileMgr::LOADOUTS_FILE, &g_extraContentFileMounter);
		CDataFileMount::RegisterMountInterface(CDataFileMgr::LEVEL_STREAMING_FILE, &g_extraContentFileMounter);
		CDataFileMount::RegisterMountInterface(CDataFileMgr::WORLD_HEIGHTMAP_FILE, &g_extraContentFileMounter);
		CDataFileMount::RegisterMountInterface(CDataFileMgr::WATER_FILE, &g_extraContentFileMounter);
		CDataFileMount::RegisterMountInterface(CDataFileMgr::CARCOLS_GEN9_FILE, &g_extraContentFileMounter);
		CDataFileMount::RegisterMountInterface(CDataFileMgr::CARMODCOLS_GEN9_FILE, &g_extraContentFileMounter);
		CDataFileMount::RegisterMountInterface(CDataFileMgr::GEN9_EXCLUSIVE_ASSETS_VEHICLES_FILE, &g_extraContentFileMounter);
		CDataFileMount::RegisterMountInterface(CDataFileMgr::GEN9_EXCLUSIVE_ASSETS_PEDS_FILE, &g_extraContentFileMounter);

		CDataFileMount::RegisterMountInterface(CDataFileMgr::GTXD_PARENTING_DATA, &g_extraContentFileMounter);

		CDataFileMount::RegisterMountInterface(CDataFileMgr::MOVE_NETWORK_DEFS, &g_extraContentFileMounterToRegisterUnload, eDFMI_UnloadFirst);
		CDataFileMount::RegisterMountInterface(CDataFileMgr::TEXTFILE_METAFILE, &g_extraContentFileMounterToRegisterUnload, eDFMI_UnloadFirst);
		CDataFileMount::RegisterMountInterface(CDataFileMgr::EXTRA_FOLDER_MOUNT_DATA, &g_extraContentFileMounterToRegisterUnload, eDFMI_UnloadFirst);

		if(fiDevice::GetDevice(TITLE_UPDATE_MOUNT_PATH))
		{
			CMountableContent mount;
			mount.SetFilename("update:/");
			mount.SetNameHash(TITLE_UPDATE_PACK_NAME);
			mount.SetPrimaryDeviceType(CMountableContent::DT_FOLDER);
			mount.SetPermanent(true);
			mount.SetDatFileLoaded(true);
			AddContent(mount);
			DATAFILEMGR.SetEnableFilePatching(true);
			LoadContent(false);
			DATAFILEMGR.SetEnableFilePatching(false);
		}


		InitialiseSpecialTriggers();

	#if __BANK
		InitBank();
	#endif
	}
	else if (initMode == INIT_SESSION)
	{
		ExecuteTitleUpdateDataPatch((u32)CCS_TITLE_UPDATE_DLC_METADATA, true);
		ExecuteTitleUpdateDataPatchGroup((u32)CCS_GROUP_UPDATE_DLC_METADATA, true);
		// When we have loaded the session, process any enumeration requests...
		ProcessEnumerationRequests(true);

		// Hack...force load the 'mph4_gtxd' for the island heist. Later cl should remove this
		// and use the content type tag in content.xml to solve this better
		CMapFileMgr::GetInstance().LoadGlobalTxdParents("mph4_gtxd");

	#if __BANK
		fragTuneStruct::PreSaveEntityFunctor preSaveEntityFunctor;
		preSaveEntityFunctor.Bind(&CExtraContentManager::ReturnAssetPathForFragTuneCB);
		FRAGTUNE->SetPreSaveEntityFunctor(preSaveEntityFunctor);
	#endif // __BANK

	#if !__FINAL
		const char* dlcPacks;
		if (PARAM_buildDLCCacheData.Get(dlcPacks))
		{
			char buffer[RAGE_MAX_PATH] = {0}; 
			strcpy(buffer,dlcPacks);
			char* cur = strtok(buffer,";");
			while(cur)
			{
				atHashString contentHash(cur);
				CMountableContent* content = GetContentByHash(contentHash);
				if(content)
				{
					ExecuteContentChangeSetGroup(contentHash,(u32)CCS_GROUP_MAP_SP);
					RevertContentChangeSetGroup(contentHash,(u32)CCS_GROUP_MAP_SP);

					ExecuteContentChangeSetGroup(contentHash,(u32)CCS_GROUP_MAP);
					RevertContentChangeSetGroup(contentHash,(u32)CCS_GROUP_MAP);
				}
				cur = strtok(NULL,";");
			}			
		}
	#endif

		if (PARAM_loadMapDLCOnStart.Get())
		{
			ExecuteContentChangeSetGroupForAll((u32)CCS_GROUP_MAP_SP);
			ExecuteContentChangeSetGroupForAll((u32)CCS_GROUP_MAP);
		}
		else
		{
			if (!CNetwork::HasMatchStarted())
				ExecuteContentChangeSetGroupForAll((u32)CCS_GROUP_MAP_SP);
		}
	#if __BANK
		const char* ccsToEnable;
		if(PARAM_enableMapCCS.Get(ccsToEnable))
		{
			char buffer[RAGE_MAX_PATH] = {0}; 
			strcpy(buffer,ccsToEnable);
			char* cur = strtok(buffer,";");
			while(cur)
			{
				atHashString contentHash(cur);
				CMountableContent* content = GetContentByHash(contentHash);
				if(content)
					ExecuteContentChangeSetGroup(contentHash,(u32)CCS_GROUP_MAP);
				cur = strtok(NULL,";");
			}			
		}

		if (PARAM_enableSPMapCCS.Get(ccsToEnable))
		{
			char buffer[RAGE_MAX_PATH] = {0}; 
			strcpy(buffer,ccsToEnable);
			char* cur = strtok(buffer,";");
			while(cur)
			{
				atHashString contentHash(cur);
				CMountableContent* content = GetContentByHash(contentHash);
				if(content)
					ExecuteContentChangeSetGroup(contentHash,(u32)CCS_GROUP_MAP_SP);
				cur = strtok(NULL,";");
			}			
		}
	#endif // __BANK
		ExecuteTitleUpdateDataPatchGroup(CCS_GROUP_POST_DLC_PATCH,true);

		CModelIndex::MatchAllModelStrings();
	}
}
void CExtraContentManager::LoadLevelPacks()
{
	DATAFILEMGR.SetEnableFilePatching(true);
	BeginEnumerateContent(true,true);
	RemountUpdate();
	DATAFILEMGR.SetEnableFilePatching(false);
}
void CExtraContentManager::Shutdown(u32 shutdownMode)
{
#if EC_CLOUD_MANIFEST
	m_cloudStatusTimer = 0;
	m_cloudTimedOut = false;
#endif // EC_DO_MANIFEST_CHECKS
	CancelEnumerateContent(true);
	pgStreamer::Drain();
	strStreamingEngine::GetLoader().Flush();
	gRenderThreadInterface.Flush();
	strStreamingEngine::GetInfo().FlushLoadedList(STR_DONTDELETE_MASK);

	for(u32 i=0;i<m_overlayInfo.GetCount();i++)
	{
		delete m_overlayInfo[i];
		m_overlayInfo[i] = NULL;
	}
	m_overlayInfo.Reset();
	for (u32 i = 0; i < m_content.GetCount(); i++)
	{
		if (!m_content[i].GetIsPermanent())
		{
			m_content[i].ShutdownContent();
			m_content.Delete(i);
			i--;
		}
	}
	m_contentLocks.Reset();
	m_corruptedContent.Reset();
#if RSG_ORBIS
	m_ps4DupeEntitlements.Reset();
#endif
#if RSG_DURANGO
	ShutdownDurangoContent();
#endif
	CPlayerSpecialAbilityManager::UpdateDlcMultipliers();

	if (shutdownMode == SHUTDOWN_CORE)
	{
		g_SysService.RemoveDelegate(&g_addContentServiceDelegate);
		g_addContentServiceDelegate.Reset();

		CDownloadableTextureManager::ShutdownClass();
		rlPresence::RemoveDelegate(&g_PresenceDlgt);

	#if RSG_ORBIS
		sysIpcWaitThreadExit(s_contentPollThread);
	#endif

#if RSG_PS3 || RSG_XENON
        s_codeCheckRun = false;
        sysIpcWaitThreadExit(s_codeCheckThread);
#endif
	}
	else if (shutdownMode == SHUTDOWN_SESSION)
	{
		ExecuteTitleUpdateDataPatchGroup((u32)CCS_GROUP_POST_DLC_PATCH,false);
		ExecuteTitleUpdateDataPatch((u32)CCS_TITLE_UPDATE_DLC_METADATA, false);
		ExecuteTitleUpdateDataPatchGroup((u32)CCS_GROUP_UPDATE_DLC_METADATA, false);
	#if __BANK
		Bank_UpdateContentDisplay();
	#endif		
	}

	if (m_currentGamerId.IsValid())
		m_enumerateOnUpdate = true;
}

void CExtraContentManager::ShutdownMetafiles(u32 /*shutdownMode*/, eDFMI_UnloadType unloadType)
{
	CDataFileMount::SetUnloadBehaviour(unloadType);
	{
		for (u32 i = 0; i < m_content.GetCount(); i++)
		{
			if (!m_content[i].GetIsPermanent())
				m_content[i].StartRevertActiveChangeSets();
		}
	}
	CDataFileMount::SetUnloadBehaviour(eDFMI_UnloadDefault);
}

void CExtraContentManager::ShutdownMapChanges(u32 /*shutdownMode*/)
{
	RevertContentChangeSetGroupForAll((u32)CCS_GROUP_MAP_SP);
	RevertContentChangeSetGroupForAll((u32)CCS_GROUP_MAP);

	CMapFileMgr::GetInstance().Cleanup();		// ensure the map file dependency data is cleared up	- bug 6177812
}

void CExtraContentManager::InitProfile()
{
	if (const rlGamerInfo* pGamerInfo = NetworkInterface::GetActiveGamerInfo())
	{
		m_localGamerIndex = pGamerInfo->GetLocalIndex();
		m_currentGamerId = pGamerInfo->GetGamerId();

	#if __XENON
		BeginDeviceEnumeration();
	#endif
	}
	m_enumerateOnUpdate = true;
#if EC_CLOUD_MANIFEST
	m_cloudStatusTimer = 0;
	m_cloudTimedOut = false;
	SetCloudManifestState(CLOUDFILESTATE_UNCACHED);
	BeginEnumerateCloudContent();
#endif // EC_DO_MANIFEST_CHECKS
}

void CExtraContentManager::ShutdownProfile()
{
#if EC_CLOUD_MANIFEST
	SetCloudManifestState(CLOUDFILESTATE_UNCACHED);
	m_cloudStatusTimer = 0;
	m_cloudTimedOut = false;
#endif // EC_DO_MANIFEST_CHECKS
	m_enumerateOnUpdate = false;
	m_localGamerIndex = -1;
	m_currentGamerId.Clear();
	#if __XENON
		fiDeviceXContent::SetLocalIndex(-1);
	#endif
}

void CExtraContentManager::UpdateRebootMessage()
{
	if (fiDeviceInstaller::HasRebooted(EC_REBOOT_ID) && !CLoadingScreens::AreActive())
	{
		bool showMessage = CLoadingScreens::ShowRebootMessage("EC_REBOOT_MSG", EC_REBOOT_ID);

		if (showMessage)
			CWarningScreen::SetWarningMessageInUse();
	}
}

void CExtraContentManager::Update()
{
	bool waitForEnumeration = false;

#if RSG_DURANGO
	m_waitingForBBCopy = false;

	// :(
	for (s32 i = 0; i < m_pendingActions.GetCount(); i++)
	{
		SPendingCSAction& currAction = m_pendingActions[i];
		CMountableContent* content = EXTRACONTENT.GetContentByHash(currAction.m_content);

		if (currAction.m_requiesLastFrame && !CPauseMenu::GetPauseRenderPhasesStatus())
		{
			m_waitingForBBCopy = true;
			continue;
		}

		if (currAction.m_execute)
		{
			if (Verifyf(content, "ProcessPending - Invalid execution contentHash %s", currAction.m_content.GetCStr()))
			{
				ExecuteContentChangeSetInternal(content, currAction.m_group, currAction.m_changeSet, currAction.m_flags, false);
			}
		}
		else
		{
			if (Verifyf(content, "ProcessPending - Invalid reversion contentHash %s", currAction.m_content.GetCStr()))
			{
				RevertContentChangeSetInternal(content, currAction.m_group, currAction.m_changeSet, currAction.m_actions, currAction.m_flags, false);
			}
		}

		m_pendingActions.Delete(i);
		i--;
	}
#endif

	UpdateRebootMessage();
#if EC_CLOUD_MANIFEST
	UpdateCloudStorage();
#endif // EC_DO_MANIFEST_CHECKS
	UpdateMapChangeState();

	UpdateSpecialTriggers();

#if __XENON
	// Check for content installed message and if it has then enumerate content again
	DWORD id;
	ULONG_PTR param;
	if (XNotifyGetNext(m_downloadContentListener, XN_LIVE_CONTENT_INSTALLED, &id, &param))
	{
		OnContentDownloadCompleted();
		waitForEnumeration = true;
	}
#endif // __XENON

	if (m_enumerating)
		EndEnumerateContent(waitForEnumeration,false);

	ProcessEnumerationRequests(false);
	ProcessDeviceChangedMessages();
	CloseLoadingScreen();

#if __BANK
	Bank_ExecuteBankUpdate();
#endif // __BANK
}

void CExtraContentManager::UpdateMapChangeState()
{
#if __BANK
	eMapChangeStates prevState = m_currMapChangeState;
#endif

	switch (m_currMapChangeState)
	{
		case MCS_INIT:
		{
			if (camInterface::IsFadedIn())
				camInterface::FadeOut(MAP_CHANGE_FADE_TIME, true);
			else if (camInterface::IsFadedOut())
				m_currMapChangeState = MCS_UPDATE;
		} break;

		case MCS_UPDATE:
		{

		} break;

		case MCS_END:
		{
			camInterface::FadeIn(MAP_CHANGE_FADE_TIME);
			m_currMapChangeState = MCS_NONE;
		} break;

		default: break;
	}

#if __BANK
	if (prevState != m_currMapChangeState)
		Bank_UpdateMapChangeState();
#endif
}

void CExtraContentManager::ProcessEnumerationRequests(bool immediate)
{
	if (m_enumerateOnUpdate || immediate)
	{
		BeginEnumerateContent(immediate,false);
		m_enumerateOnUpdate = false;
	}
}

enum eDeviceChangedMessage
{
	DC_NONE = 0,
	DC_SUCCESS,
	DC_FAIL
};
sysMessageQueue<eDeviceChangedMessage, 16> s_deviceChangedMessages;

//
// name:		ProcessDeviceChangedMessages
// description:	Main thread function to deal with devices having changed
void CExtraContentManager::ProcessDeviceChangedMessages()
{
	static bool bUnavailableContent = false;
	static bool bShowWarningScreen = false;
	static bool bPaused = false;

	eDeviceChangedMessage msg = DC_NONE;
	while (s_deviceChangedMessages.PopPoll(msg)) {}

	switch (msg)
	{
	case DC_SUCCESS:
		{
			// If content wasn't available before
			if(bUnavailableContent)
			{
				CWarningScreen::Remove();
				bShowWarningScreen = false;
			}

			// check enumerated content to update device ids
			UpdateContentArray();

			bUnavailableContent = false;
			if (bPaused)
			{
				fwTimer::EndUserPause();
				bPaused = false;
			}
		}
		break;
	case DC_FAIL:
		{
			CWarningScreen::SetWarningMessageInUse();
			bShowWarningScreen = true;

			if (!fwTimer::IsGamePaused())
			{
				fwTimer::StartUserPause();
				bPaused = true;
			}
			CControlMgr::StopPlayerPadShaking();

			// If playing network then bail
			if(NetworkInterface::IsNetworkOpen())
			{
				gnetDebug1("Bailing due to Extra content Device Change");
				NetworkInterface::Bail(BAIL_NEW_CONTENT_INSTALLED, BAIL_CTX_NONE);
			}

			bUnavailableContent = true;
		}
		break;
	default:
		break;
	}

	if (bShowWarningScreen)
	{
	#if __XENON
		// If we are showing the device removed warning screen and there is no profile (means they signed out on this screen) reboot...
		if (m_localGamerIndex == -1)
			fiDeviceInstaller::Reboot(EC_REBOOT_ID);
		else
	#endif
			CWarningScreen::SetMessage(WARNING_MESSAGE_STANDARD, "FE_NODLC");
	}
}

//
// name:		DeviceChangedCB
// description:	Function that is called when the storage device changes
void CExtraContentManager::DeviceChanged(bool bSuccess)
{
	if (!bSuccess)
	{
		s_deviceChangedMessages.Push(DC_FAIL);
	}
	else
	{
		s_deviceChangedMessages.Push(DC_SUCCESS);
	}
}

// Orbis calls this from the Polling thread
void CExtraContentManager::OnContentDownloadCompleted()
{
	dlcDebugf1("OnContentDownloadCompleted");

#if __XENON
	CLiveManager::GetCommerceMgr().GetConsumableManager()->SetOwnershipDataDirty();
#endif
	CLiveManager::GetCommerceMgr().ResetAutoConsumeMessage();

	m_enumerateOnUpdate = true;
}

void CExtraContentManager::BeginEnumerateContent(bool immediate, bool earlyStartup)
{
	dlcDebugf1("CExtraContentManager::BeginEnumerateContent");
	if (m_enumerating)
		EndEnumerateContent(true,earlyStartup);

	m_enumerating = true;

#if __XENON
	BeginDeviceEnumeration();
#endif

	if(immediate)
		EndEnumerateContent(true,earlyStartup);

#if EC_CLOUD_MANIFEST
	UpdateCloudStorage();
#endif // EC_DO_MANIFEST_CHECKS
}

#if __XENON
void CExtraContentManager::BeginDeviceEnumeration()
{
	if (m_localGamerIndex != -1)
	{
		fiDeviceXContent::SetLocalIndex(m_localGamerIndex);
		fiDeviceXContent::StartEnumerate();
		m_deviceEnumerating = true;
	}
}
#endif

#if !__FINAL
bool IsValidPath(fiFindData& find)
{
	bool isValid = false;

	if ((find.m_Attributes & FILE_ATTRIBUTE_DIRECTORY) && (find.m_Name[0] != '.'))
	{
	#if RSG_ORBIS || RSG_DURANGO || RSG_PC
		const u32 MAX_INVALID_NAMES = 8;
		const char* invalidNames[MAX_INVALID_NAMES] = 
			{ "Online", "outputPackages", "Packages", "dev", "art", "assets", "drm", "signage" };
	#else
		const u32 MAX_INVALID_NAMES = 8;
		const char* invalidNames[MAX_INVALID_NAMES] = 
			{ "Online", "outputPackages", "Packages", "dev_ng", "art", "assets", "drm", "signage" };
	#endif

		isValid = true;

		for (u32 i = 0; i < MAX_INVALID_NAMES; i++)
			isValid &= !(strcmp(find.m_Name, invalidNames[i]) == 0);
	}

	return isValid;
}

int CExtraContentManager::RecursiveEnumExtraContent(const char* searchPath)
{
	fiFindData find;
	const fiDevice *pDevice = fiDevice::GetDevice(searchPath, true);
	fiHandle handle = pDevice->FindFileBegin(searchPath, find);
	int searchPathLen = istrlen(searchPath);
	bool validSearchPath = !searchPathLen || searchPath[searchPathLen-1] == '/' || searchPath[searchPathLen-1] == '\\';
	int foundDLCCount = 0;
	Assertf(validSearchPath, "CExtraContentManager::RecursiveEnumExtraContent - searchPath must end with a slash! %s", searchPath);
	
	if ( validSearchPath && fiIsValidHandle( handle ) )
	{
#if __BANK
		char assetFolder[RAGE_MAX_PATH] = { 0 };
#ifdef RS_BRANCHSUFFIX
		formatf(assetFolder, RAGE_MAX_PATH, "%sart/ng/anim/export_mb", searchPath);
		if(ASSET.Exists(assetFolder, ""))
		{
			CDebugClipDictionary::PushAssetFolder(assetFolder);
		}
		formatf(assetFolder, RAGE_MAX_PATH, "%sassets" RS_BRANCHSUFFIX "/cuts", searchPath);
		if(ASSET.Exists(assetFolder, ""))
		{
			CDebugClipDictionary::PushAssetFolder(assetFolder);
			CutSceneManager::PushAssetFolder(assetFolder);
		}
#else
		formatf(assetFolder, RAGE_MAX_PATH, "%sart/anim/export_mb", searchPath);
		if(ASSET.Exists(assetFolder, ""))
		{
			CDebugClipDictionary::PushAssetFolder(assetFolder);
		}
		formatf(assetFolder, RAGE_MAX_PATH, "%sassets/cuts", searchPath);
		if(ASSET.Exists(assetFolder, ""))
		{
			CDebugClipDictionary::PushAssetFolder(assetFolder);
			CutSceneManager::PushAssetFolder(assetFolder);
		}
#endif
#endif // __BANK

		do 
		{
			atString filePath;

			if (IsValidPath(find))
			{
				filePath = searchPath;
				filePath += find.m_Name;

				ASSET.PushFolder(filePath.c_str());

				// Is this a DLC folder?
				if (ASSET.Exists(CMountableContent::GetSetupFileName(), ""))
				{
					if(!IsContentFilenamePresent(filePath.c_str()))
					{
						dlcDebugf3("CExtraContentManager::RecursiveEnumExtraContent - Looking for local DLC in %s", filePath.c_str());

						CMountableContent newContent;

						newContent.SetFilename(filePath);
						newContent.SetUsesPackFile(false);
						newContent.SetPrimaryDeviceType(CMountableContent::DT_FOLDER);
						newContent.SetNameHash(newContent.GetFilename());

						AddContent(newContent);

					}
					ASSET.PopFolder();
					foundDLCCount++;
				}
				else
				{
					ASSET.PopFolder();
					// It's not - let's recurse in here.
					
					char nextPath[RAGE_MAX_PATH] = { 0 };
					formatf(nextPath, "%s/", filePath.c_str());

					foundDLCCount+=RecursiveEnumExtraContent(nextPath);
				}
			}
		} 
		while (pDevice->FindFileNext( handle, find ));

		pDevice->FindFileEnd(handle);

	}
	return foundDLCCount;
}
#endif // !__FINAL

void CExtraContentManager::CancelEnumerateContent(bool XENON_ONLY(bWait))
{
	dlcDebugf1("End DLC enumeration (%s)", false XENON_ONLY(|| bWait) ? "Async" : "Block");

	m_enumerating = false;

#if __XENON
	if (m_deviceEnumerating)
		fiDeviceXContent::FinishEnumerate(bWait);

	m_deviceEnumerating = false;
#endif
}

bool CExtraContentManager::EndEnumerateContent(bool bWait, bool earlyStartup)
{
	CancelEnumerateContent(bWait);

#if __XENON
	fiDeviceXContent::LockEnumerate();
	{
		UpdateContentArray();
	}
	fiDeviceXContent::UnlockEnumerate();
#endif

#if __PPU || RSG_ORBIS || RSG_PC || RSG_DURANGO
	UpdateContentArray();
#endif

	// Load and execute any new content once we have added it to the array...
	LoadContent(!PARAM_delayDLCLoad.Get(), earlyStartup);

	return true;
}

//
// name:		strcpy
// description:	strcpy that takes a wide string input and an 8 bit output
void Strcpy(char* pDest, wchar_t* pSrc)
{
	while(*pSrc != 0)
	{
		*pDest = (char)*pSrc;
		pDest++;
		pSrc++;
	}
    *pDest = 0;
}

void CExtraContentManager::NormalizePath(char *src_path, bool trailingSlash /* = true */)
{
	Assert(src_path);

	const char SEPARATOR = '/';
	const char OTHER_SEPARATOR = '\\';

	const char *s = src_path;
	char *d = src_path;
	bool has_dots = false;

	while(*s) 
	{
		if((*s == SEPARATOR) || 
		   (*s == OTHER_SEPARATOR))
		{
			*d++ = SEPARATOR;

			while((*s == SEPARATOR) || 
				  (*s == OTHER_SEPARATOR)) 
				s++;
		}
		else
		{
			has_dots |= (*s == '.');
			*d++ = *s++;
		}
	}

	if(trailingSlash && (d > src_path) && !has_dots)
	{
		if(*(d - 1) != SEPARATOR)
		{
			*d++ = SEPARATOR;
		}
	}

	*d = '\0';
}

#if RSG_ORBIS
bool CExtraContentManager::IsContentFilenamePresent(atHashString& label) const
{
	for (u32 i = 0; i < m_content.GetCount(); i++)
	{
		atHashString currLabelHash = atHashString(m_content[i].GetPS4EntitlementLabel().data);

		if (currLabelHash == label)
			return true;
	}

	return false;
}
#endif

bool CExtraContentManager::IsContentFilenamePresent(const char* fileName) const
{
	char fname[RAGE_MAX_PATH] = { 0 };	
	char contentFname[RAGE_MAX_PATH] = { 0 };	

	strncpy(fname, fileName, RAGE_MAX_PATH);
	NormalizePath(fname);

	for (u32 i = 0; i < m_content.GetCount(); i++)
	{		
		strncpy(contentFname, m_content[i].GetFilename(), RAGE_MAX_PATH);
		NormalizePath(contentFname);

		if (!strcmpi(fname, contentFname))
			return true;
	}

	return false;
}

#if __PPU
void CExtraContentManager::UpdateContentArrayPs3(const char *path)
{
	const fiDevice& localDevice = fiDeviceLocal::GetInstance(); //	get path to usrdir folder
	fiFindData findDlcData;
	fiHandle DlcHandle;
	DlcHandle = localDevice.FindFileBegin(path, findDlcData);
	dlcDebugf2("CExtraContentManager::UpdateContentArray - Scanning %s for DLC", path);

	if ( fiIsValidHandle( DlcHandle ) )
	{
		do 
		{
			dlcDebugf3("CExtraContentManager::UpdateContentArray - Considering %s", findDlcData.m_Name);

			//	First find the folder named "dlc"
			if (findDlcData.m_Attributes == FILE_ATTRIBUTE_DIRECTORY && strnicmp(findDlcData.m_Name, "dlc", 3) == 0)
			{
				fiFindData findExtraContentData;
				fiHandle extraContentHandle;
				char DlcPath[RAGE_MAX_PATH] = { 0 };

				formatf(DlcPath, "%s/%s",path, findDlcData.m_Name);
				extraContentHandle = localDevice.FindFileBegin(DlcPath, findExtraContentData);

				dlcDebugf3("CExtraContentManager::UpdateContentArray - Looking into %s", DlcPath);

				if( fiIsValidHandle( extraContentHandle ) )
				{
					do 
					{
						dlcDebugf3("CExtraContentManager::UpdateContentArray - Checking out %s", findExtraContentData.m_Name);

						//	Then find any folders whose name begins with "episode" inside the "dlc" folder
						if (findExtraContentData.m_Attributes == FILE_ATTRIBUTE_DIRECTORY && 
							(strnicmp(findExtraContentData.m_Name, "episode", 7) == 0 || strnicmp(findExtraContentData.m_Name, "dlc", 3) == 0))
						{
							char filePath[RAGE_MAX_PATH] = { 0 };

							sprintf(filePath, "%s/%s", DlcPath, findExtraContentData.m_Name);

							if (!IsContentFilenamePresent(filePath))
							{
								CMountableContent mount;

								mount.SetFilename(filePath);

#if !__FINAL
								if (!PARAM_extracontent.Get())
#endif
									mount.SetUsesPackFile(true);

								AddContent(mount);
							}
						}
					} 
					while(localDevice.FindFileNext( extraContentHandle, findExtraContentData ));

					localDevice.FindFileEnd(extraContentHandle);
				}
			}
		}
		while(localDevice.FindFileNext( DlcHandle, findDlcData ));

		localDevice.FindFileEnd(DlcHandle);
	}
}

#endif	//	__PPU

//
// name:		CExtraContentManager::UpdateContentArray
// description:	Update content array based on enumerated content from fiDeviceXContent
void CExtraContentManager::UpdateContentArray()
{
#if !__FINAL
	bool ignoreDeployed = PARAM_ignoreDeployedPacks.Get();

	if(PARAM_usecompatpacks.Get())
	{
		if (m_enumerateCommandLines)
		{
			SMandatoryPacksData mandatoryPacksData;

			if(Verifyf(PARSER.LoadObject(COMPATPACKS_SET_PATH, NULL, mandatoryPacksData), 
				"-usecompatpacks couldn't load %s.", COMPATPACKS_SET_PATH))
			{
				Displayf("Loaded MandatoryPacksData from %s", COMPATPACKS_SET_PATH);

				for(int i = 0; i < mandatoryPacksData.m_Paths.GetCount(); i++)
				{
					int foundPacks = RecursiveEnumExtraContent(mandatoryPacksData.m_Paths[i].c_str());
					Assertf(foundPacks>0,"Couldn't find any DLC in the provided path %s , check your compatpacks.xml",mandatoryPacksData.m_Paths[i].c_str());
					Displayf("[DLC]Found %d DLC packs in %s",foundPacks, mandatoryPacksData.m_Paths[i].c_str());
				}
			}
		}

		ignoreDeployed = true;
	}

	if(PARAM_extracontent.Get())
	{
		if (m_enumerateCommandLines)
		{
			const char* pSearchPath = NULL;
			PARAM_extracontent.Get(pSearchPath);
			char buffer[RAGE_MAX_PATH] = {0}; 
			safecpy(buffer,pSearchPath);
			char* cur = strtok(buffer,";");
			while(cur)
			{
				int foundPacks = RecursiveEnumExtraContent(cur);
				Assertf(foundPacks>0,"Couldn't find any DLC in the provided path %s , check your -extracontent argument",cur);
				Displayf("[DLC]Found %d DLC packs in %s",foundPacks,cur);
				cur = strtok(NULL,";");
			}
		}
	}

	m_enumerateCommandLines = false;

	// Make commandline enumerators mutually exclusive with deployed packages, this avoids dupes as we only use file path as the ID
	if (ignoreDeployed)
		return;
#endif

	SMandatoryPacksData deployedPacksData;

	if(Verifyf(PARSER.LoadObject(COMPATPACKS_DEPLOYED_PATH, NULL, deployedPacksData), 
		"dlcList.xml isn't present as %s.", COMPATPACKS_DEPLOYED_PATH))
	{
		Displayf("Loaded dlcList data from %s", COMPATPACKS_DEPLOYED_PATH);

		for(int i = 0; i < deployedPacksData.m_Paths.GetCount(); i++)
		{
			char path[RAGE_MAX_PATH];
			DATAFILEMGR.ExpandFilename(deployedPacksData.m_Paths[i].c_str(),path,RAGE_MAX_PATH);
			AddContentFolder(path);
		}
	}

#if RSG_DURANGO
	if(CLiveManager::IsSignedIn())
	{
		EnumerateDurangoContent();
		UpdateContentArrayDurango();
	}
#endif
#if __XENON
	for (u32 i = 0; i < fiDeviceXContent::GetNumXContent(); i++)
	{
		XCONTENT_DATA* pContent = fiDeviceXContent::GetXContent(i);

		if (!IsContentFilenamePresent(pContent->szFileName))
		{
			CMountableContent newContent;
			char tempStr[XCONTENT_MAX_DISPLAYNAME_LENGTH] = { 0 };

			Strcpy(tempStr, pContent->szDisplayName);
			newContent.SetFilename(pContent->szFileName);
			newContent.SetNameHash(tempStr);
			newContent.SetPrimaryDeviceType(CMountableContent::DT_XCONTENT);
			newContent.SetXContentData(pContent);
			newContent.SetUsesPackFile(true);

			AddContent(newContent);
		}
	}
#endif	//	__XENON

#if __PPU
	 UpdateContentArrayPs3(fiPsnExtraContentDevice::GetGameDataPath());

	 if(fiDeviceInstaller::GetIsBootedFromHdd())
	 {
		UpdateContentArrayPs3(fiDeviceInstaller::GetHddBootPath());
	 }
#endif	//	__PPU

#if RSG_ORBIS

	 SceNpServiceLabel serviceLabel = 0;
	 SceAppContentAddcontInfo packages[SCE_APP_CONTENT_ADDCONT_MOUNT_MAXNUM];
	 u32 packageCount = 0;
	 s32 errValue = 0;

	 memset(packages, 0, sizeof(SceAppContentAddcontInfo) * SCE_APP_CONTENT_ADDCONT_MOUNT_MAXNUM);

	 Assertf(packageCount < SCE_APP_CONTENT_ADDCONT_MOUNT_MAXNUM, "UpdateContentArray - At the limit of DLC packs! [%u / %u]", packageCount, SCE_APP_CONTENT_ADDCONT_MOUNT_MAXNUM);

	 errValue = sceAppContentGetAddcontInfoList(serviceLabel, packages, SCE_APP_CONTENT_ADDCONT_MOUNT_MAXNUM, &packageCount);

	 if (Verifyf(errValue == SCE_OK, "UpdateContentArray - Failed to get package count! %i", errValue) && packageCount > 0)
	 {
		 for (u32 i = 0; i < packageCount; i++)
		 {
			 SceNpUnifiedEntitlementLabel& entitlementLabel = packages[i].entitlementLabel;
			 atHashString ps4EntitlementHash = atHashString(entitlementLabel.data);

			 // SDK States this must be filled with zeros, now you would have thought Sony would follow their own rules but no.
			 memset(entitlementLabel.padding, 0, sizeof(entitlementLabel.padding));

			 // Only bother with packages that are actually installed
			 if (packages[i].status == SCE_APP_CONTENT_ADDCONT_DOWNLOAD_STATUS_INSTALLED && 
				 !IsContentFilenamePresent(ps4EntitlementHash) && m_ps4DupeEntitlements.Find(ps4EntitlementHash) == -1)
			 {
				 SceAppContentMountPoint mountPoint;

				 errValue = sceAppContentAddcontMount(serviceLabel, &entitlementLabel, &mountPoint);

				 if (Verifyf(errValue == SCE_OK, "UpdateContentArray - Failed to mount package %s! %i", entitlementLabel.data, errValue))
				 {
					 CMountableContent mount;
					 char filePath[SCE_KERNEL_PATH_MAX] = { 0 };

					 formatf(filePath, "%s/dlc.rpf", mountPoint.data);

					 mount.SetPrimaryDeviceType(CMountableContent::DT_PACKFILE);
					 mount.SetFilename(filePath);
					 mount.SetPS4MountPoint(mountPoint);
					 mount.SetPS4EntitlementLabel(entitlementLabel);

					 AddContent(mount);
				 }
			 }
		 }
	 }
#endif

#if RSG_PC
	 EnumerateContentPC();
#endif // RSG_PC
}

u32 CExtraContentManager::GetContentHash(u32 index) const
{
	if (const CMountableContent* content = GetContent(index))
		return content->GetNameHash();

	return 0;
}


void CExtraContentManager::GetMapChangeArray(atArray<u32>& retArray) const
{
	for (u32 i = 0; i < m_content.GetCount(); i++)
	{
		m_content[i].GetMapChangeHashes(retArray);
	}
}


u32 CExtraContentManager::GetMapChangesCRC() const
{
	u32 retVal = 0;
	atArray<u32> activeMapChangeHashes;

	GetMapChangeArray(activeMapChangeHashes);

	std::sort(activeMapChangeHashes.begin(), activeMapChangeHashes.end());

	for (u32 i = 0; i < activeMapChangeHashes.GetCount(); i++)
	{
		u32 currHashName = activeMapChangeHashes[i];

		retVal = atDataHash((const unsigned int*)&currHashName, (s32)sizeof(currHashName), retVal);
	}

	dlcDebugf3("CExtraContentManager::GetMapChangesCRC - retVal = %u", retVal);

	return retVal;
}

#if RSG_PC

void CExtraContentManager::EnumerateContentPC()
{
	const fiDevice *pDevice = fiDevice::GetDevice(DLC_PACKS_PATH, true);
	if(!Verifyf(pDevice, "Couldn't get device for %s", DLC_PACKS_PATH))
		return;

	fiFindData find;
	fiHandle handle = pDevice->FindFileBegin(DLC_PACKS_PATH, find);

	if ( fiIsValidHandle( handle ) )
	{
		do 
		{
			atString filePath;

			if((find.m_Attributes & FILE_ATTRIBUTE_DIRECTORY) && 
			   (find.m_Name[0] != '.'))
			{
				filePath = DLC_PACKS_PATH;
				filePath += find.m_Name;
				filePath += "/";

				AddContentFolder(filePath.c_str());
			}
		} 
		while (pDevice->FindFileNext( handle, find ));

		pDevice->FindFileEnd(handle);
	}	
}

#endif // RSG_PC

u32 CExtraContentManager::GetCRC(u32 initValue /*= 0*/) const 
{
	dlcDebugf3("CExtraContentManager::GetCRC - initValue = %u", initValue);

	if(!PARAM_netSessionIgnoreECHash.Get())
	{
		for (u32 i = 0; i < m_content.GetCount(); i++)
		{
			if (m_content[i].GetIsCompatibilityPack())
			{
				u32 currHashName = m_content[i].GetNameHash();

				initValue = atDataHash((const unsigned int*)&currHashName, (s32)sizeof(currHashName), initValue);

				dlcDebugf3("CExtraContentManager::GetCRC - %s changed hash, initValue = %u", m_content[i].GetName(), initValue);
			}
		}

		u32 weaponPatchHash = CWeaponInfoManager::CalculateWeaponPatchCRC(0);

		if(weaponPatchHash)
		{
			initValue = atDataHash((const unsigned int*)&weaponPatchHash, (s32)sizeof(weaponPatchHash), initValue);

			dlcDebugf3("CExtraContentManager::GetCRC - weaponPatchHash changed hash, initValue = %u", initValue);
		}
	}

	dlcDebugf3("CExtraContentManager::GetCRC - return initValue = %u", initValue);

	return initValue;
}

void CExtraContentManager::ExecuteScriptPatch()
{
#if EC_CLOUD_MANIFEST
	u32 contentNameHash = atStringHash(m_cloudData.m_CloudManifest.m_ScriptPatchName);

	if(!contentNameHash)
		return;

	if(CMountableContent *content = GetContentByHash(contentNameHash)) 
	{
		ExecuteContentChangeSetGroup(content->GetNameHash(),(u32)CCS_GROUP_ON_DEMAND);
	}
	else
	{
		Errorf("Cloud weapon patch content '%s' hasn't been mounted!", m_cloudData.m_CloudManifest.m_ScriptPatchName.c_str());
	}
#endif // EC_DO_MANIFEST_CHECKS
}

#if EC_CLOUD_MANIFEST
void CExtraContentManager::ExecuteWeaponPatchMP(bool execute)
{
	u32 contentNameHash = atStringHash(m_cloudData.m_CloudManifest.m_WeaponPatchNameMP);

	if(!contentNameHash)
		return;

	if(GetContentByHash(contentNameHash)) 
	{
		if(execute)
		{
			ExecuteContentChangeSet(contentNameHash,(u32)CCS_GROUP_ON_DEMAND, (u32)CCS_TITLE_UPDATE_WEAPON_PATCH);
		}
		else
		{
			RevertContentChangeSet(contentNameHash,(u32)CCS_GROUP_ON_DEMAND, (u32)CCS_TITLE_UPDATE_WEAPON_PATCH);
		}
	}
	else
	{
		Errorf("Cloud weapon patch content '%s' hasn't been mounted!", m_cloudData.m_CloudManifest.m_WeaponPatchNameMP.c_str());
	}
#else
void CExtraContentManager::ExecuteWeaponPatchMP(bool /*execute*/)
{
	return;
#endif // EC_DO_MANIFEST_CHECKS
}

bool CExtraContentManager::CheckCompatPackConfiguration() const
{
#if EC_CLOUD_MANIFEST
#if __BANK
	if(EXTRACONTENT.BANK_EnableCloudWidgetsOverride())
	{
		return EXTRACONTENT.BANK_GetForceCompatState();
	}
#endif
#if !__FINAL
	if(PARAM_disablecompatpackcheck.Get())
		return true;
#endif // !__FINAL
	bool retVal = true;
	for (u32 i = 0; i < m_cloudData.m_CloudManifest.m_CompatibilityPacks.GetCount(); i++)
	{
		if(!GetContentByHash(atStringHash(m_cloudData.m_CloudManifest.m_CompatibilityPacks[i].m_Name)))
		{
			retVal = false;
			Assertf(0,"Missing compatibility pack %s",m_cloudData.m_CloudManifest.m_CompatibilityPacks[i].m_Name.c_str());
			Displayf("Missing compatibility pack %s",m_cloudData.m_CloudManifest.m_CompatibilityPacks[i].m_Name.c_str());		
		}
	}

	return retVal;
#else
	return true;
#endif // EC_DO_MANIFEST_CHECKS
}

bool CExtraContentManager::VerifySaveGameInstalledPackages() const
{
	for (u32 i = 0; i < m_savegameInstalledPackages.GetCount(); i++)
	{		
		if(!GetContentByHash(m_savegameInstalledPackages[i]))
			return false;
	}

	return true;
}

void CExtraContentManager::ResetSaveGameInstalledPackagesInfo()
{
	m_savegameInstalledPackages.ResetCount();
}

void CExtraContentManager::SetSaveGameInstalledPackage(u32 nameHash)
{
	m_savegameInstalledPackages.PushAndGrow(nameHash);
}

bool CExtraContentManager::AddContentFolder(const char *path)
{
	char filePathPreFixed[RAGE_MAX_PATH] = { 0 };
	formatf(filePathPreFixed,"%sdlc.rpf",path);

	if(Verifyf(ASSET.Exists(filePathPreFixed,""),"DLC pack at path %s not built correctly, dlc.rpf must be inside the package", path))
	{
		if(!IsContentFilenamePresent(filePathPreFixed))
		{
			CMountableContent mount;
			mount.SetFilename(filePathPreFixed);
			mount.SetUsesPackFile(false);
			mount.SetPrimaryDeviceType(CMountableContent::DT_PACKFILE);
			mount.SetNameHash(mount.GetFilename());
			mount.SetIsShippedContent(true);
			AddContent(mount);
		}
		return true;
	}
	else
	{
		Errorf("DLC pack not built correctly, DLC.rpf must be inside the package, path: %s",path);
	}
	return false;
}

int CExtraContentManager::GetMissingCompatibilityPacks(atArray<u32> &packNameHashes) const
{
#if EC_CLOUD_MANIFEST
	for(int i = 0; i < m_cloudData.m_CloudManifest.m_CompatibilityPacks.GetCount(); i++)
	{
		u32 nameHash = atStringHash(m_cloudData.m_CloudManifest.m_CompatibilityPacks[i].m_Name.c_str());
		const CMountableContent* content = GetContentByHash(nameHash);

		if(!content)

		{
			packNameHashes.PushAndGrow(nameHash);
		}
	}
#endif // EC_DO_MANIFEST_CHECKS

	return packNameHashes.GetCount();
}

//
// name:		CExtraContentManager::AddContent
// description:	Add new content
s32 CExtraContentManager::AddContent(CMountableContent& mountableData)
{
	dlcDebugf3("CExtraContentManager::AddContent - Adding DLC content [%i] %s", m_content.GetCount(), mountableData.GetFilename());

	mountableData.SetECPackFileIndex(m_content.GetCount() + 1);
	m_content.PushAndGrow(mountableData, 1);

	CPlayerSpecialAbilityManager::UpdateDlcMultipliers();

	return m_content.GetCount() - 1;
}

// name:		CExtraContentManager::OnPresenceEvent
// description:	Deal with profiles signing in and out
void CExtraContentManager::OnPresenceEvent(const rlPresenceEvent* evt)
{
	if (PRESENCE_EVENT_SIGNIN_STATUS_CHANGED == evt->GetId())
	{
		const rlGamerInfo* activeGamer = NetworkInterface::GetActiveGamerInfo();
		const rlPresenceEventSigninStatusChanged* s = evt->m_SigninStatusChanged;

		if(s->SignedIn() || s->SignedOnline()) // If signed in, init extra content (Signed online is sent instead of signed in, if we sign in and online together)
		{
			if(activeGamer && activeGamer->GetGamerId() == s->m_GamerInfo.GetGamerId() && activeGamer->GetGamerId() != EXTRACONTENT.m_currentGamerId)
				EXTRACONTENT.InitProfile();
		}
		else if(s->SignedOut()) // If signing out shutdown extra content
		{
			if(EXTRACONTENT.m_currentGamerId == s->m_GamerInfo.GetGamerId())
				EXTRACONTENT.ShutdownProfile();
		}
	}
}

void CExtraContentManager::OnServiceEvent(sysServiceEvent* evt)
{
	if(evt != NULL)
	{
		if(evt->GetType() == sysServiceEvent::ENTITLEMENT_UPDATED)
		{
			EXTRACONTENT.OnContentDownloadCompleted();
		}
	}
}

#if !__FINAL
bool CExtraContentManager::IsContentIgnored(u32 nameHash)
{
	const char* packsToIgnore;

	if(PARAM_ignorePacks.Get(packsToIgnore))
	{
		char buffer[RAGE_MAX_PATH] = {0}; 
		strcpy(buffer,packsToIgnore);
		char* cur = strtok(buffer,";");
		while(cur)
		{
			atHashString contentHash(cur);

			if (contentHash == nameHash)
				return true;

			cur = strtok(NULL,";");
		}			
	}

	return false;
}
#endif

void CExtraContentManager::LoadContent(bool executeChangeSet/* = false*/, bool executeEarlyStartup /*=false*/)
{
	sysTimer timeToAccessDLC;

	static bool	bFirstCall = true;
	bool bSlowAccessWarningHasAlreadyBeenShown = false;

	timeToAccessDLC.Reset();

#if RSG_XENON
	s32 packsToLoad = 0;

	for (u32 i = 0; i < m_content.GetCount(); i++)
	{
		if (CMountableContent* content = GetContent(i))
		{
			if (content->GetStatus() != CMountableContent::CS_FULLY_MOUNTED)
				packsToLoad++;
		}
	}

	if (packsToLoad >= LOAD_SCR_PACK_THRESHOLD)
		TriggerLoadingScreen();
#endif

#if RSG_ORBIS
	m_ps4DupeEntitlements.ResetCount();
#endif

	dlcDebugf1("CExtraContentManager::LoadContent - Package count = %i executeChangeSet = %i", m_content.GetCount(), executeChangeSet);

	//	CELL_SYSMODULE_SYSUTIL_NP2 module has already been loaded in rage/base/src/system/main.h and
	//	sceNp2Init has already been called in rage/base/src/rline/rlnp.cpp 
	//	so I don't need to do either of those before loading the encrypted EDATA file(s)

	for (u32 i = 0; i < m_content.GetCount(); i++)
	{
		timeToAccessDLC.Reset();

		if (CMountableContent* content = GetContent(i))
		{
			bool setup = content->Setup(m_localGamerIndex);
			bool isDupe = setup ? IsContentDuplicate(content) : false;
			NOTFINAL_ONLY(bool isIgnored = setup ? IsContentIgnored(content->GetNameHash()) : false;)

			if (!setup || isDupe NOTFINAL_ONLY(|| isIgnored))
			{
			#if RSG_ORBIS
				if (isDupe)
				{
					atHashString ps4EntitlementHash = atHashString(content->GetPS4EntitlementLabel().data);

					if (m_ps4DupeEntitlements.Find(ps4EntitlementHash) == -1)
						m_ps4DupeEntitlements.PushAndGrow(ps4EntitlementHash, 1);
				}
			#endif

				if (content->GetStatus() == CMountableContent::CS_SETUP_MISSING)
					Warningf("CMountableContent::Setup - DLC Pack is missing the proper setup data! [%s]", content->GetFilename());
				else
					Assertf(content->GetStatus() == CMountableContent::CS_SETUP_READ, "CMountableContent::Setup - There was an error loading the setup file! [%s] [%i]", content->GetFilename(), content->GetStatus());

				if(content->GetStatus() == CMountableContent::CS_CORRUPTED)
				{
					m_corruptedContent.PushAndGrow(atString(content->GetFilename()));
				}

				m_content[i].ShutdownContent();
				m_content.Delete(i);
				i--;
				continue;
			}
		}

		if ((!bFirstCall && !bSlowAccessWarningHasAlreadyBeenShown && (timeToAccessDLC.GetTime() > DLC_PACK_MOUNT_TIMEOUT)))
		{
		#if __XENON
			CLoadingScreens::Suspend();
			CWarningScreen::SetAndWaitOnMessageScreen(WARNING_MESSAGE_STANDARD, "DLC_ACCESS_SLOW", FE_WARNING_OK);	//	Will wait in here until the player presses the Accept button

			CWarningScreen::Update();  // one last update as we shutdown to switch off warningscreens that have been set
			CLoadingScreens::Resume();
			CGtaOldLoadingScreen::SetLoadingScreenIfRequired();
			bSlowAccessWarningHasAlreadyBeenShown = true;
		#endif //__XENON
		}
	}

	std::sort(m_content.begin(), m_content.end());

#if !__FINAL
	for(int i = 0; i < m_content.GetCount() - 1; i++)
	{
		if(m_content[i].GetIsCompatibilityPack() && m_content[i + 1].GetIsCompatibilityPack()) 
		{
			ASSERT_ONLY(bool dupe = m_content[i].GetOrder() == m_content[i + 1].GetOrder();)

			Assertf(!dupe, "Compatibility pack: '%s' and compatibility pack: '%s' both have the same order: %d", 
				m_content[i].GetName(),  m_content[i + 1].GetName(), m_content[i].GetOrder());
		}
	}
#endif

	u32 compatIndex = 0;

	for (u32 i = 0; i < m_content.GetCount(); i++)
	{
		if (CMountableContent* content = GetContent(i))
		{
			if(content->GetIsCompatibilityPack())
			{
				if (content->GetOrder() != (s32)compatIndex)
				{
					if(!PARAM_disablepackordercheck.Get())
					{
						m_everHadBadPackOrder = true;
					}

					Assertf(false, "CExtraContentManager::LoadContent compatibility pack configuration is not complete compatibility pack %s with index %d has order %d", 
						content->GetName(), compatIndex, content->GetOrder());
				}

				compatIndex++;
			}

			// Don't bother mounting packs that are setup only
			if (!content->IsSetupOnly())
			{
				if(executeEarlyStartup)
				{
					if(content->IsLevelPack())
					{
						content->SetPermanent(true);
						content->Mount(m_localGamerIndex);

						if(executeChangeSet)
							ExecuteContentChangeSetGroup(content->GetNameHash(),CCS_GROUP_EARLY_ON);
					}
				}
				else
				{
					content->Mount(m_localGamerIndex);

					if(executeChangeSet)
					{
						ExecuteContentChangeSetGroup(content->GetNameHash(),CCS_GROUP_STARTUP);
					}
				}
			}
		}
	}

	if(executeChangeSet)
 		ExecutePendingOverlays();

	CPlayerSpecialAbilityManager::UpdateDlcMultipliers();
	if(!bFirstCall)
	{
		CNetworkAssetVerifier& verifier = CNetwork::GetAssetVerifier();
		verifier.RefreshFileCRC();
	}
	if(GetCorruptedContentCount() > 0)
	{
		CWarningScreen::SetAndWaitOnMessageScreen(WARNING_MESSAGE_STANDARD, "DLC_CORRUPT_ERR", FE_WARNING_OK);	//	Will wait in here until the player presses the Accept button
		m_corruptedContent.Reset();
	}

#if __BANK
	Bank_UpdateContentDisplay();
#endif

#if !__NO_OUTPUT
	PrintState();
#endif // !__NO_OUTPUT

	bFirstCall = false;
}

#if !__NO_OUTPUT

void CExtraContentManager::PrintState() const
{
	Displayf("ExtraContent state:");

	Displayf("Mounted %d DLC packs:", m_content.GetCount());

	const char *fmt = " %-16s %-56s %-22s %-12s %-22s %-10s";

	Displayf(fmt, "Name", "Filename", "Device", "DeviceType", "TimeStamp", "Status"); 

	for (u32 i = 0; i < m_content.GetCount(); i++)
	{
		if (const CMountableContent* content = GetContent(i))
		{
			Displayf(fmt, 
				content->GetName(), 
				content->GetFilename(), 
				content->GetDeviceName(), 
				GetDeviceTypeString(content->GetPrimaryDeviceType()), 
				content->GetTimeStamp(), 
				CMountableContent::GetContentStatusName((content->GetStatus())));
		}
	}

	Displayf("DLC CRC:0x%X, DLC Map CRC:0x%X", GetCRC(0), GetMapChangesCRC());
}

#endif // !__NO_OUTPUT

bool CExtraContentManager::IsContentDuplicate(const CMountableContent *content) const
{
	for (u32 i = 0; i < m_content.GetCount(); i++)
	{
		if (const CMountableContent* c = GetContent(i))
		{
			if(c == content)
				continue;

			if(strnicmp(c->GetDeviceName(), content->GetDeviceName(), RAGE_MAX_PATH) == 0)
			{
				// Let dev extra content take priority over deployed content
				if (!content->GetIsShippedContent() && c->GetIsShippedContent())
					return false;

				Warningf("DLC with the same device name '%s' has been already mounted%s! This pack %s (%s) will be ignored.", 
					content->GetDeviceName(),
					c->GetIsShippedContent()?" as a shipped DLC pack, you seem to have both shipped and installed packs, try removing the installed packs":"",
					content->GetFilename(), 
					content->GetName());

				return true;
			}
		}
	}

	return false;
}

//
// name:		CExtraContentManager::InsertContentLock
// description:	Compatibility packs should add content locks, per DLC item (clothes, weapons etc...) and paid packs will unlock them.
//				You must have a valid index returned from AddOnContentItemUnlockedCB to supply as unlockedCBIndex
// usage:
//				s32 contentLockCBIndex = AddOnContentItemChangedCB(myCBFunctor(u32));
//				InsertContentLock(atStringHash("subsystemContentLock", contentLockCBIndex);
void CExtraContentManager::InsertContentLock(u32 hash, s16 unlockedCBIndex, bool locked)
{
	if (hash != 0)
	{
		SContentLockData* lockData = m_contentLocks.SafeGet(hash);

		if (!lockData)
		{
			SContentLockData newLockData;

			newLockData.m_locked = locked;
			newLockData.addCBIndex(unlockedCBIndex);

			m_contentLocks.Insert(hash, newLockData);
			m_contentLocks.FinishInsertion();

		#if !__FINAL
			OUTPUT_ONLY(atNonFinalHashString hashStr = hash;)

			dlcDebugf3("InsertContentLockData :: Inserting new content lock! %s, %i, %i", hashStr.GetCStr(), newLockData.m_callBackIndices, locked);
		#endif
		}
		else
		{
			OUTPUT_ONLY(s32 oldValue = lockData->m_callBackIndices;)

			lockData->addCBIndex(unlockedCBIndex);

			dlcDebugf3("InsertContentLockData :: Updating callback index! Old: %i - New: %i", oldValue, lockData->m_callBackIndices);
		}

		#if __BANK
			DisplayContentLocks(m_currentContentLocksDebugPage, 0); // refresh bank display
		#endif
	}
}

void CExtraContentManager::RemoveContentLock(u32 hash)
{
	if (SContentLockData* lockData = m_contentLocks.SafeGet(hash))
	{
	#if !__FINAL
		OUTPUT_ONLY(atNonFinalHashString hashStr = hash;)

		dlcDebugf3("RemoveContentLock :: Removing content lock! %s, %i, %i", hashStr.GetCStr(), lockData->m_callBackIndices, lockData->m_locked);
	#endif

		s32 index = m_contentLocks.GetIndexFromDataPtr(lockData);

		m_contentLocks.Remove(index);
	}
}

//
// name:		CExtraContentManager::AddOnContentItemChangedCB
// description:	Subsystems must register an unlocked callback before registering any content locks into the map. We return the index of the callback 
//				so we can attach this to a specific content lock, this means we can call only the relevant subsystem when a content lock state changes.
// usage:
//				s32 contentLockCBIndex = AddOnContentItemChangedCB(myDelegate(u32, bool));
//				InsertContentLock(atStringHash("subsystemContentLock", contentLockCBIndex);
s16 CExtraContentManager::AddOnContentItemChangedCB(atDelegate<void (u32, bool)> onChangedCB)
{
	if (m_onContentLockStateChangedCBs.GetCount() < SContentLockData::MAX_CONTENT_LOCK_SUBSYSTEM_CBS)
	{
		m_onContentLockStateChangedCBs.Push(onChangedCB);

		dlcDebugf3("AddOnContentItemUnlockedCB: index = %i", m_onContentLockStateChangedCBs.GetCount() - 1);

		return (s16)(m_onContentLockStateChangedCBs.GetCount() - 1);
	}

	dlcErrorf("AddOnContentItemChangedCB :: Cannot allocate anymore onContentItemUnlockedCB entries!");

	return SContentLockData::INVALID_CONTENT_LOCK_CB_INDEX;
}

bool CExtraContentManager::IsContentItemLocked(u32 hash) const
{
	if (hash != 0)
	{
		const SContentLockData* lockData = m_contentLocks.SafeGet(hash);

		return (lockData) ? lockData->m_locked : true;
	}

	return false;
}

//
// name:		CExtraContentManager::ModifyContentLockState
// description:	Modifies the state of a content lock and invalidates the subsystem tied to that lock to handle it's new state.
//				This function will insert new content locks into the map with the given state if one does not already exist.
void CExtraContentManager::ModifyContentLockState(u32 hash, bool locked)
{
	if (hash != 0)
	{
		SContentLockData* lockData = m_contentLocks.SafeGet(hash);

		if (lockData)
		{
			lockData->m_locked = locked;

			dlcDebugf3("ModifyContentLockState :: Items [%u] state has changed invoking call backs...", hash);

			for (int i = 0; i < m_onContentLockStateChangedCBs.GetCount(); i++)
			{
				// Call back to any subsystem this content is linked with as the state has changed...
				if (lockData->m_callBackIndices & (1 << i))
					m_onContentLockStateChangedCBs[i].Invoke(hash, lockData->m_locked);
			}
		}
		else
		{
			dlcDebugf3("ModifyContentLockState :: Inserting new content lock!");

			InsertContentLock(hash, SContentLockData::INVALID_CONTENT_LOCK_CB_INDEX, locked);
		}
	}

#if __BANK
	DisplayContentLocks(m_currentContentLocksDebugPage, 0); // refresh bank display
#endif
}

void CExtraContentManager::ExecuteContentChangeSetGroupForAll(u32 changeSetGroup)
{
	for (u32 i = 0; i < m_content.GetCount(); i++)
	{
		ExecuteContentChangeSetGroup(m_content[i].GetNameHash(),changeSetGroup);
	}
}

void CExtraContentManager::RevertContentChangeSetGroupForAll(u32 changeSetGroup)
{
	for (u32 i = 0; i < m_content.GetCount(); i++)
	{
		RevertContentChangeSetGroup(m_content[i].GetNameHash(),changeSetGroup);
	}
}

void CExtraContentManager::ExecuteContentChangeSet(u32 contentHash, u32 changeSetGroup, u32 changeSetName)
{
	CMountableContent* content = EXTRACONTENT.GetContentByHash(contentHash);
	if (Verifyf(content, "ExecuteContentChangeSet - Invalid contentHash %u", contentHash))
	{
		ExecuteContentChangeSetInternal(content,changeSetGroup,changeSetName, ECCS_FLAG_USE_LATEST_VERSION | ECCS_FLAG_USE_LOADING_SCREEN | ECCS_FLAG_MAP_CLEANUP);
	}

}

void CExtraContentManager::RevertContentChangeSet(u32 contentHash, u32 changeSetGroup, u32 changeSetName)
{
	CMountableContent* content = EXTRACONTENT.GetContentByHash(contentHash);
	if (Verifyf(content, "ExecuteContentChangeSet - Invalid contentHash %u", contentHash))
	{
		RevertContentChangeSetInternal(content,changeSetGroup,changeSetName, CDataFileMgr::ChangeSetAction::CCS_ALL_ACTIONS, ECCS_FLAG_USE_LATEST_VERSION | ECCS_FLAG_USE_LOADING_SCREEN);
	}
}

void CExtraContentManager::ExecuteContentChangeSetGroup(u32 contentHash, u32 changeSetGroup)
{
	CMountableContent* content = EXTRACONTENT.GetContentByHash(contentHash);
	if (Verifyf(content, "ExecuteContentChangeSetGroup - Invalid contentHash %u", contentHash))
	{
		ExecuteContentChangeSetGroupInternal(content,changeSetGroup);
	}
}

void CExtraContentManager::RevertContentChangeSetGroup(u32 contentHash, u32 changeSetGroup)
{
	CMountableContent* content = EXTRACONTENT.GetContentByHash(contentHash);
	if (Verifyf(content, "RevertContentChangeSetGroup - Invalid contentHash %u", contentHash))
	{
		RevertContentChangeSetGroupInternal(content,changeSetGroup, CDataFileMgr::ChangeSetAction::CCS_ALL_ACTIONS);
	}
}

//
// name:		CExtraContentManager::GetContentIndex
// description:	Get extra content index from its hash. Returns NULL if it doesn't exist
CMountableContent* CExtraContentManager::GetContentByHash(u32 nameHash)
{
	return const_cast<CMountableContent*>(GetContentByHashImpl(nameHash));
}

CMountableContent* CExtraContentManager::GetContentByDevice(const char* device)
{
	const char* isCrc = stristr(device,"CRC:");
	atHashString deviceHash = atHashString::Null();
	if(isCrc)
	{
		char tempDevice[RAGE_MAX_PATH] = {0};
		char realDevice[RAGE_MAX_PATH] = {0};
		strncpy(tempDevice,device,isCrc - device);
		formatf(realDevice,RAGE_MAX_PATH,"%s:/",tempDevice);
		Displayf("Real device: %s",realDevice);
		deviceHash = atHashString(realDevice);
	}
	else
	{
		deviceHash = atHashString(device);		
	}
	for(int i=0;i<m_content.GetCount();i++)
	{
		dlcDebugf3("content: %s device: %s", m_content[i].GetDeviceName(),deviceHash.TryGetCStr());
		if(atStringHash(m_content[i].GetDeviceName())==deviceHash)
		{
			return &m_content[i];
		}
	}
	return NULL;
}

const CMountableContent* CExtraContentManager::GetContentByHash(u32 nameHash) const
{
	return GetContentByHashImpl(nameHash);
}

const CMountableContent* CExtraContentManager::GetContentByHashImpl(u32 nameHash) const
{
	for(u32 i=0; i<m_content.GetCount(); i++)
	{
		if(m_content[i].GetNameHash() == nameHash)
			return &m_content[i];
	}

	return NULL;
}

CMountableContent* CExtraContentManager::GetContent(u32 index)
{
	if (index < m_content.GetCount())
		return &m_content[index];

	return NULL;
}

const CMountableContent* CExtraContentManager::GetContent(u32 index) const
{
	if (index < m_content.GetCount())
		return &m_content[index];

	return NULL;
}

fiDeviceRelative gTitleUpdatePlatform, gTitleUpdatePlatform2, gTitleUpdateCommon, gTitleUpdateAudio, gTitleUpdateAudioSFX, gLocalTU;
fiDeviceCrc gTitleUpdateCommonCRC, gTitleUpdatePlatformCRC, gTitleUpdatePlatform2CRC;
fiPackfile gTUPackfile;

fiDeviceRelative gTitleUpdateCommon2;
fiDeviceCrc gTitleUpdateCommonCRC2;
fiPackfile gTUPackfile2;

const char *CExtraContentManager::GetPlatformTitleUpdatePath()
{
	static char platformPath[RAGE_MAX_PATH] = { 0 };
	if(!platformPath[0])
		sprintf(platformPath, "update:/%s", RSG_PLATFORM_ID);
	return platformPath;
}

const char *CExtraContentManager::GetPlatformTitleUpdate2Path()
{
	static char platformPath[RAGE_MAX_PATH] = { 0 };
	if( !platformPath[0] )
		sprintf( platformPath, "update2:/%s", RSG_PLATFORM_ID );
	return platformPath;
}

const char *CExtraContentManager::GetAudioTitleUpdatePath()
{
	static char audioPath[RAGE_MAX_PATH] = { 0 };
	if(!audioPath[0])
	{
#if RSG_ORBIS
		sprintf(audioPath, "update:/%s/audio", RSG_PLATFORM);
#else
		sprintf(audioPath, "update:/%s/audio", RSG_PLATFORM_ID);
#endif
	}
	return audioPath;
}

void CExtraContentManager::MountUpdate()
{
	fiDevice *overrideDevice = NULL, *tu2Device = nullptr;

	if( ASSET.Exists( TITLE_UPDATE2_RPF_PATH, NULL ) )
	{
		if( Verifyf( gTUPackfile2.Init( TITLE_UPDATE2_RPF_PATH, true, fiPackfile::CACHE_NONE ), "CExtraContentManager::MountUpdate failed to init rpf : %s", TITLE_UPDATE2_RPF_PATH ) )
		{
			gTUPackfile2.MountAs( "update2:/" );
			tu2Device = &gTUPackfile2;
		}
		else
		{
			Errorf( "CExtraContentManager::MountUpdate failed to init rpf : %s", TITLE_UPDATE2_RPF_PATH );
		}
	}

	if(ASSET.Exists(TITLE_UPDATE_RPF_PATH, NULL))
	{
		if(Verifyf(gTUPackfile.Init(TITLE_UPDATE_RPF_PATH, true, fiPackfile::CACHE_NONE), "CExtraContentManager::MountUpdate failed to init rpf : %s", TITLE_UPDATE_RPF_PATH))
		{
			gTUPackfile.MountAs("update:/");
			overrideDevice = &gTUPackfile;
			gLocalTU.Init("update/" RSG_PLATFORM_ID "/dlcpacks/", true);
			gLocalTU.MountAs("dlcpacks:/");
		}
		else
		{
			Errorf("CExtraContentManager::MountUpdate failed to init rpf : %s", TITLE_UPDATE_RPF_PATH);
		}
	}
	else
	{
		if(fiDevice::IsAnythingMountedAtMountPoint(TITLE_UPDATE_MOUNT_PATH))
		{
			gLocalTU.Init("update:/" RSG_PLATFORM_ID "/dlcpacks/", true);
			gLocalTU.MountAs("dlcpacks:/");
		}
	}

	if(fiDevice::IsAnythingMountedAtMountPoint(TITLE_UPDATE_MOUNT_PATH))
	{
		//update2
		{
			gTitleUpdateCommon2.Init( "update2:/common", false, tu2Device );
			gTitleUpdateCommon2.MountAs( "common:/" );

			// mount crc device
			gTitleUpdateCommonCRC2.Init( "update2:/common", true, tu2Device );
			gTitleUpdateCommonCRC2.MountAs( "commoncrc:/" );
		}

		gTitleUpdateCommon.Init( "update:/common", false, overrideDevice);
		gTitleUpdateCommon.MountAs("common:/");	
	
		// mount crc device
		gTitleUpdateCommonCRC.Init("update:/common", true, overrideDevice);
		gTitleUpdateCommonCRC.MountAs("commoncrc:/");
	
		gTitleUpdatePlatform.Init( GetPlatformTitleUpdatePath(), false, overrideDevice);
		gTitleUpdatePlatform.MountAs("platform:/");
	
		gTitleUpdatePlatformCRC.Init(GetPlatformTitleUpdatePath(), true, overrideDevice);
		gTitleUpdatePlatformCRC.MountAs("platformcrc:/");
	
		gTitleUpdatePlatform2.Init( GetPlatformTitleUpdate2Path(), false, tu2Device );
		gTitleUpdatePlatform2.MountAs( "platform:/" );

		gTitleUpdatePlatform2CRC.Init( GetPlatformTitleUpdate2Path(), true, overrideDevice );
		gTitleUpdatePlatform2CRC.MountAs( "platformcrc:/" );

		if(!PARAM_audiofolder.Get())
		{
			gTitleUpdateAudio.Init( GetAudioTitleUpdatePath(), true, overrideDevice);
			gTitleUpdateAudio.MountAs("audio:/");
			char audioSfxPath[RAGE_MAX_PATH] = { 0 };
			sprintf(audioSfxPath, "%s/%s", GetAudioTitleUpdatePath(), "sfx");
			gTitleUpdateAudioSFX.Init(audioSfxPath, true, overrideDevice);
			gTitleUpdateAudioSFX.MountAs("audio:/sfx/");
		}
	}
}

void CExtraContentManager::RemountUpdate()
{
	if(fiDevice::IsAnythingMountedAtMountPoint(TITLE_UPDATE_MOUNT_PATH))
	{
		// Unmount and then mount the folders again, now that we remounted the files on the disk
		fiDevice::Unmount(gTitleUpdateCommon);
		gTitleUpdateCommon.MountAs("common:/");
		fiDevice::Unmount(gTitleUpdateCommonCRC);
		gTitleUpdateCommonCRC.MountAs("commoncrc:/");
		//update2
		{
			fiDevice::Unmount( gTitleUpdateCommon2 );
			gTitleUpdateCommon2.MountAs( "common:/" );
			fiDevice::Unmount( gTitleUpdateCommonCRC2 );
			gTitleUpdateCommonCRC2.MountAs( "commoncrc:/" );
		}

		fiDevice::Unmount(gTitleUpdatePlatform);
		gTitleUpdatePlatform.MountAs("platform:/");
		fiDevice::Unmount(gTitleUpdatePlatformCRC);
		gTitleUpdatePlatformCRC.MountAs("platformcrc:/");

		fiDevice::Unmount( gTitleUpdatePlatform2 );
		gTitleUpdatePlatform2.MountAs( "platform:/" );
		fiDevice::Unmount( gTitleUpdatePlatform2CRC );
		gTitleUpdatePlatform2CRC.MountAs( "platformcrc:/" );

		if(!PARAM_audiofolder.Get())
		{
			fiDevice::Unmount(gTitleUpdateAudio);
			gTitleUpdateAudio.MountAs("audio:/");
			fiDevice::Unmount(gTitleUpdateAudioSFX);
			gTitleUpdateAudioSFX.MountAs("audio:/sfx/");
		}
	}
}

void CExtraContentManager::ExecuteTitleUpdateDataPatch(u32 changeSetHash, bool execute)
{
	if(CMountableContent *content = GetContentByHash(atStringHash(TITLE_UPDATE_PACK_NAME)))
	{
		BANK_ONLY(strStreamingInfoManager::ms_bValidDlcOverlay = true;)

		if(execute)
		{
			ExecuteContentChangeSet(content->GetNameHash(),(u32)CCS_GROUP_ON_DEMAND, changeSetHash);
		}
		else
		{
			RevertContentChangeSet(content->GetNameHash(),(u32)CCS_GROUP_ON_DEMAND, changeSetHash);
		}

		BANK_ONLY(strStreamingInfoManager::ms_bValidDlcOverlay = false;)
	}
}

void CExtraContentManager::ExecuteTitleUpdateDataPatchGroup(u32 changeSetHash, bool execute)
{

	for(int i=0;i<m_content.GetCount();++i)
	{
		CMountableContent& content = m_content[i]; 
		if(content.GetIsPermanent())
		{
			BANK_ONLY(strStreamingInfoManager::ms_bValidDlcOverlay = true;)
			if(execute)
			{
				ExecuteContentChangeSetGroup(content.GetNameHash(),changeSetHash);
			}
			else
			{
				RevertContentChangeSetGroup(content.GetNameHash(),changeSetHash);
			}
			BANK_ONLY(strStreamingInfoManager::ms_bValidDlcOverlay = false;)
		}
	}
}

const char * GetDeviceTypeString (CMountableContent::eDeviceType deviceType)
{
	switch(deviceType)
	{
		case CMountableContent::DT_INVALID:
			return "DT_INVALID";
		case CMountableContent::DT_FOLDER:
			return "DT_FOLDER";
		case CMountableContent::DT_PACKFILE:
			return "DT_PACKFILE";
		case CMountableContent::DT_XCONTENT:
			return "DT_XCONTENT";
		case CMountableContent::DT_DLC_DISC:
			return "DT_DLC_DISC";
		default:
			return "INVALID";
	}
}

bool CExtraContentManager::GetPartOfCompatPack(const char* file)
{
	CMountableContent* content = GetContentByDevice(file);
	if(content)
		return content->GetIsCompatibilityPack()||content->GetIsPermanent();	
	else
		return false;
}

#if __BANK

#define MAX_CONTENT_LOCKS_PER_PAGE 10

static u32 CURR_CL_LIST[MAX_CONTENT_LOCKS_PER_PAGE] = { 0 };

bkBank* CExtraContentManager::ms_pBank = NULL;
bkCombo* CExtraContentManager::ms_pSelectedChangeSet = NULL;
bkCombo* CExtraContentManager::ms_pChangeSetGroups = NULL;
bkButton *CExtraContentManager::ms_pCreateBankButton = NULL;
bkToggle *CExtraContentManager::ms_pForcedCloudFilesStatus = NULL;
bkToggle *CExtraContentManager::ms_pForceManifestFail = NULL;
bkToggle *CExtraContentManager::ms_pForceCompatPackFail = NULL;
bkToggle *CExtraContentManager::ms_pForceLoadingScreen = NULL;
bkGroup* CExtraContentManager::ms_pBankContentStateGroup = NULL;
bkGroup* CExtraContentManager::ms_pBankContentLockGroup = NULL;
bkGroup* CExtraContentManager::ms_pSpecialTriggersGroup = NULL;
bkGroup* CExtraContentManager::ms_pBankExtraContentGroup = NULL;
bkList* CExtraContentManager::ms_pBankContentTable = NULL;
bkList* CExtraContentManager::ms_pBankVersionedContentTable = NULL;

bkGroup		*CExtraContentManager::ms_pBankFilesGroup = NULL;
bkList		*CExtraContentManager::ms_pBankInUseFilesTable = NULL;
bkList		*CExtraContentManager::ms_pBankUnusedFilesTable = NULL;
bkButton	*CExtraContentManager::ms_pBankFileListsPopulateButton = NULL;
bkButton	*CExtraContentManager::ms_pBankInUsePrevButton = NULL;
bkButton	*CExtraContentManager::ms_pBankInUseNextButton = NULL;
bkButton	*CExtraContentManager::ms_pBankUnusedPrevButton = NULL;
bkButton	*CExtraContentManager::ms_pBankUnusedNextButton = NULL;

int CExtraContentManager::ms_selectedContentIndex = -1;
int CExtraContentManager::ms_selectedChangeSetIndex = 0;
int CExtraContentManager::ms_changeSetGroupIndex = 0;
int CExtraContentManager::ms_displayedPackageCount = 0;
int CExtraContentManager::ms_displayedVersionedChangesetCount = 0;
bkList* CExtraContentManager::ms_contentLocksList = NULL;
atArray<const char *>	CExtraContentManager::m_workingDLC;							// for combo box
int						CExtraContentManager::m_workingDLCIndex;
s32 CExtraContentManager::m_currentContentLocksDebugPage = 0;
char CExtraContentManager::m_contentLockSearchString[RAGE_MAX_PATH] = { 0 };
char CExtraContentManager::m_contentLockCountString[RAGE_MAX_PATH] = { 0 };
char CExtraContentManager::m_matchMakingCrc[RAGE_MAX_PATH] = { 0 };
char CExtraContentManager::m_activeMapChange[RAGE_MAX_PATH] = { 0 };
char CExtraContentManager::m_currMapChangeStateStr[RAGE_MAX_PATH] = { 0 };

bool CExtraContentManager::ms_bForcedCloudFilesStatus = false;
bool CExtraContentManager::ms_bForcedCompatPackStatus = false;
bool CExtraContentManager::ms_bForcedLoadingScreenStatus = false;
bool CExtraContentManager::ms_bForcedManifestStatus = false;
bool CExtraContentManager::ms_bDoBankUpdate = false;


// PURPOSE:	"Normalise" filename by lowercasing, replacing `\' with `/', 
//			and morphing double slash into single slash.
// PARAMS:	filename - Name of file to "normalise" (including device ID)
// RETURNS:	atString of "normalised" filename
static atString NormaliseFileName(const char *src) {
	char outPath[RAGE_MAX_PATH];

	bool bWasSlash = false; // rudimentary double-slash stopper
	char *q = outPath;

	for (const char *p = src; *p != '\0'; ++p) {
		if (*p == '\\') {
			if (!bWasSlash) *q++ = '/';
			bWasSlash = true;
		} else if (*p == '/') {
			if (!bWasSlash) *q++ = '/';
			bWasSlash = true;
		} else {
			*q++ = (char)tolower(*p);
			bWasSlash = false;
		}
	}

	*q = '\0';

	return atString(outPath);
}

bool CExtraContentManager::GetAssetPathFromDevice(char* deviceName)
{
	if(CMountableContent* content = GetContentByDevice(deviceName))
	{
		char filePath[RAGE_MAX_PATH]={0};
		strcpy(filePath,content->m_filename);
		char* root = stristr(filePath,"build");
		if(Verifyf(root,"You are trying to access DLC assets while running with packaged DLC, if you are going to modify DLC assets, please use the DLC command lines instead"))
		{
			*root = 0;
			formatf(deviceName, RAGE_MAX_PATH, "%sassets_ng/",filePath);
			return true;
		}
	}
	return false;
}

void CExtraContentManager::ReturnAssetPathForFragTuneCB(char* assetPath, fragType* type)
{
	strStreamingInfo* strInfo = NULL;
	strLocalIndex strFileIdx = strLocalIndex(-1);
	char physicalPath[RAGE_MAX_PATH];
	for(int i = 0; i<g_FragmentStore.GetCount(); i++)
	{
		if(g_FragmentStore.Get(strLocalIndex(i)) == type)
		{
			strInfo = g_FragmentStore.GetStreamingInfo(strLocalIndex(i));
			strFileIdx = strPackfileManager::GetImageFileIndexFromHandle(strInfo->GetHandle());
			strStreamingFile* file = strPackfileManager::GetImageFile(strFileIdx.Get());
			if(Verifyf(file,"Streaming file could not be found for %s",type->GetBaseName()))
			{
				if(!strnicmp(file->m_name.TryGetCStr(),"dlc",3))
				{
					const fiDevice* device = fiDevice::GetDevice(file->m_name.TryGetCStr(),false);
					device->FixRelativeName(physicalPath,RAGE_MAX_PATH,file->m_name.TryGetCStr());
					char* end =  strstr(physicalPath,"/build/dev/");
					if(end)
					{
						*end  = '\0';
						strcat(physicalPath,"/assets/fragments");
						strcpy(assetPath,physicalPath);
					}
				}
				break;
			}
			else
			{
				return;
			}
		}
	}
	return;	
}

void CExtraContentManager::GetWorkingDLCDeviceName(char* deviceName)
{
	if(m_workingDLCIndex>=0&&m_workingDLCIndex<m_workingDLC.GetCount())
	{
		strcpy(deviceName,m_workingDLC[m_workingDLCIndex]);
	}
}

void CExtraContentManager::GetAssetPathForWorkingDLC(char* deviceName)
{
	if(m_workingDLCIndex==0)
	{
		strcpy(deviceName,"assets:/");
	}
	else
	{
		strcpy(deviceName,m_workingDLC[m_workingDLCIndex]);
		GetAssetPathFromDevice(deviceName);
	}
}

bkCombo* CExtraContentManager::AddWorkingDLCCombo(bkBank* bank)
{
	m_workingDLC.Reset();
	m_workingDLC.PushAndGrow("platform:/");
	for(int i=0;i<m_content.GetCount();i++)
	{
		m_workingDLC.PushAndGrow(m_content[i].GetDeviceName());
	}
	bkCombo* ret = bank->AddCombo("Current working DLC", &m_workingDLCIndex, m_workingDLC.GetCount(),m_workingDLC.GetElements(), datCallback(MFA(CExtraContentManager::CurrentWorkingDLCChanged),this));
	return ret;
}

void CExtraContentManager::CurrentWorkingDLCChanged()
{
	Displayf("DLC Combo: %d selected : %s", m_workingDLCIndex, m_workingDLC[m_workingDLCIndex]);
}

void CExtraContentManager::Bank_ExecuteBankUpdate()
{
	if(ms_bDoBankUpdate && !ms_pCreateBankButton)
	{
		for (u32 i = 0; i < ms_displayedPackageCount; i++)
			ms_pBankContentTable->RemoveItem(i);
		for (u32 i = 0; i < ms_displayedVersionedChangesetCount; i++)
			ms_pBankVersionedContentTable->RemoveItem(i);
		PopulateContentTableWidget();
		EXTRAMETADATAMGR.RefreshBankWidgets();
		DisplayContentCRC();
		Bank_UpdateActiveMapChangeDisplay();
		ms_bDoBankUpdate = false;
	}
}
void CExtraContentManager::Bank_UpdateContentDisplay()
{
	ms_bDoBankUpdate = true;
}

void CExtraContentManager::Bank_ExecuteSelectedChangeSet()
{
	if (CMountableContent* content = GetContent(ms_selectedContentIndex))
	{
		atHashString mapCCS = ms_pSelectedChangeSet->GetString(ms_selectedChangeSetIndex);
		atHashString ccsGroup = ms_pChangeSetGroups->GetString(ms_changeSetGroupIndex);

		if (mapCCS.IsNotNull() && ccsGroup.IsNotNull())
		{
			ExecuteContentChangeSet(content->GetNameHash(),ccsGroup,mapCCS);
			Bank_UpdateContentDisplay();
			ExecutePendingOverlays();
			EXTRAMETADATAMGR.RefreshBankWidgets();
		}
	}
	else
	{
		dlcDebugf2("Invalid DLC index for Bank_ExecuteSelectedChangeSet (%d)",ms_selectedContentIndex);
	}
}

void CExtraContentManager::Bank_RevertSelectedChangeSet()
{
	if (CMountableContent* content = GetContent(ms_selectedContentIndex))
	{
		atHashString mapCCS = ms_pSelectedChangeSet->GetString(ms_selectedChangeSetIndex);
		atHashString ccsGroup = ms_pChangeSetGroups->GetString(ms_changeSetGroupIndex);

		if (mapCCS.IsNotNull() && ccsGroup.IsNotNull())
		{
			RevertContentChangeSet(content->GetNameHash(),ccsGroup,mapCCS);
			Bank_UpdateContentDisplay();
			EXTRAMETADATAMGR.RefreshBankWidgets();
		}
	}
	else
	{
		dlcDebugf2("Invalid DLC index for Bank_RevertSelectedChangeSet (%d)",ms_selectedContentIndex);
	}
}

void CExtraContentManager::Bank_ExecuteSelectedGroup()
{
	if (CMountableContent* content = GetContent(ms_selectedContentIndex))
	{
		atHashString ccsGroup = ms_pChangeSetGroups->GetString(ms_changeSetGroupIndex);

		if (ccsGroup.IsNotNull())
		{
			ExecuteContentChangeSetGroup(content->GetNameHash(),ccsGroup);
			ExecutePendingOverlays();
			Bank_UpdateContentDisplay();
			EXTRAMETADATAMGR.RefreshBankWidgets();
		}
	}
	else
	{
		dlcDebugf2("Invalid DLC index for Bank_ExecuteSelectedGroup (%d)",ms_selectedContentIndex);
	}
}

void CExtraContentManager::Bank_RevertSelectedGroup()
{
	if (CMountableContent* content = GetContent(ms_selectedContentIndex))
	{
		atHashString ccsGroup = ms_pChangeSetGroups->GetString(ms_changeSetGroupIndex);

		if (ccsGroup.IsNotNull())
		{
			RevertContentChangeSetGroup(content->GetNameHash(),ccsGroup);
			Bank_UpdateContentDisplay();
			EXTRAMETADATAMGR.RefreshBankWidgets();
		}
	}
	else
	{
		dlcDebugf2("Invalid DLC index for Bank_RevertSelectedGroup (%d)",ms_selectedContentIndex);
	}
}

void CExtraContentManager::Bank_ChangeSetGroupChanged()
{
	if (CMountableContent* content = GetContent(ms_selectedContentIndex))
	{
		if (content->HasAnyChangesets())
		{
			atHashString selectedGroup = ms_pChangeSetGroups->GetString(ms_changeSetGroupIndex);
			atArray<const char*> changeSets;

			content->GetChangeSetsForGroup(selectedGroup, changeSets);

			ms_selectedChangeSetIndex = 0;
			ms_pSelectedChangeSet->UpdateCombo("Map Changes:", &ms_selectedChangeSetIndex, changeSets.GetCount(), &changeSets[0]);
		}
	}
}

void CExtraContentManager::Bank_PrintActiveMapChanges()
{
	Displayf("[DLC-AMC] - BEGIN ACTIVE MAP CHANGES");
	{
		atArray<u32> m_activeMapChangeHashes;

		for (u32 i = 0; i < m_content.GetCount(); i++)
			m_content[i].GetMapChangeHashes(m_activeMapChangeHashes);

		std::sort(m_activeMapChangeHashes.begin(), m_activeMapChangeHashes.end());

		for (u32 i = 0; i < m_activeMapChangeHashes.GetCount(); i++)
		{
			atHashString currHashName = m_activeMapChangeHashes[i];

			Displayf("[DLC-AMC] - 		%s", currHashName.GetCStr());
		}
	}
	Displayf("[DLC-AMC] - END ACTIVE MAP CHANGES");
}

void CExtraContentManager::Bank_UpdateActiveMapChangeDisplay()
{
	atArray<u32> mapChangeHashes;

	for (s32 i = 0; i < m_content.GetCount() ;i++)
		m_content[i].GetMapChangeHashes(mapChangeHashes);

	formatf(m_activeMapChange, "%i active map changes", mapChangeHashes.GetCount());
}

void CExtraContentManager::Bank_ApplyShouldActivateContentChangeState()
{
    int count = m_content.GetCount();
	
    for (int i = 0; i < count ;i++)
	{
        if (m_content[i].GetBankShouldExecute())
			ExecuteContentChangeSetGroup(m_content[i].GetNameHash(),CCS_GROUP_STARTUP);
		else
			RevertContentChangeSetGroup(m_content[i].GetNameHash(),CCS_GROUP_STARTUP);
	}
}

 void CExtraContentManager::Bank_ContentListClick(s32 index)
 {
     dlcDebugf3("Index %d selected in content list",index);

     ms_selectedContentIndex = index;

	 if (CMountableContent* content = GetContent(index))
	 {
		 if (content->HasAnyChangesets())
		 {
			 atArray<const char*> groupNames;

			 content->GetChangesetGroups(groupNames);
			 ms_changeSetGroupIndex = 0;
			 ms_pChangeSetGroups->UpdateCombo("ChangeSet Groups:", &ms_changeSetGroupIndex, groupNames.GetCount(), &groupNames[0], datCallback(MFA(CExtraContentManager::Bank_ChangeSetGroupChanged),&EXTRACONTENT));
			 Bank_ChangeSetGroupChanged();
		 }
	 }
 }

 void CExtraContentManager::Bank_ContentListDoubleClick(s32 index)
 {
	 if (CMountableContent* content = GetContent(index))
	 {
		 content->SetBankShouldExecute(!content->GetBankShouldExecute());
		 dlcDebugf3("Index %d in content list should activate set to %d", index, content->GetBankShouldExecute());
		 Bank_UpdateContentDisplay();
	 }
 }

 bool CExtraContentManager::BANK_EnableCloudWidgetsOverride()
 {
	return PARAM_enableCloudTestWidgets.Get();
 }

 void CExtraContentManager::InitBank()
{
	//Create the cloud bank
	
	// Create the weapons bank
	ms_pBank = &BANKMGR.CreateBank("ExtraContent", 0, 0, false); 

	if(dlcVerifyf(ms_pBank, "Failed to create Extra content bank"))
	{
		if(BANK_EnableCloudWidgetsOverride())
		{
			ms_bForcedCompatPackStatus=false;
			ms_bForcedCloudFilesStatus = false;
			ms_bForcedLoadingScreenStatus = true;
			ms_bForcedManifestStatus	= true;
			ms_pForceManifestFail = ms_pBank->AddToggle("Manifest Successfully Loaded?", &ms_bForcedManifestStatus);
			ms_pForceLoadingScreen = ms_pBank->AddToggle("Loading screens active?", &ms_bForcedLoadingScreenStatus);
			ms_pForcedCloudFilesStatus = ms_pBank->AddToggle("Cloud Files Loaded?", &ms_bForcedCloudFilesStatus);
			ms_pForceCompatPackFail = ms_pBank->AddToggle("Compatibility Pack Configuration correct?", &ms_bForcedCompatPackStatus);
		}
		ms_pCreateBankButton = ms_pBank->AddButton("Create Extra Content widgets", &CExtraContentManager::CreateBankWidgets);
	}
}

void CExtraContentManager::CreateBankWidgets()
{
	Assertf(ms_pBank, "Extra content bank needs to be created first");

	if(ms_pCreateBankButton) //delete the create bank button
	{
		ms_pCreateBankButton->Destroy();
		ms_pCreateBankButton = NULL;
	}
	else
	{
		//bank must already be setup as the create button doesn't exist so just return.
		return;
	}

	EXTRAMETADATAMGR.CreateBankWidgets(*ms_pBank);
	ms_pBank->PushGroup("Extra Content", false);
	{
		EXTRACONTENT.CreateSpecialTriggersWidgets(ms_pBank);
		EXTRACONTENT.CreateContentLocksWidgets(ms_pBank);
		EXTRACONTENT.CreateContentStatusWidgets(ms_pBank);
		EXTRACONTENT.CreateUnusedFilesWidgets(ms_pBank);
	}
	ms_pBank->PopGroup();

	g_LayoutManager.InitWidgets();
}

void CExtraContentManager::ShowPreviousContentLockPage()
{
	EXTRACONTENT.DisplayContentLocks(m_currentContentLocksDebugPage - 1);
}

void CExtraContentManager::ShowNextContentLockPage()
{
	EXTRACONTENT.DisplayContentLocks(m_currentContentLocksDebugPage + 1);
}

void ConcatCBIndices(SContentLockData* currentItem, char* strOutput)
{
	char strCatTemp[16] = { 0 };

	for (int i = 0; i < SContentLockData::MAX_CONTENT_LOCK_SUBSYSTEM_CBS; i++)
	{
		if (currentItem->m_callBackIndices & (1 << i))
		{
			formatf(strCatTemp, sizeof(strCatTemp), "%i,", i);
			strcat(strOutput, strCatTemp);
		}
	}
}

void CExtraContentManager::AddContentLockToBankList(u32 hash, SContentLockData* currentItem, s32 index)
{
	if (currentItem)
	{
		char strCBIndices[RAGE_MAX_PATH] = { 0 };
		atNonFinalHashString hashStr = hash;

		ms_contentLocksList->AddItem(hash, 0, hashStr.GetCStr());
		ms_contentLocksList->AddItem(hash, 1, currentItem->m_locked ? "true" : "false");

		ConcatCBIndices(currentItem, strCBIndices);

		ms_contentLocksList->AddItem(hash, 2, strCBIndices);

		if (Verifyf(index < MAX_CONTENT_LOCKS_PER_PAGE, "CExtraContentManager::AddContentLockToBankList - Index out of range! %u / %u", index, MAX_CONTENT_LOCKS_PER_PAGE))
			CURR_CL_LIST[index] = hash;
	}
}

void CExtraContentManager::DisplayContentLocks(s32 nextPage, u32 lookupItemHash/*=0*/)
{
	if (ms_contentLocksList)
	{
		s32 maxPage = 1;

		// Remove all the items first...
		for (u32 i = 0; i < MAX_CONTENT_LOCKS_PER_PAGE; i++)
		{
			if (CURR_CL_LIST[i] != 0)
				ms_contentLocksList->RemoveItem(CURR_CL_LIST[i]);
		}

		memset(CURR_CL_LIST, 0, sizeof(CURR_CL_LIST));

		if (m_contentLocks.GetCount() > 0)
			maxPage = ((m_contentLocks.GetCount() - 1) / MAX_CONTENT_LOCKS_PER_PAGE) + 1;

		m_currentContentLocksDebugPage = nextPage;
		m_currentContentLocksDebugPage = rage::Clamp(m_currentContentLocksDebugPage, 0, maxPage - 1);

		formatf(m_contentLockCountString, "Count: [%i] // Page: [%i / %i]", m_contentLocks.GetCount(), (m_currentContentLocksDebugPage + 1),  maxPage);

		if (lookupItemHash == 0)
		{
			int startIndex = m_currentContentLocksDebugPage * MAX_CONTENT_LOCKS_PER_PAGE;

			for (int i = startIndex; (i < (startIndex + MAX_CONTENT_LOCKS_PER_PAGE)) && (i < m_contentLocks.GetCount()); i++)
				AddContentLockToBankList(*m_contentLocks.GetKey(i), m_contentLocks.GetItem(i), i - startIndex);
		}
		else
		{
			AddContentLockToBankList(lookupItemHash, m_contentLocks.SafeGet(lookupItemHash), 0);
		}
	}
}

void CExtraContentManager::ContentLockListDblClickCB(s32 hash)
{
	if (SContentLockData* currentItem = m_contentLocks.SafeGet(hash))
		ModifyContentLockState(hash, !currentItem->m_locked);
}

void CExtraContentManager::LookupContentLock()
{
	DisplayContentLocks(0, atStringHash(m_contentLockSearchString));
}

void CExtraContentManager::DisplayContentCRC() 
{
	u32 initValue = 0;
	u32 mapChangeCRC = 0;

	initValue = EXTRACONTENT.GetCRC(initValue);
	mapChangeCRC = EXTRACONTENT.GetMapChangesCRC();

	formatf(m_matchMakingCrc, "Matchmaking: 0x%x, Map Change: 0x%x", initValue, mapChangeCRC);
}

void CExtraContentManager::Bank_InitMapChange()
{
	EXTRACONTENT.SetMapChangeState(MCS_INIT);
	Bank_UpdateMapChangeState();
}

void CExtraContentManager::Bank_EndMapChange()
{
	EXTRACONTENT.SetMapChangeState(MCS_END);
	Bank_UpdateMapChangeState();
}

void CExtraContentManager::Bank_UpdateMapChangeState()
{
	if (ms_pBankContentStateGroup)
	{
		static const char* mcStateNames[MCS_COUNT] = { "MCS_NONE",
														"MCS_INIT",
														"MCS_UPDATE",
														"MCS_END" };

		strcpy(m_currMapChangeStateStr, mcStateNames[m_currMapChangeState]);
	}
}

void CExtraContentManager::CreateContentStatusWidgets(bkBank* parentBank)
{
	const char* emptyData = "";

	if (ms_pBankContentStateGroup)
		parentBank->Remove(*ms_pBankContentStateGroup);

	ms_pBankContentStateGroup = ms_pBank->AddGroup("State",false);

	ms_pBankContentStateGroup->AddText("Content CRCs:", m_matchMakingCrc, sizeof(m_matchMakingCrc), true);
	ms_pBankContentStateGroup->AddButton("Refresh CRC", datCallback(MFA(CExtraContentManager::DisplayContentCRC), &EXTRACONTENT));
	ms_pBankContentStateGroup->AddButton("Refresh content display", datCallback(MFA(CExtraContentManager::Bank_UpdateContentDisplay), &EXTRACONTENT));

	ms_pBankContentStateGroup->AddText("Active Map Changes:", m_activeMapChange, sizeof(m_activeMapChange), true);
	ms_pBankContentStateGroup->AddButton("Print active map changes - Search for [DLC-AMC]", datCallback(MFA(CExtraContentManager::Bank_PrintActiveMapChanges), &EXTRACONTENT));
	ms_pBankContentStateGroup->AddText("Current Map Change State:", m_currMapChangeStateStr, sizeof(m_currMapChangeStateStr), true);
	ms_pBankContentStateGroup->AddToggle("Allow map change anytime:", &m_allowMapChangeAnyTime, NullCB);
	ms_pBankContentStateGroup->AddButton("Init map change", datCallback(MFA(CExtraContentManager::Bank_InitMapChange),&EXTRACONTENT));
	ms_pBankContentStateGroup->AddButton("End map change", datCallback(MFA(CExtraContentManager::Bank_EndMapChange),&EXTRACONTENT));

	ms_pChangeSetGroups = ms_pBankContentStateGroup->AddCombo("ChangeSet Groups:", &ms_changeSetGroupIndex, 0, &emptyData, datCallback(MFA(CExtraContentManager::Bank_ChangeSetGroupChanged),&EXTRACONTENT));
	ms_pSelectedChangeSet = ms_pBankContentStateGroup->AddCombo("ChangeSet:", &ms_selectedChangeSetIndex, 0, &emptyData);
	ms_pBankContentStateGroup->AddButton("Execute Selected ChangeSet", datCallback(MFA(CExtraContentManager::Bank_ExecuteSelectedChangeSet),&EXTRACONTENT));
	ms_pBankContentStateGroup->AddButton("Revert Selected ChangeSet", datCallback(MFA(CExtraContentManager::Bank_RevertSelectedChangeSet),&EXTRACONTENT));
	ms_pBankContentStateGroup->AddButton("Execute Selected Group", datCallback(MFA(CExtraContentManager::Bank_ExecuteSelectedGroup),&EXTRACONTENT));
	ms_pBankContentStateGroup->AddButton("Revert Selected Group", datCallback(MFA(CExtraContentManager::Bank_RevertSelectedGroup),&EXTRACONTENT));

	ms_pBankContentStateGroup->AddTitle("Double click items in the content list to set as Should-Activate");
	ms_pBankContentStateGroup->AddButton("Apply Should-Activate State", datCallback(MFA(CExtraContentManager::Bank_ApplyShouldActivateContentChangeState), &EXTRACONTENT));

	ms_pBankContentTable = ms_pBankContentStateGroup->AddList("Content");

	s32 column = 0;
	ms_pBankContentTable->AddColumnHeader(column++,"Name",bkList::STRING);
	ms_pBankContentTable->AddColumnHeader(column++,"Timestamp",bkList::STRING);
	ms_pBankContentTable->AddColumnHeader(column++,"CCS Should activate",bkList::STRING);
	ms_pBankContentTable->AddColumnHeader(column++,"CCS Active",bkList::STRING);
	ms_pBankContentTable->AddColumnHeader(column++,"Filename",bkList::STRING);
	ms_pBankContentTable->AddColumnHeader(column++,"DatFile",bkList::STRING);
	ms_pBankContentTable->AddColumnHeader(column++,"Uses Packfile",bkList::STRING);
	ms_pBankContentTable->AddColumnHeader(column++,"Device Name",bkList::STRING);
	ms_pBankContentTable->AddColumnHeader(column++,"Device Type",bkList::STRING);
	ms_pBankContentTable->AddColumnHeader(column++,"Status",bkList::STRING);

	bkList::ClickItemFuncType clickItemHandler;
	clickItemHandler.Reset <CExtraContentManager, &CExtraContentManager::Bank_ContentListClick> (&EXTRACONTENT);
	ms_pBankContentTable->SetSingleClickItemFunc(clickItemHandler);

	bkList::ClickItemFuncType doubleClickItemHandler;
	doubleClickItemHandler.Reset <CExtraContentManager, &CExtraContentManager::Bank_ContentListDoubleClick> (&EXTRACONTENT);
	ms_pBankContentTable->SetDoubleClickItemFunc(doubleClickItemHandler);

	ms_pBankVersionedContentTable = ms_pBankContentStateGroup->AddList("Versioned Changesets");
	column = 0;
	ms_pBankVersionedContentTable->AddColumnHeader(column++,"Versioned Changeset",bkList::STRING);
	ms_pBankVersionedContentTable->AddColumnHeader(column++,"Active Changeset",bkList::STRING);
	ms_pBankVersionedContentTable->AddColumnHeader(column++,"Active Content",bkList::STRING);
	ms_pBankVersionedContentTable->AddColumnHeader(column++,"Version",bkList::INT);
	ms_pBankVersionedContentTable->AddColumnHeader(column++,"State",bkList::STRING);
	ms_bDoBankUpdate = true;
	PopulateContentTableWidget();
	DisplayContentCRC();
	Bank_UpdateMapChangeState();
}

void CExtraContentManager::CreateUnusedFilesWidgets(bkBank* parentBank)
{
	if (ms_pBankFilesGroup)
		parentBank->Remove(*ms_pBankFilesGroup);

	ms_pBankFilesGroup = ms_pBank->AddGroup("Content Files", false); {
		ms_pBankFileListsPopulateButton = ms_pBankFilesGroup->AddButton("Populate All (will freeze game for ~10secs)", &CExtraContentManager::PopulateFileTableWidgets);

		ms_pBankInUseFilesTable = ms_pBankFilesGroup->AddList("All Opened Files");
		ms_pBankInUseFilesTable->AddColumnHeader(0, "Absolute Path", bkList::STRING);

		ms_pBankInUsePrevButton = ms_pBankFilesGroup->AddButton("<--", &CExtraContentManager::PrevPageOfOpenFiles, "Previous", 0, true);
		ms_pBankInUseNextButton = ms_pBankFilesGroup->AddButton("-->", &CExtraContentManager::NextPageOfOpenFiles, "Next", 0, true);
		
		ms_pBankUnusedFilesTable = ms_pBankFilesGroup->AddList("Unused Files");
		ms_pBankUnusedFilesTable->AddColumnHeader(0, "Absolute Path", bkList::STRING);

		ms_pBankUnusedPrevButton = ms_pBankFilesGroup->AddButton("<--", &CExtraContentManager::PrevPageOfUnusedFiles, "Previous", 0, true);
		ms_pBankUnusedNextButton = ms_pBankFilesGroup->AddButton("-->", &CExtraContentManager::NextPageOfUnusedFiles, "Next", 0, true);
	}
}

void CExtraContentManager::PopulateContentTableWidget()
{
	const char * states[sOverlayInfo::NUM_OVERLAY_STATES] = {"INACTIVE","WILL_ACTIVATE","ACTIVE"};
	CompileTimeAssert(NELEM(states) == sOverlayInfo::NUM_OVERLAY_STATES);
	ms_displayedPackageCount = m_content.GetCount();

	for (u32 i = 0; i < m_content.GetCount(); i++)
	{
		int column = 0;
		CMountableContent& mount = m_content[i];

		char tempStr[RAGE_MAX_PATH] = { 0 };
		formatf(tempStr, RAGE_MAX_PATH, "%u/%u", mount.GetActiveChangeSetCount(), mount.GetTotalContentChangeSetCount());

		ms_pBankContentTable->AddItem(i,column++,mount.GetName());
		ms_pBankContentTable->AddItem(i,column++,mount.GetTimeStamp());
		ms_pBankContentTable->AddItem(i,column++,mount.GetBankShouldExecute()?"YES":"NO");
		ms_pBankContentTable->AddItem(i,column++,tempStr);
		ms_pBankContentTable->AddItem(i,column++,mount.GetFilename());
		ms_pBankContentTable->AddItem(i,column++,mount.GetDatFileName());
		ms_pBankContentTable->AddItem(i,column++,mount.GetUsesPackfile()?"YES":"NO");
		ms_pBankContentTable->AddItem(i,column++,mount.GetDeviceName());
		ms_pBankContentTable->AddItem(i,column++,GetDeviceTypeString(mount.GetPrimaryDeviceType()));
		ms_pBankContentTable->AddItem(i,column++,CMountableContent::GetContentStatusName((mount.GetStatus())));
	}
	ms_displayedVersionedChangesetCount = m_overlayInfo.GetCount();
	for(int i=0;i<m_overlayInfo.GetCount();i++)
	{
		int column = 0;
		sOverlayInfo* info = m_overlayInfo[i];
		CMountableContent* content = GetContentByHash(m_overlayInfo[i]->m_content);
		Assertf(content, "Mountable content doesn't exist!");
		ms_pBankVersionedContentTable->AddItem(i,column++,info->m_nameId.TryGetCStr());
		ms_pBankVersionedContentTable->AddItem(i,column++,info->m_changeSet.TryGetCStr());
		ms_pBankVersionedContentTable->AddItem(i,column++,content?content->GetName():"");
		ms_pBankVersionedContentTable->AddItem(i,column++,info->m_version);
		ms_pBankVersionedContentTable->AddItem(i,column++,states[info->m_state]);
	}
}

void CExtraContentManager::PopulateFileTableWidgets()  {
	// Remove populate button
	ms_pBankFileListsPopulateButton->Destroy();
	ms_pBankFileListsPopulateButton = NULL;

	// Check param first
	if (!PARAM_checkUnusedFiles.Get()) {
		ms_pBankInUseFilesTable->AddItem(0, 0, "Re-run with -checkUnusedFiles to populate them here");
		ms_pBankUnusedFilesTable->AddItem(0, 0, "Re-run with -checkUnusedFiles to populate them here");

		return;
	}
	
	// Add files requested		
	for (auto it = g_RequestedFiles.Begin(); it != g_RequestedFiles.End(); ++it) {
		const char *originalPath = (*it).c_str();

		// work out absolute path and store
		char noPlatformPercent[RAGE_MAX_PATH];
		DATAFILEMGR.ExpandFilename(originalPath, noPlatformPercent, RAGE_MAX_PATH);

		atString finalPathString = NormaliseFileName(noPlatformPercent);
		atHashString hashed = atStringHash(finalPathString);

		if (!s_loadedFilesAbsolute.Has(hashed)) {
			s_loadedFilesAbsolute.Insert(hashed, finalPathString);
		}
	}

	s_loadedFilesPageCount = (int)rage::FPCeil((float)s_loadedFilesAbsolute.GetCount() / (float)SHOW_FILES_PER_PAGE);
	PopulateOpenFilesAtPage(0);

	// Add files existing on disk
	for (auto it = g_RequestedDevices.Begin(); it != g_RequestedDevices.End(); ++it) {
		const char *deviceName = (*it).c_str();			
		
		// we only really care about platform:, common:, dlc*:, and update:
		if (strnicmp(deviceName, "platform:",	9) != 0 &&
			strnicmp(deviceName, "common:",		7) != 0 &&
			strnicmp(deviceName, "dlc",			3) != 0 && // no colon on purpose
			strnicmp(deviceName, "update:",		7) != 0) {
				continue;
		}
		
		// add slash
		char deviceAndSlash[RAGE_MAX_PATH] = { 0 };
		strncpy(deviceAndSlash, deviceName, RAGE_MAX_PATH);
		deviceAndSlash[strlen(deviceName)] = '/';

		// find device
		if (const fiDevice *device = fiDevice::GetDevice(deviceAndSlash)) {
			atBinaryMap<atString, u32> allTheFiles;
			GetDeviceContentsRecursive(allTheFiles, deviceName); // SLOOOOOW

			for (auto it = allTheFiles.Begin(); it != allTheFiles.End(); ++it) {
				const char *file = (*it).c_str();
				
				// Compute absolute file path
				char fullPath[RAGE_MAX_PATH];
				char absFile[RAGE_MAX_PATH];
				device->FixRelativeName(absFile, RAGE_MAX_PATH, file);
				DATAFILEMGR.ExpandFilename(absFile, fullPath, RAGE_MAX_PATH);
				
				atString strFileName = NormaliseFileName(fullPath);
				atHashString strFileNameHash = atStringHash(strFileName);

				const char *fileName = strFileName.c_str();

				// Is this file open? If not, add to list!
				if (!s_loadedFilesAbsolute.Has(strFileNameHash)) {
					const char *colonPtr = strchr(fileName, ':');
					size_t deviceLen = colonPtr - fileName;
					
					if (deviceLen > 1) {
						// this is not an X:\ or C:\ path, ignore
						continue;
					}

					if (!s_unusedFilesAbsolute.Has(strFileNameHash)) {
						s_unusedFilesAbsolute.Insert(strFileNameHash, strFileName);
					}
				}
			}
		}
	} 

	// Populate Unused 
	s_unusedFilesPageCount = (int)rage::FPCeil((float)s_unusedFilesAbsolute.GetCount() / (float)SHOW_FILES_PER_PAGE);
	PopulateUnusedFilesAtPage(0);
}

void CExtraContentManager::PopulateOpenFilesAtPage(u32 pageNum) {
	s_loadedFilesPageNumber = rage::Min(pageNum, s_loadedFilesPageCount - 1);

	u32 startIndex = s_loadedFilesPageNumber * SHOW_FILES_PER_PAGE;

	auto it = s_loadedFilesAbsolute.Begin();

	// Fast-forward to start index of page
	while (startIndex --> 0 && it != s_loadedFilesAbsolute.End()) {
		++it;
	}
		
	// Run for up to SHOW_FILES_PER_PAGE files
	u32 i = 0;
	for ( ; it != s_loadedFilesAbsolute.End() && i < SHOW_FILES_PER_PAGE; ++it) {
		const char *finalPath = (*it).c_str();

		ms_pBankInUseFilesTable->AddItem(i++, 0, finalPath);
	}

	ms_pBankInUsePrevButton->SetReadOnly(pageNum == 0);
	ms_pBankInUseNextButton->SetReadOnly(pageNum == s_loadedFilesPageCount - 1);
}

void CExtraContentManager::PrevPageOfOpenFiles() {
	if (s_loadedFilesPageNumber == 0) {
		return; 
	}

	PopulateOpenFilesAtPage(s_loadedFilesPageNumber-1);
}
void CExtraContentManager::NextPageOfOpenFiles() {
	if (s_loadedFilesPageNumber == s_loadedFilesPageCount - 1) {
		return;
	}
	
	PopulateOpenFilesAtPage(s_loadedFilesPageNumber+1);
}

void CExtraContentManager::PopulateUnusedFilesAtPage(u32 pageNum) {
	s_unusedFilesPageNumber = rage::Min(pageNum, s_unusedFilesPageCount - 1);

	u32 startIndex = s_unusedFilesPageNumber * SHOW_FILES_PER_PAGE;

	auto it = s_unusedFilesAbsolute.Begin();

	// Fast-forward to start index of page
	while (startIndex --> 0 && it != s_unusedFilesAbsolute.End()) {
		++it;
	}

	// Run for up to SHOW_FILES_PER_PAGE files
	u32 i = 0;
	for ( ; it != s_unusedFilesAbsolute.End() && i < SHOW_FILES_PER_PAGE; ++it) {
		const char *finalPath = (*it).c_str();

		ms_pBankUnusedFilesTable->AddItem(i++, 0, finalPath);
	}

	ms_pBankUnusedPrevButton->SetReadOnly(pageNum == 0);
	ms_pBankUnusedNextButton->SetReadOnly(pageNum == s_unusedFilesPageCount - 1);
}

void CExtraContentManager::PrevPageOfUnusedFiles() {
	if (s_unusedFilesPageNumber == 0) {
		return; 
	}

	PopulateUnusedFilesAtPage(s_unusedFilesPageNumber-1);
}
void CExtraContentManager::NextPageOfUnusedFiles() {
	if (s_unusedFilesPageNumber == s_unusedFilesPageCount- 1) {
		return;
	}

	PopulateUnusedFilesAtPage(s_unusedFilesPageNumber+1);
}


void CExtraContentManager::CreateContentLocksWidgets(bkBank* parentBank)
{
	if (ms_pBankContentLockGroup)
		parentBank->Remove(*ms_pBankContentLockGroup);

	ms_pBankContentLockGroup = parentBank->AddGroup("Content Locks",false);
	bkList::ClickItemFuncType contentLockDblClickCB;
	contentLockDblClickCB.Reset<CExtraContentManager, &CExtraContentManager::ContentLockListDblClickCB>(this);

	ms_pBankContentLockGroup->AddText("Indices:", m_contentLockCountString, sizeof(m_contentLockCountString), true);
	ms_pBankContentLockGroup->AddText("Lookup:", m_contentLockSearchString, sizeof(m_contentLockSearchString), false, datCallback(MFA(CExtraContentManager::LookupContentLock), (datBase*)this));

	ms_contentLocksList = ms_pBankContentLockGroup->AddList("Content Locks (double click to toggle lock)");
	ms_contentLocksList->SetDoubleClickItemFunc(contentLockDblClickCB);
	ms_contentLocksList->AddColumnHeader(0, "Name", bkList::STRING);
	ms_contentLocksList->AddColumnHeader(1, "Locked", bkList::STRING);
	ms_contentLocksList->AddColumnHeader(2, "Callback Index", bkList::STRING);

	ms_pBankContentLockGroup->AddButton("<< Previous Page", &ShowPreviousContentLockPage);
	ms_pBankContentLockGroup->AddButton("Next Page >>", &ShowNextContentLockPage);
	DisplayContentLocks(0);
}

void CExtraContentManager::Bank_UpdateSpecialTriggers(eSpecialTrigger trigger)
{
	SetSpecialTrigger(trigger, !GetSpecialTrigger(trigger));
}

void CExtraContentManager::CreateSpecialTriggersWidgets(bkBank* parentBank)
{
	if (ms_pSpecialTriggersGroup)
		parentBank->Remove(*ms_pSpecialTriggersGroup);

	ms_pSpecialTriggersGroup = parentBank->AddGroup("Special Triggers",false);
	ms_pSpecialTriggersGroup->AddSlider("Special Triggers: ", EXTRACONTENT.BANK_GetSpecialTriggers(), 0, 0, 0);
	ms_pSpecialTriggersGroup->AddButton("Toggle ST_XMAS", datCallback(MFA1(CExtraContentManager::Bank_UpdateSpecialTriggers), &EXTRACONTENT, (void*)ST_XMAS));
	//bank.AddButton("Simplify 10%", datCallback(MFA1(demeshView::SimplifyPercentCb), this, (void*)10));
}

#endif // __BANK

class MetricCCRC : public MetricPlayStat
{
	RL_DECLARE_METRIC(CODE_CRC, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);
};

#if RSG_ORBIS
void CExtraContentManager::ContentPoll(void* UNUSED_PARAM(ptr))
{
	// TODO: Change this for an event when Sony add one.
	// Poll because SCE_SYSTEM_SERVICE_EVENT_ENTITLEMENT_UPDATE only happens when entitlements change not each time DLC is installed.
	// https://ps4.scedev.net/forums/thread/33348/
	// This is really lame, I want this code to die and one day soon it will.
	SceNpServiceLabel serviceLabel = 0;
	SceAppContentAddcontInfo packages[SCE_APP_CONTENT_ADDCONT_MOUNT_MAXNUM];
	u32 packageCount = 0;
	s32 errValue = 0;
	s32 prevInstalled = 0;
	s32 currInstalled = 0;

	memset(packages, 0, sizeof(SceAppContentAddcontInfo) * SCE_APP_CONTENT_ADDCONT_MOUNT_MAXNUM);

	while (true)
	{
		if (!PARAM_disableEntitlementCheck.Get() && !PARAM_usecompatpacks.Get() && !PARAM_extracontent.Get())
		{
			errValue = sceAppContentGetAddcontInfoList(serviceLabel, packages, SCE_APP_CONTENT_ADDCONT_MOUNT_MAXNUM, &packageCount);

			if (Verifyf(errValue == SCE_OK, "Update - Failed to get package count! %i", errValue))
			{
				currInstalled = 0;

				// Assume install only, not accounting for deletion
				for (u32 i = 0; i < packageCount; i++)
				{
					if (packages[i].status == SCE_APP_CONTENT_ADDCONT_DOWNLOAD_STATUS_INSTALLED)
						currInstalled++;
				}

				if (currInstalled != prevInstalled)
					EXTRACONTENT.OnContentDownloadCompleted();

				prevInstalled = currInstalled;
			}
		}

		sysIpcSleep(5000);
	}
}
#endif

void CExtraContentManager::CodeCheck(void* UNUSED_PARAM(ptr))
{
#if RSG_PS3
    const char* codeStart = (const char*)&__start__Ztext[0];
    const s32 codeSize = (s32)(&__stop__Ztext[0] - &__start__Ztext[0]);
#elif RSG_XENON
    const char* codeStart = (const char*)&__start__Ztext;
    const s32 codeSize = (s32)(&__stop__Ztext - &__start__Ztext);
#endif

#if RSG_PS3 || RSG_XENON
    sysTimer timer;
    const u32 initialKey = fwKeyGen::GetKey(codeStart, codeSize);
    Displayf("[CODE_CHECK] Initial code crc duration: %fs, code: %d", timer.GetTime(), initialKey);

    u32 testKey = 0;

	if (NetworkInterface::IsGameInProgress())
	{
		StatId crcStat("CODE_CRC");
		if (StatsInterface::IsKeyValid(crcStat)) //are stats loaded?
			StatsInterface::SetStatData(crcStat, initialKey);
	}
	else
	{
		StatId crcStat("SP_CODE_CRC");
		if (StatsInterface::IsKeyValid(crcStat)) //are stats loaded?
			StatsInterface::SetStatData(crcStat, initialKey);
	}

    const u32 numBatches = 8;
    const u32 batchSize = ((codeSize / numBatches) + 3) & ~3;
    const u32 lastBatchSize = codeSize - (batchSize * (numBatches - 1));
    const u32 timeBetweenChecks = 1 * 60 * 1000; // run check every 1 min
    u32 numBatch = 8;
    u32 nextCheckTime = fwTimer::GetTimeInMilliseconds() + timeBetweenChecks;
    while (s_codeCheckRun)
	{
        if (numBatch < numBatches)
		{
            if (CNetwork::IsGameInProgress())
			{
				// do next batch
				timer.Reset();
				Displayf("[CODE_CHECK] Doing batch %d of %d...", numBatch + 1, numBatches);

				if (numBatch == 0)
					testKey = fwKeyGen::GetKey(codeStart, batchSize);
				else if (numBatch == numBatches - 1)
					testKey = fwKeyGen::AppendDataToKey(testKey, codeStart + (batchSize * numBatch), lastBatchSize);
				else
					testKey = fwKeyGen::AppendDataToKey(testKey, codeStart + (batchSize * numBatch), batchSize);

				dlcDisplayf("[CODE_CHECK] Batch %d of %d completed in %fs, key: %d", numBatch + 1, numBatches, timer.GetTime(), testKey);
				numBatch++;

				if (numBatch == numBatches)
				{
					nextCheckTime = fwTimer::GetTimeInMilliseconds() + timeBetweenChecks;
					if (testKey != initialKey)
					{
						if (!s_codeCompromised)
						{
							MetricCCRC m;
							CNetworkTelemetry::AppendMetric(m);
						}

						Warningf("[CODE_CHECK] Code crc verification failed! Initial key: %d, latest key: %d", initialKey, testKey);
						s_codeCompromised = true;
					}
				}
			}
		}
		else
		{
			u32 curTime = fwTimer::GetTimeInMilliseconds();
			if (nextCheckTime < curTime)
                numBatch = 0;
		}

        sysIpcSleep(1000);
	}
#endif
}

bool CExtraContentManager::IsCodeCompromised()
{
    return s_codeCompromised;
}

#if GTA_REPLAY

void CExtraContentManager::SetReplayState(const u32 *extraContentHashes, u32 hashCount)
{
	SYS_CS_SYNC(m_replayLock);
	Assert(extraContentHashes);
	Assert(hashCount < MAX_EXTRACONTENTHASHES);
	m_replayChangeSetHashCount = Min(hashCount, MAX_EXTRACONTENTHASHES);
	memcpy(m_replayChangeSetHashes, extraContentHashes, m_replayChangeSetHashCount * sizeof(u32));
}

void CExtraContentManager::ResetReplayState()
{
	SYS_CS_SYNC(m_replayLock);
	memset(m_replayChangeSetHashes, 0, MAX_EXTRACONTENTHASHES * sizeof(u32));
	m_replayChangeSetHashCount = 0;
}

void CExtraContentManager::ExecuteReplayMapChanges()
{
	SYS_CS_SYNC(m_replayLock);

	atArray<u32> activeMapChangeHashes;
	GetMapChangeArray(activeMapChangeHashes);

	bool diff = activeMapChangeHashes.GetCount() != (int) m_replayChangeSetHashCount;

	if(!diff)
	{
		for(int i = 0; i < activeMapChangeHashes.GetCount(); i++)
		{
			if(activeMapChangeHashes[i] != m_replayChangeSetHashes[i])
			{
				diff = true;
				break;
			}
		}
	}

	if(!diff)
		return;

	RevertCurrentMapChanges();

	for(u32 i = 0; i < m_replayChangeSetHashCount; i++)
	{
		u32 changeSetHash = m_replayChangeSetHashes[i];

		if(CMountableContent *c = CMountableContent::GetContentForChangeSet(changeSetHash))
		{
			if(u32 group = c->GetChangeSetGroup(changeSetHash))
			{
				Assert((group == CCS_GROUP_MAP) || (group == CCS_GROUP_MAP_SP));
				ExecuteContentChangeSetInternal(c, group, changeSetHash, ECCS_FLAG_USE_LOADING_SCREEN);
			}
		}
	}

	if(m_replayChangeSetHashCount > 0)
	{
		CMountableContent::CleanupAfterMapChange();
	}
}

void CExtraContentManager::RevertCurrentMapChanges()
{
	atArray<u32> activeMapChangeHashes;
	GetMapChangeArray(activeMapChangeHashes);

	for(u32 i = 0; i < activeMapChangeHashes.GetCount(); i++)
	{
		u32 changeSetHash = activeMapChangeHashes[i];

		if(CMountableContent *c = CMountableContent::GetContentForChangeSet(changeSetHash))
		{
			if(u32 group = c->GetChangeSetGroup(changeSetHash))
			{
				Assert((group == CCS_GROUP_MAP) || (group == CCS_GROUP_MAP_SP));
				RevertContentChangeSetInternal(c, group, changeSetHash, CDataFileMgr::ChangeSetAction::CCS_ALL_ACTIONS, ECCS_FLAG_USE_LOADING_SCREEN);
			}
		}
	}
}

#endif // GTA_REPLAY


