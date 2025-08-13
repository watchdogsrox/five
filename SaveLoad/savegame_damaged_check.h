

#ifndef SAVEGAME_DAMAGED_CHECK_H
#define SAVEGAME_DAMAGED_CHECK_H

#include "SaveLoad/savegame_defines.h"

class CSavegameDamagedCheck
{
	enum eProgress
	{
		DAMAGED_CHECK_PROGRESS_BEGIN,
		DAMAGED_CHECK_PROGRESS_UPDATE
	};

public:
// Public functions
	static void ResetProgressState();

	static MemoryCardError CheckIfSlotIsDamaged(const char *pSavegameFilename, bool *pbDamagedFlag);

private:
	static eProgress ms_DamagedSlotProgressState;
};

#endif	//	SAVEGAME_DAMAGED_CHECK_H
