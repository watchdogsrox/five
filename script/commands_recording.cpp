// Rage headers
#include "script/wrapper.h"

// framework headers
#include "fwscript/scriptinterface.h"
#include "video/greatestmoment.h"

// Game Headers
#include "script/Handlers/GameScriptResources.h"
#include "script/script.h"
#include "script/script_debug.h"
#include "script/script_helper.h"
#include "control/videorecording/videorecording.h"
#include "control/replay/ReplayControl.h"
#include "control/replay/ReplayRecording.h"
#include "control/Replay/ReplayBufferMarker.h"
#include "control/replay/Audio/SpeechAudioPacket.h"

#include "commands_recording.h"

SCRIPT_OPTIMISATIONS();

namespace recording_commands
{

int	CommandStartRecording(const char *VIDEO_RECORDING_ENABLED_ONLY(pFilename))
{
	int recordingIndex = -1;
#if VIDEO_RECORDING_ENABLED
	VideoRecording::StartRecording( recordingIndex, pFilename);
#endif

	return recordingIndex;
}

void CommandStopRecording(int VIDEO_RECORDING_ENABLED_ONLY(recordingID), int VIDEO_RECORDING_ENABLED_ONLY(score))
{
#if VIDEO_RECORDING_ENABLED
	VideoRecording::StopRecording(recordingID);
	(void)score;
#endif
}

void CommandCancelRecording(int VIDEO_RECORDING_ENABLED_ONLY(recordingID))
{
#if VIDEO_RECORDING_ENABLED
	VideoRecording::CancelRecording(recordingID);
#endif
}

void	CommandReplayStartRecording(int UNUSED_PARAM(importance))
{
#if GTA_REPLAY && 0
	replayDisplayf("REPLAY_START_EVENT - %s", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	ReplayRecordingScriptInterface::StartRecording((u8)importance);
#endif
}

void	CommandReplayStopRecording()
{
#if GTA_REPLAY && 0
	replayDisplayf("REPLAY_STOP_EVENT - %s", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	ReplayRecordingScriptInterface::StopRecording();
#endif
}

void	CommandReplayCancelRecording()
{
#if GTA_REPLAY && 0
	replayDisplayf("REPLAY_CANCEL_EVENT - %s", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	ReplayRecordingScriptInterface::CancelRecording();
#endif
}

void	CommandReplayRecordForTime(float UNUSED_PARAM(theBackTime), float UNUSED_PARAM(theForwardTime), int UNUSED_PARAM(importance))
{
#if GTA_REPLAY && 0
	replayDisplayf("REPLAY_RECORD_BACK_FOR_TIME - %s", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	u32	backTimeInMS = (u32)(theBackTime * 1000.0f);
	u32	forwardTimeInMS = (u32)(theForwardTime * 1000.0f);
	ReplayRecordingScriptInterface::RecordClip(backTimeInMS, forwardTimeInMS, (u8)importance);
#endif
}

void	CommandReplayCheckEventThisFrame(const char* UNUSED_PARAM(pMissionName), const char* UNUSED_PARAM(pFilter))
{
#if GTA_REPLAY && 0
	(void)pMissionName;
	(void)pFilter;

/*
	atString filterStr = atString("");
	if(pFilter)
	{
		filterStr = atString(pFilter);
		replayAssertf(filterStr.length() < MAX_FILTER_LENGTH, "Filter char is too long, max:%i", MAX_FILTER_LENGTH);
	}
	u32 missionHash = atStringHash(pMissionName);
	REPLAY_MODE mode = CReplayControl::GetReplayMode();

	ReplayRecordingScriptInterface::EnableEventsThisFrame();

	//Mission index used for montage ordering
	{
		//Moved to new mission
		if( missionHash != 0 && mode != MANUAL &&
			( missionHash != ReplayRecordingScriptInterface::GetPrevMissionHash() || 
			( ReplayRecordingScriptInterface::GetPrevReplayMode() != mode && ReplayRecordingScriptInterface::GetPrevReplayMode() == MANUAL ) ) )
		{
			ReplayRecordingScriptInterface::SetCurrentMissionOverride(-1);
			ReplayRecordingScriptInterface::SetMissionIndex(-1);
		}

		//On start load or new mission, get the latest saved mission index 
		if(ReplayRecordingScriptInterface::GetMissionIndex() == -1 && ReplayRecordingScriptInterface::GetCurrentMissionOverride() == -1)
		{
			ReplayRecordingScriptInterface::SetMissionIndex(CReplayMgr::CalculateCurrentMissionIndex());
		}

		//Set the current index depending on mission override if we're just deleted this mission clips
		int setMissionIndex = ReplayRecordingScriptInterface::GetCurrentMissionOverride() != -1 ? ReplayRecordingScriptInterface::GetCurrentMissionOverride() :
			ReplayRecordingScriptInterface::GetMissionIndex();

		ReplayRecordingScriptInterface::SetMissionIndex(setMissionIndex);
	}

	ReplayRecordingScriptInterface::SetMissionName(atString(pMissionName));
	ReplayRecordingScriptInterface::SetFilterString(filterStr);
	
	if( missionHash != 0 && mode != MANUAL &&
		( missionHash != ReplayRecordingScriptInterface::GetPrevMissionHash() || 
		( ReplayRecordingScriptInterface::GetPrevReplayMode() != mode && ReplayRecordingScriptInterface::GetPrevReplayMode() == MANUAL ) ) )
	{
		ReplayRecordingScriptInterface::SetPrevMissionHash(missionHash);
		ReplayRecordingScriptInterface::SetPrevReplayMode(mode);
		
		ReplayBufferMarkerMgr::DeleteMissionMarkerClips();
	}
*/

#endif
}

void	CommandReplayPreventRecordingThisFrame()
{
#if GTA_REPLAY
	ReplayRecordingScriptInterface::PreventRecordingThisFrame();
#endif
}

void CommandReplayResetEventInfo()
{
#if GTA_REPLAY
	ReplayRecordingScriptInterface::SetPrevMissionHash(0);
#endif
}


void CommandReplayDisableCameraMovement()
{
#if GTA_REPLAY
	ReplayRecordingScriptInterface::DisableCameraMovementThisFrame();
#endif
}

void CommandReplayPlayVoiceOver(const char* REPLAY_ONLY(context))
{
#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		audWaveSlot* waveSlotToUse = audWaveSlot::FindWaveSlot("SCRIPT_SPEECH_0");
		u32 voiceHash = atStringHash("REPLAY_NARRATION");
		CReplayMgr::RecordFx<CPacketScriptedSpeech>(
			CPacketScriptedSpeech(atPartialStringHash(context), atPartialStringHash(context), voiceHash, (s32)0, AUD_SPEECH_FRONTEND,AUD_AUDIBILITY_NORMAL, 0, waveSlotToUse->GetSlotIndex(), false, true, false, true, true), NULL);
	}
#endif // GTA_REPLAY
}

enum eGreatestMoment
{
	FIVE_START_CRIMINAL = 0,
	CHAIN_REACTION,
	BIRD_DOWN,
	SCOPED,
	TOP_GUN,
	AIRTIME,
	UNTOUCHABLE,
	WHEELIE_RIDER,
	BUCKLE_UP,
	ROLLED_OVER,
	DIZZYING_LAWS,
	YANK_THE_CORD,
	ANIMAUL,
	FULLY_MODDED,
	GLORY_HOLE,
	
	MAX_GREATEST_MOMENTS,
};

void CommandRecordGreatestMoment(int DURANGO_ONLY(greatestMoment), int DURANGO_ONLY(startTime), int DURANGO_ONLY(duration))
{
#if RSG_DURANGO
	if(SCRIPT_VERIFYF(greatestMoment >= 0 && greatestMoment < MAX_GREATEST_MOMENTS, "Invalid GREATEST_MOMENT (%d)!", greatestMoment))
	{
		// The ids on XDP are just numbers.
		static const char* GREATEST_MOMENT_IDS[MAX_GREATEST_MOMENTS] = {"1","2","3","4","5","6","7","8","9","10","11","12","13","14","15"};
		static const char* GREATEST_MOMENT_TEXT[MAX_GREATEST_MOMENTS] = 
		{
			"GREATEST_MOMENT_1",
			"GREATEST_MOMENT_2",
			"GREATEST_MOMENT_3",
			"GREATEST_MOMENT_4",
			"GREATEST_MOMENT_5",
			"GREATEST_MOMENT_6",
			"GREATEST_MOMENT_7",
			"GREATEST_MOMENT_8",
			"GREATEST_MOMENT_9",
			"GREATEST_MOMENT_10",
			"GREATEST_MOMENT_11",
			"GREATEST_MOMENT_12",
			"GREATEST_MOMENT_13",
			"GREATEST_MOMENT_14",
			"GREATEST_MOMENT_15",
		};

		fwGreatestMoment::GetInstance().RecordGreatestMoment( CControlMgr::GetMainPlayerIndex(),
			GREATEST_MOMENT_IDS[greatestMoment],
			TheText.Get(GREATEST_MOMENT_TEXT[greatestMoment]),
			startTime,
			duration );

		scriptDisplayf("RECORD_GREATEST_MOMENT for %d, Start: %d, Duration: %d.", greatestMoment, startTime, duration);
	}
#endif // RSG_DURANGO
}

void CommandStartReplayRecording(int REPLAY_ONLY(startParam))
{
#if GTA_REPLAY
	CReplayControl::SetStartRecording((REPLAY_START_PARAM)startParam);
#endif	//GTA_REPLAY
}

void CommandStopReplayRecording()
{
#if GTA_REPLAY
	CReplayControl::SetStopRecording();
#endif	//GTA_REPLAY
}

bool CommandSaveReplayRecording()
{
#if GTA_REPLAY
	return CReplayControl::SetSaveRecording();
#else
	return false;
#endif	//GTA_REPLAY
}

void CommandCancelReplayRecording()
{
#if GTA_REPLAY
	CReplayControl::SetCancelRecording();
#endif	//GTA_REPLAY
}


bool CommandIsReplayRecording()
{
#if GTA_REPLAY
	return CReplayControl::IsRecording();
#else
	return false;
#endif
}

bool CommandIsReplayInitialized()
{
#if GTA_REPLAY
	return CReplayMgr::HasInitialized();
#else
	return false;
#endif
}

bool CommandIsReplayAvailable()
{
#if GTA_REPLAY
	return CReplayControl::IsReplayAvailable();
#else
	return false;
#endif
}

bool CommandsIsReplayRecordSpaceAvailable(bool showWarning)
{
#if GTA_REPLAY
	return CReplayControl::IsReplayRecordSpaceAvailable(showWarning);
#else
	(void)showWarning;
	return false;
#endif
}

void SetupScriptCommands()
{
	// DVR
	SCR_REGISTER_UNUSED(START_RECORDING,0x80998b5b26c0ca74,			CommandStartRecording);
	SCR_REGISTER_UNUSED(STOP_RECORDING,0x2eb276b9b09d8b92,			CommandStopRecording);
	SCR_REGISTER_UNUSED(CANCEL_RECORDING,0x4af41fa0e0fe0b76,			CommandCancelRecording);

	// REPLAY
	SCR_REGISTER_SECURE(REPLAY_START_EVENT,0x6cd556854f94f942,			CommandReplayStartRecording);
	SCR_REGISTER_SECURE(REPLAY_STOP_EVENT,0x8f70948cb5539beb,			CommandReplayStopRecording);
	SCR_REGISTER_SECURE(REPLAY_CANCEL_EVENT,0x6f274f8ab4f33116,			CommandReplayCancelRecording);

	SCR_REGISTER_SECURE(REPLAY_RECORD_BACK_FOR_TIME,0x2da9f9d8e5530e75,			CommandReplayRecordForTime);
	SCR_REGISTER_SECURE(REPLAY_CHECK_FOR_EVENT_THIS_FRAME,0x6d3f703013ed87d3,			CommandReplayCheckEventThisFrame);
	SCR_REGISTER_SECURE(REPLAY_PREVENT_RECORDING_THIS_FRAME,0x16e09ccc0bd508da,			CommandReplayPreventRecordingThisFrame);
	SCR_REGISTER_SECURE(REPLAY_RESET_EVENT_INFO,0xa315a56fdd0b167b,			CommandReplayResetEventInfo);

	SCR_REGISTER_SECURE(REPLAY_DISABLE_CAMERA_MOVEMENT_THIS_FRAME,0x584b286572b48431,		CommandReplayDisableCameraMovement);
	SCR_REGISTER_UNUSED(REPLAY_PLAY_VOICE_OVER,0x0b7da47ac607b6e7,			CommandReplayPlayVoiceOver);

	// GREATEST MOMENTS
	SCR_REGISTER_SECURE(RECORD_GREATEST_MOMENT,0xc8b81c93ffaa52d8,			CommandRecordGreatestMoment);

	// B*2058458- Add script command to start/stop replay recording, return if we are recording
	SCR_REGISTER_SECURE(START_REPLAY_RECORDING,0x406cba354a9f904a,			CommandStartReplayRecording);
	SCR_REGISTER_SECURE(STOP_REPLAY_RECORDING,0x0fccd6087693aa00,			CommandStopReplayRecording);
	SCR_REGISTER_SECURE(CANCEL_REPLAY_RECORDING,0x8fd3fca286c0f696,			CommandCancelReplayRecording);
	SCR_REGISTER_SECURE(SAVE_REPLAY_RECORDING,0x1b2ffb76f23722db,			CommandSaveReplayRecording);
	SCR_REGISTER_SECURE(IS_REPLAY_RECORDING,0x6c85295e4e1fb8b3,			CommandIsReplayRecording);
	SCR_REGISTER_SECURE(IS_REPLAY_INITIALIZED,0xc69ac9d8785cba51,			CommandIsReplayInitialized);
	SCR_REGISTER_SECURE(IS_REPLAY_AVAILABLE,0x621211b143eb7d46,			CommandIsReplayAvailable);
	SCR_REGISTER_SECURE(IS_REPLAY_RECORD_SPACE_AVAILABLE,0xe829643614370a12,			CommandsIsReplayRecordSpaceAvailable);
}

}	//	end of namespace recording_commands
