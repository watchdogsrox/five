
#ifndef SAVEGAME_SLOW_PS3_SCAN_H
#define SAVEGAME_SLOW_PS3_SCAN_H

// Game headers
#include "SaveLoad/savegame_defines.h"

#if __PPU && __QUICK_SCAN_FOR_AUTOLOAD_AND_SLOW_SCAN_ON_ANOTHER_THREAD

class CSavegameSlowPS3Scan
{
private:
//	Private data
	static bool ms_bScanHasAlreadyBeenDone;
	static bool ms_bSavegameEnumerationInProgress;
	static volatile bool sm_bHasThreadFinished;

//	Private functions
	static void SlowPS3Scan(void*);

public:
//	Public functions
	static void Init(unsigned initMode);

	static bool CreateThreadToReadNamesAndDatesOfSaveGames();

//	static void WaitForThreadToFinish(const char *pNameOfCallingFunction, bool bAssertIfNotFinished);
	static bool HasThreadFinished();

	static bool GetScanHasAlreadyBeenDone() { return ms_bScanHasAlreadyBeenDone; }

	static bool IsSavegameEnumerationInProgress();
};

#endif	//	__PPU && __QUICK_SCAN_FOR_AUTOLOAD_AND_SLOW_SCAN_ON_ANOTHER_THREAD

#endif	//	SAVEGAME_SLOW_PS3_SCAN_H
