/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : ReplayPlaybackController.h
// PURPOSE : Implementation of Playback controller interface, to feed data to replay system.
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef _REPLAY_PLAYBACK_CONTROLLER_H_
#define _REPLAY_PLAYBACK_CONTROLLER_H_

#include "control/replay/ReplaySettings.h"

#if GTA_REPLAY

// game
#include "audio/environment/microphones.h"
#include "control/replay/IReplayPlaybackController.h"
#include "control/replay/ReplayMarkerInfo.h"

namespace rage
{
	class IVideoRecordingDataProvider;
}

class CMontage;
class CClip;
class IReplayMarkerStorage;

class CReplayPlaybackController : public IReplayPlaybackController
{
public:
	CReplayPlaybackController();
	virtual ~CReplayPlaybackController() 
	{ 
		Reset(); 
	}

	void Initialize( CMontage& montage, s32 const activeClip, bool const singleClipMode, IVideoRecordingDataProvider const * const exportMode );
	bool IsValid() const { return m_activeMontage != NULL && m_activeClip >= 0; }
	void Reset();

	//! Used for ReplayCoordinator level playback control.
	char const * GetProjectName() const;
	CMontage const * GetActiveMontage() const { return m_activeMontage; }
	IReplayMarkerStorage* TryGetCurrentMarkerStorage();
	IReplayMarkerStorage const* TryGetCurrentMarkerStorage() const;

	void UpdateMarkersForPlayback( float const clipTimeMs );
	void UpdateMontageAudioFadeOut();

	CClip const* GetClip( s32 clipIndex ) const;
	inline CClip* GetCurrentClip() { return GetClip( m_activeClip ); }
	inline CClip const* GetCurrentClip() const { return GetClip( m_activeClip ); }

	// IReplayPlaybackController implementation
	virtual bool IsExportingToVideoFile() const;
	virtual bool IsExportingPaused() const;
	virtual bool IsExportReadyForReplay() const;
	virtual bool IsExportingAudioFrameCaptureAllowed() const;
	virtual bool HasExportErrored() const;
	virtual float GetExportFrameDurationMs() const;

	virtual bool IsStartingClipNextFrame() const { return m_startClipNextFrame; }
	virtual void ResetStartClipFlag()			{ m_startClipNextFrame = false; }

	virtual u64 GetCurrentRawClipOwnerId() const;
	virtual char const * GetCurrentRawClipFileName() const;
	virtual bool GetPlaybackSingleClipOnly() const { return m_singleClip; }
		
	virtual s32 GetCurrentClipIndex() const;
	virtual s32 GetTotalClipCount() const;

	virtual float	GetClipStartNonDilatedTimeMs( s32 const clipIndex ) const;
	virtual float	GetClipNonDilatedEndTimeMs( s32 const clipIndex ) const;
	virtual float	GetClipNonDilatedDurationMs( s32 const clipIndex ) const;
	virtual float	GetClipRawEndTimeMs( s32 const clipIndex ) const;
	virtual float	GetClipTrimmedTimeMs( s32 const clipIndex ) const;

	virtual float GetLengthTimeToClipMs( s32 const clipIndex ) const;
	virtual u64 GetLengthTimeToClipNs( s32 const clipIndex ) const;
	virtual float GetLengthNonDilatedTimeToClipMs( s32 const clipIndex ) const;
	virtual float ConvertNonDilatedTimeToTimeMs( s32 const clipIndex, float const currentTimeMs ) const;
	virtual float ConvertTimeToNonDilatedTimeMs( s32 const clipIndex, float const currentTimeMS ) const;

	virtual float ConvertMusicBeatTimeToNonDilatedTimeMs( s32 const clipIndex, float const beatTimeMs ) const;
	virtual s32 GetMusicIndexAtCurrentNonDilatedTimeMs( s32 const clipIndex, float const currentTimeMs ) const;
	virtual s32 GetNextMusicIndexFromCurrentNonDilatedTimeMs( s32 const clipIndex, float const currentTimeMs ) const;
	virtual s32 GetPreviousMusicIndexFromCurrentNonDilatedTimeMs( s32 const clipIndex, float const currentTimeMs ) const;
	virtual s32 GetMusicStartTimeMs( u32 const musicIndex ) const;
	virtual s32 GetMusicStartOffsetMs( u32 const musicIndex ) const;
	virtual s32 GetMusicDurationMs( u32 const musicIndex ) const;
	virtual u32 GetMusicSoundHash( u32 const musicIndex ) const;
	virtual bool PauseMusicOnClipEnd() const;

	virtual s32 GetAmbientTrackIndexAtCurrentNonDilatedTimeMs( s32 const clipIndex, float const currentTimeMs ) const;
	virtual s32 GetAmbientTrackStartTimeMs( u32 const musicIndex ) const;
	virtual s32 GetAmbientTrackStartOffsetMs( u32 const musicIndex ) const;
	virtual s32 GetAmbientTrackDurationMs( u32 const musicIndex ) const;
	virtual u32 GetAmbientTrackSoundHash( u32 const musicIndex ) const;

	virtual bool CanUpdateGameTimer() const;

	virtual float GetTotalPlaybackTimeMs() const;

	virtual void JumpToClip( s32 const clipIndex, float const clipNonDilatedTimeMs = -1 );
	virtual s32 GetPendingJumpClip() const;
	virtual float GetPendingJumpClipNonDilatedTimeMs() const;
	virtual void ClearPendingJumpClipNonDilatedTimeMs();
	virtual bool IsPendingClipJump() const;

	virtual bool OnClipChanged( s32 const clipIndex );
	virtual void OnCurrentClipFinished();
	virtual void OnPlaybackFinished();

	virtual void OnClipPlaybackStart();
	virtual void OnClipPlaybackUpdate();
	virtual void OnClipPlaybackEnd();

private: // declarations and variables
	CMontage*							m_activeMontage;
	s32									m_activeClip;
	s32									m_pendingJumpClip;
	float								m_pendingJumpClipNonDilatedTimeMs;

	bool								m_singleClip;
	IVideoRecordingDataProvider const*	m_recordingDataProvider;

	bool								m_startClipNextFrame;
	bool								m_possibleFirstClipOfProject;

private: // methods
	void SetActiveClip( s32 const activeClip );

	CClip* GetClip( s32 clipIndex );

	static eReplayMicType GetMicrophoneType( eMarkerMicrophoneType const markerMicrophoneType );
};

#endif // GTA_REPLAY

#endif // _REPLAY_PLAYBACK_CONTROLLER_H_
