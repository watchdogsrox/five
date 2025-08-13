#include "security/ragesecengine.h"


#if USE_RAGESEC

#include "net/status.h"
#include "net/task.h"
#include "script/script.h"
#include "script/thread.h"
#include "security/plugins/networkmonitorplugin.h"
#include "security/ragesecplugin.h"
#include "security/ragesecenginetasks.h"
#include "stats/StatsInterface.h"


using namespace rage;

#if RAGE_SEC_TASK_NETWORK_MONITOR_CHECK

#if RSG_PC
#pragma warning(disable : 4100)
#pragma warning(disable: 4668)
#include <iphlpapi.h>
#include <ws2tcpip.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#endif

#define NETWORK_MONITOR_POLLING_MS				60*1000*5
#define NETWORK_MONITOR_THRESHOLD_CONNECTIONS			5
#define NETWORK_MONITOR_ID 0x80895EFB
#if RSG_PC
static u32 sm_nmTrashValue = 0;
#endif
bool NetworkMonitorPlugin_Work()
{
#if RSG_PC

	bool isSinglePlayer = 
		(
		!NetworkInterface::IsNetworkOpen() 
		&& StatsInterface::GetStatsPlayerModelIsSingleplayer()
		) 
#if GTA_REPLAY
		|| CTheScripts::GetIsInDirectorMode()
#endif
		|| NetworkInterface::IsInSinglePlayerPrivateSession();

	if(isSinglePlayer)
	{
		gnetDebug3("0x%08X - Looking for active open connections on this PID", NETWORK_MONITOR_ID);
		MIB_TCPTABLE_OWNER_PID pTcpTable;
		DWORD tcpTableSize = sizeof(MIB_TCPTABLE_OWNER_PID);
		//@@: location NETWORKMONITORPLUGIN_WORK_ENTRY
		DWORD pid = GetCurrentProcessId();
		DWORD numCxns = 0;
	
		//@@: range NETWORKMONITORPLUGIN_WORK_BODY {
		if(GetExtendedTcpTable(&pTcpTable, &tcpTableSize, FALSE, AF_INET, TCP_TABLE_OWNER_PID_CONNECTIONS, 0) == NO_ERROR)
		{
			for(DWORD i = 0; i < pTcpTable.dwNumEntries; i++)
			{
				if( (u_long)pTcpTable.table[i].dwOwningPid == pid)
				{
	#if !__NO_OUTPUT

					char szLocalAddr[128];
					struct in_addr IpAddr;
					IpAddr.S_un.S_addr = (u_long)pTcpTable.table[i].dwRemoteAddr;
					InetNtop(AF_INET, &IpAddr, szLocalAddr, 128);
					RageSecDebugf3("%s: Found connection to %s", __FUNCTION__, szLocalAddr);
	#endif
					numCxns++;
				}	
			}
		} 
		else
		{
			gnetDebug3("0x%08X - Call to GetExtendedTcpTable failed.", NETWORK_MONITOR_ID);
		}
		
		if(numCxns > NETWORK_MONITOR_THRESHOLD_CONNECTIONS)
		{
			gnetDebug3("0x%08X - Found %d connections - reporting ", NETWORK_MONITOR_ID, numCxns);
			RageSecPluginGameReactionObject obj;
			obj.type = REACT_TELEMETRY;
			obj.data = 0x9E934679;
			rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
		}
		else
		{
			gnetDebug3("0x%08X - Found %d connections - NOT reporting ", NETWORK_MONITOR_ID, numCxns);
		}
		//@@: } NETWORKMONITORPLUGIN_WORK_BODY

	}
	//@@: location NETWORKMONITORPLUGIN_WORK_EXIT
	sm_nmTrashValue  += sysTimer::GetSystemMsTime();
#endif
    return true;
}

bool NetworkMonitorPlugin_Init()
{
	// Since there's no real initialization here necessary, we can just assign our
	// creation function, our destroy function, and just register the object 
	// accordingly

	RageSecPluginParameters rs;
	RageSecPluginExtendedInformation ei;

	ei.startTime		= sysTimer::GetSystemMsTime();
	ei.frequencyInMs	= NETWORK_MONITOR_POLLING_MS;
	rs.version.major	= PLUGIN_VERSION_MAJOR;
	rs.version.minor	= PLUGIN_VERSION_MINOR;
	rs.type				= EXECUTE_PERIODIC_RANDOM;
	rs.extendedInfo		= ei;

	rs.Create			= NULL;
	rs.Configure		= NULL;
	rs.Destroy			= NULL;
	rs.DoWork			= &NetworkMonitorPlugin_Work;
	rs.OnSuccess		= NULL;
	rs.OnCancel			= NULL;
	rs.OnFailure		= NULL;
	
	// Register the function
	// TO CREATE A NEW ID, GO TO WWW.RANDOM.ORG/BYTES AND GENERATE 
	// FOUR BYTES TO REPRESENT THE ID.
	rageSecPluginManager::RegisterPluginFunction(&rs, NETWORK_MONITOR_ID);
	
	return true;
}

#if RSG_PC
#pragma warning( error : 4100)
#pragma warning(error: 4668)
#endif
#endif // RAGE_SEC_TASK_NETWORK_MONITOR_CHECK

#endif // RAGE_SEC