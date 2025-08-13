#ifndef REPLAY_FILE_MANANGER_H
#define REPLAY_FILE_MANANGER_H

#include "../ReplaySettings.h"

#if GTA_REPLAY

// rage
#include "atl/string.h"
#include "net/status.h"
#include "rline/rl.h"
#include "system/ipc.h"
#include "zlib/zlib.h"

// game
#include "control/replay/ReplaySupportClasses.h"
#include "control/replay/ReplayBufferMarker.h"
#include "device_replay.h"

#include "SaveLoad/savegame_photo_async_buffer.h"

class CReplayMgrInternal;
class ReplayBufferMarker;
class CMontage;
class CSavegameQueuedOperation_EnumerateReplayFiles;
class CSavegameQueuedOperation_DeleteReplayClips;
class CSavegameQueuedOperation_LoadReplayHeader;
class CSavegameQueuedOperation_DeleteReplayFile;
class CSavegameQueuedOperation_LoadMontage;
class CSavegameQueuedOperation_SaveMontage;
class CSavegameQueuedOperation_ReplayUpdateFavourites;
class CSavegameQueuedOperation_ReplayMultiDelete;

typedef void* FileHandle;

enum eReplayFileOperations
{
	REPLAY_FILEOP_NONE = 0,
	REPLAY_FILEOP_SAVE,
	REPLAY_FILEOP_LOAD,
	REPLAY_FILEOP_SAVE_CLIP,
	REPLAY_FILEOP_LOAD_CLIP,
	REPLAY_FILEOP_LOAD_HEADER,
	REPLAY_FILEOP_DELETE_FILE,
	REPLAY_FILEOP_ENUMERATE_CLIPS,
	REPLAY_FILEOP_ENUMERATE_MONTAGES,
	REPLAY_FILEOP_LOAD_MONTAGE,
	REPLAY_FILEOP_SAVE_MONTAGE,
	REPLAY_FILEOP_UPDATE_FAVOURITES,
	REPLAY_FILEOP_UPDATE_MULTI_DELETE
};

enum eReplayEnumerationTypes
{
	REPLAY_ENUM_CLIPS,
	REPLAY_ENUM_MONTAGES,
};

enum eReplayMemoryWarning
{
	REPLAY_MEMORYWARNING_NONE,
	REPLAY_MEMORYWARNING_LOW,
	REPLAY_MEMORYWARNING_OUTOFMEMORY,
};


#define DO_CRC_CHECK 1
#define NUM_CACHED_HEADERS 50

typedef void(*tExportStatisticsFunc)(atMap<s32, packetInfo> &packetInfos);
typedef void(*tExportPacketsFunc)(FileHandle handle);
typedef bool(*tLoadStartFunc)(ReplayHeader& pHeader);
typedef void(*tOnLoadStreamingFunc)();
typedef bool(*tFileOpCompleteFunc)(int);
typedef bool(*tFileOpSignalCompleteFunc)(int);
typedef void(*tDoFileOpFunc)(FileHandle);

typedef bool(*tValidateFunc)(fiHandle handle);

struct ReplayFileInfo
{
	ReplayFileInfo() : m_BufferInfo(NULL)
	{ 
		Reset(); 
	}

	ReplayHeader			m_Header;
	float					m_ClipLength;
	CMontage*				m_Montage;
	CBufferInfo*			m_BufferInfo;
	eReplayFileOperations	m_FileOp;
	atString				m_Filename;
	atString				m_FilePath;
	u16						m_NumberReplayBlocks;
	ReplayMarkers			m_Markers;
	atArray<CBlockProxy>	m_BlocksToSave;
	FileDataStorage*		m_FileDataStorage;
	atString				m_Filter;
	tLoadStartFunc			m_LoadStartFunc;
	bool					m_CancelOp;

	uLong					m_CRC;

	void Validate();
	void Reset()
	{
		m_Header.Reset();
		m_NumberReplayBlocks = 0;
		m_ClipLength = 0;
		m_BufferInfo = NULL;
		m_Montage = NULL;
		m_FileOp = REPLAY_FILEOP_NONE;
		m_FilePath = "";
		m_Filename = "";
		m_NumberReplayBlocks = 0;
		m_BlocksToSave.clear();
		m_FileDataStorage = NULL;
		m_Filter = "";
		m_LoadStartFunc = NULL;
		m_CancelOp = false;

		m_CRC = crc32(0L, Z_NULL, 0);
	}
};

#if !REPLAY_USE_PER_BLOCK_THUMBNAILS
struct ReplayThumbnailSaveData
{
	CSavegamePhotoAsyncBuffer	m_asyncBuffer;
	atString					m_path;

	inline bool IsInitialized() const
	{
		return m_asyncBuffer.IsInitialized() && m_path.GetLength() > 0;
	}

	inline void Reset()
	{
		m_asyncBuffer.Shutdown();
		m_path = "";
	}
};
#endif // REPLAY_USE_PER_BLOCK_THUMBNAILS

class ReplayHeaderCache
{
public:
	ReplayHeaderCache()
	{
		 CleanUp();
	}

	ReplayHeader* Get(const char* pLookUp)
	{
		return Get(Find(pLookUp));
	}

	ReplayHeader* Get(u32 index)
	{
		if( index != -1 )
		{
			return &m_Cache[index];
		}

		return NULL;
	}

	void Push(const char* pFileName, const ReplayHeader* pHeader)
	{
		m_Cache[m_Top]  = *pHeader;

		sprintf_s(m_LookUp[m_Top], RAGE_MAX_PATH, "%s", pFileName);

		IncrementOffset(m_Top);

		if( m_Top == m_Bottom )
		{
			IncrementOffset(m_Bottom);
		}
	}
	
	s32 Find(const char* pFileName)
	{
		for(int i = 0; i < GetCount(); ++i)
		{
			if( strcmp(pFileName, m_LookUp[i]) == 0 )
			{
				return i;
			}
		}

		return -1;
	}

	bool Exists(const char* pFileName)
	{
		u32 found = Find(pFileName);

		return (found != -1);
	}

	void Remove(const char* pFileName)
	{
		u32 index = Find(pFileName);

		if( index == -1 )
		{
			return;
		}

		int current = index;
		int next = index; 
		IncrementOffset(next);

		//if we're at the top then just clear out the top value
		if( current == m_Top )
		{
			memset(m_LookUp[current], 0, RAGE_MAX_PATH);
			m_Cache[current].Reset();
			return;
		}

		//if we're not the top
		//we need to remove one and shuffle everything
		while(current != m_Top)
		{
			Copy(current, next);

			IncrementOffset(next);
			IncrementOffset(current);
		}

		DecrementOffset(m_Top);

		if( m_Top == m_Bottom )
		{
			DecrementOffset(m_Bottom);
		}
	}

	u32 GetCount() const
	{
		if(m_Top < m_Bottom || m_Top == NUM_CACHED_HEADERS-1)
		{
			return NUM_CACHED_HEADERS;
		}

		return m_Top-m_Bottom;
	}

	void CleanUp()
	{
		m_Top = 0;
		m_Bottom = 0;

		for(int i = 0; i < NUM_CACHED_HEADERS; ++i)
		{
			memset(m_LookUp[i], 0, RAGE_MAX_PATH);
			m_Cache[i].Reset();
		}
	}

private:
	void Copy(u32 to, u32 from)
	{
		strcpy_s(m_LookUp[to], RAGE_MAX_PATH, m_LookUp[from]);
		m_Cache[to] = m_Cache[from];

		memset(m_LookUp[from], 0, RAGE_MAX_PATH);
		m_Cache[from].Reset();
	}

	void IncrementOffset(s32& offset)
	{
		offset++;
		if(offset >= NUM_CACHED_HEADERS)
		{
			offset = 0;
		}
	}

	void DecrementOffset(s32& offset)
	{
		offset--;
		if(offset < 0)
		{
			offset = NUM_CACHED_HEADERS-1;
		}
	}

	ReplayHeader		m_Cache[NUM_CACHED_HEADERS];
	char				m_LookUp[NUM_CACHED_HEADERS][RAGE_MAX_PATH];

	s32					m_Top;
	s32					m_Bottom;
};

class ReplayFileManager
{
	friend class CSavegameQueuedOperation_EnumerateReplayFiles;
	friend class CSavegameQueuedOperation_DeleteReplayClips;
	friend class CSavegameQueuedOperation_LoadReplayHeader;
	friend class CSavegameQueuedOperation_DeleteReplayFile;
	friend class CSavegameQueuedOperation_LoadReplayClip;
	friend class CSavegameQueuedOperation_LoadMontage;
	friend class CSavegameQueuedOperation_SaveMontage;
	friend class CSavegameQueuedOperation_ReplayUpdateFavourites;
	friend class CSavegameQueuedOperation_ReplayMultiDelete;
	friend class fiDeviceReplay;

public:
	ReplayFileManager();
	~ReplayFileManager();

	void Startup();
	void Process();
	void Shutdown();

private:
	static int GetRemainingBlocksFree(CBlockProxy block, void* pParam);
	void ProcessInitialEnumeration();
	void ProcessShouldSaveReplay();

public:
#if __BANK
	void AddDebugWidgets(bkBank& bank);
#endif // __BANK

	bool StartSave(const ReplayHeader& header, atArray<CBlockProxy>& blocksToSave, tFileOpCompleteFunc fileOpCompleteFunc);

	bool StartMarkerSave(const ReplayFileInfo& info, tFileOpCompleteFunc fileOpCompleteFunc);

	static bool StartEnumerateFiles(eReplayEnumerationTypes enumType, FileDataStorage& fileList, const char* filter = "");
	static bool StartEnumerateProjectFiles(FileDataStorage& fileList, const char* filter = "");
	static bool CheckEnumerateProjectFiles(bool& result);

	static bool StartEnumerateClipFiles(FileDataStorage& fileList, const char* filter = "");
	static bool CheckEnumerateClipFiles(bool& result);
	
	static bool StartDeleteClip(const char* filePath);
	static bool CheckDeleteClip(bool& result);

	static bool StartDeleteFile(const char* filePath);
	static bool CheckDeleteFile(bool& result);

	static bool StartGetHeader(const char* filePath);
	static bool CheckGetHeader(bool& result, ReplayHeader& header);

	static bool StartLoadMontage(const char* filePath, CMontage* montage);
	static bool CheckLoadMontage(bool &result);

	static bool StartSaveMontage(const char* filePath, CMontage* montage);
	static bool CheckSaveMontage(bool &result);

	static bool IsClipFileValid( u64 const ownerId, const char* szFileName, bool checkDisk = false);
	static bool IsProjectFileValid( u64 const ownerId, const char* szFileName, bool checkDisk = false);

	static bool DoesClipHaveModdedContent( u64 const ownerId, const char* szFileName );

	static bool IsMarkedAsFavourite(const char* filePath);
	static void MarkAsFavourite(const char* filePath);
	static void UnMarkAsFavourite(const char* filePath);

	static bool StartUpdateFavourites();
	static bool CheckUpdateFavourites(bool& result);

	static bool IsMarkedAsDelete(const char* filePath);
	static u32	GetCountOfFilesToDelete(const char* filter);
	static void MarkAsDelete(const char* filePath);
	static void UnMarkAsDelete(const char* filePath);
	

	static bool StartMultiDelete(const char* filter);
	static bool CheckMultiDelete(bool& result);

	static u32 GetClipRestrictions(const ClipUID& clipUID);

	static const ClipUID* GetClipUID( u64 const ownerId, const char* filename);
	static float GetClipLength( u64 const ownerId, const char *filename);
	static u32	GetClipSequenceHash(u64 const ownerId, const char* filename, bool prev);

	u32 GetClipCount() const { return sm_Device ? sm_Device->GetClipCount() : 0; }
	u32 GetProjectCount() const { return sm_Device ? sm_Device->GetProjectCount() : 0; }

	static u32 GetMaxClips() { return sm_Device ? sm_Device->GetMaxClips() : 0; }
	static u32 GetMaxProjects() { return sm_Device ? sm_Device->GetMaxProjects() : 0; }

	static bool IsClipCacheFull() { return sm_Device ? sm_Device->IsClipCacheFull() : false; } 

	bool StartSaveFile(const char* filePath, tDoFileOpFunc fileOpFunc = NULL, tFileOpCompleteFunc fileOpCompleteFunc = NULL, tFileOpSignalCompleteFunc fileOpSignalCompleteFunc = NULL);
	bool StartLoadFile(const char* filePath, tDoFileOpFunc fileOpFunc = NULL, tFileOpCompleteFunc fileOpCompleteFunc = NULL);

	static bool StartLoadReplayHeader(const char* filePath);

	static bool ValidateClip(const char* filePath);

	static bool IsPerformingFileOp();
	static eReplayFileErrorCode GetLastErrorCode() { return sm_LastFileOpRetCode; }
	static u64 GetLastOpExtendedResultData() { return sm_LastFileOpExtendedResultData; }

	bool IsSaving() const;
	bool IsLoading() const;

	static u64 GetAvailableDiskSpace();
	static u64 GetAvailableSystemDiskSpace();
	static void SetShouldUpdateMontagesSize() { if( sm_Device ) sm_Device->SetShouldUpdateMontagesSize(); }
	static void UpdateAvailableDiskSpace(bool clipUpdated = false);
#if RSG_ORBIS
	static void RefreshNonClipDirectorySizes();
#endif

	static bool CanFullfillUserSpaceRequirement() { return sm_Device ? sm_Device->CanFullfillUserSpaceRequirement() : false; }
	static bool CanFullfillSpaceRequirement(eReplayMemoryLimit size) { return sm_Device ? sm_Device->CanFullfillSpaceRequirement(size) : false; }
	static float GetAvailableAndUsedClipSpaceInGB() { return sm_Device ? sm_Device->GetAvailableAndUsedClipSpaceInGB() : 0; }
	static eReplayMemoryLimit GetMaxAvailableMemoryLimit() { return sm_Device ? sm_Device->GetMaxAvailableMemoryLimit() : (eReplayMemoryLimit)0; }
	static eReplayMemoryLimit GetMinAvailableMemoryLimit() { return sm_Device ? sm_Device->GetMinAvailableMemoryLimit() : (eReplayMemoryLimit)0; }
	static bool GetMemLimitInfo(eReplayMemoryLimit limit, MemLimitInfo& info);

	static bool CheckAvailableDiskSpaceForMontage(u64 size, const char* path);
#if RSG_ORBIS
	static bool CheckAvailableDiskSpaceForAutoSave();
#endif

	static void AddClipToProjectCache(const char* projectPath, u64 const userId, ClipUID const& uid );
	static void RemoveClipFromProjectCache(const char* projectPath, u64 const userId, ClipUID const& uid );
	static void AddProjectToCache(const char* path, u64 const userId, u64 const size, u32 const duration );
	static bool IsProjectInProjectCache( const char* path, u64 const userId );
	static void RemoveCachedProject(const char* path, u64 const userId);
	static bool IsClipInAProject(const ClipUID& clipUID);

	static const atString& GetCurrentFilename()					{	return sm_CurrentReplayFileInfo.m_Filename;	}
	static float GetCurrentLength()								{	return sm_CurrentReplayFileInfo.m_ClipLength;	}
	static void AttemptToCancelCurrentOp()						{	sm_CurrentReplayFileInfo.m_CancelOp = true; }

	static bool getFreeSpaceAvailableForVideos( size_t& out_sizeBytes );
	static bool getFreeSpaceAvailableForClips( size_t& out_sizeBytes );
	static bool getUsedSpaceForClips( size_t& out_sizeBytes );
	static void getClipDirectoryForUser( u64 const userId, char (&out_buffer)[ REPLAYPATHLENGTH ]);
	static void getClipDirectory(char (&out_buffer)[ REPLAYPATHLENGTH ], bool userOnly = true);
	static void getVideoProjectDirectory(char (&out_buffer)[ REPLAYPATHLENGTH ], bool userOnly = true);
	static void getVideoDirectory(char (&out_buffer)[ REPLAYPATHLENGTH ], bool userOnly = true);
	static void getDataDirectory(char (&out_buffer)[ REPLAYPATHLENGTH ], bool userOnly = true);
	static void FixName(char (&out_buffer)[ REPLAYPATHLENGTH ], const char* source);
	static void InitVideoOutputPath();
	static void ShouldRefreshVideoOutputPath();

	static bool CheckAvailableDiskSpaceForClips(const u64& uDataSize, const bool& useThreshold, float minThreshold, eReplayMemoryWarning& memoryWarning);

	static bool IsInitialised() { return sm_Initialized && sm_HasCompletedInitialEnumeration; }

	//threading stuff
	static void StartFileThread();
	static void StopFileThread();

#if __BANK
	static void DumpReplayFiles_Bank();
#endif

	static u64 getCurrentUserIDToPrint();
	static u64 getUserIdFromPath( char const * const path );

#if RSG_ORBIS
	static bool UpdateUserIDDirectoriesOnChange();
	static void SetShouldRemapNpOnlineIdsToAccountIds(const bool shouldRemap);
#endif

	static bool IsForceSave()			{	return sm_ForceSave;	}

private:
	bool StartFileOp(const char* filePath, tDoFileOpFunc fileOpFunc, tFileOpCompleteFunc fileOpCompleteFunc, tFileOpSignalCompleteFunc fileOpSignalCompleteFunc, bool readOnly);

	static eReplayFileErrorCode SaveClip(ReplayFileInfo& info);
	static eReplayFileErrorCode SaveCompressedClip(ReplayFileInfo& info, FileHandle handle);
	static eReplayFileErrorCode SaveUncompressedClip(ReplayFileInfo& info, FileHandle handle);
	
	static bool StartLoad(const char* filePath, CBufferInfo* bufferInfo, tLoadStartFunc onLoadStartFunc);
	static int LoadCompressedBlock(CBlockProxy block, void* pParam);
	static eReplayFileErrorCode LoadCompressedClip(ReplayFileInfo& info, FileHandle handle);
	static int LoadUncompressedBlock(CBlockProxy block, void* pParam);
	static eReplayFileErrorCode LoadUncompressedClip(ReplayFileInfo& info, FileHandle handle);

	static bool LoadMontage(const char* filePath, CMontage* montage);
	static bool SaveMontage(const char* filePath, CMontage* montage);

	static u32 GetCrC(FileHandle handle, u32 uOffset, u32 size);
	static void SetSaved(ReplayFileInfo& info, bool doClipCrc = true);

	static bool EnumerateFiles(eReplayEnumerationTypes enumType, FileDataStorage& fileList, const char* filter = "");
	static bool DeleteFile(const char* filePath);
	
	static bool UpdateFavourites();

	static bool ProcessMultiDelete(const char* filter);

	static eReplayFileErrorCode PerformFileOp(ReplayFileInfo& info);

	static eReplayFileErrorCode ReadHeader(FileHandle handle, ReplayHeader& replayHeader, const char *pFileName, uLong& uCRC, bool doClipCrc = false);

	static const ReplayHeader* GetHeader(const char* filename);
	static void GetOffsetToClipData(const ReplayHeader& replayHeader, u32& uOffset);

	static void UpdateHeaderCache(const char* filename, const ReplayHeader* pReplayHeader);

	static bool GenerateUniqueClipName(char (&out_buffer)[ REPLAYPATHLENGTH ], ClipUID& uid);

	static FileHandle OpenFile(ReplayFileInfo& info);
	static FileHandle OpenFile(ReplayFileInfo& info, FileStoreOperation fileOp);

#if !REPLAY_USE_PER_BLOCK_THUMBNAILS
	static bool RequestThumbnailSaveAsync();
	static void BlockOnThumbnailSaveAsync();
#endif // !REPLAY_USE_PER_BLOCK_THUMBNAILS

	static void ResetSaveFlags(ReplayFileInfo& info);
	static void RefreshEnumeration();
	
	static void FileThread(void*);

#if RSG_PC
	static void WatchDirectory(void*);
#endif

#if !__FINAL
	static void DumpReplayFiles(bool clips);
public:
	static void DumpReplayFile(const char* filePath, const char* fileName, const char* destinationDir);
	static const char* GetDumpFolder();
private:
#endif

#if __BANK
	static void UpdateFavouritesCallback();
	static void MarkAsFavouriteCallback();
	static void UnMarkAsFavouriteCallback();
#endif // __BANK

	static bool appendUserIDDirectoryName(const char* baseDir, char (&out_buffer)[ REPLAYPATHLENGTH ], u64 userIDToForce = 0);

#if RSG_ORBIS
#if __BANK
	static void PrintDir(const char* path, bool allowedDeletion, int depth = 0);
	static void PrintFileContents(bool allowedDeletion);
#endif // __BANK
	static u32 sm_timeAccountMappingKicked;
	static bool sm_vitalClipFolderProcessFailed;
	static bool sm_vitalClipFolderProcessNeeded;
public:
	static bool IsVitalClipFolderProcessingStillPending() { return sm_vitalClipFolderProcessNeeded || sm_vitalClipFolderProcessFailed; }
	static s32 GetCurrentUserId(int gamerIndex = -1);
private:
	static void FixOldCookies(const char* path, bool& notReady, bool topDir = false);
	static void MapAccountIdToOwnerId(const char* path);
	static bool IsPSNIdInGivenPath(const char* PSNId, const char* path, const char* cookie);
	static bool WriteCookie(const char* fullPathAndFilename, const char* cookieValue);
	static bool CreatePSNIdFileInGivenLocalId(const char* PSNId, u64 localId, const char* cookie, bool useAccountID);
	static bool IsPSNIdInGivenLocalId(const char* PSNId, u64 localId, const char* cookie);
	static bool IsPSNIdInAnyLocalId(const char* PSNId, u64* localIdForPSN, const char* cookie);
	static void RenameLocalIdDirectories(u64 oldLocalId, u64 newLocalId);

	static rlSceNpOnlineId GetCurrentOnlineId();
	static u64 GetCurrentAccountId();
	static void AddAccountIdToOnlineId(u64 accountId, u64 onlineId);
	static u64 GetOnlineIdFromAccountId(u64 accountId);
	static u64 GetAccountIdFromOnlineId(u64 onlineId);

	static void RetrieveAccountIdFromOnlineId(const char* onlineId);

#if __BANK
	class CTestID
	{
	public:
		rlSceNpOnlineId onlineId;
		s32 userId;
		u64 accountId;
	};
	static atArray<CTestID>	sm_TestIds;
	static int				sm_UseTestIdSet;
#endif // __BANK
#endif

	static void TriggerFileRequestThread();

	static fiDeviceReplay*					sm_Device;

	static ReplayHeaderCache				sm_HeaderCache;

	static sysIpcThreadId					sm_FileThreadID;
	static sysIpcSema						sm_TriggerFileRequest;
	static sysIpcSema						sm_HasFinishedRequest;
	static sysIpcSema						sm_LoadHeaderRequest;

#if RSG_PC
	static sysIpcThreadId					sm_FileNotificationThreadID;
	static bool								sm_WatchDirectory;
#endif

	static bool								sm_HaltFileOpThread;
#if !REPLAY_USE_PER_BLOCK_THUMBNAILS
	static ReplayThumbnailSaveData			sm_thumbnailSaveData;
#endif // !REPLAY_USE_PER_BLOCK_THUMBNAILS
	static ReplayFileInfo					sm_CurrentReplayFileInfo;
	static eReplayFileOperations			sm_LastFileOp;
	static eReplayFileErrorCode				sm_LastFileOpRetCode;
	static u64								sm_LastFileOpExtendedResultData;
	static int								sm_MinNumBlocksTillSave;

	static bool								sm_Initialized;
	static bool								sm_HasCompletedInitialEnumeration;
	static bool								sm_StartInitialEnumeration;
	
#if !__FINAL
	static bool								sm_bDumpedReplayFiles;
#endif

#if __BANK	
	static u32								sm_estSaveTimePerBlockMs;
	static u32								sm_estBlockFillRateMS;

	static int								sm_numBlocksFree;
	static int								sm_numBlocksFull;
	static int								sm_totalBlocks;
	static bool								sm_simulateStreamingLoad;
#endif
	
	static tOnLoadStreamingFunc				sm_OnLoadStreamingFunc;

	static tFileOpCompleteFunc				sm_FileOpCompleteFunc;
	static tFileOpSignalCompleteFunc		sm_FileOpSignalCompleteFunc;
	static tDoFileOpFunc					sm_DoFileOpFunc;

	static u32								sm_CompressionBufferSize;
	static u32								sm_CompressionBufferSizeWorstCase;
	static Byte*							sm_CompressionBuffer;
	static bool								sm_SafeToSaveReplay;
	static bool								sm_ForceSave;
	static u32								sm_BytesToWrite;

#if RSG_ORBIS
	static s32								sm_CurrentLocalUserId;
	static bool                             sm_ShouldRemapNpOnlineIdsToAccountIds;
	static netStatus						sm_AccountIdStatus;
	static u64								sm_CurrentOnlineIdProcessing;
	static rlSceNpAccountId					sm_AccountId;
	static atMap<u64, u64>					sm_AccountIdToOnlineId;
	static u32								sm_AccountsQueried;

#endif

	static CSavegameQueuedOperation_EnumerateReplayFiles	sm_EnumerateProjectFiles;
	static CSavegameQueuedOperation_EnumerateReplayFiles	sm_EnumerateClipFiles;
	static CSavegameQueuedOperation_DeleteReplayClips		sm_DeleteClips;
	static CSavegameQueuedOperation_DeleteReplayFile		sm_DeleteFile;
	static CSavegameQueuedOperation_LoadReplayHeader		sm_LoadHeader;
	static CSavegameQueuedOperation_LoadMontage				sm_LoadMontage;
	static CSavegameQueuedOperation_SaveMontage				sm_SaveMontage;
	static CSavegameQueuedOperation_ReplayUpdateFavourites	sm_UpdateFavourites;
	static CSavegameQueuedOperation_ReplayMultiDelete		sm_MultiDelete;
	static CSavegameQueuedOperation_EnumerateReplayFiles	sm_InitialEnumerateClipFiles;
};


#endif

#endif
