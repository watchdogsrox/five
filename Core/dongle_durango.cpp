//
// dongle_durango.cpp
//
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//

#if RSG_DURANGO
#include <consoleid.h>
#include <stdio.h>

// Rage
#include "rline/rl.h"
#include "string/string.h"
#include "system/memops.h"
#include "rline/rldiag.h"

// Game
#include "dongle.h"
#include "system/FileMgr.h"

#pragma comment(lib, "consoleid.lib")

#if ENABLE_DONGLE || ENABLE_DONGLE_TOOL

bool CDongle::CheckDate(const char* dateStr)
{
	SYSTEMTIME st;
	st.wYear = 255;
	GetSystemTime(&st);

	u32 year = 0;
	u32 month = 0;
	u32 day = 0;
	if (sscanf(dateStr, "%d:%d:%d", &year, &month, &day) != 3)
	{
#if __DEV
		Displayf("[CDongle::CheckDate] Failed to parse date '%s', expected 'year:month:day'", dateStr);
#endif
		return false;
	}

	if (year > st.wYear)
		return true;

	if (year == st.wYear)
	{
		if (month > st.wMonth)
			return true;

		if (month == st.wMonth)
		{
			if (day > st.wDay)
				return true;
		}
	}

	return false;
}
#endif // ENABLE_DONGLE || ENABLE_DONGLE_TOOL

#if ENABLE_DONGLE

bool CDongle::ValidateCodeFile(const char* encodeString, char* debugString, const char* mcFileName)
{
	WCHAR consoleIdStringW[CONSOLE_ID_CCH] = {0};
	int err = GetConsoleId(consoleIdStringW, CONSOLE_ID_CCH);
	if (err != 0)
	{
		Displayf("[CDongle::ValidateCodeFile] Error calling GetConsoleId()", err);
        return false;
	}

	char consoleIdString[CONSOLE_ID_CCH * 2] = {0};
    wcstombs(consoleIdString, consoleIdStringW, CONSOLE_ID_CCH * 2);

	CompileTimeAssert(MAX_DONGLE_PATHS >= 1);
	char filePathName[MAX_DONGLE_PATHS][RAGE_MAX_PATH] = {0};
	formatf(filePathName[0], "platform:/DATA/%s", mcFileName);

	return CDongle::Validate(encodeString, consoleIdString, debugString, filePathName);
}

#endif // ENABLE_DONGLE

#if ENABLE_DONGLE_TOOL && __BANK
bool CDongle::GetExpiryString(const fiDevice::SystemTime& expiryDate, char* expiryString, size_t expiryStringLen)
{
	formatf(expiryString, expiryStringLen, "%d:%d:%d", expiryDate.wYear, expiryDate.wMonth, expiryDate.wDay);
	return true;
}
#endif // ENABLE_DONGLE_TOOL && __BANK

#if __BANK
void CDongle::PrintIdentifier()
{
	WCHAR consoleIdStringW[CONSOLE_ID_CCH] = {0};
	int err = GetConsoleId(consoleIdStringW, CONSOLE_ID_CCH);
	if (err != 0)
	{
		Displayf("[CDongle::PrintIdentifier] Error calling GetConsoleId()", err);
	}

	char consoleIdString[CONSOLE_ID_CCH * 2] = {0};
	wcstombs(consoleIdString, consoleIdStringW, CONSOLE_ID_CCH * 2);

	Displayf("\n");
	Displayf("\n");
	Displayf("/*--------------------------*/");
	Displayf("Durango Identifier: %s", consoleIdString);
	Displayf("/*--------------------------*/");
	Displayf("\n");
}
#endif // __BANK

#endif // RSG_DURANGO
