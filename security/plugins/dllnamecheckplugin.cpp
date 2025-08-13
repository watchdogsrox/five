#include "security/ragesecengine.h"

#if USE_RAGESEC

#if RAGE_SEC_TASK_DLLNAMECHECK

#include <wchar.h>

//Rage headers
#include "string/stringhash.h"

//Framework headers
#include "security/ragesecwinapi.h"
#include "system/threadtype.h"

//Game headers
#include "Network/Live/NetworkTelemetry.h"
#include "Network/NetworkInterface.h"
#include "Network/Events/NetworkEventTypes.h"
#include "Network/General/NetworkAssetVerifier.h"
#include "Network/Sessions/NetworkSession.h"
#include "script/script.h"
#include "system/InfoState.h"
#include "system/memorycheck.h"
#include "streaming/defragmentation.h"
#include "stats/StatsInterface.h"

// Security headers
#include "net/status.h"
#include "net/task.h"
#include "security/ragesecplugin.h"
#include "security/ragesecenginetasks.h"
#include "script/thread.h"
#include "security/plugins/dllnamecheckplugin.h"
#include "security/ragesecgameinterface.h"
#include "security/ragesechashutility.h"

using namespace rage;

#define DLLNAME_POLLING_MS					15*1000
#define DLLNAME_ID							0xFBC63F17

static u32 sm_MemoryCheckIndex = 0;
static u32 sm_DllNameTrashValue = 0;

bool DllNamePlugin_Create()
{
	RageSecDebugf3("0x%08X - Created", DLLNAME_ID);
	//@@: location DLLNAMEPLUGIN_CREATE
	sm_DllNameTrashValue += sysTimer::GetSystemMsTime() - fwRandom::GetRandomNumber();
	return true;
}

bool DllNamePlugin_Configure()
{
	RageSecDebugf3("0x%08X - Configure", DLLNAME_ID);
	//@@: location DLLNAMEPLUGIN_CONFIGURE
	sm_DllNameTrashValue -= sysTimer::GetSystemMsTime() + fwRandom::GetRandomNumber();
	return true;
}

bool DllNamePlugin_OnSuccess()
{
	RageSecDebugf3("0x%08X - OnSuccess", DLLNAME_ID);
	//@@: location DLLNAMEPLUGIN_ONSUCCESS
	sm_DllNameTrashValue *= sysTimer::GetSystemMsTime() - fwRandom::GetRandomNumber();
	return true;
}

bool DllNamePlugin_OnFailure()
{
	RageSecDebugf3("0x%08X - OnFailure", DLLNAME_ID);
	//@@: location DLLNAMEPLUGIN_ONFAILURE
	sm_DllNameTrashValue %= sysTimer::GetSystemMsTime() + fwRandom::GetRandomNumber();
	return true;
}

__forceinline bool DllNamePlugin_VerifySupportModules()
{
	if(NetworkInterface::IsGameInProgress())
	{
		// Check if NtDll is flagged as compromised
		if(NtDll::HasBeenHacked())
		{
			RageSecPluginGameReactionObject obj(REACT_GAMESERVER, DLLNAME_ID, 0x668FF11A, 0x84B0395B, 0x1FD96845);
			rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
			return false;
		}

		if(Kernel32::HasBeenHacked())
		{
			RageSecPluginGameReactionObject obj(REACT_GAMESERVER, DLLNAME_ID, 0x668FF11A, 0x84B0395B, 0x6DDB9C21);
			rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
			return false;
		}
	}
	return true;
}

__forceinline bool DllNamePlugin_VerifyTunableLoad()
{
	if(!Tunables::GetInstance().HaveTunablesLoaded())
	{
		RageSecDebugf3("0x%08X - Tunables have not been loaded", DLLNAME_ID);
		return false;
	}
	return true;
}

__forceinline bool DllNamePlugin_VerifyMemoryCheck(MemoryCheck *memoryCheck)
{
	// This is a dirty hack. Below, we set the processed flag with an OR operator
	// which was causing this tamper metric to incorrectly send.
	// And since that flag is well outside my range, I'm just going to && this
	// accordingly, so that we still get the correct XOR check.
	if( memoryCheck->GetXorCrc() !=  
		(memoryCheck->GetVersionAndType()	^ 
		memoryCheck->GetAddressStart()		^ 
		memoryCheck->GetSize()				^
		memoryCheck->GetFlags()	^ 
		memoryCheck->GetValue()))
	{
		return false; 
	}
	return true;
}

__forceinline void DllNamePlugin_QueueReactions(RageSecPluginGameReactionObject* obj, u32 flags)
{
	if (flags & MemoryCheck::FLAG_Telemetry)		
	{ 
		RageSecDebugf3("0x%08X - Queuing Telem", DLLNAME_ID);			
		obj->type = REACT_TELEMETRY; 
		rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(*obj); 
	}
	if (flags & MemoryCheck::FLAG_Report)
	{
		RageSecDebugf3("0x%08X - Queuing P2P", DLLNAME_ID);				
		obj->type = REACT_P2P_REPORT;
		rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(*obj);
	}
	if (flags & MemoryCheck::FLAG_KickRandomly)
	{
		RageSecDebugf3("0x%08X - Queuing Random Kick", DLLNAME_ID);		
		obj->type = REACT_KICK_RANDOMLY;
		rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(*obj);
	}
	if (flags & MemoryCheck::FLAG_NoMatchmake)
	{
		RageSecDebugf3("0x%08X - Queuing MatchMake Bust", DLLNAME_ID);	
		obj->type = REACT_DESTROY_MATCHMAKING;
		rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(*obj);
	}
	if (flags & MemoryCheck::FLAG_Crash)
	{ 
		RageSecDebugf3("0x%08X - Invoking pgStreamer Crash", DLLNAME_ID);	
		RageSecInduceStreamerCrash();
	}
	if (flags & MemoryCheck::FLAG_Gameserver)
	{
		RageSecDebugf3("0x%08X - Queuing Gameserver Report", DLLNAME_ID);	
		obj->type = REACT_GAMESERVER;
		rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(*obj);
	}
	return;
}

bool DllNamePlugin_Work()
{
	//@@: location DLLNAMEPLUGIN_WORK_SHORT_ENTRY
	RageSecDebugf3("0x%08X - Work", DLLNAME_ID);
	sm_DllNameTrashValue /= sysTimer::GetSystemMsTime() + fwRandom::GetRandomNumber();
	//@@: range DLLNAMEPLUGIN_WORK_SHORT {
	if(!DllNamePlugin_VerifySupportModules()) {return true;} 
	if(!DllNamePlugin_VerifyTunableLoad()) {return true;}

	int numChecks = Tunables::GetInstance().GetNumMemoryChecks();
	if(numChecks == 0)
	{
		RageSecDebugf3("0x%08X - No MemoryChecks loaded by tunables.", DLLNAME_ID);
		return true;
	}

	MemoryCheck memoryCheck;
	Tunables::GetInstance().GetMemoryCheck(sm_MemoryCheckIndex, memoryCheck);
	RageSecDebugf3("0x%08X - Fetching Memory Check %d", DLLNAME_ID, sm_MemoryCheckIndex);

	if(!DllNamePlugin_VerifyMemoryCheck(&memoryCheck))
	{
		RageSecDebugf3("0x%08X - Memory Check %d doesn't match the XOR'ing it would expect", DLLNAME_ID, sm_MemoryCheckIndex);
		RageSecPluginGameReactionObject obj(REACT_GAMESERVER, DLLNAME_ID, 0x158BFC8F, 0x8C74089F, 0x466AEB2D);
		rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
		++sm_MemoryCheckIndex;
		return true;
	}
	//@@: } DLLNAMEPLUGIN_WORK_SHORT

	//@@: location DLLNAMEPLUGIN_WORK_BODY_ENTRY
	RageSecDebugf3("0x%08X - All verifications complete. Entering main body.", DLLNAME_ID);
	sm_DllNameTrashValue -= sysTimer::GetSystemMsTime() + fwRandom::GetRandomNumber();
	//@@: range DLLNAMEPLUGIN_WORK_BODY {
	if(memoryCheck.getCheckType() == MemoryCheck::CHECK_DllName)
	{
		u32 xorValue = memoryCheck.GetValue() ^ MemoryCheck::GetMagicXorValue();
		u32 m_hash = memoryCheck.GetValue();
		HANDLE hProcess = Kernel32::GetCurrentProcess();
		HMODULE hModules[1024];
		DWORD dwBytesNeeded;
		char modName[MAX_PATH+1];

		if(Kernel32::EnumProcessModules(hProcess, hModules, sizeof(hModules), &dwBytesNeeded))
		{
			for (int i = 0; i < dwBytesNeeded / sizeof(HMODULE); ++i)
			{
				if(Kernel32::GetModuleFileNameFn(hModules[i], modName, sizeof(modName)/sizeof(char)))
				{
					char *filename = strrchr(modName, '\\');
					if(filename)
					{
						++filename;
						u32 hash = crcHashString(filename);
						if(hash == m_hash)
						{
							RageSecDebugf3("0x%08X - Hash of DLL name matches malicious DLL name!", DLLNAME_ID);
							RageSecPluginGameReactionObject obj(
								REACT_INVALID,
								DLLNAME_ID,
								memoryCheck.GetVersionAndType() ^ xorValue,
								memoryCheck.GetAddressStart() ^ xorValue,
								0xBC1EAA96
							);
							u32 flags = memoryCheck.GetFlags() ^ xorValue;
							DllNamePlugin_QueueReactions(&obj, flags);
							break;
						}
					}
				}
				memset(modName, 0, sizeof(modName));
			}
		}
	}
	//@@: } DLLNAMEPLUGIN_WORK_BODY 
	//@@: location DLLNAMEPLUGIN_WORK_NEXT_ENTRY
	RageSecDebugf3("0x%08X - DLL name plugin iterating to next memory check.", DLLNAME_ID);
	sm_DllNameTrashValue %= sysTimer::GetSystemMsTime() - fwRandom::GetRandomNumber();
	//@@: range DLLNAMEPLUGIN_WORK_NEXT {
	++sm_MemoryCheckIndex;
	if(sm_MemoryCheckIndex >= numChecks)
	{
		sm_MemoryCheckIndex -= numChecks;
	}
	//@@: } DLLNAMEPLUGIN_WORK_NEXT

	//@@: location DLLNAMEPLUGIN_WORK_EXIT
	sm_DllNameTrashValue %= sysTimer::GetSystemMsTime() + fwRandom::GetRandomNumber();

	return true;
}

bool DllNamePlugin_Init()
{
	// Since there's no real initialization here necessary, we can just assign our
	// creation function, our destroy function, and just register the object 
	// accordingly

	RageSecPluginParameters rs;
	RageSecPluginExtendedInformation ei;

	ei.startTime		= sysTimer::GetSystemMsTime();
	ei.frequencyInMs	= DLLNAME_POLLING_MS;
	rs.version.major	= PLUGIN_VERSION_MAJOR;
	rs.version.minor	= PLUGIN_VERSION_MINOR;
	rs.type				= EXECUTE_PERIODIC;
	rs.extendedInfo		= ei;

	rs.Create			= &DllNamePlugin_Create;
	rs.Configure		= &DllNamePlugin_Configure;
	rs.Destroy			= NULL;
	rs.DoWork			= &DllNamePlugin_Work;
	rs.OnSuccess		= &DllNamePlugin_OnSuccess;
	rs.OnCancel			= NULL;
	rs.OnFailure		= &DllNamePlugin_OnFailure;

	// Register the function
	// TO CREATE A NEW ID, GO TO WWW.RANDOM.ORG/BYTES AND GENERATE 
	// FOUR BYTES TO REPRESENT THE ID.
	rageSecPluginManager::RegisterPluginFunction(&rs, DLLNAME_ID);

	return true;
}


#endif // RAGE_SEC_TASK_DLLNAMECHECK

#endif // RAGE_SEC
