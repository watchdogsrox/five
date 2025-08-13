

#ifndef SAVEGAME_CHECK_FILE_EXISTS_H
#define SAVEGAME_CHECK_FILE_EXISTS_H

#include "SaveLoad/savegame_defines.h"

enum eCheckFileExistsState
{
	CHECK_FILE_EXISTS_DO_NOTHING = 0,
	CHECK_FILE_EXISTS_INITIAL_CHECKS,
	CHECK_FILE_EXISTS_DO_DAMAGE_CHECK,
	CHECK_FILE_EXISTS_HANDLE_ERRORS
};


class CSavegameCheckFileExists
{
private:
	//	Private data
	static eCheckFileExistsState ms_CheckFileExistsStatus;

	static s32 ms_SlotIndexToCheck;

	static bool ms_bFileExists;
	static bool ms_bFileIsDamaged;

	//	Private functions
	static void EndCheckFileExists();

public:
	//	Public functions
	static void Init(unsigned initMode);

	static MemoryCardError BeginCheckFileExists(int SlotNumber);

	static MemoryCardError CheckFileExists();

	static void GetFileStatus(bool &bFileExists, bool &bFileIsDamaged) { bFileExists = ms_bFileExists; bFileIsDamaged = ms_bFileIsDamaged; }
};

#endif	//	SAVEGAME_CHECK_FILE_EXISTS_H


