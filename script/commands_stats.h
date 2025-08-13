#ifndef _COMMANDS_STATS_H_
#define _COMMANDS_STATS_H_

namespace stats_commands
{
	void  SetupScriptCommands();
	bool  CommandGetRemotePlayerProfileStatValue(const int keyHash, const CNetGamePlayer* player, void* data, const char* command);
	bool  CommandStatSetUserId(const int keyHash, const char* data, bool ASSERT_ONLY(coderAssert));
	const char* CommandStatGetUserId(const int keyHash);
	BANK_ONLY( void  TestCommandLeaderboards2ReadRankPrediction(); )
}

#endif	//	_COMMANDS_STATS_H_
