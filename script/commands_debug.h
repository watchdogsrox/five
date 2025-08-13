#ifndef _COMMANDS_DEBUG_H_
#define _COMMANDS_DEBUG_H_

namespace debug_commands
{
	void SetupScriptCommands();

	void CommandSaveIntToDebugFile(int IntToSave);
	void CommandSaveFloatToDebugFile(float FloatToSave);
	void CommandSaveNewlineToDebugFile();
	void CommandSaveStringToDebugFile(const char *pStringToSave);
}

#endif	//	_COMMANDS_DEBUG_H_
