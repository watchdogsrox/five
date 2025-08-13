/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : ReplayPlaybackController.cpp
// PURPOSE : Implementation of Playback controller interface, to feed data to replay system.
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#include "ReplayPlaybackController.h"

#if GTA_REPLAY

// rage
#include "math/amath.h"

// framework
#include "video/IVideoRecordingDataAdapter.h"

// game
#include "audio/frontendaudioentity.h"
#include "audio/music/musicplayer.h"
#include "audio/replayaudioentity.h"
#include "camera/replay/ReplayDirector.h"
#include "camera/CamInterface.h"
#include "control/replay/replay.h"
#include "control/replay/ReplayMarkerContext.h"
#include "replaycoordinator/ReplayCoordinator.h"
#include "replaycoordinator/storage/Montage.h"


REPLAY_COORDINATOR_OPTIMISATIONS();

CReplayPlaybackController::CReplayPlaybackController()
	: m_activeMontage( NULL )
	, m_activeClip( INDEX_NONE )
	, m_pendingJumpClip( INDEX_NONE )
	, m_pendingJumpClipNonDilatedTimeMs( INDEX_NONE )
	, m_singleClip( true )
	, m_recordingDataProvider( NULL )
	, m_startClipNextFrame( 0 )
{

}

void CReplayPlaybackController::Initialize( CMontage& montage, s32 const activeClip, bool const singleClipMode, 
										   IVideoRecordingDataProvider const * const exportMode )
{
	CReplayMarkerContext::Reset();
	m_activeMontage = &montage;
	m_singleClip = singleClipMode;
	m_recordingDataProvider = exportMode;
	SetActiveClip( activeClip );
	ClearPendingJumpClipNonDilatedTimeMs();
	m_possibleFirstClipOfProject = true;
}

void CReplayPlaybackController::Reset()
{
	CReplayMarkerContext::Reset();
	SetActiveClip( INDEX_NONE );
	m_activeMontage = NULL;
	m_singleClip = true;
	m_recordingDataProvider = NULL;
	ClearPendingJumpClipNonDilatedTimeMs();
	m_possibleFirstClipOfProject = false;
}

char const * CReplayPlaybackController::GetProjectName() const
{
	return IsValid() ? m_activeMontage->GetName() : "";
}

IReplayMarkerStorage* CReplayPlaybackController::TryGetCurrentMarkerStorage()
{
	return IsValid() ? (IReplayMarkerStorage*)GetCurrentClip() : NULL;
}

IReplayMarkerStorage const* CReplayPlaybackController::TryGetCurrentMarkerStorage() const
{
	return IsValid() ? (IReplayMarkerStorage*)GetCurrentClip() : NULL;
}

void CReplayPlaybackController::UpdateMarkersForPlayback( float const clipTimeMs )
{
	IReplayMarkerStorage* markerStorage = TryGetCurrentMarkerStorage();
	if( markerStorage == NULL )
	{
		return;
	}

	sReplayMarkerInfo* pCurrentMarker = NULL;
	sReplayMarkerInfo* pNextMarker = NULL;

	// Reset current marker index.
	CReplayMarkerContext::SetCurrentMarkerIndex( -1 );

	int const c_markerCount = markerStorage->GetMarkerCount();
	if( c_markerCount == 0 )
	{
		CReplayMgr::SetMarkerSpeed( 1.f );
	}

	for( int i = 0; i < c_markerCount; ++i )
	{
		pCurrentMarker = markerStorage->GetMarker( i );
		pNextMarker = markerStorage->TryGetMarker( i + 1 );

		g_ReplayAudioEntity.SetMarkerSFXValid(i, true);
		if(pCurrentMarker)
		{
			if(pCurrentMarker->GetSfxOSVolume() == 0)
			{
				g_ReplayAudioEntity.SetMarkerSFXValid(i, false);
			}
			else if(pNextMarker)
			{
				static float minTimeBetweenSFXMarkers = 450.0f;
				f32 currentMarkerTime = ConvertNonDilatedTimeToTimeMs(GetCurrentClipIndex(), pCurrentMarker->GetNonDilatedTimeMs());
				f32 nextMarkerTime = ConvertNonDilatedTimeToTimeMs(GetCurrentClipIndex(), pNextMarker->GetNonDilatedTimeMs());
				if(pNextMarker->GetSfxHash() != g_NullSoundHash && ((nextMarkerTime - currentMarkerTime) < minTimeBetweenSFXMarkers)	)
				{
					g_ReplayAudioEntity.SetMarkerSFXValid(i, false);
				}
			}
		}
		
		if( pCurrentMarker && clipTimeMs >= pCurrentMarker->GetNonDilatedTimeMs() && 
			( !pNextMarker || ( pNextMarker && clipTimeMs < pNextMarker->GetNonDilatedTimeMs() ) )  )
		{
			CReplayMarkerContext::SetCurrentMarkerIndex( i );

			//Set Speed.
			if ( !pCurrentMarker->IsAnchor() && pCurrentMarker != markerStorage->GetLastMarker())
			{
				eReplayMicType const c_micType = GetMicrophoneType( (eMarkerMicrophoneType)pCurrentMarker->GetMicrophoneType() );
				naMicrophones::SetReplayMicType( (u8)c_micType );

				if( CReplayMgr::IsPlaying() )
				{
					CReplayMgr::SetMarkerSpeed( pCurrentMarker->GetSpeedValueFloat() );
				}

				// Need to update volumes even whilst not playing so that we start playback on the correct volume
				g_FrontendAudioEntity.SetClipSpeechOn( pCurrentMarker->IsSpeechOn() );
				if(!g_ReplayAudioEntity.ShouldDuckMusic()) // once we're ducking only the play marker of the current SFX can control the music volume
				{
					g_FrontendAudioEntity.SetClipMusicVolumeIndex( pCurrentMarker->GetMusicVolume() );
				}
				g_FrontendAudioEntity.SetClipSFXVolumeIndex( pCurrentMarker->GetSfxVolume() );
				g_FrontendAudioEntity.SetClipDialogVolumeIndex( pCurrentMarker->GetDialogVolume() );
                g_FrontendAudioEntity.SetClipCustomAmbienceVolumeIndex( pCurrentMarker->GetAmbientVolume() );
				if(pCurrentMarker->GetSfxHash() != g_NullSoundHash && g_ReplayAudioEntity.IsMarkerSFXValid(i))
				{
					g_FrontendAudioEntity.SetClipSFXOneshotVolumeIndex( pCurrentMarker->GetSfxOSVolume() );
				}
                
				float timeSinceIntensitySwitch = 0;
				bool switchMoodsInstantly = false;
				bool isDoingFullPlayback = CReplayCoordinator::IsExportingToVideoFile() || CReplayCoordinator::IsPreviewingVideo();

				// If this is the very first clip, or we're just editing a single clip, or we're editing a single clip and we've paused,
				// then switch moods instantly
				if((pCurrentMarker == markerStorage->GetFirstMarker() && GetCurrentClipIndex() == 0) || 
				   (!isDoingFullPlayback && !CReplayMgr::IsPlaying()))
				{
					switchMoodsInstantly = true;
				}

				eReplayMarkerAudioIntensity prevReplayMarkerIntensity = MARKER_AUDIO_INTENSITY_MAX;

				if(IsValid() && !isDoingFullPlayback)
				{
					// Need to find the previous music intensity marke so that we can correctly reproduce any
					// intensity crossfade behaviour if playback resumes mid-crossfade
					for( int prevMarkerIndex = i - 1; prevMarkerIndex >= 0; prevMarkerIndex-- )
					{
						sReplayMarkerInfo* pPrevMarker = markerStorage->GetMarker( prevMarkerIndex );

						if(pPrevMarker && pPrevMarker->GetScoreIntensity() != pCurrentMarker->GetScoreIntensity())
						{
							prevReplayMarkerIntensity = eReplayMarkerAudioIntensity( pPrevMarker->GetScoreIntensity() );
							CClip* clip = m_activeMontage->GetClip(m_activeClip);
							float currentTimeMs = CReplayMgr::GetCurrentTimeRelativeMsFloat();
							float prevMarkerTimeMs = clip->ConvertNonDilatedTimeMsToTimeMs( pCurrentMarker->GetNonDilatedTimeMs() );
							timeSinceIntensitySwitch = currentTimeMs - prevMarkerTimeMs;
							break;
						}
					}
				}

				g_InteractiveMusicManager.SetReplayScoreIntensity( eReplayMarkerAudioIntensity( pCurrentMarker->GetScoreIntensity() ), prevReplayMarkerIntensity, switchMoodsInstantly, (u32)timeSinceIntensitySwitch );
			}
		}
		else
		{
			if (pCurrentMarker == markerStorage->GetFirstMarker() && clipTimeMs < pCurrentMarker->GetNonDilatedTimeMs() )
			{

				g_FrontendAudioEntity.SetClipSpeechOn(g_FrontendAudioEntity.GetSpeechOn());	// if we are before a marker then use the global setting.
				g_FrontendAudioEntity.SetClipMusicVolumeIndex(8);
				g_FrontendAudioEntity.SetClipSFXVolumeIndex(8);
				g_FrontendAudioEntity.SetClipDialogVolumeIndex(8);
                g_FrontendAudioEntity.SetClipCustomAmbienceVolumeIndex(8);
                g_FrontendAudioEntity.SetClipSFXOneshotVolumeIndex(8);
				CReplayMgr::SetMarkerSpeed( 1.f );
			}
		}

	}	

	g_ReplayAudioEntity.SetSfxVolumeAffectingMusic(g_SilenceVolume); // base line is no sfx volume so no duck
	// replay marker one shot sfx
	if(c_markerCount > 1)// && (CReplayMgr::IsPreCachingScene() || CReplayMgr::IsJustPlaying()))
	{
		g_ReplayAudioEntity.SetShouldDuckMusic(false);
		g_ReplayAudioEntity.InvalidateSFX();
		bool foundPlayCandidate = false;
		for( int i = c_markerCount-1; i >= 0; i-- )
		{
			pCurrentMarker = markerStorage->GetMarker( i );

			if(!pCurrentMarker->IsAnchor())
			{
				u32 const c_sfxHash = pCurrentMarker->GetSfxHash();
				u32 sfxDuration = c_sfxHash != g_NullSoundHash ? CReplayAudioTrackProvider::GetMusicTrackDurationMs(c_sfxHash) : 0;

				// Assuming that the SFX doesn't get pitched up/down by replay marker speeds, the duration is measured in real (ie. dilated) time. 
				// We need to know the end time in non dilated time so that we can compare it to the current playback head.
				f32 const sfxEndTimeMs = ConvertTimeToNonDilatedTimeMs(GetCurrentClipIndex(), ConvertNonDilatedTimeToTimeMs(GetCurrentClipIndex(), pCurrentMarker->GetNonDilatedTimeMs()) + sfxDuration);
				if(pCurrentMarker && c_sfxHash != g_NullSoundHash && g_ReplayAudioEntity.IsMarkerSFXValid(i))
				{
					if(!foundPlayCandidate && clipTimeMs >= pCurrentMarker->GetNonDilatedTimeMs() - REPLAY_SFX_PRELAOD_MS && clipTimeMs < sfxEndTimeMs)
					{
						u32 startOffset = 0;
						if(clipTimeMs > pCurrentMarker->GetNonDilatedTimeMs())
						{   
							// As above, assuming that SFX doesn't get pitched up or down, we need to calculate the offset based on the real (ie. dilated) time since the marker was hit
							startOffset = (u32)(ConvertNonDilatedTimeToTimeMs(GetCurrentClipIndex(), clipTimeMs) - ConvertNonDilatedTimeToTimeMs(GetCurrentClipIndex(), pCurrentMarker->GetNonDilatedTimeMs()));
						}
						if(startOffset < sfxDuration)
						{
							g_ReplayAudioEntity.Preload(i, c_sfxHash, REPLAY_STREAM_SFX, startOffset);
						}
					}
					else
					{
						g_ReplayAudioEntity.Stop(i, c_sfxHash, true);
					}
					if(!foundPlayCandidate && clipTimeMs >= pCurrentMarker->GetNonDilatedTimeMs())
					{
						foundPlayCandidate = true;
					}

					if(clipTimeMs >= pCurrentMarker->GetNonDilatedTimeMs() - 1000 && clipTimeMs < sfxEndTimeMs && g_ReplayAudioEntity.IsLoadedOrPlaying(i, c_sfxHash))
					{
						//should duck music
						f32 sfxVolume = g_FrontendAudioEntity.VolumeIndexToDb(pCurrentMarker->GetSfxOSVolume());
						f32 sfxVolumeAffectingMusic = g_ReplayAudioEntity.GetSFXVolumeAffectingMusic();
						if(sfxVolume > sfxVolumeAffectingMusic) // music is affected by the loudest SFX volume within the window
						{
							g_ReplayAudioEntity.SetSfxVolumeAffectingMusic(sfxVolume);
						}
						g_ReplayAudioEntity.SetShouldDuckMusic(true);
					}
				}

				if(CReplayMgr::IsJustPlaying() && !CReplayMgr::IsSettingUp() && !CReplayMgr::IsPreCachingScene())
				{
					if(c_sfxHash != g_NullSoundHash && clipTimeMs >= pCurrentMarker->GetNonDilatedTimeMs() && clipTimeMs < sfxEndTimeMs && g_ReplayAudioEntity.IsReadyToPlay(i, c_sfxHash))
					{
						Displayf("Play SFX on marker %d", i);
						// stop any playing sfx before we start a new one
						g_ReplayAudioEntity.StopAllPlayingSFX();
						g_ReplayAudioEntity.Play(i, c_sfxHash);
						g_FrontendAudioEntity.SetClipMusicVolumeIndex( pCurrentMarker->GetMusicVolume() );
					}
				}
			}
		}
		g_ReplayAudioEntity.ClearInvalidSFX();
	}
			
	s32 currIndex = CReplayMarkerContext::GetCurrentMarkerIndex();
	CReplayMarkerContext::SetPreviousActiveMarkerIndex( currIndex ); 
}

void CReplayPlaybackController::UpdateMontageAudioFadeOut()
{
	const f32 currentTimeMs = CReplayMgr::GetCurrentTimeRelativeMsFloat();
	f32 sfxVolLinear = 1.0f;

	if(IsValid())
	{
		f32 fraction = 0.0f;
		ReplayMarkerTransitionType transition;	
		u32 clipIndex = GetCurrentClipIndex();
		clipIndex = CReplayCoordinator::IsPendingNextClip() && clipIndex > 0 ? clipIndex - 1 : clipIndex;
		m_activeMontage->GetActiveTransition(clipIndex, currentTimeMs, transition, fraction);

		if(transition != MARKER_TRANSITIONTYPE_MAX && 
		   transition != MARKER_TRANSITIONTYPE_NONE)
		{
			sfxVolLinear = 1.0f - fraction;
		}		
	}

	g_FrontendAudioEntity.SetClipFadeVolLinear(sfxVolLinear);
}

CClip const* CReplayPlaybackController::GetClip( s32 clipIndex ) const
{
	clipIndex = clipIndex < 0 ? m_activeClip : clipIndex;
	return IsValid() ? m_activeMontage->GetClip( clipIndex ) : NULL;
}

bool CReplayPlaybackController::IsExportingToVideoFile() const
{
	return m_recordingDataProvider && m_recordingDataProvider->IsRecordingVideo();
}

bool CReplayPlaybackController::IsExportingPaused() const
{
	return m_recordingDataProvider && m_recordingDataProvider->IsRecordingPaused();
}

bool CReplayPlaybackController::IsExportReadyForReplay() const
{
	return m_recordingDataProvider && m_recordingDataProvider->IsRecordingReadyForSamples();
}

bool CReplayPlaybackController::IsExportingAudioFrameCaptureAllowed() const
{
	return m_recordingDataProvider && m_recordingDataProvider->IsAudioFrameCaptureAllowed();
}

bool CReplayPlaybackController::HasExportErrored() const
{
	return m_recordingDataProvider && m_recordingDataProvider->HasRecordingErrored();
}

float CReplayPlaybackController::GetExportFrameDurationMs() const
{
	return m_recordingDataProvider ? m_recordingDataProvider->GetExportFrameDurationMs() : 33.f;
}

u64 CReplayPlaybackController::GetCurrentRawClipOwnerId() const
{
	return IsValid() ? m_activeMontage->GetClip( m_activeClip )->GetOwnerId() : 0;
}

char const * CReplayPlaybackController::GetCurrentRawClipFileName() const
{
	return IsValid() ? m_activeMontage->GetClip( m_activeClip )->GetName() : NULL;
}

s32 CReplayPlaybackController::GetCurrentClipIndex() const
{
	return IsValid() ? m_activeClip : -1;
}

s32 CReplayPlaybackController::GetTotalClipCount() const
{
	return IsValid() ? m_activeMontage->GetClipCount() : -1;
}

float CReplayPlaybackController::GetClipStartNonDilatedTimeMs( s32 const clipIndex ) const
{
	CClip const * activeClip = GetClip( clipIndex );
	return activeClip ? activeClip->GetStartNonDilatedTimeMs() : 0;
}

float CReplayPlaybackController::GetClipNonDilatedEndTimeMs( s32 const clipIndex ) const
{
	CClip const * activeClip = GetClip( clipIndex );
	return activeClip ? activeClip->GetEndNonDilatedTimeMs() : 0;
}

float CReplayPlaybackController::GetClipNonDilatedDurationMs( s32 const clipIndex ) const
{
	CClip const * activeClip = GetClip( clipIndex );
	return activeClip ? activeClip->GetRawDurationMs() : 0;
}

float CReplayPlaybackController::GetClipRawEndTimeMs( s32 const clipIndex ) const
{
	CClip const * activeClip = GetClip( clipIndex );
	return activeClip ? activeClip->GetRawDurationMs() : 0;
}

float CReplayPlaybackController::GetClipTrimmedTimeMs( s32 const clipIndex ) const
{
	CClip const * activeClip = GetClip( clipIndex );
	return activeClip ? activeClip->GetTrimmedTimeMs() : 0;
}

float CReplayPlaybackController::GetLengthTimeToClipMs( s32 const clipIndex ) const
{
	return IsValid() ? m_activeMontage->GetTrimmedTimeToClipMs( clipIndex ) : 0;
}

u64 CReplayPlaybackController::GetLengthTimeToClipNs( s32 const clipIndex ) const
{
	return IsValid() ? m_activeMontage->GetTrimmedTimeToClipNs( clipIndex ) : 0;
}

float CReplayPlaybackController::GetLengthNonDilatedTimeToClipMs( s32 const clipIndex ) const
{
	return IsValid() ? m_activeMontage->GetTrimmedNonDilatedTimeToClipMs( clipIndex ) : 0;
}

float CReplayPlaybackController::ConvertNonDilatedTimeToTimeMs( s32 const clipIndex, float const currentTimeMs ) const
{
	float realTimeMs = (float)GetLengthTimeToClipMs( clipIndex );

	CClip const* activeClip = GetClip( clipIndex );
	if( activeClip )
	{
		realTimeMs += activeClip->ConvertNonDilatedTimeMsToTimeMs( currentTimeMs );
	}

	return realTimeMs;
}

float CReplayPlaybackController::ConvertTimeToNonDilatedTimeMs( s32 const clipIndex, float const currentTimeMS ) const
{
	float nonDilatedTimeMS = GetLengthNonDilatedTimeToClipMs( clipIndex );

	CClip const* activeClip = GetClip( clipIndex );
	if( activeClip )
	{
		nonDilatedTimeMS += activeClip->ConvertTimeMsToNonDilatedTimeMs( currentTimeMS );
	}

	return nonDilatedTimeMS;
}

float CReplayPlaybackController::ConvertMusicBeatTimeToNonDilatedTimeMs( s32 const clipIndex, float const beatTimeMs ) const
{
	float clipBeatTime = -1.f;

	if( IsValid() )
	{		
		s32 beatClipIndex = -1;

		// Given a montage-relative music beat time, convert this to an offset within the given clip
		m_activeMontage->ConvertMusicBeatTimeToNonDilatedTimeMs( beatTimeMs, beatClipIndex, clipBeatTime );

		// Check that the beat is actually within the selected clip
		clipBeatTime = (beatClipIndex == clipIndex) ? clipBeatTime : -1.f;
	}

	return clipBeatTime;
}

s32 CReplayPlaybackController::GetMusicIndexAtCurrentNonDilatedTimeMs( s32 const clipIndex, float const currentTimeMs ) const
{
	s32 const index = IsValid() ? m_activeMontage->GetActiveMusicIndex( clipIndex, currentTimeMs ) : -1;
	return index;
}

s32 CReplayPlaybackController::GetNextMusicIndexFromCurrentNonDilatedTimeMs( s32 const clipIndex, float const currentTimeMs ) const
{
	s32 const index = IsValid() ? m_activeMontage->GetNextActiveMusicIndex( clipIndex, currentTimeMs ) : -1;
	return index;
}

s32 CReplayPlaybackController::GetPreviousMusicIndexFromCurrentNonDilatedTimeMs( s32 const clipIndex, float const currentTimeMs ) const
{
	s32 const index = IsValid() ? m_activeMontage->GetPreviousActiveMusicIndex( clipIndex, currentTimeMs ) : -1;
	return index;
}

s32 CReplayPlaybackController::GetMusicStartTimeMs( u32 const musicIndex ) const
{
	return IsValid() ? m_activeMontage->GetMusicStartTimeMs( musicIndex ) : -1;
}

s32 CReplayPlaybackController::GetMusicStartOffsetMs( u32 const musicIndex ) const
{
	return IsValid() ? m_activeMontage->GetMusicStartOffsetMs( musicIndex ) : -1;
}

s32 CReplayPlaybackController::GetMusicDurationMs( u32 const musicIndex ) const
{
	return IsValid() ? m_activeMontage->GetMusicDurationMs( musicIndex ) : -1;
}

u32 CReplayPlaybackController::GetMusicSoundHash( u32 const musicIndex ) const
{
	return IsValid() ? m_activeMontage->GetMusicClipSoundHash( musicIndex ) : 0;
}

bool CReplayPlaybackController::PauseMusicOnClipEnd() const
{
	return IsValid() && !CReplayCoordinator::IsRenderingVideo();
}

//Ambient track

s32 CReplayPlaybackController::GetAmbientTrackIndexAtCurrentNonDilatedTimeMs( s32 const clipIndex, float const currentTimeMs ) const
{
	s32 const index = IsValid() ? m_activeMontage->GetActiveAmbientTrackIndex( clipIndex, currentTimeMs ) : -1;
	return index;
}

s32 CReplayPlaybackController::GetAmbientTrackStartTimeMs( u32 const musicIndex ) const
{
	return IsValid() ? m_activeMontage->GetAmbientStartTimeMs( musicIndex ) : -1;
}

s32 CReplayPlaybackController::GetAmbientTrackStartOffsetMs( u32 const musicIndex ) const
{
	return IsValid() ? m_activeMontage->GetAmbientStartOffsetMs( musicIndex ) : -1;
}

s32 CReplayPlaybackController::GetAmbientTrackDurationMs( u32 const musicIndex ) const
{
	return IsValid() ? m_activeMontage->GetAmbientDurationMs( musicIndex ) : -1;
}

u32 CReplayPlaybackController::GetAmbientTrackSoundHash( u32 const musicIndex ) const
{
	return IsValid() ? m_activeMontage->GetAmbientClipSoundHash( musicIndex ) : 0;
}

bool CReplayPlaybackController::CanUpdateGameTimer() const
{
	return IsValid();
}

float CReplayPlaybackController::GetTotalPlaybackTimeMs() const
{
	return IsValid() ? m_activeMontage->GetTotalClipLengthTimeMs() : 0;
}

void CReplayPlaybackController::JumpToClip( s32 const clipIndex, float const clipNonDilatedTimeMs )
{
	m_pendingJumpClip = clipIndex;
	m_pendingJumpClipNonDilatedTimeMs = clipNonDilatedTimeMs;
}

s32 CReplayPlaybackController::GetPendingJumpClip() const
{
	return m_pendingJumpClip;
}

float CReplayPlaybackController::GetPendingJumpClipNonDilatedTimeMs() const
{
	return m_pendingJumpClipNonDilatedTimeMs;
}

void CReplayPlaybackController::ClearPendingJumpClipNonDilatedTimeMs()
{
	m_pendingJumpClipNonDilatedTimeMs = INDEX_NONE;
}

bool CReplayPlaybackController::IsPendingClipJump() const
{
	return GetPendingJumpClip() != INDEX_NONE;
}

bool CReplayPlaybackController::OnClipChanged( s32 const clipIndex )
{
	bool success = false;

	if( GetClip( clipIndex ) != NULL )
	{
		if( clipIndex != m_activeClip )
		{
			OnCurrentClipFinished();
		}

		SetActiveClip( clipIndex );
		success = true;
	}

	m_pendingJumpClip = INDEX_NONE;
	return success;
}

void CReplayPlaybackController::OnCurrentClipFinished()
{

}

void CReplayPlaybackController::OnPlaybackFinished()
{
	if( CReplayCoordinator::IsRenderingVideo() )
	{
		CReplayCoordinator::KillPlaybackOrBake( false );
	}
}

void CReplayPlaybackController::OnClipPlaybackStart()
{
	//ensure the correct playback states for the various modes are set correctly
	if( CReplayCoordinator::IsRenderingVideo() )
	{
#if USE_SRLS_IN_REPLAY
		if ( CReplayMgr::IsCreatingSRL())
		{
			CReplayMgr::SetNextPlayBackState(REPLAY_STATE_PLAY | REPLAY_DIRECTION_FWD);
		}
		else
#endif
		{
			CReplayMgr::SetNextPlayBackState(REPLAY_STATE_PAUSE);
			m_startClipNextFrame = true;
		}
	}
	else if(CReplayCoordinator::IsPreviewingClip())
	{
		CReplayMgr::SetNextPlayBackState(REPLAY_STATE_PAUSE);
	}
	else if( CReplayCoordinator::IsPreviewingVideo())
	{
		if(m_possibleFirstClipOfProject && m_activeClip == 0)
		{
			CReplayMgr::SetNextPlayBackState(REPLAY_STATE_PAUSE);
			m_startClipNextFrame = true;
		}

		m_possibleFirstClipOfProject = false;
	}
}

void CReplayPlaybackController::OnClipPlaybackEnd()
{
	if( CReplayCoordinator::IsRenderingVideo() )
	{
		audNorthAudioEngine::PumpReplayAudio();
		CReplayCoordinator::PauseBake();
	}
}

void CReplayPlaybackController::OnClipPlaybackUpdate()
{
	// should only get called when rendering a video, so don't bother doing another test
	if( m_startClipNextFrame )
	{
		// playback resumed a frame later, to give camera an opportunity to get set up
		// perhaps tie into camera returning back it's ready to go, but overkill???
		CReplayMgr::ResumeNormalPlayback();
		CReplayCoordinator::ResumeBake();
		m_startClipNextFrame = false;
	}
}

void CReplayPlaybackController::SetActiveClip( s32 const activeClip )
{
	m_activeClip = activeClip;
	CReplayMarkerContext::SetCurrentMarkerStorage( TryGetCurrentMarkerStorage() );

	camInterface::GetReplayDirector().OnReplayClipChanged();
}

CClip* CReplayPlaybackController::GetClip( s32 clipIndex )
{
	clipIndex = clipIndex < 0 ? m_activeClip : clipIndex;
	return IsValid() ? m_activeMontage->GetClip( clipIndex ) : NULL;
}

eReplayMicType CReplayPlaybackController::GetMicrophoneType( eMarkerMicrophoneType const markerMicrophoneType )
{
	eReplayMicType micType = replayMicDefault;

	switch( markerMicrophoneType )
	{
	case MARKER_MICROPHONE_AT_CAMERA:
		{
			micType = replayMicAtCamera;
			break;
		}

	case MARKER_MICROPHONE_AT_TARGET:
		{
			micType = replayMicAtTarget;
			break;
		}

	case MARKER_MICROPHONE_CINEMATIC:
		{
			micType = replayMicCinematic;
			break;
		}

	case MARKER_MICROPHONE_DEFAULT:
	default:
		{
			// NOP
		}
	}

	return micType;
}

#endif // GTA_REPLAY
