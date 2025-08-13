

#ifndef SAVEGAME_LOAD_H
#define SAVEGAME_LOAD_H

#include "SaveLoad/savegame_defines.h"

enum eGenericLoadState
{
	GENERIC_LOAD_DO_NOTHING = 0,
	GENERIC_LOAD_BEGIN_INITIAL_CHECKS,
	GENERIC_LOAD_INITIAL_CHECKS,
	GENERIC_LOAD_GET_FILE_SIZE,
//	GENERIC_LOAD_READ_SIZES_FROM_START_OF_FILE,
	GENERIC_LOAD_ALLOCATE_BUFFER,
	GENERIC_LOAD_BEGIN_LOAD,
	GENERIC_LOAD_DO_LOAD,
	GENERIC_LOAD_WAIT_FOR_LOADING_MESSAGE_TO_FINISH,
	GENERIC_LOAD_HANDLE_ERRORS
};


class CSavegameLoad
{
private:
//	Private data
	static eGenericLoadState	ms_GenericLoadStatus;
	static eTypeOfSavegame		ms_typeOfSavegame;

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	static bool ms_bLoadingForExport;
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

#if __PPU
	static bool ms_bLoadingAtEndOfNetworkGame;
#endif

//	Private functions
	static MemoryCardError		DoLoad();

public:
//	Public functions
	static void Init(unsigned initMode);

	static MemoryCardError GenericLoad();
	static MemoryCardError DeSerialize();

	static void EndGenericLoad();

	static void BeginLoad(eTypeOfSavegame typeOfSavegame);
#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	static void BeginLoadForExport();
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

	static eGenericLoadState GetLoadStatus() { return ms_GenericLoadStatus; }

#if __PPU
	static bool GetLoadingAtEndOfNetworkGame() { return ms_bLoadingAtEndOfNetworkGame; }
	static void SetLoadingAtEndOfNetworkGame(bool bNetworkLoad) { ms_bLoadingAtEndOfNetworkGame = bNetworkLoad; }
#endif
};

#endif	//	SAVEGAME_LOAD_H


