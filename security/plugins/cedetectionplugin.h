#ifndef SECURITY_CEDETECTIONPLUGIN_H
#define SECURITY_CEDETECTIONPLUGIN_H

#include "security/ragesecengine.h"
#include "system/xtl.h"

#if USE_RAGESEC
#if RAGE_SEC_TASK_CE_DETECTION

// TODO: Use the window name finder to gather a list of PIDs that can be Cheat Engine
// Use NtQuerySystemInformation to check if those PIDs have a handle to the game.

#define CE_DETECTION_ENABLE_METADATA_CHECKER	1
#define CE_DETECTION_ENABLE_PROCESS_CHECKER		0
#define CE_DETECTION_ENABLE_WINDOW_NAME_FINDER	0

#if CE_DETECTION_ENABLE_WINDOW_NAME_FINDER
class WindowNameFinder
{
public:
	WindowNameFinder(const char* name) : m_name(name) {};
	~WindowNameFinder() {};

	bool Find();

private:
	static BOOL CALLBACK ProcessWindow(HWND hwnd, LPARAM lParam);

	const char* m_name;
};
#endif //CE_DETECTION_ENABLE_WINDOW_NAME_FINDER

#if CE_DETECTION_ENABLE_METADATA_CHECKER
class CEMetadataChecker
{
public:
	CEMetadataChecker() {};
	~CEMetadataChecker() {};

	bool Check();

	__forceinline static u64 GetSystemTime();
	static void PopulateImageBoundaries();

private:
	atString GetCheatEngineDataPath() const;
	atString MakeChildPath(const char* basePath, const char* sub) const;

	__forceinline bool IsAddressFile(const atString& name) const;
	void RetrieveAddressFilePaths();
	void RetrieveSubdirectories(const atString& parent, atVector<atString>& directories);
	void RetrieveFiles(const atString& directory);

	__forceinline bool CloseFileHandle(const fiDevice& device, const fiHandle& handle) const;
	__forceinline bool HasDirectoryAttribute(const fiFindData& findData) const;
	__forceinline bool IsInvalidFileHandle(const fiHandle& handle) const;
	__forceinline bool IsCurrentDirectory(const char* path) const;
	__forceinline bool IsParentDirectory(const char* path) const;

	bool GetSoftwareKey(HKEY& outKey) const;
	bool GetUsingTempDirectory(const HKEY& inKey, DWORD& outValue) const;
	bool GetCustomDataDirectory(const HKEY& inKey, atString& outValue) const;
	bool GetDefaultDataDirectory(atString& outPath) const;
	__forceinline bool IsUsingCustomDirectory(DWORD value) const;

	__forceinline bool CheckAddressFiles() const;
	__forceinline bool CheckAddressFile(const atString& path) const;
	__forceinline bool IsInScriptGlobalsRegion(const u64& address) const;
	__forceinline bool IsInGameImageRegion(const u64& address) const;

	static const u32 ADDRESS_FILE_START_OFFSET = 7;
	static const u64 ADDRESS_FILE_MAX_BUFFER = 4096;

	atVector<atString> m_files;
};
#endif //CE_DETECTION_ENABLE_METADATA_CHECKER

#if CE_DETECTION_ENABLE_PROCESS_CHECKER
class CEProcessChecker
{
public:
	CEProcessChecker() {}
	~CEProcessChecker() {}

	bool Check();

private:
	void CheckProcesses();

	bool HasHandleToCurrentProcess(const u32 pid);
	void GetFileNameForModule(HMODULE hMod, HANDLE hProcess, atString& outName);
};
#endif //CE_DETECTION_ENABLE_PROCESS_CHECKER

bool CEDetectionPlugin_Create();
bool CEDetectionPlugin_Configure();
bool CEDetectionPlugin_Destroy();
bool CEDetectionPlugin_Work();

bool CEDetectionPlugin_OnSuccess();
bool CEDetectionPlugin_OnCancel();
bool CEDetectionPlugin_OnFailure();

bool CEDetectionPlugin_Init();

#endif //RAGE_SEC_TASK_CE_DETECTION
#endif //USE_RAGESEC

#endif //!SECURITY_CHEATENGINEDETECTIONPLUGIN_H