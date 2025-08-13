#include "ReplayRecording.h"

#if GTA_REPLAY

#include "Cutscene/CutSceneManagerNew.h"
#include "fwsys/timer.h"
#include "script/gta_thread.h"
#include "script/script.h"
#include "script/thread.h"
#include "ReplayBufferMarker.h"
#include "Replay.h"

#if __BANK
#include "camera/system/CameraManager.h"
#endif // __BANK

bool		ReplayRecordingScriptInterface::sm_bIsStarted = false;
bool		ReplayRecordingScriptInterface::sm_bHasAutoStopped = false;
u32			ReplayRecordingScriptInterface::sm_StartTime = 0;
u8			ReplayRecordingScriptInterface::sm_MarkerImportance = 0;
u32			ReplayRecordingScriptInterface::sm_PrevMissionHash = 0;
REPLAY_MODE	ReplayRecordingScriptInterface::sm_PrevReplayMode = MANUAL;
s32			ReplayRecordingScriptInterface::sm_CurrentMissionOverride = -1;


#if RSG_BANK
atString	ReplayRecordingScriptInterface::sm_StopCallstack;
atString	ReplayRecordingScriptInterface::sm_CancelCallstack;
atString	ReplayRecordingScriptInterface::sm_StartCallstack;
atString*	ReplayRecordingScriptInterface::sm_pAssignCurrentCallstack = NULL;
#endif

void	ReplayRecordingScriptInterface::StartRecording(u8 importance)
{
#if RSG_BANK
	StoreScriptCallstack(sm_StartCallstack);
	replayDebugf2("REPLAY_START_EVENT called callstack: %s", sm_StartCallstack.c_str());
#endif
	
	Assertf(sm_bIsStarted == false, "Trying to start a replay recording when one is already started");
	Assertf(importance >= IMPORTANCE_LOWEST && importance <= IMPORTANCE_HIGHEST, "Trying to start a replay recording with an invalid importance");
	sm_bIsStarted = true;
	sm_bHasAutoStopped = false;
	sm_StartTime = rage::fwTimer::GetReplayTimeInMilliseconds();
	sm_MarkerImportance = importance;

	GtaThread* scriptThread = CTheScripts::GetCurrentGtaScriptThread();
	if (replayVerifyf(scriptThread, "No current GTA Script Thread!"))
	{
		scriptThread->SetNeedsReplayCleanup(true);
	}
}

#if RSG_BANK
void    ReplayRecordingScriptInterface::StoreScriptCallstack(atString& callstack)
{
	callstack.Clear();

	scrThread *pActiveThread = scrThread::GetActiveThread();
	if (pActiveThread)
	{
		sm_pAssignCurrentCallstack = &callstack;
		pActiveThread->PrintStackTrace(AppendCallstack);
	}
	else
	{
		callstack += "No active script thread.";
	}
}

void	ReplayRecordingScriptInterface::AppendCallstack(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	char buffer[1024];
	vsnprintf(buffer, sizeof(buffer), fmt, args);

	*sm_pAssignCurrentCallstack += buffer;
	*sm_pAssignCurrentCallstack += '\n';

	va_end(args);
}
#endif

void	ReplayRecordingScriptInterface::StopRecording()
{
	if (!(sm_bIsStarted == true || (sm_bIsStarted == false && sm_bHasAutoStopped == true)))
	{
#if RSG_BANK
		replayWarningf("Trying to stop a replay recording when one isn't started");

		if (sm_StopCallstack.length() !=0)
		{
			replayWarningf("Previous callstack:\n%s", sm_StopCallstack.c_str());
		}
		else
		{
			replayWarningf("No previous callstack.");
		}

		StoreScriptCallstack(sm_StopCallstack);

		replayWarningf("Current callstack:\n%s", sm_StopCallstack.c_str());
	}
	else
	{
		StoreScriptCallstack(sm_StopCallstack);
		replayDebugf2("STOP_RECORDING called callstack: %s", sm_StopCallstack.c_str());
#endif
	}

	sm_bIsStarted = false;
	sm_bHasAutoStopped = false;
	u32 totalTime = rage::fwTimer::GetReplayTimeInMilliseconds() - sm_StartTime;
	RecordClip(totalTime, 0, sm_MarkerImportance);

	GtaThread* scriptThread = CTheScripts::GetCurrentGtaScriptThread();
	if (replayVerifyf(scriptThread, "No current GTA Script Thread!"))
	{
		scriptThread->SetNeedsReplayCleanup(false);
	}
}

void	ReplayRecordingScriptInterface::CancelRecording()
{
#if RSG_BANK
	StoreScriptCallstack(sm_CancelCallstack);
	replayDebugf2("REPLAY_CANCEL_EVENT called callstack: %s", sm_CancelCallstack.c_str());
#endif

	if( sm_bHasAutoStopped == false )
	{
		Assertf(sm_bIsStarted == true, "Trying to cancel a replay recording when one isn't started");
	}
	
	sm_bIsStarted = false;
	sm_bHasAutoStopped = false;

	GtaThread* scriptThread = CTheScripts::GetCurrentGtaScriptThread();
	if (scriptThread)
	{
		scriptThread->SetNeedsReplayCleanup(false);
	}
}

void	ReplayRecordingScriptInterface::AutoStopRecordingIfNecessary()
{
	// Early out if we're not recording
	if (!sm_bIsStarted)
	{
		return;
	}

	if (GtaThread::GetShouldAutoCleanupReplay())
	{
		replayDebugf1("Auto-stopping replay recording due to cleaning up a script that started (but didn't stop or cancel) recording a replay marker.");
		sm_bIsStarted = false;
		sm_bHasAutoStopped = true;
		return;
	}

	CutSceneManager* pCutsceneManager = CutSceneManager::GetInstance();
	if (pCutsceneManager && pCutsceneManager->IsCutscenePlayingBack() && pCutsceneManager->WasSkipped())
	{
		replayDebugf1("Auto-stopping replay recording due to player skipping a cutscene.");
		sm_bIsStarted = false;
		sm_bHasAutoStopped = true;
		return;
	}
}

bool	ReplayRecordingScriptInterface::HasStartedRecording()
{
	return sm_bIsStarted;
}

void	ReplayRecordingScriptInterface::RecordClip(u32 totalBackTime, u32 totalForwardTime, u8 importance)
{
	ReplayBufferMarkerMgr::AddMarker( totalBackTime, totalForwardTime, (MarkerImportance)importance, SCRIPT );
}

void	ReplayRecordingScriptInterface::EnableEventsThisFrame()
{
	CReplayMgr::EnableEventsThisFrame();
}

s32		ReplayRecordingScriptInterface::GetMissionIndex()
{
	return CReplayMgr::GetMissionIndex();
}

void	ReplayRecordingScriptInterface::SetMissionIndex(s32 value)
{
	CReplayMgr::SetMissionIndex(value);
}

void	ReplayRecordingScriptInterface::SetMissionName(const atString& name)
{
	CReplayMgr::SetMissionName(name);
}

void	ReplayRecordingScriptInterface::SetFilterString(const atString& name)
{
	CReplayMgr::SetFilterString(name);
}

void	ReplayRecordingScriptInterface::PreventRecordingThisFrame()
{
	CReplayMgr::DisableThisFrame(BANK_ONLY(StoreScriptCallstack));
}

void	ReplayRecordingScriptInterface::DisableCameraMovementThisFrame()
{
	CReplayMgr::DisableCameraMovementThisFrame(BANK_ONLY(StoreScriptCallstack));
#if __BANK
	camManager::ms_DebugReplayCameraMovementDisabledThisFrameScript = true;
#endif // __BANK
}

#endif	//GTA_REPLAY
