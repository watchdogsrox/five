

#ifndef SAVEGAME_INITIAL_CHECKS_H
#define SAVEGAME_INITIAL_CHECKS_H

// Game headers
#include "SaveLoad/savegame_defines.h"


class CSavegameInitialChecks
{
private:
//	Private data
	static int ms_InitialChecksState;

	static u32 ms_SizeOfBufferToBeSaved;

	static bool ms_bManageStorage;	//	This is only ever set to false if !__PPU and the user says they want to choose
									//	another device after discovering there is not enough free space on the current device

	static bool	ms_bPlayerChoseNotToSignIn;
	static bool	ms_bPlayerChoseNotToSelectADevice;

	static bool ms_bForcePlayerToChooseADevice;

//	Private functions
	static bool ShouldTryAgainMessageBeDisplayed();

	static u32 GetSizeOfBufferToBeSaved();

public:
//	Public functions
	static void BeginInitialChecks(bool bForcePlayerToChooseADevice);
	static MemoryCardError InitialChecks();

	static void SetSizeOfBufferToBeSaved(u32 SaveBufferSize) { ms_SizeOfBufferToBeSaved = SaveBufferSize; }

	static bool	GetPlayerChoseNotToSignIn() { return ms_bPlayerChoseNotToSignIn; }
	static bool	GetPlayerChoseNotToSelectADevice() { return ms_bPlayerChoseNotToSelectADevice; }

	static bool ShowLoggedSignInUi();
};

#endif	//	SAVEGAME_INITIAL_CHECKS_H

