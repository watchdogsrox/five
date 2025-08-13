//
// dongle.h
//
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//

#ifndef DONGLE_H
#define DONGLE_H

#define MAX_DONGLE_PATHS (4)
#define MAX_ENCODE_STRING_LENGTH (256)

#include "file/device.h"

#define ENABLE_DONGLE_TOOL 0

#if __MASTER && !RSG_PC
#define ENABLE_DONGLE	0
#else
#define ENABLE_DONGLE	0
#endif // __MASTER


// TODO: the xenon version is in input/dongle.h for some reason, move it over?
#if __PPU || RSG_ORBIS || RSG_DURANGO

class CDongle
{
#if ENABLE_DONGLE
public:
	static bool ValidateCodeFile(const char* encodeString, char* debugString, const char* mcFileName);
#endif // ENABLE_DONGLE


#if ENABLE_DONGLE_TOOL && __BANK
public:
	static void CreateEncodedAddressesFile();
	static void VerifyLocalMachine();

private :
	static void WriteCodeFile(const char* encodeString, const fiDevice::SystemTime& expiryDate, bool bExpires);
	static char* CreateEncodedMacAddress(const char* encodeString, const char* macAddrString, const fiDevice::SystemTime& expiryDate, bool bExpires, s32& encodeStringLen);
	static bool GetExpiryString(const fiDevice::SystemTime& expiryDate, char* expiryString, size_t expiryStringLen);
#endif // ENABLE_DONGLE_TOOL && __BANK


#if ENABLE_DONGLE || ENABLE_DONGLE_TOOL
private:
	static bool Validate(const char* encodeString, const char* macAddrString, char* debugString, char filenames[MAX_DONGLE_PATHS][RAGE_MAX_PATH]);
	static bool CheckDate(const char* dateStr);
	static void Encode(char *buffer, int sizeInBytes);
#endif // ENABLE_DONGLE || ENABLE_DONGLE_TOOL


#if __BANK
public:
	static void InitWidgets();
	static void ShutdownWidgets();
	static void PrintIdentifier();

private:
	static bool sm_bExpires;
	static fiDevice::SystemTime sm_ExpiryTime;
	static char sm_encodeString[MAX_ENCODE_STRING_LENGTH];
	static char sm_addressesPath[RAGE_MAX_PATH];
	static char sm_targetPath[RAGE_MAX_PATH];
#endif // __BANK
};

#endif // __PPU || RSG_ORBIS || RSG_DURANGO

#endif // DONGLE_H
