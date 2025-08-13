// Rage headers
#include "script/wrapper.h"

// framework headers
#include "fwscript/scriptinterface.h"

// Game Headers
#include "control/replay/replay.h"
#include "control/replay/ReplaySettings.h"
#include "replaycoordinator/rendering/ReplayPostFxRegistry.h"
#include "script/Handlers/GameScriptResources.h"
#include "script/script.h"
#include "script/script_debug.h"
#include "script/script_helper.h"

#include "commands_replay.h"

SCRIPT_OPTIMISATIONS();

namespace replay_commands
{

void RegisterReplayFilter( const char *UNUSED_PARAM(pEffectName), bool UNUSED_PARAM(useShallowDof))
{
}

bool CommandReplaySystemHasRequestedScriptsPrepareForSave()
{
#if GTA_REPLAY && ALLOW_SCRIPTS_TO_PREPARE_FOR_REPLAY_SAVE
	return CReplayMgr::ShouldScriptsPrepareForSave();
#else
	return false;
#endif	//	GTA_REPLAY && ALLOW_SCRIPTS_TO_PREPARE_FOR_REPLAY_SAVE
}

void CommandSetScriptsHavePreparedForReplaySave(bool REPLAY_ONLY(SCRIPT_PREP_ONLY(bHavePrepared)))
{
#if GTA_REPLAY && ALLOW_SCRIPTS_TO_PREPARE_FOR_REPLAY_SAVE
	CReplayMgr::SetScriptsHavePreparedForSave(bHavePrepared);
#endif	//	GTA_REPLAY && ALLOW_SCRIPTS_TO_PREPARE_FOR_REPLAY_SAVE
}

bool CommandReplaySystemHasRequestedAScriptCleanup()
{
#if GTA_REPLAY
	return CReplayMgr::ShouldScriptsCleanup();
#else
	return false;
#endif	//	GTA_REPLAY
}

void CommandSetScriptsHaveCleanedUpForReplaySystem()
{
#if GTA_REPLAY
	CReplayMgr::SetScriptsHaveCleanedUp();
#endif	//	GTA_REPLAY
}

void CommandSetReplaySystemPausedForSave(bool REPLAY_ONLY(bPaused))
{
#if GTA_REPLAY
	CReplayMgr::PauseTheClearWorldTimeout(bPaused);
#endif	//	GTA_REPLAY
}

void CommandReplayControlShutdown()
{
#if GTA_REPLAY
	CReplayControl::ShutdownSession();
#endif	//	GTA_REPLAY
}


// name:		CommandActivateFrontEndMenu
// description: activates a frontend menu
void CommandActivateRockstarEditor(int REPLAY_ONLY(iOpenedFromSource = OFS_SCRIPT_DEFAULT))
{
#if GTA_REPLAY
	Displayf("##################################################################################################");
	Displayf("SCRIPT CALLED ACTIVATE_ROCKSTAR_EDITOR");
	Displayf("##################################################################################################");

	eOpenedFromSource source = static_cast<eOpenedFromSource>(iOpenedFromSource);
	
	scriptAssertf(source >= OFS_SCRIPT_FIRST && source <= OFS_SCRIPT_LAST, "Native ACTIVATE_ROCKSTAR_EDITOR called with invalid param: OFS_CODE (%d)", source);

	CVideoEditorUi::Open(source);
#endif // GTA_REPLAY
}


void SetupScriptCommands()
{
	// Video Editor
	SCR_REGISTER_SECURE(REGISTER_EFFECT_FOR_REPLAY_EDITOR,0x5ec89c2dd91d82a8,			RegisterReplayFilter );

	SCR_REGISTER_UNUSED(REPLAY_SYSTEM_HAS_REQUESTED_SCRIPT_PREPARE_FOR_SAVE,0xe7328a11c596ff29, CommandReplaySystemHasRequestedScriptsPrepareForSave );
	SCR_REGISTER_UNUSED(SET_SCRIPTS_HAVE_PREPARED_FOR_REPLAY_SAVE,0x3b284b169597b690, CommandSetScriptsHavePreparedForReplaySave );

	SCR_REGISTER_SECURE(REPLAY_SYSTEM_HAS_REQUESTED_A_SCRIPT_CLEANUP,0xd6f85402e4158ead,			CommandReplaySystemHasRequestedAScriptCleanup );
	SCR_REGISTER_SECURE(SET_SCRIPTS_HAVE_CLEANED_UP_FOR_REPLAY_SYSTEM,0x3031cdefbe2b11fb,			CommandSetScriptsHaveCleanedUpForReplaySystem );
	SCR_REGISTER_SECURE(SET_REPLAY_SYSTEM_PAUSED_FOR_SAVE,0x210a725f491a5295,						CommandSetReplaySystemPausedForSave);

	SCR_REGISTER_SECURE(REPLAY_CONTROL_SHUTDOWN,0x1ca083636a4d1b3c,								CommandReplayControlShutdown );

	SCR_REGISTER_SECURE(ACTIVATE_ROCKSTAR_EDITOR,0x20c35e4c2507cc94,								CommandActivateRockstarEditor );
}

}	//	end of namespace replay_commands
