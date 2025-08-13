#ifndef INC_MACHINE_HASH_H_
#define INC_MACHINE_HASH_H_

#include <algorithm>
#include <map>
#include <vector>
#include <stdio.h>
#include <sstream>
#include <comdef.h>
#include <WbemIdl.h>

#if RSG_PC || RSG_LAUNCHER
// First lets go through and include any additional libraries we might need
#if RSG_LAUNCHER
// For the launcher
#include <stdio.h>
#include <string.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include "SmtpEmail.h"
#if _DEBUG
#pragma comment(lib, "libeay32MTd.lib")
#pragma comment(lib, "ssleay32MTd.lib")
#else
#pragma comment(lib, "libeay32MT.lib")
#pragma comment(lib, "ssleay32MT.lib")
#endif
#include "Channel.h"
#else
// This is for the game
#include "wolfssl/wolfcrypt/sha256.h"
#include "wolfssl/wolfcrypt/random.h"
#include "system/exec.h"
#endif

#if RSG_LAUNCHER
#define hashDebug DEBUGF1
#else
#define hashDebug rlDebug
#endif


#if RSG_ENTITLEMENT_ENABLED || !RSG_PRELOADER_LAUNCHER

// Global constants
// I want to scope out the correct defines depending on the venue of the implementation
#if RSG_LAUNCHER
#define HASH_DIGEST_LENGTH SHA256_DIGEST_LENGTH
#else
#define HASH_DIGEST_LENGTH SHA256_DIGEST_SIZE
#endif

#define BASE64_HASH_DIGEST_STRING

#ifdef BASE64_HASH_DIGEST_STRING
#define HASH_DIGEST_STRING_LENGTH (((HASH_DIGEST_LENGTH+2)/3)*4+1)
#else
#define HASH_DIGEST_STRING_LENGTH (HASH_DIGEST_LENGTH*2+1)
#endif

// Additional debugging defines in case we want to selectively try things
#define HASH_TEST_WMI 0
#define HASH_TEST_W32 0
#define HASH_TEST_SMB 0
#define HASH_TEST_FLB 0
#define HASH_TEST_SEL 0
#define DRIVE_PATH_SIZE 8
// Static variables
// These are provided by name so they can be referenced by the anti-tamper tool at runtime
static volatile char *g_machineHash;

extern __THREAD int RAGE_LOG_DISABLE;

#ifndef RSG_LAUNCHER
namespace rage
{

	RAGE_DEFINE_SUBCHANNEL(rline, machinehash);
#undef __rage_channel
#define __rage_channel rline_machinehash

#endif

// Facility function to compute the SHA-256 Hash, depending on the venue
static void SHA256Hash(const unsigned char* inData, unsigned int inDataSize, unsigned char* outHash)
{
	if (!inData || !outHash)
		return;
#if RSG_LAUNCHER
	SHA256_CTX ctx;
	SHA256_Init(&ctx);
	SHA256_Update(&ctx, inData, inDataSize);
	SHA256_Final(outHash, &ctx);
#else
	wc_Sha256 sha;
	wc_InitSha256(&sha);
	wc_Sha256Update(&sha, inData, inDataSize);
	wc_Sha256Final(&sha, outHash);
#endif
}

// Helper function accessible by the launcher and the game.
// This stringifies the hash in addition to adding the null-terminator 
// such that it can be passed along to SCS in a form they play well with
static void SHAHashHelper(const unsigned char* inData, unsigned int inDataSize, char* outHash)
{
	//@@: range MACHINEHASH_SHAHASHHELPER {
		unsigned char hashOutput[HASH_DIGEST_LENGTH] = {0};
	//@@: location MACHINEHASH_SHAHASHHELPER_CALL_HASHING
	SHA256Hash(inData, inDataSize, hashOutput);
	unsigned char * pin = hashOutput;
	//@@: range ENTITLEMENTMANAGER_SHA1HASHHELPER_TOOL_HEX_CONVERT {

#ifdef BASE64_HASH_DIGEST_STRING

	static const int BASE64_SHIFT[4] =
	{
	    10, 4, 6, 8,
	};

	static const char BASE64_ALPHABET[65] =
	{
	    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	    "abcdefghijklmnopqrstuvwxyz"
	    "0123456789"
	    "+/"
	};

	int idx0 = 0;
    int idx1 = 6;
    
    const int numBits = (int)HASH_DIGEST_LENGTH * 8;
    size_t len = (numBits + 5) / 6;

    for (size_t i = 0; i < len; ++i, idx0 += 6, idx1 += 6)
    {
        size_t tmp = (pin[idx0 >> 3] << 8);
        if(idx1 < numBits)
        {
            tmp |= pin[idx1 >> 3];
        }

        const int shift = BASE64_SHIFT[i & 0x03];
        const char val = (char) (tmp >> shift) & 0x3F;

        outHash[i] = BASE64_ALPHABET[val];
    }

    while (len < HASH_DIGEST_STRING_LENGTH-1)
    {
        outHash[len++] = '=';
    }

    outHash[HASH_DIGEST_STRING_LENGTH-1] = '\0';

	hashDebug("Machine Hash: %s", outHash);

#else
	const char * hex = "0123456789ABCDEF";
	char * pout = outHash;
	for(int i = 0; i < HASH_DIGEST_LENGTH - 1; ++i){
		*pout++ = hex[(*pin>>4)&0xF];
		*pout++ = hex[(*pin++)&0xF];
	}
	*pout++ = hex[(*pin>>4)&0xF];
	*pout++ = hex[(*pin)&0xF];
	*pout = 0;
#endif

	//@@: } ENTITLEMENTMANAGER_SHA1HASHHELPER_TOOL_HEX_CONVERT

	
	//@@: } MACHINEHASH_SHAHASHHELPER
}



class IMachineHashStrategy
{
protected:

#if RSG_LAUNCHER
	static bool AccumulateRegistryStrValue(std::vector<unsigned char>& accum, const CHAR* key64, const CHAR* valName)
#else
	static bool AccumulateRegistryStrValue(atArray<unsigned char>& accum, const CHAR* key64, const CHAR* valName)
#endif
	{

		bool retVal = false;
		HKEY hKey;
		//@@: range IMACHINEHASHSTRATEGY_ACCUMULATEREGISTRYSTRVALUE {
		DWORD dwRes = RegOpenKeyExA(
			HKEY_LOCAL_MACHINE, 
			key64,
			0,
			KEY_READ,
			&hKey);


		if(dwRes == ERROR_SUCCESS)
		{
			ULONG allocSize = 1024;
			ULONG len = allocSize;
			CHAR * buffer;
#if RSG_LAUNCHER
			buffer = new CHAR[len];
#else
			buffer = rage_new CHAR[len];
#endif

			while (RegQueryValueExA(hKey, valName, NULL, NULL, (LPBYTE)buffer, &len) == ERROR_MORE_DATA)
			{
				allocSize *= 2;
				len = allocSize;
				delete [] buffer;
#if RSG_LAUNCHER
				buffer = new CHAR[len];
#else
				buffer = rage_new CHAR[len];
#endif
			}

			accum.insert(accum.end(),buffer, buffer+(sizeof(CHAR)*len));


			retVal = true;
			delete [] buffer;
		}
		return retVal;
		//@@: } IMACHINEHASHSTRATEGY_ACCUMULATEREGISTRYSTRVALUE

	}


#if RSG_LAUNCHER
	static std::string ExecCmd(LPSTR cmdline)
#else
	static atString ExecCmd(LPSTR cmdline)
#endif
	{
		HANDLE stdOutRead, stdOutWrite;
		//@@: location IMACHINEHASHSTRATEGY_EXECCMD_ENTRY
		SECURITY_ATTRIBUTES secAttr;
		secAttr.nLength = sizeof (SECURITY_ATTRIBUTES);
		secAttr.bInheritHandle = TRUE;
		secAttr.lpSecurityDescriptor = NULL;


		CHAR pathBuffer[MAX_PATH] = {0};
		CHAR cmdBuffer[1024] = {0};
		//@@: range IMACHINEHASHSTRATEGY_EXECCMD {

		hashDebug("ExecCmd: Getting Windows Directory");

		GetWindowsDirectoryA(pathBuffer, MAX_PATH);
		formatf(cmdBuffer, "%s\\System32\\%s", pathBuffer, cmdline);



		//std::wstringstream ss;
		//ss << pathBuffer << "\\System32\\"<< cmdline;

		hashDebug("ExecCmd: Creating Pipe to Write Contents");

		if (!CreatePipe(&stdOutRead, &stdOutWrite, &secAttr, 0))
			return NULL;

		hashDebug("ExecCmd: Setting Handle Information");

		if (!SetHandleInformation(stdOutRead, HANDLE_FLAG_INHERIT, 0))
			return NULL;

		// Create child process:

		PROCESS_INFORMATION procInfo;
		STARTUPINFOA startupInfo;

		ZeroMemory(&procInfo, sizeof(PROCESS_INFORMATION));
		ZeroMemory(&startupInfo, sizeof(STARTUPINFO));

		startupInfo.cb = sizeof(STARTUPINFO);
		startupInfo.hStdError = stdOutWrite;
		startupInfo.hStdOutput = stdOutWrite;
		startupInfo.dwFlags |= STARTF_USESTDHANDLES;


		hashDebug("ExecCmd: Creating Process");

		BOOL bSuccess = CreateProcessA(NULL,
			cmdBuffer,
			NULL,
			NULL,
			TRUE,
			CREATE_NO_WINDOW,
			NULL,
			NULL,
			&startupInfo,
			&procInfo);

		if (bSuccess)
		{

			hashDebug("ExecCmd: Closing Process Handles");

			CloseHandle(procInfo.hProcess);
			CloseHandle(procInfo.hThread);
			CloseHandle(stdOutWrite);
		}
		else
			return NULL;

		// R/W to pipes

		const size_t bufSize = 4096;
		DWORD bytesRead;
#if RSG_LAUNCHER
		char* buffer = new char[bufSize+1];
		std::string pipeContents;
#else
		char* buffer = rage_new char[bufSize+1];
		atString pipeContents;
#endif
		bSuccess = FALSE;
		ZeroMemory(buffer, bufSize);


		hashDebug("ExecCmd: Looping ReadFile of Pipe");


		do
		{
			hashDebug("ExecCmd: Calling ReadFile");
			bSuccess = ReadFile(stdOutRead, buffer, bufSize, &bytesRead, NULL);

			hashDebug("ExecCmd: Read: %s", buffer);

			bSuccess &= (bytesRead != 0);
			buffer[min(bufSize, bytesRead)] = '\0';
			if (bSuccess)
				pipeContents += buffer;
		}
		while (bSuccess);

		hashDebug("ExecCmd: Closing Pipe Handle");

		delete [] buffer;
		CloseHandle(stdOutRead);
		//@@: } IMACHINEHASHSTRATEGY_EXECCMD 
		return pipeContents;
	}

public:

	virtual ~IMachineHashStrategy() { }
	virtual void GetMachineHash(void*) = 0;
};


class SMBIOSMachineHashStrategy : public IMachineHashStrategy
{
public:
	virtual ~SMBIOSMachineHashStrategy() { }

	virtual void GetMachineHash(void* outHash)
	{
		//@@: location SMBIOS_GETMACHINEHASH_ENTRY
#if RSG_LAUNCHER
		std::vector<unsigned char> accum;
#else
		atArray<unsigned char> accum;
#endif
		const DWORD provider = 'RSMB';
		
		//@@: range SMBIOS_GETMACHINEHASH {

		hashDebug("SMBIOS: GetSystemFirmwareTable");

		UINT tableSize = GetSystemFirmwareTable(provider, NULL, NULL, 0);
#if RSG_LAUNCHER
		BYTE* pTable = new BYTE[tableSize];
#else
		BYTE* pTable = rage_new BYTE[tableSize];
#endif
		
		ZeroMemory(pTable, tableSize);
		//@@: location SMBIOS_GETMACHINEHASH_LOCATION_A

		hashDebug("SMBIOS: GetSystemFirmwareTable - Again");

		if (GetSystemFirmwareTable(provider, NULL, pTable, tableSize))
		{

			hashDebug("SMBIOS: GetSystemFirmwareTable - Accumulating Data");

			RawSMBIOSData* pData = reinterpret_cast<RawSMBIOSData *>(pTable);
			accum.insert(accum.end(), (unsigned char *)pData->SMBIOSTableData, (unsigned char *)&(pData->SMBIOSTableData[pData->Length]));
		}
		else
		{
			return;
		}
		//@@: location SMBIOS_GETMACHINEHASH_LOCATION_B
		delete [] pTable;
		//@@: } SMBIOS_GETMACHINEHASH

		hashDebug("SMBIOS: SHA'ing Results");

		SHAHashHelper(&accum[0], (unsigned int)accum.size(), (char*)outHash);

	}

	typedef struct RawSMBIOSData
	{
		BYTE Used20CallingMethod;
		BYTE SMBIOSMajorVersion;
		BYTE SMBIOSMinorVersion;
		BYTE DmiRevision;
		DWORD Length;
		BYTE SMBIOSTableData[1];
	} RawSMBiosData;
};


class Win32MachineHashStrategy : public IMachineHashStrategy
{
public:
	explicit Win32MachineHashStrategy()
	{
	}
	virtual ~Win32MachineHashStrategy() { }
	virtual void GetMachineHash(void* outHash)
	{
		// CPU and System SKU info:
		//@@: location W32_GETMACHINEHASH_GET_PROCESSOR
#if RSG_LAUNCHER
		std::vector<unsigned char> accum;
#else
		atArray<unsigned char> accum;
#endif
		
		hashDebug("Win32: ");
		const CHAR* valName = "ProcessorNameString";
		const CHAR* key64 = "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0";

		hashDebug("Win32: Accumulating Registry String value %s:%s", key64, valName);
		if (!AccumulateRegistryStrValue(accum, key64, valName))
			return;
		//@@: range W32_GETMATCHINEHASH {
		
		key64 = "HARDWARE\\DESCRIPTION\\System\\BIOS";
		valName = "SystemManufacturer";
		//@@: location W32_GETMACHINEHASH_LOCATION_A
		hashDebug("Win32: Accumulating Registry String value %s:%s", key64, valName);
		if (!AccumulateRegistryStrValue(accum, key64, valName))
			return;

		key64 = "HARDWARE\\DESCRIPTION\\system\\BIOS";
		valName = "SystemProductName";
		hashDebug("Win32: Accumulating Registry String value %s:%s", key64, valName);
		if (!AccumulateRegistryStrValue(accum, key64, valName))
			return;

		key64 = "HARDWARE\\DESCRIPTION\\system\\BIOS";
		valName = "BaseBoardManufacturer";
		hashDebug("Win32: Accumulating Registry String value %s:%s", key64, valName);
		if (!AccumulateRegistryStrValue(accum, key64, valName))
			return;

		// Username:
#if RSG_LAUNCHER
		hashDebug("Win32: Executing whoami.exe");
		std::string whoami = ExecCmd("whoami.exe");
		//@@: location W32_GETMACHINEHASH_LOCATION_B
		if (!whoami.empty())
			accum.insert(accum.end(), whoami.begin(), whoami.end());
		else
			return;

		// Network info: hostname 

		hashDebug("Win32: Executing hostname.exe");
		std::string hostname = ExecCmd("hostname.exe");
		//@@: location W32_GETMACHINEHASH_LOCATION_C
		if (!hostname.empty())
			accum.insert(accum.end(), hostname.begin(), hostname.end());
		else
			return;
#else

		hashDebug("Win32: Executing whoami.exe");
		atString whoami = ExecCmd("whoami.exe");
		if (whoami.GetLength() != 0)
			accum.insert(accum.end(), whoami.c_str(), whoami.c_str() + whoami.GetLength());
		else
			return;

		// Network info: hostname 
		hashDebug("Win32: Executing hostname.exe");
		atString hostname = ExecCmd("hostname.exe");

		if (hostname.GetLength() != 0)
			accum.insert(accum.end(), hostname.c_str(), hostname.c_str() + hostname.GetLength());
		else
			return;
#endif

		// ARS-2014-09-18
		// There's something up with the game where trying to include
		// the necessary files doesn't define the NTDID global variables
		// that are needed for compilation. I tried Googling the answer(s)
		// but due to time constraints and difficulty, didn't think it made the
		// most sense to keep down that bunny trail.
		//
		// Network info: Mac addresses
		// ULONG adapterInfoSize = sizeof(IP_ADAPTER_INFO);
		// IP_ADAPTER_INFO* pAdapterInfo = (IP_ADAPTER_INFO *)new rgsc::u8[sizeof(IP_ADAPTER_INFO)];
		// ULONG res = ERROR_SUCCESS;
		// 
		// if ((res = GetAdaptersInfo(pAdapterInfo, &adapterInfoSize)) == ERROR_BUFFER_OVERFLOW)
		// {
		// 	delete [] reinterpret_cast<rgsc::u8*>(pAdapterInfo);
		// 	pAdapterInfo = (IP_ADAPTER_INFO *)new rgsc::u8[adapterInfoSize];
		// }
		// 
		// if ((res = GetAdaptersInfo(pAdapterInfo, &adapterInfoSize)) == ERROR_SUCCESS)
		// {
		// 	IP_ADAPTER_INFO* it = pAdapterInfo;
		// 	while (it)
		// 	{
		// 		if (it->Type == MIB_IF_TYPE_ETHERNET
		// 			|| it->Type == IF_TYPE_IEEE80211)
		// 		{
		// 			accum.insert(accum.end(), it->Address, &it->Address[it->AddressLength]);
		// 		}
		// 		it = it->Next;
		// 	}
		// }
		// else
		// 	return;
		//@@: } W32_GETMATCHINEHASH 

		hashDebug("Win32: SHA'ing Results");

		SHAHashHelper(&accum[0], (unsigned int)accum.size(), (char*)outHash);
	
	}

};


class FallbackMachineHashStrategy : public IMachineHashStrategy
{
public:
	FallbackMachineHashStrategy(const rgsc::RockstarId& rockstarId) : m_RockstarId(rockstarId) { }
	virtual ~FallbackMachineHashStrategy() { }
	virtual void GetMachineHash(void* outHash)
	{

#if RSG_LAUNCHER
		std::vector<rgsc::s64> cpucaps;
#else
		atArray<s64> cpucaps;
#endif

		int CPUInfo[4] = { -1 };
		hashDebug("Fallback: Calling CPUID");
		//@@: range FALLBACKMACHINEHASHSTRATEGY_GETMACHINEHASH {
		__cpuid(CPUInfo, 0);
		unsigned int nIds = CPUInfo[0];
		//@@: location FALLBACKMACHINEHASHSTRATEGY_GETMACHINEHASH_CALL_CPUID
		for (unsigned int i = 0; i <= nIds; ++i)
		{
			__cpuid(CPUInfo, i);
			for (int j = 0; j < 4; ++j)
			{
				if (i == 1 && j == 1)
				{
					// Masking off the APIC-ID, which can return incorrect results
					// depending on the CPU id
					CPUInfo[j] &= 0xFFFFFF;
				}
				if (i == 11 && j == 3)
				{
					// Masking off the APIC-ID, which can return incorrect results
					// depending on the CPU id
					CPUInfo[j] &= 0;
				}
				cpucaps.PushAndGrow(CPUInfo[j]);
			}
		}
		//@@: location FALLBACKMACHINEHASHSTRATEGY_GETMACHINEHASH_CHECK_SIZE

		if (cpucaps.size() == 0)
		{
			return;
		}
#if RSG_LAUNCHER
		cpucaps.push_back(m_RockstarId);
#else
		cpucaps.PushAndGrow(m_RockstarId);
#endif

		//@@: } FALLBACKMACHINEHASHSTRATEGY_GETMACHINEHASH
		hashDebug("Fallback: SHA'ing Results");
		SHAHashHelper((unsigned char *)&cpucaps[0], (unsigned int)(cpucaps.size() * sizeof(rgsc::s64)), (char*)outHash);
	}
private:
	rgsc::RockstarId m_RockstarId;
};


class WMIMachineHashStrategy : public IMachineHashStrategy
{
	template <class T>
	class AutoreleasePool
	{
	public:
		~AutoreleasePool()
		{
			for (auto p : m_pool)
			{
				p->Release();
			}
		}
		void AddObject(T* pObj)
		{
#if RSG_LAUNCHER
			if (std::find(m_pool.begin(), m_pool.end(), pObj) == m_pool.end())
				m_pool.push_back(pObj);
#else
			if(m_pool.Find(pObj) == -1)
				m_pool.PushAndGrow(pObj);
#endif
		}
	private:
#if RSG_LAUNCHER
		std::vector<T*> m_pool;
#else
		atArray<T*> m_pool;
#endif
	};

public:
	virtual ~WMIMachineHashStrategy() { }
	virtual void GetMachineHash(void* outHash)
	{
		//@@: location WMI_GETMACHINEHASH_ENTRY
#if RSG_LAUNCHER
		std::vector<unsigned char> accum;
#else
		atArray<unsigned char> accum;	
#endif
		
		CHAR volumeName[MAX_PATH+1] = {0};
		CHAR fileSystemName[MAX_PATH+1] = {0};
		DWORD serialNumber, maxComponentLen, fileSystemFlags;

		
		CHAR systemDrive[DRIVE_PATH_SIZE] = {0};
		//@@: range WMI_GETMACHINEHASH {
		hashDebug("WMI: Getting Environment Variable");
		GetEnvironmentVariableA("SystemDrive", systemDrive, DRIVE_PATH_SIZE);
		strcat(systemDrive,"\\");
		hashDebug("WMI: Getting volume information");
		if (GetVolumeInformationA(systemDrive, volumeName, MAX_PATH, &serialNumber, &maxComponentLen, &fileSystemFlags, fileSystemName, MAX_PATH))
		{

			hashDebug("WMI: Inserting volume information into accumulator");
			size_t len = strlen(volumeName);
			if(len>0)
				accum.insert(accum.end(), (char*)volumeName, (char*)&volumeName[len]);
			len = strlen(fileSystemName);
			if(len>0)
				accum.insert(accum.end(), (char*)fileSystemName, (char*)&fileSystemName[len]);

			accum.insert(accum.end(), (char*)&serialNumber, (char*)&serialNumber + sizeof(serialNumber));
			accum.insert(accum.end(), (char*)&maxComponentLen, (char*)&maxComponentLen + sizeof(maxComponentLen));
			accum.insert(accum.end(), (char*)&fileSystemFlags, (char*)&fileSystemFlags + sizeof(fileSystemFlags));

		}
		else
			return;

		HRESULT hr;

		// Grab various other data
		//@@: location WMI_GETMACHINEHASH_LOCATION_A
		hashDebug("WMI: CoInitializeEx");
		if (SUCCEEDED(hr = CoInitializeEx(NULL, COINIT_MULTITHREADED)))
		{
			hashDebug("WMI: CoInitializeSecurity");
			hr = CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);

			if (m_bInitializedComSecurity ||  SUCCEEDED(hr) || hr == RPC_E_TOO_LATE )
			{
				m_bInitializedComSecurity = true;

				IWbemLocator* pLocator = NULL;
				hashDebug("WMI: CoCreateInstance");
				if (SUCCEEDED(hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&pLocator)))
				{

					AutoreleasePool<IWbemLocator> locatorPool;
					locatorPool.AddObject(pLocator);

					IWbemServices* pServices = NULL;
					++RAGE_LOG_DISABLE;
					bstr_t connectServer(L"ROOT\\CIMV2");
					--RAGE_LOG_DISABLE;
					hashDebug("WMI: ConnectServer");
					if (SUCCEEDED(hr = pLocator->ConnectServer(connectServer, NULL, NULL, 0, NULL, 0, 0, &pServices)))
					{
						AutoreleasePool<IWbemServices> servicesPool;
						servicesPool.AddObject(pServices);
						hashDebug("WMI: CoSetProxyBlanket");
						if (SUCCEEDED(hr = CoSetProxyBlanket(pServices, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE)))
						{
							struct ComponentPair
							{
								const char* className;
								const wchar_t* propertyName;
							};
							//@@: location WMI_GETMACHINEHASH_LOCATION_B
							const ComponentPair components[] =
							{
								{ "Win32_DiskDrive", L"SerialNumber" },
								//{ "Win32_BIOS", L"IdentificationCode" },
								{ "Win32_OperatingSystem", L"Caption" },
								{ "Win32_BaseBoard", L"Product" },
								{ "Win32_VideoController  ", L"PNPDeviceID" },
								{ "Win32_IDEController", L"DeviceID" },
								{ "Win32_Processor", L"Name" }
							};
							hashDebug("WMI: Iterating over components");
							for (size_t i = 0; i < ARRAYSIZE(components); i++)
							{
								hashDebug("WMI: Iterating over %s", components[i].className);
								IEnumWbemClassObject* pEnumerator = NULL;
								++RAGE_LOG_DISABLE;
								bstr_t query("SELECT * FROM ");
								bstr_t wql("WQL");
								query += components[i].className;	
								--RAGE_LOG_DISABLE;
								//@@: location WMI_GETMACHINEHASH_EXEC_QUERY_FOR_SERVICES
								hashDebug("WMI: ExecQuery");
								if (SUCCEEDED(pServices->ExecQuery(wql, query, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumerator)))
								{
									IWbemClassObject* pClassObj = NULL;
									ULONG uReturn = 0;
									hashDebug("WMI: While Loop Over enumerated results");
									while (pEnumerator && SUCCEEDED(pEnumerator->Next(WBEM_INFINITE, 1, &pClassObj, &uReturn)) && uReturn != 0 && pClassObj)
									{
										VARIANT variant;
										ZeroMemory(&variant, sizeof(VARIANT));
										if (SUCCEEDED(pClassObj->Get(components[i].propertyName, 0, &variant, NULL, NULL)))
										{
											LPWSTR v = variant.bstrVal;
											if (v)
											{
												accum.insert(accum.end(), (rgsc::u8*)v, (rgsc::u8*)v + wcslen(v));
#if !RSG_FINAL
												char buffer[256] = {0};
												wcstombs(buffer, v, 256);
												hashDebug("WMI: Adding [%s]", buffer);
#endif
											}
										}

										pClassObj->Release();
									}

									pEnumerator->Release();
								}
								else
								{
									CoUninitialize();
									return;
								}
							}
						}
						else
						{
							CoUninitialize();
							return;
						}
					}
					else
					{
						CoUninitialize();
						return;
					}
				}
				else
				{
					CoUninitialize();
					return;
				}
			}
			else
			{
				CoUninitialize();
				return;
			}

			CoUninitialize();
		}
		else
		{
			return;
		}

		// Add the SMBIOS UUID, obtainable through the wmic util:


		//@@: location WMI_GETMACHINEHASH_LOCATION_C
#if RSG_LAUNCHER
		hashDebug("WMI: Calling ExecCmd wmic");
		std::string wmic = ExecCmd("wbem\\wmic.exe path win32_computersystemproduct get uuid");
		if (!wmic.empty())
			accum.insert(accum.end(), wmic.begin(), wmic.end());
		else
			return;
#else

		hashDebug("WMI: Calling ExecCmd wmic");

		atString wmic = ExecCmd("wbem\\wmic.exe path win32_computersystemproduct get uuid");

		if (wmic.GetLength() != 0)
			accum.insert(accum.end(), wmic.c_str(), wmic.c_str() + wmic.GetLength());
		else
			return;
#endif
		//@@: } WMI_GETMACHINEHASH
		hashDebug("WMI: SHA'ing Results");
		SHAHashHelper(&accum[0], (unsigned int)accum.size(), (char *)outHash);

	}

private:
	bool m_bInitializedComSecurity;
};



class CMachineHash
{
public:
	CMachineHash()
	{
		SecureGenInit();
	}

	static void Cleanup()
	{
	}

#if RSG_LAUNCHER
	static std::string ReplaceAll(std::string str, const std::string& search, const std::string& replace) 
	{
		size_t pos = 0;

		while((pos = str.find(search, pos)) != std::string::npos) 
		{
			str.replace(pos, search.length(), replace);
			pos += replace.length(); 
		}

		return str;
	}
#endif

	static void GetMachineHash(char * machineHash, rgsc::RockstarId rockstarId)
	{	
		
		// Pick a hashing strategy:
		// TODO: this will have to be updated in the future in order to accommodate our
		// plans to test all three different strategies before returning the user to the
		// login screen.

		//@@: range CMACHINEHASH_GETMACHINEHASH_SELECT_STRATEGY {
		IMachineHashStrategy* pHashStrategy = NULL;

#if RSG_LAUNCHER
        CHAR* username  = new CHAR[unamelen];
        CHAR* envval    = new CHAR[envlen];
		memset(username, 0, unamelen);
		memset(envval, 0, envlen);
#endif

#if HASH_TEST_SEL
		RAND_screen();
		RAND_bytes((unsigned char*)buff, unamelen+envlen);
#endif

		hashDebug("Jenkins Selecting");		
		unsigned i;
		unsigned int switchHash;
		u8* identifierIdx = (u8*)&rockstarId;
		// Use a jenkins hash to get our 32 bits, instead of using the expensive SHA
		// and then truncating it downards trying to get the high DWORD
		//@@: range CMACHINEHASH_GETMACHINEHASH_PICKSTRATEGYWORKER_WORKERPROC_JENKINS {
		for(switchHash = i = 0; i < sizeof(rockstarId); ++i)
		{
			switchHash += identifierIdx[i];
			switchHash += (switchHash << 10);
			switchHash ^= (switchHash >> 6);
		}
		//@@: location CMACHINEHASH_GETMACHINEHASH_PICKSTRATEGYWORKER_WORKERPROC_JENKINS_SHIFTING
		switchHash += (switchHash << 3);
		switchHash ^= (switchHash >> 11);
		switchHash += (switchHash << 15);
#if HASH_TEST_WMI
		pHashStrategy = new WMIMachineHashStrategy();
#elif HASH_TEST_W32
		pHashStrategy = new Win32MachineHashStrategy();
#elif HASH_TEST_SMB
		pHashStrategy = new SMBIOSMachineHashStrategy();
#else

#if RSG_LAUNCHER
		if (switchHash < 0xFFFFFFFF / 3)
		{

			hashDebug("Selected Win32");	
			pHashStrategy = new Win32MachineHashStrategy();
		}
		else if (switchHash < 0xFFFFFFFF / 3 * 2)
		{
			hashDebug("Selected SMBIOS");	
			pHashStrategy = new SMBIOSMachineHashStrategy();
		}
		else /* high < 0xFFFFFFFF */
		{
			hashDebug("Selected WMI");		
			pHashStrategy = new WMIMachineHashStrategy();
		}
#else
		if (switchHash < 0xFFFFFFFF / 3)
		{
			hashDebug("Selected Win32");	
			pHashStrategy = rage_new Win32MachineHashStrategy();
		}
		else if (switchHash < 0xFFFFFFFF / 3 * 2)
		{
			hashDebug("Selected SMBIOS");	
			pHashStrategy = rage_new SMBIOSMachineHashStrategy();
		}
		else /* high < 0xFFFFFFFF */
		{
			hashDebug("Selected WMI");	
			pHashStrategy = rage_new WMIMachineHashStrategy();
		}
#endif

#endif
#if RSG_LAUNCHER
		memset(username, 0, unamelen);
		memset(envval, 0, envlen);
		memset(buff, 0, unamelen+envlen);
#endif
		//@@: } CMACHINEHASH_GETMACHINEHASH_PICKSTRATEGYWORKER_WORKERPROC_JENKINS


		
		if (!pHashStrategy)
		{
			hashDebug("Failed to create one, defaulting to SMBIOS");
#if RSG_LAUNCHER
			pHashStrategy = new SMBIOSMachineHashStrategy();
#else
			pHashStrategy = rage_new SMBIOSMachineHashStrategy();
#endif
		}

		//@@: } CMACHINEHASH_GETMACHINEHASH_SELECT_STRATEGY 
				

		//@@: location CMACHINEHASH_GETMACHINEHASH_SELECT_STRATEGY_EXIT_POINT
				
			
		// Apply the fallback first, so we don't have to return
		// an error/success condition from other strategies
					
		//@@: location CMACHINEHASH_MACHINEHASHWORKER_WORKERPROC_CALL_FALLBACK

		hashDebug("Creating Fallback");
#if RSG_LAUNCHER
		FallbackMachineHashStrategy *fallback = new FallbackMachineHashStrategy(rockstarId);
#else
		FallbackMachineHashStrategy *fallback = rage_new FallbackMachineHashStrategy(rockstarId);
#endif
		//@@: range CMACHINEHASH_MACHINEHASHWORKER_WORKERPROC {
		//@@: location CMACHINEHASH_MACHINEHASHWORKER_WORKERPROC_CALL_FALLBACK_MACHINE_HASH

		hashDebug("Calling Fallback Machine Hash");
		fallback->GetMachineHash(machineHash);
#if !HASH_TEST_FLB
		// Attempt to compute actual hash:
		//@@: location CMACHINEHASH_MACHINEHASHWORKER_WORKERPROC_CALL_MACHINE_HASH
		hashDebug("Calling Legit Machine Hash");

		pHashStrategy->GetMachineHash(machineHash);
		//@@: } CMACHINEHASH_MACHINEHASHWORKER_WORKERPROC 

		//@@: location WORKERPROC_DELETE_HASH_STRATEGY
		delete pHashStrategy;
#endif

	}

private:
#if RSG_LAUNCHER
#else
	static WC_RNG *m_random;
#endif


	static bool SecureGenInit()
	{
		// TODO: consider using RAND_event() instead.
		// ItrRequires Win32 message processing but is usually more entropic.
#if RSG_LAUNCHER
		RAND_screen();
#else
		m_random = rage_new WC_RNG();
		wc_InitRng(m_random);
#endif
		return true;
	}

	static bool SecureGenRandom(unsigned char* pBuffer, DWORD bufferLen)
	{
#if RSG_LAUNCHER
		return RAND_bytes(pBuffer, bufferLen) == 1;
#else
		if (!pBuffer || bufferLen == 0)
		{
			return false;
		}
		return (wc_RNG_GenerateBlock(m_random, pBuffer, bufferLen) == 0);
#endif
	}


};

#ifndef RSG_LAUNCHER
}
#endif

#endif
#endif // RSG_PC
#endif
