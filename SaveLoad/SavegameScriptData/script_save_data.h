

#ifndef SCRIPT_SAVE_DATA_H
#define SCRIPT_SAVE_DATA_H

// Game headers
#include "SaveLoad/SavegameScriptData/script_save_stack.h"
#include "SaveLoad/SavegameScriptData/script_save_state.h"



class CScriptSaveData
{
public:

	static void	Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);

	static void Update();

	static void RegisterSaveGameDataName(const char *pSaveGameDataName);
	static void UnregisterSaveGameDataName(const char *pSaveGameDataName);
	static bool HasSaveGameDataNameAlreadyBeenRegistered(const char *pSaveGameDataName);

	static bool MainHasRegisteredScriptData(eTypeOfSavegame typeOfSavegame);
	static bool DLCHasRegisteredScriptData(eTypeOfSavegame typeOfSavegame, int index);

#if SAVEGAME_USES_XML
	//	DeleteAllRTStructures
	//	Deletes all the RTStructures that have been created to store global save data.
	static void DeleteAllRTStructures(eTypeOfSavegame typeOfSavegame);

	static void AddInactiveScriptDataToTreeToBeSaved(eTypeOfSavegame typeOfSavegame, parTree *pTreeToBeSaved, int index);
	static void ReadScriptTreesFromLoadedTree(eTypeOfSavegame typeOfSavegame, parTree *pFullLoadedTree);
	static bool HasAllScriptDataBeenDeserializedFromXmlFile(eTypeOfSavegame typeOfSavegame);
#endif

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
	static bool DeserializeForImport(parTree *pFullLoadedTree);
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

#if SAVEGAME_USES_XML || __ALLOW_EXPORT_OF_SP_SAVEGAMES
	static void AddActiveScriptDataToTreeToBeSaved(eTypeOfSavegame typeOfSavegame, parTree *pTreeToBeSaved, int index);
#endif // SAVEGAME_USES_XML || __ALLOW_EXPORT_OF_SP_SAVEGAMES

#if SAVEGAME_USES_PSO
	static void AddActiveScriptDataToPsoToBeSaved(eTypeOfSavegame typeOfSavegame, psoBuilder& builder, psoBuilderInstance* rootInstance);
	static bool ReadScriptTreesFromLoadedPso(eTypeOfSavegame typeOfSavegame, psoFile& psofile, u8* workBuffer);
	//	static bool HasAllScriptDataBeenDeserializedFromPsoFile(eTypeOfSavegame typeOfSavegame);
	static void ReadAndDeserializeAllScriptTreesFromLoadedPso(eTypeOfSavegame typeOfSavegame, psoFile& file);

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	static bool DeserializeForExport(psoFile& psofile);
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

#if __BANK
	static void DisplayCurrentValuesOfSavedScriptVariables(eTypeOfSavegame typeOfSavegame);
#endif	//	__BANK

#endif	//	SAVEGAME_USES_PSO

	static bool GetCreationOfScriptSaveStructureInProgress() { return ms_pCurrentScriptData ? ms_pCurrentScriptData->m_bCreationOfScriptSaveStructureInProgress : false; }

	//	OpenScriptSaveData
	//	Called by the START_SAVE_DATA command. Can only be called once for global script data.
	//	Creates the top-level CScriptSaveStruct that will contain all the other data to be saved for a particular script.
	static void OpenScriptSaveData(eTypeOfSavegame typeOfSavegame, const void *pStartAddressOfStruct, s32 NumberOfScrValuesInStruct, GtaThread *pScriptThread);

	//	AddMemberToCurrentStruct
	//	Called by all the REGISTER_..._TO_SAVE script commands.
	//	Adds the variable as a member of the struct at the top of the stack.
	static void AddMemberToCurrentStruct(const char *pLabel, int DataType, const void *pStartAddress, GtaThread *pScriptThread);

	//	OpenScriptStruct
	//	Called by the START_SAVE_STRUCT command
	//	Opens a new script structure and pushes it on to the stack
	static void OpenScriptStruct(const char *pLabel, void *pStartAddress, s32 NumberOfScrValuesInStruct, bool bIsArray, GtaThread *pScriptThread);

	//	CloseScriptStruct
	//	Called by the STOP_SAVE_DATA and STOP_SAVE_STRUCT script commands.
	//	If the last struct is popped off the stack then we'll check if a parTree has been previously loaded that 
	//	matches with the save data that we have just finished registering.
	static void CloseScriptStruct(GtaThread *pScriptThread);

	static bool DeserializeSavedData(eTypeOfSavegame typeOfSavegame);

	static s32 GetSizeOfSavedScriptData(eTypeOfSavegame typeOfSavegame, GtaThread *pScriptThread);

	//	Functions to access the StructStack
	static const CScriptSaveStack &GetStructStack() { return ms_pCurrentScriptData->m_ScriptSaveStack;}
	static void PushOnToStructStack(u8* StartAddress, u8* EndAddress, const char *pNameOfInstance, int ArrayIndex);
	static void PopStructStack();
	static int GetNumOfScriptStructs(){return ms_aScriptSaveDataMembers.GetCount();}
	static int GetNumOfScriptStructs(eTypeOfSavegame saveGameType);
	static const char* GetNameOfScriptStruct(int index){return index!= -1 ? ms_aScriptSaveDataMembers[index]->m_pNameOfScriptTreeNode : ms_oMainScriptSaveData.m_pNameOfScriptTreeNode;}
//	static atArray<CScriptSaveDataState*>& GetDLCScriptSaveData(){return ms_aScriptSaveDataMembers;}
	static CScriptSaveDataState& GetMainScriptSaveData(){return ms_oMainScriptSaveData;}

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES
	// An index of -1 means ms_oMainScriptSaveData
	static s32 GetNumberOfBytesToAllocateForImportExportBuffer(int index);

	// An index of -1 means ms_oMainScriptSaveData
	static void SetBufferForImportExport(int index, u8 *pBuffer);
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES


private:
	static bool DoScriptNameHashesMatch(atLiteralHashValue hash1, atLiteralHashValue hash2);
	static void SetNameOfScriptTreeNode(int index, const char *scriptName);

	static void ForceCleanupOfScriptSaveStructs();

	static bool CheckAddressIsWithinSaveStruct(const void *pAddressToCheck);
	static CScriptSaveDataState ms_oMainScriptSaveData;
	static atArray<CScriptSaveDataState*> ms_aScriptSaveDataMembers;
	static CScriptSaveDataState * ms_pCurrentScriptData;
	static s32 ms_CountdownToForceCleanup;

	static atLiteralHashString ms_HashOfStandardGlobalRegScriptNameWithUpperCaseS;
	static atLiteralHashString ms_HashOfStandardGlobalRegScriptNameWithLowerCaseS;


	static const s32 NUMBER_OF_FRAMES_BEFORE_FORCING_CLEANUP_OF_LOADED_SCRIPT_DATA = 10;
public:
	struct FileWithCount {
		psoFile* m_File;
		int m_RefCount;
		u8* m_WorkBuffer;
	};
	static atArray<FileWithCount> ms_aFiles;
};

#endif // SCRIPT_SAVE_DATA_H