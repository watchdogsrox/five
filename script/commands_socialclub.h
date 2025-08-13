#ifndef INC_COMMANDS_SOCIALCLUB_H_
#define INC_COMMANDS_SOCIALCLUB_H_

#include "event/EventNetwork.h"

struct scrUGCStateUpdate_Data
{
	scrTextLabel31 m_contentId;
	scrValue m_score;
	scrValue m_score2;
	scrTextLabel31 m_fromGamerTag;
	scrGamerHandle m_gamerHandle;

	scrTextLabel63 m_missionName;
	scrTextLabel31 m_CoPlayerName;
	scrValue m_MissionType;
	scrValue m_MissionSubType;
	scrValue m_Laps;
	scrValue m_swapSenderWithCoPlayer;
};

struct scrBountyInboxMsg_Data
{
	scrTextLabel31 m_fromGTag;
	scrTextLabel31 m_targetGTag;
	scrValue m_iOutcome;
	scrValue m_iCash;
	scrValue m_iRank;
	scrValue m_iTime;
};

namespace socialclub_commands
{
	void SetupScriptCommands();

	bool CommandCheckLicensePlate(const char* inString, int& outToken);
	bool CommandCheckLicensePlate_IsValid(int token);
	bool CommandCheckLicensePlate_IsPending(int token);
	bool CommandCheckLicensePlate_PassedValidityCheck(int token);
	int CommandCheckLicensePlate_GetStatusCode(int token);
}
#endif
