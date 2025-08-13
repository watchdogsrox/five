
#include "savegame_import.h"

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES

// Rage headers
#include "zlib/lz4.h"

// Framework headers
#include "fwsys/gameskeleton.h"

// Game headers
#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/savegame_frontend.h"
#include "SaveLoad/savegame_initial_checks.h"
#include "SaveLoad/savegame_save.h"
#include "SaveLoad/savegame_users_and_devices.h"
#include "SaveLoad/SavegameScriptData/script_save_data.h"

const u32 s_MaxBufferSize_LoadedXmlData_Compressed = 220000;	//	This might need to be increased
CSaveGameMigrationBuffer CSaveGameImport::sm_BufferContainingLoadedXmlData_Compressed(s_MaxBufferSize_LoadedXmlData_Compressed);

const u32 s_MaxBufferSize_LoadedXmlData_Uncompressed = 2500000;	//	This might need to be increased
CSaveGameMigrationBuffer CSaveGameImport::sm_BufferContainingLoadedXmlData_Uncompressed(s_MaxBufferSize_LoadedXmlData_Uncompressed);


const u32 s_MaxBufferSize_SavedPsoData = 640000;
CSaveGameMigrationBuffer CSaveGameImport::sm_BufferContainingPsoDataForSaving(s_MaxBufferSize_SavedPsoData);

CSaveGameMigrationData_Code CSaveGameImport::sm_SaveGameBufferContents_Code;
CSaveGameMigrationData_Script CSaveGameImport::sm_SaveGameBufferContents_Script;

psoBuilder *CSaveGameImport::m_pPsoBuilderToBeSaved = NULL;

CSaveGameImport::SaveBlockDataForImportStatus CSaveGameImport::sm_SaveBlockDataForImportStatus = SAVEBLOCKDATAFORIMPORT_IDLE;
CSaveGameImport::eSaveGameImportProgress CSaveGameImport::sm_SaveGameImportProgress = SAVEGAME_IMPORT_PROGRESS_GET_SIZE_OF_XML_FILE;
bool CSaveGameImport::sm_bIsImporting = false;

const char *CSaveGameImport::sm_XMLTextDirectoryName = "X:\\";
const char *CSaveGameImport::sm_XMLTextFilename = "exported_savegame_compressed.xml";


void CSaveGameImport::Init(unsigned UNUSED_PARAM(initMode))
{
	m_pPsoBuilderToBeSaved = NULL;

	sm_bIsImporting = false;
	sm_SaveBlockDataForImportStatus = SAVEBLOCKDATAFORIMPORT_IDLE;
	sm_SaveGameImportProgress = SAVEGAME_IMPORT_PROGRESS_GET_SIZE_OF_XML_FILE;
}


void CSaveGameImport::Shutdown(unsigned shutdownMode)
{
	if (shutdownMode == SHUTDOWN_SESSION)
	{
		FreeAllData();
	}
}


bool CSaveGameImport::BeginImport()
{
	sm_SaveGameImportProgress = SAVEGAME_IMPORT_PROGRESS_GET_SIZE_OF_XML_FILE;
	return true;
}


bool CSaveGameImport::BeginImportOfDownloadedData()
{
	sm_SaveGameImportProgress = SAVEGAME_IMPORT_PROGRESS_DECOMPRESS_XML_DATA;
	return true;
}

bool CSaveGameImport::CreateCopyOfDownloadedData(const u8 *pData, u32 dataSize)
{
	sm_BufferContainingLoadedXmlData_Compressed.SetBufferSize(dataSize);
	if (sm_BufferContainingLoadedXmlData_Compressed.AllocateBuffer() == MEM_CARD_COMPLETE)
	{
		memcpy(sm_BufferContainingLoadedXmlData_Compressed.GetBuffer(), pData, dataSize);
		return true;
	}

	savegameErrorf("CSaveGameImport::CreateCopyOfDownloadedData - failed to allocate %u bytes for copy of data", dataSize);
	savegameAssertf(0, "CSaveGameImport::CreateCopyOfDownloadedData - failed to allocate %u bytes for copy of data", dataSize);
	return false;
}


MemoryCardError CSaveGameImport::UpdateImport()
{
	MemoryCardError returnValue = MEM_CARD_BUSY;

	switch (sm_SaveGameImportProgress)
	{
		case SAVEGAME_IMPORT_PROGRESS_GET_SIZE_OF_XML_FILE :
			{
				// Eventually the exported data will be downloaded from the cloud
				// For now, read it from a text file on the PC.
				if (GetSizeOfXmlFile())
				{
					sm_SaveGameImportProgress = SAVEGAME_IMPORT_PROGRESS_ALLOCATE_COMPRESSED_BUFFER_FOR_XML_DATA;
				}
				else
				{
					returnValue = MEM_CARD_ERROR;
				}
			}
			break;

		case SAVEGAME_IMPORT_PROGRESS_ALLOCATE_COMPRESSED_BUFFER_FOR_XML_DATA :
			{
				switch (sm_BufferContainingLoadedXmlData_Compressed.AllocateBuffer())
				{
				case MEM_CARD_COMPLETE :
					sm_SaveGameImportProgress = SAVEGAME_IMPORT_PROGRESS_READ_XML_DATA_FROM_FILE;
					break;

				case MEM_CARD_BUSY :
					savegameDebugf1("CSaveGameImport::UpdateImport - waiting for sm_BufferContainingLoadedXmlData_Compressed.AllocateBuffer() to succeed");
					break;
				case MEM_CARD_ERROR :
					savegameErrorf("CSaveGameImport::UpdateImport - sm_BufferContainingLoadedXmlData_Compressed.AllocateBuffer() failed");
					returnValue = MEM_CARD_ERROR;
					break;
				}
			}
			break;

		case SAVEGAME_IMPORT_PROGRESS_READ_XML_DATA_FROM_FILE :
			{
				if (ReadXmlDataFromFile())
				{
					sm_SaveGameImportProgress = SAVEGAME_IMPORT_PROGRESS_DECOMPRESS_XML_DATA;
				}
				else
				{
					returnValue = MEM_CARD_ERROR;
				}
			}
			break;

		case SAVEGAME_IMPORT_PROGRESS_DECOMPRESS_XML_DATA :
			{
				if (DecompressLoadedXmlData())
				{
					sm_SaveGameImportProgress = SAVEGAME_IMPORT_PROGRESS_READ_XML_DATA_FROM_BUFFER;
				}
				else
				{
					returnValue = MEM_CARD_ERROR;
				}
			}
			break;

		case SAVEGAME_IMPORT_PROGRESS_READ_XML_DATA_FROM_BUFFER :
			{
				if (ReadXmlDataFromBuffer())
				{
					sm_SaveGameImportProgress = SAVEGAME_IMPORT_PROGRESS_CREATE_PSO_SAVE_DATA;
				}
				else
				{
					returnValue = MEM_CARD_ERROR;
				}

				// The loaded XML buffer is not needed after this, so free it now
				sm_BufferContainingLoadedXmlData_Compressed.FreeBuffer();
				sm_BufferContainingLoadedXmlData_Uncompressed.FreeBuffer();
			}
			break;

		case SAVEGAME_IMPORT_PROGRESS_CREATE_PSO_SAVE_DATA :
			{
				switch (CreateSaveDataAndCalculateSize())
				{
				case MEM_CARD_COMPLETE :
					sm_SaveGameImportProgress = SAVEGAME_IMPORT_PROGRESS_ALLOCATE_SAVE_BUFFER;
					break;

				case MEM_CARD_BUSY :
					savegameErrorf("CSaveGameImport::UpdateImport - didn't expect CreateSaveDataAndCalculateSize() to ever return MEM_CARD_BUSY");
					returnValue = MEM_CARD_ERROR;
					break;
				case MEM_CARD_ERROR :
					savegameErrorf("CSaveGameImport::UpdateImport - CreateSaveDataAndCalculateSize() returned MEM_CARD_ERROR");
					returnValue = MEM_CARD_ERROR;
					break;
				}
			}
			break;

		case SAVEGAME_IMPORT_PROGRESS_ALLOCATE_SAVE_BUFFER :
			{
				switch (sm_BufferContainingPsoDataForSaving.AllocateBuffer())
				{
				case MEM_CARD_COMPLETE :
					sm_SaveGameImportProgress = SAVEGAME_IMPORT_PROGRESS_BEGIN_WRITING_TO_SAVE_BUFFER;
					break;

				case MEM_CARD_BUSY :
					savegameDebugf1("CSaveGameImport::UpdateImport - waiting for sm_BufferContainingPsoDataForSaving.AllocateBuffer() to succeed");
					break;
				case MEM_CARD_ERROR :
					savegameErrorf("CSaveGameImport::UpdateImport - sm_BufferContainingPsoDataForSaving.AllocateBuffer() failed");
					returnValue = MEM_CARD_ERROR;
					break;
				}
			}
			break;

		case SAVEGAME_IMPORT_PROGRESS_BEGIN_WRITING_TO_SAVE_BUFFER :
			{
				if (BeginSaveBlockData())
				{
					sm_SaveGameImportProgress = SAVEGAME_IMPORT_PROGRESS_CHECK_WRITING_TO_SAVE_BUFFER;
				}
				else
				{
					savegameErrorf("CSaveGameImport::UpdateImport - BeginSaveBlockData() failed");
					EndSaveBlockData();
					returnValue = MEM_CARD_ERROR;
				}
			}
			break;

		case SAVEGAME_IMPORT_PROGRESS_CHECK_WRITING_TO_SAVE_BUFFER :
			{
				switch (sm_SaveBlockDataForImportStatus)
				{
				case SAVEBLOCKDATAFORIMPORT_IDLE :
					savegameErrorf("CSaveGameImport::UpdateImport - didn't expect status to be IDLE during the writing of the save buffer");
					EndSaveBlockData();
					returnValue = MEM_CARD_ERROR;
					break;

				case SAVEBLOCKDATAFORIMPORT_PENDING :
					// Don't do anything - still waiting for the data to finish being written
					break;

				case SAVEBLOCKDATAFORIMPORT_ERROR :
					savegameErrorf("CSaveGameImport::UpdateImport - status has been set to ERROR while writing the save buffer");
					EndSaveBlockData();
					returnValue = MEM_CARD_ERROR;
					break;

				case SAVEBLOCKDATAFORIMPORT_SUCCESS :
					savegameDebugf1("CSaveGameImport::UpdateImport - writing of the save buffer has succeeded");
					EndSaveBlockData();
					sm_SaveGameImportProgress = SAVEGAME_IMPORT_PROGRESS_BEGIN_INITIAL_CHECKS;
					break;
				}
			}
			break;

		case SAVEGAME_IMPORT_PROGRESS_BEGIN_INITIAL_CHECKS :
			{
				switch (BeginInitialChecks())
				{
					case MEM_CARD_COMPLETE :
						sm_SaveGameImportProgress = SAVEGAME_IMPORT_PROGRESS_INITIAL_CHECKS;
						break;

					case MEM_CARD_BUSY :
						break;

					case MEM_CARD_ERROR :
						savegameErrorf("CSaveGameImport::UpdateImport - BeginInitialChecks() failed");
						returnValue = MEM_CARD_ERROR;
						break;
				}
			}
			break;

		case SAVEGAME_IMPORT_PROGRESS_INITIAL_CHECKS :
			{
				switch (InitialChecks())
				{
				case MEM_CARD_COMPLETE :
					sm_SaveGameImportProgress = SAVEGAME_IMPORT_PROGRESS_BEGIN_SAVE;
					break;

				case MEM_CARD_BUSY :
					break;

				case MEM_CARD_ERROR :
					savegameErrorf("CSaveGameImport::UpdateImport - InitialChecks() failed");
					returnValue = MEM_CARD_ERROR;
					break;
				}
			}
			break;

		case SAVEGAME_IMPORT_PROGRESS_BEGIN_SAVE :
			{
				switch (BeginSave())
				{
					case MEM_CARD_COMPLETE :
						sm_SaveGameImportProgress = SAVEGAME_IMPORT_PROGRESS_CHECK_SAVE;
						break;

					case MEM_CARD_BUSY :
						break;

					case MEM_CARD_ERROR :
						savegameErrorf("CSaveGameImport::UpdateImport - BeginSave() failed");
						returnValue = MEM_CARD_ERROR;
						break;
				}
			}
			break;

		case SAVEGAME_IMPORT_PROGRESS_CHECK_SAVE :
			{
				switch (CSaveGameImport::CheckSave())
				{
				case MEM_CARD_COMPLETE :
#if RSG_ORBIS
					sm_SaveGameImportProgress = SAVEGAME_IMPORT_PROGRESS_BEGIN_GET_MODIFIED_TIME;
#else
					sm_SaveGameImportProgress = SAVEGAME_IMPORT_PROGRESS_BEGIN_ICON_SAVE;
#endif	//	RSG_ORBIS
					break;

				case MEM_CARD_BUSY :	 // the save is still in progress
					break;

				case MEM_CARD_ERROR :
					savegameErrorf("CSaveGameImport::UpdateImport - CheckSave() failed");
					returnValue = MEM_CARD_ERROR;
					break;
				}
			}
			break;

#if RSG_ORBIS
		case SAVEGAME_IMPORT_PROGRESS_BEGIN_GET_MODIFIED_TIME :
			{
				switch (CSaveGameImport::BeginGetModifiedTime())
				{
					case MEM_CARD_COMPLETE :
						sm_SaveGameImportProgress = SAVEGAME_IMPORT_PROGRESS_GET_MODIFIED_TIME;
						break;

					case MEM_CARD_BUSY :
						break;

					case MEM_CARD_ERROR :
						savegameErrorf("CSaveGameImport::UpdateImport - BeginGetModifiedTime() failed");
						returnValue = MEM_CARD_ERROR;
						break;
				}
			}
			break;

		case SAVEGAME_IMPORT_PROGRESS_GET_MODIFIED_TIME :
			{
				switch (GetModifiedTime())
				{
				case MEM_CARD_COMPLETE :
					sm_SaveGameImportProgress = SAVEGAME_IMPORT_PROGRESS_BEGIN_ICON_SAVE;
					break;

				case MEM_CARD_BUSY :
					break;

				case MEM_CARD_ERROR :
					savegameErrorf("CSaveGameImport::UpdateImport - GetModifiedTime() failed");
					returnValue = MEM_CARD_ERROR;
					break;
				}
			}
			break;
#endif	//	RSG_ORBIS

		case SAVEGAME_IMPORT_PROGRESS_BEGIN_ICON_SAVE :
			{
				char *pSaveGameIcon = CSavegameSave::GetSaveGameIcon();
				u32 sizeOfSaveGameIcon = CSavegameSave::GetSizeOfSaveGameIcon();

				if (pSaveGameIcon)
				{
					switch (BeginIconSave(pSaveGameIcon, sizeOfSaveGameIcon))
					{
					case MEM_CARD_COMPLETE :
						sm_SaveGameImportProgress = SAVEGAME_IMPORT_PROGRESS_CHECK_ICON_SAVE;
						break;

					case MEM_CARD_BUSY :
						break;

					case MEM_CARD_ERROR :
						savegameErrorf("CSaveGameImport::UpdateImport - BeginIconSave() failed");
//						savegameAssertf(0, "CSaveGameImport::UpdateImport - BeginIconSave() failed");
						returnValue = MEM_CARD_COMPLETE;	// Saving the icon isn't essential so set MEM_CARD_COMPLETE instead of MEM_CARD_ERROR
						break;
					}
				}
				else
				{
					savegameErrorf("CSaveGameImport::UpdateImport - failed to get savegame icon");
					savegameAssertf(0, "CSaveGameImport::UpdateImport - failed to get savegame icon");
					returnValue = MEM_CARD_COMPLETE;	// Saving the icon isn't essential so set MEM_CARD_COMPLETE instead of MEM_CARD_ERROR
				}
			}
			break;

		case SAVEGAME_IMPORT_PROGRESS_CHECK_ICON_SAVE :
			{
				switch (CheckIconSave())
				{
				case MEM_CARD_COMPLETE :
					returnValue = MEM_CARD_COMPLETE;
					break;

				case MEM_CARD_BUSY :
					break;

				case MEM_CARD_ERROR :
					savegameErrorf("CSaveGameImport::UpdateImport - CheckIconSave() failed");
//					savegameAssertf(0, "CSaveGameImport::UpdateImport - CheckIconSave() failed");
					returnValue = MEM_CARD_COMPLETE;	// Saving the icon isn't essential so set MEM_CARD_COMPLETE instead of MEM_CARD_ERROR
					break;
				}
			}
			break;
	}


	if ( (returnValue == MEM_CARD_COMPLETE) || (returnValue == MEM_CARD_ERROR) )
	{
		FreeAllData();

		// I think I might need to call this because I will probably have set it to OPERATION_SAVING_SAVEGAME_TO_CONSOLE
		CGenericGameStorage::SetSaveOperation(OPERATION_NONE);

		// Do this too?
		CSavingMessage::Clear();
	}

	return returnValue;
}


void CSaveGameImport::FreeAllData()
{
	sm_BufferContainingLoadedXmlData_Compressed.FreeBuffer();
	sm_BufferContainingLoadedXmlData_Uncompressed.FreeBuffer();
	sm_BufferContainingPsoDataForSaving.FreeBuffer();

	sm_SaveGameBufferContents_Code.Delete();
	sm_SaveGameBufferContents_Script.Delete();

	delete m_pPsoBuilderToBeSaved;
	m_pPsoBuilderToBeSaved = NULL;
}


bool CSaveGameImport::GetSizeOfXmlFile()
{
	bool bReturnValue = false;

	ASSET.PushFolder(sm_XMLTextDirectoryName);

	FileHandle fileHandle = CFileMgr::OpenFile(sm_XMLTextFilename);
	if (savegameVerifyf(CFileMgr::IsValidFileHandle(fileHandle), "CSaveGameImport::GetSizeOfXmlFile - failed to open text file %s on PC", sm_XMLTextFilename))
	{
		s32 fileSize = CFileMgr::GetTotalSize(fileHandle);
		if (savegameVerifyf(fileSize > 0, "CSaveGameImport::GetSizeOfXmlFile - fileSize is %d", fileSize))
		{
			sm_BufferContainingLoadedXmlData_Compressed.SetBufferSize(fileSize);
			bReturnValue = true;
		}
		CFileMgr::CloseFile(fileHandle);
	}
	ASSET.PopFolder();

	return bReturnValue;
}


bool CSaveGameImport::ReadXmlDataFromFile()
{
	bool bReturnValue = false;

	ASSET.PushFolder(sm_XMLTextDirectoryName);

	FileHandle fileHandle = CFileMgr::OpenFile(sm_XMLTextFilename);
	if (savegameVerifyf(CFileMgr::IsValidFileHandle(fileHandle), "CSaveGameImport::ReadXmlDataFromFile - failed to open text file %s on PC", sm_XMLTextFilename))
	{
		s32 amountRead = CFileMgr::Read(fileHandle, (char*) sm_BufferContainingLoadedXmlData_Compressed.GetBuffer(), sm_BufferContainingLoadedXmlData_Compressed.GetBufferSize());
		if (amountRead == (s32)sm_BufferContainingLoadedXmlData_Compressed.GetBufferSize())
		{
			bReturnValue = true;
		}
		else
		{
			savegameErrorf("CSaveGameImport::ReadXmlDataFromFile - read %d bytes, expected to read %u bytes", amountRead, sm_BufferContainingLoadedXmlData_Compressed.GetBufferSize());
			savegameAssertf(0, "CSaveGameImport::ReadXmlDataFromFile - read %d bytes, expected to read %u bytes", amountRead, sm_BufferContainingLoadedXmlData_Compressed.GetBufferSize());
		}
		
		CFileMgr::CloseFile(fileHandle);
	}
	ASSET.PopFolder();

	return bReturnValue;
}


bool CSaveGameImport::DecompressLoadedXmlData()
{
	// Decrypt the buffer
	AES aes(AES_KEY_ID_GTA5SAVE_MIGRATION);
	aes.Decrypt(sm_BufferContainingLoadedXmlData_Compressed.GetBuffer(), sm_BufferContainingLoadedXmlData_Compressed.GetBufferSize());

	const char *pHeaderId = "COMP";
	const u32 sizeOfHeaderId = 4;
	const u32 sizeOfHeaderFieldContainingUncompressedSize = 4;
	const u32 sizeOfHeader = (sizeOfHeaderId + sizeOfHeaderFieldContainingUncompressedSize);

	u8 *pCompressedBuffer = sm_BufferContainingLoadedXmlData_Compressed.GetBuffer();

	if (strncmp((reinterpret_cast<const char*>(pCompressedBuffer)), pHeaderId, sizeOfHeaderId) == 0)
	{
		// Get the size of the uncompressed buffer
		u32 sourceSizeL = 0;
		memcpy(&sourceSizeL, pCompressedBuffer + sizeOfHeaderId, sizeOfHeaderFieldContainingUncompressedSize);
		u32 uncompressedSize = sysEndian::LtoN(sourceSizeL);

		sm_BufferContainingLoadedXmlData_Uncompressed.SetBufferSize(uncompressedSize);
		if (sm_BufferContainingLoadedXmlData_Uncompressed.AllocateBuffer() != MEM_CARD_COMPLETE)
		{
			savegameErrorf("CSaveGameImport::DecompressLoadedXmlData - failed to allocate memory for uncompressed buffer");
			savegameAssertf(0, "CSaveGameImport::DecompressLoadedXmlData - failed to allocate memory for uncompressed buffer");
			return false;
		}

		int returnValue = LZ4_decompress_fast((const char*) pCompressedBuffer + sizeOfHeader, 
			(char*) sm_BufferContainingLoadedXmlData_Uncompressed.GetBuffer(), 
			sm_BufferContainingLoadedXmlData_Uncompressed.GetBufferSize());

		if(returnValue <= 0)
		{
			savegameErrorf("CSaveGameImport::DecompressLoadedXmlData - LZ4_decompress_fast failed");
			savegameAssertf(0, "CSaveGameImport::DecompressLoadedXmlData - LZ4_decompress_fast failed");
			return false;
		}
	}
	else
	{
		savegameErrorf("CSaveGameImport::DecompressLoadedXmlData - expected the first four bytes of the compressed buffer to contain '%s'", pHeaderId);
		savegameAssertf(0, "CSaveGameImport::DecompressLoadedXmlData - expected the first four bytes of the compressed buffer to contain '%s'", pHeaderId);
		return false;
	}

	return true;
}


bool CSaveGameImport::ReadXmlDataFromBuffer()
{
	bool bReturnValue = false;

	u8* buffer = sm_BufferContainingLoadedXmlData_Uncompressed.GetBuffer();
	u32 bufferSize = sm_BufferContainingLoadedXmlData_Uncompressed.GetBufferSize();

	char memFileName[RAGE_MAX_PATH];
	fiDevice::MakeMemoryFileName(memFileName, RAGE_MAX_PATH, buffer, bufferSize, false, "");

	const int inputFormat = PARSER.FindInputFormat(*(reinterpret_cast<u32*>(buffer)));
	if (inputFormat == parManager::XML) 
	{
		fiStream* pReadStream = ASSET.Open(memFileName, "");

		{
			savegameAssertf(!PARSER.Settings().GetFlag(parSettings::USE_TEMPORARY_HEAP), "CSaveGameImport::ReadXmlDataFromBuffer - Graeme - I'm not dealing with AutoUseTempMemory properly");

			parTree *pLoadedParTree = PARSER.LoadTree(pReadStream, NULL);
			if (pLoadedParTree)
			{
				if (pLoadedParTree->GetRoot())
				{
					if (sm_SaveGameBufferContents_Code.ReadDataFromParTree(pLoadedParTree))
					{
						if (sm_SaveGameBufferContents_Script.ReadDataFromParTree(pLoadedParTree))
						{
							bReturnValue = true;
						}
						else
						{
							savegameErrorf("CSaveGameImport::ReadXmlDataFromBuffer - Failed to read script save data from pLoadedParTree");
							savegameAssertf(0, "CSaveGameImport::ReadXmlDataFromBuffer - Failed to read script save data from pLoadedParTree");
						}
					}
					else
					{
						savegameErrorf("CSaveGameImport::ReadXmlDataFromBuffer - Failed to read code save data from pLoadedParTree");
						savegameAssertf(0, "CSaveGameImport::ReadXmlDataFromBuffer - Failed to read code save data from pLoadedParTree");
					}
				}
				else
				{
					savegameErrorf("CSaveGameImport::ReadXmlDataFromBuffer - Failed to find root for parTree loaded from file '%s'", pReadStream->GetName());
					savegameAssertf(0, "CSaveGameImport::ReadXmlDataFromBuffer - Failed to find root for parTree loaded from file '%s'", pReadStream->GetName());
				}

				{
					parManager::AutoUseTempMemory temp;

					delete pLoadedParTree;
					pLoadedParTree = NULL;
				}
			}
			else
			{
				savegameErrorf("CSaveGameImport::ReadXmlDataFromBuffer - Couldn't load from file '%s'. File was empty or incorrect format", pReadStream->GetName());
				savegameAssertf(0, "CSaveGameImport::ReadXmlDataFromBuffer - Couldn't load from file '%s'. File was empty or incorrect format", pReadStream->GetName());
			}
		}

		savegameDebugf2("CSaveGameImport::ReadXmlDataFromBuffer - Done loading from stream '%s'", pReadStream->GetName());

		pReadStream->Close();
	}
	else
	{
		savegameErrorf("CSaveGameImport::ReadXmlDataFromBuffer - Expected PARSER.FindInputFormat() to be parManager::XML, but it's %d. memFileName='%s'", inputFormat, memFileName);
		savegameAssertf(0, "CSaveGameImport::ReadXmlDataFromBuffer - Expected PARSER.FindInputFormat() to be parManager::XML, but it's %d. memFileName='%s'", inputFormat, memFileName);
	}

	return bReturnValue;
}

#define Align16(x) (((x)+15)&~15)

// Mostly copied from CSaveGameBuffer::CreateSaveDataAndCalculateSize()
MemoryCardError CSaveGameImport::CreateSaveDataAndCalculateSize()
{
	if (m_pPsoBuilderToBeSaved)
	{
		savegameAssertf(0, "CSaveGameImport::CreateSaveDataAndCalculateSize - didn't expect m_pPsoBuilderToBeSaved to be allocated at this stage");
		delete m_pPsoBuilderToBeSaved;
		m_pPsoBuilderToBeSaved = NULL;
	}

	m_pPsoBuilderToBeSaved = rage_new psoBuilder;
	m_pPsoBuilderToBeSaved->IncludeChecksum();

	psoBuilderInstance *pBuilderInstance = sm_SaveGameBufferContents_Code.ConstructPsoDataToSave(m_pPsoBuilderToBeSaved);

	CScriptSaveData::AddActiveScriptDataToPsoToBeSaved(SAVEGAME_SINGLE_PLAYER, *m_pPsoBuilderToBeSaved, pBuilderInstance);

	m_pPsoBuilderToBeSaved->FinishBuilding();

	u32 psoFileSize = m_pPsoBuilderToBeSaved->GetFileSize();
	savegameDisplayf("CSaveGameImport::CreateSaveDataAndCalculateSize - Size of PSO Buffer is %u", psoFileSize);
	sm_BufferContainingPsoDataForSaving.SetBufferSize(Align16(psoFileSize)); // need alignment for encryption

	return MEM_CARD_COMPLETE;
}


void CSaveGameImport::SaveBlockDataForImportThreadFunc(void* )
{
	PF_PUSH_MARKER("SaveBlockDataForImport");

	bool bSuccess = DoSaveBlockData();
	if(bSuccess)
	{
		sm_SaveBlockDataForImportStatus = SAVEBLOCKDATAFORIMPORT_SUCCESS;
	}
	else
	{
		sm_SaveBlockDataForImportStatus = SAVEBLOCKDATAFORIMPORT_ERROR;
	}

#if !__FINAL
	unsigned current, maxi;
	if (sysIpcEstimateStackUsage(current,maxi))
		savegameDisplayf("Save game thread used %u of %u bytes stack space.",current,maxi);
#endif

	PF_POP_MARKER();
}

static sysIpcThreadId s_ImportThreadId = sysIpcThreadIdInvalid;

bool CSaveGameImport::BeginSaveBlockData()
{
	if (s_ImportThreadId != sysIpcThreadIdInvalid) 
	{
		Errorf("CSaveGameImport::BeginSaveBlockData started twice. Previous thread not finished, can't start.");
		return false;
	}

	FatalAssertf(sm_SaveBlockDataForImportStatus == SAVEBLOCKDATAFORIMPORT_IDLE, "CSaveGameImport::BeginSaveBlockData called twice");
	sm_SaveBlockDataForImportStatus = SAVEBLOCKDATAFORIMPORT_PENDING;

	s_ImportThreadId = sysIpcCreateThread(SaveBlockDataForImportThreadFunc, NULL, 16384, PRIO_BELOW_NORMAL, "SaveBlockDataThread", 0, "SaveBlockDataThread");
	return s_ImportThreadId != sysIpcThreadIdInvalid;
}


bool CSaveGameImport::DoSaveBlockData()
{
	savegameAssertf(m_pPsoBuilderToBeSaved, "CSaveGameImport::DoSaveBlockData - Couldn't find a PSO builder object to save");

	m_pPsoBuilderToBeSaved->SaveToBuffer(reinterpret_cast<char*>(sm_BufferContainingPsoDataForSaving.GetBuffer()), sm_BufferContainingPsoDataForSaving.GetBufferSize());

	savegameDisplayf("CSaveGameImport::DoSaveBlockData - size before encryption = %u", sm_BufferContainingPsoDataForSaving.GetBufferSize());

	//Now encrypt the buffer
	AES aes(AES_KEY_ID_SAVEGAME);
	aes.Encrypt(sm_BufferContainingPsoDataForSaving.GetBuffer(), sm_BufferContainingPsoDataForSaving.GetBufferSize());

	savegameDisplayf("CSaveGameImport::DoSaveBlockData - size after encryption = %u", sm_BufferContainingPsoDataForSaving.GetBufferSize());

	return true;
}


void CSaveGameImport::EndSaveBlockData()
{
	if (s_ImportThreadId != sysIpcThreadIdInvalid)
	{
		sysIpcWaitThreadExit(s_ImportThreadId);
		s_ImportThreadId = sysIpcThreadIdInvalid;
	}

	sm_SaveBlockDataForImportStatus = SAVEBLOCKDATAFORIMPORT_IDLE;
}



// GraemeSW
// Should I allow the Autosave slot to be overwritten by an imported savegame?

// During normal gameplay, the PS4 game saves a backup copy of the savegame at the same time as it saves an autosave or manual save.
// I'm not currently doing that when importing a savegame.

MemoryCardError CSaveGameImport::BeginInitialChecks()
{
	if (CGenericGameStorage::GetSaveOperation() != OPERATION_NONE)
	{
		savegameErrorf("CSaveGameImport::BeginInitialChecks - CGenericGameStorage::GetSaveOperation() is not OPERATION_NONE");
		savegameAssertf(0, "CSaveGameImport::BeginInitialChecks - CGenericGameStorage::GetSaveOperation() is not OPERATION_NONE");
		return MEM_CARD_ERROR;
	}

	s32 savegameSlotNumber = CSavegameFrontEnd::GetSaveGameSlotToImport();

	CSavegameFilenames::MakeValidSaveNameForLocalFile(savegameSlotNumber);

	// Set the display name using the stats for last mission passed and completion percentage
	SetDisplayNameForSavegame();

	CSavegameInitialChecks::SetSizeOfBufferToBeSaved(sm_BufferContainingPsoDataForSaving.GetBufferSize());

#if RSG_ORBIS
	CSavegameList::SetSlotToUpdateOnceSaveHasCompleted(savegameSlotNumber);
#endif
	CGenericGameStorage::SetSaveOperation(OPERATION_SAVING_SAVEGAME_TO_CONSOLE);

	CSavegameInitialChecks::BeginInitialChecks(false);
	
	return MEM_CARD_COMPLETE;
}


MemoryCardError CSaveGameImport::InitialChecks()
{
	CSavingMessage::BeginDisplaying(CSavingMessage::STORAGE_DEVICE_MESSAGE_SAVING);

	return CSavegameInitialChecks::InitialChecks();
}


MemoryCardError CSaveGameImport::BeginSave()
{
	if (CSavegameUsers::HasPlayerJustSignedOut())
	{
		savegameErrorf("CSaveGameImport::BeginSave - Player has signed out");
		savegameAssertf(0, "CSaveGameImport::BeginSave - Player has signed out");
//		CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT);
		return MEM_CARD_ERROR;
	}

	if (!SAVEGAME.BeginSave(CSavegameUsers::GetUser(), CGenericGameStorage::GetContentType(), 
		CSavegameFilenames::GetNameToDisplay(),
		CSavegameFilenames::GetFilenameOfLocalFile(),
		sm_BufferContainingPsoDataForSaving.GetBuffer(),
		sm_BufferContainingPsoDataForSaving.GetBufferSize(),
		true) )
	{
		savegameErrorf("CSaveGameImport::BeginSave - SAVEGAME.BeginSave failed");
		savegameAssertf(0, "CSaveGameImport::BeginSave - SAVEGAME.BeginSave failed");
//		CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_BEGIN_SAVE_FAILED);
		return MEM_CARD_ERROR;
	}

	return MEM_CARD_COMPLETE;
}


MemoryCardError CSaveGameImport::CheckSave()
{
	bool bOutIsValid = false;
	bool bFileExists = false;

	if (SAVEGAME.CheckSave(CSavegameUsers::GetUser(), bOutIsValid, bFileExists))
	{	//	returns true for error or success, false for busy

		SAVEGAME.EndSave(CSavegameUsers::GetUser());

		if (CSavegameUsers::HasPlayerJustSignedOut())
		{
			savegameErrorf("CSaveGameImport::CheckSave - Player has signed out");
			savegameAssertf(0, "CSaveGameImport::CheckSave - Player has signed out");
//			CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT);
			return MEM_CARD_ERROR;
		}

		if (SAVEGAME.GetState(CSavegameUsers::GetUser()) == fiSaveGameState::HAD_ERROR)
		{
			savegameErrorf("CSaveGameImport::CheckSave - SAVEGAME.GetState() returned HAD_ERROR");
			savegameAssertf(0, "CSaveGameImport::CheckSave - SAVEGAME.GetState() returned HAD_ERROR");
//			CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_SAVE_FAILED1);
			return MEM_CARD_ERROR;
		}

		if (SAVEGAME.GetError(CSavegameUsers::GetUser()) != fiSaveGame::SAVE_GAME_SUCCESS)
		{
			savegameErrorf("CSaveGameImport::CheckSave - SAVEGAME.GetError() didn't return SAVE_GAME_SUCCESS");
			savegameAssertf(0, "CSaveGameImport::CheckSave - SAVEGAME.GetError() didn't return SAVE_GAME_SUCCESS");
//			CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_SAVE_FAILED2);
			return MEM_CARD_ERROR;
		}

		return MEM_CARD_COMPLETE;
	}

	return MEM_CARD_BUSY;
}


#if RSG_ORBIS
MemoryCardError CSaveGameImport::BeginGetModifiedTime()
{
	if (!SAVEGAME.BeginGetFileModifiedTime(CSavegameUsers::GetUser(), CSavegameFilenames::GetFilenameOfLocalFile()))
	{
		savegameErrorf("CSaveGameImport::BeginGetModifiedTime - SAVEGAME.BeginGetFileModifiedTime() failed");
		savegameAssertf(0, "CSaveGameImport::BeginGetModifiedTime - SAVEGAME.BeginGetFileModifiedTime() failed");
//		CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_BEGIN_GET_FILE_MODIFIED_TIME_FAILED);
		return MEM_CARD_ERROR;
	}
	
	return MEM_CARD_COMPLETE;
}

MemoryCardError CSaveGameImport::GetModifiedTime()
{
	u32 ModifiedTimeHigh = 0;
	u32 ModifiedTimeLow = 0;
	if (SAVEGAME.CheckGetFileModifiedTime(CSavegameUsers::GetUser(), ModifiedTimeHigh, ModifiedTimeLow))
	{	//	returns true for error or success, false for busy

		SAVEGAME.EndGetFileModifiedTime(CSavegameUsers::GetUser());

		if (CSavegameUsers::HasPlayerJustSignedOut())
		{
			savegameErrorf("CSaveGameImport::GetModifiedTime - Player has signed out");
			savegameAssertf(0, "CSaveGameImport::GetModifiedTime - Player has signed out");
//			CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT);
			return MEM_CARD_ERROR;
		}

		if (SAVEGAME.GetState(CSavegameUsers::GetUser()) == fiSaveGameState::HAD_ERROR)
		{
			savegameErrorf("CSaveGameImport::GetModifiedTime - SAVEGAME.GetState() returned HAD_ERROR");
			savegameAssertf(0, "CSaveGameImport::GetModifiedTime - SAVEGAME.GetState() returned HAD_ERROR");
//			CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_GET_FILE_MODIFIED_TIME_FAILED1);
			return MEM_CARD_ERROR;
		}

		if (SAVEGAME.GetError(CSavegameUsers::GetUser()) != fiSaveGame::SAVE_GAME_SUCCESS)
		{
			savegameErrorf("CSaveGameImport::GetModifiedTime - SAVEGAME.GetError() didn't return SAVE_GAME_SUCCESS");
			savegameAssertf(0, "CSaveGameImport::GetModifiedTime - SAVEGAME.GetError() didn't return SAVE_GAME_SUCCESS");
//			CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_GET_FILE_MODIFIED_TIME_FAILED2);
			return MEM_CARD_ERROR;
		}

		CSavegameList::UpdateSlotDataAfterSave(ModifiedTimeHigh, ModifiedTimeLow, false);	//	savingBackupSave);

		return MEM_CARD_COMPLETE;
	}

	return MEM_CARD_BUSY;
}
#endif // RSG_ORBIS


MemoryCardError CSaveGameImport::BeginIconSave(char *pSaveGameIcon, u32 sizeOfSaveGameIcon)
{
	if (pSaveGameIcon)
	{
		if (!SAVEGAME.BeginIconSave(CSavegameUsers::GetUser(), CGenericGameStorage::GetContentType(), 
			CSavegameFilenames::GetFilenameOfLocalFile(), static_cast<const void *>(pSaveGameIcon), sizeOfSaveGameIcon))
		{
			savegameErrorf("CSaveGameImport::BeginIconSave - SAVEGAME.BeginIconSave() failed");
			savegameAssertf(0, "CSaveGameImport::BeginIconSave - SAVEGAME.BeginIconSave() failed");
//			CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_BEGIN_ICON_SAVE_FAILED);
			return MEM_CARD_ERROR;
		}
	}

	return MEM_CARD_COMPLETE;
}


MemoryCardError CSaveGameImport::CheckIconSave()
{
	if (SAVEGAME.CheckIconSave(CSavegameUsers::GetUser()))
	{	//	returns true for error or success, false for busy
		SAVEGAME.EndIconSave(CSavegameUsers::GetUser());

		if (CSavegameUsers::HasPlayerJustSignedOut())
		{
			savegameErrorf("CSaveGameImport::CheckIconSave - Player has signed out");
			savegameAssertf(0, "CSaveGameImport::CheckIconSave - Player has signed out");
//			CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT);
			return MEM_CARD_ERROR;
		}

		if (SAVEGAME.GetState(CSavegameUsers::GetUser()) == fiSaveGameState::HAD_ERROR)
		{
			savegameErrorf("CSaveGameImport::CheckIconSave - GetState() returned HAD_ERROR");
			savegameAssertf(0, "CSaveGameImport::CheckIconSave - GetState() returned HAD_ERROR");
//			CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_ICON_SAVE_FAILED1);
			return MEM_CARD_ERROR;
		}

		if (SAVEGAME.GetError(CSavegameUsers::GetUser()) != fiSaveGame::SAVE_GAME_SUCCESS)
		{
			savegameErrorf("CSaveGameImport::CheckIconSave - GetError() didn't return SAVE_GAME_SUCCESS");
			savegameAssertf(0, "CSaveGameImport::CheckIconSave - GetError() didn't return SAVE_GAME_SUCCESS");
//			CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_ICON_SAVE_FAILED2);
			return MEM_CARD_ERROR;
		}

		return MEM_CARD_COMPLETE;
	}

	return MEM_CARD_BUSY;
}


void CSaveGameImport::SetDisplayNameForSavegame()
{
	const char *pNameOfLastMissionPassed = NULL;

	s32 hashOfLastMissionPassed = sm_SaveGameBufferContents_Code.GetHashOfNameOfLastMissionPassed();
	if (TheText.DoesTextLabelExist(hashOfLastMissionPassed))
	{
		pNameOfLastMissionPassed = TheText.Get(hashOfLastMissionPassed, "SP_LAST_MISSION_NAME");
	}
	else
	{
		// Still need to add this to americanCode.txt
		// It should say something like "Migrated Savegame"
		pNameOfLastMissionPassed = TheText.Get("MIGRATED_SAVE");
	}

	float fPercentageComplete = sm_SaveGameBufferContents_Code.GetSinglePlayerCompletionPercentage();

	CSavegameFilenames::SetNameToDisplay(pNameOfLastMissionPassed, fPercentageComplete);
}

#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

