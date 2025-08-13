//
// filename:	commands_streaming.h
// description:	
//

#ifndef INC_COMMANDS_STREAMING_H_
#define INC_COMMANDS_STREAMING_H_

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
// Game headers

// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// --- Globals ------------------------------------------------------------------

class GtaThread;	//	declared in script\gta_thread.h

namespace streaming_commands
{
	void SetupScriptCommands();

	void RequestIpl(const char* pName);
}

#endif // !INC_COMMANDS_STREAMING_H_

