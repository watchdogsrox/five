#ifndef __REPLAY_RECORDING_H__
#define __REPLAY_RECORDING_H__

#include "control/replay/ReplaySettings.h"

#if GTA_REPLAY

#include "control/replay/ReplayControl.h"

namespace rage
{
	class atString;
}

class ReplayRecordingScriptInterface
{
public:
	static	void	StartRecording(u8 importance);
	static	void	StopRecording();
	static	void	CancelRecording();
	static  bool	HasStartedRecording();
	static	void	RecordClip(u32 totalBackTime, u32 totalForwardTime, u8 importance);
	static	void	EnableEventsThisFrame();
	static  void    SetMissionName(const atString& name);
	static  void    SetFilterString(const atString& name);
	static  void    PreventRecordingThisFrame();
	static  void	DisableCameraMovementThisFrame();

	static	void	AutoStopRecordingIfNecessary();

	static	u32		GetPrevMissionHash() { return sm_PrevMissionHash; }
	static  void	SetPrevMissionHash(u32 value) { sm_PrevMissionHash = value; }

	static	REPLAY_MODE	GetPrevReplayMode() { return sm_PrevReplayMode; }
	static  void		SetPrevReplayMode(REPLAY_MODE value) { sm_PrevReplayMode = value; }

	static  u32		GetStartTime() { return sm_StartTime; }

	static	s32		GetMissionIndex();
	static  void	SetMissionIndex(s32 value);

	static	s32		GetCurrentMissionOverride() { return sm_CurrentMissionOverride; }
	static  void	SetCurrentMissionOverride(s32 value) { sm_CurrentMissionOverride = value; }

private:

	static	bool		sm_bIsStarted;
	static	bool		sm_bHasAutoStopped;
	static	u32			sm_StartTime;
	static  u8			sm_MarkerImportance;
	static	u32			sm_PrevMissionHash;
	static  REPLAY_MODE	sm_PrevReplayMode;
	static	s32			sm_CurrentMissionOverride;

#if RSG_BANK
	static  atString	sm_StopCallstack;
	static  atString	sm_StartCallstack;
	static  atString	sm_CancelCallstack;
	static  atString*	sm_pAssignCurrentCallstack;
	static  void        StoreScriptCallstack(atString& callstack);
	static  void		AppendCallstack(const char* fmt, ...);
#endif
};

#endif	// GTA_REPLAY

#endif // !__REPLAY_RECORDING_H__
