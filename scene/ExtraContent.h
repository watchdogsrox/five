//
// filename: ExtraContent.h
// description:	Class controlling the loading of extra content once game has been released
//

#ifndef INC_EXTRACONTENT_H_
#define INC_EXTRACONTENT_H_

// Rage headers
#include "atl/array.h"
#include "atl/hashstring.h"
#include "atl/singleton.h"
#include "file/limits.h"
#include "file/packfile.h"
#include "rline/rlgamerinfo.h"
#include "system/param.h"
#include "system/system.h"
#include "bank/group.h"
#include "bank/list.h"
#include "atl/delegate.h"
#include "atl/binmap.h"
#include "fwscene/stores/fragmentstore.h"
#include "system/service.h"

// Game headers
#include "network/live/livemanager.h"
#include "MountableContent.h"
#include "script/thread.h"
#include "streaming/streamingrequest.h"
#include "Network/Cloud/CloudManager.h"
#include "Scene/DataFileMgr.h"
#include "system/service.h"
#include "frontend/loadingscreens.h"

#if GTA_REPLAY
#include "control/replay/ReplaySupportClasses.h"
#endif // GTA_REPLAY

namespace rage
{
	class netCloudRequestGetTitleFile;
	class rlPresence;
	class rlPresenceEvent;
	class fiPackfile;
	class fwFragmentStore; 
	class fragType;
};

//DISABLING THIS FOR NOW
#define EC_CLOUD_MANIFEST 0
#if EC_CLOUD_MANIFEST

class CExtraContentCloudListener : public CloudListener
{
public:
	CExtraContentCloudListener();
	~CExtraContentCloudListener();
	void Init();
	void Shutdown();
	bool DoManifestRequest();
	virtual void OnCloudEvent(const CloudEvent* pEvent);
private:
	CloudRequestID m_manifestReqID;
//	bool m_bParsingCloud;
};
#endif

#if __BANK
// Generalized ownership information for data
template <typename T, typename keyT=T>
class COwnershipInfo
{
public:
	static void Add(keyT entry, const char* fileName)
	{
		atHashString& refStr = m_entries[entry];
		refStr.SetFromString(fileName);
	}

	static void AddArray(atArray<T>& refArray, const char* fileName)
	{
		for(int i=0; i<refArray.GetCount(); ++i)
		{
			Add(refArray[i], fileName);
		}
	}

	static void Remove(keyT entry)
	{
		m_entries.Delete(entry);
	}

	static const char* Find(keyT entry)
	{
		if(atHashString *pStr = m_entries.Access(entry))
			return pStr->TryGetCStr();
		else return "";
	}

	static atHashString* Access(keyT entry)
	{
		return m_entries.Access(entry);
	}

	static void Reset()
	{
		m_entries.Reset();
	}

	static bool HasEntryWithHash(const atArray<T>& theArray, const atHashString& fileHash)
	{
		for(s32 i=0; i < theArray.GetCount(); i++)
		{
			if(Found(theArray[i], fileHash))
				return true;
		}
		return false;
	}

	static bool Found(keyT entry, const atHashString& fileHash)
	{
		if(atHashString* pHash = Access(entry))
		{
			if(*pHash == fileHash)
				return true;
		}
		return false;
	}

	static void DumpInfo()
	{
#if !RSG_ORBIS
		Displayf("Dumping ownership info for %s, total entries: %d", GetRttiName(&m_entries), m_entries.GetNumUsed());
		atMap<keyT, atHashString>::Iterator it = m_entries.CreateIterator();
		it.Start();
		while(!it.AtEnd())
		{
			Displayf("Entry %u, fileName %s", it.GetKey(), it.GetData().TryGetCStr());
			it.Next();
		}
#endif //!RSG_ORBIS
	}

private:
	static atMap<keyT, atHashString> m_entries;
};

template <typename T, typename keyT> 
atMap<keyT, atHashString>COwnershipInfo<T, keyT>::m_entries;

#endif // __BANK

//////////////////////////////////////////////////////////////////////////
struct SLoadingScreenState
{
	SLoadingScreenState()
	{
		Reset();
	}

	void Reset()
	{
		m_modified = false;
		m_displayed = false;
		m_doneAPostFrame = false;
	}

	bool m_modified : 1; // Did we change anything?
	bool m_displayed : 1; // Did we display them?
	bool m_doneAPostFrame : 1; // Have we done one whole frame after the load?
};

#if RSG_DURANGO
struct SPendingCSAction
{
	SPendingCSAction() { Reset(); }
	SPendingCSAction(atHashString content, atHashString group, atHashString changeSet, u32 flags, u32 actions, bool execute, bool reqLastFrame) 
		: m_content(content), m_group(group), m_changeSet(changeSet), m_flags(flags), m_actions(actions), m_execute(execute), m_requiesLastFrame(reqLastFrame)
	{ }

	void Reset()
	{
		m_content.Clear();
		m_group.Clear();
		m_changeSet.Clear();
		m_flags = 0;
		m_execute = false;
		m_actions = 0;
		m_requiesLastFrame = false;
	}

	atHashString m_content;
	atHashString m_group;
	atHashString m_changeSet;
	u32 m_flags;
	u32 m_actions;
	bool m_execute;
	bool m_requiesLastFrame;
};
#endif

class CExtraContentManager : public datBase
{
public:
	enum eSpecialTrigger {
		ST_XMAS				= 1<<0
	};

    void Init(u32 initMode);
	void Shutdown(u32 shutdownMode);
	void ShutdownMetafiles(u32 shutdownMode, eDFMI_UnloadType unloadType);
	void ShutdownMapChanges(u32 shutdownMode);
	void Update();

    void InitProfile();
    void ShutdownProfile();
	static void OnPresenceEvent(const rlPresenceEvent* evt);
	static void OnServiceEvent(sysServiceEvent* evt);

	void OnContentDownloadCompleted();
	void LoadLevelPacks();

	void ExecuteContentChangeSetGroupForAll(u32 changeSetGroup);
	void RevertContentChangeSetGroupForAll(u32 changeSetGroup);
	void ExecuteContentChangeSet(u32 contentHash, u32 changeSetGroup, u32 changeSetName);
	void RevertContentChangeSet(u32 contentHash, u32 changeSetGroup, u32 changeSetName);
	void ExecuteContentChangeSetGroup(u32 contentHash, u32 changeSetGroup);
	void RevertContentChangeSetGroup(u32 contentHash, u32 changeSetGroup);

	bool GenericChangesetChecks(atHashString condition);
	bool LevelChecks(atHashString condition);
	bool ParseExpression(const char* expr);

	void InsertContentLock(u32 hash, s16 unlockedCBIndex, bool locked);
	void RemoveContentLock(u32 hash);
	bool IsContentItemLocked(u32 hash) const;
	void ModifyContentLockState(u32 hash, bool locked);
	s16 AddOnContentItemChangedCB(atDelegate<void (u32, bool)> onChangedCB);

    //	For saving to MU/Hard Drive
    bool Serialize() const { return true; }
    bool Deserialize() const { return true; }

	int GetMissingCompatibilityPacks(atArray<u32> &packNameHashes) const;
	bool CheckCompatPackConfiguration() const;
	bool VerifySaveGameInstalledPackages() const;
	void ResetSaveGameInstalledPackagesInfo();
	void SetSaveGameInstalledPackage(u32 nameHash);

	void SetMapChangeState(eMapChangeStates value) { m_currMapChangeState = value; };
	eMapChangeStates GetMapChangeState() const { return m_currMapChangeState; }

	int GetCorruptedContentCount() const { return m_corruptedContent.GetCount(); }
	const char *GetCorruptedContentFileName(int index) const { return m_corruptedContent[index]; }

	void GetMapChangeArray(atArray<u32>& retArray) const;
	u32 GetMapChangesCRC() const;
	u32 GetCRC(u32 initValue = 0) const; // return manager hash. This is necessary for players matchmaking.
	inline u32 GetEpisodeCount() const { return 1; } // TODO: If we are going to have episodes, just place holder now for some legacy code on PC.
	inline bool IsContentPresent(u32 nameHash) { return GetContentByHash(nameHash) != NULL; }
	inline u32 GetContentCount() const { return m_content.GetCount(); }
	u32 GetContentHash(u32 index) const;
	bool GetAllowMapChangeAnytime() const { return m_allowMapChangeAnyTime; }

	CMountableContent* GetContentByDevice(const char* device);

	void ExecuteWeaponPatchMP(bool execute);
	void ExecuteScriptPatch();

	static void MountUpdate();
	static void MountUpdateRpf();
	static void RemountUpdate();
	void ExecuteTitleUpdateDataPatch(u32 changeSetHash, bool execute);
	void ExecuteTitleUpdateDataPatchGroup(u32 changeSetHash, bool execute);
	bool AreAnyCCSPending();

#if EC_CLOUD_MANIFEST
public:
	void UpdateCloudStorage();
	void SetCloudManifest(const void* const & data, u32 size);
	void SetCloudManifestState(eCloudFileState state);
	void EnsureManifestLoaded();
	bool GetCloudContentRequestsFinished();
	bool HasCloudContentSuccessfullyLoaded();
	eDlcCloudResult  GetCloudContentResult();
	eDlcCloudResult GetCloudFileState(eCloudFileState state);
	bool GetCloudContentState(int &bTimedOut, int uWaitDuration);
	void BeginEnumerateCloudContent();
	void OnReceivedExtraContentManifest();
	void UpdateCloudCacher();
private:
	bool LoadCloudManifestFromCommandLine();
#if !__FINAL
	void DebugDumpCloudManifestContents();
#endif
	SCloudExtraContentData m_cloudData;
	u32 m_cloudStatusTimer;
	bool m_cloudTimedOut : 1;
#endif
	bool GetEverHadBadPackOrder() const { return m_everHadBadPackOrder; }

	void LoadOverlayInfo(const char* fileName);
	void UnloadOverlayInfo(const char* fileName);
	void UpdateOverlayInfo(sOverlayInfo &overlayInfo, const char* fileName);
	void RemoveOverlayInfo(sOverlayInfo &overlayInfo);
	void ExecutePendingOverlays();

	enum { ECCS_FLAG_USE_LATEST_VERSION = BIT(0), ECCS_FLAG_USE_LOADING_SCREEN = BIT(1), ECCS_FLAG_MAP_CLEANUP = BIT(2) };

	void ExecuteContentChangeSetGroupInternal(CMountableContent* content, atHashString changeSetGroup);
	void RevertContentChangeSetGroupInternal(CMountableContent* content, atHashString changeSetGroup, u32 actionMask);
	void ExecuteContentChangeSetInternal(CMountableContent* content, atHashString changeSetGroup, atHashString changeSetName, u32 flags DURANGO_ONLY(, bool addPending=true));
	void RevertContentChangeSetInternal(CMountableContent* content, atHashString changeSetGroup, atHashString changeSetName, u32 actionMask, u32 flags DURANGO_ONLY(, bool addPending=true));

	void			SetSpecialTrigger(eSpecialTrigger trigger, bool val)	{
		if (val) 
		{m_specialTriggers |= trigger;}
		else
		{ m_specialTriggers &= (~trigger); }
	}

	const bool		GetSpecialTrigger(eSpecialTrigger trigger)	{ return((m_specialTriggers & trigger) != 0); }

	bool GetPartOfCompatPack(const char* file);

#if __BANK
	bkCombo* AddWorkingDLCCombo(bkBank* bank);
	void CurrentWorkingDLCChanged();
	bool GetAssetPathFromDevice(char* deviceName);
	void GetAssetPathForWorkingDLC(char* deviceName);
	void GetWorkingDLCDeviceName(char* deviceName);
	void Bank_UpdateContentDisplay();
	void Bank_PrintActiveMapChanges();
	void Bank_ExecuteBankUpdate();
	bool BANK_EnableCloudWidgetsOverride();
	bool BANK_GetForceLoadingScreensState(){return ms_bForcedLoadingScreenStatus;}
	bool BANK_GetForceCompatState(){return ms_bForcedCompatPackStatus;}
	bool BANK_GetForceCloudFileState(){return ms_bForcedCloudFilesStatus;}
	bool BANK_GetForceManifestFileState(){return ms_bForcedManifestStatus;}
	u32* BANK_GetSpecialTriggers()								{ return(&m_specialTriggers); }
#endif // __BANK

#if !__NO_OUTPUT
	void PrintState() const;
#endif // __FINAL

    static bool IsCodeCompromised();

#if GTA_REPLAY
	void SetReplayState(const u32 *extraContentHashes, u32 hashCount);
	void ResetReplayState();
	void ExecuteReplayMapChanges();
#endif // GTA_REPLAY

private:

	void UpdateMapChangeState();
	void TriggerLoadingScreen(LoadingScreenContext loadingContext);
	void CloseLoadingScreen();
	
#if !__FINAL
	bool IsContentIgnored(u32 nameHash);
#endif

	void BeginEnumerateContent(bool immediate, bool earlyStartup);
	void CancelEnumerateContent(bool bWait);
	bool EndEnumerateContent(bool bWait, bool earlyStartup);
	void LoadContent(bool executeChangeSet = false, bool executeEarlyStartup = false);
	void UpdateRebootMessage();
	bool AddContentFolder(const char *path);
#if RSG_DURANGO
	void EnumerateDurangoContent();
	void UpdateContentArrayDurango();
	bool AddDurangoContent(const char *path);
	void InitDurangoContent();
	void ShutdownDurangoContent();
public:
	static void OnDurangoContentDownloadCompleted();
private:
#endif // RSG_DURANGO

	atHashString GetVersionedChangeSetName(atArray<sOverlayInfo*>& overlayInfos, atHashString changeSetName);
	sOverlayInfo* GetHighestOverlayInfoForVersionedChange(atArray<sOverlayInfo*>& overlayInfos, atHashString versionedChange);

	bool IsContentFilenamePresent(const char* fileName) const;
#if RSG_ORBIS
	bool IsContentFilenamePresent(atHashString& label) const;
#endif
    s32 AddContent(CMountableContent& desc);
	void ProcessEnumerationRequests(bool immediate);
	void ProcessDeviceChangedMessages();
    void DeviceChanged(bool bSuccess);
    void UpdateContentArray();

#if GTA_REPLAY
	void RevertCurrentMapChanges();
#endif // GTA_REPLAY

#if __PPU
	void UpdateContentArrayPs3(const char *path);
#endif // __PPU 

#if RSG_PC
	void EnumerateContentPC();
#endif // RSG_PC

	CMountableContent* GetContentByHash(u32 nameHash);
	const CMountableContent* GetContentByHash(u32 nameHash) const;
	const CMountableContent* GetContentByHashImpl(u32 nameHash) const;
	CMountableContent* GetContent(u32 index);
	const CMountableContent* GetContent(u32 index) const;
	bool IsContentDuplicate(const CMountableContent *content) const;
#if EC_CLOUD_MANIFEST
	CExtraContentCloudListener m_cloudListener;
#endif

#if __XENON
	void BeginDeviceEnumeration();
#endif

#if !__FINAL
    int RecursiveEnumExtraContent(const char *searchPath);
#endif

	void InitialiseSpecialTriggers();
	void UpdateSpecialTriggers();

	static const char *GetPlatformTitleUpdatePath();
	static const char *GetPlatformTitleUpdate2Path();
	static const char *GetAudioTitleUpdatePath();

#if RSG_ORBIS
	static void ContentPoll(void* ptr);
#endif

    static void CodeCheck(void* ptr);

	static void NormalizePath(char *src_path, bool trailingSlash = true);

#if __BANK
	void InitBank();
	static void CreateBankWidgets();

	void CreateContentStatusWidgets(bkBank* parentBank);
	void CreateSpecialTriggersWidgets(bkBank* parentBank);
	void CreateUnusedFilesWidgets(bkBank* parentBank);
		
	static void ShowPreviousContentLockPage();
	static void ShowNextContentLockPage();
	void CreateContentLocksWidgets(bkBank* parentBank);
	void AddContentLockToBankList(u32 hash, SContentLockData* currentItem, s32 index);
	void DisplayContentLocks(s32 nextPage, u32 lookupItemHash = 0);
	void LookupContentLock();
	void DisplayContentCRC();
	void ContentLockListDblClickCB(s32 hash);
	void PopulateContentTableWidget();
	static void ReturnAssetPathForFragTuneCB(char* path,fragType* frag);

	static void PopulateFileTableWidgets();
	static void PopulateOpenFilesAtPage(u32 pageNum);
	static void PrevPageOfOpenFiles();
	static void NextPageOfOpenFiles();
	static void PopulateUnusedFilesAtPage(u32 pageNum);
	static void PrevPageOfUnusedFiles();
	static void NextPageOfUnusedFiles();

	void Bank_InitMapChange();
	void Bank_EndMapChange();
	void Bank_UpdateMapChangeState();
	void Bank_UpdateActiveMapChangeDisplay();
	void Bank_ExecuteSelectedChangeSet();
	void Bank_RevertSelectedChangeSet();
	void Bank_ExecuteSelectedGroup();
	void Bank_RevertSelectedGroup();
	void Bank_ChangeSetGroupChanged();
	void Bank_ApplyShouldActivateContentChangeState();
	void Bank_ContentListClick(s32 index);
	void Bank_ContentListDoubleClick(s32 index);

	void Bank_UpdateSpecialTriggers(eSpecialTrigger trigger);

    static bkBank* ms_pBank;
	static bkCombo* ms_pSelectedChangeSet;
	static bkCombo* ms_pChangeSetGroups;
	static bkButton *ms_pCreateBankButton;
	static bkToggle *ms_pForcedCloudFilesStatus;
	static bkToggle *ms_pForceCompatPackFail;
	static bkToggle *ms_pForceManifestFail;
	static bkToggle *ms_pForceLoadingScreen;
    static bkGroup* ms_pBankContentStateGroup;
	static bkGroup* ms_pBankContentLockGroup;
	static bkGroup* ms_pBankExtraContentGroup;
	static bkGroup* ms_pSpecialTriggersGroup;
	static bkList* ms_pBankContentTable;
	static bkList* ms_pBankVersionedContentTable;
	
	static bkGroup* ms_pBankFilesGroup;
	static bkList* ms_pBankInUseFilesTable;
	static bkList* ms_pBankUnusedFilesTable;
	static bkButton* ms_pBankFileListsPopulateButton;
	static bkButton *ms_pBankInUsePrevButton;
	static bkButton *ms_pBankInUseNextButton;
	static bkButton *ms_pBankUnusedPrevButton;
	static bkButton *ms_pBankUnusedNextButton;

    static int ms_selectedContentIndex;
	static int ms_selectedChangeSetIndex;
	static int ms_changeSetGroupIndex;
	static int ms_displayedPackageCount;
	static int ms_displayedVersionedChangesetCount;
	static bkList* ms_contentLocksList;
	static s32 m_currentContentLocksDebugPage;
	static char m_contentLockSearchString[RAGE_MAX_PATH];
	static char m_contentLockCountString[RAGE_MAX_PATH];
	static char m_matchMakingCrc[RAGE_MAX_PATH];
	static bool ms_bForcedManifestStatus;
	static bool ms_bForcedCloudFilesStatus;
	static bool ms_bForcedCompatPackStatus;
	static bool ms_bForcedLoadingScreenStatus;
	static bool ms_bDoBankUpdate;
	static char m_activeMapChange[RAGE_MAX_PATH];
	static char m_currMapChangeStateStr[RAGE_MAX_PATH];
	static atArray<const char *>	m_workingDLC;							// for combo box
	static int						m_workingDLCIndex;
#endif

private:
	atArray<sOverlayInfo*> m_overlayInfo;
	atArray<sOverlayInfo*> m_pendingOverlays;
	atArray<CMountableContent> m_content;
	atArray<atString> m_corruptedContent;
	atArray<u32> m_savegameInstalledPackages;
	atBinaryMap<SContentLockData, u32> m_contentLocks;
	atFixedArray<atDelegate<void (u32, bool)>, SContentLockData::MAX_CONTENT_LOCK_SUBSYSTEM_CBS> m_onContentLockStateChangedCBs;	
	rlGamerId m_currentGamerId;	
	s32 m_localGamerIndex;
	eMapChangeStates m_currMapChangeState;
	SLoadingScreenState m_loadingScreenState;
#if __XENON
	HANDLE m_downloadContentListener;
	bool m_deviceEnumerating : 1;	
#endif
	bool m_allowMapChangeAnyTime;
	bool m_enumerating : 1;
	bool m_enumerateOnUpdate : 1;
	bool m_everHadBadPackOrder : 1;
	bool m_specialTriggerTunablesTested : 1;
	u32 m_specialTriggers;
#if RSG_ORBIS
	atArray<atHashString> m_ps4DupeEntitlements;
#endif
#if RSG_DURANGO
	atArray<SPendingCSAction> m_pendingActions;
	bool m_waitingForBBCopy;
#endif

#if !__FINAL
	bool m_enumerateCommandLines;
#endif

#if GTA_REPLAY
	sysCriticalSectionToken	m_replayLock;
	u32 m_replayChangeSetHashCount;
	u32 m_replayChangeSetHashes[MAX_EXTRACONTENTHASHES];
#endif // GTA_REPLAY
};

// wrapper class needed to interface with game skeleton code
class CExtraContentWrapper
{
public:
	static void Init(u32 shutdownMode);
    static void Shutdown(u32 shutdownMode);
	static void ShutdownStart(u32 shutdownMode);
    static void Update();
};

typedef atSingleton<CExtraContentManager> CExtraContentManagerSingleton;

#define EXTRACONTENT CExtraContentManagerSingleton::InstanceRef()

#define INIT_EXTRACONTENT										\
	do {														\
		if (!CExtraContentManagerSingleton::IsInstantiated())	\
		{														\
			CExtraContentManagerSingleton::Instantiate();		\
			EXTRACONTENT.Init(INIT_CORE);						\
		}														\
	} while(0)													\

#endif // !INC_EXTRACONTENT_H_
