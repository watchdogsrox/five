#include "FileStoreDurango.h"

#if GTA_REPLAY

#include "string/string.h"
#include "string/unicode.h"

#define UTF16_TO_UTF8(x) WIDE_TO_UTF8(reinterpret_cast<const char16* const>(x))

#if RSG_DURANGO

#include <windows.h>

FileStoreDurango::FileStoreDurango() : fiDeviceReplay()
{
}

u64 FileStoreDurango::GetMaxMemoryAvailable() const
{
	u64 totalBytesAvailable = 0;
	char buffer[RAGE_MAX_PATH] = {0};

	if(Verifyf(m_pDevice, "Failed to get File Store device!"))
	{
		m_pDevice->FixRelativeName(buffer, RAGE_MAX_PATH, GetDirectory());

		USES_CONVERSION;
		if(Verifyf(GetDiskFreeSpaceExW(reinterpret_cast<const wchar_t*>(UTF8_TO_WIDE(buffer)), NULL, (PULARGE_INTEGER)&totalBytesAvailable, NULL), "Failed to get File Store size!"))
		{
			return totalBytesAvailable;
		}
		else
		{
			// This number was extracted from a call to GetDiskFreeSpaceExW(). It is slightly less than our initial guess.
			return 15032373248LL;
		}
	}
	else
	{
		return 0;
	}
}

const char* FileStoreDurango::GetPersistentStorageLocation()
{
	static char path[REPLAYPATHLENGTH] = {0};
	
	// Whilst we could have checked if path is valid, if we fail to retrieve it we may end up constantly trying to retrieve it.
	// We use this guard to not do that as a WinRT exception can take a long time be thrown.
	static bool pathRetrieved = false;
	if(pathRetrieved == false)
	{
#if RSG_BANK
		// If the persistent storage folder name already exists on the console it will fail to mount the persistent storage in that location.
		// We delete the folder and files here so we can mount it correctly. We have to do this as some of our code used the path before it
		// is mounted which causes issues!
		const char* const OLD_DATA_PATH = "T:\\PersistentLocalStorage";
		DWORD attributes = GetFileAttributesA(OLD_DATA_PATH);
		
		// According to https://forums.xboxlive.com/AnswerPage.aspx?qid=e206eebc-1bcd-4007-be36-35d8f1dc402b&tgt=1 the FILE_ATTRIBUTE_REPARSE_POINT
		// flag means the persistent storage so do not delete it if it is the correct mount type.
		if(attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0)
		{
			Verifyf( DeleteDirectory("T:\\PersistentLocalStorage"),
					 "Failed to delete '%s' from the console. This will cause issues with the persistent storage!",
					 OLD_DATA_PATH );
		}
#endif // RSG_BANK

		pathRetrieved = true;

		USES_CONVERSION;
		try
		{
			Windows::Storage::ApplicationData^ applicationData = Windows::Storage::ApplicationData::Current;
			if(applicationData != nullptr)
			{
				Windows::Storage::StorageFolder^ storageFolder = applicationData->LocalFolder;
				if(storageFolder != nullptr)
				{
					Platform::String^ storagePath = storageFolder->Path;
					if(path != nullptr)
					{
						formatf( path, "%s\\", UTF16_TO_UTF8(storagePath->Data()) );
					}
				}
			}
		}
		catch(Platform::Exception^ e)
		{
			Assertf(false, "Failed to get persistent storage location. Exception 0x%08x, Msg: %ls",
				e->HResult,
				e->Message != nullptr ? e->Message->Data() : L"NULL");
		}
	}

	int stringLength = strlen(path);
	// ignore '\\??\\' from the start of the path when returning it
	// not everything copes with it yet, but just starting from T: works fine and works consistently with everything
	// find first 'T' as the question marks may change in the future, but the drive letter won't
	for (int i = 0; i < stringLength; ++i)
	{
		if (path[i] == 'T' || path[i] == 't')
		{
			return &path[i];
		}
	}

	// don't return '\\??\\'
	return path;
}

#if RSG_BANK
bool FileStoreDurango::DeleteDirectory(const char* path)
{
	char currentPath[RAGE_MAX_PATH];
	formatf(currentPath, "%s\\*", path);

	WIN32_FIND_DATAA fileData;
	HANDLE findHandle = FindFirstFileA(currentPath, &fileData);

	bool result = true;

	if(findHandle != INVALID_HANDLE_VALUE)
	{
		do
		{
			// Sanity check, do not delete '.' or '..'!
			if(strcmp(fileData.cFileName, ".") != 0 && strcmp(fileData.cFileName, "..") != 0)
			{
				formatf(currentPath, "%s\\%s", path, fileData.cFileName);
				DWORD attributes = GetFileAttributesA(currentPath);
				if((attributes & FILE_ATTRIBUTE_DIRECTORY))
				{
					result &= DeleteDirectory(currentPath);
				}
				else
				{
					result &= (DeleteFileA(currentPath) == TRUE);
				}
			}
		}
		while(FindNextFileA(findHandle, &fileData));
		FindClose(findHandle);
	}

	result &= (RemoveDirectoryA(path) == TRUE);

	return result;
}
#endif // RSG_BANK

#endif

#undef UTF16_TO_UTF8

#endif