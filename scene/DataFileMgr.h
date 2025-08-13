//
// filename:	DataFileMgr.h
// description:	Class storing 
//

#ifndef INC_DATAFILEMGR_H_
#define INC_DATAFILEMGR_H_

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "atl/hashstring.h"
#include "atl/singleton.h"
#include "atl/string.h"
#include "file/asset.h"
#include "file/device.h"
#include "parser/macros.h"
#include "streaming/streamingdefs.h"
#include "frontend/loadingscreencontext.h"

// Game headers
#include "debug/debug.h"

// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

class CDataFileMgr;

typedef atSingleton<CDataFileMgr> CDataFileMgrSingleton;


struct ExecutionCondition
{
	atHashString m_name;
	bool m_condition;
	PAR_SIMPLE_PARSABLE;
};

struct ExecutionConditions
{
	//More can be added later on
	atArray<ExecutionCondition> m_activeChangesetConditions;
	atFinalHashString m_genericConditions;
	PAR_SIMPLE_PARSABLE;
};

class CDataFileMgr
{
public:
	// WARNING: If you are going to add an enum here then you need to add it to the psc file as well
	enum DataFileType {
		RPF_FILE,

		// deprecated?
		IDE_FILE,
		DELAYED_IDE_FILE,
		IPL_FILE,

		// new replacement
		PERMANENT_ITYP_FILE,

		BENDABLEPLANTS_FILE,
		HANDLING_FILE,
		VEHICLEEXTRAS_FILE,
		GXT_FOLDER,
		PEDSTREAM_FILE,
		CARCOLS_FILE,
		POPGRP_FILE,
		PEDGRP_FILE,
		CARGRP_FILE,
		POPSCHED_FILE,
		ZONEBIND_FILE,
		RADIO_FILE,
		RADAR_BLIPS_FILE,
		THROWNWEAPONINFO_FILE,
		RMPTFX_FILE,
		PED_PERSONALITY_FILE,
		PED_PERCEPTION_FILE,
		VEHICLE_CAMERA_OFFSETS_FILE,
		FRONTEND_MENU_FILE,
		LEADERBOARD_DATA_FILE,
		LEADERBOARD_ICONS_FILE,
		NETWORKOPTIONS_FILE,
		TIMECYCLE_FILE,
		TIMECYCLEMOD_FILE,
		WEATHER_FILE,
		PLSETTINGSMALE_FILE,
		PLSETTINGSFEMALE_FILE,
		PROCOBJ_FILE,
		PROC_META_FILE,
		VFX_SETTINGS_FILE,
		SP_STATS_DISPLAY_LIST_FILE,
		MP_STATS_DISPLAY_LIST_FILE,
		PED_VARS_FILE,
		DISABLE_FILE,				// not a file type, but a command to remove specified IPL from data file mgr
		HUDCOLOR_FILE,
		HUD_TXD_FILE,
		FRONTEND_DAT_FILE,
		SCROLLBARS_FILE,
		TIME_FILE,
		BLOODFX_FILE,
		ENTITYFX_FILE,
		EXPLOSIONFX_FILE,
		MATERIALFX_FILE,
		MOTION_TASK_DATA_FILE,
		DEFAULT_TASK_DATA_FILE,
		MOUNT_TUNE_FILE,
		PED_BOUNDS_FILE,
		PED_HEALTH_FILE,
		PED_COMPONENT_SETS_FILE,
		PED_IK_SETTINGS_FILE,
		PED_TASK_DATA_FILE,
		PED_SPECIAL_ABILITIES_FILE,
		WHEELFX_FILE,
		WEAPONFX_FILE,
		DECALS_FILE,
		NAVMESH_INDEXREMAPPING_FILE,
		NAVNODE_INDEXREMAPPING_FILE,
		AUDIOMESH_INDEXREMAPPING_FILE,
		JUNCTION_TEMPLATES_FILE,
		PATH_ZONES_FILE,
		DISTANT_LIGHTS_FILE,
		DISTANT_LIGHTS_HD_FILE,
		FLIGHTZONES_FILE,
		WATER_FILE,
		TRAINCONFIGS_FILE,
		TRAINTRACK_FILE,
		PED_METADATA_FILE,
		WEAPON_METADATA_FILE,
		VEHICLE_METADATA_FILE,
		DISPATCH_DATA_FILE,
		DEFORMABLE_OBJECTS_FILE,
		TUNABLE_OBJECTS_FILE,
		PED_NAV_CAPABILITES_FILE,
		WEAPONINFO_FILE,
		WEAPONCOMPONENTSINFO_FILE,
		LOADOUTS_FILE,
		FIRINGPATTERNS_FILE,
		MOTIVATIONS_FILE,
		SCENARIO_POINTS_FILE,
		SCENARIO_POINTS_PSO_FILE,
		STREAMING_FILE,
		STREAMING_FILE_PLATFORM_PS3,
		STREAMING_FILE_PLATFORM_XENON,
		STREAMING_FILE_PLATFORM_OTHER,
		PED_BRAWLING_STYLE_FILE,
		AMBIENT_PED_MODEL_SET_FILE,
		AMBIENT_PROP_MODEL_SET_FILE,
		AMBIENT_VEHICLE_MODEL_SET_FILE,
		LADDER_METADATA_FILE, 
		HEIGHTMESH_INDEXREMAPPING_FILE,
		SLOWNESS_ZONES_FILE,
		LIQUIDFX_FILE,
		VFXVEHICLEINFO_FILE,
		VFXPEDINFO_FILE,
		DOOR_TUNING_FILE,
		PTFXASSETINFO_FILE,
		SCRIPTFX_FILE,
		VFXREGIONINFO_FILE,
		VFXINTERIORINFO_FILE,
		CAMERA_METADATA_FILE,
		STREET_VEHICLE_ASSOCIATION_FILE,
		VFXWEAPONINFO_FILE,
		EXPLOSION_INFO_FILE,
		JUNCTION_TEMPLATES_PSO_FILE,
		MAPZONES_FILE,
		SP_STATS_UI_LIST_FILE,
		MP_STATS_UI_LIST_FILE,
		OBJ_COVER_TUNING_FILE,
		STREAMING_REQUEST_LISTS_FILE,
		PLAYER_CARD_SETUP,
		WORLD_HEIGHTMAP_FILE,
		WORLD_WATERHEIGHT_FILE,
		PED_OVERLAY_FILE,
		WEAPON_ANIMATIONS_FILE,
		VEHICLE_POPULATION_FILE,
		ACTION_TABLE_DEFINITIONS,
		ACTION_TABLE_RESULTS,
		ACTION_TABLE_IMPULSES,
		ACTION_TABLE_RUMBLES,
		ACTION_TABLE_INTERRELATIONS,
		ACTION_TABLE_HOMINGS,
		ACTION_TABLE_DAMAGES,
		ACTION_TABLE_STRIKE_BONES,
		ACTION_TABLE_BRANCHES,
		ACTION_TABLE_STEALTH_KILLS,
		ACTION_TABLE_VFX,
		ACTION_TABLE_FACIAL_ANIM_SETS,
		VEHGEN_MARKUP_FILE,
		PED_COMPONENT_CLOTH_FILE,
		TATTOO_SHOP_DLC_FILE,
		VEHICLE_VARIATION_FILE,
		CONTENT_UNLOCKING_META_FILE,
		SHOP_PED_APPAREL_META_FILE,
		AUDIO_SOUNDDATA,
		AUDIO_CURVEDATA,
		AUDIO_GAMEDATA,
		AUDIO_DYNAMIXDATA,
		AUDIO_SPEECHDATA,
		AUDIO_SYNTHDATA,
		AUDIO_WAVEPACK,
		CLIP_SETS_FILE,
		EXPRESSION_SETS_FILE,
		FACIAL_CLIPSET_GROUPS_FILE, 
		NM_BLEND_OUT_SETS_FILE,
		VEHICLE_SHOP_DLC_FILE,
		WEAPON_SHOP_INFO_METADATA_FILE,
		SCALEFORM_PREALLOC_FILE,
		CONTROLLER_LABELS_FILE,
		CONTROLLER_LABELS_FILE_360,
		CONTROLLER_LABELS_FILE_PS3,
		CONTROLLER_LABELS_FILE_PS3_JPN,
		CONTROLLER_LABELS_FILE_ORBIS,
		CONTROLLER_LABELS_FILE_ORBIS_JPN,
		CONTROLLER_LABELS_FILE_DURANGO,
		IPLCULLBOX_METAFILE,
		TEXTFILE_METAFILE,
		NM_TUNING_FILE,
		MOVE_NETWORK_DEFS,
		WEAPONINFO_FILE_PATCH,
		DLC_SCRIPT_METAFILE,
		VEHICLE_LAYOUTS_FILE,
		DLC_WEAPON_PICKUPS,
		EXTRA_TITLE_UPDATE_DATA,
		SCALEFORM_DLC_FILE,
		OVERLAY_INFO_FILE,
		ALTERNATE_VARIATIONS_FILE,
		HORSE_REINS_FILE,
		FIREFX_FILE,
		INTERIOR_PROXY_ORDER_FILE,
		DLC_ITYP_REQUEST,
		EXTRA_FOLDER_MOUNT_DATA,
		BLOODFX_FILE_PATCH,
		SCRIPT_BRAIN_FILE,
		SCALEFORM_VALID_METHODS_FILE,
		DLC_POP_GROUPS,
		POPGRP_FILE_PATCH,
		SCENARIO_INFO_FILE,
		CONDITIONAL_ANIMS_FILE,
		STATS_METADATA_PSO_FILE,
		VFXFOGVOLUMEINFO_FILE,
		RPF_FILE_PRE_INSTALL,
		RPF_FILE_PRE_INSTALL_ONLY,
		LEVEL_STREAMING_FILE,
		SCENARIO_POINTS_OVERRIDE_FILE,
		SCENARIO_POINTS_OVERRIDE_PSO_FILE,
		SCENARIO_INFO_OVERRIDE_FILE,
		PED_FIRST_PERSON_ASSET_DATA,
		GTXD_PARENTING_DATA,
		COMBAT_BEHAVIOUR_OVERRIDE_FILE,
		EVENTS_OVERRIDE_FILE,
		PED_DAMAGE_OVERRIDE_FILE,
		PED_DAMAGE_APPEND_FILE,
		BACKGROUND_SCRIPT_FILE,
		PS3_SCRIPT_RPF,
		X360_SCRIPT_RPF,
		PED_FIRST_PERSON_ALTERNATE_DATA,
		ZONED_ASSET_FILE,
		ISLAND_HOPPER_FILE,
		CARCOLS_GEN9_FILE,
		CARMODCOLS_GEN9_FILE,
		COMMUNITY_STATS_FILE,
		GEN9_EXCLUSIVE_ASSETS_VEHICLES_FILE,
		GEN9_EXCLUSIVE_ASSETS_PEDS_FILE,
		//--------------
		INVALID_FILE
	};

	enum DataFileContents {
		CONTENTS_DEFAULT = 0,
		CONTENTS_PROPS,
		CONTENTS_MAP,
		CONTENTS_LODS,
		CONTENTS_PEDS,
		CONTENTS_VEHICLES,
		CONTENTS_ANIMATION,
		CONTENTS_CUTSCENE,
		CONTENTS_DLC_MAP_DATA,
		CONTENTS_DEBUG_ONLY,
		//--------------
		CONTENTS_MAX
	};

	enum InstallPartition {
		PARTITION_NONE = -1,
		PARTITION_0 = 0,
		PARTITION_1,
		PARTITION_2,
		PARTITION_MAX
	};

	enum { TOKEN_STR_LEN = 32};
	struct DataFile {
		DataFile()	: m_fileType(INVALID_FILE), m_handle(-1), m_locked(false)
					, m_enforceLsnSorting(true), m_contents(CONTENTS_DEFAULT), m_installPartition(PARTITION_NONE)
					, m_disabled(false), m_persistent(false), m_loadCompletely(false), m_overlay(false), m_patchFile(false) 
					{
						memset(m_filename,0,128);
					}

		char m_filename[128];
		atString m_registerAs;
		DataFileType m_fileType;
		s32 m_handle;
		bool m_locked;
		bool m_enforceLsnSorting;
		bool m_loadCompletely;
		bool m_disabled;
		bool m_persistent;
		bool m_overlay;
		bool m_patchFile;
		DataFileContents m_contents;
		InstallPartition m_installPartition;

#if !__FINAL
		atHashString m_Source;	// File that referenced this DataFile
#endif // !__FINAL

		PAR_SIMPLE_PARSABLE;
	};

	struct DataFileArray {
		atArray<DataFile> m_dataFiles;

		PAR_SIMPLE_PARSABLE;
	};

	struct ResourceReference
	{
		//atHashString m_AssetName;
		atString m_AssetName;
		char m_Extension[8];

		strIndex Resolve() const;

		PAR_SIMPLE_PARSABLE;
	};

	struct ChangeSetData
	{
		atString m_associatedMap;
		atArray<atString> m_filesToInvalidate;
		atArray<atString> m_filesToDisable;
		atArray<atString> m_filesToEnable;
		atArray<atHashString> m_txdToUnload;
		atArray<atHashString> m_txdToLoad;
		atArray<ResourceReference> m_residentResources;
		atArray<ResourceReference> m_unregisterResources;
		atArray<atHashString> m_dataFilesToLoad;

		bool IsValid(bool executing) const;

		PAR_SIMPLE_PARSABLE;
	};

	struct ContentChangeSet
	{
		atString m_changeSetName;
		atArray<ChangeSetData> m_mapChangeSetData;
		atArray<atString> m_filesToInvalidate;
		atArray<atString> m_filesToDisable;
		atArray<atString> m_filesToEnable;
		atArray<atHashString> m_txdToUnload;
		atArray<atHashString> m_txdToLoad;
		atArray<ResourceReference> m_residentResources;
		atArray<ResourceReference> m_unregisterResources;
		atArray<atHashString> m_dataFilesToLoad;
		bool m_requiresLoadingScreen;
		LoadingScreenContext m_loadingScreenContext;
		ExecutionConditions m_executionConditions;
		bool m_useCacheLoader;

		PAR_SIMPLE_PARSABLE;
	};



    struct ChangeSetAction
    {
		enum
		{
			CSA_FIRST_PRIORITY = 0x0,
			CSA_NO_PRIORITY = 0xfe,
			CSA_LAST_PRIORITY = 0xff
		};

		ChangeSetAction() :
			m_file(NULL),
			m_priority(CSA_NO_PRIORITY),
			m_add(false)
		{
		}

        union
        {
            DataFile * m_file;
            const atHashString * m_txd;
            const ResourceReference * m_residentResource;
			s32 archiveIndex;
        };

        enum actionTypeEnum {   CHANGE_SET_ACTION_RPF,
                                CHANGE_SET_ACTION_DATA_FILE,
                                CHANGE_SET_ACTION_TXD,
                                CHANGE_SET_ACTION_RESIDENT_RESOURCE,
                                CHANGE_SET_ACTION_LOAD_REQUESTED_OBJECTS,
								CHANGE_SET_ACTION_UNREGISTER_RESOURCE,
								CHANGE_SET_ACTION_INVALIDATE_FILE,
								CHANGE_SET_ACTION_INVALIDATE_INTERIOR,
								CHANGE_SET_ACTION_INVALIDATE_STREAMABLE,
								CHANGE_SET_ACTION_COUNT } ;

		static const u32 CCS_ALL_ACTIONS = ~0u;
		CompileTimeAssert(CHANGE_SET_ACTION_COUNT < 32);

		bool operator < (const ChangeSetAction &r) const { return m_priority < r.m_priority; };

        actionTypeEnum	m_actionType;
        bool			m_add; // if false then remove/unload item
		u8				m_priority;
		strIndex m_strIndex;
    };

	struct ContentsOfDataFileXml
	{
	public:
		atArray<atString> m_disabledFiles;
		atArray<atString> m_includedDataFiles;
		atArray<DataFileArray> m_includedXmlFiles;
		atArray<DataFile> m_dataFiles;
		atArray<ContentChangeSet> m_contentChangeSets;
		atArray<atString> m_patchFiles;
		atArray<atString> m_allowedFolders;

		PAR_SIMPLE_PARSABLE;
	};

	CDataFileMgr();

	const DataFile* GetFirstFile(DataFileType type);
	const DataFile* GetNextFile(const DataFile* pData);
	const DataFile* GetLastFile(DataFileType type);
	const DataFile* GetPrevFile(const DataFile* pData);
    DataFile & GetFileFromHandle (s32 handle) {return m_dataFiles[handle];}
   
	bool IsValid(const DataFile* pData) {return (pData->m_handle != -1);}
	void Clear();

	void SetEnableFilePatching(bool value){m_addingPatchFiles = value;}

	void GetCCSArchivesToEnable(atHashString changeSetName, bool execute, atMap<s32, bool>& archives);
	void CreateDependentsGraph(atHashString changeSetName, bool execute);
	void DestroyDependentsGraph();

	void Load(const char *pFilename, bool bCullOtherPlatformData = true);
	void Unload(const char *pFilename, bool bCullOtherPlatformData = true);
	bool CheckFileValidity(const char *pFilename);

	FASTRETURNCHECK(const char *) RemapDataFileName(const char *origFile, char *outTargetBuffer, size_t targetBufferSize);

	void SnapshotFiles();
	void RecallSnapshot();

	//	Once we've loaded the ipl, ide and rpf files, it should be safe to remove their entries from the DataFileMgr
	void RemoveMapDataFiles();

	bool IsNullDataFile(const DataFile *dataFile) const		{ return dataFile == &m_nullDataFile; }

	/** PURPOSE: Take a filename and expand all the environment variables inside,
	 *  like %PLATFORM%.
	 *
	 *  PARAMS:
	 *    filename - The string to expand.
	 *    outBuffer - Buffer to receive the expanded string.
	 *    bufferSize - Size of outBuffer.
	 */
	void ExpandFilename(const char *filename, char *outBuffer, size_t bufferSize);

	void ResetChangeSetCollisions() { m_changeSetCollisions.Reset(); }
	const atArray<atHashString>& GetChangeSetCollisions() const { return m_changeSetCollisions; }

	const ContentChangeSet *GetContentChangeSet(atHashString contentChangeSetName) const;

	DataFile *FindDataFile(atHashString fileName);

#if __BANK
	static bool	GetFatcuts();
#endif

private:
    void AddFile(DataFileType type, const char* pFilename, bool locked, bool enforceLsnSorting, DataFileContents contents, const char *registerAs, const char *source, bool disabled, bool persistent, bool loadCompletely, bool overlay);
    void AddFile(const DataFile & datFile, const char * filename);
	void AddDisabledFile(const char* pFilename);
	void RemoveFile(const char* pFilename);
	void RemoveDisabledFile(const char* pFilename);

#if __ASSERT
	bool CheckCRCValidity(const DataFile* dataFile);
	void VerifyXmlContents(ContentsOfDataFileXml& XmlContents, const char* pFilename) const;
	void AddDefaultAllowedFolders(ContentsOfDataFileXml& XmlContents, const char* pFilename) const;
	bool IsInAllowedFolder(const ContentsOfDataFileXml& XmlContents, const char* filePath) const;
#endif // __ASSERT

	atArray<DataFile> m_dataFiles;
	atArray<atString> m_disabledFiles;
	atMap<atHashString, ContentChangeSet> m_contentChangeSets;
	atArray<atHashString> m_changeSetCollisions;
	DataFile m_nullDataFile;

	// The first m_LockedFilesCount files will not be deleted during a normal clear.
	int m_LockedFilesCount;
	int m_LockedDisabledFilesCount;

	bool m_addingPatchFiles;

	atMap<u32, ConstString> m_EnvVariableMap;
};

enum eDFMI_UnloadType
{
	eDFMI_UnloadInit = -1,// Used to check whether the mount interface for the data file type has already been registered
	eDFMI_UnloadFirst, // Called at the top of SHUTDOWN_SESSION
	eDFMI_UnloadLast, // Called at the bottom of SHUTDOWN_SESSION
	eDFMI_UnloadDefault
};

struct CDataFileMountInterface
{
	CDataFileMountInterface() {}
	virtual ~CDataFileMountInterface() {};

	virtual bool LoadDataFile(const CDataFileMgr::DataFile & file) = 0;

	virtual void UnloadDataFile(const CDataFileMgr::DataFile & file) = 0;
};

class CDataFileMount
{
public:

	CDataFileMount(){}

	static void RegisterMountInterface(CDataFileMgr::DataFileType type, 
									   CDataFileMountInterface *mountInterface, 
									   eDFMI_UnloadType unloadBehaviour = eDFMI_UnloadDefault)
	{
		static bool lazyInit = true;

		// Initialize the unload behaviour array.
		if(lazyInit)
		{
			for(int i=0; i<CDataFileMgr::INVALID_FILE; ++i)
			{
				sm_UnloadBehaviours[i] = eDFMI_UnloadInit;
			}
			lazyInit = false;
		}

		// Check if file type is valid or has already been registered.
		if(type >= CDataFileMgr::INVALID_FILE || sm_UnloadBehaviours[type] != eDFMI_UnloadInit)
		{
			return;
		}

		sm_UnloadBehaviours[type] = unloadBehaviour;
		sm_Interfaces[type] = mountInterface;
	}

	static void LoadFile(const CDataFileMgr::DataFile & file)
	{
		if (Verifyf(sm_Interfaces[file.m_fileType], "LoadFile No known mount interface for type %d %s", file.m_fileType, file.m_filename))
		{
			sm_Interfaces[file.m_fileType]->LoadDataFile(file);
		}
	}

	static void UnloadFile(const CDataFileMgr::DataFile & file)
	{
		if (Verifyf(sm_Interfaces[file.m_fileType], "UnloadFile No known mount interface for type %d %s", file.m_fileType, file.m_filename))
		{
			if (sm_overrideUnload || (sm_unloadBehaviour == sm_UnloadBehaviours[file.m_fileType]))
			{
				sm_Interfaces[file.m_fileType]->UnloadDataFile(file);
			}
		}
	}

	static CDataFileMountInterface *GetMountInterfaceForFileType(CDataFileMgr::DataFileType type) { return sm_Interfaces[type]; }; 

	static void SetUnloadBehaviour(eDFMI_UnloadType b) { sm_unloadBehaviour = b; };
	static eDFMI_UnloadType GetUnloadBehaviour() { return sm_unloadBehaviour; };

	static void SetOverrideUnload(bool value) { sm_overrideUnload = value; }

	BANK_ONLY(static void DebugOutputTable();)

private:
	static eDFMI_UnloadType sm_UnloadBehaviours[CDataFileMgr::INVALID_FILE];
	static atRangeArray<CDataFileMountInterface *, CDataFileMgr::INVALID_FILE> sm_Interfaces;
	static bool sm_overrideUnload;
	static eDFMI_UnloadType sm_unloadBehaviour;
};


#define DATAFILEMGR CDataFileMgrSingleton::InstanceRef()

#define INIT_DATAFILEMGR	\
	do {														\
		if (!CDataFileMgrSingleton::IsInstantiated()){	\
			CDataFileMgrSingleton::Instantiate();		\
		}															\
	} while(0)													\
	//END
// --- Globals ------------------------------------------------------------------

//
// name:		CDataFileMgr::AddFile
// description:	Add data file to list
inline void CDataFileMgr::AddFile(DataFileType type, const char* pFilename, bool locked, bool enforceLsnSorting, DataFileContents contents, const char *registerAs, const char *NOTFINAL_ONLY(source), bool disabled, bool persistent, bool loadCompletely, bool overlay)
{
	DataFile file;

	// Expand the path.
	char expandedFilename[RAGE_MAX_PATH];
	ExpandFilename(pFilename, expandedFilename, sizeof(expandedFilename));

	// don't add files which appear in the disabled list
	for(s32 i=0; i<m_disabledFiles.GetCount();i++){
		if (!stricmp(m_disabledFiles[i].c_str(), expandedFilename)){
			return;
		}
	}

#if __ASSERT
    if ( const DataFile * already = FindDataFile(atStringHash(expandedFilename)) )
	{
        Errorf("Adding already added file to DataFileMgr : %s (%s) before in %s",already->m_filename,source,already->m_Source.GetCStr());    
	}
#endif

	const fiDevice *d = fiDevice::GetDevice(pFilename);

	if (d)
	{
		char path[RAGE_MAX_PATH];
		ASSET.RemoveNameFromPath(path, sizeof(path), pFilename);

		d->PrefetchDir(path);
	}

	file.m_handle = m_dataFiles.GetCount();
	file.m_fileType = type;
	file.m_locked = locked;
	file.m_enforceLsnSorting = enforceLsnSorting;
	file.m_loadCompletely = loadCompletely;
	file.m_overlay = overlay;
	file.m_patchFile = overlay && m_addingPatchFiles;
	file.m_contents = contents;
	file.m_registerAs = registerAs;
	file.m_disabled = disabled;
	file.m_persistent = persistent;
#if __BANK
	if(GetFatcuts()&&file.m_contents == CONTENTS_CUTSCENE)
	{
		file.m_locked = true;
	}
#endif

	//strncpy(file.m_filename, pFilename, sizeof(file.m_filename));
	ASSET.FullPath(&file.m_filename[0], sizeof(file.m_filename), expandedFilename,"");
#if !__BANK
	if(file.m_contents != CONTENTS_DEBUG_ONLY)
#endif
	{
		m_dataFiles.PushAndGrow(file);
	}

#if !__FINAL
	m_dataFiles.back().m_Source = source;
#endif // !__FINAL
}

inline void CDataFileMgr::AddFile(const DataFile & datFile, const char * filename)
{
    DataFileType type = datFile.m_fileType;

    Assertf (type != (DataFileType)-1, "File %s in %s does not have type specification",datFile.m_filename,filename);
    // -1 is the default if left unspecified in the meta file. We don't actually want to
    // add a file with type -1 though. Let's default to RPF (default type used to be 0, 
    // and some stuff seems to depend on this now.)
    if (type == (DataFileType)-1)  
       type = RPF_FILE;

    AddFile(type, datFile.m_filename, datFile.m_locked,
        datFile.m_enforceLsnSorting, datFile.m_contents, datFile.m_registerAs,
        filename, datFile.m_disabled, datFile.m_persistent, datFile.m_loadCompletely, datFile.m_overlay);
}


//
// name:		CDataFileMgr::AddDisabledFile
// description:	Add disalbed data file to list (file which is not to be loaded later)
inline void CDataFileMgr::AddDisabledFile(const char* pFilename)
{
	// Expand the path.
	char expandedFilename[RAGE_MAX_PATH];
	ExpandFilename(pFilename, expandedFilename, sizeof(expandedFilename));

	atString fileName(expandedFilename);
	m_disabledFiles.PushAndGrow(fileName);
}

inline void CDataFileMgr::RemoveFile(const char* pFilename)
{
	char expandedFilename[RAGE_MAX_PATH] = { 0 };
	char finalFileName[RAGE_MAX_PATH] = { 0 };

	ExpandFilename(pFilename, expandedFilename, sizeof(expandedFilename));
	ASSET.FullPath(finalFileName, sizeof(finalFileName), expandedFilename,"");

	for (int i = 0; i < m_dataFiles.GetCount(); i++)
	{
		if (stricmp(finalFileName, m_dataFiles[i].m_filename) == 0)
		{
			m_dataFiles.DeleteFast(i);
			i--;
			break;
		}
	}
}

inline void CDataFileMgr::RemoveDisabledFile(const char* pFilename)
{
	char expandedFilename[RAGE_MAX_PATH] = { 0 };
	char finalFileName[RAGE_MAX_PATH] = { 0 };

	ExpandFilename(pFilename, expandedFilename, sizeof(expandedFilename));
	ASSET.FullPath(finalFileName, sizeof(finalFileName), expandedFilename,"");

	for (int i = 0; i < m_disabledFiles.GetCount(); i++)
	{
		if (stricmp(finalFileName, m_disabledFiles[i].c_str()) == 0)
		{
			m_disabledFiles.DeleteFast(i);
			i--;
			break;
		}
	}
}

inline const CDataFileMgr::DataFile* CDataFileMgr::GetFirstFile(DataFileType type)
{
	DataFile data;
	data.m_handle = -1;
	data.m_fileType = type;
	return GetNextFile(&data);
}

inline const CDataFileMgr::DataFile* CDataFileMgr::GetNextFile(const CDataFileMgr::DataFile* pData)
{
	// handle is basically an index into the array
	s32 index = pData->m_handle + 1;
	// go through array finding next object with same type
	while(index < m_dataFiles.GetCount())
	{
		// if type is the same return datafile
		if(m_dataFiles[index].m_fileType == pData->m_fileType)
		{
			return &m_dataFiles[index];
		}
		index++;
	}
	return &m_nullDataFile;
}

inline const CDataFileMgr::DataFile* CDataFileMgr::GetLastFile(DataFileType type)
{
	DataFile data;
	data.m_handle = m_dataFiles.GetCount();
	data.m_fileType = type;
	return GetPrevFile(&data);
}

inline const CDataFileMgr::DataFile* CDataFileMgr::GetPrevFile(const CDataFileMgr::DataFile* pData)
{
	// handle is basically an index into the array
	s32 index = Clamp(pData->m_handle, 0, m_dataFiles.GetCount());
	index -= 1;

	// go through array finding next object with same type
	while(index >= 0)
	{
		// if type is the same return datafile
		if(m_dataFiles[index].m_fileType == pData->m_fileType)
		{
			return &m_dataFiles[index];
		}
		index--;
	}
	return &m_nullDataFile;
}

#endif // !INC_DATAFILEMGR_H_
