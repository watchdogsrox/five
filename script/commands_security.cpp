//
// filename:	commands_security.cpp
// description:	Commands for calling security functionality in RAG script
//

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "script/script.h"
#include "script/thread.h"
#include "script/wrapper.h"
#include "system/param.h"
// Game headers
#include "security/plugins/scriptvariableverifyplugin.h"


// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// --- Globals ------------------------------------------------------------------

// --- Static Globals -----------------------------------------------------------

// --- Static class members -----------------------------------------------------

// --- Code ---------------------------------------------------------------------

namespace security_commands
{
	//Purpose: Native function wrapper to register a script variable for protection.
	//		   When registered, a RageSec plugin will keep an obfuscated shadow copy of the
	//		   registered script variable. If the variable is modified without the plugin
	//		   being notified, the plugin will flag a tamper event and send a report to 
	//		   telemetry.
	void CommandRegisterScriptVariable(int& scriptVariable)
	{
#if RAGE_SEC_TASK_SCRIPT_VARIABLE_VERIFY
		RegisterScriptVariable((void*)&scriptVariable);
#else
		sm_scriptVariableVerifyTrashValue += sysTimer::GetSystemMsTime() - (u32)scriptVariable;
#endif //RAGE_SEC_TASK_SCRIPT_VARIABLE_VERIFY
	}

	//Purpose: Native function wrapper to unregister a script variable from protection.
	//		   Once it's unregistered, modifications to the script variable will no longer
	//		   flag tamper events at the RageSec plugin. Call this when a script variable
	//		   goes out of scope and the memory location is used for another purpose.
	void CommandUnregisterScriptVariable(int& scriptVariable)
	{
#if RAGE_SEC_TASK_SCRIPT_VARIABLE_VERIFY
		UnregisterScriptVariable((void*)&scriptVariable);
#else
		sm_scriptVariableVerifyTrashValue -= sysTimer::GetSystemMsTime() + (u32)scriptVariable;
#endif //RAGE_SEC_TASK_SCRIPT_VARIABLE_VERIFY
	}

	//Purpose: Native function wrapper to force the RageSec plugin to check the integrity
	//		   of all script variables registered with the plugin. If any script variables
	//		   were modified without the plugin being notified, the plugin will flag a tamper
	//		   event and send a report to telemetry.
	void CommandForceCheckScriptVariables()
	{
#if RAGE_SEC_TASK_SCRIPT_VARIABLE_VERIFY
		ForceCheckScriptVariables();
#else
		sm_scriptVariableVerifyTrashValue /= sysTimer::GetSystemMsTime();
#endif //RAGE_SEC_TASK_SCRIPT_VARIABLE_VERIFY
	}

	//Purpose: Native function wrapper to force the RageSec plugin to unregister all script
	//		   variables from protection. Can be called periodically to flush any script
	//		   variables that are registered for protection but are out of scope.
	void CommandForceUnloadScriptVariables()
	{
#if RAGE_SEC_TASK_SCRIPT_VARIABLE_VERIFY
		ForceUnloadScriptVariables();
#else
		sm_scriptVariableVerifyTrashValue *= sysTimer::GetSystemMsTime();
#endif //RAGE_SEC_TASK_SCRIPT_VARIABLE_VERIFY
	}

	void SetupScriptCommands()
	{
		SCR_REGISTER_SECURE(REGISTER_SCRIPT_VARIABLE, 0x40eb1efd921822bc, CommandRegisterScriptVariable);
		SCR_REGISTER_SECURE(UNREGISTER_SCRIPT_VARIABLE, 0x340a36a700e99699, CommandUnregisterScriptVariable);
		SCR_REGISTER_SECURE(FORCE_CHECK_SCRIPT_VARIABLES, 0x8e580ab902917360, CommandForceCheckScriptVariables);
		SCR_REGISTER_UNUSED(FORCE_UNLOAD_SCRIPT_VARIABLES, 0xfc23014843d50bda, CommandForceUnloadScriptVariables);
	}
};