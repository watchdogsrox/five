#ifndef SAVEGAME_EXPORT_H
#define SAVEGAME_EXPORT_H

// Game headers
#include "SaveLoad/savegame_defines.h"

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES

#include "SaveLoad/savegame_filenames.h"		// For SAVE_GAME_MAX_DISPLAY_NAME_LENGTH
#include "SaveLoad/savegame_messages.h"			// For MAX_MENU_ITEM_CHAR_LENGTH
#include "SaveLoad/SavegameMigration/savegame_migration_buffer.h"
#include "SaveLoad/SavegameMigration/savegame_migration_data_code.h"
#include "SaveLoad/SavegameMigration/savegame_migration_data_script.h"
#include "SaveLoad/SavegameMigration/savegame_migration_metadata.h"
#include "SaveMigration/SaveMigration.h"

// Forward declarations
namespace rage {
 	class parTree;
 	class psoFile;
}
class CSaveGameExport_BackgroundCheckForPreviousUpload;


class CSaveGameExport_DetailsOfPreviouslyUploadedSavegame
{
public:
	CSaveGameExport_DetailsOfPreviouslyUploadedSavegame();

	void Clear();
	void Set(bool bHaveChecked, u32 posixTime, float percentage, u32 missionNameHash);

	bool GetHaveCheckedForPreviouslyUploadedSavegame() { return m_bHaveCheckedForPreviouslyUploadedSavegame; }
	bool HasASavegameBeenPreviouslyUploaded();
	const char *GetLastCompletedMissionName() { return m_LastCompletedMissionName; }
	const char *GetSaveTimeString() { return m_SaveTimeString; }

private:
	bool FillStringContainingLastMissionPassed();
	bool FillStringContainingSaveTime();

private:
	char m_LastCompletedMissionName[SAVE_GAME_MAX_DISPLAY_NAME_LENGTH];
	char m_SaveTimeString[MAX_MENU_ITEM_CHAR_LENGTH];

	bool m_bHaveCheckedForPreviouslyUploadedSavegame;
	float m_CompletionPercentage;
	u32 m_LastCompletedMissionId;
	u32 m_SavePosixTime;
};


class CSaveGameExport
{
	friend class CSaveGameExport_BackgroundCheckForPreviousUpload;
public:
	enum eNetworkProblemsForSavegameExport
	{
		SAVEGAME_EXPORT_NETWORK_PROBLEM_NONE,
		SAVEGAME_EXPORT_NETWORK_PROBLEM_SIGNED_OUT_LOCALLY,
		SAVEGAME_EXPORT_NETWORK_PROBLEM_NO_NETWORK_CONNECTION,
		SAVEGAME_EXPORT_NETWORK_PROBLEM_AGE_RESTRICTED,
		SAVEGAME_EXPORT_NETWORK_PROBLEM_SIGNED_OUT_ONLINE,
		SAVEGAME_EXPORT_NETWORK_PROBLEM_SOCIAL_CLUB_UNAVAILABLE,
		SAVEGAME_EXPORT_NETWORK_PROBLEM_NOT_SIGNED_IN_TO_SOCIAL_CLUB,
		SAVEGAME_EXPORT_NETWORK_PROBLEM_SOCIAL_CLUB_POLICIES_OUT_OF_DATE
	};

public:
	//	Public functions
	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);

	static void Update();

	static bool BeginExport();
	static MemoryCardError UpdateExport();

	static void FreeAllData();

	static void SetBufferSizeForLoading(u32 bufferSize);
	static MemoryCardError AllocateBufferForLoading();
	static void FreeBufferForLoading();
	static u8 *GetBufferForLoading();
	static u32 GetSizeOfBufferForLoading();

	static void SetIsExporting(bool bIsExporting) { sm_bIsExporting = bIsExporting; }
	static bool GetIsExporting() { return sm_bIsExporting; }

	static bool GetTransferAlreadyCompleted();
	static bool GetHaveCheckedForPreviousUpload();
	static bool GetHasASavegameBeenPreviouslyUploaded();
	static const char *GetMissionNameOfPreviousUpload();
	static const char *GetSaveTimeStringOfPreviousUpload();

	static eNetworkProblemsForSavegameExport CheckForNetworkProblems();

#if !__NO_OUTPUT
	static const char *GetNetworkProblemName(eNetworkProblemsForSavegameExport networkProblem);
#endif // !__NO_OUTPUT

#if __BANK
	static void InitWidgets(bkBank *pParentBank);
#endif // __BANK

private:
	static bool LoadBlockData();

	static bool ReadDataFromPsoFile(psoFile* psofile);

	static MemoryCardError CreateSaveDataAndCalculateSize();

	// Kicks off a thread that runs the SaveBlockData operation
	static bool BeginSaveBlockData();
	static void SaveBlockDataForExportThreadFunc(void*);
	static bool DoSaveBlockData(); //Called by SaveBlockDataForExportThreadFunc
	static void EndSaveBlockData();

	static bool BeginCheckForPreviousUpload();
	static MemoryCardError CheckForPreviousUpload();
	static MemoryCardError DisplayResultsOfCheckForPreviousUpload();

	static bool BeginUpload();
	static MemoryCardError CheckUpload();
	static bool DisplayUploadResult();

	static void FillMetadata();
	static MemoryCardError WriteMetadataToTextFile();

	static void FreeParTreeToBeSaved();

	static bool CompressXmlDataForSaving();

	static const char *GetErrorMessageText(s32 errorCode);
	static const char *GetNetworkErrorMessageText(eNetworkProblemsForSavegameExport networkProblem);

//	Private data
	enum SaveBlockDataForExportStatus
	{
		SAVEBLOCKDATAFOREXPORT_IDLE,
		SAVEBLOCKDATAFOREXPORT_PENDING,
		SAVEBLOCKDATAFOREXPORT_ERROR,
		SAVEBLOCKDATAFOREXPORT_SUCCESS
	};

	enum eSaveGameExportProgress
	{
		SAVEGAME_EXPORT_PROGRESS_READ_PSO_DATA_FROM_BUFFER,
		SAVEGAME_EXPORT_PROGRESS_CREATE_XML_SAVE_DATA,
		SAVEGAME_EXPORT_PROGRESS_ALLOCATE_UNCOMPRESSED_SAVE_BUFFER,
		SAVEGAME_EXPORT_PROGRESS_BEGIN_WRITING_TO_SAVE_BUFFER,
		SAVEGAME_EXPORT_PROGRESS_CHECK_WRITING_TO_SAVE_BUFFER,
		SAVEGAME_EXPORT_PROGRESS_BEGIN_CHECK_FOR_PREVIOUS_UPLOAD,
		SAVEGAME_EXPORT_PROGRESS_CHECK_FOR_PREVIOUS_UPLOAD,
		SAVEGAME_EXPORT_PROGRESS_DISPLAY_RESULTS_OF_CHECK_FOR_PREVIOUS_UPLOAD,
		SAVEGAME_EXPORT_PROGRESS_BEGIN_UPLOAD,
		SAVEGAME_EXPORT_PROGRESS_CHECK_UPLOAD,
		SAVEGAME_EXPORT_PROGRESS_DISPLAY_UPLOAD_RESULT,
		SAVEGAME_EXPORT_PROGRESS_WRITE_SAVE_BUFFER_TO_TEXT_FILE,
		SAVEGAME_EXPORT_PROGRESS_WRITE_SAVEGAME_METADATA_TO_TEXT_FILE
	};

	static CSaveGameMigrationBuffer sm_BufferContainingLoadedPsoData;
	static CSaveGameMigrationBuffer sm_BufferContainingXmlDataForSaving_Uncompressed;
	static CSaveGameMigrationBuffer sm_BufferContainingXmlDataForSaving_Compressed;

//	Created in CreateSaveDataAndCalculateSize and deleted after saving in SaveBlockData
	static parTree *sm_pParTreeToBeSaved;

	static CSaveGameMigrationData_Code sm_SaveGameBufferContents_Code;
	static CSaveGameMigrationData_Script sm_SaveGameBufferContents_Script;

	static CSaveGameMigrationMetadata sm_SaveGameMetadata;

	static SaveBlockDataForExportStatus sm_SaveBlockDataForExportStatus;

	static eSaveGameExportProgress sm_SaveGameExportProgress;

// For writing the XML data to a text file on the PC
// This is just for checking the contents of the XML data
// Eventually we will be uploading the XML buffer
	static const char *sm_XMLTextDirectoryName;
	static const char *sm_XMLTextFilename;

	static bool sm_bIsExporting;

//	static bool sm_UseEncryptionForSaveBlockData;

	static eSaveMigrationStatus sm_StatusOfPreviouslyUploadedSavegameCheck;
	static s32 sm_UploadErrorCodeOfPreviouslyUploadedSavegameCheck;
	static eNetworkProblemsForSavegameExport sm_NetworkProblemDuringCheckForPreviousUpload;
	static bool sm_bTransferAlreadyCompleted;
	static CSaveGameExport_DetailsOfPreviouslyUploadedSavegame sm_DetailsOfPreviouslyUploadedSavegame;

	static eSaveMigrationStatus sm_UploadStatusOfLastUploadAttempt;
	static s32 sm_UploadErrorCodeOfLastUploadAttempt;
	static eNetworkProblemsForSavegameExport sm_NetworkProblemDuringUpload;

	static const u32 MAX_LENGTH_OF_ERROR_CODE_STRING = 16;
	static char sm_ErrorCodeAsString[MAX_LENGTH_OF_ERROR_CODE_STRING];
};

#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

#endif // SAVEGAME_EXPORT_H
