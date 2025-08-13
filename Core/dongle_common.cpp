//
// dongle_common.cpp
//
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//

#if __PPU || RSG_ORBIS || RSG_DURANGO

// Rage
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "data/aes.h"
#include "rline/rl.h"
#include "string/string.h"
#include "system/memops.h"
#include "rline/rldiag.h"
#include "file/stream.h"

// Game
#include "dongle.h"
#include "system/FileMgr.h"

#if ENABLE_DONGLE || ENABLE_DONGLE_TOOL

bool CDongle::Validate(const char* encodeString, const char* macAddrString, char* debugString, char filenames[MAX_DONGLE_PATHS][RAGE_MAX_PATH])
{
	bool ret = false;

	// Note: This gets a bit messy since key and mac address are encoded one way (irreversibly) but 
	// date stamp is encoded using AES (reversibly). This is done to maintain backwards compatibility with
	// previous dongles

	/*-------------------------------*/
	s32 bufferMacAddressLen = StringLength(macAddrString)+1;
	s32 bufferKeyStringLen  = StringLength(encodeString)+1;
	s32 bufferDateStringLen = 32;
	s32 bufferLen = bufferMacAddressLen+bufferKeyStringLen+bufferDateStringLen;

	char* origBuffer = Alloca(char, bufferLen);
	char* encodedBuffer = Alloca(char, bufferLen);
	// code to ensure release and beta versions of this work
	memset(origBuffer, 0xcd, bufferLen);

	// Encode current key and mac address for comparison with read one
	// No need to do for date
	char* origBufferMacAddress = Alloca(char, bufferMacAddressLen);
	char* origBufferKeyString  = Alloca(char, bufferKeyStringLen);
	// code to ensure release and beta versions of this work
	memset(origBufferMacAddress, 0xcd, bufferMacAddressLen);
	memset(origBufferKeyString, 0xcd, bufferKeyStringLen);

	// Encode Mac Address
	sysMemCpy(origBufferMacAddress, macAddrString, bufferMacAddressLen);
	Encode(origBufferMacAddress, bufferMacAddressLen);

	// Encode Key String
	sysMemCpy(origBufferKeyString, encodeString,bufferKeyStringLen);
	Encode(origBufferKeyString, bufferKeyStringLen);

	// Mac Address + Key String
	sysMemCpy(origBuffer, origBufferMacAddress, bufferMacAddressLen); 
	sysMemCpy(origBuffer+bufferMacAddressLen, origBufferKeyString, bufferKeyStringLen);

	for (s32 i = 0; i < MAX_DONGLE_PATHS; ++i)
	{
        if (!filenames[i])
            continue;

		FileHandle fid = CFileMgr::OpenFile(filenames[i]); // open for reading
		if (!fid)
			continue;

		bool bDateOK = false;
		bool bKeyOK = false;
		bool bMacOK = false;

		while(true)
		{
			if(CFileMgr::Read(fid, encodedBuffer, bufferLen) != bufferLen)
				break;
			//if(*pLine == '#' || *pLine == '\0')
			//	break;

			// Check if current date is past expiry
			//int bytesRead = 0;
			//bytesRead = StringLength(pLine);

			/*--------------------------*/
			// Compare with read date
			char* readDateStr = encodedBuffer+bufferMacAddressLen+bufferKeyStringLen;
			AES aes(AES_KEY_ID_DEFAULT);
			aes.Decrypt(readDateStr, bufferLen-bufferMacAddressLen-bufferKeyStringLen);

            bDateOK = CDongle::CheckDate(readDateStr);

			/*--------------------------*/
			// Check MAC
			if(memcmp(origBufferMacAddress, encodedBuffer, bufferMacAddressLen) == 0)
			{
				bMacOK = true;
			}

			/*--------------------------*/
			// Check key
			if(memcmp(origBufferKeyString, encodedBuffer + bufferMacAddressLen, bufferKeyStringLen) == 0)
			{
				bKeyOK = true;
			}

			// Note if no date could be read then we assume an expiry date was not set and continue loading
			if(bKeyOK && bMacOK && bDateOK)
			{
				ret = true;

				Displayf("\n");
				Displayf("\n");
				Displayf("/*--------------------------*/");
				Displayf("AUTHORIZATION OK");
				Displayf("/*--------------------------*/");
				Displayf("\n");
				break;
			}
			else
			{
				if( !bMacOK )
					strcpy(debugString, " UNAUTHORIZED demo copy - The dongle is for a different machine.!");
				if( !bKeyOK )
					strcpy(debugString, " UNAUTHORIZED demo copy - The dongle key does not match the game.! ");
				if( !bDateOK )
					strcpy(debugString, " UNAUTHORIZED demo copy - The dongle has expired.! ");
			}
		}
		CFileMgr::CloseFile(fid);

		bool failed = false;
		if (!ret && !(bKeyOK && bMacOK && bDateOK))
		{
			Displayf("\n");
			Displayf("\n");
			Displayf("/*--------------------------*/");
			Displayf("UNAUTHORIZED demo copy");
			Displayf("/*--------------------------*/");
			Displayf("\n");
			failed = true;
		}

		if (!failed)
			break;
	}

	return ret;
}

void CDongle::Encode(char *buffer, int sizeInBytes)
{
	int						value;
	int						i;

	for(i=0; i<sizeInBytes; i++)
	{
		value= buffer[i];
		value += 61 - (77 * i)%13 + i/5;

		if(i>0)
		{
			value += buffer[i-1];
		}

		if(i<sizeInBytes-1)
		{
			value -= buffer[i+1] / 2;
		}

		value= value % 255;
		buffer[i]= (char)value;
	}
}
#endif // ENABLE_DONGLE || ENABLE_DONGLE_TOOL

#if ENABLE_DONGLE_TOOL && __BANK
void CDongle::WriteCodeFile(const char* encodeString, const fiDevice::SystemTime& expiryDate, bool bExpires)
{
	int iNumAddresses = 0;

	CFileMgr::SetDir("");
	FileHandle fid = CFileMgr::OpenFile(sm_addressesPath); // open for reading

	if (!CFileMgr::IsValidFileHandle(fid))
	{
		Assertf(0, "[CDongle::WriteCodeFile] - Could not open %s file to read mac addresses!", sm_addressesPath );
		return;
	}

	if (fid)
	{
		char filePathName[RAGE_MAX_PATH];
		snprintf(filePathName, RAGE_MAX_PATH, "%s", sm_targetPath);
		fiStream *fileStream = fiStream::Create(&filePathName[0]);
		if(!fileStream)
		{
			CFileMgr::CloseFile(fid);
			Displayf("[CDongle::WriteCodeFile] - DONGLE NOT WRITTEN, failed to create %s", sm_targetPath);
			return;
		}

		char* pLine = NULL;
		while ((pLine = CFileMgr::ReadLine(fid)) != NULL)
		{
			if(*pLine == '#' || *pLine == '\0')
				continue;

			s32 length;
			char* origString = CreateEncodedMacAddress(encodeString, pLine, expiryDate, bExpires, length);
			if (origString)
			{
				fileStream->Write(origString, length);
			}

			iNumAddresses++;
		}

		Displayf("[CDongle::WriteCodeFile] FOUND -  %d MAC ADDRESSES", iNumAddresses);

		int ret = fileStream->Close();
		if (ret != 0)
		{
			Assertf(0, "[CDongle::WriteCodeFile] ERROR CLOSING FILE");
			Assertf(0, "[CDongle::WriteCodeFile] DONGLE NOT WRITTEN");
		}

		CFileMgr::CloseFile(fid);
	}
}

char* CDongle::CreateEncodedMacAddress(const char* encodeString, const char* macAddrString, const fiDevice::SystemTime& expiryDate, bool bExpires, s32& encodeStringLen)
{
	u32 bufferMacAddressLen = StringLength(macAddrString)+1;
	u32 bufferKeyStringLen  = StringLength(encodeString)+1;
	u32 bufferExpiryStringLen = 32;

	fiDevice::SystemTime localExpiryDate = expiryDate;
	char* origBufferExpiryString = Alloca(char, bufferExpiryStringLen);
	if (!bExpires)
	{
		localExpiryDate.wYear = 2100;
	}

	if (!CDongle::GetExpiryString(localExpiryDate, origBufferExpiryString, bufferExpiryStringLen))
	{
		Displayf("[CDongle::CreateEncodedMacAddress] Error converting expiry time! Dongle not writtenf or '%s'", macAddrString);
		return NULL;
	}

#if __BANK
	Displayf("[CDongle] Key : %s || %d", encodeString, bufferKeyStringLen);
	Displayf("[CDongle] Mac Address : %s || %d", macAddrString, bufferMacAddressLen);
	Displayf("[CDongle] Expiry Date : %s || %d", origBufferExpiryString, bufferExpiryStringLen);
#endif // __BANK

	// We will use AES to encrypt the date, its length must be a multiple of 16
	if (bufferExpiryStringLen % 16 > 0)
		bufferExpiryStringLen += 16-(bufferExpiryStringLen % 16);

	u32 bufferLen = bufferMacAddressLen + bufferKeyStringLen + bufferExpiryStringLen;
	char* outputString = Alloca(char, bufferLen);
	// code to ensure release and beta versions of this work
	memset(outputString, 0xcd, bufferLen);

	char* origBufferMacAddress = Alloca(char, bufferMacAddressLen);
	char* origBufferKeyString  = Alloca(char, bufferKeyStringLen);

	// Encode Mac Address
	sysMemCpy(origBufferMacAddress, macAddrString, bufferMacAddressLen);
	CDongle::Encode(origBufferMacAddress, bufferMacAddressLen);

	// Encode Key String
	sysMemCpy(origBufferKeyString, encodeString, bufferKeyStringLen);
	CDongle::Encode(origBufferKeyString, bufferKeyStringLen);

	AES aes(AES_KEY_ID_DEFAULT);
	aes.Encrypt(origBufferExpiryString, bufferExpiryStringLen);

	// Copy Mac Address + Key String + Date string into origBuffer (for writing)
	sysMemCpy(outputString, origBufferMacAddress, bufferMacAddressLen);
	sysMemCpy(outputString+bufferMacAddressLen, origBufferKeyString, bufferKeyStringLen);
	sysMemCpy(outputString+bufferMacAddressLen+bufferKeyStringLen, origBufferExpiryString, bufferExpiryStringLen);

	encodeStringLen = bufferLen;
	return outputString;
}

void CDongle::CreateEncodedAddressesFile()
{
	CDongle::WriteCodeFile(sm_encodeString, sm_ExpiryTime, sm_bExpires);
}

void CDongle::VerifyLocalMachine()
{
	char* pLine;
	CFileMgr::SetDir("");
	FileHandle fid = CFileMgr::OpenFile(sm_addressesPath); // open for reading

	if (!CFileMgr::IsValidFileHandle(fid))
	{
		Assertf(0, "[CDongle::VerifyLocalMachine] - Could not open %s file to read mac addresses!", sm_addressesPath);
		return;
	}

	if (fid)
	{
		char debugString [500] = {0};
		char filePathName[MAX_DONGLE_PATHS][RAGE_MAX_PATH] = {0};
		formatf(filePathName[0], sm_targetPath);

		while( ((pLine = CFileMgr::ReadLine(fid)) != NULL) )
		{
			if(*pLine == '#' || *pLine == '\0')
				continue;

			if (!CDongle::Validate(sm_encodeString, pLine, debugString, filePathName))
			{
				Displayf("[CDongle::VerifyLocalMachine] - %s", debugString);
				Assertf(0, "Validation failed for '%s', see TTY for more info!", pLine);
			}
		}
		CFileMgr::CloseFile(fid);
	}
}
#endif // ENABLE_DONGLE_TOOL && __BANK

#if __BANK
bool                 CDongle::sm_bExpires = false;
fiDevice::SystemTime CDongle::sm_ExpiryTime;
char                 CDongle::sm_encodeString[MAX_ENCODE_STRING_LENGTH];
char                 CDongle::sm_addressesPath[RAGE_MAX_PATH];
char                 CDongle::sm_targetPath[RAGE_MAX_PATH];

void CDongle::InitWidgets()
{
	sm_ExpiryTime.wDay = 1;
	sm_ExpiryTime.wMonth = 1;
	sm_ExpiryTime.wYear = 1900;

#if ENABLE_DONGLE_TOOL
	formatf(sm_encodeString, "testingtesting123");
	formatf(sm_targetPath, "platform:/data/root.sys");
	formatf(sm_addressesPath, "platform:/data/addresses.dat");
#endif // ENABLE_DONGLE_TOOL

	rage::bkBank& bank = BANKMGR.CreateBank("ConsoleId");

#if ENABLE_DONGLE_TOOL
	bank.PushGroup("Expiry Date");
	{
		bank.AddToggle("Dongle expires", &sm_bExpires);
		bank.AddSlider("Day",  &sm_ExpiryTime.wDay, 1, 31, 1.0f);
		bank.AddSlider("Month (1: Jan, 2: Feb, etc)",&sm_ExpiryTime.wMonth, 1, 12, 1.0f);
		bank.AddSlider("Year", &sm_ExpiryTime.wYear, 1900, 2100, 1.0f);
	}
	bank.PopGroup();
	bank.PushGroup("Operations");
	{
		bank.AddText("Encode string", sm_encodeString, MAX_ENCODE_STRING_LENGTH, false);
		bank.AddText("Addresses file", sm_addressesPath, RAGE_MAX_PATH, false);
		bank.AddText("Target file", sm_targetPath, RAGE_MAX_PATH, false);
		bank.AddButton("Create file with encoded addresses", datCallback(CreateEncodedAddressesFile));
		bank.AddButton("Verify Local Machine", datCallback(VerifyLocalMachine));
		bank.AddButton("Print identifier", datCallback(PrintIdentifier));
	}
	bank.PopGroup();
#else
	bank.AddButton("Print identifier", datCallback(PrintIdentifier));
#endif  // ENABLE_DONGLE_TOOL
}

void CDongle::ShutdownWidgets()
{
	rage::bkBank* bank = BANKMGR.FindBank("ConsoleId");
	if (bank)
		BANKMGR.DestroyBank(*bank);
}
#endif // __BANK

#endif // __PPU || RSG_ORBIS || RSG_DURANGO