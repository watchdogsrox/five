// 
// script/script_helper.h 
// 
// Copyright (C) 1999-2008 Rockstar Games.  All Rights Reserved. 
// 


#ifndef INC_SCRIPT_HELPER_H 
#define INC_SCRIPT_HELPER_H 

#include "script\handlers\GameScriptHandlerNetwork.h"
#include "script\script_channel.h"
#include "script\script.h"

//Adding script verifies

//Standard script assert
#define SCRIPT_VERIFYF(CONDITION, STRINGFORMAT, ...) (scriptVerifyf (CONDITION, "%s: " STRINGFORMAT, CTheScripts::GetCurrentScriptNameAndProgramCounter(), __VA_ARGS__ ))
#define SCRIPT_VERIFY(CONDITION, STRINGTODISPLAY) (SCRIPT_VERIFYF(CONDITION, "%s", STRINGTODISPLAY ))

#define SCRIPT_ASSERTF(CONDITION, STRINGFORMAT, ...) scriptAssertf (CONDITION, "%s: " STRINGFORMAT, CTheScripts::GetCurrentScriptNameAndProgramCounter(), __VA_ARGS__ )
#define SCRIPT_ASSERT(CONDITION, STRINGTODISPLAY) SCRIPT_ASSERTF(CONDITION, "%s", STRINGTODISPLAY )

#define SCRIPT_VERIFY_TWO_STRINGS(CONDITION, STRINGTODISPLAY,STRINGTODISPLAYTWO )(scriptVerifyf (CONDITION, "%s: %s %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), STRINGTODISPLAY, STRINGTODISPLAYTWO ))

#define SCRIPT_ASSERT_TWO_STRINGS(CONDITION, STRINGTODISPLAY,STRINGTODISPLAYTWO ) scriptAssertf (CONDITION, "%s: %s %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), STRINGTODISPLAY, STRINGTODISPLAYTWO )

// network script verifies
#define IS_SCRIPT_NETWORK_REGISTERED() (CTheScripts::GetCurrentGtaScriptHandlerNetwork())

#define SCRIPT_VERIFY_NETWORK_REGISTERED(commandName)\
	(scriptVerifyf(CTheScripts::GetCurrentGtaScriptHandlerNetwork(), "%s: %s - This script has not been set as a network script.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName))

#define IS_SCRIPT_NETWORK_ACTIVE() (CTheScripts::GetCurrentGtaScriptHandlerNetwork() && \
									CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNetworkComponent() && \
									CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNetworkComponent()->GetState() == scriptHandlerNetComponent::NETSCRIPT_PLAYING)

#define SCRIPT_VERIFY_NETWORK_ACTIVE(commandName)\
	(scriptVerifyf(CTheScripts::GetCurrentGtaScriptHandlerNetwork(), "%s: %s - This script has not been set as a network script.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName) && \
	 scriptVerifyf(CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNetworkComponent(), "%s: %s - A network game is not running.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName) && \
	 scriptVerifyf(CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNetworkComponent()->GetState() == scriptHandlerNetComponent::NETSCRIPT_PLAYING,  "%s: %s - This script is not in a NETSCRIPT_PLAYING state.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName))

#define SCRIPT_VERIFY_PARTICIPANT_INDEX(commandName, playerIndex)\
	(scriptVerifyf(CTheScripts::GetCurrentGtaScriptHandlerNetwork(), "%s: %s - This script has not been set as a network script.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName) && \
	 scriptVerifyf(playerIndex>=0&&playerIndex<CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetMaxNumParticipants(), "%s: %s - Invalid participant index (%d)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName, playerIndex))

#define SCRIPT_VERIFY_PLAYER_INDEX(commandName, playerIndex)\
	(scriptVerifyf(playerIndex>=0&&playerIndex<MAX_NUM_PHYSICAL_PLAYERS, "%s: %s - Invalid player index (%d)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName, playerIndex))

#define SCRIPT_VERIFY_TEAM_INDEX(commandName, teamIndex)\
	(scriptVerifyf(teamIndex>=0&&teamIndex<MAX_NUM_TEAMS, "%s: %s - Invalid team index (%d)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName, teamIndex))

#define GET_SCRIPT_ID() (CGameScriptId(*CTheScripts::GetCurrentGtaScriptThread()))

// Helper to verify that a certain script has been registered with the script event manager.
#define SCRIPT_VERIFY_EVENTMGR_REGISTERED(commandName) \
			scriptVerifyf(NetworkInterface::GetScriptEventMgr().IsScriptRegistered(scrThread::GetActiveThread()->GetThreadId()), "%s:%s - Script was not registered.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName)

#endif