//
//
//
//
//
#ifndef SAVEGAME_NEW_H
#define SAVEGAME_NEW_H

// Rage headers
//	#include "file\savegame.h"
//	#include "rline/rlgamerinfo.h"

// Game headers
//	#include "game\Wanted.h"
//	#include "game\Weather.h"
//	#include "system\FileMgr.h"
//	#include "text\text.h"
//	#include "scene\ExtraContent.h"
#include "SaveLoad/SavegameBuffers/savegame_buffer_mp.h"
#include "SaveLoad/SavegameBuffers/savegame_buffer_sp.h"
#include "SaveLoad/savegame_data.h"
//	#include "SaveLoad/savegame_date.h"
#include "SaveLoad/savegame_defines.h"
//	#include "SaveLoad/savegame_filenames.h"
#include "SaveLoad/savegame_list.h"
#include "SaveLoad/savegame_messages.h"
#include "SaveLoad/savegame_queued_operations.h"
#include "SaveLoad/SavegameMigration/savegame_migration_cloud_text.h"
#include "text/TextFile.h"

#define NUM_FRAMES_TO_DISPLAY_BLACK_SCREEN_AFTER_WARNING_MESSAGE	(3)
#define NUM_FRAMES_TO_DISPLAY_WARNING_MESSAGE_BEFORE_PAUSING_GAME	(4)

#if RSG_ORBIS
#include <save_data.h>
#endif

class CGenericGameStorage
{
private :
//	Private data
	static CSaveGameBuffer_SinglePlayer			ms_SaveGameBuffer_SinglePlayer;
	static CSaveGameBuffer_MultiplayerCharacter	ms_SaveGameBuffer_MultiplayerCharacter_Cloud;
	static CSaveGameBuffer_MultiplayerCommon	ms_SaveGameBuffer_MultiplayerCommon_Cloud;
#if __ALLOW_LOCAL_MP_STATS_SAVES
	static CSaveGameBuffer_MultiplayerCharacter	ms_SaveGameBuffer_MultiplayerCharacter_Local;
	static CSaveGameBuffer_MultiplayerCommon	ms_SaveGameBuffer_MultiplayerCommon_Local;
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES

	static CSavegameQueuedOperation_CheckFileExists sm_CheckFileExistsQueuedOperation;
	static CSavegameQueuedOperation_Autosave sm_AutosaveStructure;

	static CSavegameQueuedOperation_MPStats_Load sm_MPStatsLoadQueuedOperation;
	static CSavegameQueuedOperation_MPStats_Save sm_MPStatsSaveQueuedOperation;
	static u32                                   sm_StatsSaveCategory;

	static CSavegameQueuedOperation_MissionRepeatLoad ms_MissionRepeatLoadQueuedOperation;
	static CSavegameQueuedOperation_MissionRepeatSave ms_MissionRepeatSaveQueuedOperation;

	static CSavegameQueuedOperation_LoadMostRecentSave ms_LoadMostRecentSave;

#if __PPU
#if __QUICK_SCAN_FOR_AUTOLOAD_AND_SLOW_SCAN_ON_ANOTHER_THREAD
	static CSavegameQueuedOperation_SlowPS3Scan sm_SlowPS3ScanStructure;
#endif	//	__QUICK_SCAN_FOR_AUTOLOAD_AND_SLOW_SCAN_ON_ANOTHER_THREAD
#endif	//	__PPU

#if USE_SAVE_DATA_MEMORY && !USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP
	static CSavegameQueuedOperation_CreateBackupOfAutosaveSDM ms_CreateBackupOfAutosaveSDM;

	static bool ms_bBackupOfAutosaveHasBeenRequested;
#endif	//	USE_SAVE_DATA_MEMORY && !USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
	static CSavegameQueuedOperation_ImportSinglePlayerSavegameIntoFixedSlot sm_ImportSinglePlayerSavegameIntoFixedSlot;
	static bool sm_QueueImportSinglePlayerSavegameIntoFixedSlot;
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	static CSaveGameMigrationCloudText *sm_pSaveGameMigrationCloudText;
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

	static eSaveOperation ms_SavegameOperation;

	static bool ms_bDisplayAutosaveOverwriteMessage;

#if __ALLOW_LOCAL_MP_STATS_SAVES
	static bool ms_bOnlyReadTimestampFromMPSave;
	static u64 ms_TimestampReadFromMpSave;

	static bool ms_bAllowReasonForFailureToBeSet;
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES

	static eReasonForFailureToLoadSavegame ms_ReasonForFailureToLoadSavegame;
	static int ms_CloudSaveResultcode;

//	Private functions
#if __PPU
//	static void		InitDefaultIcon();
	static bool		SetSaveGameFolder();
#endif

#if USE_SAVE_DATA_MEMORY && !USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP
	static void UpdatePS4AutosaveBackup();
#endif	//	USE_SAVE_DATA_MEMORY && !USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP

#if __BANK
	static void CheckForProfileWriteRequest();
	static void CheckForLoadFromPC();
	static void CheckForMpStatsSaveAndLoad();
#endif	//	__BANK

	static CSaveGameBuffer *GetSavegameBuffer(eTypeOfSavegame SavegameType);


#if RSG_ORBIS
	// THESE NEED TO MATCH SAVEGAME_ORBIS.cpp
	#define SAVE_DATA_COUNT				(16)
	#define SAVE_DATA_DIRNAME_PREFIX	"SAVEDATA"  // TODO: JRM
	#define SAVE_DATA_DIRNAME_DEFAULT	SAVE_DATA_DIRNAME_PREFIX "00"
	#define SAVE_DATA_DIRNAME_TEMPLATE	SAVE_DATA_DIRNAME_PREFIX "%02u"

	enum DialogState
	{
		STATE_NONE = 0,
		// Saving
		STATE_SAVE_BEGIN,
		STATE_SAVE_SEARCH,
		STATE_SAVE_SHOW_LIST_START,
		STATE_SAVE_SHOW_LIST,
		STATE_SAVE_FINISH,
		// Loading
		STATE_LOAD_BEGIN,
		STATE_LOAD_SEARCH,
		STATE_LOAD_SHOW_LIST_START,
		STATE_LOAD_SHOW_LIST,
		STATE_LOAD_SHOW_NODATA_START,
		STATE_LOAD_SHOW_NODATA,
		STATE_LOAD_FINISH,
		// Deleting
		STATE_DELETE_BEGIN,
		STATE_DELETE_SEARCH,
		STATE_DELETE_SHOW_LIST_START,
		STATE_DELETE_SHOW_LIST,
		STATE_DELETE_SHOW_CONFIRM_DELETE_START,
		STATE_DELETE_SHOW_CONFIRM_DELETE,
		STATE_DELETE_SHOW_DELETE_START,
		STATE_DELETE_SHOW_DELETE,
		STATE_DELETE_SHOW_ERROR_START,
		STATE_DELETE_SHOW_ERROR,
		STATE_DELETE_SHOW_NODATA_START,
		STATE_DELETE_SHOW_NODATA,
		STATE_DELETE_FINISH,
	};

	static SceSaveDataDirName	m_SearchedDirNames[SAVE_DATA_COUNT];
	static uint32_t				m_SearchedDirNum;
	static DialogState			m_DialogState;
	static SceUserServiceUserId	m_UserId;
	static int					m_UserIndex;
	static SceSaveDataTitleId	m_TitleId;
	static SceSaveDataDirName	m_SelectedDirName;

	static int Search();

	static void UpdateDialog();
	static void UpdateSaveDialog();
	static void UpdateLoadDialog();
	static void UpdateDeleteDialog();
	
public: 
	static void ShowSaveDialog(int userIndex);
	static void ShowLoadDialog(int userIndex);
	static void ShowDeleteDialog(int userIndex);

	static void UpdateUserId(int userIndex);

	static bool IsSafeToUseSaveLibrary();
	REPLAY_ONLY(static bool IsSafeToSaveReplay();)
#endif
public: 
	REPLAY_ONLY(static bool ms_PushThroughReplaySave;)

public:
//	Public data
	static bool		ms_bClosestSaveHouseDataIsValid;
	static float	ms_fHeadingOfClosestSaveHouse;
	static Vector3	ms_vCoordsOfClosestSaveHouse;

	static bool		ms_bFadeInAfterSuccessfulLoad;
	
	static bool		ms_bPlayerShouldSnapToGroundOnSpawn;

#if __BANK
	static const int ms_MaxLengthOfPcSaveFileName = 64;
	static const int ms_MaxLengthOfPcSaveFolderName	= 128;

	static bool ms_bPrintValuesAsPsoFileLoads;

	static bool ms_bPrintScriptDataFromLoadedMpSaveFile;

	static bool ms_bSaveToPC;
	static bool ms_bSaveToConsoleWhenSavingToPC;
	static char ms_NameOfPCSaveFile[ms_MaxLengthOfPcSaveFileName];

	static bool ms_bLoadFromPC;
	static bool ms_bDecryptOnLoading;
	static char ms_NameOfPCLoadFile[ms_MaxLengthOfPcSaveFileName];

	static bool ms_bLoadMpStatsFileFromPC;
	static bool ms_bMpStatsFileIsACharacterFile;
	static bool ms_bDecryptMpStatsFileOnLoading;
	static char ms_NameOfMpStatsPCLoadFile[ms_MaxLengthOfPcSaveFileName];

	static bool ms_bSaveAsPso;

	static char ms_NameOfPCSaveFolder[ms_MaxLengthOfPcSaveFolderName];

	static bool ms_bStartMpStatsCloudSave;
	static bool ms_bStartMpStatsCloudLoad;

	static bool ms_bDisplayLoadingSpinnerWhenSaving;

#if RSG_ORBIS
	static bool ms_bAlwaysLoadBackupSaves;
#endif	//	RSG_ORBIS

	static bool ms_bRequestProfileWrite;

// 	static bool ms_bStartCheckIfMpStatsFileExists;
// 	static bool ms_bMpStatsFileExistsCheckIsInProgress;
// 	static bool ms_bMpStatsFileExists;
// 	static bool ms_bMpStatsFileIsDamaged;
// 	static u32  ms_MpSlotIndex;

//	WARNING MESSAGES
	static u32 ms_NumberOfFramesToDisplayWarningMessageForBeforePausingTheGame;
	static u32 ms_NumberOfFramesToDisplayBlackScreenAfterWarningMessage;

//	SAVED DATA
	static bool ms_bSaveSimpleVariables;
	static bool ms_bSaveMiniMapVariables;
	static bool ms_bSavePlayerPedVariables;
	static bool ms_bSaveExtraContentVariables;
	static bool ms_bSaveGarageVariables;
	static bool ms_bSaveStatsVariables;
	static bool ms_bSaveStuntJumpVariables;
	static bool ms_bSaveRadioStationVariables;
	static bool ms_bSavePS3AchievementVariables;
	static bool ms_bSaveDoorVariables;
	static bool ms_bSaveCameraVariables;
	static bool ms_bSaveScriptVariables;

//	PHOTO GALLERY
	static bool ms_bSavePhoto;
	static bool ms_bPhotoSaveInProgressReadOnly;
//	END OF PHOTO GALLERY

#if RSG_ORBIS
	static int ms_iUserIndex;
#endif

#else	//	__BANK
//	WARNING MESSAGES
	static const u32 ms_NumberOfFramesToDisplayWarningMessageForBeforePausingTheGame = NUM_FRAMES_TO_DISPLAY_WARNING_MESSAGE_BEFORE_PAUSING_GAME;
	static const u32 ms_NumberOfFramesToDisplayBlackScreenAfterWarningMessage = NUM_FRAMES_TO_DISPLAY_BLACK_SCREEN_AFTER_WARNING_MESSAGE;
#endif	//	__BANK


//	Public functions
	static void		Init(unsigned initMode);
    static void		Shutdown(unsigned shutdownMode);

//	I copied these two lines from GTA4. Should I try to move the code into the new Init and Shutdown?
//	static void		InitMap();
//	static void		ShutdownMap();

#if __BANK
	static void		InitWidgets();
#endif

    static void     Update();
	static void UpdatePhotoGallery();

	static void QueueAutosave();

	static bool QueueMpStatsSave(const u32 saveCategory
#if __ALLOW_LOCAL_MP_STATS_SAVES
		, CSavegameQueuedOperation_MPStats_Save::eMPStatsSaveDestination saveDestination
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES
		);

	static MemoryCardError GetMpStatsSaveStatus();
	static void ClearMpStatsSaveStatus();
	static void CancelMpStatsSave();

	static bool QueueMpStatsLoad(const u32 saveCategory);
	static MemoryCardError GetMpStatsLoadStatus();
	static void ClearMpStatsLoadStatus();
	static void CancelMpStatsLoad();

	static bool QueueCheckFileExists(s32 SlotIndex);
	static MemoryCardError GetCheckFileExistsStatus(bool &bFileExists, bool &bFileIsDamaged);
	static void ClearCheckFileExistsStatus( );

	static bool QueueMissionRepeatLoad();
	static MemoryCardError GetMissionRepeatLoadStatus();
	static void ClearMissionRepeatLoadStatus();

	static bool QueueMissionRepeatSave(bool bBenchmarkTest);
	static MemoryCardError GetMissionRepeatSaveStatus();
	static void ClearMissionRepeatSaveStatus();

	static bool QueueLoadMostRecentSave();

#if __PPU
#if __QUICK_SCAN_FOR_AUTOLOAD_AND_SLOW_SCAN_ON_ANOTHER_THREAD
	static void QueueSlowPS3Scan();
#endif	//	__QUICK_SCAN_FOR_AUTOLOAD_AND_SLOW_SCAN_ON_ANOTHER_THREAD
#endif	//	__PPU

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
	static bool RequestImportSinglePlayerSavegameIntoFixedSlot(const u8 *pData, u32 dataSize);
	static void CheckForImportSinglePlayerSavegameIntoFixedSlot();
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

	static bool PushOnToSavegameQueue(CSavegameQueuedOperation *pNewOperation);
	static bool ExistsInQueue(CSavegameQueuedOperation *pOperationToCheck, bool &bAtTop);
	static void RemoveFromSavegameQueue(CSavegameQueuedOperation *pOperationToRemove);

	static u32 GetContentType();

	static void ResetForEpisodeChange();

	static eSaveOperation GetSaveOperation() { return ms_SavegameOperation; }
	static void SetSaveOperation(eSaveOperation Operation);

	static void SetFlagForAutosaveOverwriteMessage(bool bDisplayAutosaveOverwrite);
	static bool ShouldWeDisplayTheMessageAboutOverwritingTheAutosaveSlot();

//	static void ClearBufferToStoreFirstFewBytesOfSaveFile();
//	static int GetVersionNumberFromStartOfSaveFile();
//	static bool CheckSaveStringAtStartOfSaveFile();


	static void SetBuffer(u8* buffer, const u32 bufferSize, eTypeOfSavegame SavegameType);
	static MemoryCardError AllocateBuffer(eTypeOfSavegame SavegameType, bool bSaving);
	static u8 *GetBuffer(eTypeOfSavegame SavegameType);
	static u32 GetBufferSize(eTypeOfSavegame SavegameType);
	static void SetBufferSize(eTypeOfSavegame SavegameType, u32 BufferSize);

	static u32 GetMaxBufferSize(eTypeOfSavegame SavegameType);

	static void FreeBufferContainingContentsOfSavegameFile(eTypeOfSavegame SavegameType);
	static void FreeAllDataToBeSaved(eTypeOfSavegame SavegameType);

	static bool LoadBlockData(eTypeOfSavegame SavegameType, const bool useEncryption = true);
	static bool BeginSaveBlockData(eTypeOfSavegame SavegameType, const bool useEncryption = true);
	static void EndSaveBlockData(eTypeOfSavegame SavegameType);
	static SaveBlockDataStatus GetSaveBlockDataStatus(eTypeOfSavegame SavegameType);
	static MemoryCardError CreateSaveDataAndCalculateSize(eTypeOfSavegame SavegameType, bool bIsAnAutosave);

//	static bool 	SaveDataToWorkBuffer(const void* pData, s32 SizeOfData);
//	static bool 	LoadDataFromWorkBuffer(void* pData, s32 SizeOfData);
//	static bool		ForwardReadPositionInWorkBuffer(s32 SizeOfData);


//	static bool		IsCalculatingBufferSize(void)	{ return CSaveGameData::IsCalculatingBufferSize(); }

	//	bCheckForGameOnly - set this to TRUE if you only want to check for savegames
	//						set it to FALSE if you also want to check for photos and MPStats files
//	static bool IsSaveInProgress(bool bCheckForGameOnly);
	static bool IsSavegameSaveInProgress();
	static bool IsMpStatsSaveInProgress();
	static bool IsMpStatsSaveAtTopOfSavegameQueue();
	static bool IsPhotoSaveInProgress();

#if __ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
	static bool IsUploadingSavegameToCloud();
#endif	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME


//	static bool IsLoadInProgress(bool bCheckForGameOnly);
	static bool IsSavegameLoadInProgress();
	static bool IsMpStatsLoadInProgress();
	static bool IsMpStatsLoadAtTopOfSavegameQueue();
	static bool IsPhotoLoadInProgress();

	static bool IsLocalDeleteInProgress();
	static bool IsCloudDeleteInProgress();

	static bool IsStorageDeviceBeingAccessed();

	static bool DoCreatorCheck();

	static void		FinishAllUnfinishedSaveGameOperations();

	static bool DetermineShouldSnapToGround();

//	static void		ClearSaveHouseData();
	static bool FindClosestSaveHouse(bool bThisIsAnAutosave, Vector3 &vReturnCoords, float &fReturnHeading);

	//	bLoadingFromStorageDevice - true means a proper save game, false means a network restore save
	static void 	DoGameSpecificStuffAfterSuccessLoad(bool bLoadingFromStorageDevice);

	//Retrieve the category of the stats savegame file
	static u32      GetStatsSaveCategory() { return sm_StatsSaveCategory; }

	static void SetReasonForFailureToLoadSavegame(eReasonForFailureToLoadSavegame reasonForFailureToLoad)
	{
#if __ALLOW_LOCAL_MP_STATS_SAVES
		if (ms_bAllowReasonForFailureToBeSet)
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES
		{
			ms_ReasonForFailureToLoadSavegame = reasonForFailureToLoad;
		}
	}

	static eReasonForFailureToLoadSavegame GetReasonForFailureToLoadSavegame() {return ms_ReasonForFailureToLoadSavegame; }

#if __ALLOW_LOCAL_MP_STATS_SAVES
	static void AllowReasonForFailureToBeSet(bool bAllowReasonToBeSet) { ms_bAllowReasonForFailureToBeSet = bAllowReasonToBeSet; }

	static void SetOnlyReadTimestampFromMPSave(bool bOnlyReadTimestamp) { ms_bOnlyReadTimestampFromMPSave = bOnlyReadTimestamp; }
	static bool GetOnlyReadTimestampFromMPSave() { return ms_bOnlyReadTimestampFromMPSave; }

	static void SetTimestampReadFromMpSave(u64 timestamp) { ms_TimestampReadFromMpSave = timestamp; }
	static u64 GetTimestampReadFromMpSave() { return ms_TimestampReadFromMpSave; }
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES

	static void SetCloudSaveResultcode(int code) { ms_CloudSaveResultcode = code; }
	static int GetCloudSaveResultcode() {return ms_CloudSaveResultcode; }

#if USE_SAVE_DATA_MEMORY && !USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP
	static void RequestBackupOfAutosave() { ms_bBackupOfAutosaveHasBeenRequested = true; }

	static bool IsAutosaveBackupAtTopOfSavegameQueue();
#endif	//	USE_SAVE_DATA_MEMORY && !USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP

	static bool AllowPreSaveAndPostLoad(const char* pFunctionName);
	static bool IsImportingOrExporting();

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	static bool IsSaveGameMigrationCloudTextLoaded();
	static const char *SearchForSaveGameMigrationCloudText(u32 textKeyHash);
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES
};

#endif	//	SAVEGAME_NEW_H

