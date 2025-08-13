/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : VideoProjectPlaybackController.cpp
// PURPOSE : Represents instance data for controlling a video preview playback.
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#include "VideoProjectPlaybackController.h"

#if defined( GTA_REPLAY ) && GTA_REPLAY

// rage
#include "math/amath.h"

// game
#include "control/replay/replay.h"
#include "control/replay/ReplayMarkerContext.h"
#include "frontend/ui_channel.h"
#include "frontend/VideoEditor/VideoEditorInterface.h"
#include "frontend/VideoEditor/Core/VideoEditorProject.h"
#include "replaycoordinator/ReplayCoordinator.h"

FRONTEND_OPTIMISATIONS();

#define MIN_CURSOR_SPEED		(1.f)
#define MAX_CURSOR_SPEED		(3.f)
#define CURSOR_SPEED_INCREMENT	(0.5f)

CVideoProjectPlaybackController::CVideoProjectPlaybackController()
	: m_playbackMode( PLAYBACK_INVALID )
	, m_isEditing( false )
{

}

CVideoProjectPlaybackController::~CVideoProjectPlaybackController()
{

}

bool CVideoProjectPlaybackController::IsPlaying() const
{
	CReplayPlaybackController const& c_replayPlaybackController = CReplayCoordinator::GetReplayPlaybackController();
    bool const c_isPlaying = CReplayMgr::IsPlaying();
    bool const c_wantsPause = CReplayMgr::WantsPause();
    bool const c_isAboutToPlay = ( c_replayPlaybackController.IsValid() && c_replayPlaybackController.IsStartingClipNextFrame() );

	return ( c_isPlaying || c_isAboutToPlay ) && !c_wantsPause;
}

bool CVideoProjectPlaybackController::IsPaused() const
{
	return CReplayMgr::IsUserPaused() || CReplayMgr::WantsPause();
}

float CVideoProjectPlaybackController::SetCursorSpeed( float const speed )
{
	float cursorSpeed = speed;

	if( cursorSpeed != 0.f )
	{
		bool const c_forward = speed >= 0.f;

		float const c_minThreshold = c_forward ? MIN_CURSOR_SPEED : -MAX_CURSOR_SPEED;
		float const c_maxThreshold = c_forward ? MAX_CURSOR_SPEED : -MIN_CURSOR_SPEED;

		cursorSpeed = Clamp( speed, c_minThreshold, c_maxThreshold );

		u32 replayState =	c_forward ? ( REPLAY_STATE_PLAY | REPLAY_STATE_FAST | REPLAY_DIRECTION_FWD ) : 
										( REPLAY_STATE_PLAY | REPLAY_STATE_FAST | REPLAY_DIRECTION_BACK );

		CReplayMgr::StopPreCaching();
		CReplayMgr::SetNextPlayBackState( replayState );
		CReplayMgr::SetCursorSpeed( cursorSpeed );
	}
	else
	{
		Pause();
	}

	return cursorSpeed;
}

float CVideoProjectPlaybackController::GetCursorSpeed() const
{
	return IsEnabled() ? CReplayMgr::GetCursorSpeed() : 0.f;
}

bool CVideoProjectPlaybackController::IsEnabled() const
{
	return CReplayMgr::IsEditModeActive();
}

bool CVideoProjectPlaybackController::IsEditing() const
{
	return CVideoProjectPlaybackController::IsEnabled() && m_isEditing;
}

bool CVideoProjectPlaybackController::CanQueryStartAndEndTimes() const
{
	return CReplayCoordinator::GetReplayPlaybackController().IsValid() && CVideoEditorInterface::GetActiveProject() != NULL;
}

float CVideoProjectPlaybackController::GetPlaybackStartTimeMs() const
{
	float startTime = 0;

	if( CanQueryStartAndEndTimes() )
	{
		//! Project playback spans all clips, so just say the start time is always 0
		startTime = ( m_playbackMode == PLAYBACK_PROJECT ) ? 0 : CReplayCoordinator::GetReplayPlaybackController().GetClipStartNonDilatedTimeMs( -1 );
	}

	return startTime;
}

float CVideoProjectPlaybackController::GetPlaybackEndTimeMs() const
{
	float endTime = 0;

	if( CanQueryStartAndEndTimes() )
	{
		CVideoEditorProject* activeProject = CVideoEditorInterface::GetActiveProject();
		s32 const c_currentClipIndex = CReplayCoordinator::GetIndexOfCurrentPlaybackClip();

		//! Project playback spans all clips, so end time must be all clips
		if( m_playbackMode == PLAYBACK_PROJECT && activeProject )
		{
			endTime = activeProject->GetTotalClipTimeMs();
		}
		else 
		{
			endTime = activeProject ? 
				activeProject->GetClipDurationTimeMs( c_currentClipIndex ) : CReplayCoordinator::GetReplayPlaybackController().GetClipNonDilatedEndTimeMs( -1 );
		}
	}

	return endTime;
}

float CVideoProjectPlaybackController::GetPlaybackCurrentNonDilatedTimeMs() const
{
	float currentTime = 0;

	CVideoEditorProject* activeProject = CVideoEditorInterface::GetActiveProject();
	if( IsEnabled() && activeProject )
	{
		s32 const c_currentClipIndex = CReplayCoordinator::GetIndexOfCurrentPlaybackClip();
		float const c_clipStartTimeMs = activeProject->GetClipStartNonDilatedTimeMs( c_currentClipIndex );
		float const c_clipEndTimeMs = activeProject->GetClipEndNonDilatedTimeMs( c_currentClipIndex );

		//! Get current time in non-dilated format
		float const c_currentClipTimeMs = Clamp( GetEffectiveCurrentClipTimeMs(), c_clipStartTimeMs, c_clipEndTimeMs );
		currentTime = c_currentClipTimeMs;

		if( m_playbackMode == PLAYBACK_PROJECT && activeProject )
		{
			currentTime += activeProject->GetTrimmedNonDilatedTimeToClipMs( (u32)c_currentClipIndex );	
		}

	}

	return currentTime;
}

float CVideoProjectPlaybackController::GetPlaybackCurrentTimeMs() const
{
	float currentTime = 0;

	CVideoEditorProject* activeProject = CVideoEditorInterface::GetActiveProject();
	if( IsEnabled() && activeProject )
	{
		s32 const c_currentClipIndex = CReplayCoordinator::GetIndexOfCurrentPlaybackClip();
		float const c_clipStartTimeMs = activeProject->GetClipStartNonDilatedTimeMs( c_currentClipIndex );
		float const c_clipEndTimeMs = activeProject->GetClipEndNonDilatedTimeMs( c_currentClipIndex );

		//! Get current time in non-dilated format
		float const c_currentClipTimeMs = Clamp( GetEffectiveCurrentClipTimeMs(), c_clipStartTimeMs, c_clipEndTimeMs );
		currentTime = activeProject->ConvertClipNonDilatedTimeMsToClipTimeMs( c_currentClipIndex, c_currentClipTimeMs );

		if( m_playbackMode == PLAYBACK_PROJECT )
		{
			currentTime += activeProject->GetTrimmedTimeToClipMs( (u32)c_currentClipIndex );
		}

	}

	return currentTime;
}

float CVideoProjectPlaybackController::GetPlaybackCurrentClipStartNonDilatedTimeMs() const
{
	float const c_endTime = CanQueryStartAndEndTimes() ? CReplayCoordinator::GetReplayPlaybackController().GetClipStartNonDilatedTimeMs( -1 ) : 0;
	return c_endTime;
}

float CVideoProjectPlaybackController::GetPlaybackCurrentClipCurrentNonDilatedTimeMs() const
{
	float currentTime = IsEnabled() ? GetEffectiveCurrentClipTimeMs() : 0;
	currentTime = Clamp( currentTime, GetPlaybackCurrentClipStartNonDilatedTimeMs(), GetPlaybackCurrentClipEndNonDilatedTimeMs() );

	return currentTime;
}

float CVideoProjectPlaybackController::GetPlaybackCurrentClipEndNonDilatedTimeMs() const
{
	float const c_endTime = CanQueryStartAndEndTimes() ? CReplayCoordinator::GetReplayPlaybackController().GetClipNonDilatedEndTimeMs( -1 ) : 0;
	return c_endTime;
}

float CVideoProjectPlaybackController::GetPlaybackCurrentClipRawEndTimeMsAsTimeMs() const
{
	CVideoEditorProject* activeProject = CVideoEditorInterface::GetActiveProject();
	s32 const c_currentClipIndex = CReplayCoordinator::GetIndexOfCurrentPlaybackClip();

	float const c_endTime = activeProject ? 
		activeProject->GetRawEndTimeAsTimeMs( c_currentClipIndex ) : CReplayCoordinator::GetReplayPlaybackController().GetClipRawEndTimeMs( -1 );

	return c_endTime;
}

bool CVideoProjectPlaybackController::CanJumpToBeatInCurrentClip() const
{
	CVideoEditorProject* activeProject = CVideoEditorInterface::GetActiveProject();
	s32 const c_currentClipIndex = CReplayCoordinator::GetIndexOfCurrentPlaybackClip();

	bool const c_result = activeProject ? 
		activeProject->DoesClipContainMusicClipWithBeatMarkers( (u32)c_currentClipIndex ) : false;

	return c_result;
}

float CVideoProjectPlaybackController::CalculateClosestAudioBeatNonDilatedTimeMs( float const nonDilatedTimeMs, bool const nextBeat ) const
{
	CReplayPlaybackController const& replayPlaybackController = CReplayCoordinator::GetReplayPlaybackControllerConst();

	s32 const c_clipIndex = replayPlaybackController.GetCurrentClipIndex();
	float const c_montageBeatTimeMs = CReplayMgr::GetClosestAudioBeatTimeMs( nonDilatedTimeMs, nextBeat );
	float const c_clipBeatTimeMs =  c_montageBeatTimeMs >= 0 ? replayPlaybackController.ConvertMusicBeatTimeToNonDilatedTimeMs( c_clipIndex, c_montageBeatTimeMs ) : -1;

	return c_clipBeatTimeMs;
}

float CVideoProjectPlaybackController::CalculateNextAudioBeatNonDilatedTimeMs( float const * const overrideTimeMs ) const
{
	CReplayPlaybackController const& replayPlaybackController = CReplayCoordinator::GetReplayPlaybackControllerConst();

	s32 const c_clipIndex = replayPlaybackController.GetCurrentClipIndex();
	float const c_montageBeatTimeMs = CReplayMgr::GetNextAudioBeatTime( overrideTimeMs );		
	float const c_clipBeatTimeMs =  c_montageBeatTimeMs >= 0 ? replayPlaybackController.ConvertMusicBeatTimeToNonDilatedTimeMs( c_clipIndex, c_montageBeatTimeMs ) : -1;

	return c_clipBeatTimeMs;
}

float CVideoProjectPlaybackController::CalculatePrevAudioBeatNonDilatedTimeMs( float const * const overrideTimeMs ) const
{
	CReplayPlaybackController const& replayPlaybackController = CReplayCoordinator::GetReplayPlaybackControllerConst();

	s32 const c_clipIndex = replayPlaybackController.GetCurrentClipIndex();
	float const c_montageBeatTimeMs = CReplayMgr::GetPrevAudioBeatTime( overrideTimeMs );		
	float const c_clipBeatTimeMs =  c_montageBeatTimeMs >= 0 ? replayPlaybackController.ConvertMusicBeatTimeToNonDilatedTimeMs( c_clipIndex, c_montageBeatTimeMs ) : -1;

	return c_clipBeatTimeMs;
}

void CVideoProjectPlaybackController::JumpToPreviousClip()
{
	CReplayPlaybackController& replayPlaybackController = CReplayCoordinator::GetReplayPlaybackController();
	s32 const c_targetClip = replayPlaybackController.GetCurrentClipIndex() - 1;
	if( c_targetClip >= 0 )
	{
		replayPlaybackController.JumpToClip( c_targetClip );
	}
	else
	{
		JumpToNonDilatedTimeMs( CReplayCoordinator::GetReplayPlaybackController().GetClipStartNonDilatedTimeMs( -1 ), false );
	}
}

void CVideoProjectPlaybackController::JumpToStart()
{
	if( IsEnabled() && !IsAtStartOfPlayback() )
	{
		CReplayMgr::StopPreCaching();
		Pause();

		if( m_playbackMode == PLAYBACK_RAW_CLIP )
		{
			// Jump to start of buffer
			JumpToNonDilatedTimeMs( 0, CReplayMgr::JO_FreezeRenderPhase );
		}
		else if( m_playbackMode == PLAYBACK_PROJECT )
		{
			CReplayPlaybackController& replayPlaybackController = CReplayCoordinator::GetReplayPlaybackController();
			CVideoEditorProject* activeProject = CVideoEditorInterface::GetActiveProject();
			
			if( GetCurrentClipIndex() == 0 )
			{
				// Jump to start of clip marker, rather than raw start of clip
				JumpToNonDilatedTimeMs( CReplayCoordinator::GetReplayPlaybackController().GetClipStartNonDilatedTimeMs( -1 ), false );
			}
			else
			{
				float const c_clipStartTimeMs = activeProject ? activeProject->GetClipStartNonDilatedTimeMs( 0 ) : 0;
				replayPlaybackController.JumpToClip( 0, c_clipStartTimeMs );
			}
		}
		else if( m_playbackMode >= PLAYBACK_CLIP )
		{
			// Jump to start of clip marker, rather than raw start of clip
			JumpToNonDilatedTimeMs( CReplayCoordinator::GetReplayPlaybackController().GetClipStartNonDilatedTimeMs( -1 ), false );
		}
	}
}

void CVideoProjectPlaybackController::ApplyRewind()
{
	if( IsEnabled() && !IsAtStartOfPlayback() )
	{
		float cursorSpeed = -1.f;

		if( CReplayMgr::IsCursorRewinding() )
		{
			cursorSpeed = CReplayMgr::GetCursorSpeed();
			cursorSpeed -= CURSOR_SPEED_INCREMENT;
		}
		else
		{
			CReplayMgr::SetNextPlayBackState( REPLAY_STATE_PLAY | REPLAY_STATE_FAST | REPLAY_DIRECTION_BACK );
		}

		// Clamp speed
		cursorSpeed = Clamp( cursorSpeed, -MAX_CURSOR_SPEED, -MIN_CURSOR_SPEED );
		CReplayMgr::SetCursorSpeed( cursorSpeed );
	}
}

void CVideoProjectPlaybackController::Play()
{
	if( IsEnabled() && IsPaused())
	{
		CReplayMgr::SetCursorSpeed( 1.f );
		CReplayMgr::SetNextPlayBackState( REPLAY_STATE_PLAY | REPLAY_DIRECTION_FWD | REPLAY_CURSOR_NORMAL );
	}
}

void CVideoProjectPlaybackController::Pause()
{
	if( IsEnabled() && IsPlaying() )
	{
		CReplayMgr::SetNextPlayBackState( REPLAY_STATE_PAUSE );
		CReplayMgr::KickPrecaching(false);
	}
}

void CVideoProjectPlaybackController::ApplyFastForward()
{
	if( IsEnabled() && !IsAtEndOfPlayback() )
	{
		float cursorSpeed = 1.5f;
		if( CReplayMgr::IsCursorFastForwarding() )
		{
			cursorSpeed = CReplayMgr::GetCursorSpeed();
			cursorSpeed += CURSOR_SPEED_INCREMENT;
		}
		else
		{
			CReplayMgr::SetNextPlayBackState( REPLAY_STATE_PLAY | REPLAY_STATE_FAST | REPLAY_DIRECTION_FWD );
		}

		// Clamp speed
		cursorSpeed = Clamp( cursorSpeed, MIN_CURSOR_SPEED, MAX_CURSOR_SPEED );
		CReplayMgr::SetCursorSpeed( cursorSpeed );
	}
}

void CVideoProjectPlaybackController::JumpToEnd()
{
	if( IsEnabled() && !IsAtEndOfPlayback() )
	{
		CReplayMgr::StopPreCaching();
		if( m_playbackMode == PLAYBACK_RAW_CLIP )
		{
			// Go to end of buffer.
			JumpToNonDilatedTimeMs( (float)CReplayMgr::GetTotalMsInBuffer(), CReplayMgr::JO_FreezeRenderPhase );
		}
		else if( m_playbackMode == PLAYBACK_PROJECT )
		{
			JumpToNonDilatedTimeMs( CReplayCoordinator::GetReplayPlaybackController().GetClipNonDilatedEndTimeMs( -1 ), false );
		}
		else if( m_playbackMode >= PLAYBACK_CLIP )
		{
			//  Jump to end of playback marker, rather than raw end of clip
			JumpToNonDilatedTimeMs( CReplayCoordinator::GetReplayPlaybackController().GetClipNonDilatedEndTimeMs( -1 ), false );
		}
	}
}

void CVideoProjectPlaybackController::JumpToNextClip()
{
	CReplayPlaybackController& replayPlaybackController = CReplayCoordinator::GetReplayPlaybackController();
	s32 const c_targetClip = replayPlaybackController.GetCurrentClipIndex() + 1;
	if( c_targetClip < replayPlaybackController.GetTotalClipCount() )
	{
		replayPlaybackController.JumpToClip( c_targetClip );
	}
	else
	{
		JumpToEnd();
	}
}

void CVideoProjectPlaybackController::TogglePlayOrPause()
{
	if( CReplayMgr::IsPlaying() )
	{
		Pause();
	}
	else if( CReplayMgr::IsPlaybackPaused() )
	{
		Play();
	}
}

void CVideoProjectPlaybackController::JumpToClip( u32 const clipIndex, float const clipNonDilatedTimeMs )
{
	CReplayPlaybackController& replayPlaybackController = CReplayCoordinator::GetReplayPlaybackController();
	if( GetCurrentClipIndex() != (s32)clipIndex && (s32)clipIndex < replayPlaybackController.GetTotalClipCount() )
	{
		float clipTimeToJumpToMs = clipNonDilatedTimeMs;

		if( clipNonDilatedTimeMs > -1 )
		{
			float const c_startTimeMs = CReplayCoordinator::GetReplayPlaybackController().GetClipStartNonDilatedTimeMs( clipIndex );
			float const c_endTimeMs = CReplayCoordinator::GetReplayPlaybackController().GetClipNonDilatedEndTimeMs( clipIndex );

			clipTimeToJumpToMs = Clamp( clipNonDilatedTimeMs, c_startTimeMs, c_endTimeMs );

			/*uiCondLogf( c_timeCodeClamped == (u32)clipNonDilatedTimeMs, DIAG_SEVERITY_WARNING, 
				"CVideoProjectPlaybackController::JumpToClip - Clamped jump time from %u to %u", clipNonDilatedTimeMs, c_timeCodeClamped );*/
		}

		replayPlaybackController.JumpToClip( clipIndex, clipTimeToJumpToMs );
	}
}

bool CVideoProjectPlaybackController::JumpToNonDilatedTimeMs( float const timecode, u32 jumpOptions )
{
	bool jumpRequested = false;

	if( IsEnabled() && timecode != CReplayMgr::GetCurrentTimeRelativeMs() )
	{
		float const c_startTimeMs = (float)CReplayCoordinator::GetReplayPlaybackController().GetClipStartNonDilatedTimeMs( -1 );
		float const c_endTimeMs = (float)CReplayCoordinator::GetReplayPlaybackController().GetClipNonDilatedEndTimeMs( -1 );

		float const c_timeCodeClamped = Clamp( timecode, c_startTimeMs, c_endTimeMs );

		/*uiCondLogf( c_timeCodeClamped == timecode, DIAG_SEVERITY_WARNING, 
			"CVideoProjectPlaybackController::JumpToNonDilatedTimeMs - Clamped jump time from %u to %u", timecode, c_timeCodeClamped );*/

		CReplayMgr::JumpToFloat( c_timeCodeClamped, jumpOptions );
		jumpRequested = true;
	}

	return jumpRequested;
}

bool CVideoProjectPlaybackController::IsJumping() const
{
	return IsEnabled() && CReplayMgr::IsJumping();
}

bool CVideoProjectPlaybackController::IsRewinding() const
{
	return CReplayMgr::IsCursorRewinding();
}

bool CVideoProjectPlaybackController::IsFastForwarding() const
{
	return CReplayMgr::IsCursorFastForwarding();
}

bool CVideoProjectPlaybackController::IsAtStartOfPlayback() const
{
	float const c_playbackTimeMs = GetPlaybackCurrentNonDilatedTimeMs();
	float const c_startTimeMs = GetPlaybackStartTimeMs();

	return c_playbackTimeMs <= c_startTimeMs;
}

bool CVideoProjectPlaybackController::IsAtEndOfPlayback() const
{
	float const c_playbackTimeMs = GetPlaybackCurrentTimeMs();
	float const c_endTimeMs = GetPlaybackEndTimeMs();

	return c_playbackTimeMs >= c_endTimeMs;
}

s32 CVideoProjectPlaybackController::GetCurrentClipIndex()
{
	return CReplayCoordinator::GetReplayPlaybackController().GetCurrentClipIndex();
}

float CVideoProjectPlaybackController::GetEffectiveCurrentClipTimeMs()
{
	return CReplayCoordinator::GetEffectiveCurrentClipTimeMs();
}

void CVideoProjectPlaybackController::Initialize( int playbackMode, bool const isEditing )
{
	if( uiVerifyf( playbackMode>= PLAYBACK_MIN, "CVideoProjectPlaybackController::Initialize - Unsupported playback mode %d requested!", playbackMode  ) ) 
	{
		m_playbackMode = playbackMode;
		m_isEditing = isEditing;
	}
}

void CVideoProjectPlaybackController::Reset()
{
	m_playbackMode = PLAYBACK_INVALID;
}

#endif // defined( GTA_REPLAY ) && GTA_REPLAY
