
#ifndef SAVEGAME_FREE_SPACE_CHECK_H
#define SAVEGAME_FREE_SPACE_CHECK_H

// Game headers
#include "SaveLoad/savegame_defines.h"


enum eTypeOfSpaceCheck
{
	FREE_SPACE_CHECK,
	EXTRA_SPACE_CHECK
};


class CSavegameFreeSpaceCheck
{
public:
//	Public functions
	static MemoryCardError CheckForFreeSpaceForCurrentFilename(bool bInitialise, u32 SizeOfNewFile, int &SpaceRequired);

	//	TotalFreeStorageDeviceSpace will only have a non-zero value after an EXTRA_SPACE_CHECK check has completed.
	static MemoryCardError CheckForFreeSpaceForNamedFile(eTypeOfSpaceCheck TypeOfSpaceCheck, bool bInitialise, const char *pFilename, u32 SizeOfNewFile, int &SpaceRequired, int &TotalFreeStorageDeviceSpace, int &SizeOfSystemFiles);

};

#endif	//	SAVEGAME_FREE_SPACE_CHECK_H

