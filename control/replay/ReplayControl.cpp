#include "control/replay/replaycontrol.h"

#if GTA_REPLAY

#include "control/gamelogic.h"
#include "control/replay/Replay.h"
#include "control/replay/ReplayBufferMarker.h"
#include "control/replay/ReplayInternal.h"
#include "control/replay/ReplayRecording.h"
#include "frontend/GameStreamMgr.h"
#include "frontend/ProfileSettings.h"
#include "Peds/PlayerInfo.h"
#include "Network/Live/LiveManager.h"
#include "Network/NetworkInterface.h"
#include "scene/world/GameWorld.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "system/controlMgr.h"
#include "text/TextConversion.h"

#define MANUAL_ONLY (RSG_DURANGO || RSG_ORBIS)
#define KEYBOARD_START_AND_SAVE_ENABLED 0
#define JOYPAD_START_AND_SELECT_RECORDING_ENABLED	(0)

CONTROL_STATE				CReplayControl::sm_CurrentState;
CONTROL_STATE				CReplayControl::sm_DesiredState;
ReplayInternalControl		CReplayControl::sm_InternalControl;

u32							CReplayControl::sm_UserStartTime = 0;
bool						CReplayControl::sm_HasCachedUserStartTime = false;
bool						CReplayControl::sm_ShouldEnableUserMode = false;
bool						CReplayControl::sm_TurnOffUserMode = false;
bool						CReplayControl::sm_UserEnabled = false;
bool						CReplayControl::sm_MissionEnabled = false;
bool						CReplayControl::sm_ContinuousEnabled = false;
u32							CReplayControl::sm_StartSavingTime = 0;

bool						CReplayControl::sm_DisablePrimary = false;
bool						CReplayControl::sm_DisableSecondary = false;
bool						CReplayControl::sm_ControlsDisabled = false;
bool						CReplayControl::sm_ControlsDisabledPrev = false;

u32							CReplayControl::sm_recordingAllowed = ALLOWED;
s32							CReplayControl::sm_LastFeedStatusID = -1;

s32							CReplayControl::sm_recordingFeedID = -1;
s32							CReplayControl::sm_savingFeedID = -1;

float						CReplayControl::sm_lastProgress = -1.0f;

// Interface for B*2058458 - Add script command to start/stop replay recording, return if we are recording
REPLAY_START_PARAM			CReplayControl::sm_startParam = REPLAY_START_PARAM_HIGHLIGHT;
REPLAY_START_PARAM			CReplayControl::sm_nextStartParam = REPLAY_START_PARAM_HIGHLIGHT;
bool						CReplayControl::sm_SetToggleUserMode = false;
bool						CReplayControl::sm_WasCancelled = false;
bool						CReplayControl::sm_StopRecordingOnCameraMovementDisabled = false;

u16							CReplayControl::sm_PrevSessionIndex = 0;
u16							CReplayControl::sm_BufferProgress = 0;
bool						CReplayControl::sm_ShownLowMemoryWarning = false;

ServiceDelegate				CReplayControl::sm_ServiceDelegate;

#if __BANK
bool						CReplayControl::sm_padReplayToggle = false;
bool						CReplayControl::sm_DebugRecord = false;
#endif // __BANK

#if !__NO_OUTPUT
atString					CReplayControl::sm_LastStateChangeReason;
#endif // !__NO_OUTPUT

extern CPlayerSwitchInterface g_PlayerSwitch;

PARAM(replayRecordingIgnoreLostFocus, "[Replay] Don't disable recording when focus is lost");
PARAM(replayRecordingStopOnCameraDisabled, "[Replay] Stop recording if the camera is disabled.");

void CReplayControl::OnServiceEvent( sysServiceEvent* evt )
{
	if(evt != NULL)
	{
		switch(evt->GetType())
		{
		case sysServiceEvent::FOCUS_LOST:
			{
				//cancel recording if we've lost focus.
				if(PARAM_replayRecordingIgnoreLostFocus.Get() == false)
				{
					if( sm_startParam == REPLAY_START_PARAM_HIGHLIGHT )
					{
						SetCancelRecording();
					}
					else
					{
						SetStopRecording();
					}
				}
				break;
			}
		default:
			break;
		}
	}
}

void CReplayControl::InitSession(unsigned initMode)
{
	ChangeState(IDLE);
	sm_ShouldEnableUserMode = false;

	if(initMode == INIT_CORE)
	{
		sm_ServiceDelegate.Bind(&CReplayControl::OnServiceEvent);
		g_SysService.AddDelegate(&sm_ServiceDelegate);
	}
}

void CReplayControl::InitWidgets()
{
#if __BANK
	bkBank* bank = BANKMGR.FindBank("Replay");

	if( bank != NULL )
	{
		bank->PushGroup("Replay Control");
		bank->AddButton("Start Highlight Mode", StartHighlightModeRecording);
		bank->AddButton("Start Director Mode", StartDirectorModeRecording);
		bank->AddButton("Stop Recording", SetStopRecording);
		bank->AddButton("Save Recording", SaveRecordingCallback);
		bank->AddButton("Cancel Recording", SetCancelRecording);

		sm_StopRecordingOnCameraMovementDisabled = PARAM_replayRecordingStopOnCameraDisabled.Get();
		bank->AddToggle("Stop Recording On Camera Movement Disabled", &sm_StopRecordingOnCameraMovementDisabled);
		bank->PopGroup();
	}
#endif // __BANK
}

void CReplayControl::ShutdownSession(unsigned shutdownMode)
{
	if(shutdownMode == SHUTDOWN_CORE)
	{
		g_SysService.RemoveDelegate(&sm_ServiceDelegate);
		sm_ServiceDelegate.Reset();
	}

	sm_UserStartTime = 0;
	sm_HasCachedUserStartTime = false;
	sm_ShouldEnableUserMode = false;
	sm_TurnOffUserMode = false;
	sm_UserEnabled = false;
	sm_MissionEnabled = false;
	sm_ContinuousEnabled = false;

	sm_DisablePrimary = false;
	sm_DisableSecondary = false;
	sm_ControlsDisabled = false;
	sm_ControlsDisabledPrev = false;

	sm_startParam = REPLAY_START_PARAM_HIGHLIGHT;
	sm_nextStartParam = REPLAY_START_PARAM_HIGHLIGHT;
	sm_SetToggleUserMode = false;

	sm_WasCancelled = false;
	sm_PrevSessionIndex = 0;
	sm_BufferProgress = 0;
	sm_ShownLowMemoryWarning = false;

#if __BANK
	sm_padReplayToggle = false;
	sm_DebugRecord = false;
	
#endif // __BANK

#if !__NO_OUTPUT
	sm_LastStateChangeReason = "";
#endif // !__NO_OUTPUT
}

void CReplayControl::Process()
{
	UpdateControlModes();

	ProcessState();

	CReplayMgr::ProcessInputs(sm_InternalControl);

	//reset the internal control state every frame
	sm_InternalControl.Reset();
}


void CReplayControl::SetStartRecording(REPLAY_START_PARAM startParam )
{
	if( sm_UserEnabled == false )
	{
		// Set a flag to toggle the user mode.
		sm_SetToggleUserMode = true;
		sm_nextStartParam = sm_startParam = startParam;
		replayDebugf1("[REPLAY CONTROL] Script has started recording");
	}
}

bool CReplayControl::SetSaveRecording()
{
	replayAssertf(sm_startParam == REPLAY_START_PARAM_HIGHLIGHT, "SAVE_REPLAY_RECORDING - should only be called in highlight mode");

	if( sm_UserEnabled == true && sm_startParam == REPLAY_START_PARAM_HIGHLIGHT )
	{
		u32 time = 0;
		if(CReplayMgr::GetCurrentBlock().IsValid())
			time = CReplayMgr::GetCurrentBlock().GetEndTime() - sm_UserStartTime;

		//length of a clip should be at least 3 secs
		if(time >= MINIMUM_CLIP_LENGTH)
		{
			//Save out the current buffer using the marker system
			ReplayBufferMarkerMgr::AddMarkerInternal(time, 0, IMPORTANCE_NORMAL, CODE_MANUAL);

			//Force a save of any markers
			ReplayBufferMarkerMgr::Flush();

			// AND clear the user marker
			sm_HasCachedUserStartTime = false;

			replayDisplayf("[REPLAY CONTROL] Clip was marked for save with %u length", time);

			return true;
		}
		else
		{
			ShowSaveFailedMessage();
			replayDisplayf("[REPLAY CONTROL] Clip was shorter than 3 seconds");
		}
	}

	return false;
}

void CReplayControl::SetStopRecording()
{
	if( sm_UserEnabled == true )
	{
		// Set a flag to toggle the user mode.
		sm_SetToggleUserMode = true;
		sm_WasCancelled = false;

		replayDebugf1("[REPLAY CONTROL] Script has stopped recording");
	}
}

void CReplayControl::SetCancelRecording()
{
	if( sm_UserEnabled == true )
	{
		SetStopRecording();
		sm_WasCancelled = true;

		replayDebugf1("[REPLAY CONTROL] Recording has been cancelled");
	}
}

bool CReplayControl::IsRecording()
{
	return sm_UserEnabled;
}


REPLAY_MODE CReplayControl::GetReplayMode()
{
	return MANUAL;	//(REPLAY_MODE)CProfileSettings::GetInstance().GetInt(CProfileSettings::REPLAY_MODE);
}

void CReplayControl::UpdateControlModes()
{
	REPLAY_MODE mode = GetReplayMode();

#if MISSION_MODE_ENABLED
	sm_MissionEnabled = ShouldMissionModeBeEnabled();
#endif //MISSION_MODE_ENABLED

	sm_ContinuousEnabled = mode == CONTINUOUS;
	
	//Manual mode works in all three states
	if(mode >= MANUAL)
	{
		Update_smUserEnabled();
	}

	//turn off the mission recording if we're just in manual mode
	if(mode == MANUAL)
	{
		sm_MissionEnabled = false;
	}

#if __BANK
	//this increments the display for the replay system
	sm_InternalControl.m_KEY_F3	= CControlMgr::GetKeyboard().GetKeyJustDown(KEY_F3, KEYBOARD_MODE_REPLAY);
#endif // __BANK
}

#if __BANK
bool CReplayControl::TogglePad()
{
	if( (CControlMgr::GetPlayerPad()->GetLeftShoulder1() && CControlMgr::GetPlayerPad()->GetLeftShoulder2() &&
		CControlMgr::GetPlayerPad()->GetRightShoulder1() && CControlMgr::GetPlayerPad()->GetRightShoulder2()) || 
		(CControlMgr::GetDebugPad().GetLeftShoulder1() && CControlMgr::GetDebugPad().GetLeftShoulder2() &&
		CControlMgr::GetDebugPad().GetRightShoulder1() && CControlMgr::GetDebugPad().GetRightShoulder2()) )
	{
		if(!sm_padReplayToggle)
		{
			sm_padReplayToggle = true;
			return true;
		}
	
		return false;
	}

	sm_padReplayToggle = false;

	return false;
}
#endif // __BANK

bool CReplayControl::IsPlayerOutOfControl()
{
	bool outOfControl = false;
	if(CGameWorld::FindLocalPlayer())
	{
		CPlayerInfo* pPlayerInfo = CGameWorld::FindLocalPlayer()->GetPlayerInfo();
		if(pPlayerInfo == NULL || pPlayerInfo->AreControlsDisabled())
		{
			outOfControl = true;
		}
	}

	if( sm_ControlsDisabled || sm_ControlsDisabledPrev )
	{
		outOfControl = true;
	}

	return outOfControl;
}

void CReplayControl::GetEstimatedDataSize(u64& dataSize, u16& blockCount)
{
	CBlockProxy firstBlock = CReplayMgrInternal::FindFirstBlock();	
	CBlockProxy currentBlock = firstBlock;
	u32 time = sm_UserStartTime;
	blockCount = 0;

	do
	{
		if( !currentBlock.IsValid() )
		{
			break;
		}

		if(currentBlock.GetStatus() == REPLAYBLOCKSTATUS_SAVING || currentBlock.GetStatus() == REPLAYBLOCKSTATUS_EMPTY)
		{
			currentBlock = currentBlock.GetNextBlock();
			continue;
		}

		if(currentBlock.GetStartTime() > time)
		{
			dataSize += currentBlock.GetSizeUsed();
			time = currentBlock.GetStartTime();
			blockCount++;
		}

		currentBlock = currentBlock.GetNextBlock();
	}
	while(currentBlock != firstBlock);
}

bool CReplayControl::IsClipCacheFull(bool showWarning)
{
	bool result = ReplayFileManager::IsClipCacheFull();

	if( result && showWarning )
	{
		const int bufferSize = 128;
		char buffer[bufferSize];

		CNumberWithinMessage number[1];
		number[0].Set(ReplayFileManager::GetMaxClips());
		CMessages::InsertNumbersAndSubStringsIntoString(TheText.Get("REC_FEED_9"), &number[0], 1, NULL, 0, buffer, bufferSize);

		CGameStreamMgr::GetGameStream()->PostTicker(buffer, false);
	}

	return result;
}

bool CReplayControl::IsOutOfDiskSpace(bool showWarning, float minThreshold)
{
	u64 dataSize = 0;
	GetEstimatedDataSize(dataSize, sm_BufferProgress);
	
	eReplayMemoryWarning warning = REPLAY_MEMORYWARNING_NONE;
	bool result = ReplayFileManager::CheckAvailableDiskSpaceForClips(dataSize, true, minThreshold, warning);

	if( showWarning )
	{
		switch(warning)
		{
		case REPLAY_MEMORYWARNING_LOW:
			{
				ShowLowMemoryWarning();
				break;
			}
		case REPLAY_MEMORYWARNING_OUTOFMEMORY:
			{
				ShowOutOfMemoryWarning();
				break;
			}
		default:
			break;
		}
	}

	return !result;
}

void CReplayControl::ShowOutOfMemoryWarning()
{
	CGameStreamMgr::GetGameStream()->SetImportantParamsRGBA(255, 0, 0, 100);
	CGameStreamMgr::GetGameStream()->SetImportantParamsFlashCount(5);
#if DISABLE_PER_USER_STORAGE
	CGameStreamMgr::GetGameStream()->PostTicker(TheText.Get("REPLAY_OUT_OF_MEMORY"), true);
#else
	CGameStreamMgr::GetGameStream()->PostTicker(TheText.Get("REPLAY_OUT_OF_SHARED_CLIP_SPACE_1"), true);
#endif
	CGameStreamMgr::GetGameStream()->ResetImportantParams();
}

void CReplayControl::ShowLowMemoryWarning()
{
	if( !sm_ShownLowMemoryWarning )
	{
		CGameStreamMgr::GetGameStream()->SetImportantParamsRGBA(255, 0, 0, 100);
		CGameStreamMgr::GetGameStream()->SetImportantParamsFlashCount(5);
		CGameStreamMgr::GetGameStream()->PostTicker(TheText.Get("REPLAY_LOW_MEMORY"), true);
		CGameStreamMgr::GetGameStream()->ResetImportantParams();
		sm_ShownLowMemoryWarning = true;
	}
}


void CReplayControl::ShowVitalClipMaintenancePending()
{
	// "Failed to Record Clip due to required maintenance. Ensure you're connected to the internet and restart the game"
	CGameStreamMgr::GetGameStream()->PostTicker(TheText.Get("REC_FEED_12"), false, true, false, 0);
}

void CReplayControl::ShowSaveFailedMessage()
{
	const int bufferSize = 128;
	char buffer[bufferSize];

	CNumberWithinMessage number[1];
	number[0].Set(MINIMUM_CLIP_LENGTH / 1000);
	CMessages::InsertNumbersAndSubStringsIntoString(TheText.Get("REC_FEED_8"), &number[0], 1, NULL, 0, buffer, bufferSize);

	CGameStreamMgr::GetGameStream()->PostTicker(buffer, false, true, false, 0, true);
}

void CReplayControl::ShowCameraMovementDisabledWarning(bool bIgnoreState)
{
	if( CReplayMgr::IsCameraMovementSupressedThisFrame() )
	{
		//We only want to display the feed message if the game continues to record whilst the camera movement is disabled.
		if( !sm_StopRecordingOnCameraMovementDisabled && ( sm_CurrentState == RECORDING || bIgnoreState ) )
		{
			CGameStreamMgr::GetGameStream()->PostTicker(TheText.Get("REC_FEED_11"), false, true, false, 0);
		}
	}
}

bool CReplayControl::IsReplayAvailable()
{
	bool outOfControl = IsPlayerOutOfControl();
	bool shouldStopRecording = ShouldStopRecording();
	bool isSavingAndRecording = (sm_CurrentState == SAVING && sm_UserEnabled);
	//disable replay from displaying on the hud if we're not allowed to activate it.
	bool cameraDisabled = sm_StopRecordingOnCameraMovementDisabled && ( CReplayMgr::IsCameraMovementSupressedThisFrame() || CReplayMgr::WasCameraMovementSupressedLastFrame() );

	return !(outOfControl || shouldStopRecording || cameraDisabled || isSavingAndRecording);
}

bool CReplayControl::IsReplayRecordSpaceAvailable(bool showWarning)
{
	bool isOutOfDiskSpace = IsOutOfDiskSpace(showWarning, 3);
	bool isCacheFull = IsClipCacheFull(showWarning);

	return !(isOutOfDiskSpace || isCacheFull);
}

void CReplayControl::CheckUserSpaceRequirement()
{
	if( !ReplayFileManager::CanFullfillUserSpaceRequirement() )
	{
		CGameStreamMgr::GetGameStream()->PostTicker(TheText.Get("REC_FEED_10"), false);
	}
}

const u32	gMinimumSavingTime = 2 * 1000;
void CReplayControl::Update_smUserEnabled()
{
	bool outOfControl = IsPlayerOutOfControl();
	bool shouldStopRecording = ShouldStopRecording();
	bool isOutOfDiskSpace = IsOutOfDiskSpace(sm_CurrentState == RECORDING);
	ORBIS_ONLY(bool isVitalClipMaintenanceOutstanding = ReplayFileManager::IsVitalClipFolderProcessingStillPending());

	//are we automatically turning off recording
	bool autoStopRecording = outOfControl || shouldStopRecording || isOutOfDiskSpace ORBIS_ONLY(|| isVitalClipMaintenanceOutstanding);

	bool togglingUserMode = false;


#if JOYPAD_START_AND_SELECT_RECORDING_ENABLED		// enable start+select to start/stop recordings.

	// In control (not out of control) and not playing back.. 
	if (!outOfControl && !CReplayMgr::IsEditModeActive())
	{
		// Toggle signaled (debug pad/key/menu etc..)
		//CControlMgr::GetMainFrontendControl().WasKeyboardMouseLastKnownSource()
		if(CControlMgr::GetMainFrontendControl().GetReplayStartStopRecordingPrimary().GetSource().m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE)
		{
			if( CControlMgr::GetMainFrontendControl().GetReplayStartStopRecordingPrimary().IsPressed() )
			{
				togglingUserMode = true;
				sm_DisablePrimary = true;
			}
		}
		else if( CControlMgr::GetMainFrontendControl().GetReplayStartStopRecordingPrimary().IsDown() &&  
				CControlMgr::GetMainFrontendControl().GetReplayStartStopRecordingSecondary().IsDown() &&
				sm_DisablePrimary == false && sm_DisableSecondary == false )
		{
			sm_DisablePrimary = true;
			sm_DisableSecondary = true;
			togglingUserMode = true;
		}
	}
	
	if( sm_DisablePrimary )
	{
		if( CControlMgr::GetMainFrontendControl().GetReplayStartStopRecordingPrimary().IsUp() )
		{		
			sm_DisablePrimary = false;
		}

		CControlMgr::GetMainFrontendControl().SetInputExclusive(INPUT_REPLAY_START_STOP_RECORDING);

	}
	
	if( sm_DisableSecondary )
	{
		if( CControlMgr::GetMainFrontendControl().GetReplayStartStopRecordingSecondary().IsUp() )
		{
			sm_DisableSecondary = false;
		}

		CControlMgr::GetMainFrontendControl().SetInputExclusive(INPUT_REPLAY_START_STOP_RECORDING_SECONDARY);
	}
#endif	//JOYPAD_START_AND_SELECT_RECORDING_ENABLED

	// Interface for B*2058458 - Add script command to start/stop replay manual recording
	if( sm_SetToggleUserMode )
	{
		togglingUserMode = sm_SetToggleUserMode;
		sm_SetToggleUserMode = false;
	}

	// Turn off user enabling if...
	// we are out of control
	if ( autoStopRecording )
	{
		// We know we are enabled, so indicate that we want to toggle [to turn us off]
		if (sm_UserEnabled)
		{
			togglingUserMode = true;

			if( ShouldReEnableUserMode() )
			{
				sm_ShouldEnableUserMode = true;
			}
		}
	}
	else
	{
		if(sm_ShouldEnableUserMode)
		{
			togglingUserMode = true; //[to turn us on]
			sm_HasCachedUserStartTime = false;
			sm_ShouldEnableUserMode = false;
		}
	}

	if( sm_UserEnabled )
	{
		//cache the user start time if we haven't
		//this needs to be done a frame later so the replay system has time to turn on.
		if( !sm_HasCachedUserStartTime )
		{
			sm_HasCachedUserStartTime = true;
			sm_UserStartTime = fwTimer::GetReplayTimeInMilliseconds();
		}
	}

	u32 time = 0;
	if(CReplayMgr::GetCurrentBlock().IsValid())
		time = CReplayMgr::GetCurrentBlock().GetEndTime() - sm_UserStartTime;

	if( sm_TurnOffUserMode )
	{
		if(time >= MINIMUM_CLIP_LENGTH)
		{
			togglingUserMode = true;
		}
	}

	if (togglingUserMode)
	{
		if(sm_UserEnabled)
		{
			replayDebugf1("[REPLAY CONTROL] - Attempting to add a marker to save the clip");

			//Only attempt to save automatically if we're in director mode AND
			//Only attempt to save if we're not canceled the recording
			if( ShouldAutoSave() && sm_WasCancelled == false )
			{
				//length of a clip should be at least 3 secs
				if(time >= MINIMUM_CLIP_LENGTH)
				{
					//Save out the current buffer using the marker system
					if( !ReplayBufferMarkerMgr::AddMarkerInternal(time, 0, IMPORTANCE_NORMAL, CODE_MANUAL) )
					{
						replayDebugf1("[REPLAY CONTROL] - Failed to add clip marker!");
					}

					//Force a save of any markers
					ReplayBufferMarkerMgr::Flush();

					// AND clear the user marker
					sm_HasCachedUserStartTime = false;
				}
				else
				{
					replayDebugf1("[REPLAY CONTROL] - Clip is too short so failed to save out.");

					ShowSaveFailedMessage();
				}
			}
			else
			{
				replayDebugf1("[REPLAY CONTROL] - Clip was cancelled so nothing will have saved out. AUTO_SAVE: %s, WasCancelled: %s",  ShouldAutoSave() ? "TRUE" : "FALSE", sm_WasCancelled ? "TRUE" : "FALSE");

				// Yes, don't bother with any of the minimum time stuff, or the marker/flush.
				// Reset, since we're about to stop.
				sm_WasCancelled = false;
				// AND clear the user marker
				sm_HasCachedUserStartTime = false;
			}

			replayDebugf1("[REPLAY CONTROL] - Shutting down replay after attempting to save a clip");

			sm_nextStartParam = sm_startParam;
			sm_startParam = REPLAY_START_PARAM_HIGHLIGHT;
			sm_TurnOffUserMode = false;
		
			sm_UserStartTime = 0;
			sm_UserEnabled = false;
		}
		else
		{
			//warn that we are out of space when trying to turn on the replay
			if( !IsOutOfDiskSpace(true, 3) && !IsClipCacheFull(true) )
			{
				//warn users if they allocated space is too low.
				CheckUserSpaceRequirement();

				ShowCameraMovementDisabledWarning(true);

				sm_HasCachedUserStartTime = false;
				sm_UserEnabled = true;
			}
		}
	}

	if( outOfControl )
	{
		if(!ShouldAllowMarkUp() && !ShouldStopRecording() &&
			!ReplayRecordingScriptInterface::HasStartedRecording())
		{
			CReplayMgr::UpdateFrameTimeScriptEventsWereDisabled();
		}
	}

	sm_ControlsDisabledPrev = sm_ControlsDisabled;
	sm_ControlsDisabled = false;
}

void CReplayControl::OnSignOut()
{
	SetCancelRecording();

	sm_ShouldEnableUserMode = false;
}

bool CReplayControl::IsPlayerInPlay()
{
	CPed* pPlayerPed = FindPlayerPed();
	if( !CGameLogic::IsGameStateInPlay() || (pPlayerPed && pPlayerPed->IsFatallyInjured()))
	{
		return false;
	}

	return true;
}

bool CReplayControl::ShouldReEnableUserMode()
{
	if( sm_WasCancelled )
	{
		return false;
	}

	if( fwTimer::IsGamePaused() || fwTimer::IsUserPaused() )
	{
		return true;
	}

	//don't re-enable if we've ran out of space
	if( IsOutOfDiskSpace(false) )
	{
		return false;
	}

	//don't re-enable replay if we died or got arrested during a single player game.
	if( !IsPlayerInPlay() )
	{
		if( !NetworkInterface::IsGameInProgress() )
		{
			return false;
		}

		//auto resume if we are in an MP game.
		return true;
	}

	//don't re-enable if we've just performed a player switch.
	if(g_PlayerSwitch.IsActive())
	{
		return false;
	}

	//Don't re-enable recording if the camera movement was disabled
	if( sm_StopRecordingOnCameraMovementDisabled )
	{
		if( CReplayMgr::ShouldDisabledCameraMovement() )
		{
			return false;
		}
	}

	//Removing in B*2224491
	/*eReplayAutoResumeMode mode = (eReplayAutoResumeMode)CProfileSettings::GetInstance().GetInt(CProfileSettings::REPLAY_AUTO_RESUME_RECORDING);*/

	//if mode is ON we can turn recording back on after cut scenes or out of control sections.
	//don't re-enable replay if we are in director mode and out of control or we've been suppressed by script.
	bool outOfControl = IsPlayerOutOfControl();
	if(sm_startParam == REPLAY_START_PARAM_DIRECTOR && (outOfControl || CReplayMgr::IsSuppressedThisFrame()) /*&& mode == eReplayAutoResumeMode::REPLAY_AUTO_RESUME_RECORDING_OFF*/)
	{
		return false;
	}

	return true;
}

bool CReplayControl::ShouldAutoSave()
{
	eReplayAutoSaveMode mode = (eReplayAutoSaveMode)CProfileSettings::GetInstance().GetInt(CProfileSettings::REPLAY_AUTO_SAVE_RECORDING);

	return sm_startParam == REPLAY_START_PARAM_DIRECTOR || mode == REPLAY_AUTO_SAVE_RECORDING_ON;
}

void CReplayControl::ProcessState()
{
	UpdateReplayFeed(CReplayMgr::IsRecordingEnabled());

	//Swap states if we need to.
	if(sm_DesiredState != sm_CurrentState)
	{
		sm_CurrentState = sm_DesiredState;
	}

	switch(sm_CurrentState)
	{
	case IDLE:
		{
			if(CReplayMgr::IsEditModeActive())
			{
				//we're in edit mode don't do anything
				break;
			}

#if __BANK && KEYBOARD_START_AND_SAVE_ENABLED
			if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_R, KEYBOARD_MODE_REPLAY))
			{
				sm_DebugRecord = true;
				replayDebugf1("[REPLAY CONTROL] Replay debug mode has turned on recording");
			}
#endif // __BANK && KEYBOARD_START_AND_SAVE_ENABLED

			if(ShouldRecord())
			{
				replayDebugf1("[REPLAY CONTROL] Changing state from IDLE to Recording");

				//update the start st
				sm_startParam = sm_nextStartParam;
				sm_InternalControl.m_KEY_R	= true;
				ChangeState(RECORDING);
				break;
			}

			replayAssertf(!CReplayMgr::IsRecordingEnabled(), "Replay control is IDLE when the replay system is RECORDING");

			break;
		}
	case RECORDING:
		{
#if __BANK && KEYBOARD_START_AND_SAVE_ENABLED
			//Update Bank Debug Controls
			//debug F1 save
			if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_F1, KEYBOARD_MODE_REPLAY))
			{
				sm_InternalControl.m_KEY_F1 = true;
				ChangeState(SAVING_DEBUG);
				break;
			}

			if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_R, KEYBOARD_MODE_REPLAY))
			{
				sm_DebugRecord = !sm_DebugRecord;
				if( !ShouldRecord() && !sm_DebugRecord)
				{
					replayDebugf1("[REPLAY CONTROL] ShouldRecord is false, Last state change reason: %s", sm_LastStateChangeReason.c_str()); 
					replayDebugf1("[REPLAY CONTROL] Replay debug mode has turned off recording");
					sm_InternalControl.m_KEY_R	= true;
					ChangeState(IDLE);
				}
				break;
			}
#endif // __BANK
			
			//If we're recording, f3 will add an event for 5 seconds backwards.
			//Only works when continuous mode is enabled and the player is in control
			if(CControlMgr::GetMainFrontendControl().GetSaveReplayClip().IsPressed() && sm_ContinuousEnabled && !IsPlayerOutOfControl())
			{
				//Save out a marker and ignore the script check to suppress game events.
				if(ReplayBufferMarkerMgr::AddMarkerInternal(20000, 0, IMPORTANCE_NORMAL, CODE_MANUAL))
				{
					ReplayBufferMarkerMgr::Flush();
				}
			}

			//if none of the modes are set then turn off recording
			if(!ShouldRecord())
			{
				replayDebugf1("[REPLAY CONTROL] ShouldRecord is false, Last state change reason: %s", sm_LastStateChangeReason.c_str()); 

#if RSG_ORBIS
				if(ReplayFileManager::IsVitalClipFolderProcessingStillPending())
				{
					replayDebugf1("[REPLAY CONTROL] - Can't start recording due to clip maintenance not having succeeded");
					ShowVitalClipMaintenancePending();
					ReplayBufferMarkerMgr::ClipUnprocessedMarkerEndTimesToCurrentEndTime(true);
				}
				//Clip any markers that might have ended in an unrecorded section.
				else
#endif // RSG_ORBIS
					if( ReplayBufferMarkerMgr::ClipUnprocessedMarkerEndTimesToCurrentEndTime(true) )
				{
					replayDebugf1("[REPLAY CONTROL] - Marker has been clipped so no clip will save out because its too short.");
					ShowSaveFailedMessage();
				}

				//flush markers before we turn off the replay system
				ReplayBufferMarkerMgr::Flush();
			}

			if(ReplayBufferMarkerMgr::IsSaving())
			{
				replayDebugf1("[REPLAY CONTROL] ReplayBufferMarkerMgr::IsSaving() == true, change state to SAVING");
				ChangeState(SAVING);
				break;
			}

			replayAssertf(CReplayMgr::IsRecordingEnabled(), "Replay control is RECORDING when the replay system is not RECORDING");

			break;
		}
	case SAVING:
		{
			//If we're done saving change back to recording state
			if(!CReplayMgr::IsSaving() && !CReplayMgr::WillSave() && !ReplayBufferMarkerMgr::IsSaving())
			{
				ClearFeedStatus();
				sm_BufferProgress = 0;

				//If we failed to save, cancel recording.
				if( CReplayMgr::GetLastErrorCode() != REPLAY_ERROR_SUCCESS )
				{
					SetCancelRecording();

					replayDebugf1("[REPLAY CONTROL] Failed to save out recording, change to IDLE state");

					sm_InternalControl.m_KEY_R	= true;
					ChangeState(IDLE);
				}
				else if(sm_startParam != REPLAY_START_PARAM_DIRECTOR || !ShouldRecord())
				{
					replayDebugf1("[REPLAY CONTROL] Finished saving, Change to IDLE state");

					sm_InternalControl.m_KEY_R	= true;
					ChangeState(IDLE);
				}
				else if( IsOutOfDiskSpace(true, 3) || IsClipCacheFull(true) )
				{
					replayDebugf1("[REPLAY CONTROL] Finished saving but the clip cache is full or we've ran out of space to save. Returning to idle");

					sm_InternalControl.m_KEY_R	= true;
					ChangeState(IDLE);
				}
				else
				{
					ChangeState(RECORDING);
				}
			}
			break;
		}
	case SAVING_DEBUG:
		{
			//If we're done saving change back to recording state
			if(!CReplayMgr::IsSaving() && !CReplayMgr::WillSave())
			{
				replayDebugf1("[REPLAY CONTROL] Saving debug has finished, change state to RECORDING");

				ChangeState(RECORDING);
			}
			break;
		}
	default:
		{
			break;
		}
	}

#if __BANK
#if REPLAY_CONTROL_OUTPUT
	PrintState();
#endif
#endif // __BANK
}

bool CReplayControl::ShouldStopRecording()
{
	if( !CLiveManager::IsSignedIn() )
	{
		OUTPUT_ONLY(sm_LastStateChangeReason = "CLiveManager::IsSignedIn()";)
		return true;
	}

	//stop recording if the game is paused.
	if (fwTimer::IsUserPaused())
	{
		OUTPUT_ONLY(sm_LastStateChangeReason = "fwTimer::IsUserPaused()";)
		return true;
	}

	if( fwTimer::IsGamePaused())
	{
		OUTPUT_ONLY(sm_LastStateChangeReason = "fwTimer::IsGamePaused()";)
		return true;
	}

	if( !IsPlayerInPlay() )
	{
		OUTPUT_ONLY(sm_LastStateChangeReason = "CGameLogic::IsGameStateInPlay() && !CGameLogic::IsGameStateInArrest() && !CGameLogic::IsGameStateInDeath()";)
		return true;
	}

	//if we're trying to launch the video editor stop recording
	if(CReplayMgr::IsEditModeActive() || CReplayMgr::IsWaitingForSave())
	{
		BANK_ONLY(sm_LastStateChangeReason = "CReplayMgr::IsEditModeActive() || CReplayMgr::IsWaitingForSave()";)
		return true;
	}

	if( CReplayMgr::WantsToQuit() )
	{
		OUTPUT_ONLY(sm_LastStateChangeReason = "WantsToQuit";)
		return true;
	}

	if(sm_recordingAllowed != ALLOWED)
	{
		OUTPUT_ONLY(sm_LastStateChangeReason = "sm_recordingAllowed != ALLOWED";)
		return true;
	}

	if(g_PlayerSwitch.IsActive())
	{
		OUTPUT_ONLY(sm_LastStateChangeReason = "g_PlayerSwitch.IsActive()";)
		return true;
	}

	//if we are suppress this frame then turn recording off.
	if(CReplayMgr::IsSuppressedThisFrame())
	{
		OUTPUT_ONLY(sm_LastStateChangeReason = "CReplayMgr::IsSuppressedThisFrame()";)
		return true;
	}

	//Cancel recording if the camera movement was disabled
	if( sm_StopRecordingOnCameraMovementDisabled )
	{
		if( CReplayMgr::ShouldDisabledCameraMovement() )
		{
			OUTPUT_ONLY(sm_LastStateChangeReason = "CReplayMgr::IsCameraMovementSupressedThisFrame()";)
			return true;
		}
	}

	return false;
}

bool CReplayControl::ShouldRecord()
{
	if( ShouldStopRecording() )
		return false;

	//check if we're in continuous mode or the mission is recording
	return (sm_UserEnabled || sm_MissionEnabled || sm_ContinuousEnabled BANK_ONLY(|| sm_DebugRecord));
}

bool CReplayControl::ShouldAllowMarkUp()
{
	return CReplayMgr::IsEnableEventsThisFrame() && !CReplayMgr::IsSuppressedThisFrame();
}

void CReplayControl::PostStatusToFeed(const char* label, int numFlashes, bool persistent)
{
	if(label)
	{
		CGameStream* pGameStream = CGameStreamMgr::GetGameStream();
		if(pGameStream)
		{
			// Only post one replay status at a time
			pGameStream->RemoveItem(sm_LastFeedStatusID);
			if(persistent)
				pGameStream->FreezeNextPost();
			sm_LastFeedStatusID = pGameStream->PostTicker(TheText.Get(label), false, false, true, numFlashes);
		}
	}
}

void CReplayControl::ClearFeedStatus()
{
	CGameStream* pGameStream = CGameStreamMgr::GetGameStream();
	if(pGameStream && sm_LastFeedStatusID != -1)
	{
		// Only post one replay status at a time
		pGameStream->RemoveItem(sm_LastFeedStatusID);
	}

	sm_ShownLowMemoryWarning = false;
}


void CReplayControl::UpdateReplayFeed(bool recording)
{
	bool saving = sm_CurrentState == SAVING && CReplayMgr::IsSaving();
	float progress = ((float)(sm_BufferProgress) / (float)CReplayMgr::GetNumberOfReplayBlocks());
	bool bufferIsFull = (progress >= 1.0f);
	bool removeRecordingSpinnerOverride = (saving || (sm_savingFeedID != -1)) && !sm_ContinuousEnabled;	// Force the recording spinner to remove if we're saving and not in continuous mode

	// Recording progress spinner...
	if(sm_startParam != REPLAY_START_PARAM_HIGHLIGHT && recording && (sm_lastProgress != progress || sm_recordingFeedID == -1) && !removeRecordingSpinnerOverride)
	{
		// Only update if buffer wasn't full last frame...otherwise the 'buffer full' spin keeps resetting
		if(sm_lastProgress < 1.0f)
		{
			// No need to remove last here; PostReplay updates any matching posts rather than creating an additional to be removed later

			sm_recordingFeedID = CGameStreamMgr::GetGameStream()->PostReplay(CGameStream::DIRECTOR_RECORDING, TheText.Get("REPLAY_RECORDING"), "", CGameStream::BUFFER_ICON, Min(1.0f, progress), bufferIsFull);
			sm_lastProgress = progress;
		}
	}
	else if((recording == false || removeRecordingSpinnerOverride) && sm_recordingFeedID != -1)
	{	
		// Not recording so get rid of the old one
		CGameStreamMgr::GetGameStream()->RemoveItem(sm_recordingFeedID);
		sm_recordingFeedID = -1;
		sm_lastProgress = -1.0f;
	}

	const u32 currentTime = fwTimer::GetSystemTimeInMilliseconds();
	u32 savingTime = currentTime - sm_StartSavingTime;

	// Saving spinner...
	if(saving && sm_savingFeedID == -1 && ReplayFileManager::GetCurrentLength() != 0)
	{
		sm_StartSavingTime = currentTime;
		char subtitle[64];
		u32 length = u32(ReplayFileManager::GetCurrentLength() * 1000);
		CTextConversion::FormatMsTimeAsString( subtitle, 64, length );
		sm_savingFeedID = CGameStreamMgr::GetGameStream()->PostReplay(CGameStream::BUTTON_ICON, ReplayFileManager::GetCurrentFilename().c_str(), subtitle, ICON_SPINNER);
	}
	else if(saving == false && sm_savingFeedID != -1 && savingTime > gMinimumSavingTime)
	{
		CGameStreamMgr::GetGameStream()->RemoveItem(sm_savingFeedID);
		sm_savingFeedID = -1;
		sm_StartSavingTime = 0;
	}
}


bool CReplayControl::ShouldMissionModeBeEnabled()
{
	return (CReplayMgr::IsEnableEventsThisFrame() || ReplayRecordingScriptInterface::HasStartedRecording()) && !CReplayMgr::IsSuppressedThisFrame();
}

#if __BANK

void CReplayControl::StartHighlightModeRecording()
{
	SetStartRecording(REPLAY_START_PARAM_HIGHLIGHT);
}

void CReplayControl::StartDirectorModeRecording()
{
	SetStartRecording(REPLAY_START_PARAM_DIRECTOR);
}

void CReplayControl::SaveRecordingCallback()
{
	SetSaveRecording();
}

void CReplayControl::PrintState()
{
	static char debugString[256];
	const char* state = "";

	switch(sm_CurrentState)
	{
	case IDLE:
		{
			state = "IDLE";
			break;
		}
	case RECORDING:
		{
			state = "RECORDING";
			break;
		}
	case SAVING:
		{
			state = "SAVING";
			break;
		}
	case SAVING_DEBUG:
		{
			state = "SAVING_DEBUG";
			break;
		}
	}

	sprintf(debugString, "REPLAY_CONTROL_STATE: %s - DebugMode: %s, UserMode: %s, MissionMode: %s, ContinuousMode: %s", state, sm_DebugRecord ? "TRUE" : "FALSE", sm_UserEnabled ? "TRUE" : "FALSE", sm_MissionEnabled ? "TRUE" : "FALSE", sm_ContinuousEnabled ? "TRUE" : "FALSE");

	Color32 color = Color32(0.0f, 0.0f, 1.0f);
	grcDebugDraw::Text(Vec2V(0.002f, 0.032f), color, debugString, false, 1.2f);
}

bool CReplayControl::ShowDisabledRecordingMessage()
{
	if( sm_DebugRecord )
	{
		if( IsPlayerOutOfControl() && !sm_MissionEnabled )
		{
			return true;
		}
	}

	return false;
}
#endif // __BANK

#if __ASSERT
const char* CReplayControl::GetRecordingDisabledMessage()
{
	if( CReplayMgr::IsPlaybackFlagsSet(FRAME_PACKET_RECORDING_DISABLED) )
	{
		return "DEBUG RECORDING ONLY - DON'T BUG";
	}
	else
	{
		return NULL;
	}
}
#endif // __ASSERT

#endif // GTA_REPLAY
