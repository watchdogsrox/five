#ifndef _COMMANDS_NETWORK_H_
#define _COMMANDS_NETWORK_H_

namespace network_commands
{
	void SetupScriptCommands();
	void ResetTrackingVariables();

	bool HasValidLocalGamerIndex(OUTPUT_ONLY(const char* fn));

	// Expose some stuff to RAG for debugging
#if __BANK
	bool CommandTriggerPlayerCrcHackerCheck(int PlayerIndex, int systemToCheck, int specificSubsystem);
	bool CommandTriggerTuningCrcHackerCheck(int PlayerIndex, const char* tuningGroup, const char* tuningItem);
	bool CommandTriggerFileCrcHackerCheck(int PlayerIndex, const char* filePath);
	bool CommandShouldWarnOfSimpleModCheck();
#endif
}

#endif
