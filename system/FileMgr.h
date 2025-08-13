// Title	:	FileMgr.h
// Author	:	Adam Fowler
// Started	:	22/02/05

#ifndef _FILEMGR_H_
#define _FILEMGR_H_

#define MAX_PATH          260
#define MAX_DIRNAME_CHARS	(128)

#define READ_BLOCK_SIZE 	(16384)

typedef void* FileHandle;		//!< A file handle
#define	INVALID_FILE_HANDLE	(NULL)

#include "atl/atfixedstring.h"

// Define structure to hold file details
typedef struct sFilename
{
	char *pBitmapFileName;
	char *pMaskFileName;
} sFilename;


class CFileMgr
{
public:
	static void Initialise();
	static void SetDir(const char* pDirName);
	static s32 LoadFile(const char* pFilename, u8* pBuffer, s32 bufferSize = 0, const char* pReadString = "r");

	static FileHandle OpenFile(const char* pFilename, const char* pReadString = "r");
	static FileHandle OpenFileForWriting(const char* pFilename);
	static FileHandle OpenFileForAppending(const char* pFilename);
	static s32 Read(FileHandle id, char* pData, s32 size);
	static s32 Write(FileHandle id, const char* pData, s32 size);
	static s32 Tell(FileHandle id);
	static s32 Seek(FileHandle id, s32 offset);
	static s32 Flush(FileHandle id);
	static bool ReadLine(FileHandle id, char* pLine, s32 maxLen=256);
	static char* ReadLine(FileHandle id, bool bRemoveUnwantedChars = true);
	static s32 GetTotalSize(FileHandle id);
	static s32 CloseFile(FileHandle id);
	
#if __WIN32PC
	static void SetAppDataDir(const char* pDirName, bool bUseOriginal = false);
	static void SetUserDataDir(const char* pDirName, bool bUseOriginal = false);
	static void SetRockStarDir(const char* pDirName);

 	static const char* GetAppDataRootDirectory() {return ms_appDataRootDirName.c_str();}
 	static const char* GetUserDataRootDirectory() {return ms_userDataRootDirName.c_str();}
 	static const char* GetOriginalAppDataRootDirectory() {return ms_originalAppDataRootDirName.c_str();}
 	static const char* GetOriginalUserDataRootDirectory() {return ms_originalUserDataRootDirName.c_str();}
 	static const char* GetRockStarRootDirectory() {return ms_rockStarRootDirName.c_str();}
#endif	

	static char* GetRootDirectory() {return ms_rootDirName;}
	static char* GetCurrDirectory() {return ms_dirName;}
	static char* GetHddBootPath() {return ms_hddBootPath;}

	static inline bool IsValidFileHandle(FileHandle handle)
	{
		return ( handle != INVALID_FILE_HANDLE );
	}

	static void PreSetupDevices();
	static void SetupDevices();
	static void SetupDevicesAfterInit();

#if !__FINAL
	static void AllocateDummyResourceMem(void);
	static void FreeDummyResourceMem(void);
	static void AllocateExtraResourceMem(size_t memory);
	static void FreeExtraResourceMem(void);
#endif //!__FINAL

	static bool NeedsPath(const char *base);

	static const char* GetAssetsFolder();
#if __BANK
	static const char* GetToolFolder();
#endif

#if __PPU || RSG_DURANGO || RSG_ORBIS
	static void SetGameHddBootPath(const char* hddBootPath);
#endif

	// PURPOSE
	//	Returns true if we've forced the release packlist even if this is a non-release build.
	static bool ShouldForceReleaseAudioPacklist();

	static bool IsDownloadableVersion();

    static bool HasAsyncInstallFinished();
    static void CleanupAsyncInstall();

    static bool IsGameInstalled() { return ms_allChunksOnHdd; }

private:

	static bool IsAudioPackInInitialPackage(const char *packName);
	static bool ShouldAudioPackBeSkipped(const char *packName);
	static void MountAudioPacks(const char *audioPackFile);
	
	static char ms_rootDirName[MAX_DIRNAME_CHARS];
	static char ms_dirName[MAX_DIRNAME_CHARS];
	static char ms_hddBootPath[MAX_DIRNAME_CHARS];



#if __WIN32PC
	static void	SetPCDir(atFixedString<MAX_DIRNAME_CHARS>& baseDir, const char* subDir);

	static atFixedString<MAX_DIRNAME_CHARS> ms_appPath;
	static atFixedString<MAX_DIRNAME_CHARS> ms_userPath;
	static atFixedString<MAX_DIRNAME_CHARS> ms_rgscPath;

	static atFixedString<MAX_DIRNAME_CHARS> ms_originalAppDataRootDirName;
	static atFixedString<MAX_DIRNAME_CHARS> ms_appDataRootDirName;
	static atFixedString<MAX_DIRNAME_CHARS> ms_originalUserDataRootDirName;
	static atFixedString<MAX_DIRNAME_CHARS> ms_userDataRootDirName;
	static atFixedString<MAX_DIRNAME_CHARS> ms_rockStarRootDirName;
#endif	// __WIN32PC

    static bool ms_allChunksOnHdd;
};


//! Functions to do endian conversion on some assets.
//! On any platform other than Xenon, these compile out.
//! @todo KS - should be inline templatised function 
#if __XENON || __PPU
#define ConvertEndian2(x)																					\
{																											\
	u16 temp = (u16)(x);																					\
	(x) = (temp >> 8) | (temp << 8);																		\
}

#define ConvertEndian4(x)																					\
{																											\
	u32 temp = (u32)(x);																					\
	(x) = (temp >> 24) | ((temp >> 8) & 0xFF00) | ((temp << 8) & 0xFF0000) | (temp << 24);					\
}

#else
#define ConvertEndian2(x)
#define ConvertEndian4(x)
#endif

#endif
