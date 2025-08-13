#ifndef SAVEGAME_IMPORT_H
#define SAVEGAME_IMPORT_H

// Game headers
#include "SaveLoad/savegame_defines.h"

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES

#include "SaveLoad/SavegameMigration/savegame_migration_buffer.h"
#include "SaveLoad/SavegameMigration/savegame_migration_data_code.h"
#include "SaveLoad/SavegameMigration/savegame_migration_data_script.h"


class CSaveGameImport
{
public:
	//	Public functions
	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);

	// This is an earlier version of the code that was used for test purposes.
	// It reads the XML file from the PC's hard drive.
	static bool BeginImport();

	static bool BeginImportOfDownloadedData();
	static bool CreateCopyOfDownloadedData(const u8 *pData, u32 dataSize);

	static MemoryCardError UpdateImport();

	static void SetIsImporting(bool bIsImporting) { sm_bIsImporting = bIsImporting; }
	static bool GetIsImporting() { return sm_bIsImporting; }

private:
	static bool GetSizeOfXmlFile();
	static bool ReadXmlDataFromFile();
	static bool DecompressLoadedXmlData();
	static bool ReadXmlDataFromBuffer();

	static MemoryCardError CreateSaveDataAndCalculateSize();

	// Kicks off a thread that runs the SaveBlockData operation
	static bool BeginSaveBlockData();
	static void SaveBlockDataForImportThreadFunc(void*);
	static bool DoSaveBlockData(); //Called by SaveBlockDataForImportThreadFunc
	static void EndSaveBlockData();

// Functions to save the proper savegame
	static MemoryCardError BeginInitialChecks();
	static MemoryCardError InitialChecks();
	static MemoryCardError BeginSave();
	static MemoryCardError CheckSave();
#if RSG_ORBIS
	static MemoryCardError BeginGetModifiedTime();
	static MemoryCardError GetModifiedTime();
#endif // RSG_ORBIS
	static MemoryCardError BeginIconSave(char *pSaveGameIcon, u32 sizeOfSaveGameIcon);
	static MemoryCardError CheckIconSave();

	static void FreeAllData();

	static void SetDisplayNameForSavegame();

//	Private data
	enum SaveBlockDataForImportStatus
	{
		SAVEBLOCKDATAFORIMPORT_IDLE,
		SAVEBLOCKDATAFORIMPORT_PENDING,
		SAVEBLOCKDATAFORIMPORT_ERROR,
		SAVEBLOCKDATAFORIMPORT_SUCCESS
	};

	enum eSaveGameImportProgress
	{
		SAVEGAME_IMPORT_PROGRESS_GET_SIZE_OF_XML_FILE,
		SAVEGAME_IMPORT_PROGRESS_ALLOCATE_COMPRESSED_BUFFER_FOR_XML_DATA,
		SAVEGAME_IMPORT_PROGRESS_READ_XML_DATA_FROM_FILE,
		SAVEGAME_IMPORT_PROGRESS_DECOMPRESS_XML_DATA,
		SAVEGAME_IMPORT_PROGRESS_READ_XML_DATA_FROM_BUFFER,
		SAVEGAME_IMPORT_PROGRESS_CREATE_PSO_SAVE_DATA,
		SAVEGAME_IMPORT_PROGRESS_ALLOCATE_SAVE_BUFFER,
		SAVEGAME_IMPORT_PROGRESS_BEGIN_WRITING_TO_SAVE_BUFFER,
		SAVEGAME_IMPORT_PROGRESS_CHECK_WRITING_TO_SAVE_BUFFER,

		SAVEGAME_IMPORT_PROGRESS_BEGIN_INITIAL_CHECKS,
		SAVEGAME_IMPORT_PROGRESS_INITIAL_CHECKS,
		SAVEGAME_IMPORT_PROGRESS_BEGIN_SAVE,
		SAVEGAME_IMPORT_PROGRESS_CHECK_SAVE,
#if RSG_ORBIS
		SAVEGAME_IMPORT_PROGRESS_BEGIN_GET_MODIFIED_TIME,
		SAVEGAME_IMPORT_PROGRESS_GET_MODIFIED_TIME,
#endif	//	RSG_ORBIS
		SAVEGAME_IMPORT_PROGRESS_BEGIN_ICON_SAVE,
		SAVEGAME_IMPORT_PROGRESS_CHECK_ICON_SAVE
//		SAVEGAME_IMPORT_PROGRESS_HANDLE_ERRORS
	};

	static CSaveGameMigrationBuffer sm_BufferContainingLoadedXmlData_Compressed;
	static CSaveGameMigrationBuffer sm_BufferContainingLoadedXmlData_Uncompressed;
	static CSaveGameMigrationBuffer sm_BufferContainingPsoDataForSaving;

	static CSaveGameMigrationData_Code sm_SaveGameBufferContents_Code;
	static CSaveGameMigrationData_Script sm_SaveGameBufferContents_Script;

	//	Created in CreateSaveDataAndCalculateSize and deleted after saving in SaveBlockData
	static psoBuilder *m_pPsoBuilderToBeSaved;

	static SaveBlockDataForImportStatus sm_SaveBlockDataForImportStatus;

	static eSaveGameImportProgress sm_SaveGameImportProgress;

	// For reading the XML data from a text file on the PC
	// Eventually we will be downloading the XML buffer instead
	static const char *sm_XMLTextDirectoryName;
	static const char *sm_XMLTextFilename;

	static bool sm_bIsImporting;
};


#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

#endif // SAVEGAME_IMPORT_H
