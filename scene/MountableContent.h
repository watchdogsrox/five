//
// filename: MountableContent.h
// description: Contains all the required information for mounting a content package
//

#ifndef __MOUNTABLE_CONTENT_H__
#define __MOUNTABLE_CONTENT_H__

#include "file/device_crc.h"
#include "ExtraContentDefs.h"
#if RSG_ORBIS
#include <app_content.h>
#endif

namespace rage {
class fiDeviceRelative;
};

struct ExecutionConditions;

class CMountableContent
{
public:
	friend class CExtraContentManager;

	enum eDeviceType { DT_INVALID = 0, DT_FOLDER, DT_PACKFILE, DT_XCONTENT, DT_DLC_DISC, DT_NUMTYPES };
	enum eContentStatus { CS_NONE = 0, CS_SETUP_READ, CS_FULLY_MOUNTED, CS_CORRUPTED, CS_INVALID_SETUP, CS_SETUP_MISSING, CS_UPDATE_REQUIRED, CS_COUNT };
	enum eMountResult { MR_MOUNTED = 0, MR_INIT_FAILED, MR_MOUNT_FAILED, MR_CONTENT_CORRUPTED };

	CMountableContent();
	~CMountableContent();
	
	bool Setup(s32 userIndex);
	bool Mount(s32 userIndex);

	void ShutdownContent();
	static void CleanupAfterMapChange();

	void GetChangeSetHashesForGroup(u32 changeSetGroup, atArray<atHashString>& setNames);
	void EndRevertActiveChangeSets();
	void StartRevertActiveChangeSets();

	void Reset();

#if __XENON
	void SetXContentData(XCONTENT_DATA* data) { m_xContentData = data; }
#endif

#if RSG_ORBIS
	void SetPS4MountPoint(SceAppContentMountPoint value) { m_ps4MountPoint = value; }
	void SetPS4EntitlementLabel(SceNpUnifiedEntitlementLabel& value) { m_ps4EntitlementLab = value; }
	const SceNpUnifiedEntitlementLabel& GetPS4EntitlementLabel() const { return m_ps4EntitlementLab; }
#endif
	
	void SetFilename(const char* name) { m_filename = name; }
	void SetUsesPackFile(bool value) { m_usesPackfile = value; }
	void SetPrimaryDeviceType(eDeviceType type) { m_deviceType = type; }
	void SetNameHash(const char* name) { m_setupData.m_nameHash = name; }
	void SetDeviceName(const char* name) { m_setupData.m_deviceName = name; }
	void SetECPackFileIndex(s32 value) { m_packFileECIndex = value; }
	void SetStatus(eContentStatus value) { m_status = value; }
	void SetPermanent(bool permanent) { m_permanent = permanent; }
	bool GetIsPermanent() const { return m_permanent; }

	eContentStatus GetStatus() const { return m_status; }
	u32 GetNameHash() const { return m_setupData.m_nameHash; }
	const char* GetFilename() const { return m_filename.c_str(); }
	const char* GetName() const { return m_setupData.m_nameHash.GetCStr(); }
	const char* GetTimeStamp() const { return m_setupData.m_timeStamp.c_str(); }
	const char* GetDeviceName() const { return m_setupData.m_deviceName.c_str(); }
	eDeviceType GetPrimaryDeviceType() const { return m_deviceType; }
	const char* GetDatFileName() const { return m_setupData.m_datFile.c_str(); }
	bool GetUsesPackfile() const { return m_usesPackfile; }
	bool GetIsShippedContent() const { return m_isShippedContent; }
	void SetIsShippedContent(bool val) { m_isShippedContent = val; }
	bool GetIsDatFileValid() const { return m_setupData.m_datFile.length() > 0; }
	atFinalHashString GetStartupScript() const { return m_setupData.m_startupScript; }
	s32 GetScriptCallStackSize() const { return m_setupData.m_scriptCallstackSize; }
	bool GetIsCompatibilityPack() const { return (m_setupData.m_type == EXTRACONTENT_COMPAT_PACK); }
	bool GetIsPaidPack() const { return (m_setupData.m_type == EXTRACONTENT_PAID_PACK); }
	s32 GetECPackFileIndex() const { return m_packFileECIndex; }
	s32 GetOrder() const { return m_setupData.m_order; }
	atHashString GetDependencyPackHash() const { return m_setupData.m_dependencyPackHash; }
	static const char *GetContentStatusName(eContentStatus status);
	static const char *GetSetupFileName();
	void GetMapChangeHashes(atArray<u32>& mapChangeHashes) const;
	
	bool operator < (const CMountableContent &d) const
	{
		if(m_setupData.m_type < d.m_setupData.m_type)
		{
			return true;
		}
		else if(m_setupData.m_type > d.m_setupData.m_type)
		{
			return false;
		}
		if(m_setupData.m_isLevelPack && d.m_setupData.m_isLevelPack)
		{
			if(m_setupData.m_order==d.m_setupData.m_order)
			{
				return m_setupData.m_minorOrder > d.m_setupData.m_minorOrder;
			}
			return m_setupData.m_order > d.m_setupData.m_order;
		}
			
		return m_setupData.m_order < d.m_setupData.m_order;
	}

	void SetDatFileLoaded(bool loaded) { m_datFileLoaded = loaded; };
	bool IsSetupOnly() const { return m_setupData.IsSetupOnly(); };
	bool IsLevelPack() const { return m_setupData.m_isLevelPack;}
	static void LoadExtraTitleUpdateData(const char *fname);
	static void LoadExtraFolderMountData(const char *fname);
	static void UnloadExtraFolderMountData(const char *fname);
	static bool AddExtraFolderMount(const SExtraFolderMount &mountData);
	static bool RemoveExtraFolderMount(const SExtraFolderMount &mountData);
	static CMountableContent *GetContentForChangeSet(u32 hash);
	static void RegisterExecutionCheck(atDelegate<bool(atHashString)>*  callback,atHashString controlString);
	u32 GetChangeSetGroup(u32 changeSet);

#if __BANK
public:
	bool HasAnyChangesets();
	void SetBankShouldExecute(bool value) { m_bankShouldExecute = value; };
	bool GetBankShouldExecute() const { return m_bankShouldExecute; };
	void GetChangesetGroups(atArray<const char*>& groupNames);
	void GetChangeSetsForGroup(u32 changeSetGroup, atArray<const char*>& setNames);
	u32 GetTotalContentChangeSetCount() const;
	u32 GetActiveChangeSetCount() const { return m_activeChangeSets.GetCount(); }
private:
	bool m_bankShouldExecute;
#endif

private:

	void ExecuteContentChangeSet(atHashString groupNameHash, atHashString ccsHashName);
	void RevertContentChangeSet(atHashString groupNameHash, atHashString ccsHashName, u32 actionMask);

	bool CheckContentChangeSetConditions(const ExecutionConditions* conditions);

	eMountResult MountContent(s32 userIndex, const char* deviceName);
	void UnmountContent(const char* deviceName);
	void UnmountPlatform();
	bool LoadSetupXML(const char* path);
	void InitCrcDevice(const char* deviceName,  fiDevice* parentDevice, bool isTu = false);
	void DeleteDevice();
	void DeleteCrcDevice();
	const ContentChangeSetGroup *FindContentChangeSetGroup(atHashString groupNameHash) const { return m_setupData.FindContentChangeSetGroup(groupNameHash); }
	void EnsureDatFileLoaded();
	void UnloadDatFile();

	void SetChangeSetActive(atHashString groupName, atHashString hashName, bool active);
	bool GetChangeSetActive(atHashString groupName, atHashString hashName);

	static const SExtraTitleUpdateMount *FindExtraTitleUpdateMount(const char *deviceName);
	void PatchSetup();
	void MountTitleUpdateDevice(const char *deviceName);
	void UnmountTitleUpdateDevice();

	bool UpdateRequired(const char* currentVer, const char* requiredVer) const;
	void ParseVersionString(const char* cc, s32& majVer, s32& minVer) const;

	void RegisterContentChangeSets();
	void UnregisterContentChangeSets();

	static bool ParseExpression(const char* expression);
	static bool CheckCondition(atHashString registeredCallHash, atHashString control,bool negate);

	eMountResult InitSubPacks(const char* deviceName);
	void ShutdownSubPacks();

#if RSG_ORBIS
	SceAppContentMountPoint m_ps4MountPoint;
	SceNpUnifiedEntitlementLabel m_ps4EntitlementLab;
#endif

#if __PPU
	fiDeviceRelative* m_pDevice;
	fiPackfile* m_pSecondaryDevice;

	void DeleteSecondaryDevice();
#else
	fiDevice* m_pDevice;
#endif
	fiDeviceCrc* m_pCrcDevice;
	fiDeviceCrc* m_pCrcDeviceTU;
	fiDeviceRelative* m_pDeviceTU;

#if __XENON
	XCONTENT_DATA* m_xContentData;
#endif

	atArray<fiPackfile*>	m_subPacks;

	SSetupData m_setupData;
	atString m_filename;
	eDeviceType m_deviceType;
	s32 m_packFileECIndex;
	eContentStatus m_status;
	atArray<SActiveChangeSet> m_activeChangeSets;
	bool m_datFileLoaded : 1;
	bool m_usesPackfile : 1;
	bool m_permanent : 1;
	bool m_isShippedContent : 1;

	static bool sm_mapChangeDirty;
	static atBinaryMap<atDelegate<bool(atHashString)>*,atHashString> sm_genericConditionChecks;

	// Title update patching data
	static SExtraTitleUpdateData	sm_ExtraTitleUpdateData;
	// Extra folder mounts
	static SExtraFolderMountData	sm_ExtraFolderMountData;
	// content change set to content lookup
	static atMap<u32, CMountableContent*>		sm_changeSetLookup;
};

#endif
