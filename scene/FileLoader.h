//
//
//    Filename: FileLoader.h
//     Creator: Adam Fowler
//     $Author: $
//       $Date: $
//   $Revision: $
// Description: Class that loads GTA files
//
//
#ifndef INC_FILE_LOADER_H_
#define INC_FILE_LOADER_H_

#include <string.h>
// Rage headers
#include "diag/channel.h"
#include "fwscene/Ide.h"
#include "fwscene/Ipl.h"
#include "fwscene/stores/txdstore.h"

// Game headers
#include "system/FileMgr.h" // For FileHandle typedef
#include "scene/DataFileMgr.h"
#include "scene/Entity.h"
#include "scene/EntityTypes.h"
#include "system/SettingsManager.h"
#include "Peds/Horse/horseDefines.h"

#define MAX_LINE_LENGTH			(512)
#define IDE_VERSION				(1)
#define SCRIPT_NAME_LENGTH		(24)

class CEntity;
class CMloModelInfo;
class CInteriorInst;

namespace rage
{
	class fwRect;
#if ENABLE_DEBUG_HEAP
	extern size_t g_sysExtraStreamingMemory;
#endif
}
RAGE_DECLARE_CHANNEL(fileloader)


enum MloFlags // should these belong to fwIplInteriorInstance?
{
	FL_MLO_NONE 		 = 0,
	FL_MLO_LOD			 = 1,
	FL_MLO_LOD2			 = 2,
};

//
//   Class Name: CFileLoader
//  Description: Loads GTA files
//
class CFileLoader
{
public:

	static void InitClass();

	static void PostProcessIplLoad();
	static bool CacheLoadStart(const char* pDatFilename);
	static void CacheLoadEnd();
	static void LoadIplFiles();
	static void PreLoadIplPostLoadIde();
	static void LoadVehicleMetaData();
	static void LoadPedMetaData();
	static void LoadWeaponMetaData();
	static void LoadDelayedIdeFiles();
	static bool LoadObjectTypesXml(const char* pLevelName, CDataFileMgr::DataFileType fileType, bool permanent = true, s32 mapTypeDefIndex = fwFactory::GLOBAL);
	static void LoadScene(const char* pLevelName);
	static void LoadMapZones(const char* pLevelName);
	static void LoadLevelInstanceInfo(const char* pLevelName);
	static char* LoadLine(FileHandle fid);
	static char* LoadLine(u8** ppData, s32& size);

    static void ResetLevelInstanceInfo(void);
	static void AddAllRpfFiles(bool registerFiles);

	static void ExecuteContentChangeSet(const CDataFileMgr::ContentChangeSet &contentChangeSet);
	static void RevertContentChangeSet(const CDataFileMgr::ContentChangeSet &changeSet, u32 actionMask);

	static void AddDLCItypeFile(const char* fileName);
	static void RemoveDLCItypeFile(const char* fileName);
	static void RequestDLCItypFiles();

	static void SetPtfxSetting(CSettings::eSettingsLevel level){ms_ptfxSetting = level;}
	static CSettings::eSettingsLevel GetPtfxSetting(){return ms_ptfxSetting;}

	static bool CheckFileForReplacements(const char* finalName,char* replacementName,size_t bufferSize);
	static const char* GetPtfxFileSuffix()
	{
		switch(ms_ptfxSetting)
		{
		case CSettings::Low:
			return "_lo";
		case CSettings::High:
		case CSettings::Ultra:
		case CSettings::Custom:
			return "_hi";
		case CSettings::Medium:
		default:
			return "";
		}
	}

	static void MarkArchetypesWhichCannotBeDummy();

private:

	static void BuildInvalidateList(const atArray<atString>& filesToInvalidate, atArray<CDataFileMgr::ChangeSetAction>& actionList, u32 actionMask);
	static void BuildRPFEnablementList(const CDataFileMgr::ContentChangeSet& ASSERT_ONLY(contentChangeSet), const atArray<atString>& fileList, 
		atArray<CDataFileMgr::ChangeSetAction>& actionList, bool add, u32 mapDataPriority);
	static void BuildDataFileEnablementList(const CDataFileMgr::ContentChangeSet& ASSERT_ONLY(contentChangeSet), const atArray<atString>& fileList, 
		atArray<CDataFileMgr::ChangeSetAction>& actionList, bool add, bool inverter);
	static void BuildTxdEnablementList(const atArray<atHashString>& fileList, atArray<CDataFileMgr::ChangeSetAction>& actionList, bool add);

    static void BuildContentChangeSetActionList(const CDataFileMgr::ContentChangeSet &contentChangeSet, 
		atArray<CDataFileMgr::ChangeSetAction> & actionList, u32 actionMask, u32 mapDataPriority, bool execute);
    static void ExecuteChangeSetAction(const CDataFileMgr::ChangeSetAction &contentChangeAction);
    
	static void ChangeSetAction_ModifyFileValidity(s32 archiveIndex, bool invalid);
    static void ChangeSetAction_LoadRPF(CDataFileMgr::DataFile* pData);
    static void ChangeSetAction_UnloadRPF(CDataFileMgr::DataFile* pData);
	static void ChangeSetAction_UnregisterRPF(CDataFileMgr::DataFile* pData);
    static void ChangeSetAction_UnloadTxd(const atHashString * txd);
    static void ChangeSetAction_LoadTxd(const atHashString * txd);
    static void ChangeSetAction_LoadResidentResource(const CDataFileMgr::ResourceReference * ref);
    static void ChangeSetAction_UnloadResidentResource(const CDataFileMgr::ResourceReference * ref);
	static void ChangeSetAction_UnregisterResource(const CDataFileMgr::ResourceReference * ref);

    static CDataFileMgr::ChangeSetAction & AppendChangeSetAction(atArray<CDataFileMgr::ChangeSetAction> & actionList, 
		CDataFileMgr::ChangeSetAction::actionTypeEnum type, 
		bool add, 
		u32 priority = CDataFileMgr::ChangeSetAction::CSA_NO_PRIORITY);
    static u32 GetDataTypePriority (CDataFileMgr::DataFileType type, const CDataFileMgr::DataFileType * typeList, int listLen);
    
	static void LoadRpfFiles();

	static s32 AddRpfFile(const CDataFileMgr::DataFile *pData, bool registerFiles);
	static bool IsValidRpfType(const CDataFileMgr::DataFileType &type);

	static void RequestPermanentItypFiles();
	static void LoadPermanentItypFiles();
	static void LoadItypFiles();

	static void MarkArchetypesWhichIntentionallyLeakObjects();

	static void LoadCapsuleFile();
	static void LoadExplosionInfoFiles();
	static void LoadHealthConfigFile();
#if ENABLE_HORSE
	static void LoadMountTuneFile();
	static void LoadHorseReinsFile();
#endif
	static void LoadDoorTypeTuningFile();
	static void LoadObjectCoverTuningFile();
	static void LoadMotionTaskDataFile();
	static void LoadDefaultTaskDataFile();
	static void LoadPedComponentSetsFile();
	static void LoadPedComponentClothFile();
	static void LoadPedIKSettingsFile();
	static void LoadTaskDataFile();
	static void LoadSpecialAbilitiesFiles();
	static void LoadBrawlingStyleFiles();
	static void LoadDeformableObjectFile();
	static void LoadTunableObjectFile();
	static void LoadNavCapabilitiesDataFile();
	static void LoadScenarioPointFiles();
	static void LoadAudioGroup(const char* pLine);
	static void LoadGarage(const char* pLine);
	static void LoadEntryExit(const char* pLine);
	static void Load2dEffect(const char* pLine);
	static void LoadCullZone(const char* pLine);
	static void LoadPopZone(const char* pLine);
	static void LoadMapArea(const char* pLine);

private:
	static char ms_line[MAX_LINE_LENGTH];
	static CDataFileMgr::DataFileType ms_validRpfTypes[];
	static CDataFileMgr::DataFileType ms_validRpfTypesPreInstall[];

	static atArray<s32> ms_permItyp;
	static bool ms_requestDLCItypes;
	static CSettings::eSettingsLevel ms_ptfxSetting;

	//////////////////////////////////////////////////////////////////////////
#if __DEV
public:
	static void SetCurrentLoadingFile(const char* pName) {if(pName) strcpy(sm_currentFile, pName); else sm_currentFile[0] = '\0';}
	static const char* GetCurrentLoadingFile() {return sm_currentFile;}

private:
	static char sm_currentFile[RAGE_MAX_PATH];
#endif	//__DEV

#if __BANK
public:
	static void InitWidgets();
	//////////////////////////////////////////////////////////////////////////
#endif	//__BANK
};

#endif  // INC_FILE_LOADER_H_
