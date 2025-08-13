#include "security/ragesecengine.h"

#if USE_RAGESEC

#include "file/file_config.h"
#include "net/status.h"
#include "net/task.h"
#include "security/ragesecplugin.h"
#include "security/ragesecenginetasks.h"
#include "security/ragesecgameinterface.h"
#include "security/obfuscatedtypes.h"
#include "script/thread.h"
#include "revolvingcheckerplugin.h"

using namespace rage;

#if RAGE_SEC_TASK_LINK_DATA_REPORTER

#define LINKDATAREPORTER_POLLING_MS 1000*10
#define LINKDATAREPORTER_ID			 0xAC487D9D
const unsigned int LINKDATAREPORTED_MAX_QUEUE =256; 

#define DEBUGGER_CHECK_POLLING_MS 60*1000
RAGE_DEFINE_SUBCHANNEL(net, linkdatareporter, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_linkdatareporter

static u32 sm_ldTrashValue	= 0;
static atQueue<LinkDataReport, LINKDATAREPORTED_MAX_QUEUE> sm_ldQueue;
void LinkDataReporterPlugin_Report(LinkDataReport report)
{
	bool isMultiplayer = NetworkInterface::IsAnySessionActive() 
		&& NetworkInterface::IsGameInProgress() 
		&& !NetworkInterface::IsInSinglePlayerPrivateSession();
	if(isMultiplayer) {sm_ldQueue.Push(report);}
}
bool LinkDataReporterPlugin_Create()
{
	gnetDebug3("0x%08X - Created", LINKDATAREPORTER_ID);	
	sm_ldTrashValue += sysTimer::GetSystemMsTime();
	//sm_rcSelector.Set(fwRandom::GetRandomNumber16());
	return true;
}

bool LinkDataReporterPlugin_Configure()
{
	gnetDebug3("0x%08X - Configure", LINKDATAREPORTER_ID);
	sm_ldTrashValue -= sysTimer::GetSystemMsTime();
	return true;
}


bool LinkDataReporterPlugin_Work()
{
	gnetDebug3("0x%08X - Work", LINKDATAREPORTER_ID);
	//@@: location LINKDATAREPORTERPLUGIN_WORK_ENTRY
	sm_ldTrashValue	-= sysTimer::GetSystemMsTime();
	
	// The max-count handles the scenario where the queue is being written to as 
	// its being processed here.
	int maxCount = 0;
	//@@: range LINKDATAREPORTERPLUGIN_WORK_BODY {
	while(!sm_ldQueue.IsEmpty() && maxCount!=LINKDATAREPORTED_MAX_QUEUE)
	{
		
		LinkDataReport report = sm_ldQueue.Pop();
		if(report.Category == LinkDataReporterCategories::LDRC_PLAYERRESET)
		{
			RageSecPluginGameReactionObject obj(
				REACT_TELEMETRY, 
				LINKDATAREPORTER_ID, 
				fwRandom::GetRandomNumber(),
				report.Data,
				report.Category);

			gnetAssertf(false, "0x%08X - Player Reset Flags inconsistent. If this wasn't driven by RAG, please create a B* on Amir Soofi", LINKDATAREPORTER_ID);
			
			rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
			obj.type = REACT_GAMESERVER;
			rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
		}
		maxCount++;
	}
	//@@: location LINKDATAREPORTERPLUGIN_WORK_MOD_TIME
	sm_ldTrashValue	%= sysTimer::GetSystemMsTime();
	RAGE_SEC_POP_REACTION
	//@@: } LINKDATAREPORTERPLUGIN_WORK_BODY

	return true;
}

bool LinkDataReporterPlugin_OnSuccess()
{
	gnetDebug3("0x%08X - OnSuccess", LINKDATAREPORTER_ID);
	// Make this different enough so the compiler doesn't collapse this with another function
	//@@: location LINKDATAREPORTERPLUGIN_ONSUCCESS
	sm_ldTrashValue	   += sysTimer::GetSystemMsTime() - fwRandom::GetRandomNumber();
	return true;
}
bool LinkDataReporterPlugin_OnFailure()
{
	gnetDebug3("0x%08X - OnFailure", LINKDATAREPORTER_ID);
	// Make this different enough so the compiler doesn't collapse this with another function
	//@@: location LINKDATAREPORTERPLUGIN_ONFAILURE
	sm_ldTrashValue	   *= sysTimer::GetSystemMsTime() - fwRandom::GetRandomNumber();
	return true;
}

bool LinkDataReporterPlugin_Init()
{
	// Since there's no real initialization here necessary, we can just assign our
	// creation function, our destroy function, and just register the object 
	// accordingly

	RageSecPluginParameters rs;
	RageSecPluginExtendedInformation ei;
	ei.startTime		= sysTimer::GetSystemMsTime();
	ei.frequencyInMs	= LINKDATAREPORTER_POLLING_MS;
	rs.version.major	= PLUGIN_VERSION_MAJOR;
	rs.version.minor	= PLUGIN_VERSION_MINOR;
	rs.type				= EXECUTE_PERIODIC;
	rs.extendedInfo		= ei;

	rs.Create			= &LinkDataReporterPlugin_Create;
	rs.Configure		= &LinkDataReporterPlugin_Configure;
	rs.Destroy			= NULL;
	rs.DoWork			= &LinkDataReporterPlugin_Work;
	rs.OnSuccess		= &LinkDataReporterPlugin_OnSuccess;
	rs.OnCancel			= NULL;
	rs.OnFailure		= &LinkDataReporterPlugin_OnFailure;

	// Register the function
	// TO CREATE A NEW ID, GO TO WWW.RANDOM.ORG/BYTES AND GENERATE 
	// FOUR BYTES TO REPRESENT THE ID.
	rageSecPluginManager::RegisterPluginFunction(&rs,LINKDATAREPORTER_ID);
	return true;
}

#endif // RAGE_SEC_TASK_REVOLVING_CHECKER

#endif // RAGE_SEC