#ifndef FILE_STORE_ORBIS_H
#define FILE_STORE_ORBIS_H 

#include "../ReplaySettings.h"

#if GTA_REPLAY

// rage
#include "file/cachepartition.h"
#include "file/savegame.h"

// game
#include "SaveLoad/savegame_queued_operations.h"
#include "system/SettingsManager.h"
#include "device_replay.h"

#if RSG_ORBIS

class FileStoreOrbis : public fiDeviceReplay
{
public:
	FileStoreOrbis();

	virtual u64 GetMaxMemoryAvailable() const;
	u64	GetMaxMemoryAvailableForClips() const;

	virtual u64 GetFreeSpaceAvailable(bool force = false);

	virtual u64 GetFreeSpaceAvailableForClipsAndMontages();
	virtual u64 GetFreeSpaceAvailableForClips();
	virtual u64 GetFreeSpaceAvailableForMontages();
	virtual u64 GetFreeSpaceAvailableForVideos() { return 0; } // we're not checking videos
	virtual bool CanFullfillSpaceRequirement( eReplayMemoryLimit size ) { (void)size; return true; }

	u64 GetFreeSpaceAvailableForExtras();
	bool IsEnoughSpaceForAutoSave();

	virtual const char* GetDirectory() const { return "/download0/";}

	u64 GetFileTime(const char *filename) const;

	void RefreshNonClipDirectorySizes();

private:
	u64 GetFreeSpaceAvailableForAutoSave();

	u64 m_maxMemory;
	u64 m_freeMemory;

	u64 m_autoSaveSize;
	u64 m_extrasSize;
};

#endif

#endif

#endif
