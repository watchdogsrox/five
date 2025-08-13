#include "security/ragesecengine.h"

#if USE_RAGESEC

#include "net/status.h"
#include "net/task.h"
#include "script/script.h"
#include "script/thread.h"
#include "security/plugins/playernamemonitorplugin.h"
#include "security/ragesecplugin.h"
#include "security/ragesecenginetasks.h"
#include "stats/StatsInterface.h"
#include "Peds\ped.h"
#include "Peds\PlayerInfo.h"
#include "rline\rlgamerinfo.h "
#include "security/ragesechashutility.h"

using namespace rage;

#if RAGE_SEC_TASK_PLAYERNAME_MONITOR_CHECK

#define PLAYERNAME_MONITOR_POLLING_MS	60*1000
#define PLAYERNAME_MONITOR_ID			0x83063454

#if RSG_PC
static u32 sm_pmTrashValue = 0;
#endif

static sysObfuscated<u32> nameHash;
static sysObfuscated<u32> displayNameHash;

bool PlayerNameMonitorPlugin_Create()
{
#if RSG_PC
	//@@: location PLAYERNAMEMONITOR_CREATE_START
	sm_pmTrashValue -= (sysTimer::GetSystemMsTime() + fwRandom::GetRandomNumber());
	RageSecDebugf3("0x%08X - PlayerName Monitor Create", PLAYERNAME_MONITOR_ID);
	//@@: range PLAYERNAMEMONITOR_CREATE_BODY {
#endif
	bool flag = true;
	CPed *player = FindPlayerPed();
	if(!player)
	{
		RageSecDebugf3("0x%08X - Unable to find player Ped pointer", PLAYERNAME_MONITOR_ID);
		flag = false;
	}
	CPlayerInfo *playerInfo = player->GetPlayerInfo();
	if(!playerInfo)
	{
		RageSecDebugf3("0x%08X - Unable to find Player Info pointer", PLAYERNAME_MONITOR_ID);
		flag = false;
	}

	rlGamerInfo gamerInfo = playerInfo->m_GamerInfo;
	nameHash.Set(crcHashString(gamerInfo.GetName()));
	displayNameHash.Set(crcHashString(gamerInfo.GetDisplayName()));
#if RSG_PC
	//@@: } PLAYERNAMEMONITOR_CREATE_BODY
	sm_pmTrashValue *= (sysTimer::GetSystemMsTime() - fwRandom::GetRandomNumber());
	if(sm_pmTrashValue > sysTimer::GetSystemMsTime())
	{
		sm_pmTrashValue %= sysTimer::GetSystemMsTime();
	}
	//@@: location PLAYERNAMEMONITOR_CREATE_EXIT
#endif
	return flag;
}

bool PlayerNameMonitorPlugin_Work()
{
#if RSG_PC
	//@@: location PLAYERNAMEMONITOR_WORK_START
	sm_pmTrashValue += (sysTimer::GetSystemMsTime() - fwRandom::GetRandomNumber());
	RageSecDebugf3("0x%08X - PlayerName Monitor Work", PLAYERNAME_MONITOR_ID);
	//@@: range PLAYERNAMEMONITOR_WORK_BODY {
#endif
	CPed *player = FindPlayerPed();
	CPlayerInfo *playerInfo = player->GetPlayerInfo();
	rlGamerInfo gamerInfo = playerInfo->m_GamerInfo;
	u32 newNameHash = crcHashString(gamerInfo.GetName());
	u32 newDisplayNameHash = crcHashString(gamerInfo.GetDisplayName());
	bool flag = false;
	if(newNameHash != nameHash.Get())
	{
		RageSecDebugf3("0x%08X - Hash of new name does not match old hash!", PLAYERNAME_MONITOR_ID);
		flag = true;
	}
	if(newDisplayNameHash != displayNameHash.Get())
	{
		RageSecDebugf3("0x%08X - Hash of new display name does not match old hash!", PLAYERNAME_MONITOR_ID);
		flag = true;
	}

	if(flag)
	{
		RageSecPluginGameReactionObject obj(
			REACT_GAMESERVER,
			PLAYERNAME_MONITOR_ID,
			fwRandom::GetRandomNumber(),
			fwRandom::GetRandomNumber(),
			0x69EBBED5
		);
		rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
	}
#if RSG_PC
	//@@: } PLAYERNAMEMONITOR_WORK_BODY
	sm_pmTrashValue /= (sysTimer::GetSystemMsTime() + fwRandom::GetRandomNumber());
	if(sm_pmTrashValue > sysTimer::GetSystemMsTime())
	{
		sm_pmTrashValue %= sysTimer::GetSystemMsTime();
	}
	//@@: location PLAYERNAMEMONITOR_WORK_EXIT
#endif
	return true;
	
}

bool PlayerNameMonitorPlugin_Init()
{
	// Since there's no real initialization here necessary, we can just assign our
	// creation function, our destroy function, and just register the object 
	// accordingly

	RageSecPluginParameters rs;
	RageSecPluginExtendedInformation ei;

	ei.startTime		= sysTimer::GetSystemMsTime();
	ei.frequencyInMs	= PLAYERNAME_MONITOR_POLLING_MS;
	rs.version.major	= PLUGIN_VERSION_MAJOR;
	rs.version.minor	= PLUGIN_VERSION_MINOR;
	rs.type				= EXECUTE_PERIODIC_RANDOM;
	rs.extendedInfo		= ei;

	rs.Create			= &PlayerNameMonitorPlugin_Create;
	rs.Configure		= NULL;
	rs.Destroy			= NULL;
	rs.DoWork			= &PlayerNameMonitorPlugin_Work;
	rs.OnSuccess		= NULL;
	rs.OnCancel			= NULL;
	rs.OnFailure		= NULL;
	
	// Register the function
	// TO CREATE A NEW ID, GO TO WWW.RANDOM.ORG/BYTES AND GENERATE 
	// FOUR BYTES TO REPRESENT THE ID.
	rageSecPluginManager::RegisterPluginFunction(&rs, PLAYERNAME_MONITOR_ID);

	return true;
}

#endif // RAGE_SEC_TASK_PLAYERNAME_MONITOR_CHECK

#endif // RAGE_SEC