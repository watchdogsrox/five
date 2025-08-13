
#include "script_save_data.h"


// Rage headers
#include "parser/psofile.h"

// Game headers
//	#include "SaveLoad/SavegameScriptData/script_save_struct.h"	//	For Script_Save_Data_Type_Int etc.
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/savegame_data.h"
#include "script/script.h"


PARAM(output_registered_script_save_data, "Output to the TTY the script save data as it is registered");


// **************************************** SaveGameDataBlock - Script - CScriptSaveData ************************************

static SaveDataMap sMapToCheckForDuplicateSaveGameScriptNames;

//	bool CScriptSaveData::ms_bScriptDataHasBeenLoaded = false;

atArray<CScriptSaveDataState*> CScriptSaveData::ms_aScriptSaveDataMembers;
CScriptSaveDataState CScriptSaveData::ms_oMainScriptSaveData;
CScriptSaveDataState* CScriptSaveData::ms_pCurrentScriptData = NULL;
s32 CScriptSaveData::ms_CountdownToForceCleanup = 0;

atLiteralHashString CScriptSaveData::ms_HashOfStandardGlobalRegScriptNameWithUpperCaseS;
atLiteralHashString CScriptSaveData::ms_HashOfStandardGlobalRegScriptNameWithLowerCaseS;


atArray<CScriptSaveData::FileWithCount> CScriptSaveData::ms_aFiles; 


void CScriptSaveData::Init(unsigned initMode)
{
	if (initMode == INIT_CORE)
	{
		ms_aScriptSaveDataMembers.Reset();

		ms_HashOfStandardGlobalRegScriptNameWithUpperCaseS.SetFromString("Standard_global_reg");	//	2962185187
		ms_HashOfStandardGlobalRegScriptNameWithLowerCaseS.SetFromString("standard_global_reg");	//	2003913879
	}
	else
	if(initMode == INIT_BEFORE_MAP_LOADED)
	{
				
	}
	else if (initMode == INIT_AFTER_MAP_LOADED)
	{	//	Maybe I should be calling CScriptSaveData::Init directly in game.cpp rather than going through
		//	CGenericGameStorage::Init and CSaveGameData::Init
		ms_oMainScriptSaveData.m_bCreationOfScriptSaveStructureInProgress = false;
		ms_oMainScriptSaveData.m_eTypeOfSavegame = SAVEGAME_MAX_TYPES;
		ms_oMainScriptSaveData.m_pScriptThreadThatIsCreatingScriptSaveStructure= NULL;
		ms_oMainScriptSaveData.m_pStartAddressOfScriptSaveStructure = NULL;
		ms_oMainScriptSaveData.m_pEndAddressOfScriptSaveStructure = NULL;
		strcpy(ms_oMainScriptSaveData.m_pNameOfGlobalDataTree, "Globals");
		strcpy(ms_oMainScriptSaveData.m_pNameOfScriptTreeNode, "ScriptSaveData");
		ms_oMainScriptSaveData.SetHash();

		ms_CountdownToForceCleanup = 0;
	}
}

void CScriptSaveData::Shutdown(unsigned shutdownMode)
{
	//	Should I only be doing this for a specific shutdownMode?
	ms_oMainScriptSaveData.m_ScriptSaveStack.Shutdown();
	for(int i=0;i<SAVEGAME_MAX_TYPES;i++)
	{
		ms_oMainScriptSaveData.m_NestedGlobalScriptData[i].Shutdown();		
	}
	for(u32 i = 0; i<ms_aScriptSaveDataMembers.GetCount();++i)
	{
		for (u32 loop = 0; loop < SAVEGAME_MAX_TYPES; loop++)
		{
			ms_aScriptSaveDataMembers[i]->m_NestedGlobalScriptData[loop].Shutdown();
		}
		ms_aScriptSaveDataMembers[i]->m_ScriptSaveStack.Shutdown();
	}
	ms_aScriptSaveDataMembers.Reset();
	sMapToCheckForDuplicateSaveGameScriptNames.Kill();

	if (shutdownMode == SHUTDOWN_CORE)
	{
	}
	else if (shutdownMode == SHUTDOWN_SESSION)
	{
	}
}

void CScriptSaveData::Update()
{
	if (ms_CountdownToForceCleanup > 0)
	{
		ms_CountdownToForceCleanup--;
		if (ms_CountdownToForceCleanup == 0)
		{
			ForceCleanupOfScriptSaveStructs();
		}
	}
}


void CScriptSaveData::RegisterSaveGameDataName(const char *pSaveGameDataName)
{
	atString SaveGameDataName(pSaveGameDataName);
	sMapToCheckForDuplicateSaveGameScriptNames.Insert(SaveGameDataName, 0);
}

void CScriptSaveData::UnregisterSaveGameDataName(const char *pSaveGameDataName)
{
	atString SaveGameDataName(pSaveGameDataName);
	sMapToCheckForDuplicateSaveGameScriptNames.Delete(SaveGameDataName);
}

bool CScriptSaveData::HasSaveGameDataNameAlreadyBeenRegistered(const char *pSaveGameDataName)
{
	for(int i = 0; i<ms_aScriptSaveDataMembers.GetCount(); ++i)
	{
		if (strcmp(pSaveGameDataName, ms_aScriptSaveDataMembers[i]->m_pNameOfGlobalDataTree) == 0)
		{	//	Don't allow "Globals" to be used as a name by the level designers
			return true;
		}
	}
	atString SaveGameDataName(pSaveGameDataName);
	int *pExistingData = sMapToCheckForDuplicateSaveGameScriptNames.Access(SaveGameDataName);
	if (pExistingData)
	{
		return true;
	}

	return false;
}

bool CScriptSaveData::MainHasRegisteredScriptData(eTypeOfSavegame typeOfSavegame)
{	
	if (savegameVerifyf( (typeOfSavegame == SAVEGAME_SINGLE_PLAYER) || (typeOfSavegame == SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA), "CScriptSaveData::HasRegisteredScriptData - expected typeOfSavegame to be SAVEGAME_SINGLE_PLAYER or SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA"))
	{
		return ms_oMainScriptSaveData.m_NestedGlobalScriptData[typeOfSavegame].HasRegisteredScriptData();
	}
	return false;
}

bool CScriptSaveData::DLCHasRegisteredScriptData(eTypeOfSavegame typeOfSavegame, int index)
{
	if(savegameVerifyf(index < ms_aScriptSaveDataMembers.GetCount(),"Index out of bounds"))
	{
		if (savegameVerifyf( (typeOfSavegame == SAVEGAME_SINGLE_PLAYER) || (typeOfSavegame == SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA), "CScriptSaveData::HasRegisteredScriptData - expected typeOfSavegame to be SAVEGAME_SINGLE_PLAYER or SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA"))
		{
			return ms_aScriptSaveDataMembers[index]->m_NestedGlobalScriptData[typeOfSavegame].HasRegisteredScriptData();
		}
	}

	return false;
}

#if SAVEGAME_USES_PSO

bool CScriptSaveData::ReadScriptTreesFromLoadedPso(eTypeOfSavegame typeOfSavegame, psoFile& file, u8* workBuffer)
{
	psoStruct root = file.GetRootInstance();
	//Main scripts load
	psoMember mainThread = root.GetMemberByName(atLiteralHashValue(CSaveGameData::GetNameOfMainScriptTreeNode()));
	bool mainLoaded= false;
	if(savegameVerifyf(mainThread.IsValid(),"No Main script save data in save file"))
	{		
		psoStruct subStruct = mainThread.GetSubStructure();
		int SubStructCount = subStruct.GetSchema().GetNumMembers();
		for(int i = 0; i < SubStructCount; i++)
		{
			psoMember scriptStructMember = subStruct.GetMemberByIndex(i);
			psoStruct scriptStruct = subStruct.GetMemberByIndex(i).GetSubStructure();
			savegameDisplayf("CScriptSaveData::ReadScriptTreesFromLoadedPso - about to call Append for main script. The new schema has hash %u", scriptStructMember.GetSchema().GetNameHash().GetHash());
			ms_oMainScriptSaveData.m_NestedGlobalScriptData[typeOfSavegame].m_ScriptSavePsoFileTracker.Append(scriptStructMember.GetSchema().GetNameHash(), scriptStruct, workBuffer);
		}
		mainLoaded= SubStructCount!=0;
	}
	//DLC scripts load
	psoMember scriptThreads = root.GetMemberByName(atLiteralHashValue(CSaveGameData::GetNameOfDLCScriptTreeNode()));
	if (scriptThreads.IsValid())
	{
		int savedScriptThreadCount = scriptThreads.GetSubStructure().GetSchema().GetNumMembers();
		for(int i = 0; i<savedScriptThreadCount; ++i)
		{
			psoMember mem = scriptThreads.GetSubStructure().GetMemberByIndex(i);

//	Graeme - we're now going to be registering the save data for DLC inside Title Update scripts instead of the startup script of the DLC package itself.
//		That means that we can't do the following check
//			if(CDLCScript::ContainsScript(mem.GetSchema().GetNameHash()))
			{
				CScriptSaveDataState* saveMember = NULL;
				for(int j= 0; j<ms_aScriptSaveDataMembers.GetCount(); ++j)
				{
// Graeme - I don't know if this if statement ever evaluates to true.
// When I debugged it in May 2022, m_pNameOfScriptTreeNode was always an empty string so the hash was always 0
// That was the case before and after I made my changes that added the DoScriptNameHashesMatch() function.
					if (DoScriptNameHashesMatch(atLiteralHashValue(ms_aScriptSaveDataMembers[j]->m_pNameOfScriptTreeNode), mem.GetSchema().GetNameHash()))
					{
						savegameDisplayf("CScriptSaveData::ReadScriptTreesFromLoadedPso - found match for name %s. index in ms_aScriptSaveDataMembers array is %d", ms_aScriptSaveDataMembers[j]->m_pNameOfScriptTreeNode, j);
						saveMember = ms_aScriptSaveDataMembers[j];
						break;
					}
				}
				if(!saveMember)
				{
				
					CScriptSaveDataState* newItem = rage_new CScriptSaveDataState;
					newItem->m_bCreationOfScriptSaveStructureInProgress = false;
					newItem->m_eTypeOfSavegame = typeOfSavegame;
					newItem->m_pScriptThreadThatIsCreatingScriptSaveStructure = NULL;
					newItem->m_pStartAddressOfScriptSaveStructure = NULL;
					newItem->m_pEndAddressOfScriptSaveStructure = NULL;
					strcpy(newItem->m_pNameOfGlobalDataTree, "Globals");
					strcpy(newItem->m_pNameOfScriptTreeNode, "");
					newItem->SetHash(mem.GetSchema().GetNameHash());
					ms_aScriptSaveDataMembers.PushAndGrow(newItem);				
					saveMember = newItem;

					savegameDisplayf("CScriptSaveData::ReadScriptTreesFromLoadedPso - creating new struct for DLC script data with hash %u", mem.GetSchema().GetNameHash().GetHash());
				
					//else DISPLAY CONTENT NOT INSTALLED MESSAGE?
				}
				psoStruct subStruct = mem.GetSubStructure();
				int sSCount = subStruct.GetSchema().GetNumMembers();
				for(int x = 0; x<sSCount;++x)
				{
					psoMember scriptStructMember = subStruct.GetMemberByIndex(x);
					psoStruct scriptStruct = subStruct.GetMemberByIndex(x).GetSubStructure();
					savegameDisplayf("CScriptSaveData::ReadScriptTreesFromLoadedPso - about to call Append for DLC script data with hash %u. The new schema has hash %u", mem.GetSchema().GetNameHash().GetHash(), scriptStructMember.GetSchema().GetNameHash().GetHash());
					saveMember->m_NestedGlobalScriptData[typeOfSavegame].m_ScriptSavePsoFileTracker.Append(scriptStructMember.GetSchema().GetNameHash(), scriptStruct, workBuffer);
				}
			}
		}
		return true;
	}

	return mainLoaded;
}

/*
bool CScriptSaveData::HasAllScriptDataBeenDeserializedFromPsoFile(eTypeOfSavegame typeOfSavegame)
{
//	if (savegameVerifyf( (typeOfSavegame == SAVEGAME_SINGLE_PLAYER) || (typeOfSavegame == SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA), "CScriptSaveData::HasAllScriptDataBeenDeserializedFromPsoFile - expected typeOfSavegame to be SAVEGAME_SINGLE_PLAYER or SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA"))
	if (savegameVerifyf( (typeOfSavegame == SAVEGAME_SINGLE_PLAYER), "CScriptSaveData::HasAllScriptDataBeenDeserializedFromPsoFile - expected typeOfSavegame to be SAVEGAME_SINGLE_PLAYER"))
	{
		return ms_NestedGlobalScriptData[typeOfSavegame].HasAllScriptDataBeenDeserializedFromPsoFile();
	}

	return true;
}
*/

void CScriptSaveData::ReadAndDeserializeAllScriptTreesFromLoadedPso(eTypeOfSavegame typeOfSavegame, psoFile& file)
{
#if RSG_PC && ENABLE_SCRIPT_TAMPER_CHECKER
	CScriptGlobalTamperChecker::UncheckedScope uncheckedScope;
#endif
	if (savegameVerifyf( (typeOfSavegame == SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA), "CScriptSaveData::ReadAndDeserializeAllScriptTreesFromLoadedPso - expected typeOfSavegame to be SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA"))
	{
		if (savegameVerifyf(MainHasRegisteredScriptData(typeOfSavegame), "CScriptSaveData::ReadAndDeserializeAllScriptTreesFromLoadedPso - script hasn't registered any MP save data"))
		{
			ms_oMainScriptSaveData.m_NestedGlobalScriptData[typeOfSavegame].ReadAndDeserializeAllScriptTreesFromLoadedPso(file,ms_oMainScriptSaveData.m_pNameOfScriptTreeNode,ms_oMainScriptSaveData.m_pNameOfGlobalDataTree);
		}
	}
	for(int index = 0; index < ms_aScriptSaveDataMembers.GetCount(); index++)
	{
		if (savegameVerifyf( (typeOfSavegame == SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA), "CScriptSaveData::ReadAndDeserializeAllScriptTreesFromLoadedPso - expected typeOfSavegame to be SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA"))
		{
			if (DLCHasRegisteredScriptData(typeOfSavegame,index))
			{
				ms_aScriptSaveDataMembers[index]->m_NestedGlobalScriptData[typeOfSavegame].ReadAndDeserializeAllScriptTreesFromLoadedPso(file,ms_aScriptSaveDataMembers[index]->m_pNameOfScriptTreeNode,ms_aScriptSaveDataMembers[index]->m_pNameOfGlobalDataTree);
			}
		}
	}
}


#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
// The following function does the equivalent of 
// CScriptSaveData::ReadScriptTreesFromLoadedPso 
// followed by 
// CNestedScriptData::DeserializeSavedData
bool CScriptSaveData::DeserializeForExport(psoFile& psofile)
{
	psoStruct root = psofile.GetRootInstance();
	//Main scripts load
	psoMember mainThreadMember = root.GetMemberByName(atLiteralHashValue(CSaveGameData::GetNameOfMainScriptTreeNode()));
	bool mainLoaded = false;
	if(savegameVerifyf(mainThreadMember.IsValid(),"No Main script save data in save file"))
	{
		mainLoaded = ms_oMainScriptSaveData.m_NestedGlobalScriptData[SAVEGAME_SINGLE_PLAYER].DeserializeForExport(mainThreadMember);
	}

	//DLC scripts
	psoMember scriptThreads = root.GetMemberByName(atLiteralHashValue(CSaveGameData::GetNameOfDLCScriptTreeNode()));
	if (scriptThreads.IsValid())
	{
		int savedScriptThreadCount = scriptThreads.GetSubStructure().GetSchema().GetNumMembers();
		for(int i = 0; i<savedScriptThreadCount; ++i)
		{
			psoMember dlcThreadMember = scriptThreads.GetSubStructure().GetMemberByIndex(i);

			CScriptSaveDataState* dlcSaveDataState = NULL;
			for(int j= 0; j<ms_aScriptSaveDataMembers.GetCount(); ++j)
			{
				if (DoScriptNameHashesMatch(atLiteralHashValue(ms_aScriptSaveDataMembers[j]->m_pNameOfScriptTreeNode), dlcThreadMember.GetSchema().GetNameHash()) )
				{
					savegameDisplayf("CScriptSaveData::DeserializeForExport - found match for name %s. index in ms_aScriptSaveDataMembers array is %d", ms_aScriptSaveDataMembers[j]->m_pNameOfScriptTreeNode, j);
					dlcSaveDataState = ms_aScriptSaveDataMembers[j];
					break;
				}
			}
			if(dlcSaveDataState)
			{
				if (!dlcSaveDataState->m_NestedGlobalScriptData[SAVEGAME_SINGLE_PLAYER].DeserializeForExport(dlcThreadMember))
				{
					savegameWarningf("CScriptSaveData::DeserializeForExport - didn't read any saved script data for %s", dlcSaveDataState->m_pNameOfScriptTreeNode);
				}
			}
			else
			{
				savegameErrorf("CScriptSaveData::DeserializeForExport - failed to find registered DLC script save data for the loaded PSO data with hash %u", dlcThreadMember.GetSchema().GetNameHash().GetHash());
				savegameAssertf(0, "CScriptSaveData::DeserializeForExport - failed to find registered DLC script save data for the loaded PSO data with hash %u", dlcThreadMember.GetSchema().GetNameHash().GetHash());
			}
		}
	}

	return mainLoaded;
}
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES


#if __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES
// An index of -1 means ms_oMainScriptSaveData
s32 CScriptSaveData::GetNumberOfBytesToAllocateForImportExportBuffer(int index)
{
	if (index == -1)
	{
		return ms_oMainScriptSaveData.GetNumberOfBytesToAllocateForImportExportBuffer();
	}
	else if ((index >= 0) && (index < ms_aScriptSaveDataMembers.GetCount()))
	{
		return ms_aScriptSaveDataMembers[index]->GetNumberOfBytesToAllocateForImportExportBuffer();
	}

	savegameErrorf("CScriptSaveData::GetNumberOfBytesToAllocateForImportExportBuffer - %d is an invalid index", index);
	savegameAssertf(0, "CScriptSaveData::GetNumberOfBytesToAllocateForImportExportBuffer - %d is an invalid index", index);
	return -1;
}

// An index of -1 means ms_oMainScriptSaveData
void CScriptSaveData::SetBufferForImportExport(int index, u8 *pBuffer)
{
	if (index == -1)
	{
		return ms_oMainScriptSaveData.SetBufferForImportExport(pBuffer);
	}
	else if ((index >= 0) && (index < ms_aScriptSaveDataMembers.GetCount()))
	{
		return ms_aScriptSaveDataMembers[index]->SetBufferForImportExport(pBuffer);
	}
	else
	{
		savegameErrorf("CScriptSaveData::SetBufferForImportExport - %d is an invalid index", index);
		savegameAssertf(0, "CScriptSaveData::SetBufferForImportExport - %d is an invalid index", index);
	}
}

#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES


#if __BANK
void CScriptSaveData::DisplayCurrentValuesOfSavedScriptVariables(eTypeOfSavegame typeOfSavegame)
{
	if (savegameVerifyf( (typeOfSavegame == SAVEGAME_SINGLE_PLAYER) || (typeOfSavegame == SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA), "CScriptSaveData::DisplayCurrentValuesOfSavedScriptVariables - expected typeOfSavegame to be SAVEGAME_SINGLE_PLAYER or SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA"))
	{
		if (savegameVerifyf(MainHasRegisteredScriptData(typeOfSavegame), "CScriptSaveData::DisplayCurrentValuesOfSavedScriptVariables - script hasn't registered any save data for %d", (s32) typeOfSavegame))
		{
			ms_oMainScriptSaveData.m_NestedGlobalScriptData[typeOfSavegame].DisplayCurrentValuesOfSavedScriptVariables();
		}
	}
	for(int index = 0; index < ms_aScriptSaveDataMembers.GetCount(); index++)
	{
		if (savegameVerifyf( (typeOfSavegame == SAVEGAME_SINGLE_PLAYER) || (typeOfSavegame == SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA), "CScriptSaveData::DisplayCurrentValuesOfSavedScriptVariables - expected typeOfSavegame to be SAVEGAME_SINGLE_PLAYER or SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA"))
		{
			if (DLCHasRegisteredScriptData(typeOfSavegame,index))
			{
				ms_aScriptSaveDataMembers[index]->m_NestedGlobalScriptData[typeOfSavegame].DisplayCurrentValuesOfSavedScriptVariables();
			}
		}
	}
}
#endif	//	__BANK

#endif // SAVEGAME_USES_PSO


#if SAVEGAME_USES_XML
void CScriptSaveData::DeleteAllRTStructures(eTypeOfSavegame typeOfSavegame)
{
	ms_oMainScriptSaveData.m_NestedGlobalScriptData[typeOfSavegame].DeleteAllRTStructures();
	for(int index = 0 ;index<ms_aScriptSaveDataMembers.GetCount(); ++index)
	{
		if (savegameVerifyf( (typeOfSavegame == SAVEGAME_SINGLE_PLAYER) || (typeOfSavegame == SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA), "CScriptSaveData::DeleteAllRTStructures - expected typeOfSavegame to be SAVEGAME_SINGLE_PLAYER or SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA"))
		{
			ms_aScriptSaveDataMembers[index]->m_NestedGlobalScriptData[typeOfSavegame].DeleteAllRTStructures();
		}
	}
}
#endif // SAVEGAME_USES_XML

#if SAVEGAME_USES_XML || __ALLOW_EXPORT_OF_SP_SAVEGAMES
void CScriptSaveData::AddActiveScriptDataToTreeToBeSaved(eTypeOfSavegame typeOfSavegame, parTree *pTreeToBeSaved, int index)
{
	if(index==-1)
	{
		if (savegameVerifyf( (typeOfSavegame == SAVEGAME_SINGLE_PLAYER) || (typeOfSavegame == SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA), "CScriptSaveData::AddActiveScriptDataToTreeToBeSaved - expected typeOfSavegame to be SAVEGAME_SINGLE_PLAYER or SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA"))
		{
			ms_oMainScriptSaveData.m_NestedGlobalScriptData[typeOfSavegame].AddActiveScriptDataToTreeToBeSaved(pTreeToBeSaved, index);
		}
	}
	else
	if(savegameVerifyf(index < ms_aScriptSaveDataMembers.GetCount(),"Index out of bounds"))
	{
		if (savegameVerifyf( (typeOfSavegame == SAVEGAME_SINGLE_PLAYER) || (typeOfSavegame == SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA), "CScriptSaveData::AddActiveScriptDataToTreeToBeSaved - expected typeOfSavegame to be SAVEGAME_SINGLE_PLAYER or SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA"))
		{
			ms_aScriptSaveDataMembers[index]->m_NestedGlobalScriptData[typeOfSavegame].AddActiveScriptDataToTreeToBeSaved(pTreeToBeSaved, index);
		}
	}
}
#endif // SAVEGAME_USES_XML || __ALLOW_EXPORT_OF_SP_SAVEGAMES

#if SAVEGAME_USES_XML
void CScriptSaveData::AddInactiveScriptDataToTreeToBeSaved(eTypeOfSavegame typeOfSavegame, parTree *pTreeToBeSaved, int index)
{
	if(index==-1)
	{
		if (savegameVerifyf( (typeOfSavegame == SAVEGAME_SINGLE_PLAYER) || (typeOfSavegame == SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA), "CScriptSaveData::AddActiveScriptDataToTreeToBeSaved - expected typeOfSavegame to be SAVEGAME_SINGLE_PLAYER or SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA"))
		{
			ms_oMainScriptSaveData.m_NestedGlobalScriptData[typeOfSavegame].AddInactiveScriptDataToTreeToBeSaved(pTreeToBeSaved, ms_oMainScriptSaveData.m_pNameOfScriptTreeNode);
		}
	}
	else
	if(savegameVerifyf(index < ms_aScriptSaveDataMembers.GetCount(),"Index out of bounds"))
	{
		if (savegameVerifyf( (typeOfSavegame == SAVEGAME_SINGLE_PLAYER) || (typeOfSavegame == SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA), "CScriptSaveData::AddInactiveScriptDataToTreeToBeSaved - expected typeOfSavegame to be SAVEGAME_SINGLE_PLAYER or SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA"))
		{
			ms_aScriptSaveDataMembers[index]->m_NestedGlobalScriptData[typeOfSavegame].AddInactiveScriptDataToTreeToBeSaved(pTreeToBeSaved,ms_aScriptSaveDataMembers[index]->m_pNameOfScriptTreeNode);
		}
	}
}

void CScriptSaveData::ReadScriptTreesFromLoadedTree(eTypeOfSavegame typeOfSavegame, parTree *pFullLoadedTree)
{
	if (savegameVerifyf( (typeOfSavegame == SAVEGAME_SINGLE_PLAYER) || (typeOfSavegame == SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA), "CScriptSaveData::ReadScriptTreesFromLoadedTree - expected typeOfSavegame to be SAVEGAME_SINGLE_PLAYER or SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA"))
	{
		ms_oMainScriptSaveData.m_NestedGlobalScriptData[typeOfSavegame].ReadScriptTreesFromLoadedTree(pFullLoadedTree, ms_oMainScriptSaveData.m_pNameOfScriptTreeNode);
	}
	for(int index = 0; index<ms_aScriptSaveDataMembers.GetCount();++index)
	{
		if (savegameVerifyf( (typeOfSavegame == SAVEGAME_SINGLE_PLAYER) || (typeOfSavegame == SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA), "CScriptSaveData::ReadScriptTreesFromLoadedTree - expected typeOfSavegame to be SAVEGAME_SINGLE_PLAYER or SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA"))
		{
			ms_aScriptSaveDataMembers[index]->m_NestedGlobalScriptData[typeOfSavegame].ReadScriptTreesFromLoadedTree(pFullLoadedTree, ms_aScriptSaveDataMembers[index]->m_pNameOfScriptTreeNode);
		}
	}	
}

bool CScriptSaveData::HasAllScriptDataBeenDeserializedFromXmlFile(eTypeOfSavegame typeOfSavegame)
{
	int structsHaveData = ms_aScriptSaveDataMembers.GetCount()+1;
	if (savegameVerifyf( (typeOfSavegame == SAVEGAME_SINGLE_PLAYER) || (typeOfSavegame == SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA), "CScriptSaveData::ReadScriptTreesFromLoadedTree - expected typeOfSavegame to be SAVEGAME_SINGLE_PLAYER or SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA"))
	{
		structsHaveData -= ms_oMainScriptSaveData.m_NestedGlobalScriptData[typeOfSavegame].HasAllScriptDataBeenDeserializedFromXmlFile() ? 1:0;
	}
	
	for(int index = 0; index < ms_aScriptSaveDataMembers.GetCount(); ++index)
	{
		if (savegameVerifyf( (typeOfSavegame == SAVEGAME_SINGLE_PLAYER) || (typeOfSavegame == SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA), "CScriptSaveData::HasAllScriptDataBeenDeserializedFromXmlFile - expected typeOfSavegame to be SAVEGAME_SINGLE_PLAYER or SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA"))
		{
			structsHaveData -= ms_aScriptSaveDataMembers[index]->m_NestedGlobalScriptData[typeOfSavegame].HasAllScriptDataBeenDeserializedFromXmlFile() ? 1:0;
		}
	}

	return structsHaveData==0;
}
#endif // SAVEGAME_USES_XML


#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
// The following function does the equivalent of 
// CScriptSaveData::ReadScriptTreesFromLoadedTree
// followed by 
// CNestedScriptData::DeserializeSavedData
bool CScriptSaveData::DeserializeForImport(parTree *pFullLoadedTree)
{
	bool mainLoaded = false;

	if (savegameVerifyf(pFullLoadedTree, "CScriptSaveData::DeserializeForImport - the main tree to be loaded does not exist"))
	{
		parTreeNode *pRootOfTreeToBeLoaded = pFullLoadedTree->GetRoot();
		if (savegameVerifyf(pRootOfTreeToBeLoaded, "CScriptSaveData::DeserializeForImport - the main tree to be loaded does not have a root node"))
		{
			mainLoaded = ms_oMainScriptSaveData.m_NestedGlobalScriptData[SAVEGAME_SINGLE_PLAYER].DeserializeForImport(pRootOfTreeToBeLoaded, ms_oMainScriptSaveData.m_pNameOfScriptTreeNode, ms_oMainScriptSaveData.m_pNameOfGlobalDataTree);

			parTreeNode *pDLCScriptNode = pRootOfTreeToBeLoaded->FindChildWithName(CSaveGameData::GetNameOfDLCScriptTreeNode());
			if (savegameVerifyf(pDLCScriptNode, "CScriptSaveData::DeserializeForImport - the root node of the main tree to be loaded does not have a node named %s", CSaveGameData::GetNameOfDLCScriptTreeNode()))
			{
				for(int index = 0; index<ms_aScriptSaveDataMembers.GetCount();++index)
				{
					ms_aScriptSaveDataMembers[index]->m_NestedGlobalScriptData[SAVEGAME_SINGLE_PLAYER].DeserializeForImport(pDLCScriptNode, ms_aScriptSaveDataMembers[index]->m_pNameOfScriptTreeNode, ms_aScriptSaveDataMembers[index]->m_pNameOfGlobalDataTree);
				}
			}
		}
	}

	return mainLoaded;
}
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

void CScriptSaveData::OpenScriptSaveData(eTypeOfSavegame typeOfSavegame, const void *pStartAddressOfStruct, s32 NumberOfScrValuesInStruct, GtaThread *pScriptThread)
{
	ms_pCurrentScriptData = NULL;

	savegameAssertf(strlen(pScriptThread->GetScriptName()) < SCRIPT_NAME_LENGTH, "CScriptSaveData::OpenScriptSaveData - name of script registering save data is %s. It should be less than %d characters. I'll try to fix it up for now", pScriptThread->GetScriptName(), SCRIPT_NAME_LENGTH);
	char scriptName[SCRIPT_NAME_LENGTH];
	safecpy(scriptName, pScriptThread->GetScriptName(), SCRIPT_NAME_LENGTH);

	if(atLiteralHashValue(scriptName)==atLiteralHashValue("startup"))
	{
		savegameDisplayf("CScriptSaveData::OpenScriptSaveData - setting ms_pCurrentScriptData for the main startup script");
		ms_pCurrentScriptData = &ms_oMainScriptSaveData;
	}
	for(int i = 0; i<ms_aScriptSaveDataMembers.GetCount() && !ms_pCurrentScriptData; ++i)
	{
		if (DoScriptNameHashesMatch(ms_aScriptSaveDataMembers[i]->m_NameHashOfScriptName, atLiteralHashValue(scriptName)))
		{
#if __ASSERT
			if ( (typeOfSavegame == SAVEGAME_SINGLE_PLAYER) && (MainHasRegisteredScriptData(SAVEGAME_SINGLE_PLAYER)) )
			{
				savegameAssertf(0, "CScriptSaveData::OpenScriptSaveData - the DLC script called %s is registering save data after the startup script has registered its save data. This could cause problems as all loaded single player save data is deleted a few frames after the data for startup is registered", scriptName);
			}
#endif	//	__ASSERT

			savegameDisplayf("CScriptSaveData::OpenScriptSaveData - setting ms_pCurrentScriptData for the DLC script called %s (hash %u)", scriptName, ms_aScriptSaveDataMembers[i]->m_NameHashOfScriptName.GetHash());
			ms_pCurrentScriptData = ms_aScriptSaveDataMembers[i];
			SetNameOfScriptTreeNode(i, scriptName);
		}
	}
	if(!ms_pCurrentScriptData)
	{
		CScriptSaveDataState* newItem = rage_new CScriptSaveDataState;
		newItem->m_bCreationOfScriptSaveStructureInProgress = false;
		newItem->m_eTypeOfSavegame = SAVEGAME_MAX_TYPES;
		newItem->m_pScriptThreadThatIsCreatingScriptSaveStructure = NULL;
		newItem->m_pStartAddressOfScriptSaveStructure = NULL;
		newItem->m_pEndAddressOfScriptSaveStructure = NULL;
		safecpy(newItem->m_pNameOfScriptTreeNode,scriptName, NELEM(newItem->m_pNameOfScriptTreeNode));
		strcpy(newItem->m_pNameOfGlobalDataTree,"Globals");
		newItem->SetHash();
		ms_aScriptSaveDataMembers.PushAndGrow(newItem);

#if __ASSERT
		if ( (typeOfSavegame == SAVEGAME_SINGLE_PLAYER) && (MainHasRegisteredScriptData(SAVEGAME_SINGLE_PLAYER)) )
		{
			savegameAssertf(0, "CScriptSaveData::OpenScriptSaveData - the DLC script called %s is registering save data after the startup script has registered its save data. This could cause problems as all loaded single player save data is deleted a few frames after the data for startup is registered", scriptName);
		}
#endif	//	__ASSERT

		savegameDisplayf("CScriptSaveData::OpenScriptSaveData - creating a new ms_pCurrentScriptData for the DLC script called %s (hash %u)", scriptName, newItem->m_NameHashOfScriptName.GetHash());
		ms_pCurrentScriptData = newItem;
		
	}
	if (CTheScripts::GetProcessingTheScriptsDuringGameInit())
	{
		savegameAssertf(0, "CScriptSaveData::OpenScriptSaveData - you can't call START_SAVE_DATA in the first frame of the game as any loaded savegame won't have been deserialized yet");
		return;
	}

	if (ms_pCurrentScriptData->m_bCreationOfScriptSaveStructureInProgress)
	{
		savegameAssertf(0, "CScriptSaveData::OpenScriptSaveData - you can't nest calls to START_SAVE_DATA");
		return;
	}

	if (pScriptThread == NULL)
	{
		savegameAssertf(0, "CScriptSaveData::OpenScriptSaveData - the GtaThread pointer is NULL");
		return;
	}

	if ((typeOfSavegame != SAVEGAME_SINGLE_PLAYER) && (typeOfSavegame != SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA))
	{
		savegameAssertf(0, "CScriptSaveData::OpenScriptSaveData - expected typeOfSavegame to be SAVEGAME_SINGLE_PLAYER or SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA");
		return;
	}

	bool bOpenedSuccessfully = ms_pCurrentScriptData->m_NestedGlobalScriptData[typeOfSavegame].OpenScriptSaveData(ms_pCurrentScriptData->m_pNameOfGlobalDataTree, pStartAddressOfStruct, NumberOfScrValuesInStruct, pScriptThread);

	if (bOpenedSuccessfully)
	{
		ms_pCurrentScriptData->m_bCreationOfScriptSaveStructureInProgress = true;
		ms_pCurrentScriptData->m_eTypeOfSavegame = typeOfSavegame;
		ms_pCurrentScriptData->m_pScriptThreadThatIsCreatingScriptSaveStructure = pScriptThread;
		ms_pCurrentScriptData->m_pStartAddressOfScriptSaveStructure = pStartAddressOfStruct;
		ms_pCurrentScriptData->m_pEndAddressOfScriptSaveStructure = ((const scrValue*) pStartAddressOfStruct) + NumberOfScrValuesInStruct;
	}

#if	!__NO_OUTPUT
	if (PARAM_output_registered_script_save_data.Get())
	{
		savegameScriptDataDisplayf("Start of savegame data for %s", scriptName);
		savegameScriptDataDisplayf("Size of scrValue = %d", (int) sizeof(scrValue));
	}
#endif	//	!__NO_OUTPUT
}

s32 CScriptSaveData::GetSizeOfSavedScriptData(eTypeOfSavegame typeOfSavegame, GtaThread *pScriptThread)
{
	if (savegameVerifyf( (typeOfSavegame == SAVEGAME_SINGLE_PLAYER) || (typeOfSavegame == SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA), "CScriptSaveData::GetSizeOfSavedScriptData - expected typeOfSavegame to be SAVEGAME_SINGLE_PLAYER or SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA"))
	{
		CScriptSaveDataState *pStateForThisScriptThread = NULL;
		if (savegameVerifyf(pScriptThread, "CScriptSaveData::GetSizeOfSavedScriptData - pScriptThread is NULL"))
		{
			savegameAssertf(strlen(pScriptThread->GetScriptName()) < SCRIPT_NAME_LENGTH, "CScriptSaveData::GetSizeOfSavedScriptData - name of script registering save data is %s. It should be less than %d characters. I'll try to fix it up for now", pScriptThread->GetScriptName(), SCRIPT_NAME_LENGTH);
			char scriptName[SCRIPT_NAME_LENGTH];
			safecpy(scriptName, pScriptThread->GetScriptName(), SCRIPT_NAME_LENGTH);

			atLiteralHashValue hashOfScriptName(scriptName);
			if(hashOfScriptName==atLiteralHashValue("startup"))
			{
				pStateForThisScriptThread = &ms_oMainScriptSaveData;
			}
			else
			{
				for(int i = 0; i<ms_aScriptSaveDataMembers.GetCount() && !pStateForThisScriptThread; ++i)
				{
					if (DoScriptNameHashesMatch(ms_aScriptSaveDataMembers[i]->m_NameHashOfScriptName, hashOfScriptName))
					{
						pStateForThisScriptThread = ms_aScriptSaveDataMembers[i];
					}
				}
			}

			if(pStateForThisScriptThread)
			{
				return pStateForThisScriptThread->m_NestedGlobalScriptData[typeOfSavegame].GetSizeOfSavedScriptData();
			}
		}
	}

	return 0;
}


void CScriptSaveData::PushOnToStructStack(u8* StartAddress, u8* EndAddress, const char *pNameOfInstance, int ArrayIndex)
{
	if(savegameVerifyf(ms_pCurrentScriptData!=NULL,"Current script data pointer is NULL!"))
		ms_pCurrentScriptData->m_ScriptSaveStack.Push(StartAddress, EndAddress, pNameOfInstance, ArrayIndex);
}

void CScriptSaveData::PopStructStack()
{
	if(savegameVerifyf(ms_pCurrentScriptData!=NULL,"Current script data pointer is NULL!"))
		ms_pCurrentScriptData->m_ScriptSaveStack.Pop();
}

#if !__NO_OUTPUT
char IndentString[256] = "";
u32 IndentCount = 0;

void AdjustIndentString(bool bIndentFurther)
{
	if (bIndentFurther)
	{
		IndentCount++;
	}
	else
	{
		if (IndentCount > 0)
		{
			IndentCount--;
		}
	}

	safecpy(IndentString, "");
	for (u32 indentLoop = 0; indentLoop < IndentCount; indentLoop++)
	{
		safecat(IndentString, " ");
	}
}
#endif	//	!__NO_OUTPUT

void CScriptSaveData::AddMemberToCurrentStruct(const char *pLabel, int DataType, const void *pStartAddress, GtaThread *pScriptThread)
{
	if(!savegameVerifyf(ms_pCurrentScriptData!=NULL,"Current script data pointer is NULL!"))
	{
		return;
	}

	if (!ms_pCurrentScriptData->m_bCreationOfScriptSaveStructureInProgress)
	{
		savegameAssertf(0, "CScriptSaveData::AddMemberToCurrentStruct - %s START_SAVE_DATA has to be called before registering any variables to be saved", pScriptThread->GetScriptName());
		return;
	}

	if (pScriptThread != ms_pCurrentScriptData->m_pScriptThreadThatIsCreatingScriptSaveStructure)
	{
		savegameAssertf(0, "CScriptSaveData::AddMemberToCurrentStruct - this function has been called by %s but START_SAVE_DATA was called by %s", pScriptThread->GetScriptName(), ms_pCurrentScriptData->m_pScriptThreadThatIsCreatingScriptSaveStructure->GetScriptName());
		return;
	}

	if (!CheckAddressIsWithinSaveStruct(pStartAddress))
	{
		savegameAssertf(0, "CScriptSaveData::AddMemberToCurrentStruct - %s %s this member variable is not inside the structure passed to START_SAVE_DATA", pScriptThread->GetScriptName(), pLabel);
		return;
	}

	if (savegameVerifyf( (ms_pCurrentScriptData->m_eTypeOfSavegame == SAVEGAME_SINGLE_PLAYER) || (ms_pCurrentScriptData->m_eTypeOfSavegame == SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA), "CScriptSaveData::AddMemberToCurrentStruct - expected ms_TypeOfSavegame to be SAVEGAME_SINGLE_PLAYER or SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA"))
	{
		ms_pCurrentScriptData->m_NestedGlobalScriptData[ms_pCurrentScriptData->m_eTypeOfSavegame].AddMemberToCurrentStruct(pLabel, DataType, pStartAddress, pScriptThread);

#if !__NO_OUTPUT
		if (PARAM_output_registered_script_save_data.Get())
		{
			const char *pTypeString = "INT";

			switch (DataType)
			{
			case Script_Save_Data_Type_Int :
//			Script_Save_Data_Type_Bool
				break;

			case Script_Save_Data_Type_Int64 :
				pTypeString = "INT64";
				break;

			case Script_Save_Data_Type_Float :
				pTypeString = "FLOAT";
				break;

			case Script_Save_Data_Type_TextLabel3 :
				pTypeString = "TEXT_LABEL_3";
				break;
			case Script_Save_Data_Type_TextLabel7 :
				pTypeString = "TEXT_LABEL_7";
				break;
			case Script_Save_Data_Type_TextLabel15 :
				pTypeString = "TEXT_LABEL_15";
				break;
			case Script_Save_Data_Type_TextLabel23 :
				pTypeString = "TEXT_LABEL_23";
				break;
			case Script_Save_Data_Type_TextLabel31 :
				pTypeString = "TEXT_LABEL_31";
				break;
			case Script_Save_Data_Type_TextLabel63 :
				pTypeString = "TEXT_LABEL_63";
				break;
			}
			savegameScriptDataDisplayf("%s%s %s", IndentString, pTypeString, pLabel);
		}
#endif	//	!__NO_OUTPUT
	}
}

void CScriptSaveData::OpenScriptStruct(const char *pLabel, void *pStartAddress, s32 NumberOfScrValuesInStruct, bool bIsArray, GtaThread *pScriptThread)
{
	if(!savegameVerifyf(ms_pCurrentScriptData!=NULL,"Current script data pointer is NULL!"))
	{
		return;
	}

	if (!ms_pCurrentScriptData->m_bCreationOfScriptSaveStructureInProgress)
	{
		savegameAssertf(0, "CScriptSaveData::OpenScriptStruct - %s START_SAVE_DATA has to be called before registering any variables to be saved", pScriptThread->GetScriptName());
		return;
	}

	if (pScriptThread != ms_pCurrentScriptData->m_pScriptThreadThatIsCreatingScriptSaveStructure)
	{
		savegameAssertf(0, "CScriptSaveData::OpenScriptStruct - this function has been called by %s but START_SAVE_DATA was called by %s", pScriptThread->GetScriptName(), ms_pCurrentScriptData->m_pScriptThreadThatIsCreatingScriptSaveStructure->GetScriptName());
		return;
	}

	if (!CheckAddressIsWithinSaveStruct(pStartAddress))
	{
		savegameAssertf(0, "CScriptSaveData::OpenScriptStruct - %s %s this member variable is not inside the structure passed to START_SAVE_DATA", pScriptThread->GetScriptName(), pLabel);
		return;
	}

	if (savegameVerifyf( (ms_pCurrentScriptData->m_eTypeOfSavegame == SAVEGAME_SINGLE_PLAYER) || (ms_pCurrentScriptData->m_eTypeOfSavegame == SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA), "CScriptSaveData::OpenScriptStruct - expected ms_TypeOfSavegame to be SAVEGAME_SINGLE_PLAYER or SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA"))
	{
		ms_pCurrentScriptData->m_NestedGlobalScriptData[ms_pCurrentScriptData->m_eTypeOfSavegame].OpenScriptStruct(pLabel, pStartAddress, NumberOfScrValuesInStruct, bIsArray, pScriptThread);

#if !__NO_OUTPUT
		if (PARAM_output_registered_script_save_data.Get())
		{
			savegameScriptDataDisplayf("%s%s %s", IndentString, bIsArray?"ARRAY":"STRUCT", pLabel);
			savegameScriptDataDisplayf("%s{", IndentString);

			AdjustIndentString(true);
		}
#endif	//	!__NO_OUTPUT
	}
}

void CScriptSaveData::CloseScriptStruct(GtaThread *pScriptThread)
{
	if(!savegameVerifyf(ms_pCurrentScriptData!=NULL,"Current script data pointer is NULL!"))
	{
		return;
	}
	if (!ms_pCurrentScriptData->m_bCreationOfScriptSaveStructureInProgress)
	{
		savegameAssertf(0, "CScriptSaveData::CloseScriptStruct - STOP_SAVE_DATA/STOP_SAVE_STRUCT has been called without a matching START_SAVE_DATA/START_SAVE_STRUCT %s", pScriptThread->GetScriptName());
		return;
	}

	if (pScriptThread != ms_pCurrentScriptData->m_pScriptThreadThatIsCreatingScriptSaveStructure)
	{
		savegameAssertf(0, "CScriptSaveData::CloseScriptStruct - this function has been called by %s but START_SAVE_DATA was called by %s", pScriptThread->GetScriptName(), ms_pCurrentScriptData->m_pScriptThreadThatIsCreatingScriptSaveStructure->GetScriptName());
		return;
	}

	bool bStackIsNowEmpty = false;
	if (savegameVerifyf( (ms_pCurrentScriptData->m_eTypeOfSavegame == SAVEGAME_SINGLE_PLAYER) || (ms_pCurrentScriptData->m_eTypeOfSavegame == SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA), "CScriptSaveData::CloseScriptStruct - expected ms_TypeOfSavegame to be SAVEGAME_SINGLE_PLAYER or SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA"))
	{
		bStackIsNowEmpty = ms_pCurrentScriptData->m_NestedGlobalScriptData[ms_pCurrentScriptData->m_eTypeOfSavegame].CloseScriptStruct();
	}

	if (bStackIsNowEmpty)
	{
		ms_pCurrentScriptData->m_bCreationOfScriptSaveStructureInProgress = false;
		ms_pCurrentScriptData->m_pScriptThreadThatIsCreatingScriptSaveStructure = NULL;
		ms_pCurrentScriptData->m_pStartAddressOfScriptSaveStructure = NULL;
		ms_pCurrentScriptData->m_pEndAddressOfScriptSaveStructure = NULL;

		if (ms_pCurrentScriptData->m_eTypeOfSavegame == SAVEGAME_SINGLE_PLAYER)
		{
			savegameVerifyf(DeserializeSavedData(ms_pCurrentScriptData->m_eTypeOfSavegame),"Couldn't deserialize script data for %s",ms_pCurrentScriptData->m_pNameOfScriptTreeNode);				
		}
		ms_pCurrentScriptData->m_eTypeOfSavegame = SAVEGAME_MAX_TYPES;
	}

#if !__NO_OUTPUT
	if (PARAM_output_registered_script_save_data.Get())
	{
		AdjustIndentString(false);

		savegameScriptDataDisplayf("%s}", IndentString);
	}
#endif	//	!__NO_OUTPUT
}


bool CScriptSaveData::DeserializeSavedData(eTypeOfSavegame typeOfSavegame)
{
	if(!savegameVerifyf(ms_pCurrentScriptData!=NULL,"Current script data pointer is NULL!"))
	{
		return false;
	}
	if (savegameVerifyf(!ms_pCurrentScriptData->m_bCreationOfScriptSaveStructureInProgress, "CScriptSaveData::DeserializeSavedData - this shouldn't be called while script save data is being registered"))
	{
		if (savegameVerifyf( (typeOfSavegame == SAVEGAME_SINGLE_PLAYER), "CScriptSaveData::DeserializeSavedData - expected typeOfSavegame to be SAVEGAME_SINGLE_PLAYER"))
		{
			savegameDisplayf("CScriptSaveData::DeserializeSavedData - ");

			if (ms_pCurrentScriptData == &ms_oMainScriptSaveData)
			{	//	If we're currently deserializing the main script data then set a countdown number of frames before calling ForceCleanupOfScriptSaveStructs.
				//	NUMBER_OF_FRAMES_BEFORE_FORCING_CLEANUP_OF_LOADED_SCRIPT_DATA might need adjusted.
				//	For now, I'm assuming that all save data for DLC scripts will be deserialized within a few frames after the main script (or even before the main script).
				ms_CountdownToForceCleanup = NUMBER_OF_FRAMES_BEFORE_FORCING_CLEANUP_OF_LOADED_SCRIPT_DATA;
			}

			return ms_pCurrentScriptData->m_NestedGlobalScriptData[typeOfSavegame].DeserializeSavedData(ms_pCurrentScriptData->m_pNameOfGlobalDataTree);
		}
	}

	return false;
}


bool CScriptSaveData::DoScriptNameHashesMatch(atLiteralHashValue hash1, atLiteralHashValue hash2)
{
	if (hash1 == hash2)
	{
		return true;
	}

	const u32 upper = ms_HashOfStandardGlobalRegScriptNameWithUpperCaseS.GetHash();
	const u32 lower = ms_HashOfStandardGlobalRegScriptNameWithLowerCaseS.GetHash();

	const u32 h1 = hash1.GetHash();
	if ( (h1 == upper) || (h1 == lower) )
	{
		const u32 h2 = hash2.GetHash();
		if ( (h2 == upper) || (h2 == lower) )
		{
			savegameDisplayf("CScriptSaveData::DoScriptNameHashesMatch - returning TRUE. hash1 is %u(%s). hash2 is %u(%s)", 
				h1, (h1==upper)?ms_HashOfStandardGlobalRegScriptNameWithUpperCaseS.GetCStr():ms_HashOfStandardGlobalRegScriptNameWithLowerCaseS.GetCStr(),
				h2, (h2==upper)?ms_HashOfStandardGlobalRegScriptNameWithUpperCaseS.GetCStr():ms_HashOfStandardGlobalRegScriptNameWithLowerCaseS.GetCStr() );

			return true;
		}
	}

	return false;
}

void CScriptSaveData::SetNameOfScriptTreeNode(int index, const char *scriptName)
{
// There are four scripts that register save data.
// The main script (startup.sc) actually only registers one script variable for Single Player save data. 
// Two of the "DLC scripts" (sp_dlc_registration.sc and sp_pilotschool_reg.sc) also only register one script variable each. 
// The other "DLC script" is Standard_global_reg.sc. It contains all the real Single Player save data.

// The Build team have changed the way that they compile the scripts.
// This has changed the ScriptName string inside scrProgram
// The old method (compiling with the Rage Script Editor) would result in this string always being lower case for all scrProgram's
// The new method (compiling with IncrediBuildScriptProject.exe via OozyBuild) causes the string to keep the case of the .sc filename as it exists in Perforce and on the local hard drive.
// This only affects one of the four savegame scripts (Standard_global_reg.sc) The filenames of the other three .sc files are lower case.

// When loading a savegame, CScriptSaveData::ReadScriptTreesFromLoadedPso() will fill in the m_NameHashOfScriptName using the hash from the savegame.
// This hash is an atLiteralHashValue (so it's case sensitive).
// In the case of the save data for the "standard_global_reg", the hash will depend on whether the script was called "Standard_global_reg" or "standard_global_reg" when the save was created.

// A bit later, when the session starts, the scripts will run and register their save data.
// This starts when they call CScriptSaveData::OpenScriptSaveData()
// OpenScriptSaveData() tries to match the loaded script data with the script that is currently registering.
// It does this by calling CScriptSaveData::DoScriptNameHashesMatch()

// In the past, we've just set m_pNameOfScriptTreeNode to be the name of the script that called OpenScriptSaveData()
// But now that could cause m_pNameOfScriptTreeNode to be "Standard_global_reg" while m_NameHashOfScriptName is the literal hash of "standard_global_reg" (or vice versa)

// The idea with this SetNameOfScriptTreeNode() function is to ensure that m_pNameOfScriptTreeNode and m_NameHashOfScriptName match by setting the name according to the value of the hash.
// If we didn't do this then I think a later attempt to save would fail to save any of the "standard_global_reg" data.
// It looks like AddActiveScriptDataToPsoToBeSaved() uses a mixture of m_NameHashOfScriptName and the atLiteralHashValue of m_pNameOfScriptTreeNode

// I'm relying on the strings being available in Final builds for 
// ms_HashOfStandardGlobalRegScriptNameWithUpperCaseS
//  and 
// ms_HashOfStandardGlobalRegScriptNameWithLowerCaseS

	if (savegameVerifyf((index >= 0) && (index < ms_aScriptSaveDataMembers.GetCount()), "CScriptSaveData::SetNameOfScriptTreeNode - index %d is out of range. Expected it to be >=0 and < %d", index, ms_aScriptSaveDataMembers.GetCount()) )
	{
		CScriptSaveDataState &currentArrayEntry = *ms_aScriptSaveDataMembers[index];
		const u32 hash = currentArrayEntry.m_NameHashOfScriptName.GetHash();

		const atLiteralHashString &upper = ms_HashOfStandardGlobalRegScriptNameWithUpperCaseS;
		const atLiteralHashString &lower = ms_HashOfStandardGlobalRegScriptNameWithLowerCaseS;

#if __ASSERT
		if ( (hash == upper.GetHash()) || (hash == lower.GetHash()) )
		{
			savegameAssertf(!strcmp(scriptName, "Standard_global_reg") || !strcmp(scriptName, "standard_global_reg"), "CScriptSaveData::SetNameOfScriptTreeNode - expected scriptName to be either 'Standard_global_reg' or 'standard_global_reg', but it's %s", scriptName);
		}
#endif // __ASSERT

		if (hash == upper.GetHash())
		{
			scriptName = upper.GetCStr();
		}
		else if (hash == lower.GetHash())
		{
			scriptName = lower.GetCStr();
		}

		safecpy(currentArrayEntry.m_pNameOfScriptTreeNode, scriptName, NELEM(currentArrayEntry.m_pNameOfScriptTreeNode));
	}
}



bool CScriptSaveData::CheckAddressIsWithinSaveStruct(const void *pAddressToCheck)
{
	if(!savegameVerifyf(ms_pCurrentScriptData!=NULL,"Current script data pointer is NULL!"))
	{
		return false;
	}
	return (pAddressToCheck >= ms_pCurrentScriptData->m_pStartAddressOfScriptSaveStructure && pAddressToCheck < ms_pCurrentScriptData->m_pEndAddressOfScriptSaveStructure);
}


void CScriptSaveData::ForceCleanupOfScriptSaveStructs()
{
#if !__NO_OUTPUT
	int FileCount = ms_aFiles.GetCount();
	savegameDisplayf("CScriptSaveData::ForceCleanupOfScriptSaveStructs - number of entries in ms_aFiles array is %d before cleanup. If it's not 0 now, there are two possible reasons - 1. You are loading a savegame that contains a block of script save data that is no longer being registered by script or 2. Graeme is calling ForceCleanupOfScriptSaveStructs() before all scripts have had a chance to register their save data", FileCount);
	savegameAssertf(FileCount == 0, "CScriptSaveData::ForceCleanupOfScriptSaveStructs - number of entries in ms_aFiles array is %d before cleanup. If it's not 0 now, there are two possible reasons - 1. You are loading a savegame that contains a block of script save data that is no longer being registered by script or 2. Graeme is calling ForceCleanupOfScriptSaveStructs() before all scripts have had a chance to register their save data", FileCount);
#endif	//	!__NO_OUTPUT

	savegameDisplayf("CScriptSaveData::ForceCleanupOfScriptSaveStructs - about to call ForceCleanupOfScriptSaveStructs for the main script save data");
	ms_oMainScriptSaveData.m_NestedGlobalScriptData[SAVEGAME_SINGLE_PLAYER].ForceCleanupOfScriptSaveStructs(ms_oMainScriptSaveData.m_pNameOfGlobalDataTree);

	for(int index = 0; index < ms_aScriptSaveDataMembers.GetCount(); index++)
	{
		savegameDisplayf("CScriptSaveData::ForceCleanupOfScriptSaveStructs - about to call ForceCleanupOfScriptSaveStructs for the DLC script save data in slot %d with hash %u", index, ms_aScriptSaveDataMembers[index]->m_NameHashOfScriptName.GetHash());
		ms_aScriptSaveDataMembers[index]->m_NestedGlobalScriptData[SAVEGAME_SINGLE_PLAYER].ForceCleanupOfScriptSaveStructs(ms_aScriptSaveDataMembers[index]->m_pNameOfGlobalDataTree);
	}

#if !__NO_OUTPUT
	FileCount = ms_aFiles.GetCount();
	savegameDisplayf("CScriptSaveData::ForceCleanupOfScriptSaveStructs - number of entries in ms_aFiles array is %d after cleanup. It should be 0 now", FileCount);
	savegameAssertf(FileCount == 0, "CScriptSaveData::ForceCleanupOfScriptSaveStructs - number of entries in ms_aFiles array is %d after cleanup. It should be 0 now", FileCount);
#endif	//	!__NO_OUTPUT
}

/*
void DisplaySizeOfRTStructure(parRTStructure *pRTStructure)
{
//	m_Name
	int Size = strlen(pRTStructure->GetName());

//	m_Members
	Size += 32 * sizeof(parMember*);

//	m_MemberData
	struct AnyData
	{
		union {
			// we make sure the struct is big enough to hold any data
			char	StructDataBuf	[sizeof(parMemberStruct::Data)];
			char	ArrayDataBuf	[sizeof(parMemberArray::Data)];
			char	EnumDataBuf		[sizeof(parMemberEnum::Data)];
			char	SimpleDataBuf	[sizeof(parMemberSimple::Data)];
			char	StringDataBuf	[sizeof(parMemberString::Data)];
			char	VectorDataBuf	[sizeof(parMemberVector::Data)];
			char	MatrixDataBuf	[sizeof(parMemberMatrix::Data)];
		};
	};

	parMemberStructData InstanceOfStructData;
	int TempSize = sizeof(InstanceOfStructData);
	savegameDebugf1("Size of parMemberStructData = %d\n", TempSize);

	datCallback InstanceOfDatCallback;
	TempSize = sizeof(InstanceOfDatCallback);
	savegameDebugf1("Size of datCallback = %d\n", TempSize);

	parMemberSimpleData InstanceOfSimpleData;
	TempSize = sizeof(InstanceOfSimpleData);
	savegameDebugf1("Size of parMemberSimpleData = %d\n", TempSize);

	int sizeofAnyData = sizeof(AnyData);

	Size += 32 * sizeofAnyData;

//	m_MemberNamePool
	Size += 1024;

	Size += sizeof(*pRTStructure);

	Assertf(0, "Size of RTStructure is %d", Size);
}
*/


int CScriptSaveData::GetNumOfScriptStructs(eTypeOfSavegame saveGameType)
{
	int savedataCount = 0;
	for(int i=0; i<ms_aScriptSaveDataMembers.GetCount();i++)
	{
		switch(saveGameType)
		{
		case SAVEGAME_SINGLE_PLAYER:
			savedataCount+= ms_aScriptSaveDataMembers[i]->HasSingleplayerData();
			break;
		case SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA:
			savedataCount+= ms_aScriptSaveDataMembers[i]->HasMultiplayerData();
		default:			
			break;		
		}
	}
	return savedataCount;
}

void CScriptSaveData::AddActiveScriptDataToPsoToBeSaved(eTypeOfSavegame typeOfSavegame, psoBuilder& builder, psoBuilderInstance* rootInstance)
{
	OUTPUT_ONLY( u32 size = builder.ComputeSize(); )
		psoSizeDebugf1("CNestedScriptData::CreatePsoStructuresForSaving :: Start size is '%d'", size);

	//Main script data
	if (savegameVerifyf( (typeOfSavegame == SAVEGAME_SINGLE_PLAYER) || (typeOfSavegame == SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA), "CScriptSaveData::AddActiveScriptDataToPsoToBeSaved - expected typeOfSavegame to be SAVEGAME_SINGLE_PLAYER or SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA"))
	{
		if(savegameVerifyf(CScriptSaveData::MainHasRegisteredScriptData(typeOfSavegame), "CSaveGameBuffer::CreateSaveDataAndCalculateSize - expected main script to have already registered save data before saving in pso format"))	
		{
			ms_oMainScriptSaveData.m_NestedGlobalScriptData[typeOfSavegame].AddActiveScriptDataToPsoToBeSaved(builder, rootInstance ,ms_oMainScriptSaveData.m_pNameOfScriptTreeNode);

			psoSizeDebugf1("Size Of Member '%s' is '%d'", ms_oMainScriptSaveData.m_pNameOfScriptTreeNode, builder.ComputeSize() - size);
			OUTPUT_ONLY( size = builder.ComputeSize(); )
		}
	}

	//DLC script data
	if(GetNumOfScriptStructs(typeOfSavegame)>0)
	{
		psoBuilderStructSchema& dlcScriptNameContainerSchema = builder.CreateStructSchema(atLiteralHashValue(CSaveGameData::GetNameOfDLCScriptTreeNode()));
		for(int index = 0; index<ms_aScriptSaveDataMembers.GetCount();++index)
		{
			if (savegameVerifyf( (typeOfSavegame == SAVEGAME_SINGLE_PLAYER) || (typeOfSavegame == SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA), "CScriptSaveData::AddActiveScriptDataToPsoToBeSaved - expected typeOfSavegame to be SAVEGAME_SINGLE_PLAYER or SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA"))
			{
				if(CScriptSaveData::DLCHasRegisteredScriptData(typeOfSavegame,index))	
				{
					dlcScriptNameContainerSchema.AddMemberPointer(ms_aScriptSaveDataMembers[index]->m_NameHashOfScriptName, parMemberStructSubType::SUBTYPE_SIMPLE_POINTER);

					psoSizeDebugf1("Size Of Member '%s' is '%d'", ms_aScriptSaveDataMembers[index]->m_pNameOfScriptTreeNode, builder.ComputeSize() - size);
					OUTPUT_ONLY( size = builder.ComputeSize(); )
				}
			}
		}
		dlcScriptNameContainerSchema.FinishBuilding();

		psoBuilderInstance& dlcScriptNameContainer = builder.CreateStructInstances(dlcScriptNameContainerSchema);

		for(int index = 0; index<ms_aScriptSaveDataMembers.GetCount();++index)
		{
			if (savegameVerifyf( (typeOfSavegame == SAVEGAME_SINGLE_PLAYER) || (typeOfSavegame == SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA), "CScriptSaveData::AddActiveScriptDataToPsoToBeSaved - expected typeOfSavegame to be SAVEGAME_SINGLE_PLAYER or SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA"))
			{
				if(CScriptSaveData::DLCHasRegisteredScriptData(typeOfSavegame,index))	
				{
					ms_aScriptSaveDataMembers[index]->m_NestedGlobalScriptData[typeOfSavegame].AddActiveScriptDataToPsoToBeSaved(builder, &dlcScriptNameContainer ,ms_aScriptSaveDataMembers[index]->m_pNameOfScriptTreeNode);

					psoSizeDebugf1("Size Of Member '%s' is '%d'", ms_aScriptSaveDataMembers[index]->m_pNameOfScriptTreeNode, builder.ComputeSize() - size);
					OUTPUT_ONLY( size = builder.ComputeSize(); )
				}
			}
		} 
		rootInstance->AddFixup(0, CSaveGameData::GetNameOfDLCScriptTreeNode(), &dlcScriptNameContainer);
	}

	psoSizeDebugf1("CNestedScriptData::CreatePsoStructuresForSaving :: End size is '%d'", size);
}

