//
// dongle_orbis.cpp
//
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//

#if RSG_ORBIS
#include <net.h>
#include <rtc.h>
#include <stdio.h>

// Rage
#include "rline/rl.h"
#include "string/string.h"
#include "system/memops.h"
#include "rline/rldiag.h"

// Game
#include "dongle.h"
#include "system/FileMgr.h"

#if ENABLE_DONGLE

bool CDongle::ValidateCodeFile(const char* encodeString, char* debugString, const char* mcFileName)
{
    SceNetEtherAddr addr;
	u8 mac[SCE_NET_ETHER_ADDR_LEN] = {0};
	char macAddrString[SCE_NET_ETHER_ADDRSTRLEN] ={0};
    int err = sceNetGetMacAddress(&addr, 0);
    if (err == 0)
	{
		sysMemCpy(mac, addr.data, SCE_NET_ETHER_ADDR_LEN);

		sprintf(macAddrString, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		macAddrString[17] = '\0';
	}
	else
	{
		rlNpError("[CDongle::ValidateCodeFile] Error calling sceNetGetMacAddress()", err);
	}

    CompileTimeAssert(MAX_DONGLE_PATHS >= 4);
	char filePathName[MAX_DONGLE_PATHS][RAGE_MAX_PATH] = {0};
	formatf(filePathName[0], "platform:/DATA/%s", mcFileName);
	formatf(filePathName[1], "/usb0/%s", mcFileName);
	formatf(filePathName[2], "/usb1/%s", mcFileName);
	formatf(filePathName[3], "/usb2/%s", mcFileName);
	// we have up to /usb7 but checking the first three storage devices should be enough

    return CDongle::Validate(encodeString, macAddrString, debugString, filePathName);
}

#endif // ENABLE_DONGLE

#if ENABLE_DONGLE_TOOL && __BANK
bool CDongle::GetExpiryString(const fiDevice::SystemTime& expiryDate, char* expiryString, size_t expiryStringLen)
{
	// Generate a system time strin	// g
	SceRtcDateTime cellDate;
	cellDate.year = expiryDate.wYear;
	cellDate.month = expiryDate.wMonth;
	cellDate.day = expiryDate.wDay;
	cellDate.hour = expiryDate.wHour;
	cellDate.minute = expiryDate.wMinute;
	cellDate.second = expiryDate.wSecond;
	cellDate.microsecond = 0;

	SceRtcTick cellExpiryTick;
	if (sceRtcGetTick(&cellDate, &cellExpiryTick) != SCE_OK)
	{
#if __BANK
		Displayf("[CDongle] Error converting expiry time! Dongle not written");
#endif // __BANK
		return false;
	}

	sceRtcFormatRFC2822LocalTime(expiryString, &cellExpiryTick);
	return true;
}
#endif // ENABLE_DONGLE_TOOL && __BANK

#if ENABLE_DONGLE || ENABLE_DONGLE_TOOL
bool CDongle::CheckDate(const char* dateStr)
{
	// Look up current date and time
	SceRtcTick currentTick;
	int err = sceRtcGetCurrentTick(&currentTick);
	if (err != SCE_OK)
	{
#if __DEV
		Displayf("[CDongle::CheckDate] Error %d calling sceRtcGetCurrentTick!", err);
#endif
	}

	SceRtcTick readTick;
	err = sceRtcParseDateTime(&readTick, dateStr);
	if (err == SCE_OK)
	{
		if(readTick.tick > currentTick.tick)
		{
			return true;
		}
	}
	else
	{
#if __DEV
		Displayf("[CDongle::CheckDate] Error %d parsing data '%s' in dongle!", err, dateStr);
#endif
	}

	return false;
}
#endif // ENABLE_DONGLE || ENABLE_DONGLE_TOOL

#if __BANK
void CDongle::PrintIdentifier()
{
	SceNetEtherAddr addr;
	u8 mac[SCE_NET_ETHER_ADDR_LEN] = {0};
	char macAddrString[SCE_NET_ETHER_ADDRSTRLEN] ={0};
	int err = sceNetGetMacAddress(&addr, 0);
	if (err == 0)
	{
		sysMemCpy(mac, addr.data, SCE_NET_ETHER_ADDR_LEN);

		sprintf(macAddrString, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		macAddrString[17] = '\0';
	}
	else
	{
		rlNpError("[CDongle::PrintIdentifier] Error calling sceNetGetMacAddress()", err);
	}

	Displayf("\n");
	Displayf("\n");
	Displayf("/*--------------------------*/");
	Displayf("Orbis Identifier: %s", macAddrString);
	Displayf("/*--------------------------*/");
	Displayf("\n");
}
#endif // __BANK

#endif // RSG_ORBIS