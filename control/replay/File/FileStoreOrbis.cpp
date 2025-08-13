#include "FileStoreOrbis.h"

#if GTA_REPLAY

#include "control/replay/File/ReplayFileManager.h"
#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/savegame_users_and_devices.h"

#if RSG_ORBIS

PARAM(fillDownload0, "[REPLAY] Pretend PS4 Filestore on download0 is full");

#define AUTOSAVE_PATH				"replay:/saves/"

static const u64 PS4_AUTOSAVE_BUFFER = 3 * 1024 * 1024;						//	Assume three autosaves per user (GTA5, Clifford, Norman). 1MB per autosave should be more than enough.

static const u64 PS4_AUTOSAVE_FILE_LIMIT = 17 * PS4_AUTOSAVE_BUFFER;		//	Handle a maximum of 16 users, but add 1 to compensate for the fact that we're not checking whether we're overwriting an existing file.
																			//	This is to avoid a possible problem - if all 16 users had saved three large autosaves then the current user should still be allowed to overwrite an existing autosave.
																			//	We don't want IsEnoughSpaceForAutoSave() to incorrectly return false in this situation.
static const u64 PS4_EXTRAS_FILE_LIMIT = 2 * 1024 * 1024;

FileStoreOrbis::FileStoreOrbis() 
	: fiDeviceReplay()
	, m_maxMemory(0)
	, m_freeMemory(0)
	, m_autoSaveSize(0)
	, m_extrasSize(0)
{

}

u64	FileStoreOrbis::GetMaxMemoryAvailable() const
{
	replayAssertf(m_maxMemory, "FileStoreOrbis::GetMaxMemoryAvailable - Max Memory is 0");
	return m_maxMemory;
}

u64	FileStoreOrbis::GetMaxMemoryAvailableForClips() const
{ 
	u64 maxAvailable = fiDeviceReplay::GetMaxMemoryAvailableForClips();

	if(PS4_EXTRAS_FILE_LIMIT + PS4_AUTOSAVE_FILE_LIMIT >= maxAvailable)
	{
		return 0;
	}

	return maxAvailable - PS4_EXTRAS_FILE_LIMIT - PS4_AUTOSAVE_FILE_LIMIT;
}

u64 FileStoreOrbis::GetFreeSpaceAvailable(bool force)
{
	(void)force;

	if(PARAM_fillDownload0.Get())
	{
		m_freeMemory = 0;
		m_montageSize = 0;
		m_autoSaveSize = PS4_AUTOSAVE_FILE_LIMIT;
		m_extrasSize = PS4_EXTRAS_FILE_LIMIT;
		m_maxMemory = m_montageSize + m_autoSaveSize + m_extrasSize;
		m_ShouldUpdateDirectorySize = false;
		m_ShouldUpdateClipsSize = false;
		return m_freeMemory;
	}

	// on orbis, we just update everything since the tests we do are cheaper
	if (m_ShouldUpdateClipsSize)
	{
		m_ShouldUpdateDirectorySize = true;
		m_ShouldUpdateClipsSize = false;
	}

	if( !m_ShouldUpdateDirectorySize && m_freeMemory != 0 )
	{
		return m_freeMemory;
	}

	u64 availableSpaceKb;
	SceAppContentMountPoint	mountPoint;
	strncpy(mountPoint.data, "/download0", SCE_APP_CONTENT_MOUNTPOINT_DATA_MAXSIZE);
	int ret = sceAppContentDownloadDataGetAvailableSpaceKb(&mountPoint, &availableSpaceKb);
	if(ret != SCE_OK)
	{
		replayDisplayf("GetFreeSpaceAvaliable - sceAppContentDownloadDataGetAvailableSpaceKb FAIL 0x%08x - you may need to update sce_sys param.sfo", ret);
		return 0;
	}

	replayDisplayf("GetFreeSpaceAvaliable - sceAppContentDownloadDataGetAvailableSpaceKb - %lu kb", availableSpaceKb);

	// convert to bytes as rest of code uses that
	m_freeMemory = availableSpaceKb << 10;
	replayDisplayf("GetFreeSpaceAvaliable - m_freeMemory - %lu bytes", m_freeMemory);

	// max memory shouldn't change. this should stop GetCurrentSize getting called so often, mid-game
	if (m_maxMemory == 0)
	{
		// should do a refresh of max memory here
		// awkward, but to do it elsewhere would require a big rewrite
		// just do clip folder, as montage file limit is used here, and videos are currently stored elsewhere anyway

		u64 totalSize = GetDirectorySize(REPLAY_ROOT_PATH);
		m_clipSize = GetDirectorySize(REPLAY_CLIPS_PATH);
		m_montageSize = GetDirectorySize(REPLAY_MONTAGE_PATH);
		m_autoSaveSize = GetDirectorySize(AUTOSAVE_PATH);
		m_dataSize = GetDirectorySize(REPLAY_DATA_PATH);
		m_extrasSize = totalSize - m_clipSize - m_montageSize - m_autoSaveSize - m_dataSize;
		// videos aren't stored here like other platforms

		m_maxMemory = totalSize + m_freeMemory;

		replayDisplayf("download0 - m_maxMemory --- %lu bytes", m_maxMemory);
		replayDisplayf("download0 - totalSize - %lu bytes", totalSize);
		replayDisplayf("download0 - clipsSize - %lu bytes", m_clipSize);
		replayDisplayf("download0 - montageSize - %lu bytes", m_montageSize);
		replayDisplayf("download0 - autoSaveSize - %lu bytes", m_autoSaveSize);
		replayDisplayf("download0 - dataSize - %lu bytes", m_dataSize);
		replayDisplayf("download0 - extrasSize - %lu bytes", m_extrasSize);
		replayDisplayf("download0 - m_freeMemory -- %lu bytes", m_freeMemory);
		replayDisplayf("download0 - PS4_EXTRAS_FILE_LIMIT - %lu bytes", PS4_EXTRAS_FILE_LIMIT);
		replayDisplayf("download0 - DATA_FILE_LIMIT - %lu bytes", DATA_FILE_LIMIT);
	}

	return m_freeMemory;
}

u64	FileStoreOrbis::GetFreeSpaceAvailableForClipsAndMontages()
{
	// find clip free space, by removing free space allocated to other areas
	u64 freeSpace = fiDeviceReplay::GetFreeSpaceAvailableForClipsAndMontages();

	//free space reserved for other file types
	u64 accountedFreeSpace = GetFreeSpaceAvailableForExtras() + GetFreeSpaceAvailableForAutoSave();

	//ensure this doesn't wrap when getting the clip space.
	if( freeSpace >= accountedFreeSpace )
	{
		freeSpace -= accountedFreeSpace;
	}
	else
	{
		freeSpace = 0;
	}
	
	return freeSpace;
}

u64 FileStoreOrbis::GetFreeSpaceAvailableForClips()
{
	u64 estFreeSpace = fiDeviceReplay::GetFreeSpaceAvailableForClips();

	u64 freeSpace = GetFreeSpaceAvailable();

	//return the minimum disk space as the actual free space could be less than our estimate.
	return MIN(estFreeSpace, freeSpace);
}

u64 FileStoreOrbis::GetFreeSpaceAvailableForMontages()
{
	u64 estFreeSpace = fiDeviceReplay::GetFreeSpaceAvailableForMontages();

	u64 freeSpace = GetFreeSpaceAvailable();

	//return the minimum disk space as the actual free space could be less than our estimate.
	return MIN(estFreeSpace, freeSpace);
}

u64 FileStoreOrbis::GetFreeSpaceAvailableForExtras()
{
	if (m_extrasSize >= PS4_EXTRAS_FILE_LIMIT)
	{
		return 0;
	}

	return PS4_EXTRAS_FILE_LIMIT - m_extrasSize;
}

u64 FileStoreOrbis::GetFreeSpaceAvailableForAutoSave()
{
	if (m_autoSaveSize >= PS4_AUTOSAVE_FILE_LIMIT)
	{
		return 0;
	}

	return PS4_AUTOSAVE_FILE_LIMIT - m_autoSaveSize;
}

bool FileStoreOrbis::IsEnoughSpaceForAutoSave()
{
	return (GetFreeSpaceAvailableForAutoSave() > PS4_AUTOSAVE_BUFFER);
}

void FileStoreOrbis::RefreshNonClipDirectorySizes()
{
	m_montageSize = GetDirectorySize(REPLAY_MONTAGE_PATH);
	m_autoSaveSize = GetDirectorySize(AUTOSAVE_PATH);
	m_dataSize = GetDirectorySize(REPLAY_DATA_PATH);
}

u64 FileStoreOrbis::GetFileTime(const char *filename) const
{
	char namebuf[RAGE_MAX_PATH];
	FixName(namebuf,sizeof(namebuf),filename);
	SceKernelStat stat;
	if (sceKernelStat(namebuf,&stat)==0)
	{
		return stat.st_mtim.tv_sec;
	}

	return 0;
}

#endif

#endif
