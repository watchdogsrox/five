#ifndef FILE_STORE_DURANGO_H
#define FILE_STORE_DURANGO_H

#include "../ReplaySettings.h"

#if GTA_REPLAY

// rage
#include "file/cachepartition.h"

// game
#include "device_replay.h"
#include "system/SettingsManager.h"

#if RSG_DURANGO

class FileStoreDurango : public fiDeviceReplay
{
public:
	FileStoreDurango();

	virtual u64 GetMaxMemoryAvailable() const;

	virtual u64 GetMaxMemoryAvailableForVideo() const 
	{
		u64 maxMem = 4 * 1024 * 1024;
		return maxMem * 1024;
	}

	virtual const char* GetDirectory() const { return GetPersistentStorageLocation(); }

	// PURPOSE:	Retrieves the persistent storage location for the Xbox One.
	// NOTES:	This must be called exactly once.
	static const char* GetPersistentStorageLocation();

	virtual bool CanFullfillSpaceRequirement( eReplayMemoryLimit size ) { (void)size; return true; }

private:
#if RSG_BANK
	// PURPOSE: Deletes a directory and all of its contents.
	// PARAMS:	path - the path to the directory to delete.
	// NOTES:	This function is recursive. It exists to remove the persistent storage location if it already
	//			exists before we mount it. This can happen if we used the location before it was mounted (which we used to).
	static bool DeleteDirectory(const char* path);
#endif // RSG_BANK
};

#endif

#endif

#endif