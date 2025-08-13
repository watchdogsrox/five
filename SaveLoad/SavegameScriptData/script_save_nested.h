

#ifndef SCRIPT_SAVE_NESTED_H
#define SCRIPT_SAVE_NESTED_H

// Rage headers
#include "parser/psobuilder.h"
#include "parser/tree.h"

// Game headers
#include "SaveLoad/savegame_defines.h"
#include "SaveLoad/SavegameScriptData/script_save_array_of_structs.h"
#include "SaveLoad/SavegameScriptData/script_save_pso_file_tracker.h"
#include "SaveLoad/SavegameScriptData/script_save_xml_trees.h"

// Forward declarations
class GtaThread;

//	CNestedScriptData - wraps a CScriptSaveArrayOfStructs for use within 
//		CScriptSaveData (for global script data)
class CNestedScriptData
{
public:

	~CNestedScriptData();

	void Shutdown(/*unsigned shutdownMode*/);

	//	OpenScriptSaveData
	//	Called by the START_SAVE_DATA command. Can only be called once for global script data.
	//	Creates the top-level CScriptSaveStruct that will contain all the other data to be saved for a particular script.
	bool OpenScriptSaveData(const char *pLabel, const void *pStartAddressOfStruct, s32 NumberOfScrValuesInStruct, GtaThread *pScriptThread);

	//	If the struct at the top of the stack is open (i.e. hasn't already been registered and had STOP_SAVE_STRUCT called on it)
	//	then add a new member
	void AddMemberToCurrentStruct(const char *pLabel, int DataType, const void *pStartAddress, GtaThread *pScriptThread);

	//	If a struct has already been registered at the current script Program Counter then use its index. 
	//	Otherwise add a new struct to the array of structs.
	//	In either case, add the new instance of the struct as a child of the struct at the top of the stack
	//	and then push this new struct on to the stack.
	void OpenScriptStruct(const char *pLabel, void *pStartAddress, s32 NumberOfScrValuesInStruct, bool bIsArray, GtaThread *pScriptThread);

	//	CloseScriptStruct
	//	If the last element has just been popped then check if there's a previously loaded parTree to read data from 
	//	for this struct.
	//	Returns true if the last element has just been popped off the StructStack
	bool CloseScriptStruct();

	bool DeserializeSavedData(const char* pNameOfGlobalDataTree);

	void ForceCleanupOfScriptSaveStructs(const char* pNameOfGlobalDataTree);

	s32 GetSizeOfSavedScriptData();
	s32 GetSizeOfSavedScriptDataInBytes();

#if SAVEGAME_USES_XML
	//	DeleteAllRTStructures
	//	Deletes all the RTStructures that have been created
	void DeleteAllRTStructures();

	void AddInactiveScriptDataToTreeToBeSaved(parTree *pTreeToBeSaved, const char* pNameOfScriptTreeNode);

	void ReadScriptTreesFromLoadedTree(parTree *pFullLoadedTree, const char* pNameOfScriptTreeNode);

	bool HasAllScriptDataBeenDeserializedFromXmlFile();
#endif	//	SAVEGAME_USES_XML

#if SAVEGAME_USES_XML || __ALLOW_EXPORT_OF_SP_SAVEGAMES
	void AddActiveScriptDataToTreeToBeSaved(parTree *pTreeToBeSaved, int index);
#endif	//	SAVEGAME_USES_XML || __ALLOW_EXPORT_OF_SP_SAVEGAMES

#if SAVEGAME_USES_PSO
	psoBuilderInstance* CreatePsoStructuresForSaving(psoBuilder& builder, atLiteralHashValue nameHashOfScript);

	void AddActiveScriptDataToPsoToBeSaved(psoBuilder& builder, psoBuilderInstance* rootInstance, const char* pNameOfScriptTreeNode);

	//	bool HasAllScriptDataBeenDeserializedFromPsoFile();

	void ReadAndDeserializeAllScriptTreesFromLoadedPso(psoFile& file, const char* pNameOfScriptTreeNode, const char* pNameOfGlobalDataTree);

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	bool DeserializeForExport(psoMember& member);
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
	bool DeserializeForImport(parTreeNode *pParentNode, const char* pNameOfScriptTreeNode, const char* pNameOfGlobalDataTree);
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

#if __BANK
	void DisplayCurrentValuesOfSavedScriptVariables();
#endif	//	__BANK

#endif	//	SAVEGAME_USES_PSO

	atString& GetTopLevelName();

	bool HasRegisteredScriptData();

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES
	void SetBufferForImportExport(u8 *pBaseAddress);
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES

	//DLCTODO: encapsulate!!!!!!!!11111111111111bir
	//private:
	//	Private functions
	static bool IsValidCharacterForElementName(char c);

	bool DoesElementNameAlreadyExistInStructsOnStack(const char *pElementName);

	bool CheckElementName(const char *pElementName);

	//	Private data
	CScriptSaveArrayOfStructs m_ScriptSaveArrayOfStructs;

#if SAVEGAME_USES_PSO
	CScriptSavePsoFileTracker m_ScriptSavePsoFileTracker;
#endif	//	SAVEGAME_USES_PSO

#if SAVEGAME_USES_XML
	CScriptSaveXmlTrees m_ScriptSaveXmlTrees;
#endif	//	SAVEGAME_USES_XML
};


#endif // SCRIPT_SAVE_NESTED_H

