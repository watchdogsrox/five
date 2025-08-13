#include "device_replay.h"

#if GTA_REPLAY

#include "system/param.h" 


#include "control/replay/File/ReplayFileManager.h"
#include "replaycoordinator/ReplayEditorParameters.h"
#include "replaycoordinator/storage/Montage.h"
#include "text/TextFile.h"

#if !__FINAL
#include "file\asset.h"
#endif

#if RSG_ORBIS
#include <_fs.h>
#include "video/contentDelete_Orbis.h"
#endif

#define REPLAY_PROCESS_VERSION_ACTION	1
#define REPLAY_DEBUG_DELETE_FILES !__FINAL

PARAM(fakeModdedContent, "[REPLAY] fakes that clips have modded content");
PARAM(deleteIncompatibleReplayClips, "[REPLAY] deletes incompatible replay clips");
XPARAM(replayFilesWorkingDir);

const ClipUID ClipUID::INVALID_UID;

#if __BANK
bool		fiDeviceReplay::sm_UpdateClipWidget = false;
bool		fiDeviceReplay::sm_UpdateFavouritesWidget = false;
bool		fiDeviceReplay::sm_UpdateSpaceWidgets = false;

int			fiDeviceReplay::sm_ClipIndex = 0;
bkCombo*	fiDeviceReplay::sm_ClipCacheCombo = NULL;
int			fiDeviceReplay::sm_FavouritesIndex = 0;
bkCombo*	fiDeviceReplay::sm_FavouritesCombo = NULL;
bkText*		fiDeviceReplay::sm_MontageMemory = NULL;
bkText*		fiDeviceReplay::sm_ClipMemory = NULL;

bkText*		fiDeviceReplay::sm_MontageUsage = NULL;
bkText*		fiDeviceReplay::sm_ClipUsage = NULL;
bkText*		fiDeviceReplay::sm_VideoUsage = NULL;
bkText*		fiDeviceReplay::sm_DataUsage = NULL;

char		fiDeviceReplay::sm_montMemBuffer[64];
char		fiDeviceReplay::sm_clipMemBuffer[64];
char		fiDeviceReplay::sm_montUsgBuffer[64];
char		fiDeviceReplay::sm_clipUsgBuffer[64];
char		fiDeviceReplay::sm_vidUsgBuffer[64];
char		fiDeviceReplay::sm_dataUsgBuffer[64];
#endif // __BANK

void ClipData::Init(const char* filename, const ReplayHeader& header, eFileStoreVersionOperation versionOp, bool favourite, bool moddedContent, bool bCorrupt)
{
	CacheFileData::Init(filename);
	SetDuration(u32(header.fClipTime * 1000));
	m_VersionOp = versionOp;
	m_HeaderVersion = header.uHeaderVersion;
	m_Favourite = favourite;
	m_FavouriteModified = false;
	m_FrameFlags = header.frameFlags;
	m_Uid = header.uID;
	m_ModdedContent = moddedContent;
	m_Corrupt = bCorrupt;

	if(header.uHeaderVersion >= HEADERVERSION_V2)
	{
		m_prevSequenceHash = header.previousSequenceHash;
		m_nextSequenceHash = header.nextSequenceHash;
	}
}


fiDeviceReplay::fiDeviceReplay() 
	: fiDeviceRelative()
	, m_ShouldUpdateDirectorySize(true)
	, m_ShouldUpdateClipsSize(false)
	, m_ShouldUpdateMontageSize(false)
	, m_clipSize(0)
	, m_montageSize(0)
	, m_videoSize(0)
	, m_dataSize(0)
	, m_HasEnumeratedClips(false)
	, m_HasEnumeratedMontages(false)
	, m_IsEnumerating(false)
	, m_ResetEnumerationState(false)
{

}

void fiDeviceReplay::Init()
{
	const char* directory = GetDirectory();

#if DO_CUSTOM_WORKING_DIR
	//forces the directory to be our custom one.
	const char* debugDirectory = "";
	if( GetDebugPath(debugDirectory) )
	{
		directory = debugDirectory;
	}
#endif

	fiDeviceRelative::Init(directory, false);

	m_IsEnumerating = false;
	m_ResetEnumerationState = false;
}

void fiDeviceReplay::Process()
{
#if __BANK
	if( sm_UpdateClipWidget )
	{
		UpdateClipCacheWidget();
		sm_UpdateClipWidget = false;
	}

	if( sm_UpdateFavouritesWidget )
	{
		UpdateFavouritesWidget();
		sm_UpdateFavouritesWidget = false;
	}

	if( sm_UpdateSpaceWidgets )
	{
		UpdateDebugSpaceWidgets();
		sm_UpdateSpaceWidgets = false;
	}
#endif

	if(!m_IsEnumerating && m_ResetEnumerationState)
	{
		m_HasEnumeratedClips = false;
		m_HasEnumeratedMontages = false;
		m_ResetEnumerationState = false;
	}
}

#if DO_CUSTOM_WORKING_DIR
bool fiDeviceReplay::GetDebugPath(const char* &path)
{
	if(PARAM_replayFilesWorkingDir.Get(path))
	{
		return true;
	}

	return false;
}
#endif // DO_CUSTOM_WORKING_DIR

#if __BANK

void fiDeviceReplay::AddDebugWidgets(bkBank& bank)
{
	sm_ClipIndex = 0;
	sm_ClipCacheCombo = bank.AddCombo( "Clip Cache", &sm_ClipIndex, 0, NULL);

	sm_FavouritesIndex = 0;
	sm_FavouritesCombo = bank.AddCombo( "Favourites", &sm_FavouritesIndex, 0, NULL);

	sprintf_s(sm_montMemBuffer, 64, "%.2f MB", BytesToMB(GetFreeSpaceAvailableForMontages()));
	sm_MontageMemory = bank.AddText("Free project space", sm_montMemBuffer, 64);
	
	sprintf_s(sm_clipMemBuffer, 64, "%.2f MB", BytesToMB(GetFreeSpaceAvailableForClips()));
	sm_ClipMemory = bank.AddText("Free clips space", sm_clipMemBuffer, 64);
	
	sprintf_s(sm_montUsgBuffer, 64, "%.2f MB", BytesToMB(m_montageSize));
	sm_MontageUsage = bank.AddText("Project Usage", sm_montUsgBuffer, 64);

	sprintf_s(sm_clipUsgBuffer, 64, "%.2f MB", BytesToMB(m_clipSize));
	sm_ClipUsage = bank.AddText("Clip Usage", sm_clipUsgBuffer, 64);

	sprintf_s(sm_vidUsgBuffer, 64, "%.2f MB", BytesToMB(m_videoSize));
	sm_VideoUsage = bank.AddText("Video Usage", sm_vidUsgBuffer, 64);

	sprintf_s(sm_dataUsgBuffer, 64, "%.2f MB", BytesToMB(m_dataSize));
	sm_DataUsage = bank.AddText("Data Usage", sm_dataUsgBuffer, 64);
}

void fiDeviceReplay::UpdateDebugSpaceWidgets()
{
	if( sm_MontageMemory )
	{
		sprintf_s(sm_montMemBuffer, 64, "%.2f MB", BytesToMB(GetFreeSpaceAvailableForMontages()));
	}
	
	if( sm_ClipMemory )
	{
		sprintf_s(sm_clipMemBuffer, 64, "%.2f MB", BytesToMB(GetFreeSpaceAvailableForClips()));
	}

	if( sm_MontageUsage )
	{
		sprintf_s(sm_montUsgBuffer, 64, "%.4f MB", BytesToMB(m_montageSize));
	}
	
	if( sm_ClipUsage )
	{
		sprintf_s(sm_clipUsgBuffer, 64, "%.2f MB", BytesToMB(m_clipSize));
	}
	
	if( sm_VideoUsage )
	{
		sprintf_s(sm_vidUsgBuffer, 64, "%.2f MB", BytesToMB(m_videoSize));
	}
	
	if( sm_DataUsage )
	{
		sprintf_s(sm_dataUsgBuffer, 64, "%.2f MB", BytesToMB(m_dataSize));
	}
}

const char* fiDeviceReplay::GetDebugCurrentClip() const
{
	return sm_ClipCacheCombo->GetString(sm_ClipIndex);
}

void fiDeviceReplay::UpdateClipCacheWidget()
{
	sm_ClipIndex = 0;
	sm_ClipCacheCombo->UpdateCombo("Clip Cache", &sm_ClipIndex, m_CachedClips.GetCount()+1, NULL);
	sm_ClipCacheCombo->SetString( 0, "(none)" );

	for(int i = 0; i < m_CachedClips.GetCount(); i++)
	{
		const ClipData& data = m_CachedClips[i];
		sm_ClipCacheCombo->SetString( i, data.GetFilePath());
	}
}

void fiDeviceReplay::UpdateFavouritesWidget()
{
	int count = 0;
	for(int i = 0; i < m_CachedClips.GetCount(); i++)
	{
		const ClipData& data = m_CachedClips[i];
		if( data.GetFavourite() )
		{
			++count;
		}
	}
	
	sm_FavouritesIndex = 0;
	sm_FavouritesCombo->UpdateCombo("Favourites", &sm_FavouritesIndex, count > 0 ? count : 1, NULL);
	sm_FavouritesCombo->SetString( 0, "(none)" );

	count = 0;
	for(int i = 0; i < m_CachedClips.GetCount(); i++)
	{
		const ClipData& data = m_CachedClips[i];

		if( data.GetFavourite() )
		{
			sm_FavouritesCombo->SetString(count, data.GetFilePath());
			count++;
		}
	}
}
#endif // __BANK

u64	fiDeviceReplay::GetMaxMemoryAvailableForClips() const
{
	return GetMaxMemoryAvailableForClipsAndMontages();
}

u64	fiDeviceReplay::GetMaxMemoryAvailableForClipsAndMontages() const
{ 
	u64 maxAvailable = GetMaxMemoryAvailable();
	u64 maxLimitVideo = GetMaxMemoryAvailableForVideo();

	if(maxLimitVideo >= maxAvailable)
	{
		return 0;
	}

	return maxAvailable - maxLimitVideo;
}

void fiDeviceReplay::SetShouldUpdateDirectorySize() 
{
	m_ShouldUpdateDirectorySize = true; 
}

void fiDeviceReplay::SetShouldUpdateClipsSize() 
{
	m_ShouldUpdateClipsSize = true; 
}

void fiDeviceReplay::SetShouldUpdateMontagesSize() 
{
	m_ShouldUpdateMontageSize = true; 
}

u64 fiDeviceReplay::RefreshSizesAndGetTotalSize()
{
	// Only allow one thread at a time to enumerate. 
	sysCriticalSection cs(ms_CritSec);

	if( m_ShouldUpdateDirectorySize )
	{
		// The current directory size is invalid, or we've been asked to get it again.
		m_clipSize = GetDirectorySize(REPLAY_CLIPS_PATH);
		m_montageSize = GetDirectorySize(REPLAY_MONTAGE_PATH);
		m_videoSize = GetDirectorySize(REPLAY_VIDEOS_PATH);
		m_dataSize = GetDirectorySize(REPLAY_DATA_PATH);
		m_ShouldUpdateDirectorySize = false;
		m_ShouldUpdateClipsSize = false;
		BANK_ONLY(sm_UpdateSpaceWidgets = true;)
	}
	// a lighter test for clips, since this will be getting performed more in-game ...and others can't be changed in-game
	else 
	{
		if (m_ShouldUpdateClipsSize)
		{
			m_clipSize = GetDirectorySize(REPLAY_CLIPS_PATH);
			m_ShouldUpdateClipsSize = false;
			BANK_ONLY(sm_UpdateSpaceWidgets = true;)
		}
		
		if( m_ShouldUpdateMontageSize )
		{
			m_montageSize = GetDirectorySize(REPLAY_MONTAGE_PATH);
			m_ShouldUpdateMontageSize = false;
			BANK_ONLY(sm_UpdateSpaceWidgets = true;)
		}
	}

	return m_clipSize + m_montageSize + m_videoSize + m_dataSize;
}

u64 fiDeviceReplay::GetFreeSpaceAvailable(bool force)
{
	(void)force;

	u64 currentMemory = RefreshSizesAndGetTotalSize();
	u64 maxMemory = GetMaxMemoryAvailable();

	if(currentMemory >= maxMemory)
	{
		return 0;
	}

	return maxMemory - currentMemory;
}

u64 fiDeviceReplay::GetFreeSpaceAvailableForClips()
{
	return GetFreeSpaceAvailableForClipsAndMontages();
}

u64 fiDeviceReplay::GetUsedSpaceForClips() 
{ 
	u64 maxSpace = GetMaxMemoryAvailableForClips();
	u64 freeSpace = GetFreeSpaceAvailableForClips();

	return maxSpace - freeSpace; 
}

u64 fiDeviceReplay::GetFreeSpaceAvailableForClipsAndMontages()
{
	RefreshSizesAndGetTotalSize();
	u64 maxMemory = GetMaxMemoryAvailableForClipsAndMontages();
	u64 currentMemory = m_clipSize + m_montageSize;

	if(currentMemory >= maxMemory)
	{
		return 0;
	}

	return maxMemory - currentMemory;
}

u64 fiDeviceReplay::GetSizeOfProject(const char* path) const
{
	FileHandle handle = CFileMgr::OpenFile(path);

	if( !CFileMgr::IsValidFileHandle(handle) )
	{
		return 0;
	}

	s32 size = CFileMgr::GetTotalSize(handle);

	CFileMgr::CloseFile(handle);

	return u64(size);
}

u64 fiDeviceReplay::GetFreeSpaceAvailableForMontages()
{
	return GetFreeSpaceAvailableForClipsAndMontages();
}

u64 fiDeviceReplay::GetFreeSpaceAvailableForVideos()
{
	RefreshSizesAndGetTotalSize();

	u64 maxVideoMem = GetMaxMemoryAvailableForVideo();
	if (m_videoSize >= maxVideoMem)
	{
		return 0;
	}

	return maxVideoMem - m_videoSize;
}

u64 fiDeviceReplay::GetFreeSpaceAvailableForData()
{
	RefreshSizesAndGetTotalSize();

	if (m_dataSize >= DATA_FILE_LIMIT)
	{
		return 0;
	}

	return DATA_FILE_LIMIT - m_dataSize;
}

eReplayMemoryLimit fiDeviceReplay::GetMaxAvailableMemoryLimit()
{
	u64 availableSpace = GetAvailableAndUsedClipSpace();

	eReplayMemoryLimit max = REPLAY_MEM_250MB;
	for(int i = 0; i < REPLAY_MEM_LIMIT_MAX; ++i)
	{
		u64 memLimit = MemLimits[i].AsBytes();

		if( availableSpace < memLimit )
		{
			break;
		}

		max = (eReplayMemoryLimit)i;
	}

	return max;
}

eReplayMemoryLimit fiDeviceReplay::GetMinAvailableMemoryLimit()
{
	m_ShouldUpdateClipsSize = true;
	RefreshSizesAndGetTotalSize();

	eReplayMemoryLimit min = REPLAY_MEM_50GB;
	for(int i = REPLAY_MEM_LIMIT_MAX-1; i >= 0; --i)
	{
		u64 memLimit = MemLimits[i].AsBytes();

		if( m_clipSize > memLimit )
		{
			break;
		}

		min = (eReplayMemoryLimit)i;
	}

	return min;
}

float fiDeviceReplay::GetAvailableAndUsedClipSpaceInGB()
{
	u64 totalSpace = GetAvailableAndUsedClipSpace();

	return float(totalSpace >> 20) / 1024.0f;
}

u64 fiDeviceReplay::GetAvailableAndUsedClipSpace()
{
	u64 freeSpace = GetFreeSpaceAvailable(true);
	u64 totalSpace = freeSpace + m_clipSize + m_montageSize;

	return totalSpace;
}

bool fiDeviceReplay::CanFullfillSpaceRequirement( eReplayMemoryLimit size )
{
	u64 totalSpace = GetAvailableAndUsedClipSpace();
	u64 spaceAllocated = MemLimits[size].AsBytes();

	return totalSpace > spaceAllocated;
}

eReplayFileErrorCode fiDeviceReplay::LoadClip(ReplayFileInfo &info)
{
	FileHandle handle = ReplayFileManager::OpenFile(info);

	if( handle == NULL )
	{
		return REPLAY_ERROR_FILE_IO_ERROR;
	}

	uLong crc = 0;
	//Read the file header
	eReplayFileErrorCode ret = ReplayFileManager::ReadHeader(handle, info.m_Header, info.m_FilePath.c_str(), crc, true);

	if(ret != REPLAY_ERROR_SUCCESS)
	{
		//an error occurred close the file
		CFileMgr::CloseFile(handle);
		replayAssertf(false, "Clip file is invalid - %s", info.m_FilePath.c_str());
		return ret;
	}

	replayAssertf(info.m_Header.PlatformType == ReplayHeader::GetCurrentPlatform(), "Attempting to play clip %s recorded on %s...this may not work", info.m_FilePath.c_str(), ReplayHeader::GetPlatformString(info.m_Header.PlatformType));

	replayDebugf1("Clip file creation stamp - %s", info.m_Header.creationDateTime);

	//tell replay internal to allocate the memory if needed
	if( info.m_LoadStartFunc != NULL )
	{
		if( !(*info.m_LoadStartFunc)(info.m_Header) )
		{
			CFileMgr::CloseFile(handle);
			return REPLAY_ERROR_FAILURE;
		}
	}

	if( info.m_BufferInfo == NULL )
	{
		CFileMgr::CloseFile(handle);
		return REPLAY_ERROR_FAILURE;
	}

	eReplayFileErrorCode result = info.m_Header.bCompressed ? ReplayFileManager::LoadCompressedClip(info, handle) : ReplayFileManager::LoadUncompressedClip(info, handle);

#if DO_CRC_CHECK
	if( info.m_Header.uHeaderVersion >= HEADERVERSION_V4 )
	{
		if( crc != info.m_CRC )
		{
			replayDebugf1("CRC is different!");

			return REPLAY_ERROR_FILE_CORRUPT;
		}
	}
#endif

	CFileMgr::CloseFile(handle);

	return result;
}

eFileStoreVersionOperation	fiDeviceReplay::GetVersionOperation(u32 version)
{
#if REPLAY_PROCESS_VERSION_ACTION
	u32 currentVersion = ReplayHeader::GetVersionNumber();
	if( currentVersion != version )
	{
		if(PARAM_deleteIncompatibleReplayClips.Get())
		{
			return VERSION_OPERATION_DELETE;
		}
		else
		{
			return VERSION_OPERATION_DELETE;	//VERSION_OPERATION_LEAVE_OUT_OF_CACHE;
		}
	}
#else
	(void)version;
#endif

	return VERSION_OPERATION_OK;
}

bool fiDeviceReplay::ValidateMontage(const char* path)
{
	bool isValid = false;

	FileHandle hFile = CFileMgr::OpenFile(path);

	if( hFile != NULL )
	{
		CMontage::MontageHeaderVersionBlob versionData;

		CFileMgr::Read( hFile, (char*)&versionData, sizeof(versionData) );
		isValid = ( versionData.m_montageVersion <= CMontage::MONTAGE_VERSION && versionData.m_replayVersion == ReplayHeader::GetVersionNumber() );
		replayDisplayf("versionData.m_montageVersion %d %s", versionData.m_montageVersion, path);
		CFileMgr::CloseFile(hFile);
	}

	return isValid;
}

eReplayFileErrorCode fiDeviceReplay::LoadMontage( ReplayFileInfo &info, u64& inout_extendedResultData )
{
	u64 const c_userId = GetUserIdFromFileInfo( info );
	inout_extendedResultData = info.m_Montage->Load( info.m_FilePath.c_str(), c_userId, false, info.m_Filename.c_str() );
	eReplayFileErrorCode const c_errorCode = ( inout_extendedResultData & CReplayEditorParameters::PROJECT_LOAD_RESULT_FAILED ) == 0  ?
		REPLAY_ERROR_SUCCESS : REPLAY_ERROR_FAILURE;

	return c_errorCode;
}

eReplayFileErrorCode fiDeviceReplay::SaveMontage( ReplayFileInfo &info, u64& inout_extendedResultData )
{
	inout_extendedResultData = info.m_Montage->Save( info.m_Filename.c_str(), false );
	eReplayFileErrorCode const c_errorCode = inout_extendedResultData == 1 ? REPLAY_ERROR_SUCCESS : REPLAY_ERROR_FAILURE;

	return c_errorCode;
}

eReplayFileErrorCode fiDeviceReplay::LoadHeader(ReplayFileInfo& info)
{
	info.m_FileOp = REPLAY_FILEOP_LOAD_HEADER;
	FileHandle handle = ReplayFileManager::OpenFile(info);

	if( handle == NULL )
	{
		ReplayFileManager::UpdateHeaderCache(info.m_FilePath.c_str(), NULL);
		return REPLAY_ERROR_FILE_IO_ERROR;
	}

	uLong crc = 0;
	//Read the file header
	eReplayFileErrorCode ret = ReplayFileManager::ReadHeader(handle, info.m_Header, info.m_FilePath.c_str(), crc);
	if(ret != REPLAY_ERROR_SUCCESS)
	{
		ReplayFileManager::UpdateHeaderCache(info.m_FilePath.c_str(), NULL);

		CFileMgr::CloseFile(handle);

		return ret;
	}

	CFileMgr::CloseFile(handle);

	//add the header to the cache.
	ReplayFileManager::UpdateHeaderCache(info.m_FilePath.c_str(), &info.m_Header);

	return REPLAY_ERROR_SUCCESS;
}

u64 fiDeviceReplay::GetDirectorySize(const char* pathName, bool recurse )
{
	if (!recurse)
	{
		// format the path being passed in to make sure it's correct
		s32 stringLength = static_cast<s32>(strlen(pathName));
		if (stringLength == 0)
			return 0;

		if (pathName[stringLength-1] == '\\' || pathName[stringLength-1] == '/')
		{
			char formattedPath[RAGE_MAX_PATH] = { 0 };
			safecpy(formattedPath, pathName);
			formattedPath[stringLength-1] = '\0';
			return GetDirectorySize(formattedPath, true);
		}
	}

	//reset
	u64 directorySize = 0;
	bool result = true;

	fiFindData data;
	fiHandle handle = FindFileBegin(pathName, data);
	if(fiIsValidHandle(handle))
	{
		while(result)
		{
			char currentPath[RAGE_MAX_PATH];
			if((strcmp(data.m_Name, ".") != 0) && (strcmp(data.m_Name, "..") != 0))
			{
				// Found a directory, so deal with it
				if((data.m_Attributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)
				{
					// Build the new pathname to use
					formatf(currentPath, sizeof(currentPath), "%s\\%s", pathName, data.m_Name);

					// Recurse
					directorySize += GetDirectorySize(currentPath, true);
				}
				else
				{
					formatf(currentPath, sizeof(currentPath), "%s\\%s", pathName, data.m_Name);
					directorySize += GetFileSize(currentPath);
				}
			}
			result = FindFileNext(handle, data);
		}
		FindFileEnd(handle);
	}

	return directorySize;
}

bool fiDeviceReplay::CheckMontageCaches(FileDataStorage* fileList)
{
	if( m_HasEnumeratedMontages )
	{
		char directory[REPLAYPATHLENGTH];
		ReplayFileManager::getVideoProjectDirectory(directory);
		PopulateFileListWithCache(fileList, directory, m_CachedMontages);
		return true;
	}

	return false;
}

bool fiDeviceReplay::CheckClipCaches(FileDataStorage* fileList)
{
	if( m_HasEnumeratedClips )
	{
		char directory[REPLAYPATHLENGTH];
		ReplayFileManager::getClipDirectory(directory);
		PopulateFileListWithCache(fileList, directory, m_CachedClips);

		return true;
	}

	return false;
}

template<class _Type, int Count>
void fiDeviceReplay::PopulateFileListWithCache(FileDataStorage* fileList, const char* path, const atFixedArray<_Type, Count>& cache)
{
	u32 uCorruptCount = 1;

	for(int i = 0; i < cache.GetCount(); i++)
	{
		const CacheFileData& cachedData = cache[i];

		if( fileList != NULL )
		{
			CFileData fileData;
		
#if SEE_ALL_USERS_FILES
			(void)path;
			atString name = atString(ASSET.FileName(cachedData.GetFilePath()));
#else
			atString name = atString(cachedData.GetFilePath());
			name.Replace(path, "");
#endif

			fileData.setFilename( name.c_str() );
			DisplayName displayName = fileData.getFilename();
			fwTextUtil::TruncateFileExtension( displayName );

			fileData.setUserId(cachedData.GetUserId());

			if( cachedData.GetIsCorrupt() )
			{
				char buffer[REPLAYPATHLENGTH];
				sprintf_s(buffer, REPLAYPATHLENGTH, "%s%04d", TheText.Get("VEUI_CORRUPT_CLIP"), uCorruptCount);
				fileData.setDisplayName( buffer );
				uCorruptCount++;
			}
			else
			{
				fileData.setDisplayName( displayName.getBuffer() );
			}

			fileData.setExtraDataU32(cachedData.GetDuration());
			fileData.setSize(cachedData.GetSize());
			fileData.setLastWriteTime(cachedData.GetLastWriteTime());
			fileList->PushAndGrow( fileData );

			if( fileList->GetCount() >= cache.max_size() )
			{
				replayDisplayf("We've reached the max amount of items currently allowed - %u", cache.max_size());

				break;
			}
		}
	}
}

eReplayFileErrorCode fiDeviceReplay::Enumerate(const ReplayFileInfo &info, const char* filePathParam)
{
	const char* filePath = (filePathParam) ? filePathParam : info.m_FilePath.c_str();
	const char* filter = info.m_Filter.c_str();
	FileDataStorage* fileList = info.m_FileDataStorage;

	replayDisplayf("*** ENUMERATE START *** ");

	bool isClipEnumeration = false;
	bool isMontageEnumeration = false;
	if( strlen(filter) > 0 )
	{
		if( strstr(REPLAY_CLIP_FILE_EXT, filter ) != NULL )
		{
			if( !filePathParam && CheckClipCaches(fileList) )
			{
				return REPLAY_ERROR_SUCCESS;
			}

			isClipEnumeration = true;
			if (!filePathParam)
				m_CachedClips.clear();
		}
		else if( strstr(REPLAY_MONTAGE_FILE_EXT, filter ) != NULL )
		{
			if( !filePathParam && CheckMontageCaches(fileList) )
			{
				return REPLAY_ERROR_SUCCESS;
			}

			isMontageEnumeration = true;
			if (!filePathParam)
				m_CachedMontages.clear();
		}
	}

	m_IsEnumerating = true;

	// needed, since this is in fiDeviceReplay ???
	const fiDevice* device = fiDevice::GetDevice( filePath );

	// Enumerate directory for files
	fiFindData findData;
	DisplayName displayName;

	fiHandle hFileHandle = fiHandleInvalid;
	
	bool validDataFound = true;

#if RSG_ORBIS
	// for building the full file path to get size later
	char currentPath[RAGE_MAX_PATH];
#endif

	u64 const c_userId = GetUserIdFromPath( filePath );
	replayDisplayf("Enumerate folder: %s", filePath);

	for( hFileHandle = device->FindFileBegin( filePath, findData );
		fiIsValidHandle( hFileHandle ) && validDataFound; 
		validDataFound = device->FindFileNext( hFileHandle, findData ) )
	{

		if( info.m_CancelOp == true )
		{
			break;
		}

		s32 nameLength = static_cast<s32>(strlen(findData.m_Name));
		if((findData.m_Attributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)
		{
#if SEE_ALL_USERS_FILES
			// make sure directory is a proper user directory and not just '.' or '..'
			if (nameLength > REPLAYDIRMINLENGTH)
			{
				replayDisplayf("ENUMERATE CLIPS - directory - %s", findData.m_Name);
				// should enumerate this directory if requested
				char childPath[REPLAYPATHLENGTH] = { 0 };
				formatf_n(childPath, REPLAYPATHLENGTH, "%s%s/", filePath, findData.m_Name);
				Enumerate(info, childPath);
			}
#endif
			continue;
		}

		// If the filename+path is greater than REPLAYPATHLENGTH, then discard (since we can't cache it & thus can't load the file later using the name)
		if( nameLength > (REPLAYPATHLENGTH - 1) )		// -1 for the zero terminator that will be required.
			continue;

#if SEE_ALL_USERS_FILES
#if DO_CUSTOM_WORKING_DIR
		if( !PARAM_replayFilesWorkingDir.Get() )
#endif // DO_CUSTOM_WORKING_DIR
		// if base directory, don't list any files ... shouldn't be any in here, but might catch old dev stuff
		if (!filePathParam)
			continue;
#endif

		char name[REPLAYPATHLENGTH];
		sprintf_s(name, REPLAYPATHLENGTH, "%s", findData.m_Name);

		if( strstr(strlwr(name), filter ) != NULL )
		{
			CFileData fileData;
			fileData.setFilename(findData.m_Name);

			fileData.setUserId(c_userId);

			replayDisplayf("enumerate : %s %s", filePath, findData.m_Name);
			

#if RSG_ORBIS
#if DO_CUSTOM_WORKING_DIR
			if( !PARAM_replayFilesWorkingDir.Get() )
			{
#endif // DO_CUSTOM_WORKING_DIR
			// Orbis's sceKernelFstat doesn't cope with iterating through all the files in the directory like on previous platforms
			// best to get the information direct by using the path

			// accessing GetFileTime from the device is inconsistent to the method in FindNextFile too, so do it all here
			// also saves us having to call the stat info twice too if we used the device interface

			formatf(currentPath, sizeof(currentPath), "%s%s", filePath, findData.m_Name);
			char namebuf[RAGE_MAX_PATH];
			FixName(namebuf,sizeof(namebuf),currentPath);
			SceKernelStat stat;
			if (sceKernelStat(namebuf,&stat)==0)
			{
				fileData.setSize(stat.st_size);
				// Clips will overwrite this from the header.
				fileData.setLastWriteTime(stat.st_mtim.tv_sec);
			}
			else
			{
				fileData.setSize(0);
				// Clips will overwrite this from the header.
				fileData.setLastWriteTime(0);
			}
#if DO_CUSTOM_WORKING_DIR
			}
			else
			{
				fileData.setSize(findData.m_Size);
				// Clips will overwrite this from the header.
				fileData.setLastWriteTime(findData.m_LastWriteTime);
			}
#endif // DO_CUSTOM_WORKING_DIR
#else
			fileData.setSize(findData.m_Size);
			// Clips will overwrite this from the header.
			fileData.setLastWriteTime(findData.m_LastWriteTime);
#endif

			bool add = true;

			if( isClipEnumeration )
			{
				if( m_CachedClips.GetCount() == MAX_CLIPS )
				{
					//reached the clip limit here.
					break;
				}

				char path[REPLAYPATHLENGTH];
				sprintf_s(path, REPLAYPATHLENGTH, "%s%s", filePath, findData.m_Name);

				replayDisplayf("isClipEnumeration : %s", path);

				static ReplayFileInfo fileOpInfo;
				fileOpInfo.Reset();
				fileOpInfo.m_FilePath = path;
				fileOpInfo.m_FileOp = REPLAY_FILEOP_LOAD_HEADER;

				eReplayFileErrorCode eResult = LoadHeader(fileOpInfo);
				if( eResult == REPLAY_ERROR_SUCCESS || eResult == REPLAY_ERROR_FILE_CORRUPT )
				{
					UpdateClipHeaderVersion(fileOpInfo);

					const ReplayHeader& header = fileOpInfo.m_Header;

					// Get an idea of what to do with this version of the file
					eFileStoreVersionOperation	versionOp = GetVersionOperation(header.uFileVersion);

					fileData.setExtraDataU32(u32(header.fClipTime * 1000));

#if RSG_ORBIS
					fileData.setLastWriteTime(header.eventMarkers[0].GetUnixTime());
#else
					LONGLONG ll;
					FILETIME pft;

					ll = Int32x32To64(header.eventMarkers[0].GetUnixTime(), 10000000) + 116444736000000000;
					pft.dwLowDateTime = (DWORD)ll;
					pft.dwHighDateTime = ll >> 32;
					u64 time = ((u64)pft.dwHighDateTime<<32) | pft.dwLowDateTime;
					fileData.setLastWriteTime(time);
#endif

					replayDisplayf("isClipEnumeration loaded : %s %d", path, versionOp);


					if( versionOp == VERSION_OPERATION_OK || eResult == REPLAY_ERROR_FILE_CORRUPT )
					{
						if( !m_CachedClips.IsFull() )
						{
							ClipData& clipData = m_CachedClips.Append();
#if DO_CUSTOM_WORKING_DIR
							const char* workingDir = NULL;
 							if(PARAM_replayFilesWorkingDir.Get(workingDir))
							{
								sprintf_s(path, REPLAYPATHLENGTH, "%svideos\\clips\\%s", workingDir, findData.m_Name);
							}
#endif // DO_CUSTOM_WORKING_DIR

							clipData.Init(path, header, versionOp, header.bFavourite, header.wasModifiedContent, eResult == REPLAY_ERROR_FILE_CORRUPT);
							clipData.SetSize(fileData.getSize());
							clipData.SetLastWriteTime(fileData.getLastWriteTime());

							clipData.SetUserId(fileData.getUserId());
						}
					}
					else if( versionOp == VERSION_OPERATION_LEAVE_OUT_OF_CACHE )
					{
						add = false;
					}
					else if( versionOp == VERSION_OPERATION_DELETE)
					{
						DeleteOldClips(fileOpInfo);

						add = false;
					}
				}
				else
				{
					DeleteOldClips(fileOpInfo);
					add = false;
				}
			}

			if( isMontageEnumeration )
			{
				if( m_CachedMontages.GetCount() == MAX_MONTAGES )
				{
					//reached the montage limit here.
					break;
				}

				char path[REPLAYPATHLENGTH];
				sprintf_s(path, REPLAYPATHLENGTH, "%s%s", filePath, findData.m_Name);

				replayDisplayf("isMontageEnumeration : %s", path);
				
				//need some sort of validation check here.
				if( ValidateMontage(path) && !m_CachedMontages.IsFull() )
				{
					replayDisplayf("isMontageEnumeration valid : %s", path);

					ProjectData& project = m_CachedMontages.Append();
					project.Init(path);

					//Load to cache clips inside the montage
					CMontage* previewMontage = CMontage::Create();
					if( previewMontage )
					{
						project.SetSize(fileData.getSize());
						project.SetLastWriteTime(fileData.getLastWriteTime());

						project.SetUserId(fileData.getUserId());

						u64 const c_loadResult = previewMontage->Load( path, c_userId, true, NULL );
						bool const c_success = ( c_loadResult & CReplayEditorParameters::PROJECT_LOAD_RESULT_FAILED ) == 0;
						if( c_success )
						{
							for(int i = 0; i < previewMontage->GetClipCount(); ++i)
							{
								ClipUID const& uid = previewMontage->GetClip(i)->GetUID();

								//find the clip
								for( int clipIndex = 0; clipIndex < m_CachedClips.GetCount(); ++clipIndex)
								{
									const ClipData& clip = m_CachedClips[clipIndex];

									if( clip == uid )
									{
										//add it to the project
										project.AddClip(&clip);
										break;
									}
								}
							}

                            previewMontage->UpdateCachedDurationMs();
							u32 const duration = (u32)previewMontage->GetCachedDurationMs();
							fileData.setExtraDataU32( duration );
							project.SetDuration( duration );
						}

						CMontage::Destroy( previewMontage );
					}
				}
				else
				{
					add = false;
				}
			}

			if( add )
			{
				replayDisplayf("add : %s", filePath);

				displayName = findData.m_Name;
				fwTextUtil::TruncateFileExtension( displayName.getWritableBuffer() );
				fileData.setDisplayName( displayName.getBuffer() );

				replayDisplayf("add : %s %s", fileData.getFilename(), fileData.getDisplayName());

				if( fileList != NULL )
				{
					fileList->PushAndGrow( fileData );

					if( fileList->GetCount() >= MAX_CLIPS )
					{
						replayAssertf(false, "We've reached the max amount of clips currently allowed - %u", MAX_CLIPS);

						break;
					}
				}
			}
		}
	}

	if( fiIsValidHandle( hFileHandle ) )
	{
		device->FindFileEnd( hFileHandle );
	}

	if( !info.m_CancelOp )
	{
		if( isClipEnumeration )
		{
			m_HasEnumeratedClips = true;
		}

		if( isMontageEnumeration )
		{
			m_HasEnumeratedMontages = true;
		}

		BANK_ONLY(sm_UpdateClipWidget = true);
		BANK_ONLY(sm_UpdateFavouritesWidget = true);
	}

	m_IsEnumerating = false;

	replayDisplayf("*** ENUMERATE END *** ");

	return REPLAY_ERROR_SUCCESS;
}

void fiDeviceReplay::UpdateClipHeaderVersion(ReplayFileInfo& fileOpInfo)
{
	u32 version = fileOpInfo.m_Header.uHeaderVersion;
	if( version < CURRENTHEADERVERSION )
	{
		bool update = false;
		u32 versionCount = HEADERVERSION_V1;
		while(versionCount < CURRENTHEADERVERSION)
		{
			if( version != versionCount )
			{
				++versionCount;
				continue;
			}

			switch(versionCount)
			{
			case HEADERVERSION_V1:
			case HEADERVERSION_V2:
				{
					update = true;
					//updates the header version
					version = HEADERVERSION_V3;
					break;
				}
			case HEADERVERSION_V3:
			case HEADERVERSION_V4:
				//header version 3 doesn't do anything to update the clips
				break;
			default:
				{
					//header version only needed to be updated to allow older versions (HEADERVERSION_V2 and previous) to exclude the header from the
					//CRC.  Future version increments (HEADERVERSION_V3 and greater) will be fine to ignore this option. i.e. update = false
					replayAssertf(false, "UpdateClipHeaderVersion - Version update not implemented!");
					update = false;
				}
			}

			++versionCount;
		}

		if( update )
		{
			fileOpInfo.m_Header.uHeaderVersion = version;
			//saves out the clip with the updated header + recomputes CRC
			ReplayFileManager::SetSaved(fileOpInfo, true);
		}
	}
}

void fiDeviceReplay::DeleteOldClips(ReplayFileInfo& fileOpInfo)
{
	(void)fileOpInfo;
#if REPLAY_DEBUG_DELETE_FILES
	DeleteFile(fileOpInfo);

	// NOTE: We also need to delete the screenshots and the outputted XML files.
	atString path = fileOpInfo.m_FilePath;
	path.Replace(".clip", ".jpg");
	Delete(path.c_str());

	path.Replace(".jpg", ".xml");
	Delete(path.c_str());

	SetShouldUpdateDirectorySize();
#endif // REPLAY_DEBUG_DELETE_FILES
}

eReplayFileErrorCode fiDeviceReplay::EnumerateClips(ReplayFileInfo &info)
{
	return Enumerate(info);
}

eReplayFileErrorCode fiDeviceReplay::EnumerateMontages(ReplayFileInfo &info)
{
	return Enumerate(info);
}

eReplayFileErrorCode fiDeviceReplay::DeleteFile(ReplayFileInfo& info)
{
	const char* path = info.m_FilePath;

	// lets us delete if full path passed in (for videos)
	const fiDevice* pDevice = fiDevice::GetDevice(path, false);
	replayAssertf(pDevice, "fiDeviceReplay::DeleteFile - path didn't return a valid device - NULL");
	if (!pDevice || !pDevice->Delete(path))
	{
		return REPLAY_ERROR_FAILURE;
	}

	SetShouldUpdateDirectorySize();
	
	RemoveCachedClip(path);

	u64 const c_userId = GetUserIdFromFileInfo( info );
	RemoveCachedProject(path, c_userId);

	return REPLAY_ERROR_SUCCESS;
}

eReplayFileErrorCode fiDeviceReplay::UpdateFavourites(ReplayFileInfo& /*info*/)
{
	for(int i = 0; i < m_CachedClips.GetCount(); ++i)
	{
		const ClipData& data = m_CachedClips[i];

		//only re-save if the version was deemed okay.
		if( data.IsFavouriteModified() )
		{
			static ReplayFileInfo fileOpInfo;
			fileOpInfo.m_FilePath = data.GetFilePath();
			if( LoadHeader(fileOpInfo) == REPLAY_ERROR_SUCCESS )
			{
				ReplayHeader& header = fileOpInfo.m_Header;

				header.bFavourite = data.GetFavourite();

				bool doCrc = true;
				if( fileOpInfo.m_Header.uHeaderVersion >= HEADERVERSION_V3 )
					doCrc = false;

				ReplayFileManager::SetSaved(fileOpInfo, doCrc);
			}
			else
			{
				replayDisplayf("Failed to marker clip %s as a favourite", data.GetFilePath());
			}
		}
	}

	return REPLAY_ERROR_SUCCESS;
}

eReplayFileErrorCode fiDeviceReplay::ProcessMultiDelete(ReplayFileInfo& info)
{
	bool fileBeenDeleted = false;
	eReplayFileErrorCode finalErrorCondition = REPLAY_ERROR_SUCCESS;

	if (strcmp(info.m_Filter, REPLAY_CLIP_FILE_EXT) == 0)
	{
		for( int i = m_CachedClips.GetCount()-1; i >= 0; --i)
		{
			const ClipData& data = m_CachedClips[i];
			
			if (data.GetMarkForDelete())
			{
				const char* path = data.GetFilePath();

				// delete clip
				if( !Delete(path) )
				{
					finalErrorCondition = REPLAY_ERROR_FAILURE;
				}

				fileBeenDeleted = true;

				{
					// delete thumb
					atString thumbpath(path);
					thumbpath.Replace(".clip", ".jpg");
					if( !Delete(thumbpath.c_str()) )
					{
						finalErrorCondition = REPLAY_ERROR_FAILURE;
					}
				}
				
#if !__FINAL // ... shouldn't check against DO_REPLAY_OUTPUT_XML ... might not be on while deleting
				{
					// delete log
					atString debugpath(path);
					debugpath.Replace(".clip", ".xml");
					if ( ASSET.Exists(debugpath.c_str(), ".xml") )
					{
						if( !Delete(debugpath.c_str()) )
						{
							finalErrorCondition = REPLAY_ERROR_FAILURE;
						}
					}
				}
#endif

				DeleteClipFromAllProjectCaches(m_CachedClips[i]);
				m_CachedClips.DeleteFast(i);
			}
		}
	}
	else if ( strcmp(info.m_Filter, REPLAY_MONTAGE_FILE_EXT) == 0)
	{
		for( int i = m_CachedMontages.GetCount()-1; i >= 0; --i)
		{
			const ProjectData& data = m_CachedMontages[i];

			if (data.GetMarkForDelete())
			{
				const char* path = data.GetFilePath();

				if( !Delete(path) )
				{
					finalErrorCondition = REPLAY_ERROR_FAILURE;
				}

				fileBeenDeleted = true;

				m_CachedMontages.DeleteFast(i);
			}
		}
	}
	else
	{
		for( int i = m_FilesToDelete.GetCount()-1; i >= 0; --i)
		{
			const CacheFileData& data = m_FilesToDelete[i];

			if (data.GetMarkForDelete())
			{
				// from filedataprovider.cpp delete, which videos was originally using
				const char* path = data.GetFilePath();

#if RSG_ORBIS
				if (!CContentDelete::DoDeleteVideo(path))
				{
					finalErrorCondition = REPLAY_ERROR_FAILURE;
				}
#else
				// lets us delete from full paths (for videos)
				const fiDevice* pDevice = fiDevice::GetDevice(path, false);
				replayAssertf(pDevice, "fiDeviceReplay::ProcessMultiDelete - path didn't return a valid device - NULL");

				if (!pDevice || !pDevice->Delete(path))
				{
					finalErrorCondition = REPLAY_ERROR_FAILURE;
				}
#endif

				fileBeenDeleted = true;

				m_FilesToDelete.DeleteFast(i);
			}
		}

		// clear remaining list, as some can be unmarked for delete
		// saves having to remove from list each time it's unmarked
		m_FilesToDelete.clear();
	}

	if (fileBeenDeleted)
	{
		SetShouldUpdateDirectorySize();
	}

	return finalErrorCondition;
}


void fiDeviceReplay::RemoveCachedClip(const char* path)
{
	//check if this is a .clip file
	if( strstr(path, REPLAY_CLIP_FILE_EXT) != 0 )
	{
		for( int i = m_CachedClips.GetCount()-1; i >= 0; --i)
		{
			if( m_CachedClips[i].IsEqual( path ) )
			{
				DeleteClipFromAllProjectCaches(m_CachedClips[i]);
				m_CachedClips.DeleteFast(i);
				break;
			}
		}
	}

	BANK_ONLY(sm_UpdateClipWidget = true);
}

bool CheckUserID(u64 id1, u64 id2)
{
#if DO_CUSTOM_WORKING_DIR
	if(PARAM_replayFilesWorkingDir.Get())
		return true;
#endif // DO_CUSTOM_WORKING_DIR

	return id1 == id2;
}

void fiDeviceReplay::RemoveCachedProject( const char* path, u64 const userId )
{
	//check if this is a .vid file
	if( strstr(path, REPLAY_MONTAGE_FILE_EXT) != 0 )
	{
		for( int i = m_CachedMontages.GetCount()-1; i >= 0; --i)
		{
			const ProjectData& data = m_CachedMontages[i];
			if( data == path && CheckUserID(data.GetUserId(), userId) )
			{
				m_CachedMontages.DeleteFast(i);
				break;
			}
		}
	}	
}

bool fiDeviceReplay::IsClipFileValid( u64 const userId, const char* path, bool checkDisk)
{
	if( !checkDisk )
	{
		for( int i = 0; i < m_CachedClips.GetCount(); ++i)
		{
			const ClipData& data = m_CachedClips[i];
			if( data.IsEqual( path ) && CheckUserID(data.GetUserId(), userId) )
			{
				return true;
			}
		}
	}
	else
	{
		return IsFileValid( path );
	}

	return false;
}

bool fiDeviceReplay::IsProjectFileValid( u64 const userId, const char* path, bool checkDisk)
{
	return checkDisk ? IsFileValid( path ) : IsProjectInProjectCache( path, userId );
}

bool fiDeviceReplay::DoesClipHaveModdedContent(const char* path)
{
#if __BANK
	if (PARAM_fakeModdedContent.Get())
	{
		return true;
	}
#endif

	for( int i = 0; i < m_CachedClips.GetCount(); ++i)
	{
		if( m_CachedClips[i].IsEqual(path) )
		{
			if (m_CachedClips[i].GetHasModdedContent())
				return true;
			
			return false;
		}
	}

	return false;
}

bool fiDeviceReplay::IsFileValid(const char* path)
{
	bool isValid = false;

#if RSG_PC
	FileHandle fileHandle = CFileMgr::OpenFile(path);

	isValid = CFileMgr::IsValidFileHandle(fileHandle);
	if( isValid )
	{
		CFileMgr::CloseFile( fileHandle );
	}

#else
	isValid = path != NULL;
#endif

	return isValid;
}

const ClipUID* fiDeviceReplay::GetClipUID(u64 const userId, const char* path) const
{
	for( int i = 0; i < m_CachedClips.GetCount(); ++i)
	{
		const ClipData& clip = m_CachedClips[i];

		if( clip.IsEqual( path ) && CheckUserID(clip.GetUserId(), userId) )
		{
			return &clip.GetUID();
		}
	}

	return NULL;
}

float fiDeviceReplay::GetClipLength(u64 const userId, const char* path) 
{
	for( int i = 0; i < m_CachedClips.GetCount(); ++i)
	{
		const ClipData& clip = m_CachedClips[i];

		if( clip.IsEqual( path ) && CheckUserID(clip.GetUserId(), userId) )
		{
			return clip.GetLengthAsFloat();
		}
	}

	return 0.0f;
}


u32 fiDeviceReplay::GetClipSequenceHash(u64 const userId, const char* path, bool prev)
{
	for( int i = 0; i < m_CachedClips.GetCount(); ++i)
	{
		const ClipData& clip = m_CachedClips[i];

		if( clip.IsEqual( path ) && CheckUserID(clip.GetUserId(), userId) )
		{
			return prev ? clip.GetPreviousSequenceHash() : clip.GetNextSequenceHash();
		}
	}

	return 0;
}

void fiDeviceReplay::AddClipToProjectCache(const char* projectPath, u64 const userId, ClipUID const& uid )
{
	for( int projIndex = 0; projIndex < m_CachedMontages.GetCount(); ++projIndex)
	{
		ProjectData& project = m_CachedMontages[projIndex];
		if( project == projectPath && CheckUserID(project.GetUserId(), userId) )
		{
			for( int clipIndex = 0; clipIndex < m_CachedClips.GetCount(); ++clipIndex)
			{
				const ClipData& clip = m_CachedClips[clipIndex];

				if( clip == uid )
				{
					project.AddClip(&clip);
					break;
				}
			}
			break;
		}
	}
}

void fiDeviceReplay::DeleteClipFromProjectCache(const char* path, u64 const userId, ClipUID const& uid )
{
	for( int projIndex = 0; projIndex < m_CachedMontages.GetCount(); ++projIndex)
	{
		ProjectData& project = m_CachedMontages[projIndex];
		if( project == path && CheckUserID(project.GetUserId(), userId) )
		{
			for( int clipIndex = 0; clipIndex < m_CachedClips.GetCount(); ++clipIndex)
			{
				const ClipData& clip = m_CachedClips[clipIndex];

				if( clip == uid )
				{
					project.RemoveClip(clip);
					break;
				}
			}
			break;
		}
	}
}

void fiDeviceReplay::DeleteClipFromAllProjectCaches(const ClipData& clip)
{
	for( int i = 0; i < m_CachedMontages.GetCount(); ++i)
	{
		m_CachedMontages[i].RemoveClip(clip, true);
	}
}

void fiDeviceReplay::AddProjectToCache(const char* path, u64 const userId, u64 const size, u32 const duration )
{
	if( !IsProjectInProjectCache( path, userId ) &&
		replayVerifyf( !m_CachedMontages.IsFull(), "Attempting to add %s to project cache, but cache is full!", path ) )
	{
		ProjectData& project = m_CachedMontages.Append();
		project.Init(path);
		project.SetSize(size);
		project.SetLastWriteTime(GetFileTime(path));
		project.SetUserId(userId);
        project.SetDuration( duration );
	}
}

bool fiDeviceReplay::IsProjectInProjectCache( const char* path, u64 userId ) const
{
	bool result = false;

	int const c_montageCount = m_CachedMontages.GetCount();
	for( int i = 0; result == false && i < c_montageCount; ++i)
	{
		ProjectData const& project =  m_CachedMontages[i];
		result = ( project == path && CheckUserID(project.GetUserId(), userId) );
	}

	return result;
}

bool fiDeviceReplay::IsClipInAProject(const ClipUID& clipUID) const
{
	for( int i = 0; i < m_CachedMontages.GetCount(); ++i)
	{
		if( m_CachedMontages[i].FindFirstIndexOfClip( clipUID ) != -1 )
		{
			return true;
		}
	}

	return false;
}

ClipData* fiDeviceReplay::AppendClipToCache(const ClipUID& clipUID)
{
	for( int i = 0; i < m_CachedClips.GetCount(); ++i)
	{
		const ClipData& clipData = m_CachedClips[i];

		if( clipData == clipUID )
		{
			return NULL;
		}
	}

	if( replayVerifyf( !m_CachedClips.IsFull(), "Attempting to add clip to cache, but cache is full!" ) )
	{
		BANK_ONLY(sm_UpdateClipWidget = true);
		return &m_CachedClips.Append();
	}

	return NULL;
}

bool fiDeviceReplay::IsMarkedAsFavourite(const char* path)
{
	for( int i = 0; i < m_CachedClips.GetCount(); ++i)
	{
		const ClipData& clipData = m_CachedClips[i];

		if( clipData.IsEqual( path ) )
		{
			return clipData.GetFavourite();
		}
	}

	return false;
}

bool fiDeviceReplay::IsMarkedAsDelete(const char* path)
{
	if (fwTextUtil::HasFileExtension(path, REPLAY_CLIP_FILE_EXT))
	{
		for( int i = 0; i < m_CachedClips.GetCount(); ++i)
		{
			const ClipData& clipData = m_CachedClips[i];

			if( clipData.IsEqual( path ) )
			{
				return clipData.GetMarkForDelete();
			}
		}
	}
	else if ( fwTextUtil::HasFileExtension(path, REPLAY_MONTAGE_FILE_EXT) )
	{
		for( int i = 0; i < m_CachedMontages.GetCount(); ++i)
		{
			const ProjectData& projData = m_CachedMontages[i];

			if( projData == path )
			{
				return projData.GetMarkForDelete();
			}
		}
	}
	else
	{
		for( int i = 0; i < m_FilesToDelete.GetCount(); ++i)
		{
			const CacheFileData& fileData = m_FilesToDelete[i];
			
			if( fileData == path )
			{
				return fileData.GetMarkForDelete();
			}
		}
	}

	return false;
}

void fiDeviceReplay::MarkFile(const char* path, eMarkedUpType type)
{
	bool const isClip = fwTextUtil::HasFileExtension(path, REPLAY_CLIP_FILE_EXT);
	
	// early out if trying to favourite something other than a clip
	if ( !isClip && (type == UNMARK_AS_FAVOURITE || type == MARK_AS_FAVOURITE))
	{
		return;
	}

	CacheFileData *data = NULL;
	if ( isClip )
	{
		for( int i = 0; i < m_CachedClips.GetCount(); ++i)
		{
			CacheFileData& clipData = m_CachedClips[i];

			if( clipData == path )
			{
				data = &clipData;
				break;
			}
		}
	}

	// if not a clip, then check for projects (and soon videos)
	else if ( fwTextUtil::HasFileExtension(path, REPLAY_MONTAGE_FILE_EXT) )
	{
		for( int i = 0; i < m_CachedMontages.GetCount(); ++i)
		{
			ProjectData& projData = m_CachedMontages[i];

			if( projData == path )
			{
				data = &projData;
				break;
			}
		}
	}
	else
	{
		// is already in delete list?
		for( int i = 0; i < m_FilesToDelete.GetCount(); ++i)
		{
			CacheFileData& fileData = m_FilesToDelete[i];

			if( fileData == path )
			{
				data = &fileData;
				break;
			}
		}

		// only add to list. we can clear it later on the actual delete operation
		if (data == NULL && type == MARK_AS_DELETE)
		{
			CacheFileData fileData;
			fileData.Init(path);
			m_FilesToDelete.PushAndGrow(fileData);
			data = &m_FilesToDelete[m_FilesToDelete.GetCount()-1];
		}
	}
	
	if( data == NULL )
	{
		return;
	}

	switch(type)
	{
	case MARK_AS_FAVOURITE:
		{
			// must be a clip if we're marking as favourite
			ClipData* clipData = static_cast<ClipData*>(data);
			
			//we don't want up modify if we're already a favorite
			if( clipData->GetFavourite() )
			{
				return;
			}

			clipData->SetFavourite(true);
		}
		break;

	case UNMARK_AS_FAVOURITE:
		{
			// must be a clip if we're marking as favourite
			ClipData* clipData = static_cast<ClipData*>(data);

			//we don't want up modify if we're not a favorite
			if( !clipData->GetFavourite() )
			{
				return;
			}

			clipData->SetFavourite(false);
		}
		break;
	case MARK_AS_DELETE:
		{
			//we don't want up modify if already marked for delete
			if( data->GetMarkForDelete() )
			{
				return;
			}

			data->SetMarkForDelete(true);
		}
		break;

	case UNMARK_AS_DELETE:
		{
			//we don't want up modify if not already marked for delete
			if( !data->GetMarkForDelete())
			{
				return;
			}

			data->SetMarkForDelete(false);
		}
		break;
	default:
		replayAssertf(false, "Operator not implemented");
		break;
	}

#if __BANK
	if( type == UNMARK_AS_FAVOURITE || type == MARK_AS_FAVOURITE )
	{
		sm_UpdateFavouritesWidget = true;
	}
#endif
}

u32 fiDeviceReplay::GetCountOfFilesToDelete(const char* filter) const
{
	u32 count = 0;
	if( strcmp(REPLAY_CLIP_FILE_EXT, filter ) == 0 )
	{
		for( int i = 0; i < m_CachedClips.GetCount(); ++i)
		{
			const ClipData& fileData = m_CachedClips[i];
			if (fileData.GetMarkForDelete())
			{
				++count;
			}
		}
	}
	else if( strcmp(REPLAY_MONTAGE_FILE_EXT, filter ) == 0  )
	{
		for( int i = 0; i < m_CachedMontages.GetCount(); ++i)
		{
			const ProjectData& fileData = m_CachedMontages[i];
			if (fileData.GetMarkForDelete())
			{
				++count;
			}
		}
	}
	else
	{
		for( int i = 0; i < m_FilesToDelete.GetCount(); ++i)
		{
			const CacheFileData& fileData = m_FilesToDelete[i];
			if (fileData.GetMarkForDelete())
			{
				++count;
			}
		}
	}

	return count;
}

u32 fiDeviceReplay::GetClipRestrictions(const ClipUID& clipUID)
{
	for( int i = 0; i < m_CachedClips.GetCount(); ++i)
	{
		const ClipData& clipData = m_CachedClips[i];

		if( clipData == clipUID )
		{
			return clipData.GetFrameFlags();
		}
	}

	return false;
}

u64 fiDeviceReplay::GetUserIdFromFileInfo( ReplayFileInfo const& fileInfo )
{
    char pathBuffer[RAGE_MAX_PATH];
    safecpy( pathBuffer, fileInfo.m_FilePath.c_str() );

    fwTextUtil::TruncateFileName( pathBuffer );

	return GetUserIdFromPath( pathBuffer );
}

u64 fiDeviceReplay::GetUserIdFromPath( char const * const filePath )
{
	u64 result = 0;

#if RSG_ORBIS || RSG_DURANGO
	char pathBuffer[RAGE_MAX_PATH];
    formatf(pathBuffer, strlen(filePath), "%s", filePath);

	char* lastSlash = strrchr(pathBuffer, '\\');
    lastSlash = lastSlash == NULL ? strrchr( pathBuffer, '/' ) : lastSlash;

	if( lastSlash && lastSlash[1] >= '0' && lastSlash[1] <= '9' )
	{
#if RSG_DURANGO
		result = _strtoui64(&lastSlash[1], NULL, 10);
#else
		result = strtoull(&lastSlash[1], NULL, 10);
#endif
	}
#else
	(void)filePath;
#endif

	return result;
}

#endif // GTA_REPLAY
