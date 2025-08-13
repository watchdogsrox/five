
#ifndef SCRIPT_SAVE_STRUCT_H
#define SCRIPT_SAVE_STRUCT_H

// Rage headers
#include "atl/map.h"
#include "atl/string.h"

typedef atMap<atString, int> SaveDataMap;

//	The simple types all have a negative index. 
//	An index >= 0 will be the index of a struct within CScriptSaveArrayOfStructs::m_ScriptStructs
#define Script_Save_Data_Type_Int (-1)
#define Script_Save_Data_Type_Float (-2)
//	#define Script_Save_Data_Type_Bool (-3)
#define Script_Save_Data_Type_TextLabel3 (-4)
#define Script_Save_Data_Type_TextLabel7 (-5)
#define Script_Save_Data_Type_TextLabel15 (-6)
#define Script_Save_Data_Type_TextLabel23 (-7)
#define Script_Save_Data_Type_TextLabel31 (-8)
#define Script_Save_Data_Type_TextLabel63 (-9)
#define Script_Save_Data_Type_Int64 (-10)

//	CScriptSaveStruct - contains the layout of data within one script struct for saving
class CScriptSaveStruct
{
	struct SaveDataItem
	{
		u32 m_OffsetWithinStruct;
		int m_DataType;	//	negative for a simple type, >= 0 for the index of another ScriptStruct
		atString m_Name;
	};

public:

	void Set(int PCOfRegisterCommand, bool bIsArray, const char* pNameOfInstance);

	void Shutdown(/*unsigned shutdownMode*/);

	bool GetHasBeenClosed() { return m_bHasBeenClosed; }
	void SetHasBeenClosed(bool bClosed);

	int GetPC() { return m_PCOfRegisterCommand; }

	void AddMember(u32 Offset, int DataType, const char *pLabel);
	int GetNumberOfMembers();

	u32 GetOffsetWithinStructOfMember(int MemberIndex);
	int GetDataTypeOfMember(int MemberIndex);
	atString &GetNameOfMember(int MemberIndex);

	bool DoesElementNameAlreadyExist(const char *pElementNameToCheck);
	void ClearAllRegisteredElementNames();

	bool GetIsArray() { return m_bIsArray; }

private:
	//	Private functions
	void RegisterElementName(atString &ElementNameToCheck);

	//	Private data
	int m_PCOfRegisterCommand;			//	Assume that two structs registered at the same Program Counter are the same struct and don't register more than once.
	atArray<SaveDataItem> m_Members;
	bool m_bHasBeenClosed;
	bool m_bIsArray;	//	this is only used when calculating the size of the script save data

	SaveDataMap m_MapOfUsedElementNames;
#if __DEV
	atString m_NameOfFirstInstanceThatWasRegistered;
#endif	//	__DEV
};


#endif // SCRIPT_SAVE_STRUCT_H

