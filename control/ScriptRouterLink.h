/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : ScriptRouterLink.h
// PURPOSE : Link created from a Script Router Context Data representing user's intended destination
//
// AUTHOR  : ross.mcdonald
// STARTED : February 2021
//
/////////////////////////////////////////////////////////////////////////////////

#include "atl/hashstring.h"
#include "string/string.h"
#include "ScriptRouterContext.h"

#if GEN9_STANDALONE_ENABLED

#ifndef SCRIPT_ROUTER_LINK_H
#define SCRIPT_ROUTER_LINK_H

class ScriptRouterLink
{

public:	// functions
	static const atHashString SRL_SOURCE;
	static const atHashString SRL_MODE;
	static const atHashString SRL_ARGTYPE;
	static const atHashString SRL_ARG;

	static bool SetScriptRouterLink(const ScriptRouterContextData& SRCData);
	static int GetScriptRouterLinkHash();
	static const atString& GetScriptRouterLink();
	static bool HasPendingScriptRouterLink();
	static void ClearScriptRouterLink();

	static bool ParseScriptRouterLink(ScriptRouterContextData& outSRCData);

private: // members
	static atString sm_scriptRouterLink;
};

#endif // SCRIPT_ROUTER_LINK_H

#endif //GEN9_STANDALONE_ENABLED