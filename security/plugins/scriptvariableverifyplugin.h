#include "security/ragesecengine.h"
#ifndef SECURITY_SCRIPTVARIABLEVERIFYPLUGIN_H
#define SECURITY_SCRIPTVARIABLEVERIFYPLUGIN_H

#if USE_RAGESEC

extern u32 sm_scriptVariableVerifyTrashValue;

#if RAGE_SEC_TASK_SCRIPT_VARIABLE_VERIFY

#include "net/status.h"
#include "net/task.h"
#include "security/ragesecplugin.h"
#include "security/ragesecenginetasks.h"
#include "security/ragesecgameinterface.h"
#include "security/obfuscatedtypes.h"

#define SCRIPT_VARIABLE_VERIFY_ID			0x8C6181AE 
extern atMap<void*, sysObfuscated<int>> sm_scriptVariableShadowCopies;
extern sysCriticalSectionToken sm_shadowCopyLock;

bool ScriptVariableVerifyPlugin_Init();
bool ScriptVariableVerifyPlugin_Work();

//Purpose: Using the address of a script variable as the key, adds an entry in scriptVariableShadowCopies
//		   with a shadow copy of the value at the time. If an entry for the script variable address already
//		   exists, updates the shadow copy with the current value.
//		   Note that this is designed to be called from RAG script through a native wrapper function.
//		   See commands_security.cpp for more information.
#if __FINAL
__forceinline void RegisterScriptVariable(void* scriptVariable)
{
	SYS_CS_SYNC(sm_shadowCopyLock);
	if(sm_scriptVariableShadowCopies.Access(scriptVariable) == nullptr)
	{
		RageSecDebugf3("0x%08X - Registering Script Variable at address 0x%016p with value %d", SCRIPT_VARIABLE_VERIFY_ID, scriptVariable, *((int*)scriptVariable));
		sysObfuscated<int> obfuscatedValue(*((int*)scriptVariable));
		sm_scriptVariableShadowCopies.Insert(scriptVariable, obfuscatedValue);
	}
	else
	{
		RageSecDebugf3("0x%08X - Updating Script Variable at address 0x%016p with value %d", SCRIPT_VARIABLE_VERIFY_ID, scriptVariable, *((int*)scriptVariable));
		sm_scriptVariableShadowCopies.Access(scriptVariable)->Set(*((int*)scriptVariable));
	}
	return;
}
#else
void RegisterScriptVariable(void* scriptVariable);
#endif


//Purpose: Using the address of a script variable as the key, removes an entry in scirptVariableShadowCopies.
//		   Note that this is designed to be called from RAG script through a native wrapper function.
//		   See commands_security.cpp for more information.
#if __FINAL
__forceinline void UnregisterScriptVariable(void* scriptVariable)
{


	SYS_CS_SYNC(sm_shadowCopyLock);
	if(sm_scriptVariableShadowCopies.Access(scriptVariable))
	{
		sm_scriptVariableShadowCopies.Delete(scriptVariable);
		RageSecDebugf3("0x%08X - Unregistering Script Variable at address 0x%016x", SCRIPT_VARIABLE_VERIFY_ID, &scriptVariable);
	}
	else
	{
		RageSecDebugf3("0x%08X - Unable to find Script Variable to unregister at address 0x%016x", SCRIPT_VARIABLE_VERIFY_ID, &scriptVariable);
	}
	return;
}
#else
void UnregisterScriptVariable(void* scriptVariable);
#endif

//Purpose: Force scriptVariableShadowCopies to check if each script variable registered with the
//		   atMap has the expected value stored in the data field.
//		   Note that this is designed to be called from RAG script through a native wrapper function.
//		   See commands_security.cpp for more information.
#if __FINAL
__forceinline void ForceCheckScriptVariables()
{
	RageSecDebugf3("0x%08X - Force checking scriptVariableShadowCopies", SCRIPT_VARIABLE_VERIFY_ID);
	bool isTamperDetected = false;
	{
		
		SYS_CS_SYNC(sm_shadowCopyLock);
		if(sm_scriptVariableShadowCopies.GetNumUsed())
		{
			for(auto it = sm_scriptVariableShadowCopies.CreateIterator(); !it.AtEnd(); it.Next())
			{
				if(*(int*)(it.GetKey()) != it.GetData().Get())
				{

					RageSecDebugf3("0x%08X - Script variable tamper detected at address 0x%016x.", SCRIPT_VARIABLE_VERIFY_ID, it.GetKey());
					RageSecAssertf(false, "0x%08X - Script variable value does not match the value of the shadow copy stored by ScriptVariableVerifyPlugin. "
						"Please create a B* on the Code Security team with the SCL that's causing this assert.", SCRIPT_VARIABLE_VERIFY_ID);
					isTamperDetected = true;
				}
			}
		}
	}
	sm_scriptVariableVerifyTrashValue -= sysTimer::GetSystemMsTime();
	if(isTamperDetected)
	{
		RageSecDebugf3("0x%08X - Sending script variable tamper report", SCRIPT_VARIABLE_VERIFY_ID);
		RageSecPluginGameReactionObject obj(
			REACT_GAMESERVER,
			SCRIPT_VARIABLE_VERIFY_ID,
			fwRandom::GetRandomNumber(),
			fwRandom::GetRandomNumber(),
			0x38DFF122
			);
		rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
	}
}
#else 
void ForceCheckScriptVariables();
#endif

//Purpose: Unregister all script variables registered with scriptVariableShadowCopies.
//		   Note that this is designed to be called from RAG script through a native wrapper function.
//		   See commands_security.cpp for more information.
#if __FINAL
__forceinline void ForceUnloadScriptVariables()
{
	SYS_CS_SYNC(sm_shadowCopyLock);
	RageSecDebugf3("0x%08X - Force unloading scriptVariableShadowCopies", SCRIPT_VARIABLE_VERIFY_ID);
	sm_scriptVariableShadowCopies.Reset();
}
#else
void ForceUnloadScriptVariables();
#endif

#endif // RAGE_SEC_TASK_SCRIPT_VARIABLE_VERIFY

#endif  // USE_RAGESEC

#endif  // SECURITY_SCRIPTVARIABLEVERIFYPLUGIN_H