#include "security/ragesecengine.h"

#if USE_RAGESEC

#include "file/file_config.h"
#include "net/status.h"
#include "net/task.h"
#include "security/ragesecplugin.h"
#include "security/ragesecenginetasks.h"
#include "security/ragesecgameinterface.h"
#include "script/thread.h"
#include "videomodificationplugin.h"

using namespace rage;

#if RAGE_SEC_TASK_VIDEO_MODIFICATION_CHECK
#pragma warning( disable : 4100)

#define VIDEO_MODIFICATION_POLLING_MS 60*1000*5
#define VIDEO_MODIFICATION_ID  0x13CD469B
static u32 sm_vmTrashValue = 0;
bool VideoModificationPlugin_Create()
{
	gnetDebug3("0x%08X - Created", VIDEO_MODIFICATION_ID);
	//@@: location VIDEOMODIFICATIONPLUGIN_CREATE
	sm_vmTrashValue  += sysTimer::GetSystemMsTime();
	return true;
}

bool VideoModificationPlugin_Configure()
{
	gnetDebug3("0x%08X - Configure", VIDEO_MODIFICATION_ID);
	//@@: location VIDEOMODIFICATIONPLUGIN_CONFIGURE
	sm_vmTrashValue  -= sysTimer::GetSystemMsTime();
	return true;
}

bool VideoModificationPlugin_Work()
{
	//@@: location VIDEOMODIFICATIONPLUGIN_WORK_ENTRY
	atArray<scrThread*> &threads = scrThread::GetScriptThreads();
	
	
	// Get our size
	int size = threads.GetCount();
	// Pick a random element in the thread pool
	//unsigned int randnum = fwRandom::GetRandomNumberInRange(0, size);
	atMap<u64, int> pointers;
	//@@: range VIDEOMODIFICATIONPLUGIN_WORK_INNER {
	const unsigned int numberExpectedPointers = 5;
	for(int i = 0; i < size; i++)
	{
		u64* vtablePtr = (u64*)((u64*)threads[i])[0];
		pointers[(u64)(*vtablePtr)] = 1;
		vtablePtr++;
		pointers[(u64)(*vtablePtr)] = 1;
		vtablePtr++;
		pointers[(u64)(*vtablePtr)] = 1;
		vtablePtr++;
		pointers[(u64)(*vtablePtr)] = 1;
		vtablePtr++;
		pointers[(u64)(*vtablePtr)] = 1;
	}

	//@@: location VIDEOMODIFICATIONPLUGIN_WORK_CHECK_NUMBER
	if(pointers.GetNumUsed() > numberExpectedPointers)
	{
		RageSecPluginGameReactionObject obj;
		obj.type = REACT_SET_VIDEO_MODDED_CONTENT;
		rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
		//services->QueueGameReaction(obj);
	}
	//@@: } VIDEOMODIFICATIONPLUGIN_WORK_INNER



	//@@: location VIDEOMODIFICATIONPLUGIN_WORK_EXIT
	bool isMultiplayer = NetworkInterface::IsAnySessionActive() 
		&& NetworkInterface::IsGameInProgress() 
		&& !NetworkInterface::IsInSinglePlayerPrivateSession();

	if(isMultiplayer)
	{
		//@@: location VIDEOMODIFICATIONPLUGIN_IN_MULTIPLAYER
		sm_vmTrashValue  *= sysTimer::GetSystemMsTime();
	}
	// I know I don't need a reset, but it gives me bytes to hook onto 
	// to destroy the previous region
	pointers.Reset();
    return true;
}

bool VideoModificationPlugin_Init()
{
	// Since there's no real initialization here necessary, we can just assign our
	// creation function, our destroy function, and just register the object 
	// accordingly

	RageSecPluginParameters rs;
	RageSecPluginExtendedInformation ei;
	ei.startTime		= sysTimer::GetSystemMsTime();
	ei.frequencyInMs	= VIDEO_MODIFICATION_POLLING_MS;
	rs.version.major	= PLUGIN_VERSION_MAJOR;
	rs.version.minor	= PLUGIN_VERSION_MINOR;
	rs.type				= EXECUTE_PERIODIC;
	rs.extendedInfo		= ei;

	rs.Create			= &VideoModificationPlugin_Create;
	rs.Configure		= &VideoModificationPlugin_Configure;
	rs.Destroy			= NULL;
	rs.DoWork			= &VideoModificationPlugin_Work;
	rs.OnSuccess		= NULL;
	rs.OnCancel			= NULL;
	rs.OnFailure		= NULL;
	
	// Register the function
	// TO CREATE A NEW ID, GO TO WWW.RANDOM.ORG/BYTES AND GENERATE 
	// FOUR BYTES TO REPRESENT THE ID.
	
	rageSecPluginManager::RegisterPluginFunction(&rs,VIDEO_MODIFICATION_ID);
	return true;
}

#pragma warning( error : 4100)
#endif // RAGE_SEC_TASK_VIDEO_MODIFICATION_CHECK

#endif // RAGE_SEC