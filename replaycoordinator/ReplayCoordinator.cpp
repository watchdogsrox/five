/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : ReplayCoordinator.cpp
// PURPOSE : Coordinates playback/rendering of replay footage and glues higher level interface
//		     to underlying raw replay playback.
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#include "ReplayCoordinator.h"

#if GTA_REPLAY

// game
#include "control/replay/replay.h"
#include "control/videorecording/videorecording.h"
#include "rendering/AncillaryReplayRenderer.h"
#include "rendering/ReplayPostFxRegistry.h"
#include "replay_coordinator_channel.h"
#include "ReplayMontageGenerator.h"
#include "ReplayPlaybackController.h"
#include "audiohardware/driver.h"
#include "grcore/device.h"
#include "audio/radioaudioentity.h"

#if USE_SRLS_IN_REPLAY
#include "streaming/streamingrequestlist.h"
#endif

PARAM(noreplaysrl, "Disable srl clip processing stage");
PARAM(showsrlpass, "Shows the SRL preprocess step");

REPLAY_COORDINATOR_OPTIMISATIONS();

RAGE_DEFINE_CHANNEL(replayCoordinator); 

CReplayCoordinator::eCoordinatorState CReplayCoordinator::ms_currentState = CReplayCoordinator::COORDINATORSTATE_INVALID;
CReplayPlaybackController CReplayCoordinator::ms_playbackController;
CReplayPostFxRegistry CReplayCoordinator::ms_postfxRegistry;

s32 CReplayCoordinator::ms_recordingInstanceIndex = INDEX_NONE;
s32 CReplayCoordinator::ms_recordingInstanceIndexForErrorChecking = INDEX_NONE;
bool CReplayCoordinator::ms_renderTransitionsInEditMode = false;

void CReplayCoordinator::Init( unsigned initMode )
{
	if(initMode == INIT_CORE)
	{
		CAncillaryReplayRenderer::InitializeTextureBlendState();
		PARSER.LoadObject("common:/data/ui/replayfx", "meta", ms_postfxRegistry);
	}
}

void CReplayCoordinator::Update()
{
	if( ms_playbackController.IsValid() )
	{
		if( !IsPendingPendingBakeStart() )
		{
			float const c_overrideTime = CReplayMarkerContext::GetJumpingOverrideNonDilatedTimeMs();
			float const c_currentTime = c_overrideTime >= 0 ? c_overrideTime : CReplayMgr::GetCurrentTimeRelativeMsFloat();

			ms_playbackController.UpdateMarkersForPlayback( c_currentTime );
			ms_playbackController.UpdateMontageAudioFadeOut();

			UpdateOverlaysAndEffects();
		}
	}
}

void CReplayCoordinator::Shutdown( unsigned shutdownMode )
{
	UNUSED_VAR(shutdownMode);
	Deactivate();
}

void CReplayCoordinator::Activate()
{
	rcAssertf( ms_currentState == COORDINATORSTATE_INVALID, "CReplayCoordinator::Activate - Double activiation. Logic Error?" );
	if( ms_currentState == COORDINATORSTATE_INVALID )
	{
		setState( COORDINATORSTATE_IDLE );
		CReplayMgr::Enable();
		CReplayControl::SetRecordingDisallowed(DISALLOWED1);
	}
}

bool CReplayCoordinator::IsActive()
{
	return CReplayMgr::IsEnabled() && ms_currentState != COORDINATORSTATE_INVALID;
}

void CReplayCoordinator::Deactivate()
{
	if( IsActive() )
	{		
		KillPlaybackOrBake();
		CleanupPlayback();

		CReplayControl::RemoveRecordingDisallowed(DISALLOWED1);
		CReplayMgr::QuitReplay();
		setState( COORDINATORSTATE_INVALID );
	}
}

bool CReplayCoordinator::IsCachingForPlayback()
{
	return ( ms_currentState == COORDINATORSTATE_VIDEO_CLIP_PREVIEW || ms_currentState == COORDINATORSTATE_VIDEO_PREVIEW || 
		ms_currentState == COORDINATORSTATE_VIDEO_BAKE ) && CReplayMgr::IsPreCachingScene() && CReplayMgr::GetLoadState() == REPLAYLOADING_CLIP;
}

bool CReplayCoordinator::IsPendingNextClip()
{
	return ( IsPreviewingVideo() || IsRenderingVideo() ) && CReplayMgr::IsClipTransition();
}

bool CReplayCoordinator::IsPreviewingVideo()
{
	return ms_currentState == COORDINATORSTATE_VIDEO_PREVIEW && ms_playbackController.IsValid();
}

bool CReplayCoordinator::IsPreviewingClip()
{
	return ms_currentState == COORDINATORSTATE_VIDEO_CLIP_PREVIEW && ms_playbackController.IsValid();
}

bool CReplayCoordinator::IsRenderingVideo()
{
	return ( ms_currentState == COORDINATORSTATE_VIDEO_PENDING_BAKE || ms_currentState == COORDINATORSTATE_VIDEO_BAKE || 
		ms_currentState == COORDINATORSTATE_VIDEO_BAKE_PAUSED || ms_currentState == COORDINATORSTATE_VIDEO_BAKE_PAUSED_CALLED ) && 
		ms_playbackController.IsValid();
}

bool CReplayCoordinator::IsPendingVideoFinalization()
{
#if VIDEO_RECORDING_ENABLED
	return ms_recordingInstanceIndexForErrorChecking != INDEX_NONE && ms_recordingInstanceIndex == INDEX_NONE
		&& VideoRecording::IsFinalizing( ms_recordingInstanceIndexForErrorChecking );
#else
	return false;
#endif
}

bool CReplayCoordinator::HasVideoRenderErrored()
{
#if VIDEO_RECORDING_ENABLED
	return ms_recordingInstanceIndexForErrorChecking == INDEX_NONE || VideoRecording::HasError( ms_recordingInstanceIndexForErrorChecking );
#else
	return false;
#endif
}

char const * CReplayCoordinator::GetVideoRenderErrorLngKey()
{
#if VIDEO_RECORDING_ENABLED
	return ms_recordingInstanceIndexForErrorChecking == INDEX_NONE ? 
		NULL : VideoRecording::GetErrorLngKey( VideoRecording::GetLastError( ms_recordingInstanceIndexForErrorChecking ) );
#else
	return "";
#endif
}

bool CReplayCoordinator::IsVideoRenderReadyForReplay()
{
#if VIDEO_RECORDING_ENABLED
	return VideoRecording::IsReadyForSamples();
#else
	return true;
#endif
}

bool CReplayCoordinator::HasVideoRenderSuspended()
{
#if VIDEO_RECORDING_ENABLED
	return VideoRecording::HasBeenSuspended( ms_recordingInstanceIndexForErrorChecking );
#else
	return false;
#endif
}

bool CReplayCoordinator::HasVideoRenderBeenConstrained()
{
#if VIDEO_RECORDING_ENABLED
	return VideoRecording::HasBeenConstrained( ms_recordingInstanceIndexForErrorChecking );
#else
	return false;
#endif
}

bool CReplayCoordinator::IsRenderingPaused()
{
	return IsRenderingVideo() && ms_currentState == COORDINATORSTATE_VIDEO_BAKE_PAUSED;
}

bool CReplayCoordinator::DoesCurrentClipContainCutscene()
{
	return CReplayMgr::DoesCurrentClipContainCutscene();
}

bool CReplayCoordinator::DoesCurrentClipContainFirstPersonCamera()
{
	return CReplayMgr::DoesCurrentClipContainFirstPersonCamera();
}

bool CReplayCoordinator::DoesCurrentClipDisableCameraMovement()
{
	return CReplayMgr::DoesCurrentClipDisableCameraMovement();
}

bool CReplayCoordinator::StartAutogeneratingProject( CReplayEditorParameters::eAutoGenerationDuration const duration )
{
	CMissionMontageGenerator montageGenerator( duration, atString("") );
	bool const c_started = montageGenerator.StartMontageGeneration();
	return c_started;
}

bool CReplayCoordinator::IsAutogeneratingProject()
{
	return CMissionMontageGenerator::IsProcessing();
}

bool CReplayCoordinator::HasAutogenerationFailed()
{
	return CMissionMontageGenerator::HasProcessingFailed();
}

CMontage* CReplayCoordinator::GetAutogeneratedMontage()
{
	return CMissionMontageGenerator::GetGeneratedMontage();
}

u64 CReplayCoordinator::GetActiveUserId()
{
	return CReplayMgr::GetCurrentReplayUserId();
}

bool CReplayCoordinator::IsClipUsedInAnyProject( ClipUID const& uid )
{
	return CReplayMgr::IsClipUsedInAnyMontage( uid );
}

bool CReplayCoordinator::ShouldShowLoadingScreen()
{
	bool isReplayPrep = (!CReplayMgr::IsEditModeActive() && (CReplayMgr::IsClipTransition() || CReplayMgr::IsLoading())) || CReplayMgr::WantsToLoad() || CReplayMgr::IsSettingUpFirstFrame();
	
#if USE_SRLS_IN_REPLAY
	isReplayPrep |= (CReplayMgr::IsCreatingSRL() BANK_ONLY(&& !PARAM_showsrlpass.Get()));
#endif
	return isReplayPrep || IsCachingForPlayback() || IsPendingNextClip() || IsAutogeneratingProject() || IsPendingVideoFinalization() || IsPendingPendingBakeStart();
}

bool CReplayCoordinator::ShouldShowProcessingText()
{
#if USE_SRLS_IN_REPLAY
	return CReplayMgr::IsCreatingSRL() && CReplayMgr::IsPlaying();
#else
	return false;
#endif
}

bool CReplayCoordinator::HasClipLoadFailed()
{
	bool isLoadingClip = CReplayMgr::IsLoading() || CReplayMgr::WantsToLoad();
	return isLoadingClip && CReplayMgr::GetLastErrorCode() > REPLAY_ERROR_SUCCESS;
}

s32 CReplayCoordinator::GetIndexOfCurrentPlaybackClip()
{
	return ms_playbackController.IsValid() ? ms_playbackController.GetCurrentClipIndex() : -1;
}

float CReplayCoordinator::GetEffectiveCurrentClipTimeMs()
{
	float const c_overrideTime = CReplayMarkerContext::GetJumpingOverrideNonDilatedTimeMs();
	return c_overrideTime >= 0.f ? c_overrideTime : CReplayMgr::GetCurrentTimeRelativeMsFloat();
}

void CReplayCoordinator::setState( eCoordinatorState const stateToSet )
{
	if( ms_currentState == stateToSet )
		return;

	switch( stateToSet )
	{
	case COORDINATORSTATE_IDLE:
		{
			rcAssertf( !ms_playbackController.IsValid(), "CReplayCoordinator::setState - Switching to idle with active playback" );
			break;
		}
	case COORDINATORSTATE_INVALID:
			{
				// These states don't use the playback controller, so do nowt!
				break;
			}
	default:
		{
			rcAssertf( ms_playbackController.IsValid(), "CReplayCoordinator::setState - Switching to none idle state without active montage" );
			break;
		}
	}

	ms_currentState = stateToSet;
}

bool CReplayCoordinator::PlayClipPreview( u32 clipIndex, CMontage& montage )
{
	rcAssertf( !ms_playbackController.IsValid(), "CReplayCoordinator::PlayClipPreview - Existing playback operation in progress!" );
	if( !ms_playbackController.IsValid() )
	{
		ms_playbackController.Initialize( montage, clipIndex, true,  NULL );
		CReplayMgr::BeginPlayback( ms_playbackController );
		setState( COORDINATORSTATE_VIDEO_CLIP_PREVIEW );
	}

	return IsPreviewingClip();
}

bool CReplayCoordinator::PlayProjectPreview( CMontage& montage )
{
	rcAssertf( !ms_playbackController.IsValid(), "CReplayCoordinator::PlayProjectPreview - Existing playback operation in progress!" );
	if( !ms_playbackController.IsValid() )
	{
		ms_playbackController.Initialize( montage, 0, false, NULL );
		CReplayMgr::BeginPlayback( ms_playbackController );
		setState( COORDINATORSTATE_VIDEO_PREVIEW );
	}

	return IsPreviewingVideo();
}

bool CReplayCoordinator::GetSpaceForVideoBake( size_t& out_availableSpace )
{
	bool const c_success = CReplayMgr::GetFreeSpaceAvailableForVideos( out_availableSpace );
	return c_success;
}

bool CReplayCoordinator::StartBakeProject( CMontage& montage, char const * const outputName )
{
	bool success = false;
	rcAssertf( !ms_playbackController.IsValid(), "CReplayCoordinator::BakeProject - Existing playback operation in progress!" );
	if( !ms_playbackController.IsValid() )
	{

#if VIDEO_RECORDING_ENABLED
		success = VideoRecording::StartRecording( ms_recordingInstanceIndex,
			outputName && strlen(outputName) != 0 ? outputName : montage.GetName(), true, true, montage.AreAnyClipsOfModdedContent() );

		//! Set error instance even if we failed, as we need it to check the failure errors
		ms_recordingInstanceIndexForErrorChecking = ms_recordingInstanceIndex;
#else
		(void)outputName;
		success = true;
#endif

		if( success )
		{
			CReplayCoordinator::PrepareForVideoBake();

			IVideoRecordingDataProvider const* dataProvider( NULL );
			VIDEO_RECORDING_ENABLED_ONLY( dataProvider = &VideoRecording::sm_recordingDataAdapter );

			ms_playbackController.Initialize( montage, 0, false, dataProvider );	
			setState( COORDINATORSTATE_VIDEO_PENDING_BAKE );
		}
	}

	return success;
}

bool CReplayCoordinator::UpdateBakeProject()
{
	if( IsPendingPendingBakeStart() &&
		IsVideoRenderReadyForReplay() )
	{
		CReplayMgr::BeginPlayback( ms_playbackController );
		setState( COORDINATORSTATE_VIDEO_BAKE );
	}

	return !HasVideoRenderErrored();
}

void CReplayCoordinator::PauseBake()
{
	if( ms_recordingInstanceIndex != INDEX_NONE )
	{
#if VIDEO_RECORDING_ENABLED
		VideoRecording::PauseRecording( ms_recordingInstanceIndex );

		setState( COORDINATORSTATE_VIDEO_BAKE_PAUSED_CALLED );
#endif
	}
}

void CReplayCoordinator::ResumeBake()
{
	if( ms_recordingInstanceIndex != INDEX_NONE )
	{
#if VIDEO_RECORDING_ENABLED
		VideoRecording::ResumeRecording( ms_recordingInstanceIndex );
		
		setState( COORDINATORSTATE_VIDEO_BAKE );
#endif
	}
}

void CReplayCoordinator::KillPlaybackOrBake( bool userCancelled )
{	
	g_RadioAudioEntity.StopReplayMusic(false, true);

	if( IsRenderingVideo() && ms_recordingInstanceIndex != INDEX_NONE )
	{
#if VIDEO_RECORDING_ENABLED
		if( userCancelled )
		{
			VideoRecording::CancelRecording( ms_recordingInstanceIndex );
		}
		else
		{
			VideoRecording::StopRecording( ms_recordingInstanceIndex );
		}
#else
		(void)userCancelled;
#endif
		CReplayCoordinator::CleanupVideoBake();
	}

	ms_recordingInstanceIndex = INDEX_NONE;

	CAncillaryReplayRenderer::ResetActiveReplayFx();
	CAncillaryReplayRenderer::DisableReplayDefaultEffects();

	if( IsPendingPendingBakeStart() )
	{
		ms_playbackController.Reset();
		setState( COORDINATORSTATE_IDLE );
	}
	else
	{
		if( userCancelled )
		{
			CleanupReplayPlaybackInternal();
		}
		else
		{
			// Delayed clean-up mechanics
			setState( COORDINATORSTATE_VIDEO_BAKE_PENDING_CLEANUP );
		}
	}
}

void CReplayCoordinator::CleanupPlayback()
{
	if( IsPendingCleanup() )
	{
		CleanupReplayPlaybackInternal();
	}
}

bool CReplayCoordinator::IsExportingToVideoFile()
{
	if( ms_playbackController.IsValid() )
	{
		return ms_playbackController.IsExportingToVideoFile();
	}

	return false;
}

bool CReplayCoordinator::ShouldDisablePopStreamer()
{
	return CReplayMgr::IsReplayInControlOfWorld();
}

bool CReplayCoordinator::IsSettingUpFirstFrame()
{
	return CReplayMgr::IsSettingUpFirstFrame();
}

bool CReplayCoordinator::IsPendingPendingBakeStart()
{
	return ms_currentState == COORDINATORSTATE_VIDEO_PENDING_BAKE;
}

bool CReplayCoordinator::IsPendingCleanup()
{
	return ms_currentState == COORDINATORSTATE_VIDEO_BAKE_PENDING_CLEANUP;
}

void CReplayCoordinator::UpdateOverlaysAndEffects()
{
	// check to see if encoder is actually paused yet
	if (ms_currentState == COORDINATORSTATE_VIDEO_BAKE_PAUSED_CALLED)
	{
		if (VideoRecording::IsPaused())
			ms_currentState = COORDINATORSTATE_VIDEO_BAKE_PAUSED;
	}

	//! Keep using the previous clip if we are requesting a transition, as the playback controller
	//! may be ahead of the replay system.
	u32 clipIndex = ms_playbackController.GetCurrentClipIndex();
	clipIndex = IsPendingNextClip() && clipIndex > 0 ? clipIndex - 1 : clipIndex;

	float const c_currentTimeMs = GetEffectiveCurrentClipTimeMs();
	float const c_currentTimeClampedMs = Clamp( c_currentTimeMs, 
		ms_playbackController.GetClipStartNonDilatedTimeMs( clipIndex ), ms_playbackController.GetClipNonDilatedEndTimeMs( clipIndex ) );

	CAncillaryReplayRenderer::UpdateActiveReplayFx( *ms_playbackController.GetActiveMontage(),
		clipIndex, c_currentTimeClampedMs );

	CAncillaryReplayRenderer::UpdateOverlays( *ms_playbackController.GetActiveMontage(),
		clipIndex, c_currentTimeClampedMs, ShouldUpdateOverlays() );

	CAncillaryReplayRenderer::UpdateTransitions( *ms_playbackController.GetActiveMontage(),
		clipIndex, c_currentTimeClampedMs );
}

void CReplayCoordinator::RenderOverlaysAndEffects()
{
	if( IsViewingReplay() && ShouldRenderTransitions() )
	{
		CAncillaryReplayRenderer::RenderTransitions();
	}
}

void CReplayCoordinator::RenderPostFadeOverlaysAndEffects()
{
	if( IsViewingReplay() )
	{
		CAncillaryReplayRenderer::RenderOverlays( );
	}
}

bool CReplayCoordinator::ShouldRenderOverlays()
{
	if (IsRenderingVideo())
	{
		return !IsRenderingPaused();
	}

	return ShouldUpdateOverlays();
}

void CReplayCoordinator::PrepareForVideoBake()
{
    CAncillaryReplayRenderer::ResetOverlays();

#if USES_MEDIA_ENCODER
	CReplayMgr::SetFixedTimeExport(true);
	PrepareForAudioBake();
	
#if RSG_PC
	if(CPauseMenu::GetMenuPreference(PREF_ROCKSTAR_EDITOR_EXPORT_GRAPHICS_UPGRADE) != 0)
	{
		CSettingsManager::GetInstance().ApplyReplaySettings();
		GRCDEVICE.ForceDeviceReset();
	}	
#endif
#endif

#if USE_SRLS_IN_REPLAY
	//we do baking in two stages, the first stage generates the SRL 
	//the 2nd stage does the actual "baking" to video using the SRL to 
	//make sure that things are loaded before they are shown
	//here we prepare for the first stage
	// See: CReplayMgrInternal::UpdateClip() for how we switch between stages	
#if !__FINAL
	if(!PARAM_noreplaysrl.Get())
	{
#endif
		CReplayMgr::SetRecordingSRL(true);
		CReplayMgr::SetNextPlayBackState( REPLAY_RECORD_SRL );	
#if !__FINAL
	}
#endif

#endif
}

void CReplayCoordinator::PrepareForAudioBake()
{
#if defined(AUDIO_CAPTURE_ENABLED) && AUDIO_CAPTURE_ENABLED
	if(audDriver::GetMixer())
	{
		audDriver::GetMixer()->PrepareForReplayRender();
	}
#endif
}

void CReplayCoordinator::CleanupVideoBake()
{
#if RSG_PC
	if(CPauseMenu::GetMenuPreference(PREF_ROCKSTAR_EDITOR_EXPORT_GRAPHICS_UPGRADE) != 0)
	{
		CSettingsManager::GetInstance().RevertToPreviousSettings();
	}
#endif
	CleanupAudioBake();

#if USE_SRLS_IN_REPLAY
	#if !__FINAL
	if(!PARAM_noreplaysrl.Get())
	{
	#endif
		//make sure that we fully clean up any SRL usage here
		CReplayMgr::SetRecordingSRL(false);
		CReplayMgr::ClearNextPlayBackState( REPLAY_RECORD_SRL | REPLAY_PLAYBACK_SRL );	
		gStreamingRequestList.Finish();					
		gStreamingRequestList.DeleteRecording();
		gStreamingRequestList.DisableForReplay();
	#if !__FINAL
	}
	#endif
#endif 

}


void CReplayCoordinator::CleanupAudioBake()
{
#if defined(AUDIO_CAPTURE_ENABLED) && AUDIO_CAPTURE_ENABLED
	if(audDriver::GetMixer())
	{
		audDriver::GetMixer()->CleanUpReplayRender();
	}
#endif
}


bool CReplayCoordinator::ShouldUpdateOverlays()
{
	// on export, just check against export, which has already done these checks
	if (IsRenderingVideo())
	{
		return !ShouldShowLoadingScreen();  // fix for 2364709 - means we do not miss a frame of overlays at the very start
	}

	return ( IsPreviewingClip() || IsPreviewingVideo() ) && !IsCachingForPlayback() && !IsPendingNextClip() &&
		!CReplayMgr::IsLoading() && !CReplayMgr::WantsToLoad() && !CReplayMgr::IsClipTransition()
#if USE_SRLS_IN_REPLAY
		&& !CReplayMgr::IsCreatingSRL()
#endif
		;
}

bool CReplayCoordinator::ShouldRenderTransitions()
{
	// Preview and export should always try to render transitions 
	return ms_renderTransitionsInEditMode || IsPreviewingVideo() || IsExportingToVideoFile();
}

bool CReplayCoordinator::IsViewingReplay()
{
	return ( (IsRenderingVideo() && !IsRenderingPaused()) || IsPreviewingClip() || IsPreviewingVideo() );
}

void CReplayCoordinator::CleanupReplayPlaybackInternal()
{
	CReplayMgr::CleanupPlayback();
	ms_playbackController.Reset();
	setState( COORDINATORSTATE_IDLE );
}

#endif // GTA_REPLAY
