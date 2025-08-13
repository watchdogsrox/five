#ifndef _VIDEORECORDING_H_
#define _VIDEORECORDING_H_

#include "bank/bank.h"
#include "bank/button.h"
#include "bank/bkmgr.h"
#include "system/service.h"

#include "video/recordinginterface.h"
#include "videorecordingframebuffer.h"
#if (RSG_PC || RSG_DURANGO)
#pragma warning( push )
#pragma warning(disable: 4200)
#include "video/recordinginterface_win.h"
#pragma warning( pop )
#endif

#include "video/mediaencoder_params.h"
#include "control/replay/ReplaySettings.h"

#define VIDEO_RECORDING_ENABLED	( (1 && RSG_DURANGO) || (1 && RSG_ORBIS) || ( 1 && RSG_PC )) && GTA_REPLAY
#if VIDEO_RECORDING_ENABLED
#	define VIDEO_RECORDING_ENABLED_ONLY(x)  x
#else
#	define VIDEO_RECORDING_ENABLED_ONLY(x)
#endif

#if VIDEO_RECORDING_ENABLED

#define MAX_VIDEORECORDING_INSTANCES	(1)

class VideoRecordingInstance
{
public: // declarations and variables

	enum RECORDING_ERROR
	{
		RECORDING_ERROR_FIRST = 0,
		RECORDING_ERROR_NONE = RECORDING_ERROR_FIRST,
		RECORDING_ERROR_INSUFFICIENT_SPACE,	// No room on storage medium
		RECORDING_ERROR_ACCESS_DENIED,		// File/Directory is inaccessible or locked
		RECORDING_ERROR_SHARING_VIOLATION,
		RECORDING_ERROR_UNKNOWN,			// Catch-all for displaying errors we don't want to detail to the user

		RECORDING_ERROR_MAX,
	};

public: // methods

	VideoRecordingInstance()
		: m_pRecordingTask( NULL )
		, m_lastError( rage::RecordingTask::ERROR_STATE_NONE )
		, m_hasBeenSuspended( false )
		, m_isConstrained( false )
	{
		
		m_pRecordingTask = NULL;
	}

	~VideoRecordingInstance()
	{
	}

	void	Reset()
	{
		m_pRecordingTask = NULL;
		m_hasBeenSuspended = false;
		m_isConstrained = false;
	}

	void Init(rage::RecordingTask *pTask)
	{
		m_CanCleanUp = false;
		m_pRecordingTask = pTask;
		m_lastError = rage::RecordingTask::ERROR_STATE_NONE;
		m_hasBeenSuspended = false;
		m_isConstrained = false;
#if RSG_ORBIS
		m_takenScreenshot = false;
		m_takeScreenshotNextActiveFrame = false;
#endif
	}

	RecordingTask* GetRecordingTask()
	{
		return m_pRecordingTask;
	}
	
	RecordingTask const* GetRecordingTask() const
	{
		return m_pRecordingTask;
	}

	bool	StartRecording( const char *pName, bool const startPaused = false, bool const synchronized = false, bool const hasModdedContent = false );
	void	PauseRecording();
	void	ResumeRecording();
	void	StopRecording();
	void	CancelRecording();

	bool	IsInitialized();
	bool	IsAudioFrameCaptureAllowed();
	bool	IsRecording();
	bool	IsPaused();

	bool				HasError() const;
	RECORDING_ERROR		GetLastError() const;
	static char const*	GetErrorLngKey( VideoRecordingInstance::RECORDING_ERROR const errorCode );

	bool IsReadyForSamples() const;

	// Returns true if the task has ended
	bool	Update();
	void	UpdateAndGetLastErrorCode( RecordingTask& task );

	void	MarkForCleanUp()		{ m_CanCleanUp = true; }
	bool	IsMarkedForCleanUp()	{ return m_CanCleanUp; }

	inline bool GetHasBeenSuspended() const { return m_hasBeenSuspended; }
	inline void SetHasBeenSuspended() { m_hasBeenSuspended = true; }

	inline bool GetHasBeenConstrained() const { return m_isConstrained; }
	inline void SetHasBeenConstrained( bool const isConstrained ) { m_isConstrained = isConstrained; }

private:

	bool								m_CanCleanUp;
	rage::RecordingTask*				m_pRecordingTask;

	rage::RecordingTask::ERROR_STATE	m_lastError;
	bool								m_hasBeenSuspended;
	bool								m_isConstrained;

#if RSG_ORBIS
	bool								m_takenScreenshot;
	bool								m_takeScreenshotNextActiveFrame;
#endif
};

class VideoRecording
{
public: // declarations and variables

	class VideoRecordingDataProviderAdapter : public IVideoRecordingDataProvider
	{
	public:
		virtual void GetGameFramebufferDimensions( u32& out_width, u32& out_height ) const;
		virtual void GetVideoOutputDimensions( u32& out_width, u32& out_height ) const;

		virtual MediaEncoderParams::eQualityLevel GetRecordingQualityLevel() const;
		virtual MediaEncoderParams::eOutputFps GetRecordingFps() const;
		virtual float GetExportFrameDurationMs() const;
		virtual MediaCommon::eVideoFormat GetFormatRequested() const;

		virtual bool IsRecordingVideo() const;
		virtual bool IsRecordingPaused() const;
		virtual bool IsRecordingReadyForSamples() const;
		virtual bool IsAudioFrameCaptureAllowed() const;
		virtual bool HasRecordingErrored() const;

		virtual u32 GetEstimatedDurationMs() const;
		virtual const wchar_t* GetGameTitle() const;
		virtual const wchar_t* GetCopyright() const;
	};

	static VideoRecordingDataProviderAdapter sm_recordingDataAdapter;

public:

	static	void			Init(unsigned initMode);
	static	void			Shutdown(unsigned shutdownMode);
	static	void			Update();

	static void				EnableRecording();
	static void				DisableRecording();

	// These should probably pass the VideoInfo rather than name?
	static bool				StartRecording( s32& out_instanceIndex, const char *pName, bool const startPaused = false, bool const synchronized = false, bool const hasModdedContent = false
		SAVE_VIDEO_ONLY(, const char *pSaveName = NULL));

	static bool				IsPaused();
	static bool				IsPaused( s32 id );
	static void				PauseRecording( s32 id );
	static void				ResumeRecording( s32 id );

	static bool				IsAudioFrameCaptureAllowed();

	static bool										HasError();
	static bool										HasError( s32 id );
	static VideoRecordingInstance::RECORDING_ERROR	GetLastError( s32 id );
	static char const *								GetErrorLngKey( VideoRecordingInstance::RECORDING_ERROR const errorCode );

	static bool										IsReadyForSamples();
	static bool										IsReadyForSamples( s32 id );

	static bool	HasBeenSuspended( s32 id );
	static bool	HasBeenConstrained( s32 id );

	static void				StopRecording(s32 idx);
	static void				CancelRecording(s32);
	static bool				IsFinalizing( s32 const idx );

	static bool				IsRecording();
	static size_t			EstimateSizeOfRecording( u32 const durationMs, rage::MediaEncoderParams::eQualityLevel const qualityLevel );
#if RSG_PC
	static void				SetMetadataInfo(u32 const estimatedDurationMs);
#endif
	static void				SetExportFps(MediaEncoderParams::eOutputFps fps) {ms_outputFpsLevel = fps;}
	static void				SetExportQuality(MediaEncoderParams::eQualityLevel quality) {ms_qualityLevel = quality;}

	static rage::VideoRecordingInterface *GetRecordingInterface() { return pRecordingInterface; }

#if (RSG_PC || RSG_DURANGO)
	static void InitializeExtraRecordingData( char const * const directoryOverride = NULL, bool makePermanent = false );
#endif

#if USES_MEDIA_ENCODER
	static void CaptureVideoFrame(float const frameDuration);
#endif

	static void RenderThreadInit();
	static void RenderThreadCleanup();

#if __BANK

	static bool HasOutputFormatOverride() { return ms_formatOverride < MediaCommon::VIDEO_FORMAT_MAX; }
	static MediaCommon::eVideoFormat GetFormatOverride() { return (MediaCommon::eVideoFormat)ms_formatOverride; }

#endif

private:

	static VideoRecordingInstance &GetRecordingInstanceFromID (s32 const taskID)
	{
		return m_RecordingInstances[taskID];
	}

	static s32	FindUnusedRecordingInstanceID();

	static void sysOnServiceEvent( rage::sysServiceEvent* evnt);

	static void	ProcessStopRecording(s32 idx, bool const cancelled);

	// A pointer to the recording interface
	static rage::VideoRecordingInterface *pRecordingInterface;

	// Some storage for the recording tasks created.
	static atFixedArray<VideoRecordingInstance, MAX_VIDEORECORDING_INSTANCES> m_RecordingInstances;

	static ServiceDelegate ms_serviceDelegate;
	static MediaEncoderParams::eQualityLevel	ms_qualityLevel;
	static MediaEncoderParams::eOutputFps		ms_outputFpsLevel;

#if VIDEO_CONVERSION_ENABLED
	static VideoRecordingFrameBuffer ms_VideoFrameConverter;
	static s32 ms_idToStop;
	static sysCriticalSectionToken m_frameBufferCsToken;
#endif

#if RSG_PC
	static u32 ms_estimatedDurationMs;
	static const wchar_t* ms_gameTitle;
	static const wchar_t* ms_copyright;
#endif

#if (RSG_PC || RSG_DURANGO)
	static bool		ms_UsingPermanentVideoPath;
#endif

#if __BANK

	// RAG widgets
	static bkBank	*ms_pBank;
	static bkButton	*ms_pCreateButton;

	// Clip offset & duration
	static s32		ms_StartOffset;
	static s32		ms_Duration;

	static s32		ms_RecordTaskID;

	static bool		ms_RecordToDisc;
	static int		ms_qualityOverride;
	static int		ms_fpsOverride;
	static int		ms_formatOverride;
	static bool		ms_PauseCapture;

	static void InitBank();
	static void CreateBank();
	static void UpdateBank();
	static void ShutdownBank();

	static void StartRecordingCB();
	static void StopRecordingCB();
	static void CancelRecordingCB();

	// Delete all the clips
	static void DeleteAllRecordingsCB();

	// Display Info about all clips in the cloud
	static void DumpAllRecordingsInfoCB();

#endif	//__BANK

};

#endif	//VIDEO_RECORDING_ENABLED

#endif // _VIDEORECORDING_H_