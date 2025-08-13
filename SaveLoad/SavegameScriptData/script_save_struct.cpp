
#include "script_save_struct.h"

// Game headers
#include "SaveLoad/savegame_channel.h"

// **************************************** SaveGameDataBlock - Script - CScriptSaveStruct ********************************

void CScriptSaveStruct::Set(int PCOfRegisterCommand, bool bIsArray, const char* DEV_ONLY(pNameOfInstance))
{
	m_PCOfRegisterCommand = PCOfRegisterCommand;
	//	m_Members.Reset();	//	Is this necessary?
	m_bHasBeenClosed = false;
	m_bIsArray = bIsArray;

#if __DEV
	m_NameOfFirstInstanceThatWasRegistered = pNameOfInstance;
#endif	//	__DEV
}

void CScriptSaveStruct::Shutdown()
{
	m_PCOfRegisterCommand = -1;

	m_Members.Reset();

	m_bHasBeenClosed = false;

	m_MapOfUsedElementNames.Kill();
#if __DEV
	m_NameOfFirstInstanceThatWasRegistered.Clear();
#endif	//	__DEV
}

void CScriptSaveStruct::SetHasBeenClosed(bool bClosed)
{
	m_bHasBeenClosed = bClosed;
}

void CScriptSaveStruct::AddMember(u32 Offset, int DataType, const char *pLabel)
{
	SaveDataItem NewDataItem;
	NewDataItem.m_OffsetWithinStruct = Offset;
	NewDataItem.m_DataType = DataType;
	NewDataItem.m_Name = pLabel;

#if !__NO_OUTPUT
	const char *pDataTypeString = "Struct";
	switch (DataType)
	{
	case Script_Save_Data_Type_Int :
		pDataTypeString = "Int";
		break;
	case Script_Save_Data_Type_Float :
		pDataTypeString = "Float";
		break;
	case Script_Save_Data_Type_TextLabel3 :
		pDataTypeString = "Text3";
		break;

	case Script_Save_Data_Type_TextLabel7 :
		pDataTypeString = "Text7";
		break;

	case Script_Save_Data_Type_TextLabel15 :
		pDataTypeString = "Text15";
		break;

	case Script_Save_Data_Type_TextLabel23 :
		pDataTypeString = "Text23";
		break;

	case Script_Save_Data_Type_TextLabel31 :
		pDataTypeString = "Text31";
		break;

	case Script_Save_Data_Type_TextLabel63 :
		pDataTypeString = "Text63";
		break;

	case Script_Save_Data_Type_Int64 :
		pDataTypeString = "Int64";
		break;
	}

	savegameDebugf3("CScriptSaveStruct::AddMember %s %s Offset=%u", pLabel, pDataTypeString, Offset);
#endif	//	!__NO_OUTPUT

	m_Members.PushAndGrow(NewDataItem);
	RegisterElementName(NewDataItem.m_Name);
}

int CScriptSaveStruct::GetNumberOfMembers()
{
	return m_Members.GetCount();
}

u32 CScriptSaveStruct::GetOffsetWithinStructOfMember(int MemberIndex)
{
	if (savegameVerifyf(MemberIndex >= 0 && MemberIndex < m_Members.GetCount(), "CScriptSaveStruct::GetOffsetWithinStructOfMember - member index is out of range"))
	{
		return m_Members[MemberIndex].m_OffsetWithinStruct;
	}

	return 0;
}

int CScriptSaveStruct::GetDataTypeOfMember(int MemberIndex)
{
	if (savegameVerifyf(MemberIndex >= 0 && MemberIndex < m_Members.GetCount(), "CScriptSaveStruct::GetDataTypeOfMember - member index is out of range"))
	{
		return m_Members[MemberIndex].m_DataType;
	}

	return 0;
}

atString &CScriptSaveStruct::GetNameOfMember(int MemberIndex)
{
	static atString ErrorString;
	if (savegameVerifyf(MemberIndex >= 0 && MemberIndex < m_Members.GetCount(), "CScriptSaveStruct::GetNameOfMember - member index is out of range"))
	{
		return m_Members[MemberIndex].m_Name;
	}
	else
	{
		ErrorString = "Unknown";
		return ErrorString;
	}
}

bool CScriptSaveStruct::DoesElementNameAlreadyExist(const char *pElementNameToCheck)
{
	atString ElementName(pElementNameToCheck);
	int *pExistingData = m_MapOfUsedElementNames.Access(ElementName);
	if (pExistingData)
	{
		return true;
	}

	return false;
}

void CScriptSaveStruct::ClearAllRegisteredElementNames()
{
	m_MapOfUsedElementNames.Kill();
}

void CScriptSaveStruct::RegisterElementName(atString &ElementNameToCheck)
{
	//	push the name on to MapOfUsedElementNames
	//	I'm just using this map as a set to check if pElementNameToCheck already exists 
	//	so the actual data doesn't matter. I'll always set it to 0
	m_MapOfUsedElementNames.Insert(ElementNameToCheck, 0);
}


