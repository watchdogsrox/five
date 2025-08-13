
#include "script_save_state.h"


CScriptSaveDataState::CScriptSaveDataState()	
{
	memset(m_pNameOfGlobalDataTree, '\0', sizeof(char) * SCRIPT_NAME_LENGTH);
	memset(m_pNameOfScriptTreeNode, '\0', sizeof(char) * SCRIPT_NAME_LENGTH);
}


CScriptSaveDataState::~CScriptSaveDataState()
{

}


#if __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES
s32 CScriptSaveDataState::GetNumberOfBytesToAllocateForImportExportBuffer()
{
	CNestedScriptData &nestedScriptData = m_NestedGlobalScriptData[SAVEGAME_SINGLE_PLAYER];

	s32 sizeInBytes = nestedScriptData.GetSizeOfSavedScriptDataInBytes();
	s32 sizeInScrValues = nestedScriptData.GetSizeOfSavedScriptData();
	s32 sizeInBytes2 = sizeInScrValues * sizeof(scrValue);
	savegameDisplayf("CScriptSaveDataState::GetNumberOfBytesToAllocateForImportExportBuffer - Size = %d bytes (%d scrValues)", sizeInBytes, sizeInScrValues);
	savegameAssertf(sizeInBytes == sizeInBytes2, "CScriptSaveDataState::GetNumberOfBytesToAllocateForImportExportBuffer - sizeInBytes=%d (sizeInScrValues*sizeof(scrValue))=%d", sizeInBytes, sizeInBytes2);

	s32 sizeToAllocate = sizeInBytes;
	if (sizeInBytes2 > sizeToAllocate)
	{
		sizeToAllocate = sizeInBytes2;
	}

	return sizeToAllocate;
}

void CScriptSaveDataState::SetBufferForImportExport(u8 *pBuffer)
{
	m_NestedGlobalScriptData[SAVEGAME_SINGLE_PLAYER].SetBufferForImportExport(pBuffer);
}

#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES

