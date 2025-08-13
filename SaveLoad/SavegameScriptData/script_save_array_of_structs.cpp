
#include "script_save_array_of_structs.h"

// Rage headers
#include "parser/treenode.h"

// Game headers
#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/SavegameScriptData/script_save_data.h"
#include "SaveLoad/SavegameScriptData/script_save_struct.h"
#include "SaveLoad/savegame_channel.h"


// **************************************** SaveGameDataBlock - Script - CScriptSaveArrayOfStructs ********************************

CScriptSaveArrayOfStructs::CScriptSaveArrayOfStructs()
{
	m_BaseAddressOfTopLevelStructure = NULL;

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES
	m_BaseAddressOfTopLevelStructureForImportExport = NULL;
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES
}

void CScriptSaveArrayOfStructs::Shutdown()
{
	for (int loop = 0; loop < m_ScriptStructs.GetCount(); loop++)
	{
		m_ScriptStructs[loop].Shutdown();
	}
	m_ScriptStructs.Reset();

#if SAVEGAME_USES_XML || __ALLOW_IMPORT_OF_SP_SAVEGAMES
	//	DeleteRTStructures() will also get called through CSaveGameBuffer::FreeAllDataToBeSaved() when shutting down the session
	//	but I'm just calling it here to be safe
	DeleteRTStructures();
#endif // SAVEGAME_USES_XML || __ALLOW_IMPORT_OF_SP_SAVEGAMES

	m_NameOfTopLevelStructure.Clear();
	m_BaseAddressOfTopLevelStructure = 0;

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES
	m_BaseAddressOfTopLevelStructureForImportExport = NULL;
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES
}

int CScriptSaveArrayOfStructs::AddNewScriptStruct(int PCOfRegisterCommand, bool bIsArray, const char *pNameOfInstance)
{
	int SizeOfArray = m_ScriptStructs.GetCount();

	for (int loop = 0; loop < SizeOfArray; loop++)
	{
		if (m_ScriptStructs[loop].GetPC() == PCOfRegisterCommand)
		{
			return loop;
		}
	}

	CScriptSaveStruct NewEntry;
	NewEntry.Set(PCOfRegisterCommand, bIsArray, pNameOfInstance);
	m_ScriptStructs.PushAndGrow(NewEntry);

	//	Is it safe to assume that after PushAndGrow, the index of the new entry will be the old SizeOfArray?
	return SizeOfArray;
}

void CScriptSaveArrayOfStructs::AddNewDataItemToStruct(int ArrayIndexOfStruct, u32 Offset, int DataType, const char *pLabel)
{
	if (savegameVerifyf( (ArrayIndexOfStruct >= 0) && (ArrayIndexOfStruct < m_ScriptStructs.GetCount()), "CScriptSaveArrayOfStructs::AddNewDataItemToStruct - array index is out of range"))
	{
		m_ScriptStructs[ArrayIndexOfStruct].AddMember(Offset, DataType, pLabel);
	}
}

bool CScriptSaveArrayOfStructs::GetHasBeenClosed(int ArrayIndex)
{
	if (savegameVerifyf( (ArrayIndex >= 0) && (ArrayIndex < m_ScriptStructs.GetCount()), "CScriptSaveArrayOfStructs::GetHasBeenClosed - array index is out of range"))
	{
		return m_ScriptStructs[ArrayIndex].GetHasBeenClosed();
	}

	return true;
}

void CScriptSaveArrayOfStructs::SetHasBeenClosed(int ArrayIndex, bool bHasBeenClosed)
{
	if (savegameVerifyf( (ArrayIndex >= 0) && (ArrayIndex < m_ScriptStructs.GetCount()), "CScriptSaveArrayOfStructs::SetHasBeenClosed - array index is out of range"))
	{
		m_ScriptStructs[ArrayIndex].SetHasBeenClosed(bHasBeenClosed);
	}
}

bool CScriptSaveArrayOfStructs::DoesElementNameAlreadyExist(int ArrayIndex, const char *pElementNameToCheck)
{
	if (savegameVerifyf( (ArrayIndex >= 0) && (ArrayIndex < m_ScriptStructs.GetCount()), "CScriptSaveArrayOfStructs::DoesElementNameAlreadyExist - array index is out of range"))
	{
		return m_ScriptStructs[ArrayIndex].DoesElementNameAlreadyExist(pElementNameToCheck);
	}

	return false;
}

void CScriptSaveArrayOfStructs::ClearRegisteredElementNames(int ArrayIndex)
{
	if (savegameVerifyf( (ArrayIndex >= 0) && (ArrayIndex < m_ScriptStructs.GetCount()), "CScriptSaveArrayOfStructs::ClearRegisteredElementNames - array index is out of range"))
	{
		m_ScriptStructs[ArrayIndex].ClearAllRegisteredElementNames();
	}
}

#if SAVEGAME_USES_XML || __ALLOW_IMPORT_OF_SP_SAVEGAMES
//	This function will recursively call itself to handle nested structures
//	Only used for loading
parRTStructure *CScriptSaveArrayOfStructs::CreateRTStructureForLoading(int ArrayIndex, atString &NameOfStructureInstance, u8* BaseAddressOfStructure)
{
	if (savegameVerifyf( (ArrayIndex >= 0) && (ArrayIndex < m_ScriptStructs.GetCount()), "CScriptSaveArrayOfStructs::CreateRTStructureForLoading - array index is out of range"))
	{
		CScriptSaveStruct &CurrentStruct = m_ScriptStructs[ArrayIndex];
		int NumberOfMembers = CurrentStruct.GetNumberOfMembers();

		savegameDisplayf("CScriptSaveArrayOfStructs::CreateRTStructureForLoading - creating an RTStructure for %s Members = %d\n", NameOfStructureInstance.c_str(), NumberOfMembers);

		parRTStructure *pRTStructure = rage_new parRTStructure(NameOfStructureInstance.c_str(), NumberOfMembers);
		m_ArrayOfRTStructures.PushAndGrow(pRTStructure);

		for (int member_loop = 0; member_loop < NumberOfMembers; member_loop++)
		{
			switch (CurrentStruct.GetDataTypeOfMember(member_loop))
			{
			case Script_Save_Data_Type_Int :
				{
					u8* BaseAddressOfInt = BaseAddressOfStructure + CurrentStruct.GetOffsetWithinStructOfMember(member_loop);
					int *pPointerToInt = reinterpret_cast<int*>(BaseAddressOfInt);

					pRTStructure->AddValue(CurrentStruct.GetNameOfMember(member_loop), pPointerToInt);
				}
				break;
			case Script_Save_Data_Type_Int64 :
				{
					u8* BaseAddressOfInt64 = BaseAddressOfStructure + CurrentStruct.GetOffsetWithinStructOfMember(member_loop);
					s64 *pPointerToInt64 = reinterpret_cast<s64*>(BaseAddressOfInt64);

					pRTStructure->AddValue(CurrentStruct.GetNameOfMember(member_loop), pPointerToInt64);
				}
				break;
			case Script_Save_Data_Type_Float :
				{
					u8* BaseAddressOfFloat = BaseAddressOfStructure + CurrentStruct.GetOffsetWithinStructOfMember(member_loop);
					float *pPointerToFloat = reinterpret_cast<float*>(BaseAddressOfFloat);

					pRTStructure->AddValue(CurrentStruct.GetNameOfMember(member_loop), pPointerToFloat);
				}
				break;
				//		case Script_Save_Data_Type_Bool
			case Script_Save_Data_Type_TextLabel3 :
			case Script_Save_Data_Type_TextLabel7 :
			case Script_Save_Data_Type_TextLabel15 :
			case Script_Save_Data_Type_TextLabel23 :
			case Script_Save_Data_Type_TextLabel31 :
			case Script_Save_Data_Type_TextLabel63 :
				{
					u8* BaseAddressOfTextLabel = BaseAddressOfStructure + CurrentStruct.GetOffsetWithinStructOfMember(member_loop);
					char *pPointerToTextLabel = reinterpret_cast<char*>(BaseAddressOfTextLabel);

					pRTStructure->AddStringValue(CurrentStruct.GetNameOfMember(member_loop), pPointerToTextLabel, GetNumberOfCharactersInATextLabelOfThisType(CurrentStruct.GetDataTypeOfMember(member_loop)));
				}
				break;
			default :
				{
					int ArrayIndexOfMember = CurrentStruct.GetDataTypeOfMember(member_loop);
					if (savegameVerifyf(ArrayIndexOfMember > 0, "CScriptSaveArrayOfStructs::CreateRTStructureForLoading - expected all non-simple types to be struct indices >= 0"))
					{	//	Array Index 0 shouldn't occur here because it should always be the struct containing all the others
						//	and m_ScriptStructs[0].CreateRTStructureForLoading will be called to start creating the RTStructures
						if (savegameVerifyf(ArrayIndexOfMember < m_ScriptStructs.GetCount(), "CScriptSaveArrayOfStructs::CreateRTStructureForLoading - index into array of structures is too large"))
						{
							u8* BaseAddressOfChild = BaseAddressOfStructure + CurrentStruct.GetOffsetWithinStructOfMember(member_loop);
							parRTStructure *pChildRTStructure = CreateRTStructureForLoading(ArrayIndexOfMember, CurrentStruct.GetNameOfMember(member_loop), BaseAddressOfChild);

							//	Add child RTStructure to parent
							pRTStructure->AddStructure(CurrentStruct.GetNameOfMember(member_loop), *pChildRTStructure);
						}
					}
				}
				break;
			}
		}

		return pRTStructure;
	}

	return NULL;
}
#endif // SAVEGAME_USES_XML || __ALLOW_IMPORT_OF_SP_SAVEGAMES


#if SAVEGAME_USES_XML || __ALLOW_EXPORT_OF_SP_SAVEGAMES
//	This function will recursively call itself to handle nested structures
//	Only used for saving
parTreeNode *CScriptSaveArrayOfStructs::CreateTreeNodesFromScriptData(int ArrayIndex, atString &NameOfStructureInstance, u8* BaseAddressOfStructure)
{
	if (savegameVerifyf( (ArrayIndex >= 0) && (ArrayIndex < m_ScriptStructs.GetCount()), "CScriptSaveArrayOfStructs::CreateTreeNodesFromScriptData - array index is out of range"))
	{
		CScriptSaveStruct &CurrentStruct = m_ScriptStructs[ArrayIndex];
		int NumberOfMembers = CurrentStruct.GetNumberOfMembers();

		parTreeNode *pTreeNode = rage_new parTreeNode(NameOfStructureInstance.c_str(), true);

		parTreeNode *pChildNode = NULL;
		for (int member_loop = 0; member_loop < NumberOfMembers; member_loop++)
		{
			pChildNode = NULL;
			switch (CurrentStruct.GetDataTypeOfMember(member_loop))
			{
			case Script_Save_Data_Type_Int :
				{
					u8* BaseAddressOfInt = BaseAddressOfStructure + CurrentStruct.GetOffsetWithinStructOfMember(member_loop);
					int *pPointerToInt = reinterpret_cast<int*>(BaseAddressOfInt);

					pChildNode = parTreeNode::CreateStdLeaf(CurrentStruct.GetNameOfMember(member_loop), (s64)*pPointerToInt, true);
					pChildNode->AppendAsChildOf(pTreeNode);
				}
				break;
			case Script_Save_Data_Type_Int64 :
				{
					u8* BaseAddressOfInt64 = BaseAddressOfStructure + CurrentStruct.GetOffsetWithinStructOfMember(member_loop);
					s64 *pPointerToInt64 = reinterpret_cast<s64*>(BaseAddressOfInt64);

					pChildNode = parTreeNode::CreateStdLeaf(CurrentStruct.GetNameOfMember(member_loop), *pPointerToInt64, true);
					pChildNode->AppendAsChildOf(pTreeNode);
				}
				break;
			case Script_Save_Data_Type_Float :
				{
					u8* BaseAddressOfFloat = BaseAddressOfStructure + CurrentStruct.GetOffsetWithinStructOfMember(member_loop);
					float *pPointerToFloat = reinterpret_cast<float*>(BaseAddressOfFloat);

					pChildNode = parTreeNode::CreateStdLeaf(CurrentStruct.GetNameOfMember(member_loop), *pPointerToFloat, true);
					pChildNode->AppendAsChildOf(pTreeNode);
				}
				break;
				//		case Script_Save_Data_Type_Bool
			case Script_Save_Data_Type_TextLabel3 :
			case Script_Save_Data_Type_TextLabel7 :
			case Script_Save_Data_Type_TextLabel15 :
			case Script_Save_Data_Type_TextLabel23 :
			case Script_Save_Data_Type_TextLabel31 :
			case Script_Save_Data_Type_TextLabel63 :
				{
					u8* BaseAddressOfTextLabel = BaseAddressOfStructure + CurrentStruct.GetOffsetWithinStructOfMember(member_loop);
					const char *pPointerToTextLabel = reinterpret_cast<const char*>(BaseAddressOfTextLabel);

					pChildNode = parTreeNode::CreateStdLeaf(CurrentStruct.GetNameOfMember(member_loop), pPointerToTextLabel, true);
					pChildNode->AppendAsChildOf(pTreeNode);
				}
				break;
			default :
				{
					int ArrayIndexOfMember = CurrentStruct.GetDataTypeOfMember(member_loop);
					if (savegameVerifyf(ArrayIndexOfMember > 0, "CScriptSaveArrayOfStructs::CreateTreeNodesFromScriptData - expected all non-simple types to be struct indices >= 0"))
					{	//	Array Index 0 shouldn't occur here because it should always be the struct containing all the others
						//	and CreateTreeNodesFromScriptData will be called for m_ScriptStructs[0] to start creating the tree nodes
						if (savegameVerifyf(ArrayIndexOfMember < m_ScriptStructs.GetCount(), "CScriptSaveArrayOfStructs::CreateTreeNodesFromScriptData - index into array of structures is too large"))
						{
							u8* BaseAddressOfChild = BaseAddressOfStructure + CurrentStruct.GetOffsetWithinStructOfMember(member_loop);
							pChildNode = CreateTreeNodesFromScriptData(ArrayIndexOfMember, CurrentStruct.GetNameOfMember(member_loop), BaseAddressOfChild);

							//	Add child tree node to parent
							pChildNode->AppendAsChildOf(pTreeNode);
						}
					}
				}
				break;
			}
		}

		return pTreeNode;
	}

	return NULL;
}
#endif // SAVEGAME_USES_XML || __ALLOW_EXPORT_OF_SP_SAVEGAMES


s32 CScriptSaveArrayOfStructs::GetSizeOfStructAndItsChildren(int ArrayIndex, atString& /*OUTPUT_ONLY(NameOfStructureInstance)*/)
{
	if (savegameVerifyf( (ArrayIndex >= 0) && (ArrayIndex < m_ScriptStructs.GetCount()), "CScriptSaveArrayOfStructs::GetSizeOfStructAndItsChildren - array index is out of range"))
	{
		CScriptSaveStruct &CurrentStruct = m_ScriptStructs[ArrayIndex];
		int NumberOfMembers = CurrentStruct.GetNumberOfMembers();

		s32 TotalSize = 0;

		if (CurrentStruct.GetIsArray())
		{
			TotalSize++;
		}

		for (int member_loop = 0; member_loop < NumberOfMembers; member_loop++)
		{
			switch (CurrentStruct.GetDataTypeOfMember(member_loop))
			{
			case Script_Save_Data_Type_Int :
			case Script_Save_Data_Type_Float :
				//			case Script_Save_Data_Type_Bool
			case Script_Save_Data_Type_TextLabel3 :
				{
					TotalSize += 1;
				}
				break;
			case Script_Save_Data_Type_Int64 :
				{
					TotalSize += 1;	//	I think this should be 1 (rather than 2) since the variable will just be an INT on the script side. All scrValues have a size of 8 bytes on Next Gen.
					//	The scripts usually only use 4 bytes of an INT. The GAMER_HANDLE struct containing 13 INTs is a special case. I've added support for Int64 so that those gamer handles can be stored in MP savegames.
				}
				break;
			case Script_Save_Data_Type_TextLabel7 :
				{
					TotalSize += 2;
				}
				break;
			case Script_Save_Data_Type_TextLabel15 :
				{
					TotalSize += 4;
				}
				break;
			case Script_Save_Data_Type_TextLabel23 :
				{
					TotalSize += 6;
				}
				break;
			case Script_Save_Data_Type_TextLabel31 :
				{
					TotalSize += 8;
				}
				break;
			case Script_Save_Data_Type_TextLabel63 :
				{
					TotalSize += 16;
				}
				break;
			default :
				{
					int ArrayIndexOfMember = CurrentStruct.GetDataTypeOfMember(member_loop);
					if (savegameVerifyf(ArrayIndexOfMember > 0, "CScriptSaveArrayOfStructs::GetSizeOfStructAndItsChildren - expected all non-simple types to be struct indices >= 0"))
					{	//	Array Index 0 shouldn't occur here because it should always be the struct containing all the others
						//	and GetSizeOfStructAndItsChildren will be called for m_ScriptStructs[0] to start creating the tree nodes
						if (savegameVerifyf(ArrayIndexOfMember < m_ScriptStructs.GetCount(), "CScriptSaveArrayOfStructs::GetSizeOfStructAndItsChildren - index into array of structures is too large"))
						{
							TotalSize += GetSizeOfStructAndItsChildren(ArrayIndexOfMember, CurrentStruct.GetNameOfMember(member_loop));
						}
					}
				}
				break;
			}
		}

		//		savegameDisplayf("CScriptSaveArrayOfStructs::GetSizeOfStructAndItsChildren - %s has size %d\n", NameOfStructureInstance.c_str(), TotalSize);

		return TotalSize;
	}

	return 0;
}

int CScriptSaveArrayOfStructs::ComputeSizeInBytesWithGaps(u32 ArrayIndex)
{
	CScriptSaveStruct& str = m_ScriptStructs[ArrayIndex];

	// No members => size 0
	if (str.GetNumberOfMembers() == 0)
	{
		return 0;
	}

	// The size of the structure is the offset of its last member, plus the size of the last member
	int lastMemberOffset = str.GetOffsetWithinStructOfMember(0);
	int lastMemberIndex = 0;

	for(int loop = 0; loop < str.GetNumberOfMembers(); loop++)
	{
		int currOffset = str.GetOffsetWithinStructOfMember(loop);
		if (currOffset > lastMemberOffset)
		{
			lastMemberOffset = currOffset;
			lastMemberIndex = loop;
		}
	}

	int sizeOfLastMember = 0;

	int dataType = str.GetDataTypeOfMember(lastMemberIndex);

	switch(dataType)
	{
	case Script_Save_Data_Type_Float:
	case Script_Save_Data_Type_Int:
		sizeOfLastMember = 8;				//	On Next Gen, scrValue is now 8 bytes
		break;
	case Script_Save_Data_Type_Int64:
		sizeOfLastMember = 8;
		break;
	case Script_Save_Data_Type_TextLabel3:
	case Script_Save_Data_Type_TextLabel7:
	case Script_Save_Data_Type_TextLabel15:
	case Script_Save_Data_Type_TextLabel23:
	case Script_Save_Data_Type_TextLabel31:
	case Script_Save_Data_Type_TextLabel63:
		sizeOfLastMember = GetNumberOfCharactersInATextLabelOfThisType(dataType);
		savegameAssertf( (sizeOfLastMember & 7) == 0, "CScriptSaveArrayOfStructs::ComputeSizeInBytesWithGaps - struct ends with a text label type of size %d. Sizes that aren't a multiple of 8 may cause alignment errors if the struct contains any members that are registered using REGISTER_INT64_TO_SAVE", sizeOfLastMember);
		break;
	default:
		int childIndex = dataType;
		sizeOfLastMember = ComputeSizeInBytesWithGaps(childIndex);
		break;
	}

	savegameDebugf3("CScriptSaveArrayOfStructs::ComputeSizeInBytesWithGaps - size of struct %u is %d", ArrayIndex, (lastMemberOffset + sizeOfLastMember));
	return lastMemberOffset + sizeOfLastMember;
}


#if SAVEGAME_USES_XML ||__ALLOW_IMPORT_OF_SP_SAVEGAMES
void CScriptSaveArrayOfStructs::CreateRTStructuresForLoading()
{
	if (m_ScriptStructs.GetCount() > 0)
	{	//	Only do this if there is some save data
		if (m_ArrayOfRTStructures.GetCount() > 0)
		{
			savegameAssertf(0, "CScriptSaveArrayOfStructs::CreateRTStructuresForLoading - expected array of RTStructures to be empty at this stage");
			DeleteRTStructures();
		}
		CreateRTStructureForLoading(0, m_NameOfTopLevelStructure, GetBaseAddressOfTopLevelStructure());
	}
}
#endif // SAVEGAME_USES_XML || __ALLOW_IMPORT_OF_SP_SAVEGAMES



#if SAVEGAME_USES_PSO

psoBuilderStructSchema* CScriptSaveArrayOfStructs::CreatePsoStructureSchemaForSaving(psoBuilder& builder, u32 ArrayIndex, u32 anonOffset)
{
	atString dummyName("");
	int structSize = ComputeSizeInBytesWithGaps(ArrayIndex); // returns the number of words, we need bytes.

	savegameDebugf3("CScriptSaveArrayOfStructs::CreatePsoStructureSchemaForSaving - calling CreateStructSchema with structSize = %d (0x%x)", structSize, structSize);
	psoBuilderStructSchema& schema = builder.CreateStructSchema(atLiteralHashValue(ArrayIndex + anonOffset), structSize);

	CScriptSaveStruct& str = m_ScriptStructs[ArrayIndex];

	for(int loop = 0; loop < str.GetNumberOfMembers(); loop++)
	{
		atLiteralHashValue nameHash(str.GetNameOfMember(loop).c_str());

		int dataType = str.GetDataTypeOfMember(loop);
		u32 offset = str.GetOffsetWithinStructOfMember(loop);

		savegameDebugf3("CScriptSaveArrayOfStructs::CreatePsoStructureSchemaForSaving - member %d has name %s and offset %u", loop, str.GetNameOfMember(loop).c_str(), offset);

		switch(dataType)
		{
		case Script_Save_Data_Type_Int:
			schema.AddMemberSimple(nameHash, parMemberType::TYPE_INT, 0, offset);
			break;
		case Script_Save_Data_Type_Int64:
			schema.AddMemberSimple(nameHash, parMemberType::TYPE_INT64, 0, offset);
			break;
		case Script_Save_Data_Type_Float:
			schema.AddMemberSimple(nameHash, parMemberType::TYPE_FLOAT, 0, offset);
			break;
		case Script_Save_Data_Type_TextLabel3:
		case Script_Save_Data_Type_TextLabel7:
		case Script_Save_Data_Type_TextLabel15:
		case Script_Save_Data_Type_TextLabel23:
		case Script_Save_Data_Type_TextLabel31:
		case Script_Save_Data_Type_TextLabel63:
			schema.AddMemberString(nameHash, parMemberStringSubType::SUBTYPE_MEMBER, (u16)GetNumberOfCharactersInATextLabelOfThisType(dataType), offset);
			break;
		default:
			{
				int childIndex = dataType;
				psoBuilderStructSchema* childSchema = builder.FindStructSchema(atLiteralHashValue(anonOffset + childIndex));
				if (!childSchema)
				{
					childSchema = CreatePsoStructureSchemaForSaving(builder, childIndex, anonOffset);
				}
				savegameAssertf(childSchema, "Couldn't find or create a PSO schema for child index %d", childIndex);
				schema.AddMemberStruct(nameHash, *childSchema, offset);
			}
			break;
		}
	}

	schema.FinishBuilding();

	return &schema;
}

//	void CScriptSaveArrayOfStructs::AddDataToPsoInstance(psoBuilder& /*builder*/, psoBuilderInstance& /*inst*/, u32 /*ArrayIndex*/, u32 /*BaseAddressOfStructure*/)
//	{
//	}

psoBuilderInstance* CScriptSaveArrayOfStructs::CreatePsoStructuresForSaving(psoBuilder& builder, atLiteralHashValue nameHashOfScript)
{
	u32 anonIdOffset = psoConstants::FIRST_ANONYMOUS_ID + nameHashOfScript.GetHash(); // Some arbitrary offset, in case we need to add other anon IDs somewhere else.

	// Create a bunch of anonymous structure schemas, one for each member of the array.
	for(int loop = 0; loop < m_ScriptStructs.GetCount(); loop++)
	{
		// We may have already added the schema while recursing
		// TODO: In fact haven't we _always_ already added the schema? Should we just add element 0 and let recursion handle the rest?
		if (!builder.FindStructSchema(atLiteralHashValue(anonIdOffset + loop)))
		{
			CreatePsoStructureSchemaForSaving(builder, loop, anonIdOffset);
		}
	}

	psoBuilderStructSchema* rootSchema = builder.FindStructSchema(atLiteralHashValue(anonIdOffset));
	savegameAssertf(rootSchema, "Couldn't find root schema!");

	psoBuilderInstance& builderInst = builder.CreateStructInstances(*rootSchema);
	sysMemCpy(builderInst.GetRawData(), GetBaseAddressOfTopLevelStructure(), rootSchema->GetSchemaData()->m_Size);

	return &builderInst;
}

#if __BANK
u32 g_SavegamePsoMemCounter = 0;
u32 sDepthOfSavegameStructure = 0;
#endif

void CScriptSaveArrayOfStructs::LoadFromPsoStruct(psoStruct& PsoStructure)
{
#if __BANK
	g_SavegamePsoMemCounter = 0;
	if (CGenericGameStorage::ms_bPrintValuesAsPsoFileLoads)
	{
		sDepthOfSavegameStructure = 0;
		savegameDisplayf("***Savegame - start of data read from PSO***");
	}
#endif	//	__BANK

	LoadFromPsoStruct(0, GetBaseAddressOfTopLevelStructure(), PsoStructure);

#if __BANK
	if (CGenericGameStorage::ms_bPrintValuesAsPsoFileLoads)
	{
		savegameDisplayf("***Savegame - end of data read from PSO***");
	}
	savegameDebugf1("Fixed up %d PSO script variables", g_SavegamePsoMemCounter);
#endif	//	__BANK
}

void CScriptSaveArrayOfStructs::LoadFromPsoStruct(int ArrayIndex, u8* BaseAddressOfStructure, psoStruct& PsoStructure)
{
	CScriptSaveStruct& scriptStruct = m_ScriptStructs[ArrayIndex];

	for(int loop = 0; loop < scriptStruct.GetNumberOfMembers(); loop++)
	{
		const char* name = scriptStruct.GetNameOfMember(loop).c_str();
		atLiteralHashValue nameHash(name);

		int dataType = scriptStruct.GetDataTypeOfMember(loop);
		u32 offset = scriptStruct.GetOffsetWithinStructOfMember(loop);

		psoMember psoMem = PsoStructure.GetMemberByName(nameHash);
		if (psoMem.IsValid())
		{
			switch(dataType)
			{
			case Script_Save_Data_Type_Int:
				if (savegameVerifyf(psoMem.GetType() == psoType::TYPE_S32, "Member named %s is not an int in the PSO file", name))
				{
					int *pIntReadFromSaveFile = reinterpret_cast<int*>(BaseAddressOfStructure + offset);
					*pIntReadFromSaveFile = psoMem.GetDataAs<int>();
#if __BANK
					if (CGenericGameStorage::ms_bPrintValuesAsPsoFileLoads)
					{
						savegameDisplayf("%s = %d", name, *pIntReadFromSaveFile);
					}
#endif	//	__BANK
				}
				break;
			case Script_Save_Data_Type_Int64:
				if (savegameVerifyf(psoMem.GetType() == psoType::TYPE_S64, "Member named %s is not of TYPE_S64 in the PSO file", name))
				{
					s64 *pInt64ReadFromSaveFile = reinterpret_cast<s64*>(BaseAddressOfStructure + offset);
					*pInt64ReadFromSaveFile = psoMem.GetDataAs<s64>();
#if __BANK
					if (CGenericGameStorage::ms_bPrintValuesAsPsoFileLoads)
					{
						savegameDisplayf("%s = %ld", name, *pInt64ReadFromSaveFile);
					}
#endif	//	__BANK
				}
				break;
			case Script_Save_Data_Type_Float:
				if (savegameVerifyf(psoMem.GetType() == psoType::TYPE_FLOAT, "Member named %s is not an float in the PSO file", name))
				{
					float *pFloatReadFromSaveFile = reinterpret_cast<float*>(BaseAddressOfStructure + offset);
					*pFloatReadFromSaveFile = psoMem.GetDataAs<float>();
#if __BANK
					if (CGenericGameStorage::ms_bPrintValuesAsPsoFileLoads)
					{
						savegameDisplayf("%s = %f", name, *pFloatReadFromSaveFile);
					}
#endif	//	__BANK
				}
				break;
			case Script_Save_Data_Type_TextLabel3:
			case Script_Save_Data_Type_TextLabel7:
			case Script_Save_Data_Type_TextLabel15:
			case Script_Save_Data_Type_TextLabel23:
			case Script_Save_Data_Type_TextLabel31:
			case Script_Save_Data_Type_TextLabel63:
				if (savegameVerifyf(psoMem.GetType() == psoType::TYPE_STRING_MEMBER, 
					"Member named %s is not a member string in the PSO file", name))
				{
					char* destString = reinterpret_cast<char*>(BaseAddressOfStructure + offset);
					char* sourceString = &psoMem.GetDataAs<char>();
					safecpy(destString, sourceString, GetNumberOfCharactersInATextLabelOfThisType(dataType));
#if __BANK
					if (CGenericGameStorage::ms_bPrintValuesAsPsoFileLoads)
					{
						savegameDisplayf("%s = %s", name, destString);
					}
#endif	//	__BANK
				}
				break;
			default:
				// A nested structure
				if (savegameVerifyf(psoMem.GetType() == psoType::TYPE_STRUCT,
					"Member named %s is not a structure in the PSO file", name))
				{
					psoStruct subStruct = psoMem.GetSubStructure();
					int childIndex = dataType;
#if __BANK
					if (CGenericGameStorage::ms_bPrintValuesAsPsoFileLoads)
					{
						savegameDisplayf("Start of struct %s", name);
						sDepthOfSavegameStructure++;
					}
#endif	//	__BANK
					LoadFromPsoStruct(childIndex, BaseAddressOfStructure + offset, subStruct);
#if __BANK
					if (CGenericGameStorage::ms_bPrintValuesAsPsoFileLoads)
					{
						sDepthOfSavegameStructure--;
						savegameDisplayf("End of struct %s", name);
					}
#endif	//	__BANK
				}
				break;
			}
			BANK_ONLY(g_SavegamePsoMemCounter++);
		}
		else
		{
			savegameWarningf("Couldn't find member named %s in PSO file", name);
		}
	}
}


#if __BANK
void CScriptSaveArrayOfStructs::DisplayCurrentValuesOfSavedScriptVariables()
{
	g_SavegamePsoMemCounter = 0;
	sDepthOfSavegameStructure = 0;
	savegameDebugf1("***DisplayCurrentValuesOfSavedScriptVariables - start of saved script variables ***");

	DisplayCurrentValuesOfSavedScriptVariables(0, GetBaseAddressOfTopLevelStructure());

	savegameDebugf1("***DisplayCurrentValuesOfSavedScriptVariables - end of saved script variables ***");

	savegameDebugf1("***DisplayCurrentValuesOfSavedScriptVariables - Fixed up %u PSO script variables", g_SavegamePsoMemCounter);
}

void CScriptSaveArrayOfStructs::DisplayCurrentValuesOfSavedScriptVariables(int ArrayIndex, u8* BaseAddressOfStructure)
{
	CScriptSaveStruct& scriptStruct = m_ScriptStructs[ArrayIndex];

	for(int loop = 0; loop < scriptStruct.GetNumberOfMembers(); loop++)
	{
		const char* name = scriptStruct.GetNameOfMember(loop).c_str();

		int dataType = scriptStruct.GetDataTypeOfMember(loop);
		u32 offset = scriptStruct.GetOffsetWithinStructOfMember(loop);

		{
			switch(dataType)
			{
			case Script_Save_Data_Type_Int:
				{
					int *pIntReadFromSaveFile = reinterpret_cast<int*>(BaseAddressOfStructure + offset);
					savegameDebugf2("%s = %d", name, *pIntReadFromSaveFile);
				}
				break;
			case Script_Save_Data_Type_Int64:
				{
					s64 *pInt64ReadFromSaveFile = reinterpret_cast<s64*>(BaseAddressOfStructure + offset);
					savegameDebugf2("%s = %ld", name, *pInt64ReadFromSaveFile);
				}
				break;
			case Script_Save_Data_Type_Float:
				{
					float *pFloatReadFromSaveFile = reinterpret_cast<float*>(BaseAddressOfStructure + offset);
					savegameDebugf2("%s = %f", name, *pFloatReadFromSaveFile);
				}
				break;
			case Script_Save_Data_Type_TextLabel3:
			case Script_Save_Data_Type_TextLabel7:
			case Script_Save_Data_Type_TextLabel15:
			case Script_Save_Data_Type_TextLabel23:
			case Script_Save_Data_Type_TextLabel31:
			case Script_Save_Data_Type_TextLabel63:
				{
					char* destString = reinterpret_cast<char*>(BaseAddressOfStructure + offset);
					savegameDebugf2("%s = %s", name, destString);
				}
				break;
			default:
				// A nested structure
				{
					savegameDebugf2("Start of struct %s", name);
					sDepthOfSavegameStructure++;

					int childIndex = dataType;
					DisplayCurrentValuesOfSavedScriptVariables(childIndex, BaseAddressOfStructure + offset);

					sDepthOfSavegameStructure--;
					savegameDebugf2("End of struct %s", name);
				}
				break;
			}
			g_SavegamePsoMemCounter++;
		}
	}
}
#endif	//	__BANK

#endif // SAVEGAME_USES_PSO


#if SAVEGAME_USES_XML || __ALLOW_IMPORT_OF_SP_SAVEGAMES
void CScriptSaveArrayOfStructs::DeleteRTStructures()
{
	//	Because the elements of m_ArrayOfRTStructures are parRTStructure* rather than parRTStructure, 
	//	Reset is not enough to call ~parRTStructure so I need to call delete 
	//	for each element to free all the memory for parRTStructures
	int ArraySize = m_ArrayOfRTStructures.GetCount();
	for (int loop = 0; loop < ArraySize; loop++)
	{
		delete m_ArrayOfRTStructures[loop];
	}
	m_ArrayOfRTStructures.Reset();
}

parRTStructure *CScriptSaveArrayOfStructs::GetTopLevelRTStructure()
{
	if (m_ArrayOfRTStructures.GetCount() > 0)
	{
		return m_ArrayOfRTStructures[0];
	}

	return NULL;
}
#endif // SAVEGAME_USES_XML || __ALLOW_IMPORT_OF_SP_SAVEGAMES


void CScriptSaveArrayOfStructs::SetDataForTopLevelStruct(u8* BaseAddress, const char *pName)
{
	m_NameOfTopLevelStructure = pName;
	m_BaseAddressOfTopLevelStructure = BaseAddress;
}

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES
void CScriptSaveArrayOfStructs::SetBufferForImportExport(u8 *pBaseAddress)
{
	m_BaseAddressOfTopLevelStructureForImportExport = pBaseAddress;
}
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES


#if SAVEGAME_USES_XML || __ALLOW_EXPORT_OF_SP_SAVEGAMES
void CScriptSaveArrayOfStructs::AddActiveScriptDataToTreeToBeSaved(parTree *pTreeToBeSaved, int index)
{
	if (m_ScriptStructs.GetCount() > 0)
	{	//	Only do this if there is some save data
		if (savegameVerifyf(pTreeToBeSaved, "CScriptSaveArrayOfStructs::AddActiveScriptDataToTreeToBeSaved - the main tree that will be added to does not exist"))
		{
			parTreeNode *pRootOfTreeToBeSaved = index==-1? pTreeToBeSaved->GetRoot() : pTreeToBeSaved->GetRoot()->FindChildWithName(CSaveGameData::GetNameOfDLCScriptTreeNode());

			if (savegameVerifyf(pRootOfTreeToBeSaved, "CScriptSaveArrayOfStructs::AddActiveScriptDataToTreeToBeSaved - the main tree that will be added to does not have a root node"))
			{
				parTreeNode *pScriptDataNode = pRootOfTreeToBeSaved->FindChildWithName(CScriptSaveData::GetNameOfScriptStruct(index));

				if (savegameVerifyf(pScriptDataNode, "CScriptSaveArrayOfStructs::AddActiveScriptDataToTreeToBeSaved - the main tree that will be added to does not have a node named %s", CScriptSaveData::GetNameOfScriptStruct(index)))
				{
					parTreeNode *pChild = CreateTreeNodesFromScriptData(0, m_NameOfTopLevelStructure, GetBaseAddressOfTopLevelStructure());
					pChild->AppendAsChildOf(pScriptDataNode);
				}
			}
		}
	}
}
#endif // SAVEGAME_USES_XML || __ALLOW_EXPORT_OF_SP_SAVEGAMES

bool CScriptSaveArrayOfStructs::HasRegisteredScriptData()
{
	return (m_ScriptStructs.GetCount() > 0);
}

s32 CScriptSaveArrayOfStructs::GetSizeOfSavedScriptData()
{
	if (m_ScriptStructs.GetCount() > 0)
	{	//	Only do this if there is some save data
		return GetSizeOfStructAndItsChildren(0, m_NameOfTopLevelStructure);
	}

	return 0;
}

s32 CScriptSaveArrayOfStructs::GetSizeOfSavedScriptDataInBytes()
{
	if (m_ScriptStructs.GetCount() > 0)
	{	//	Only do this if there is some save data
		return ComputeSizeInBytesWithGaps(0);
	}

	return 0;
}

u8 *CScriptSaveArrayOfStructs::GetBaseAddressOfTopLevelStructure()
{
#if __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES
	if (CGenericGameStorage::IsImportingOrExporting())
	{
		if (savegameVerifyf(m_BaseAddressOfTopLevelStructureForImportExport, "CScriptSaveArrayOfStructs::GetBaseAddressOfTopLevelStructure - game is importing or exporting a savegame, but m_BaseAddressOfTopLevelStructureForImportExport has not been set"))
		{
			return m_BaseAddressOfTopLevelStructureForImportExport;
		}
		else
		{
			savegameErrorf("CScriptSaveArrayOfStructs::GetBaseAddressOfTopLevelStructure - game is importing or exporting a savegame, but m_BaseAddressOfTopLevelStructureForImportExport has not been set");
			return m_BaseAddressOfTopLevelStructure;
		}
	}
	else
	{
		savegameAssertf(!m_BaseAddressOfTopLevelStructureForImportExport, "CScriptSaveArrayOfStructs::GetBaseAddressOfTopLevelStructure - didn't expect m_BaseAddressOfTopLevelStructureForImportExport to be set. The game is not currently importing or exporting a savegame");
		return m_BaseAddressOfTopLevelStructure;
	}
#else // __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES
	return m_BaseAddressOfTopLevelStructure;
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES
}

int CScriptSaveArrayOfStructs::GetNumberOfCharactersInATextLabelOfThisType(int TypeOfTextLabel)
{
	switch (TypeOfTextLabel)
	{
	case Script_Save_Data_Type_TextLabel3 :
		return 4;
	case Script_Save_Data_Type_TextLabel7 :
		return 8;
	case Script_Save_Data_Type_TextLabel15 :
		return 16;
	case Script_Save_Data_Type_TextLabel23 :
		return 24;
	case Script_Save_Data_Type_TextLabel31 :
		return 32;
	case Script_Save_Data_Type_TextLabel63 :
		return 64;
	}

	savegameAssertf(0, "CScriptSaveArrayOfStructs::GetNumberOfCharactersInATextLabelOfThisType - unexpected type of text label %d", TypeOfTextLabel);
	return 16;
}

