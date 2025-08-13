

#ifndef SCRIPT_SAVE_ARRAY_OF_STRUCTS_H
#define SCRIPT_SAVE_ARRAY_OF_STRUCTS_H

// Rage headers
#include "parser/psobuilder.h"
#include "parser/psodata.h"
#include "parser/rtstructure.h"
#include "parser/tree.h"

// Game headers
#include "SaveLoad/savegame_defines.h"
#include "SaveLoad/SavegameScriptData/script_save_struct.h"


//	CScriptSaveArrayOfStructs - contains the layout of all save data for the global script data
class CScriptSaveArrayOfStructs
{
public:
	CScriptSaveArrayOfStructs();

	// Public functions
	void Shutdown(/*unsigned shutdownMode*/);

	//	Assume that two structs registered at the same Program Counter are the same struct 
	//	and don't register more than once - just return the index of the first struct.
	int AddNewScriptStruct(int PCOfRegisterCommand, bool bIsArray, const char *pNameOfInstance);

	void AddNewDataItemToStruct(int ArrayIndexOfStruct, u32 Offset, int DataType, const char *pLabel);

	bool GetHasBeenClosed(int ArrayIndex);
	void SetHasBeenClosed(int ArrayIndex, bool bHasBeenClosed);

	bool DoesElementNameAlreadyExist(int ArrayIndex, const char *pElementNameToCheck);
	void ClearRegisteredElementNames(int ArrayIndex);

	void SetDataForTopLevelStruct(u8* BaseAddress, const char *pName);
#if __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES
	void SetBufferForImportExport(u8 *pBaseAddress);
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES

	bool HasRegisteredScriptData();

	s32 GetSizeOfSavedScriptData();
	s32 GetSizeOfSavedScriptDataInBytes();

	atString& GetTopLevelName() { return m_NameOfTopLevelStructure; }

#if SAVEGAME_USES_XML || __ALLOW_IMPORT_OF_SP_SAVEGAMES
	//	Only create RTStructures from the struct data when 
	//	loading the contents of a parTree within STOP_SAVE_DATA (CloseScriptStruct)
	void CreateRTStructuresForLoading();

	//	Should be called after loading from a parTree within STOP_SAVE_DATA (CloseScriptStruct)
	void DeleteRTStructures();

	//	The top level RTStructure will be the one that contains all the other variables and structs for a particular GtaThread
	parRTStructure *GetTopLevelRTStructure();
#endif // SAVEGAME_USES_XML || __ALLOW_IMPORT_OF_SP_SAVEGAMES

#if SAVEGAME_USES_XML || __ALLOW_EXPORT_OF_SP_SAVEGAMES
	void AddActiveScriptDataToTreeToBeSaved(parTree *pTreeToBeSaved, int index);
#endif // SAVEGAME_USES_XML || __ALLOW_EXPORT_OF_SP_SAVEGAMES

#if SAVEGAME_USES_PSO
	psoBuilderInstance* CreatePsoStructuresForSaving(psoBuilder& Builder, atLiteralHashValue nameHashOfScript);

	//	void AddActiveScriptDataToPsoToBeSaved(psoBuilder& Builder);

	void LoadFromPsoStruct(psoStruct& Structure);

#if __BANK
	void DisplayCurrentValuesOfSavedScriptVariables();
#endif	//	__BANK

#endif	//	SAVEGAME_USES_PSO

private:
	//	Private functions

#if SAVEGAME_USES_XML || __ALLOW_EXPORT_OF_SP_SAVEGAMES
	//	This function will recursively call itself to handle nested structures
	//	AddActiveScriptDataToTreeToBeSaved() calls this function for the top level structure.
	//	Only used for saving
	parTreeNode *CreateTreeNodesFromScriptData(int ArrayIndex, atString &NameOfStructureInstance, u8* BaseAddressOfStructure);
#endif // SAVEGAME_USES_XML || __ALLOW_EXPORT_OF_SP_SAVEGAMES

#if SAVEGAME_USES_XML || __ALLOW_IMPORT_OF_SP_SAVEGAMES
	//	CreateRTStructureForLoading
	//	This function will recursively call itself to handle nested structures.
	//	CreateRTStructuresForLoading() calls this function for the top level structure.
	//	Only used for loading
	parRTStructure *CreateRTStructureForLoading(int ArrayIndex, atString &NameOfStructureInstance, u8* BaseAddressOfStructure);
#endif // SAVEGAME_USES_XML || __ALLOW_IMPORT_OF_SP_SAVEGAMES

	//	This function will recursively call itself to handle nested structures.
	//	GetSizeOfSavedScriptData() calls this function for the top level structure.
	s32 GetSizeOfStructAndItsChildren(int ArrayIndex, atString &NameOfStructureInstance);
	int ComputeSizeInBytesWithGaps(u32 ArrayIndex);

#if SAVEGAME_USES_PSO
	//	psoBuilderInstance* CreatePsoStructure(psoBuilder& builder, int ArrayIndex, atString &NameOfStructureInstance, u8* BaseAddressOfStructure);

	psoBuilderStructSchema* CreatePsoStructureSchemaForSaving(psoBuilder& builder, u32 ArrayIndex, u32 anonOffset);
	//	void AddDataToPsoInstance(psoBuilder& builder, psoBuilderInstance& inst, u32 ArrayIndex, u8* BaseAddressOfStructure);

	void LoadFromPsoStruct(int ArrayIndex, u8* BaseAddressOfStructure, psoStruct& Structure);

#if __BANK
	void DisplayCurrentValuesOfSavedScriptVariables(int ArrayIndex, u8* BaseAddressOfStructure);
#endif	//	__BANK

#endif	//	SAVEGAME_USES_PSO

	u8* GetBaseAddressOfTopLevelStructure();

	static int GetNumberOfCharactersInATextLabelOfThisType(int TypeOfTextLabel);

	//	Private data
	atArray<CScriptSaveStruct> m_ScriptStructs;

#if SAVEGAME_USES_XML || __ALLOW_IMPORT_OF_SP_SAVEGAMES
	atArray<parRTStructure*> m_ArrayOfRTStructures;	//	This will only contain RTStructures during a save or when loading data from a parTree
#endif // SAVEGAME_USES_XML || __ALLOW_IMPORT_OF_SP_SAVEGAMES

	atString m_NameOfTopLevelStructure;
	u8* m_BaseAddressOfTopLevelStructure;

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES
	u8* m_BaseAddressOfTopLevelStructureForImportExport;
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES

};

#endif // SCRIPT_SAVE_ARRAY_OF_STRUCTS_H
