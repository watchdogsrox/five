/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : VideoProjectPlaybackController.h
// PURPOSE : Represents instance data for controlling a video preview playback.
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#include "control/replay/ReplaySettings.h"

#if defined( GTA_REPLAY ) && GTA_REPLAY

#ifndef VIDEO_PROJECT_PLAYBACK_CONTROLLER_H_
#define VIDEO_PROJECT_PLAYBACK_CONTROLLER_H_

class CVideoEditorInterface;
class IReplayMarkerStorage;

class CVideoProjectPlaybackController
{
public:
	bool IsPlaying() const;
	bool IsPaused() const;

	float SetCursorSpeed( float const speed );
	float GetCursorSpeed() const;

	bool IsEnabled() const;
	bool IsEditing() const;
	bool CanQueryStartAndEndTimes() const;

	float GetPlaybackStartTimeMs() const;
	float GetPlaybackEndTimeMs() const;

	float GetPlaybackCurrentNonDilatedTimeMs() const;
	float GetPlaybackCurrentTimeMs() const;

	float GetPlaybackCurrentClipStartNonDilatedTimeMs() const;
	float GetPlaybackCurrentClipCurrentNonDilatedTimeMs() const;
	float GetPlaybackCurrentClipEndNonDilatedTimeMs() const;
	float GetPlaybackCurrentClipRawEndTimeMsAsTimeMs() const;

	bool CanJumpToBeatInCurrentClip() const;
	float CalculateClosestAudioBeatNonDilatedTimeMs( float const nonDilatedTimeMs, bool const nextBeat ) const;
	float CalculateNextAudioBeatNonDilatedTimeMs( float const * const overrideTimeMs = NULL ) const;
	float CalculatePrevAudioBeatNonDilatedTimeMs( float const * const overrideTimeMs = NULL ) const;

	void JumpToPreviousClip();
	void JumpToStart();

	void ApplyRewind();
	void Play();
	void Pause();
	void ApplyFastForward();

	void JumpToEnd();
	void JumpToNextClip();

	void TogglePlayOrPause();

	void JumpToClip( u32 const clipIndex, float const clipNonDilatedTimeMs );
	bool JumpToNonDilatedTimeMs( float const timecode, u32 jumpOptions );
	bool IsJumping() const;

	bool IsRewinding() const;
	bool IsFastForwarding() const;

	bool IsAtStartOfPlayback() const;
	bool IsAtEndOfPlayback() const;

	static s32 GetCurrentClipIndex();
	static float GetEffectiveCurrentClipTimeMs();

private: // declarations and variables
	friend class CVideoEditorInterface; // for private construction

	enum ePlaybackMode
	{
		PLAYBACK_INVALID = -3,

		PLAYBACK_MIN = -2,

		PLAYBACK_RAW_CLIP = PLAYBACK_MIN,
		PLAYBACK_PROJECT = -1,
		PLAYBACK_CLIP = 0		// Greater/Equal to this? Assume we are only viewing that specific clip.
	};

	int m_playbackMode;
	bool m_isEditing;
	
private: // methods
	
	CVideoProjectPlaybackController();
	~CVideoProjectPlaybackController();

	void Initialize( int playbackMode, bool const isEditing );
	void Reset();

};

#endif // VIDEO_PROJECT_PLAYBACK_CONTROLLER_H_

#endif // defined( GTA_REPLAY ) && GTA_REPLAY
