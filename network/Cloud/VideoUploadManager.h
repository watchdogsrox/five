#ifndef VIDEOUPLOADMANAGER_H
#define VIDEOUPLOADMANAGER_H

#include "control/replay/ReplaySettings.h"
#include "rline/ugc/rlugccommon.h"

#if defined(GTA_REPLAY) && GTA_REPLAY

CompileTimeAssert(RL_UGC_YOUTUBE); // If this fails, you need to have this defined to true in rlugccommon.h

#include "atl/string.h"
#include "atl/singleton.h"
#include "Network/Cloud/Tunables.h"
#include "system/service.h"

#define ENABLE_VIDEO_UPLOAD_THROTTLES 0
#define COMPETITION_STRING_LENGTH 128


enum eVideoUploadPrivacy
{
	VIDEO_UPLOAD_PRIVACY_PUBLIC = 0,
	VIDEO_UPLOAD_PRIVACY_UNLISTED,
	VIDEO_UPLOAD_PRIVACY_PRIVATE
};

class CVideoUploadTunablesListener : public TunablesListener
{
public:
#if !__NO_OUTPUT
	CVideoUploadTunablesListener() : TunablesListener("CVideoUploadTunablesListener") {}
#endif
	virtual void OnTunablesRead();
};

struct VideoUploadTextInfo
{
	static const u32 MAX_VIDEOUPLOAD_TEXT = 256;	// # of characters
	static const u32 MAX_VIDEOUPLOAD_TEXT_BYTES = MAX_VIDEOUPLOAD_TEXT * 3; // # of bytes
};

class CVideoUploadManager
{
public:
	
	struct UploadProgressState
	{
		enum Enum
		{
			NotUploaded,
			Uploading,
			Processing,
			Uploaded
		};
	};

	CVideoUploadManager();
	virtual ~CVideoUploadManager();

	virtual bool RequestPostVideo(const char* sourcePathAndFileName, u32 duration, const char* createdDate, const char* displayName, s64 contentId = 0) = 0;

	virtual void Init();
	virtual void Shutdown();

	virtual void Update();

	void UpdateTunables();

	bool IsUploadEnabled() const { return m_uploadCurrentlyEnabled && (m_localGamerIndex>=0) && (m_userID != 0); }

	bool HasCorrectPrivileges() const { return m_hasCorrectPrivileges; }

protected:
	bool UpdateUsers();
	bool GatherHeaderData(const char* sourcePathAndFileName, u32 duration, const char* createdDate, const char* displayName);

	void ResetCoreValues();

	s32 m_localGamerIndex;
#if RSG_DURANGO
	u64 m_userID;
#elif RSG_ORBIS
	s32 m_userID;
#else // PC but should be valid for any other platform with no users, just use social club instead
	s64 m_userID;
#endif

	bool m_enableYoutubeUploads;
	bool m_enableYoutubePublic;
	bool m_enableAlbumAdvert;
	u32 m_advertTrackId;
	bool m_serverKillSwitch;
	bool m_enableResetAuthOn401;
	
	bool m_uploadCurrentlyEnabled;
	bool m_hasCorrectPrivileges;

	atString m_sourcePathAndFileName;
	atString m_displayName;
	atString m_customDesc;
	atString m_customTag;
	atString m_competitionName;
	atString m_competitionTag;
	
	atString m_sourceCreatedDate;
	u64 m_videoSize;
	u32 m_videoDurationMs;

private:
	void RefreshUploadEnabledFlag();
	void ReadTunables();
	void SetInitialTunables();
	static CVideoUploadTunablesListener* m_pTunablesListener;

};

#if USE_ORBIS_UPLOAD
class CVideoUploadManager_Orbis : public CVideoUploadManager
{
public:
	virtual bool RequestPostVideo(const char* sourcePathAndFileName, u32 duration, const char* createdDate, s64 contentId = 0);

	virtual void Init();
	virtual void Shutdown();

	virtual void Update();

	UploadProgressState::Enum GetProgressState(s64 contentId, u32 duration, const char* createdDate) const;
};

typedef atSingleton<CVideoUploadManager_Orbis> VideoUploadManager;

#else

namespace rage
{
	struct rlYoutubeUpload;
}

class CVideoUploadManager_Custom : public CVideoUploadManager
{
public:
	virtual bool RequestPostVideo(const char* sourcePathAndFileName, u32 duration, const char* createdDate, const char* displayName, s64 contentId = 0);

	virtual void Init();
	virtual void Shutdown();

	virtual void Update();

	UploadProgressState::Enum GetProgressState(const char *sourcePathAndFileName, u32 durationMs, u64 size);

	void PrepOptimisedUploadedList();
	void FinaliseOptimisedUploadedList();

	bool UGCQueryYoutubeContent(const int offset = 0, const int maxQueries = 500);

	static void OnServiceEvent( rage::sysServiceEvent* evnt );
	static void OnPresenceEvent(const rlPresenceEvent* evt);
	static void UGCVideoQueryContentDelegate(const int nIndex,
		const char* szContentID,
		const rlUgcMetadata* pMetadata,
		const rlUgcRatings* pRatings,
		const char* szStatsJSON,
		const unsigned nPlayers,
		const unsigned nPlayerIndex,
		const rlUgcPlayer* pPlayer);

	static bool IsCompetitionOn(char* out_name, u32 nameLength, char* out_tag, u32 tagLength, s32& minDuration, s32& maxDuration);

#if RSG_ORBIS
	static void PostToActivityFeed(const char* videoId);
#endif

	void LinkSuccess(bool success);

	void ReturnConfirmationResult(bool result);
	void ReturnStringResult(const char* string);

	// if we want to save progress and/or show progress
	bool IsUploading() const;
	bool IsInSetup() const;
	bool IsProcessing() const;
	bool IsAddingText() const;
	f32 GetUploadFileProgress() const;
	bool IsGettingOAuth() const;
	bool IsAuthORecieved() const;
	const char* GetSourcePathAndFileName() const;

	bool IsPathTheCurrentUpload(const char *path) const;

	void PushToRawUploadedList(u32 filenameHash, u32 durationMs, u64 size);

	// bit wasteful, but didn't want to risk just having flags around so many resets
	// or have a big callback system 
	u32 GetUploadedCounterValue() const;
	u32 GetYTFailedSetupCounterValue()  const;
	u32 GetYTFailedCounterValue() const;
	u32 GetUGCFailedCounterValue() const;
	u32 GetAlreadyUploadedCounterValue() const;

	void StopUpload(bool cancelCompletely = true, bool silent = false, bool deleteUploadSaveFile = true);

	void CancelTaskIfNecessary(netStatus* status);

	void NoTextEntered();

	void SetForcePause(bool pause);
	bool IsUploadingPaused();

	// TODO: should replace with multiple of 256k
	struct BlockSizeFraction
	{
		enum Enum
		{
			Full = 1,
			Half = 2,
			Quarter = 4,
			Eighth = 8
		};
	};

#if ENABLE_VIDEO_UPLOAD_THROTTLES
	void SetBlockSize(BlockSizeFraction::Enum blockSize);
	void SetIdealUploadSpeed(u32 speed); // kb/s 
#endif // ENABLE_VIDEO_UPLOAD_THROTTLES

private:

	struct ReadVideoBlockWorker : public rlWorker
	{
		ReadVideoBlockWorker();
		bool Configure(u8* buf, u32 bufLen, u64 readOffset, const char* pathAndFilename, netStatus* status);
		bool Start(const int cpuAffinity);
		bool Stop();

	private:

		//Make Start() un-callable
		bool Start(const char* /*name*/,
			const unsigned /*stackSize*/,
			const int /*cpuAffinity*/)
		{
			FastAssert(false);
			return false;
		}

		virtual void Perform();

		u32 m_bufLen;
		u64 m_readOffset;
		u8* m_buffer;
		atString m_pathAndFilename;

		netStatus* m_status;
	};

	struct SaveHeaderDataWorker : public rlWorker
	{
		SaveHeaderDataWorker();
		bool Configure(rage::rlYoutubeUpload* uploadData, const char* savePathAndFilename, const char* videoPathAndFilename, u32 videoDurationMs, const char* createdDate, const char* displayName, const char* description, const char* customTag, netStatus* status);
		bool Start(const int cpuAffinity);
		bool Stop();

	private:

		//Make Start() un-callable
		bool Start(const char* /*name*/,
			const unsigned /*stackSize*/,
			const int /*cpuAffinity*/)
		{
			FastAssert(false);
			return false;
		}

		virtual void Perform();

		rage::rlYoutubeUpload* m_uploadData;
		atString m_savePathAndFilename;
		atString m_videoPathAndFilename;
		atString m_createdDate;
		atString m_displayName;
		atString m_description;
		atString m_customTag;
		u32 m_videoDurationMs;

		netStatus* m_status;
	};

	struct DeleteFileWorker : public rlWorker
	{
		DeleteFileWorker();
		bool Configure(const char* pathAndFilename, netStatus* status);
		bool Start(const int cpuAffinity);
		bool Stop();

		const char* GetPathAndFileName() { return m_pathAndFilename.c_str(); }

	private:

		//Make Start() un-callable
		bool Start(const char* /*name*/,
			const unsigned /*stackSize*/,
			const int /*cpuAffinity*/)
		{
			FastAssert(false);
			return false;
		}

		virtual void Perform();

		atString m_pathAndFilename;
		netStatus* m_status;
	};

	enum {
		MaxNoofRetrys = 3,
		MaxNoofDisconnectRetry = 1,
		MaxUploadSpeed = 30, // kb / s  ...currently set as 1kb per frame, going to be changed soon
		MinBlockSize = (256 * 1024), // 256kb ... youtube demands this as a minimum, and the upload block must be a multiple of this
		MaxBlockSize = (512 * 1024), // 512 kb,
		TextBoxStringLength = 256
	};

	// broad stages for updating UI
	struct UploadVideoStage
	{
		enum Enum
		{
			Setup,
			Uploading,
			Processing,
			Other
		};
	};
	UploadVideoStage::Enum m_currentVideoStage;

	struct UploadState
	{
		enum Enum
		{
			Unused,
			UploadPaused,
			SocialClubSignup,
			GettingOAuth,
			LinkingAccounts,
			StartChannelCheck,
			ChannelCheck,
			AcceptPolicies,
			AddTags,
			StartSending,
			UploadingHeader,
			SaveHeaderData,
			SavingHeaderData,
			StartReadingVideoBlock,
			ReadingVideoBlock,
			UploadingBlock,
			StartGetUploadedVideoInfo,
			GetUploadedVideoInfo,
			UploadingPublish,
			GettingStatus,
			RequestResume,
			ResumeChecks,
			UploadComplete,
			UploadYTSetupFail,
			UploadYTChannelFail,
			UploadYTFail,
			UploadUGCFail,
			CancelledInSetup
		};
	};
	UploadState::Enum m_currentState;
	UploadState::Enum m_beforePauseState;

	struct UploadScreen
	{
		enum Enum
		{
			None,
			LinkToAccount,
			LinkPermissions,
			LinkToAccountSuccess,
			LinkToAccountFail,
			LinkToAccountCancel,
			SharingPolicies,
			TitleText,
			DescText,
			TagText,
			CompTagText,
			NowPublishing,
			AlreadyPublishing,
			Cancelled,
			Failed,
			FailedChannel
		};
	};
	UploadScreen::Enum m_screenToShow;
	UploadScreen::Enum m_screenShowing;

	bool IsOnGalleryMenu();

	void UpdatePauseFlags();
	bool UpdateWarnings();
	bool UpdateUGCUploadList();

	// internal helper functions
	bool AddTitle(const char* string);
	bool AddDescription(const char* string);
	bool AddYTTag(const char* string);

	void CreateUploadInfo();
	void CreateTags();
	bool RestoreSavedUploadInfo(const char *pathAndFilename);
	bool GetOAuthToken();
	bool RefreshOAuthToken();
	bool GetOAuthTokenWithExistingUploadInfo();
	bool GetChannelInfo();
	bool SendHeader();
	bool RequestReadVideoBufferBlock();
	bool UploadNextBlock();
	bool StartPublishUGC();
	bool PublishUGC();
	bool CheckTextOnUGC(UploadScreen::Enum textScreen, const char* string);
	bool GetUploadStatus();
	void TestStringAgainstUGC(const char* string);
	void SubmitStringToUploadDetails(const char* string);

	bool StartNetRetryTimer(s32 minTime, s32 maxTime = 0);
	void StopNetRetryTimer();
	bool UpdateNetRetryTimer();

	void Reset(bool deleteUploadSaveFile = true);

	
	struct PauseFlags
	{
		enum Enum
		{
			LoadingScreen,
			Online,
			Forced,
			Exporting,
			Suspend
		};
	};
	u8 m_pauseFlags;
	bool m_forcePause;
	bool m_suspendPause;

	bool ResumeUploadUsingParameters(const char* sourcePathAndFileName, u32 videoDuration, const char* createdDate);

	void ForcePauseStateOnSuspend(bool pause);
	void PauseUpload();

	void RequestResumeUpload();
	bool StartResumeUpload();
	bool ResumeUpload();

	u8 GetLatestPauseFlags();
	static bool IsUploadSaveFileDeleting();
	static bool DeleteUploadFile(const char* path);

	void FailUpload(UploadState::Enum m_failState = UploadState::UploadYTFail, s32 errorCode = 0, const char* failDetails = NULL);

	bool m_linkedToAccountsAttempted;
	bool m_acceptPolicies;
	bool m_oAuthSuccess;
	bool m_resumeSetup;
	bool m_headerSuccess;
	bool m_noTextEntered;
	bool m_hasCheckedForResumeFile;

	float m_uploadProgressForNextInfo;

	atString m_currentUploadSavePathAndFilename;
	
	// the concept of keeping count of blocks can be removed now we're onto youtube
	BlockSizeFraction::Enum m_nextBlockSizeFraction;
	BlockSizeFraction::Enum m_currentBlockSizeFraction;

	u32 m_uploadedCounter;
	u32 m_failedYTSetupCounter;
	u32 m_failedYTCounter;
	u32 m_failedUGCCounter;
	u32 m_alreadyUploadedCounter;
	s32 m_replaceGameStreamId;

	u32 m_errorCode;
	atString m_errorDetails;

	netRetryTimer m_netRetryTimer;
	s32 m_netRetryMinTimeSeconds;
	s32 m_netRetryMaxTimeSeconds;
	bool m_netRetryTimerActive;

	u32 m_maxUploadSpeed;
	u32 m_throttleSpeed;

	u32 m_maxBlockSize;
	u32 m_currentUploadBlockSize;

	rlUgcMetadata m_UgcMetadata;
	netStatus m_MyStatus;
	char m_profaneWord[64];
	u8* m_pBuffer;
	ReadVideoBlockWorker* m_readBlockWorker;
	SaveHeaderDataWorker* m_saveHeaderWorker;

	static DeleteFileWorker* m_deleteFileWorker;
	static netStatus* m_deleteFileStatus;

	static ServiceDelegate ms_serviceDelegate;

	rage::rlYoutubeUpload* m_currentYTUpload;
	u8 m_retryCounter;
	u8 m_disconnectRetryCounter;

	char m_textBoxString[TextBoxStringLength];
	bool m_textBoxValid;

	struct GetUploadedListState
	{
		enum Enum
		{
			NotStarted = 0,
			GettingNoofVideos,
			BuildList,
			WaitForList,
			DoneWaitingOnOptimisation,
			Done,
			Failed
		};
	};

	int m_noofUGCVideos;
	int m_UGCVideosOffset;

	GetUploadedListState::Enum m_getUploadedListState;

	struct UploadedInfo
	{
		u32 m_filenameHash;
		u32 m_durationMs;
		u64	m_size;
	};

	typedef atArray<UploadedInfo> UploadedList;
	UploadedList m_uploadedList;
	UploadedList m_uploadedRawList;

	static void GenerateDateString(atString& string);
	static time_t					sm_SaveTimeStamp;
};

typedef atSingleton<CVideoUploadManager_Custom> VideoUploadManager;

#endif // !USE_ORBIS_UPLOAD

#endif // defined(GTA_REPLAY) && GTA_REPLAY

#endif // VIDEOUPLOADMANAGER_H
