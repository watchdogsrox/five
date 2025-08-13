#include "Network/Cloud/VideoUploadManager.h"

#if defined(GTA_REPLAY) && GTA_REPLAY 
#if !USE_ORBIS_UPLOAD

#include "Network/Cloud/CloudManager.h"
#include "string/stringhash.h"
#include "fwsys/timer.h"
#include "fwnet/netchannel.h"
#include "file/asset.h"
#include <time.h>

#include "rline/rl.h"
#include "rline/youtube/rlyoutubetasks.h"
#include "rline/rlpresence.h"
#include "rline/ros/rlroscommon.h"
#include "rline/ugc/rlugc.h"

#if RSG_PC
#include "rline/rlsystemui.h"
#else
#include "frontend/SocialClubMenu.h"
#endif

#include "frontend/PolicyMenu.h"
#include "Network/Cloud/UserContentManager.h"
#include "Network/Live/NetworkTelemetry.h"
#include "frontend/VideoEditor/ui/Editor.h"
#include "control/replay/File/ReplayFileManager.h"
#include "system/filemgr.h"

#include "video/video_channel.h"
#include "frontend/GameStreamMgr.h"
#include "frontend/BusySpinner.h"
#include "frontend/loadingscreens.h"

#include "frontend/VideoEditor/ui/Menu.h"

#include "replaycoordinator/ReplayMetadata.h"
#include "Network/SocialClubServices/SocialClubCommunityEventMgr.h"

#include "network/live/NetworkTelemetry.h"

#include "control/videorecording/videorecording.h"

#if RSG_ORBIS
// used by activity feed scripts
#include "fwlocalisation/languagePack.h"
#endif

#if RSG_PC
#include "video/mediadecoder.h"
#endif

#define DISABLE_YOUTUBE_EXTRA_SAFETY 0

#define UPLOAD_RSON_BUFFER_SIZE 2096
#define UPLOAD_READ_BUFFER_SIZE 512

#define ALWAYS_SHOW_POLICIES 1

#define TEST_SUSPEND_WITH_CONSTRAIN 0

time_t CVideoUploadManager_Custom::sm_SaveTimeStamp = 0;

static rlPresence::Delegate g_VideoUploadDlgt;

static rlUgc::QueryContentDelegate sm_UGCVideoQueryDelegate;

CVideoUploadManager_Custom::DeleteFileWorker* CVideoUploadManager_Custom::m_deleteFileWorker = NULL;
netStatus* CVideoUploadManager_Custom::m_deleteFileStatus = NULL;

rage::ServiceDelegate CVideoUploadManager_Custom::ms_serviceDelegate;

// line up with UploadStates ... for use with Telemetry
static const char * g_stateNames[] = { 
	"Unused",
	"UploadPaused",
	"SocialClubJoin",
	"GettingOAuth",
	"LinkingAccounts",
	"ChannelCheck1",
	"ChannelCheck2",
	"AcceptPolicies",
	"AddTags",
	"StartSending",
	"UploadingHeader",
	"SaveHeader",
	"SavingHeader",
	"ReadingVideo1",
	"ReadingVideo2",
	"UploadingVideo",
	"GetVideoInfo1",
	"GetVideoInfo2",
	"Publish",
	"GettingStatis",
	"RequestResume",
	"ResumeChecks",
	"UploadComplete",
	"YTSetupFail",
	"YTChannelFail",
	"YTFail",
	"UGCFail",
	"Cancelled"
};

static u32 FilenameHashFromFullPath(const char* path)
{
	if (path)
	{
		const char* filename = ASSET.FileName(path);
		if (filename)
		{
			return atStringHash(filename);
		}
	}

	return 0;
}

void CVideoUploadManager_Custom::Init()
{
	CVideoUploadManager::Init();

	m_pBuffer = NULL;
	m_currentYTUpload = NULL;
	m_acceptPolicies = false;
	m_currentState = UploadState::Unused;
	m_currentVideoStage = UploadVideoStage::Other;
	m_beforePauseState = UploadState::Unused;
	m_screenToShow = UploadScreen::None;
	m_screenShowing = UploadScreen::None;
	m_readBlockWorker = NULL;
	m_saveHeaderWorker = NULL;
	Reset();

	m_uploadedCounter = 0;
	m_failedYTCounter = 0;
	m_failedUGCCounter = 0;
	m_alreadyUploadedCounter = 0;
	m_replaceGameStreamId = -1;

	m_maxUploadSpeed = MaxUploadSpeed; // set as tunable when instantiated
	m_throttleSpeed = m_maxUploadSpeed; //kb/s

	m_nextBlockSizeFraction = BlockSizeFraction::Full;
	m_currentBlockSizeFraction = BlockSizeFraction::Full;

	m_maxBlockSize = MaxBlockSize; // set as tunable when instantiated
	m_currentUploadBlockSize = 0;

	if (!g_VideoUploadDlgt.IsBound())
	{
		g_VideoUploadDlgt.Bind(CVideoUploadManager_Custom::OnPresenceEvent);
		rlPresence::AddDelegate(&g_VideoUploadDlgt);
	}

	ms_serviceDelegate.Bind(CVideoUploadManager_Custom::OnServiceEvent);
	g_SysService.AddDelegate(&ms_serviceDelegate);

	m_hasCheckedForResumeFile = false;

	m_forcePause = false;
	m_suspendPause = false;
	m_pauseFlags = 0;
	m_pauseFlags |= PauseFlags::LoadingScreen;

	m_deleteFileStatus = rage_new netStatus;

	m_noofUGCVideos = 0;
	m_UGCVideosOffset = 0;
	m_getUploadedListState = GetUploadedListState::NotStarted;
	m_retryCounter = MaxNoofRetrys;

	m_netRetryTimer = netRetryTimer();
	m_netRetryTimer.Stop();
	m_netRetryMinTimeSeconds = 0;
	m_netRetryMaxTimeSeconds = 0;
	m_netRetryTimerActive = false;

	if (!sm_UGCVideoQueryDelegate.IsBound())
		sm_UGCVideoQueryDelegate.Bind(CVideoUploadManager_Custom::UGCVideoQueryContentDelegate);
}

void CVideoUploadManager_Custom::Shutdown()
{
	if (m_pBuffer)
		delete m_pBuffer;
	m_pBuffer = NULL;

	if (m_currentYTUpload)
		delete m_currentYTUpload;
	m_currentYTUpload = NULL;

	if (m_readBlockWorker)
	{
		delete m_readBlockWorker;
		m_readBlockWorker = NULL;
	}

	if (m_saveHeaderWorker)
	{
		delete m_saveHeaderWorker;
		m_saveHeaderWorker = NULL;
	}

	if (m_deleteFileWorker)
	{
		delete m_deleteFileWorker;
		m_deleteFileWorker = NULL;
	}

	if (m_deleteFileStatus)
	{
		delete m_deleteFileStatus;
		m_deleteFileStatus = NULL;
	}

	g_SysService.RemoveDelegate(&ms_serviceDelegate);
	ms_serviceDelegate.Reset();

	if (g_VideoUploadDlgt.IsBound())
		g_VideoUploadDlgt.Unbind();

	rlPresence::RemoveDelegate(&g_VideoUploadDlgt);

	if (!sm_UGCVideoQueryDelegate.IsBound())
		sm_UGCVideoQueryDelegate.Unbind();

	CVideoUploadManager::Shutdown();
}

void CVideoUploadManager_Custom::OnServiceEvent( rage::sysServiceEvent* evnt )
{
	if(evnt != NULL)
	{
		switch(evnt->GetType())
		{
		case sysServiceEvent::SUSPENDED:
		case sysServiceEvent::SUSPEND_IMMEDIATE:
#if TEST_SUSPEND_WITH_CONSTRAIN
		case sysServiceEvent::CONSTRAINED:
#endif
			{
				VideoUploadManager::GetInstance().ForcePauseStateOnSuspend(true);
				videoDisplayf("ForcePauseStateOnSuspend - true");
			}
			break;
		case sysServiceEvent::RESUMING:
		case sysServiceEvent::RESUME_IMMEDIATE:
		case sysServiceEvent::GAME_RESUMED:
		case sysServiceEvent::UNCONSTRAINED:
			{
				VideoUploadManager::GetInstance().ForcePauseStateOnSuspend(false);
				videoDisplayf("ForcePauseStateOnSuspend - false");
			}
			break;
		default:
			{
				break;
			}
		}
	}
}

void CVideoUploadManager_Custom::OnPresenceEvent(const rlPresenceEvent* evt)
{
	if(PRESENCE_EVENT_SC_MESSAGE == evt->GetId())
	{
		const rlScPresenceMessage& msgBuf = evt->m_ScMessage->m_Message;
		if(msgBuf.IsA<rlScPresenceAccountLinkResult>())
		{
			rlScPresenceAccountLinkResult msg;
			if (rlVerifyf(msg.Import(msgBuf), "CVideoUploadManager_Custom::OnPresenceEvent - Error importing '%s'", msg.Name()))
			{
				if (msg.m_Success)
				{
					VideoUploadManager::GetInstance().LinkSuccess(true);
					return;
				}
			}

			VideoUploadManager::GetInstance().LinkSuccess(false);
		}
	}
}

void CVideoUploadManager_Custom::UGCVideoQueryContentDelegate(const int nIndex,
							   const char* szContentID,
							   const rlUgcMetadata* pMetadata,
							   const rlUgcRatings* UNUSED_PARAM(pRatings),
							   const char* UNUSED_PARAM(szStatsJSON),
							   const unsigned UNUSED_PARAM(nPlayers),
							   const unsigned UNUSED_PARAM(nPlayerIndex),
							   const rlUgcPlayer* UNUSED_PARAM(pPlayer))
{
#if __NO_OUTPUT
	UNUSED_VAR(nIndex);
	UNUSED_VAR(szContentID);
#else
	videoDisplayf("UGCVideoQueryContentDelegate :: [%d] ID: %s, Title: %s", nIndex, szContentID, pMetadata->GetContentName());
	videoDisplayf("UGCVideoQueryContentDelegate :: [%d] Metadata: %s", nIndex, pMetadata->GetData());
#endif

	rlUgcYoutubeMetadata metadata;
	if (UserContentManager::GetInstance().ReadYoutubeMetadata(pMetadata->GetData(), pMetadata->GetDataSize(), &metadata))
	{
		if (metadata.nFilenameHash != 0 && metadata.nSize != 0 && metadata.nDuration_ms != 0)
			VideoUploadManager::GetInstance().PushToRawUploadedList(metadata.nFilenameHash, metadata.nDuration_ms, metadata.nSize);
	}
}

bool CVideoUploadManager_Custom::IsCompetitionOn(char* out_name, u32 nameLength, char* out_tag, u32 tagLength, s32& minDuration, s32& maxDuration)
{
// 	{  
// 		"multiplayerEvents":[  
// 		{  
// 			"posixStartTime":1425186000,
// 				"posixEndTime":1426219200,
// 				"eventRosGameTypes":[  
// 					"gta5"
// 				],
// 				"eventPlatformTypes":[  
// 					"pcros"
// 				],
// 				"displayName":"Editor Contest Test",
// 				"eventId":106078,
// 				"extraData":{  
// 					"tag":"yolo",
// 						"eventType":"videoEditor",
// 						"minLengthSeconds":90,
// 						"maxLengthSeconds":305
// 					}
// 		}
// 		]
// 	}

	// platform and game are filtered within event manager
	s32 eventId = SocialClubEventMgr::Get().GetActiveEventCode("videoEditor");
	if (eventId != 0)
	{
		char stringBuffer[COMPETITION_STRING_LENGTH];
		if (SocialClubEventMgr::Get().GetEventDisplayNameById(eventId, stringBuffer))
		{
			if (nameLength > COMPETITION_STRING_LENGTH)
				nameLength = COMPETITION_STRING_LENGTH;

			safecpy(out_name, stringBuffer, nameLength);

			if (SocialClubEventMgr::Get().GetExtraEventData(eventId, "tag", stringBuffer))
			{
				if (tagLength > COMPETITION_STRING_LENGTH)
					tagLength = COMPETITION_STRING_LENGTH;

				safecpy(out_tag, stringBuffer, tagLength);

				// going to allow duration to be optional. if zero, then any length is cool
				minDuration = 0;
				SocialClubEventMgr::Get().GetExtraEventData(eventId, "minLengthSeconds", minDuration);

				maxDuration = 0;
				SocialClubEventMgr::Get().GetExtraEventData(eventId, "maxLengthSeconds", maxDuration);

				return true;
			}
		}
	}

	return false;
}

#if RSG_ORBIS
void CVideoUploadManager_Custom::PostToActivityFeed(const char* videoId)
{
	const char* captionString = "AF_YOUTUBE";
	const char* condensedCaptionString = "AF_YOUTUBE_COMB";
	const char* labelString = "AF_YOUTUBE_LINK";

	int key = atStringHash(condensedCaptionString);
	g_rlNp.GetNpActivityFeed().Start(key);

	for (sysLanguage language = LANGUAGE_ENGLISH ; language < MAX_LANGUAGES; ++language)
	{
		const char* iso = fwLanguagePack::GetIsoLanguageCode( language );

		char caption[64] = {0};
		formatf( caption, sizeof(caption),"%s_%c%c", captionString, std::toupper(iso[0]), std::toupper(iso[1]));

		char condensed[64] = {0};
		formatf( condensed, sizeof(condensed),"%s_%c%c", condensedCaptionString, std::toupper(iso[0]), std::toupper(iso[1]));

		g_rlNp.GetNpActivityFeed().PostCaptions(language, TheText.Get(caption), TheText.Get(condensed));

		char label[64] = {0};
		formatf( label, sizeof(label),"%s_%c%c", labelString, std::toupper(iso[0]), std::toupper(iso[1]));

		g_rlNp.GetNpActivityFeed().PostTagForActionURL(language, TheText.Get(label));
	}

	//g_rlNp.GetNpActivityFeed().PostThumbnailImageURL("https://media.rockstargames.com/socialclub/activity/ps4/V/gtav_68x68.png"); // title image is the same for all posts

	// YT video not supported in custom posts
//	g_rlNp.GetNpActivityFeed().PostVideoURL("https://www.youtube.com/embed/KMJPasuw_7M");

	// video link
	atString linkUrl("https://www.youtube.com/watch?v=~a~");
	linkUrl.Replace("~a~", videoId);
	g_rlNp.GetNpActivityFeed().PostActionURL(linkUrl.c_str());

	// examples
// 	https://img.youtube.com/vi/~a~/default.jpg
// 	https://img.youtube.com/vi/~a~/hqdefault.jpg
// 	https://img.youtube.com/vi/~a~/mqdefault.jpg
// 	https://img.youtube.com/vi/~a~/sddefault.jpg
// 	https://img.youtube.com/vi/~a~/maxresdefault.jpg

	atString hqImageUrl("https://img.youtube.com/vi/~a~/maxresdefault.jpg");
	hqImageUrl.Replace("~a~", videoId);
	g_rlNp.GetNpActivityFeed().PostLargeImageURL(hqImageUrl.c_str());

	atString mqImageUrl("https://img.youtube.com/vi/~a~/mqdefault.jpg");
	mqImageUrl.Replace("~a~", videoId);
	g_rlNp.GetNpActivityFeed().PostSmallImageURL(mqImageUrl.c_str(), "16:9");

	g_rlNp.GetNpActivityFeed().PostCurrentMessage();
}
#endif


void CVideoUploadManager_Custom::LinkSuccess(bool success)
{
	if (m_screenShowing != UploadScreen::LinkToAccountCancel)
		return;

	if (success)
		m_screenToShow = UploadScreen::LinkToAccountSuccess;
	else
		m_screenToShow = UploadScreen::LinkToAccountFail;

	CWarningScreen::Remove();
	m_screenShowing = UploadScreen::None;
}

bool CVideoUploadManager_Custom::RequestPostVideo(const char* sourcePathAndFileName, u32 duration, const char* createdDate, const char* displayName, s64 UNUSED_PARAM(contentId))
{
 	if (!IsUploadEnabled())
 		return false;

	if (!sourcePathAndFileName)
		return false;

	// already uploading a video ...go away!
	if (m_videoSize > 0)
		return false;

	if (!GatherHeaderData(sourcePathAndFileName, duration, createdDate, displayName))
		return false;

	m_forcePause = false;

	// don't bother even trying if there is no social club account
	// just notify until I workout how to get social club menu over editor
	const rlRosCredentials & cred = rlRos::GetCredentials(m_localGamerIndex);
	if( !cred.IsValid() || cred.GetRockstarId() == 0 )
	{
#if RSG_PC
		g_SystemUi.ShowSigninUi();
#else
		SocialClubMenu::Open();
#endif
		m_currentState = UploadState::SocialClubSignup;
		return true;
	}

	return GetOAuthToken();
}

CVideoUploadManager::UploadProgressState::Enum CVideoUploadManager_Custom::GetProgressState(const char *sourcePathAndFileName, u32 durationMs, u64 size)
{
	// if list isn't done, just return not uploaded
	if (m_getUploadedListState < GetUploadedListState::DoneWaitingOnOptimisation)
		return UploadProgressState::NotUploaded;

	bool buildOptimisedList = m_getUploadedListState == GetUploadedListState::DoneWaitingOnOptimisation;
	UploadedList& uploadList = buildOptimisedList ? m_uploadedRawList : m_uploadedList;

	if (IsPathTheCurrentUpload(sourcePathAndFileName) && IsUploading())
	{
		if (IsProcessing())
			return UploadProgressState::Processing;

		return UploadProgressState::Uploading;
	}

	for (u32 index = 0; index < uploadList.size(); ++index)
	{
		UploadedInfo const & info = uploadList[index];
		if (info.m_durationMs == durationMs)
		{
			if (info.m_size == size)
			{
				u32 filenameHash = FilenameHashFromFullPath(sourcePathAndFileName);
				if (info.m_filenameHash == filenameHash)
				{
					if (buildOptimisedList)
					{
						m_uploadedList.PushAndGrow(info);
					}

					return UploadProgressState::Uploaded;
				}
			}

		}
	}

	return UploadProgressState::NotUploaded;
}

void CVideoUploadManager_Custom::PrepOptimisedUploadedList()
{
	// stick in any resumed from restart uploads in proper list into raw, so they can get checked too
	if (m_getUploadedListState == GetUploadedListState::DoneWaitingOnOptimisation)
	{
		u32 uploadedListCount = m_uploadedList.GetCount();
		while (uploadedListCount > 0)
		{
			m_uploadedRawList.PushAndGrow(m_uploadedList[uploadedListCount-1]);
			--uploadedListCount;
		}
		m_uploadedList.Reset();
	}
}

void CVideoUploadManager_Custom::FinaliseOptimisedUploadedList()
{
	if (m_getUploadedListState == GetUploadedListState::DoneWaitingOnOptimisation)
	{
		m_getUploadedListState = GetUploadedListState::Done;
		m_uploadedRawList.Reset();
	}
}

bool CVideoUploadManager_Custom::IsOnGalleryMenu()
{
	return CVideoEditorUi::IsActive() && (CVideoEditorMenu::IsOnGalleryMenu() || CVideoEditorMenu::IsOnGalleryFilteringMenu());
}

void CVideoUploadManager_Custom::Update()
{
	CVideoUploadManager::Update();

	// dangerous to work with files to upload, unless the file manager for it is all set up
	// B*2520733 - an operation using ReplayFileManager::Fixname is failing because device not set up yet, on some timings
	if (!ReplayFileManager::IsInitialised())
		return;

	if (UpdateUsers())
	{
#if HAS_YT_DISCONNECT_MESSAGE
		CWarningScreen::BlockYTConnectionLossMessage( GetLatestPauseFlags() ? true : false );
#endif

#if RSG_ORBIS
		// Worth running this before we check the resume upload file
		// to make sure this local user Id is still the same for the PSN user
		ReplayFileManager::UpdateUserIDDirectoriesOnChange();
#endif
		// if true, there is a new user
		// stop any uploads, check for any uploads to resume
		if (m_userID != 0)
		{
			m_hasCheckedForResumeFile = false;
		}
		else if (IsUploading())
		{
			// need to force a pause if no user, for the resume code to work on a brief disconnect 
			// needed to kick off a resumeupload when a user does appear
			// (previously the loading screen pause did the trick, but brief disconnects don't have that luxury)
			m_forcePause = true;

#if HAS_YT_DISCONNECT_MESSAGE
			CWarningScreen::SetYTConnectionLossMessage( CLiveManager::IsSignedIn() && m_userID == 0 );
#endif
		}

		// make sure menu refreshes on change of state (could be from survivable disconnect)
		if (IsOnGalleryMenu())
		{
			CVideoEditorMenu::ForceUploadUpdate(m_userID ? true : false);
			CBusySpinner::Off(SPINNER_SOURCE_VIDEO_UPLOAD);
		}

		if (PolicyMenu::IsActive())
			PolicyMenu::Close();

		// also clear any already uploaded data
		m_getUploadedListState = GetUploadedListState::NotStarted;
		m_uploadedList.Reset();
		m_uploadedRawList.Reset();

		if (m_currentVideoStage == UploadVideoStage::Setup)
			FailUpload(UploadState::UploadYTSetupFail, -1, CLiveManager::IsAccountSignedOutOfOnlineService() ? TheText.Get("HUD_RESIGN") : TheText.Get("HUD_DISCON"));
		else
			StopUpload(true, true, false);

		// special case. had to keep this up, for UI polish, but on disconnect it doesn't go away making a black screens of nothingness
		if (m_currentState == UploadState::CancelledInSetup)
			m_currentState = UploadState::Unused;
	}

	if (m_localGamerIndex < 0)
		return;

 	if (!UpdateWarnings())
 		return;

	if (!m_hasCheckedForResumeFile && ReplayFileManager::IsInitialised() && IsUploadEnabled())
	{
		char path[RAGE_MAX_PATH];
		ReplayFileManager::getDataDirectory(path);
		m_currentUploadSavePathAndFilename = path;

// PS4 and XB1 already sort out a user directory, for PC (and potentially others) ... create a directory with SC nickname
#if !RSG_ORBIS && !RSG_DURANGO
		if (m_userID != 0)
		{
			u64 rockstarIdPositive = static_cast<u64>(m_userID + (ULLONG_MAX >> 1));
			char rockstarIdString[32];
			sprintf_s(rockstarIdString, 32, "%" I64FMT "u", rockstarIdPositive);

			m_currentUploadSavePathAndFilename += rockstarIdString;
			m_currentUploadSavePathAndFilename += "/";
		}
#endif

		atString fullPathAndFilename;
		char platformPath[RAGE_MAX_PATH];
		ReplayFileManager::FixName(platformPath, m_currentUploadSavePathAndFilename.c_str());
		fullPathAndFilename = platformPath;

		if ( ASSET.CreateLeadingPath( fullPathAndFilename.c_str() ) )
		{
			fullPathAndFilename += "currentupload.json";
			m_currentUploadSavePathAndFilename += "currentupload.json";

			if ( ASSET.Exists( fullPathAndFilename.c_str(), "json" ) )
			{
				RestoreSavedUploadInfo(fullPathAndFilename.c_str());

				CProfileSettings& settings = CProfileSettings::GetInstance();
				if(settings.AreSettingsValid())
				{
					if(settings.Exists(CProfileSettings::VIDEO_UPLOAD_PAUSE))
					{
						m_forcePause = settings.GetInt(CProfileSettings::VIDEO_UPLOAD_PAUSE) ? true : false;
					}
				}
			}
			m_hasCheckedForResumeFile = true;
		}
	}

	if (!UpdateUGCUploadList())
		return;

	UpdatePauseFlags();

	if (!UpdateNetRetryTimer())
		return;

	if (IsUploadingPaused())
		return;

	if (m_videoSize == 0)
		return;

	// Just make sure we still have permission to upload
	// unlikely, but bail out and generic message will be enough ...they'll know what they've done if this happens
	if (!IsUploadEnabled())
	{
		FailUpload(UploadState::UploadYTFail, -1);
		return;
	}

	// progress through blocks

	UploadVideoStage::Enum prevStage = m_currentVideoStage;

	// We initially send up a header with data like filename and filesize
	// if that is successful we then upload each block in sequence. 
	switch (m_currentState)
	{
		case UploadState::UploadYTSetupFail:
		case UploadState::UploadYTChannelFail:
			{
				++m_failedYTSetupCounter;
				videoDisplayf("CVideoUploadManager_Custom::Update - Youtube upload setup failed - see log");
				Reset();

				// need to reset beforePauseState here
				m_beforePauseState = UploadState::Unused;
			}
			break;
		case UploadState::UploadYTFail:
			{
				++m_failedYTCounter;
				m_replaceGameStreamId = CGameStreamMgr::GetGameStream()->PostMessageText( "", "CHAR_YOUTUBE", "CHAR_YOUTUBE", false, 0, TheText.Get("YT_YOUTUBE"), TheText.Get("YT_UPLOAD_FAILED"), 1.0f, "", 0, m_replaceGameStreamId );
				videoDisplayf("CVideoUploadManager_Custom::Update - Youtube upload failed - see log");
				Reset();

				// need to reset beforePauseState here
				m_beforePauseState = UploadState::Unused;
			}
			break;
		case UploadState::UploadUGCFail:
			{
				++m_failedUGCCounter;
				m_replaceGameStreamId = CGameStreamMgr::GetGameStream()->PostMessageText( "", "CHAR_YOUTUBE", "CHAR_YOUTUBE", false, 0, TheText.Get("YT_YOUTUBE"), TheText.Get("YT_UPLOAD_FAILED"), 1.0f, "", 0, m_replaceGameStreamId );
				videoDisplayf("CVideoUploadManager_Custom::Update - UGC upload failed - see log");
				Reset();

				// need to reset beforePauseState here
				m_beforePauseState = UploadState::Unused;
			}
			break;
		case UploadState::SocialClubSignup:
			{
#if RSG_PC
				if (!g_SystemUi.IsUiShowing())
#else					
				if (!SocialClubMenu::IsActive())
#endif

				{
					const rlRosCredentials & cred = rlRos::GetCredentials(m_localGamerIndex);
					if( !cred.IsValid() || cred.GetRockstarId() == 0 )
					{
#if GTA_REPLAY
						if (IsOnGalleryMenu())
							CVideoEditorMenu::SetUserConfirmationScreen( "YT_UPLOAD_NEED_SC" );
#endif	
						// not a real failure, so just stop as the previous message counts as the fail message
						StopUpload(true, true);
					}
					else
					{
						if (!GetOAuthToken())
						{
							videoDisplayf("CVideoUploadManager_Custom::Update - UploadState::SocialClubSignup - failed setting up GetOAuthToken");
							FailUpload(UploadState::UploadYTSetupFail, -1);
						}
					}
				}
			}
			break;
		case UploadState::GettingOAuth:
			{
				if (!m_MyStatus.Pending())
				{
					bool keepGoing = false;
					int result = m_MyStatus.GetResultCode();
					videoDisplayf("CVideoUploadManager_Custom::Update - GettingOAuth - result code %d", result);
					switch(result)
					{
					case rlRosGetYoutubeAccessTokenTask::RL_YT_ERROR_NONE:
						{
							if (m_MyStatus.Succeeded()) // probably not needed, but no harm
							{
								m_oAuthSuccess = true;

								if (m_resumeSetup)
								{
									m_currentState = UploadState::RequestResume;
								}
								else
								{
									// always go through channel check now 
									// ... we test for video length after, as we now use this test for various things
									// resume will end up in here after resuming
									m_currentState = UploadState::StartChannelCheck;

									// reset retry counter ... if Channel Check fails for YT quota reasons, we have to try a few times but bail out eventually
									m_retryCounter = MaxNoofRetrys;
								}

								m_linkedToAccountsAttempted = true;
								keepGoing = true;
							}
						}
						break;
					case rlRosGetYoutubeAccessTokenTask::RL_YT_ERROR_NOT_LINKED:
						{
							if (m_resumeSetup)
							{
								// if we're resuming, we'll want to fail quietly. if we're not linked, we just want to forget about this
								FailUpload(UploadState::UploadYTFail, 0);
								keepGoing = true;
							}
							else
							{
								m_currentState = UploadState::LinkingAccounts;
								m_screenToShow = UploadScreen::LinkToAccount;
								m_linkedToAccountsAttempted = true;
								keepGoing = true;
							}

						}
						break;
					default : break;
					};

					if (!keepGoing)
					{
						if (result == 400)
						{
							videoDisplayf("CVideoUploadManager_Custom::Update - UploadState::GettingOAuth - state failed - 400 - Bad Request");
							// mask ugly error number with custom string B*2411438
							FailUpload(UploadState::UploadYTSetupFail, -1, TheText.Get("YT_VIDEO_BAD_REQUEST"));
						}
						else if (result == 408)
						{
							videoDisplayf("CVideoUploadManager_Custom::Update - UploadState::GettingOAuth - state failed - 400 - Request Timeout");
							// mask ugly error number with custom string B*2411438
							// quite rare, only seems to happen on final when pulling network cable out (probably down to differing timeout timings)
							FailUpload(UploadState::UploadYTSetupFail, -1, TheText.Get("YT_VIDEO_REQUEST_TIMEOUT"));
						}
						else
						{
							videoDisplayf("CVideoUploadManager_Custom::Update - UploadState::GettingOAuth - state failed");
							FailUpload(UploadState::UploadYTSetupFail, result);
						}

					}
				}
			}
			break;
		case UploadState::StartChannelCheck:
			{
				if (GetChannelInfo())
				{
					m_currentState = UploadState::ChannelCheck;
				}
				else
				{
					videoDisplayf("CVideoUploadManager_Custom::Update - UploadState::StartChannelCheck - GetChannelInfo failed");
					FailUpload(UploadState::UploadYTSetupFail, -1);
				}
			}
			break;
		case UploadState::ChannelCheck:
			{
				if (!m_MyStatus.Pending())
				{
					if (m_MyStatus.Succeeded())
					{
						// if no channel title, then no channel ... bring up the error for that
						if (!m_currentYTUpload->ChannelInfo.IsLinked || m_currentYTUpload->ChannelInfo.ChannelTitle.GetLength() == 0)
						{
							videoDisplayf("CVideoUploadManager_Custom::Update - UploadState::ChannelCheck - no channel");
							FailUpload(UploadState::UploadYTChannelFail, m_currentYTUpload->m_ErrCode, m_currentYTUpload->m_ErrResponse.c_str());
						}
						else
						{
							bool success = false;

							switch (m_currentYTUpload->ChannelInfo.UploadStatus)
							{
							case rlYoutubeChannelInfo::LongUploadStatus::ALLOWED:
								{
									success = true;
								}
								break;
							case rlYoutubeChannelInfo::LongUploadStatus::ELIGIBLE:
								{
									const u32 maxVideoUploadLength = 15*60000;
									if (m_videoDurationMs > maxVideoUploadLength)
									{
										CVideoEditorMenu::SetUserConfirmationScreen( "YT_LONG_VIDEO_ELIGIBLE" );
										// not a real failure, so just stop as the previous message counts as the fail message
										StopUpload(true, true);
									}
									else
									{
										success = true;
									}
								}
								break;
							case rlYoutubeChannelInfo::LongUploadStatus::DISALLOWED:
							default: 
								{
									const u32 maxVideoUploadLength = 15*60000;
									if (m_videoDurationMs > maxVideoUploadLength)
									{
										if (m_currentYTUpload->ChannelInfo.IsGoodStanding)
										{
											CVideoEditorMenu::SetUserConfirmationScreen( "YT_LONG_VIDEO_NO" );
										}
										else
										{
											CVideoEditorMenu::SetUserConfirmationScreen( "YT_LONG_VIDEO_NOT_GOOD_STANDING" );
										}

										StopUpload(true, true);
									}
									else
									{
										// despite being "disallowed" we can still upload videos 15 or less
										success = true;
									}
								}
								break;
						
							};

							// if not in good standing, show another error stating that this is the case
							// only sub 15 mins, public videos can be posted
							if (success)
							{
								if (!m_currentYTUpload->ChannelInfo.IsGoodStanding)
								{
									if (m_currentYTUpload->VideoInfo.Privacy == rage::rlYoutubeVideoInfo::PRIVATE)
									{
										CVideoEditorMenu::SetUserConfirmationScreen( "YT_PRIVATE_NOT_GOOD_STANDING" );
										StopUpload(true, true);
										success = false;
									}
									else if (m_currentYTUpload->VideoInfo.Privacy == rage::rlYoutubeVideoInfo::UNLISTED)
									{
										CVideoEditorMenu::SetUserConfirmationScreen( "YT_UNLISTED_NOT_GOOD_STANDING" );
										StopUpload(true, true);
										success = false;
									}
								}
							}

							// if successful, then go to the next stage
							if (success)
							{
								StopNetRetryTimer();

								if (m_acceptPolicies)
								{
									m_currentState = UploadState::AddTags;
									m_screenToShow = UploadScreen::TitleText;
								}
								else
								{
									m_currentState = UploadState::AcceptPolicies;
									m_screenToShow = UploadScreen::SharingPolicies;
								}
							}
						}

					}
					else
					{
						if (m_currentYTUpload->m_ErrCode == 403)
						{
							// deal with hitting per second quota by trying again with exponential retry (netRetryTimer)
							if (strcmp(m_currentYTUpload->m_ErrReason, "quotaExceeded") == 0 || strcmp(m_currentYTUpload->m_ErrReason, "userRateLimitExceeded") == 0)
							{
								videoDisplayf("CVideoUploadManager_Custom::Update - UploadState::ChannelCheck - 403 quotaExceeded or userRateLimitExceeded");
								m_retryCounter--;
								if (m_retryCounter == 0)
								{
									// if we run out of retries, we have to notify why
									videoDisplayf("CVideoUploadManager_Custom::Update - UploadState::ChannelCheck - 403 quotaExceeded or userRateLimitExceeded ... out of retries");
									FailUpload(UploadState::UploadYTSetupFail, -1, TheText.Get("YT_VIDEO_CHANNEL_QUOTA_SECOND_FAIL"));
								}
								else
								{
									m_currentState = UploadState::StartChannelCheck;
									StartNetRetryTimer(1, 5);						
								}
							}							
							// if daily limit, bail out straight away
							else if (strcmp(m_currentYTUpload->m_ErrReason, "dailyLimitExceeded") == 0)
							{
								videoDisplayf("CVideoUploadManager_Custom::Update - UploadState::ChannelCheck - 403 dailyLimitExceeded");
								
								// if daily limit, just bail out ... unlikely to be ready soon
								FailUpload(UploadState::UploadYTSetupFail, -1, TheText.Get("YT_VIDEO_CHANNEL_QUOTA_DAILY_FAIL"));
							}
							// if insufficient permission, we have to link accounts more fully to give those permissions
							else if (strcmp(m_currentYTUpload->m_ErrReason, "insufficientPermissions") == 0)
							{
								videoDisplayf("CVideoUploadManager_Custom::Update - UploadState::ChannelCheck - 403 - Insufficient Permission");
								m_currentState = UploadState::LinkingAccounts;
								m_screenToShow = UploadScreen::LinkPermissions;
							}
							else
							{
								videoDisplayf("CVideoUploadManager_Custom::Update - UploadState::ChannelCheck - unsupported 403 error");
								FailUpload(UploadState::UploadYTSetupFail, -1);
							}
						}
						else
						{
							videoDisplayf("CVideoUploadManager_Custom::Update - UploadState::ChannelCheck - channel check failed");
							FailUpload(UploadState::UploadYTSetupFail, -1);
						}

					}
				}
			}
			break;
		case UploadState::LinkingAccounts:
			{
#if GTA_REPLAY
				if (!CVideoEditorMenu::CheckForUserConfirmationScreens())
				{
					m_screenToShow = UploadScreen::LinkToAccountCancel;
				}
#endif
			}
			break;
		case UploadState::AcceptPolicies:
		case UploadState::AddTags:
			{

			}
			break;
		case UploadState::StartSending:
			{
				if (m_oAuthSuccess == true)
				{
					if (!SendHeader())
					{
						videoDisplayf("CVideoUploadManager_Custom::Update - UploadState::UploadingHeader - send header failed");
						FailUpload(UploadState::UploadYTSetupFail, -1);
					}
				}
				else
				{
					videoDisplayf("CVideoUploadManager_Custom::Update - not authorised to send header info");
					FailUpload(UploadState::UploadYTSetupFail, -1);
				}
			}
			break;
	
		case UploadState::UploadingHeader:
			{
				if (!m_MyStatus.Pending())
				{
					bool stopNetRetryTimer = true;

					if (m_MyStatus.Succeeded())
					{
						videoDisplayf("CVideoUploadManager_Custom::Update - UploadState::UploadingHeader succeeded");
						m_currentState = UploadState::SaveHeaderData;

						// now it's up to youtube, let's nuke all the extra desc that SC doesn't need, as it's already up
						formatf(m_currentYTUpload->VideoInfo.Description, m_currentYTUpload->VideoInfo.MAX_DESCRIPTION_LEN, "%s", m_customDesc.c_str());
					}
					else
					{
						if (m_currentYTUpload->m_ErrCode == 403)
						{
							// deal with hitting per second quota by trying again with exponential retry (netRetryTimer)
							if (strcmp(m_currentYTUpload->m_ErrReason, "quotaExceeded") == 0 || strcmp(m_currentYTUpload->m_ErrReason, "userRateLimitExceeded") == 0)
							{
								videoDisplayf("CVideoUploadManager_Custom::Update - UploadState::UploadingHeader - 403 quotaExceeded or userRateLimitExceeded");
								m_retryCounter--;
								if (m_retryCounter == 0)
								{
									// if we run out of retries, we have to notify why
									videoDisplayf("CVideoUploadManager_Custom::Update - UploadState::ChannelCheck - 403 quotaExceeded or userRateLimitExceeded ... out of retries");
									FailUpload(UploadState::UploadYTSetupFail, -1, TheText.Get("YT_VIDEO_CHANNEL_QUOTA_SECOND_FAIL"));
								}
								else
								{
									m_currentState = UploadState::StartSending;
									StartNetRetryTimer(1, 5);
									stopNetRetryTimer = false;

								}
							}
							else if (strcmp(m_currentYTUpload->m_ErrReason, "dailyLimitExceeded") == 0)
							{
								videoDisplayf("CVideoUploadManager_Custom::Update - UploadState::UploadingHeader - 403 dailyLimitExceeded");
								
								// if daily limit, just bail out as in setup
								FailUpload(UploadState::UploadYTSetupFail, -1, TheText.Get("YT_VIDEO_CHANNEL_QUOTA_DAILY_FAIL"));
							}
							else
							{
								videoDisplayf("CVideoUpytNoQuotaVideoloadManager_Custom::Update - UploadState::UploadingHeader - unsupported 403 error");
								FailUpload(UploadState::UploadYTSetupFail, -1);
							}
						}
						else
						{
							int result = m_MyStatus.GetResultCode();
							videoDisplayf("CVideoUploadManager_Custom::Update - UploadState::UploadingHeader - state failed");
							FailUpload(UploadState::UploadYTSetupFail, result);
						}
					}

					if (stopNetRetryTimer)
					{
						StopNetRetryTimer();
					}
				}
			}
			break;
		case UploadState::SaveHeaderData:
			{
				m_saveHeaderWorker = rage_new SaveHeaderDataWorker;
				if (m_saveHeaderWorker)
				{
					if (m_saveHeaderWorker->Configure(m_currentYTUpload, m_currentUploadSavePathAndFilename.c_str(), m_sourcePathAndFileName.c_str(), m_videoDurationMs, m_sourceCreatedDate.c_str(), m_displayName.c_str(), m_customDesc.c_str(), m_customTag.c_str(), &m_MyStatus))
					{
						if (m_saveHeaderWorker->Start(0))
						{
							m_MyStatus.SetPending();
							m_saveHeaderWorker->Wakeup();

							m_currentState = UploadState::SavingHeaderData;
						}
					}
				}

				// if it failed, then the state won't have changed
				if (m_currentState != UploadState::SavingHeaderData)
				{
					videoDisplayf("CVideoUploadManager_Custom::Update - SaveHeaderData fail");
					FailUpload(UploadState::UploadYTSetupFail, -1);
				}
			}
			break;
		case UploadState::SavingHeaderData:
			{
				if (!m_MyStatus.Pending())
				{
					if (m_saveHeaderWorker)
					{
						delete m_saveHeaderWorker;
						m_saveHeaderWorker = NULL;
					}
					if (m_MyStatus.Succeeded())
					{
						m_screenToShow = UploadScreen::NowPublishing;
						videoDisplayf("CVideoUploadManager_Custom::Update - UploadState::SavingHeaderData succeeded");
						m_headerSuccess = true;
						m_replaceGameStreamId = CGameStreamMgr::GetGameStream()->PostMessageText( "", "CHAR_YOUTUBE", "CHAR_YOUTUBE", false, 0, TheText.Get("YT_YOUTUBE"), TheText.Get("YT_UPLOAD_STARTED"), 1.0f, "", 0, m_replaceGameStreamId );
						if (!RequestReadVideoBufferBlock())
						{
							videoDisplayf("CVideoUploadManager_Custom::Update - UploadState::UploadinSavingHeaderDatagHeader - read first block failed");
							FailUpload(UploadState::UploadYTSetupFail, -1);
						}
					}
					else
					{
						int result = m_MyStatus.GetResultCode();
						videoDisplayf("CVideoUploadManager_Custom::Update - UploadState::SavingHeaderData - state failed");
						FailUpload(UploadState::UploadYTSetupFail, result);
					}
				}
			}
			break;
		case UploadState::StartReadingVideoBlock:
			{
				if (m_readBlockWorker && m_readBlockWorker->Start(0))
				{
					m_MyStatus.SetPending();
					m_readBlockWorker->Wakeup();

					m_currentState = UploadState::ReadingVideoBlock;
				}
				else
				{
					videoDisplayf("CVideoUploadManager_Custom::Update - StartReadingVideoBlock failed");
					FailUpload(UploadState::UploadYTFail, -1);
				}
			}
			break;
		case UploadState::ReadingVideoBlock:
			{
				if (!m_MyStatus.Pending())
				{
					if (m_MyStatus.Succeeded())
					{
						// if block size same as video size, we just upload the whole thing
						if (m_currentUploadBlockSize == m_videoSize)
						{
							if (!rage::rlYoutube::Upload(m_currentYTUpload, m_pBuffer, &m_MyStatus))
							{
								videoDisplayf("CVideoUploadManager_Custom::Update - rage::rlYoutube::Upload failed");
								FailUpload(UploadState::UploadYTFail, -1);
							}
						}
						else
						{
							if (!rage::rlYoutube::UploadChunk(m_currentYTUpload, m_pBuffer, 0, m_currentUploadBlockSize, m_resumeSetup, &m_MyStatus))
							{
								videoDisplayf("CVideoUploadManager_Custom::Update - rage::rlYoutube::UploadChunk failed");
								FailUpload(UploadState::UploadYTFail, -1);
							}
						}

						m_currentState = UploadState::UploadingBlock;
					}
					else
					{
						videoDisplayf("CVideoUploadManager_Custom::Update - Read Block failed");
						FailUpload(UploadState::UploadYTFail, -1);
					}

					if (m_readBlockWorker)
					{
						delete m_readBlockWorker;
						m_readBlockWorker = NULL;
					}
				}
			}
			break;
		case UploadState::UploadingBlock:
			{
				if (!m_MyStatus.Pending())
				{
					bool stopNetRetryTimer = true;

					if (m_MyStatus.Succeeded())
					{
						m_beforePauseState = UploadState::Unused;

						videoDisplayf("CVideoUploadManager_Custom::Update - UploadState::UploadingBlock succeeded");
						int result = m_MyStatus.GetResultCode();
						switch(result)
						{
						case rlYoutubeUploadChunkTask::UPLOAD_SUCCESS_COMPLETE:
							{
								// The upload was successful and the upload is now complete
								if (!StartPublishUGC())
								{
									videoDisplayf("CVideoUploadManager_Custom::Update - PublishUGC() failed");
									FailUpload(UploadState::UploadUGCFail);
								}
								m_retryCounter = MaxNoofRetrys;
							}
							break;
						case rlYoutubeUploadChunkTask::UPLOAD_SUCCESS_CONTINUE:
							{
								// The upload was successful and we can upload the next chunk
								UploadNextBlock();

								videoDisplayf("CVideoUploadManager_Custom::Update - UploadNextBlock - %.2f%%", GetUploadFileProgress());
								// post a feed message when we go over halfway
								if (GetUploadFileProgress() >= m_uploadProgressForNextInfo)
								{
									if (m_currentYTUpload && m_currentYTUpload->UploadSize)
									{
										const char maxStringLength = 32;
										char uploadedString[maxStringLength] = {0};
										if (m_uploadProgressForNextInfo == 0.25f)
											formatf(uploadedString, maxStringLength, "%s 25%%", TheText.Get("YT_UPLOAD_UPLOADED"));
										else if (m_uploadProgressForNextInfo == 0.5f)
											formatf(uploadedString, maxStringLength, "%s 50%%", TheText.Get("YT_UPLOAD_UPLOADED"));
										else if (m_uploadProgressForNextInfo == 0.75f)
											formatf(uploadedString, maxStringLength, "%s 75%%", TheText.Get("YT_UPLOAD_UPLOADED"));
										m_replaceGameStreamId = CGameStreamMgr::GetGameStream()->PostMessageText( "", "CHAR_YOUTUBE", "CHAR_YOUTUBE", false, 0, TheText.Get("YT_YOUTUBE"), uploadedString, 1.0f, "", 0, m_replaceGameStreamId );
										m_uploadProgressForNextInfo += 0.25f;
									}
								}

								m_retryCounter = MaxNoofRetrys;
							}
							break;
						case rlYoutubeUploadChunkTask::UPLOAD_FAILED_RETRY:
							{
								// The upload has failed, but we can retry.
								--m_retryCounter;
								if (m_retryCounter > 0)
								{
									GetUploadStatus();
									StopNetRetryTimer();
								}
								else
								{
									m_errorCode = m_MyStatus.GetResultCode();
									videoDisplayf("CVideoUploadManager_Custom::Update - Block UPLOAD_FAILED_RETRY failed too many times.");
									FailUpload(UploadState::UploadYTFail, result);
								}
							}
							break;
						case rlYoutubeUploadChunkTask::UPLOAD_FAILED_PERMANENTLY:
						default:
							{
								m_errorCode = m_MyStatus.GetResultCode();
								videoDisplayf("CVideoUploadManager_Custom::Update - Block UPLOAD_FAILED_PERMANENTLY");
								FailUpload(UploadState::UploadYTFail, result);
							}
							break;
						}

						// even if it's "failed" it still succeeded in connecting, so reset this disconnect counter
						m_disconnectRetryCounter = MaxNoofDisconnectRetry;
					}
					else
					{
						// not sure upload can actually hit quota errors, we think it's in the initial resumable URL call ...but no harm supporting it
						if (m_currentYTUpload->m_ErrCode == 403)
						{
							// deal with hitting per second quota by trying again with exponential retry (netRetryTimer)
							if (strcmp(m_currentYTUpload->m_ErrReason, "quotaExceeded") == 0 || strcmp(m_currentYTUpload->m_ErrReason, "userRateLimitExceeded") == 0)
							{
								videoDisplayf("CVideoUploadManager_Custom::Update - UploadState::UploadingBlock - 403 quotaExceeded or userRateLimitExceeded");
								UploadNextBlock();
								StartNetRetryTimer(10, 10 * 60);
								m_retryCounter = MaxNoofRetrys;
								stopNetRetryTimer = false;
							}
							// longer delay for daily limit - 30 - 45 mins ... can't be an hour, or else we have to reauth
							else if (strcmp(m_currentYTUpload->m_ErrReason, "dailyLimitExceeded") == 0)
							{
								videoDisplayf("CVideoUploadManager_Custom::Update - UploadState::UploadingBlock - 403 dailyLimitExceeded");
								GetUploadStatus();
								StartNetRetryTimer(30 * 60, 45 * 60);
								m_retryCounter = MaxNoofRetrys;
								stopNetRetryTimer = false;

								// keep updating feed each time this fails as a reminder why it's not uploading
								CGameStreamMgr::GetGameStream()->PostTicker(TheText.Get("YT_VIDEO_QUOTA_FAIL"), true);
							}
							else
							{
								videoDisplayf("CVideoUpytNoQuotaVideoloadManager_Custom::Update - UploadState::UploadingBlock - unsupported 403 error");
								FailUpload(UploadState::UploadYTFail, -1);
							}

							// even if it's "failed" it still succeeded in connecting and doing a retry, so reset this disconnect counter
							m_disconnectRetryCounter = MaxNoofDisconnectRetry;
						}
						else
						{
							// do some retries, to give code higher up dealing with disconnections time, to make sure it's a legit fail first 
							// should keep slipping into here on each retry
							if (m_disconnectRetryCounter > 0)
							{
								--m_disconnectRetryCounter;

								videoDisplayf("CVideoUploadManager_Custom::Update - UploadState::UploadingBlock - retry on fail ...probably connection");
								UploadNextBlock();
								StartNetRetryTimer(60, 120);
								stopNetRetryTimer = false;
							}
							else
							{
								videoDisplayf("CVideoUploadManager_Custom::Update - Block send failed ...probably connection ?");
								FailUpload(UploadState::UploadYTFail, -1);
							}
						}
					}

					if (stopNetRetryTimer)
					{
						StopNetRetryTimer();
					}
				}
			}
			break;
		case UploadState::StartGetUploadedVideoInfo:
			{
				StartPublishUGC();
			}
			break;
		case UploadState::GetUploadedVideoInfo:
			{
				if(!m_MyStatus.Pending())
				{
					if(m_MyStatus.Succeeded())
					{
						if (m_currentYTUpload->VideoInfo.VideoStatus.UploadStatus == "uploaded")
						{
							// if it's just uploaded, we need to check again for it being "processed" which is the real final state
							// use exponential timer, as checking this might be expensive on quota		
							m_currentState = UploadState::StartGetUploadedVideoInfo;

							// usually used for hitting quota limits, but no harm using here with longer times
							StartNetRetryTimer(60, 10*60);
						}
						else if (m_currentYTUpload->VideoInfo.VideoStatus.UploadStatus == "deleted")
						{
							videoDisplayf("CVideoUploadManager_Custom::Update - GetUploadedVideoInfo - YT video has been deleted");
							FailUpload(UploadState::UploadYTFail, -1, "failed - deleted");	
						}
						else if (m_currentYTUpload->VideoInfo.VideoStatus.UploadStatus == "failed")
						{
							videoDisplayf("CVideoUploadManager_Custom::Update - GetUploadedVideoInfo - YT video upload failed");

							char errorString[128];
							formatf(errorString, "failed - %s", m_currentYTUpload->VideoInfo.VideoStatus.FailureReason.c_str());
							FailUpload(UploadState::UploadYTFail, -1, errorString);
						}
						else if (m_currentYTUpload->VideoInfo.VideoStatus.UploadStatus == "rejected")
						{
							// special message for duplicate videos ...informing them to delete first so it can be uploaded and linked with SC
							if (m_currentYTUpload->VideoInfo.VideoStatus.RejectionReason == "duplicate")
							{
								videoDisplayf("CVideoUploadManager_Custom::Update - GetUploadedVideoInfo - YT video upload rejected - duplicate");
								FailUpload(UploadState::UploadYTFail, -1, TheText.Get("YT_VIDEO_DUPLICATE"));
							}
							else
							{
								videoDisplayf("CVideoUploadManager_Custom::Update - GetUploadedVideoInfo - YT video upload rejected");
								
								char errorString[128];
								formatf(errorString, "failed - %s", m_currentYTUpload->VideoInfo.VideoStatus.RejectionReason.c_str());
								FailUpload(UploadState::UploadYTFail, -1, errorString);
							}
						}
						else
						{
							// if the video is fine, we can publish
							PublishUGC();
							StopNetRetryTimer();
						}

						// even if it's "failed" it still succeeded in connecting, so reset this disconnect counter
						m_disconnectRetryCounter = MaxNoofDisconnectRetry;
					}
					else
					{
						if (m_MyStatus.GetResultCode() == rlYoutubeGetVideoInfoTask::GetVideoInfoResult::NO_ITEMS_FOUND)
						{
							videoDisplayf("CVideoUploadManager_Custom::Update - GetUploadedVideoInfo - YT video upload fail - NO_ITEMS_FOUND");
							FailUpload(UploadState::UploadYTFail, -1, TheText.Get("YT_VIDEO_CANT_BE_FOUND"));
						}
						else if (m_enableResetAuthOn401 && m_currentYTUpload->m_ErrCode == 401)
						{
							// 401 - these errors occur when auth token has expired. 
							// Restart the upload from scratch to get a new auth token, which will pretty much jump back to this stage
							if (strcmp(m_currentYTUpload->m_ErrReason, "authError") == 0 || strcmp(m_currentYTUpload->m_ErrReason, "expired") == 0)
							{
								RequestResumeUpload();
								m_oAuthSuccess = false;
							}
							else
							{
								videoDisplayf("CVideoUploadManager_Custom::Update - UploadState::GetUploadedVideoInfo - unsupported 401 error");
								FailUpload(UploadState::UploadYTSetupFail, -1);
							}
						}
						else if (m_currentYTUpload->m_ErrCode == 403)
						{
							// deal with hitting per second quota by trying again with exponential retry (netRetryTimer)
							if (strcmp(m_currentYTUpload->m_ErrReason, "quotaExceeded") == 0 || strcmp(m_currentYTUpload->m_ErrReason, "userRateLimitExceeded") == 0)
							{
								videoDisplayf("CVideoUploadManager_Custom::Update - UploadState::ChannelCheck - 403 quotaExceeded or userRateLimitExceeded");
								// treat like "uploaded" state and try again later
								m_currentState = UploadState::StartGetUploadedVideoInfo;
								StartNetRetryTimer(60, 10*60);
							}
							// longer delay for daily limit - 30 - 45 mins ... can't be an hour, or else we have to reauth
							else if (strcmp(m_currentYTUpload->m_ErrReason, "dailyLimitExceeded") == 0)
							{
								videoDisplayf("CVideoUploadManager_Custom::Update - UploadState::ChannelCheck - 403 dailyLimitExceeded");
								GetUploadStatus();
								StartNetRetryTimer(30 * 60, 10 * 60);
							}
							else
							{
								videoDisplayf("CVideoUploadManager_Custom::Update - UploadState::GetUploadedVideoInfo - unsupported 403 error");
								FailUpload(UploadState::UploadYTSetupFail, -1);
							}

							// even if it's "failed" it still succeeded in connecting and doing a retry, so reset this disconnect counter
							m_disconnectRetryCounter = MaxNoofDisconnectRetry;
						}
						else
						{
							// do some retries, to give code higher up dealing with disconnections time, to make sure it's a legit fail first 
							// should keep slipping into here on each retry
							if (m_disconnectRetryCounter > 0)
							{
								--m_disconnectRetryCounter;

								videoDisplayf("CVideoUploadManager_Custom::Update - UploadState::UploadingBlock - retry on fail ...probably connection");
								// treat like "uploaded" state and try again later
								m_currentState = UploadState::StartGetUploadedVideoInfo;
								StartNetRetryTimer(60, 10*60);
							}
							else
							{
								videoDisplayf("CVideoUploadManager_Custom::Update - PrePublishUGC - Get Uploaded Video Info failed");
								FailUpload(UploadState::UploadYTFail, -1);
							}
						}


					}
				}
			}
			break;
		case UploadState::UploadingPublish:
			{
				if(!m_MyStatus.Pending())
				{
					if(m_MyStatus.Succeeded())
					{
						videoDisplayf("CVideoUploadManager_Custom::Update - UploadState::UploadingBlock succeeded");
						m_currentState = UploadState::UploadComplete;
					}
					else
					{
						--m_retryCounter;
						if (m_retryCounter > 0)
						{
							StartPublishUGC();
						}
						else
						{
							m_errorCode = m_MyStatus.GetResultCode();
							videoDisplayf("CVideoUploadManager_Custom::Update - Publish To UGC failed too many times.");
							FailUpload(UploadState::UploadUGCFail, -1, TheText.Get("YT_VIDEO_SC_FAIL"));
						}
					}
				}
			}
			break;
		case UploadState::GettingStatus:
			if (!m_MyStatus.Pending())
			{
				if (m_MyStatus.Succeeded())
				{
					int result = m_MyStatus.GetResultCode();
					// The status request determined we can continue
					if (result == rlYoutubeGetUploadStatusTask::UPLOAD_STATUS_CONTINUE)
					{
						StartResumeUpload();
					}
					// The status request determined that the upload has completed
 					else if (result == rlYoutubeGetUploadStatusTask::UPLOAD_STATUS_COMPLETE)
 					{
						// if set to resume, we can assume it's not been published to SC yet
						if (m_resumeSetup)
						{
							StartResumeUpload();
						}
						else
						{
							Reset();
							++m_alreadyUploadedCounter;
						}
 					}
					else
					{
						videoDisplayf("CVideoUploadManager_Custom::Update - UploadState::GettingStatus - RESULT CODE - UPLOAD_STATUS_FAILED");
						FailUpload(UploadState::UploadYTFail);
					}
				}
				else
				{
					videoDisplayf("CVideoUploadManager_Custom::Update - UploadState::GettingStatus - Status Failed");
					FailUpload(UploadState::UploadYTFail);
				}
			}
			break;
		case UploadState::RequestResume:
			{
				if (!m_MyStatus.Pending())
				{
					if (!m_oAuthSuccess)
					{
						RefreshOAuthToken();
					}
					else
					{
						GetUploadStatus();
					}
				}
			}
			break;
		case UploadState::ResumeChecks:
			{
				if (!m_MyStatus.Pending())
				{
					if (m_MyStatus.Succeeded())
					{
						// add tags back now, because some of the generated ones don't pass the banned word test
						CreateTags();
						if (m_customTag.GetLength() > 0)
							m_currentYTUpload->VideoInfo.AddTag(m_customTag.c_str());
						ResumeUpload();
					}
					else
					{
						videoDisplayf("CVideoUploadManager_Custom::Update - UploadState::ResumeChecks - Status Failed");
						FailUpload(UploadState::UploadYTFail, -1);
					}
				}
			}
			break;
		default : break;
	}

	// check for broad changes in state, so we can refresh UI when a big change happens
	if (m_currentState >= UploadState::SocialClubSignup && m_currentState < UploadState::StartReadingVideoBlock)
	{
		m_currentVideoStage = UploadVideoStage::Setup;
	}
	else if (m_currentState >= UploadState::StartReadingVideoBlock && m_currentState < UploadState::StartGetUploadedVideoInfo)
	{
		m_currentVideoStage = UploadVideoStage::Uploading;
	}
	else if (m_currentState >= UploadState::StartGetUploadedVideoInfo && m_currentState < UploadState::UploadingPublish)
	{
		m_currentVideoStage = UploadVideoStage::Processing;
	}
	else
	{
		m_currentVideoStage = UploadVideoStage::Other;
	}

	if (prevStage != m_currentVideoStage)
	{
		if (IsOnGalleryMenu())
		{
			CVideoEditorMenu::ForceUploadUpdate();
		}
	}


	if (m_currentState == UploadState::UploadComplete)
	{
		// complete

		UploadedInfo info;
		info.m_filenameHash = FilenameHashFromFullPath(m_sourcePathAndFileName.c_str());
		info.m_size = m_videoSize;
		info.m_durationMs = m_videoDurationMs;
		m_uploadedList.PushAndGrow(info);

		// console version of this doesn't use upload as a step, just a video creation
#if RSG_PC
		// Unlock ACH H11 - Vinewood Visionaryer
		CLiveManager::GetAchMgr().AwardAchievement(player_schema::Achievements::ACHH11);
#endif

#if RSG_ORBIS
		// B*2365291 - don't post to activity feed if video is private
		if (m_currentYTUpload->VideoInfo.Privacy != rage::rlYoutubeVideoInfo::PRIVATE)
			PostToActivityFeed(m_currentYTUpload->VideoInfo.VideoId);
#endif

		Reset();
		++m_uploadedCounter;
		m_replaceGameStreamId = CGameStreamMgr::GetGameStream()->PostMessageText( "", "CHAR_YOUTUBE", "CHAR_YOUTUBE", false, 0, TheText.Get("YT_YOUTUBE"), TheText.Get("YT_UPLOAD_COMPLETE"), 1.0f, "", 0, m_replaceGameStreamId );
#if GTA_REPLAY
		if (IsOnGalleryMenu())
		{
			CVideoEditorMenu::SetUserConfirmationScreen( "VEUI_UPLOAD_SUCCESS", "VE_SUCCESS" );
			CVideoEditorMenu::ForceUploadUpdate();
		}
#endif
	}
}

bool CVideoUploadManager_Custom::UpdateWarnings()
{
#if GTA_REPLAY

	if (m_screenShowing == UploadScreen::None)
	{
		if (m_screenToShow != UploadScreen::None)
		{
			if (IsOnGalleryMenu())
			{
				switch(m_screenToShow)
				{
				case UploadScreen::LinkToAccount:
					{
						CVideoEditorMenu::SetUserConfirmationScreenWithAction( "TRIGGER_UPLOAD_SUCCESS", "VEUI_UPLOAD_LINK_ACCOUNTS" );
					}
					break;
				case UploadScreen::LinkPermissions:
					{
						CVideoEditorMenu::SetUserConfirmationScreenWithAction( "TRIGGER_UPLOAD_SUCCESS", "VEUI_UPLOAD_LINK_PERMISSIONS" );
					}
					break;
				case UploadScreen::LinkToAccountSuccess:
					{
						CBusySpinner::Off(SPINNER_SOURCE_VIDEO_UPLOAD);
						CVideoEditorMenu::SetUserConfirmationScreen( "VEUI_UPLOAD_LINK_SUCCESS", NULL, FE_WARNING_OK, true );
					}
					break;
				case UploadScreen::LinkToAccountFail:
					{
						CBusySpinner::Off(SPINNER_SOURCE_VIDEO_UPLOAD);
						CVideoEditorMenu::SetUserConfirmationScreenWithAction( "TRIGGER_UPLOAD_SUCCESS", "VEUI_UPLOAD_LINK_FAIL", NULL, FE_WARNING_YES_NO, true );
					}
					break;
				case UploadScreen::LinkToAccountCancel:
					{
						CBusySpinner::On(TheText.Get("VEUI_UPLOAD_LINKING"), BUSYSPINNER_LOADING, SPINNER_SOURCE_VIDEO_UPLOAD);	
						CVideoEditorMenu::SetUserConfirmationScreen( "VEUI_UPLOAD_LINK_CANCEL", NULL, FE_WARNING_CANCEL, true );
					}
					break;
				case UploadScreen::SharingPolicies:
					{
						PolicyMenu::Open();
					}
					break;
				case UploadScreen::TitleText:
					{
						if (CVideoEditorMenu::CheckForUserConfirmationScreens())
							return false;
						CVideoEditorUi::SetupVideoUploadTextEntry("VEUI_CONFIRM_TITLE", 41, m_displayName.c_str());
						m_noTextEntered = false;
					}
					break;
				case UploadScreen::DescText:
					{
						if (CVideoEditorMenu::CheckForUserConfirmationScreens())
							return false;
						CVideoEditorUi::SetupVideoUploadTextEntry("VEUI_ADD_DESC", 256);
						m_noTextEntered = false;
					}
					break;
				case UploadScreen::TagText:
					{
						if (CVideoEditorMenu::CheckForUserConfirmationScreens())
							return false;
						CVideoEditorUi::SetupVideoUploadTextEntry("VEUI_ADD_TAG", 41);
						m_noTextEntered = false;
					}
					break;
				case UploadScreen::CompTagText:
					{
						CVideoEditorMenu::SetUserConfirmationScreenWithAction( "TRIGGER_UPLOAD_SUCCESS", "VEUI_UPLOAD_COMPETITION_TAG", NULL, FE_WARNING_YES_NO, false, m_competitionName.c_str(), m_competitionTag.c_str() );
						m_noTextEntered = false;
					}
					break;
				case UploadScreen::NowPublishing:
					{
						CVideoEditorMenu::SetUserConfirmationScreen( "VEUI_UPLOAD_VID_B" );
						CVideoEditorMenu::RebuildMenu();
					}
					break;
				case UploadScreen::AlreadyPublishing:
					{
						CVideoEditorMenu::SetUserConfirmationScreen( "VEUI_UPLOAD_ALREADY" );
					}
					break;
				case UploadScreen::Cancelled:
					{
						CVideoEditorMenu::SetUserConfirmationScreen( "VEUI_UPLOAD_CANCELLED" );
						CVideoEditorMenu::RebuildMenu();
					}
					break;
				case UploadScreen::Failed:
					{
						if (m_errorCode)
						{
							CVideoEditorMenu::SetUserConfirmationScreen( "VEUI_UPLOAD_FAILED_ERROR" , NULL, FE_WARNING_OK, false, m_errorDetails.c_str() );
						}
						else
						{
							CVideoEditorMenu::SetUserConfirmationScreen( "VEUI_UPLOAD_FAILED" );
						}
						CVideoEditorMenu::RebuildMenu();
					}
					break;
				case UploadScreen::FailedChannel:
					{
						CVideoEditorMenu::SetUserConfirmationScreen( "VEUI_UPLOAD_FAILED_CHANNEL" );
						CVideoEditorMenu::RebuildMenu();
					}
					break;
				default : break;
				}
			}

			m_screenShowing = m_screenToShow;
			m_screenToShow = UploadScreen::None;
		}

		return true;
	}

	// if gone with no result, assume cancel, back, etc
	if (!CVideoEditorMenu::CheckForUserConfirmationScreens())
	{
		switch(m_screenShowing)
		{
		case UploadScreen::LinkToAccount:
		case UploadScreen::LinkPermissions:
		case UploadScreen::LinkToAccountSuccess:
		case UploadScreen::LinkToAccountFail:
		case UploadScreen::LinkToAccountCancel:
		case UploadScreen::CompTagText:
		case UploadScreen::NowPublishing:
		case UploadScreen::Cancelled:
		case UploadScreen::Failed:
		case UploadScreen::FailedChannel:
			{
				CVideoUploadManager_Custom::ReturnConfirmationResult(false);
			}
			break;
		default: break;
		}
	}

	switch(m_screenShowing)
	{
	case UploadScreen::TitleText:
	case UploadScreen::DescText:
	case UploadScreen::TagText:
		{
			if (m_noTextEntered)
			{
				// if no string is entered, we don't need to check it on UGC, so go straight in for it to fail
				ReturnStringResult(NULL);
				CBusySpinner::Off(SPINNER_SOURCE_VIDEO_UPLOAD);
			}
			else if (m_textBoxValid)
			{
				if (!m_MyStatus.Pending())
				{
					CBusySpinner::Off(SPINNER_SOURCE_VIDEO_UPLOAD);
					if (m_MyStatus.Succeeded())
					{
						SubmitStringToUploadDetails(m_textBoxString);
					}
					else
					{
						if (m_MyStatus.GetResultCode() == RLUGC_ERROR_NOTALLOWED_PROFANE)
						{
							CVideoEditorMenu::SetUserConfirmationScreen( "VEUI_UPLOAD_BANNED_WORD" );
							m_screenToShow = m_screenShowing;
							m_screenShowing = UploadScreen::None;
						}
						else
						{
							CVideoEditorMenu::SetUserConfirmationScreen( "VEUI_UPLOAD_SERVICES_LOST" );
							StopUpload(true, true, true);
						}
					}

					m_textBoxValid = false;
				}
			}
		}
		break;
	case UploadScreen::SharingPolicies:
		{
			if (PolicyMenu::HasAccepted())
			{
				ReturnConfirmationResult(true);
			}
			else if (PolicyMenu::HasCancelled())
			{
				ReturnConfirmationResult(false);
			}
		}
		break;
	default : break;
	}

	if (m_screenShowing == UploadScreen::None && m_screenToShow == UploadScreen::None)
		return true;

#endif

	return false;
}


void CVideoUploadManager_Custom::ReturnConfirmationResult(bool result)
{
	bool canStopUpload = true;
	switch(m_screenShowing)
	{
	case UploadScreen::LinkToAccount:
	case UploadScreen::LinkPermissions:
		{
			if (result)
			{
				rlYoutube::ShowAccountLinkUi(m_localGamerIndex, "gtav", "replay", &m_MyStatus);
			}
		}
		break;
	case UploadScreen::LinkToAccountSuccess:
		{
			// always going to be false, as it's just a notification
			if (GetOAuthTokenWithExistingUploadInfo())
			{
				canStopUpload =  false;
			}
			else
			{
				result = false; // to cancel upload
			}
		}
		break;
	case UploadScreen::LinkToAccountFail:
		{
			// if true, then we want to try again
			if (result)
			{
				if (!GetOAuthTokenWithExistingUploadInfo())
					result = false; // to cancel upload
			}
		}
		break;
	case UploadScreen::LinkToAccountCancel:
		{
			CBusySpinner::Off(SPINNER_SOURCE_VIDEO_UPLOAD);
			// if we're not in linking state, then linking is complete and we want to ignore this cancel
			if (m_currentState == UploadState::LinkingAccounts)
			{
				result = false;
			}
			else
			{
				return;
			}
		}
		break;
	case UploadScreen::SharingPolicies: // replace with proper screen
		{
			if (result)
			{
#if !ALWAYS_SHOW_POLICIES
				m_acceptPolicies = true;
#endif
				m_currentState = UploadState::AddTags;
				m_screenToShow = UploadScreen::TitleText;
			}
			PolicyMenu::FlagForDeletion();
		}
		break;
	case UploadScreen::CompTagText:
		{
			if (result)
			{
				m_customTag = m_competitionTag;
				AddYTTag(m_competitionTag.c_str());
				m_currentState = UploadState::StartSending;
			}
			else
			{
				m_screenToShow = UploadScreen::TagText;
				canStopUpload = false;
			}
		}
		break;
	case UploadScreen::NowPublishing:
	case UploadScreen::Cancelled:
	case UploadScreen::Failed:
	case UploadScreen::FailedChannel:
		{
			canStopUpload = false;
			if(m_currentState == UploadState::CancelledInSetup)
			{
				m_currentState = UploadState::Unused;
			}
		}
		break;
	default : break;
	}

	if (canStopUpload)
	{
		if (!result)
		{
			StopUpload(true);
			return;
		}
	}

	if (IsOnGalleryMenu())
	{
		CVideoEditorMenu::ForceUploadUpdate();
	}

	m_screenShowing = UploadScreen::None;
}

void CVideoUploadManager_Custom::ReturnStringResult(const char* string)
{
	// make sure we're not just sending up a blank string ... filter doesn't like it
	// also doesn't like commands like \n
	char editedString[VideoUploadTextInfo::MAX_VIDEOUPLOAD_TEXT_BYTES];
	if (string)
	{
		safecpy(editedString, string, VideoUploadTextInfo::MAX_VIDEOUPLOAD_TEXT_BYTES);
		Utf8RemoveInvalidSurrogates(editedString);
		// make sure we're not just sending up a blank string ... filter doesn't like it
		bool hasContent = false;
		u32 stringLength = static_cast<u32>(strlen(editedString));
		for (u32 i = 0; i < stringLength; ++i)
		{
			if (editedString[i] != ' ')
			{
				hasContent = true;
				break;
			}
		}
		if (!hasContent)
			string = NULL;
		else
			string = editedString;
	}

	if (m_screenShowing == UploadScreen::TitleText)
	{
		if (!string)
		{
			if (!m_noTextEntered)
			{
	#if GTA_REPLAY
				CVideoEditorUi::SetErrorState("FAILED_TO_INPUT_TEXT");
	#endif
				// set flag that nothing was entered, and we can go and reset this when error warning is done
				m_noTextEntered = true;

				m_screenShowing = UploadScreen::None;
				m_screenToShow = UploadScreen::TitleText;
			}
			return;
		}
	}

	if (string)
	{
		safecpy(m_textBoxString, string, TextBoxStringLength);
		m_textBoxValid = CheckTextOnUGC(m_screenShowing, m_textBoxString);
		videoAssertf(m_textBoxValid, "CVideoUploadManager_Custom::UpdateWarnings - Calling CheckTextOnUGC - call immediately fails. Page: %d String: %s", m_screenShowing, m_textBoxString);

		// can't happen, but to be on the safe side stick code in to fail upload
		if (!m_textBoxValid)
			FailUpload(UploadState::UploadYTSetupFail, -1);
	}
	else
	{
		// if no string, just submit it ...if it requires a string, we'll have already dealt with that
		SubmitStringToUploadDetails(NULL);
	}

}

void CVideoUploadManager_Custom::SubmitStringToUploadDetails(const char* string)
{
	if (!string && m_screenShowing == UploadScreen::TitleText)
	{
		videoAssertf(false, "CVideoUploadManager_Custom::SubmitStringToUploadDetails - Null string passed in");
		return;
	}

	switch(m_screenShowing)
	{
	case UploadScreen::TitleText: // replace with proper screen
		{
			if (AddTitle(string))
			{
				m_screenToShow = UploadScreen::DescText;
			}
			else
			{	
				// if it fails, try again
				m_screenToShow = m_screenShowing;
			}
		}
		break;
	case UploadScreen::DescText: // replace with proper screen
		{
			if (AddDescription(string))
			{
				char competitionName[COMPETITION_STRING_LENGTH];
				char competitionTag[COMPETITION_STRING_LENGTH];
				s32 minDuration = 0;
				s32 maxDuration = 0;
				bool success = false;
				if ( IsCompetitionOn(competitionName, COMPETITION_STRING_LENGTH, competitionTag, COMPETITION_STRING_LENGTH, minDuration, maxDuration) )
				{
					if ((maxDuration == 0 && maxDuration == 0) ||
						(m_videoDurationMs >= minDuration*1000 && m_videoDurationMs <= maxDuration*1000))
					{
						m_competitionName = competitionName;
						m_competitionTag = competitionTag;
						success = true;
					}
				}

				m_screenToShow = success ? UploadScreen::CompTagText : UploadScreen::TagText;
			}
			else
			{	
				// if it fails, try again
				m_screenToShow = m_screenShowing;
			}
		}
		break;
	case UploadScreen::TagText: // replace with proper screen
		{
			if (AddYTTag(string))
			{
				m_currentState = UploadState::StartSending;
				m_retryCounter = MaxNoofRetrys;
			}
			else
			{	
				// if it fails, try again
				m_screenToShow = m_screenShowing;
			}
		}
		break;
	default : break;
	}

	m_screenShowing = UploadScreen::None;
}


bool CVideoUploadManager_Custom::UpdateUGCUploadList()
{
	// early out, pretend everything is cool as there could be no internet connection
	// stops this being called too early before online is setup
	if (!IsUploadEnabled())
		return true;

	const u32 maxQueryCount = 500;
	bool failed = false;

	switch (m_getUploadedListState)
	{
	case GetUploadedListState::NotStarted:
		{
			// just an initial check to get number of files
			if (UGCQueryYoutubeContent(0, 1))
			{
				m_getUploadedListState = GetUploadedListState::GettingNoofVideos;
			}
			else
			{
				videoDisplayf("CVideoUploadManager_Custom::UpdateUGCUploadList ... UGCQueryYoutubeContent failed status in GettingNoofVideos");
				failed = true;
			}
		}
		break;
	case GetUploadedListState::GettingNoofVideos:
		{
			if (!m_MyStatus.Pending())
			{
				if (m_MyStatus.Succeeded())
				{
					if (m_noofUGCVideos > 0)
					{
						m_getUploadedListState = GetUploadedListState::BuildList;
					}
					else
					{
						m_getUploadedListState = GetUploadedListState::Done;
					}
				}
				else
				{
					videoDisplayf("CVideoUploadManager_Custom::UpdateUGCUploadList ... UGCQueryYoutubeContent failed status in GettingNoofVideos");
					m_getUploadedListState = GetUploadedListState::NotStarted;
					failed = true;
				}
			}
		}
		break;
	case GetUploadedListState::BuildList:
		{
			u32 maxNoToQuery = m_noofUGCVideos-m_UGCVideosOffset;
			if (maxNoToQuery > maxQueryCount)
				maxNoToQuery = maxQueryCount;

			if (UGCQueryYoutubeContent(m_UGCVideosOffset, m_noofUGCVideos-m_UGCVideosOffset))
			{
				m_getUploadedListState = GetUploadedListState::WaitForList;
			}
			else
			{
				videoDisplayf("CVideoUploadManager_Custom::UpdateUGCUploadList ... UGCQueryYoutubeContent failed in Build List");
				m_getUploadedListState = GetUploadedListState::NotStarted;
				failed = true;
			}
		}
		break;
	case GetUploadedListState::WaitForList:
		{	
			if (!m_MyStatus.Pending())
			{
				if (m_MyStatus.Succeeded())
				{
					m_UGCVideosOffset += maxQueryCount;
					if (m_UGCVideosOffset > m_noofUGCVideos)
						m_UGCVideosOffset = m_noofUGCVideos;
				}
				
				if (m_UGCVideosOffset < m_noofUGCVideos)
				{
					m_getUploadedListState = GetUploadedListState::BuildList;
				}
				else
				{
					m_getUploadedListState = GetUploadedListState::DoneWaitingOnOptimisation;
					if (IsOnGalleryMenu())
					{
						CVideoEditorMenu::ForceUploadUpdate();
					}
					
				}
			}
		}
		break;
	case GetUploadedListState::DoneWaitingOnOptimisation:
	case GetUploadedListState::Done:
	case GetUploadedListState::Failed:
		{
			return true;
		}
		break;
	default : break;
	}

	if (failed)
	{
		m_retryCounter--;
		if (m_retryCounter == 0)
		{
			videoDisplayf("CVideoUploadManager_Custom::UpdateUGCUploadList ... failed too many times. give up");
			m_getUploadedListState = GetUploadedListState::Failed;
		}
	}

	return false;
}

bool CVideoUploadManager_Custom::IsUploading() const
{
	return static_cast<bool>(m_videoSize != 0);
}

bool CVideoUploadManager_Custom::IsInSetup() const
{
	return !m_resumeSetup && ((m_currentState >= UploadState::SocialClubSignup && m_currentState <= UploadState::SavingHeaderData) || m_currentState == UploadState::CancelledInSetup || m_screenToShow == UploadScreen::NowPublishing || m_screenShowing == UploadScreen::NowPublishing);
}

bool CVideoUploadManager_Custom::IsProcessing() const
{
	if (m_currentState == UploadState::RequestResume)
	{
		return (m_beforePauseState == UploadState::StartGetUploadedVideoInfo || m_beforePauseState == UploadState::GetUploadedVideoInfo || m_beforePauseState == UploadState::UploadingPublish);
	}

	// m_beforePauseState will get reset when uploading happens again, or a new video starts uploading
	return (m_beforePauseState == UploadState::StartGetUploadedVideoInfo || m_beforePauseState == UploadState::GetUploadedVideoInfo || m_beforePauseState == UploadState::UploadingPublish ||
				m_currentState == UploadState::StartGetUploadedVideoInfo || m_currentState == UploadState::GetUploadedVideoInfo || m_currentState == UploadState::UploadingPublish);
}

bool CVideoUploadManager_Custom::IsAddingText() const
{
	return (m_currentState == UploadState::AddTags);
}

f32 CVideoUploadManager_Custom::GetUploadFileProgress() const
{
	return (m_videoSize && m_currentYTUpload && m_currentYTUpload->UploadSize ) ? static_cast<float>(m_currentYTUpload->BytesUploaded) / static_cast<float>(m_currentYTUpload->UploadSize) : 0.0f;
}

bool CVideoUploadManager_Custom::IsGettingOAuth() const
{
	return !m_resumeSetup && m_currentState == UploadState::GettingOAuth;
}

bool CVideoUploadManager_Custom::IsAuthORecieved() const
{
	return m_oAuthSuccess;
}

const char* CVideoUploadManager_Custom::GetSourcePathAndFileName() const
{
	return m_sourcePathAndFileName.c_str();
}

bool CVideoUploadManager_Custom::IsPathTheCurrentUpload(const char* path) const
{
	atString paramPath(path);
#if RSG_PC
	paramPath.Replace("\\", "/");
#endif
	return (strcmp(paramPath.c_str(), m_sourcePathAndFileName.c_str()) == 0);
}

void CVideoUploadManager_Custom::PushToRawUploadedList(u32 filenameHash, u32 durationMs, u64 size)
{
	UploadedInfo info;
	info.m_filenameHash = filenameHash;
	info.m_durationMs = durationMs;
	info.m_size = size;
	m_uploadedRawList.PushAndGrow(info);
}

u32 CVideoUploadManager_Custom::GetUploadedCounterValue() const
{
	return m_uploadedCounter;
}

u32 CVideoUploadManager_Custom::GetYTFailedSetupCounterValue() const
{
	return m_failedYTSetupCounter;
}

u32 CVideoUploadManager_Custom::GetYTFailedCounterValue() const
{
	return m_failedYTCounter;
}

u32 CVideoUploadManager_Custom::GetUGCFailedCounterValue() const
{
	return m_failedUGCCounter;
}

u32 CVideoUploadManager_Custom::GetAlreadyUploadedCounterValue() const
{
	return m_alreadyUploadedCounter;
}

void CVideoUploadManager_Custom::FailUpload(UploadState::Enum failState, s32 errorCode, const char* failDetails)
{
	const char* metricFailReason = NULL;
	if (failDetails)
		metricFailReason = failDetails;
	else if (m_currentYTUpload && m_currentYTUpload->m_ErrReason.GetLength() > 0)
		metricFailReason = m_currentYTUpload->m_ErrReason.c_str();

	s32 metricErrorCode = m_currentYTUpload && (errorCode == 0 || errorCode == -1 ) ? m_currentYTUpload->m_ErrCode : errorCode;
	MetricFAILED_YOUTUBE_SUB metric(metricErrorCode, metricFailReason, g_stateNames[m_currentState]);
	APPEND_METRIC(metric);


	StopUpload(true);

	// record error code for posterity ...we fiddle with the errorCode parameter to show/hide a message
	m_errorCode = (errorCode > 0) ? static_cast<u32>(errorCode) : 0;
	if (m_errorCode)
	{
		char errorCodeString[8];
		formatf(errorCodeString, " %u", m_errorCode);

		m_errorDetails = TheText.Get("VEUI_UPLOAD_ERROR");
		m_errorDetails += errorCodeString;
		if ( failDetails )
		{
			m_errorDetails += " - ";
			m_errorDetails += failDetails;
		}
	}
	else if ( failDetails )
	{
		m_errorDetails = failDetails;
		m_errorCode = 1; // give value something valid over zero to display string
	}

	if (failState < UploadState::UploadYTSetupFail)
		failState = UploadState::UploadYTFail;

	// always show errors on set up
	if (failState == UploadState::UploadYTSetupFail || failState == UploadState::UploadYTChannelFail)
	{
		if (errorCode == 0)
			errorCode = -1;
	}
#if GTA_REPLAY
	else if (!IsOnGalleryMenu())
	{
		// only show other errors if on MyVideos page
		errorCode = 0;
	}
#endif

	if (failState == UploadState::UploadYTChannelFail)
	{
		m_screenToShow = UploadScreen::FailedChannel;
	}
	else
	{
		m_screenToShow = (errorCode != 0) ? UploadScreen::Failed :  UploadScreen::None;
	}

	m_screenShowing = UploadScreen::None;

	m_currentState = failState;

}

void CVideoUploadManager_Custom::CancelTaskIfNecessary(netStatus* status)
{
	if (!status)
		return;

	if (!status->Pending())
		return;

	if (netTask::HasTask(status))
	{
		netTask::Cancel(status);
	}
	else if (rlGetTaskManager() && rlGetTaskManager()->FindTask(status) != NULL)
	{
		rlGetTaskManager()->CancelTask(status);
	}
}

void CVideoUploadManager_Custom::StopUpload(bool cancelCompletely, bool silent, bool deleteUploadSaveFile)
{
	if (m_MyStatus.Pending()) 
	{
		if (m_currentState == UploadState::ReadingVideoBlock)
		{
			if (m_readBlockWorker)
			{
				delete m_readBlockWorker;
				m_readBlockWorker = NULL;
			}
		}

		CancelTaskIfNecessary(&m_MyStatus);
		m_MyStatus.Reset();
	}

	StopNetRetryTimer();

	// if we're publishing to UCG, we shouldn't cancel as the important big youtube upload is done 
	if (m_currentState == UploadState::UploadingPublish)
	{
		return;
	}

	if (cancelCompletely)
	{
		// make sure no warnings pop up if "silent" flag is on ...for switching users and Social Club stage
		if (IsInSetup() && !silent)
		{
			m_currentState = UploadState::CancelledInSetup;
		}

		Reset(deleteUploadSaveFile);
		
		if (!silent)
			m_screenToShow = UploadScreen::Cancelled;
		
		m_screenShowing = UploadScreen::None;
	}

	// ensure UI is updated to show the stop
	if (IsOnGalleryMenu())
	{
		CVideoEditorMenu::ForceUploadUpdate(m_userID ? true : false);
	}
}

void CVideoUploadManager_Custom::NoTextEntered()
{
	m_noTextEntered = true;
}

bool CVideoUploadManager_Custom::ResumeUploadUsingParameters(const char* sourcePathAndFileName, u32 videoDuration, const char* createdDate)
{
	if (!ASSET.Exists(sourcePathAndFileName,""))
		return false; // odds are very high the file has been destroyed since last playing

	// should already be authorised and have a bunch of stuff saved off
	// so just crack on with GetResumableUploadUrl to find where to carry on from
	if (m_currentYTUpload)
	{
		m_headerSuccess = true;

		// need to work out next percentage to notify user of progress
		float uploadProgress = GetUploadFileProgress();
		m_uploadProgressForNextInfo = 0.0f;
		bool found = false;
		while (m_uploadProgressForNextInfo < 1.0f && !found)
		{
			m_uploadProgressForNextInfo += 0.25f;
			if (m_uploadProgressForNextInfo > uploadProgress)
				found = true;
		}
		// bring up a upload started message too, if you're not in the My Video menu
#if GTA_REPLAY
		if (!IsOnGalleryMenu())
#endif
		{
			m_replaceGameStreamId = CGameStreamMgr::GetGameStream()->PostMessageText( "", "CHAR_YOUTUBE", "CHAR_YOUTUBE", false, 0, TheText.Get("YT_YOUTUBE"), TheText.Get("YT_UPLOAD_RESUMED"), 1.0f, "", 0, m_replaceGameStreamId );
		}
		return RequestReadVideoBufferBlock();
	}

	// if there is no current upload, start again
	Reset();
	return RequestPostVideo(sourcePathAndFileName, videoDuration, createdDate, 0);
}

void CVideoUploadManager_Custom::PauseUpload()
{
	if (!IsUploading())
		return;

	m_beforePauseState = m_currentState;

	m_oAuthSuccess = false;
	StopUpload(false);

	// bring up pause message, if not in My video menu (will get a bigger, better message in there ...this is more for online pausing it)
#if GTA_REPLAY
	if (!IsOnGalleryMenu() && !CLoadingScreens::AreActive())
#endif
	{
		m_replaceGameStreamId = CGameStreamMgr::GetGameStream()->PostMessageText( "", "CHAR_YOUTUBE", "CHAR_YOUTUBE", false, 0, TheText.Get("YT_YOUTUBE"), TheText.Get("YT_UPLOAD_PAUSED"), 1.0f, "", 0, m_replaceGameStreamId );
	}
}

void CVideoUploadManager_Custom::RequestResumeUpload()
{
	// if no YTUpload then we have nothing to bother resuming
	if (!m_currentYTUpload)
		return;

	m_currentState = UploadState::RequestResume;
	m_resumeSetup = true;
}

bool CVideoUploadManager_Custom::StartResumeUpload()
{
	m_currentState = UploadState::ResumeChecks;

	// just worried about the custom desc and custom tag, since we generate the rest
	// (and the generated stuff just for YT that SC considers "banned words" like Rockstar, that we nuke before SC publish)
	if (!rlUgc::CheckText(NetworkInterface::GetLocalGamerIndex(), m_currentYTUpload->VideoInfo.Title, m_customDesc.c_str(), m_customTag.c_str(), NULL, NULL, &m_MyStatus))
	{
		FailUpload(UploadState::UploadYTFail);
		return false;
	}
	return true;
}

bool CVideoUploadManager_Custom::ResumeUpload()
{
	m_resumeSetup = true;
 	
 	if (m_videoSize)
 	{
 		return ResumeUploadUsingParameters(m_sourcePathAndFileName.c_str(), m_videoDurationMs, m_sourceCreatedDate.c_str());
 	}

	return false;
}

#if ENABLE_VIDEO_UPLOAD_THROTTLES
void CVideoUploadManager_Custom::SetBlockSize(BlockSizeFraction::Enum blockSize)
{
	m_nextBlockSizeFraction = blockSize;
}

void CVideoUploadManager_Custom::SetIdealUploadSpeed(u32 speed)
{
	m_throttleSpeed = (speed > m_maxUploadSpeed) ? m_maxUploadSpeed : speed; //kb/s
}
#endif // ENABLE_VIDEO_UPLOAD_THROTTLES

void CVideoUploadManager_Custom::CreateUploadInfo()
{
	videoAssertf(!m_currentYTUpload, "CVideoUploadManager_Custom::GetOAuthToken - m_currentYTUpload already exists. This shouldn't happen");

	m_currentYTUpload = rage_new rage::rlYoutubeUpload;
	rage::rlYoutubeVideoInfo* videoInfo = &m_currentYTUpload->VideoInfo;

	CreateTags();

	videoInfo->Privacy = rage::rlYoutubeVideoInfo::PRIVATE;

	if (m_enableYoutubePublic)
	{
		CProfileSettings& settings = CProfileSettings::GetInstance();
		if(settings.AreSettingsValid())
		{
			switch (settings.GetInt(CProfileSettings::VIDEO_UPLOAD_PRIVACY))
			{
			case VIDEO_UPLOAD_PRIVACY_PUBLIC: 
				{
					videoInfo->Privacy = rage::rlYoutubeVideoInfo::PUBLIC;
				}
				break;
			case VIDEO_UPLOAD_PRIVACY_UNLISTED: 
				{
					videoInfo->Privacy = rage::rlYoutubeVideoInfo::UNLISTED;
				}
				break;
			default: break;
			};
		}
	}

	videoInfo->Embeddable = true;
}

void CVideoUploadManager_Custom::CreateTags()
{
	videoAssertf(m_currentYTUpload, "CVideoUploadManager_Custom::CreateTags - m_currentYTUpload should exist while adding tags");
	if (!m_currentYTUpload)
		return;

	// 	Game Name (all uploads get this): GTAV
	// 	Platform (Include correct platform): PC, XB1, PS4
	// 	Source (all uploads get this): ROCKSTAR_EDITOR

	// clear tags
	m_currentYTUpload->VideoInfo.Tags[0] = '\0';

#if RSG_ORBIS
	m_currentYTUpload->VideoInfo.AddTag("PS4"); 
#elif RSG_DURANGO
	m_currentYTUpload->VideoInfo.AddTag("XB1"); 
#elif RSG_PC
	m_currentYTUpload->VideoInfo.AddTag("PC"); 
#endif

	m_currentYTUpload->VideoInfo.AddTag("GTAV");

	// this will need to be removed before publishing to SC, as SC bans the word "rockstar"
	m_currentYTUpload->VideoInfo.AddTag("ROCKSTAR_EDITOR"); 
}

bool CVideoUploadManager_Custom::RestoreSavedUploadInfo(const char *pathAndFilename)
{
	videoAssertf(!m_currentYTUpload, "CVideoUploadManager_Custom::RestoreSavedUploadInfo - m_currentYTUpload already exists. This shouldn't happen");
	videoAssertf(pathAndFilename, "CVideoUploadManager_Custom::RestoreSavedUploadInfo - no path and filename passed in. This shouldn't happen. compiler should have complained");

	bool success = false;
	fiStream* stream = ASSET.Open(pathAndFilename, "json");
	if (stream)
	{
		m_currentYTUpload = rage_new rage::rlYoutubeUpload;
		rage::rlYoutubeVideoInfo* videoInfo = &m_currentYTUpload->VideoInfo;

		RsonReader rr;
		char buff[UPLOAD_RSON_BUFFER_SIZE] = {0};
		stream->Read(&buff[0], stream->Size());
		if (rr.Init(buff, sizeof(buff)))
		{
			RsonReader rrValue;
			if (rr.GetMember("duration", &rrValue))
			{
				rrValue.AsUns(m_videoDurationMs);
				if (m_videoDurationMs > 0)
				{
					char readString[UPLOAD_READ_BUFFER_SIZE] = {0};
					rr.GetValue("pathAndFilename", readString, UPLOAD_READ_BUFFER_SIZE);
					char createdString[UPLOAD_READ_BUFFER_SIZE] = {0};
					rr.GetValue("createdDate", createdString, UPLOAD_READ_BUFFER_SIZE);
					char displayName[UPLOAD_READ_BUFFER_SIZE] = {0};
					rr.GetValue("displayName", displayName, UPLOAD_READ_BUFFER_SIZE);
					if (GatherHeaderData(readString, m_videoDurationMs, createdString, displayName))
					{
						m_currentYTUpload->UploadSize= m_videoSize;

						rr.GetValue("uploadURL", readString, UPLOAD_READ_BUFFER_SIZE);
						m_currentYTUpload->UploadUrl = readString;

						rr.GetValue("title", readString, UPLOAD_READ_BUFFER_SIZE);
						safecpy(m_currentYTUpload->VideoInfo.Title, readString, rage::rlYoutubeVideoInfo::MAX_TITLE_LEN);

						rr.GetValue("description", readString, UPLOAD_READ_BUFFER_SIZE);
						safecpy(m_currentYTUpload->VideoInfo.Description, readString, rage::rlYoutubeVideoInfo::MAX_DESCRIPTION_LEN);
						m_customDesc = m_currentYTUpload->VideoInfo.Description;

						// create the generated tags first
						rr.GetValue("tags", readString, UPLOAD_READ_BUFFER_SIZE);
						safecpy(m_currentYTUpload->VideoInfo.Tags, readString, rage::rlYoutubeVideoInfo::MAX_TAG_LEN);
						m_customTag = readString;

						rr.GetValue("privacyStatus", readString, UPLOAD_READ_BUFFER_SIZE);

						m_currentYTUpload->VideoInfo.Privacy = rage::rlYoutubeVideoInfo::VideoPrivacy::PRIVATE;
						if (m_enableYoutubePublic)
						{
							if (strcmp(readString, "public") == 0)
								m_currentYTUpload->VideoInfo.Privacy = rage::rlYoutubeVideoInfo::VideoPrivacy::PUBLIC;
							else if (strcmp(readString, "unlisted") == 0)
								m_currentYTUpload->VideoInfo.Privacy = rage::rlYoutubeVideoInfo::VideoPrivacy::UNLISTED;
						}

						videoInfo->Embeddable = true;

						success = true;	
					}
				}
			}
		}
		stream->Close();
	}
	return success;
}

bool CVideoUploadManager_Custom::AddTitle(const char* string)
{
	if (!string || !m_currentYTUpload)
		return false;

	safecpy(m_currentYTUpload->VideoInfo.Title, string); 
	Utf8RemoveInvalidSurrogates(m_currentYTUpload->VideoInfo.Title);
	return true;
}

bool CVideoUploadManager_Custom::AddDescription(const char* string)
{
	if (!m_currentYTUpload)
		return false;

	// no text is valid
	m_customDesc = string ? string : "";

	atString description = m_customDesc;

	// add SC user profile link
	const char* nickName = NULL;
	atString userPageString;
	if ( CLiveManager::GetSocialClubMgr().GetLocalGamerAccountInfo() )
	{
		nickName = CLiveManager::GetSocialClubMgr().GetLocalGamerAccountInfo()->m_Nickname;
		if (nickName)
		{
			userPageString = TheText.Get("VEUI_UPLOAD_SC_LINK_DESC");
			userPageString.Replace("~a~", nickName);
		}
	}

	if (nickName)
	{
		if (description.GetLength() > 0)
			description += " - ";
		description += userPageString.c_str();
	}

	// make sure advert tunable is on
	if (m_enableAlbumAdvert)
	{
		u32 trackId = 0;
		atString albumString;
		CReplayVideoMetadataStore::VideoMetadata metaData;
		if (CReplayVideoMetadataStore::GetInstance().GetMetadata(m_displayName.c_str(), metaData))
		{
			// track ids are those for the Los Santos album ... so add an itunes link to desc
			if (metaData.trackId == 1415 || (metaData.trackId >= 1646 && metaData.trackId <= 1658) )
			{
				// if we only want to display with one track Id, this check will do it ... but if it's zero, then it'll just be the m_enableAlbumAdvert
				if (m_advertTrackId == 0 || m_advertTrackId == metaData.trackId)
				{
					trackId = metaData.trackId;
					albumString = TheText.Get("VEUI_UPLOAD_SELL_ALBUM_DESC");
					// put in URL ...doing it this way as it might change
					albumString.Replace("~a~", TheText.Get("VEUI_UPLOAD_SELL_ALBUM_URL"));
				}
			}
		}

		if (trackId > 0)
		{
			if (description.GetLength() > 0)
				description += " - ";
			description += albumString.c_str();
		}
	}

	formatf(m_currentYTUpload->VideoInfo.Description, m_currentYTUpload->VideoInfo.MAX_DESCRIPTION_LEN, "%s", description.c_str());
	Utf8RemoveInvalidSurrogates(m_currentYTUpload->VideoInfo.Description);
	return true;
}

bool CVideoUploadManager_Custom::AddYTTag(const char* string)
{
	// no text is valid
	if (!string)
	{
		m_customTag = "";
		return true;
	}

	if (!m_currentYTUpload)
		return false;

	m_currentYTUpload->VideoInfo.AddTag(string);
	m_customTag = string;
	return true;
}

bool CVideoUploadManager_Custom::GetOAuthToken()
{
	CreateUploadInfo();

	return GetOAuthTokenWithExistingUploadInfo();
}

bool CVideoUploadManager_Custom::RefreshOAuthToken()
{
	if (!m_currentYTUpload)
	{
		videoAssertf(false, "CVideoUploadManager_Custom::RefreshOAuthToken - m_currentYTUpload doesn't exists so we can't refresh the OAuthToken ...should be using Get0AuthToken");
		return false;
	}
	
	return GetOAuthTokenWithExistingUploadInfo();
}

bool CVideoUploadManager_Custom::GetOAuthTokenWithExistingUploadInfo()
{
	if (videoVerifyf(m_currentYTUpload, "VideoUploadManager::GetOAuthTokenWithExistingUploadInfo - No existing m_currentYTUpload. Fail.") )
	{
		if ( rage::rlYoutube::GetOAuthToken(m_localGamerIndex, m_currentYTUpload, &m_MyStatus) )
		{
			m_currentState = UploadState::GettingOAuth;
			return true;
		}
		else
		{
			videoAssertf(false, "CVideoUploadManager_Custom::GetOAuthToken - rlYoutube::GetOAuthToken FAIL");
		}
	}

	return false;
}

bool CVideoUploadManager_Custom::GetChannelInfo()
{	
	if (videoVerifyf(m_currentYTUpload, "VideoUploadManager::GetChannelInfo - No existing m_currentYTUpload. Fail.") )
	{
		if ( rage::rlYoutube::GetChannelInfo(m_currentYTUpload, &m_MyStatus) )
		{
			m_currentState = UploadState::ChannelCheck;
			return true;
		}
		else
		{
			videoAssertf(false, "CVideoUploadManager_Custom::GetChannelInfo - rlYoutube::GetChannelInfo FAIL");
		}
	}

	return false;
}

bool CVideoUploadManager_Custom::SendHeader()
{
	if ( rage::rlYoutube::GetResumableUploadUrl(m_currentYTUpload, &m_MyStatus))
	{
		m_currentState = UploadState::UploadingHeader;
		return true;
	}
	else
	{
		videoAssertf(false, "CVideoUploadManager_Custom::SendHeader - rlYoutube::GetResumableUploadUrl FAIL");
	}

	return false;
}

bool CVideoUploadManager_Custom::RequestReadVideoBufferBlock()
{
 	if (m_sourcePathAndFileName)
 	{
		if (!ASSET.Exists(m_sourcePathAndFileName, ""))
			return false;

		u32 uploadSize = m_maxBlockSize / m_currentBlockSizeFraction; // all power of 2 kind of numbers being chopped down into sensible fractions
		if (uploadSize < MinBlockSize)
			uploadSize = MinBlockSize;

		// allocate memory for full block if memory not allocated yet ...we'll keep reusing this
		if (!m_pBuffer)
		{
			m_pBuffer = rage_new u8[m_maxBlockSize]; // consider using streaming memory
			m_currentYTUpload->UploadSize = m_videoSize;
		}

		if (gnetVerifyf(m_pBuffer, "CVideoUploadManager_Custom::ReadVideoBufferBlock - couldn't allocate memory to video data buffer"))
		{
			memset(m_pBuffer, 0, sizeof(u8)*m_maxBlockSize);

			u64 bufferRead = 0;

			if (m_videoSize <= uploadSize)
			{
				m_currentUploadBlockSize = static_cast<u32>(m_videoSize);
			}
			else
			{
				bufferRead = m_currentYTUpload->BytesUploaded;

				u64 remaining = m_videoSize - bufferRead;

				uploadSize = remaining > uploadSize ? uploadSize : static_cast<u32>(remaining); // trim it down if we're just doing the end of the file
				m_currentUploadBlockSize = uploadSize;
			}

			m_readBlockWorker = rage_new ReadVideoBlockWorker;
			if (m_readBlockWorker && m_readBlockWorker->Configure(m_pBuffer, m_currentUploadBlockSize, bufferRead, m_sourcePathAndFileName.c_str(), &m_MyStatus))
			{
				m_currentState = UploadState::StartReadingVideoBlock;
				return true;
			}
		}
 	}

	return false;
}

bool CVideoUploadManager_Custom::UploadNextBlock()
{
	m_currentBlockSizeFraction = m_nextBlockSizeFraction;

	if (!RequestReadVideoBufferBlock())
	{
		FailUpload(UploadState::UploadYTFail);
		return false;
	}

	return true;
}

bool CVideoUploadManager_Custom::StartPublishUGC()
{
	if (rage::rlYoutube::GetVideoInfo(m_currentYTUpload, &m_MyStatus))
	{
		m_currentState = UploadState::GetUploadedVideoInfo;
		return true;
	}

	FailUpload(UploadState::UploadYTFail);
	return false;
}

bool CVideoUploadManager_Custom::PublishUGC()
{
	// now it's up to youtube, let's nuke all the extra desc that SC doesn't need, as it's already up
	formatf(m_currentYTUpload->VideoInfo.Description, m_currentYTUpload->VideoInfo.MAX_DESCRIPTION_LEN, "%s", m_customDesc.c_str());

	// get hash of filename ...actually, just do whole path. saves a few string commands
	m_currentYTUpload->VideoInfo.FilenameHash = FilenameHashFromFullPath(m_sourcePathAndFileName.c_str());

	// we are forcing in millisecond duration, as the one stored in YT is in seconds and not always returned correctly in responses
	m_currentYTUpload->VideoInfo.DurationMs = m_videoDurationMs;

	// get quality score and main track id for video
	m_currentYTUpload->VideoInfo.QualityScore = 0;
	m_currentYTUpload->VideoInfo.TrackId = 0;

	CReplayVideoMetadataStore::VideoMetadata metaData;
	if (CReplayVideoMetadataStore::GetInstance().GetMetadata(m_displayName.c_str(), metaData))
	{
		m_currentYTUpload->VideoInfo.QualityScore = metaData.score;
		m_currentYTUpload->VideoInfo.TrackId = metaData.trackId;
	}

	bool hasModdedContent = false;
#if RSG_PC
	// on PC, we need to set a UGC flag to know there may be modded content on video.
	u32 width, height, durationMs = 0;
	MediaDecoder::PeekVideoDetails( m_sourcePathAndFileName.c_str(), width, height, durationMs, &hasModdedContent );
#endif
	m_currentYTUpload->VideoInfo.ModdedContent = hasModdedContent;

	// need to remove ROCKSTAR_EDITOR tag, as SC doesn't allow the word "rockstar" as a tag
	atString tags(&m_currentYTUpload->VideoInfo.Tags[0]);
	tags.Replace(",ROCKSTAR_EDITOR", "");
	safecpy(m_currentYTUpload->VideoInfo.Tags, tags.c_str());

	if (UserContentManager::GetInstance().CreateYoutubeContent(m_currentYTUpload, true, &m_MyStatus) == INVALID_UGC_REQUEST_ID)
	{
		FailUpload(UploadState::UploadUGCFail, -1, TheText.Get("YT_VIDEO_SC_FAIL"));
		return false;
	}

	m_currentState = UploadState::UploadingPublish;

	CNetworkTelemetry::AppendVideoEditorUploaded(m_currentYTUpload->VideoInfo.DurationMs, m_currentYTUpload->VideoInfo.QualityScore);

	return true;
}

bool CVideoUploadManager_Custom::CheckTextOnUGC(UploadScreen::Enum textScreen, const char* string)
{
	videoAssertf(textScreen == UploadScreen::TitleText || textScreen == UploadScreen::DescText || textScreen == UploadScreen::TagText, "CVideoUploadManager_Custom::CheckTextOnUGC - must be a text screen being tested. Page passed in index: %d", textScreen);

	bool success = false;
	switch (textScreen)
	{
	case UploadScreen::TitleText:
		{
			success = rlUgc::CheckText(NetworkInterface::GetLocalGamerIndex(), string, NULL, NULL, NULL, NULL, &m_MyStatus);
		}
		break;
	case UploadScreen::DescText:
		{
			success = rlUgc::CheckText(NetworkInterface::GetLocalGamerIndex(), NULL, string, NULL, NULL, NULL, &m_MyStatus);
		}
		break;
	case UploadScreen::TagText:
		{
			success = rlUgc::CheckText(NetworkInterface::GetLocalGamerIndex(), NULL, NULL, string, NULL, NULL, &m_MyStatus);
		}
		break;
	default:
		{
			videoAssertf(false, "CVideoUploadManager_Custom::CheckTextOnUGC - must be a text screen being tested. Page passed in index: %d", textScreen);
		}
		break;
	};

	if (success)
	{
		CBusySpinner::On(TheText.Get("VEUI_CHECK_BANNED_WORD"), BUSYSPINNER_LOADING, SPINNER_SOURCE_VIDEO_UPLOAD);
	}

	return success;
}

bool CVideoUploadManager_Custom::UGCQueryYoutubeContent(const int offset, const int maxQueries)
{
	return (UserContentManager::GetInstance().QueryYoutubeContent(offset, maxQueries, true, &m_noofUGCVideos, sm_UGCVideoQueryDelegate, &m_MyStatus ) != INVALID_UGC_REQUEST_ID);
}

bool CVideoUploadManager_Custom::GetUploadStatus()
{
	if (rage::rlYoutube::GetUploadStatus(m_currentYTUpload, &m_MyStatus))
	{
		m_currentState = UploadState::GettingStatus;
		return true;
	}

	FailUpload(UploadState::UploadYTFail);
	return false;
}

void CVideoUploadManager_Custom::Reset(bool deleteUploadSaveFile)
{
	ResetCoreValues();

	bool fileGettingDeleted = false;
	if (deleteUploadSaveFile && m_currentUploadSavePathAndFilename.GetLength() > 0)
	{
		DeleteUploadFile(m_currentUploadSavePathAndFilename.c_str());
		fileGettingDeleted = true;
	}

	m_oAuthSuccess = false;
	m_resumeSetup = false;
	m_headerSuccess = false;
	m_noTextEntered = false;
	m_linkedToAccountsAttempted = false;

	m_uploadProgressForNextInfo = 0.25f;

	m_errorCode = 0;

	m_currentUploadBlockSize = 0;

	if (m_currentState != UploadState::CancelledInSetup)
		m_currentState = UploadState::Unused;

	m_currentVideoStage = UploadVideoStage::Other;

	m_retryCounter = MaxNoofRetrys;
	m_disconnectRetryCounter = MaxNoofDisconnectRetry;

	if (m_pBuffer)
		delete m_pBuffer;
	m_pBuffer = NULL;

	if (m_currentYTUpload)
		delete m_currentYTUpload;
	m_currentYTUpload = NULL;

	if (m_readBlockWorker)
	{
		delete m_readBlockWorker;
		m_readBlockWorker = NULL;
	}

	if (m_saveHeaderWorker)
	{
		delete m_saveHeaderWorker;
		m_saveHeaderWorker = NULL;
	}

	m_textBoxValid = false;

	// if file is getting deleted, and now everything is clear
	// make sure pausing is reset fully before starting something new
	if(fileGettingDeleted)
		SetForcePause(false);
}

bool CVideoUploadManager_Custom::StartNetRetryTimer(s32 minTime, s32 maxTime)
{
	// keep adjusting max if needed ... the last thing that calls takes priority, and timers will cope
	m_netRetryMaxTimeSeconds = maxTime;

	// no max time? well that's just crazy. We'll say 15 mins
	if (m_netRetryMaxTimeSeconds == 0)
	{
		m_netRetryMaxTimeSeconds = 15 * 60;
	}

	// sort out min time, and go exponential if already in use
	if (!m_netRetryTimerActive)
	{
		m_netRetryMinTimeSeconds = minTime;
		m_netRetryTimer.InitSeconds(m_netRetryMinTimeSeconds, m_netRetryMinTimeSeconds); // makes sure both are same, to stop randomness
		m_netRetryTimerActive = true;
	}
	else
	{
		s32 newMinTime = m_netRetryMinTimeSeconds * 2;
		if (newMinTime < minTime)
		{
			m_netRetryMinTimeSeconds = newMinTime;
		}
		else
		{
			s32 oldMinTime = m_netRetryMinTimeSeconds;
		
			// no max time, then keep getting bigger
			if (newMinTime > m_netRetryMaxTimeSeconds)
			{
				oldMinTime = m_netRetryMaxTimeSeconds;
				newMinTime = m_netRetryMaxTimeSeconds;
			}

			// introduce a bit of random, by ranging from last min to new min time
			m_netRetryTimer.InitSeconds(oldMinTime, newMinTime);

			if (m_netRetryMinTimeSeconds < m_netRetryMaxTimeSeconds)
				m_netRetryMinTimeSeconds *= 2; // always double
		}
	}

	return (m_netRetryMinTimeSeconds < m_netRetryMaxTimeSeconds);
}

void CVideoUploadManager_Custom::StopNetRetryTimer()
{
	m_netRetryMinTimeSeconds = 0;
	m_netRetryTimerActive = false;
	m_netRetryTimer.Stop();
}

bool CVideoUploadManager_Custom::UpdateNetRetryTimer()
{
	if (m_netRetryMinTimeSeconds)
	{
		m_netRetryTimer.Update();

		if (!m_netRetryTimer.IsTimeToRetry())
		{
			return false;
		}
		else
		{
			// stop ready for next restart
			m_netRetryTimer.Stop();
		}
	}

	return true;
}

u8 CVideoUploadManager_Custom::GetLatestPauseFlags()
{
	u8 flags = 0;

	if (m_forcePause)
		flags |= (1 << PauseFlags::Forced);

	if (CLoadingScreens::AreActive() || !m_hasCheckedForResumeFile)
		flags |= (1 << PauseFlags::LoadingScreen);
#if HAS_YT_DISCONNECT_MESSAGE
	// can't have recording and loading at same time ... and avoids a timing issue in the early loading screen
	else if (VideoRecording::IsRecording())
		flags |= (1 << PauseFlags::Exporting);
#endif

	if (NetworkInterface::IsInSession())
		flags |= (1 << PauseFlags::Online);

	if (m_suspendPause)
		flags |= (1 << PauseFlags::Suspend);

	return flags;
}

bool CVideoUploadManager_Custom::IsUploadSaveFileDeleting() 
{
	return m_deleteFileStatus && m_deleteFileStatus->Pending();
}

bool CVideoUploadManager_Custom::DeleteUploadFile(const char* path)
{
	if (!m_deleteFileStatus)
		return false;

	if (m_deleteFileStatus->Pending())
		return false;

	if (!ASSET.Exists(path, "json"))
		return false;

	// can't have more than two. 
	// if not pending, we're free to delete the last worker to start a new one
	if (m_deleteFileWorker)
	{
		delete m_deleteFileWorker;
		m_deleteFileWorker = NULL;
	}

	m_deleteFileWorker = rage_new DeleteFileWorker;
	if (m_deleteFileWorker && m_deleteFileWorker->Configure(path, m_deleteFileStatus))
	{
		if (m_deleteFileWorker->Start(0))
		{
			m_deleteFileStatus->SetPending();
			m_deleteFileWorker->Wakeup();
			return true;
		}
	}

	return false;
}


bool CVideoUploadManager_Custom::IsUploadingPaused()
{
	return m_pauseFlags ? true : false;
}

void CVideoUploadManager_Custom::SetForcePause(bool pause)
{
	m_forcePause = pause;
	UpdatePauseFlags();

	CProfileSettings& settings = CProfileSettings::GetInstance();
	if(settings.AreSettingsValid())
	{
		settings.Set(CProfileSettings::VIDEO_UPLOAD_PAUSE, m_forcePause ? 1 : 0);
		settings.Write();
	}
}

void CVideoUploadManager_Custom::UpdatePauseFlags()
{
	u8 pauseFlags = GetLatestPauseFlags();
	// pause
	if (pauseFlags != 0 && m_pauseFlags == 0)
	{
		PauseUpload();
	}
	// resume
	else if (pauseFlags == 0 && m_pauseFlags != 0)
	{
		RequestResumeUpload();
	}
	m_pauseFlags = pauseFlags;
}

void CVideoUploadManager_Custom::ForcePauseStateOnSuspend(bool pause)
{
	m_suspendPause = pause;
}

CVideoUploadManager_Custom::ReadVideoBlockWorker::ReadVideoBlockWorker()
	: m_bufLen(0)
	, m_readOffset(0)
	, m_buffer(NULL)
	, m_status(NULL)
{
}

bool CVideoUploadManager_Custom::ReadVideoBlockWorker::Configure(u8* buf, unsigned bufLen, u64 readOffset, const char* pathAndFilename, netStatus* status)
{
	bool success = false;

	rtry
	{
		rverify(buf, catchall, );
		rverify(status, catchall, );
		rverify(bufLen > 0, catchall, );
		rverify(pathAndFilename, catchall, );

		m_bufLen = bufLen;
		m_readOffset = readOffset;
		m_buffer = buf;
		m_pathAndFilename = pathAndFilename;
		m_status = status;

		success = true;
	}
	rcatchall
	{
		success = false;
	}

	return success;
}

bool CVideoUploadManager_Custom::ReadVideoBlockWorker::Start(const int cpuAffinity)
{
	bool success = this->rlWorker::Start("[GTAV] Upload Read Video Block", sysIpcMinThreadStackSize, cpuAffinity, true);
	return success;
}

bool CVideoUploadManager_Custom::ReadVideoBlockWorker::Stop()
{
	const bool success = this->rlWorker::Stop();
	return success;
}

void CVideoUploadManager_Custom::ReadVideoBlockWorker::Perform()
{
	rtry
	{
		rverify(m_buffer, catchall, );

		fiStream* pStream = ASSET.Open(m_pathAndFilename.c_str(),"");
		rverify(pStream, catchall, );

		pStream->Seek64(m_readOffset);
		pStream->Read(m_buffer, m_bufLen);
		pStream->Close();

		m_status->SetSucceeded(0);
	}
	rcatchall
	{
		m_status->SetFailed(0);
	}
}

CVideoUploadManager_Custom::SaveHeaderDataWorker::SaveHeaderDataWorker()
	: m_uploadData(NULL)
	, m_status(NULL)
{
}

bool CVideoUploadManager_Custom::SaveHeaderDataWorker::Configure(rage::rlYoutubeUpload* uploadData, const char* savePathAndFilename, const char* videoPathAndFilename, u32 videoDurationMs, const char* createdDate, const char* displayName, const char* description, const char* customTag, netStatus* status)
{
	bool success = false;

	rtry
	{
		rverify(uploadData, catchall, );
		rverify(savePathAndFilename, catchall, );
		rverify(videoPathAndFilename, catchall, );
		rverify(createdDate, catchall, );
		rverify(displayName, catchall, );
		rverify(description, catchall, );
		rverify(customTag, catchall, )
		rverify(videoDurationMs > 0, catchall, );
		rverify(status, catchall, );

		m_uploadData = uploadData;
		m_savePathAndFilename = savePathAndFilename;
		m_videoPathAndFilename = videoPathAndFilename;
		m_videoDurationMs = videoDurationMs;
		m_createdDate = createdDate;
		m_displayName = displayName;
		m_description = description;
		m_customTag = customTag;
		m_status = status;

		success = true;
	}
	rcatchall
	{
		success = false;
	}

	return success;
}

bool CVideoUploadManager_Custom::SaveHeaderDataWorker::Start(const int cpuAffinity)
{
	bool success = this->rlWorker::Start("[GTAV] Save Header Data", sysIpcMinThreadStackSize, cpuAffinity, true);
	return success;
}

bool CVideoUploadManager_Custom::SaveHeaderDataWorker::Stop()
{
	const bool success = this->rlWorker::Stop();
	return success;
}

void CVideoUploadManager_Custom::SaveHeaderDataWorker::Perform()
{
	rtry
	{
		rverify(m_uploadData, catchall, );

		atString fullPathAndFilename;
		char platformPath[RAGE_MAX_PATH];
		ReplayFileManager::FixName(platformPath, m_savePathAndFilename.c_str());
		fullPathAndFilename = platformPath;

		// make sure any deletion operations are done before checking to see if the save file still exists
		while (CVideoUploadManager_Custom::IsUploadSaveFileDeleting())
		{ 
			sysIpcSleep(1);
		}

		// make sure file is deleted if it still exists ...shouldn't be, but you never know
		if (ASSET.Exists( fullPathAndFilename.c_str(), "json" ))
		{
			if (CVideoUploadManager_Custom::DeleteUploadFile(m_savePathAndFilename.c_str()))
			{
				while (CVideoUploadManager_Custom::IsUploadSaveFileDeleting())
				{ 
					sysIpcSleep(1);
				}
			}
		}

		fiStream* pStream = ASSET.Create(m_savePathAndFilename.c_str(), "", false);
		rverify(pStream, catchall, );

		RsonWriter rw;
		char buff[UPLOAD_RSON_BUFFER_SIZE] = {0};
		rw.Init(buff, RSON_FORMAT_JSON);
		rcheck(rw.Begin(NULL, NULL), catchall, );

		rcheck(rw.WriteString("uploadURL", m_uploadData->UploadUrl), catchall, );
		rcheck(rw.WriteString("privacyStatus", m_uploadData->VideoInfo.GetPrivacyString()), catchall, );
		rcheck(rw.WriteString("title", m_uploadData->VideoInfo.Title), catchall, );
		rcheck(rw.WriteString("description", m_description), catchall, );
		rcheck(rw.WriteString("tags", m_customTag), catchall, );
		rcheck(rw.WriteString("pathAndFilename", m_videoPathAndFilename), catchall, );
		rcheck(rw.WriteUns("duration", m_videoDurationMs), catchall, );
		rcheck(rw.WriteString("createdDate", m_createdDate), catchall, );
		rcheck(rw.WriteString("displayName", m_displayName), catchall, );

		rcheck(rw.End(), catchall,);

		pStream->Write(buff, (int)strlen(buff));
		pStream->Close();

		m_status->SetSucceeded(0);
	}
	rcatchall
	{
		m_status->SetFailed(0);
	}
}

CVideoUploadManager_Custom::DeleteFileWorker::DeleteFileWorker()
	: m_status(NULL)
{
}

bool CVideoUploadManager_Custom::DeleteFileWorker::Configure(const char* pathAndFilename, netStatus* status)
{
	bool success = false;

	rtry
	{
		rverify(pathAndFilename, catchall, );
		rverify(status, catchall, );

		rverify(ASSET.Exists(pathAndFilename, "json"), catchall, );

		m_pathAndFilename = pathAndFilename;
		m_status = status;

		success = true;
	}
	rcatchall
	{
		success = false;
	}

	return success;
}

bool CVideoUploadManager_Custom::DeleteFileWorker::Start(const int cpuAffinity)
{
	bool success = this->rlWorker::Start("[GTAV] Delete Upload Data File", sysIpcMinThreadStackSize, cpuAffinity, true);
	return success;
}

bool CVideoUploadManager_Custom::DeleteFileWorker::Stop()
{
	const bool success = this->rlWorker::Stop();
	return success;
}

void CVideoUploadManager_Custom::DeleteFileWorker::Perform()
{
	int resultCode = -1;
	rtry
	{
		rverify(m_pathAndFilename, catchall, );
		atString fullPathAndFilename;
		char platformPath[RAGE_MAX_PATH];
		ReplayFileManager::FixName(platformPath, m_pathAndFilename.c_str());
		fullPathAndFilename = platformPath;
		
#if (RSG_PC || RSG_DURANGO)
		resultCode = DeleteFileA(fullPathAndFilename.c_str()) ? 0 : -1;
#elif RSG_ORBIS
		resultCode = sceKernelUnlink(fullPathAndFilename.c_str());
#endif
		rcheck(resultCode == 0, catchall, );

		m_status->SetSucceeded(0);
	}
	rcatchall
	{
		m_status->SetFailed(resultCode);
	}
}

#endif // !USE_ORBIS_UPLOAD
#endif //  defined(GTA_REPLAY) && GTA_REPLAY 
