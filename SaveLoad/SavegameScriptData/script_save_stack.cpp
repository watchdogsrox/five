
#include "script_save_stack.h"


// Game headers
#include "SaveLoad/savegame_channel.h"

// **************************************** SaveGameDataBlock - Script - CScriptSaveStack ************************************

CScriptSaveStack::CScriptSaveStack()
{
	m_nCurrentStackLevel = -1;
}

void CScriptSaveStack::Shutdown()
{
	for (int loop = 0; loop < MAX_DEPTH_OF_STACK_OF_STRUCTS; loop++)
	{
		m_StackOfStructs[loop].m_StartAddressOfStruct = 0;
		m_StackOfStructs[loop].m_EndAddressOfStruct = 0;
		m_StackOfStructs[loop].m_NameOfInstanceOfStruct.Clear();
		m_StackOfStructs[loop].m_ArrayIndexOfStruct = -1;
	}

	m_nCurrentStackLevel = -1;
}

void CScriptSaveStack::Push(u8* StartAddress, u8* EndAddress, const char *pNameOfInstance, int ArrayIndex)
{
	if (savegameVerifyf(m_nCurrentStackLevel < (MAX_DEPTH_OF_STACK_OF_STRUCTS-1), "CScriptSaveStack::Push - no space left on the stack for %s", pNameOfInstance))
	{
		m_nCurrentStackLevel++;

		m_StackOfStructs[m_nCurrentStackLevel].m_StartAddressOfStruct = StartAddress;
		m_StackOfStructs[m_nCurrentStackLevel].m_EndAddressOfStruct = EndAddress;
		m_StackOfStructs[m_nCurrentStackLevel].m_NameOfInstanceOfStruct = pNameOfInstance;
		m_StackOfStructs[m_nCurrentStackLevel].m_ArrayIndexOfStruct = ArrayIndex;
	}
}

void CScriptSaveStack::Pop()
{
	if (savegameVerifyf(m_nCurrentStackLevel >= 0, "CScriptSaveStack::Pop - stack is already empty"))
	{
		m_StackOfStructs[m_nCurrentStackLevel].m_NameOfInstanceOfStruct.Clear();
		m_nCurrentStackLevel--;
	}
}

int CScriptSaveStack::GetArrayIndex(int StackLevel) const
{
	if (savegameVerifyf( (StackLevel >= 0) && (StackLevel <= m_nCurrentStackLevel), "CScriptSaveStack::GetArrayIndex - stack level is invalid"))
	{
		return 	m_StackOfStructs[StackLevel].m_ArrayIndexOfStruct;
	}

	return -1;
}

u8* CScriptSaveStack::GetStartAddress(int StackLevel) const
{
	if (savegameVerifyf( (StackLevel >= 0) && (StackLevel <= m_nCurrentStackLevel), "CScriptSaveStack::GetStartAddress - stack level is invalid"))
	{
		return 	m_StackOfStructs[StackLevel].m_StartAddressOfStruct;
	}

	return 0;
}

u8* CScriptSaveStack::GetEndAddress(int StackLevel) const
{
	if (savegameVerifyf( (StackLevel >= 0) && (StackLevel <= m_nCurrentStackLevel), "CScriptSaveStack::GetEndAddress - stack level is invalid"))
	{
		return 	m_StackOfStructs[StackLevel].m_EndAddressOfStruct;
	}

	return 0;
}

const atString &CScriptSaveStack::GetNameOfInstance(int StackLevel) const
{
	static atString ErrorString;
	if (savegameVerifyf( (StackLevel >= 0) && (StackLevel <= m_nCurrentStackLevel), "CScriptSaveStack::GetNameOfInstance - stack level is invalid"))
	{
		return 	m_StackOfStructs[StackLevel].m_NameOfInstanceOfStruct;
	}
	else
	{
		ErrorString = "Unknown";
		return ErrorString;
	}
}


