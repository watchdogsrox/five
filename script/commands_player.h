#ifndef _COMMANDS_PLAYER_H_
#define _COMMANDS_PLAYER_H_

namespace rage { class Vector3; }

namespace player_commands
{
	void SetupScriptCommands();

	const char* GetPlayerName(int PlayerIndex, const char* commandName);
	int CommandGetPlayerHUDColour(int PlayerIndex);
	int CommandGetPlayerIndex();
}

#endif	//	_COMMANDS_PLAYER_H_
