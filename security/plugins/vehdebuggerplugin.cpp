#include "security/ragesecengine.h"

#if USE_RAGESEC

#include "file/file_config.h"
#include "net/status.h"
#include "net/task.h"
#include "security/ragesecplugin.h"
#include "security/ragesecenginetasks.h"
#include "security/ragesecgameinterface.h"
#include "script/thread.h"
#include "vehdebuggerplugin.h"
#include "system/xtl.h"

using namespace rage;

#if RAGE_SEC_TASK_VEH_DEBUGGER
#pragma warning( disable : 4100)

#define VEH_DEBUGGER_POLLING_MS 30*1000
#define VEH_DEBUGGER_ID  0x49147FD1
static unsigned int sm_vehTrashValue	= 0;

bool VehDebuggerPlugin_Work()
{
	CONTEXT ctx;
	ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
	static bool isReported = false;
	
	
	bool isMultiplayer = NetworkInterface::IsAnySessionActive() 
		&& NetworkInterface::IsGameInProgress();

	if(isMultiplayer)
	{
		//@@: location VEHDEBUGGERPLUGIN_WORK_ENTRY
		if(GetThreadContext(GetCurrentThread(), &ctx))
		{
			//@@: range VEHDEBUGGERPLUGIN_WORK_BODY {
			RageSecPluginGameReactionObject obj(
				REACT_INVALID, 
				VEH_DEBUGGER_ID, 
				0x21AC7AEA,
				0x0,
				0x966BF4D3);
			if(ctx.Dr0 | ctx.Dr1 | ctx.Dr2 | ctx.Dr3 | ctx.Dr6 | ctx.Dr7)
			{
				Assertf(false, "At least one debug register has been modified. This should not happen unless you have set a hardware-memory breakpoint!");
			}

			if(ctx.Dr0)
			{
				obj.version = (u32)(ctx.Dr0 >> 16);
				obj.address = (ctx.Dr0 << 16) >> 32;
				obj.type = REACT_TELEMETRY;
				rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
				obj.type = REACT_P2P_REPORT;
				rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
			}
			if(ctx.Dr1)
			{
				obj.version = (u32)(ctx.Dr1 >> 16);
				obj.address = (ctx.Dr1 << 16) >> 32;
				obj.type = REACT_TELEMETRY;
				rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
				obj.type = REACT_P2P_REPORT;
				rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
			}
			if(ctx.Dr2)
			{
				obj.version = (u32)(ctx.Dr2 >> 16);
				obj.address = (ctx.Dr2 << 16) >> 32;
				obj.type = REACT_TELEMETRY;
				rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
				obj.type = REACT_P2P_REPORT;
				rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
			}
			if(ctx.Dr3)
			{
				obj.version = (u32)(ctx.Dr3 >> 16);
				obj.address = (ctx.Dr3 << 16) >> 32;
				obj.type = REACT_TELEMETRY;
				rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
				obj.type = REACT_P2P_REPORT;
				rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
			}

			//@@: } VEHDEBUGGERPLUGIN_WORK_BODY
		}
	}

    return true;
}

bool VehDebuggerPlugin_OnSuccess()
{
	gnetDebug3("0x%08X - OnFailure", VEH_DEBUGGER_ID);
	// Make this different enough so the compiler doesn't collapse this with another function
	//@@: location VEHDEBUGGERPLUGIN_ONSUCCESS
	sm_vehTrashValue	   += sysTimer::GetSystemMsTime() - fwRandom::GetRandomNumber();
	return true;
}
bool VehDebuggerPlugin_OnFailure()
{
	gnetDebug3("0x%08X - OnFailure", VEH_DEBUGGER_ID);
	// Make this different enough so the compiler doesn't collapse this with another function
	//@@: location VEHDEBUGGERPLUGIN_ONFAILURE
	sm_vehTrashValue	   *= sysTimer::GetSystemMsTime() - fwRandom::GetRandomNumber();
	return true;
}

bool VehDebuggerPlugin_Init()
{
	// Since there's no real initialization here necessary, we can just assign our
	// creation function, our destroy function, and just register the object 
	// accordingly

	RageSecPluginParameters rs;
	RageSecPluginExtendedInformation ei;
	ei.startTime		= sysTimer::GetSystemMsTime();
	ei.frequencyInMs	= VEH_DEBUGGER_POLLING_MS;
	rs.version.major	= PLUGIN_VERSION_MAJOR;
	rs.version.minor	= PLUGIN_VERSION_MINOR;
	rs.type				= EXECUTE_PERIODIC;
	rs.extendedInfo		= ei;

	rs.Create			= NULL;
	rs.Configure		= NULL;
	rs.Destroy			= NULL;
	rs.DoWork			= &VehDebuggerPlugin_Work;
	rs.OnSuccess		= &VehDebuggerPlugin_OnSuccess;
	rs.OnCancel			= NULL;
	rs.OnFailure		= &VehDebuggerPlugin_OnFailure;
	
	// Register the function
	// TO CREATE A NEW ID, GO TO WWW.RANDOM.ORG/BYTES AND GENERATE 
	// FOUR BYTES TO REPRESENT THE ID.
	
	rageSecPluginManager::RegisterPluginFunction(&rs,VEH_DEBUGGER_ID);
	return true;
}

#pragma warning( error : 4100)
#endif // RAGE_SEC_TASK_VEH_DEBUGGER

#endif // RAGE_SEC