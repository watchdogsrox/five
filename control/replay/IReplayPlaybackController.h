/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : IReplayPlaybackController.cpp
// PURPOSE : Interface for injecting higher-level playback detail into the replay system. Allows for multiple
//			 sequential clip playback, rendering, etc... 
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef _I_REPLAY_PLAYBACK_CONTROLLER_H_
#define _I_REPLAY_PLAYBACK_CONTROLLER_H_

#include "ReplaySettings.h"

#if GTA_REPLAY

// rage
#include "math/vecmath.h"

// game
#include "ReplayMarkerInfo.h"

class IReplayPlaybackController
{
public:

	//! Functions used to pass data to the replay system from interface
	virtual bool IsExportingToVideoFile() const = 0;
	virtual bool IsExportingPaused() const = 0;
	virtual bool IsExportReadyForReplay() const = 0;
	virtual bool HasExportErrored() const = 0;
	virtual float GetExportFrameDurationMs() const = 0;

	virtual bool IsStartingClipNextFrame() const = 0;
	virtual void ResetStartClipFlag() = 0;

	virtual u64 GetCurrentRawClipOwnerId() const = 0;
	virtual char const * GetCurrentRawClipFileName() const = 0;
	virtual bool GetPlaybackSingleClipOnly() const = 0;

	virtual s32 GetCurrentClipIndex() const = 0;
	virtual s32 GetTotalClipCount() const = 0;
	inline bool IsCurrentClipLastClip() const { return GetCurrentClipIndex() == GetTotalClipCount() - 1; }

	virtual float	GetClipStartNonDilatedTimeMs( s32 const clipIndex ) const = 0;
	virtual float	GetClipNonDilatedEndTimeMs( s32 const clipIndex ) const = 0;
	virtual float	GetClipNonDilatedDurationMs( s32 const clipIndex ) const = 0;
	virtual float	GetClipRawEndTimeMs( s32 const clipIndex ) const = 0;
	virtual float	GetClipTrimmedTimeMs( s32 const clipIndex ) const = 0;

	virtual float GetLengthTimeToClipMs( s32 const clipIndex ) const = 0;
	virtual u64 GetLengthTimeToClipNs( s32 const clipIndex ) const = 0;
	virtual float GetLengthNonDilatedTimeToClipMs( s32 const clipIndex ) const = 0;
	virtual float ConvertNonDilatedTimeToTimeMs( s32 const clipIndex, float const currentTimeMs ) const = 0;
	virtual float ConvertTimeToNonDilatedTimeMs( s32 const clipIndex, float const currentTimeMs ) const = 0;

	virtual float ConvertMusicBeatTimeToNonDilatedTimeMs( s32 const clipIndex, float const beatTimeMs ) const = 0;
	virtual s32 GetMusicIndexAtCurrentNonDilatedTimeMs( s32 const clipIndex, float const currentTimeMs ) const = 0;
	virtual s32 GetNextMusicIndexFromCurrentNonDilatedTimeMs( s32 const clipIndex, float const currentTimeMs ) const = 0;
	virtual s32 GetPreviousMusicIndexFromCurrentNonDilatedTimeMs( s32 const clipIndex, float const currentTimeMs ) const = 0;
	virtual s32 GetMusicStartTimeMs( u32 const musicIndex ) const = 0;
	virtual s32 GetMusicStartOffsetMs( u32 const musicIndex ) const = 0;
	virtual s32 GetMusicDurationMs( u32 const musicIndex ) const = 0;
	virtual u32 GetMusicSoundHash( u32 const musicIndex ) const = 0;
	virtual bool PauseMusicOnClipEnd() const = 0;

	virtual s32 GetAmbientTrackIndexAtCurrentNonDilatedTimeMs( s32 const clipIndex, float const currentTimeMs ) const = 0;
	virtual s32 GetAmbientTrackStartTimeMs( u32 const musicIndex ) const = 0;
	virtual s32 GetAmbientTrackStartOffsetMs( u32 const musicIndex ) const = 0;
	virtual s32 GetAmbientTrackDurationMs( u32 const musicIndex ) const = 0;
	virtual u32 GetAmbientTrackSoundHash( u32 const musicIndex ) const = 0;


	virtual bool CanUpdateGameTimer() const = 0;
	virtual float GetTotalPlaybackTimeMs() const = 0;	

	// Used for arbitrary jumping, not standard playback jumping
	virtual void JumpToClip( s32 const clipIndex, float const clipTimeMs ) = 0;
	virtual s32 GetPendingJumpClip() const = 0;
	virtual float GetPendingJumpClipNonDilatedTimeMs() const = 0;
	virtual void ClearPendingJumpClipNonDilatedTimeMs() = 0;
	virtual bool IsPendingClipJump() const = 0;

	//! Callbacks called from Replay system to interface
	virtual bool OnClipChanged( s32 const clipIndex ) = 0;
	virtual void OnCurrentClipFinished() = 0;
	virtual void OnPlaybackFinished() = 0;

	virtual void OnClipPlaybackStart() = 0;
	virtual void OnClipPlaybackUpdate() = 0;
	virtual void OnClipPlaybackEnd() = 0;
};

#endif // GTA_REPLAY

#endif // _I_REPLAY_PLAYBACK_CONTROLLER_H_
