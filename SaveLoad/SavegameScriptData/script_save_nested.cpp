

#include "script_save_nested.h"

// Rage headers
#include "parser/psofile.h"

// Game headers
#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/SavegameScriptData/script_save_data.h"
#include "script/gta_thread.h"

#define MAX_LENGTH_OF_LABEL_FOR_SAVE_DATA		(64)

// **************************************** SaveGameDataBlock - Script - CNestedScriptData ************************************

CNestedScriptData::~CNestedScriptData()
{
	Shutdown();
}

void CNestedScriptData::Shutdown()
{
	m_ScriptSaveArrayOfStructs.Shutdown();

#if SAVEGAME_USES_XML
	m_ScriptSaveXmlTrees.Shutdown();
#endif

#if SAVEGAME_USES_PSO
	m_ScriptSavePsoFileTracker.Shutdown();
#endif
}

bool CNestedScriptData::OpenScriptSaveData(const char *pLabel, const void *pStartAddressOfStruct, s32 NumberOfScrValuesInStruct, GtaThread *pScriptThread)
{
#if SAVEGAME_USES_XML
	if (m_ScriptSaveArrayOfStructs.GetTopLevelRTStructure())
	{
		savegameAssertf(0, "CNestedScriptData::OpenScriptSaveData - START_SAVE_DATA has already been called for %s", pScriptThread->GetScriptName());
		return false;
	}
#endif

	if (CScriptSaveData::GetStructStack().GetStackLevel() != -1)
	{
		savegameAssertf(0, "CNestedScriptData::OpenScriptSaveData - expected stack level to be -1 for the stack of script structs in %s", pScriptThread->GetScriptName());
		return false;
	}

	scrValue *pStartAddress = (scrValue*)pStartAddressOfStruct;
	if(atLiteralHashValue(pScriptThread->GetScriptName())==atLiteralHashValue("startup"))
	{
		if (!scrThread::IsValidGlobal(pStartAddress))
		{
			savegameAssertf(0, "CNestedScriptData::OpenScriptSaveData - %s %s Start address of the global save struct is outside the global save data block", pScriptThread->GetScriptName(), pLabel);
			return false;
			//DLCTODO: Make sure this works properly, not stopping at the moment, works fine
		}

		if (!scrThread::IsValidGlobal(pStartAddress + NumberOfScrValuesInStruct - 1))
		{
			savegameAssertf(0, "CNestedScriptData::OpenScriptSaveData - %s %s End address of the global save struct is outside the global save data block", pScriptThread->GetScriptName(), pLabel);
			return false;
			//DLCTODO: Make sure this works properly, not stopping at the moment, works fine
		}
	}

	int ArrayIndex = m_ScriptSaveArrayOfStructs.AddNewScriptStruct(pScriptThread->GetProgramCounter(0), false, pLabel);
	savegameAssertf(ArrayIndex == 0, "CNestedScriptData::OpenScriptSaveData - expected the array index of the struct containing all the other data to be 0");

	u8* StartAddress = reinterpret_cast<u8*>(pStartAddress);
	u8* EndAddress = reinterpret_cast<u8*>(pStartAddress + NumberOfScrValuesInStruct);
	CScriptSaveData::PushOnToStructStack(StartAddress, EndAddress, pLabel, ArrayIndex);

	m_ScriptSaveArrayOfStructs.SetDataForTopLevelStruct(StartAddress, pLabel);

	return true;
}

void CNestedScriptData::AddMemberToCurrentStruct(const char *pLabel, int DataType, const void *pStartAddress, GtaThread *ASSERT_ONLY(pScriptThread))
{
	if (!CheckElementName(pLabel))
	{
		return;
	}

	int CurrentStackLevel = CScriptSaveData::GetStructStack().GetStackLevel();
	if (savegameVerifyf(CurrentStackLevel >= 0, "CNestedScriptData::AddMemberToCurrentStruct - stack of structs is empty %s", pScriptThread->GetScriptName()))
	{
		int ArrayIndexOfStructAtCurrentStackLevel = CScriptSaveData::GetStructStack().GetArrayIndex(CurrentStackLevel);
		if (!m_ScriptSaveArrayOfStructs.GetHasBeenClosed(ArrayIndexOfStructAtCurrentStackLevel))
		{
			const u8* StartAddressAsUInt32 = reinterpret_cast<const u8*>(pStartAddress);
			u8* StartAddressOfParentStruct = CScriptSaveData::GetStructStack().GetStartAddress(CurrentStackLevel);
			u8* EndAddressOfParentStruct = CScriptSaveData::GetStructStack().GetEndAddress(CurrentStackLevel);

			if (savegameVerifyf(StartAddressAsUInt32 >= StartAddressOfParentStruct, "CNestedScriptData::AddMemberToCurrentStruct - %s occurs outside its parent struct", pLabel))
			{
				if (savegameVerifyf( (EndAddressOfParentStruct == 0) || (StartAddressAsUInt32 < EndAddressOfParentStruct), "CNestedScriptData::AddMemberToCurrentStruct - %s occurs outside its parent struct", pLabel))
				{
					ptrdiff_t Offset = StartAddressAsUInt32 - StartAddressOfParentStruct;
					Assert(Offset <= 0xffffffff);

					savegameDebugf3("CNestedScriptData::AddMemberToCurrentStruct - adding %s with offset %d (%u). Start Address Of Parent Struct = %p Address Of This Member = %p", pLabel, (s32) Offset, (u32) Offset, StartAddressOfParentStruct, StartAddressAsUInt32);

#if !__NO_OUTPUT
					if (Offset > 200000)
					{
						savegameDebugf1("CNestedScriptData::AddMemberToCurrentStruct - offset %06d label %s", (s32) Offset, pLabel);
					}
#endif	//	!__NO_OUTPUT

					savegameAssertf(Offset < psoSchemaMemberData::MAX_OFFSET, "CNestedScriptData::AddMemberToCurrentStruct - offset %d of %s is too large. It needs to be less than psoSchemaMemberData::MAX_OFFSET (%d)", (s32) Offset, pLabel, psoSchemaMemberData::MAX_OFFSET);

					m_ScriptSaveArrayOfStructs.AddNewDataItemToStruct(ArrayIndexOfStructAtCurrentStackLevel, u32(Offset), DataType, pLabel);
				}
			}
		}
	}
}

void CNestedScriptData::OpenScriptStruct(const char *pLabel, void *pStartAddress, s32 NumberOfScrValuesInStruct, bool bIsArray, GtaThread *pScriptThread)
{
	if (!CheckElementName(pLabel))
	{
		return;
	}

	int ArrayIndex = m_ScriptSaveArrayOfStructs.AddNewScriptStruct(pScriptThread->GetProgramCounter(0), bIsArray, pLabel);
	AddMemberToCurrentStruct(pLabel, ArrayIndex, pStartAddress, pScriptThread);
	u8* StartAddress = reinterpret_cast<u8*>(pStartAddress);
	u8* EndAddress = NULL;
	if (NumberOfScrValuesInStruct > 0)
	{
		EndAddress = reinterpret_cast<u8*>( ((scrValue*) pStartAddress) + NumberOfScrValuesInStruct);
	}
	CScriptSaveData::PushOnToStructStack(StartAddress, EndAddress, pLabel, ArrayIndex);
}

bool CNestedScriptData::CloseScriptStruct()
{
	int CurrentStackLevel = CScriptSaveData::GetStructStack().GetStackLevel();

	//	char NameOfPoppedStruct[MAX_LENGTH_OF_LABEL_FOR_SAVE_DATA];
	//	const atString &NameOfStruct = CScriptSaveData::GetStructStack().GetNameOfInstance(CurrentStackLevel);
	//	strncpy(NameOfPoppedStruct, NameOfStruct.c_str(), MAX_LENGTH_OF_LABEL_FOR_SAVE_DATA);

	int ArrayIndexOfPoppedStruct = CScriptSaveData::GetStructStack().GetArrayIndex(CurrentStackLevel);
	m_ScriptSaveArrayOfStructs.ClearRegisteredElementNames(ArrayIndexOfPoppedStruct);
	m_ScriptSaveArrayOfStructs.SetHasBeenClosed(ArrayIndexOfPoppedStruct, true);

	CScriptSaveData::PopStructStack();

	bool bStackIsNowEmpty = false;
	if (CurrentStackLevel == 0)
	{	//	we've just popped the top level
		bStackIsNowEmpty = true;
	}

	return bStackIsNowEmpty;
}

void CNestedScriptData::ForceCleanupOfScriptSaveStructs(const char* pNameOfGlobalDataTree)
{
#if SAVEGAME_USES_PSO
	atLiteralHashValue nameHash(pNameOfGlobalDataTree);
	psoStruct* pStruct = m_ScriptSavePsoFileTracker.GetStruct(nameHash);
	if (pStruct)
	{
		if (pStruct->IsValid() && !pStruct->IsNull())
		{
			savegameDisplayf("CNestedScriptData::ForceCleanupOfScriptSaveStructs - About to call ForceCleanup for %s", pNameOfGlobalDataTree);
			m_ScriptSavePsoFileTracker.ForceCleanup();
		}
	}
#endif	//	SAVEGAME_USES_PSO
}

bool CNestedScriptData::DeserializeSavedData(const char* pNameOfGlobalDataTree)
{
	//	if (bStackIsNowEmpty)
	{
#if SAVEGAME_USES_PSO
		// Check for PSO data to load
		atLiteralHashValue nameHash(pNameOfGlobalDataTree);
		psoStruct* pStruct = m_ScriptSavePsoFileTracker.GetStruct(nameHash);
		if (pStruct)
		{
			if (pStruct->IsValid() && !pStruct->IsNull())
			{
				m_ScriptSaveArrayOfStructs.LoadFromPsoStruct(*pStruct);
				savegameDisplayf("CNestedScriptData::DeserializeSavedData - About to call DeleteStruct for %s", pNameOfGlobalDataTree);
				m_ScriptSavePsoFileTracker.DeleteStruct(nameHash);
			}
		}
#else
		if (0) { }
#endif

#if SAVEGAME_USES_XML
		else
		{
			//	Search the array of parTrees for one with a matching name
			int ArrayCount = m_ScriptSaveXmlTrees.GetTreeCount();
			int loop = 0;
			bool bFound = false;
			while ((loop < ArrayCount) && !bFound)
			{
				if (m_ScriptSaveXmlTrees.GetTreeNode(loop))
				{
					parTreeNode *pRootNode = m_ScriptSaveXmlTrees.GetTreeNode(loop);
					if (pRootNode && (strcmp(pRootNode->GetElement().GetName(), pNameOfGlobalDataTree) == 0))
					{
						m_ScriptSaveArrayOfStructs.CreateRTStructuresForLoading();
						parRTStructure *pRTStructureOfSaveData = m_ScriptSaveArrayOfStructs.GetTopLevelRTStructure();

						if (savegameVerifyf(pRTStructureOfSaveData, "CNestedScriptData::CloseScriptStruct - RTStructure doesn't exist for %s", pNameOfGlobalDataTree))
						{
							PARSER.LoadFromStructure(pRootNode, *pRTStructureOfSaveData, NULL, false);
						}

						m_ScriptSaveArrayOfStructs.DeleteRTStructures();

						m_ScriptSaveXmlTrees.DeleteTreeFromArray(loop);
						bFound = true;
					}
				}	//	if (m_ScriptSaveXmlTrees.GetTreeNode(loop))

				loop++;
			}	//	while ((loop < ArrayCount) && !bFound)
		}
#endif
	}

	return true;	//	I'm always returning true just now. Is there any way I can check for failure?
}


atString& CNestedScriptData::GetTopLevelName()
{
	return m_ScriptSaveArrayOfStructs.GetTopLevelName();
}

bool CNestedScriptData::HasRegisteredScriptData()
{
	return m_ScriptSaveArrayOfStructs.HasRegisteredScriptData();
}

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES
void CNestedScriptData::SetBufferForImportExport(u8 *pBaseAddress)
{
	m_ScriptSaveArrayOfStructs.SetBufferForImportExport(pBaseAddress);
}
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES


#if SAVEGAME_USES_XML
void CNestedScriptData::DeleteAllRTStructures()
{
	m_ScriptSaveArrayOfStructs.DeleteRTStructures();
}
#endif // SAVEGAME_USES_XML

#if SAVEGAME_USES_XML || __ALLOW_EXPORT_OF_SP_SAVEGAMES
void CNestedScriptData::AddActiveScriptDataToTreeToBeSaved(parTree *pTreeToBeSaved, int index)
{
	m_ScriptSaveArrayOfStructs.AddActiveScriptDataToTreeToBeSaved(pTreeToBeSaved, index);
}
#endif // SAVEGAME_USES_XML || __ALLOW_EXPORT_OF_SP_SAVEGAMES

#if SAVEGAME_USES_XML
void CNestedScriptData::AddInactiveScriptDataToTreeToBeSaved(parTree *pTreeToBeSaved, const char* pNameOfScriptTreeNode)
{
	if (savegameVerifyf(pTreeToBeSaved, "CNestedScriptData::AddInactiveScriptDataToTreeToBeSaved - the main tree that will be added to does not exist"))
	{
		parTreeNode *pRootOfTreeToBeSaved = pTreeToBeSaved->GetRoot();

		if (savegameVerifyf(pRootOfTreeToBeSaved, "CNestedScriptData::AddInactiveScriptDataToTreeToBeSaved - the main tree that will be added to does not have a root node"))
		{
			parTreeNode *pScriptDataNode = pRootOfTreeToBeSaved->FindChildWithName(pNameOfScriptTreeNode);

			if (savegameVerifyf(pScriptDataNode, "CNestedScriptData::AddInactiveScriptDataToTreeToBeSaved - the main tree that will be added to does not have a node named %s", pNameOfScriptTreeNode))
			{
				//	loop through array of parTrees and clone each one then append the clone as a child of the pNameOfScriptTreeNode node
				for (u32 loop = 0; loop < m_ScriptSaveXmlTrees.GetTreeCount(); loop++)
				{	//	loop through any parTrees that have been loaded but not converted to an RTStructure during the current session
					if (savegameVerifyf(m_ScriptSaveXmlTrees.GetTreeNode(loop), "CNestedScriptData::AddInactiveScriptDataToTreeToBeSaved - expected all entries in tree array to be valid"))
					{
						//	As with the names in the rtStructs that are local to CreateSaveDataAndCalculateSize
						//	it would be better if the strings in the clone were duplicates of the original string
						parTreeNode *pClone = m_ScriptSaveXmlTrees.GetTreeNode(loop)->Clone();
						pClone->AppendAsChildOf(pScriptDataNode);
					}
				}
			}
		}
	}
}

void CNestedScriptData::ReadScriptTreesFromLoadedTree(parTree *pFullLoadedTree, const char* pNameOfScriptTreeNode)
{
	savegameAssertf(m_ScriptSaveXmlTrees.GetTreeCount() == 0, "CNestedScriptData::ReadScriptTreesFromLoadedTree - expected the array of parTrees to be empty at this stage");

	if (savegameVerifyf(pFullLoadedTree, "CNestedScriptData::ReadScriptTreesFromLoadedTree - the main tree to be loaded does not exist"))
	{
		parTreeNode *pRootOfTreeToBeLoaded = pFullLoadedTree->GetRoot();

		if (savegameVerifyf(pRootOfTreeToBeLoaded, "CNestedScriptData::ReadScriptTreesFromLoadedTree - the main tree to be loaded does not have a root node"))
		{
			parTreeNode *pScriptDataNode = pRootOfTreeToBeLoaded->FindChildWithName(pNameOfScriptTreeNode);

			if (savegameVerifyf(pScriptDataNode, "CNestedScriptData::ReadScriptTreesFromLoadedTree - the main tree to be loaded does not have a node named %s", pNameOfScriptTreeNode))
			{
				parTreeNode *pCurrent = pScriptDataNode->GetChild();
				parTreeNode *pNext = NULL;
				while (pCurrent)
				{
					pNext = pCurrent->GetSibling();
					pCurrent->RemoveSelf();				//	Remove the branch containing the data for a GtaThread remove from the tree...
					m_ScriptSaveXmlTrees.Append(pCurrent);	//	and add it to the atArray of parTreeNodes
					pCurrent = pNext;
				}
			}
		}
	}
}

bool CNestedScriptData::HasAllScriptDataBeenDeserializedFromXmlFile()
{
	return (m_ScriptSaveXmlTrees.GetTreeCount() == 0);
}

#endif	//	SAVEGAME_USES_XML

s32 CNestedScriptData::GetSizeOfSavedScriptData()
{
	return m_ScriptSaveArrayOfStructs.GetSizeOfSavedScriptData();
}

s32 CNestedScriptData::GetSizeOfSavedScriptDataInBytes()
{
	return m_ScriptSaveArrayOfStructs.GetSizeOfSavedScriptDataInBytes();
}

#if SAVEGAME_USES_PSO
psoBuilderInstance* CNestedScriptData::CreatePsoStructuresForSaving(psoBuilder& pso, atLiteralHashValue nameHashOfScript)
{
	return m_ScriptSaveArrayOfStructs.CreatePsoStructuresForSaving(pso, nameHashOfScript);
}


void CNestedScriptData::AddActiveScriptDataToPsoToBeSaved(psoBuilder& builder, psoBuilderInstance* rootInstance, const char* pNameOfScriptTreeNode)
{
#if __BANK
	if (CGenericGameStorage::ms_bSaveScriptVariables)
#endif	//	__BANK
	{
		// Define a container structure that holds pointers to each thread's script data
		psoBuilderStructSchema& scriptThreadContainerSchema = builder.CreateStructSchema(atLiteralHashValue(pNameOfScriptTreeNode));

		scriptThreadContainerSchema.AddMemberPointer(atLiteralHashValue(GetTopLevelName().c_str()), parMemberStructSubType::SUBTYPE_SIMPLE_POINTER);

		scriptThreadContainerSchema.FinishBuilding();

		// Now create an instance of that structure (the one containing pointers to all of the script data structures)
		psoBuilderInstance& scriptThreadContainer = builder.CreateStructInstances(scriptThreadContainerSchema);

		// Create each of the script data structure objects
		psoBuilderInstance* scriptThreadData = CreatePsoStructuresForSaving(builder, atLiteralHashValue(pNameOfScriptTreeNode));

		// Then fix up the pointer from the scriptThreadContainer to the scriptThreadData
		scriptThreadContainer.AddFixup(0, GetTopLevelName().c_str(), scriptThreadData);

		// Finally add a fixup so that the root object in the PSO file points to the script data container
		rootInstance->AddFixup(0, pNameOfScriptTreeNode, &scriptThreadContainer);
	}
}

void CNestedScriptData::ReadAndDeserializeAllScriptTreesFromLoadedPso(psoFile& file, const char* pNameOfScriptTreeNode, const char* pNameOfGlobalDataTree)
{
	psoStruct root = file.GetRootInstance();
	psoMember mem =root.GetMemberByName(atLiteralHashValue(CSaveGameData::GetNameOfMainScriptTreeNode()));
	if(!(atLiteralHashValue(pNameOfScriptTreeNode)==atLiteralHashValue(CSaveGameData::GetNameOfMainScriptTreeNode())))
	{
		//mem = mem.GetSubStructure().GetMemberByName(atLiteralHashValue(pNameOfScriptTreeNode));
		mem = root.GetMemberByName(atLiteralHashValue(CSaveGameData::GetNameOfDLCScriptTreeNode())).GetSubStructure().GetMemberByName(atLiteralHashValue(pNameOfScriptTreeNode));
	}
	//psoMember mem = root.GetMemberByName(atLiteralHashValue(pNameOfScriptTreeNode));
	if (savegameVerifyf(mem.IsValid(), "CNestedScriptData::ReadAndDeserializeAllScriptTreesFromLoadedPso - Couldn't find member %s in structure", pNameOfScriptTreeNode))
	{
		// The current format is that the root node has a substructure named pNameOfScriptTreeNode, which contains pointers to all
		// of the actual script data structures.
		psoStruct subStruct = mem.GetSubStructure();

		atLiteralHashValue nameHash(pNameOfGlobalDataTree);

		int SubStructCount = subStruct.GetSchema().GetNumMembers();
		savegameAssertf(SubStructCount == 1, "CNestedScriptData::ReadAndDeserializeAllScriptTreesFromLoadedPso - SubStructCount is %d. Expected it to always be 1 for now. Graeme", SubStructCount);
		for(int i = 0; i < SubStructCount; i++)
		{
			psoMember scriptStructMember = subStruct.GetMemberByIndex(i);
			atLiteralHashValue sHash = scriptStructMember.GetSchema().GetNameHash();
			if (sHash == nameHash)
			{
				psoStruct scriptStruct = subStruct.GetMemberByIndex(i).GetSubStructure();

				if (scriptStruct.IsValid() && !scriptStruct.IsNull())
				{
					m_ScriptSaveArrayOfStructs.LoadFromPsoStruct(scriptStruct);
				}
			}
		}
	}
}

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
bool CNestedScriptData::DeserializeForExport(psoMember& member)
{
	u32 numberOfLoadedStructs = 0;
	psoStruct subStruct = member.GetSubStructure();
	int SubStructCount = subStruct.GetSchema().GetNumMembers();
	for(int i = 0; i < SubStructCount; i++)
	{
		psoMember scriptStructMember = subStruct.GetMemberByIndex(i);
		psoStruct scriptStruct = scriptStructMember.GetSubStructure();
		if (scriptStruct.IsValid() && !scriptStruct.IsNull())
		{
			savegameDisplayf("CNestedScriptData::DeserializeForExport - about to call m_ScriptSaveArrayOfStructs.LoadFromPsoStruct()");
			m_ScriptSaveArrayOfStructs.LoadFromPsoStruct(scriptStruct);
			numberOfLoadedStructs++;
		}
		else
		{
			savegameWarningf("CNestedScriptData::DeserializeForExport - scriptStruct is not valid");
		}
	}
	return numberOfLoadedStructs!=0;
}
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
bool CNestedScriptData::DeserializeForImport(parTreeNode *pParentNode, const char* pNameOfScriptTreeNode, const char* pNameOfGlobalDataTree)
{
	bool bFound = false;

	parTreeNode *pScriptDataNode = pParentNode->FindChildWithName(pNameOfScriptTreeNode);
	if (savegameVerifyf(pScriptDataNode, "CNestedScriptData::DeserializeForImport - the main tree to be loaded does not have a node named %s", pNameOfScriptTreeNode))
	{
		parTreeNode *pCurrentTreeNode = pScriptDataNode->GetChild();
		while (pCurrentTreeNode && !bFound)
		{
			if (pCurrentTreeNode && (strcmp(pCurrentTreeNode->GetElement().GetName(), pNameOfGlobalDataTree) == 0))
			{
				m_ScriptSaveArrayOfStructs.CreateRTStructuresForLoading();
				parRTStructure *pRTStructureOfSaveData = m_ScriptSaveArrayOfStructs.GetTopLevelRTStructure();
				if (savegameVerifyf(pRTStructureOfSaveData, "CNestedScriptData::DeserializeForImport - RTStructure doesn't exist for %s", pNameOfGlobalDataTree))
				{
					PARSER.LoadFromStructure(pCurrentTreeNode, *pRTStructureOfSaveData, NULL, false);
				}

				m_ScriptSaveArrayOfStructs.DeleteRTStructures();
				bFound = true;
			}

			pCurrentTreeNode = pCurrentTreeNode->GetSibling();
		}

		if (!bFound)
		{
			savegameErrorf("CNestedScriptData::DeserializeForImport - failed to find a child node of %s with the the name %s", pNameOfScriptTreeNode, pNameOfGlobalDataTree);
			savegameAssertf(0, "CNestedScriptData::DeserializeForImport - failed to find a child node of %s with the the name %s", pNameOfScriptTreeNode, pNameOfGlobalDataTree);
		}
	}

	return bFound;
}
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES



#if __BANK
void CNestedScriptData::DisplayCurrentValuesOfSavedScriptVariables()
{
	m_ScriptSaveArrayOfStructs.DisplayCurrentValuesOfSavedScriptVariables();
}
#endif	//	__BANK


#endif // SAVEGAME_USES_PSO

bool CNestedScriptData::IsValidCharacterForElementName(char c)
{
	if (c >= 'a' && c <= 'z')
		return true;

	if (c >= 'A' && c <= 'Z')
		return true;

	if (c >= '0' && c <= '9')
		return true;

	if (c == '.')
		return true;

	if (c == ':')
		return true;

	if (c == '_')
		return true;

	return false;
}

bool CNestedScriptData::DoesElementNameAlreadyExistInStructsOnStack(const char *pElementName)
{
	int CurrentStackLevel = CScriptSaveData::GetStructStack().GetStackLevel();

	if (CurrentStackLevel < 0)
	{
		savegameAssertf(0, "CNestedScriptData::DoesElementNameAlreadyExistInStructsOnStack - didn't expect stack to be empty");
		return false;
	}

	for (int stack_level = 0; stack_level <= CurrentStackLevel; stack_level++)
	{	//	Need to use <= rather than <
		int ArrayIndex = CScriptSaveData::GetStructStack().GetArrayIndex(stack_level);
		if (m_ScriptSaveArrayOfStructs.DoesElementNameAlreadyExist(ArrayIndex, pElementName))
		{
			return true;
		}
	}

	return false;
}

bool CNestedScriptData::CheckElementName(const char *pElementName)
{
	int char_index = 0;
	while (pElementName[char_index] != '\0')
	{
		if (!IsValidCharacterForElementName(pElementName[char_index]))
		{
			savegameAssertf(0, "CNestedScriptData::CheckElementName - label %s contains an invalid character %c", pElementName, pElementName[char_index]);
			return false;
		}

		char_index++;
	}

	if (char_index >= MAX_LENGTH_OF_LABEL_FOR_SAVE_DATA)
	{
		savegameAssertf(0, "CNestedScriptData::CheckElementName - label %s is longer than 63 characters", pElementName);
		return false;
	}

	bool bNameAlreadyExists = DoesElementNameAlreadyExistInStructsOnStack(pElementName);
	if (bNameAlreadyExists)
	{
		savegameAssertf(0, "CNestedScriptData::CheckElementName - data with label %s has already been registered", pElementName);
		return false;
	}

	return true;
}

