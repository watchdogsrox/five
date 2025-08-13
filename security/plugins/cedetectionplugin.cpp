#include "security/ragesecengine.h"

#if USE_RAGESEC
#if RAGE_SEC_TASK_CE_DETECTION

#define CE_DETECTION_POLLING_MS				60 * 1000
#define CE_DETECTION_ID						0xF207FC78 
#define CE_WINDOWS_TICK						10000000
#define CE_PROCESS_CHECKER_MAX_PROCESSES	1024
#define CE_PROCESS_CHECKER_MAX_MODULES		1024

#define STATUS_INFO_LENGTH_MISMATCH			0xC0000004

#include "cedetectionplugin.h"

// rage includes
#include "security/papi/papi.h"
#include "string/stringutil.h"

// Windows includes
#include <Shlobj.h>

using namespace papi;
using namespace rage;

static unsigned int sm_ceTrashValue	= 0;

#if !__FINAL
bool IS_ADDRESS_IN_RANGE(u64 address, u64 low, u64 high)
{
	if (address >= low && address <= high)
	{
		return true;
	}
	return false;
}
#else
#define IS_ADDRESS_IN_RANGE(address,low,high) (	\
	(											\
	(address >= low)	&&						\
	(address <= high)							\
	)											\
	)	
#endif

#if CE_DETECTION_ENABLE_WINDOW_NAME_FINDER
bool WindowNameFinder::Find()
{
	// Invoke the callback for each window
	BOOL result = EnumWindows(ProcessWindow, (LPARAM)m_name);
	if (result == 0)
	{
		RageSecPluginGameReactionObject obj(REACT_GAMESERVER, CE_DETECTION_ID, 0x3A8F10B0, 0x35E821BF, 0xD148F4A3);
		rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
		return true;
	}
	else
	{
		return false;
	}
}

BOOL CALLBACK WindowNameFinder::ProcessWindow(HWND hwnd, LPARAM lParam)
{
	const u32 length = GetWindowTextLength(hwnd);
	const u32 bufferLength = length + 1;
	char* buffer = nullptr;
	const char* textToFind = (const char*)lParam;
	atString currWindowText;

	// Check if the window does not have any text, or if the function failed
	if (length == 0)
		return true;

	// Allocate a buffer to store the window text
	buffer = rage_new char[bufferLength];
	RtlSecureZeroMemory(buffer, bufferLength);

	// Check if data was written to the buffer
	if (GetWindowText(hwnd, buffer, bufferLength) == 0)
	{
		delete[] buffer;
		return true;
	}

	currWindowText = buffer;
	delete[] buffer;

	if (currWindowText.StartsWith(textToFind))
		return 0;

	return 1;
}
#endif //CE_DETECTION_ENABLE_WINDOW_NAME_FINDER

#if CE_DETECTION_ENABLE_METADATA_CHECKER
static sysObfuscated<u64> sm_cePluginStartTime(CEMetadataChecker::GetSystemTime());
static sysObfuscated<u64> sm_cePluginImageLow(0);
static sysObfuscated<u64> sm_cePluginImageHigh(0);

bool CEMetadataChecker::Check()
{
	RetrieveAddressFilePaths();

	if (!CheckAddressFiles())
		return false;

	return true;
}

bool CEMetadataChecker::IsAddressFile(const atString& name) const
{
	char filename[] = {'A', 'D', 'D', 'R', 'E', 'S', 'S', 'E', 'S', '.', 'T', 'M', 'P', '\0' };
	if (StringIsEqual(name, filename))
		return true;

	return false;
}

void CEMetadataChecker::RetrieveAddressFilePaths()
{
	const atString dataPath = GetCheatEngineDataPath();
	atVector<atString> subdirectories;
	RetrieveSubdirectories(dataPath, subdirectories);

	for (const atString& subdir : subdirectories)
		RetrieveFiles(subdir);
}

atString CEMetadataChecker::GetCheatEngineDataPath() const
{
	atString path;

	HKEY hKey;
	if (GetSoftwareKey(hKey))
	{
		DWORD value;
		if (GetUsingTempDirectory(hKey, value))
		{
			if (IsUsingCustomDirectory(value))
			{
				if (GetCustomDataDirectory(hKey, path))
				{
					return path;
				}
			}
		}
	}

	GetDefaultDataDirectory(path);
	return path;
}

atString CEMetadataChecker::MakeChildPath(const char* basePath, const char* child) const
{
	const char pathSeparator[] = { '\\', '\0' };
	atString result(basePath);
	result += pathSeparator;
	result += child;
	return result;
}

void CEMetadataChecker::RetrieveSubdirectories(const atString& parent, atVector<atString>& directories)
{
	if (StringNullOrEmpty(parent))
		return;

	fiFindData findData;
	const fiDevice& device = fiDeviceLocal::GetInstance();
	const fiHandle handle = device.FindFileBegin(parent, findData);

	if (IsInvalidFileHandle(handle))
		return;

	do
	{
		if (HasDirectoryAttribute(findData))
		{
			atString name(findData.m_Name);
			atString subdir;

			// Skip implicit directories included by Windows 
			if (IsCurrentDirectory(name))
				continue;
			if (IsParentDirectory(name))
				continue;

			subdir = MakeChildPath(parent, name);
			directories.PushAndGrow(subdir);
		}
	} while (device.FindFileNext(handle, findData));


	device.FindFileEnd(handle);
}

void CEMetadataChecker::RetrieveFiles(const atString& directory)
{
	if (StringNullOrEmpty(directory))
		return;

	fiFindData findData;
	const fiDevice& device = fiDeviceLocal::GetInstance();
	const fiHandle handle = device.FindFileBegin(directory, findData);

	if (IsInvalidFileHandle(handle))
		return;

	do
	{
		if (!HasDirectoryAttribute(findData))
		{
			atString name(findData.m_Name);
			if (!IsAddressFile(name))
				continue;

			if (findData.m_LastWriteTime < sm_cePluginStartTime)
				continue;

			atString file = MakeChildPath(directory, name);
			m_files.PushAndGrow(file);
		}
	} while (device.FindFileNext(handle, findData));

	device.FindFileEnd(handle);
}

bool CEMetadataChecker::CloseFileHandle(const fiDevice& device, const fiHandle& handle) const
{
	return device.FindFileEnd(handle) == 0;
}

bool CEMetadataChecker::IsInvalidFileHandle(const fiHandle& handle) const
{
	return handle == fiHandleInvalid;
}

bool CEMetadataChecker::IsCurrentDirectory(const char* path) const
{
	const char currentDirectoryIdentifier[] = { '.', '\0' };
	return StringIsEqual(path, currentDirectoryIdentifier);
}

bool CEMetadataChecker::IsParentDirectory(const char* path) const
{
	const char parentDirectoryIdentifier[] = { '.', '.', '\0' };
	return StringIsEqual(path, parentDirectoryIdentifier);
}

bool CEMetadataChecker::HasDirectoryAttribute(const fiFindData& findData) const
{
	return (findData.m_Attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

u64 CEMetadataChecker::GetSystemTime()
{
	FILETIME ft;
	GetSystemTimeAsFileTime(&ft);
	return ((u64)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
}

bool CEMetadataChecker::GetSoftwareKey(HKEY& hKey) const
{
	const MODULE_ID ADVAPI32_MODULE_ID = HashString("Advapi32.dll").GetHash();
	const FUNCTION_ID REGOPENKEYEXA_FUNCTION_ID = HashString("RegOpenKeyExA").GetHash();
	const RegOpenKeyExAFn regOpenKeyExA = (RegOpenKeyExAFn)GetFunctionAddressByHash(
		ADVAPI32_MODULE_ID, REGOPENKEYEXA_FUNCTION_ID);

	char softwareCheatEngine[] = { 'S', 'O', 'F', 'T', 'W', 'A', 'R', 'E', '\\', 'C', 'h', 'e',
		'a', 't', ' ', 'E', 'n', 'g', 'i', 'n', 'e', '\0' };
	return SUCCEEDED(regOpenKeyExA(HKEY_CURRENT_USER, softwareCheatEngine, 0, KEY_READ, &hKey));
}

bool CEMetadataChecker::GetUsingTempDirectory(const HKEY& inKey, DWORD& outValue) const
{
	const MODULE_ID ADVAPI32_MODULE_ID = HashString("Advapi32.dll").GetHash();
	const FUNCTION_ID REGQUERYVALUEEXA_FUNCTION_ID = HashString("RegQueryValueExA").GetHash();
	const RegQueryValueExAFn regQueryValueExA = (RegQueryValueExAFn)GetFunctionAddressByHash(
		ADVAPI32_MODULE_ID, REGQUERYVALUEEXA_FUNCTION_ID);

	DWORD dwData;
	DWORD cbData = sizeof(DWORD);
	// Don't use tempdir
	const char szValueName[] = { 'D', 'o', 'n', '\'', 't', ' ', 'u', 's', 'e', ' ', 't', 'e', 'm',
		'p', 'd', 'i', 'r', '\0' };

	if (SUCCEEDED(regQueryValueExA(inKey, szValueName, NULL, NULL, (LPBYTE)&dwData, &cbData)))
	{
		outValue = dwData;
		return true;
	}

	return false;
}

bool CEMetadataChecker::GetCustomDataDirectory(const HKEY& inKey, atString& outValue) const
{
	const MODULE_ID ADVAPI32_MODULE_ID = HashString("Advapi32.dll").GetHash();
	const FUNCTION_ID REGQUERYVALUEEXA_FUNCTION_ID = HashString("RegQueryValueExA").GetHash();
	const RegQueryValueExAFn regQueryValueExA = (RegQueryValueExAFn)GetFunctionAddressByHash(
		ADVAPI32_MODULE_ID, REGQUERYVALUEEXA_FUNCTION_ID);

	// Scanfolder
	char relativePath[] = { 'C', 'h', 'e', 'a', 't', ' ', 'E', 'n', 'g', 'i', 'n', 'e', '\0' };
	char separator[] = { '\\', '\0' };
	char szValueName[] = { 'S', 'c', 'a', 'n', 'f', 'o', 'l', 'd', 'e', 'r', '\0' };
	char szValue[4096] = { 0 };
	DWORD cbData = sizeof(szValue);

	if (!SUCCEEDED(regQueryValueExA(inKey, szValueName, NULL, NULL, (LPBYTE)&szValue, &cbData)))
		return false;

	outValue = szValue;
	if (!StringEndsWith(outValue, separator))
		outValue += separator;

	outValue += relativePath;
	return true;
}

bool CEMetadataChecker::GetDefaultDataDirectory(atString& outPath) const
{
	char appDataRoot[MAX_PATH] = { '\0' };
	char relativePath[] = { '\\', 'T', 'e', 'm', 'p', '\\', 'C', 'h', 'e', 'a', 't', ' ', 'E', 'n',
		'g', 'i', 'n', 'e', '\0' };

	const MODULE_ID SHELL32_MODULE_ID = HashString("Shell32.dll").GetHash();
	const FUNCTION_ID SHGETFOLDERPATHA_FUNCTION_ID = HashString("SHGetFolderPathA").GetHash();
	const SHGetFolderPathAFn shGetFolderPathA = (SHGetFolderPathAFn)GetFunctionAddressByHash(
		SHELL32_MODULE_ID, SHGETFOLDERPATHA_FUNCTION_ID);

	if (!SUCCEEDED(shGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, appDataRoot)))
		return false;

	outPath = appDataRoot;
	outPath += relativePath;
	return true;
}

bool CEMetadataChecker::IsUsingCustomDirectory(DWORD value) const
{
	return value != 0;
}

bool CEMetadataChecker::CheckAddressFiles() const
{
	for (const atString& file : m_files)
	{
		if (!CheckAddressFile(file))
			return false;
	}

	return true;
}

bool CEMetadataChecker::CheckAddressFile(const atString& path) const
{
	bool checkFailed = false;

	const fiDevice& device = fiDeviceLocal::GetInstance();
	fiHandle handle = device.Open(path, true);

	u8* buffer = NULL;
	const u32 bufferOffset = ADDRESS_FILE_START_OFFSET;
	const u32 bufferStep = sizeof(u64);

	if (IsInvalidFileHandle(handle))
		return true;

	const u64 fileSize = device.Size64(handle);
	if (fileSize == 0)
	{
		device.Close(handle);
		return true;
	}

	const u64 bufferSize = fileSize > ADDRESS_FILE_MAX_BUFFER ? ADDRESS_FILE_MAX_BUFFER : fileSize;
	buffer = rage_new u8[bufferSize];
	memset(buffer, 0, bufferSize);

	if (device.Read(handle, buffer, (int)bufferSize) >= 0)
	{
		u8* cursor = buffer + bufferOffset;
		for (u32 i = 0; i < bufferSize; i += bufferStep)
		{
			u64 address = *reinterpret_cast<u64*>(cursor);
			if (address == 0)
				break;

			if (IsInGameImageRegion(address))
			{
				gnetDebug1("Cheat Engine address table detection - Game Region");
				checkFailed = true;
				RageSecPluginGameReactionObject obj(REACT_GAMESERVER, CE_DETECTION_ID, 0xED40E0B9, 0x05C5B3EB, 0xF5E8C851);
				rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
				break;
			}
			else if (IsInScriptGlobalsRegion(address))
			{
				gnetDebug1("Cheat Engine address table detection - Script Globals Region");
				checkFailed = true;
				RageSecPluginGameReactionObject obj(REACT_GAMESERVER, CE_DETECTION_ID, 0xED40E0B9, 0x05C5B3EB, 0x2725BAE3);
				rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
				break;
			}

			cursor += bufferStep;
		}
	}

	if (buffer != NULL)
		delete[] buffer;

	device.Close(handle);

	return !checkFailed;
}

bool CEMetadataChecker::IsInGameImageRegion(const u64& address) const
{
	if (IS_ADDRESS_IN_RANGE(address, sm_cePluginImageLow, sm_cePluginImageHigh))
		return true;

	return false;
}

bool CEMetadataChecker::IsInScriptGlobalsRegion(const u64& address) const
{
	static const u32 maxBlocks = scrProgram::MAX_GLOBAL_BLOCKS;

	for (u32 blockIndex = 0; blockIndex < maxBlocks; blockIndex++)
	{
		const u64 blockSize = scrProgram::GetGlobalSize(blockIndex);
		if (blockSize == 0)
			continue;

		const scrValue* block = scrProgram::GetGlobals(blockIndex);
		if (block == NULL)
			continue;

		if (IS_ADDRESS_IN_RANGE(address, (u64)block, (u64)block + blockSize))
			return true;
	}

	return false;
}

void CEMetadataChecker::PopulateImageBoundaries()
{
	const MODULE_ID KERNEL32_MODULE_ID = HashString("Kernel32.dll").GetHash();
	const MODULE_ID PSAPI_MODULE_ID = HashString("Psapi.dll").GetHash();
	const FUNCTION_ID GETMODULEINFORMATION_FUNCTION_ID = HashString("GetModuleInformation").GetHash();
	const FUNCTION_ID GETCURRENTPROCESS_FUNCTION_ID = HashString("GetCurrentProcess").GetHash();
	const GetModuleInformationFn getModuleInformation = (GetModuleInformationFn)GetFunctionAddressByHash(
		PSAPI_MODULE_ID, GETMODULEINFORMATION_FUNCTION_ID);
	const GetCurrentProcessFn getCurrentProcess = (GetCurrentProcessFn)GetFunctionAddressByHash(
		KERNEL32_MODULE_ID, GETCURRENTPROCESS_FUNCTION_ID);

	HMODULE hMod = GetCurrentModule();
	MODULEINFO info;

	getModuleInformation(getCurrentProcess(), hMod, &info, sizeof(info));

	sm_cePluginImageLow = (u64)info.lpBaseOfDll;
	sm_cePluginImageHigh = sm_cePluginImageLow + info.SizeOfImage;
}
#endif //CE_DETECTION_ENABLE_METADATA_CHECKER

#if CE_DETECTION_ENABLE_PROCESS_CHECKER
bool CEProcessChecker::Check()
{
	CheckProcesses();
	return true;
}

void CEProcessChecker::CheckProcesses()
{
	const MODULE_ID PSAPI_MODULE_ID = HashString("PSAPI.DLL").GetHash();
	const FUNCTION_ID ENUMPROCESSES_FUNCTION_ID = HashString("EnumProcesses").GetHash();

	EnumProcessesFn enumProcesses = (EnumProcessesFn)GetFunctionAddressByHash(
		PSAPI_MODULE_ID, ENUMPROCESSES_FUNCTION_ID);

	static const u32 maxProcesses = CE_PROCESS_CHECKER_MAX_PROCESSES;
	DWORD processes[maxProcesses], cbNeeded;

	if (enumProcesses(processes, sizeof(processes), &cbNeeded))
	{
		static const u32 currentPid = GetCurrentProcessId();
		const u32 numProcesses = cbNeeded / sizeof(DWORD);

		for (u32 i = 0; i < numProcesses; i++)
		{
			u32 pid = processes[i];		
			if (HasHandleToCurrentProcess(pid))
			{
				return;
			}
		}
	}
}

bool CEProcessChecker::HasHandleToCurrentProcess(const u32 pid)
{
	const MODULE_ID NTDLL_MODULE_ID = HashString("ntdll.dll").GetHash();
	const FUNCTION_ID NTQUERYSYSTEMINFORMATION_FUNCTION_ID = HashString("NtQuerySystemInformation").GetHash();

	NtQuerySystemInformationFnPtr ntQuerySystemInformation = (NtQuerySystemInformationFnPtr)GetFunctionAddressByHash(
		NTDLL_MODULE_ID, NTQUERYSYSTEMINFORMATION_FUNCTION_ID);

	ULONG handleInfoSize = 0x10000;
	PSYSTEM_HANDLE_INFORMATION_EX handleInfo = (PSYSTEM_HANDLE_INFORMATION_EX)LocalAlloc(LPTR, handleInfoSize);

	// Allocate memory
	while (true)
	{
		NTSTATUS status = ntQuerySystemInformation(SYSTEM_INFORMATION_CLASS::SystemExtendedHandleInformation, handleInfo,
			handleInfoSize, NULL);

		// Failed
		if (status < 0)
		{
			// Free memory on failure
			LocalFree(handleInfo);

			if (status == STATUS_INFO_LENGTH_MISMATCH)
			{
				handleInfoSize += 0x8000;
				handleInfo = (PSYSTEM_HANDLE_INFORMATION_EX)LocalAlloc(LPTR, handleInfoSize);
			}
			else
			{
				return true;
			}
		}
		else
		{
			break;
		}	
	}

	LocalFree(handleInfo);
	return false;
}

void CEProcessChecker::GetFileNameForModule(HMODULE hMod, HANDLE hProcess, atString& outName)
{
	const MODULE_ID PSAPI_MODULE_ID = HashString("PSAPI.DLL").GetHash();
	const FUNCTION_ID GETMODULEFILENAMEEXA_FUNCTION_ID = HashString("GetModuleFileNameExA").GetHash();

	GetModuleFileNameExAFnPtr getModuleFileNameExA = (GetModuleFileNameExAFnPtr)GetFunctionAddressByHash(
		PSAPI_MODULE_ID, GETMODULEFILENAMEEXA_FUNCTION_ID);

	if (hProcess == NULL)
		return;
	if (hMod == NULL)
		return;

	char szModName[MAX_PATH];
	if (getModuleFileNameExA(hProcess, hMod, szModName, sizeof(szModName) / sizeof(char)) == 0)
		return;

	outName = szModName;
}
#endif //CE_DETECTION_ENABLE_PROCESS_CHECKER

bool CEDetectionPlugin_Create()
{
	return true;
}

bool CEDetectionPlugin_Configure()
{
	//@@: location CEDETECTIONPLUGIN_CONFIGURE_ENTRY
	CEMetadataChecker::PopulateImageBoundaries();
	return true;
}

bool CEDetectionPlugin_Destroy()
{
	return true;
}

bool CEDetectionPlugin_Work()
{
	//@@: location CEDETECTIONPLUGIN_WORK_ENTRY
	bool isMultiplayer = NetworkInterface::IsAnySessionActive() 
		&& NetworkInterface::IsGameInProgress();

	//@@: range CEDETECTIONPLUGIN_WORK_BODY {	
	if(isMultiplayer)
	{
#if CE_DETECTION_ENABLE_WINDOW_NAME_FINDER
		const char cheatEngineWindowName[] = { 'C', 'h', 'e', 'a', 't', ' ', 'E', 'n', 'g', 'i', 'n', 'e', '\0' };
		WindowNameFinder windowNameFinder(cheatEngineWindowName);
		windowNameFinder.Find();
#endif //CE_DETECTION_ENABLE_WINDOW_NAME_FINDER

#if CE_DETECTION_ENABLE_METADATA_CHECKER
		CEMetadataChecker ceMetadataChecker;
		ceMetadataChecker.Check();
#endif //CE_DETECTION_ENABLE_METADATA_CHECKER

#if CE_DETECTION_ENABLE_PROCESS_CHECKER
		CEProcessChecker processChecker;
		processChecker.Check();
#endif //CE_DETECTION_ENABLE_PROCESS_CHECKER

	}
	//@@: } CEDETECTIONPLUGIN_WORK_BODY
	return true;
}


bool CEDetectionPlugin_OnSuccess()
{
	gnetDebug3("0x%08X - OnSuccess", CE_DETECTION_ID);
	//@@: location CEDETECTIONPLUGIN_ONSUCCESS
	sm_ceTrashValue		    += sysTimer::GetSystemMsTime() - fwRandom::GetRandomNumber();
	return true;
}

bool CEDetectionPlugin_OnFailure()
{
	gnetDebug3("0x%08X - OnFailure", CE_DETECTION_ID);
	//@@: location CEDETECTIONPLUGIN_ONFAILURE
	sm_ceTrashValue		   *= sysTimer::GetSystemMsTime() - fwRandom::GetRandomNumber();
	return true;
}


bool CEDetectionPlugin_Init()
{
	RageSecPluginParameters rs;
	RageSecPluginExtendedInformation ei;

	ei.startTime		= sysTimer::GetSystemMsTime();
	ei.frequencyInMs	= CE_DETECTION_POLLING_MS;
	rs.version.major	= PLUGIN_VERSION_MAJOR;
	rs.version.minor	= PLUGIN_VERSION_MINOR;
	rs.type				= EXECUTE_PERIODIC;
	rs.extendedInfo		= ei;

	rs.Create			= &CEDetectionPlugin_Create;
	rs.Configure		= &CEDetectionPlugin_Configure;
	rs.Destroy			= &CEDetectionPlugin_Destroy;
	rs.DoWork			= &CEDetectionPlugin_Work;
	rs.OnSuccess		= &CEDetectionPlugin_OnSuccess;
	rs.OnCancel			= NULL;
	rs.OnFailure		= &CEDetectionPlugin_OnFailure;

	rageSecPluginManager::RegisterPluginFunction(&rs, CE_DETECTION_ID);

	return true;
}

#endif //RAGE_SEC_TASK_CE_DETECTION
#endif //USE_RAGESEC
