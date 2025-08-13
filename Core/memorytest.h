//
// core/memorytest.h
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

#ifndef CORE_MEMORYTEST_H__
#define CORE_MEMORYTEST_H__

#define __INCLUDE_MEMORY_TEST_IN_XENON_BUILD	0

#if __XENON && __INCLUDE_MEMORY_TEST_IN_XENON_BUILD

#include "vector/color32.h"

class CMemoryTest
{
public:
	CMemoryTest();
	~CMemoryTest();

	static void StartTest();
	static void Render();

private:

	static bool TestMemory(u32 * pMem, int iNumBytes, u32 iExpectedVal, int iTestIteration);
	static void FoundMemoryError(u32 * pMemAddr, u32 iDesiredVal, u32 iActualVal, int iFailedAfter);

	static void AddOutputString(Color32 iCol, const char * fmt, ...);
	static void ModifyOutputString(const int iIndex, Color32 iCol, const char * fmt, ...);

	static void AllocOutputStrings();
	static void DeallocOutputStrings();
	static void ClearOutputStrings();

	static bool m_bActive;
	static bool m_bTextArrayInUse;

	static atArray<char *> m_TextLines;
	static atArray<Color32> m_TextCols;

	static s32 m_iNumOutputStrings;
	static s32 m_iMaxNumOutputStrings;
};
#endif

#endif // CORE_MEMORYTEST_H__