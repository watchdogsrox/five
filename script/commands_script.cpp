//
// filename:	commands_script.cpp
// description:	Commands for running, killing, streaming script programs
//

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "script/script.h"
#include "script/thread.h"
#include "script/wrapper.h"
#include "system/param.h"
// Game headers
#include "control/replay/ReplayRecording.h"
#include "event/EventGroup.h"
#include "event/EventNetwork.h"
#include "game/BackgroundScripts/BackgroundScripts.h"
#include "network/Events/NetworkEventTypes.h"
#include "script/Handlers/GameScriptResources.h"
#include "script/streamedscripts.h"
#include "script/script_brains.h"
#include "script/script_debug.h"
#include "script/script_helper.h"
#include "script/script_hud.h"
#include "network/NetworkInterface.h"
#include "frontend/loadingscreens.h"
#include "Network/Live/NetworkMetrics.h"
#include "Network/Live/NetworkTelemetry.h"
#include "security/plugins/scripthook.h"


// --- Defines ------------------------------------------------------------------

enum SCRIPT_EVENT_QUEUES
{
	SCRIPT_EVENT_QUEUE_AI,
	SCRIPT_EVENT_QUEUE_NETWORK,
	SCRIPT_EVENT_QUEUE_ERRORS,
	NUM_SCRIPT_EVENT_QUEUES
};

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// --- Globals ------------------------------------------------------------------

// --- Static Globals -----------------------------------------------------------

// --- Static class members -----------------------------------------------------

// --- Code ---------------------------------------------------------------------
PARAM(missionnum, "The number of the mission you wish to have invoked when the game boots.");
PARAM(missionname, "The name of the mission you wish to have invoked when the game boots.");
PARAM(scripttestname, "The name of the script test you wish to have invoked when the game boots.");

#if !__FINAL
PARAM(spewtriggerscriptevent, "The name of the script test you wish to have invoked when the game boots.");
#endif

SCRIPT_OPTIMISATIONS()

namespace script_commands
{


#if !__FINAL
static int CommandGetCommandLineMissionNum(void)
{
	int missionNum = -1;
	if(PARAM_missionnum.Get(missionNum))
	{
		return missionNum;
	}
	else
	{
		return -1;
	}
}

static const char* CommandGetCommandLineMissionName(void)
{
	const char* missionName = NULL;
	if(PARAM_missionname.Get(missionName))
	{
		return missionName;
	}
	else
	{
		return NULL;
	}
}

static const char* CommandGetCommandLineScriptTestName(void)
{
	const char* scriptTestName = NULL;
	if(PARAM_scripttestname.Get(scriptTestName))
	{
		return scriptTestName;
	}
	else
	{
		return NULL;
	}
}
#endif // !__FINAL

static void RequestScript(const char* pName)
{
	if (scrProgram::IsCompiledAndResident(pName)) {
		scriptDisplayf("Compiled script '%s' requested.",pName);
		return;
	}

	s32 id = g_StreamedScripts.FindSlot(pName).Get();
	if(scriptVerifyf(id != -1, "%s - REQUEST_SCRIPT - Script %s doesn't exist", CTheScripts::GetCurrentScriptName(), pName))
	{
#if !__FINAL
		if (CScriptDebug::GetDisplayScriptRequests())
		{
			scriptDisplayf("%s called REQUEST_SCRIPT for %s", CTheScripts::GetCurrentScriptName(), pName);
		}
#endif	//	!__FINAL
//		g_StreamedScripts.StreamingRequest(id, STRFLAG_PRIORITY_LOAD|STRFLAG_MISSION_REQUIRED);
		CScriptResource_StreamedScript Script(id);
		CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResource(Script);
	}
	else
	{
		scriptWarningf("%s - REQUEST_SCRIPT - Script %s doesn't exist", CTheScripts::GetCurrentScriptName(), pName);
	}
}

static void MarkScriptAsNoLongerNeeded(const char* pName)
{
	if (scrProgram::IsCompiledAndResident(pName)) {
		scriptDisplayf("Compiled script '%s' no longer needed.",pName);
		return;
	}
	s32 id = g_StreamedScripts.FindSlot(pName).Get();
	if(scriptVerifyf(id != -1, "%s - SET_SCRIPT_AS_NO_LONGER_NEEDED - Script %s doesn't exist", CTheScripts::GetCurrentScriptName(), pName))
	{
#if !__FINAL
		if (CScriptDebug::GetDisplayScriptRequests())
		{
			scriptDisplayf("%s called SET_SCRIPT_AS_NO_LONGER_NEEDED for %s", CTheScripts::GetCurrentScriptName(), pName);
		}
#endif	//	!__FINAL
//		g_StreamedScripts.ClearRequiredFlag(id, STRFLAG_MISSION_REQUIRED);
		CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_STREAMED_SCRIPT, id);
		//CStreaming::SetMissionDoesntRequireObject(id, g_StreamedScripts.GetStreamingModuleId());
	}
	else
	{
		scriptWarningf("%s - SET_SCRIPT_AS_NO_LONGER_NEEDED - Script %s doesn't exist", CTheScripts::GetCurrentScriptName(), pName);
	}
}	

static bool HasScriptLoaded(const char* pName)
{
	if (scrProgram::IsCompiledAndResident(pName)) {
		scriptDisplayf("Compiled script '%s' is always loaded.",pName);
		return true;
	}
	strLocalIndex id = g_StreamedScripts.FindSlot(pName);
	if(scriptVerifyf(id != -1, "%s:Script doesn't exist", pName))
	{
		return g_StreamedScripts.HasObjectLoaded(id);
	}
	return false;
}

static bool DoesScriptExist(const char* pName)
{
	if (scrProgram::IsCompiledAndResident(pName)) {
		scriptDisplayf("Compiled script '%s' exists.",pName);
		return true;
	}
	const s32 id = g_StreamedScripts.FindSlot(pName).Get();
	return id != -1;
}

static void RequestScriptWithNameHash(s32 hashOfScriptName)
{
	//	I'm not sure what to do with IsCompiledAndResident. It seems to be using a different hash function - scrComputeHash
	//	if (scrProgram::IsCompiledAndResident(pName)) {
	//		scriptDisplayf("Compiled script '%s' requested.",pName);
	//		return;
	//	}

	s32 id = g_StreamedScripts.FindSlotFromHashKey((u32) hashOfScriptName).Get();
	if(scriptVerifyf(id != -1, "%s - REQUEST_SCRIPT_WITH_NAME_HASH - Script with hash %d (unsigned %u) doesn't exist", CTheScripts::GetCurrentScriptName(), hashOfScriptName, (u32) hashOfScriptName))
	{
#if !__FINAL
		if (CScriptDebug::GetDisplayScriptRequests())
		{
			scriptDisplayf("%s called REQUEST_SCRIPT_WITH_NAME_HASH for script with hash %d (unsigned %u)", CTheScripts::GetCurrentScriptName(), hashOfScriptName, (u32) hashOfScriptName);
		}
#endif	//	!__FINAL
//		g_StreamedScripts.StreamingRequest(id, STRFLAG_PRIORITY_LOAD|STRFLAG_MISSION_REQUIRED);
		CScriptResource_StreamedScript Script(id);
		CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResource(Script);
	}
	else
	{
		scriptWarningf("%s - REQUEST_SCRIPT_WITH_NAME_HASH - Script with hash %d (unsigned %u) doesn't exist", CTheScripts::GetCurrentScriptName(), hashOfScriptName, (u32) hashOfScriptName);
	}
}

static void MarkScriptWithNameHashAsNoLongerNeeded(s32 hashOfScriptName)
{
	//	I'm not sure what to do with IsCompiledAndResident. It seems to be using a different hash function - scrComputeHash
	// 	if (scrProgram::IsCompiledAndResident(pName)) {
	// 		scriptDisplayf("Compiled script '%s' no longer needed.",pName);
	// 		return;
	// 	}
	s32 id = g_StreamedScripts.FindSlotFromHashKey((u32) hashOfScriptName).Get();
	if(scriptVerifyf(id != -1, "%s - SET_SCRIPT_WITH_NAME_HASH_AS_NO_LONGER_NEEDED - Script with hash %d (unsigned %u) doesn't exist", CTheScripts::GetCurrentScriptName(), hashOfScriptName, (u32) hashOfScriptName))
	{
#if !__FINAL
		if (CScriptDebug::GetDisplayScriptRequests())
		{
			scriptDisplayf("%s called SET_SCRIPT_WITH_NAME_HASH_AS_NO_LONGER_NEEDED for script with hash %d (unsigned %u)", CTheScripts::GetCurrentScriptName(), hashOfScriptName, (u32) hashOfScriptName);
		}
#endif	//	!__FINAL
//		g_StreamedScripts.ClearRequiredFlag(id, STRFLAG_MISSION_REQUIRED);
		CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_STREAMED_SCRIPT, id);
//		CStreaming::SetMissionDoesntRequireObject(id, g_StreamedScripts.GetStreamingModuleId());
	}
	else
	{
		scriptWarningf("%s - SET_SCRIPT_WITH_NAME_HASH_AS_NO_LONGER_NEEDED - Script with hash %d (unsigned %u) doesn't exist", CTheScripts::GetCurrentScriptName(), hashOfScriptName, (u32) hashOfScriptName);
	}
}

static bool HasScriptWithNameHashLoaded(s32 hashOfScriptName)
{
//	I'm not sure what to do with IsCompiledAndResident. It seems to be using a different hash function - scrComputeHash
// 	if (scrProgram::IsCompiledAndResident(pName)) {
// 		scriptDisplayf("Compiled script '%s' is always loaded.",pName);
// 		return true;
// 	}
	strLocalIndex id = g_StreamedScripts.FindSlotFromHashKey((u32) hashOfScriptName);
	if(scriptVerifyf(id != -1, "Script with hash %d (unsigned %u) doesn't exist", hashOfScriptName, (u32) hashOfScriptName))
	{
		return g_StreamedScripts.HasObjectLoaded(id);
	}
	return false;
}

bool DoesScriptWithNameHashExist(s32 hashOfScriptName)
{
//	I'm not sure what to do with IsCompiledAndResident. It seems to be using a different hash function - scrComputeHash
// 	if (scrProgram::IsCompiledAndResident(pName)) {
// 		scriptDisplayf("Compiled script '%s' exists.",pName);
// 		return true;
// 	}
	const s32 id = g_StreamedScripts.FindSlotFromHashKey((u32) hashOfScriptName).Get();
	return id != -1;
}

static void DestroyThread(s32 threadId)
{
	scrThread::KillThread(static_cast<scrThreadId>(threadId));
}

static bool IsThreadActive(s32 threadId)
{
	scrThread *pThread = scrThread::GetThread(static_cast<scrThreadId>(threadId));
	if ( (pThread == NULL) || (pThread->GetState() == scrThread::ABORTED) )
	{
		return false;
	}
	return true;
}

const char *CommandGetNameOfScriptWithThisId(s32 threadId)
{
	static char s_scriptName[64];
	scrThread *pThread = scrThread::GetThread(static_cast<scrThreadId>(threadId));
	if ( (pThread != NULL) && (pThread->GetState() != scrThread::ABORTED) )
	{
		strncpy(s_scriptName, pThread->GetScriptName(), NELEM(s_scriptName));
	}
	else
	{
		strncpy(s_scriptName, "", NELEM(s_scriptName));
	}

	return s_scriptName;
}


void CommandScriptThreadIteratorReset()
{
	GtaThread::ScriptThreadIteratorReset();
}

s32 CommandScriptThreadIteratorGetNextThreadId()
{
	return GtaThread::ScriptThreadIteratorGetNextThreadId();
}

static int GetIDOfThisThread()
{
	scrThreadId id = CTheScripts::GetCurrentGtaScriptThread()->GetThreadId();
	return ( (int) (size_t) id );
}

void CommandTerminateThisScript()
{
	scrThread::GetActiveThread()->Kill();
}


void AssertForNumberOfThreadsWithHash(const char* ASSERT_ONLY(pErrorMessage), s32 ASSERT_ONLY(NameHash), const char* ASSERT_ONLY(pScriptName), s32 ASSERT_ONLY(StreamedScriptsRefs), s32 ASSERT_ONLY(ProgramRefs))
{
#if __ASSERT
	if (pScriptName == NULL)
	{
		scriptAssertf(0, "%s : %s Script hash = %d StreamedScriptsRefs = %d ProgramRefs = %d", 
			CTheScripts::GetCurrentScriptNameAndProgramCounter(), pErrorMessage, NameHash,
			StreamedScriptsRefs, ProgramRefs);
	}
	else
	{
		scriptAssertf(0, "%s : %s Script name = %s (hash = %d) StreamedScriptsRefs = %d ProgramRefs = %d", 
			CTheScripts::GetCurrentScriptNameAndProgramCounter(), pErrorMessage, pScriptName, NameHash,
			StreamedScriptsRefs, ProgramRefs);
	}
#endif	//	__ASSERT
}

int CommandGetNumberOfThreadsRunningTheScriptWithThisHash(s32 NameHash)
{
	s32 id = g_StreamedScripts.FindSlotFromHashKey((u32) NameHash).Get();

	s32 number_of_active_scripts = 0;

	if(id != -1)
	{
		// catch the situation where the script isn't in memory
		if(g_StreamedScripts.GetNumRefs(strLocalIndex(id)) > 0)
		{
			//	Get the program id
			scrProgramId programid = g_StreamedScripts.GetProgramId(id);

			scrProgram *pProgram = scrProgram::GetProgram(programid);
			if (pProgram)
			{
				number_of_active_scripts = pProgram->GetNumRefs()-1;
			}
		}
	}
	else
	{
		AssertForNumberOfThreadsWithHash("GET_NUMBER_OF_THREADS_RUNNING_THE_SCRIPT_WITH_THIS_HASH - Script doesn't exist", NameHash, NULL, 0, 0);
	}

	return number_of_active_scripts;
}


static const char* CommandGetThisScriptName()
{
	return CTheScripts::GetCurrentGtaScriptThread()->GetScriptName();
}

s32 CommandGetHashOfThisScriptName()
{
	return CTheScripts::GetCurrentGtaScriptThread()->m_HashOfScriptName;
}

bool CommandIsThisScriptAWorldPointScript(int& positionHash)
{
	positionHash = 0;

	if (CTheScripts::GetCurrentGtaScriptThread()->ScriptBrainType == CScriptsForBrains::WORLD_POINT)
	{
		positionHash = static_cast<CGameScriptId&>(CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetScriptId()).GetPositionHash();
		Assert(positionHash != 0);
		return true;
	}

	return false;
}

int  CommandGetNumberOfEvents(int eventQueue)
{
	int numEvents = 0;

	switch (eventQueue)
	{
	case SCRIPT_EVENT_QUEUE_ERRORS:
		numEvents = GetEventScriptErrorsGroup()->GetNumEvents();
		break;
	case SCRIPT_EVENT_QUEUE_AI:
		numEvents = GetEventScriptAIGroup()->GetNumEvents();
		break;
	case SCRIPT_EVENT_QUEUE_NETWORK:
		numEvents = GetEventScriptNetworkGroup()->GetNumEvents();
		break;
	default:
		scriptAssertf(0, "%s: GET_NUMBER_OF_EVENTS - Unrecognised script event queue type",  CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	return numEvents;
}

bool CommandGetEventExists(int eventQueue, int eventType)
{
	bool bEventExists = false;

	switch (eventQueue)
	{
	case SCRIPT_EVENT_QUEUE_ERRORS:
		scriptAssertf(eventType>NUM_NETWORK_EVENTTYPE && eventType<NUM_ERRORS_EVENTTYPE, "%s: GET_EVENT_EXISTS - Event type %d is wrong for this type of queue",  CTheScripts::GetCurrentScriptNameAndProgramCounter(), eventType);
		bEventExists = (GetEventScriptErrorsGroup()->GetEventOfType(eventType) != NULL);
		break;
	case SCRIPT_EVENT_QUEUE_AI:
		scriptAssertf(eventType<NUM_AI_EVENTTYPE, "%s: GET_EVENT_EXISTS - Event type %d is wrong for this type of queue",  CTheScripts::GetCurrentScriptNameAndProgramCounter(), eventType);
		bEventExists = (GetEventScriptAIGroup()->GetEventOfType(eventType) != NULL);
		break;
	case SCRIPT_EVENT_QUEUE_NETWORK:
		scriptAssertf(eventType>NUM_AI_EVENTTYPE && eventType<NUM_NETWORK_EVENTTYPE, "%s: GET_EVENT_EXISTS - Event type %d is wrong for this type of queue",  CTheScripts::GetCurrentScriptNameAndProgramCounter(), eventType);
		bEventExists = (GetEventScriptNetworkGroup()->GetEventOfType(eventType) != NULL);
		break;
	default:
		scriptAssertf(0, "%s: GET_EVENT_EXISTS - Unrecognised script event queue type",  CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	return bEventExists;
}


fwEvent* GetEvent(int eventQueue, int eventIndex, const char* ASSERT_ONLY(commandName))
{
	fwEvent* pEvent = NULL;

	if (scriptVerifyf(eventIndex >= 0 && eventIndex < CommandGetNumberOfEvents(eventQueue), "%s: %s - Invalid index=\"%d\". Valid are from \"0\" to \"%d\".", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName, eventIndex, GetEventScriptNetworkGroup()->GetNumEvents()))
	{
		switch (eventQueue)
		{
		case SCRIPT_EVENT_QUEUE_ERRORS:
			pEvent = GetEventScriptErrorsGroup()->GetEventByIndex(eventIndex);
			break;
		case SCRIPT_EVENT_QUEUE_AI:
			pEvent = GetEventScriptAIGroup()->GetEventByIndex(eventIndex);
			break;
		case SCRIPT_EVENT_QUEUE_NETWORK:
			pEvent = GetEventScriptNetworkGroup()->GetEventByIndex(eventIndex);
			break;
		default:
			scriptAssertf(0, "%s: %s - Unrecognised script event queue type",  CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
		}

		scriptAssertf(pEvent, "%s: %s - Event does not exist",  CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}

	return pEvent;
}

int CommandGetEventAtIndex(int eventQueue, int eventIndex)
{
	int eventType = -1;

	fwEvent* pEvent = GetEvent(eventQueue, eventIndex, "GET_EVENT_AT_INDEX");

	if (pEvent)
	{
		eventType = pEvent->GetEventType();
	}

	return eventType;
}

bool  CommandGetEventData(int eventQueue, int eventIndex, int &AddressTemp, int sizeOfData)
{
	u8* Address = reinterpret_cast< u8* >(&AddressTemp);
	sizeOfData *= sizeof(scrValue); // Script returns size in script words

	fwEvent* pEvent = GetEvent(eventQueue, eventIndex, "GET_EVENT_DATA");

	if (pEvent)
	{
		return pEvent->RetrieveData(Address, sizeOfData);
	}

	return false;
}

void  CommandTriggerScriptEvent(int eventQueue, int& data, int sizeOfData, int playerFlags)
{
#if !__FINAL
	if (NetworkInterface::IsGameInProgress())
		scriptAssertf(0, "%s: TRIGGER_SCRIPT_EVENT - DONT USE THIS COMMAND ANYMORE.",  CTheScripts::GetCurrentScriptNameAndProgramCounter());
#endif //!__FINAL

	sizeOfData *= sizeof(scrValue); // Script returns size in script words

	bool triggerLocally = true;

	if (NetworkInterface::IsGameInProgress())
	{
		if (scriptVerifyf(playerFlags != 0, "%s: TRIGGER_SCRIPT_EVENT - Player flags are 0",  CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			const netPlayer* myPlayer = NetworkInterface::GetLocalPlayer();
			if (AssertVerify(myPlayer))
			{
				PhysicalPlayerIndex playerIndex = myPlayer->GetPhysicalPlayerIndex();
				if (!(playerFlags & (1<<playerIndex)))
				{
					triggerLocally = false;
				}
				else
				{
					playerFlags &= ~(1<<playerIndex);
				}
			}
		}

		SCRIPT_CHECK_CALLING_FUNCTION

		PlayerFlags physicalPlayers = NetworkInterface::GetPlayerMgr().GetRemotePhysicalPlayersBitmask();

		if ((playerFlags & ~physicalPlayers) != 0)
		{
			scriptAssertf(0, "%s: TRIGGER_SCRIPT_EVENT - Some players flagged to receive this event are not active",  CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}

		CScriptedGameEvent::Trigger(static_cast<CGameScriptId&>(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId()), &data, sizeOfData, playerFlags);
	}
	
	if (triggerLocally)
	{
		CEventNetworkScriptEvent scriptEvent((void*)&data, sizeOfData, NetworkInterface::GetLocalPlayer());

		switch (eventQueue)
		{
		case SCRIPT_EVENT_QUEUE_ERRORS:
			GetEventScriptErrorsGroup()->Add(scriptEvent);
			break;
		case SCRIPT_EVENT_QUEUE_AI:
			GetEventScriptAIGroup()->Add(scriptEvent);
			break;
		case SCRIPT_EVENT_QUEUE_NETWORK:
			GetEventScriptNetworkGroup()->Add(scriptEvent);
			break;
		default:
			scriptAssertf(0, "%s: TRIGGER_SCRIPT_EVENT - Unrecognised script event queue type",  CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}

#if !__FINAL && !__NO_OUTPUT

	if (PARAM_spewtriggerscriptevent.Get())
	{
		const scrValue* dataNew = (const scrValue*)&data;
		int size = 0;
		const int eventtype  = dataNew[size].Int; ++size;
		const int fromPlayerIndex = dataNew[size].Int;

		gnetDebug1("%s: TRIGGER_SCRIPT_EVENT - eventtype='%d', playerindex='%d', eventQueue='%d', data='%d', sizeOfData='%d', playerFlags='%d'."
			, CTheScripts::GetCurrentScriptNameAndProgramCounter()
			, eventtype
			, fromPlayerIndex
			, eventQueue
			, data
			, sizeOfData
			, playerFlags);
	}

#endif // !__FINAL && !__NO_OUTPUT

	if (NetworkInterface::IsGameInProgress())
	{
		const rlGamerHandle& fromGH = NetworkInterface::GetLocalGamerHandle();
		if (fromGH.IsValid())
		{
			MetricLAST_VEH metric;
			metric.m_fields[0] = ATSTRINGHASH("TRIGGER_SCRIPT_EVENT_HONEYPOT", 0x5E0AB3D3);

			const scrValue* dataNew = (const scrValue*)&data;
			int size = 0;
			metric.m_fields[1] = dataNew[size].Int; // eventid

			fromGH.ToString(metric.m_FromGH, COUNTOF(metric.m_FromGH));
			fromGH.ToString(metric.m_ToGH, COUNTOF(metric.m_ToGH));

			CNetworkTelemetry::AppendSecurityMetric(metric);
		}
	}
}


int CommandConvertFilenameToInt(const char *pFilename)
{
	char buffer[256];
	fiAssetManager::RemoveExtensionFromPath(buffer,256,pFilename);
	return atoi(buffer);
}

void CommandShutdownLoadingScreen()
{
	CLoadingScreens::Shutdown(SHUTDOWN_CORE);
}

void CommandNoLoadingScreen(bool value)
{
	scriptDisplayf("SET_NO_LOADING_SCREEN(%s)", value?"TRUE":"FALSE");
	#if !__FINAL
		scrThread::PrePrintStackTrace();
	#endif
	CScriptHud::SetStopLoadingScreen(value);
}

bool CommandGetNoLoadingScreen()
{
	return CScriptHud::GetStopLoadingScreen();
}

#if RSG_GEN9
void CommandSetLoadingScreenBlank()
{
	CLoadingScreens::SetForceBlank(true);
}

bool CommandGetLoadingScreenBlank()
{
	return CLoadingScreens::GetForceBlank();
}
#endif // RSG_GEN9

void CommandCommitLoadingScreenSelection()
{
	CLoadingScreens::CommitToMpSp();
}

bool CommandIsLoadingScreenCommitedToMpSp()
{
	return CLoadingScreens::IsCommitedToMpSp();
}


bool CommandBGIsExitFlagSet()
{
	return CTheScripts::GetCurrentGtaScriptThread()->IsExitFlagSet();
}

void CommandBGSetExitFlagResponse()
{
	CTheScripts::GetCurrentGtaScriptThread()->SetExitFlagResponse();
}

void CommandBGStartContextHash( const int crc )
{
	SBackgroundScripts::GetInstance().StartContext(static_cast<u32>(crc));
}

void CommandBGEndContextHash( const int crc )
{
	SBackgroundScripts::GetInstance().EndContext(static_cast<u32>(crc));
}

void CommandBGStartContext( const char* pContext )
{
	SBackgroundScripts::GetInstance().StartContext(
#if __NO_OUTPUT
		atStringHash(pContext)
#else
		pContext
#endif
		);
}

void CommandBGEndContext( const char* pContext )
{
	SBackgroundScripts::GetInstance().EndContext(
#if __NO_OUTPUT
		atStringHash(pContext)
#else
		pContext
#endif
		);
}

bool CommandBGDoesLaunchParamExist(int scriptId, const char* name)
{
	if( const BGScriptInfo* pScript = SBackgroundScripts::GetInstance().GetScriptInfo(scriptId) )
	{
		return pScript->GetLaunchParam(atStringHash(name)) != NULL;
	}

	bgScriptAssertf(false, "Bad BG script ID(%d)", scriptId);
	return false;
}

int CommandBGGetLaunchParamValue(int scriptId, const char* name)
{
	const BGScriptInfo* pScript = SBackgroundScripts::GetInstance().GetScriptInfo(scriptId);

	if(bgScriptVerifyf(pScript, "Bad BG script ID(%d)", scriptId))
	{
		const BGScriptInfo::LaunchParam* pParam = pScript->GetLaunchParam(atStringHash(name));
		if(bgScriptVerifyf(pParam, "Couldn't find param(%s) in object(%s)", name, pScript->GetName()))
		{
			return pParam->GetValueFromData();
		}
	}

	return 0;
}

int CommandBGGetScriptIdFromNameHash(int nameHash)
{
	const int retVal = SBackgroundScripts::GetInstance().GetScriptIDFromHash(static_cast<u32>(nameHash));

	bgScriptAssertf(retVal >= 0, "Bad BG script name Hash(%u)", nameHash);
	return retVal;
}

void CommandWinterIsComing(int eventQueue, int& data, int sizeOfData, int playerFlags)
{
	sizeOfData *= sizeof(scrValue); // Script returns size in script words

	bool triggerLocally = true;

	if (NetworkInterface::IsGameInProgress())
	{
		if (scriptVerifyf(playerFlags != 0, "%s: SEND_TU_SCRIPT_EVENT - Player flags are 0", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			const netPlayer* myPlayer = NetworkInterface::GetLocalPlayer();
			if (AssertVerify(myPlayer))
			{
				PhysicalPlayerIndex playerIndex = myPlayer->GetPhysicalPlayerIndex();
				if (!(playerFlags & (1<<playerIndex)))
				{
					triggerLocally = false;
				}
				else
				{
					playerFlags &= ~(1<<playerIndex);
				}
			}
		}

		PlayerFlags physicalPlayers = NetworkInterface::GetPlayerMgr().GetRemotePhysicalPlayersBitmask();

		if ((playerFlags & ~physicalPlayers) != 0)
		{
			scriptAssertf(0, "%s: SEND_TU_SCRIPT_EVENT - Some players flagged to receive this event are not active", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}

		CScriptedGameEvent::Trigger(static_cast<CGameScriptId&>(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId()), &data, sizeOfData, playerFlags);
	}

	if (triggerLocally)
	{
		CEventNetworkScriptEvent scriptEvent((void*)&data, sizeOfData, NetworkInterface::IsNetworkOpen() ? NetworkInterface::GetLocalPlayer() : nullptr);

		switch (eventQueue)
		{
		case SCRIPT_EVENT_QUEUE_ERRORS:
			GetEventScriptErrorsGroup()->Add(scriptEvent);
			break;
		case SCRIPT_EVENT_QUEUE_AI:
			GetEventScriptAIGroup()->Add(scriptEvent);
			break;
		case SCRIPT_EVENT_QUEUE_NETWORK:
			GetEventScriptNetworkGroup()->Add(scriptEvent);
			break;
		default:
			scriptAssertf(0, "%s: SEND_TU_SCRIPT_EVENT - Unrecognised script event queue type", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}

#if !__FINAL && !__NO_OUTPUT

	if (PARAM_spewtriggerscriptevent.Get())
	{
		const scrValue* dataNew = (const scrValue*)&data;
		int size = 0;
		const int eventtype  = dataNew[size].Int; ++size;
		const int fromPlayerIndex = dataNew[size].Int;

		gnetDebug1("%s: SEND_TU_SCRIPT_EVENT - eventtype='%d', playerindex='%d', eventQueue='%d', data='%d', sizeOfData='%d', playerFlags='%d'."
			, CTheScripts::GetCurrentScriptNameAndProgramCounter()
			, eventtype
			, fromPlayerIndex
			, eventQueue
			, data
			, sizeOfData
			, playerFlags);
	}

#endif // !__FINAL && !__NO_OUTPUT
}

void SetupScriptCommands()
{
	SCR_REGISTER_UNUSED(GET_COMMAND_LINE_MISSION_NUM, 0x9e304cf7, CommandGetCommandLineMissionNum);
	SCR_REGISTER_UNUSED(GET_COMMAND_LINE_MISSION_NAME, 0xf631222f, CommandGetCommandLineMissionName);
	SCR_REGISTER_UNUSED(GET_COMMAND_LINE_SCRIPT_TEST_NAME, 0x3c217762, CommandGetCommandLineScriptTestName);

	SCR_REGISTER_SECURE(REQUEST_SCRIPT,0xaf76a37c80efd1d8, RequestScript);
	SCR_REGISTER_SECURE(SET_SCRIPT_AS_NO_LONGER_NEEDED,0xd195d79715508efa, MarkScriptAsNoLongerNeeded);
	SCR_REGISTER_SECURE(HAS_SCRIPT_LOADED,0xf79f112ce5999680, HasScriptLoaded);
	SCR_REGISTER_SECURE(DOES_SCRIPT_EXIST,0x11adef87bad88f70, DoesScriptExist);

	SCR_REGISTER_SECURE(REQUEST_SCRIPT_WITH_NAME_HASH,0x251c5c1ddc74d01d, RequestScriptWithNameHash);
	SCR_REGISTER_SECURE(SET_SCRIPT_WITH_NAME_HASH_AS_NO_LONGER_NEEDED,0xd48ac12d763bbe7f, MarkScriptWithNameHashAsNoLongerNeeded);
	SCR_REGISTER_SECURE(HAS_SCRIPT_WITH_NAME_HASH_LOADED,0x095032c22dee1587, HasScriptWithNameHashLoaded);
	SCR_REGISTER_SECURE(DOES_SCRIPT_WITH_NAME_HASH_EXIST,0xff09675ec057a8d2, DoesScriptWithNameHashExist);

	SCR_REGISTER_SECURE(TERMINATE_THREAD,0x5b71484d4dac41e5, DestroyThread);
	SCR_REGISTER_SECURE(IS_THREAD_ACTIVE,0x87c0da419f19ff57, IsThreadActive);
	SCR_REGISTER_SECURE(GET_NAME_OF_SCRIPT_WITH_THIS_ID,0xfcc2c8d6f42dbd7e, CommandGetNameOfScriptWithThisId);
	SCR_REGISTER_SECURE(SCRIPT_THREAD_ITERATOR_RESET,0x27053be21ec6a549, CommandScriptThreadIteratorReset);
	SCR_REGISTER_SECURE(SCRIPT_THREAD_ITERATOR_GET_NEXT_THREAD_ID,0x2b33daa8227a8dae, CommandScriptThreadIteratorGetNextThreadId);
	SCR_REGISTER_SECURE(GET_ID_OF_THIS_THREAD,0x69ce66b03b2184eb, GetIDOfThisThread);
	SCR_REGISTER_SECURE(TERMINATE_THIS_THREAD,0xafbdf6a5e54114c1, CommandTerminateThisScript);
	SCR_REGISTER_SECURE(GET_NUMBER_OF_THREADS_RUNNING_THE_SCRIPT_WITH_THIS_HASH,0xb4c854dd86e40fda, CommandGetNumberOfThreadsRunningTheScriptWithThisHash);
	SCR_REGISTER_SECURE(GET_THIS_SCRIPT_NAME,0x05cba41180f5d521, CommandGetThisScriptName);
	SCR_REGISTER_SECURE(GET_HASH_OF_THIS_SCRIPT_NAME,0x1307d54181723a6e, CommandGetHashOfThisScriptName);
	SCR_REGISTER_UNUSED(IS_THIS_SCRIPT_A_WORLD_POINT_SCRIPT,0xe7d485c11e8cd1d7, CommandIsThisScriptAWorldPointScript);


	// --------- Script Events 
	SCR_REGISTER_SECURE(GET_NUMBER_OF_EVENTS,0x9418088815c89d59,  CommandGetNumberOfEvents);
	SCR_REGISTER_SECURE(GET_EVENT_EXISTS,0x32c089fafda9ba2f,  CommandGetEventExists);
	SCR_REGISTER_SECURE(GET_EVENT_AT_INDEX,0xb1d84e0ef6979085,  CommandGetEventAtIndex);
	SCR_REGISTER_SECURE(GET_EVENT_DATA,0x92039f20b184ab4d,  CommandGetEventData);
	SCR_REGISTER_SECURE_HONEYPOT(TRIGGER_SCRIPT_EVENT,0xa29c2ecc2c86354d,  CommandTriggerScriptEvent);

	// --------- Misc
	SCR_REGISTER_UNUSED(CONVERT_FILENAME_TO_INT,0xe60c4631a160725b,  CommandConvertFilenameToInt);

	// --------- Loading Screen Shutdown
	SCR_REGISTER_SECURE(SHUTDOWN_LOADING_SCREEN,0xcdee827e5ce50ca4,  CommandShutdownLoadingScreen);
	SCR_REGISTER_SECURE(SET_NO_LOADING_SCREEN,0x94b5910a9c945688, CommandNoLoadingScreen);
	SCR_REGISTER_SECURE(GET_NO_LOADING_SCREEN,0x1088b18071adccbd, CommandGetNoLoadingScreen);
#if RSG_GEN9
	SCR_REGISTER_UNUSED(SET_LOADING_SCREEN_BLANK, 0x8a519a9f853aba8d, CommandSetLoadingScreenBlank);
	SCR_REGISTER_UNUSED(GET_LOADING_SCREEN_BLANK, 0xf29489825d441421, CommandGetLoadingScreenBlank);
#endif // RSG_GEN9

	// --------- Commit to the players selection of multiplayer or single player on the loading screen.
	SCR_REGISTER_SECURE(COMMIT_TO_LOADINGSCREEN_SELCTION,0xc8fc2af21935c18e, CommandCommitLoadingScreenSelection);
	SCR_REGISTER_UNUSED(IS_LOADINGSCREEN_COMMITTED_TO_SELECTION,0xa93c974ce6ca9e38, CommandIsLoadingScreenCommitedToMpSp );

	// --- Background Scripts -------------------------------------------------------------------
	DLC_SCR_REGISTER_SECURE(BG_IS_EXITFLAG_SET,0x6231d60cba3296d1, CommandBGIsExitFlagSet);
	DLC_SCR_REGISTER_SECURE(BG_SET_EXITFLAG_RESPONSE,0x5fec2fc5ade05110, CommandBGSetExitFlagResponse);
	DLC_SCR_REGISTER_SECURE(BG_START_CONTEXT_HASH,0x3b4d9908a204de37, CommandBGStartContextHash);
	DLC_SCR_REGISTER_SECURE(BG_END_CONTEXT_HASH,0x24b10d7aa3a45b84, CommandBGEndContextHash);
	DLC_SCR_REGISTER_SECURE(BG_START_CONTEXT,0x6eebdda01d438e11, CommandBGStartContext);
	DLC_SCR_REGISTER_SECURE(BG_END_CONTEXT,0xd08d25cbd7b14310, CommandBGEndContext);
	DLC_SCR_REGISTER_SECURE(BG_DOES_LAUNCH_PARAM_EXIST,0x82b35eceebc89fa3, CommandBGDoesLaunchParamExist);
	DLC_SCR_REGISTER_SECURE(BG_GET_LAUNCH_PARAM_VALUE,0xe5538467850e2536, CommandBGGetLaunchParamValue);
	DLC_SCR_REGISTER_SECURE(BG_GET_SCRIPT_ID_FROM_NAME_HASH,0x9da3bf7ed82b02c6, CommandBGGetScriptIdFromNameHash);
	SCR_REGISTER_SECURE_HONEYPOT(SEND_TU_SCRIPT_EVENT,0x2700c00f82c16bf0,  CommandWinterIsComing);

}

};
