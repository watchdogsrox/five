//
// dongle_psn.cpp
//
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//

#if __PPU
#include <netex/libnetctl.h>
#include <cell/rtc.h>
#include <sys/return_code.h>
#include <stdio.h>
#include <sys/paths.h>
#include <sys/fs_external.h>

// Rage
#include "data/aes.h"
#include "rline/rl.h"
#include "string/string.h"
#include "system/memops.h"
#include "rline/rldiag.h"

// Game
#include "dongle.h"
#include "system/FileMgr.h"

#if ENABLE_DONGLE

#define GAMEDATA_DIR SYS_DEV_USB "/"
#define GAMEDATA_DIR2 SYS_DEV_MS "/"

bool CDongle::ValidateCodeFile(const char* encodeString, char* debugString, const char* mcFileName)
{
	CellNetCtlInfo cell_info;
	u8 mac[6];
	char macAddrString [18];

	// Look up current MAC address
	int err = cellNetCtlGetInfo(CELL_NET_CTL_INFO_ETHER_ADDR, &cell_info);
	if(0 <= err)
	{
		CompileTimeAssert(sizeof(mac) == CELL_NET_CTL_ETHER_ADDR_LEN);
		sysMemCpy(mac, cell_info.ether_addr.data, CELL_NET_CTL_ETHER_ADDR_LEN);

		// Get the Hexa String of the Mac Address
		sprintf(macAddrString, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		macAddrString[17] = '\0';
	}
	else
	{
		rlNpError("[CDongle::ValidateCodeFile] Error calling cellNetCtlGetInfo()", err);
	}

    CompileTimeAssert(MAX_DONGLE_PATHS >= 4);
	char filePathName[MAX_DONGLE_PATHS][RAGE_MAX_PATH] = {0};
	formatf(filePathName[0], "platform:/DATA/%s", mcFileName);
	snprintf(filePathName[1], RAGE_MAX_PATH, "%s%s", GAMEDATA_DIR, mcFileName);
	snprintf(filePathName[2], RAGE_MAX_PATH, "%s%s", GAMEDATA_DIR2, mcFileName);

    return CDongle::Validate(encodeString, macAddrString, debugString, filePathName);
}

#endif // ENABLE_DONGLE

#if __BANK
void CDongle::PrintIdentifier()
{
	CellNetCtlInfo cell_info;
	u8 mac[6];
	char macAddrString [18];

	// Look up current MAC address
	int err = cellNetCtlGetInfo(CELL_NET_CTL_INFO_ETHER_ADDR, &cell_info);
	if(0 <= err)
	{
		CompileTimeAssert(sizeof(mac) == CELL_NET_CTL_ETHER_ADDR_LEN);
		sysMemCpy(mac, cell_info.ether_addr.data, CELL_NET_CTL_ETHER_ADDR_LEN);

		// Get the Hexa String of the Mac Address
		sprintf(macAddrString, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		macAddrString[17] = '\0';
	}
	else
	{
		rlNpError("[CDongle::ValidateCodeFile] Error calling cellNetCtlGetInfo()", err);
	}

	Displayf("\n");
	Displayf("\n");
	Displayf("/*--------------------------*/");
	Displayf("Orbis Identifier: %s", macAddrString);
	Displayf("/*--------------------------*/");
	Displayf("\n");
}

#endif // __BANK

#if ENABLE_DONGLE_TOOL && __BANK
bool CDongle::GetExpiryString(const fiDevice::SystemTime& expiryDate, char* expiryString, size_t UNUSED_PARAM(expiryStringLen))
{
	// Generate a system time string
	CellRtcDateTime cellDate;
	cellDate.year = expiryDate.wYear;
	cellDate.month = expiryDate.wMonth;
	cellDate.day = expiryDate.wDay;
	cellDate.hour = expiryDate.wHour;
	cellDate.minute = expiryDate.wMinute;
	cellDate.second = expiryDate.wSecond;
	cellDate.microsecond = 0;

	CellRtcTick cellExpiryTick;
	if (cellRtcGetTick(&cellDate, &cellExpiryTick) != CELL_OK)
	{
#if __BANK
		Displayf("[CDongle] Error converting expiry time! Dongle not written");
#endif // __BANK
		return false;
	}

	cellRtcFormatRfc2822LocalTime(expiryString, &cellExpiryTick);
	return true;
}
#endif // ENABLE_DONGLE_TOOL && __BANK

#if ENABLE_DONGLE || ENABLE_DONGLE_TOOL
bool CDongle::CheckDate(const char* dateStr)
{
	// Look up current date and time
	CellRtcTick currentTick;
	cellRtcGetCurrentTick(&currentTick); // Get current time (local time)

	CellRtcTick readTick;
	if(cellRtcParseDateTime(&readTick, dateStr) == CELL_OK)
	{
		if(readTick.tick > currentTick.tick)
		{
			return true;
		}
	}
	else
	{
#if __DEV
		Displayf("[CDongle::CheckDate] Notice: No expiry found in dongle");
#endif
	}
	return false;
}
#endif // ENABLE_DONGLE || ENABLE_DONGLE_TOOL

#endif // __PPU

// EOF

