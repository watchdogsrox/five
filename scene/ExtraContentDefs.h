//
// filename: ExtraContentDefs.h
// description: A place for all the classes/structs associated with ExtraContent to live without cluttering the header
//

#ifndef __EXTRACONTENT_DEFS_H__
#define __EXTRACONTENT_DEFS_H__

#include "atl/array.h"
#include "atl/hashstring.h"
#include "file/packfile.h"
#include "streaming/packfilemanager.h"
#include "streaming/streaminginstall_psn.h"
#include "fwnet/netcloudfiledownloadhelper.h"
#include "Network/Cloud/CloudManager.h"
#include "script/thread.h"
#include "streaming/streamingrequest.h"

#if __XENON
#include "system/device_xcontent.h"
#include "system/xtl.h"
#endif

#define DEFINE_CCS_GROUP(enumName, stringName, hash) enumName = hash,

#define CCS_TITLES \
	DEFINE_CCS_GROUP(CCS_GROUP_VERSIONINFO, "GROUP_VERSIONINFO", 0x5F48ECE7) \
	DEFINE_CCS_GROUP(CCS_GROUP_EARLY_ON, "GROUP_EARLY_ON", 0x6F8BD408) \
	DEFINE_CCS_GROUP(CCS_GROUP_STARTUP, "GROUP_STARTUP", 0x5BF6578F) \
	DEFINE_CCS_GROUP(CCS_GROUP_MAP, "GROUP_MAP", 0xBCC89179) \
	DEFINE_CCS_GROUP(CCS_GROUP_MAP_SP, "GROUP_MAP_SP", 0x578F99E2) \
	DEFINE_CCS_GROUP(CCS_GROUP_ON_DEMAND, "GROUP_ON_DEMAND", 0x9EA3E635) \
	DEFINE_CCS_GROUP(CCS_GROUP_TITLEUPDATE_STARTUP, "GROUP_TITLEUPDATE_STARTUP", 0x2487ED35) \
	DEFINE_CCS_GROUP(CCS_GROUP_UPDATE_STREAMING, "GROUP_UPDATE_STREAMING", 0x93DAE61B) \
	DEFINE_CCS_GROUP(CCS_GROUP_UPDATE_TEXT, "GROUP_UPDATE_TEXT", 0x3226FD26) \
	DEFINE_CCS_GROUP(CCS_GROUP_UPDATE_DLC_PATCH, "GROUP_UPDATE_DLC_PATCH", 0xCE38605F) \
	DEFINE_CCS_GROUP(CCS_GROUP_UPDATE_DLC_METADATA, "GROUP_UPDATE_DLC_METADATA", 0x6EF87BF5) \
	DEFINE_CCS_GROUP(CCS_GROUP_UPDATE_WEAPON_PATCH, "GROUP_UPDATE_WEAPON_PATCH", 0xA390DD) \
	DEFINE_CCS_GROUP(CCS_GROUP_POST_DLC_PATCH, "GROUP_POST_DLC_PATCH", 0x69205EB1) \
	DEFINE_CCS_GROUP(CCS_TITLE_UPDATE_STARTUP, "CCS_TITLE_UPDATE_STARTUP", 0x1C3BC144) \
	DEFINE_CCS_GROUP(CCS_TITLE_UPDATE_STREAMING, "CCS_TITLE_UPDATE_STREAMING", 0x19E536C7) \
	DEFINE_CCS_GROUP(CCS_TITLE_UPDATE_TEXT, "CCS_TITLE_UPDATE_TEXT", 0xA00A2F2B) \
	DEFINE_CCS_GROUP(CCS_TITLE_UPDATE_WEAPON_PATCH, "CCS_TITLE_UPDATE_WEAPON_PATCH", 0xAA73C8E9) \
	DEFINE_CCS_GROUP(CCS_TITLE_UPDATE_DLC_PATCH, "CCS_TITLE_UPDATE_DLC_PATCH", 0xEEAF512A) \
	DEFINE_CCS_GROUP(CCS_TITLE_UPDATE_DLC_METADATA, "CCS_TITLE_UPDATE_DLC_METADATA", 0x9869E220) \

enum eChangeSetTitles
{
	CCS_TITLES
};

enum eCloudFileState
{
	CLOUDFILESTATE_UNCACHED			= BIT(0),
	CLOUDFILESTATE_CACHING			= BIT(1),
	CLOUDFILESTATE_TRANSFER_ERROR	= BIT(2),
	CLOUDFILESTATE_NOT_FOUND		= BIT(3),
	CLOUDFILESTATE_CACHED			= BIT(4)
};


enum eDlcCloudResult
{
	DLCRESULT_NOT_READY = -1,
	DLCRESULT_OK,
	DLCRESULT_CONNECTION_ERROR,
	DLCRESULT_TIMEOUT,
	DLCRESULT_COUNT
};

#define TITLE_UPDATE_PACK_NAME "TitleUpdate"
class CMountableContent;
struct sOverlayInfo
{
	sOverlayInfo() : m_version(0),m_state(INACTIVE), m_content(0) { m_changeSet.SetHash(0); m_changeSetGroupToExecuteWith = (u32)CCS_GROUP_ON_DEMAND;}
	u32 m_content;
	atHashString m_nameId;
	atHashString m_changeSet;
	atHashString m_changeSetGroupToExecuteWith;
	enum State
	{
		INACTIVE,
		WILL_ACTIVATE,
		ACTIVE,
		NUM_OVERLAY_STATES,
	};
	State m_state;
	s8 m_version;
	PAR_SIMPLE_PARSABLE;
};

struct sOverlayInfos
{
	sOverlayInfos(){m_overlayInfos.Reset();}
	atArray<sOverlayInfo> m_overlayInfos;

	PAR_SIMPLE_PARSABLE;
};

struct sWorldPointScriptBrains
{
	atFinalHashString m_ScriptName;
	float m_ActivationRange;
	s32 m_setToWhichThisBrainBelongs;

	PAR_SIMPLE_PARSABLE;
};

struct sScriptBrainLists
{
	atArray<sWorldPointScriptBrains> m_worldPointBrains;

	PAR_SIMPLE_PARSABLE;
};

struct CExtraTextMetaFile
{
	CExtraTextMetaFile() : m_hasGlobalTextFile(false), m_hasAdditionalText(false),m_isTitleUpdate(false),m_deviceName(){}
	atString m_deviceName;
	bool m_hasAdditionalText;
	bool m_hasGlobalTextFile;
	bool m_isTitleUpdate;
	PAR_SIMPLE_PARSABLE;
};

struct CCloudStorageFile
{
	CCloudStorageFile()	: m_State(CLOUDFILESTATE_UNCACHED), m_requestID(INVALID_CLOUD_REQUEST_ID), m_lastRequestTime(0)	{ }

	// The filename of this file (no path)
	atString m_File;

	// The path we'll mount this RPF file to, like "dlc5:".
	atString m_MountPoint;

	// The full path for the packfile on our local cache drive.
	atString m_LocalPath;

	// The packfile structure for this RPF.
	fiPackfile *m_Packfile;

	CloudRequestID m_requestID;

	u64 m_lastRequestTime;
	// The current state of this file
	eCloudFileState m_State;

	PAR_SIMPLE_PARSABLE;
};

struct CExtraContentCloudPackDescriptor
{
	atString	m_Name;
	atString	m_ProductId;

	PAR_SIMPLE_PARSABLE;
};

struct CExtraContentCloudManifest
{
	CExtraContentCloudManifest()	: m_State(CLOUDFILESTATE_UNCACHED) {}
	atArray<CCloudStorageFile>					m_Files;
	atArray<CExtraContentCloudPackDescriptor>	m_CompatibilityPacks;
	atArray<CExtraContentCloudPackDescriptor>	m_PaidPacks;
	atString									m_WeaponPatchNameMP;
	atString									m_ScriptPatchName;
	eCloudFileState m_State;

	PAR_SIMPLE_PARSABLE;
};

struct SCloudExtraContentData
{
	netCloudRequestGetTitleFile* m_CloudManifestFileRequest; // This is the request to download the master file with all the available DLC.	
	fiHandle m_CloudTransferHandle; // Handle of the file being transferred from the cloud
	netStatus m_CloudTransferStatus;
	int m_CloudTransfers; // Number of files currently being transferred from the cloud
	CExtraContentCloudManifest m_CloudManifest;
};

struct SContentUnlocks
{
	atArray<atHashString> m_listOfUnlocks;

	PAR_SIMPLE_PARSABLE;
};

enum eExtraContentPackType
{
	EXTRACONTENT_TU_PACK,
	EXTRACONTENT_LEVEL_PACK,
	EXTRACONTENT_COMPAT_PACK,
	EXTRACONTENT_PAID_PACK
};

struct ContentChangeSetGroup
{
	atHashString m_NameHash;	
	atArray<atHashString> m_ContentChangeSets;

	bool Contains(atHashString changeSetName) const
	{
		for(int i = 0; i < m_ContentChangeSets.GetCount(); i++)
		{
			if(m_ContentChangeSets[i] == changeSetName)
				return true;
		}
		
		return false;
	}

	PAR_SIMPLE_PARSABLE;
};

struct SActiveChangeSet
{
	SActiveChangeSet() { }
	SActiveChangeSet(atHashString group, atHashString ccsName) : m_group(group), m_changeSetName(ccsName) { }

	atHashString m_group;
	atHashString m_changeSetName;
};

struct SSetupData
{
	atString m_deviceName;
	atString m_datFile;
	atString m_timeStamp;
	atFinalHashString m_nameHash;
	atArray<atHashString> m_contentChangeSets;
	atArray<ContentChangeSetGroup> m_contentChangeSetGroups;
	atFinalHashString m_startupScript;
	s32 m_scriptCallstackSize;
	eExtraContentPackType m_type;
	s32	m_order;
	s32 m_minorOrder;
	atHashString m_dependencyPackHash;
	atString m_requiredVersion;
	bool m_isLevelPack;
	int	m_subPackCount;	

	SSetupData() { Reset(); }

	void Reset()
	{
		m_deviceName.Clear();
		m_datFile.Clear();
		m_timeStamp.Clear();
		m_nameHash.Clear();
		m_contentChangeSets.ResetCount();
		m_startupScript.Clear();
		m_scriptCallstackSize = -1;
		m_type = EXTRACONTENT_PAID_PACK;
		m_order = 0;
		m_minorOrder = 0;
		m_dependencyPackHash.SetHash(0);
		m_requiredVersion.Reset();
		m_isLevelPack = false;
		m_subPackCount = 0;
		ResetChangeSetGroups();
	}

	bool IsSetupOnly() const
	{
		const u32 SETUP_ONLY_NAME[] = { 
			ATSTRINGHASH("dlc_atomicblimp", 0xB6DE61E2),
			ATSTRINGHASH("dlc_specialedition", 0x2040A77),
			ATSTRINGHASH("dlc_online_pass", 0x876AA600),
			ATSTRINGHASH("dlc_collectorsedition", 0x4F98C2A7),
			ATSTRINGHASH("dlc_jpn_bonus", 0xF0DC7DF2),
			ATSTRINGHASH("mpCashPack01", 0xFAA5EE9E),
			ATSTRINGHASH("mpCashPack02", 0xE3DFC112),
			ATSTRINGHASH("mpCashPack03", 0xA2E73F16),
			ATSTRINGHASH("mpCashPack04", 0x8D29139A),
			ATSTRINGHASH("mpCashPack05", 0xFDB574B1)
		};

		for (u32 i = 0; i < NELEM(SETUP_ONLY_NAME); i++)
		{
			if (m_nameHash == SETUP_ONLY_NAME[i])
				return true;
		}

		return false;
	}

	void ResetChangeSetGroups()
	{
		for(int i = 0; i < m_contentChangeSetGroups.GetCount(); i++)
		{
			m_contentChangeSetGroups[i].m_NameHash.Clear();
			m_contentChangeSetGroups[i].m_ContentChangeSets.ResetCount();
		}

		m_contentChangeSetGroups.ResetCount();
	}

	const ContentChangeSetGroup *FindContentChangeSetGroup(atHashString groupNameHash) const
	{
		for(int i = 0; i < m_contentChangeSetGroups.GetCount(); i++)
		{
			if(m_contentChangeSetGroups[i].m_NameHash == groupNameHash)
				return &m_contentChangeSetGroups[i];
		}

		return NULL;
	}

	ContentChangeSetGroup *FindContentChangeSetGroup(atHashString groupNameHash)
	{
		for(int i = 0; i < m_contentChangeSetGroups.GetCount(); i++)
		{
			if(m_contentChangeSetGroups[i].m_NameHash == groupNameHash)
				return &m_contentChangeSetGroups[i];
		}

		return NULL;
	}

	void OnPostLoad()
	{
		// legacy data support

		if(m_contentChangeSets.GetCount() > 0)
		{
			ContentChangeSetGroup *startupGroup = FindContentChangeSetGroup(CCS_GROUP_STARTUP);

			if(!startupGroup)
			{
				startupGroup = &m_contentChangeSetGroups.Grow();
			#if __FINAL
				startupGroup->m_NameHash = CCS_GROUP_STARTUP;
			#else
				startupGroup->m_NameHash = "GROUP_STARTUP";
			#endif
			}

			Assert(startupGroup);
			Assert(startupGroup->m_ContentChangeSets.GetCount() == 0);

			for(int i = 0; i < m_contentChangeSets.GetCount(); i++)
			{		
				startupGroup->m_ContentChangeSets.PushAndGrow(m_contentChangeSets[i]);
			}

			m_contentChangeSets.Reset();
		}
	}

	PAR_SIMPLE_PARSABLE;
};

// Holds the locked state of a contentLock, also stores callback indices as bit field
struct SContentLockData
{
	static const u8 MAX_CONTENT_LOCK_SUBSYSTEM_CBS = 15; // can store up to MAX_CONTENT_LOCK_SUBSYSTEM_CBS indices, will need to bump m_callBackIndices storage type
	static const s8 INVALID_CONTENT_LOCK_CB_INDEX = -1;

	SContentLockData() : m_locked(true), m_callBackIndices(0) {	}

	void addCBIndex(s32 index)
	{
		if (index != INVALID_CONTENT_LOCK_CB_INDEX)
			m_callBackIndices |= (1 << index);
	}

	bool m_locked : 1;
	u16 m_callBackIndices : MAX_CONTENT_LOCK_SUBSYSTEM_CBS;
};

enum eMapChangeStates
{
	MCS_INVALID = -1,
	MCS_NONE,
	MCS_INIT,
	MCS_UPDATE,
	MCS_END,
	MCS_COUNT
};

struct SMandatoryPacksData
{
	atArray<atString>		m_Paths;
	PAR_SIMPLE_PARSABLE;
};

// DLC patching

struct SExtraTitleUpdateMount
{
	SExtraTitleUpdateMount() {}
	atString m_deviceName;
	atString m_path;
	PAR_SIMPLE_PARSABLE;
};

struct SExtraTitleUpdateData
{
	atArray<SExtraTitleUpdateMount>	m_Mounts;
	PAR_SIMPLE_PARSABLE;
};

// Folder mounting

struct SExtraFolderMount
{	
	SExtraFolderMount() : m_device(NULL) {}
	atString m_path;
	atString m_mountAs;
	fiDevice *m_device;

	bool Init();
	void Shutdown();
	bool IsRPF() const { return strstr(m_path.c_str(), ".rpf") != NULL; };

	PAR_SIMPLE_PARSABLE;
};

struct SExtraFolderMountData
{
	atArray<SExtraFolderMount>	m_FolderMounts;
	PAR_SIMPLE_PARSABLE;
};

#endif
