

#ifndef SCRIPT_SAVE_STATE_H
#define SCRIPT_SAVE_STATE_H


// Game headers
#include "SaveLoad/SavegameScriptData/script_save_nested.h"
#include "SaveLoad/SavegameScriptData/script_save_stack.h"
#include "scene/FileLoader.h"	//	For SCRIPT_NAME_LENGTH

class CScriptSaveDataState
{
public:
	char m_pNameOfGlobalDataTree[SCRIPT_NAME_LENGTH];
	char m_pNameOfScriptTreeNode[SCRIPT_NAME_LENGTH];
	atLiteralHashValue m_NameHashOfScriptName;
	bool m_bCreationOfScriptSaveStructureInProgress;
	eTypeOfSavegame m_eTypeOfSavegame;
	GtaThread *m_pScriptThreadThatIsCreatingScriptSaveStructure;
	const void *m_pStartAddressOfScriptSaveStructure;
	const void *m_pEndAddressOfScriptSaveStructure;
	CNestedScriptData m_NestedGlobalScriptData[SAVEGAME_MAX_TYPES];
	CScriptSaveStack m_ScriptSaveStack;
	void SetHash(){m_NameHashOfScriptName = atLiteralHashValue(m_pNameOfScriptTreeNode);}
	void SetHash(atLiteralHashValue literalHash){m_NameHashOfScriptName = literalHash;}
	bool HasMultiplayerData(){return m_NestedGlobalScriptData[SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA].HasRegisteredScriptData();}
	bool HasSingleplayerData(){return m_NestedGlobalScriptData[SAVEGAME_SINGLE_PLAYER].HasRegisteredScriptData();}
	CScriptSaveDataState();
	~CScriptSaveDataState();

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES
	s32 GetNumberOfBytesToAllocateForImportExportBuffer();

	void SetBufferForImportExport(u8 *pBuffer);
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES

};


#endif // SCRIPT_SAVE_STATE_H

