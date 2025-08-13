//
// filename:	commands_hud.h
// description:	
//

#ifndef INC_COMMANDS_HUD_H_
#define INC_COMMANDS_HUD_H_

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
// Game headers

// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// --- Globals ------------------------------------------------------------------

namespace hud_commands
{
	void SetupScriptCommands();

	bool CommandIsPauseMenuActive();

	void CommandSetTextRenderId(int renderID);
}


#endif // !INC_COMMANDS_HUD_H_
