#include "videorecording.h"

#if VIDEO_RECORDING_ENABLED

#include "atl/string.h"
#include "file/cachepartition.h"
#include "grcore/device.h"
#include "math/amath.h"

#include "system/controlMgr.h"
#include "system/keyboard.h"
#include "system/service.h"

#if (RSG_PC || RSG_DURANGO)
#include "video/recordinginterface_win.h"
#endif

#if USES_MEDIA_ENCODER
#include "renderer/rendertargets.h"
#include "video/mediaencoder_colourconversion.h"
#endif

#if RSG_ORBIS
#include "video/recordinginterface_orbis.h"
#include "renderer/ScreenshotManager.h"
#include "video/video_channel.h"
#elif RSG_DURANGO
#include "control/replay/File/FileStoreDurango.h"
#endif

#if !RSG_PC
#include "rline/rlsystemui.h"
#endif

// DON'T COMMIT
//PRAGMA_OPTIMIZE_OFF();

rage::VideoRecordingInterface *VideoRecording::pRecordingInterface = NULL;
atFixedArray<VideoRecordingInstance, MAX_VIDEORECORDING_INSTANCES> VideoRecording::m_RecordingInstances;
rage::ServiceDelegate VideoRecording::ms_serviceDelegate;

MediaEncoderParams::eQualityLevel VideoRecording::ms_qualityLevel	= MediaEncoderParams::QUALITY_DEFAULT;
MediaEncoderParams::eOutputFps VideoRecording::ms_outputFpsLevel	= MediaEncoderParams::OUTPUT_FPS_DEFAULT;

#if VIDEO_CONVERSION_ENABLED
VideoRecordingFrameBuffer VideoRecording::ms_VideoFrameConverter;
s32 VideoRecording::ms_idToStop = -1;
sysCriticalSectionToken VideoRecording::m_frameBufferCsToken;
#endif

#if RSG_PC
u32 VideoRecording::ms_estimatedDurationMs = 0;
const wchar_t* VideoRecording::ms_gameTitle = L"GTA V";
const wchar_t* VideoRecording::ms_copyright = L"© 2008 - 2015 Rockstar Games, Inc.";
#endif

//--------------------------------------------------------------
//	VideoRecordingInstance
//--------------------------------------------------------------

bool	VideoRecordingInstance::StartRecording( const char *pName, bool const startPaused, bool const synchronized, bool const hasModdedContent )
{
	m_pRecordingTask->StartRecording( pName, startPaused, synchronized, hasModdedContent ); 

#if RSG_ORBIS
	static_cast<rage::RecordingTaskOrbis*>(m_pRecordingTask)->AddMetadata("Grand Theft Auto V", "GTAV");
	
	m_takeScreenshotNextActiveFrame = true;
#endif

	return !m_pRecordingTask->HasError();
}

void VideoRecordingInstance::PauseRecording()
{
	m_pRecordingTask->PauseRecording();
}

void VideoRecordingInstance::ResumeRecording()
{
	m_pRecordingTask->ResumeRecording();
}

void	VideoRecordingInstance::StopRecording()
{
	m_pRecordingTask->StopRecording();
}

void	VideoRecordingInstance::CancelRecording()
{
	m_pRecordingTask->CancelRecording();
}

bool VideoRecordingInstance::IsInitialized()
{
	RecordingTask const * c_task = GetRecordingTask();
	return c_task != NULL;
}

bool VideoRecordingInstance::IsAudioFrameCaptureAllowed()
{
	RecordingTask* pTask = GetRecordingTask();
	if (pTask && pTask->IsAudioFrameCaptureAllowed())
	{
		return true;
	}
	return false;
}

bool VideoRecordingInstance::IsRecording()
{
	RecordingTask *pTask = GetRecordingTask();
	if( pTask && !pTask->IsComplete() )
	{
		return true;
	}
	return false;
}

bool VideoRecordingInstance::IsPaused()
{
	RecordingTask *pTask = GetRecordingTask();
	bool const result = pTask && pTask->HasIssuedStart() && pTask->IsPaused();
	return result;
}

bool VideoRecordingInstance::HasError() const
{ 
	return m_lastError != rage::RecordingTask::ERROR_STATE_NONE; 
}

VideoRecordingInstance::RECORDING_ERROR VideoRecordingInstance::GetLastError() const
{
	RECORDING_ERROR result = RECORDING_ERROR_NONE;

	switch( m_lastError )
	{
	case RecordingTask::ERROR_STATE_UNKNOWN:
	case RecordingTask::ERROR_STATE_OOM:
		{
			result = RECORDING_ERROR_UNKNOWN;
			break;
		}
	case RecordingTask::ERROR_STATE_OUT_OF_SPACE:
		{
			result = RECORDING_ERROR_INSUFFICIENT_SPACE;
			break;
		}
	case RecordingTask::ERROR_STATE_ACCESS_DENIED:
		{
			result = RECORDING_ERROR_ACCESS_DENIED;
			break;
		}
	case RecordingTask::ERROR_STATE_SHARING_VIOLATION:
		{
			result = RECORDING_ERROR_SHARING_VIOLATION;
			break;
		}
	default:
		{
			// nop
		}
	}

	return result;
}

char const * VideoRecordingInstance::GetErrorLngKey( RECORDING_ERROR const errorCode )
{
	static char const * const sc_errorLngKeys[ RECORDING_ERROR_MAX ] =
	{
		"", "VE_BAKE_NOSPACE", "VE_BAKE_NOACCESS", "VE_BAKE_SHARE_VIOLATE", "VE_BAKE_ERROR",
	};

	char const * const c_lngKey = errorCode >= RECORDING_ERROR_FIRST && errorCode < RECORDING_ERROR_MAX ? 
		sc_errorLngKeys[ errorCode ] : "";

	return c_lngKey;
}

bool VideoRecordingInstance::IsReadyForSamples() const
{
	RecordingTask const * const c_task = GetRecordingTask();
	bool const c_result = c_task ? c_task->IsReadyForSamples() : false;
	return c_result;
}

bool VideoRecordingInstance::Update()
{
	RecordingTask *pTask = GetRecordingTask();
	if( pTask )
	{
		if( pTask->IsComplete() )
		{
			if( IsMarkedForCleanUp() )
			{
				VideoRecording::GetRecordingInterface()->DeleteRecordingTask(pTask);
				return true;
			}
		}
		else
		{
#if !RSG_PC
			if (g_SystemUi.IsUiShowing())
			{
				SetHasBeenConstrained(true);
			}
#endif

#if RSG_ORBIS
			// do here to make sure we are a frame in before screenshot while it is recording
			// as in the video editor, when start is called, pause is called immediately after for the loading screen
			if (m_takeScreenshotNextActiveFrame && !pTask->IsPaused())
			{
				if (!m_takenScreenshot)
				{
					// it's highly unlikely you'll record another video before the last screenshot is done, but on the off-chance we can carry on without one
					videoDisplayf("Video Export: Check to see if CHighQualityScreenshot is free to take a screenshot");
					if (!CHighQualityScreenshot::IsProcessingScreenshot())
					{
						videoDisplayf("Video Export: Free to take a screenshot");
						const char* thumbnailPath = "/temp0/thumbnailPhoto.jpg";
						if ( CHighQualityScreenshot::TakeScreenShot(thumbnailPath, 512, 256) ) // needs to be done game-side...this is as low as we can go
						{
							videoDisplayf("Video Export: Taking screenshot for video clip");
							static_cast<rage::RecordingTaskOrbis*>(m_pRecordingTask)->PrepareForExport(thumbnailPath);
						}
					}
					videoDisplayf("Video Export: Screenshot setup complete");
					m_takenScreenshot = true;
				}
				m_takeScreenshotNextActiveFrame = false;
			}

			rage::RecordingTaskOrbis* orbisTask = static_cast<rage::RecordingTaskOrbis*>(m_pRecordingTask);
			if (orbisTask && orbisTask->HasThumbnail() && orbisTask->ReadyToCheckForThumbnail() && CHighQualityScreenshot::HasScreenShotFinished())
			{
			//	videoDisplayf("Video Export: Video thumbnail has finished");
				if (CHighQualityScreenshot::HasScreenShotFailed())
				{
					videoDisplayf("Video Export: Video thumbnail has failed");
					orbisTask->SetThumbnailHasFailed();
				}
				else
				{
				//	videoDisplayf("Video Export: Video thumbnail is ready for export");
					orbisTask->ThumbnailIsReady();
				}
			}
#endif
		}

		UpdateAndGetLastErrorCode( *pTask );
	}

	return false;
}

void VideoRecordingInstance::UpdateAndGetLastErrorCode( RecordingTask & task )
{
	task.Update();
	m_lastError = task.GetLastError();
}

VideoRecording::VideoRecordingDataProviderAdapter VideoRecording::sm_recordingDataAdapter;

//--------------------------------------------------------------
//	VideoRecordingDataProviderAdapter
//--------------------------------------------------------------
void VideoRecording::VideoRecordingDataProviderAdapter::GetGameFramebufferDimensions( u32& out_width, u32& out_height ) const
{
	out_width = VideoResManager::GetNativeWidth();
	out_height = VideoResManager::GetNativeHeight();
}

void VideoRecording::VideoRecordingDataProviderAdapter::GetVideoOutputDimensions( u32& out_width, u32& out_height ) const
{
    int resultWidth, resultHeight;
	MediaEncoderParams::CalculateOutputDimensions( resultWidth, resultHeight, VideoResManager::GetNativeWidth(), VideoResManager::GetNativeHeight() );
    out_width = (u32)resultWidth;
    out_height = (u32)resultHeight;
}

rage::MediaEncoderParams::eQualityLevel VideoRecording::VideoRecordingDataProviderAdapter::GetRecordingQualityLevel() const
{
#if __BANK && !RSG_ORBIS
	return VideoRecording::ms_qualityOverride >= MediaEncoderParams::QUALITY_FIRST && VideoRecording::ms_qualityOverride < MediaEncoderParams::QUALITY_MAX ? 
		(rage::MediaEncoderParams::eQualityLevel)VideoRecording::ms_qualityOverride : (rage::MediaEncoderParams::eQualityLevel)VideoRecording::ms_qualityLevel;
#else
	return (rage::MediaEncoderParams::eQualityLevel)VideoRecording::ms_qualityLevel;
#endif
}

MediaEncoderParams::eOutputFps VideoRecording::VideoRecordingDataProviderAdapter::GetRecordingFps() const
{
#if __BANK && !RSG_ORBIS
	return VideoRecording::ms_fpsOverride >= MediaEncoderParams::OUTPUT_FPS_FIRST && VideoRecording::ms_fpsOverride < MediaEncoderParams::OUTPUT_FPS_MAX ? 
		(rage::MediaEncoderParams::eOutputFps)VideoRecording::ms_fpsOverride : (rage::MediaEncoderParams::eOutputFps)VideoRecording::ms_outputFpsLevel;
#else
	return (rage::MediaEncoderParams::eOutputFps)VideoRecording::ms_outputFpsLevel;
#endif
}

float VideoRecording::VideoRecordingDataProviderAdapter::GetExportFrameDurationMs() const
{
	MediaEncoderParams::eOutputFps const c_outputFps = GetRecordingFps();
	float const c_frameDurationMs = rage::MediaEncoderParams::GetOutputFrameDurationMs( c_outputFps );
	return c_frameDurationMs;
}

MediaCommon::eVideoFormat VideoRecording::VideoRecordingDataProviderAdapter::GetFormatRequested() const
{
#if __BANK
	return HasOutputFormatOverride() ?
		(MediaCommon::eVideoFormat)VideoRecording::ms_formatOverride : MediaCommon::GetDefaultVideoFormat();
#else
	return MediaCommon::GetDefaultVideoFormat();
#endif
}

bool VideoRecording::VideoRecordingDataProviderAdapter::IsRecordingVideo() const
{
	return VideoRecording::IsRecording();
}

bool VideoRecording::VideoRecordingDataProviderAdapter::IsRecordingPaused() const
{
	return VideoRecording::IsRecording() && VideoRecording::IsPaused();
}

bool VideoRecording::VideoRecordingDataProviderAdapter::IsRecordingReadyForSamples() const
{
	return VideoRecording::IsRecording() && VideoRecording::IsReadyForSamples();
}

bool VideoRecording::VideoRecordingDataProviderAdapter::IsAudioFrameCaptureAllowed() const
{
	return VideoRecording::IsRecording() && VideoRecording::IsAudioFrameCaptureAllowed();
}

bool VideoRecording::VideoRecordingDataProviderAdapter::HasRecordingErrored() const
{
	return VideoRecording::IsRecording() && VideoRecording::HasError();
}

u32 VideoRecording::VideoRecordingDataProviderAdapter::GetEstimatedDurationMs() const
{
#if RSG_PC
	return VideoRecording::ms_estimatedDurationMs;
#else
	return 0;
#endif
}

const wchar_t* VideoRecording::VideoRecordingDataProviderAdapter::GetGameTitle() const
{
#if RSG_PC
	return VideoRecording::ms_gameTitle;
#else
	return NULL;
#endif
}

const wchar_t* VideoRecording::VideoRecordingDataProviderAdapter::GetCopyright() const
{
#if RSG_PC
	return VideoRecording::ms_copyright;
#else
	return NULL;
#endif
}
//--------------------------------------------------------------
//	VideoRecording
//--------------------------------------------------------------
#if (RSG_PC || RSG_DURANGO)
bool VideoRecording::ms_UsingPermanentVideoPath = false;
#endif

void VideoRecording::Init(unsigned /*initMode*/)
{
	ms_serviceDelegate.Bind(&sysOnServiceEvent);
	g_SysService.AddDelegate(&ms_serviceDelegate);

	pRecordingInterface = rage::VideoRecordingInterface::Create();

#if RSG_ORBIS
	rage::VideoRecordingSetInfo(SCE_VIDEO_RECORDING_INFO_DESCRIPTION, "Grand Theft Auto V");

	rage::VideoRecordingSetInfo(SCE_VIDEO_RECORDING_INFO_SUBTITLE, "GTAV");

	rage::VideoRecordingSetInfo(SCE_VIDEO_RECORDING_INFO_COPYRIGHT, "\u00A9 2008 - 2014 Rockstar Games, Inc.");

#if	USES_MEDIA_ENCODER
	RecordingTaskOrbis::SetRecordingDataProvider( &sm_recordingDataAdapter );
#endif

#endif

	// Clear out our task array
	m_RecordingInstances.Resize(m_RecordingInstances.GetMaxCount());
	for(int i=0;i<m_RecordingInstances.size();i++)
	{
		m_RecordingInstances[i].Reset();
	}

#if __BANK
	InitBank();
#endif	//__BANK
}

void VideoRecording::Shutdown(unsigned /*shutdownMode*/)
{
#if VIDEO_CONVERSION_ENABLED
	SYS_CS_SYNC( m_frameBufferCsToken );
#endif

#if __BANK
	ShutdownBank();
#endif	//__BANK

	// TODO: Delete all active recording instances? (May need to wait until they're complete?)
#if VIDEO_CONVERSION_ENABLED
	ms_VideoFrameConverter.Shutdown();
#endif

	// Remove the interface
	if( pRecordingInterface )
	{
		delete pRecordingInterface;
		pRecordingInterface = NULL;
	}

	g_SysService.RemoveDelegate(&ms_serviceDelegate);
	ms_serviceDelegate.Unbind();
}

void VideoRecording::Update()
{
#if __BANK
	UpdateBank();
#endif // __BANK

	if( pRecordingInterface )
	{
		// If we have any running tasks we stop the screen dimming.
		bool hasRunningTask = false;

		// Update any allocated tasks, and cleanup any that have completed
		for(int i=0;i<m_RecordingInstances.size();i++)
		{
			VideoRecordingInstance &thisInstance = GetRecordingInstanceFromID(i);

			if( thisInstance.GetRecordingTask() )
			{
				hasRunningTask = true;

				// Update returns true if it's to be cleaned up
				if( thisInstance.Update() )
				{
					thisInstance.Reset();
				}
			}
		}

		// Stop the screen dimming if we have any running tasks.
		if(hasRunningTask)
		{
			g_SysService.DisableScreenDimmingThisFrame();
		}
	}

#if RSG_PC
	GRCDEVICE.SetVideoEncodingOverride( IsRecording() );
#endif
}

void VideoRecording::EnableRecording()
{
	if( pRecordingInterface )
	{
		pRecordingInterface->EnableRecording();
	}
}

void VideoRecording::DisableRecording()
{
	if( pRecordingInterface )
	{
		pRecordingInterface->DisableRecording();
	}
}

bool VideoRecording::StartRecording( s32& out_instanceIndex, const char *pName, bool const startPaused, bool const synchronized, bool const hasModdedContent SAVE_VIDEO_ONLY(, const char *pSaveName) )
{
	out_instanceIndex = -1;
	bool success = false;

	if( pRecordingInterface )
	{
		out_instanceIndex = FindUnusedRecordingInstanceID();
		if( out_instanceIndex >= 0)
		{
			VideoRecordingInstance& thisInstance = GetRecordingInstanceFromID( out_instanceIndex );
			RecordingTask* pNewTask = pRecordingInterface->CreateRecordingTask();
			if( pNewTask )
			{
				thisInstance.Init( pNewTask );

#if RSG_PC
				GRCDEVICE.SetVideoEncodingOverride( true );
#endif

#if SAVE_VIDEO
				if( pSaveName )
				{
					pNewTask->SetSaveToDisc(pSaveName);
				}
#endif
				success = thisInstance.StartRecording( pName, startPaused, synchronized, hasModdedContent );

				u32 outputWidth, outputHeight;
                sm_recordingDataAdapter.GetVideoOutputDimensions( outputWidth, outputHeight );

#if defined(USE_HARDWARE_COLOUR_CONVERSION_FOR_VIDEO_ENCODING) && USE_HARDWARE_COLOUR_CONVERSION_FOR_VIDEO_ENCODING
				success &= rage::MEDIA_ENCODER_COLOUR_CONVERTER.Init( outputWidth, outputHeight );
#endif

#if VIDEO_CONVERSION_ENABLED
				success &= ms_VideoFrameConverter.Init( outputWidth, outputHeight );
#endif

				if( !success )
				{
					thisInstance.MarkForCleanUp();
				}
			}
			else
			{
				out_instanceIndex = -1;
			}
		}
	}

#if VIDEO_CONVERSION_ENABLED
	ms_idToStop = -1;
#endif

	return success;
}

bool VideoRecording::IsPaused()
{
	bool anyPaused = false;

	for( s32 index = 0; !anyPaused && index < MAX_VIDEORECORDING_INSTANCES; ++index )
	{
		anyPaused = IsPaused( index );
	}

	return anyPaused;
}

bool VideoRecording::IsPaused( s32 id )
{
	Assertf(id >= 0 && id < MAX_VIDEORECORDING_INSTANCES, "VideoRecording::IsPaused() - ERROR - Invalid ID");

	VideoRecordingInstance &thisInstance = GetRecordingInstanceFromID(id);

	return thisInstance.IsRecording() && thisInstance.IsPaused();
}

void VideoRecording::PauseRecording( s32 id )
{
	Assertf(id >= 0 && id < MAX_VIDEORECORDING_INSTANCES, "VideoRecording::PauseRecording() - ERROR - Invalid ID");

	VideoRecordingInstance &thisInstance = GetRecordingInstanceFromID(id);

	if( thisInstance.IsRecording() && !thisInstance.IsPaused() )
	{
		thisInstance.PauseRecording();
	}
}

void VideoRecording::ResumeRecording( s32 id )
{
	Assertf(id >= 0 && id < MAX_VIDEORECORDING_INSTANCES, "VideoRecording::ResumeRecording() - ERROR - Invalid ID");

	VideoRecordingInstance &thisInstance = GetRecordingInstanceFromID(id);

	if( thisInstance.IsRecording() && thisInstance.IsPaused() )
	{
		thisInstance.ResumeRecording();
	}
}

bool VideoRecording::IsAudioFrameCaptureAllowed()
{
	bool anyAudioFrameCaptureAllowed = false;

	for (s32 index = 0; !anyAudioFrameCaptureAllowed && index < MAX_VIDEORECORDING_INSTANCES; ++index)
	{
		VideoRecordingInstance& thisInstance = GetRecordingInstanceFromID(index);

		if (thisInstance.IsRecording())
		{
			anyAudioFrameCaptureAllowed = thisInstance.IsAudioFrameCaptureAllowed();
		}
	}

	return anyAudioFrameCaptureAllowed;
}

bool VideoRecording::HasError()
{
	bool anyHasError = false;

	for( s32 index = 0; !anyHasError && index < MAX_VIDEORECORDING_INSTANCES; ++index )
	{
		anyHasError = HasError( index );
	}

	return anyHasError;
}

bool VideoRecording::HasError( s32 id )
{
	Assertf(id >= 0 && id < MAX_VIDEORECORDING_INSTANCES, "VideoRecording::HasError() - ERROR - Invalid ID");

	VideoRecordingInstance &thisInstance = GetRecordingInstanceFromID(id);
	bool const c_hasError = thisInstance.HasError();
	return c_hasError;
}

VideoRecordingInstance::RECORDING_ERROR VideoRecording::GetLastError( s32 id )
{
	VideoRecordingInstance::RECORDING_ERROR error = VideoRecordingInstance::RECORDING_ERROR_NONE;

	Assertf(id >= 0 && id < MAX_VIDEORECORDING_INSTANCES, "VideoRecording::GetError() - ERROR - Invalid ID");

	VideoRecordingInstance &thisInstance = GetRecordingInstanceFromID( id );
	error = thisInstance.GetLastError();

	return error;
}

char const * VideoRecording::GetErrorLngKey( VideoRecordingInstance::RECORDING_ERROR const errorCode )
{
	return VideoRecordingInstance::GetErrorLngKey( errorCode );
}

bool VideoRecording::IsReadyForSamples()
{
	bool anyReadyForSamples = false;

	for( s32 index = 0; !anyReadyForSamples && index < MAX_VIDEORECORDING_INSTANCES; ++index )
	{
		anyReadyForSamples = IsReadyForSamples( index );
	}

	return anyReadyForSamples;
}

bool VideoRecording::IsReadyForSamples( s32 id )
{
	Assertf(id >= 0 && id < MAX_VIDEORECORDING_INSTANCES, "VideoRecording::HasBeenSuspended() - ERROR - Invalid ID");

	VideoRecordingInstance &thisInstance = GetRecordingInstanceFromID(id);
	bool const c_readyForSamples = thisInstance.IsReadyForSamples();
	return c_readyForSamples;
}

bool VideoRecording::HasBeenSuspended( s32 id )
{
	Assertf(id >= 0 && id < MAX_VIDEORECORDING_INSTANCES, "VideoRecording::HasBeenSuspended() - ERROR - Invalid ID");

	VideoRecordingInstance &thisInstance = GetRecordingInstanceFromID(id);
	bool const c_hasBeenSuspended = thisInstance.GetHasBeenSuspended();
	return c_hasBeenSuspended;
}

bool VideoRecording::HasBeenConstrained( s32 id )
{
	Assertf(id >= 0 && id < MAX_VIDEORECORDING_INSTANCES, "VideoRecording::HasBeenConstrained() - ERROR - Invalid ID");

	VideoRecordingInstance &thisInstance = GetRecordingInstanceFromID(id);
	bool const c_hasBeenConstrained = thisInstance.GetHasBeenConstrained();
	return c_hasBeenConstrained;
}

void VideoRecording::StopRecording( s32 id )
{
#if VIDEO_CONVERSION_ENABLED
	Assertf(id >= 0 && id < MAX_VIDEORECORDING_INSTANCES, "VideoRecording::StopRecording() - ERROR - Invalid ID");

	ms_idToStop = id;
	PauseRecording(id);
#else
	ProcessStopRecording( id, false );
#endif
}

void VideoRecording::ProcessStopRecording( s32 id, bool const cancelled )
{
#if VIDEO_CONVERSION_ENABLED
	SYS_CS_SYNC( m_frameBufferCsToken );
#endif

	Assertf(id >= 0 && id < MAX_VIDEORECORDING_INSTANCES, "VideoRecording::ProcessStopRecording() - ERROR - Invalid ID");

	VideoRecordingInstance &thisInstance = GetRecordingInstanceFromID(id);

	if( thisInstance.IsRecording() )
	{
#if VIDEO_CONVERSION_ENABLED
		ms_VideoFrameConverter.Shutdown();
#endif

#if defined(USE_HARDWARE_COLOUR_CONVERSION_FOR_VIDEO_ENCODING) && USE_HARDWARE_COLOUR_CONVERSION_FOR_VIDEO_ENCODING
		rage::MEDIA_ENCODER_COLOUR_CONVERTER.Shutdown();
#endif
		// Actually do the stop
		if( cancelled )
		{
			thisInstance.CancelRecording();
		}
		else
		{
			thisInstance.StopRecording();
		}

		thisInstance.MarkForCleanUp();	// Allow cleanup
	}
}

void VideoRecording::CancelRecording(s32 id)
{
	ProcessStopRecording( id, true );
}

bool VideoRecording::IsFinalizing( s32 const idx )
{
	Assertf(idx >= 0 && idx < MAX_VIDEORECORDING_INSTANCES, "VideoRecording::ProcessStopRecording() - ERROR - Invalid ID");
	VideoRecordingInstance &thisInstance = GetRecordingInstanceFromID( idx );
	return thisInstance.IsInitialized() && !thisInstance.HasError() && !thisInstance.IsPaused() && thisInstance.IsRecording() &&
		!thisInstance.IsMarkedForCleanUp();
}

bool VideoRecording::IsRecording()
{
	bool recording = false;

	for( s32 index = 0; !recording && index < MAX_VIDEORECORDING_INSTANCES; ++index )
	{
		VideoRecordingInstance &thisInstance = GetRecordingInstanceFromID(index);
		recording = thisInstance.IsRecording();
	}

	return recording;
}

size_t VideoRecording::EstimateSizeOfRecording( u32 const durationMs, rage::MediaEncoderParams::eQualityLevel const qualityLevel )
{
	rage::MediaCommon::eVideoFormat const c_videoFormat =
#if __BANK
		HasOutputFormatOverride() ? GetFormatOverride() : 
#endif
		rage::MediaCommon::GetDefaultVideoFormat();


	rage::MediaCommon::eAudioFormat const c_audioFormat =
#if __BANK
		HasOutputFormatOverride() ? rage::MediaCommon::GetDefaultAudioFormatForVideoFormat( GetFormatOverride() ) : 
#endif
		rage::MediaCommon::GetDefaultAudioFormatForVideoFormat( c_videoFormat );

	size_t const c_outputSize = rage::MediaEncoderParams::EstimateSizeOfRecording( durationMs, qualityLevel, c_videoFormat, c_audioFormat );
	return c_outputSize;
}

#if RSG_PC
void VideoRecording::SetMetadataInfo(u32 const estimatedDurationMs)
{
	ms_estimatedDurationMs = estimatedDurationMs;
}
#endif

#if (RSG_PC || RSG_DURANGO)

//! Initializes extra recording information required on PC.
void VideoRecording::InitializeExtraRecordingData( char const * const directory, bool makePermanent )
{
	if (ms_UsingPermanentVideoPath)
		return;

	char path[RAGE_MAX_PATH];
	ReplayFileManager::FixName(path, directory ? directory : REPLAY_VIDEOS_PATH);

#if (RSG_PC || RSG_DURANGO)
	ms_UsingPermanentVideoPath = makePermanent && directory ? true : false;
#endif

	VideoRecordingInterfaceWindows::SetOutputPath( path );
	RecordingTaskWindows::SetRecordingDataProvider( &sm_recordingDataAdapter );
}
#endif

#if USES_MEDIA_ENCODER
void VideoRecording::CaptureVideoFrame( float const frameDuration )
{
#if VIDEO_CONVERSION_ENABLED
	bool workPerformed = false;
#endif

	grcRenderTarget const* rt = CRenderTargets::GetUIBackBuffer();
	if( pRecordingInterface && rt )
	{
		for( u32 i = 0; i<MAX_VIDEORECORDING_INSTANCES; ++i )
		{
#if RSG_ORBIS
			RecordingTaskOrbis* task = static_cast<RecordingTaskOrbis*>(m_RecordingInstances[i].GetRecordingTask());
#else
			RecordingTaskWindows* task = static_cast<RecordingTaskWindows*>(m_RecordingInstances[i].GetRecordingTask());
#endif			

#if VIDEO_CONVERSION_ENABLED
			if( m_frameBufferCsToken.TryLock() )
#endif
			{
				// finding the odd frame sneaking in during initial loading. check ready for samples to see if encoding is ready
				if( task && task->IsReadyForSamples())
				{
					float timeOut = 0.0f;
#if VIDEO_CONVERSION_ENABLED
					// flag that work has been performed this frame
					workPerformed = true;

					bool const c_flipFrame = 
#if RSG_PC
						false;
#else
						true;
#endif
					// need to check for frameDuration being valid as pause, sometimes the pause state isn't correct in some loading screens :/
					bool isPaused = task->IsPaused() || frameDuration == 0.0f;

					grcRenderTarget* pOut = NULL;
					bool isFrameReady = ms_VideoFrameConverter.ConvertTarget(pOut, &timeOut, rt, frameDuration, c_flipFrame, isPaused );				
					if(isFrameReady)
						rt = pOut;
					else
						rt = NULL;
#else
					rt = !task->IsPaused() && frameDuration ? rt : NULL;
					timeOut = frameDuration;
#endif
					if(rt)
					{
						task->PushVideoFrame( *rt, frameDuration );

#if RSG_DURANGO
						grcTextureFactory::GetInstance().LockRenderTarget( 0, rt, NULL );
						grcTextureFactory::GetInstance().UnlockRenderTarget( 0 );
#endif
					}
				}
#if VIDEO_CONVERSION_ENABLED
				m_frameBufferCsToken.Unlock();
#endif
			}
		}
	}

#if VIDEO_CONVERSION_ENABLED
	// deals with the shutdown once the frame buffer in the converter is drained
	if (ms_idToStop != -1)
	{
		// B*2490759 - ...very rare but possible
		// workPerformed is a catch all flag in case any of the conditions to performing
		// video conversion fails during the draining of the buffer
		if (!rt || !workPerformed)
		{
			ProcessStopRecording( ms_idToStop, false );
			ms_idToStop = -1;
		}
	}
#endif
}
#endif // USES_MEDIA_ENCODER

void VideoRecording::RenderThreadInit()
{
#if USES_MEDIA_ENCODER
	MediaEncoder::PrepareRenderThread();
#endif
}

void VideoRecording::RenderThreadCleanup()
{
#if USES_MEDIA_ENCODER
	MediaEncoder::CleanupRenderThread();
#endif
}

s32	VideoRecording::FindUnusedRecordingInstanceID()
{
	for(int i=0;i<MAX_VIDEORECORDING_INSTANCES;i++)
	{
		VideoRecordingInstance &thisInstance = m_RecordingInstances[i];
		if( thisInstance.GetRecordingTask() == NULL )
		{
			return i;
		}
	}
	return -1;
}

void VideoRecording::sysOnServiceEvent( rage::sysServiceEvent* evnt )
{
	if(evnt != NULL)
	{
		switch(evnt->GetType())
		{
		case sysServiceEvent::SUSPENDED:
		case sysServiceEvent::SUSPEND_IMMEDIATE:
			{
				for(int i=0;i<MAX_VIDEORECORDING_INSTANCES;i++)
				{
					VideoRecordingInstance &thisInstance = m_RecordingInstances[i];
					thisInstance.SetHasBeenSuspended();
				}

				break;
			}
			
		case sysServiceEvent::CONSTRAINED:
			{
				for(int i=0;i<MAX_VIDEORECORDING_INSTANCES;i++)
				{
					VideoRecordingInstance &thisInstance = m_RecordingInstances[i];
					thisInstance.SetHasBeenConstrained( true );
				}

				break;

				 break;
			}

		default:
			{
				break;
			}
		}
	}
}

#if __BANK

// RAG Widgets
bkBank *VideoRecording::ms_pBank = NULL;
bkButton *VideoRecording::ms_pCreateButton = NULL;

// Clip offset & duration
s32		VideoRecording::ms_StartOffset = -10;
s32		VideoRecording::ms_Duration = 10;
// Clip Task ID
s32		VideoRecording::ms_RecordTaskID = -1;

bool	VideoRecording::ms_RecordToDisc = false;
int		VideoRecording::ms_qualityOverride = MediaEncoderParams::QUALITY_MAX;
int		VideoRecording::ms_fpsOverride = MediaEncoderParams::OUTPUT_FPS_MAX;
int		VideoRecording::ms_formatOverride = MediaCommon::VIDEO_FORMAT_MAX;
bool	VideoRecording::ms_PauseCapture = false;

#define RAG_WIDGET_VIDEO_NAME	"RAGWidgetVideo"
#define RECORD_TO_DISC_NAME		"D:\\RAG_Clip.mp4"

void VideoRecording::InitBank()
{
	if(ms_pBank)
	{
		ShutdownBank();
	}

	// Create the weapons bank
	ms_pBank = &BANKMGR.CreateBank("VideoRecording", 0, 0, false); 
	if(Verifyf(ms_pBank, "Failed to create VideoRecording bank"))
	{
		ms_pCreateButton = ms_pBank->AddButton("Create VideoRecording widgets", &VideoRecording::CreateBank);
	}
}

void VideoRecording::CreateBank()
{
	Assertf(ms_pBank, "GameDVR bank needs to be created first");
	Assertf(ms_pCreateButton, "GameDVR bank needs to be created first");

	if( ms_pCreateButton )
	{
		ms_pCreateButton->Destroy();
		ms_pCreateButton = NULL;
	}

	bkBank* pBank = ms_pBank;

	// Only make the widgets if we have a recording interface
	if( pRecordingInterface )
	{
		pBank->AddSeparator("Video Recording");

		pBank->AddSeparator("Clip Start/Stop");
		pBank->AddButton("Start Recording", StartRecordingCB);
		pBank->AddButton("Stop Recording", StopRecordingCB);
		pBank->AddToggle("Record To Local Disc (D:\\RAG_Clip.mp4)", &ms_RecordToDisc);

		pBank->AddSeparator("General");
		pBank->AddButton("Cancel Current Recording", CancelRecordingCB);
		pBank->AddToggle( "Pause Recording (If capturing)", &ms_PauseCapture );

#if (RSG_PC || RSG_DURANGO)

		static const char* qualityNames[  MediaEncoderParams::QUALITY_MAX + 1 ] = {
			"Low",
			"Medium",
			"High",
			"Ultra",
			"Default"
		};

		static const char* fpsNames[  MediaEncoderParams::OUTPUT_FPS_MAX + 1 ] = {
			"30",
			"60",
			"Default"
		};

		pBank->AddSeparator("Output Options");
		pBank->AddCombo( "Quality", &ms_qualityOverride, MediaEncoderParams::QUALITY_MAX + 1, qualityNames );
		pBank->AddCombo( "FPS", &ms_fpsOverride, MediaEncoderParams::OUTPUT_FPS_MAX + 1, fpsNames );

		if( MediaCommon::IsWmvSupported() )
		{
			static const char* formatNames[  MediaCommon::VIDEO_FORMAT_MAX + 1 ] = {
				"H264",
				"WMV",
				"Default"
			};

			pBank->AddCombo( "Video Format", &ms_formatOverride, MediaCommon::VIDEO_FORMAT_MAX + 1, formatNames );
		}

#endif
	}

}

void VideoRecording::UpdateBank()
{
	if( ms_RecordTaskID != -1 )
	{
		bool const paused = IsPaused( ms_RecordTaskID );

		if( paused && !ms_PauseCapture )
		{
			ResumeRecording( ms_RecordTaskID );
		}
		else if( ms_PauseCapture && !paused )
		{
			PauseRecording( ms_RecordTaskID );
		}
	}
}

void VideoRecording::ShutdownBank()
{
	if(ms_pBank)
	{
		BANKMGR.DestroyBank(*ms_pBank);
	}
	ms_pBank = NULL;
	ms_pCreateButton = NULL;
}

void VideoRecording::StartRecordingCB()
{
	// Check the previous capture has completed
	if( ms_RecordTaskID != -1)
	{
		VideoRecordingInstance &thisInstance = GetRecordingInstanceFromID(ms_RecordTaskID);
		if( thisInstance.GetRecordingTask() == NULL)
		{
			ms_RecordTaskID = -1;
		}
	}

	if( ms_RecordTaskID == -1)
	{
		const char *pSaveName = NULL;
		if( ms_RecordToDisc )
		{
			pSaveName = RECORD_TO_DISC_NAME;
		}

		 videoVerifyf( StartRecording( ms_RecordTaskID, RAG_WIDGET_VIDEO_NAME, false, false, false, pSaveName),
			 "RAG Recording failed to start! Reported error was %s", GetErrorLngKey( GetLastError( ms_RecordTaskID ) ) );
	}
}

void VideoRecording::StopRecordingCB()
{
	// Check there's something to stop
	if( ms_RecordTaskID != -1)
	{
		StopRecording(ms_RecordTaskID);
	}
}

void VideoRecording::CancelRecordingCB()
{
	if( ms_RecordTaskID != -1)
	{
		CancelRecording(ms_RecordTaskID);
	}
}

#endif	//__BANK

#endif	//VIDEO_RECORDING_ENABLED
