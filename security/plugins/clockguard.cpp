#include "security/ragesecengine.h"

#if USE_RAGESEC

#include "file/file_config.h"
#include "net/status.h"
#include "net/task.h"
#include "security/ragesecplugin.h"
#include "security/ragesecenginetasks.h"
#include "security/ragesecgameinterface.h"
#include "script/thread.h"
#include "clockguard.h"
#include "system/xtl.h"
#include "rline/rltelemetry.h"
using namespace rage;

#if RAGE_SEC_TASK_CLOCK_GUARD
#include <winsock2.h>
#include <ws2tcpip.h>
// Need to link with Ws2_32.lib
#pragma comment(lib, "ws2_32.lib")
RAGE_DEFINE_SUBCHANNEL(net, clockguard, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_clockguard

#pragma warning( disable : 4100)

#if !__NO_OUTPUT
#define CLOCK_GUARD_POLLING_MS 5*1000;
#else
#define CLOCK_GUARD_POLLING_MS 60*1000*5
#endif

#define CLOCK_GUARD_ID  0x4A729ECB 
static unsigned int sm_cgTrashValue	= 0;

// Forward declare?
bool ClockGuardPlugin_Work()
{
	//@@: location CLOCKGUARDPLUGIN_WORK_ENTRY
	bool isMultiplayer = 
		NetworkInterface::IsAnySessionActive() 
		&& NetworkInterface::IsGameInProgress();


	//@@: range CLOCKGUARDPLUGIN_WORK_BODY {
	// Empty, for now, so I can fill this on a different CL
	time_t t = time(0);   // get time now
	struct tm * now = localtime( & t );
	int minuteMod = now->tm_min % 60;

#if !__NO_OUTPUT
	// Make testing easier for logging builds
	minuteMod = now->tm_sec % 60;
#endif
	if(minuteMod > 50)
	{
		gnetDebug3("0x%08X - 50 %s %d", CLOCK_GUARD_ID, NetworkInterface::IsAnySessionActive() ? "true" : "false",  netInterface::GetNumPhysicalPlayers() );
		//@@: location CLOCKGUARDPLUGIN_WORK_50
		if(	NetworkInterface::IsAnySessionActive() == false 
			&& (netInterface::GetNumPhysicalPlayers() > 0))
		{
			//@@: range CLOCKGUARDPLUGIN_WORK_50_BODY {
			gnetAssertf(false, "0x%08X - NetworkInterface::IsAnySessionActive() tampered with.", CLOCK_GUARD_ID);	
			RageSecPluginGameReactionObject obj(
				REACT_GAMESERVER, 
				CLOCK_GUARD_ID, 
				0xD1A33666,
				0x3D1FD94E,
				0xB5150D96);
			rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
			sm_cgTrashValue	   *= sysTimer::GetSystemMsTime() - fwRandom::GetRandomNumber();
			//@@: } CLOCKGUARDPLUGIN_WORK_50_BODY
		}
	}
	else if(minuteMod > 40)
	{
		gnetDebug3("0x%08X - 40", CLOCK_GUARD_ID);
		//@@: location CLOCKGUARDPLUGIN_WORK_40
		if( NetworkInterface::IsGameInProgress())
		{
			//@@: range CLOCKGUARDPLUGIN_WORK_40_BODY {
			if(isMultiplayer && NetworkInterface::GetLocalPlayer()->GetPlayerInfo()->MaxArmour > 250)
			{
				gnetAssertf(false, "0x%08X - Armor is above allowed threshold (250).", CLOCK_GUARD_ID);	
				RageSecPluginGameReactionObject obj(
					REACT_GAMESERVER, 
					CLOCK_GUARD_ID, 
					0x185E1ABF,
					NetworkInterface::GetLocalPlayer()->GetPlayerInfo()->MaxArmour,
					0x882C3BF1);
				rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
				obj.type = REACT_TELEMETRY; rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
				sm_cgTrashValue	   += sysTimer::GetSystemMsTime() ^ fwRandom::GetRandomNumber();
			}
			//@@: } CLOCKGUARDPLUGIN_WORK_40_BODY
		}
	}
	else if(minuteMod > 30)
	{
		gnetDebug3("0x%08X - 30", CLOCK_GUARD_ID);
		//@@: location CLOCKGUARDPLUGIN_WORK_30
		const unsigned int buffLen = 256;
		char telemBuff[buffLen] = {0};
		rlRos::GetServiceHost("/Telemetry.asmx/SubmitCompressed", telemBuff, buffLen);
		//@@: range CLOCKGUARDPLUGIN_WORK_30_BODY {
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0){
			gnetDebug3("0x%08X - Failed to WSAStartup", CLOCK_GUARD_ID);
		}

		hostent *telemRemoteHost = gethostbyname(telemBuff);
		// Declare some local variables
		bool isOnAWS = true;
		bool connectedTelemHost = false;
		int aliasCount = 0;
		int addrListCount = 0;

		SOCKET Socket;
		SOCKADDR_IN SockAddr;

		if(telemRemoteHost == NULL) { 
			gnetDebug3("0x%08X - Telemetry Remote Host is NULL %s %d", CLOCK_GUARD_ID, telemBuff, WSAGetLastError());
			// This is yielding false positives, so let's set to true to avoid that...
			connectedTelemHost = true;
		}
		else
		{
			// First, we're going to check to see if we have a non-aws host
			isOnAWS = strncmp(telemBuff, telemRemoteHost->h_name, buffLen) != 0;
			
			// Now lets look for our set of aliases. We should have at least 1
			// alias, as the prod.*.rockstargames.com is the alias for the 
			// AWS address
			for(char **pAlias = telemRemoteHost->h_aliases;*pAlias != 0; pAlias++)
			{
				aliasCount++;
			}
			gnetDebug3("0x%08X - %d aliases found", CLOCK_GUARD_ID, aliasCount);

			// Now lets count our address list, and see what alternative
			// IPs we've gotten back. We should have more than one.
			while (telemRemoteHost->h_addr_list[addrListCount] != 0) {
				addrListCount++;
			}

			gnetDebug3("0x%08X - %d addresses found", CLOCK_GUARD_ID, addrListCount);

			Socket=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
			SockAddr.sin_port=htons(80);
			SockAddr.sin_family=telemRemoteHost->h_addrtype;
			SockAddr.sin_addr.s_addr = *((unsigned long*)telemRemoteHost->h_addr);

			if(connect(Socket,(SOCKADDR*)(&SockAddr),sizeof(SockAddr)) != 0)
			{
				gnetDebug3("0x%08X - Telemetry Remote Host %s failed basic connection", CLOCK_GUARD_ID, telemBuff);
			}
			else
			{
				connectedTelemHost = true;
			}

			closesocket(Socket);

			gnetDebug3(
				"0x%08X - %-16s %s | %-16s %s | %-16s %s | %-16s %s",
				CLOCK_GUARD_ID,
				"AWS hostname?",
				isOnAWS ? "true" : "false",
				"1 Alias?",
				aliasCount == 1 ? "true" : "false",
				">1 IP?",
				addrListCount >= 1 ? "true" : "false",
				">Valid Connect?",
				connectedTelemHost? "true" : "false");

		}

		char condcBuff[buffLen] = {0};
		rlRos::GetServiceHost("conductor", condcBuff, buffLen);
		hostent *condcRemoteHost = gethostbyname(condcBuff);
		bool connectedConductor = false;
		if(condcRemoteHost == NULL) { 
			gnetDebug3("0x%08X - Conductor Remote Host is NULL %s %d", CLOCK_GUARD_ID, condcBuff, WSAGetLastError());
			// This is yielding false positives, so let's set to true to avoid that...
			connectedConductor = true;
		}
		else
		{
			Socket=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
			SockAddr.sin_port=htons(80);
			SockAddr.sin_family=condcRemoteHost->h_addrtype;
			SockAddr.sin_addr.s_addr = *((unsigned long*)condcRemoteHost->h_addr);

			if(connect(Socket,(SOCKADDR*)(&SockAddr),sizeof(SockAddr)) != 0)
			{
				gnetDebug3("0x%08X - Conductor Host %s failed basic connection", CLOCK_GUARD_ID, telemBuff);

			}
			else
			{
				connectedConductor = true;
			}
			closesocket(Socket);
		}

		WSACleanup();

		// Check each, and report appropriately
		if(	(!isOnAWS || aliasCount!=1 || addrListCount ==1)
			&& isMultiplayer
			&& connectedConductor
			&& !connectedTelemHost)
		{
			gnetDebug3(
				"0x%08X - %-16s %s | %-16s %s | %-16s %s | %-16s %s",
				CLOCK_GUARD_ID,
				"AWS hostname?",
				isOnAWS ? "true" : "false",
				"1 Alias?",
				aliasCount == 1 ? "true" : "false",
				">1 IP?",
				addrListCount >= 1 ? "true" : "false",
				">Valid Connect?",
				connectedTelemHost? "true" : "false");

			gnetAssertf(false, "0x%08X - Failed connectivity test A to %s", CLOCK_GUARD_ID, telemBuff);	
			RageSecPluginGameReactionObject obj(
				REACT_INVALID, 
				CLOCK_GUARD_ID, 
				0xD961F45B,
				0x83383652,
				0x2DDA3A2C);
			obj.type = REACT_GAMESERVER; rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
		}

		sm_cgTrashValue	   *= sysTimer::GetSystemMsTime() + fwRandom::GetRandomNumber();
		//@@: } CLOCKGUARDPLUGIN_WORK_30_BODY
	}
	else if(minuteMod > 20)
	{
		gnetDebug3("0x%08X - 20", CLOCK_GUARD_ID);
		//@@: location CLOCKGUARDPLUGIN_WORK_20
		const unsigned int buffLen = 256;
		char telemBuff[buffLen] = {0};
		rlRos::GetServiceHost("/Telemetry.asmx/SubmitCompressed", telemBuff, buffLen);
		char condcBuff[buffLen] = {0};
		rlRos::GetServiceHost("conductor", condcBuff, buffLen);
		addrinfo *result = NULL;
		struct addrinfo hints;
		//@@: range CLOCKGUARDPLUGIN_WORK_20_BODY {
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0){
			gnetDebug3("0x%08X - Failed to WSAStartup", CLOCK_GUARD_ID);
		}
		DWORD dwRetval = getaddrinfo(condcBuff, 0, &hints, &result);
		bool conductorHostValid = dwRetval==0;

		if(conductorHostValid == false) { gnetDebug3("0x%08X - Failed to call GetAddrInfo on %s %d", CLOCK_GUARD_ID, condcBuff, WSAGetLastError());}

		dwRetval = getaddrinfo(telemBuff, 0, &hints, &result);
		bool telemHostValid = dwRetval==0;
		if(telemHostValid == false) { gnetDebug3("0x%08X - Failed to call GetAddrInfo on %s %d", CLOCK_GUARD_ID, telemBuff, WSAGetLastError());}

		int telemAddrCount = 0;
		for (addrinfo *ptr = result; ptr != NULL; ptr = ptr->ai_next) {
			telemAddrCount++;
		}
		
		gnetDebug3("0x%08X - Found %d addresses on GetAddrInfo to %s", CLOCK_GUARD_ID, telemAddrCount, condcBuff);

		WSACleanup();
		if(conductorHostValid && isMultiplayer && (telemAddrCount<=1 || telemHostValid == false))
		{
			gnetAssertf(false, "0x%08X - Failed connectivity test B to %s", CLOCK_GUARD_ID, telemBuff);	
			RageSecPluginGameReactionObject obj(
				REACT_INVALID, 
				CLOCK_GUARD_ID, 
				0x84D6C1C5,
				0x49DAE3DE,
				0x61C8D725);
			obj.type = REACT_GAMESERVER; rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
		}

		sm_cgTrashValue	   += sysTimer::GetSystemMsTime() * fwRandom::GetRandomNumber();
		//@@: } CLOCKGUARDPLUGIN_WORK_20_BODY
	}
	else if(minuteMod > 10)
	{
		gnetDebug3("0x%08X - 10", CLOCK_GUARD_ID);
		//@@: location CLOCKGUARDPLUGIN_WORK_10
		bool manualIsMultiplayer =  CNetwork::IsNetworkSessionValid() && 
			 NetworkInterface::IsGameInProgress() &&
				(CNetwork::GetNetworkSession().IsSessionActive() 
				|| CNetwork::GetNetworkSession().IsTransitionActive()
				|| CNetwork::GetNetworkSession().IsTransitionMatchmaking()
				|| CNetwork::GetNetworkSession().IsRunningDetailQuery());

		//@@: range CLOCKGUARDPLUGIN_WORK_10_BODY {
		if(manualIsMultiplayer!=isMultiplayer)
		{
			//@@: location CLOCKGUARDPLUGIN_WORK_10_BODY_GPU_CRASH
			gnetAssertf(false, "0x%08X - Failed manual check for IsMultiplayer()", CLOCK_GUARD_ID);	
			RageSecPluginGameReactionObject obj(
				REACT_INVALID, 
				CLOCK_GUARD_ID, 
				0xDF97C343,
				0xF580C6C2,
				0x74351203);
			obj.type = REACT_GAMESERVER;
			rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);

			RageSecInduceGpuCrash();
		}
		//@@: } CLOCKGUARDPLUGIN_WORK_10_BODY
	}
	else if(minuteMod > 0)
	{
		//@@: range CLOCKGUARDPLUGIN_WORK_00_BODY {
		gnetDebug3("0x%08X - 00", CLOCK_GUARD_ID);
		//@@: location CLOCKGUARDPLUGIN_WORK_00
		sm_cgTrashValue	   ^= sysTimer::GetSystemMsTime() - fwRandom::GetRandomNumber();
		//@@: } CLOCKGUARDPLUGIN_WORK_00_BODY
		sm_cgTrashValue	   *= sysTimer::GetSystemMsTime() + fwRandom::GetRandomNumber();
	}
	
    return true;
    //@@: } CLOCKGUARDPLUGIN_WORK_BODY
}

bool ClockGuardPlugin_OnSuccess()
{
	gnetDebug3("0x%08X - OnSuccess", CLOCK_GUARD_ID);
	// Make this different enough so the compiler doesn't collapse this with another function
	//@@: location CLOCKGUARDPLUGIN_ONSUCCESS
	sm_cgTrashValue	   += sysTimer::GetSystemMsTime() - fwRandom::GetRandomNumber();
	return true;
}
bool ClockGuardPlugin_OnFailure()
{
	gnetDebug3("0x%08X - OnFailure", CLOCK_GUARD_ID);
	// Make this different enough so the compiler doesn't collapse this with another function
	//@@: location CLOCKGUARDPLUGIN_ONFAILURE
	sm_cgTrashValue	   *= sysTimer::GetSystemMsTime() - fwRandom::GetRandomNumber();
	return true;
}

bool ClockGuardPlugin_Init()
{
	// Since there's no real initialization here necessary, we can just assign our
	// creation function, our destroy function, and just register the object 
	// accordingly

	RageSecPluginParameters rs;
	RageSecPluginExtendedInformation ei;
	ei.startTime		= sysTimer::GetSystemMsTime();
	ei.frequencyInMs	= CLOCK_GUARD_POLLING_MS;
	rs.version.major	= PLUGIN_VERSION_MAJOR;
	rs.version.minor	= PLUGIN_VERSION_MINOR;
	rs.type				= EXECUTE_PERIODIC;
	rs.extendedInfo		= ei;

	rs.Create			= NULL;
	rs.Configure		= NULL;
	rs.Destroy			= NULL;
	rs.DoWork			= &ClockGuardPlugin_Work;
	rs.OnSuccess		= &ClockGuardPlugin_OnSuccess;
	rs.OnCancel			= NULL;
	rs.OnFailure		= &ClockGuardPlugin_OnFailure;
	
	// Register the function
	// TO CREATE A NEW ID, GO TO WWW.RANDOM.ORG/BYTES AND GENERATE 
	// FOUR BYTES TO REPRESENT THE ID.
	
	rageSecPluginManager::RegisterPluginFunction(&rs,CLOCK_GUARD_ID);
	return true;
}

#pragma warning( error : 4100)
#endif // RAGE_SEC_TASK_CLOCK_GUARD

#endif // RAGE_SEC