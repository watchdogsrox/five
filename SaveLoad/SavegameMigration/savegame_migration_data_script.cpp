
#include "savegame_migration_data_script.h"

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES

// Game headers
#include "Network/Network.h"
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/SavegameScriptData/script_save_data.h"


void CSaveGameMigrationData_Script::Initialize()
{
	m_pSavedScriptGlobalsForMainScript = NULL;
	m_pSavedScriptGlobalsForDLCScript.Reset();
}


u8 *CSaveGameMigrationData_Script::AllocateMemoryForScriptGlobals(s32 numberOfBytes)
{
	if (CNetwork::IsNetworkOpen())
	{
		savegameErrorf("CSaveGameMigrationData_Script::AllocateMemoryForScriptGlobals - didn't expect the player to export a single player save game during MP. We can't use the Network Heap for memory allocations");
		savegameAssertf(0, "CSaveGameMigrationData_Script::AllocateMemoryForScriptGlobals - didn't expect the player to export a single player save game during MP. We can't use the Network Heap for memory allocations");
		return NULL;
	}

	u8 *pAllocatedMemory = NULL;

	{
		// SP: Use network heap
		sysMemAutoUseNetworkMemory mem;
		pAllocatedMemory = (u8*) rage_aligned_new(16) u8[numberOfBytes];
	}

	if (pAllocatedMemory)
	{
		savegameDisplayf("CSaveGameMigrationData_Script::AllocateMemoryForScriptGlobals - allocated %d bytes", numberOfBytes);
	}
	else
	{
		savegameErrorf("CSaveGameMigrationData_Script::AllocateMemoryForScriptGlobals - failed to allocate %d bytes", numberOfBytes);
		savegameAssertf(0, "CSaveGameMigrationData_Script::AllocateMemoryForScriptGlobals - failed to allocate %d bytes", numberOfBytes);
	}

	return pAllocatedMemory;
}


void CSaveGameMigrationData_Script::FreeMemoryForScriptGlobals(u8 **ppScriptGlobalsMemory)
{
	if (*ppScriptGlobalsMemory)
	{
		if (CNetwork::GetNetworkHeap()->IsValidPointer(*ppScriptGlobalsMemory))
		{		
			sysMemAutoUseNetworkMemory mem;
			delete [] *ppScriptGlobalsMemory;
		}
		else
		{
// 			sysMemAutoUseFragCacheMemory mem;
// 			delete [] *ppScriptGlobalsMemory;
			Quitf(0, "CSaveGameMigrationData_Script::FreeMemoryForScriptGlobals - memory was not allocated from the Network heap");
		}	

		*ppScriptGlobalsMemory = NULL;
	}
}


bool CSaveGameMigrationData_Script::Create()
{
	// Main script - use an index of -1
	s32 numberOfBytesToAllocate = CScriptSaveData::GetNumberOfBytesToAllocateForImportExportBuffer(-1);
	if (numberOfBytesToAllocate == -1)
	{
		savegameErrorf("CSaveGameMigrationData_Script::Create - GetNumberOfBytesToAllocateForImportExportBuffer() failed for main script");
		savegameAssertf(0, "CSaveGameMigrationData_Script::Create - GetNumberOfBytesToAllocateForImportExportBuffer() failed for main script");
		return false;
	}
	savegameDisplayf("CSaveGameMigrationData_Script::Create - allocating %d bytes for Saved Script Globals for main script", numberOfBytesToAllocate);
	m_pSavedScriptGlobalsForMainScript = AllocateMemoryForScriptGlobals(numberOfBytesToAllocate);
	if (!m_pSavedScriptGlobalsForMainScript)
	{
		return false;
	}

	CScriptSaveData::SetBufferForImportExport(-1, m_pSavedScriptGlobalsForMainScript);


	// DLC scripts
	s32 numberOfDlcSaveBlocks = CScriptSaveData::GetNumOfScriptStructs();

	m_pSavedScriptGlobalsForDLCScript.Reset();
	m_pSavedScriptGlobalsForDLCScript.Resize(numberOfDlcSaveBlocks);

	for (s32 loop = 0; loop < numberOfDlcSaveBlocks; loop++)
	{
		numberOfBytesToAllocate = CScriptSaveData::GetNumberOfBytesToAllocateForImportExportBuffer(loop);
		if (numberOfBytesToAllocate == -1)
		{
			savegameErrorf("CSaveGameMigrationData_Script::Create - GetNumberOfBytesToAllocateForImportExportBuffer() failed for DLC script %d", loop);
			savegameAssertf(0, "CSaveGameMigrationData_Script::Create - GetNumberOfBytesToAllocateForImportExportBuffer() failed for DLC script %d", loop);
			return false;
		}
		savegameDisplayf("CSaveGameMigrationData_Script::Create - allocating %d bytes for Saved Script Globals for DLC script %d", numberOfBytesToAllocate, loop);
		m_pSavedScriptGlobalsForDLCScript[loop] = AllocateMemoryForScriptGlobals(numberOfBytesToAllocate);
		if (!m_pSavedScriptGlobalsForDLCScript[loop])
		{
			return false;
		}

		CScriptSaveData::SetBufferForImportExport(loop, m_pSavedScriptGlobalsForDLCScript[loop]);
	}

	return true;
}


void CSaveGameMigrationData_Script::Delete()
{
	FreeMemoryForScriptGlobals(&m_pSavedScriptGlobalsForMainScript);
	CScriptSaveData::SetBufferForImportExport(-1, NULL);

	s32 numberOfDLCSaveBlocks = m_pSavedScriptGlobalsForDLCScript.GetCount();
	for (s32 loop = 0; loop < numberOfDLCSaveBlocks; loop++)
	{
		FreeMemoryForScriptGlobals(&m_pSavedScriptGlobalsForDLCScript[loop]);
		CScriptSaveData::SetBufferForImportExport(loop, NULL);
	}
	m_pSavedScriptGlobalsForDLCScript.Reset();
}


#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
bool CSaveGameMigrationData_Script::ReadDataFromPsoFile(psoFile* psofile)
{
	if (!Create())
	{
		Delete();
		return false;
	}

	return CScriptSaveData::DeserializeForExport(*psofile);
}
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
bool CSaveGameMigrationData_Script::ReadDataFromParTree(parTree *pParTree)
{
	if (!Create())
	{
		Delete();
		return false;
	}

	return CScriptSaveData::DeserializeForImport(pParTree);
}
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES


#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES

