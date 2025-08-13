
#ifndef SAVEGAME_NEW_GAME_CHECKS_H
#define SAVEGAME_NEW_GAME_CHECKS_H


#if RSG_ORBIS

// Rage headers
#include "file/savegame.h"

// Game headers
#include "SaveLoad/savegame_defines.h"


class CSavegameNewGameChecks
{
private:
//	Private data
	static bool ms_bShouldNewGameChecksBePerformed;		//	TRUE if the code should check for free space for the profile and save games at the start of the game.
														//	Can only be set to FALSE in !__FINAL builds (by running with -nosignincheck)
	static int ms_nFreeSpaceRequiredForSaveGames;

//	Private functions
	static MemoryCardError	HandleFreeSpaceChecksAtStartOfGame(void);	

public:
//	Public functions
	static void Init(unsigned initMode);

	static void BeginNewGameChecks();
	static MemoryCardError ChecksAtStartOfNewGame();

	static bool ShouldNewGameChecksBePerformed() { return ms_bShouldNewGameChecksBePerformed; }
	static bool HaveFreeSpaceChecksCompleted() { return !ms_bShouldNewGameChecksBePerformed; }
#if RSG_BANK
	static void SetFreeSpaceChecksCompleted(const bool set) { ms_bShouldNewGameChecksBePerformed = !set; }
#endif
	static int GetFreeSpaceRequiredForSaveGames() { return ms_nFreeSpaceRequiredForSaveGames; }
};

#endif	//	RSG_ORBIS


#endif	//	SAVEGAME_NEW_GAME_CHECKS_H

