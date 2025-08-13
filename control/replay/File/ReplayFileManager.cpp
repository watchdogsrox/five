#include "ReplayFileManager.h"

#if GTA_REPLAY

//game

#include "control/replay/streaming/ReplayStreaming.h"
#include "control/replay/replay.h"
#include "control/replay/replay_channel.h"
#include "control/replay/ReplayControl.h"
#include "frontend/GameStreamMgr.h"
#include "grcore/image.h"
#include "renderer/PostProcessFXHelper.h"
#include "SaveLoad/savegame_photo_manager.h"
#include "SaveLoad/savegame_queued_operations.h"
#include "SaveLoad/GenericGameStorage.h"
#include "streaming/streamingengine.h"
#include "replaycoordinator/storage/Montage.h"
#include "zlib/lz4.h"
#include "zlib/zlib.h"
#include "rline/rlgamerinfo.h"

#if RSG_PC
#include "control/replay/file/FileStorePC.h"
#endif
#if RSG_ORBIS
#include "control/replay/file/FileStoreOrbis.h"
#endif
#if RSG_DURANGO
#include "control/replay/File/FileStoreDurango.h"
#endif

#if (RSG_PC || RSG_DURANGO)
#include "control/videorecording/videorecording.h"
#endif

// This will delay saving whilst the streaming is under load
// unless there is a risk that we might over write data.
#define STREAMING_LOAD_SAVING RSG_ORBIS

PARAM(useCustomProfileID, "[Replay] Force the profile ID to a certain value (Durango and Orbis only)");
PARAM(replayFilesWorkingDir, "[Replay] Use this folder instead of the one used by default for saving/loading replay files");
PARAM(replayFilesDumpDir, "[Replay] Folder where the replays clips/projects will be dumped once there's loaded");
PARAM(replayDebugClipPrefix, "[Replay] Name to prefix to a clip to differentiate from other peoples clips");
PARAM(replayDontUseAccountID, "");
PARAM(replayUseTestID, "");
PARAM(replayForceAccountFromUserFail, "");
PARAM(replayObtainOnlyOneAccount, "");
PARAM(replayFakeDelayAccountRetrieval, "");
PARAM(replayRandomlyFailAccountRetrieval, "");
PARAM(replayDeleteNewCookiesFirst, "");
PARAM(replayDontEnumerateDownload0Prior, "");
PARAM(replayDontEnumerateDownload0Post, "");
PARAM(replayFailGetCurrentAccountID, "");
PARAM(replayAccountIDAlwaysPending, "");

//Defines the number of bytes to write per iteration when saving out a block
#define							BYTES_TO_WRITE_PER_ITERATION 64 * 1024 // 64 KB

#if RSG_ORBIS
u32								ReplayFileManager::sm_timeAccountMappingKicked = 0;
bool							ReplayFileManager::sm_vitalClipFolderProcessFailed = false;
bool							ReplayFileManager::sm_vitalClipFolderProcessNeeded = true;
#endif // RSG_ORBIS

fiDeviceReplay*					ReplayFileManager::sm_Device = NULL;

ReplayHeaderCache				ReplayFileManager::sm_HeaderCache;

sysIpcThreadId					ReplayFileManager::sm_FileThreadID = sysIpcThreadIdInvalid;
sysIpcSema						ReplayFileManager::sm_TriggerFileRequest = NULL;
sysIpcSema						ReplayFileManager::sm_HasFinishedRequest = NULL;
sysIpcSema						ReplayFileManager::sm_LoadHeaderRequest = NULL;

#if RSG_PC
sysIpcThreadId					ReplayFileManager::sm_FileNotificationThreadID = sysIpcThreadIdInvalid;
bool							ReplayFileManager::sm_WatchDirectory = false;
#endif

#if !REPLAY_USE_PER_BLOCK_THUMBNAILS
ReplayThumbnailSaveData			ReplayFileManager::sm_thumbnailSaveData;
#endif //!REPLAY_USE_PER_BLOCK_THUMBNAILS

ReplayFileInfo					ReplayFileManager::sm_CurrentReplayFileInfo;

tOnLoadStreamingFunc			ReplayFileManager::sm_OnLoadStreamingFunc = NULL;

tFileOpCompleteFunc				ReplayFileManager::sm_FileOpCompleteFunc = NULL;
tFileOpSignalCompleteFunc		ReplayFileManager::sm_FileOpSignalCompleteFunc = NULL;
tDoFileOpFunc					ReplayFileManager::sm_DoFileOpFunc = NULL;

bool							ReplayFileManager::sm_HaltFileOpThread = false;
eReplayFileErrorCode			ReplayFileManager::sm_LastFileOpRetCode = REPLAY_ERROR_SUCCESS;
u64								ReplayFileManager::sm_LastFileOpExtendedResultData = 0;

eReplayFileOperations			ReplayFileManager::sm_LastFileOp = REPLAY_FILEOP_NONE;

bool							ReplayFileManager::sm_Initialized = false;
bool							ReplayFileManager::sm_HasCompletedInitialEnumeration = false;
bool							ReplayFileManager::sm_StartInitialEnumeration = false;
int								ReplayFileManager::sm_MinNumBlocksTillSave = 3;

#if !__FINAL
bool							ReplayFileManager::sm_bDumpedReplayFiles = false;
#endif

#if __BANK
u32								ReplayFileManager::sm_estSaveTimePerBlockMs = 600;
u32								ReplayFileManager::sm_estBlockFillRateMS = 1100;

int								ReplayFileManager::sm_numBlocksFree = 0;
int								ReplayFileManager::sm_numBlocksFull = 0;
int								ReplayFileManager::sm_totalBlocks = 0;
bool							ReplayFileManager::sm_simulateStreamingLoad = false;
#endif

u32								ReplayFileManager::sm_CompressionBufferSize = BYTES_IN_REPLAY_BLOCK;
u32								ReplayFileManager::sm_CompressionBufferSizeWorstCase = LZ4_compressBound(BYTES_IN_REPLAY_BLOCK);
u32								ReplayFileManager::sm_BytesToWrite = BYTES_TO_WRITE_PER_ITERATION;
Byte*							ReplayFileManager::sm_CompressionBuffer = NULL;
bool							ReplayFileManager::sm_SafeToSaveReplay = false;//Is streaming under load
bool							ReplayFileManager::sm_ForceSave = false;

#if RSG_ORBIS
s32								ReplayFileManager::sm_CurrentLocalUserId = 0;
bool							ReplayFileManager::sm_ShouldRemapNpOnlineIdsToAccountIds = true;
netStatus						ReplayFileManager::sm_AccountIdStatus;
u64								ReplayFileManager::sm_CurrentOnlineIdProcessing = 0;
rlSceNpAccountId				ReplayFileManager::sm_AccountId;
atMap<u64, u64>					ReplayFileManager::sm_AccountIdToOnlineId;
u32								ReplayFileManager::sm_AccountsQueried = 0;

#if __BANK
atArray<ReplayFileManager::CTestID> ReplayFileManager::sm_TestIds;
int								ReplayFileManager::sm_UseTestIdSet = -1;
#endif // __BANK
#endif

CSavegameQueuedOperation_EnumerateReplayFiles	ReplayFileManager::sm_EnumerateProjectFiles;
CSavegameQueuedOperation_EnumerateReplayFiles	ReplayFileManager::sm_EnumerateClipFiles;
CSavegameQueuedOperation_DeleteReplayClips		ReplayFileManager::sm_DeleteClips;
CSavegameQueuedOperation_DeleteReplayFile		ReplayFileManager::sm_DeleteFile;
CSavegameQueuedOperation_LoadReplayHeader		ReplayFileManager::sm_LoadHeader;
CSavegameQueuedOperation_LoadMontage			ReplayFileManager::sm_LoadMontage;
CSavegameQueuedOperation_SaveMontage			ReplayFileManager::sm_SaveMontage;
CSavegameQueuedOperation_ReplayUpdateFavourites	ReplayFileManager::sm_UpdateFavourites;
CSavegameQueuedOperation_ReplayMultiDelete		ReplayFileManager::sm_MultiDelete;
CSavegameQueuedOperation_EnumerateReplayFiles	ReplayFileManager::sm_InitialEnumerateClipFiles;

void ReplayFileInfo::Validate()
{
	if( (m_FileOp == REPLAY_FILEOP_SAVE_CLIP || m_FileOp == REPLAY_FILEOP_LOAD_CLIP) )
	{
		if(m_FileOp == REPLAY_FILEOP_LOAD_CLIP)
		{
			replayAssertf(m_BufferInfo != NULL, "m_BufferInfo cannot be NULL!");
		}
		
		// Ensure the filename has the correct extension, but only for clip files
		if(m_FilePath.EndsWith(REPLAY_CLIP_FILE_EXT) == false)
			m_FilePath += REPLAY_CLIP_FILE_EXT;
	}
}

ReplayFileManager::ReplayFileManager()
{

}

ReplayFileManager::~ReplayFileManager()
{
	Shutdown();
}

void ReplayFileManager::Startup()
{
	if( sm_Initialized )
	{
		return;
	}

	if( sm_Device == NULL )
	{
		{
			sysMemAutoUseAllocator replayAlloc(*MEMMANAGER.GetReplayAllocator());
#if RSG_PC
			sm_Device = rage_new FileStorePC();
#elif RSG_ORBIS
			sm_Device = rage_new FileStoreOrbis();
#elif RSG_DURANGO
			sm_Device = rage_new FileStoreDurango();
#endif
		}
	}

	sm_Device->Init();
	sm_Device->MountAs(REPLAY_ROOT_PATH);

	sm_StartInitialEnumeration = !sm_HasCompletedInitialEnumeration;

	//create compression buffers
	sm_CompressionBuffer = (Byte*)sysMemManager::GetInstance().GetReplayAllocator()->Allocate(sm_CompressionBufferSizeWorstCase * sizeof(Byte), 16);

	sm_CurrentReplayFileInfo.Reset();
#if !REPLAY_USE_PER_BLOCK_THUMBNAILS
	sm_thumbnailSaveData.Reset();
#endif // !REPLAY_USE_PER_BLOCK_THUMBNAILS

	// Init free space.
	UpdateAvailableDiskSpace();
	GetAvailableDiskSpace();

//	replayDisplayf("%s", NetworkInterface::GetLocalGamerHandle().GetNpId().handle.data);

	StartFileThread();

#if __BANK && RSG_ORBIS
	if(PARAM_replayUseTestID.Get(sm_UseTestIdSet))
	{
		CTestID test0;
		test0.accountId = 111111111110;
		test0.onlineId.FromString("Test0");
		test0.userId = 2220;
		sm_TestIds.PushAndGrow(test0);

		CTestID test1;
		test1.accountId = 111111111111;
		test1.onlineId.FromString("Test1");
		test1.userId = 2221;
		sm_TestIds.PushAndGrow(test1);

		CTestID test2;
		test2.accountId = 111111111112;
		test2.onlineId.FromString("Test2");
		test2.userId = 2222;
		sm_TestIds.PushAndGrow(test2);

		CTestID test3;
		test3.accountId = 111111111113;
		test3.onlineId.FromString("Test3");
		test3.userId = 2223;
		sm_TestIds.PushAndGrow(test3);

		if(sm_UseTestIdSet < 0 || sm_UseTestIdSet > sm_TestIds.GetCount())
			sm_UseTestIdSet = 0;
	}
#endif // __BANK

	sm_Initialized = true;
}

void ReplayFileManager::Shutdown()
{
	if( !sm_Initialized )
	{
		return;
	}

	StopFileThread();

	//delete compression buffers
	if( sm_CompressionBuffer )
	{
		sysMemManager::GetInstance().GetReplayAllocator()->Free(sm_CompressionBuffer);
		sm_CompressionBuffer = NULL;
	}

	if( sm_Device != NULL )
	{
		fiDevice::Unmount(*sm_Device);
	}

	sm_HeaderCache.CleanUp();

#if !REPLAY_USE_PER_BLOCK_THUMBNAILS
	sm_thumbnailSaveData.Reset();
#endif // !REPLAY_USE_PER_BLOCK_THUMBNAILS
	sm_CurrentReplayFileInfo.Reset();

	sm_LastFileOp = REPLAY_FILEOP_NONE;

	sm_Initialized = false;
}

#if __BANK
void ReplayFileManager::AddDebugWidgets(bkBank& bank)
{
	bank.PushGroup("FileManager", false);

	if( sm_Device )
	{
		bank.AddButton("Add Favourite", MarkAsFavouriteCallback);
		bank.AddButton("Remove Favourite", UnMarkAsFavouriteCallback);
		bank.AddButton("Update Favourites", UpdateFavouritesCallback);

		sm_Device->AddDebugWidgets(bank);
		
		bank.AddSlider("Est Block Save Time (MS)", &sm_estSaveTimePerBlockMs, 0, 10000, sm_estSaveTimePerBlockMs / 10000);
		bank.AddSlider("Est Block Fill Rate (MS)", &sm_estBlockFillRateMS, 0, 10000, sm_estBlockFillRateMS / 10000);
		bank.AddSlider("bytes to write", &sm_BytesToWrite, 1, BYTES_IN_REPLAY_BLOCK, (BYTES_TO_WRITE_PER_ITERATION / BYTES_IN_REPLAY_BLOCK));
		
		bank.AddText("Total Number Of Blocks", &sm_totalBlocks);
		bank.AddText("Free Blocks", &sm_numBlocksFree);
		bank.AddText("Used Blocks", &sm_numBlocksFull);

		bank.AddSlider("Min Number Of Blocks Till Save", &sm_MinNumBlocksTillSave, 0, MAX_NUM_REPLAY_BLOCKS, sm_MinNumBlocksTillSave / MAX_NUM_REPLAY_BLOCKS);

		bank.AddToggle("Simulate Streaming load", &sm_simulateStreamingLoad);
	}

	bank.PopGroup();
}
#endif // __BANK


bool ReplayFileManager::GenerateUniqueClipName(char (&out_buffer)[ REPLAYPATHLENGTH ], ClipUID& uid)
{
	atString date;
	u32 id;
	ReplayHeader::GenerateDateString(date, id);
	uid = id;

	char directory[REPLAYPATHLENGTH];
	getClipDirectory(directory);

	char path[REPLAYPATHLENGTH];
	char name[REPLAYPATHLENGTH];

	u32 uniqueNumber = 1;
	u64 const c_userId = getCurrentUserIDToPrint();

	do 
	{
		name[0] = rage::TEXT_CHAR_TERMINATOR;
		path[0] = rage::TEXT_CHAR_TERMINATOR;

		const char* customPrefix = "";
#if __BANK
		PARAM_replayDebugClipPrefix.Get(customPrefix);
#endif // __BANK

		sprintf_s(name, REPLAYPATHLENGTH, "%s%s-%s-%04d", customPrefix, date.c_str(), TheText.Get("SG_CLIP"), uniqueNumber);

		sprintf_s(path, REPLAYPATHLENGTH, "%s%s.clip", directory, name);

		++uniqueNumber;
	} 
	while ( sm_Device->IsClipFileValid( c_userId, path ) );

	formatf( out_buffer, "%s", name );

	return !sm_Device->IsClipFileValid( c_userId, path );
}

u64 ReplayFileManager::GetAvailableDiskSpace()
{
	if( sm_Device == NULL )
	{
		return 0;
	}

	u64 availableSpace = sm_Device->GetFreeSpaceAvailable();

	return availableSpace;
}

static float compressionThreshold =  0.515f;
const u64 imageSize = (SCREENSHOT_WIDTH * SCREENSHOT_HEIGHT * 4);
bool ReplayFileManager::CheckAvailableDiskSpaceForClips(const u64& uDataSize, const bool& useThreshold, float minThreshold, eReplayMemoryWarning& memoryWarningy)
{
	if(sm_Device == NULL)
	{
		return false;
	}

	memoryWarningy = REPLAY_MEMORYWARNING_NONE;

	u64 availableSpace = sm_Device->GetFreeSpaceAvailableForClips();
	u64 totalSpace = sm_Device->GetMaxMemoryAvailableForClips();
	u64 fileSize = sizeof(ReplayHeader) + sizeof(ReplayBlockHeader) + u64(uDataSize * compressionThreshold) + imageSize;

	//return false with no warning.
	bool hitThreshold = useThreshold && ( availableSpace - fileSize ) < u64(minThreshold * 1024 * 1024);

	if( availableSpace < fileSize || hitThreshold )
	{
		memoryWarningy = REPLAY_MEMORYWARNING_OUTOFMEMORY;
		return false;
	}

	float percentage = totalSpace != 0 ? (float)availableSpace / (float)totalSpace : 0;

	if( percentage <= 0.1f )
	{
		memoryWarningy = REPLAY_MEMORYWARNING_LOW;
	}

	return true;
}

bool ReplayFileManager::CheckAvailableDiskSpaceForMontage(u64 size, const char* path)
{
	if(sm_Device == NULL)
	{
		return false;
	}

	u64 currentSize = path ? sm_Device->GetSizeOfProject(path) : 0;

	u64 availableSpace = sm_Device->GetFreeSpaceAvailableForMontages();

	//if the delta is negative i.e. we've removed clips from a project then clamp at 0 as we're not needing any more memory
	u64 delta = MAX(s64(size - currentSize), 0);

	return (availableSpace >= delta);
}

#if RSG_ORBIS
bool ReplayFileManager::CheckAvailableDiskSpaceForAutoSave()
{
	if(sm_Device == NULL)
	{
		return false;
	}

	return static_cast<FileStoreOrbis*>(sm_Device)->IsEnoughSpaceForAutoSave();
}
#endif

void ReplayFileManager::UpdateAvailableDiskSpace(bool clipUpdated)
{
	if(sm_Device == NULL)
	{
		return;
	}

	if (clipUpdated)
	{
		sm_Device->SetShouldUpdateClipsSize();
	}
	else
	{
		// force calculations if not a clip. minimises time doing this in-game
		sm_Device->SetShouldUpdateDirectorySize();
	}

	GetAvailableDiskSpace();
}

#if RSG_ORBIS
void ReplayFileManager::RefreshNonClipDirectorySizes()
{
	if(sm_Device == NULL)
	{
		return;
	}

	static_cast<FileStoreOrbis*>(sm_Device)->RefreshNonClipDirectorySizes();
}
#endif

void ReplayFileManager::AddClipToProjectCache(const char* projectPath, u64 const userId, ClipUID const& uid )
{
	if(sm_Device == NULL)
	{
		return;
	}

	sm_Device->AddClipToProjectCache(projectPath, userId, uid);
}

void ReplayFileManager::RemoveClipFromProjectCache( const char* projectPath, u64 userId, ClipUID const& uid )
{
	if(sm_Device == NULL)
	{
		return;
	}

	sm_Device->DeleteClipFromProjectCache( projectPath, userId, uid );
}

void ReplayFileManager::AddProjectToCache(const char* path, u64 const userId, u64 const size, u32 const duration )
{
	if(sm_Device == NULL)
	{
		return;
	}

	sm_Device->AddProjectToCache( path, userId, size, duration );
}

bool ReplayFileManager::IsProjectInProjectCache( const char* path, u64 userId )
{
	return (sm_Device == NULL) ? false : sm_Device->IsProjectInProjectCache(path, userId);
}

void ReplayFileManager::RemoveCachedProject( const char* path, u64 const userId )
{
	if(sm_Device == NULL)
	{
		return;
	}

	sm_Device->RemoveCachedProject(path, userId);
}

bool ReplayFileManager::IsClipInAProject(const ClipUID& clipUID)
{
	if(sm_Device == NULL)
	{
		return false;
	}

	return sm_Device->IsClipInAProject(clipUID);
}

u64 ReplayFileManager::getCurrentUserIDToPrint()
{
#if DO_CUSTOM_WORKING_DIR
	bool bCustomFolder = PARAM_replayFilesWorkingDir.Get();
	if( bCustomFolder )
	{
		return 0;
	}
#endif 

#if RSG_XENON || RSG_DURANGO
	return NetworkInterface::GetLocalGamerHandle().GetXuid();
#elif RSG_PS3 // Don't think this is a thing? (Replay not on PS3)
 	s32 userID = g_rlNp.GetUserServiceId(NetworkInterface::GetLocalGamerIndex());
 	return static_cast<u64>(userID + (UINT_MAX >> 1));
#elif RSG_ORBIS
	rlSceNpAccountId acc = GetCurrentAccountId();

	// Convert the accountID to the OnlineID that we stored in the map
	// This might be the current userId or it might be an old one if the user has changed their name
	u64 userIdFromAcc = GetOnlineIdFromAccountId(acc);
	replayDebugf1("getCurrentUserIDToPrint() - Acc:%" I64FMT "u User:%" I64FMT "u", acc, userIdFromAcc);
	if(userIdFromAcc == 0)
	{
		s32 userID = GetCurrentUserId();
		replayDebugf1("No mapped account Ids...falling back to user service ID %d", userID);
		return static_cast<u64>(userID + (UINT_MAX >> 1));
	}
	return userIdFromAcc;
#else
	return 0;
#endif
}

u64 ReplayFileManager::getUserIdFromPath( char const * const path )
{
	return fiDeviceReplay::GetUserIdFromPath( path );
}

bool ReplayFileManager::appendUserIDDirectoryName(const char* baseDir, char (&out_buffer)[ REPLAYPATHLENGTH ], u64 userIDToForce)
{
#if DO_CUSTOM_WORKING_DIR
	bool bCustomFolder = PARAM_replayFilesWorkingDir.Get();
	if( bCustomFolder )
	{
		return false;
	}
#endif 

	bool idFound = false;

#if RSG_ORBIS || RSG_DURANGO
#if !__FINAL
	const char* sCustomProfileID;
	bool bCustomProfileID = PARAM_useCustomProfileID.Get(sCustomProfileID);
	if(bCustomProfileID)
	{
		u64 uCustomProfileID;
		sscanf(sCustomProfileID,"%" I64FMT "u", &uCustomProfileID);
		formatf_n(out_buffer, REPLAYPATHLENGTH,  "%s%" I64FMT "u/", baseDir, uCustomProfileID );
		idFound = true;
	}
	else
#endif
	{
		u64 reportedId = userIDToForce ? userIDToForce : getCurrentUserIDToPrint();
		if (reportedId != 0)
		{
			formatf_n(out_buffer, REPLAYPATHLENGTH,  "%s%" I64FMT "u/", baseDir, reportedId );
			idFound = true;
		}
	}
#else
	// not doing this for PC, but if we did we could use RockstarId ...perhaps make a fake if not on SC
	// NetworkInterface::GetLocalGamerHandle().GetRockstarId();
	UNUSED_VAR(baseDir);
	UNUSED_VAR(out_buffer);
	UNUSED_VAR(userIDToForce);
#endif
	return idFound;
}


#if RSG_ORBIS
const char* OLD_COOKIE = "psn.txt";
const char* NEW_COOKIE = "psn2.txt";
const int NEWCOOKIEDATALENGTH = 64;

#if __BANK
void ReplayFileManager::PrintDir(const char* path, bool allowedDeletion, int depth)
{
	static bool deleteOldCookie = false;
	static bool deleteNewCookie = false;

	if(PARAM_replayDeleteNewCookiesFirst.Get())
	{
		deleteNewCookie = allowedDeletion;
	}

	atArray<atString> oldCookiesFound;
	atArray<atString> newCookiesFound;

	fiFindData findData;
	fiHandle hFileHandle = fiHandleInvalid;
	bool validDataFound = true;
	hFileHandle = sm_Device->FindFileBegin(path, findData);
	replayAssertf(fiIsValidHandle(hFileHandle), "Failed to FindFileBegin on %s", path);
	for( ;
		fiIsValidHandle( hFileHandle ) && validDataFound; 
		validDataFound = sm_Device->FindFileNext( hFileHandle, findData ) )
	{
		if((findData.m_Attributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)
		{
			s32 nameLength = static_cast<s32>(strlen(findData.m_Name));
			if (nameLength > REPLAYDIRMINLENGTH)
			{
				replayDebugf1("%*s%s", depth, "", findData.m_Name);
				atString subdir(path);
				subdir += "/";
				subdir += findData.m_Name;
				PrintDir(subdir.c_str(), allowedDeletion, depth+1);
			}
		}
		else
		{
			replayDebugf1("%*s %s", depth, "", findData.m_Name);
			if(strstr(findData.m_Name, "psn") != nullptr)
			{
				atString fullPathAndFilename(path);
				fullPathAndFilename += "/";
				fullPathAndFilename += findData.m_Name;
				fiStream* stream = ASSET.Open(fullPathAndFilename.c_str(), "txt");
				if(replayVerifyf(stream, "PrintDir: Failed to open stream %s", fullPathAndFilename.c_str()))
				{
					char PSNIdString[NEWCOOKIEDATALENGTH + 1] = {0};
					stream->Read(&PSNIdString[0], sizeof(char)*NEWCOOKIEDATALENGTH);
					stream->Close();
					replayDebugf1("%*s   Contents:%s", depth, "", PSNIdString);
				}

				if(deleteOldCookie && strstr(findData.m_Name, OLD_COOKIE))
				{
					oldCookiesFound.PushAndGrow(fullPathAndFilename);
				}
				else if(deleteNewCookie && strstr(findData.m_Name, NEW_COOKIE))
				{
					newCookiesFound.PushAndGrow(fullPathAndFilename);
				}
			}
			
		}
	}
	sm_Device->FindFileEnd(hFileHandle);

	for(int i = 0; i < oldCookiesFound.GetCount(); ++i)
	{
		sm_Device->Delete(oldCookiesFound[i].c_str());
	}

	if(deleteNewCookie && newCookiesFound.GetCount() > 0)
	{
		for(int i = 0; i < newCookiesFound.GetCount(); ++i)
		{
			replayDebugf1("%*s ***Deleting new cookie %s***", depth, "", newCookiesFound[i].c_str());
			sm_Device->Delete(newCookiesFound[i].c_str());
		}
	}
}


void ReplayFileManager::PrintFileContents(bool allowedDeletion)
{
	replayDebugf1("\n******************************");
	replayDebugf1("Printing contents for %s\n", REPLAY_ROOT_PATH);

	PrintDir(REPLAY_ROOT_PATH, allowedDeletion);

	replayDebugf1("\n******************************\n");
}
#endif // __BANK

const char* GetOnlineIdStrFromPath(const char* path)
{
	// This path 'should' be "videos/data/<onlineId>"
	atString pathString(path);
	replayAssert(pathString.EndsWith("/") == false);
	while(pathString.EndsWith("/"))
	{
		pathString.Truncate(pathString.GetLength() - 1);
	}

	int i = pathString.LastIndexOf("/");
	i += strlen("/");
	return &(path[i]);
}

u64 GetOnlineIdFromStr(const char* s)
{
	return strtoul(s, nullptr, 0);
}

void ReplayFileManager::FixOldCookies(const char* path, bool& notReady, bool topDir)
{
	const u32 timeoutValue = 10000;
	if(sm_vitalClipFolderProcessFailed)
	{
		notReady = false;
		/*return;*/
	}

#if __BANK
	// Force to fail after only fixing up one cookie
	if(PARAM_replayObtainOnlyOneAccount.Get())
	{
		if(sm_AccountsQueried > 0)
		{
			replayDebugf1("*****Force Only getting one account*****");
			sm_vitalClipFolderProcessFailed = true;
			notReady = false;
			/*return;*/
		}
	}
#endif // __BANK

	if(!sm_AccountIdStatus.None())
	{
#if __BANK
		if(PARAM_replayForceAccountFromUserFail.Get())
		{
			replayDebugf1("*****Force failing account from user*****");
			sm_vitalClipFolderProcessFailed = true;
			notReady = false;
			return;
		}

		// Fake set minimum time account retrieval can take
		int delay = 0;
		if(PARAM_replayFakeDelayAccountRetrieval.Get(delay))
		{
			if(sysTimer::GetSystemMsTime() - sm_timeAccountMappingKicked < delay)
			{
				replayDebugf1("*****Fake delay set %d, still waiting...%d *****", delay, sysTimer::GetSystemMsTime() - sm_timeAccountMappingKicked);
				notReady = true;
				return;
			}
		}
#endif // __BANK

		// If account mapping process completed then continue
		BANK_ONLY(bool alwaysPending = PARAM_replayAccountIDAlwaysPending.Get());
		if(sm_AccountIdStatus.Succeeded() BANK_ONLY(&& !alwaysPending))
		{
#if __BANK
			int maxVal = 1;
			if(PARAM_replayRandomlyFailAccountRetrieval.Get(maxVal))
			{
				mthRandom random;
				int result = random.GetRanged(0, maxVal);
				if(result != 0)
				{
					replayDebugf1("*****Failed with fake random to get account retrieval*****");
					notReady = false;
					sm_vitalClipFolderProcessFailed = true;
					return;
				}
			}
#endif // __BANK
			replayDebugf1("Successfully found accountID %" I64FMT "u", sm_AccountId);
			
			sm_AccountIdStatus.Reset();
			sm_timeAccountMappingKicked = 0;
		}
		else if(sm_timeAccountMappingKicked != 0)
		{
			if(sysTimer::GetSystemMsTime() - sm_timeAccountMappingKicked >= timeoutValue || sm_AccountIdStatus.Failed() || sm_AccountIdStatus.Canceled())
			{
				replayDebugf1("*****Failed to get account ID.... %u, kicked %u, Status: %d*****", sysTimer::GetSystemMsTime(), sm_timeAccountMappingKicked, sm_AccountIdStatus.GetStatus());
				replayDebugf1("*****Note: This could be due to running on an account from a different environment to the game...check this!*****");
				replayAssertf(false, "Failed to get account ID.  Could be you're running with an account from a different environment to the game?");
				sm_vitalClipFolderProcessFailed = true; // This is the error state.  If we hit this then recording is not allowed
				notReady = false;

				return;
			}
			else
			{
				replayDebugf1("Waiting for Account Mapping to complete... (kicked %u) %u", sm_timeAccountMappingKicked, sysTimer::GetSystemMsTime() - sm_timeAccountMappingKicked);
				notReady = true;
				return;
			}
		}
	}

	// Search the directory structure for the cookie files...
	bool foundOldCookie = false;
	bool foundNewCookie = false;
	fiFindData findData;
	fiHandle hFileHandle = fiHandleInvalid;
	bool validDataFound = true;
	for( hFileHandle = sm_Device->FindFileBegin( path, findData );
		fiIsValidHandle( hFileHandle ) && validDataFound; 
		validDataFound = sm_Device->FindFileNext( hFileHandle, findData ) )
	{
		if((findData.m_Attributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)
		{
			s32 nameLength = static_cast<s32>(strlen(findData.m_Name));
			if (nameLength > REPLAYDIRMINLENGTH && notReady == false)
			{
				replayDebugf1(" %s", findData.m_Name);
				atString subdir(path);
				subdir += "/";
				subdir += findData.m_Name;
				FixOldCookies(subdir.c_str(), notReady);
			}
		}
		else
		{
			if(strstr(findData.m_Name, OLD_COOKIE) != nullptr)
			{
				foundOldCookie = true;
			}
			else if(strstr(findData.m_Name, NEW_COOKIE) != nullptr)
			{
				foundNewCookie = true;
			}
		}
	}
	sm_Device->FindFileEnd(hFileHandle);

	// Only process if we've found an old cookie, but not found a new one
	// This means we need to obtain the correct corresponding accountId from Sony
	// and write this into a new cookie file
	BANK_ONLY(if(!PARAM_replayObtainOnlyOneAccount.Get() || sm_AccountsQueried < 1))
	{
		if(foundOldCookie && !foundNewCookie && !sm_vitalClipFolderProcessFailed)
		{
			const char* pOnlineIdStr = GetOnlineIdStrFromPath(path);
			u64 onlineIdFromPath = GetOnlineIdFromStr(pOnlineIdStr);

			replayDebugf1("Found old cookie, but not new cookie in %s... (pOnlineIdStr %s)", path, pOnlineIdStr);

			// Read the contents of the old cookie
			atString fullPathAndFilenameOld(path);
			fullPathAndFilenameOld += "/";
			fullPathAndFilenameOld += OLD_COOKIE;

			fiStream* stream = ASSET.Open(fullPathAndFilenameOld.c_str(), "txt");
			char PSNIdString[SCE_NP_ONLINEID_MAX_LENGTH + 1] = {0};
			if(replayVerifyf(stream, "FixOldCookies: Failed to open stream %s", fullPathAndFilenameOld.c_str()))
			{
				stream->Read(&PSNIdString[0], sizeof(char)*SCE_NP_ONLINEID_MAX_LENGTH);
				stream->Close();
			}

			// So do we have the corresponding accountId for this onlineId?
			if(/*sm_AccountIdStatus.Succeeded() &&*/ sm_CurrentOnlineIdProcessing == onlineIdFromPath)
			{
				// If so create a new cookie...
				char AccountID[32] = {0};
				rlSceNpAccountId acc = sm_AccountId;
				sprintf(AccountID, "%" I64FMT "u", acc);
				replayDebugf1("Old id %s ...new %s %" I64FMT "u", PSNIdString, AccountID, acc);

				atString fullPathAndFilenameNew(path);
				fullPathAndFilenameNew += "/";
				fullPathAndFilenameNew += NEW_COOKIE;
				replayDebugf1("Writing new cookie @ %s containing %s", fullPathAndFilenameNew.c_str(), AccountID);
				WriteCookie(fullPathAndFilenameNew.c_str(), AccountID);

				// Delete the old cookie
				// Do we want to bother deleting it?  Once it's gone it's gone...
				//sm_Device->Delete(fullPathAndFilenameOld.c_str());

				foundNewCookie = true;

				// Reset these to be reused
				sm_CurrentOnlineIdProcessing = 0;
				sm_AccountId = 0;
				++sm_AccountsQueried;
				/*sm_AccountIdStatus.Reset();*/
			}
			else if(sm_AccountIdStatus.None()) // No accountId currently processing?
			{
				// Kick off the account mapping process and return...we'll be back to check
				sm_timeAccountMappingKicked = sysTimer::GetSystemMsTime();
				replayDebugf1("Attempting to get AccountID... - OnlineID %" I64FMT "u", onlineIdFromPath);
				sm_CurrentOnlineIdProcessing = onlineIdFromPath;
				RetrieveAccountIdFromOnlineId(PSNIdString);

				notReady = true;
				return;
			}
			else
			{
				replayAssertf(false, "Shouldn't get here?");
				notReady = true;
				return;
			}
		}
	}

	// If we've count a cookie containing the AccountID then we need to find the 
	// onlineId in the path and add these to the map in the device
	if(foundNewCookie)
	{
		MapAccountIdToOwnerId(path);
	}
}

void ReplayFileManager::MapAccountIdToOwnerId(const char* path)
{
	u64 onlineId = 0;
	u64 accountId = 0;

	// This path 'should' be "videos/data/<onlineId>"
	atString pathString(path);
	while(pathString.EndsWith("/")) // Remove trailing slashes
	{
		pathString.Truncate(pathString.GetLength() - 1);
	}

	int i = pathString.LastIndexOf("/");
	i += strlen("/");
	onlineId = strtoul(&(pathString.c_str()[i]), nullptr, 0); // This bit should be the 'onlineId'

	if(GetAccountIdFromOnlineId(onlineId) == 0)
	{
		// Now append the cookie filename, open it and read the accountId out of it
		pathString += "/";
		pathString += NEW_COOKIE;
		fiStream* stream = ASSET.Open(pathString.c_str(), "txt");
		if(replayVerifyf(stream, "MapAccountIdToOwnerId: Failed to open stream %s", pathString.c_str()))
		{
			char PSNIdString[NEWCOOKIEDATALENGTH + 1] = {0};
			stream->Read(&PSNIdString[0], sizeof(char)*NEWCOOKIEDATALENGTH);
			accountId = strtoul(PSNIdString, nullptr, 0);
			stream->Close();
		}

		replayDebugf1("MapAccountIdToOwnerId: %s AccountId %" I64FMT "u, OnlineId %" I64FMT "u  %s", path, accountId, onlineId, pathString.c_str());
		if(replayVerifyf(onlineId != 0 && accountId != 0, "Failed to find either the onlineId or the accountId"))
		{
			// Add these to the map in the device
			// We can use this during replay usage to find the correct folder given the accountId
			AddAccountIdToOnlineId(accountId, onlineId);
		}
	}
}

bool ReplayFileManager::UpdateUserIDDirectoriesOnChange()
{
#if DO_CUSTOM_WORKING_DIR
	bool bCustomFolder = PARAM_replayFilesWorkingDir.Get();
	if( bCustomFolder )
	{
		return true;
	}
#endif 

	bool UseAccountID = sm_ShouldRemapNpOnlineIdsToAccountIds && rlGamerHandle::IsUsingAccountIdAsKey();

	if(PARAM_replayDontUseAccountID.Get())
	{
		UseAccountID = false;
	}

	replayDebugf1("Begin UpdateUserIDDirectoriesOnChange...");

#if __BANK
	static bool printOnce = true;
	if(printOnce && !PARAM_replayDontEnumerateDownload0Prior.Get())
	{
		PrintFileContents(true);
		printOnce = false;
	}
#endif // __BANK

	// only need to do this when the local Id changes
	s32 userID = GetCurrentUserId();
	if (userID == sm_CurrentLocalUserId)
	{
		replayDebugf1("User ID not changed so no need to go further");
		return true;
	}

	// only need to do this check if online, as we're checking for PSN Ids with new local Ids
	if (!CLiveManager::IsOnline())
	{
		replayDebugf1("Not online so can't proceed");
		return true;
	}

	// no PSNId? ... not worth doing then
	rlSceNpOnlineId onlineId = GetCurrentOnlineId();
	if (!onlineId.IsValid())
	{
		replayDebugf1("Failed to get local gamer id");
		return true;
	}

	// Try to get the current Account ID
	rlSceNpAccountId acc = GetCurrentAccountId();
	if(acc == 0)
	{
		replayDebugf1("Failed to get Account ID...");
		return true;
	}
	char PSNAccountID[NEWCOOKIEDATALENGTH] = {0};
	sprintf(PSNAccountID, "%" I64FMT "u", acc);
	

	u64 localIdFormattedForDirectoryName = static_cast<u64>(userID + (UINT_MAX >> 1));

	replayDebugf1("userID = %d - %" I64FMT "u", userID, localIdFormattedForDirectoryName);
	replayDebugf1("AccountID = %s (%s)", PSNAccountID, onlineId.data);

	if(UseAccountID)
	{
		// If this succeeds then the local user hasn't changed and we've just fixed up the cookie...all good
		replayDebugf1("===Fix old cookies...===");
		bool notReady = false;
		FixOldCookies(REPLAY_DATA_PATH, notReady, true);
		if(notReady)
		{
			// Can't enumerate yet as we've not fixed folders
			return false;
		}
	}
	else
	{
		if(PARAM_replayDontUseAccountID.Get())
			replayDebugf1("===Skipping fixing old cookies due to replayDontUseAccountID===");
		else
			replayDebugf1("===Skipping fixing old cookies due to ShouldRemapNpOnlineIdsToAccountIds tunable===");
	}

	sm_vitalClipFolderProcessNeeded = false;

	sm_CurrentLocalUserId = userID;

#if __BANK
	if(!PARAM_replayDontEnumerateDownload0Post.Get())
	{
		PrintFileContents(false);
	}
#endif // __BANK

	const char* pIDToUse = PSNAccountID;
	const char* pCookieToUse = NEW_COOKIE;
	if(!UseAccountID)
	{
		pIDToUse = onlineId.data;
		pCookieToUse = OLD_COOKIE;
	}
	replayDebugf1("Using %s, %s", pIDToUse, pCookieToUse);
	
	// if the info that the PSN Id belongs to this Local Id exists, then cool ... we don't need to do anything
	if (IsPSNIdInGivenLocalId(pIDToUse, localIdFormattedForDirectoryName, pCookieToUse))
		return true;

	// if we got this far, then check to see if PSNId exists for any other user
	u64 otherUserID = 0;
	if (IsPSNIdInAnyLocalId(pIDToUse, &otherUserID, pCookieToUse))
	{
		RenameLocalIdDirectories(otherUserID, localIdFormattedForDirectoryName);
		return true;
	}

	// if it can't be found, it must be the first time this PSN has been used
	// make a data directory and stick in a txt file with this PSN
	CreatePSNIdFileInGivenLocalId(pIDToUse, localIdFormattedForDirectoryName, pCookieToUse, UseAccountID);

	return true;
}

bool ReplayFileManager::IsPSNIdInGivenPath(const char* PSNId, const char* path, const char* cookie)
{
	char fullPath[REPLAYPATHLENGTH];
	ReplayFileManager::FixName(fullPath, path);

	if ( ASSET.CreateLeadingPath( path ) )
	{
		atString fullPathAndFilename;
		fullPathAndFilename = fullPath;
		fullPathAndFilename += cookie;

		if ( ASSET.Exists( fullPathAndFilename.c_str(), "txt" ) )
		{
			fiStream* stream = ASSET.Open(fullPathAndFilename.c_str(), "txt");
			if(replayVerifyf(stream, "IsPSNIdInGivenPath: Failed to open stream %s", fullPathAndFilename.c_str()))
			{
				char PSNIdString[NEWCOOKIEDATALENGTH + 1] = {0};
				stream->Read(&PSNIdString[0], sizeof(char)*NEWCOOKIEDATALENGTH);
				stream->Close();

				if (strcmp(PSNIdString, PSNId) == 0)
				{
					return true;
				}
			}
		}
	}

	return false;
}

void ReplayFileManager::SetShouldRemapNpOnlineIdsToAccountIds(const bool shouldRemap)
{
    if(sm_ShouldRemapNpOnlineIdsToAccountIds != shouldRemap)
    {
        replayDebugf1("SetShouldRemapNpOnlineIdsToAccountIds :: %s", shouldRemap ? "True" : "False");
        sm_ShouldRemapNpOnlineIdsToAccountIds = shouldRemap;
    }
}

bool ReplayFileManager::WriteCookie(const char* fullPathAndFilename, const char* cookieValue)
{
	fiStream* stream = ASSET.Create(fullPathAndFilename, "txt");
	if (replayVerifyf(stream, "WriteCookie: Failed to create %s", fullPathAndFilename))
	{
		int bytesWritten = stream->Write(cookieValue, sizeof(char)*NEWCOOKIEDATALENGTH);
		stream->Close();

		stream = ASSET.Open(fullPathAndFilename, "txt");
		char PSNIdString[NEWCOOKIEDATALENGTH + 1] = {0};
		if(replayVerifyf(stream, "WriteCookie: Failed to open stream %s", fullPathAndFilename))
		{
			stream->Read(&PSNIdString[0], sizeof(char)*NEWCOOKIEDATALENGTH);
			stream->Close();

			replayAssertf(strcmp(cookieValue, PSNIdString) == 0, "Reading (%s) a different string to that written (%s)", PSNIdString, cookieValue);
		}
		return (bytesWritten > 0);
	}
	return false;
}

bool ReplayFileManager::CreatePSNIdFileInGivenLocalId(const char* PSNId, u64 localId, const char* cookie, bool useAccountID)
{
	char path[REPLAYPATHLENGTH] = {0};
	if (appendUserIDDirectoryName(REPLAY_DATA_PATH, path, localId))
	{
		if ( ASSET.CreateLeadingPath( path ) )
		{
			atString fullPathAndFilename;
			fullPathAndFilename = path;
			fullPathAndFilename += cookie;

			WriteCookie(fullPathAndFilename.c_str(), &PSNId[0]);

			if(useAccountID)
			{
				MapAccountIdToOwnerId(path);
			}
		}
	}

	return false;
}

bool ReplayFileManager::IsPSNIdInGivenLocalId(const char* PSNId, u64 localId, const char* cookie)
{
	// initially check current Id for PSN file
	char path[REPLAYPATHLENGTH] = {0};
	if (appendUserIDDirectoryName(REPLAY_DATA_PATH, path, localId))
	{
		return IsPSNIdInGivenPath(PSNId, path, cookie);
	}

	return false;
}

bool ReplayFileManager::IsPSNIdInAnyLocalId(const char* PSNId, u64* localIdForPSN, const char* cookie)
{
	char path[REPLAYPATHLENGTH] = {0};
	getDataDirectory(path, false);

	fiFindData findData;
	fiHandle hFileHandle = fiHandleInvalid;
	bool validDataFound = true;
	for( hFileHandle = sm_Device->FindFileBegin( path, findData );
		fiIsValidHandle( hFileHandle ) && validDataFound; 
		validDataFound = sm_Device->FindFileNext( hFileHandle, findData ) )
	{
		if((findData.m_Attributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)
		{
			// make sure directory is a proper user directory and not just '.' or '..'
			s32 nameLength = static_cast<s32>(strlen(findData.m_Name));
			if (nameLength > REPLAYDIRMINLENGTH)
			{
				char userPath[REPLAYPATHLENGTH] = {0};
				formatf_n(userPath,REPLAYPATHLENGTH, "%s%s/", REPLAY_DATA_PATH, findData.m_Name );
				if (IsPSNIdInGivenPath(PSNId, userPath, cookie))
				{
					*localIdForPSN =  fiDeviceReplay::GetUserIdFromPath(userPath);
					return true;
				}
			}
		}
	}
	sm_Device->FindFileEnd(hFileHandle);

	return false;
}

void ReplayFileManager::RenameLocalIdDirectories(u64 oldLocalId, u64 newLocalId)
{
	char basePath[REPLAYPATHLENGTH] = {0};
	char oldPath[REPLAYPATHLENGTH] = {0};
	char newPath[REPLAYPATHLENGTH] = {0};

	getDataDirectory(basePath, false);
	appendUserIDDirectoryName(basePath, oldPath, oldLocalId);
	if ( ASSET.Exists( oldPath, "" ) )
	{
		appendUserIDDirectoryName(basePath, newPath, newLocalId);
		sm_Device->Rename(oldPath, newPath);
	}

	getClipDirectory(basePath, false);
	appendUserIDDirectoryName(basePath, oldPath, oldLocalId);
	if ( ASSET.Exists( oldPath, "" ) )
	{
		appendUserIDDirectoryName(basePath, newPath, newLocalId);
		sm_Device->Rename(oldPath, newPath);
	}
	
	getVideoProjectDirectory(basePath, false);
	appendUserIDDirectoryName(basePath, oldPath, oldLocalId);
	if ( ASSET.Exists( oldPath, "" ) )
	{
		appendUserIDDirectoryName(basePath, newPath, newLocalId);
		sm_Device->Rename(oldPath, newPath);
	}
}

rlSceNpOnlineId ReplayFileManager::GetCurrentOnlineId()
{
#if __BANK
	if(sm_UseTestIdSet != -1)
	{
		return sm_TestIds[sm_UseTestIdSet].onlineId;
	}
#endif // __BANK
	return g_rlNp.GetNpOnlineId(NetworkInterface::GetLocalGamerIndex());
}

s32 ReplayFileManager::GetCurrentUserId(int gamerIndex)
{
#if __BANK
	if(sm_UseTestIdSet != -1)
	{
		return sm_TestIds[sm_UseTestIdSet].userId;
	}
#endif // __BANK
	return g_rlNp.GetUserServiceId(gamerIndex == -1 ? NetworkInterface::GetLocalGamerIndex() : gamerIndex);
}

u64 ReplayFileManager::GetCurrentAccountId()
{
#if __BANK
	if(sm_UseTestIdSet != -1)
	{
		return sm_TestIds[sm_UseTestIdSet].accountId;
	}

	if(PARAM_replayFailGetCurrentAccountID.Get())
	{
		return 0;
	}
#endif // __BANK
	return NetworkInterface::GetLocalGamerHandle().GetNpAccountId();
}

void ReplayFileManager::AddAccountIdToOnlineId(u64 accountId, u64 onlineId)
{
	replayDebugf1("**Adding mapping account %" I64FMT "u to onlineID %" I64FMT "u**", accountId, onlineId);
	sm_AccountIdToOnlineId[accountId] = onlineId;
}

u64 ReplayFileManager::GetOnlineIdFromAccountId(u64 accountId)
{
	if(sm_AccountIdToOnlineId.Access(accountId))
		return *sm_AccountIdToOnlineId.Access(accountId);
	return 0;
}

u64 ReplayFileManager::GetAccountIdFromOnlineId(u64 onlineId)
{
	atMap<u64, u64>::/*Const*/Iterator iter = sm_AccountIdToOnlineId.CreateIterator();
	while(!iter.AtEnd())
	{
		if(iter.GetData() == onlineId)
			return iter.GetKey();

		iter.Next();
	}

	return 0;
}

void ReplayFileManager::RetrieveAccountIdFromOnlineId(const char* onlineId)
{
#if __BANK
	if(sm_UseTestIdSet != -1)
	{
		for(CTestID& i : sm_TestIds)
		{
			if(strcmp(onlineId, i.onlineId.data) == 0)
			{
				sm_AccountId = i.accountId;
				sm_AccountIdStatus.SetPending();
				sm_AccountIdStatus.SetSucceeded();
				return;
			}
		}
		sm_AccountIdStatus.SetPending();
		sm_AccountIdStatus.SetFailed();
		return;
	}
#endif // __BANK

	rlSceNpOnlineId onlineIdToProcess;
	onlineIdToProcess.FromString(onlineId);
	// It doesn't matter that the local gamer index passed in may be unrelated to the online Id
	g_rlNp.GetWebAPI().GetAccountId(NetworkInterface::GetLocalGamerIndex(), onlineIdToProcess, &sm_AccountId, &sm_AccountIdStatus);
}
#endif

void ReplayFileManager::TriggerFileRequestThread()
{
#if RSG_ORBIS
	// Worth running this before any request ...normally this will early out really quick
	ReplayFileManager::UpdateUserIDDirectoriesOnChange();
#endif

	// Trigger the fileOp thread to begin working
	sysIpcSignalSema(sm_TriggerFileRequest);
}


bool ReplayFileManager::GetMemLimitInfo(eReplayMemoryLimit limit, MemLimitInfo& info)
{ 
	if( sm_Device )
	{
		info = sm_Device->GetMemLimitInfo(limit);

		return true;
	}
	
	return false; 
}

bool ReplayFileManager::getFreeSpaceAvailableForVideos( size_t& out_sizeBytes )
{
	out_sizeBytes = sm_Device ? sm_Device->GetFreeSpaceAvailableForVideos() : 0;
	bool const c_success = sm_Device ? true : false;
	return c_success;
}

bool ReplayFileManager::getFreeSpaceAvailableForClips( size_t& out_sizeBytes )
{
	out_sizeBytes = sm_Device ? sm_Device->GetFreeSpaceAvailableForClips() : 0;
	bool const c_success = sm_Device ? true : false;
	return c_success;
}

bool ReplayFileManager::getUsedSpaceForClips( size_t& out_sizeBytes )
{
	out_sizeBytes = sm_Device ? sm_Device->GetUsedSpaceForClips() : 0;
	bool const c_success = sm_Device ? true : false;
	return c_success;
}

void ReplayFileManager::getClipDirectoryForUser( u64 const userId, char (&out_buffer)[ REPLAYPATHLENGTH ] )
{
#if DO_CUSTOM_WORKING_DIR
	const char* replayFolder;
	bool bCustomFolder = PARAM_replayFilesWorkingDir.Get(replayFolder);
	if (bCustomFolder)
	{
		formatf_n(out_buffer,REPLAYPATHLENGTH, "%s%s", replayFolder, "videos\\clips\\" );
		return;
	}
#	if __BANK
	else if (CReplayMgrInternal::sm_bUseCustomWorkingDir)
	{
		formatf_n(out_buffer,REPLAYPATHLENGTH, "%s%s", CReplayMgrInternal::sm_customReplayPath, "clips\\" );
		return;
	}
#	endif // __BANK
#endif // DO_CUSTOM_WORKING_DIR

#if DISABLE_PER_USER_STORAGE

	UNUSED_VAR( userId );
	formatf_n(out_buffer,REPLAYPATHLENGTH, "%s", REPLAY_CLIPS_PATH );

#else

	appendUserIDDirectoryName( REPLAY_CLIPS_PATH, out_buffer, userId );

#endif

}

void ReplayFileManager::getClipDirectory(char (&out_buffer)[ REPLAYPATHLENGTH ], bool userOnly)
{
#if !DISABLE_PER_USER_STORAGE
	if (userOnly)
	{
		if (appendUserIDDirectoryName(REPLAY_CLIPS_PATH, out_buffer))
			return;
	}
#else
	UNUSED_VAR(userOnly);
#endif // !DISABLE_PER_USER_STORAGE

	formatf_n(out_buffer,REPLAYPATHLENGTH, "%s", REPLAY_CLIPS_PATH );
}

void ReplayFileManager::getVideoProjectDirectory(char (&out_buffer)[ REPLAYPATHLENGTH ], bool userOnly)
{
#if !DISABLE_PER_USER_STORAGE
	if (userOnly)
	{
		if (appendUserIDDirectoryName(REPLAY_MONTAGE_PATH, out_buffer))
			return;
	}
#else
	UNUSED_VAR(userOnly);
#endif // !DISABLE_PER_USER_STORAGE

	formatf_n(out_buffer,REPLAYPATHLENGTH, "%s", REPLAY_MONTAGE_PATH );
}

void ReplayFileManager::getVideoDirectory(char (&out_buffer)[ REPLAYPATHLENGTH ], bool userOnly)
{
#if !DISABLE_PER_USER_STORAGE
	if (userOnly)
	{
		if (appendUserIDDirectoryName(REPLAY_VIDEOS_PATH, out_buffer))
			return;
	}
#else
	UNUSED_VAR(userOnly);
#endif // !DISABLE_PER_USER_STORAGE

	formatf_n(out_buffer,REPLAYPATHLENGTH, "%s", REPLAY_VIDEOS_PATH );
}

void ReplayFileManager::getDataDirectory(char (&out_buffer)[ REPLAYPATHLENGTH ], bool userOnly)
{
#if !DISABLE_PER_USER_STORAGE
	if (userOnly)
	{
		if (appendUserIDDirectoryName(REPLAY_DATA_PATH, out_buffer))
			return;
	}
#else
	UNUSED_VAR(userOnly);
#endif // !DISABLE_PER_USER_STORAGE

	formatf_n(out_buffer,REPLAYPATHLENGTH, "%s", REPLAY_DATA_PATH );
}

void ReplayFileManager::FixName(char (&out_buffer)[ REPLAYPATHLENGTH ], const char* source)
{
	if (!source)
		return;
	const fiDevice* pDevice = fiDevice::GetDevice(source, false);
	pDevice->FixRelativeName(out_buffer, REPLAYPATHLENGTH, source);
}

void ReplayFileManager::InitVideoOutputPath()
{
#if (RSG_PC || RSG_DURANGO)
#if defined(VIDEO_RECORDING_ENABLED) && VIDEO_RECORDING_ENABLED
	char path[RAGE_MAX_PATH];
	ReplayFileManager::getVideoDirectory(path);
	char platformPath[RAGE_MAX_PATH];
	ReplayFileManager::FixName(platformPath, path);

	const char* outputPathForVideo = &platformPath[0];

	// paths from replayfilemanager tend to have slashes at the end...remove to suit encoder's path making
	s32 stringLength = static_cast<s32>(strlen(platformPath));
	if (stringLength > 0)
	{
		// paths from replayfilemanager tend to have slashes at the end...remove to suit encoder's path making
		if (platformPath[stringLength-1] == '/' || platformPath[stringLength-1] == '\\')
			platformPath[stringLength-1] = '\0';
	}

	VideoRecording::InitializeExtraRecordingData( outputPathForVideo );
#endif
#endif // (RSG_PC || RSG_DURANGO)
}

void ReplayFileManager::UpdateHeaderCache(const char* pFileName, const ReplayHeader* pReplayHeader)
{
	bool exists = sm_HeaderCache.Exists(pFileName);

	if( pReplayHeader == NULL && exists)
	{
		//remove header if its cached but no longer exists on disk.
		sm_HeaderCache.Remove(pFileName);
	}
	else if( !exists && pReplayHeader != NULL )
	{
		//set new header if it doesn't exist and isn't NULL
		sm_HeaderCache.Push(pFileName, pReplayHeader);
	}
	else
	{
		replayDisplayf("ReplayHeader type already exists");
	}
}

bool ReplayFileManager::StartSaveFile(const char* filePath, tDoFileOpFunc doFileOpFunc, tFileOpCompleteFunc fileOpCompleteFunc, tFileOpSignalCompleteFunc fileOpSignalCompleteFunc)
{
	return StartFileOp(filePath, doFileOpFunc, fileOpCompleteFunc, fileOpSignalCompleteFunc, false);
}

bool ReplayFileManager::StartLoadFile(const char* filePath, tDoFileOpFunc doFileOpFunc, tFileOpCompleteFunc fileOpCompleteFunc)
{
	return StartFileOp(filePath, doFileOpFunc, fileOpCompleteFunc, NULL, true);
}

bool ReplayFileManager::StartFileOp(const char* filePath, tDoFileOpFunc fileOpFunc, tFileOpCompleteFunc fileOpCompleteFunc, tFileOpSignalCompleteFunc fileOpSignalCompleteFunc, bool readOnly)
{
	if( IsPerformingFileOp() )
	{
		replayDisplayf("FileOp is already running");

		return false;
	}

	replayDisplayf("ReplayFileManager::StartFileOp (FileOp check) - File name %s", filePath);

	sm_CurrentReplayFileInfo.Reset();
	sm_CurrentReplayFileInfo.m_FilePath = filePath;
	sm_CurrentReplayFileInfo.m_FileOp = readOnly ? REPLAY_FILEOP_LOAD : REPLAY_FILEOP_SAVE;
	sm_CurrentReplayFileInfo.Validate();

	sm_DoFileOpFunc = fileOpFunc;
	sm_FileOpCompleteFunc = fileOpCompleteFunc;
	sm_FileOpSignalCompleteFunc = fileOpSignalCompleteFunc;

	// Trigger the fileOp thread to begin working
	TriggerFileRequestThread();

	return true;
}

bool ReplayFileManager::StartSave(const ReplayHeader& header, atArray<CBlockProxy>& blocksToSave, tFileOpCompleteFunc fileOpCompleteFunc)
{
	if( IsPerformingFileOp() )
	{
		replayDisplayf("FileOp is already running");

		return false;
	}

	sm_CurrentReplayFileInfo.Reset();
	sm_CurrentReplayFileInfo.m_BlocksToSave = blocksToSave;
	sm_CurrentReplayFileInfo.m_Header = header;

	char name[REPLAYPATHLENGTH];
	GenerateUniqueClipName(name, sm_CurrentReplayFileInfo.m_Header.uID);

	char directory[REPLAYPATHLENGTH];
	getClipDirectory(directory);

	sm_CurrentReplayFileInfo.m_Filename = name;
	sm_CurrentReplayFileInfo.m_FilePath = directory;
	sm_CurrentReplayFileInfo.m_FilePath += name;

	replayDisplayf("ReplayFileManager::StartSave - (FileOp check) - File name %s", name);

	sm_CurrentReplayFileInfo.m_ClipLength = 0;
	sm_CurrentReplayFileInfo.m_FileOp = REPLAY_FILEOP_SAVE_CLIP;
	sm_CurrentReplayFileInfo.Validate();

#if RSG_ORBIS
	sm_SafeToSaveReplay = CGenericGameStorage::IsSafeToSaveReplay();
#else
	sm_SafeToSaveReplay = true;
#endif //RSG_ORBIS

#if __BANK
	for(u16 i = 0; i < blocksToSave.GetCount(); i++)
	{
		CBlockProxy block = blocksToSave[i];

		replayDebugf3("BlockInfo - Session Index: %u, Memory Index: %u", block.GetSessionBlockIndex(), block.GetMemoryBlockIndex());
	}
#endif

#if !REPLAY_USE_PER_BLOCK_THUMBNAILS
	//save the screen shot
	RequestThumbnailSaveAsync();
#endif // !REPLAY_USE_PER_BLOCK_THUMBNAILS
	
	sm_FileOpCompleteFunc = fileOpCompleteFunc;
		
	// Trigger the fileOp thread to begin working
	TriggerFileRequestThread();

	return true;
}

bool ReplayFileManager::StartLoad(const char* filePath, CBufferInfo* bufferInfo, tLoadStartFunc loadStartFunc = NULL )
 {
	if( IsPerformingFileOp() )
	{
		replayDisplayf("FileOp is already running");

		return false;
	}

	replayDisplayf("ReplayFileManager::StartLoad - (FileOp check) - File name %s", filePath);
	
	sm_CurrentReplayFileInfo.Reset();
	sm_CurrentReplayFileInfo.m_BufferInfo = bufferInfo;	
	sm_CurrentReplayFileInfo.m_FilePath = filePath;
	sm_CurrentReplayFileInfo.m_FileOp = REPLAY_FILEOP_LOAD_CLIP;
	sm_CurrentReplayFileInfo.m_LoadStartFunc = loadStartFunc;
	sm_CurrentReplayFileInfo.Validate();
	
	// Trigger the fileOp thread to begin working
	TriggerFileRequestThread();

	return true;
}

bool ReplayFileManager::LoadMontage(const char* filePath, CMontage* montage)
{
	if( IsPerformingFileOp() )
	{
		replayDisplayf("FileOp is already running");

		return false;
	}

	replayDisplayf("ReplayFileManager::LoadMontage - (FileOp check) - File name %s", filePath);

	sm_CurrentReplayFileInfo.Reset();
	sm_CurrentReplayFileInfo.m_FilePath = filePath;

	sm_CurrentReplayFileInfo.m_Filename = filePath;

	atArray< atString > montagePathSplit;
	sm_CurrentReplayFileInfo.m_Filename.Replace( "\\", "/" );
	sm_CurrentReplayFileInfo.m_Filename.Split( montagePathSplit, "/");
	sm_CurrentReplayFileInfo.m_Filename = montagePathSplit.size() > 0 ? montagePathSplit[ montagePathSplit.size() - 1 ] : sm_CurrentReplayFileInfo.m_Filename;

	int const c_extensionStartIndex = sm_CurrentReplayFileInfo.m_Filename.LastIndexOf( "." );
	if( c_extensionStartIndex >= 0 && c_extensionStartIndex < sm_CurrentReplayFileInfo.m_Filename.GetLength() )
	{
		sm_CurrentReplayFileInfo.m_Filename.Truncate( c_extensionStartIndex );
	}

	sm_CurrentReplayFileInfo.m_Montage = montage;
	sm_CurrentReplayFileInfo.m_FileOp = REPLAY_FILEOP_LOAD_MONTAGE;

	sm_CurrentReplayFileInfo.Validate();

	// Trigger the fileOp thread to begin working
	TriggerFileRequestThread();

	return true;
}


bool ReplayFileManager::SaveMontage(const char* filePath, CMontage* montage)
{
	if( IsPerformingFileOp() )
	{
		replayDisplayf("FileOp is already running");

		return false;
	}

	replayDisplayf("ReplayFileManager::SaveMontage - (FileOp check) - File name %s", filePath);

	sm_CurrentReplayFileInfo.Reset();
	sm_CurrentReplayFileInfo.m_FilePath = filePath;

	sm_CurrentReplayFileInfo.m_Filename = filePath;

	atArray< atString > montagePathSplit;
	sm_CurrentReplayFileInfo.m_Filename.Replace( "\\", "/" );
	sm_CurrentReplayFileInfo.m_Filename.Split( montagePathSplit, "/");
	sm_CurrentReplayFileInfo.m_Filename = montagePathSplit.size() > 0 ? montagePathSplit[ montagePathSplit.size() - 1 ] : sm_CurrentReplayFileInfo.m_Filename;

	int const c_extensionStartIndex = sm_CurrentReplayFileInfo.m_Filename.LastIndexOf( "." );
	if( c_extensionStartIndex >= 0 && c_extensionStartIndex < sm_CurrentReplayFileInfo.m_Filename.GetLength() )
	{
		sm_CurrentReplayFileInfo.m_Filename.Truncate( c_extensionStartIndex );
	}

	sm_CurrentReplayFileInfo.m_Montage = montage;
	sm_CurrentReplayFileInfo.m_FileOp = REPLAY_FILEOP_SAVE_MONTAGE;

	sm_CurrentReplayFileInfo.Validate();

	// Trigger the fileOp thread to begin working
	TriggerFileRequestThread();

	return true;
}

bool ReplayFileManager::StartLoadReplayHeader(const char* filePath)
{
	if( IsPerformingFileOp() )
	{
		replayDisplayf("FileOp is already running");

		return false;
	}

	replayDisplayf("ReplayFileManager::StartLoadReplayHeader - (FileOp check) - File name %s", filePath);

	sm_CurrentReplayFileInfo.Reset();
	sm_CurrentReplayFileInfo.m_FilePath = filePath;
	sm_CurrentReplayFileInfo.m_FileOp = REPLAY_FILEOP_LOAD_HEADER;
	sm_CurrentReplayFileInfo.Validate();

	// Trigger the fileOp thread to begin working
	TriggerFileRequestThread();

	return true;
}

bool ReplayFileManager::ValidateClip(const char* /*filePath*/)
{
	return false;
}

bool ReplayFileManager::DeleteFile(const char* filePath)
{
	if( IsPerformingFileOp() )
	{
		replayDisplayf("FileOp is already running");

		return false;
	}

	replayDisplayf("ReplayFileManager::DeleteFile - (FileOp check) - File name %s", filePath);

	sm_CurrentReplayFileInfo.Reset();
	sm_CurrentReplayFileInfo.m_FilePath = filePath;
	sm_CurrentReplayFileInfo.m_FileOp = REPLAY_FILEOP_DELETE_FILE;
	sm_CurrentReplayFileInfo.Validate();

	// Trigger the fileOp thread to begin working
	TriggerFileRequestThread();

	return true;
}

bool ReplayFileManager::StartDeleteClip(const char* filePath)
{
	sm_DeleteClips.Init(filePath);

	if( !CGenericGameStorage::PushOnToSavegameQueue(&sm_DeleteClips) )
	{
		return false;
	}

	return true;
}

bool ReplayFileManager::CheckDeleteClip(bool& result)
{
	bool complete = false;
	switch(sm_DeleteClips.GetStatus())
	{
	case MEM_CARD_ERROR:
		{
			result = false;
			complete =  true;
		}
	case MEM_CARD_COMPLETE:
		{
			result = true;
			complete = true;
		}
	case MEM_CARD_BUSY:
		{
			break;
		}
	}

	if (complete)
	{
		UpdateAvailableDiskSpace(true);
		return true;
	}

	return false;
}

bool ReplayFileManager::StartDeleteFile(const char* filePath)
{
	sm_DeleteFile.Init(filePath);

	if( !CGenericGameStorage::PushOnToSavegameQueue(&sm_DeleteFile) )
	{
		return false;
	}

	return true;
}

bool ReplayFileManager::CheckDeleteFile(bool& result)
{
	bool complete = false;
	switch(sm_DeleteFile.GetStatus())
	{
	case MEM_CARD_ERROR:
		{
			result = false;
			complete = true;
		}
	case MEM_CARD_COMPLETE:
		{
			result = true;
			complete = true;
		}
	case MEM_CARD_BUSY:
		{
			break;
		}
	}

	if (complete)
	{
#if RSG_ORBIS
		RefreshNonClipDirectorySizes();
#endif
		UpdateAvailableDiskSpace();
		return true;
	}

	return false;
}

bool ReplayFileManager::StartUpdateFavourites()
{
	sm_UpdateFavourites.Init();

	if( !CGenericGameStorage::PushOnToSavegameQueue(&sm_UpdateFavourites) )
	{
		return false;
	}

	return true;
}

bool ReplayFileManager::CheckUpdateFavourites(bool& result)
{
	switch(sm_UpdateFavourites.GetStatus())
	{
	case MEM_CARD_ERROR:
		{
			result = false;
			return true;
		}
	case MEM_CARD_COMPLETE:
		{
			result = true;
			return true;
		}
	case MEM_CARD_BUSY:
		{
			break;
		}
	}

	return false;
}

bool ReplayFileManager::UpdateFavourites()
{
	if( IsPerformingFileOp() )
	{
		replayDisplayf("FileOp is already running");
		
		return false;
	}

	sm_CurrentReplayFileInfo.Reset();
	sm_CurrentReplayFileInfo.m_FileOp = REPLAY_FILEOP_UPDATE_FAVOURITES;
	sm_CurrentReplayFileInfo.Validate();

	// Trigger the fileOp thread to begin working
	TriggerFileRequestThread();

	return true;
}


bool ReplayFileManager::StartMultiDelete(const char* filter)
{
	sm_MultiDelete.Init(filter);

	if( !CGenericGameStorage::PushOnToSavegameQueue(&sm_MultiDelete) )
	{
		return false;
	}

	return true;
}

bool ReplayFileManager::CheckMultiDelete(bool& result)
{
	switch(sm_MultiDelete.GetStatus())
	{
	case MEM_CARD_ERROR:
		{
			result = false;
			return true;
		}
	case MEM_CARD_COMPLETE:
		{
			result = true;
			return true;
		}
	case MEM_CARD_BUSY:
		{
			break;
		}
	}

	return false;
}

bool ReplayFileManager::ProcessMultiDelete(const char* filter)
{
	if( IsPerformingFileOp() )
	{
		replayDisplayf("FileOp is already running");

		return false;
	}

	sm_CurrentReplayFileInfo.Reset();
	sm_CurrentReplayFileInfo.m_FileOp = REPLAY_FILEOP_UPDATE_MULTI_DELETE;
	sm_CurrentReplayFileInfo.m_Filter = filter;
	sm_CurrentReplayFileInfo.Validate();

	// Trigger the fileOp thread to begin working
	TriggerFileRequestThread();

	return true;
}

bool ReplayFileManager::StartGetHeader(const char* filePath)
{
	sm_LoadHeader.Init(filePath);

	if( !CGenericGameStorage::PushOnToSavegameQueue(&sm_LoadHeader) )
	{
		return false;
	}

	return true;
}

bool ReplayFileManager::CheckGetHeader(bool& result, ReplayHeader& header)
{
	switch(sm_LoadHeader.GetStatus())
	{
	case MEM_CARD_ERROR:
		{
			result = false;
			return true;
		}
	case MEM_CARD_COMPLETE:
		{
			result = true;

			header = *sm_LoadHeader.GetHeader();

			return true;
		}
	case MEM_CARD_BUSY:
		{
			break;
		}
	}

	return false;
}

bool ReplayFileManager::StartLoadMontage(const char* path, CMontage* montage)
{
	sm_LoadMontage.Init(path, montage);

	if( !CGenericGameStorage::PushOnToSavegameQueue(&sm_LoadMontage) )
	{
		return false;
	}

	return true;
}

bool ReplayFileManager::CheckLoadMontage(bool &result)
{
	switch(sm_LoadMontage.GetStatus())
	{
	case MEM_CARD_ERROR:
		{
			result = false;
			return true;
		}
	case MEM_CARD_COMPLETE:
		{
			result = true;
			return true;
		}
	case MEM_CARD_BUSY:
		{
			break;
		}
	}

	return false;
}

bool ReplayFileManager::StartSaveMontage(const char* path, CMontage* montage)
{
	sm_SaveMontage.Init(path, montage);

	if( !CGenericGameStorage::PushOnToSavegameQueue(&sm_SaveMontage) )
	{
		return false;
	}

	return true;
}

bool ReplayFileManager::CheckSaveMontage(bool &result)
{
	switch(sm_SaveMontage.GetStatus())
	{
	case MEM_CARD_ERROR:
		{
			result = false;
			return true;
		}
	case MEM_CARD_COMPLETE:
		{
			result = true;
			return true;
		}
	case MEM_CARD_BUSY:
		{
			break;
		}
	}

	return false;
}

bool ReplayFileManager::IsClipFileValid( u64 const ownerId, const char* szFileName, bool checkDisk)
{
	char directory[REPLAYPATHLENGTH];
	ReplayFileManager::getClipDirectoryForUser( ownerId, directory );
	char path[REPLAYPATHLENGTH];

	const char* ext = !fwTextUtil::HasFileExtension(szFileName, REPLAY_CLIP_FILE_EXT) ? REPLAY_CLIP_FILE_EXT : "";
	sprintf_s(path, REPLAYPATHLENGTH, "%s%s%s", directory, szFileName, ext );

	return sm_Device->IsClipFileValid(ownerId, path, checkDisk);
}

bool ReplayFileManager::IsProjectFileValid( u64 const ownerId, const char* szFileName, bool checkDisk)
{
	char directory[REPLAYPATHLENGTH];
	ReplayFileManager::getVideoProjectDirectory(directory);
	char path[REPLAYPATHLENGTH];
	sprintf_s(path, REPLAYPATHLENGTH, "%s%s%s", directory, szFileName, REPLAY_MONTAGE_FILE_EXT );

	return sm_Device->IsProjectFileValid(ownerId, path, checkDisk);
}

bool ReplayFileManager::DoesClipHaveModdedContent( u64 const ownerId, const char* szFileName)
{
	char directory[REPLAYPATHLENGTH];
	ReplayFileManager::getClipDirectoryForUser( ownerId, directory );
	char path[REPLAYPATHLENGTH];

	const char* ext = !fwTextUtil::HasFileExtension(szFileName, REPLAY_CLIP_FILE_EXT) ? REPLAY_CLIP_FILE_EXT : "";
	sprintf_s(path, REPLAYPATHLENGTH, "%s%s%s", directory, szFileName, ext );

	return sm_Device->DoesClipHaveModdedContent(path);
}

const ClipUID* ReplayFileManager::GetClipUID( u64 const ownerId, const char* filename)
{
	char directory[REPLAYPATHLENGTH];
	ReplayFileManager::getClipDirectoryForUser( ownerId, directory);
	char path[REPLAYPATHLENGTH];
	sprintf_s(path, REPLAYPATHLENGTH, "%s%s%s", directory, filename, REPLAY_CLIP_FILE_EXT);

	return sm_Device->GetClipUID(ownerId, path);
}

float ReplayFileManager::GetClipLength( u64 const ownerId, const char *filename)
{
	char directory[REPLAYPATHLENGTH];
	ReplayFileManager::getClipDirectoryForUser( ownerId, directory );
	char path[REPLAYPATHLENGTH];
	sprintf_s(path, REPLAYPATHLENGTH, "%s%s%s", directory, filename, REPLAY_CLIP_FILE_EXT);

	return sm_Device->GetClipLength(ownerId, path);
}


u32	ReplayFileManager::GetClipSequenceHash(u64 const ownerId, const char* filename, bool prev)
{
	char directory[REPLAYPATHLENGTH];
	ReplayFileManager::getClipDirectoryForUser( ownerId, directory);
	char path[REPLAYPATHLENGTH];
	sprintf_s(path, REPLAYPATHLENGTH, "%s%s%s", directory, filename, REPLAY_CLIP_FILE_EXT);

	return sm_Device->GetClipSequenceHash(ownerId, path, prev);
}


bool ReplayFileManager::StartEnumerateProjectFiles(FileDataStorage& fileList, const char* filter)
{
	sm_EnumerateProjectFiles.Init(REPLAY_ENUM_MONTAGES, &fileList, filter);

	if( !CGenericGameStorage::PushOnToSavegameQueue(&sm_EnumerateProjectFiles) )
	{
		return false;
	}

	return true;
}

bool ReplayFileManager::CheckEnumerateProjectFiles(bool& result)
{
	switch(sm_EnumerateProjectFiles.GetStatus())
	{
	case MEM_CARD_ERROR:
		{
			result = false;
			return true;
		}
	case MEM_CARD_COMPLETE:
		{
			result = true;
			return true;
		}
	case MEM_CARD_BUSY:
		{
			break;
		}
	}

	return false;
}

bool ReplayFileManager::StartEnumerateClipFiles(FileDataStorage& fileList, const char* filter)
{
	sm_EnumerateClipFiles.Init(REPLAY_ENUM_CLIPS, &fileList, filter);

	if( !CGenericGameStorage::PushOnToSavegameQueue(&sm_EnumerateClipFiles) )
	{
		return false;
	}

	return true;
}

bool ReplayFileManager::CheckEnumerateClipFiles(bool& result)
{
	switch(sm_EnumerateClipFiles.GetStatus())
	{
	case MEM_CARD_ERROR:
		{
			result = false;
			return true;
		}
	case MEM_CARD_COMPLETE:
		{
			result = true;
			return true;
		}
	case MEM_CARD_BUSY:
		{
			break;
		}
	}

	return false;
}

bool ReplayFileManager::StartEnumerateFiles(eReplayEnumerationTypes enumType, FileDataStorage& fileList, const char* filter)
{
	if( enumType == REPLAY_ENUM_CLIPS )
	{
		return StartEnumerateClipFiles(fileList, filter);
	}
	else if( enumType == REPLAY_ENUM_MONTAGES )
	{
		return StartEnumerateProjectFiles(fileList, filter);
	}

	return false;
}

bool ReplayFileManager::EnumerateFiles(eReplayEnumerationTypes enumType, FileDataStorage& fileList, const char* filter)
{
	if( IsPerformingFileOp() )
	{
		replayDisplayf("FileOp is already running");

		return false;
	}

	sm_CurrentReplayFileInfo.Reset();

	switch(enumType)
	{
	case REPLAY_ENUM_CLIPS:
		{
			replayDisplayf("ReplayFileManager::EnumerateFiles - (FileOp check) - REPLAY_FILEOP_ENUMERATE_CLIPS");
			sm_CurrentReplayFileInfo.m_FileOp = REPLAY_FILEOP_ENUMERATE_CLIPS;
			// REPLAY_CLIPS_PATH change
			char clipPath[REPLAYPATHLENGTH];
			ReplayFileManager::getClipDirectory(clipPath, false);
			sm_CurrentReplayFileInfo.m_FilePath = clipPath;
			break;
		}
	case REPLAY_ENUM_MONTAGES:
		{
			replayDisplayf("ReplayFileManager::EnumerateFiles - (FileOp check) - REPLAY_FILEOP_ENUMERATE_MONTAGES");
			sm_CurrentReplayFileInfo.m_FileOp = REPLAY_FILEOP_ENUMERATE_MONTAGES;
			// REPLAY_MONTAGE_PATH change
			char montagePath[REPLAYPATHLENGTH];
			ReplayFileManager::getVideoProjectDirectory(montagePath, false);
			sm_CurrentReplayFileInfo.m_FilePath = montagePath;
			break;
		}
	}

	sm_CurrentReplayFileInfo.m_FileDataStorage = &fileList;
	sm_CurrentReplayFileInfo.m_Filter = filter;
	sm_CurrentReplayFileInfo.Validate();
	
	// Trigger the fileOp thread to begin working
	TriggerFileRequestThread();

	return true;
}

bool ReplayFileManager::IsPerformingFileOp()
{
#if DO_REPLAY_OUTPUT
	if (sm_CurrentReplayFileInfo.m_FileOp != REPLAY_FILEOP_NONE)
	{
		replayDisplayf("ReplayFileManager::IsPerformingFileOp - %d", static_cast<int>(sm_CurrentReplayFileInfo.m_FileOp));
	}
#endif
	return (sm_CurrentReplayFileInfo.m_FileOp != REPLAY_FILEOP_NONE); 
}

bool ReplayFileManager::IsSaving() const
{
	return (sm_CurrentReplayFileInfo.m_FileOp == REPLAY_FILEOP_SAVE_CLIP || sm_LastFileOp == REPLAY_FILEOP_SAVE_CLIP ); 
}

bool ReplayFileManager::IsLoading() const
{
	return (sm_CurrentReplayFileInfo.m_FileOp == REPLAY_FILEOP_LOAD_CLIP || sm_LastFileOp == REPLAY_FILEOP_LOAD_CLIP);
}

#if __BANK
void ReplayFileManager::DumpReplayFiles_Bank()
{
	//dumping clips and projects
	DumpReplayFiles(true);
	DumpReplayFiles(false);
}

void ReplayFileManager::UpdateFavouritesCallback()
{
	StartUpdateFavourites();
}

void ReplayFileManager::MarkAsFavouriteCallback()
{
	MarkAsFavourite(sm_Device->GetDebugCurrentClip());
}

void ReplayFileManager::UnMarkAsFavouriteCallback()
{
	UnMarkAsFavourite(sm_Device->GetDebugCurrentClip());
}
#endif

#if !__FINAL
void ReplayFileManager::DumpReplayFiles(bool clips)
{
	fiFindData findData;
	fiHandle hFileHandle = fiHandleInvalid;
	bool validDataFound = true;

	char fileDir[REPLAYPATHLENGTH];
	appendUserIDDirectoryName(clips ? REPLAY_CLIPS_PATH : REPLAY_MONTAGE_PATH, fileDir);

	for( hFileHandle = sm_Device->FindFileBegin( fileDir, findData );
		fiIsValidHandle( hFileHandle ) && validDataFound; 
		validDataFound = sm_Device->FindFileNext( hFileHandle, findData ) )
	{
		s32 nameLength = static_cast<s32>(strlen(findData.m_Name));
		if((findData.m_Attributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)
		{
			continue;
		}

		// If the filename+path is greater than REPLAYFILELENGTH, then discard (since we can't cache it & thus can't load the file later using the name)
		if( nameLength > (REPLAYFILELENGTH - 1) )		// -1 for the zero terminator that will be required.
			continue;

		char filePath[RAGE_MAX_PATH];
		sprintf_s(filePath, RAGE_MAX_PATH, "%s%s", fileDir, findData.m_Name);
		atString atFilePath(filePath);

		if (atFilePath.EndsWith(".clip") || atFilePath.EndsWith(".jpg") || atFilePath.EndsWith(".vid") )
		{
			//open the path to the file
			FileHandle handle = CFileMgr::OpenFile(filePath, "r");

			if( handle == NULL )
			{
				replayDebugf1("Could not open %s", filePath);
				continue;
			}

			char replayfile[RAGE_MAX_PATH];
			const char* dumpReplayFolder;
			bool bCustomFolder = PARAM_replayFilesDumpDir.Get(dumpReplayFolder);
			if (bCustomFolder)
			{
				sprintf(replayfile, "%s%s%s", dumpReplayFolder, clips? "clips\\":"projects\\", findData.m_Name);
			}
#	if __BANK
			else
			{
				sprintf(replayfile, "%s%s%s", CReplayMgrInternal::sm_customReplayPath, clips? "clips\\":"projects\\", findData.m_Name);
			}
#	endif

			// if the file doesn't already exist, we read from the clip and write to a new one
			if (!ASSET.Exists(replayfile, ""))
			{
				s32 fileSize = CFileMgr::GetTotalSize(handle);
				s32 dataRead = 0;
				const u32 LENGTH = 512;
				char dataBuffer[LENGTH];		//we read/write chunks of 512
				CFileMgr::Seek(handle, 0);
				fiStream* pStream = ASSET.Create(replayfile, "", false);

				while (dataRead < fileSize)
				{
					s32 size = fileSize - dataRead;
					if (size < LENGTH)
					{
						CFileMgr::Read(handle, dataBuffer, size);
						pStream->WriteByte(dataBuffer, size);
						dataRead += size;
					}
					else
					{
						CFileMgr::Read(handle, dataBuffer, LENGTH);
						pStream->WriteByte(dataBuffer, LENGTH);
						dataRead += LENGTH;
					}
				}

				// close files
				pStream->Close();
			}

			CFileMgr::CloseFile(handle);
		}
	}
	sm_Device->FindFileEnd(hFileHandle);
}

void ReplayFileManager::DumpReplayFile(const char* filePath, const char* fileName, const char* destinationDir)
{
	char fullFilePath[RAGE_MAX_PATH];
	sprintf(fullFilePath, "%s%s", filePath, fileName);

	//open the path to the file
	FileHandle handle = CFileMgr::OpenFile(fullFilePath, "r");

	if(handle == NULL)
	{
		replayWarningf("Could not open %s", fullFilePath);
		return;
	}

	char replayfile[RAGE_MAX_PATH];
	sprintf(replayfile, "%s%s", destinationDir, fileName);

	// if the file doesn't already exist, we read from the clip and write to a new one
	if(!ASSET.Exists(replayfile, ""))
	{
		s32 fileSize = CFileMgr::GetTotalSize(handle);
		s32 dataRead = 0;
		const u32 LENGTH = 512;
		char dataBuffer[LENGTH];		//we read/write chunks of 512
		CFileMgr::Seek(handle, 0);
		fiStream* pStream = ASSET.Create(replayfile, "", false);

		while(dataRead < fileSize)
		{
			s32 size = fileSize - dataRead;
			if(size < LENGTH)
			{
				CFileMgr::Read(handle, dataBuffer, size);
				pStream->WriteByte(dataBuffer, size);
				dataRead += size;
			}
			else
			{
				CFileMgr::Read(handle, dataBuffer, LENGTH);
				pStream->WriteByte(dataBuffer, LENGTH);
				dataRead += LENGTH;
			}
		}

		// close files
		pStream->Close();
	}

	CFileMgr::CloseFile(handle);
}

const char* ReplayFileManager::GetDumpFolder()
{
	const char* dumpReplayFolder;
	bool customFolder = PARAM_replayFilesDumpDir.Get(dumpReplayFolder);
	if(customFolder)
	{
		return dumpReplayFolder;
	}
	else
	{
		return "X:\\ReplayDump\\";
	}

}

#endif

void ReplayFileManager::StartFileThread()
{	
	sm_HaltFileOpThread = false;

	//create the semaphores
	sm_TriggerFileRequest = sysIpcCreateSema(0);
	sm_HasFinishedRequest = sysIpcCreateSema(0);

	//create the thread
	if(sm_FileThreadID == sysIpcThreadIdInvalid)
	{
#if RSG_DURANGO
		const int primaryThreadCore = 5;
#elif RSG_ORBIS
		const int primaryThreadCore = 3;
#else
		const int primaryThreadCore = 0;
#endif
		sm_FileThreadID = sysIpcCreateThread(&FileThread,  NULL, sysIpcMinThreadStackSize, PRIO_LOWEST, "ReplayFileManager Thread", primaryThreadCore, "ReplayFileManager");
		replayAssertf(sm_FileThreadID != sysIpcThreadIdInvalid, "Could not create the ReplayFileManager thread, out of memory?");
	}

#if RSG_PC
	if( sm_FileNotificationThreadID == sysIpcThreadIdInvalid )
	{
		sm_WatchDirectory = true;
		sm_FileNotificationThreadID = sysIpcCreateThread(&WatchDirectory,  NULL, sysIpcMinThreadStackSize, PRIO_LOWEST, "ReplayFile Notification Thread", 0, "ReplayFileManagerFileNotification");
		replayAssertf(sm_FileNotificationThreadID != sysIpcThreadIdInvalid, "Could not create the ReplayFileManager File Notification thread, out of memory?");
	}
#endif
}

void ReplayFileManager::StopFileThread()
{
	//stop and remove the thread
	if(sm_FileThreadID != sysIpcThreadIdInvalid)
	{
		sm_HaltFileOpThread = true;

		if(sm_CurrentReplayFileInfo.m_FileOp == REPLAY_FILEOP_NONE)
		{
			sysIpcSignalSema(sm_TriggerFileRequest);
		}

		//some ops want to be canceled, this sets a flag so those ops
		//can deal with the cancel.
		AttemptToCancelCurrentOp();

		sm_FileOpCompleteFunc = NULL;

		//if we canceled this op then we need to ensure we 
		//force an enumeration before the player can record clips
		if(sm_CurrentReplayFileInfo.m_FileOp == REPLAY_FILEOP_ENUMERATE_CLIPS)
		{
			sm_HasCompletedInitialEnumeration = false;
		}

		//wait to make sure the IO thread has finished its business
		sysIpcWaitSema(sm_HasFinishedRequest);
		
		sysIpcWaitThreadExit(sm_FileThreadID);
		sm_FileThreadID = sysIpcThreadIdInvalid;
	}

#if RSG_PC
	if(sm_FileNotificationThreadID != sysIpcThreadIdInvalid)
	{
		sm_WatchDirectory = false;
		sysIpcWaitThreadExit(sm_FileNotificationThreadID);
		sm_FileNotificationThreadID = sysIpcThreadIdInvalid;
	}
#endif

	//tidy up the semaphores
	if( sm_TriggerFileRequest )
	{
		sysIpcDeleteSema(sm_TriggerFileRequest);
		sm_TriggerFileRequest = NULL;
	}
	
	if( sm_HasFinishedRequest )
	{
		sysIpcDeleteSema(sm_HasFinishedRequest); 
		sm_HasFinishedRequest = NULL;
	}
}

#if RSG_PC
void ReplayFileManager::WatchDirectory(void*)
{
	DWORD dwWaitStatus; 
	HANDLE dwChangeHandles[2]; 
	TCHAR lpDrive[4];
	TCHAR lpFile[_MAX_FNAME];
	TCHAR lpExt[_MAX_EXT];

	char directory[RAGE_MAX_PATH] = {0};
	atString clipPath = atString(REPLAY_CLIPS_PATH);

	FixName(directory, clipPath);

	_tsplitpath_s(directory, lpDrive, 4, NULL, 0, lpFile, _MAX_FNAME, lpExt, _MAX_EXT);

	lpDrive[2] = (TCHAR)'\\';
	lpDrive[3] = (TCHAR)'\0';

	// Watch the directory for file creation and deletion. 

	dwChangeHandles[0] = FindFirstChangeNotification( 
		directory,                         // directory to watch 
		FALSE,                         // do not watch subtree 
		FILE_NOTIFY_CHANGE_FILE_NAME); // watch file name changes 

	if (dwChangeHandles[0] == INVALID_HANDLE_VALUE) 
	{
		return;
	}

	// Watch the subtree for directory creation and deletion.
	dwChangeHandles[1] = FindFirstChangeNotification( 
		lpDrive,                       // directory to watch 
		TRUE,                          // watch the subtree 
		FILE_NOTIFY_CHANGE_DIR_NAME);  // watch dir name changes 

	if (dwChangeHandles[1] == INVALID_HANDLE_VALUE) 
	{
		replayDisplayf("\n ERROR: FindFirstChangeNotification function failed.\n");
		return;
	}

	// Make a final validation check on our handles.
	if ((dwChangeHandles[0] == NULL) || (dwChangeHandles[1] == NULL))
	{
		replayDisplayf("\n ERROR: Unexpected NULL from FindFirstChangeNotification.\n");
		return;
	}

	// Change notification is set. Now wait on both notification 
	// handles and refresh accordingly. 
	while (sm_WatchDirectory)
	{ 
		dwWaitStatus = WaitForMultipleObjects(2, dwChangeHandles, 
			FALSE, 100); 

		switch (dwWaitStatus) 
		{ 
		case WAIT_OBJECT_0: 
			UpdateAvailableDiskSpace(true);
			RefreshEnumeration();
			
			if ( FindNextChangeNotification(dwChangeHandles[0]) == FALSE )
			{
				replayDisplayf("\n ERROR: FindNextChangeNotification function failed.\n");
			}
			break; 

		case WAIT_OBJECT_0 + 1: 
			UpdateAvailableDiskSpace(true);
		
			if (FindNextChangeNotification(dwChangeHandles[1]) == FALSE )
			{
				replayDisplayf("\n ERROR: FindNextChangeNotification function failed.\n");
				
			}
			break;	
		case WAIT_TIMEOUT:
			break;

		default: 
			replayDisplayf("\n ERROR: Unhandled dwWaitStatus.\n");
			
			break;
		}
	}

	if( FindCloseChangeNotification(dwChangeHandles[0]) == 0 )
	{
		replayDisplayf("\n ERROR: FindCloseChangeNotification failed.\n");
	}

	if( FindCloseChangeNotification(dwChangeHandles[1]) == 0 )
	{
		replayDisplayf("\n ERROR: FindCloseChangeNotification failed.\n");
	}
}
#endif

void ReplayFileManager::RefreshEnumeration()
{
	//if we've just saved a clip we don't need to update
	if( sm_CurrentReplayFileInfo.m_FileOp == REPLAY_FILEOP_SAVE_CLIP )
	{
		return;
	}
	
	if( sm_Device )
	{
		sm_Device->RefreshCaches();
	}
}

void ReplayFileManager::ProcessInitialEnumeration()
{
	if(sm_StartInitialEnumeration ORBIS_ONLY(&& CGenericGameStorage::IsSafeToUseSaveLibrary()) )
	{
		sm_InitialEnumerateClipFiles.Init(REPLAY_ENUM_CLIPS, NULL, REPLAY_CLIP_FILE_EXT);

		if(CGenericGameStorage::PushOnToSavegameQueue(&sm_InitialEnumerateClipFiles))
		{
			sm_StartInitialEnumeration = false;
		}
	}
	else if( !sm_StartInitialEnumeration && !sm_HasCompletedInitialEnumeration )
	{
		switch( sm_InitialEnumerateClipFiles.GetStatus() )
		{
		case MEM_CARD_ERROR:
		case MEM_CARD_COMPLETE:
			{
				sm_HasCompletedInitialEnumeration = true;
				break;
			}
		case MEM_CARD_BUSY:
			{
				break;
			}
		default:
			break;
		}
	}
}

struct saveInfo
{
	int numBlocksWriteable;
	int numBlocksWaitingToSave;
	int totalBlocks;
};

int ReplayFileManager::GetRemainingBlocksFree(CBlockProxy block, void* pParam)
{
	saveInfo& param = *((saveInfo*)pParam);

	if( (block.GetStatus() == REPLAYBLOCKSTATUS_EMPTY && block.IsTempBlock()) || 
		(block.GetStatus() != REPLAYBLOCKSTATUS_SAVING && !block.IsTempBlock() && !block.IsEquivOfTemp()) )
	{
		param.numBlocksWriteable++;
	}
	else
	{
		param.numBlocksWaitingToSave++;
	}
	
	param.totalBlocks++;

	return REPLAY_ERROR_SUCCESS;
}

//Updates the flag to tell the filemanger is should hold off on the save whilst we're
//under heavy streaming load.
//Forces the save if there is a chance we could overwrite data.
void ReplayFileManager::ProcessShouldSaveReplay()
{
#if STREAMING_LOAD_SAVING
	if( sm_CurrentReplayFileInfo.m_FileOp == REPLAY_FILEOP_SAVE_CLIP )
	{
#if RSG_ORBIS
		sm_SafeToSaveReplay = CGenericGameStorage::IsSafeToSaveReplay();
#else
		sm_SafeToSaveReplay = true;
#endif //RSG_ORBIS

		sm_ForceSave = false;

		saveInfo param;
		param.totalBlocks = 0;
		param.numBlocksWriteable = 0;
		param.numBlocksWaitingToSave = 0;

		//work out how many blocks we have
		CReplayMgr::ForeachBlock(GetRemainingBlocksFree, (void*)&param);

#if __BANK
		sm_numBlocksFree = (int)param.numBlocksWriteable;
		sm_numBlocksFull = (int)param.numBlocksWaitingToSave;
		sm_totalBlocks = (int)param.totalBlocks;
#endif // __BANK

		// Force the save through if we're being forced to stop record.
 		if( CReplayControl::ShouldStopRecording() )
		{
			sm_ForceSave = true;
			replayDisplayf("Force allow the save due to recording being force stopped (Reason %s)", CReplayControl::GetLastStateChangeReason().c_str());
		}
	}
#endif //STREAMING_LOAD_SAVING
}

void ReplayFileManager::Process()
{
	ProcessInitialEnumeration();

	if( sm_Device )
	{
		sm_Device->Process();
	}

	ProcessShouldSaveReplay();

#if DO_REPLAY_OUTPUT
	if (sm_CurrentReplayFileInfo.m_FileOp == REPLAY_FILEOP_SAVE_CLIP)
	{
		replayDisplayf("ReplayFileManager::IsSaving - sm_CurrentReplayFileInfo.m_FileOp - REPLAY_FILEOP_SAVE_CLIP");
	}
	if (sm_LastFileOp == REPLAY_FILEOP_SAVE_CLIP)
	{
		replayDisplayf("ReplayFileManager::IsSaving - sm_LastFileOp - REPLAY_FILEOP_SAVE_CLIP");
	}
#endif

	//ensure the call back is fired on the main thread
	if( sm_LastFileOp != REPLAY_FILEOP_NONE )
	{
		if( sm_FileOpCompleteFunc != NULL )
		{
			(*sm_FileOpCompleteFunc)(sm_LastFileOpRetCode);
			sm_FileOpCompleteFunc = NULL;
		}

		sm_OnLoadStreamingFunc = NULL;
		sm_DoFileOpFunc = NULL;

		sm_LastFileOp = REPLAY_FILEOP_NONE;
	}
}

void ReplayFileManager::FileThread(void*)
{
	eReplayFileErrorCode result = REPLAY_ERROR_SUCCESS;
	u64 opExtendedResultData = 0;

	while( sm_HaltFileOpThread == false )
	{
		sysIpcWaitSema(sm_TriggerFileRequest);
		opExtendedResultData = 0;

		//run the operation
		switch(sm_CurrentReplayFileInfo.m_FileOp)
		{
		case REPLAY_FILEOP_LOAD:
		case REPLAY_FILEOP_SAVE:
			{
				result = PerformFileOp( sm_CurrentReplayFileInfo );

				UpdateAvailableDiskSpace();
				break;
			}
		case REPLAY_FILEOP_SAVE_CLIP:
			{
				CBlockProxy blockInfo = sm_CurrentReplayFileInfo.m_BlocksToSave[0];

#if REPLAY_USE_PER_BLOCK_THUMBNAILS
				CReplayThumbnail* thumbNail = NULL;

#if __BANK
				if( !blockInfo.HasRequestedThumbNail() )
				{
					for(u16 i = 0; i < sm_CurrentReplayFileInfo.m_BlocksToSave.GetCount(); i++)
					{
						CBlockProxy block = sm_CurrentReplayFileInfo.m_BlocksToSave[i];
						replayDebugf3("Block Thumbnail Status - Req: %s Pop: %s", block.HasRequestedThumbNail() ? "TRUE" : "FALSE", block.IsThumbnailPopulated() ? "TRUE" : "FALSE");
					}

					replayAssertf(false, "ReplayFileManager::FileThread() [REPLAY_FILEOP_SAVE_CLIP]...No thumbnail requested");
				}
#endif //__BANK

#endif // REPLAY_USE_PER_BLOCK_THUMBNAILS

				// Don't save a clip if we have no thumbnail.
				//if(blockInfo.IsThumbnailPopulated())
				{
					thumbNail = &blockInfo.GetThumbnailRef().GetThumbnail();

					result = SaveClip( sm_CurrentReplayFileInfo );

					// the file has saved, update available disc space.
					UpdateAvailableDiskSpace(true);

#if !REPLAY_USE_PER_BLOCK_THUMBNAILS
					BlockOnThumbnailSaveAsync();
#endif // !REPLAY_USE_PER_BLOCK_THUMBNAILS

					if( result != REPLAY_ERROR_SUCCESS )
					{
						//reset the save flags if we've failed.
						ResetSaveFlags(sm_CurrentReplayFileInfo);
					}
					else
					{
#if REPLAY_USE_PER_BLOCK_THUMBNAILS
						if(thumbNail && thumbNail->IsPopulated())
						{
							u32 pitch = thumbNail->GetPitch();
							char *pRGB8Pixels = thumbNail->GetRawBuffer();

							atString filename = sm_CurrentReplayFileInfo.m_FilePath;
							filename.Replace(REPLAY_CLIP_FILE_EXT, ".jpg");

							grcImage::SaveRGB8SurfaceToJPEG(filename, CHighQualityScreenshot::GetDefaultQuality(),
								(void *)pRGB8Pixels,
								(int)thumbNail->GetWidth(),
								(int)thumbNail->GetHeight(),
								(int)pitch);

							thumbNail->SetLocked(false);
						}
						else if(thumbNail)
						{
#if __FINAL
							replayDebugf1("Haven't got a thumbnail for this clip... filter was %d", thumbNail->GetFailureReason());
#else
							replayDebugf1("Haven't got a thumbnail for this clip... filter was %s", AnimPostFXManagerReplayEffectLookup::GetFilterName(thumbNail->GetFailureReason()));
#endif // __FINAL
						}
#endif // REPLAY_USE_PER_BLOCK_THUMBNAILS
					}
				}
// 				else
// 				{
// 					replayWarningf("REPLAY_FILEOP_SAVE_CLIP - WARNING:- Clip wasn't saved because no thumbnail could be obtained, may have recorded a faded out section?");
// 				}

			#if !REPLAY_USE_PER_BLOCK_THUMBNAILS
				sm_thumbnailSaveData.Reset();
			#endif // REPLAY_USE_PER_BLOCK_THUMBNAILS
				break;
			}
		case REPLAY_FILEOP_LOAD_CLIP:
			{
				result = sm_Device->LoadClip(sm_CurrentReplayFileInfo);
				
				break;
			}
		case REPLAY_FILEOP_LOAD_MONTAGE:
			{
				result = sm_Device->LoadMontage( sm_CurrentReplayFileInfo, opExtendedResultData );

				break;
			}
		case REPLAY_FILEOP_SAVE_MONTAGE:
			{
				result = sm_Device->SaveMontage( sm_CurrentReplayFileInfo, opExtendedResultData );

				break;
			}
		case REPLAY_FILEOP_LOAD_HEADER:
			{
				result = sm_Device->LoadHeader(sm_CurrentReplayFileInfo);
				
				break;
			}
		case REPLAY_FILEOP_ENUMERATE_CLIPS:
			{
#if !__FINAL
				const char* dumpReplayFolder;
				bool bCustomFolder = PARAM_replayFilesDumpDir.Get(dumpReplayFolder);
				if (bCustomFolder && !sm_bDumpedReplayFiles)
				{
					DumpReplayFiles(true);
					DumpReplayFiles(false);
					sm_bDumpedReplayFiles = true;
				}
#endif

				result = sm_Device->EnumerateClips(sm_CurrentReplayFileInfo);

				break;
			}
		case REPLAY_FILEOP_ENUMERATE_MONTAGES:
			{
				result = sm_Device->EnumerateMontages(sm_CurrentReplayFileInfo);

				break;
			}
		case REPLAY_FILEOP_DELETE_FILE:
			{
				result = sm_Device->DeleteFile(sm_CurrentReplayFileInfo);

#if RSG_ORBIS
				// likely to be in a menu when deleting files, so we can take the hit for refreshing directory sizes firsts
				RefreshNonClipDirectorySizes();
#endif
				// the file has deleted, update available disc space.
				UpdateAvailableDiskSpace();

				break;
			}
		case REPLAY_FILEOP_UPDATE_FAVOURITES:
			{
				result = sm_Device->UpdateFavourites(sm_CurrentReplayFileInfo);

				break;
			}
		case REPLAY_FILEOP_UPDATE_MULTI_DELETE:
			{
				result = sm_Device->ProcessMultiDelete(sm_CurrentReplayFileInfo);

				break;
			}
		default:
			{
				break;
			}
		}


		GetAvailableDiskSpace();
		
		sm_LastFileOpRetCode = result;
		sm_LastFileOpExtendedResultData = opExtendedResultData;
		sm_LastFileOp = sm_CurrentReplayFileInfo.m_FileOp;

		if( sm_LastFileOp != REPLAY_FILEOP_NONE)
		{
			if(sm_FileOpSignalCompleteFunc)
			{
				(*sm_FileOpSignalCompleteFunc)(sm_LastFileOpRetCode);
				sm_FileOpSignalCompleteFunc = NULL;
			}
		}

		//Clear out previous info
		sm_CurrentReplayFileInfo.Reset();

		//enable main thread if it was halted
		if( sm_LastFileOp == REPLAY_FILEOP_LOAD_HEADER && sm_LoadHeaderRequest != NULL )
		{
			//allow the main thread to continue
			sysIpcSignalSema(sm_LoadHeaderRequest);
		}		
	}

	sysIpcSignalSema(sm_HasFinishedRequest);	
}

eReplayFileErrorCode ReplayFileManager::PerformFileOp(ReplayFileInfo& info)
{
	FileHandle handle = OpenFile(info);

	if( handle == NULL )
	{
		return REPLAY_ERROR_FILE_IO_ERROR;
	}

	if( sm_DoFileOpFunc != NULL )
	{
		sm_DoFileOpFunc(handle);
	}				

	CFileMgr::CloseFile(handle);

	return REPLAY_ERROR_SUCCESS;
}

eReplayFileErrorCode ReplayFileManager::SaveClip(ReplayFileInfo& info)
{
	// This check should already be cached.

	u64 dataSize = 0;
	for(int i = 0; i < info.m_BlocksToSave.GetCount(); i++)
	{
		CBlockProxy proxy = info.m_BlocksToSave[i];

		if( proxy.IsValid())
		{
			dataSize += proxy.GetSizeUsed();
		}
	}

	eReplayMemoryWarning warning = REPLAY_MEMORYWARNING_NONE;
	if( !CheckAvailableDiskSpaceForClips(dataSize, false, 1.0f, warning) )
	{
		return REPLAY_ERROR_LOW_DISKSPACE;
	}

	/*
	// Debugging to output uncompressed replay files
	static int counter = 1;
	char buffer[256] = {0};
	sprintf(buffer, "C:/replay_%02d.dat", counter++);
	info.m_FilePath = buffer;
	info.m_Header.bCompressed = false;
	*/

	if( info.m_BlocksToSave.GetCount() == 0 )
	{
		return REPLAY_ERROR_FAILURE;
	}
	
	u32 startTime = 0;
	u32 endTime = 0;
	u32 frameFlags = 0;

	//grab the start/end times of the clip
	CReplayMgr::GetPacketInfoInSaveBlocks(info.m_BlocksToSave, startTime, endTime, frameFlags);
	
	//we've failed to find a end time or start time. Return failure as this isn't handled.
	if( startTime == 0 || endTime == 0 )
	{
#if __BANK
		for(int i = 0; i < info.m_BlocksToSave.GetCount(); ++i)
		{
			CBlockProxy block = info.m_BlocksToSave[i];

			replayDisplayf("Block Index: %u, Session Index: %u, Block Start Time: %u, Block End Time: %u", block.GetMemoryBlockIndex(), block.GetSessionBlockIndex(), block.GetStartTime(), block.GetEndTime());
		}
#endif //__BANK

		replayAssertf(false, "Replay: Failed to find a valid start or end time. Start Time: %u, End Time: %u", startTime, endTime);

		return REPLAY_ERROR_FAILURE;
	}

	u32 markerStartTime = info.m_Header.eventMarkers[0].GetStartTime();

	if( replayVerifyf(markerStartTime != 0, "Marker start time is invalid, using the clips first gametime packet value instead"))
	{
		//override start time with the marker start time as we want to clip any unwanted bits from the start.		
		if( markerStartTime < startTime )
		{
			replayAssertf(false, "Marker start time is before the beginning of the clip. Marker Time: %u, Start Time: %u", markerStartTime, startTime);
			markerStartTime = startTime;
			info.m_Header.eventMarkers[0].SetStartTime(markerStartTime);
		}

		startTime = markerStartTime;
	}

	info.m_Header.fClipTime = float(endTime - startTime) / 1000.0f;
	info.m_ClipLength = info.m_Header.fClipTime;

	replayDebugf3("Clip Length: %f", info.m_Header.fClipTime);

	if( u32(info.m_Header.fClipTime * 1000) < MINIMUM_CLIP_LENGTH )
	{
		replayAssertf(false, "Replay Clip Length is too short: %u, should be at least %u", u32(info.m_Header.fClipTime * 1000), MINIMUM_CLIP_LENGTH);
	
		return REPLAY_ERROR_FAILURE;
	}

	info.m_Header.frameFlags = frameFlags;

	FileHandle handle = OpenFile(info);
	
	if( handle == NULL )
	{
		return REPLAY_ERROR_FILE_IO_ERROR;
	}

	//write the empty bytes for the crc
	//this will be filled when working out the crc in SetSaved()
	u32 headerCrc = 0;
	CFileMgr::Write(handle, (const char*)&(headerCrc), sizeof(u32));

	u32 clipCrc = 0;
	CFileMgr::Write(handle, (const char*)&(clipCrc), sizeof(u32));

	CFileMgr::Write(handle, (const char*)&(info.m_Header), sizeof(ReplayHeader));
	
	eReplayFileErrorCode result = info.m_Header.bCompressed ? SaveCompressedClip(info, handle) : SaveUncompressedClip(info, handle);

	u64 size = CFileMgr::GetTotalSize(handle);
	CFileMgr::CloseFile(handle);

	const char* filePath = info.m_FilePath.c_str();

	if( result == REPLAY_ERROR_SUCCESS )
	{
		SetSaved(info);

		//update the cache with the clip
		ClipData* clip = sm_Device->AppendClipToCache( info.m_Header.uID );
		if( clip )
		{
			ReplayHeader& header = info.m_Header;
			clip->Init(filePath, header, VERSION_OPERATION_OK, false, info.m_Header.wasModifiedContent);
			clip->SetSize(size);
			clip->SetLastWriteTime(sm_Device->GetFileTime(info.m_FilePath));
			clip->SetUserId(getCurrentUserIDToPrint());

			replayDisplayf("Adding clip %s to cache", filePath);
		}
		else
		{
			replayDisplayf("Failed to add clip %s to cache", filePath);
		}
	}
	else
	{
		replayDebugf3("Failed to save out clip, deleting empty file.");
		sm_Device->Delete(filePath);
		return result;
	}

	return result;
}

eReplayFileErrorCode ReplayFileManager::SaveCompressedClip(ReplayFileInfo& info, FileHandle handle)
{
	//write out the replay buffer
	int result = 0;
	s16 prevSessionBlock = info.m_BlocksToSave[0].GetSessionBlockIndex()-1;

	for(u16 i = 0; i < info.m_BlocksToSave.GetCount(); i++)
	{
		CBlockProxy block = info.m_BlocksToSave[i];

		if(prevSessionBlock != -1 && prevSessionBlock+1 != block.GetSessionBlockIndex())
		{
			replayAssertf(false, "Failed to save clip: %s Session block index is not in sequence. Session Block: %u Previous Session Block: %u ", info.m_FilePath.c_str(), prevSessionBlock, block.GetSessionBlockIndex());

			return REPLAY_ERROR_FAILURE;
		}

		prevSessionBlock = block.GetSessionBlockIndex();

		result = LZ4_compress(reinterpret_cast<const char*>(block.GetData()), reinterpret_cast<char*>(sm_CompressionBuffer), info.m_Header.PhysicalBlockSize);
		if( result <= 0 )
		{
#if DO_REPLAY_OUTPUT
			for(u16 i = 0; i < info.m_BlocksToSave.GetCount(); i++)
			{
				CBlockProxy proxy = info.m_BlocksToSave[i];

				replayDisplayf("Block - Index: %u Session: %u Status: %u IsTempBlock: %s IsValid: %s", proxy.GetMemoryBlockIndex(), proxy.GetSessionBlockIndex(), proxy.GetStatus(), proxy.IsTempBlock() ? "YES" : "NO", proxy.IsValid() ? "YES" : "NO");
			}
#endif
			replayAssertf(false, "Failed to compress block, returning failure!");

			return REPLAY_ERROR_FAILURE;
		}

		//write out the block header
		ReplayBlockHeader blockHeader;
		blockHeader.m_CompressedSize = (u32)result;
		block.PopulateBlockHeader(blockHeader);
		blockHeader.Validate();

#if DO_CRC_CHECK
		info.m_CRC = crc32(info.m_CRC, (const u8*)sm_CompressionBuffer, blockHeader.m_CompressedSize);
#endif
		CFileMgr::Write(handle, (const char*)&blockHeader, sizeof(ReplayBlockHeader));

		u32 remainingBytes = (u32)result;
		const char* buffer = (const char*)sm_CompressionBuffer;

		//write out the block in small chunks
		while(remainingBytes > 0)
		{
			u32 sizeToWrite = MIN(sm_BytesToWrite, remainingBytes);
			s32 ReturnVal = CFileMgr::Write(handle, buffer, sizeToWrite);

			if( ReturnVal <= 0 )
			{
				replayAssertf(false, "Failed to write any data out to disk, check disk space availiable.");
				return REPLAY_ERROR_FAILURE;
			}

			buffer += sizeToWrite;
			remainingBytes -= sizeToWrite;

			//whilst streaming is under load, don't allow replay to save
			//if we are under load, only continue saving if we risk overwriting data.
			while((!sm_SafeToSaveReplay BANK_ONLY(|| sm_simulateStreamingLoad)) && !sm_ForceSave)
			{
				sysIpcYield(PRIO_NORMAL);
			}
		}

		info.m_BlocksToSave[i].SetSaved();
	}

	return (result > 0) ? REPLAY_ERROR_SUCCESS : REPLAY_ERROR_FAILURE;
}

eReplayFileErrorCode ReplayFileManager::SaveUncompressedClip(ReplayFileInfo& info, FileHandle handle)
{
	s16 prevSessionBlock = info.m_BlocksToSave[0].GetSessionBlockIndex()-1;
	//write out the replay buffer
	for(u16 i = 0; i < info.m_BlocksToSave.GetCount(); i++)
	{
		CBlockProxy block = info.m_BlocksToSave[i];

		if(prevSessionBlock != -1 && prevSessionBlock+1 != block.GetSessionBlockIndex())
		{
			replayAssertf(false, "Failed to save clip: %s Session block index is not in sequence. Session Block: %u Previous Session Block: %u ", info.m_FilePath.c_str(), prevSessionBlock, block.GetSessionBlockIndex());

			return REPLAY_ERROR_FAILURE;
		}


		prevSessionBlock = block.GetSessionBlockIndex();

		ReplayBlockHeader blockHeader;
		block.PopulateBlockHeader(blockHeader);
		blockHeader.Validate();
		
		CFileMgr::Write(handle, (const char*)&blockHeader, sizeof(ReplayBlockHeader));

		CFileMgr::Write(handle, (const char*)block.GetData(), info.m_Header.PhysicalBlockSize);

		info.m_BlocksToSave[i].SetSaved();
	}

	return REPLAY_ERROR_SUCCESS;
}

u32 ReplayFileManager::GetCrC(FileHandle handle, u32 uOffset, u32 uSize)
{
	//grab current offset
	s32 offset = CFileMgr::Tell(handle);

	//seek to the offset
	CFileMgr::Seek(handle, uOffset);

	//init crc
	uLong crc = crc32(0L, Z_NULL, 0);

	const u32 bufferSize = 1024;
	u8* buffer[bufferSize] = {0};
	
	replayAssertf(uSize+uOffset <= CFileMgr::GetTotalSize(handle), "Potentially CRC'd passed the end of the file. (Total Size: %u - Actual Size: %u)", uSize+uOffset, CFileMgr::GetTotalSize(handle));

	while (CFileMgr::Tell(handle) < uSize+uOffset)
	{
		s32 readSize = CFileMgr::Read(handle, (char*)buffer, bufferSize);
		crc = crc32(crc, (const u8*)buffer, readSize);
	}

	//reset back to the original position
	CFileMgr::Seek(handle, offset);

	return crc;
}

#if DO_CRC_CHECK
void ReplayFileManager::SetSaved(ReplayFileInfo& info, bool doClipCrc)
#else
void ReplayFileManager::SetSaved(ReplayFileInfo& info, bool UNUSED_PARAM(doClipCrc))
#endif
{
	//Reopen the file and rewrite the header with the save flag set so we're sure the file has been written correctly.
	FileHandle handle = OpenFile(info, EDIT_OP);

	if(handle == NULL)
	{
		return;
	}

	//set file as saved
	info.m_Header.bSaved = true;

	//in version 4 and greater we need to account for the header crc
	u32 uSof = info.m_Header.uHeaderVersion <= HEADERVERSION_V3 ? sizeof(u32) : sizeof(u32)*2;

	//make sure we're at the start of the file.
	//accounting for the crc offset
	CFileMgr::Seek(handle, uSof);

	//write the new header
	CFileMgr::Write(handle, (const char*)&(info.m_Header), info.m_Header.uSize);

	if( info.m_Header.uHeaderVersion >= HEADERVERSION_V4)
	{
		//-------------------------HEADER CRC---------------------------------

		//grab the header CRC
		uLong crc = crc32(0L, Z_NULL, 0);							
		crc = crc32(crc, (const u8*)&info.m_Header, info.m_Header.uSize);

		//grab current offset
		s32 uOffset = CFileMgr::Tell(handle);

		//seek to the beginning so we can write the header crc as it will be the first thing in the file
		CFileMgr::Seek(handle, 0);

		//write the new header
		CFileMgr::Write(handle, (const char*)&(crc), sizeof(u32));

		//reset back to previous offset
		CFileMgr::Seek(handle, uOffset);

		//-------------------------EOF HEADER CRC-------------------------------
	}
	
#if DO_CRC_CHECK
	if( doClipCrc )
	{
		uLong uCRC = info.m_CRC;
		u32 uOffset = 0;

		if( info.m_Header.uHeaderVersion < HEADERVERSION_V4 )
		{		
#if RSG_PC
			//Work out the current offset of where to start the crc			
			GetOffsetToClipData(info.m_Header, uOffset);

			//get size of the file
			s32 uSize = CFileMgr::GetTotalSize(handle) - uOffset;

			//grab the CRC
			uCRC = GetCrC(handle, uOffset, uSize);

			uOffset = 0;
#endif
		}
		else
		{
			//this is to offset us passed the clip crc in version 3 or less.
			uOffset = sizeof(u32);
		}

		//make sure we're at the start of the file.
		CFileMgr::Seek(handle, uOffset);

		//write the crc
		CFileMgr::Write(handle, (const char*)&uCRC, sizeof(u32));
	}
#endif // RSG_PC && DO_CRC_CHECK
	
	//close the file
	CFileMgr::CloseFile(handle);

	UpdateHeaderCache(info.m_FilePath.c_str(), &info.m_Header);
}

struct loadInfo
{
	FileHandle handle;
	int previousSessionIndex;
	ReplayFileInfo* info;
};
int ReplayFileManager::LoadCompressedBlock(CBlockProxy block, void* pParam)
{
	loadInfo* pInfo = (loadInfo*)pParam;

	ReplayBlockHeader blockHeader;
	u32 sizeOfheader;
	s32 pos = CFileMgr::Tell(pInfo->handle);
	CFileMgr::Read(pInfo->handle, (char*)&sizeOfheader, sizeof(u32));

	//Jump back to the start of the file after we've grabbed the size.
	CFileMgr::Seek(pInfo->handle, pos);

	//read the file header
	CFileMgr::Read(pInfo->handle, (char*)&blockHeader, sizeOfheader);

	if( !blockHeader.IsValid() )
	{
		replayAssertf(false, "Block header is corrupt!");

		return REPLAY_ERROR_FILE_CORRUPT;
	}

	if(pInfo->previousSessionIndex != -1 && pInfo->previousSessionIndex+1 != blockHeader.m_SessionBlock)
	{
		replayAssertf(false, "Failed to load clip: %s Session block index is not in sequence. Session Block: %u Previous Session Block: %u ", pInfo->info->m_FilePath.c_str(), pInfo->previousSessionIndex, blockHeader.m_SessionBlock);

		return REPLAY_ERROR_FAILURE;
	}

	if(blockHeader.m_Status != REPLAYBLOCKSTATUS_FULL && pInfo->previousSessionIndex == -1 && pInfo->info->m_Header.PhysicalBlockCount > 1)
	{
		replayAssertf(false, "Block inconsistency issue...");

		return REPLAY_ERROR_FILE_CORRUPT;
	}

	pInfo->previousSessionIndex = blockHeader.m_SessionBlock;

	block.SetFromBlockHeader(blockHeader);
	
	CFileMgr::Read(pInfo->handle, (char*)sm_CompressionBuffer, blockHeader.m_CompressedSize);
	
#if DO_CRC_CHECK
	pInfo->info->m_CRC = crc32(pInfo->info->m_CRC, (const u8*)sm_CompressionBuffer, blockHeader.m_CompressedSize);
#endif

	int result = LZ4_decompress_fast((const char*) sm_CompressionBuffer, (char*) block.GetData(), sm_CompressionBufferSize);
	if( result <= 0 )
	{
		replayAssertf(false, "Failed to decompress clip %s with error %i", pInfo->info->m_FilePath.c_str(), result);

		return result;
	}

	Assertf((u32) result == blockHeader.m_CompressedSize, "Invalid decompressed replay buffer size: %d should have been: %d!", result, blockHeader.m_CompressedSize);

	return REPLAY_ERROR_SUCCESS;
}


eReplayFileErrorCode ReplayFileManager::LoadCompressedClip(ReplayFileInfo& info, FileHandle handle)
{	
	loadInfo param;
	param.handle = handle;
	param.previousSessionIndex = -1;
	param.info = &info;

	int result = CReplayMgr::ForeachBlock(LoadCompressedBlock, (void*)&param, info.m_Header.PhysicalBlockCount);

	return (result == 0) ? REPLAY_ERROR_SUCCESS : REPLAY_ERROR_FAILURE;
}

int ReplayFileManager::LoadUncompressedBlock(CBlockProxy block, void* pParam)
{
	loadInfo* pInfo = (loadInfo*)pParam;

	ReplayBlockHeader blockHeader;
	u32 sizeOfheader;
	s32 pos = CFileMgr::Tell(pInfo->handle);
	CFileMgr::Read(pInfo->handle, (char*)&sizeOfheader, sizeof(u32));

	//Jump back to the start of the file after we've grabbed the size.
	CFileMgr::Seek(pInfo->handle, pos);

	//read the file header
	CFileMgr::Read(pInfo->handle, (char*)&blockHeader, sizeOfheader);

	block.SetFromBlockHeader(blockHeader);

	if( !blockHeader.IsValid() )
	{
		replayAssertf(false, "Block header is corrupt!");

		return REPLAY_ERROR_FILE_CORRUPT;
	}

	if(pInfo->previousSessionIndex != -1 && pInfo->previousSessionIndex+1 != blockHeader.m_SessionBlock)
	{
		replayAssertf(false, "Failed to load clip: %s Session block index is not in sequence. Session Block: %u Previous Session Block: %u ", pInfo->info->m_FilePath.c_str(), pInfo->previousSessionIndex, blockHeader.m_SessionBlock);

		return REPLAY_ERROR_FAILURE;
	}

	if(blockHeader.m_Status != REPLAYBLOCKSTATUS_FULL && pInfo->previousSessionIndex == -1 && pInfo->info->m_Header.PhysicalBlockCount > 1)
	{
		replayAssertf(false, "Block inconsistency issue...");

		return REPLAY_ERROR_FILE_CORRUPT;
	}

	pInfo->previousSessionIndex = blockHeader.m_SessionBlock;

	CFileMgr::Read(pInfo->handle, (char*)block.GetData(), pInfo->info->m_Header.PhysicalBlockSize);

	return REPLAY_ERROR_SUCCESS;
}

eReplayFileErrorCode ReplayFileManager::LoadUncompressedClip(ReplayFileInfo& info, FileHandle handle)
{
	loadInfo param;
	param.handle = handle;
	param.previousSessionIndex = -1;
	param.info = &info;

	int result = CReplayMgr::ForeachBlock(LoadUncompressedBlock, (void*)&param, info.m_Header.PhysicalBlockCount);

	return (result == 0) ? REPLAY_ERROR_SUCCESS : REPLAY_ERROR_FAILURE;
}

eReplayFileErrorCode ReplayFileManager::ReadHeader(FileHandle handle, ReplayHeader& replayHeader, const char *pFileName, uLong& uCRC, bool doClipCrc)
{	
	(void)pFileName;
	(void)doClipCrc;

	u32 uStartOfHeader = sizeof(u32);

	CFileMgr::Seek(handle, uStartOfHeader);

	u32 fileType;
	CFileMgr::Read(handle, (char*)&fileType, sizeof(u32));
	
	CFileMgr::Seek(handle, 0);

	//crude check to see if the this is a a clip with a header version of HEADERVERSION_V4, 
	//this this check fails we assume the header CRC is that the top of the file
	u32 headerOriginalCrc = 0;
	if(fileType != 'RPLY')
	{
		//read the original crc
		CFileMgr::Read(handle, (char*)&headerOriginalCrc, sizeof(u32));

		//start of the header is after the clip data crc and the header crc (both u32)
		uStartOfHeader = sizeof(u32)*2;
	}
	/*
	u32 originalCrc = 0;*/
	//read the original crc
	CFileMgr::Read(handle, (char*)&uCRC, sizeof(u32));

	//we check this again when we think the file is in the correct order.
	fileType = 0;
	CFileMgr::Read(handle, (char*)&fileType, sizeof(u32));
	
	//check to see if its a valid file
	if(fileType != 'RPLY')
	{
		replayDisplayf("CReplayMgr::LoadHeader: Filetype invalid [%s]", pFileName);
		return REPLAY_ERROR_FILE_CORRUPT;
	}

	u32 fileVersion;
	CFileMgr::Read(handle, (char*)&fileVersion, sizeof(u32));

	if(fileVersion != ReplayHeader::GetVersionNumber())
	{
		replayErrorf("CReplayMgr::LoadHeader: FileVersion invalid [%s]", pFileName);
		return REPLAY_ERROR_FILE_CORRUPT;
	}
	
	u32 sizeOfheader;
	CFileMgr::Read(handle, (char*)&sizeOfheader, sizeof(u32));

	//Jump back to the start of the file after we've grabbed the size.
	//accounting for the crc offsets
	CFileMgr::Seek(handle, uStartOfHeader);
	
	if( sizeOfheader > u32(sizeof(ReplayHeader)) )
	{
		replayDisplayf("CReplayMgr::LoadHeader: Size of the header in the clip is greater than the size of the current header (Size in clip: %u - Size of header: %u)", sizeOfheader, u32(sizeof(ReplayHeader)));
		return REPLAY_ERROR_FILE_CORRUPT;
	}

	//read the file header
	CFileMgr::Read(handle, (char*)&replayHeader, Min(u32(sizeof(ReplayHeader)), sizeOfheader));
	
	if( replayHeader.uHeaderVersion >= HEADERVERSION_V4 )
	{
		uLong crc = crc32(0L, Z_NULL, 0);							
		crc = crc32(crc, (const u8*)&replayHeader, sizeOfheader);

		//generate the crc
		if( headerOriginalCrc != crc )
		{
			replayDisplayf("[ReplayFileManager] - Header crc doesn't match (original: %u - actual: %u)", headerOriginalCrc, u32(crc));

			return REPLAY_ERROR_FILE_CORRUPT;
		}
	}
	
#if DO_CRC_CHECK
	if( doClipCrc )
	{
#if RSG_PC
		if( replayHeader.uHeaderVersion < HEADERVERSION_V4 )
		{
			//offset from the beginning of the file
			u32 uOffset = 0;
			GetOffsetToClipData(replayHeader, uOffset);

			//get size of the file
			s32 uSize = CFileMgr::GetTotalSize(handle) - uOffset;

			//generate the crc
			u32 crc = GetCrC(handle, uOffset, uSize);

			//compare crc's
			if((uCRC != crc)  BANK_ONLY(&& (uCRC != 0))) // PS4 doesn't write out a CRC, for internal dev. we skip the check if CRC is zero for cross platform compatibility.
			{
				replayDisplayf("Crc doesn't match");

				return REPLAY_ERROR_FILE_CORRUPT;
			}
		}
#endif
		
	}
#endif // DO_CRC_CHECK

	if(replayHeader.bSaved == false)
	{
		replayDisplayf("CReplayMgr::LoadHeader: File wasn't saved correctly, replayHeader.bSaved = %s [%s]", replayHeader.bSaved ? "TRUE" : "FALSE", pFileName);
		return REPLAY_ERROR_FILE_CORRUPT;
	}

	if( !(replayHeader.bHasGuards && REPLAY_GUARD_ENABLE == 1) )
	{
		replayDisplayf("CReplayMgr::LoadHeader: File guard check doesn't match REPLAY_GUARD_ENABLE, replayHeader.bHasGuards = %s, REPLAY_GUARD_ENABLE = %s [%s]", replayHeader.bHasGuards ? "TRUE" : "FALSE", REPLAY_GUARD_ENABLE == 1 ? "TRUE" : "FALSE", pFileName);
		return REPLAY_ERROR_FILE_CORRUPT;
	}

//#if DO_VERSION_CHECK
//	if(replayHeader.uFileType != 'RPLY' || replayHeader.uFileVersion != ReplayHeader::GetVersionNumber())
//#else

	int dateCmp = strcmp(replayHeader.replayVersionDate, __DATE__);
	int timeCmp = strcmp(replayHeader.replayVersionTime, __TIME__);
	if(dateCmp != 0 || timeCmp != 0)
	{
		replayDebugf3("CReplayMgr::LoadHeader: [INFO ONLY] Loading a clip recorded with a different executable! (Clip file: %s, %s | Exe: %s, %s) [%s]", 
			replayHeader.replayVersionDate, replayHeader.replayVersionTime, __DATE__, __TIME__, pFileName);
	}

	replayDebugf3("Clip Length: %f", replayHeader.fClipTime);
	replayAssertf(u32(replayHeader.fClipTime * 1000) >= MINIMUM_CLIP_LENGTH, "Replay Clip Length is too short: %u, should be at least %u", u32(replayHeader.fClipTime * 1000), MINIMUM_CLIP_LENGTH);

	return REPLAY_ERROR_SUCCESS;
}

void ReplayFileManager::GetOffsetToClipData(const ReplayHeader& replayHeader, u32& uOffset)
{
	if( replayHeader.uHeaderVersion <= HEADERVERSION_V2 )
	{
		//this version includes the header with the crc
		uOffset = sizeof(u32);
	}
	else if( replayHeader.uHeaderVersion <= HEADERVERSION_V3 )
	{
		uOffset = sizeof(u32) + replayHeader.uSize;
	}
	else if( replayHeader.uHeaderVersion <= HEADERVERSION_V4 )
	{
		uOffset = (sizeof(u32) * 2) + replayHeader.uSize;
	}
}

const ReplayHeader* ReplayFileManager::GetHeader(const char* filename)
{
	ReplayHeader* header = sm_HeaderCache.Get(filename);

	if( header )
	{
		return header;
	}

	return NULL;
}

FileHandle ReplayFileManager::OpenFile(ReplayFileInfo& info, FileStoreOperation fileOp)
{	
	//make sure the directory have been created
	if( !ASSET.CreateLeadingPath(info.m_FilePath.c_str()) )
	{
		replayDebugf2("REPLAY: CreateLeadingPath - %s", info.m_FilePath.c_str());
	}

	FileHandle fileHandle = NULL;
	BANK_ONLY(const char* fileOpStr = "";)

		switch (fileOp)
	{
		case CREATE_OP:
			BANK_ONLY(fileOpStr = "create";)
				fileHandle = CFileMgr::OpenFile(info.m_FilePath.c_str(), "w");
			break;
		case READ_ONLY_OP:
			BANK_ONLY(fileOpStr = "open";)
				fileHandle = CFileMgr::OpenFile(info.m_FilePath.c_str());
			break;
		case EDIT_OP:
			BANK_ONLY(fileOpStr = "edit";)
				fileHandle = CFileMgr::OpenFile(info.m_FilePath.c_str(), "a");
			break;
		default:
			break;
	}	

	if(CFileMgr::IsValidFileHandle(fileHandle) == false)
	{
		BANK_ONLY(replayDebugf2("REPLAY: Failed to %s file - %s", fileOpStr, info.m_FilePath.c_str());)

			return NULL;
	}

	return fileHandle;
}

FileHandle ReplayFileManager::OpenFile(ReplayFileInfo& info)
{
	FileStoreOperation op = CREATE_OP;
	if( info.m_FileOp == REPLAY_FILEOP_LOAD || info.m_FileOp == REPLAY_FILEOP_LOAD_CLIP || info.m_FileOp == REPLAY_FILEOP_LOAD_HEADER )
	{
		op = READ_ONLY_OP;
	}

	return OpenFile(info, op);
}

#if !REPLAY_USE_PER_BLOCK_THUMBNAILS
bool ReplayFileManager::RequestThumbnailSaveAsync()
{
	bool success = false;

	if( Verifyf( !sm_thumbnailSaveData.IsInitialized(), "ReplayFileManager::RequestAsyncThumbnail - Async thumbnail save already in progress. Logic error?" ) )
	{
		if( Verifyf( sm_thumbnailSaveData.m_asyncBuffer.Initialize( SCREENSHOT_WIDTH, SCREENSHOT_HEIGHT ),
			"ReplayFileManager::RequestAsyncThumbnail - Unable to allocate thumbnail buffer" ) )
		{
			sm_thumbnailSaveData.m_path = sm_CurrentReplayFileInfo.m_FilePath;
			sm_thumbnailSaveData.m_path.Replace(".clip", "");

			success = CPhotoManager::RequestSaveScreenshotToBuffer( 
				sm_thumbnailSaveData.m_asyncBuffer, sm_thumbnailSaveData.m_asyncBuffer.GetWidth(), sm_thumbnailSaveData.m_asyncBuffer.GetHeight() );
		}
	}

	return success;
}

void ReplayFileManager::BlockOnThumbnailSaveAsync()
{
	// Spin - Chances are by the time we reach here we have the buffer though!
	while( sm_thumbnailSaveData.IsInitialized() 
		&& !sm_thumbnailSaveData.m_asyncBuffer.IsPopulated() )
	{
		sysIpcSleep( 10 );
	}
}
#endif // REPLAY_USE_PER_BLOCK_THUMBNAILS

void ReplayFileManager::ResetSaveFlags(ReplayFileInfo& info)
{
	for(u16 i = 0; i < info.m_BlocksToSave.GetCount(); i++)
	{
		info.m_BlocksToSave[i].SetSaved();
	}
}

bool ReplayFileManager::IsMarkedAsFavourite(const char* filePath)
{
	return sm_Device->IsMarkedAsFavourite(filePath);
}

void ReplayFileManager::MarkAsFavourite(const char* filePath)
{
	sm_Device->MarkFile(filePath, MARK_AS_FAVOURITE);
}

void ReplayFileManager::UnMarkAsFavourite(const char* filePath)
{
	sm_Device->MarkFile(filePath, UNMARK_AS_FAVOURITE);
}

bool ReplayFileManager::IsMarkedAsDelete(const char* filePath)
{
	return sm_Device->IsMarkedAsDelete(filePath);
}

u32	ReplayFileManager::GetCountOfFilesToDelete(const char* filter)
{
	return sm_Device->GetCountOfFilesToDelete(filter);
}

void ReplayFileManager::MarkAsDelete(const char* filePath)
{
	sm_Device->MarkFile(filePath, MARK_AS_DELETE);
}

void ReplayFileManager::UnMarkAsDelete(const char* filePath)
{
	sm_Device->MarkFile(filePath, UNMARK_AS_DELETE);
}

u32 ReplayFileManager::GetClipRestrictions(const ClipUID& clipUID)
{
	return sm_Device->GetClipRestrictions(clipUID);
}

#endif
