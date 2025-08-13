//    Filename: FileLoader.cpp
//     Creator: Adam Fowler
//     $Author: $
//       $Date: $
//   $Revision: $
// Description: Class used to load GTA data files
//

// This File
#include "scene/FileLoader.h"

// Rage
#include "diag/art_channel.h"
#include "file/default_paths.h"
#include "file/stream.h"
#include "grcore/texturedefault.h"
#include "paging/rscbuilder.h"
#include "parser/manager.h"
#include "system/cache.h"
#include "system/memory.h"
#include "spatialdata/sphere.h"
#include "string/stringutil.h"

// Framework headers
#include "fwscene/lod/lodattach.h"
#include "fwscene/lod/lodtypes.h"
#include "fwscene/lod/ContainerLod.h"
#include "fwscene/mapdata/maptypes.h"
#include "fwscene/stores/drawablestore.h"
#include "fwscene/stores/fragmentstore.h"
#include "fwscene/stores/maptypesstore.h"
#include "fwscene/stores/mapdatastore.h"
#include "fwscene/stores/staticboundsstore.h"
#include "fwscene/world/entitydesc.h"
#include "fwsys/fileexts.h"
#include "streaming/packfilemanager.h"
#include "vfx/ptfx/ptfxmanager.h"

// Game headers
#include "control/garages.h"
#include "control/slownesszones.h"
#include "core/game.h"
#include "debug/Debug.h"
#include "debug/debugscene.h"
#include "game/modelindices.h"
#include "game/zones.h"
#include "modelinfo/Basemodelinfo.h"
#include "modelinfo/Mlomodelinfo.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "modelinfo/Pedmodelinfo.h"
#include "modelinfo/timemodelinfo.h"
#include "modelinfo/Vehiclemodelinfo.h"
#include "modelinfo/Weaponmodelinfo.h"
#include "objects/CoverTuning.h"
#include "objects/DoorTuning.h"
#include "objects/DummyObject.h"
#include "objects/object.h"
#include "pathserver/ExportCollision.h"
#include "physics/Deformable.h"
#include "physics/Tunable.h"
#include "Peds/Action/BrawlingStyle.h"
#include "Peds/Horse/horseTune.h"
#include "Peds/Horse/horseComponent.h"
#include "Peds/HealthConfig.h"
#include "Peds/NavCapabilities.h"
#include "Peds/PedCapsule.h"
#include "Peds/PedComponentSetInfo.h"
#include "Peds/PedIKSettings.h"
#include "Peds/TaskData.h"
#include "peds/pedpopulation.h"
#include "peds/PlayerSpecialAbility.h"
#include "peds/popzones.h"
#include "renderer/Lights/LightSource.h"
#include "renderer/occlusion.h"
#include "renderer/renderer.h"
#include "renderer/water.h"
#include "renderer/ZoneCull.h"
#include "scene/AnimatedBuilding.h"
#include "scene/Building.h"
#include "scene/datafilemgr.h"
#include "scene/dlc_channel.h"
#include "scene/DownloadableTextureManager.h"
#include "scene/Entity.h"
#include "scene/entities/compEntity.h"
#include "scene/ExtraContent.h"
#include "scene/InstancePriority.h"
#include "scene/ipl/IplCullBox.h"
#include "scene/loader/ManagedImapGroup.h"
#include "scene/loader/mapTypes.h"
#include "scene/loader/mapdata.h"
#include "scene/loader/MapData_Buildings.h"
#include "scene/loader/MapData_Interiors.h"
#include "scene/loader/MapData_Extensions.h"
#include "scene/loader/Map_TxdParents.h"
#include "scene/loader/mapFileMgr.h"
#include "scene/lod/LodMgr.h"
#include "scene/MapChange.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "scene/portals/InteriorInst.h"
#include "scene/scene.h"
#include "scene/scene_channel.h"
#include "scene/texLod.h"
#include "scene/world/gameWorld.h"
#include "scene/worldpoints.h"
#include "streaming/CacheLoader.h"
#include "streaming/streaming.h"
#include "streaming/streamingengine.h"
#include "system/FileMgr.h"
#include "Task/Default/Patrol/PatrolRoutes.h"
#include "Task/Scenario/ScenarioManager.h"
#include "Task/Scenario/ScenarioPointManager.h"
#include "Task/Scenario/TaskScenario.h"
#include "Task/System/DefaultTaskData.h"
#include "Task/System/MotionTaskData.h"
#include "Task/System/TaskTypes.h"
#include "TimeCycle/TimeCycle.h"
#include "vehicleAi/pathfind.h"
#include "vehicles/cargen.h"
#include "vehicles/handlingmgr.h"
#include "vehicles/train.h"
#include "vehicles/vehiclefactory.h"
#include "vehicles/vehiclepopulation.h"
#include "Vfx/Misc/Scrollbars.h"
#include "Vfx/VisualEffects.h"
#include "weapons/Explosion.h"
#include "control/gamelogic.h"
#include "frontend/LoadingScreens.h"

#include "fwsys/metadatastore.h"
#include "fwscene/stores/mapdatastore.h"
#include "objects/Door.h"
#include "network/Stats/NetworkStockMarketStats.h"

#if RSG_PC
#include "core/app.h"
#endif // RSG_PC

#if __PS3 && !__FINAL && __DEV && 0
#include <sn/libsntuner.h>
#define PROFLOAD_ONLY(x)		x
#else
#define PROFLOAD_ONLY(x)
#endif // #if __PS3 && !__FINAL

XPARAM(PartialDynArch);
#if !__FINAL
XPARAM(ptfxassetoverride);
#endif

SCENE_OPTIMISATIONS()

RAGE_DEFINE_CHANNEL(fileloader)

#define fileloaderAssert(cond)						RAGE_ASSERT(fileloader,cond)
#define fileloaderAssertf(cond,fmt,...)				RAGE_ASSERTF(fileloader,cond,fmt,##__VA_ARGS__)
#define fileloaderVerifyf(cond,fmt,...)				RAGE_VERIFYF(fileloader,cond,fmt,##__VA_ARGS__)
#define fileloaderErrorf(fmt,...)					RAGE_ERRORF(fileloader,fmt,##__VA_ARGS__)
#define fileloaderWarningf(fmt,...)					RAGE_WARNINGF(fileloader,fmt,##__VA_ARGS__)
#define fileloaderDisplayf(fmt,...)					RAGE_DISPLAYF(fileloader,fmt,##__VA_ARGS__)
#define fileloaderDebugf1(fmt,...)					RAGE_DEBUGF1(fileloader,fmt,##__VA_ARGS__)
#define fileloaderDebugf2(fmt,...)					RAGE_DEBUGF2(fileloader,fmt,##__VA_ARGS__)
#define fileloaderDebugf3(fmt,...)					RAGE_DEBUGF3(fileloader,fmt,##__VA_ARGS__)
#define fileloaderLogf(severity,fmt,...)			RAGE_LOGF(fileloader,severity,fmt,##__VA_ARGS__)
#define fileloaderCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,fileloader,severity,fmt,##__VA_ARGS__)

//PARAM(imposterDivisions,"Number of tree imposter divisons");
PARAM(fileloader_tty_windows,	"Create RAG windows for Fileloader related TTY.");

PARAM(override_script, "[FileLoader] Use this file instead of script.rpf. The replacement .rpf has to be in the same folder as the original script.rpf. You don't need to specify the .rpf extension");
PARAM(override_script_dlc, "[FileLoader] Use this file instead of script_RELEASE.rpf in DLC packs. The replacement .rpf has to be in the same folder as the original script_RELEASE.rpf. You don't need to specify the .rpf extension");
PARAM(usefatcuts, "[FileLoader] Use the fat cutscenes");
#if __BANK
PARAM(fileMountWidgets, "Add Data File Mount widgets to the bank");
#endif

#include "Network\NetworkInterface.h"

#define FILELOADER_COMMENT_CHAR '#'

// --- Globals ---------------------------------------
CMapTypes* s_MapTypes = NULL;
CMloArchetypeDef* s_CurrentMloArchetypeDef = NULL;
u32 s_CurrentMloObject = 0xffffffff;
u32 s_CurrentMloPortal = 0xffffffff;
u32 s_CurrentMloRoom = 0xffffffff;

char CFileLoader::ms_line[MAX_LINE_LENGTH];
atArray<s32> CFileLoader::ms_permItyp;
bool CFileLoader::ms_requestDLCItypes = false;
CSettings::eSettingsLevel CFileLoader::ms_ptfxSetting = CSettings::Medium;


CDataFileMgr::DataFileType CFileLoader::ms_validRpfTypesPreInstall[] = 
{
	CDataFileMgr::RPF_FILE_PRE_INSTALL,
	CDataFileMgr::RPF_FILE_PRE_INSTALL_ONLY,
};

CDataFileMgr::DataFileType CFileLoader::ms_validRpfTypes[] = 
{
	CDataFileMgr::RPF_FILE_PRE_INSTALL,
	CDataFileMgr::RPF_FILE,
	CDataFileMgr::PEDSTREAM_FILE
};

#if __DEV
char CFileLoader::sm_currentFile[RAGE_MAX_PATH];
static CPedModelInfo::InitDataList s_PedModelInfoInitList;
#endif

namespace 
{
	// This is kind of a hack to get around some compiler problems. If the compiler thinks
	// that nothing in a translation unit (a .cpp) is getting used it can strip out the whole
	// translation unit. In this case since it doesn't think CMapTypes is getting used it strips
	// out all of maptypes.cpp, including the parser autoregistration code in that .cpp which will 
	// in fact reference CMapTypes (and therefore everything else in the .cpp).
	bool g_alwaysTrue = true;
	void DummyFactory()
	{
		if (g_alwaysTrue)
		{
			return;
		}
		REGISTER_PARSABLE_CLASS(CMapTypes);
	}
}

class CBrawlingStyleMetaDataFileMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile & file)
	{
		dlcDebugf3("CBrawlingStyleMetaDataFileMounter::LoadDataFile: %s type = %d", file.m_filename, file.m_fileType);
		CBrawlingStyleManager::Shutdown();
		CBrawlingStyleManager::Init(file.m_filename);
		return true;
	}

	virtual void UnloadDataFile(const CDataFileMgr::DataFile & OUTPUT_ONLY(file)) 
	{
		dlcDebugf3("CBrawlingStyleMetaDataFileMounter::UnloadDataFile %s type = %d", file.m_filename, file.m_fileType);
		CBrawlingStyleManager::Shutdown();
		CBrawlingStyleManager::Init("common:/data/pedbrawlingstyle.meta");
	}

} g_BrawlingStyleMetaDataFileMounter;

class CWeaponMetaDataFileMounter : public CDataFileMountInterface
{
    virtual bool LoadDataFile(const CDataFileMgr::DataFile & file)
	{
		CFileLoader::LoadObjectTypesXml(file.m_filename, CDataFileMgr::WEAPON_METADATA_FILE, false, fwFactory::EXTRA);
		return true;
	}

	virtual void UnloadDataFile(const CDataFileMgr::DataFile & ) {}

} g_WeaponMetaDataFileMounter;

class CDLCItypFileMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile & file)
	{
		CFileLoader::AddDLCItypeFile(file.m_filename);
		return true;
	}

	virtual void UnloadDataFile(const CDataFileMgr::DataFile & file) 
	{
		CFileLoader::RemoveDLCItypeFile(file.m_filename);
	}

} g_DLCItypeFileMounter;

//
//        name: CFileLoader::InitClass
// description: Basic initialization of the file loader class.
//
void CFileLoader::InitClass()
{
	CDataFileMount::RegisterMountInterface(CDataFileMgr::PED_BRAWLING_STYLE_FILE, &g_BrawlingStyleMetaDataFileMounter, eDFMI_UnloadFirst);
	CDataFileMount::RegisterMountInterface(CDataFileMgr::WEAPON_METADATA_FILE, &g_WeaponMetaDataFileMounter);
	CDataFileMount::RegisterMountInterface(CDataFileMgr::DLC_ITYP_REQUEST, &g_DLCItypeFileMounter, eDFMI_UnloadLast);
	CInteriorProxy::RegisterMountInterface();
}

bool CFileLoader::IsValidRpfType(const CDataFileMgr::DataFileType &type)
{
	for(int i=0;i<NELEM(ms_validRpfTypes);i++)
	{
		if(ms_validRpfTypes[i]==type)
			return true;
	}
	return false;
}


#if RSG_PC||!__FINAL
#define FINAL_PCONLY(x) x
#else
#define FINAL_PCONLY(x)
#endif
bool CFileLoader::CheckFileForReplacements(const char* finalName,char* replacementName,size_t /*bufferSize*/)
{
	bool isReplaced = false;
	char expandedName[RAGE_MAX_PATH]={0};
	const char *filename = ASSET.FileName(finalName);
	size_t string_length_of_path = filename - finalName;
	bool optionalReplacement = true;

	// Set this to force using script_rel, useful when you have a disc burned with both types of scripts.
#define FORCE_SCRIPT_REL __FINAL || 0

#if !__FINAL
	const char *overrideScriptRpfName = 0;
	const char *overrideDlcScriptRpfName = 0;
	PARAM_override_script.Get(overrideScriptRpfName);
	PARAM_override_script_dlc.Get(overrideDlcScriptRpfName);
#endif 

	//Checks for script.rpf filename overrides
	if(stricmp(filename,"script.rpf")==0||stricmp(filename,"script_install.rpf")==0||stricmp(filename,"script_release.rpf")==0)
	{
#if FORCE_SCRIPT_REL
		//	Add 4 below for the length of "_rel" suffix
		if (Verifyf( ( strlen(finalName) + 4) < RAGE_MAX_PATH, "CFileLoader::LoadRpfFiles - path to override script rpf is too long"))
		{
			safecpy(replacementName, finalName, string_length_of_path);
			strcat(replacementName, "/");
			strcat(replacementName, filename);
			ASSET.RemoveExtensionFromPath(replacementName,RAGE_MAX_PATH,replacementName);
			strcat(replacementName, "_rel.rpf");
			isReplaced = true;
			optionalReplacement = false;
			Displayf("Script pack %s being replaced with %s",finalName,replacementName);
		}
#else
		const char* overrideName = (strnicmp(finalName,"dlc",3)!=0) ? overrideScriptRpfName : overrideDlcScriptRpfName;
		if(overrideName)
		{
			//	Add 5 below for the length of ".rpf" and for a forward slash before the filename
			if (Verifyf( (string_length_of_path + strlen(overrideName) + 5) < RAGE_MAX_PATH, "CFileLoader::LoadRpfFiles - path to override script rpf is too long"))
			{
				safecpy(replacementName, finalName, string_length_of_path);
				strcat(replacementName, "/");
				strcat(replacementName, overrideName);
				strcat(replacementName, ".rpf");
				isReplaced = true;
			}
			optionalReplacement = false;
			isReplaced = true;
		}
#endif
	}

#if !__FINAL

	//Check for fatcuts
	else if(PARAM_usefatcuts.Get())
	{
		//	Check path is at least as long as cutscene platform path and matches cutscene platform path i.e. platform:/anim/ should not be remappedconst char *cutscene_platform_path = "platform:/anim/cutscene/";
		const char *cutscene_platform_path = "platform:/anim/cutscene/";
		static const size_t string_length_of_cutscene_platform_path = strlen(cutscene_platform_path);
		if (stricmp(filename, "common_cutscene.meta") == 0)
		{
			safecpy(replacementName, finalName, string_length_of_path);
			strcat(replacementName, "/");
			strcat(replacementName, "common_cutscene_fat.meta");

			isReplaced = true;
			optionalReplacement = false;
		}
		else if (stricmp(filename, "cuts.meta") == 0)
		{
			safecpy(replacementName, finalName, string_length_of_path);
			strcat(replacementName, "/");
			strcat(replacementName, "cuts_fat.meta");

			isReplaced = true;
			optionalReplacement = false;
		}
		else if (string_length_of_path >= string_length_of_cutscene_platform_path && strnicmp(finalName, cutscene_platform_path, string_length_of_path) == 0)
		{
			char buildPath[RAGE_MAX_PATH];
			safecpy(buildPath, "assets:/non_final/build/dev");
#ifdef RS_BRANCHSUFFIX
			safecat(buildPath, RS_BRANCHSUFFIX);
#endif
			safecat(buildPath, "/");
#if RSG_ORBIS
			// the orbis are in the ps4 specific folder, adding ps4 specific fix, can move to RSG_PLATFORM_ID when that changes to PS4
			safecat(buildPath, RSG_PLATFORM); 
#else
			safecat(buildPath, RSG_PLATFORM_ID); 
#endif
			safecat(buildPath, "/anim/cutscene/");
			safecpy(replacementName, buildPath, RAGE_MAX_PATH);

			strcat(replacementName, filename);

			isReplaced = true;
			optionalReplacement = true;
		}
#if __BANK
		else
		{
			const char * source =  stristr(finalName , RSG_PLATFORM_ID"/anim/cutscene/");
			if(source)
			{

				char buildPath[RAGE_MAX_PATH];
				safecpy(buildPath, "assets:/non_final/build/dev");
#ifdef RS_BRANCHSUFFIX
				safecat(buildPath, RS_BRANCHSUFFIX);
#endif
				safecat(buildPath, "/");
#if RSG_ORBIS
				// the orbis are in the ps4 specific folder, adding ps4 specific fix, can move to RSG_PLATFORM_ID when that changes to PS4
				safecat(buildPath, RSG_PLATFORM); 
#else
				safecat(buildPath, RSG_PLATFORM_ID); 
#endif
				safecat(buildPath, "/anim/cutscene/"); 

				safecpy(replacementName, buildPath, RAGE_MAX_PATH);

				strcat(replacementName, filename);

				isReplaced = true;
				optionalReplacement = true;
			}
		}
#endif // __BANK
	}
#endif

	//PTFX settings
	if (stricmp(filename, "ptfx.rpf") == 0 )
	{
#if RSG_PC
		//PC specific
		safecpy(replacementName, finalName, string_length_of_path);
		strcat(replacementName, "/ptfx");
		strcat(replacementName, GetPtfxFileSuffix());
		strcat(replacementName, ".rpf");
		isReplaced = true;
#endif // __RSG_PC

#if !__FINAL
		//Check for ptfx debug overrides
		if (PARAM_ptfxassetoverride.Get())
		{
			const char* pPtfxAssetOverride = NULL;
			PARAM_ptfxassetoverride.Get(pPtfxAssetOverride);

			safecpy(replacementName, finalName, string_length_of_path);
			strcat(replacementName, "/ptfx");
			strcat(replacementName, pPtfxAssetOverride);
			strcat(replacementName, ".rpf");
			isReplaced = true;
			optionalReplacement = true;
		}
#endif
	}

	DATAFILEMGR.ExpandFilename(replacementName,expandedName,RAGE_MAX_PATH);
	if(isReplaced)
	{
		if(ASSET.Exists(expandedName,""))
		{
			Displayf("Found replacement for %s New file: %s",finalName,replacementName);
			return true;
		}
		else
		{
			Warningf("Couldn't find %s, falling back to the default version %s",replacementName,finalName);
			Assertf(optionalReplacement,"Tried to replace %s, but we couldn't find the replacement file: %s",finalName,replacementName);
			return false;
		}
	}
	return isReplaced;
}


//
//        name: CFileLoader::LoadLine
// description: Load line from ASCII file and clear out all crap (commas etc)
//          in: fid = file handle
//         out: pointer to start of text in line
//
char* CFileLoader::LoadLine(FileHandle fid)
{
	s32 linePosn = 0;
	char *pLineStart = &ms_line[0];

	if(!CFileMgr::ReadLine(fid, &ms_line[0], MAX_LINE_LENGTH))
		return NULL;

	// first, strip out all commas and escape sequence chars, and replace them with spaces
	while (ms_line[linePosn] != '\0')
	{
		if (ms_line[linePosn] < ' ' || ms_line[linePosn] == ',')
		{
			ms_line[linePosn] = ' ';
		}
		linePosn++;
	}

	// get beginning of line first non-space/non-control character
	while(*pLineStart <= ' ' && *pLineStart != '\0')
		pLineStart++;

	//!XBOX - for some reason lines are picking up trailing spaces in xbox version. Need removed.
#if defined (GTA_XBOX)
	if (ms_line[linePosn-1] == ' ')
	{
		ms_line[linePosn-1] = '\0';
	}
#endif //GTA_XBOX

	return pLineStart;
}

//
//        name: CFileLoader::LoadLine
// description: Load line from ASCII file and clear out all crap (commas etc)
//          in: fid = file handle
//         out: pointer to start of text in line
//
char* CFileLoader::LoadLine(u8** ppData, s32& size)
{
	static char gLine[512];
	char *pLineStart = &gLine[0];
	u8* pData = *ppData;

	if(size <= 0 || *pData == '\0')
		return NULL;

	// copy line striping out commas and escape sequence chars by replacing them
	// with spaces
	while(size-- && *pData != '\n' && *pData != '\0')
	{
		if(*pData < ' ' || *pData == ',')
			*pLineStart = ' ';
		else
			*pLineStart = *pData;

		pLineStart++;
		pData++;
		//size--; // moved to while to make sure carriage returns are counted as taken off of data as well.
	}

	pData++;
	*ppData = pData;

	// end line

	*pLineStart = '\0';
	pLineStart = &gLine[0];

	// get beginning of line first non-space/non-control character
	while(*pLineStart <= ' ' && *pLineStart != '\0')
		pLineStart++;

	return pLineStart;
}

s32 CFileLoader::AddRpfFile(const CDataFileMgr::DataFile *pData, bool registerFiles)
{
	strLocalIndex imageIndex = strLocalIndex(-1);

	s32 cachePartition = -1;	//	This used to be read from gta5.dat as a number after the filename
	const char *finalName = pData->m_filename;

	while (true)
	{
		strStreamingEngine::GetLoader().CallKeepAliveCallbackIfNecessary();
#if (__XENON || __PS3)
		// don't attempt to completely load meta data on last gen platforms - there isn't enough memory to do it.
		imageIndex = strPackfileManager::AddImageToList(finalName, true, cachePartition, pData->m_locked, (u8) pData->m_contents, false, pData->m_overlay);
#else //(__XENON || __PS3)
		imageIndex = strPackfileManager::AddImageToList(finalName, true, cachePartition, pData->m_locked, (u8) pData->m_contents, pData->m_loadCompletely, pData->m_overlay, pData->m_patchFile);
#endif //(__XENON || __PS3)

		if (imageIndex != -1)
		{
#if __DEV
			strPackfileManager::SetEnforceLsnSorting(imageIndex.Get(), pData->m_enforceLsnSorting);
#endif // __DEV

			if (registerFiles)
			{
				bool wasUnloaded = strPackfileManager::GetStreamingModule()->GetStreamingInfo(imageIndex)->GetStatus() == STRINFO_NOTLOADED;

				strPackfileManager::LoadImageIfUnloaded(imageIndex.Get(), false);
				// If it was not loaded before, keep it that way.
				if (wasUnloaded)
				{
					strPackfileManager::UnloadRpf(imageIndex.Get());
				}
			}
		}
		else
		{
			{
#if RSG_PC || !__NO_OUTPUT
				const char *source = "?";
#endif // RSG_PC || !__NO_OUTPUT

	#if !__FINAL
				source = pData->m_Source.GetCStr();
	#endif // !__FINAL
				Quitf(ERR_SYS_FILELOAD,"RPF file '%s' is missing. a) Make sure you have it locally, get it from Perforce if necessary. Maybe you're using a label that doesn't have this file. b) The file is referenced by '%s', maybe it shouldn't be.",
					finalName, source);
			}
		}

		break;
	}

	return imageIndex.Get();
}
void CFileLoader::ChangeSetAction_LoadRPF( CDataFileMgr::DataFile* pData )
{
    // If it's not already enabled, enable and load it.
    if (pData->m_disabled)
    {
        fileloaderDebugf1("ChangeSetAction_LoadRPF: Enabling RPF file %s", pData->m_filename);

        pData->m_disabled = false;
        s32 archiveIndex = AddRpfFile(pData, true);

		// When loading map changes make sure we re-validate hierarchies
		if (EXTRACONTENT.GetMapChangeState() == MCS_UPDATE)
			strStreamingEngine::GetInfo().ModifyHierarchyStatusForArchive(archiveIndex, HMT_ENABLE);

        if (archiveIndex != -1 && pData->m_locked)
        {
			// Normally, user-locked RPFs get their ref count incremented by OpenImageList, which
			// is not getting called in this case.
			strPackfileManager::AddStreamableReference(archiveIndex);

			// Re-enable this archive again.
			strPackfileManager::SetEnabled(archiveIndex, true);
        }
    }
    else
    {
        fileloaderDebugf1("ChangeSetAction_LoadRPF: RPF file %s was already enabled", pData->m_filename);
    }
}

void CFileLoader::ChangeSetAction_UnloadRPF(CDataFileMgr::DataFile* pData)
{
    // If it's not already disabled, unload and disable it.
    if (!pData->m_disabled)
    {
        char filename[RAGE_MAX_PATH];
        ASSET.FullPath(filename, sizeof(filename), pData->m_filename, "");

        fileloaderDebugf1("ChangeSetAction_UnloadRPF: Disabling RPF file %s", filename);

        strLocalIndex archiveIndex = strLocalIndex(strPackfileManager::FindImage(filename));

        if (Verifyf(archiveIndex != -1, "RPF file '%s' has never been registered - are we calling DisableAllRpfsByContent() before AddAllRpfFiles()?", filename))
        {
            // Remove the user lock, if there is one.
            strPackfileManager::RemoveUserLock(archiveIndex.Get());

			// When unloading map changes make sure we invalidate hierarchies
			if (EXTRACONTENT.GetMapChangeState() == MCS_UPDATE)
			{
				eHierarchyModType modType = HMT_DISABLE;

				if (pData->m_overlay)
					modType = HMT_FLUSH;

				// Attempt to stream out any files in this archive before we invalidate it, hopefully meaning it won't be in use.
				strStreamingEngine::GetInfo().ModifyHierarchyStatusForArchive(archiveIndex.Get(), modType);
			}

            // We're planning on killing all streamables using this archive, so we have to invalidate their handles.
			// TODO: Consider passing "true" here - that way, we free up the strStreamingInfos. But let's wait with that at least
			// until we have paged pools enabled.
            strStreamingEngine::GetInfo().InvalidateFilesForArchive(archiveIndex.Get(), false);

            // Now try to unload the RPF file. We're assuming that there are no dependencies
            // on this RPF - if the user calls this function while this RPF is needed,
            // the function we're calling here will assert.
            strPackfileManager::CloseArchive(archiveIndex.Get());

            // Don't open this archive again.
            strPackfileManager::SetEnabled(archiveIndex.Get(), false);
        }

        pData->m_disabled = true;
    }
    else
    {
        fileloaderDebugf1("ChangeSetAction_UnloadRPF: RPF file %s was already disable", pData->m_filename);
    }
}
void CFileLoader::ChangeSetAction_LoadTxd(const atHashString * txd)
{
    strLocalIndex index = strLocalIndex(g_TxdStore.FindSlotFromHashKey(*txd));

    if (Verifyf(index != -1, "Unable to find TXD '%s' that we're meant to load", txd->TryGetCStr()))
    {
        fileloaderDebugf1("ChangeSetAction_LoadTxd: Loading TXD %s", g_TxdStore.GetName(index));

        strIndex stIndex = g_TxdStore.GetStreamingIndex(index);
        strStreamingEngine::GetInfo().RequestObject(stIndex, STRFLAG_FORCE_LOAD);
    }
}

void CFileLoader::ChangeSetAction_UnloadTxd(const atHashString * txd)
{
    strLocalIndex index = strLocalIndex(g_TxdStore.FindSlotFromHashKey(*txd));

    if (Verifyf(index != -1, "Unable to find TXD '%s' that we're meant to unload", txd->TryGetCStr()))
    {
        fileloaderDebugf1("ChangeSetAction_UnloadTxd: Unloading TXD %s", g_TxdStore.GetName(index));

        g_TxdStore.RemoveRef(index, REF_OTHER);
        int refCnt = g_TxdStore.GetNumRefs(index);

        if (Verifyf(refCnt == 0, "TXD '%s' that we're meant to unload still has ref count %d", txd->TryGetCStr(), refCnt))
        {
            strIndex stIndex = g_TxdStore.GetStreamingIndex(index);
            strStreamingEngine::GetInfo().RemoveObject(stIndex);
        }
    }
}


void CFileLoader::ChangeSetAction_LoadResidentResource(const CDataFileMgr::ResourceReference * ref)
{
    strIndex index = ref->Resolve();

    if (index.IsValid())
    {
        fileloaderDebugf1("ChangeSetAction_LoadResidentResource: Loading file %s", strStreamingEngine::GetObjectName(index));
        strStreamingEngine::GetInfo().RequestObject(index, STRFLAG_DONTDELETE);
    }
}

void CFileLoader::ChangeSetAction_UnloadResidentResource(const CDataFileMgr::ResourceReference * ref)
{
    strIndex index = ref->Resolve();

    if (index.IsValid())
    {
        fileloaderDebugf1("ChangeSetAction_UnloadResidentResource: removing file %s", strStreamingEngine::GetObjectName(index));
        strStreamingEngine::GetInfo().SetObjectIsDeletable(index);
        strStreamingEngine::GetInfo().RemoveObject(index);
	}
}

void CFileLoader::ChangeSetAction_UnregisterResource(const CDataFileMgr::ResourceReference * ref)
{
	strIndex index = ref->Resolve();

	if (index.IsValid())
	{
		fileloaderDebugf1("ChangeSetAction_UnregisterResource: unregistering file %s", strStreamingEngine::GetObjectName(index));

#if __ASSERT
		strStreamingInfo &info = strStreamingEngine::GetInfo().GetStreamingInfoRef(index);
		Assertf(info.GetStatus() == STRINFO_NOTLOADED, "Trying to unregister %s, but it's still %s", strStreamingEngine::GetObjectName(index), info.GetFriendlyStatusName());
#endif // __ASSERT

		strStreamingModule *module = strStreamingEngine::GetInfo().GetModule(index);
		strLocalIndex objIndex = module->GetObjectIndex(index);

		module->RemoveSlot(objIndex);
	}
}

void CFileLoader::ChangeSetAction_ModifyFileValidity(s32 archiveIndex, bool invalid)
{
	if (Verifyf(archiveIndex != -1, "ChangeSetAction_ModifyFileValidity: RPF file has never been registered!"))
	{
		if ( strPackfileManager::GetImageFile(archiveIndex) )
		{
			fileloaderDebugf1("ChangeSetAction_ModifyFileValidity: Invalidating RPF file %s", strPackfileManager::GetImageFile(archiveIndex)->m_name.GetCStr());

			strStreamingEngine::GetInfo().ModifyHierarchyStatusForArchive(archiveIndex, invalid ? HMT_DISABLE : HMT_ENABLE);
		}
	}
}

void CFileLoader::ExecuteChangeSetAction(const CDataFileMgr::ChangeSetAction &action)
{
	strStreamingEngine::GetLoader().CallKeepAliveCallbackIfNecessary();

    switch (action.m_actionType)
    {
		case CDataFileMgr::ChangeSetAction::CHANGE_SET_ACTION_INVALIDATE_FILE:
		{
			ChangeSetAction_ModifyFileValidity(action.archiveIndex, action.m_add);
		}
		break;

		case CDataFileMgr::ChangeSetAction::CHANGE_SET_ACTION_INVALIDATE_INTERIOR:
		{
			if (CInteriorProxy* pProxy = CInteriorProxy::FindProxy(action.archiveIndex))
			{
				pProxy->SetIsDisabledDLC(action.m_add);
			}
		}
		break;

		case CDataFileMgr::ChangeSetAction::CHANGE_SET_ACTION_INVALIDATE_STREAMABLE:
		{
			if (action.m_strIndex.IsValid())
			{
				strStreamingEngine::GetInfo().RemoveObjectAndDependents(action.m_strIndex, action.m_add ? HMT_DISABLE : HMT_ENABLE);
			}
		}
		break;

		case CDataFileMgr::ChangeSetAction::CHANGE_SET_ACTION_RPF:
        {
            if (action.m_add)
			{
				fileloaderDebugf1("ChangeSetAction_LoadRPF %s", action.m_file->m_filename);
                ChangeSetAction_LoadRPF(action.m_file);
			}
            else
			{
				fileloaderDebugf1("ChangeSetAction_UnloadRPF %s", action.m_file->m_filename);
                ChangeSetAction_UnloadRPF(action.m_file);
			}
        }
        break;

		case CDataFileMgr::ChangeSetAction::CHANGE_SET_ACTION_DATA_FILE:
        {
			bool skipDisable = IsValidRpfType(action.m_file->m_fileType);
            if (action.m_add)
            {
                fileloaderDebugf1("ChangeSetAction:Loading data file %s", action.m_file->m_filename);
				action.m_file->m_disabled = !skipDisable ? false : action.m_file->m_disabled;
                CDataFileMount::LoadFile(*action.m_file);
            }
            else
            {
				fileloaderDebugf1("ChangeSetAction:Unloading data file %s", action.m_file->m_filename);
				action.m_file->m_disabled = !skipDisable ? true : action.m_file->m_disabled;
                CDataFileMount::UnloadFile(*action.m_file);
            }
        }
        break;

		case CDataFileMgr::ChangeSetAction::CHANGE_SET_ACTION_TXD:
        {
            if (action.m_add)
                ChangeSetAction_LoadTxd(action.m_txd);
            else
                ChangeSetAction_UnloadTxd(action.m_txd);
        }
        break;

		case CDataFileMgr::ChangeSetAction::CHANGE_SET_ACTION_RESIDENT_RESOURCE:
        {
            if (action.m_add)
                ChangeSetAction_LoadResidentResource(action.m_residentResource);
            else
                ChangeSetAction_UnloadResidentResource(action.m_residentResource);
        }
        break;
        
		case CDataFileMgr::ChangeSetAction::CHANGE_SET_ACTION_LOAD_REQUESTED_OBJECTS:
        {
            if (action.m_add) //doesn't have any meaning unloading
            {
                fileloaderDebugf1("ChangeSetAction: LoadAllRequestedObjects");
                CStreaming::LoadAllRequestedObjects();
            }
        }
        break;

		case CDataFileMgr::ChangeSetAction::CHANGE_SET_ACTION_UNREGISTER_RESOURCE:
		{
			if (action.m_add)
			{
				fileloaderDebugf1("ChangeSetAction: UnregisterResource");
				ChangeSetAction_UnregisterResource(action.m_residentResource);
			}
		}
		break;

		default:
        {
            fileloaderWarningf("Unexpected CCS action type : %d",action.m_actionType);
        }
        break;
    }
}

void CFileLoader::ExecuteContentChangeSet(const CDataFileMgr::ContentChangeSet &contentChangeSet)
{
	fileloaderDebugf1("Executing content change set %s", contentChangeSet.m_changeSetName.c_str());

    atArray<CDataFileMgr::ChangeSetAction> actionList;
	u32 actionMask = CDataFileMgr::ChangeSetAction::CCS_ALL_ACTIONS;

    BuildContentChangeSetActionList(contentChangeSet, actionList, actionMask, CDataFileMgr::ChangeSetAction::CSA_FIRST_PRIORITY, true);

    int numActions = actionList.GetCount();
    for (int i = 0; i < numActions; i++)
    {
        ExecuteChangeSetAction(actionList[i]);
    }

	// MASSIVE HACK:
	if (atStringHash(contentChangeSet.m_changeSetName.c_str()) == ATSTRINGHASH("MPBUSINESS_AUTOGEN", 0xA02B068C))
		MeshBlendManager::GetInstance().ExtraContentUpdated(true);
}

void CFileLoader::RevertContentChangeSet(const CDataFileMgr::ContentChangeSet &contentChangeSet, u32 actionMask)
{
    fileloaderDebugf1("Reverting content change set %s", contentChangeSet.m_changeSetName.c_str());

    atArray<CDataFileMgr::ChangeSetAction> actionList;

    BuildContentChangeSetActionList(contentChangeSet, actionList, actionMask, CDataFileMgr::ChangeSetAction::CSA_LAST_PRIORITY, false);

	// MASSIVE HACK:
	if (atStringHash(contentChangeSet.m_changeSetName.c_str()) == ATSTRINGHASH("MPBUSINESS_AUTOGEN", 0xA02B068C))
		MeshBlendManager::GetInstance().ExtraContentUpdated(false);

    int numActions = actionList.GetCount();
    for (int i = numActions-1; i >= 0; i--)
    {
        actionList[i].m_add = !actionList[i].m_add;
        ExecuteChangeSetAction(actionList[i]);
    }
	
}
CDataFileMgr::ChangeSetAction & CFileLoader::AppendChangeSetAction(atArray<CDataFileMgr::ChangeSetAction> & actionList, CDataFileMgr::ChangeSetAction::actionTypeEnum type, bool add, 
																   u32 priority /* = CDataFileMgr::ChangeSetAction::NO_PRIORITY */)
{
    fileloaderDebugf3("AppendChangeSetAction index:%d type:%d add:%d",actionList.GetCount(),type,add);
    CDataFileMgr::ChangeSetAction & action = actionList.Grow();
    action.m_actionType = type;
    action.m_add = add;
	action.m_priority = (u8)priority;
    return action;
}

u32 CFileLoader::GetDataTypePriority(CDataFileMgr::DataFileType type, const CDataFileMgr::DataFileType * typeList, int listLen)
{
    for (int i = 0 ; i < listLen; i ++)
    {
        if (typeList[i] == type)
            return (i + 1); // Allow space for CSA_FIRST_PRIORITY
    }

    return CDataFileMgr::ChangeSetAction::CSA_NO_PRIORITY;
}

#if __ASSERT
#define CHECK_DATA_FILES(ptr,expandedFilename,changeSetName)\
	const fiDevice *device = fiDevice::GetDevice(expandedFilename,false);\
	char path[RAGE_MAX_PATH]={0};\
	if(device)\
	device->FixRelativeName(path, RAGE_MAX_PATH, expandedFilename);\
	if(strcmp(path,expandedFilename)==0)\
	{\
		Assertf(ptr,"File %s not found in CCS %s it is likely that the DLC rpf's need updating",expandedFilename,changeSetName);\
	}\
		else\
	{\
		Assertf(ptr,"File %s not found in CCS %s , Full Path: %s",expandedFilename,changeSetName,path);\
	}
#else
#define CHECK_DATA_FILES(ptr,expandedFilename,changeSetName)
#endif

#define ENUMERATE_DATA_FILES(array,ptr) for (const atString * filename =  (array).begin(); filename != (array).end(); filename++)\
{\
    char expandedFilename[RAGE_MAX_PATH];\
    DATAFILEMGR.ExpandFilename(*filename, expandedFilename, sizeof(expandedFilename));\
	CDataFileMgr::DataFile * ptr = DATAFILEMGR.FindDataFile(atStringHash(expandedFilename));\
	CHECK_DATA_FILES(ptr,expandedFilename,contentChangeSet.m_changeSetName.c_str())\
	if (ptr)

#define ENUMERATE_DATA_FILES_END }

strIndex GetStreamIndexFromFileAndExt(const char* filePath)
{
	strIndex returnIndex = strIndex();
	const char* extPattern = ASSET.FindExtensionInPath(filePath);
	char newPath[256] = { 0 };

	if (extPattern)
	{
		extPattern++;

		if (const char* platformExt = strStreamingEngine::GetInfo().GetModuleMgr().getPlatExtFromExtPattern(extPattern))
		{
			if (strStreamingModule* module = strStreamingEngine::GetInfo().GetModuleMgr().GetModuleFromFileExtension(platformExt))
			{
				ASSET.RemoveExtensionFromPath(newPath, sizeof(newPath), filePath);

				strLocalIndex slot = module->FindSlot(newPath);

				if (Verifyf(slot.IsValid(), "Cannot find %s in module %s", filePath, module->GetModuleName()))
					returnIndex = module->GetStreamingIndex(slot);
			}
		}
	}

	return returnIndex;
}

void CFileLoader::BuildInvalidateList(const atArray<atString>& filesToInvalidate, atArray<CDataFileMgr::ChangeSetAction>& actionList, u32 actionMask)
{
	for (u32 i = 0; i < filesToInvalidate.GetCount(); i++)
	{
		const atString& currFile = filesToInvalidate[i];
		const char* extension = ASSET.FindExtensionInPath(currFile.c_str());

		// Check the extension and store the image index as map data files are removed from DataFileManager after being loaded...
		if (extension)
		{
			char filename[RAGE_MAX_PATH] = { 0 };

			++extension;

			// Able to invalidate a whole RPF, an interior or a single streamable
			if (stricmp(extension, PI_ARCHIVE_FILE_EXT) == 0 && BIT_ISSET((u32)CDataFileMgr::ChangeSetAction::CHANGE_SET_ACTION_INVALIDATE_FILE, actionMask))
			{
				char expandedFilename[RAGE_MAX_PATH] = { 0 };

				ASSET.FullPath(filename, sizeof(filename), currFile.c_str(), "");
				DATAFILEMGR.ExpandFilename(filename, expandedFilename, sizeof(expandedFilename));

				s32 archiveIndex = strPackfileManager::FindImage(expandedFilename);

				if (const atArray<s32>* associatedFiles = strStreamingEngine::GetInfo().GetPackfileRelationships(archiveIndex))
				{
					for (s32 k = 0; k < associatedFiles->GetCount(); k++)
					{
						archiveIndex = (*associatedFiles)[k];

						if (Verifyf(archiveIndex != -1, "BuildInvalidateList - cannot find archive %s", currFile.c_str()))
						{
							AppendChangeSetAction(actionList, CDataFileMgr::ChangeSetAction::CHANGE_SET_ACTION_INVALIDATE_FILE, 
								true, CDataFileMgr::ChangeSetAction::CSA_FIRST_PRIORITY).archiveIndex = archiveIndex;
						}
					}
				}
				else
				{
					if (Verifyf(archiveIndex != -1, "BuildInvalidateList - cannot find archive %s", currFile.c_str()))
					{
						AppendChangeSetAction(actionList, CDataFileMgr::ChangeSetAction::CHANGE_SET_ACTION_INVALIDATE_FILE, 
							true, CDataFileMgr::ChangeSetAction::CSA_FIRST_PRIORITY).archiveIndex = archiveIndex;
					}
				}
			}
			else if (stricmp(extension, "interior") == 0 && BIT_ISSET((u32)CDataFileMgr::ChangeSetAction::CHANGE_SET_ACTION_INVALIDATE_INTERIOR, actionMask))
			{
				ASSET.RemoveExtensionFromPath(filename, sizeof(filename), currFile.c_str());
				strLocalIndex slotIndex = fwMapDataStore::GetStore().FindSlot(filename);

				if (Verifyf(slotIndex != -1, "BuildInvalidateList - cannot find interior %s", currFile.c_str()))
				{
					AppendChangeSetAction(actionList, CDataFileMgr::ChangeSetAction::CHANGE_SET_ACTION_INVALIDATE_INTERIOR, 
						true, CDataFileMgr::ChangeSetAction::CSA_FIRST_PRIORITY).archiveIndex = slotIndex.Get();
				}
			}
			else if (BIT_ISSET((u32)CDataFileMgr::ChangeSetAction::CHANGE_SET_ACTION_INVALIDATE_STREAMABLE, actionMask))
			{
				strIndex tempStrIdx = GetStreamIndexFromFileAndExt(currFile.c_str());

				if (Verifyf(tempStrIdx.IsValid(), "BuildInvalidateList - cannot find streamable %s", currFile.c_str()))
				{
					AppendChangeSetAction(actionList, CDataFileMgr::ChangeSetAction::CHANGE_SET_ACTION_INVALIDATE_STREAMABLE, 
						true, CDataFileMgr::ChangeSetAction::CSA_FIRST_PRIORITY).m_strIndex = tempStrIdx;
				}
			}
		}
	}
}

void CFileLoader::BuildRPFEnablementList(const CDataFileMgr::ContentChangeSet& ASSERT_ONLY(contentChangeSet), const atArray<atString>& fileList, 
	atArray<CDataFileMgr::ChangeSetAction>& actionList, bool add, u32 mapDataPriority)
{
	ENUMERATE_DATA_FILES(fileList, dataFile)
	{
		if (IsValidRpfType(dataFile->m_fileType))
		{
			u32 priority = CDataFileMgr::ChangeSetAction::CSA_NO_PRIORITY;

			if (dataFile->m_contents == CDataFileMgr::CONTENTS_DLC_MAP_DATA)
				priority = mapDataPriority;

			AppendChangeSetAction(actionList, CDataFileMgr::ChangeSetAction::CHANGE_SET_ACTION_RPF, add, priority).m_file = dataFile;
		}
	}
	ENUMERATE_DATA_FILES_END
}

void CFileLoader::BuildDataFileEnablementList(const CDataFileMgr::ContentChangeSet& ASSERT_ONLY(contentChangeSet), const atArray<atString>& fileList, 
	atArray<CDataFileMgr::ChangeSetAction>& actionList, bool add, bool inverter)
{
	//File types in this list will be loaded before streaming objects are requested
	CDataFileMgr::DataFileType earlyDataTypes[] = 
	{ 
		CDataFileMgr::WEAPONCOMPONENTSINFO_FILE,
		CDataFileMgr::CLIP_SETS_FILE, 
		CDataFileMgr::WEAPON_ANIMATIONS_FILE,
		CDataFileMgr::WEAPON_METADATA_FILE,
		CDataFileMgr::INTERIOR_PROXY_ORDER_FILE,
		CDataFileMgr::HANDLING_FILE,
		CDataFileMgr::VEHICLE_LAYOUTS_FILE,
		CDataFileMgr::PED_METADATA_FILE,
		CDataFileMgr::DLC_POP_GROUPS
	};

	int numEarlyDataTypes = NELEM(earlyDataTypes);

	ENUMERATE_DATA_FILES(fileList, dataFile)
	{
		u32 priority = GetDataTypePriority(dataFile->m_fileType, earlyDataTypes, numEarlyDataTypes);

		if (dataFile->m_fileType != CDataFileMgr::RPF_FILE && dataFile->m_fileType != CDataFileMgr::RPF_FILE_PRE_INSTALL && (inverter == (priority == CDataFileMgr::ChangeSetAction::CSA_NO_PRIORITY)))
		{
			AppendChangeSetAction(actionList, CDataFileMgr::ChangeSetAction::CHANGE_SET_ACTION_DATA_FILE, add, priority).m_file = dataFile;
		}
	}
	ENUMERATE_DATA_FILES_END
}

void CFileLoader::BuildTxdEnablementList(const atArray<atHashString>& fileList, atArray<CDataFileMgr::ChangeSetAction>& actionList, bool add)
{
	for (const atHashString * txd = fileList.begin(); txd != fileList.end(); txd++)
		AppendChangeSetAction(actionList, CDataFileMgr::ChangeSetAction::CHANGE_SET_ACTION_TXD, add).m_txd = txd;
}

void CFileLoader::BuildContentChangeSetActionList(const CDataFileMgr::ContentChangeSet &contentChangeSet, 
	atArray<CDataFileMgr::ChangeSetAction> & actionList, u32 actionMask, u32 mapDataPriority, bool execute)
{
    fileloaderDebugf1("BuildContentChangeSetActionList");
	
	// ----- Invalidate files
	fileloaderDebugf2("BuildContentChangeSetActionList: Invalidate file(s)");
	BuildInvalidateList(contentChangeSet.m_filesToInvalidate, actionList, actionMask);

	for (u32 i = 0; i < contentChangeSet.m_mapChangeSetData.GetCount(); i++)
	{
		if (contentChangeSet.m_mapChangeSetData[i].IsValid(execute))
			BuildInvalidateList(contentChangeSet.m_mapChangeSetData[i].m_filesToInvalidate, actionList, actionMask);
	}

	if (BIT_ISSET((u32)CDataFileMgr::ChangeSetAction::CHANGE_SET_ACTION_TXD, actionMask))
	{
		// ----- Unload txds
		fileloaderDebugf2("BuildContentChangeSetActionList: Unload txds");

		BuildTxdEnablementList(contentChangeSet.m_txdToUnload, actionList, false);

		for (u32 i = 0; i < contentChangeSet.m_mapChangeSetData.GetCount(); i++)
		{
			if (contentChangeSet.m_mapChangeSetData[i].IsValid(execute))
				BuildTxdEnablementList(contentChangeSet.m_mapChangeSetData[i].m_txdToUnload, actionList, false);
		}
	}

	if (BIT_ISSET((u32)CDataFileMgr::ChangeSetAction::CHANGE_SET_ACTION_RPF, actionMask))
	{
		// ----- Disable RPFs
		fileloaderDebugf2("BuildContentChangeSetActionList: Disable RPFs");

		BuildRPFEnablementList(contentChangeSet, contentChangeSet.m_filesToDisable, actionList, false, mapDataPriority);

		for (u32 i = 0; i < contentChangeSet.m_mapChangeSetData.GetCount(); i++)
		{
			if (contentChangeSet.m_mapChangeSetData[i].IsValid(execute))
				BuildRPFEnablementList(contentChangeSet, contentChangeSet.m_mapChangeSetData[i].m_filesToDisable, actionList, false, mapDataPriority);
		}
	}
   
	if (BIT_ISSET((u32)CDataFileMgr::ChangeSetAction::CHANGE_SET_ACTION_RPF, actionMask))
	{
		// ----- Enable RPFs
		fileloaderDebugf2("BuildContentChangeSetActionList: Enable RPFs");

		BuildRPFEnablementList(contentChangeSet, contentChangeSet.m_filesToEnable, actionList, true, mapDataPriority);

		for (u32 i = 0; i < contentChangeSet.m_mapChangeSetData.GetCount(); i++)
		{
			if (contentChangeSet.m_mapChangeSetData[i].IsValid(execute))
				BuildRPFEnablementList(contentChangeSet, contentChangeSet.m_mapChangeSetData[i].m_filesToEnable, actionList, true, mapDataPriority);
		}
	}

	if (BIT_ISSET((u32)CDataFileMgr::ChangeSetAction::CHANGE_SET_ACTION_TXD, actionMask))
	{
		// ----- Load txds
		fileloaderDebugf2("BuildContentChangeSetActionList: Load txds");

		BuildTxdEnablementList(contentChangeSet.m_txdToLoad, actionList, true);

		for (u32 i = 0; i < contentChangeSet.m_mapChangeSetData.GetCount(); i++)
		{
			if (contentChangeSet.m_mapChangeSetData[i].IsValid(execute))
				BuildTxdEnablementList(contentChangeSet.m_mapChangeSetData[i].m_txdToLoad, actionList, true);
		}
	}

	if (BIT_ISSET((u32)CDataFileMgr::ChangeSetAction::CHANGE_SET_ACTION_DATA_FILE, actionMask))
	{
		// ----- Early data
		fileloaderDebugf2("BuildContentChangeSetActionList: Early data (disable)");

		BuildDataFileEnablementList(contentChangeSet, contentChangeSet.m_filesToDisable, actionList, false, false);

		for (u32 i = 0; i < contentChangeSet.m_mapChangeSetData.GetCount(); i++)
		{
			if (contentChangeSet.m_mapChangeSetData[i].IsValid(execute))
				BuildDataFileEnablementList(contentChangeSet, contentChangeSet.m_mapChangeSetData[i].m_filesToDisable, actionList, false, false);
		}
	}

	size_t filesToEnableOffset = actionList.GetCount();

	std::sort(actionList.begin(), actionList.end());

	if (BIT_ISSET((u32)CDataFileMgr::ChangeSetAction::CHANGE_SET_ACTION_DATA_FILE, actionMask))
	{
		fileloaderDebugf2("BuildContentChangeSetActionList: Early data (enable)");

		BuildDataFileEnablementList(contentChangeSet, contentChangeSet.m_filesToEnable, actionList, true, false);

		for (u32 i = 0; i < contentChangeSet.m_mapChangeSetData.GetCount(); i++)
		{
			if (contentChangeSet.m_mapChangeSetData[i].IsValid(execute))
				BuildDataFileEnablementList(contentChangeSet, contentChangeSet.m_mapChangeSetData[i].m_filesToEnable, actionList, true, false);
		}
	}

	std::sort(actionList.begin() + filesToEnableOffset, actionList.end());

	if (BIT_ISSET((u32)CDataFileMgr::ChangeSetAction::CHANGE_SET_ACTION_RESIDENT_RESOURCE, actionMask))
	{
		// ----- Request objs
		fileloaderDebugf2("BuildContentChangeSetActionList: Request objs");
		for (const CDataFileMgr::ResourceReference * residentResource = contentChangeSet.m_residentResources.begin(); residentResource != contentChangeSet.m_residentResources.end(); residentResource++)
			AppendChangeSetAction(actionList, CDataFileMgr::ChangeSetAction::CHANGE_SET_ACTION_RESIDENT_RESOURCE, true).m_residentResource = residentResource;
	}

    // ----- Wait for loading objs
    fileloaderDebugf2("BuildContentChangeSetActionList: Wait for loading objs");
    AppendChangeSetAction(actionList, CDataFileMgr::ChangeSetAction::CHANGE_SET_ACTION_LOAD_REQUESTED_OBJECTS, true);

	if (BIT_ISSET((u32)CDataFileMgr::ChangeSetAction::CHANGE_SET_ACTION_DATA_FILE, actionMask))
	{
		// ----- Rest of data
		fileloaderDebugf2("BuildContentChangeSetActionList: Rest of data (disable)");
		BuildDataFileEnablementList(contentChangeSet, contentChangeSet.m_filesToDisable, actionList, false, true);

		for (u32 i = 0; i < contentChangeSet.m_mapChangeSetData.GetCount(); i++)
		{
			if (contentChangeSet.m_mapChangeSetData[i].IsValid(execute))
				BuildDataFileEnablementList(contentChangeSet, contentChangeSet.m_mapChangeSetData[i].m_filesToDisable, actionList, false, true);
		}
	}

	if (BIT_ISSET((u32)CDataFileMgr::ChangeSetAction::CHANGE_SET_ACTION_DATA_FILE, actionMask))
	{
		fileloaderDebugf2("BuildContentChangeSetActionList: Rest of data (enable)");
		BuildDataFileEnablementList(contentChangeSet, contentChangeSet.m_filesToEnable, actionList, true, true);

		for (u32 i = 0; i < contentChangeSet.m_mapChangeSetData.GetCount(); i++)
		{
			if (contentChangeSet.m_mapChangeSetData[i].IsValid(execute))
				BuildDataFileEnablementList(contentChangeSet, contentChangeSet.m_mapChangeSetData[i].m_filesToEnable, actionList, true, true);
		}
	}

	if (BIT_ISSET((u32)CDataFileMgr::ChangeSetAction::CHANGE_SET_ACTION_UNREGISTER_RESOURCE, actionMask))
	{
		fileloaderDebugf2("BuildContentChangeSetActionList: Unregister objs");
		for (const CDataFileMgr::ResourceReference * residentResource = contentChangeSet.m_unregisterResources.begin(); residentResource != contentChangeSet.m_unregisterResources.end(); residentResource++)
		{
			AppendChangeSetAction(actionList, CDataFileMgr::ChangeSetAction::CHANGE_SET_ACTION_UNREGISTER_RESOURCE, true).m_residentResource = residentResource;
		}
	}
}

#undef ENUMERATE_DATA_FILES

#undef ENUMERATE_DATA_FILES_END


void CFileLoader::AddAllRpfFiles(bool registerFiles)
{
	USE_MEMBUCKET(MEMBUCKET_WORLD);

	const u32 preInstallCount = NELEM(ms_validRpfTypesPreInstall);
	const u32 count = NELEM(ms_validRpfTypes);

	u32 iType = 0;

	do 
	{
		// Load RPF files
		const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CFileMgr::IsGameInstalled() ? ms_validRpfTypes[iType] : ms_validRpfTypesPreInstall[iType]);

		while(DATAFILEMGR.IsValid(pData))
		{
			strStreamingEngine::GetLoader().CallKeepAliveCallbackIfNecessary();

			// Don't add disabled files.
			if (!pData->m_disabled)
			{
				AddRpfFile(pData, registerFiles);
			}

		pData = DATAFILEMGR.GetNextFile(pData);

			PS3_ONLY(pfDummySync());
		}
	} while (++iType < (CFileMgr::IsGameInstalled() ? count : preInstallCount));
}

void CFileLoader::LoadRpfFiles()
{
	DummyFactory(); // See above for the reason for this

	fileloaderDebugf3("LoadRpfFiles");

	PF_START_STARTUPBAR("Add All RPF files");
	strStreamingEngine::GetLoader().CallKeepAliveCallback();
	AddAllRpfFiles(false);
	PF_START_STARTUPBAR("Streaming InitLevel");
	strStreamingEngine::GetLoader().CallKeepAliveCallback();
	CStreaming::LoadAllRequestedObjects();
	CStreaming::InitLevelWithMapUnloaded();
	strStreamingEngine::GetLoader().CallKeepAliveCallback();
}

//	default.ide and peds.ide used to be loaded before any rpf files
void CFileLoader::LoadWeaponMetaData()
{
	fileloaderDebugf3("LoadWeaponMetaData");

	USE_MEMBUCKET(MEMBUCKET_WORLD);

	// Load IDE_XML files
	//@@: location CFILELOADER_LOADWEAPONMETADATA_GETFIRSTFILE
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::WEAPON_METADATA_FILE);

	while(DATAFILEMGR.IsValid(pData))
	{
		if (!pData->m_disabled)
		{
			LoadObjectTypesXml(pData->m_filename, CDataFileMgr::WEAPON_METADATA_FILE);
		}
			
		pData = DATAFILEMGR.GetNextFile(pData);
	}

}

//	default.ide and peds.ide used to be loaded before any rpf files
void CFileLoader::LoadPedMetaData()
{
	fileloaderDebugf3("LoadPedMetaData");

	USE_MEMBUCKET(MEMBUCKET_WORLD);

    CPedModelInfo::LoadPedPersonalityData();

	CPedModelInfo::LoadPedPerceptionData();

	// Load IDE_XML files
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::PED_METADATA_FILE);

	while(DATAFILEMGR.IsValid(pData))
	{
		if(!pData->m_disabled)
		{
			LoadObjectTypesXml(pData->m_filename, CDataFileMgr::PED_METADATA_FILE);
		}

		pData = DATAFILEMGR.GetNextFile(pData);
	}

}

void CFileLoader::LoadVehicleMetaData()
{
	fileloaderDebugf3("LoadVehicleMetaData");

	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::VEHICLE_METADATA_FILE);

	while(DATAFILEMGR.IsValid(pData))
	{
		if (!pData->m_disabled)
		{
	#if __DEV
			SetCurrentLoadingFile(pData->m_filename);
	#endif

			CVehicleModelInfo::LoadVehicleMetaFile(pData->m_filename, true, fwFactory::GLOBAL);

	#if __DEV
			SetCurrentLoadingFile(NULL);
	#endif
		}

		pData = DATAFILEMGR.GetNextFile(pData);
	}
}

// have to load object data after loading IDE files but before IPL files
void CFileLoader::PreLoadIplPostLoadIde()
{
	fileloaderDebugf3("PreLoadIplPostLoadIde");

	USE_MEMBUCKET(MEMBUCKET_WORLD);

	// PreProcess before all IPLs are loaded
}

void CFileLoader::LoadIplFiles()
{
	PF_START_STARTUPBAR("Begin load IPL files");
	fileloaderDebugf3("LoadIplFiles");

	USE_MEMBUCKET(MEMBUCKET_WORLD);

	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::IPL_FILE);
	atArray<const fiDevice*> m_iplDeviceArray;
	atArray<u32> m_filenameHashArray;

	m_iplDeviceArray.Reserve(100);
	m_filenameHashArray.Reserve(100);

	while(DATAFILEMGR.IsValid(pData))
	{
		char basename[32];
		ASSET.BaseName(basename, sizeof(basename), ASSET.FileName(pData->m_filename));
		const fiDevice* pDevice = fiDevice::GetDevice(pData->m_filename);
		u32 filenameHash = atStringHash(basename);

		// search for file in list with different device. If we find one then don't load this one
		s32 i;
		for(i=0; i<m_iplDeviceArray.GetCount(); i++)
		{
			if(m_iplDeviceArray[i] != pDevice &&
				m_filenameHashArray[i] == filenameHash)
				break;
		}

		// If a version of this file hasn't been loaded already then load it
		if(i == m_iplDeviceArray.GetCount())
		{
			//PF_START_STARTUPBAR_TEMPNAME(pData->m_filename);
			LoadScene(pData->m_filename); // DW - IPLs are loaded here
		}
		else {
			artDisplayf("Ignoring %s as we have loaded a version of this already", pData->m_filename);
		}

		// add to lists
		m_iplDeviceArray.PushAndGrow(pDevice);
		m_filenameHashArray.PushAndGrow(filenameHash);

		pData = DATAFILEMGR.GetNextFile(pData);
	}
}

//	pedbounds.xml loading
void CFileLoader::LoadCapsuleFile()
{
	fileloaderDebugf3("LoadCapsuleFile");

	USE_MEMBUCKET(MEMBUCKET_WORLD);

	// Load  files
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::PED_BOUNDS_FILE);

	while(DATAFILEMGR.IsValid(pData))
	{
		if(!pData->m_disabled)
		{
			CPedCapsuleInfoManager::Init(pData->m_filename);
		}

		pData = DATAFILEMGR.GetNextFile(pData);
	}
}

//	pedhealth.meta loading
void CFileLoader::LoadHealthConfigFile()
{
	fileloaderDebugf3("LoadHealthConfigFile");

	USE_MEMBUCKET(MEMBUCKET_WORLD);

	// Load  files
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::PED_HEALTH_FILE);

	while(DATAFILEMGR.IsValid(pData))
	{
		CHealthConfigInfoManager::Init(pData->m_filename);

		pData = DATAFILEMGR.GetNextFile(pData);
	}
}

#if ENABLE_HORSE
//	mounttune.xml loading
void CFileLoader::LoadMountTuneFile()
{
	fileloaderDebugf3("LoadMountTuneFile");

	USE_MEMBUCKET(MEMBUCKET_WORLD);

	// Load  files
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::MOUNT_TUNE_FILE);

	while(DATAFILEMGR.IsValid(pData))
	{
		HRSSIMTUNEMGR.Load(pData->m_filename);

		pData = DATAFILEMGR.GetNextFile(pData);
	}
}

void CFileLoader::LoadHorseReinsFile()
{
	fileloaderDebugf3("LoadHorseReinsFile");

	USE_MEMBUCKET(MEMBUCKET_WORLD);

	// Load  files
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::HORSE_REINS_FILE);

	while(DATAFILEMGR.IsValid(pData))
	{
		CReins* defaultReins = CReins::GetDefaultReins();
		Assert( defaultReins );
		defaultReins->Load(pData->m_filename);

		pData = DATAFILEMGR.GetNextFile(pData);
	}
}

#endif

void CFileLoader::LoadDoorTypeTuningFile()
{
	fileloaderDebugf3("LoadDoorTypeTuningFile");

	USE_MEMBUCKET(MEMBUCKET_WORLD);

	CDoorTuningManager::InitClass();

	// Load  files
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::DOOR_TUNING_FILE);
	while(DATAFILEMGR.IsValid(pData))
	{
		CDoorTuningManager::GetInstance().Load(pData->m_filename);
		pData = DATAFILEMGR.GetNextFile(pData);
	}

	CDoorTuningManager::GetInstance().FinishedLoading();
}

void CFileLoader::LoadObjectCoverTuningFile()
{
	fileloaderDebugf3("LoadObjectCoverTuningFile");

	USE_MEMBUCKET(MEMBUCKET_WORLD);

	CCoverTuningManager::InitClass();

	// Load  files
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::OBJ_COVER_TUNING_FILE);
	while(DATAFILEMGR.IsValid(pData))
	{
		CCoverTuningManager::GetInstance().Load(pData->m_filename);
		pData = DATAFILEMGR.GetNextFile(pData);
	}
}

//  pedmotiontaskdata.xml loading
void CFileLoader::LoadMotionTaskDataFile()
{
	fileloaderDebugf3("LoadMotionTaskDataFile");

	USE_MEMBUCKET(MEMBUCKET_WORLD);

	// Load files
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::MOTION_TASK_DATA_FILE);

	while(DATAFILEMGR.IsValid(pData))
	{
		CMotionTaskDataManager::Init(pData->m_filename);

		pData = DATAFILEMGR.GetNextFile(pData);
	}
}

//  defaulttaskdata.xml loading
void CFileLoader::LoadDefaultTaskDataFile()
{
	fileloaderDebugf3("LoadDefaultTaskDataFile");

	USE_MEMBUCKET(MEMBUCKET_WORLD);

	// Load files
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::DEFAULT_TASK_DATA_FILE);

	while(DATAFILEMGR.IsValid(pData))
	{
		CDefaultTaskDataManager::Init(pData->m_filename);

		pData = DATAFILEMGR.GetNextFile(pData);
	}
}

//	pedcomponentcloth.xml loading
void CFileLoader::LoadPedComponentClothFile()
{
	fileloaderDebugf3("LoadPedComponentClothFile");

	USE_MEMBUCKET(MEMBUCKET_WORLD);

	// Load  files
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::PED_COMPONENT_CLOTH_FILE);

	while(DATAFILEMGR.IsValid(pData))
	{
		CPedComponentClothManager::Init(pData->m_filename);

		pData = DATAFILEMGR.GetNextFile(pData);
	}
}

//	pedcomponentsets.xml loading
void CFileLoader::LoadPedComponentSetsFile()
{
	fileloaderDebugf3("LoadPedComponentSetsFile");

	USE_MEMBUCKET(MEMBUCKET_WORLD);

	// Load  files
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::PED_COMPONENT_SETS_FILE);

	while(DATAFILEMGR.IsValid(pData))
	{
		if(!pData->m_disabled)
		{
			CPedComponentSetManager::Init(pData->m_filename);
		}

		pData = DATAFILEMGR.GetNextFile(pData);
	}
}

//	pedcomponentsets.xml loading
void CFileLoader::LoadPedIKSettingsFile()
{
	fileloaderDebugf3("LoadPedIKSettingsFile");

	USE_MEMBUCKET(MEMBUCKET_WORLD);

	// Load  files
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::PED_IK_SETTINGS_FILE);

	while(DATAFILEMGR.IsValid(pData))
	{
		CPedIKSettingsInfoManager::Init(pData->m_filename);

		pData = DATAFILEMGR.GetNextFile(pData);
	}
}

//	task data loading
void CFileLoader::LoadTaskDataFile()
{
	fileloaderDebugf3("LoadTaskDataFile");

	USE_MEMBUCKET(MEMBUCKET_WORLD);

	// Load  files
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::PED_TASK_DATA_FILE);

	while(DATAFILEMGR.IsValid(pData))
	{
		CTaskDataInfoManager::Init(pData->m_filename);

		pData = DATAFILEMGR.GetNextFile(pData);
	}
}

//	playerspecialabilities.xml loading
void CFileLoader::LoadSpecialAbilitiesFiles()
{
	fileloaderDebugf3("LoadSpecialAbilitiesFile");

	USE_MEMBUCKET(MEMBUCKET_WORLD);

	// Load  files
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::PED_SPECIAL_ABILITIES_FILE);

	while(DATAFILEMGR.IsValid(pData))
	{
		CPlayerSpecialAbilityManager::Init(pData->m_filename);

		pData = DATAFILEMGR.GetNextFile(pData);
	}
}

void CFileLoader::LoadExplosionInfoFiles()
{
	fileloaderDebugf3("LoadExplosionInfoFile");

	USE_MEMBUCKET(MEMBUCKET_GAMEPLAY);

	// Load files
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::EXPLOSION_INFO_FILE);

	while (DATAFILEMGR.IsValid(pData))
	{
		CExplosionInfoManager::Init(pData->m_filename);
		pData = DATAFILEMGR.GetNextFile(pData);
	}
}

void CFileLoader::LoadBrawlingStyleFiles()
{
	fileloaderDebugf3("LoadBrawlingStyleFile");

	USE_MEMBUCKET(MEMBUCKET_GAMEPLAY);

	// Load  files
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::PED_BRAWLING_STYLE_FILE);

	while(DATAFILEMGR.IsValid(pData))
	{
		CBrawlingStyleManager::Init(pData->m_filename);
		pData = DATAFILEMGR.GetNextFile(pData);
	}
}

void CFileLoader::LoadDeformableObjectFile()
{
	fileloaderDebugf3("LoadDeformableObjectsFile");

	USE_MEMBUCKET(MEMBUCKET_WORLD);

	// Load file.
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::DEFORMABLE_OBJECTS_FILE);

	while(DATAFILEMGR.IsValid(pData))
	{
		CDeformableObjectManager::Load(pData->m_filename);

		pData = DATAFILEMGR.GetNextFile(pData);
	}
}

void CFileLoader::LoadTunableObjectFile()
{
	fileloaderDebugf3("LoadTunableObjectsFile");

	USE_MEMBUCKET(MEMBUCKET_WORLD);

	// Load file.
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::TUNABLE_OBJECTS_FILE);

	while(DATAFILEMGR.IsValid(pData))
	{
		CTunableObjectManager::Load(pData->m_filename);

		pData = DATAFILEMGR.GetNextFile(pData);
	}
}

void CFileLoader::LoadNavCapabilitiesDataFile()
{
	fileloaderDebugf3("LoadNavCapabilitiesDataFile");

	USE_MEMBUCKET(MEMBUCKET_WORLD);

	// Load files
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::PED_NAV_CAPABILITES_FILE);

	while(DATAFILEMGR.IsValid(pData))
	{
		CPedNavCapabilityInfoManager::Init(pData->m_filename);

		pData = DATAFILEMGR.GetNextFile(pData);
	}
}

void CFileLoader::LoadScenarioPointFiles()
{
	fileloaderDebugf3("LoadScenarioPointFiles");

	USE_MEMBUCKET(MEMBUCKET_WORLD);

	SCENARIOPOINTMGR.Load();
}

void CFileLoader::LoadItypFiles()
{
	// load permanent files
	for(s32 index=0; index<g_MapTypesStore.GetSize(); index++)
	{
		fwMapTypesDef* pDef = g_MapTypesStore.GetSlot(strLocalIndex(index));
		if (pDef && pDef->GetIsPermanent()/* && !pDef->GetIsDelayLoading()*/)
		{
			g_MapTypesStore.StreamingRequest(strLocalIndex(index), STRFLAG_PRIORITY_LOAD | STRFLAG_FORCE_LOAD | STRFLAG_DONTDELETE);
		}
	}

	strStreamingEngine::GetLoader().LoadAllRequestedObjects();

	for(s32 index=0; index<g_MapTypesStore.GetSize(); index++)
	{
		fwMapTypesDef* pDef = g_MapTypesStore.GetSlot(strLocalIndex(index));
		if (pDef && pDef->GetIsPermanent()/* && !pDef->GetIsDelayLoading()*/)
		{
			if (!g_MapTypesStore.HasObjectLoaded(strLocalIndex(index))){
				// fail safe - if can't load all in one go, then bring the unloaded ones in one at a time
				g_MapTypesStore.StreamingRequest(strLocalIndex(index), STRFLAG_PRIORITY_LOAD | STRFLAG_FORCE_LOAD | STRFLAG_DONTDELETE);
				strStreamingEngine::GetLoader().LoadAllRequestedObjects();

				Assertf(g_MapTypesStore.HasObjectLoaded(strLocalIndex(index)), "file %s was requested (twice) but did not load\n",g_MapTypesStore.GetName(strLocalIndex(index)));
			}
		}
	}
}

void CFileLoader::AddDLCItypeFile(const char* fileName)
{
	char achBaseName[RAGE_MAX_PATH] = { 0 };
	ASSET.BaseName(achBaseName, sizeof(achBaseName), ASSET.FileName(fileName));
	strLocalIndex index = g_MapTypesStore.FindSlotFromHashKey(atHashString(achBaseName));
	if (Verifyf(index.IsValid(),"DLC_ITYP_REQUEST file :%s not found", fileName))
	{
	#if !__FINAL && 0 //disabled for now but may be useful
		char tempStr[256] = { 0 };
		const char *refCntString = g_MapTypesStore.GetRefCountString(index, tempStr, sizeof(tempStr));

		Displayf("TOMW: AddDLCItypeFile - %s - %i - %s", g_MapTypesStore.GetName(index), g_MapTypesStore.GetNumRefs(index), refCntString);
	#endif

		g_MapTypesStore.StreamingRequest(index, STRFLAG_PRIORITY_LOAD | STRFLAG_FORCE_LOAD | STRFLAG_DONTDELETE);
		g_MapTypesStore.GetSlot(index)->SetIsPermanentDLC(true);
		g_MapTypesStore.GetSlot(index)->SetIsDependency(true);
		ms_requestDLCItypes = true;
	}
}

void CFileLoader::RemoveDLCItypeFile(const char* fileName)
{
	char achBaseName[RAGE_MAX_PATH] = { 0 };
	ASSET.BaseName(achBaseName, sizeof(achBaseName), ASSET.FileName(fileName));
	
	strLocalIndex index = g_MapTypesStore.FindSlotFromHashKey(atHashString(achBaseName));
	if (index.IsValid())
	{
	#if !__FINAL && 0 //disabled for now but may be useful
		char tempStr[256] = { 0 };
		const char *refCntString = g_MapTypesStore.GetRefCountString(index, tempStr, sizeof(tempStr));

		Displayf("TOMW: RemoveDLCItypeFile - %s - %i - %s", g_MapTypesStore.GetName(index), g_MapTypesStore.GetNumRefs(index), refCntString);
	#endif

		g_MapTypesStore.ClearRequiredFlag(index.Get(), STRFLAG_DONTDELETE);
		g_MapTypesStore.GetSlot(index)->SetIsPermanentDLC(false);

	#if !__FINAL // Temporary non-final output for over-reffed archetypes in a departing ITYP
		if (g_MapTypesStore.GetNumRefs(index) > 1) {
			Displayf("Too many refs on ITYP '%s'", g_MapTypesStore.GetName(index));

			fwArchetypeManager::ForAllArchetypesInFile(index.Get(), [](fwArchetype *pArchetype) {
				Displayf("Archetype %s has %u refs still", pArchetype->GetModelName(), pArchetype->GetNumRefs());				
			});
		}
	#endif

		strStreamingEngine::GetInfo().RemoveObjectAndDependents(g_MapTypesStore.GetStreamingIndex(index), HMT_FLUSH);
	}
}

void CFileLoader::RequestDLCItypFiles()
{
	if (ms_requestDLCItypes)
	{
		strStreamingEngine::GetLoader().LoadAllRequestedObjects();
		ms_requestDLCItypes = false;
	}
}

void CFileLoader::RequestPermanentItypFiles()
{
	fileloaderDebugf3("RequestPermanentItypFiles");

	USE_MEMBUCKET(MEMBUCKET_WORLD);

	PF_START_STARTUPBAR("Request all files");

	{
		// Load IDE files
		const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::PERMANENT_ITYP_FILE);
		ms_permItyp.Reset();

		while(DATAFILEMGR.IsValid(pData))
		{
			if ( !pData->m_filename )
				continue;

			char achBaseName[RAGE_MAX_PATH];
			ASSET.BaseName(achBaseName, 32, ASSET.FileName(pData->m_filename));

			strLocalIndex index = strLocalIndex(g_MapTypesStore.FindSlotFromHashKey(atHashString(achBaseName)));
			if (index.Get() >= 0)			
			{
				ms_permItyp.PushAndGrow(index.Get());
				g_MapTypesStore.GetSlot(index)->SetIsPermanent(true);
				g_MapTypesStore.StreamingRequest(index, STRFLAG_PRIORITY_LOAD | STRFLAG_FORCE_LOAD | STRFLAG_DONTDELETE);
			}

			pData = DATAFILEMGR.GetNextFile(pData);
		}

		strStreamingEngine::GetLoader().LoadRequestedObjects();
	}
}

//	default.ide and peds.ide used to be loaded before any rpf files
void CFileLoader::LoadPermanentItypFiles()
{
	fileloaderDebugf3("LoadPermanentItypFiles");

	USE_MEMBUCKET(MEMBUCKET_WORLD);

	PF_PUSH_STARTUPBAR("Wait for files to stream");
	{
		for (s32 i = 0; i < ms_permItyp.GetCount(); ++i)
		{
			if (g_MapTypesStore.GetStreamingInfo(strLocalIndex(ms_permItyp[i]))->GetStatus() != STRINFO_LOADED)
			{
				strStreamingEngine::GetLoader().LoadAllRequestedObjects();
				break;
			}
		}
		ms_permItyp.Reset();
	}

	PF_POP_STARTUPBAR();

	PF_START_STARTUPBAR("Checking files");

#if __DEV
	//verify that all type files loaded...
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::PERMANENT_ITYP_FILE);

	while(DATAFILEMGR.IsValid(pData))
	{
		if ( !pData->m_filename )
			continue;

		char achBaseName[RAGE_MAX_PATH];
		ASSET.BaseName(achBaseName, 32, ASSET.FileName(pData->m_filename));

		strLocalIndex index = strLocalIndex(g_MapTypesStore.FindSlotFromHashKey(atHashString(achBaseName)));
		if (index.Get() >= 0 && (g_MapTypesStore.GetSlot(index)->GetIsPermanent()))			// only load global .ityp files
		{
			if (!g_MapTypesStore.HasObjectLoaded(index)){
				// fail safe - if can't load all in one go, then bring the unloaded ones in one at a time
				//PF_START_STARTUPBAR_TEMPNAME(achBaseName);
				g_MapTypesStore.StreamingRequest(index, STRFLAG_PRIORITY_LOAD | STRFLAG_FORCE_LOAD | STRFLAG_DONTDELETE);
				strStreamingEngine::GetLoader().LoadAllRequestedObjects();

				Assertf(g_MapTypesStore.HasObjectLoaded(index), "file %s was requested (twice) but did not load\n",pData->m_filename);
			}
		}

		pData = DATAFILEMGR.GetNextFile(pData);
	}

	Displayf("*** total archetypes :%" SIZETFMT "u\n", fwArchetypeManager::GetPool().GetCount());


#endif //__DEV
}

void CFileLoader::MarkArchetypesWhichCannotBeDummy()
{
	// mark specific skinned animated objects to never be dummy objects (because they won't render properly as dummy objects)
	// this is just for pump jacks and windmills, so a very special case really

	static const u32 adwNeverDummyModels[] =
	{
		//////////////////////////////////////////////////////////////////////////
		// stuff that just doesn't render as a dummy due to being animated objects using skinning etc
		0x28cadfdb,		//P_Oil_Pjack_01_FRG_S
		0x5c80b38,		//P_Oil_Pjack_02_FRG_S
		0x5f4a5f22,		//P_Oil_Pjack_03_FRG_S
		0x21dfb7cf,		//p_oil_pjack_01_s
		0x708d300f,		//p_oil_pjack_02_s
		0x134f68e5,		//p_oil_pjack_03_s
		0x745f3383,		//prop_windmill_01
		0x427a3150,		//prop_storagetank_02
		0x8973a868,		//prop_air_bigradar
		0x3b9e6ecc,		//prop_rural_windmill

		//////////////////////////////////////////////////////////////////////////
		// rail junctions - so they can lower when trains are passing despite being some distance away
		0xd639398a,		//prop_railway_barrier_01
		0xa9755fff,		//prop_railway_barrier_02
		0xc8dad835,		//prop_traffic_rail_1c

		//////////////////////////////////////////////////////////////////////////
		// large, low-LOD buoyancy objects which sit at inappropriate heights when in the distance
		0xbf8918de,		//prop_jetski_ramp_01
		0x73e498db,		//prop_dock_bouy_3
		0xdea4ebf4,		//prop_byard_float_01
		0x4653780,		//prop_byard_float_02

		//////////////////////////////////////////////////////////////////////////
		// large static props that really should have been baked into the static collisions
		0xbc4649e5,		//prop_rub_railwreck_2
		0xbe862050,		//prop_portacabin01
		0x2e042597,		//prop_container_01c
		0x12bdebf2,		//prop_container_03a
		0x69381681,		//prop_container_05a
		0xda76fe6e,		//prop_container_01a

		//////////////////////////////////////////////////////////////////////////
		// special case stuff
		0x8da1c0e,		//prop_juicestand - a ball you can roll across the map
		0xeb2e00e0,		//prop_game_clock_02  - clock in the window of the tattoo parlor for family reunion
		0xa49287f8,		//p_tumbler_cs2_s tumbler is Michael's house
		0x1d18abd6,		//p_whiskey_bottle_s whiskey bottle
		0xe0545565,		//prop_tanktrailer_01a
		0x17236aa7,		//prop_skip_08a

		//////////////////////////////////////////////////////////////////////////
		// ambient vehicles passing through gates
		0xd8809a3,		//prop_fnclink_02gate6_r
		0x5afd248c,		//prop_fnclink_02gate6_l
		0x87024c72,		//prop_arm_gate_l

		// mpApartments - Yacht
		0x4fcad2e0,		//apa_mp_apa_yacht

		// Arena Wars - pump jack
		0x6b676d83,		//xs_prop_arena_oil_Jack_02a

		0
	};

	const u32* pCurrent = &adwNeverDummyModels[0];
	while (*pCurrent)
	{
		u32 index;
		CBaseModelInfo* pModelInfo = (CBaseModelInfo*) fwArchetypeManager::GetArchetypeFromHashKey(*pCurrent, &index);
		if (pModelInfo)
		{
			Displayf("Setting model %s to never dummy", pModelInfo->GetModelName());
			pModelInfo->SetNeverDummyFlag(true);
		}
		pCurrent++;
	}
}

void CFileLoader::MarkArchetypesWhichIntentionallyLeakObjects()
{
	// usually map objects get removed when the corresponding map data is streamed out.
	// however some frags are fun to play with and push around the map (e.g. a big orange ball that can be pushed about the world etc)
	// so we mark them as being permitted to leak
	//
	// only allowed for permanent archetypes
	static const u32 adwAllowedToLeakModels[] =
	{
		0x8da1c0e,		//prop_juicestand - a ball you can roll across the map

		0
	};

	const u32* pCurrent = &adwAllowedToLeakModels[0];
	while (*pCurrent)
	{
		u32 index;
		CBaseModelInfo* pModelInfo = (CBaseModelInfo*) fwArchetypeManager::GetArchetypeFromHashKey(*pCurrent, &index);
		if (pModelInfo)
		{
			Displayf("Setting model %s to allow intentional object leaking", pModelInfo->GetModelName());
			pModelInfo->SetLeakObjectsFlag(true);
		}
		pCurrent++;
	}
}

bool CFileLoader::CacheLoadStart(const char* pDatFilename)
{
	atMap<s32, bool> cacheLoaderArchives;

	PF_PUSH_STARTUPBAR("Cache Load Init");
	fileloaderDisplayf("CacheLoadStart");

	USE_MEMBUCKET(MEMBUCKET_PHYSICS);

	WIN32PC_ONLY(CApp::CheckExit());

	// CacheLoader attempt (bracketed to clearly define the specific code...
	strCacheLoader::Init(pDatFilename, NULL, SCL_DEFAULT, cacheLoaderArchives);
// 	if(CExtraContentManagerSingleton::IsInstantiated() && EXTRACONTENT.GetEnabledContentMask() != 0)
// 		strCacheLoader::SetReadOnly();
	PF_START_STARTUPBAR("Cache Load Load");
	bool bCacheLoaded = strCacheLoader::Load();
	PF_START_STARTUPBAR("Update changed static bounds");

	WIN32PC_ONLY(CApp::CheckExit());

	g_StaticBoundsStore.UpdateChangedStaticBounds();

	WIN32PC_ONLY(CApp::CheckExit());

	PF_POP_STARTUPBAR();

	return bCacheLoaded;
}  // end of CacheLoader attempt

// Cacheloader must save AFTER the IPLs have loaded.
void CFileLoader::CacheLoadEnd()
{
	fileloaderDisplayf("CacheLoadEnd");

	PF_PUSH_STARTUPBAR("Fix up entries after reg");
	INSTANCE_STORE.FixupAllEntriesAfterRegistration(true);
	CManagedImapGroupMgr::MarkManagedImapFiles();
	PF_POP_STARTUPBAR();

	PF_START_STARTUPBAR("Register IPL Cullbox containers");
	CIplCullBox::RegisterContainers();

	PF_START_STARTUPBAR("Cache save");
	strCacheLoader::Save();

	PF_START_STARTUPBAR("Cache shutdown");
	strCacheLoader::Shutdown();


	{
		////////////////////////////////////////////////////////////////////////
		// GTAV map only - mark important emissive IPLs to stay in memory - late in the day GTAV hack

		const s32 downtownEmissiveIndex = g_MapDataStore.FindSlot("dt1_emissive").Get();	// downtown skyscrapers!
		if (downtownEmissiveIndex>0)
		{
			CEntity::ms_DowntownIplIndex = (u32) downtownEmissiveIndex;
		}
		const s32 vinewoodEmissiveIndex = g_MapDataStore.FindSlot("ch2_03").Get();		// vinewood sign!
		if (vinewoodEmissiveIndex>0)
		{
			CEntity::ms_VinewoodIplIndex = (u32) vinewoodEmissiveIndex;
		}

		//////////////////////////////////////////////////////////////////////////
	}

}

//Post process after IPL files are loaded
void CFileLoader::PostProcessIplLoad()
{
	fileloaderDebugf3("PostProcessIplLoad");

	PF_START_STARTUPBAR("static bounds postRegistration");
	g_StaticBoundsStore.PostRegistration();

	PF_START_STARTUPBAR("INSTANCE STORE postProcess");
	INSTANCE_STORE.GetBoxStreamer().PostProcess();

	PF_START_STARTUPBAR("static bounds postProcess");
	g_StaticBoundsStore.GetBoxStreamer().PostProcess();

	PF_START_STARTUPBAR("metadata postProcess");
	g_fwMetaDataStore.GetBoxStreamer().PostProcess();

	PF_START_STARTUPBAR("Remove All");
	g_StaticBoundsStore.RemoveAll();

	PF_START_STARTUPBAR("IPL cull box load");
	CIplCullBox::Load();
	g_PlayerSwitch.LoadSettings();
}


//
// name:		CFileLoader::LoadLevelInstanceInfo
// description:	Load IPL files after all IDE files have been loaded
void CFileLoader::LoadLevelInstanceInfo(const char* pDatFilename)
{
	strStreamingEngine::GetLoader().CallKeepAliveCallback();

	PF_PUSH_STARTUPBAR(pDatFilename);
	fileloaderDebugf3("LoadLevelInstanceInfo %s", pDatFilename);
	
	PROFLOAD_ONLY(pfDummySync());
    PF_START_STARTUPBAR("Reset Level Instance Info");
	PROFLOAD_ONLY(pfDummySync());
    ResetLevelInstanceInfo();

	PF_PUSH_STARTUPBAR("Load RPF files");

	PROFLOAD_ONLY(pfDummySync());

	CVisualEffects::InitDefinitionsPreRpfs();

	WIN32PC_ONLY(CApp::CheckExit());

	LoadRpfFiles();

	WIN32PC_ONLY(CApp::CheckExit());

	CVisualEffects::InitDefinitionsPostRpfs();

    RequestPermanentItypFiles();

	PROFLOAD_ONLY(pfDummySync());
	PROFLOAD_ONLY(snStopCapture());

	PROFLOAD_ONLY(pfDummySync());
	PROFLOAD_ONLY(pfDummySync());

	PF_POP_STARTUPBAR();

	CScaleformMgr::LoadPreallocationInfo(SCALEFORM_PREALLOC_XML_FILENAME);

#if !__FINAL
	CStreaming::BeginRecordingStartupSequence();
#endif // !__FINAL
	CStreaming::ReplayStartupSequenceRecording();

	// We need to load capsule information prior to loading IDE files as peds reference capsule information
	PF_START_STARTUPBAR("Load Capsule files");
	LoadCapsuleFile();	

	PROFLOAD_ONLY(pfDummySync());

	//LOAD EXPLOSION DATA
	PF_START_STARTUPBAR("Load Explosion Info File");
	LoadExplosionInfoFiles();

	PF_START_STARTUPBAR("Load Ped Health files");
	LoadHealthConfigFile();	

	// We need to load component set information prior to loading IDE files as peds reference this
	PF_START_STARTUPBAR("Load Ped Component Cloth file");
	LoadPedComponentClothFile();
	//@@: range FILELOADER_LOADLEVELINSTANCEINFO_LOAD_PED_STUFF {

	// We need to load component set information prior to loading IDE files as peds reference this
	PF_START_STARTUPBAR("Load Ped Component Set files");
	LoadPedComponentSetsFile();

	// We need to load component set information prior to loading IDE files as peds reference this
	PF_START_STARTUPBAR("Load Ped IK Settings files");
	LoadPedIKSettingsFile();

	//@@: } FILELOADER_LOADLEVELINSTANCEINFO_LOAD_PED_STUFF

	// We need to load component set information prior to loading IDE files as peds reference this
	PF_START_STARTUPBAR("Load Task Data files");
	LoadTaskDataFile();

	// We need to load special ability information prior to loading IDE files as peds reference them
	PF_START_STARTUPBAR("Load Special Abilities file");
	LoadSpecialAbilitiesFiles();

	// We need to load brawling style information prior to loading IDE files as peds reference them
	PF_START_STARTUPBAR("Load Brawling Style file");
	LoadBrawlingStyleFiles();

	// We need to load the motion task data info prior to loading IDE files as peds reference them
	PF_START_STARTUPBAR("Load Motion Task Data files");
	LoadMotionTaskDataFile();

	PF_START_STARTUPBAR("Load Default Task Data files");
	LoadDefaultTaskDataFile();

	PF_START_STARTUPBAR("Load Nav Capabilities Data files");
	LoadNavCapabilitiesDataFile();

#if ENABLE_HORSE
	PF_START_STARTUPBAR("Load Mount tuning file");
	LoadMountTuneFile();

	PF_START_STARTUPBAR("Load Horse Reins file");
	LoadHorseReinsFile();
#endif

	PF_START_STARTUPBAR("Load Door Type Tuning file");
	LoadDoorTypeTuningFile();

	PF_START_STARTUPBAR("Load Object Cover Tuning file");
	LoadObjectCoverTuningFile();

	PF_PUSH_STARTUPBAR("Load .ityp files");

	CMapFileMgr::GetInstance().LoadGlobalTxdParents();

	if (!PARAM_PartialDynArch.Get()){
		LoadPermanentItypFiles();
		CMapFileMgr::GetInstance().SetMapFileDependencies();		// need to know permanent .ityps
		g_MapTypesStore.VerifyItypFiles();							// check that all .ityp files have been processed
		MarkArchetypesWhichCannotBeDummy();
		MarkArchetypesWhichIntentionallyLeakObjects();
	} else{		
		LoadItypFiles();
	}
	
	PF_POP_STARTUPBAR();

	PF_PUSH_STARTUPBAR("Load Scenario Point Files");
	LoadScenarioPointFiles();
	PF_POP_STARTUPBAR();

	PF_START_STARTUPBAR("Load Ped Metadata");
	LoadPedMetaData();

	PF_START_STARTUPBAR("Load Weapon Metadata");
	LoadWeaponMetaData();

	PF_START_STARTUPBAR("Load Vehicle Metadata");
	LoadVehicleMetaData();
	// Need to Initialize the water fog texture slots after loading RPF Files
	Water::InitFogTextureTxdSlots();

	fwArchetypeManager::LockPermanentArchetypes();

//		snStartCapture(); // Necessary if driving Tuner from
	// the command line (ignored otherwise)

//		DummySync();
	

//	 	snStopCapture();  // Signal to host app to finalize
	// 	// 	// the tuning session
	// 	// 
// 	 	DummySync();
// 	 	DummySync();

	PF_START_STARTUPBAR("PreLoadIplPostLoadIde");
	PreLoadIplPostLoadIde();

	WIN32PC_ONLY(CApp::CheckExit());

	CacheLoadStart(pDatFilename);

	WIN32PC_ONLY(CApp::CheckExit());

	PF_START_STARTUPBAR("Time cycle modifier files");
	g_timeCycle.LoadModifierFiles();

	PF_PUSH_STARTUPBAR("Load IPL files");

	LoadIplFiles();

	PF_POP_STARTUPBAR();

	PF_START_STARTUPBAR("Cache load end");

	WIN32PC_ONLY(CApp::CheckExit());

	CacheLoadEnd();

	WIN32PC_ONLY(CApp::CheckExit());

#if __ASSERT
	CDownloadableTextureManager::CheckTXDStoreCapacity();
#endif

	CInteriorProxy::Sort();
	CInteriorProxy::AddAllDummyBounds(0);

	PF_START_STARTUPBAR("Post process IPL Load");
	PostProcessIplLoad();

	DATAFILEMGR.RemoveMapDataFiles();

	PF_START_STARTUPBAR("Load All Requested");
	CStreaming::LoadAllRequestedObjects();

	PF_START_STARTUPBAR("Modelinfo Post Load");
	CModelInfo::PostLoad();

	PF_START_STARTUPBAR("Load deformable objects file");
	LoadDeformableObjectFile();

	PF_START_STARTUPBAR("Load tunable objects file");
	LoadTunableObjectFile();

	CInstancePriority::Init();

	CMapFileMgr::GetInstance().Cleanup();		// cleanup up all our file meta data now loading is done

	PF_POP_STARTUPBAR();
}

void CFileLoader::ResetLevelInstanceInfo(void)
{
    // clear lod trees before loading etc
	g_LodMgr.Reset();
    g_ContainerLodMap.Reset();
	g_MapChangeMgr.Reset();
    CIplCullBox::Reset();

    CBrawlingStyleManager::Shutdown();
    CHealthConfigInfoManager::Shutdown();
	CDefaultTaskDataManager::Shutdown();
    CMotionTaskDataManager::Shutdown();
    CPedCapsuleInfoManager::Shutdown();
    CPedComponentSetManager::Shutdown();
	CPedComponentClothManager::Shutdown();
    CPedIKSettingsInfoManager::Shutdown();
    CPedNavCapabilityInfoManager::Shutdown();
	CPedPerceptionInfoManager::Shutdown();
    CPlayerSpecialAbilityManager::Shutdown();
    CSlownessZoneManager::Shutdown();
    CTaskDataInfoManager::Shutdown();
	CDoorTuningManager::ShutdownClass();
	CExplosionInfoManager::Shutdown();
	CCoverTuningManager::ShutdownClass();
}

#if __BANK

void CFileLoader::InitWidgets()
{
	if (PARAM_fileloader_tty_windows.Get())
	{
		BANKMGR.CreateOutputWindow("fileloader", "NamedColor:Green"); 
		BANKMGR.CreateOutputWindow("fileloader_cacheloader", "NamedColor:Green");
		BANKMGR.CreateOutputWindow("fileloader_iplstore", "NamedColor:Green");
	}

	if(PARAM_fileMountWidgets.Get())
	{
		if( bkBank *pBank = &BANKMGR.CreateBank("Data File Mount", 0, 0, false) )
		{
			pBank->AddButton("Dump valid interfaces list", &CDataFileMount::DebugOutputTable);
		}
	}
}

#endif

//
//        name: CFileLoader::LoadAudioGroup
// description: Load an audio group
//
void CFileLoader::LoadAudioGroup(const char* pLine)
{	
	char pedmodel[STORE_NAME_LENGTH];
	char groupname[STORE_NAME_LENGTH];
	s32 headcomponent;

	if (sscanf(pLine, "%s %s %d", pedmodel, groupname, &headcomponent ) != 3)
	{
		Assert(0);
	}

	u32 iModelIndex = CModelInfo::GetModelIdFromName(pedmodel).GetModelIndex();

	Assert(CModelInfo::IsValidModelInfo(iModelIndex));
	if( !CModelInfo::IsValidModelInfo(iModelIndex) )
		return;

	CPedModelInfo* pPedModelInfo = static_cast<CPedModelInfo*>(CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(iModelIndex))));

	Assert(pPedModelInfo);
	if( pPedModelInfo == NULL )
		return;

	pPedModelInfo->AddAudioGroupMap(headcomponent, atStringHash(groupname));
}

//
// name:		CFileLoader::Load2dEffect
// description:
void CFileLoader::Load2dEffect(const char* pLine)
{
	char name[STORE_NAME_LENGTH];
	s32 type;
	float x,y,z;

	float qx, qy, qz, qw; // all data currently gets exported with rotation, but in a lot of cases it gets ignored
	::sscanf(pLine, "%s %f %f %f %d %f %f %f %f", &name[0], &x, &y, &z, &type, &qx, &qy, &qz, &qw);

	C2dEffect* pEffect = NULL;

	switch(type)
	{
		case(ET_LIGHT):
			Assertf(0, "Unrecognised 2d effect %d", type);
			break;

		case(ET_LIGHT_SHAFT):
		{
			Assertf(0, "Unrecognised 2d effect %d", type);
		}
		break;

		case(ET_PARTICLE):
		{
			Assertf(0, "Unrecognised 2d effect %d", type);
		}
		break;

		case(ET_EXPLOSION):
		{
			Assertf(0, "Unrecognised 2d effect %d", type);
		}
		break;

		case ET_PROCEDURALOBJECTS :
		{
			Assertf(0, "Unrecognised 2d effect %d", type);
			break;
		}

		case ET_COORDSCRIPT :
		{
			// THIS IS STILL AN ACTIVE CODE PATH - FOR FUCKS SAKE!
			{
				pEffect = CModelInfo::GetWorldPointStore().CreateItem(fwFactory::GLOBAL);
				CWorldPointAttr* wp = pEffect->GetWorldPoint();

				char FullLine[256];
				char seperators[] = " ,\t\n";

				Assertf(strlen(pLine) < 256, "%s:CFileLoader::Load2dEffect - line for coord script is too long to copy", name);
				strncpy(FullLine, pLine, 256);	//	make a copy of this string as strtok will destroy it

				int token_number = 0;
				char *pToken = NULL;
				bool bReadFirstToken = false;
				int num_of_extra_coords = 0;

				while (token_number <= 10)
				{
					if (bReadFirstToken == false)
					{
						bReadFirstToken = true;
						pToken = strtok( FullLine, seperators );
						Assertf(pToken, "%s:CFileLoader::Load2dEffect - failed to load first token from script coord line for this model", name);
					}
					else
					{
						pToken = strtok( NULL, seperators );
						Assertf(pToken || token_number >= 11, "%s:CFileLoader::Load2dEffect - failed to load token from script coord line for this model", name);
					}

					if (pToken)	// This 'if' can be removed when all have been re-exported with 15 params
					{
						switch (token_number)
						{
						case 0 :	//	model name
						case 1 :	//	x offset
						case 2 :	//	y offset
						case 3 :	//	z offset
						case 4 :	//	2d effect type
						case 5 :	//	quaternion x
						case 6 :	//	quaternion y
						case 7 :	//	quaternion z
						case 8 :	//	quaternion w
							break;

						case 9 :	//	script name
							wp->m_scriptName.SetFromString( pToken );
							break;

						case 10 :	//	number of extra coordinates
							num_of_extra_coords = atoi(pToken);
							wp->num_of_extra_coords = (s8) num_of_extra_coords;
							break;

						}
					}
					token_number++;
				}

				// Calculate the first orientation
				Matrix34 matRotation;
				matRotation.FromQuaternion(Quaternion(qx, qy, qz, qw));
				float fHeading = rage::Atan2f(-matRotation.b.x, matRotation.b.y);
				wp->extraCoordOrientations[0] = RtoD*fHeading;

				Assertf(num_of_extra_coords <= MAX_NUMBER_OF_EXTRA_WORLD_POINT_COORDS, "%s:CFileLoader::Load2dEffect - too many extra coords for a script coord attached to this model", name);
				for (int current_extra_coord = 0; current_extra_coord < num_of_extra_coords; current_extra_coord++)
				{
					pToken = strtok( NULL, seperators );
					Assertf(pToken, "%s:CFileLoader::Load2dEffect - failed to load token from script coord line for this model (extra coord x)", name);
					wp->extraCoords[current_extra_coord].x = static_cast<float> (atof(pToken));

					pToken = strtok( NULL, seperators );
					Assertf(pToken, "%s:CFileLoader::Load2fX, fY, fZ, dEffect - failed to load token from script coord line for this model (extra coord y)", name);
					wp->extraCoords[current_extra_coord].y = static_cast<float> (atof(pToken));

					pToken = strtok( NULL, seperators );
					Assertf(pToken, "%s:CFileLoader::Load2dEffect - failed to load token from script coord line for this model (extra coord z)", name);
					wp->extraCoords[current_extra_coord].z = static_cast<float> (atof(pToken));

					pToken = strtok( NULL, seperators );
					Assertf(pToken, "%s:CFileLoader::Load2dEffect - failed to load token from script coord line for this model (orientation)", name);
					wp->extraCoordOrientations[current_extra_coord+1] = static_cast<float> (atof(pToken));
				}

				pToken = strtok( NULL, seperators );
				Assertf(pToken == NULL, "%s:CFileLoader::Load2dEffect - unexpected token at end of script coord line for this model", name);
			}
		}
		break;

		case ET_LADDER :
		{
			Assertf(0, "Unrecognised 2d effect %d", type);

			break;
		}

		case ET_AUDIOEFFECT:
		{	
			Assertf(0, "Unrecognised 2d effect %d", type);
		}

		break;

		case ET_SCROLLBAR:
			{
				Assertf(0, "Unrecognised 2d effect %d", type);
			}
			break;

		case(ET_BUOYANCY):
			{
				Assertf(0, "Unrecognised 2d effect %d", type);
			}
			break;

		default:
			//temporarily commented out until we get rid of
			//the old 2dfx GRS
			Assertf(0, "Unrecognised 2d effect %d", type);

	}// end of switch(type)...

	{
		if(pEffect)
		{
			pEffect->SetPos(Vector3(x,y,z));

			C2dEffect** ppEffect = NULL;

			// The ipl (ambient.ipl, scenarios.ipl, worldlights.ipl, worldparticles.ipl contain 2d effects that are not connected to objects
			{
				ppEffect = &CModelInfo::GetWorld2dEffectArray().Append();
				*ppEffect = pEffect;
			}

			Assert(ppEffect);
		}
	}
//	g_TxdStore.PopCurrentTxd();
}

bool CFileLoader::LoadObjectTypesXml(const char* pLevelName, CDataFileMgr::DataFileType dataType, bool permanent, s32 mapTypeDefIndex /* = fwFactory::GLOBAL */)
{
	bool success = false;
#if __DEV
	SetCurrentLoadingFile(pLevelName);
#endif

	switch(dataType){
		case(CDataFileMgr::PED_METADATA_FILE) :
		{
			success = CPedModelInfo::LoadPedMetaFile(pLevelName, permanent, mapTypeDefIndex);
			break;
		}
		case(CDataFileMgr::WEAPON_METADATA_FILE) :
		{
			fwPsoStoreLoadInstance instance;
			fwPsoStoreLoader loader = fwPsoStoreLoader::MakeSimpleFileLoader<CWeaponModelInfo::InitDataList>();

			loader.Load(pLevelName, instance);

			struct CWeaponModelInfo::InitDataList * pInitDataList = reinterpret_cast<CWeaponModelInfo::InitDataList*>(instance.GetInstance());
			if (instance.IsLoaded() && pInitDataList)
			{
				for(int i=0;i<pInitDataList->m_InitDatas.GetCount();i++)
				{
					CWeaponModelInfo * pModelInfo = CModelInfo::AddWeaponModel(pInitDataList->m_InitDatas[i].m_modelName.c_str(), permanent, mapTypeDefIndex);
					pModelInfo->Init(pInitDataList->m_InitDatas[i]);
					pModelInfo->SetIsTypeObject(true);

					if (!permanent)
					{
						CModelInfo::PostLoad(pModelInfo);
					}
				}
				success = true;
			}
			loader.Unload(instance);

			break;
		}
		default:
				fileloaderAssertf(false,"unrecognised xml data type");
	}


#if __DEV
	SetCurrentLoadingFile(NULL);
#endif
	return success;
}


//
//        name: CFileLoader::LoadScene
// description: Load Object instance information from IPL file
//          in: pLevelName = pointer to level name string e.g "INDUSTRIAL"
//
void CFileLoader::LoadScene(const char* pFilename)
{
	fileloaderDebugf3("LoadScene");

	enum IPLLoadingStatus {
		LOADING_NOTHING,
		LOADING_IGNORE,
		LOADING_INSTANCES,
		LOADING_MULTIBUILDINGS,
		LOADING_POPZONES,
		LOADING_MAPAREAS,
		LOADING_CULLZONES,
		LOADING_OCCLUSION,
		LOADING_GARAGE,
		LOADING_ENTRYEXIT,
		LOADING_PICKUPS,
		LOADING_CARGENERATORS,
		LOADING_STUNTJUMPS,
		LOADING_TIMECYCLEMODS,
		LOADING_AUDIOZONES,
		LOADING_BLOCKS,
		LOADING_MLOAPPEND,
		LOADING_2DEFFECTS,
		LOADING_STREAM_BOXES,
		LOADING_SLOWNESS_ZONES
	};

	// Displayf("Creating objects from %s...", pFilename);
	fileloaderDebugf1("Creating objects from %s...", pFilename);

	gWorldMgr.PreScan();

	char binaryIplName[RAGE_MAX_PATH];
	strcpy(binaryIplName, pFilename);
	char* pDotIndex;
	pDotIndex = strrchr(binaryIplName,'.');
	*pDotIndex = '\0';
	if(ASSET.Exists(binaryIplName, PLACEMENTS_FILE_EXT))
		return;

	char* pLine;
	IPLLoadingStatus status = LOADING_NOTHING;
	FileHandle fid;

	fid = CFileMgr::OpenFile(pFilename, "rb");
	Assertf( CFileMgr::IsValidFileHandle(fid), "Can not open '%s'", pFilename);
	if(!CFileMgr::IsValidFileHandle(fid))
		return;

#if __DEV
	SetCurrentLoadingFile(pFilename);
#endif

	// first, strip out all commas and escape sequence chars, and replace them with spaces
	while ((pLine = CFileMgr::ReadLine(fid)) != NULL)
	{
		// if beginning of line is the end it is empty
		if(*pLine == '\0')
			continue;

		// ignore lines starting with # they are comments
		if(*pLine == FILELOADER_COMMENT_CHAR)
			continue;

		if(status == LOADING_NOTHING)
		{
			// 'inst' tag indicates start of object instance section of file
			if(*pLine == 'i' && *(pLine+1) == 'n' && *(pLine+2) == 's' && *(pLine+3) =='t')
				status = LOADING_INSTANCES;
			// 'inst' tag indicates start of object instance section of file
			if(*pLine == 'm' && *(pLine+1) == 'u' && *(pLine+2) == 'l' && *(pLine+3) =='t')
				status = LOADING_MULTIBUILDINGS;
			// 'zone' tag indicates start of population zone objects section of file
			else if(*pLine == 'z' && *(pLine+1) == 'o' && *(pLine+2) == 'n' && *(pLine+3) =='e')
				status = LOADING_POPZONES;
			//	'mzon' tag indicates start of map area objects section of file
			else if (*pLine == 'm' && *(pLine+1) == 'z' && *(pLine+2) == 'o' && *(pLine+3) =='n')
				status = LOADING_MAPAREAS;
			// 'cull' tag indicates start of cullzone objects section of file
			else if(*pLine == 'c' && *(pLine+1) == 'u' && *(pLine+2) == 'l' && *(pLine+3) =='l')
				status = LOADING_CULLZONES;
			// 'occl' tag indicates start of occlusion volumes section of file
			else if(*pLine == 'o' && *(pLine+1) == 'c' && *(pLine+2) == 'c' && *(pLine+3) =='l')
				status = LOADING_OCCLUSION;
			// 'grge' tag indicates start of garage section of file
			else if(*pLine == 'g' && *(pLine+1) == 'r' && *(pLine+2) == 'g' && *(pLine+3) =='e')
				status = LOADING_GARAGE;
			// 'enex' tag indicates start of entry exit section of file
			else if(*pLine == 'e' && *(pLine+1) == 'n' && *(pLine+2) == 'e' && *(pLine+3) =='x')
				status = LOADING_ENTRYEXIT;
			else if(*pLine == 'p' && *(pLine+1) == 'i' && *(pLine+2) == 'c' && *(pLine+3) =='k')
				status = LOADING_PICKUPS;
			else if(*pLine == 'c' && *(pLine+1) == 'a' && *(pLine+2) == 'r' && *(pLine+3) =='s')
				status = LOADING_CARGENERATORS;
			else if(*pLine == 'j' && *(pLine+1) == 'u' && *(pLine+2) == 'm' && *(pLine+3) =='p')
				status = LOADING_STUNTJUMPS;
			else if(*pLine == 't' && *(pLine+1) == 'c' && *(pLine+2) == 'y' && *(pLine+3) =='c')
				status = LOADING_TIMECYCLEMODS;
			else if(*pLine == 'a' && *(pLine+1) == 'u' && *(pLine+2) == 'z' && *(pLine+3) =='o')
				status = LOADING_AUDIOZONES;
			else if(*pLine == 'b' && *(pLine+1) == 'l' && *(pLine+2) == 'o' && *(pLine+3) =='k')
				status = LOADING_BLOCKS;
			else if (*pLine == 'm' && *(pLine+1) == 'l' && *(pLine+2) == 'o' && *(pLine+3) =='+')
				status = LOADING_MLOAPPEND;
			else if (*pLine == '2' && *(pLine+1) == 'd' && *(pLine+2) == 'f' && *(pLine+3) =='x')
				status = LOADING_2DEFFECTS;
//			else if (*pLine == 'r' && *(pLine+1) == 't' && *(pLine+2) == 'f' && *(pLine+3) =='x')
//				status = LOADING_ROOT_SCRIPT;
			else if(*pLine == 'l' && *(pLine+1) == 'o' && *(pLine+2) == 'd' && *(pLine+3) =='m')
				status = LOADING_STREAM_BOXES;
			else if(*pLine == 's' && *(pLine+1) == 'l' && *(pLine+2) == 'o' && *(pLine+3) =='w')
				status = LOADING_SLOWNESS_ZONES;

		}
		else
		{
			// 'end' tag indicates end of current section
			if(*pLine == 'e' && *(pLine+1) == 'n' && *(pLine+2) == 'd')
			{
				status = LOADING_NOTHING;
			}
			else
			{
				switch(status)
				{
				case LOADING_INSTANCES:
					Assert(0);	// IanK
					break;
				case LOADING_MULTIBUILDINGS:
					Assert(0);	// IanK
					break;
				case LOADING_POPZONES:
					LoadPopZone(pLine);
					break;
				case LOADING_MAPAREAS:
					LoadMapArea(pLine);
					break;
				case LOADING_CULLZONES:
					LoadCullZone(pLine);
					break;
 				case LOADING_OCCLUSION:
					Assertf(0, "Loading occlusion data through IPL's is no longer supported");
					break;
				case LOADING_AUDIOZONES:
					Assert(0);			//IanK
					break;
				case LOADING_ENTRYEXIT:
					LoadEntryExit(pLine);
					break;
				case LOADING_GARAGE:
					LoadGarage(pLine);
					break;
				case LOADING_PICKUPS:
					Assert(0);		// IanK
					break;
				case LOADING_CARGENERATORS:
					Assert(0);		// IanK
					break;
				case LOADING_STUNTJUMPS:
					Assert(0);		// IanK
					break;
				case LOADING_TIMECYCLEMODS:
					Assert(0);
					break;
				case LOADING_MLOAPPEND:
					Assert(0);		// IanK - JohnW plans to fix this with new map data
					break;
				case LOADING_2DEFFECTS:
					Load2dEffect(pLine);
					break;
				case LOADING_BLOCKS:
					break;
				case LOADING_IGNORE:
					break;
				case LOADING_STREAM_BOXES:
					break;
				case LOADING_SLOWNESS_ZONES:
					Assert(0);
					break;

				default:
					Assert(0);
					break;
				}
			}
			// If status is still IGNORE then found a line so jump out of reading
			// this file
			if(status == LOADING_IGNORE)
			{
				break;
			}
		}
	}

	CFileMgr::CloseFile(fid);
#if __DEV
	SetCurrentLoadingFile(NULL);
#endif
}

#define EE_FLAG_RUBBISH		(1<<0)
#define EE_FLAG_OLDSTYLE	(1<<1)
#define EE_FLAG_LINK		(1<<2)
#define EE_FLAG_SHOP		(1<<3)
#define EE_FLAG_SHOP_EXIT	(1<<4)
#define EE_FLAG_CAR_WARP	(1<<5)
#define EE_FLAG_BIKE_WARP	(1<<6)
#define EE_FLAG_MIRROR		(1<<7)

//
//        name: CFileLoader::LoadEntryExit
// description: Loads one entry exit from an ipl file
void CFileLoader::LoadEntryExit(const char* pLine)
{
	float px,py,pz,prot;
	float wx,wy,wz;
	float spawnx,spawny,spawnz,spawnrot;
	s32 areacode,flags,extracol;
	char cTemp[32];
	char* p_cIndex;
	s32 numRandomPeds = 2;
	s32 openTime = 0;
	s32 shutTime = 24;

	ASSERT_ONLY( s32 iNumVals = )sscanf(pLine, "%f %f %f %f %f %f %f %f %f %f %f %d %d %s %d %d %d %d",
							&px,&py,&pz,&prot,
							&wx,&wy,&wz,
							&spawnx,&spawny,&spawnz,&spawnrot,
							&areacode,&flags,cTemp,&extracol,
							&numRandomPeds,
							&openTime, &shutTime);


	Assertf(iNumVals == 18,"incorrect num args for entry exit");

	p_cIndex = strrchr(cTemp,'\"');//"

	if(p_cIndex)
	{
		//if it has a speech mark at the end of the string
		//then it better have one at the start

		*p_cIndex = '\0';

		p_cIndex = cTemp;
		p_cIndex++;
	}
}

//
//        name: CFileLoader::LoadGarage
// description: Loads one garage from an ipl file
void CFileLoader::LoadGarage(const char* UNUSED_PARAM(pLine))
{
	//float x1,y1,z1;
	//float x2,y2;
	//float x3,y3;
	//float ztop;
	//s32 flag;
	//s32 type;
	//char cBuffer[GARAGE_NAME_LENGTH + 1];

	//if(sscanf(pLine, "%f %f %f %f %f %f %f %f %d %d %s", &x1,&y1,&z1,&x2,&y2,&x3,&y3,&ztop,&flag,&type,cBuffer ) != 11)
	//	return;

	//artAssertf(strlen(cBuffer) < 8, "garage name %s should be less than eight characters", cBuffer);
	//CGarages::AddOne(x1, y1, z1, x2, y2, x3, y3, ztop, type, 0, cBuffer, flag);
}

//
//        name: CFileLoader::LoadPopZone
// description: Load a zone from a line in the IPL
//
void CFileLoader::LoadPopZone(const char* pLine)
{
	char zoneIdNameBuff[STORE_NAME_LENGTH];
	Vector3 min,max;
	char associateTextLabelNameBuff[STORE_NAME_LENGTH];
	s32 zoneCategory;

	s32 numArgs = sscanf(pLine, "%s %f %f %f %f %f %f %s %d",
		&zoneIdNameBuff[0],
		&min.x, &min.y, &min.z,
		&max.x, &max.y, &max.z,
		&associateTextLabelNameBuff[0],
		&zoneCategory);

	if (numArgs==8)
	{
		zoneCategory = ZONECAT_NAVIGATION;
	}
	else if (numArgs != 9)
	{
		return;
	}

	switch (zoneCategory)
	{
		case ZONECAT_NAVIGATION :
		case ZONECAT_LOCAL_NAVIGATION :
			break;

		default:
			Assertf(0, "CFileLoader::LoadPopZone - expected zoneCategory to be NAVIGATION or LOCAL_NAVIGATION. Setting to NAVIGATION for %s", zoneIdNameBuff);
			zoneCategory = ZONECAT_NAVIGATION;
			break;
	}

 	CPopZones::CreatePopZone(zoneIdNameBuff, Vector3(min.x, min.y, min.z), Vector3(max.x, max.y, max.z), associateTextLabelNameBuff, static_cast<eZoneCategory>(zoneCategory));
}


//
//        name: CFileLoader::LoadMapArea
// description: Load a map zone from a line in the IPL
//
void CFileLoader::LoadMapArea(const char* pLine)
{
	Vector3 min,max;
	char BoxName[STORE_NAME_LENGTH];
	u32 HashOfAreaName = 0;

	s32 numArgs = sscanf(pLine, "%f %f %f %f %f %f %s %u",
		&min.x, &min.y, &min.z,
		&max.x, &max.y, &max.z,
		&BoxName[0], &HashOfAreaName);

	if(numArgs != 8)
	{
		Assertf(0, "CFileLoader::LoadMapZone - line in maparea.ipl doesn't contain the correct number of values");
		return;
	}

	CMapAreas::AddAreaBoxToArea(Vector3(min.x, min.y, min.z),Vector3(max.x, max.y, max.z), HashOfAreaName);
}

//
//        name: CFileLoader::LoadCullZone
// description: Loads one cull zone
void CFileLoader::LoadCullZone(const char* pLine)
{
	Vector3 posn;
	float Vec1X, Vec1Y, MinZ, Vec2X, Vec2Y, MaxZ;
	s32 Flags;
	s32 wantedLevelDrop=0;
	float MirrorNormalX, MirrorNormalY, MirrorNormalZ, MirrorV;

		// First test whether this might be a mirror attribute zone.
	if (sscanf(pLine, "%f %f %f %f %f %f %f %f %f %d %f %f %f %f",
			&posn.x, &posn.y, &posn.z, &Vec1X, &Vec1Y, &MinZ, &Vec2X, &Vec2Y, &MaxZ,
			&Flags, &MirrorNormalX, &MirrorNormalY, &MirrorNormalZ, &MirrorV) == 14)
	{
		CCullZones::AddMirrorAttributeZone(posn,
											Vec1X, Vec1Y, MinZ,
											Vec2X, Vec2Y, MaxZ,
											static_cast<u16>(Flags),
											MirrorV, MirrorNormalX, MirrorNormalY, MirrorNormalZ);

		return;
	}


	sscanf(pLine, "%f %f %f %f %f %f %f %f %f %d %d",
			&posn.x, &posn.y, &posn.z, &Vec1X, &Vec1Y, &MinZ, &Vec2X, &Vec2Y, &MaxZ,
			&Flags, &wantedLevelDrop);

	CCullZones::AddCullZone(posn,
							Vec1X, Vec1Y, MinZ,
							Vec2X, Vec2Y, MaxZ,
							static_cast<u16>(Flags),
							static_cast<s16>(wantedLevelDrop));
}

inline void Accurate_Normalize(Quaternion& q)
{
	float	mag2 = q.Mag2();
	float	invSqrtf = invsqrtf_precise(mag2);
	q.Scale(invSqrtf);
}
