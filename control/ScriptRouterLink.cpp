/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : ScriptRouterLink.cpp
// PURPOSE : Link created from a Script Router Context Data representing user's intended destination
//
// AUTHOR  : ross.mcdonald
// STARTED : February 2021
//
/////////////////////////////////////////////////////////////////////////////////

#include "ScriptRouterLink.h"
#include "string/stringhash.h"

#if GEN9_STANDALONE_ENABLED

const atHashString ScriptRouterLink::SRL_SOURCE = ATSTRINGHASH("source", 0x2220A0A0);
const atHashString ScriptRouterLink::SRL_MODE = ATSTRINGHASH("mode", 0x68155C83);
const atHashString ScriptRouterLink::SRL_ARGTYPE = ATSTRINGHASH("argtype", 0xCA8EBFC6);
const atHashString ScriptRouterLink::SRL_ARG = ATSTRINGHASH("arg", 0x87903243);
atString ScriptRouterLink::sm_scriptRouterLink;

#define SRL_TOKEN_SOURCE "source"
#define SRL_TOKEN_MODE "mode"
#define SRL_TOKEN_ARGTYPE "argType"
#define SRL_TOKEN_ARG "arg"

#define SRL_TOKEN_DELIMITER ','
#define SRL_TOKEN_VALUEDELIMITER '='

/*
Sets the pending script router link based on the provided script router context
Returns true if the set was successful
*/
bool ScriptRouterLink::SetScriptRouterLink(const ScriptRouterContextData& SRCData)
{
	Assertf(!HasPendingScriptRouterLink(), "Setting Script Router Link while there is already a valid link. If this is intentional call ClearScriptRouterLink.");

	if (HasPendingScriptRouterLink())
	{
		return false;
	}

	sm_scriptRouterLink += SRL_TOKEN_SOURCE;
	sm_scriptRouterLink += SRL_TOKEN_VALUEDELIMITER;
	sm_scriptRouterLink += ScriptRouterContext::GetStringFromSRCSource((eScriptRouterContextSource)SRCData.m_ScriptRouterSource.Int);
	
	sm_scriptRouterLink += SRL_TOKEN_DELIMITER;
	sm_scriptRouterLink += SRL_TOKEN_MODE;
	sm_scriptRouterLink += SRL_TOKEN_VALUEDELIMITER;
	sm_scriptRouterLink += ScriptRouterContext::GetStringFromSRCMode((eScriptRouterContextMode)SRCData.m_ScriptRouterMode.Int);
	
	sm_scriptRouterLink += SRL_TOKEN_DELIMITER;
	sm_scriptRouterLink += SRL_TOKEN_ARGTYPE;
	sm_scriptRouterLink += SRL_TOKEN_VALUEDELIMITER;
	sm_scriptRouterLink += ScriptRouterContext::GetStringFromSRCArgument((eScriptRouterContextArgument)SRCData.m_ScriptRouterArgType.Int);

	// add argument if one is expected
	if (SRCData.m_ScriptRouterArgType.Int != (int)eScriptRouterContextArgument::SRCA_NONE)
	{
		sm_scriptRouterLink += SRL_TOKEN_DELIMITER;
		sm_scriptRouterLink += SRL_TOKEN_ARG;
		sm_scriptRouterLink += SRL_TOKEN_VALUEDELIMITER;
		sm_scriptRouterLink += SRCData.m_ScriptRouterArg;
	}
	
	Displayf("ScriptRouterLink::SetScriptRouterLink: Set to: %s", sm_scriptRouterLink.c_str());

	return true;
}

/*
Parses the pending script router link into outSRCData
*/
bool ScriptRouterLink::ParseScriptRouterLink(ScriptRouterContextData& outSRCData)
{
	if (!HasPendingScriptRouterLink())
	{
		Errorf("Attempting to parse Script Router Link while there is no pending link.");
		return false;
	}

	// Split the link into tokens
	atArray<atString> linkTokens;
	sm_scriptRouterLink.Split(linkTokens, SRL_TOKEN_DELIMITER);
	const int tokenCount = linkTokens.GetCount();

	// Set up values for parsing data
	eScriptRouterContextSource eSRCSource = SRCS_INVALID;
	eScriptRouterContextMode eSRCMode = SRCM_INVALID;
	eScriptRouterContextArgument eSRCArgType = SRCA_INVALID;
	atString sSRCArg;

	// Obtain values from the tokens
	bool bSuccess = true;
	atString tokenId, tokenValue;
	for (int i = 0; i < tokenCount; ++i)
	{
		const int separatorIndex = linkTokens[i].IndexOf(SRL_TOKEN_VALUEDELIMITER);
		tokenId.Set(linkTokens[i], 0, separatorIndex); // Start of string to before separator
		tokenValue.Set(linkTokens[i], separatorIndex + 1); // After separator to end of string
		if (tokenId == SRL_TOKEN_SOURCE)
		{
			eSRCSource = ScriptRouterContext::GetSRCSourceFromString(tokenValue);
		}
		else if (tokenId == SRL_TOKEN_MODE)
		{
			eSRCMode = ScriptRouterContext::GetSRCModeFromString(tokenValue);
		}
		else if (tokenId == SRL_TOKEN_ARGTYPE)
		{
			eSRCArgType = ScriptRouterContext::GetSRCArgFromString(tokenValue);
		}
		else if (tokenId == SRL_TOKEN_ARG)
		{
			sSRCArg = tokenValue;
		}
		else
		{
			Errorf("Unrecognised script router token %s", tokenId.c_str());
		}
	}

	// Populate outSRCData
	Assertf(eSRCSource != SRCS_INVALID, "Invalid Script Link: Missing %s token", SRL_TOKEN_SOURCE);
	bSuccess = bSuccess && eSRCSource != SRCS_INVALID;
	outSRCData.m_ScriptRouterSource.Int = (int)eSRCSource;

	Assertf( eSRCMode != SRCM_INVALID, "Invalid Script Link: Missing %s token", SRL_TOKEN_MODE);
	bSuccess = bSuccess && eSRCMode != SRCM_INVALID;
	outSRCData.m_ScriptRouterMode.Int = (int)eSRCMode;

	Assertf( eSRCArgType != SRCA_INVALID, "Invalid Script Link: Missing %s token", SRL_TOKEN_ARGTYPE);
	bSuccess = bSuccess && eSRCArgType != SRCA_INVALID;
	outSRCData.m_ScriptRouterArgType.Int = (int)eSRCArgType;

	if (eSRCArgType != SRCA_NONE)
	{
		int iSRCArgSize = sSRCArg.GetLength();

		Assertf(iSRCArgSize > 0, "Invalid Script Link: Missing %s token", SRL_TOKEN_ARG);
		bSuccess = bSuccess && iSRCArgSize > 0;
		
		Assertf(iSRCArgSize <= scrMaxTextLabelSize, "Invalid Script Link: %s token is too long", SRL_TOKEN_ARG);
		bSuccess = bSuccess && iSRCArgSize <= scrMaxTextLabelSize;

		safecpy(outSRCData.m_ScriptRouterArg, sSRCArg.c_str());
	}

	return bSuccess;

}

/*
Returns the hashed pending script router link
*/
int ScriptRouterLink::GetScriptRouterLinkHash()
{
	return atStringHash(sm_scriptRouterLink.c_str());
}

/*
Returns the pending script router link
*/
const atString& ScriptRouterLink::GetScriptRouterLink()
{
	return sm_scriptRouterLink;
}

/*
Returns true if there is a pending script router link
*/
bool ScriptRouterLink::HasPendingScriptRouterLink()
{
	return sm_scriptRouterLink.GetLength() > 0;
}

/*
Clears the pending script router link
*/
void ScriptRouterLink::ClearScriptRouterLink()
{
	sm_scriptRouterLink.Clear();
}

#endif