#ifndef _REPLAYCONTROL_H_
#define _REPLAYCONTROL_H_

#include "ReplaySettings.h"

#if GTA_REPLAY

#include "control/replay/ReplayBufferMarker.h"
#include "system/service.h"

#define REPLAY_CONTROL_OUTPUT 0
#define MISSION_MODE_ENABLED 0

enum eReplayAutoResumeMode
{
	REPLAY_AUTO_RESUME_RECORDING_OFF = 0,
	REPLAY_AUTO_RESUME_RECORDING_ON,
};

enum eReplayAutoSaveMode
{
	REPLAY_AUTO_SAVE_RECORDING_OFF = 0,
	REPLAY_AUTO_SAVE_RECORDING_ON,
};

struct ReplayInternalControl
{
	bool m_KEY_R;
	bool m_KEY_S;
	bool m_KEY_ADD;
	bool m_KEY_NUMPADENTER;
	bool m_KEY_SPACE;
	bool m_KEY_F1;
	bool m_KEY_F3;
	bool m_KEY_Q;
	bool m_KEY_F5;
	bool m_KEY_RIGHT;
	bool m_KEY_LEFT;
	bool m_KEY_UP;
	bool m_KEY_DOWN;
	bool m_KEY_RETURN;

	ReplayInternalControl()
	{
		Reset();
	}

	void Reset()
	{
		m_KEY_R = false;
		m_KEY_S = false;
		m_KEY_ADD = false;
		m_KEY_NUMPADENTER = false;
		m_KEY_SPACE = false;
		m_KEY_F1 = false;
		m_KEY_F3 = false;
		m_KEY_Q = false;
		m_KEY_F5 = false;
		m_KEY_RIGHT = false;
		m_KEY_LEFT = false;
		m_KEY_UP = false;
		m_KEY_DOWN = false;
		m_KEY_RETURN = false;
	}
};

enum CONTROL_STATE
{
	IDLE = 0,
	RECORDING,
	SAVING,
	//Temporary for the moment to mimic old functionality
	SAVING_DEBUG,
};

enum REPLAY_MODE
{
	MANUAL = 0,
#if MISSION_MODE_ENABLED
	MISSION,
#endif //MISSION_MODE_ENABLED
	CONTINUOUS,
};

enum RECORDING_ALLOWED
{
	ALLOWED = 0,
	DISALLOWED1 = 1 << 1,
	DISALLOWED2 = 1 << 2,
};

// This is reproduced for script in commands_recording.sch (be sure to stay in sync)
enum REPLAY_START_PARAM
{
	REPLAY_START_PARAM_HIGHLIGHT = 0,		// Default, user start/stops
	REPLAY_START_PARAM_DIRECTOR,			// User start/stops, but buffer overruns are output as clips.
};

class CReplayControl
{
	friend class ReplayBufferMarkerMgr;
	friend class ReplayFileManager;

public:
	static void InitSession(unsigned initMode = 0);
	static void InitWidgets();
	static void ShutdownSession(unsigned shutdownMode = 0);
	static void Process();

	// Interface for B*2058458 - Add script command to start/stop replay recording, return if we are recording
	static void SetStartRecording(REPLAY_START_PARAM startParam );
	static void SetStopRecording();
	static bool SetSaveRecording();
	static void SetCancelRecording();
	static bool IsRecording();
	static bool ShouldStopRecordingOnCameraMovementDisabled() { return sm_StopRecordingOnCameraMovementDisabled; }

#if __ASSERT
	static const char* GetRecordingDisabledMessage();
#endif //__ASSERT

#if __BANK
	static bool ShowDisabledRecordingMessage();
#endif // __BANK

	static bool IsMissionModeEnabled() { return sm_MissionEnabled; }

	static bool	IsRecordingAllowed()								{	return sm_recordingAllowed == ALLOWED;		}
	static void	SetRecordingDisallowed(RECORDING_ALLOWED stage)		{	sm_recordingAllowed |= stage;				}
	static void	RemoveRecordingDisallowed(RECORDING_ALLOWED stage)	{	sm_recordingAllowed &= ~stage;				}
	static REPLAY_MODE GetReplayMode();

	static void SetControlsDisabled()								{	sm_ControlsDisabled = true;				}

	static bool	GetCachedUserStartTime(u32 &startTime)
	{
		if( sm_HasCachedUserStartTime )
		{
			startTime = sm_UserStartTime;
		}
		return sm_HasCachedUserStartTime;
	}

	static void	UpdateCachedUserStartTime(u32 startTime)
	{
		Assertf(sm_HasCachedUserStartTime, "CReplayControl::UpdateCachedUserStartTime() - ERROR - Updating a start time that was never set!");
		sm_UserStartTime = startTime;
	}
	
	static bool IsPlayerOutOfControl();
	static bool IsReplayAvailable();
	static bool IsReplayRecordSpaceAvailable(bool showWarning = false);

	static void ShowCameraMovementDisabledWarning(bool bIgnoreState = false);

	static REPLAY_START_PARAM GetStartParam() { return sm_startParam; }

	static void OnSignOut();

#if !__NO_OUTPUT
	static const atString&	GetLastStateChangeReason()	{	return sm_LastStateChangeReason;	}
#endif // !__NO_OUTPUTS

private:
	static void ChangeState(CONTROL_STATE state) { sm_DesiredState = state; }
	static void ProcessState();
	static void UpdateControlModes();
	static void Update_smUserEnabled();
	static bool ShouldStopRecording();
	static bool ShouldRecord();
	static bool ShouldMissionModeBeEnabled();
	static bool ShouldAllowMarkUp();
	static void PostStatusToFeed(const char* label, int numFlashes, bool persistent=false);
	static void ClearFeedStatus();
	static bool IsClipCacheFull(bool showWarning = false);
	static bool IsOutOfDiskSpace(bool showWarning = false, float minThreshold = 1.0f);
	static void GetEstimatedDataSize(u64& dataSize, u16& blockCount);

	static void ShowLowMemoryWarning();
	static void ShowOutOfMemoryWarning();
	static void ShowVitalClipMaintenancePending();
	static void ShowSaveFailedMessage();
	static void CheckUserSpaceRequirement();

	static void OnServiceEvent( sysServiceEvent* evt );

	static bool IsPlayerInPlay();
	static bool ShouldReEnableUserMode();
	static bool ShouldAutoSave();

	static void UpdateReplayFeed(bool recording);

#if __BANK
	static bool TogglePad();
	static void PrintState();
	static void StartHighlightModeRecording();
	static void StartDirectorModeRecording();
	static void SaveRecordingCallback();
#endif // __BANK

	static CONTROL_STATE sm_CurrentState;
	static CONTROL_STATE sm_DesiredState;
	static ReplayInternalControl sm_InternalControl;

	//used for manual start stop toggle to remember start time
	static u32  sm_UserStartTime;
	static bool sm_HasCachedUserStartTime;
	static bool sm_TurnOffUserMode;// True if user turned of within 3 seconds of turning on
	static bool sm_ShouldEnableUserMode;//True if the player mode was turn off automatically
	static u32  sm_StartSavingTime;

	//signaling bools for the recording
	static bool sm_UserEnabled;
	static bool sm_MissionEnabled;
	static bool sm_ContinuousEnabled;

	static bool sm_DisablePrimary;
	static bool sm_DisableSecondary;
	static bool sm_ControlsDisabled;
	static bool sm_ControlsDisabledPrev;

	static u32	sm_recordingAllowed;
	static s32	sm_LastFeedStatusID;

	static s32	sm_recordingFeedID;
	static s32	sm_savingFeedID;
	static float sm_lastProgress;

	// Interface for B*2058458 - Add script command to start/stop replay recording, return if we are recording
	static REPLAY_START_PARAM sm_startParam;
	static REPLAY_START_PARAM sm_nextStartParam;
	static bool sm_SetToggleUserMode;
	static bool sm_WasCancelled;

	static u16 sm_PrevSessionIndex;
	static u16 sm_BufferProgress;
	static bool sm_ShownLowMemoryWarning;
	static bool sm_StopRecordingOnCameraMovementDisabled;

	static ServiceDelegate sm_ServiceDelegate;

#if __BANK
	static bool sm_padReplayToggle;
	static bool sm_DebugRecord;
#endif // __BANK

#if !__NO_OUTPUT
	static atString sm_LastStateChangeReason;
#endif // !__NO_OUTPUTS
};

#endif // GTA_REPLAY

#endif // _REPLAYCONTROL_H_
