

#ifndef SCRIPT_SAVE_STACK_H
#define SCRIPT_SAVE_STACK_H

// Rage headers
#include "atl/string.h"

//	CScriptSaveStack - the stack of structs is only used between START_SAVE_DATA and STOP_SAVE_DATA calls.
//	It keeps track of nested structs for saving.
class CScriptSaveStack
{
	struct StructStackEntry
	{
		u8* m_StartAddressOfStruct;
		u8* m_EndAddressOfStruct;
		atString m_NameOfInstanceOfStruct;
		int m_ArrayIndexOfStruct;
	};

public:

	CScriptSaveStack();

	void Shutdown(/*unsigned shutdownMode*/);

	void Push(u8* StartAddress, u8* EndAddress, const char *pNameOfInstance, int ArrayIndex);
	void Pop();

	int GetStackLevel() const { return m_nCurrentStackLevel; }

	int GetArrayIndex(int StackLevel) const;
	u8* GetStartAddress(int StackLevel) const;
	u8* GetEndAddress(int StackLevel) const;
	const atString &GetNameOfInstance(int StackLevel) const;

private:

#define MAX_DEPTH_OF_STACK_OF_STRUCTS	(8)

	StructStackEntry m_StackOfStructs[MAX_DEPTH_OF_STACK_OF_STRUCTS];
	int m_nCurrentStackLevel;
};

#endif // SCRIPT_SAVE_STACK_H
