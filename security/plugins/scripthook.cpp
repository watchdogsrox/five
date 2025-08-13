#include "security/ragesecengine.h"

#if USE_RAGESEC
#include "net/status.h"
#include "net/task.h"
#include "security/plugins/scripthook.h"
#include "security/ragesecplugin.h"
#include "security/ragesecenginetasks.h"
#include "system/bootmgr.h"

using namespace rage;

#if RAGE_SEC_TASK_SCRIPTHOOK
#include "script/thread.h"
#if !__NO_OUTPUT
#define SCRIPTHOOK_POLLING_MS       3*60*1000
#else
#define SCRIPTHOOK_POLLING_MS       30*1000
#endif
#define SCRIPTHOOK_ID               0x5329359D
static bool sm_handledSp = false;
static bool sm_handledMp = false;
static unsigned int sm_shTrashValue	= 0;

static sysObfuscated<DWORD64> sm_baseAddress;
static sysObfuscated<DWORD64> sm_sizeOfImage;
atMap<int, int> sm_runtimeRsp;
static sysObfuscated<DWORD64> sm_calledOutOfBounds;

void ScriptHookPlugin_CheckCallingFunction(void* addr)
{
	s64 *castedAddr = (s64*)addr;

	if(*castedAddr < sm_baseAddress.Get() || (*castedAddr >= (sm_baseAddress.Get()+sm_sizeOfImage.Get())))
	{
		// 
		sm_calledOutOfBounds.Set(fwRandom::GetRandomNumber());
	}
}

bool ScriptHookPlugin_Configure()
{
	MODULEINFO modinfo = { NULL, };
	//@@: range SCRIPTHOOKPLUGIN_CONFIGURE_BODY {
	GetModuleInformation( GetCurrentProcess(), GetModuleHandle(NULL), &modinfo, sizeof( modinfo ) );
	sm_baseAddress.Set((DWORD64)modinfo.lpBaseOfDll);
	sm_sizeOfImage.Set((DWORD64)modinfo.SizeOfImage);
	//@@: } SCRIPTHOOKPLUGIN_CONFIGURE_BODY  
	return true;
}

bool ScriptHookPlugin_Work()
{
	bool isMultiplayer = NetworkInterface::IsAnySessionActive() 
		&& NetworkInterface::IsGameInProgress() 
		&& !NetworkInterface::IsInSinglePlayerPrivateSession();
	//@@: location SCRIPTHOOKPLUGIN_WORK_ENTRY
	if(sm_handledSp && !isMultiplayer)
		return true;
	if(sm_handledMp && isMultiplayer)
		return true;

	//@@: range SCRIPTHOOKPLUGIN_WORK_DETECTED_BODY {
	bool isBusted = false;
	atArray<scrThread*> &threads = scrThread::GetScriptThreads();
	// Iterate over the threads (in case they set just one of the many).
	for(int i = 0; i < threads.GetCount(); i++)
	{
		u64* vtablePtr = (u64*)((u64*)threads[i])[0];
		vtablePtr+=2;
		if(*vtablePtr < sm_baseAddress.Get() || (*vtablePtr >= (sm_baseAddress.Get()+sm_sizeOfImage.Get())))
		{
			isBusted = true;
		}
		vtablePtr+=1;
		if(*vtablePtr < sm_baseAddress.Get() || (*vtablePtr >= (sm_baseAddress.Get()+sm_sizeOfImage.Get())))
		{
			isBusted = true;
		}
	}

	bool manualIsMultiplayer =  CNetwork::IsNetworkSessionValid() && 
		NetworkInterface::IsGameInProgress() &&
		(CNetwork::GetNetworkSession().IsSessionActive() 
		|| CNetwork::GetNetworkSession().IsTransitionActive()
		|| CNetwork::GetNetworkSession().IsTransitionMatchmaking()
		|| CNetwork::GetNetworkSession().IsRunningDetailQuery());

	isMultiplayer|=manualIsMultiplayer;

	if(isBusted)
	{
		RageSecVerifyf(false, "Firing detection of ScriptHook. If you aren't security testing, this is likely a false positive. Please create a B* on Amir Soofi");
		if(isMultiplayer && !sm_handledMp)
		{
			gnetDebug3("0x%08X - Reporting MP", SCRIPTHOOK_ID);
			RageSecPluginGameReactionObject obj(
				REACT_TELEMETRY, 
				SCRIPTHOOK_ID, 
				0x0239643F,
				0xE30518ED,
				0xCD7DA152);
			rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
			obj.type = REACT_GAMESERVER;
			rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
			//@@: location SCRIPTHOOKPLUGIN_WORK_SET_MP_HANDLED
			sm_handledMp = true;
		}
		else if(!sm_handledSp)
		{
			gnetDebug3("0x%08X - Reporting SP", SCRIPTHOOK_ID);
			RageSecPluginGameReactionObject obj(
				REACT_TELEMETRY, 
				SCRIPTHOOK_ID, 
				0x2554FD27,
				0x2554FD27,
				0xA7736615);
			rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
			obj.type = REACT_SET_VIDEO_MODDED_CONTENT;
			rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
			obj.type = REACT_GAMESERVER;
			rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
			sm_handledSp = true;
		}
	}
	// V2 of our script detection... 
	if(isMultiplayer && sm_runtimeRsp.GetNumUsed() > 1 )
	{
		gnetAssertf(false, "0x%08X - Scripts Running from unauthorized call chain.");
		gnetDebug3("0x%08X - Scripts Running from unauthorized call chain.", SCRIPTHOOK_ID);
		RageSecPluginGameReactionObject obj(
			REACT_TELEMETRY, 
			SCRIPTHOOK_ID, 
			0x91D8D61,
			0x35C8BB2,
			0xF51220D);
		rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
		obj.type = REACT_P2P_REPORT;
		rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
		obj.type = REACT_GAMESERVER;
		rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
	}

	// V3 of our script detection...
	if(isMultiplayer && sm_calledOutOfBounds.Get() > 0)
	{
		gnetAssertf(false, "0x%08X - Scripts Running from unauthorized call chain - B.");
		gnetDebug3("0x%08X - Scripts Running from unauthorized call chain - B.", SCRIPTHOOK_ID);
		RageSecPluginGameReactionObject obj(
			REACT_TELEMETRY, 
			SCRIPTHOOK_ID, 
			0x54C6E5B4,
			0x30A96D0C,
			0x11B26B99);
		rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
		obj.type = REACT_GAMESERVER;
		rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
	}
	//@@: } SCRIPTHOOKPLUGIN_WORK_DETECTED_BODY 
    return true;
}


bool ScriptHookPlugin_OnSuccess()
{
	gnetDebug3("0x%08X - OnSuccess", SCRIPTHOOK_ID);
	// Make this different enough so the compiler doesn't collapse this with another function
	//@@: location SCRIPTHOOKPLUGIN_ONSUCCESS
	sm_shTrashValue	   -= sysTimer::GetSystemMsTime() + fwRandom::GetRandomNumber();
	return true;
}

bool ScriptHookPlugin_OnFailure()
{
	gnetDebug3("0x%08X - OnFailure", SCRIPTHOOK_ID);
	// Make this different enough so the compiler doesn't collapse this with another function
	//@@: location SCRIPTHOOKPLUGIN_ONFAILURE
	sm_shTrashValue	   *= sysTimer::GetSystemMsTime() - fwRandom::GetRandomNumber();
	return true;
}

bool ScriptHookPlugin_Init()
{
	// Since there's no real initialization here necessary, we can just assign our
	// creation function, our destroy function, and just register the object 
	// accordingly

	RageSecPluginParameters rs;
	RageSecPluginExtendedInformation ei;

	ei.startTime		= sysTimer::GetSystemMsTime();
	ei.frequencyInMs	= SCRIPTHOOK_POLLING_MS;
	rs.version.major	= PLUGIN_VERSION_MAJOR;
	rs.version.minor	= PLUGIN_VERSION_MINOR;
	rs.type				= EXECUTE_PERIODIC;
	rs.extendedInfo		= ei;

	rs.Create			= NULL;
	rs.Configure		= &ScriptHookPlugin_Configure;
	rs.Destroy			= NULL;
	rs.DoWork			= &ScriptHookPlugin_Work;
	rs.OnSuccess		= &ScriptHookPlugin_OnSuccess;
	rs.OnCancel			= NULL;
	rs.OnFailure		= &ScriptHookPlugin_OnFailure;
	
	// Register the function
	rageSecPluginManager::RegisterPluginFunction(&rs, SCRIPTHOOK_ID);
	
	return true;
}

#endif // RAGE_SEC_TASK_SCRIPTHOOK_CHECK

#endif // RAGE_SEC