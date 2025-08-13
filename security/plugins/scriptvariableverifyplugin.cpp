#include "security/ragesecengine.h"

#if USE_RAGESEC

#include "system/criticalsection.h"

#include "net/status.h"
#include "net/task.h"
#include "script/script.h"
#include "script/thread.h"
#include "security/plugins/scriptvariableverifyplugin.h"
#include "security/ragesecplugin.h"
#include "security/ragesecenginetasks.h"
#include "security/obfuscatedtypes.h"

using namespace rage;

u32 sm_scriptVariableVerifyTrashValue = 0;

#if RAGE_SEC_TASK_SCRIPT_VARIABLE_VERIFY

#define SCRIPT_VARIABLE_VERIFY_POLLING_MS	10*1000

atMap<void*, sysObfuscated<int>> sm_scriptVariableShadowCopies;
sysCriticalSectionToken sm_shadowCopyLock;

#if !__FINAL
void RegisterScriptVariable(void* scriptVariable)
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

void UnregisterScriptVariable(void* scriptVariable)
{


	SYS_CS_SYNC(sm_shadowCopyLock);
	if(sm_scriptVariableShadowCopies.Access(scriptVariable))
	{
		sm_scriptVariableShadowCopies.Delete(scriptVariable);
		RageSecDebugf3("0x%08X - Unregistering Script Variable at address 0x%016p", SCRIPT_VARIABLE_VERIFY_ID, &scriptVariable);
	}
	else
	{
		RageSecDebugf3("0x%08X - Unable to find Script Variable to unregister at address 0x%016p", SCRIPT_VARIABLE_VERIFY_ID, &scriptVariable);
	}
	return;
}

void ForceCheckScriptVariables()
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

void ForceUnloadScriptVariables()
{
	SYS_CS_SYNC(sm_shadowCopyLock);
	RageSecDebugf3("0x%08X - Force unloading scriptVariableShadowCopies", SCRIPT_VARIABLE_VERIFY_ID);
	sm_scriptVariableShadowCopies.Reset();
}
#endif

bool ScriptVariableVerifyPlugin_Create()
{
	RageSecDebugf3("0x%08X - ScriptVariableVerify Create", SCRIPT_VARIABLE_VERIFY_ID);
	//@@: location SCRIPTVARIABLEVERIFY_CREATE
	sm_scriptVariableVerifyTrashValue += sysTimer::GetSystemMsTime();
	return true;
}

bool ScriptVariableVerifyPlugin_Work()
{
	RageSecDebugf3("0x%08X - ScriptVariableVerify Work", SCRIPT_VARIABLE_VERIFY_ID);
	bool isTamperDetected = false;
	{
		SYS_CS_SYNC(sm_shadowCopyLock);
		//@@: location SCRIPTVARIABLEVERIFY_WORK_ENTER
		sm_scriptVariableVerifyTrashValue -= sysTimer::GetSystemMsTime();
		//@@: range SCRIPTVARIABLEVERIFY_WORK_MAIN {
		if(sm_scriptVariableShadowCopies.GetNumUsed())
		{
			for(auto it = sm_scriptVariableShadowCopies.CreateIterator(); !it.AtEnd(); it.Next())
			{
				if(*(int*)(it.GetKey()) != it.GetData().Get())
				{
					RageSecDebugf3("0x%08X - Script variable tamper detected at address 0x%016p.", SCRIPT_VARIABLE_VERIFY_ID, it.GetKey());
					RageSecAssertf(false, "0x%08X - Script variable value does not match the value of the shadow copy stored by ScriptVariableVerifyPlugin. "
						"Please create a B* on the Code Security team with the SCL that's causing this assert.", SCRIPT_VARIABLE_VERIFY_ID);
					isTamperDetected = true;
				}
			}
		}
		//@@: } SCRIPTVARIABLEVERIFY_WORK_MAIN
		sm_scriptVariableVerifyTrashValue *= sysTimer::GetSystemMsTime();
		//@@: location SCRIPTVARIABLEVERIFY_WORK_EXIT
	}
	sm_scriptVariableVerifyTrashValue -= sysTimer::GetSystemMsTime();
	//@@: range SCRIPTVARIABLEVERIFY_WORK_REPORT {
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
	//@@: } SCRIPTVARIABLEVERIFY_WORK_REPORT
	sm_scriptVariableVerifyTrashValue += sysTimer::GetSystemMsTime();
	//@@: location SCRIPTVARIABLEVERIFY_WORK_AFTER_REPORT
	return true;
}

bool ScriptVariableVerifyPlugin_Init()
{
	// Since there's no real initialization here necessary, we can just assign our
	// creation function, our destroy function, and just register the object 
	// accordingly

	RageSecPluginParameters rs;
	RageSecPluginExtendedInformation ei;

	ei.startTime		= sysTimer::GetSystemMsTime();
	ei.frequencyInMs	= SCRIPT_VARIABLE_VERIFY_POLLING_MS;
	rs.version.major	= PLUGIN_VERSION_MAJOR;
	rs.version.minor	= PLUGIN_VERSION_MINOR;
	rs.type				= EXECUTE_PERIODIC;
	rs.extendedInfo		= ei;

	rs.Create			= &ScriptVariableVerifyPlugin_Create;
	rs.Configure		= NULL;
	rs.Destroy			= NULL;
	rs.DoWork			= &ScriptVariableVerifyPlugin_Work;
	rs.OnSuccess		= NULL;
	rs.OnCancel			= NULL;
	rs.OnFailure		= NULL;

	// Register the function
	// TO CREATE A NEW ID, GO TO WWW.RANDOM.ORG/BYTES AND GENERATE 
	// FOUR BYTES TO REPRESENT THE ID.
	rageSecPluginManager::RegisterPluginFunction(&rs, SCRIPT_VARIABLE_VERIFY_ID);

	return true;
}

#endif // RAGE_SEC_TASK_SCRIPT_VARIABLE_VERIFY

#endif // RAGE_SEC