/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : ScriptRouterContext.h
// PURPOSE : Data set for the script side transition router, which can be used to create Script Router Links, and into which Script Router Links can be resolved.
//
// AUTHOR  : glenn.cullen
// STARTED : December 2020
//
/////////////////////////////////////////////////////////////////////////////////

#include "game/ScriptRouterContextEnums.h"
#include "network/NetworkTypes.h"
#include "script/wrapper.h"
#include "core/game.h"

#if GEN9_STANDALONE_ENABLED

#ifndef SCRIPT_ROUTER_CONTEXT_H
#define SCRIPT_ROUTER_CONTEXT_H

struct ScriptRouterContextData {
	scrValue m_ScriptRouterSource;		// stored in .Int
	scrValue m_ScriptRouterMode;		// stored in .Int
	scrValue m_ScriptRouterArgType;		// stored in .Int
	scrTextLabel63 m_ScriptRouterArg;

	ScriptRouterContextData()
	{
		m_ScriptRouterSource.Int = (int)SRCS_INVALID;
		m_ScriptRouterMode.Int = (int)SRCM_INVALID;
		m_ScriptRouterArgType.Int = (int)SRCA_INVALID;
		safecpy(m_ScriptRouterArg, "");
	}

	ScriptRouterContextData(eScriptRouterContextSource eSource, eScriptRouterContextMode eMode, eScriptRouterContextArgument eArgtype = SRCA_NONE, const char* sArg = NULL)
	{
		m_ScriptRouterSource.Int = (int)eSource;
		m_ScriptRouterMode.Int = (int)eMode;
		m_ScriptRouterArgType.Int = (int)eArgtype;
		safecpy(m_ScriptRouterArg, sArg);
	}
};

SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(ScriptRouterContextData);

class ScriptRouterContext
{
public: // utility functions
	static atString GetStringFromSRCSource(eScriptRouterContextSource SRCSource);
	static atString GetStringFromSRCMode(eScriptRouterContextMode SRCMode);
	static atString GetStringFromSRCArgument(eScriptRouterContextArgument SRCArg);

	static eScriptRouterContextSource GetSRCSourceFromString(const atString& str);
	static eScriptRouterContextMode GetSRCModeFromString(const atString& str);
	static eScriptRouterContextArgument GetSRCArgFromString(const atString& str);

	static MultiplayerGameMode GetGameModeFromSRCData(const ScriptRouterContextData& srcData);
};
#endif // SCRIPT_ROUTER_CONTEXT_H

#endif // GEN9_STANDALONE_ENABLED
