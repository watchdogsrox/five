//	#include "basetypes.h"

#if __XENON
	#include "system/xtl.h"
#endif

#if __PPU
//	#include <time.h>
//	#include <cell/rtc.h>
//	#include <sysutil/sysutil_common.h>
//	#include <sysutil/sysutil_sysparam.h>
//	#pragma comment(lib, "rtc_stub")	//	Does this need to go in savegame_save.cpp now?
#endif


//	#include "system/timer.h"	//	Temp to time how long the reading of the filenames takes

// network includes
//	#include "network/Live/livemanager.h"

// Rage headers
#include "file/savegame.h"
//	#include "script/program.h"
#include "system/param.h"
//	#include "data/compress.h"

// Game headers

#include "camera/CamInterface.h"
//	#include "camera/gameplay/GameplayDirector.h"
//	#include "camera/gameplay/follow/FollowPedCamera.h"
//	#include "camera/gameplay/follow/FollowVehicleCamera.h"
#include "camera/viewports/ViewportManager.h"
//	#include "control/gamelogic.h"
#include "control/restart.h"
#include "Core/game.h"
#include "frontend/NewHud.h"
#include "frontend/WarningScreen.h"
#include "frontend/MiniMap.h"
#include "renderer/ScreenshotManager.h"
#include "renderer/PostProcessFx.h"
#include "SaveLoad/GenericGameStorage.h"
#include "peds/Ped.h"
#include "physics/physics.h"
#include "SaveLoad/savegame_autoload.h"
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/savegame_check_file_exists.h"
#include "SaveLoad/savegame_delete.h"
#include "SaveLoad/savegame_list.h"
#include "SaveLoad/savegame_load.h"
#include "SaveLoad/savegame_load_block.h"
#include "SaveLoad/savegame_new_game_checks.h"
#include "SaveLoad/savegame_photo_cloud_deleter.h"
#include "SaveLoad/savegame_photo_load_local.h"
#include "SaveLoad/savegame_photo_local_list.h"
#include "SaveLoad/savegame_photo_manager.h"
#include "SaveLoad/savegame_photo_save_local.h"
#include "SaveLoad/savegame_photo_unique_id.h"
#include "SaveLoad/savegame_queue.h"
#include "SaveLoad/savegame_save.h"
#include "SaveLoad/savegame_save_block.h"
#include "SaveLoad/savegame_slow_ps3_scan.h"
#include "SaveLoad/savegame_users_and_devices.h"
#include "SaveLoad/SavegameMigration/savegame_export.h"
#include "SaveLoad/SavegameMigration/savegame_import.h"
#include "SaveLoad/SavegameScriptData/script_save_data.h"
//	#include "scene/ExtraContent.h"
//	#include "Scene/portals/Portal.h"
#include "scene/streamer/SceneStreamerMgr.h"
#include "scene/world/GameWorld.h"
#include "script/commands_misc.h"
#include "scene/WarpManager.h"
//	#include "script/commands_object.h"
//	#include "script/mission_cleanup.h"
#include "script/script.h"
#include "script/script_cars_and_peds.h"
#include "script/script_debug.h"
#include "script/script_hud.h"
#include "Stats/StatsInterface.h"
//	#include "stats/StatsUtils.h"
#include "streaming/streaming.h"
#include "streaming/streamingengine.h"
#include "streaming/streamingrequestlist.h"
#include "Text/TextConversion.h"
#include "text/TextFile.h"
#include "modelinfo/PedModelInfo.h"
#include "System/controlMgr.h"
#if GTA_REPLAY && __ASSERT
#include "control/replay/replay.h"
#endif
#if RSG_PC
#include "rline/rlpc.h"
#endif

SAVEGAME_OPTIMISATIONS()


//Define a savegame assert channel
RAGE_DEFINE_CHANNEL(savegame, DIAG_SEVERITY_DEBUG2, DIAG_SEVERITY_DEBUG1, DIAG_SEVERITY_ASSERT)
RAGE_DEFINE_SUBCHANNEL(savegame,photo)
RAGE_DEFINE_SUBCHANNEL(savegame,script_data)
RAGE_DEFINE_SUBCHANNEL(savegame, psosize, DIAG_SEVERITY_DEBUG3)
RAGE_DEFINE_SUBCHANNEL(savegame, mp_fow)

#if RSG_PC
	RAGE_DEFINE_SUBCHANNEL(savegame, photo_mouse)
#endif	//	RSG_PC


#if __PPU
	#define sprintf_s sprintf
#endif //__PPU...

PARAM(nocreatorcheck, "[GenericGameStorage] Skips the playeriscreator check when loading XBox360 save games");

PARAM(doWarpAfterLoading, "[GenericGameStorage] Script is now in charge of setting the player's coords, loading the scene and fading in after a load. If something goes wrong (e.g. screen doesn't fade in) then try running with this param");

#if __BANK
PARAM(dontPrintScriptDataFromLoadedMpSaveFile, "[GenericGameStorage] don't print the script values read from loaded MP save files");
#endif	//	__BANK

#if __PPU
PARAM(savegameFolder, "[GenericGameStorage] On PS3, set the savegame folder to allow the European exe to load US saves (or vice versa)");
#endif	//	__PPU

// Started at 400000
// 405000 09/04/2014
// 420000 15/05/2014
// 430000 19/08/2014
// 465000 18/09/2014
// 475000 23/09/2014
// 480000 17/10/2014
// 490000 11/11/2014
// 495000 17/02/2015
// 500000 28/04/2015
// 505000 05/05/2015
// 520000 29/06/2015
// 560000 12/08/2015
// 590000 18/12/2015
// 610000 16/09/2016
// 640000 28/02/2017
// 01/05/2017 - Newly created saves should be much smaller than 640000. We've removed the saving of shop data in CExtraContentSaveStructure. It wasn't being used. 540000 should be a reasonable limit for newly created saves.
const u32 s_MaxBufferSize_SinglePlayer = 640000;
CSaveGameBuffer_SinglePlayer CGenericGameStorage::ms_SaveGameBuffer_SinglePlayer(s_MaxBufferSize_SinglePlayer);

// Started at 64000
// 72000 29/04/2014
// 85000 15/05/2014
// 90000 12/08/2014
// 96000 17/10/2014
// 102000 29/11/2014
// 120000 1/12/2014
// 140000 29/09/2015
// 150000 01/07/2016
// 160000 13/04/2017
// 170000 09/10/2017
// 180000 14/11/2017
// 190000 31/10/2018
const u32 s_MaxBufferSize_Multiplayer = 190000;
CSaveGameBuffer_MultiplayerCharacter	CGenericGameStorage::ms_SaveGameBuffer_MultiplayerCharacter_Cloud(s_MaxBufferSize_Multiplayer);
CSaveGameBuffer_MultiplayerCommon	CGenericGameStorage::ms_SaveGameBuffer_MultiplayerCommon_Cloud(s_MaxBufferSize_Multiplayer);

#if __ALLOW_LOCAL_MP_STATS_SAVES
CSaveGameBuffer_MultiplayerCharacter	CGenericGameStorage::ms_SaveGameBuffer_MultiplayerCharacter_Local(s_MaxBufferSize_Multiplayer);
CSaveGameBuffer_MultiplayerCommon	CGenericGameStorage::ms_SaveGameBuffer_MultiplayerCommon_Local(s_MaxBufferSize_Multiplayer);
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES

CSavegameQueuedOperation_CheckFileExists CGenericGameStorage::sm_CheckFileExistsQueuedOperation;
CSavegameQueuedOperation_Autosave CGenericGameStorage::sm_AutosaveStructure;

CSavegameQueuedOperation_MPStats_Load CGenericGameStorage::sm_MPStatsLoadQueuedOperation;
CSavegameQueuedOperation_MPStats_Save CGenericGameStorage::sm_MPStatsSaveQueuedOperation;
u32                                   CGenericGameStorage::sm_StatsSaveCategory = 0;


#if __PPU
#if __QUICK_SCAN_FOR_AUTOLOAD_AND_SLOW_SCAN_ON_ANOTHER_THREAD
CSavegameQueuedOperation_SlowPS3Scan CGenericGameStorage::sm_SlowPS3ScanStructure;
#endif	//	__QUICK_SCAN_FOR_AUTOLOAD_AND_SLOW_SCAN_ON_ANOTHER_THREAD
#endif	//	__PPU

CSavegameQueuedOperation_MissionRepeatLoad CGenericGameStorage::ms_MissionRepeatLoadQueuedOperation;
CSavegameQueuedOperation_MissionRepeatSave CGenericGameStorage::ms_MissionRepeatSaveQueuedOperation;

CSavegameQueuedOperation_LoadMostRecentSave CGenericGameStorage::ms_LoadMostRecentSave;

#if USE_SAVE_DATA_MEMORY && !USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP
CSavegameQueuedOperation_CreateBackupOfAutosaveSDM CGenericGameStorage::ms_CreateBackupOfAutosaveSDM;

bool CGenericGameStorage::ms_bBackupOfAutosaveHasBeenRequested = false;	//	I might not need to reset this when starting a new session.
																		//	On PS4, the player can't sign out of one profile and sign in to a different profile while the game is running 
																		//	so it's probably okay for the autosave to be made in one session and the backup to be made in the next session.
#endif	//	USE_SAVE_DATA_MEMORY && !USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
CSavegameQueuedOperation_ImportSinglePlayerSavegameIntoFixedSlot CGenericGameStorage::sm_ImportSinglePlayerSavegameIntoFixedSlot;
bool CGenericGameStorage::sm_QueueImportSinglePlayerSavegameIntoFixedSlot = false;
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
CSaveGameMigrationCloudText *CGenericGameStorage::sm_pSaveGameMigrationCloudText = NULL;
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES


eSaveOperation CGenericGameStorage::ms_SavegameOperation = OPERATION_NONE;


bool		CGenericGameStorage::ms_bDisplayAutosaveOverwriteMessage = true;

#if __ALLOW_LOCAL_MP_STATS_SAVES
bool CGenericGameStorage::ms_bOnlyReadTimestampFromMPSave = false;
u64 CGenericGameStorage::ms_TimestampReadFromMpSave = 0;

bool CGenericGameStorage::ms_bAllowReasonForFailureToBeSet = true;
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES

eReasonForFailureToLoadSavegame CGenericGameStorage::ms_ReasonForFailureToLoadSavegame = LOAD_FAILED_REASON_NONE;
int CGenericGameStorage::ms_CloudSaveResultcode = 0;
REPLAY_ONLY(bool CGenericGameStorage::ms_PushThroughReplaySave = false;)

bool CGenericGameStorage::ms_bClosestSaveHouseDataIsValid = false;
float CGenericGameStorage::ms_fHeadingOfClosestSaveHouse = 0.0f;
Vector3	CGenericGameStorage::ms_vCoordsOfClosestSaveHouse;

bool	CGenericGameStorage::ms_bFadeInAfterSuccessfulLoad = true;

bool	CGenericGameStorage::ms_bPlayerShouldSnapToGroundOnSpawn = true;

#if RSG_ORBIS
SceSaveDataDirName	CGenericGameStorage::m_SearchedDirNames[SAVE_DATA_COUNT];
uint32_t			CGenericGameStorage::m_SearchedDirNum;
CGenericGameStorage::DialogState CGenericGameStorage::m_DialogState;
SceUserServiceUserId	CGenericGameStorage::m_UserId;
int CGenericGameStorage::m_UserIndex;
SceSaveDataTitleId	CGenericGameStorage::m_TitleId;
SceSaveDataDirName CGenericGameStorage::m_SelectedDirName;
#endif

#if __BANK
bool CGenericGameStorage::ms_bPrintValuesAsPsoFileLoads = false;
bool CGenericGameStorage::ms_bPrintScriptDataFromLoadedMpSaveFile = true;

bool CGenericGameStorage::ms_bSaveToPC = false;
bool CGenericGameStorage::ms_bSaveToConsoleWhenSavingToPC = false;
char CGenericGameStorage::ms_NameOfPCSaveFile[ms_MaxLengthOfPcSaveFileName];

bool CGenericGameStorage::ms_bLoadFromPC = false;
bool CGenericGameStorage::ms_bDecryptOnLoading = true;
char CGenericGameStorage::ms_NameOfPCLoadFile[ms_MaxLengthOfPcSaveFileName];

bool CGenericGameStorage::ms_bLoadMpStatsFileFromPC = false;
bool CGenericGameStorage::ms_bMpStatsFileIsACharacterFile = false;
bool CGenericGameStorage::ms_bDecryptMpStatsFileOnLoading = true;
char CGenericGameStorage::ms_NameOfMpStatsPCLoadFile[ms_MaxLengthOfPcSaveFileName];

bool CGenericGameStorage::ms_bSaveAsPso = true;

char CGenericGameStorage::ms_NameOfPCSaveFolder[ms_MaxLengthOfPcSaveFolderName];

bool CGenericGameStorage::ms_bStartMpStatsCloudSave = false;
bool CGenericGameStorage::ms_bStartMpStatsCloudLoad = false;

bool CGenericGameStorage::ms_bDisplayLoadingSpinnerWhenSaving = false;

#if RSG_ORBIS
bool CGenericGameStorage::ms_bAlwaysLoadBackupSaves = false;
#endif	//	RSG_ORBIS

bool CGenericGameStorage::ms_bRequestProfileWrite = false;

// bool CGenericGameStorage::ms_bStartCheckIfMpStatsFileExists = false;
// bool CGenericGameStorage::ms_bMpStatsFileExistsCheckIsInProgress = false;
// bool CGenericGameStorage::ms_bMpStatsFileExists = false;
// bool CGenericGameStorage::ms_bMpStatsFileIsDamaged = false;
// u32  CGenericGameStorage::ms_MpSlotIndex = 0;

//	WARNING MESSAGES
u32 CGenericGameStorage::ms_NumberOfFramesToDisplayWarningMessageForBeforePausingTheGame = NUM_FRAMES_TO_DISPLAY_WARNING_MESSAGE_BEFORE_PAUSING_GAME;
u32 CGenericGameStorage::ms_NumberOfFramesToDisplayBlackScreenAfterWarningMessage = NUM_FRAMES_TO_DISPLAY_BLACK_SCREEN_AFTER_WARNING_MESSAGE;


//	SAVED DATA
bool CGenericGameStorage::ms_bSaveSimpleVariables = true;
bool CGenericGameStorage::ms_bSaveMiniMapVariables = true;
bool CGenericGameStorage::ms_bSavePlayerPedVariables = true;
bool CGenericGameStorage::ms_bSaveExtraContentVariables = true;
bool CGenericGameStorage::ms_bSaveGarageVariables = true;
bool CGenericGameStorage::ms_bSaveStatsVariables = true;
bool CGenericGameStorage::ms_bSaveStuntJumpVariables = true;
bool CGenericGameStorage::ms_bSaveRadioStationVariables = true;
bool CGenericGameStorage::ms_bSavePS3AchievementVariables = true;
bool CGenericGameStorage::ms_bSaveDoorVariables = true;
bool CGenericGameStorage::ms_bSaveCameraVariables = true;
bool CGenericGameStorage::ms_bSaveScriptVariables = true;

//	PHOTO GALLERY
bool CGenericGameStorage::ms_bSavePhoto = false;
bool CGenericGameStorage::ms_bPhotoSaveInProgressReadOnly = false;
//	END OF PHOTO GALLERY

#if RSG_ORBIS
int CGenericGameStorage::ms_iUserIndex = 0;
#endif

#endif	//	__BANK


// GSWTODO - should really get rid of these and CGenericGameStorage::IsCalculatingBufferSize() - remove these three functions this week
//	They're just here to stop me having to change CGenericGameStorage to CSaveGameData in lots of files for now
/*
bool CGenericGameStorage::SaveDataToWorkBuffer(const void* pData, s32 SizeOfData)
{
	return CSaveGameData::SaveDataToWorkBuffer(pData, SizeOfData);
}

bool CGenericGameStorage::LoadDataFromWorkBuffer(void* pData, s32 SizeOfData)
{
	return CSaveGameData::LoadDataFromWorkBuffer(pData, SizeOfData, true);
}

bool CGenericGameStorage::ForwardReadPositionInWorkBuffer(s32 SizeOfData)
{
	return CSaveGameData::LoadDataFromWorkBuffer(NULL, SizeOfData, false);
}
*/



// *************************************************************************************************


void CGenericGameStorage::Init(unsigned initMode)
{
#if __PPU && __QUICK_SCAN_FOR_AUTOLOAD_AND_SLOW_SCAN_ON_ANOTHER_THREAD
	//	The PS3 scan thread has to finish (in CSavegameSlowPS3Scan::Init(INIT_AFTER_MAP_LOADED))
	//	before ms_NumberOfSaveGameSlots is reset in CSavegameList::Init(INIT_AFTER_MAP_LOADED)
	CSavegameSlowPS3Scan::Init(initMode);
#endif

	CSavegameAutoload::Init(initMode);
	CSavegameCheckFileExists::Init(initMode);
	CSaveGameData::Init(initMode);
	CSavegameDelete::Init(initMode);
	CSavegameDevices::Init(initMode);
	CSavegameDialogScreens::Init(initMode);
//	CSavegameGetFileTimes::Init(initMode);
	CSavegameList::Init(initMode);
	CSavegameLoad::Init(initMode);
//	CSavegameLoadBlock::Init(initMode);
	CSavegameLoadJpegFromCloud::Init(initMode);
#if RSG_ORBIS
	CSavegameNewGameChecks::Init(initMode);
#endif
	CSavegameQueue::Init(initMode);
	CSavegameSave::Init(initMode);
	CSavegameSaveBlock::Init(initMode);

	CSavegamePhotoUniqueId::Init(initMode);
	CSavegamePhotoLocalList::Init();

#if __LOAD_LOCAL_PHOTOS
	CSavegamePhotoCloudDeleter::Init(initMode);
	CSavegamePhotoCloudList::Init(initMode);
#endif	//	__LOAD_LOCAL_PHOTOS

	CSavegamePhotoSaveLocal::Init(initMode);
	CSavegamePhotoLoadLocal::Init(initMode);
	CSavegameUsers::Init(initMode);

	ms_SaveGameBuffer_SinglePlayer.Init(initMode);
	ms_SaveGameBuffer_MultiplayerCharacter_Cloud.Init(initMode);
	ms_SaveGameBuffer_MultiplayerCommon_Cloud.Init(initMode);

#if __ALLOW_LOCAL_MP_STATS_SAVES
	ms_SaveGameBuffer_MultiplayerCharacter_Local.Init(initMode);
	ms_SaveGameBuffer_MultiplayerCommon_Local.Init(initMode);
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
	CSaveGameImport::Init(initMode);
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	CSaveGameExport::Init(initMode);
	if (!sm_pSaveGameMigrationCloudText)
	{
		sm_pSaveGameMigrationCloudText = rage_new CSaveGameMigrationCloudText();
	}
	if (sm_pSaveGameMigrationCloudText)
	{
		sm_pSaveGameMigrationCloudText->Init(initMode);
	}
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

    if(initMode == INIT_CORE)
    {
		ms_bClosestSaveHouseDataIsValid = false;

	    ms_bFadeInAfterSuccessfulLoad = true;
		ms_bPlayerShouldSnapToGroundOnSpawn = true;

    #if __XENON
		CSavegameDevices::InitNotificationListener();
    #endif

    #if __PPU
		SetSaveGameFolder();

		SAVEGAME.SetSaveGameTitle("Grand Theft Auto V");

		CDate::SetPS3DateFormat();
    #endif	//	__PPU

#if RSG_ORBIS
		m_UserIndex = 0; // default
		m_UserId = g_rlNp.GetUserServiceId(0);
		memcpy(m_TitleId.data, XEX_TITLE_ID, SCE_SAVE_DATA_TITLE_ID_DATA_SIZE);
		SAVEGAME.SetSaveGameTitle("Grand Theft Auto V");
		memset(&m_SelectedDirName, 0x00, sizeof(m_SelectedDirName));
#endif

		CSavingMessage::Clear();

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
		sm_QueueImportSinglePlayerSavegameIntoFixedSlot = false;
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

#if __BANK
		ms_bPrintValuesAsPsoFileLoads = false;

		ms_bPrintScriptDataFromLoadedMpSaveFile = true;
		if (PARAM_dontPrintScriptDataFromLoadedMpSaveFile.Get())
		{
			ms_bPrintScriptDataFromLoadedMpSaveFile = false;
		}		

		ms_bSaveToPC = false;
		ms_bSaveToConsoleWhenSavingToPC = false;
		ms_NameOfPCSaveFile[0] = '\0';

		ms_bLoadFromPC = false;
		ms_bDecryptOnLoading = true;
		ms_NameOfPCLoadFile[0] = '\0';

		ms_bLoadMpStatsFileFromPC = false;
		ms_bMpStatsFileIsACharacterFile = false;
		ms_bDecryptMpStatsFileOnLoading = true;
		ms_NameOfMpStatsPCLoadFile[0] = '\0';

		ms_bSaveAsPso = true;

		strncpy(ms_NameOfPCSaveFolder, "saves", ms_MaxLengthOfPcSaveFolderName);

		ms_bStartMpStatsCloudSave = false;
		ms_bStartMpStatsCloudLoad = false;

		ms_bDisplayLoadingSpinnerWhenSaving = false;

#if RSG_ORBIS
		ms_bAlwaysLoadBackupSaves = false;
#endif	//	RSG_ORBIS

		ms_bRequestProfileWrite = false;

// 		ms_bStartCheckIfMpStatsFileExists = false;
// 		ms_bMpStatsFileExistsCheckIsInProgress = false;
// 		ms_bMpStatsFileExists = false;
// 		ms_bMpStatsFileIsDamaged = false;

		//	SAVED DATA
		ms_bSaveSimpleVariables = true;
		ms_bSaveMiniMapVariables = true;
		ms_bSavePlayerPedVariables = true;
		ms_bSaveExtraContentVariables = true;
		ms_bSaveGarageVariables = true;
		ms_bSaveStatsVariables = true;
		ms_bSaveStuntJumpVariables = true;
		ms_bSaveRadioStationVariables = true;
		ms_bSavePS3AchievementVariables = true;
		ms_bSaveDoorVariables = true;
		ms_bSaveCameraVariables = true;
		ms_bSaveScriptVariables = true;

#endif	//	__BANK
    }
    else if(initMode == INIT_AFTER_MAP_LOADED)
    {
	    ms_SavegameOperation = OPERATION_NONE;

	    SetFlagForAutosaveOverwriteMessage(true);

    //	ms_bFadeInAfterSuccessfulLoad = true;	//	Can't do this here. After a load, it happens before the flag is checked
											    //	to decided whether to fade in or not.
											    //	It would have been good if it could have been set here
											    //	to ensure that it's in the default state when 
											    //	a new session is started.

    //	try moving this to Init() so that load successful message can be displayed
    //	CSavingMessage::Clear();

#if RSG_ORBIS
		if (TheText.DoesTextLabelExist("SG_BACKUP"))
		{
			SAVEGAME.SetLocalisedBackupTitleString(TheText.Get("SG_BACKUP"));
		}
#endif	//	RSG_ORBIS
    }
}


void CGenericGameStorage::Shutdown(unsigned shutdownMode)
{
	CSavegameQueue::Shutdown(shutdownMode);

	if(shutdownMode == SHUTDOWN_CORE)
	{
		//	Do I have to wait for ms_ReadNamesThreadId to end here?
		FinishAllUnfinishedSaveGameOperations();

#if __XENON
		CSavegameDevices::ShutdownNotificationListener();
#endif
	}
	else if(shutdownMode == SHUTDOWN_SESSION)
	{
		//	Graeme - maybe I should be emptying the savegame queue or something, rather than doing this
		while (CGenericGameStorage::IsSavegameSaveInProgress())
		{
#if RSG_PC
			// the Social Club UI needs updates in order to close the UI/respond to user input, etc.
			g_rlPc.Update();
#endif
			CWarningScreen::Update();
			//	Will this work? It could be that some of the required save data has already been freed by this stage
			CSavegameSave::GenericSave(false);
			sysIpcSleep(16);
		}

		//	Is this going to work at all?
		FinishAllUnfinishedSaveGameOperations(); // to ensure that the network doesn't get started until the save is done.
	}

	CSaveGameData::Shutdown(shutdownMode);
	CSavegameDelete::Shutdown(shutdownMode);
//	CSavegameLoadBlock::Shutdown(shutdownMode);
	CSavegameLoadJpegFromCloud::Shutdown(shutdownMode);

	CSavegameSave::Shutdown(shutdownMode);
	CSavegameSaveBlock::Shutdown(shutdownMode);
	CSavegamePhotoSaveLocal::Shutdown(shutdownMode);
	CSavegamePhotoLoadLocal::Shutdown(shutdownMode);

	ms_SaveGameBuffer_SinglePlayer.Shutdown(shutdownMode);
	ms_SaveGameBuffer_MultiplayerCharacter_Cloud.Shutdown(shutdownMode);
	ms_SaveGameBuffer_MultiplayerCommon_Cloud.Shutdown(shutdownMode);

#if __ALLOW_LOCAL_MP_STATS_SAVES
	ms_SaveGameBuffer_MultiplayerCharacter_Local.Shutdown(shutdownMode);
	ms_SaveGameBuffer_MultiplayerCommon_Local.Shutdown(shutdownMode);
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
	CSaveGameImport::Shutdown(shutdownMode);
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	CSaveGameExport::Shutdown(shutdownMode);

	if (sm_pSaveGameMigrationCloudText)
	{
		sm_pSaveGameMigrationCloudText->Shutdown(shutdownMode);
		if(shutdownMode == SHUTDOWN_CORE)
		{
			delete sm_pSaveGameMigrationCloudText;
			sm_pSaveGameMigrationCloudText = NULL;
		}
	}
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES
}


void CGenericGameStorage::UpdatePhotoGallery()
{
	PF_AUTO_PUSH_TIMEBAR("CGenericGameStorage UpdatePhotoGallery");

#if	__BANK
//	Saving
	if (CPhotoManager::GetSaveCameraPhotoStatus() == MEM_CARD_BUSY)
	{
		ms_bPhotoSaveInProgressReadOnly = true;
	}
	else
	{
		ms_bPhotoSaveInProgressReadOnly = false;
	}

	if (ms_bSavePhoto)
	{
		if (!ms_bPhotoSaveInProgressReadOnly)
		{
//			CScriptHud::BeginPhotoSave();	//	ms_SlotToSavePhotoTo, ms_bSaveToCloud);
			CPhotoManager::RequestSaveCameraPhoto();
		}

		ms_bSavePhoto = false;
	}
#endif	//	__BANK
}

#if USE_SAVE_DATA_MEMORY && !USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP
void CGenericGameStorage::UpdatePS4AutosaveBackup()
{
	if (ms_bBackupOfAutosaveHasBeenRequested)
	{
		if (IsSafeToUseSaveLibrary())
		{
			if (ms_CreateBackupOfAutosaveSDM.GetStatus() != MEM_CARD_BUSY)
			{
				if (CSavegameQueue::IsEmpty())
				{	//	To give the backup save the best chance of completing quickly, only push it on to the queue if the queue is empty.
					//	Any delay in starting this save increases the likelihood of the game getting back into a state where IsSafeToUseSaveLibrary() would have returned false.
					ms_CreateBackupOfAutosaveSDM.Init();
					if (savegameVerifyf(PushOnToSavegameQueue(&ms_CreateBackupOfAutosaveSDM), "CGenericGameStorage::UpdatePS4AutosaveBackup - failed to push a PS4 autosave backup on to the savegame queue"))
					{
						ms_bBackupOfAutosaveHasBeenRequested = false;
					}
				}
			}
		}
	}
}
#endif	//	USE_SAVE_DATA_MEMORY && !USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP


void CGenericGameStorage::Update()
{
	// the save game is taking too long so this function gets called way too many times
	// the result is that too many timebars are pushed
	// the timebar call has been moved to whoever calls Update()
	// PF_AUTO_PUSH_TIMEBAR("CGenericGameStorage Update");

	CSavegameQueue::Update();

	CSaveGameData::Update();

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
	CheckForImportSinglePlayerSavegameIntoFixedSlot();
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	CSaveGameExport::Update();
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

//	UpdatePhotoGallery();

//    if (IsGameSaveInProgress())
//	{
//		CSavegameSave::GenericSave();
//	}

#if USE_SAVE_DATA_MEMORY && !USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP
	UpdatePS4AutosaveBackup();
#endif	//	USE_SAVE_DATA_MEMORY && !USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP

#if RSG_ORBIS
	CSavegameList::BackgroundScanForDamagedSavegames();
#endif	//	__PPU

#if RSG_ORBIS
	UpdateDialog();
#endif

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	if (sm_pSaveGameMigrationCloudText)
	{
		sm_pSaveGameMigrationCloudText->Update();
	}
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

#if __BANK
	CheckForProfileWriteRequest();

	CheckForLoadFromPC();

	CheckForMpStatsSaveAndLoad();

	CSavegameQueue::UpdateWidgets();
#endif	//	__BANK
}

void CGenericGameStorage::QueueAutosave()
{
	if (savegameVerifyf(sm_AutosaveStructure.GetStatus() != MEM_CARD_BUSY, "CGenericGameStorage::QueueAutosave - can't queue an autosave while one is already in progress"))
	{
		sm_AutosaveStructure.Init();
		if (!PushOnToSavegameQueue(&sm_AutosaveStructure))
		{
			savegameAssertf(0, "CGenericGameStorage::QueueAutosave - failed to queue the autosave operation");
		}

#if __PPU || RSG_ORBIS
		//	Push a profile save on to the save queue too?
		CProfileSettings::GetInstance().Write(false);
#endif	//	__PPU
	}
}


bool CGenericGameStorage::QueueMpStatsSave(const u32 saveCategory
#if __ALLOW_LOCAL_MP_STATS_SAVES
										   , CSavegameQueuedOperation_MPStats_Save::eMPStatsSaveDestination saveDestination
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES
										   )
{
	if (savegameVerifyf(sm_MPStatsSaveQueuedOperation.GetStatus() != MEM_CARD_BUSY, "CGenericGameStorage::QueueMpStatsSave - can't queue an MP stats save while one is already in progress"))
	{
		sm_StatsSaveCategory = saveCategory;

		sm_MPStatsSaveQueuedOperation.Init(saveCategory
#if __ALLOW_LOCAL_MP_STATS_SAVES
			, saveDestination
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES
			);

		if (CGenericGameStorage::PushOnToSavegameQueue(&sm_MPStatsSaveQueuedOperation))
		{
			return true;
		}

		savegameAssertf(0, "CGenericGameStorage::QueueMpStatsSave - failed to queue the MP stats save operation");
	}

	return false;
}

MemoryCardError CGenericGameStorage::GetMpStatsSaveStatus()
{
	return sm_MPStatsSaveQueuedOperation.GetStatus();
}

void CGenericGameStorage::ClearMpStatsSaveStatus()
{
	return sm_MPStatsSaveQueuedOperation.ClearStatus();
}

bool CGenericGameStorage::QueueMpStatsLoad(const u32 saveCategory)
{
	if (savegameVerifyf(sm_MPStatsLoadQueuedOperation.GetStatus() != MEM_CARD_BUSY, "CGenericGameStorage::QueueMpStatsLoad - can't queue an MP stats load while one is already in progress"))
	{
		sm_StatsSaveCategory = saveCategory;

		sm_MPStatsLoadQueuedOperation.Init(saveCategory);

		if (CGenericGameStorage::PushOnToSavegameQueue(&sm_MPStatsLoadQueuedOperation))
		{
			return true;
		}

		savegameAssertf(0, "CGenericGameStorage::QueueMpStatsLoad - failed to queue the MP stats load operation");
	}

	return false;
}

MemoryCardError CGenericGameStorage::GetMpStatsLoadStatus()
{
	return sm_MPStatsLoadQueuedOperation.GetStatus();
}

void CGenericGameStorage::ClearMpStatsLoadStatus()
{
	return sm_MPStatsLoadQueuedOperation.ClearStatus();
}

void CGenericGameStorage::CancelMpStatsSave()
{
	sm_MPStatsSaveQueuedOperation.Cancel();
}

void CGenericGameStorage::CancelMpStatsLoad()
{
	sm_MPStatsLoadQueuedOperation.Cancel();
}

bool CGenericGameStorage::QueueCheckFileExists(s32 SlotIndex)
{
	if (savegameVerifyf(sm_CheckFileExistsQueuedOperation.GetStatus() != MEM_CARD_BUSY, "CGenericGameStorage::QueueCheckFileExists - can't queue a CheckFileExists operation while one is already in progress"))
	{
		sm_CheckFileExistsQueuedOperation.Init(SlotIndex);
		if (CGenericGameStorage::PushOnToSavegameQueue(&sm_CheckFileExistsQueuedOperation))
		{
			return true;
		}
		savegameAssertf(0, "CGenericGameStorage::QueueCheckFileExists - failed to queue the CheckFileExists operation");
	}

	return false;
}

MemoryCardError CGenericGameStorage::GetCheckFileExistsStatus(bool &bFileExists, bool &bFileIsDamaged)
{
	sm_CheckFileExistsQueuedOperation.GetFileExists(bFileExists, bFileIsDamaged);
	return sm_CheckFileExistsQueuedOperation.GetStatus();
}

void CGenericGameStorage::ClearCheckFileExistsStatus( )
{
	sm_CheckFileExistsQueuedOperation.ClearStatus();
}


bool CGenericGameStorage::QueueMissionRepeatLoad()
{
	if (savegameVerifyf(ms_MissionRepeatLoadQueuedOperation.GetStatus() != MEM_CARD_BUSY, "CGenericGameStorage::QueueMissionRepeatLoad - can't queue a MissionRepeatLoad operation while one is already in progress"))
	{
		ms_MissionRepeatLoadQueuedOperation.Init();
		if (CGenericGameStorage::PushOnToSavegameQueue(&ms_MissionRepeatLoadQueuedOperation))
		{
			REPLAY_ONLY(ms_PushThroughReplaySave = true;)// Force any replay clip save we might need
			return true;
		}
		savegameAssertf(0, "CGenericGameStorage::QueueMissionRepeatLoad - failed to queue the MissionRepeatLoad operation");
	}

	return false;
}

MemoryCardError CGenericGameStorage::GetMissionRepeatLoadStatus()
{
	return ms_MissionRepeatLoadQueuedOperation.GetStatus();
}

void CGenericGameStorage::ClearMissionRepeatLoadStatus()
{
	ms_MissionRepeatLoadQueuedOperation.ClearStatus();
}

bool CGenericGameStorage::QueueMissionRepeatSave(bool bBenchmarkTest)
{
	if (savegameVerifyf(ms_MissionRepeatSaveQueuedOperation.GetStatus() != MEM_CARD_BUSY, "CGenericGameStorage::QueueMissionRepeatSave - can't queue a MissionRepeatSave operation while one is already in progress"))
	{
		ms_MissionRepeatSaveQueuedOperation.Init(bBenchmarkTest);
		if (CGenericGameStorage::PushOnToSavegameQueue(&ms_MissionRepeatSaveQueuedOperation))
		{
			return true;
		}
		savegameAssertf(0, "CGenericGameStorage::QueueMissionRepeatSave - failed to queue the MissionRepeatSave operation");
	}

	return false;
}

MemoryCardError CGenericGameStorage::GetMissionRepeatSaveStatus()
{
	return ms_MissionRepeatSaveQueuedOperation.GetStatus();
}

void CGenericGameStorage::ClearMissionRepeatSaveStatus()
{
	ms_MissionRepeatSaveQueuedOperation.ClearStatus();
}


bool CGenericGameStorage::QueueLoadMostRecentSave()
{
	if (savegameVerifyf(ms_LoadMostRecentSave.GetStatus() != MEM_CARD_BUSY, "CGenericGameStorage::QueueLoadMostRecentSave - can't queue a LoadMostRecentSave operation while one is already in progress"))
	{
		ms_LoadMostRecentSave.Init();
		if (CGenericGameStorage::PushOnToSavegameQueue(&ms_LoadMostRecentSave))
		{
			return true;
		}
		savegameAssertf(0, "CGenericGameStorage::QueueLoadMostRecentSave - failed to queue the LoadMostRecentSave operation");
	}

	return false;
}


#if __PPU
#if __QUICK_SCAN_FOR_AUTOLOAD_AND_SLOW_SCAN_ON_ANOTHER_THREAD
void CGenericGameStorage::QueueSlowPS3Scan()
{
	if (CSavegameSlowPS3Scan::GetScanHasAlreadyBeenDone())
	{	//	Only want this to do something at the start of the game
		return;
	}

	if (savegameVerifyf(sm_SlowPS3ScanStructure.GetStatus() != MEM_CARD_BUSY, "CGenericGameStorage::QueueSlowPS3Scan - can't queue the slow PS3 scan while one is already in progress"))
	{
		sm_SlowPS3ScanStructure.Init();
		if (!PushOnToSavegameQueue(&sm_SlowPS3ScanStructure))
		{
			savegameAssertf(0, "CGenericGameStorage::QueueSlowPS3Scan - failed to queue the slow PS3 scan operation");
		}
	}
}
#endif	//	__QUICK_SCAN_FOR_AUTOLOAD_AND_SLOW_SCAN_ON_ANOTHER_THREAD
#endif	//	__PPU


#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
bool CGenericGameStorage::RequestImportSinglePlayerSavegameIntoFixedSlot(const u8 *pData, u32 dataSize)
{
	if (savegameVerifyf(sm_ImportSinglePlayerSavegameIntoFixedSlot.GetStatus() != MEM_CARD_BUSY, "CGenericGameStorage::RequestImportSinglePlayerSavegameIntoFixedSlot - can't modify sm_ImportSinglePlayerSavegameIntoFixedSlot while it's already in progress"))
	{
		// Graeme - this is unusual
		// We don't normally do any work until the queued operation is at the top of the queue.
		// All the work is then done by the operation's Update() function.
		// In this case, the grow buffer is only guaranteed to exist for one frame so we need to make a copy of 
		//	its contents immediately.
		if (sm_ImportSinglePlayerSavegameIntoFixedSlot.CopyDataImmediately(pData, dataSize))
		{
			sm_QueueImportSinglePlayerSavegameIntoFixedSlot = true;
			return true;
		}
		else
		{
			savegameErrorf("CGenericGameStorage::RequestImportSinglePlayerSavegameIntoFixedSlot - failed to create a copy of the data");
			savegameAssertf(0, "CGenericGameStorage::RequestImportSinglePlayerSavegameIntoFixedSlot - failed to create a copy of the data");
		}
	}

	return false;
}


void CGenericGameStorage::CheckForImportSinglePlayerSavegameIntoFixedSlot()
{
	if (sm_QueueImportSinglePlayerSavegameIntoFixedSlot)
	{
#if RSG_SCE
		if (CSavegameNewGameChecks::HaveFreeSpaceChecksCompleted())
#endif // RSG_SCE
		{
			if (CScriptSaveData::MainHasRegisteredScriptData(SAVEGAME_SINGLE_PLAYER))
			{
				if (savegameVerifyf(sm_ImportSinglePlayerSavegameIntoFixedSlot.GetStatus() != MEM_CARD_BUSY, "CGenericGameStorage::CheckForImportSinglePlayerSavegameIntoFixedSlot - can't queue an ImportSinglePlayerSavegameIntoFixedSlot operation while one is already in progress"))
				{
					sm_ImportSinglePlayerSavegameIntoFixedSlot.Init();
					if (CGenericGameStorage::PushOnToSavegameQueue(&sm_ImportSinglePlayerSavegameIntoFixedSlot))
					{
						sm_QueueImportSinglePlayerSavegameIntoFixedSlot = false;
					}
					else
					{
						savegameErrorf("CGenericGameStorage::CheckForImportSinglePlayerSavegameIntoFixedSlot - failed to queue the ImportSinglePlayerSavegameIntoFixedSlot operation");
						savegameAssertf(0, "CGenericGameStorage::CheckForImportSinglePlayerSavegameIntoFixedSlot - failed to queue the ImportSinglePlayerSavegameIntoFixedSlot operation");
					}
				}
			}
			else
			{
				savegameDebugf1("CGenericGameStorage::CheckForImportSinglePlayerSavegameIntoFixedSlot - waiting for the scripts to register their save data");
			}
		}
#if RSG_SCE
		else
		{
			savegameDebugf1("CGenericGameStorage::CheckForImportSinglePlayerSavegameIntoFixedSlot - waiting for the free space checks to complete");
		}
#endif // RSG_SCE
	}
}
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES


bool CGenericGameStorage::PushOnToSavegameQueue(CSavegameQueuedOperation *pNewOperation)
{
	return CSavegameQueue::Push(pNewOperation);
}

bool CGenericGameStorage::ExistsInQueue(CSavegameQueuedOperation *pOperationToCheck, bool &bAtTop)
{
	return CSavegameQueue::ExistsInQueue(pOperationToCheck, bAtTop);
}

void CGenericGameStorage::RemoveFromSavegameQueue(CSavegameQueuedOperation *pOperationToRemove)
{
	CSavegameQueue::Remove(pOperationToRemove);
}


#if __BANK
//
// Sets up a debug widget to test the save game.
//
namespace misc_commands {
void CommandActivateSaveMenu();
};

#if RSG_ORBIS
void ShowSaveDialog()
{
	CGenericGameStorage::ShowSaveDialog(CGenericGameStorage::ms_iUserIndex);
}

void ShowLoadDialog()
{
	CGenericGameStorage::ShowLoadDialog(CGenericGameStorage::ms_iUserIndex);
}

void ShowDeleteDialog()
{
	CGenericGameStorage::ShowDeleteDialog(CGenericGameStorage::ms_iUserIndex);
}
#endif

void CGenericGameStorage::InitWidgets()
{
	bkBank *pBank = BANKMGR.FindBank("Savegame");
	if (!pBank)
	{
		pBank = &BANKMGR.CreateBank("Savegame");
	}

	if(pBank)
	{
#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
		CSaveGameExport::InitWidgets(pBank);
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

		pBank->AddToggle("Autosave Enabled", &CScriptDebug::GetAutoSaveEnabled());	//	These two are duplicates of the widgets
		pBank->AddToggle("Perform Autosave", &CScriptDebug::GetPerformAutoSave());	//	in script.cpp

		pBank->AddToggle("Request Profile Write", &ms_bRequestProfileWrite);

		pBank->AddToggle("Save as PSO file", &ms_bSaveAsPso);
		pBank->AddToggle("Print values as PSO file loads", &ms_bPrintValuesAsPsoFileLoads);

		pBank->AddToggle("Print Script Data From Loaded Mp Save File", &ms_bPrintScriptDataFromLoadedMpSaveFile);

		pBank->AddToggle("Save to PC", &ms_bSaveToPC);
		pBank->AddToggle("Save to Console when saving to PC", &ms_bSaveToConsoleWhenSavingToPC);

		pBank->AddText("Name of PC Save file(no ext)", ms_NameOfPCSaveFile, ms_MaxLengthOfPcSaveFileName, false);

		pBank->AddToggle("Perform Load from PC", &ms_bLoadFromPC);
		pBank->AddToggle("Decrypt file on loading", &ms_bDecryptOnLoading);
		pBank->AddText("Name of PC Load file", ms_NameOfPCLoadFile, ms_MaxLengthOfPcSaveFileName, false);

		pBank->AddToggle("Load MP Stats file from PC", &ms_bLoadMpStatsFileFromPC);
		pBank->AddToggle("MP Stats file is a Character file", &ms_bMpStatsFileIsACharacterFile);
		pBank->AddToggle("Decrypt MP Stats file on loading", &ms_bDecryptMpStatsFileOnLoading);
		pBank->AddText("Name of MP Stats PC Load file", ms_NameOfMpStatsPCLoadFile, ms_MaxLengthOfPcSaveFileName, false);

		pBank->AddToggle("Start MP Stats Cloud Save", &ms_bStartMpStatsCloudSave);
		pBank->AddToggle("Start MP Stats Cloud Load", &ms_bStartMpStatsCloudLoad);

		pBank->AddToggle("Display Loading Spinner when Saving", &ms_bDisplayLoadingSpinnerWhenSaving);

#if RSG_ORBIS
		pBank->AddToggle("Always Load Backup Saves", &ms_bAlwaysLoadBackupSaves);

#if USE_SAVE_DATA_MEMORY && !USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP
		pBank->AddToggle("Backup Of Autosave Has Been Requested", &ms_bBackupOfAutosaveHasBeenRequested);
#endif	//	USE_SAVE_DATA_MEMORY && !USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP

#endif	//	RSG_ORBIS

//		pBank->AddSlider("Multiplayer Slot", &ms_MpSlotIndex, 0, 6, 1);

// 		pBank->AddToggle("Start Check if MP Stats File Exists", &ms_bStartCheckIfMpStatsFileExists);
// 		pBank->AddToggle("Checking if MP Stats File Exists", &ms_bMpStatsFileExistsCheckIsInProgress);
// 		pBank->AddToggle("MP Stats File Does Exist", &ms_bMpStatsFileExists);
// 		pBank->AddToggle("MP Stats File Is Damaged", &ms_bMpStatsFileIsDamaged);

//	SAVED DATA
		pBank->PushGroup("Saved Data");
			pBank->AddToggle("ms_bSaveSimpleVariables", &ms_bSaveSimpleVariables);
			pBank->AddToggle("ms_bSaveMiniMapVariables", &ms_bSaveMiniMapVariables);
			pBank->AddToggle("ms_bSavePlayerPedVariables", &ms_bSavePlayerPedVariables);
			pBank->AddToggle("ms_bSaveExtraContentVariables", &ms_bSaveExtraContentVariables);
			pBank->AddToggle("ms_bSaveGarageVariables", &ms_bSaveGarageVariables);
			pBank->AddToggle("ms_bSaveStatsVariables", &ms_bSaveStatsVariables);
			pBank->AddToggle("ms_bSaveStuntJumpVariables", &ms_bSaveStuntJumpVariables);
			pBank->AddToggle("ms_bSaveRadioStationVariables", &ms_bSaveRadioStationVariables);
			pBank->AddToggle("ms_bSavePS3AchievementVariables", &ms_bSavePS3AchievementVariables);
			pBank->AddToggle("ms_bSaveDoorVariables", &ms_bSaveDoorVariables);
			pBank->AddToggle("ms_bSaveCameraVariables", &ms_bSaveCameraVariables);
			pBank->AddToggle("ms_bSaveScriptVariables", &ms_bSaveScriptVariables);
		pBank->PopGroup();
//	END OF SAVED DATA

//	WARNING MESSAGES
		pBank->PushGroup("Savegame Warning Messages");
			pBank->AddSlider("FramesToDisplayBeforePausingTheGame", &ms_NumberOfFramesToDisplayWarningMessageForBeforePausingTheGame, 0, 100, 1);
			pBank->AddSlider("FramesToDisplayBlackScreen", &ms_NumberOfFramesToDisplayBlackScreenAfterWarningMessage, 0, 100, 1);
		pBank->PopGroup();
//	END OF WARNING MESSAGES

//	PHOTO GALLERY
		pBank->PushGroup("Photo Gallery");
//			CHighQualityScreenshot::AddWidgets(*pBank);
//			CLowQualityScreenshot::AddWidgets(*pBank);
			CPhotoManager::AddWidgets(*pBank);

			pBank->PushGroup("Save Photo");
				pBank->AddToggle("Save Photo", &ms_bSavePhoto);
				pBank->AddToggle("Photo Save In Progress (read-only)", &ms_bPhotoSaveInProgressReadOnly);
			pBank->PopGroup();
		pBank->PopGroup();
//	END OF PHOTO GALLERY

#if RSG_ORBIS
		pBank->PushGroup("Orbis System Dialog");
			pBank->AddSlider("User Index", &ms_iUserIndex, 0, 3, 1);
			pBank->AddButton("Save Dialog", CFA(ShowSaveDialog));
			pBank->AddButton("Load Dialog", CFA(ShowLoadDialog));
			pBank->AddButton("Delete Dialog", CFA(ShowDeleteDialog));
		pBank->PopGroup();
#endif // RSG_ORBIS

		CSavegameQueue::InitWidgets(pBank);
	}
}


void CGenericGameStorage::CheckForProfileWriteRequest()
{
	if (ms_bRequestProfileWrite)
	{
		CProfileSettings::GetInstance().Write(true);
		ms_bRequestProfileWrite = false;
	}
}

void CGenericGameStorage::CheckForLoadFromPC()
{
	if (ms_bLoadFromPC)
	{
//		bLoadFromPC = false;	//	This is now reset in CSaveGameData::LoadBlockData

		bool bLoadSuccessful = false;

		if (savegameVerifyf(strlen(ms_NameOfPCLoadFile) > 0, "CGenericGameStorage::CheckForLoadFromPC - Name of PC Load file has not been specified in Rag"))
		{
//			ASSET.PushFolder("common:/data");
			ASSET.PushFolder(ms_NameOfPCSaveFolder);

			FileHandle fileHandle = CFileMgr::OpenFile(ms_NameOfPCLoadFile);
			if (savegameVerifyf(CFileMgr::IsValidFileHandle(fileHandle), "CGenericGameStorage::CheckForLoadFromPC - failed to open savegame file on the PC - check that X:/gta5/build/dev/saves/%s exists", ms_NameOfPCLoadFile))
			{
				//	Get size of file
				int FileSize = CFileMgr::GetTotalSize(fileHandle);

				SetSaveOperation(OPERATION_LOADING_SAVEGAME_FROM_CONSOLE);

				//	Allocate buffer
				SetBufferSize(SAVEGAME_SINGLE_PLAYER, FileSize);
				AllocateBuffer(SAVEGAME_SINGLE_PLAYER, false);
				
				//	Load file
				CFileMgr::Read(fileHandle, (char *) GetBuffer(SAVEGAME_SINGLE_PLAYER), GetBufferSize(SAVEGAME_SINGLE_PLAYER));

				CFileMgr::CloseFile(fileHandle);
				//	The buffer will be freed in Deserialize, won't it?

//				CloseAllMenus();
//				RequestState(FRONTEND_STATE_CLOSE);  // request that the menu system is shut down

				//	Jump straight to CGameSessionStateMachine HANDLE_LOADED_SAVE_GAME
				CGame::HandleLoadedSaveGame();
				bLoadSuccessful = true;
			}

			ASSET.PopFolder();
		}

		if (!bLoadSuccessful)
		{	//	CSaveGameData::LoadBlockData will not be called so we need to reset bLoadFromPC here
			ms_bLoadFromPC = false;
		}
	}
}

void CGenericGameStorage::CheckForMpStatsSaveAndLoad()
{
	if (ms_bStartMpStatsCloudSave)
	{
		StatsInterface::Save(STATS_SAVE_CLOUD, STAT_MP_CATEGORY_DEFAULT);
		ms_bStartMpStatsCloudSave = false;
	}

// 	if (ms_bStartCheckIfMpStatsFileExists)
// 	{
// 		ms_bStartCheckIfMpStatsFileExists = false;
// 		ms_bMpStatsFileExistsCheckIsInProgress = false;
// 		ms_bMpStatsFileExists = false;
// 		ms_bMpStatsFileIsDamaged = false;
// 
// 		if (QueueCheckFileExists(CSavegameList::GetBaseIndexForSavegameFileType(SG_FILE_TYPE_MULTIPLAYER_STATS)+ms_MpSlotIndex))
// 		{
// 			ms_bMpStatsFileExistsCheckIsInProgress = true;
// 		}
// 	}

// 	if (ms_bMpStatsFileExistsCheckIsInProgress)
// 	{
// 		switch (GetCheckFileExistsStatus(ms_bMpStatsFileExists, ms_bMpStatsFileIsDamaged))
// 		{
// 			case MEM_CARD_BUSY :
// 					break;
// 
// 			case MEM_CARD_COMPLETE :
// 			case MEM_CARD_ERROR :
// 				ms_bMpStatsFileExistsCheckIsInProgress = false;
// 				break;
// 		}
// 	}

	if (ms_bStartMpStatsCloudLoad)
	{
		StatsInterface::Load(STATS_LOAD_CLOUD, -1);
		ms_bStartMpStatsCloudLoad = false;
	}

	if (ms_bLoadMpStatsFileFromPC)
	{
		ms_bLoadMpStatsFileFromPC = false;

		if (savegameVerifyf(strlen(ms_NameOfMpStatsPCLoadFile) > 0, "CGenericGameStorage::CheckForMpStatsSaveAndLoad - Name of MP Stats PC Load file has not been specified in Rag"))
		{
			ASSET.PushFolder(ms_NameOfPCSaveFolder);

			FileHandle fileHandle = CFileMgr::OpenFile(ms_NameOfMpStatsPCLoadFile);
			if (savegameVerifyf(CFileMgr::IsValidFileHandle(fileHandle), "CGenericGameStorage::CheckForMpStatsSaveAndLoad - failed to open savegame file on the PC - check that X:/gta5/build/dev_ng/saves/%s exists", ms_NameOfMpStatsPCLoadFile))
			{
				//	Get size of file
				int FileSize = CFileMgr::GetTotalSize(fileHandle);

				SetSaveOperation(OPERATION_LOADING_MPSTATS_FROM_CLOUD);

				eTypeOfSavegame savegameType = SAVEGAME_MULTIPLAYER_COMMON;
				if (ms_bMpStatsFileIsACharacterFile)
				{
					savegameType = SAVEGAME_MULTIPLAYER_CHARACTER;
				}

				//	Allocate buffer
				SetBufferSize(savegameType, FileSize);
				AllocateBuffer(savegameType, false);

				//	Load file
				CFileMgr::Read(fileHandle, (char *) GetBuffer(savegameType), GetBufferSize(savegameType));

				CFileMgr::CloseFile(fileHandle);

				if (CGenericGameStorage::LoadBlockData(savegameType, ms_bDecryptMpStatsFileOnLoading))
				{
					savegameDisplayf("CSavegameQueuedOperation_MPStats_Load::CheckForMpStatsSaveAndLoad - LoadBlockData succeeded when reading data from MP Stats file on PC");
				}
				else
				{
					savegameAssertf(0, "CSavegameQueuedOperation_MPStats_Load::CheckForMpStatsSaveAndLoad - LoadBlockData failed when reading data from MP Stats file on PC");
				}

				CGenericGameStorage::FreeBufferContainingContentsOfSavegameFile(savegameType);
				CGenericGameStorage::SetSaveOperation(OPERATION_NONE);
			}

			ASSET.PopFolder();
		}
	}
}

#endif	//	__BANK


//	Is this going to work at all?
void CGenericGameStorage::FinishAllUnfinishedSaveGameOperations()
{
	const int signInId = 0;		//	This doesn't seem right. Surely this value changes when one profile signs out and another signs in?

	savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - SAVEGAME.GetState(signInId) = %d", (s32) SAVEGAME.GetState(signInId));

	switch (SAVEGAME.GetState(signInId))
	{
		case fiSaveGameState::IDLE :
			//	Do nothing
			savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - IDLE");
			break;

		case fiSaveGameState::HAD_ERROR :
			savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - SAVEGAME.GetState() returned HAD_ERROR so I don't know which SAVEGAME.End function to call");
			savegameAssertf(0, "CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - SAVEGAME.GetState() returned HAD_ERROR so I don't know which SAVEGAME.End function to call");
			break;

		case fiSaveGameState::SELECTING_DEVICE :
			{
				savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - SELECTING_DEVICE");

				while (!SAVEGAME.CheckSelectDevice(signInId))
				{
					savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - waiting for Device Selection to complete");
					sysIpcSleep(10);
				}
				SAVEGAME.EndSelectDevice(signInId);
			}
			break;

		case fiSaveGameState::SELECTED_DEVICE :
			savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - SELECTED_DEVICE");
			SAVEGAME.EndSelectDevice(signInId);
			break;

		case fiSaveGameState::ENUMERATING_CONTENT :
			{
				savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - ENUMERATING_CONTENT");

				int NumberOfFiles = 0;
				while (!SAVEGAME.CheckEnumeration(signInId, NumberOfFiles))
				{
					savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - waiting for Enumeration to complete");
					sysIpcSleep(10);
				}
				SAVEGAME.EndEnumeration(signInId);
			}
			break;

		case fiSaveGameState::ENUMERATED_CONTENT :
			savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - ENUMERATED_CONTENT");
			SAVEGAME.EndEnumeration(signInId);
			break;

	// Ensure we aren't still saving anything
		case fiSaveGameState::SAVING_CONTENT :
		{
			savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - SAVING_CONTENT");
			bool outIsValid, fileExists;
			while(!SAVEGAME.CheckSave(signInId, outIsValid, fileExists))
			{
				savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - waiting for Save to complete");
				sysIpcSleep(10);
			}
			SAVEGAME.EndSave(signInId);
		}
		break;

		case fiSaveGameState::SAVED_CONTENT :
			savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - SAVED_CONTENT");
			SAVEGAME.EndSave(signInId);
			break;

	// Ensure we aren't still loading anything
		case fiSaveGameState::LOADING_CONTENT :
		{
			savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - LOADING_CONTENT");

			u32 SizeOfBufferToBeLoaded = 0;
			bool valid; // MIGRATE_FIXME - use for something
			while(!SAVEGAME.CheckLoad(signInId, valid, SizeOfBufferToBeLoaded))	//	returns true for error or success, false for busy
			{
				savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - waiting for Load to complete");
				sysIpcSleep(10);
			}
			SAVEGAME.EndLoad(signInId);
		}
		break;

		case fiSaveGameState::LOADED_CONTENT :
			savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - LOADED_CONTENT");
			SAVEGAME.EndLoad(signInId);
			break;

		case fiSaveGameState::SAVING_ICON :
			{
				savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - SAVING_ICON");

				while (!SAVEGAME.CheckIconSave(signInId))
				{
					savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - waiting for Icon Save to complete");
					sysIpcSleep(10);
				}
				SAVEGAME.EndIconSave(signInId);
			}
			break;

		case fiSaveGameState::SAVED_ICON :
			savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - SAVED_ICON");
			SAVEGAME.EndIconSave(signInId);
			break;

		case fiSaveGameState::VERIFYING_CONTENT :
			{
				savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - VERIFYING_CONTENT");

				bool bVerified = false;
				while (!SAVEGAME.CheckContentVerify(signInId, bVerified))
				{
					savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - waiting for ContentVerify to complete");
					sysIpcSleep(10);
				}
				SAVEGAME.EndContentVerify(signInId);
			}
			break;

		case fiSaveGameState::VERIFIED_CONTENT :
			savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - VERIFIED_CONTENT");
			SAVEGAME.EndContentVerify(signInId);
			break;

		case fiSaveGameState::DELETING_CONTENT :
			{
				savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - DELETING_CONTENT");

				while (!SAVEGAME.CheckDelete(signInId))
				{
					savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - waiting for Delete to complete");
					sysIpcSleep(10);
				}
				SAVEGAME.EndDelete(signInId);
			}
			break;

		case fiSaveGameState::DELETED_CONTENT :
			savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - DELETED_CONTENT");
			SAVEGAME.EndDelete(signInId);
			break;

		case fiSaveGameState::DELETING_CONTENT_FROM_LIST :
		case fiSaveGameState::DELETED_CONTENT_FROM_LIST :

			savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - DELETING_CONTENT_FROM_LIST or DELETED_CONTENT_FROM_LIST - should never happen");

			//	PS3 only. We never use it.
			//	bool CheckDeleteFromList(int signInId);
			//	void EndDeleteFromList(int signInId);
			break;

		case fiSaveGameState::CHECKING_FREE_SPACE :
		case fiSaveGameState::CHECKING_FREE_SPACE_NO_EXISTING_FILE :
		{
			savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - CHECKING_FREE_SPACE or CHECKING_FREE_SPACE_NO_EXISTING_FILE");

			int ExtraSpaceRequired = 0;
			while (!SAVEGAME.CheckFreeSpaceCheck(signInId, ExtraSpaceRequired))
			{
				savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - waiting for Free Space Check to complete");
				sysIpcSleep(10);
			}
			SAVEGAME.EndFreeSpaceCheck(signInId);
		}
		break;

		case fiSaveGameState::HAVE_CHECKED_FREE_SPACE :
			savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - HAVE_CHECKED_FREE_SPACE");
			SAVEGAME.EndFreeSpaceCheck(signInId);
			break;

		case fiSaveGameState::CHECKING_EXTRA_SPACE :
		case fiSaveGameState::CHECKING_EXTRA_SPACE_NO_EXISTING_FILE :
		{
			savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - CHECKING_EXTRA_SPACE or CHECKING_EXTRA_SPACE_NO_EXISTING_FILE");

			int ExtraSpaceRequiredKB = 0;
			int TotalHDDFreeSizeKB = 0;
			int SizeOfSystemFileKB = 0;
			while (!SAVEGAME.CheckExtraSpaceCheck(signInId, ExtraSpaceRequiredKB, TotalHDDFreeSizeKB, SizeOfSystemFileKB))
			{
				savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - waiting for Extra Space Check to complete");
				sysIpcSleep(10);
			}
			SAVEGAME.EndExtraSpaceCheck(signInId);
		}
		break;

		case fiSaveGameState::HAVE_CHECKED_EXTRA_SPACE :
			savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - HAVE_CHECKED_EXTRA_SPACE");
			SAVEGAME.EndExtraSpaceCheck(signInId);
			break;

		case fiSaveGameState::READING_NAMES_AND_SIZES_OF_ALL_SAVE_GAMES :
		{
			savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - READING_NAMES_AND_SIZES_OF_ALL_SAVE_GAMES");

			int NumberOfFiles = 0;
			while (!SAVEGAME.CheckReadNamesAndSizesOfAllSaveGames(signInId, NumberOfFiles))
			{
				savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - waiting for ReadNamesAndSizes to complete");
				sysIpcSleep(10);
			}
			SAVEGAME.EndReadNamesAndSizesOfAllSaveGames(signInId);
		}
		break;

		case fiSaveGameState::HAVE_READ_NAMES_AND_SIZES_OF_ALL_SAVE_GAMES :
			savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - HAVE_READ_NAMES_AND_SIZES_OF_ALL_SAVE_GAMES");
			SAVEGAME.EndReadNamesAndSizesOfAllSaveGames(signInId);
			break;

		case fiSaveGameState::GETTING_CREATOR :
		{
			savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - GETTING_CREATOR");

			bool bIsCreator = false;
	#if __PPU && HACK_GTA4
			bool bHasCreator = false;
			while (!SAVEGAME.CheckGetCreator(signInId, bIsCreator, bHasCreator))
	#else
			while (!SAVEGAME.CheckGetCreator(signInId, bIsCreator))
	#endif
			{
				savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - waiting for GetCreator to complete");
				sysIpcSleep(10);
			}
			SAVEGAME.EndGetCreator(signInId);
		}
		break;

		case fiSaveGameState::GOT_CREATOR :
			savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - GOT_CREATOR");
			SAVEGAME.EndGetCreator(signInId);
			break;

		case fiSaveGameState::GETTING_FILE_SIZE :
		{
			savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - GETTING_FILE_SIZE");

			u64 fileSize = 0;
			while (!SAVEGAME.CheckGetFileSize(signInId, fileSize))
			{
				savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - waiting for GetFileSize to complete");
				sysIpcSleep(10);
			}
			SAVEGAME.EndGetFileSize(signInId);
		}
		break;

		case fiSaveGameState::HAVE_GOT_FILE_SIZE :
			savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - HAVE_GOT_FILE_SIZE");
			SAVEGAME.EndGetFileSize(signInId);
			break;

		case fiSaveGameState::GETTING_FILE_MODIFIED_TIME :
		{
			savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - GETTING_FILE_MODIFIED_TIME");

			u32 modTimeHigh = 0;
			u32 modTimeLow = 0;
			while (!SAVEGAME.CheckGetFileModifiedTime(signInId, modTimeHigh, modTimeLow))
			{
				savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - waiting for GetFileModifiedTime to complete");
				sysIpcSleep(10);
			}
			SAVEGAME.EndGetFileModifiedTime(signInId);
		}
		break;

		case fiSaveGameState::HAVE_GOT_FILE_MODIFIED_TIME :
			savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - HAVE_GOT_FILE_MODIFIED_TIME");
			SAVEGAME.EndGetFileModifiedTime(signInId);
			break;
		case fiSaveGameState::GETTING_FILE_EXISTS :
			{
				savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - GETTING_FILE_EXISTS");

				bool exists = 0;
				while (!SAVEGAME.CheckGetFileExists(signInId, exists))
				{
					savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - waiting for GetFileModifiedTime to complete");
					sysIpcSleep(10);
				}
				SAVEGAME.EndGetFileExists(signInId);
			}
			break;

		case fiSaveGameState::HAVE_GOT_FILE_EXISTS :
			savegameDisplayf("CGenericGameStorage::FinishAllUnfinishedSaveGameOperations - HAVE_GOT_FILE_EXISTS");
			SAVEGAME.EndGetFileExists(signInId);
			break;
	}

	SAVEGAME.SetStateToIdle(signInId);	//	CSavegameUsers::GetUser());
}

void CGenericGameStorage::SetSaveOperation(eSaveOperation Operation)
{
	if (OPERATION_NONE != Operation)
	{
		savegameAssertf( (OPERATION_NONE == ms_SavegameOperation) || (ms_SavegameOperation == Operation), "CGenericGameStorage::SetSaveOperation - setting ms_SavegameOperation when it already has a different value");
	}
	ms_SavegameOperation = Operation;
}

// this is needed to disable all frontend controls (including ESC to enter/exit the frontend)
// maybe needed for other things too
// Maybe don't allow save game bed to trigger until this returns false
bool CGenericGameStorage::IsStorageDeviceBeingAccessed()
{
	if (OPERATION_NONE == ms_SavegameOperation)
	{
		return false;
	}

	return true;
}

/*
bool CGenericGameStorage::IsSaveInProgress(bool bCheckForGameOnly)
{
	if (!bCheckForGameOnly)
	{
		if (OPERATION_SAVING_BLOCK == ms_SavegameOperation)
		{
			return true;
		}
	}

	//	Could OPERATION_SAVING_CLOUD refer to any cloud save? Not just a savegame? It could be MPStats or a photo?

	return ((OPERATION_SAVING_CLOUD == ms_SavegameOperation) 
				|| (OPERATION_SAVING == ms_SavegameOperation) 
				|| (OPERATION_AUTOSAVING == ms_SavegameOperation));	//	Should this also check OPERATION_SAVING_BLOCK?
}
*/

bool CGenericGameStorage::IsSavegameSaveInProgress()
{
	return ( (OPERATION_SAVING_SAVEGAME_TO_CONSOLE == ms_SavegameOperation) 
		|| (OPERATION_SAVING_MISSION_REPEAT_SAVEGAME_TO_CONSOLE == ms_SavegameOperation) 
		|| (OPERATION_SAVING_REPLAY_SAVEGAME_TO_CONSOLE == ms_SavegameOperation)
		|| (OPERATION_AUTOSAVING == ms_SavegameOperation)
#if __ALLOW_LOCAL_MP_STATS_SAVES
		|| (OPERATION_SAVING_MPSTATS_SAVEGAME_TO_CONSOLE == ms_SavegameOperation)
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES
		);
}

bool CGenericGameStorage::IsMpStatsSaveInProgress()
{
	return ( (OPERATION_SAVING_MPSTATS_TO_CLOUD == ms_SavegameOperation)
#if __ALLOW_LOCAL_MP_STATS_SAVES
		|| (OPERATION_SAVING_MPSTATS_SAVEGAME_TO_CONSOLE == ms_SavegameOperation)
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES
		);
}


bool CGenericGameStorage::IsMpStatsSaveAtTopOfSavegameQueue()
{
	bool bAtTopOfQueue = false;
	if (CGenericGameStorage::ExistsInQueue(&sm_MPStatsSaveQueuedOperation, bAtTopOfQueue))
	{
		if (bAtTopOfQueue)
		{
			return true;
		}
	}

	return false;
}


bool CGenericGameStorage::IsPhotoSaveInProgress()
{
	return ( (OPERATION_SAVING_PHOTO_TO_CLOUD == ms_SavegameOperation) || (OPERATION_SAVING_LOCAL_PHOTO == ms_SavegameOperation) );
}

#if __ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
bool CGenericGameStorage::IsUploadingSavegameToCloud()
{
	return (OPERATION_UPLOADING_SAVEGAME_TO_CLOUD == ms_SavegameOperation);
}
#endif	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME


/*
bool CGenericGameStorage::IsLoadInProgress(bool bCheckForGameOnly)
{
	if (!bCheckForGameOnly)
	{
		if (OPERATION_LOADING_BLOCK == ms_SavegameOperation)
		{
			return true;
		}
	}

	//	Could OPERATION_LOADING_CLOUD refer to any cloud load? Not just a savegame? It could be MPStats or a photo?

	return ((OPERATION_LOADING_CLOUD == ms_SavegameOperation) 
				|| (OPERATION_LOADING_SAVEGAME_FROM_CONSOLE == ms_SavegameOperation));	//	Should this also check OPERATION_LOADING_BLOCK?
}
*/

bool CGenericGameStorage::IsSavegameLoadInProgress()
{
	//	 || (OPERATION_LOADING_MPSTATS_SAVEGAME_FROM_CONSOLE == ms_SavegameOperation) 
	return (OPERATION_LOADING_SAVEGAME_FROM_CONSOLE == ms_SavegameOperation) || (OPERATION_LOADING_REPLAY_SAVEGAME_FROM_CONSOLE == ms_SavegameOperation);
}

bool CGenericGameStorage::IsMpStatsLoadInProgress()
{
	return ( (OPERATION_LOADING_MPSTATS_FROM_CLOUD == ms_SavegameOperation)
#if __ALLOW_LOCAL_MP_STATS_SAVES
		|| (OPERATION_LOADING_MPSTATS_SAVEGAME_FROM_CONSOLE == ms_SavegameOperation)
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES
		);
}

bool CGenericGameStorage::IsMpStatsLoadAtTopOfSavegameQueue()
{
	bool bAtTopOfQueue = false;
	if (CGenericGameStorage::ExistsInQueue(&sm_MPStatsLoadQueuedOperation, bAtTopOfQueue))
	{
		if (bAtTopOfQueue)
		{
			return true;
		}
	}

	return false;
}


#if USE_SAVE_DATA_MEMORY && !USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP
bool CGenericGameStorage::IsAutosaveBackupAtTopOfSavegameQueue()
{
	bool bAtTopOfQueue = false;
	if (CGenericGameStorage::ExistsInQueue(&ms_CreateBackupOfAutosaveSDM, bAtTopOfQueue))
	{
		if (bAtTopOfQueue)
		{
			return true;
		}
	}

	return false;
}
#endif	//	USE_SAVE_DATA_MEMORY && !USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP


bool CGenericGameStorage::IsPhotoLoadInProgress()
{
	return ( (OPERATION_LOADING_PHOTO_FROM_CLOUD == ms_SavegameOperation) || (OPERATION_LOADING_LOCAL_PHOTO == ms_SavegameOperation) );
}


bool CGenericGameStorage::IsLocalDeleteInProgress()
{
	return (OPERATION_DELETING_LOCAL == ms_SavegameOperation);
}

bool CGenericGameStorage::IsCloudDeleteInProgress()
{
	return (OPERATION_DELETING_CLOUD == ms_SavegameOperation);
}

/*
void CGenericGameStorage::ClearBufferToStoreFirstFewBytesOfSaveFile()
{
	ms_SaveGameBuffer.ClearBufferToStoreFirstFewBytesOfSaveFile();
}

int CGenericGameStorage::GetVersionNumberFromStartOfSaveFile()
{
	return ms_SaveGameBuffer.GetVersionNumberFromStartOfSaveFile();
}

bool CGenericGameStorage::CheckSaveStringAtStartOfSaveFile()
{
	return ms_SaveGameBuffer.CheckSaveStringAtStartOfSaveFile();
}
*/


CSaveGameBuffer *CGenericGameStorage::GetSavegameBuffer(eTypeOfSavegame SavegameType)
{
	if (SAVEGAME_MULTIPLAYER_CHARACTER == SavegameType)
	{
		switch (ms_SavegameOperation)
		{
			case OPERATION_LOADING_MPSTATS_FROM_CLOUD :
			case OPERATION_SAVING_MPSTATS_TO_CLOUD :
				return &ms_SaveGameBuffer_MultiplayerCharacter_Cloud;

#if __ALLOW_LOCAL_MP_STATS_SAVES
			case OPERATION_LOADING_MPSTATS_SAVEGAME_FROM_CONSOLE :
			case OPERATION_SAVING_MPSTATS_SAVEGAME_TO_CONSOLE :
#if RSG_ORBIS
			case OPERATION_CHECKING_FOR_FREE_SPACE_AT_START_OF_GAME :
#endif	//	__PPU
				return &ms_SaveGameBuffer_MultiplayerCharacter_Local;
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES

			default :
				savegameAssertf(0, "CGenericGameStorage::GetSavegameBuffer - SAVEGAME_MULTIPLAYER_CHARACTER - expected savegame operation to be loading/saving MP Stats to/from cloud/console but it's %d", ms_SavegameOperation);
				return &ms_SaveGameBuffer_MultiplayerCharacter_Cloud;	//	I'll return this but maybe NULL would be more correct
				break;
		}
	}
	else if (SAVEGAME_MULTIPLAYER_COMMON == SavegameType)
	{
		switch (ms_SavegameOperation)
		{
		case OPERATION_LOADING_MPSTATS_FROM_CLOUD :
		case OPERATION_SAVING_MPSTATS_TO_CLOUD :
			return &ms_SaveGameBuffer_MultiplayerCommon_Cloud;

#if __ALLOW_LOCAL_MP_STATS_SAVES
		case OPERATION_LOADING_MPSTATS_SAVEGAME_FROM_CONSOLE :
		case OPERATION_SAVING_MPSTATS_SAVEGAME_TO_CONSOLE :
			return &ms_SaveGameBuffer_MultiplayerCommon_Local;
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES

		default :
			savegameAssertf(0, "CGenericGameStorage::GetSavegameBuffer - SAVEGAME_MULTIPLAYER_COMMON - expected savegame operation to be loading/saving MP Stats to/from cloud/console but it's %d", ms_SavegameOperation);
			return &ms_SaveGameBuffer_MultiplayerCommon_Cloud;	//	I'll return this but maybe NULL would be more correct
			break;
		}
	}

	return &ms_SaveGameBuffer_SinglePlayer;
}

bool CGenericGameStorage::LoadBlockData(eTypeOfSavegame SavegameType, const bool useEncryption)
{
	return GetSavegameBuffer(SavegameType)->LoadBlockData(useEncryption);
}

bool CGenericGameStorage::BeginSaveBlockData(eTypeOfSavegame SavegameType, const bool useEncryption)
{
	return GetSavegameBuffer(SavegameType)->BeginSaveBlockData(useEncryption);
}

void CGenericGameStorage::EndSaveBlockData(eTypeOfSavegame SavegameType)
{
	GetSavegameBuffer(SavegameType)->EndSaveBlockData();
}

SaveBlockDataStatus CGenericGameStorage::GetSaveBlockDataStatus(eTypeOfSavegame SavegameType)
{
	return GetSavegameBuffer(SavegameType)->GetSaveBlockDataStatus();
}


MemoryCardError CGenericGameStorage::CreateSaveDataAndCalculateSize(eTypeOfSavegame SavegameType, bool bIsAnAutosave)
{
	if (SAVEGAME_SINGLE_PLAYER == SavegameType)
	{
		CSavegameFilenames::SetCompletionPercentageForSinglePlayerDisplayName();

		ms_bClosestSaveHouseDataIsValid = FindClosestSaveHouse(bIsAnAutosave, ms_vCoordsOfClosestSaveHouse, ms_fHeadingOfClosestSaveHouse);

		// Was the savehouse overridden, meaning this could be a quick-save?
		ms_bPlayerShouldSnapToGroundOnSpawn = true;
		if(CRestart::ms_bOverrideSavehouse)
		{
			// We're not spawning into a safehouse, determine if we need to snap to ground when spawning upon load
			ms_bPlayerShouldSnapToGroundOnSpawn = DetermineShouldSnapToGround();
		}
	}
	return GetSavegameBuffer(SavegameType)->CreateSaveDataAndCalculateSize();
}

void  CGenericGameStorage::SetBuffer(u8* buffer, const u32 bufferSize, eTypeOfSavegame SavegameType)
{
	//Just now only the operation OPERATION_LOADING_MPSTATS_FROM_CLOUD manages its own memory.
	if (savegameVerifyf(ms_SavegameOperation == OPERATION_LOADING_MPSTATS_FROM_CLOUD, "CGenericGameStorage::SetBuffer - Invalid Operation in progress = %d", ms_SavegameOperation))
	{
		if (SAVEGAME_SINGLE_PLAYER == SavegameType)
		{
			savegameAssertf((ms_SaveGameBuffer_SinglePlayer.GetBuffer() && !buffer) 
								|| (!ms_SaveGameBuffer_SinglePlayer.GetBuffer() && buffer), "Invalid buffers");

			ms_SaveGameBuffer_SinglePlayer.SetBufferSize(bufferSize);
			ms_SaveGameBuffer_SinglePlayer.SetBuffer(buffer);
		}
		else if(SAVEGAME_MULTIPLAYER_CHARACTER == SavegameType)
		{
			CSaveGameBuffer_MultiplayerCharacter &SaveGameBuffer_MultiplayerCharacter = ms_SaveGameBuffer_MultiplayerCharacter_Cloud;
			switch (ms_SavegameOperation)
			{
			case OPERATION_LOADING_MPSTATS_FROM_CLOUD :
			case OPERATION_SAVING_MPSTATS_TO_CLOUD :
				break;

#if __ALLOW_LOCAL_MP_STATS_SAVES
			case OPERATION_LOADING_MPSTATS_SAVEGAME_FROM_CONSOLE :
			case OPERATION_SAVING_MPSTATS_SAVEGAME_TO_CONSOLE :
				SaveGameBuffer_MultiplayerCharacter = ms_SaveGameBuffer_MultiplayerCharacter_Local;
				break;
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES

			default :
				savegameAssertf(0, "CGenericGameStorage::SetBuffer - SAVEGAME_MULTIPLAYER_CHARACTER - expected savegame operation to be loading/saving MP Stats to/from cloud/console but it's %d", ms_SavegameOperation);
				break;
			}


			savegameAssertf((SaveGameBuffer_MultiplayerCharacter.GetBuffer() && !buffer) 
								|| (!SaveGameBuffer_MultiplayerCharacter.GetBuffer() && buffer), "Invalid buffers");

			SaveGameBuffer_MultiplayerCharacter.SetBufferSize(bufferSize);
			SaveGameBuffer_MultiplayerCharacter.SetBuffer(buffer);
		}
		else if (SAVEGAME_MULTIPLAYER_COMMON == SavegameType)
		{
			CSaveGameBuffer_MultiplayerCommon &SaveGameBuffer_MultiplayerCommon = ms_SaveGameBuffer_MultiplayerCommon_Cloud;
			switch (ms_SavegameOperation)
			{
			case OPERATION_LOADING_MPSTATS_FROM_CLOUD :
			case OPERATION_SAVING_MPSTATS_TO_CLOUD :
				break;

#if __ALLOW_LOCAL_MP_STATS_SAVES
			case OPERATION_LOADING_MPSTATS_SAVEGAME_FROM_CONSOLE :
			case OPERATION_SAVING_MPSTATS_SAVEGAME_TO_CONSOLE :
				SaveGameBuffer_MultiplayerCommon = ms_SaveGameBuffer_MultiplayerCommon_Local;
				break;
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES

			default :
				savegameAssertf(0, "CGenericGameStorage::SetBuffer - SAVEGAME_MULTIPLAYER_COMMON - expected savegame operation to be loading/saving MP Stats to/from cloud/console but it's %d", ms_SavegameOperation);
				break;
			}

			savegameAssertf((SaveGameBuffer_MultiplayerCommon.GetBuffer() && !buffer) 
				|| (!SaveGameBuffer_MultiplayerCommon.GetBuffer() && buffer), "Invalid buffers");

			SaveGameBuffer_MultiplayerCommon.SetBufferSize(bufferSize);
			SaveGameBuffer_MultiplayerCommon.SetBuffer(buffer);
		}
	}
}

MemoryCardError CGenericGameStorage::AllocateBuffer(eTypeOfSavegame SavegameType, bool bSaving)
{
	if (bSaving)
	{
		if (SAVEGAME_MULTIPLAYER_CHARACTER == SavegameType)
		{
//	The following Verify was happening when trying to save the MP Stats file so I've commented out until I have more time to look at it
//			if (savegameVerifyf(ms_SavegameOperation == OPERATION_SAVING_BLOCK, "CGenericGameStorage::AllocateBuffer - No block save is in progress"))
			{
				switch (ms_SavegameOperation)
				{
//				case OPERATION_LOADING_MPSTATS_FROM_CLOUD :
				case OPERATION_SAVING_MPSTATS_TO_CLOUD :
					return ms_SaveGameBuffer_MultiplayerCharacter_Cloud.AllocateDataBuffer();
//					break;

#if __ALLOW_LOCAL_MP_STATS_SAVES
//				case OPERATION_LOADING_MPSTATS_SAVEGAME_FROM_CONSOLE :
				case OPERATION_SAVING_MPSTATS_SAVEGAME_TO_CONSOLE :
					return ms_SaveGameBuffer_MultiplayerCharacter_Local.AllocateDataBuffer();
//					break;
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES

				default :
					savegameAssertf(0, "CGenericGameStorage::AllocateBuffer - SAVEGAME_MULTIPLAYER_CHARACTER - Saving - expected savegame operation to be loading/saving MP Stats to/from cloud/console but it's %d", ms_SavegameOperation);
					return MEM_CARD_ERROR;
//					break;
				}
			}
		}
		else if (SAVEGAME_MULTIPLAYER_COMMON == SavegameType)
		{
//	The following Verify was happening when trying to save the MP Stats file so I've commented out until I have more time to look at it
//			if (savegameVerifyf(ms_SavegameOperation == OPERATION_SAVING_BLOCK, "CGenericGameStorage::AllocateBuffer - No block save is in progress"))
			{
				switch (ms_SavegameOperation)
				{
//				case OPERATION_LOADING_MPSTATS_FROM_CLOUD :
				case OPERATION_SAVING_MPSTATS_TO_CLOUD :
					return ms_SaveGameBuffer_MultiplayerCommon_Cloud.AllocateDataBuffer();
//					break;

#if __ALLOW_LOCAL_MP_STATS_SAVES
//				case OPERATION_LOADING_MPSTATS_SAVEGAME_FROM_CONSOLE :
				case OPERATION_SAVING_MPSTATS_SAVEGAME_TO_CONSOLE :
					return ms_SaveGameBuffer_MultiplayerCommon_Local.AllocateDataBuffer();
//					break;
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES

				default :
					savegameAssertf(0, "CGenericGameStorage::AllocateBuffer - SAVEGAME_MULTIPLAYER_COMMON - Saving - expected savegame operation to be loading/saving MP Stats to/from cloud/console but it's %d", ms_SavegameOperation);
					return MEM_CARD_ERROR;
//					break;
				}
			}
		}
		else
		{
			if (savegameVerifyf( (ms_SavegameOperation == OPERATION_SAVING_SAVEGAME_TO_CONSOLE) || (ms_SavegameOperation == OPERATION_SAVING_MISSION_REPEAT_SAVEGAME_TO_CONSOLE)
				|| (ms_SavegameOperation == OPERATION_SAVING_REPLAY_SAVEGAME_TO_CONSOLE) || (ms_SavegameOperation == OPERATION_AUTOSAVING), "CGenericGameStorage::AllocateBuffer - No save game is in progress"))
			{
				return ms_SaveGameBuffer_SinglePlayer.AllocateDataBuffer();
			}
		}
	}
	else
	{
		if (SAVEGAME_MULTIPLAYER_CHARACTER == SavegameType)
		{
//			if (savegameVerifyf(ms_SavegameOperation == OPERATION_LOADING_BLOCK, "CGenericGameStorage::AllocateBuffer - No block load is in progress"))
			{
				switch (ms_SavegameOperation)
				{
				case OPERATION_LOADING_MPSTATS_FROM_CLOUD :
//				case OPERATION_SAVING_MPSTATS_TO_CLOUD :
					return ms_SaveGameBuffer_MultiplayerCharacter_Cloud.AllocateDataBuffer();
//					break;

#if __ALLOW_LOCAL_MP_STATS_SAVES
				case OPERATION_LOADING_MPSTATS_SAVEGAME_FROM_CONSOLE :
//				case OPERATION_SAVING_MPSTATS_SAVEGAME_TO_CONSOLE :
					return ms_SaveGameBuffer_MultiplayerCharacter_Local.AllocateDataBuffer();
//					break;
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES

				default :
					savegameAssertf(0, "CGenericGameStorage::AllocateBuffer - SAVEGAME_MULTIPLAYER_CHARACTER - Loading - expected savegame operation to be loading/saving MP Stats to/from cloud/console but it's %d", ms_SavegameOperation);
					return MEM_CARD_ERROR;
//					break;
				}
			}
		}
		else if (SAVEGAME_MULTIPLAYER_COMMON == SavegameType)
		{
//			if (savegameVerifyf(ms_SavegameOperation == OPERATION_LOADING_BLOCK, "CGenericGameStorage::AllocateBuffer - No block load is in progress"))
			{
				switch (ms_SavegameOperation)
				{
				case OPERATION_LOADING_MPSTATS_FROM_CLOUD :
//				case OPERATION_SAVING_MPSTATS_TO_CLOUD :
					return ms_SaveGameBuffer_MultiplayerCommon_Cloud.AllocateDataBuffer();
//					break;

#if __ALLOW_LOCAL_MP_STATS_SAVES
				case OPERATION_LOADING_MPSTATS_SAVEGAME_FROM_CONSOLE :
//				case OPERATION_SAVING_MPSTATS_SAVEGAME_TO_CONSOLE :
					return ms_SaveGameBuffer_MultiplayerCommon_Local.AllocateDataBuffer();
//					break;
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES

				default :
					savegameAssertf(0, "CGenericGameStorage::AllocateBuffer - SAVEGAME_MULTIPLAYER_COMMON - Loading - expected savegame operation to be loading/saving MP Stats to/from cloud/console but it's %d", ms_SavegameOperation);
					return MEM_CARD_ERROR;
//					break;
				}
			}
		}
		else
		{
			if (savegameVerifyf( (ms_SavegameOperation == OPERATION_LOADING_SAVEGAME_FROM_CONSOLE) || (ms_SavegameOperation == OPERATION_LOADING_REPLAY_SAVEGAME_FROM_CONSOLE), "CGenericGameStorage::AllocateBuffer - No load game is in progress"))
			{
				return ms_SaveGameBuffer_SinglePlayer.AllocateDataBuffer();
			}
		}
	}

	return MEM_CARD_ERROR;
}

void CGenericGameStorage::FreeBufferContainingContentsOfSavegameFile(eTypeOfSavegame SavegameType)
{
	GetSavegameBuffer(SavegameType)->FreeBufferContainingContentsOfSavegameFile();
}

void CGenericGameStorage::FreeAllDataToBeSaved(eTypeOfSavegame SavegameType)
{
	GetSavegameBuffer(SavegameType)->FreeAllDataToBeSaved();
}

u32 CGenericGameStorage::GetBufferSize(eTypeOfSavegame SavegameType)
{
	return (GetSavegameBuffer(SavegameType)->GetBufferSize());
}

u8 *CGenericGameStorage::GetBuffer(eTypeOfSavegame SavegameType)
{
	if (SAVEGAME_MULTIPLAYER_CHARACTER == SavegameType)
	{
//	The following Verify was happening when trying to save the MP Stats file so I've commented out until I have more time to look at it
//		if (savegameVerifyf( ( (ms_SavegameOperation == OPERATION_SAVING_BLOCK) || (ms_SavegameOperation == OPERATION_LOADING_BLOCK) ), "CGenericGameStorage::GetBuffer - Neither loading/saving a block of data"))
		{
			switch (ms_SavegameOperation)
			{
				case OPERATION_LOADING_MPSTATS_FROM_CLOUD :
				case OPERATION_SAVING_MPSTATS_TO_CLOUD :
					return ms_SaveGameBuffer_MultiplayerCharacter_Cloud.GetBuffer();
//					break;

#if __ALLOW_LOCAL_MP_STATS_SAVES
				case OPERATION_LOADING_MPSTATS_SAVEGAME_FROM_CONSOLE :
				case OPERATION_SAVING_MPSTATS_SAVEGAME_TO_CONSOLE :
					return ms_SaveGameBuffer_MultiplayerCharacter_Local.GetBuffer();
//					break;
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES

			default :
					savegameAssertf(0, "CGenericGameStorage::GetBuffer - SAVEGAME_MULTIPLAYER_CHARACTER - expected savegame operation to be loading/saving MP Stats to/from cloud/console but it's %d", ms_SavegameOperation);
					return NULL;
//					break;
			}
		}
	}
	else if (SAVEGAME_MULTIPLAYER_COMMON == SavegameType)
	{
		//	The following Verify was happening when trying to save the MP Stats file so I've commented out until I have more time to look at it
		//		if (savegameVerifyf( ( (ms_SavegameOperation == OPERATION_SAVING_BLOCK) || (ms_SavegameOperation == OPERATION_LOADING_BLOCK) ), "CGenericGameStorage::GetBuffer - Neither loading/saving a block of data"))
		{
			switch (ms_SavegameOperation)
			{
				case OPERATION_LOADING_MPSTATS_FROM_CLOUD :
				case OPERATION_SAVING_MPSTATS_TO_CLOUD :
					return ms_SaveGameBuffer_MultiplayerCommon_Cloud.GetBuffer();
//					break;

#if __ALLOW_LOCAL_MP_STATS_SAVES
				case OPERATION_LOADING_MPSTATS_SAVEGAME_FROM_CONSOLE :
				case OPERATION_SAVING_MPSTATS_SAVEGAME_TO_CONSOLE :
					return ms_SaveGameBuffer_MultiplayerCommon_Local.GetBuffer();
//					break;
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES

				default :
					savegameAssertf(0, "CGenericGameStorage::GetBuffer - SAVEGAME_MULTIPLAYER_COMMON - expected savegame operation to be loading/saving MP Stats to/from cloud/console but it's %d", ms_SavegameOperation);
					return NULL;
//					break;
			}
		}
	}
	else
	{
		return (ms_SaveGameBuffer_SinglePlayer.GetBuffer());
	}
}

void CGenericGameStorage::SetBufferSize(eTypeOfSavegame SavegameType, u32 BufferSize)
{
	GetSavegameBuffer(SavegameType)->SetBufferSize(BufferSize);
}

u32 CGenericGameStorage::GetMaxBufferSize(eTypeOfSavegame SavegameType)
{
	return GetSavegameBuffer(SavegameType)->GetMaxBufferSize();
}

bool CGenericGameStorage::ShouldWeDisplayTheMessageAboutOverwritingTheAutosaveSlot()
{
	// if this is the first autosave of a new session (either new game or after loading a saved game)
	//	then check if the slot already contains a save
	//	If it was the autosave slot that was loaded then maybe don't bother checking this

	if (ms_bDisplayAutosaveOverwriteMessage)
	{
		if (!CSavegameList::IsSaveGameSlotEmpty(CSavegameList::GetAutosaveSlotNumberForCurrentEpisode()))
		{
			return true;
		}
	}

	return false;
}


void CGenericGameStorage::SetFlagForAutosaveOverwriteMessage(bool bDisplayAutosaveOverwrite)
{
	ms_bDisplayAutosaveOverwriteMessage = bDisplayAutosaveOverwrite;
}


void CGenericGameStorage::DoGameSpecificStuffAfterSuccessLoad(bool bLoadingFromStorageDevice)
{
	CHudTools::SetAsRestarted(true);
	PostFX::ResetAdaptedLuminance();
#if ENABLE_FOG_OF_WAR
	CMiniMap::SetRequestFoWClear(false);
	CMiniMap::SetFoWMapValid(true);
#if __D3D11
	CMiniMap::SetUploadFoWTextureData(true);
#endif // __D3D11
#endif // ENABLE_FOG_OF_WAR

	if (PARAM_doWarpAfterLoading.Get())
	{
		float fNewPlayerHeading = 0.0f;
		Vector3 vecNewPlayerPos;

		if (savegameVerifyf(ms_bClosestSaveHouseDataIsValid, "CGenericGameStorage::DoGameSpecificStuffAfterSuccessLoad - ClosestSaveHouse variables have not been set"))
		{
			vecNewPlayerPos = ms_vCoordsOfClosestSaveHouse;
			//	might need to set camera behind player after setting his heading
			fNewPlayerHeading = ( DtoR * ms_fHeadingOfClosestSaveHouse);
			fNewPlayerHeading = fwAngle::LimitRadianAngleSafe(fNewPlayerHeading);
		}

		CPed * pPlayerPed = CGameWorld::FindLocalPlayer();
		Assertf(pPlayerPed, "CGenericGameStorage::DoGameSpecificStuffAfterSuccessLoad - local player doesn't exist");
		if (pPlayerPed)
		{
			if (ms_bClosestSaveHouseDataIsValid)
			{
				CScriptPeds::SetPedCoordinates(pPlayerPed, vecNewPlayerPos.x, vecNewPlayerPos.y, vecNewPlayerPos.z, CScriptPeds::SetPedCoordFlag_OffsetZ);	//	false);
				pPlayerPed->SetDesiredHeading(fNewPlayerHeading);
				pPlayerPed->SetHeading(fNewPlayerHeading);
			}
			else
			{
				vecNewPlayerPos = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition());
			}

			// Do disk cache schedule before load scene
			CGame::DiskCacheSchedule();

			Vector3 vecVel(VEC3_ZERO);
			CWarpManager::SetWarp(vecNewPlayerPos, vecVel, fNewPlayerHeading, true, ms_bPlayerShouldSnapToGroundOnSpawn, 200.f);

			if (ms_bFadeInAfterSuccessfulLoad)
			{
				CWarpManager::FadeInWhenComplete(true);
			}
			else
			{
				CWarpManager::FadeInWhenComplete(false);
			}
		}
	}


	if (bLoadingFromStorageDevice)
	{
		//	Only do this for a proper load. For a network restore load, the stat will be reloaded
		//	with the value it had when the single player game was saved.

		StatId stat = StatsInterface::GetStatsModelHashId("KILLS_SINCE_SAFEHOUSE_VISIT");
		StatsInterface::SetStatData(stat, 0);

		if (CSavegameFilenames::IsThisTheNameOfAnAutosaveFile(CSavegameFilenames::GetFilenameOfLocalFile())	// can't use the slot number directly as for autoload this could be much larger than 15 and the slots won't have been sorted
#if USE_SAVE_DATA_MEMORY || USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP
			|| CSavegameFilenames::IsThisTheNameOfAnAutosaveBackupFile(CSavegameFilenames::GetFilenameOfLocalFile())
#endif	//	USE_SAVE_DATA_MEMORY || USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP
			)
		{
			SetFlagForAutosaveOverwriteMessage(false);
		}
	}
}

bool CGenericGameStorage::DetermineShouldSnapToGround()	
{
	CPed * pPlayerPed = CGameWorld::FindLocalPlayer();
	Assertf(pPlayerPed, "CGenericGameStorage::DetermineShouldSnapToGround - local player doesn't exist");
	if (pPlayerPed)
	{
		// If we're not on the ground or are swimming, we need to disable the "snap to ground" functionality on post load player spawn
		if(pPlayerPed->GetIsSwimming() || !pPlayerPed->IsOnGround() )
		{
			return false;
		}
	}
	return true;
}

bool CGenericGameStorage::FindClosestSaveHouse(bool bThisIsAnAutosave, Vector3 &vReturnCoords, float &fReturnHeading)
{
	if (CRestart::ms_bOverrideSavehouse)
	{
		vReturnCoords = CRestart::ms_vCoordsOfSavehouseOverride;
		fReturnHeading = CRestart::ms_fHeadingOfSavehouseOverride;

		return true;
	}


	Vector2 vecPlayerCoords2d = Vector2(0.0f, 0.0f);
	u32 currentPlayerNameHash = 0;
	CPed * pPlayerPed = CGameWorld::FindLocalPlayer();
	Assertf(pPlayerPed, "CGenericGameStorage::FindClosestSaveHouse - local player doesn't exist");
	if (pPlayerPed)
	{
		const Vector3 vPlayerPosition = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition());
		vecPlayerCoords2d = Vector2(vPlayerPosition.x, vPlayerPosition.y);

		currentPlayerNameHash = pPlayerPed->GetBaseModelInfo()->GetModelNameHash();
	}

	s32 indexOfClosestSaveHouse = -1;

	float fDistanceToClosestSaveHouseSqr = FLT_MAX;
	Assertf(CRestart::NumberOfSaveHouses > 0, "CGenericGameStorage::FindClosestSaveHouse - no save houses have been registered");
	s32 loop = 0;
	while (loop < CRestart::NumberOfSaveHouses)
	{
		if (CRestart::SaveHouses[loop].m_bEnabled)
		{
			if ( (!bThisIsAnAutosave) || (CRestart::SaveHouses[loop].m_bAvailableForAutosaves) )
			{
				if ( (CRestart::SaveHouses[loop].m_PlayerModelNameHash == 0)
					|| (CRestart::SaveHouses[loop].m_PlayerModelNameHash == currentPlayerNameHash) )
				{
					Vector2 vecSaveHouse2d = Vector2(CRestart::SaveHouses[loop].Coords.x, CRestart::SaveHouses[loop].Coords.y);
					Vector2 vecDiff2D = vecPlayerCoords2d - vecSaveHouse2d;
					float fDistanceToSaveHouseSqr = vecDiff2D.Mag2();

					if (fDistanceToSaveHouseSqr < fDistanceToClosestSaveHouseSqr)
					{
						fDistanceToClosestSaveHouseSqr = fDistanceToSaveHouseSqr;
						indexOfClosestSaveHouse = loop;
					}
				}
			}
		}
		loop++;
	}

#if GTA_REPLAY && __ASSERT
	//this is fine when playing the game normally, but if your debugging in free roam mode this assert fires while saving before going into a replay
	if(indexOfClosestSaveHouse == -1 && CReplayMgr::IsEnabled() && CReplayMgr::IsEditModeActive() == false)
	{
		indexOfClosestSaveHouse = 6;
	}
#endif

	if (savegameVerifyf(indexOfClosestSaveHouse != -1, "CGenericGameStorage::FindClosestSaveHouse - failed to find an open savehouse for current player. Check that you've started the single player mission flow. If you're running Debug scripts and you know that you've not started the mission flow, ignore this assert"))
	{
		if (savegameVerifyf( (indexOfClosestSaveHouse >= 0) && (indexOfClosestSaveHouse < CRestart::NumberOfSaveHouses), "CGenericGameStorage::FindClosestSaveHouse - indexOfClosestSaveHouse = %d. It's outside the range 0 to %d", indexOfClosestSaveHouse, CRestart::NumberOfSaveHouses))
		{
			fReturnHeading = CRestart::SaveHouses[indexOfClosestSaveHouse].Heading;
			vReturnCoords = CRestart::SaveHouses[indexOfClosestSaveHouse].Coords;
			return true;
		}
	}

	vReturnCoords = VEC3_ZERO;
	fReturnHeading = 0.0f;

	return false;
}


bool CGenericGameStorage::DoCreatorCheck()
{
#if __FINAL
	return true;
#else
    if (PARAM_nocreatorcheck.Get())
    {
	    return false;
    }
	else
	{
		return true;
	}
#endif
}


void CGenericGameStorage::ResetForEpisodeChange()
{
//	ms_SaveGameBuffer.SetCalculatingBufferSizeForFirstTime(true);

//	ClearBufferToStoreFirstFewBytesOfSaveFile();
}

#if __PPU
bool CGenericGameStorage::SetSaveGameFolder()
{
	const char *pSavegameFolder = XEX_TITLE_ID;

#if __BANK
	PARAM_savegameFolder.Get(pSavegameFolder);
#endif	//	__BANK

	SAVEGAME.SetSaveGameFolder(pSavegameFolder);
	return true;
}
#endif	//	#if __PPU

u32 CGenericGameStorage::GetContentType()
{
#if __XENON
	return XCONTENTTYPE_SAVEDGAME;
#else
	return 0;
#endif
}

bool CGenericGameStorage::AllowPreSaveAndPostLoad(const char* OUTPUT_ONLY(pFunctionName))
{
	if (IsImportingOrExporting())
	{
		savegameDisplayf("Skip %s because the savegame is being imported or exported", pFunctionName);
		return false;
	}

	return true;
}

bool CGenericGameStorage::IsImportingOrExporting()
{
#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
	if (CSaveGameImport::GetIsImporting())
	{
		return true;
	}
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	if (CSaveGameExport::GetIsExporting())
	{
		return true;
	}
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

	return false;
}

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
bool CGenericGameStorage::IsSaveGameMigrationCloudTextLoaded()
{
	if (sm_pSaveGameMigrationCloudText)
	{
		return sm_pSaveGameMigrationCloudText->IsInitialised();
	}

	return false;
}

const char *CGenericGameStorage::SearchForSaveGameMigrationCloudText(u32 textKeyHash)
{
	if (sm_pSaveGameMigrationCloudText)
	{
		return sm_pSaveGameMigrationCloudText->SearchForText(textKeyHash);
	}

	return NULL;
}
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES


#if RSG_ORBIS
void CGenericGameStorage::ShowSaveDialog(int userIndex)
{
	m_UserId = g_rlNp.GetUserServiceId(userIndex);
	if (m_UserId == SCE_USER_SERVICE_USER_ID_INVALID)
	{
		savegameWarningf("ShowDaveDialog called with user service id invalid.");
		return;
	}

	if (m_DialogState == STATE_NONE)
	{
		m_DialogState = STATE_SAVE_BEGIN;
	}
	else
	{
		savegameWarningf("ShowDaveDialog called when m_DialogState=%d",(int)m_DialogState);
	}
}

void CGenericGameStorage::ShowLoadDialog(int userIndex)
{
	m_UserId = g_rlNp.GetUserServiceId(userIndex);
	if (m_UserId == SCE_USER_SERVICE_USER_ID_INVALID)
	{
		savegameWarningf("ShowDaveDialog called with user service id invalid.");
		return;
	}

	if (m_DialogState == STATE_NONE)
	{
		m_DialogState = STATE_LOAD_BEGIN;
	}
	else
	{
		savegameWarningf("ShowLoadDialog called when m_DialogState=%d",(int)m_DialogState);
	}
}

void CGenericGameStorage::ShowDeleteDialog(int userIndex)
{
	m_UserId = g_rlNp.GetUserServiceId(userIndex);
	if (m_UserId == SCE_USER_SERVICE_USER_ID_INVALID)
	{
		savegameWarningf("ShowDaveDialog called with user service id invalid.");
		return;
	}

	if (m_DialogState == STATE_NONE)
	{
		m_DialogState = STATE_DELETE_BEGIN;
	}
	else
	{
		savegameWarningf("ShowDeleteDialog called when m_DialogState=%d",(int)m_DialogState);
	}
}

void CGenericGameStorage::UpdateUserId(int userIndex)
{
	if (RL_IS_VALID_LOCAL_GAMER_INDEX(userIndex))
	{
		m_UserId = g_rlNp.GetUserServiceId(userIndex);
	}
	else
	{
		m_UserId =  SCE_USER_SERVICE_USER_ID_INVALID;
	}
}

void CGenericGameStorage::UpdateDialog()
{
	if (m_DialogState == STATE_NONE)
	{
		return;
	}
	else if (m_DialogState <= STATE_SAVE_FINISH)
	{
		UpdateSaveDialog();
	}
	else if (m_DialogState <= STATE_LOAD_FINISH)
	{
		UpdateLoadDialog();
	}
	else if (m_DialogState <= STATE_DELETE_FINISH)
	{
		UpdateDeleteDialog();
	}
}


int CGenericGameStorage::Search()
{
	int ret = SCE_OK;

	SceSaveDataDirNameSearchCond cond;
	memset(&cond, 0x00, sizeof(SceSaveDataDirNameSearchCond));
	cond.userId = m_UserId;
	cond.titleId = &m_TitleId;
	cond.key = SCE_SAVE_DATA_SORT_KEY_DIRNAME;
	cond.order = SCE_SAVE_DATA_SORT_ORDER_DESCENT;

	SceSaveDataDirNameSearchResult result;
	memset(&result, 0x00, sizeof(result));
	result.dirNames = &m_SearchedDirNames[0];
	result.dirNamesNum = SAVE_DATA_COUNT;

	ret = sceSaveDataDirNameSearch(&cond, &result);
	if(ret < SCE_OK)
	{
		savegameWarningf("sceSaveDataDirNameSearch : 0x%08x\n", ret);
		return ret;
	}

	m_SearchedDirNum = result.hitNum;
	return ret;
}

void CGenericGameStorage::UpdateSaveDialog()
{
	int ret = SCE_OK;
	SceCommonDialogStatus status = g_rlNp.GetCommonDialog().GetStatus();

	switch(m_DialogState)
	{
	case STATE_SAVE_BEGIN:
		{
			if (status == SCE_COMMON_DIALOG_STATUS_RUNNING)
			{
				savegameWarningf("STATE_SAVE_BEGIN called while dialog already running");
				m_DialogState = STATE_NONE;
			}
			else
			{
				m_DialogState = STATE_SAVE_SEARCH;
			}
		}
		break;
	case STATE_SAVE_SEARCH:
		{
			ret = Search();
			if (ret < SCE_OK)
			{
				m_DialogState = STATE_SAVE_FINISH;
			}
			else
			{
				m_DialogState = STATE_SAVE_SHOW_LIST_START;
			}
		}
		break;
	case STATE_SAVE_SHOW_LIST_START:
		{
			ret = g_rlNp.GetCommonDialog().ShowSaveListDialog(m_UserId, SCE_SAVE_DATA_DIALOG_TYPE_SAVE, &m_TitleId, m_SearchedDirNames, m_SearchedDirNum, true);
			if (ret < SCE_OK)
			{
				m_DialogState = STATE_SAVE_FINISH;
			}
			else
			{
				m_DialogState = STATE_SAVE_SHOW_LIST;
			}
		}
		break;
	case STATE_SAVE_SHOW_LIST:
		{
			if (status == SCE_COMMON_DIALOG_STATUS_FINISHED)
			{
				memset(&m_SelectedDirName, 0x00, sizeof(m_SelectedDirName));
				SceSaveDataParam param;
				memset(&param, 0x00, sizeof(param));

				SceSaveDataDialogResult result;
				memset(&result, 0x00, sizeof(SceSaveDataDialogResult));
				result.dirName = &m_SelectedDirName;
				result.param = &param;

				// backup save dir name in case users presses cancel
				char buffer[SCE_SAVE_DATA_DIRNAME_DATA_MAXSIZE];
				safecpy(buffer, m_SelectedDirName.data);

				ret = sceSaveDataDialogGetResult(&result);
				if(ret < SCE_OK)
				{
					savegameWarningf("sceSaveDataDialogGetResult : 0x%x\n", ret);
					m_DialogState = STATE_SAVE_FINISH;
				}
				else if (result.result == SCE_OK)
				{
					if (m_SelectedDirName.data[0] == '\0')
					{
						formatf(m_SelectedDirName.data, sizeof(m_SelectedDirName.data), SAVE_DATA_DIRNAME_TEMPLATE, m_SearchedDirNum);
					}
					else
					{
						safecpy(m_SelectedDirName.data, result.dirName->data);
					}

					SAVEGAME.SetSelectedDirectory(m_UserId, m_SelectedDirName.data);
					m_DialogState = STATE_SAVE_FINISH;
				}
				else if(result.result == SCE_COMMON_DIALOG_RESULT_USER_CANCELED)
				{
					safecpy(m_SelectedDirName.data, buffer);
					m_DialogState = STATE_SAVE_FINISH;
				}
				else
				{
					m_DialogState = STATE_SAVE_FINISH;
				}
			}
		}
		break;
	case STATE_SAVE_FINISH:
		m_DialogState = STATE_NONE;
		break;
	default:
		savegameWarningf("Unhandled save game dialog state...");
		break;
	}
}

void CGenericGameStorage::UpdateLoadDialog()
{
	int ret = SCE_OK;
	SceCommonDialogStatus status = g_rlNp.GetCommonDialog().GetStatus();

	switch(m_DialogState)
	{
	case STATE_LOAD_BEGIN:
		{
			if (status == SCE_COMMON_DIALOG_STATUS_RUNNING)
			{
				savegameWarningf("STATE_SAVE_BEGIN called while dialog already running");
				m_DialogState = STATE_NONE;
			}
			else
			{
				m_DialogState = STATE_LOAD_SEARCH;
			}
		}
		break;
	case STATE_LOAD_SEARCH:
		{
			ret = Search();
			if (ret < SCE_OK)
			{
				m_DialogState = STATE_LOAD_FINISH;
			}
			else
			{
				if (m_SearchedDirNum == 0)
				{
					m_DialogState = STATE_LOAD_SHOW_NODATA_START;
				}
				else
				{
					m_DialogState = STATE_LOAD_SHOW_LIST_START;
				}
			}
		}
		break;
	case STATE_LOAD_SHOW_LIST_START:
		{
			ret = g_rlNp.GetCommonDialog().ShowSaveListDialog(m_UserId, SCE_SAVE_DATA_DIALOG_TYPE_LOAD, &m_TitleId, m_SearchedDirNames, m_SearchedDirNum, true);
			if (ret < SCE_OK)
			{
				m_DialogState = STATE_LOAD_FINISH;
			}
			else
			{
				m_DialogState = STATE_LOAD_SHOW_LIST;
			}
		}
		break;
	case STATE_LOAD_SHOW_LIST:
		{
			if(SCE_COMMON_DIALOG_STATUS_FINISHED == status)
			{
				memset(&m_SelectedDirName, 0x00, sizeof(m_SelectedDirName));
				SceSaveDataParam param;
				memset(&param, 0x00, sizeof(param));

				SceSaveDataDialogResult result;
				memset(&result, 0x00, sizeof(SceSaveDataDialogResult));
				result.dirName = &m_SelectedDirName;
				result.param = &param;

				// backup save dir name in case users presses cancel
				char buffer[SCE_SAVE_DATA_DIRNAME_DATA_MAXSIZE];
				safecpy(buffer, m_SelectedDirName.data);

				ret = sceSaveDataDialogGetResult(&result);
				if(result.result == SCE_OK)
				{
					m_DialogState = STATE_LOAD_FINISH;
				}
				else if(result.result == SCE_COMMON_DIALOG_RESULT_USER_CANCELED)
				{
					safecpy(m_SelectedDirName.data, buffer);
					m_DialogState = STATE_LOAD_FINISH;
				}
				else
				{
					m_DialogState = STATE_LOAD_FINISH;
				}
			}
		}
		break;
	case STATE_LOAD_SHOW_NODATA_START:
		{
			ret = g_rlNp.GetCommonDialog().ShowSaveDialogConfirm(m_UserId, SCE_SAVE_DATA_DIALOG_TYPE_LOAD, &m_TitleId, NULL,  SCE_SAVE_DATA_DIALOG_SYSMSG_TYPE_NODATA);
			if (ret < SCE_OK)
			{
				savegameWarningf("OpenDialogConfirm : 0x%x\n", ret);
				m_DialogState = STATE_LOAD_FINISH;
			}
			else
			{
				m_DialogState = STATE_LOAD_SHOW_NODATA;
			}
		}
		break;
	case STATE_LOAD_SHOW_NODATA:
		{
			if(SCE_COMMON_DIALOG_STATUS_FINISHED == status)
			{
				SceSaveDataDialogResult result;
				memset(&result, 0x00, sizeof(SceSaveDataDialogResult));
				ret = sceSaveDataDialogGetResult(&result);
				m_DialogState = STATE_LOAD_FINISH;
			}
		}
		break;
	case STATE_LOAD_FINISH:
		m_DialogState = STATE_NONE;
		break;
	default:
		savegameWarningf("Unhandled save game dialog state...");
		break;
	}
}

void CGenericGameStorage::UpdateDeleteDialog()
{
	int ret = SCE_OK;
	SceCommonDialogStatus status = g_rlNp.GetCommonDialog().GetStatus();

	switch(m_DialogState)
	{
	case STATE_DELETE_BEGIN:
		{
			if (status == SCE_COMMON_DIALOG_STATUS_RUNNING)
			{
				savegameWarningf("STATE_SAVE_BEGIN called while dialog already running");
				m_DialogState = STATE_NONE;
			}
			else
			{
				m_DialogState = STATE_DELETE_SEARCH;
			}
		}
		break;
	case STATE_DELETE_SEARCH:
		{
			ret = Search();
			if (ret < SCE_OK)
			{
				m_DialogState = STATE_DELETE_FINISH;
			}
			else
			{
				if (m_SearchedDirNum == 0)
				{
					m_DialogState = STATE_DELETE_SHOW_NODATA_START;
				}
				else
				{
					m_DialogState = STATE_DELETE_SHOW_LIST_START;
				}
			}
		}
		break;
	case STATE_DELETE_SHOW_LIST_START:
		{
			ret = g_rlNp.GetCommonDialog().ShowSaveListDialog(m_UserId, SCE_SAVE_DATA_DIALOG_TYPE_DELETE, &m_TitleId, m_SearchedDirNames, m_SearchedDirNum, true);
			if (ret < SCE_OK)
			{
				m_DialogState = STATE_DELETE_FINISH;
			}
			else
			{
				m_DialogState = STATE_DELETE_SHOW_LIST;
			}
			break;
		}
		break;
	case STATE_DELETE_SHOW_LIST:
		{
			if(SCE_COMMON_DIALOG_STATUS_FINISHED == status)
			{
				memset(&m_SelectedDirName, 0x00, sizeof(m_SelectedDirName));
				SceSaveDataParam param;
				memset(&param, 0x00, sizeof(param));

				SceSaveDataDialogResult result;
				memset(&result, 0x00, sizeof(SceSaveDataDialogResult));
				result.dirName = &m_SelectedDirName;
				result.param = &param;

				// backup save dir name in case users presses cancel
				char buffer[SCE_SAVE_DATA_DIRNAME_DATA_MAXSIZE];
				safecpy(buffer, m_SelectedDirName.data);

				ret = sceSaveDataDialogGetResult(&result);
				if(result.result == SCE_OK)
				{
					m_DialogState = STATE_DELETE_SHOW_CONFIRM_DELETE_START;
				}
				else if(result.result == SCE_COMMON_DIALOG_RESULT_USER_CANCELED)
				{
					safecpy(m_SelectedDirName.data, buffer);
					m_DialogState = STATE_DELETE_FINISH;
				}
				else
				{
					m_DialogState = STATE_DELETE_FINISH;
				}
			}
		}
		break;
	case STATE_DELETE_SHOW_CONFIRM_DELETE_START:
		{
			ret = g_rlNp.GetCommonDialog().ShowSaveDialogConfirm(m_UserId, SCE_SAVE_DATA_DIALOG_TYPE_DELETE, &m_TitleId, &m_SelectedDirName, SCE_SAVE_DATA_DIALOG_SYSMSG_TYPE_CONFIRM);
			if (ret < SCE_OK)
			{
				m_DialogState = STATE_DELETE_FINISH;
			}
			else
			{
				m_DialogState = STATE_DELETE_SHOW_CONFIRM_DELETE;
			}
		}
		break;
	case STATE_DELETE_SHOW_CONFIRM_DELETE:
		{
			if(SCE_COMMON_DIALOG_STATUS_FINISHED == status)
			{
				memset(&m_SelectedDirName, 0x00, sizeof(m_SelectedDirName));
				SceSaveDataParam param;
				memset(&param, 0x00, sizeof(param));

				SceSaveDataDialogResult result;
				memset(&result, 0x00, sizeof(SceSaveDataDialogResult));
				result.dirName = &m_SelectedDirName;
				result.param = &param;

				ret = sceSaveDataDialogGetResult(&result);
				if(ret < SCE_OK)
				{
					savegameWarningf("sceSaveDataDialogGetResult : 0x%x\n", ret);
					m_DialogState = STATE_DELETE_FINISH;
				}
				else
				{
					if(result.result == SCE_OK)
					{
						if(result.buttonId == SCE_SAVE_DATA_DIALOG_BUTTON_ID_YES)
						{
							m_DialogState = STATE_DELETE_SHOW_DELETE_START;
						}
						else
						{
							m_DialogState = STATE_DELETE_FINISH;
						}
					}
					else if(result.result == SCE_COMMON_DIALOG_RESULT_USER_CANCELED)
					{
						m_DialogState = STATE_DELETE_FINISH;
					}
					else
					{
						m_DialogState = STATE_DELETE_SHOW_ERROR_START;
					}
				}
			}
		}
		break;
	case STATE_DELETE_SHOW_DELETE_START:
		{
			ret = g_rlNp.GetCommonDialog().ShowSaveDialogOperating(m_UserId, SCE_SAVE_DATA_DIALOG_TYPE_DELETE, &m_TitleId, &m_SelectedDirName);
			if (ret < SCE_OK)
			{
				savegameWarningf("OpenDialogOperating : 0x%x\n", ret);
				m_DialogState = STATE_DELETE_FINISH;
				return;
			}

			ret = SAVEGAME.BeginDeleteMount(m_UserIndex, &m_SelectedDirName.data[0], false);
			if (ret < SCE_OK)
			{
				savegameWarningf("DeleteMount : 0x%x\n", ret);
				m_DialogState = STATE_DELETE_FINISH;
			}
			else
			{
				m_DialogState = STATE_DELETE_SHOW_DELETE;
			}
		}
		break;
	case STATE_DELETE_SHOW_DELETE:
		{
			if (SCE_COMMON_DIALOG_STATUS_RUNNING == status)
			{
				if (SAVEGAME.CheckDeleteMount(m_UserIndex))
				{
					SAVEGAME.EndDeleteMount(m_UserIndex);
				}

#if SCE_ORBIS_SDK_VERSION >= (0x01000051u)
				SceSaveDataDialogCloseParam closeParam;
				memset(&closeParam, 0x00, sizeof(closeParam));
				closeParam.anim = SCE_SAVE_DATA_DIALOG_ANIMATION_ON;
				sceSaveDataDialogClose(&closeParam);
#else
				sceSaveDataDialogClose();
#endif

			}
			else if(SCE_COMMON_DIALOG_STATUS_FINISHED == status)
			{
				if(SAVEGAME.GetError(m_UserIndex) < SCE_OK)
				{
					m_DialogState = STATE_DELETE_SHOW_ERROR_START;
				}
				else
				{
					m_DialogState = STATE_DELETE_FINISH;
				}
			}
		}
		break;
	case STATE_DELETE_SHOW_ERROR_START:
		{
			ret = g_rlNp.GetCommonDialog().ShowSaveDialogError(m_UserId, SAVEGAME.GetError(m_UserIndex), SCE_SAVE_DATA_DIALOG_TYPE_DELETE, &m_TitleId, &m_SelectedDirName);
			if (ret < SCE_OK)
			{
				savegameWarningf("OpenDialogError : 0x%x\n", ret);
				m_DialogState = STATE_DELETE_FINISH;
			}

			m_DialogState = STATE_DELETE_SHOW_ERROR;
		}
		break;
	case STATE_DELETE_SHOW_ERROR:
		{
			if(SCE_COMMON_DIALOG_STATUS_FINISHED == status)
			{
				SceSaveDataDialogResult result;
				memset(&result, 0x00, sizeof(SceSaveDataDialogResult));
				ret = sceSaveDataDialogGetResult(&result);
				m_DialogState = STATE_DELETE_FINISH;
			}
		}
		break;
	case STATE_DELETE_SHOW_NODATA_START:
		{
			ret = g_rlNp.GetCommonDialog().ShowSaveDialogConfirm(m_UserId, SCE_SAVE_DATA_DIALOG_TYPE_DELETE, &m_TitleId, NULL, SCE_SAVE_DATA_DIALOG_SYSMSG_TYPE_NODATA);
			if (ret < SCE_OK)
			{
				savegameWarningf("OpenDialogConfirm : 0x%x\n", ret);
				m_DialogState = STATE_DELETE_FINISH;
			}
			else
			{
				m_DialogState = STATE_DELETE_SHOW_NODATA;
			}
		}
		break;
	case STATE_DELETE_SHOW_NODATA:
		{
			if(SCE_COMMON_DIALOG_STATUS_FINISHED == status)
			{
				SceSaveDataDialogResult result;
				memset(&result, 0x00, sizeof(SceSaveDataDialogResult));
				ret = sceSaveDataDialogGetResult(&result);
				m_DialogState = STATE_DELETE_FINISH;
			}
		}
		break;
	case STATE_DELETE_FINISH:
		m_DialogState = STATE_NONE;
		break;
	default:
		savegameWarningf("Unhandled save game dialog state...");
		break;
	}
}

bool CGenericGameStorage::IsSafeToUseSaveLibrary()
{
	if(!fwTimer::IsGamePaused())
	{
		if(strStreamingEngine::GetBurstMode())
			return false;

		float velocitySq = 0.0f;
		CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
		if (pPlayerPed)
		{
			if (pPlayerPed->GetIsInVehicle())
			{
				CVehicle* pVehicle = pPlayerPed->GetVehiclePedInside();
				if ( pVehicle ){
					velocitySq = pVehicle->GetVelocity().Mag2();
				}
			}
		}

		if (velocitySq > 10.0f)
			return false;
		if(strStreamingEngine::GetInfo().GetNumberObjectsRequested() > 20) 
			return false;
		if(strStreamingEngine::GetInfo().GetNumberPriorityObjectsRequested() > 0) 
			return false;
		if(gStreamingRequestList.IsActive()) //we are in or about to enter an SRL cutscene,
			return false;
	}
	else
	{
		if (!CPauseMenu::IsActive( PM_EssentialAssets ))
			return false;

		if (!CPauseMenu::IsLoadingPhaseDone())
			return false;
	}
	return true;
}

#if GTA_REPLAY
const int SAFETOSAVECOOLDOWN = 1000;
struct countdownManager
{
	countdownManager(int& c)
	: cooldownVal(c)
	, restart(true)
	{}

	~countdownManager()
	{
		if(restart)
			cooldownVal = SAFETOSAVECOOLDOWN;
	}

	bool Countdown()
	{
		restart = false;
		
		cooldownVal -= fwTimer::GetNonPausedTimeStepInMilliseconds();
		cooldownVal = Max(0, cooldownVal);
		
		return cooldownVal == 0;
	}

	int& cooldownVal;
	bool restart;
};


bool CGenericGameStorage::IsSafeToSaveReplay()
{
	if(ms_PushThroughReplaySave) // Force the save
	{
		replayDebugf1("Forcing the clip save through...");
		return true;
	}

	static int coolDownCount = 0;
	countdownManager m(coolDownCount);

	if(strStreamingEngine::GetBurstMode())
		return false;

	if(strStreamingEngine::GetInfo().GetNumberObjectsRequested() > 20) 
		return false;

	if(strStreamingEngine::GetInfo().GetNumberPriorityObjectsRequested() > 0) 
		return false;

 	if(gStreamingRequestList.IsPlaybackActive())
 		return false;

	float cachedNeededScore = CSceneStreamerMgr::GetCachedNeededScore();

	if ( cachedNeededScore >= (float)STREAMBUCKET_VISIBLE )
	{
		replayDisplayf("cachedNeededScore >= (float)STREAMBUCKET_VISIBLE_FAR");
		return false;
	}
	
	return m.Countdown();
}
#endif // GTA_REPLAY

#endif

