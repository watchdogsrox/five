// Rage headers
#include "script/wrapper.h"
#include "system/language.h"
#include "system/service.h"

// framework headers
#include "fwlocalisation/languagePack.h"
#include "fwscript/scriptinterface.h"

// Game Headers
#include "frontend/PauseMenu.h"
#include "frontend/ProfileSettings.h"
#include "script/Handlers/GameScriptResources.h"
#include "script/script.h"
#include "script/script_debug.h"
#include "script/script_helper.h"

#include "commands_localization.h"

SCRIPT_OPTIMISATIONS();

namespace localization_commands
{

rage::sysLanguage GetSystemLanguage()
{
	return g_SysService.GetSystemLanguage();
}

rage::sysLanguage GetCurrentPlayerLanguage()
{
	rage::s32 const displayLanguage = CPauseMenu::GetMenuPreference( PREF_CURRENT_LANGUAGE );
	rage::sysLanguage const c_finalLanguage = displayLanguage >= rage::LANGUAGE_UNDEFINED && displayLanguage < rage::MAX_LANGUAGES ? 
		(rage::sysLanguage)displayLanguage : rage::LANGUAGE_UNDEFINED;

	return c_finalLanguage;
}

rage::fwLanguagePack::DATE_FORMAT GetSystemDateFormat()
{
	rage::sysLanguage const c_sysLanguage = GetSystemLanguage();
	return fwLanguagePack::GetDateFormatType( c_sysLanguage );
}

rage::fwLanguagePack::DATE_FORMAT GetPlayerDateFormat()
{
	rage::sysLanguage const c_sysLanguage = GetCurrentPlayerLanguage();
	return fwLanguagePack::GetDateFormatType( c_sysLanguage );
}

rage::fwLanguagePack::DATE_FORMAT_DELIMITER GetSystemDateDelimiterType()
{
	rage::sysLanguage const c_sysLanguage = GetSystemLanguage();
	return fwLanguagePack::GetDateFormatDelimiterType( c_sysLanguage );
}

rage::fwLanguagePack::DATE_FORMAT_DELIMITER GetPlayerDateDelimiterType()
{
	rage::sysLanguage const c_sysLanguage = GetCurrentPlayerLanguage();
	return fwLanguagePack::GetDateFormatDelimiterType( c_sysLanguage );
}

void SetupScriptCommands()
{
	// Video Editor
	SCR_REGISTER_SECURE(LOCALIZATION_GET_SYSTEM_LANGUAGE,0x7cacf6619466f437, GetSystemLanguage );
	SCR_REGISTER_SECURE(GET_CURRENT_LANGUAGE,0xe2f2d76a4aa269ff, GetCurrentPlayerLanguage );
	SCR_REGISTER_SECURE(LOCALIZATION_GET_SYSTEM_DATE_TYPE,0xd15434691627435d, GetSystemDateFormat );
	SCR_REGISTER_UNUSED(LOCALIZATION_GET_PLAYER_DATE_TYPE,0x9b03d85b27d70fd5, GetPlayerDateFormat );
	SCR_REGISTER_UNUSED(LOCALIZATION_GET_SYSTEM_DATE_DELIMITER_TYPE,0x0744946f35983dc0, GetSystemDateDelimiterType );
	SCR_REGISTER_UNUSED(LOCALIZATION_GET_PLAYER_DATE_DELIMITER_TYPE,0xf3b0b87e86ff676d, GetPlayerDateDelimiterType );
}

}	//	end of namespace replay_commands
