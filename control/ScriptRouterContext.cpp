/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : ScriptRouterContext.cpp
// PURPOSE : Data set for the script side transition router, which can be used to create Script Router Links, and into which Script Router Links can be resolved.
//
// AUTHOR  : glenn.cullen
// STARTED : December 2020
//
/////////////////////////////////////////////////////////////////////////////////


#include "ScriptRouterContext.h"

#if GEN9_STANDALONE_ENABLED

#define SRC_SOURCE_LANDINGPAGE			"SRCS_NATIVE_LANDING_PAGE"
#define SRC_SOURCE_PROSPEROGAMEINTENT	"SRCS_PROSPERO_GAME_INTENT"
#define SRC_SOURCE_SCRIPT				"SRCS_SCRIPT"

#define SRC_MODE_FREEMODE				"SRCM_FREE"
#define SRC_MODE_STORYMODE				"SRCM_STORY"
#define SRC_MODE_CREATORMODE			"SRCM_CREATOR"
#define SRC_MODE_EDITORMODE				"SRCM_EDITOR"
#define SRC_MODE_LANDINGPAGE			"SRCM_LANDING_PAGE"

#define SRC_ARGTYPE_NONE				"SRCA_NONE"
#define SRC_ARGTYPE_LOCATION			"SRCA_LOCATION"
#define SRC_ARGTYPE_PROPERTY			"SRCA_PROPERTY"
#define SRC_ARGTYPE_HEIST				"SRCA_HEIST"
#define SRC_ARGTYPE_SERIES				"SRCA_SERIES"
#define SRC_ARGTYPE_WEBPAGE				"SRCA_WEBPAGE"
#define SRC_ARGTYPE_GAMBLING			"SRCA_GAMBLING"
#define SRC_ARGTYPE_CHARACTER_CREATOR	"SRCA_CHARACTER_CREATOR"
#define SRC_ARGTYPE_STORE				"SRCA_STORE"
#define SRC_ARGTYPE_MEMBERSHIP			"SRCA_MEMBERSHIP"
#define SRC_ARGTYPE_LANDING_PAGE_ENTRYPOINT_ID	"SRCA_LANDING_PAGE_ENTRYPOINT_ID"

atString ScriptRouterContext::GetStringFromSRCSource(eScriptRouterContextSource SRCSource)
{
	atString returnVal;

	switch (SRCSource)
	{
	case eScriptRouterContextSource::SRCS_INVALID:
		break;
	case eScriptRouterContextSource::SRCS_NATIVE_LANDING_PAGE:
		returnVal = SRC_SOURCE_LANDINGPAGE;
		break;
	case eScriptRouterContextSource::SRCS_PROSPERO_GAME_INTENT:
		returnVal = SRC_SOURCE_PROSPEROGAMEINTENT;
		break;
	case eScriptRouterContextSource::SRCS_SCRIPT:
		returnVal = SRC_SOURCE_SCRIPT;
		break;
	}

	Assertf(returnVal != NULL, "ScriptRouterContext::GetStringFromSRCMode - Supplied source type is none, invalid or not supported.");

	return returnVal;
}

atString ScriptRouterContext::GetStringFromSRCMode(eScriptRouterContextMode SRCMode)
{
	atString returnVal;

	switch (SRCMode)
	{
	case eScriptRouterContextMode::SRCM_INVALID:
		break;
	case eScriptRouterContextMode::SRCM_FREE:
		returnVal = SRC_MODE_FREEMODE;
		break;
	case eScriptRouterContextMode::SRCM_STORY:
		returnVal = SRC_MODE_STORYMODE;
		break;
	case eScriptRouterContextMode::SRCM_CREATOR:
		returnVal = SRC_MODE_CREATORMODE;
		break;
	case eScriptRouterContextMode::SRCM_EDITOR:
		returnVal = SRC_MODE_EDITORMODE;
		break;
	case eScriptRouterContextMode::SRCM_LANDING_PAGE:
		returnVal = SRC_MODE_LANDINGPAGE;
		break;
	}

	Assertf(returnVal != NULL, "ScriptRouterContext::GetStringFromSRCMode - Supplied mode type is none, invalid or not supported.");

	return returnVal;
}

atString ScriptRouterContext::GetStringFromSRCArgument(eScriptRouterContextArgument SRCArg)
{
	atString returnVal;

	switch (SRCArg)
	{
	case eScriptRouterContextArgument::SRCA_NONE:
		returnVal = SRC_ARGTYPE_NONE;
		break;
	case eScriptRouterContextArgument::SRCA_LOCATION:
		returnVal = SRC_ARGTYPE_LOCATION;
		break;
	case eScriptRouterContextArgument::SRCA_PROPERTY:
		returnVal = SRC_ARGTYPE_PROPERTY;
		break;
	case eScriptRouterContextArgument::SRCA_HEIST:
		returnVal = SRC_ARGTYPE_HEIST;
		break;
	case eScriptRouterContextArgument::SRCA_SERIES:
		returnVal = SRC_ARGTYPE_SERIES;
		break;
	case eScriptRouterContextArgument::SRCA_WEBPAGE:
		returnVal = SRC_ARGTYPE_WEBPAGE;
		break;
	case eScriptRouterContextArgument::SRCA_GAMBLING:
		returnVal = SRC_ARGTYPE_GAMBLING;
		break;
	case eScriptRouterContextArgument::SRCA_CHARACTER_CREATOR:
		returnVal = SRC_ARGTYPE_CHARACTER_CREATOR;
		break;
	case eScriptRouterContextArgument::SRCA_STORE:
		returnVal = SRC_ARGTYPE_STORE;
		break;
	case eScriptRouterContextArgument::SRCA_MEMBERSHIP:
		returnVal = SRC_ARGTYPE_MEMBERSHIP;
		break;
	case eScriptRouterContextArgument::SRCA_LANDING_PAGE_ENTRYPOINT_ID:
		returnVal = SRC_ARGTYPE_MEMBERSHIP;
		break;
	case eScriptRouterContextArgument::SRCA_INVALID:
		break;
	}

	Assertf(returnVal != NULL, "ScriptRouterContext::GetStringFromSRCArgument - Supplied arg type is invalid or not supported.");

	return returnVal;
}

eScriptRouterContextSource ScriptRouterContext::GetSRCSourceFromString(const atString& str) 
{
	eScriptRouterContextSource returnVal = eScriptRouterContextSource::SRCS_INVALID;

	if (str == SRC_SOURCE_LANDINGPAGE)
	{
		returnVal = eScriptRouterContextSource::SRCS_NATIVE_LANDING_PAGE;
	}
	else if (str == SRC_SOURCE_PROSPEROGAMEINTENT)
	{
		returnVal = eScriptRouterContextSource::SRCS_PROSPERO_GAME_INTENT;
	}
	else if (str == SRC_SOURCE_SCRIPT)
	{
		returnVal = eScriptRouterContextSource::SRCS_SCRIPT;
	}
	
	Assertf(returnVal != eScriptRouterContextSource::SRCS_INVALID, "ScriptRouterContext::GetSRCSourceFromString - There is no enum mapping for the supplied string: %s", str.c_str());

	return returnVal;
}

eScriptRouterContextMode ScriptRouterContext::GetSRCModeFromString(const atString& str) 
{
	eScriptRouterContextMode returnVal = eScriptRouterContextMode::SRCM_INVALID;

	if (str == SRC_MODE_FREEMODE)
	{
		returnVal = eScriptRouterContextMode::SRCM_FREE;
	}
	else if (str == SRC_MODE_STORYMODE)
	{
		returnVal = eScriptRouterContextMode::SRCM_STORY;
	}
	else if (str == SRC_MODE_CREATORMODE)
	{
		returnVal = eScriptRouterContextMode::SRCM_CREATOR;
	}
	else if (str == SRC_MODE_EDITORMODE)
	{
		returnVal = eScriptRouterContextMode::SRCM_EDITOR;
	}
	else if (str == SRC_MODE_LANDINGPAGE)
	{
		returnVal = eScriptRouterContextMode::SRCM_LANDING_PAGE;
	}

	Assertf(returnVal != eScriptRouterContextMode::SRCM_INVALID, "ScriptRouterContext::GetSRCModeFromString - There is no enum mapping for the supplied string: %s", str.c_str());

	return returnVal;
}

eScriptRouterContextArgument ScriptRouterContext::GetSRCArgFromString(const atString& str) 
{
	eScriptRouterContextArgument returnVal = eScriptRouterContextArgument::SRCA_INVALID;

	if (str == SRC_ARGTYPE_NONE)
	{
		returnVal = eScriptRouterContextArgument::SRCA_NONE;
	}
	else if (str == SRC_ARGTYPE_LOCATION)
	{
		returnVal = eScriptRouterContextArgument::SRCA_LOCATION;
	}
	else if (str == SRC_ARGTYPE_PROPERTY)
	{
		returnVal = eScriptRouterContextArgument::SRCA_PROPERTY;
	}
	else if (str == SRC_ARGTYPE_HEIST)
	{
		returnVal = eScriptRouterContextArgument::SRCA_HEIST;
	}
	else if (str == SRC_ARGTYPE_SERIES)
	{
		returnVal = eScriptRouterContextArgument::SRCA_SERIES;
	}
	else if (str == SRC_ARGTYPE_WEBPAGE)
	{
		returnVal = eScriptRouterContextArgument::SRCA_WEBPAGE;
	}
	else if (str == SRC_ARGTYPE_GAMBLING)
	{
		returnVal = eScriptRouterContextArgument::SRCA_GAMBLING;
	}
	else if (str == SRC_ARGTYPE_CHARACTER_CREATOR)
	{
		returnVal = eScriptRouterContextArgument::SRCA_CHARACTER_CREATOR;
	}
	else if (str == SRC_ARGTYPE_STORE)
	{
		returnVal = eScriptRouterContextArgument::SRCA_STORE;
	}
	else if (str == SRC_ARGTYPE_MEMBERSHIP)
	{
		returnVal = eScriptRouterContextArgument::SRCA_MEMBERSHIP;
	}
	else if (str == SRC_ARGTYPE_LANDING_PAGE_ENTRYPOINT_ID)
	{
		returnVal = eScriptRouterContextArgument::SRCA_LANDING_PAGE_ENTRYPOINT_ID;
	}

	Assertf(returnVal != eScriptRouterContextArgument::SRCA_INVALID, "ScriptRouterContext::GetSRCArgFromString - There is no enum mapping for the supplied string: %s", str.c_str());

	return returnVal;
}

MultiplayerGameMode ScriptRouterContext::GetGameModeFromSRCData(const ScriptRouterContextData& srcData)
{
	const eScriptRouterContextMode srcMode = (eScriptRouterContextMode)srcData.m_ScriptRouterMode.Int;
	switch (srcMode)
	{
	case SRCM_FREE:
		return GameMode_Freeroam;
	case SRCM_STORY:
		return GameMode_Invalid; // GameMode_Invalid is GAMEMODE_SP in script
	case SRCM_CREATOR:
		return GameMode_Creator;
	case SRCM_EDITOR:
		return GameMode_Editor;
	case SRCM_LANDING_PAGE:
		return GameMode_Invalid; // TODO GEN9_STANDALONE: Add Landing Page mode to MultiplayerGameMode enum
	case SRCM_INVALID:
		Errorf("[ScriptRouterContext::GetGameModeFromSRCData] Attempting to retrieve invalid SRCMode");
		return GameMode_Invalid;
	}

	return GameMode_Invalid;
}

#endif
