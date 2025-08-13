/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : SocialClubMenu.cpp
// PURPOSE : manages warning messages in the game
// AUTHOR  : James Chagaris
// STARTED : 08/08/2012
//
/////////////////////////////////////////////////////////////////////////////////

// rage
#include "bank/text.h"
#include "rline/socialclub/rlsocialclub.h"
#include "fwrenderer/renderthread.h"
#include "string/stringutil.h"
#include "math/random.h"
#include "parser/manager.h"
#include "system/nelem.h"

// game
#include "frontend/MousePointer.h"
#include "frontend/SocialClubMenu.h"
#include "frontend/BusySpinner.h"
#include "frontend/PauseMenu.h"
#include "frontend/scaleform/ScaleFormMgr.h"
#include "frontend/ProfileSettings.h"
#include "frontend/ui_channel.h"
#include "Network/Live/livemanager.h"
#include "Network/General/NetworkUtil.h"
#include "Network/Sessions/NetworkSession.h"
#include "system/controlMgr.h"
#include "text/messages.h"
#include "text/TextConversion.h"
#include "System/ControlMgr.h"
#include "cutscene/CutSceneManagerNew.h"

#if RSG_PC
#include "rline/rlpc.h"
#endif // RSG_PC

RAGE_DEFINE_SUBCHANNEL(ui, scmenu, DIAG_SEVERITY_DEBUG1, DIAG_SEVERITY_DEBUG1)

#undef __ui_channel
#define __ui_channel ui_scmenu

#define SC_BUTTON_MOVIE_FILENAME	"PAUSE_MENU_INSTRUCTIONAL_BUTTONS"

#define INVALID_DAY (0) // range 1-31
#define INVALID_MONTH (0) // range 1-12
#define INVALID_YEAR (0) // range 1900+

#define NUM_DAY_CHARACTERS (3)
#define NUM_MONTH_CHARACTERS (3)
#define NUM_YEAR_CHARACTERS (5)

#define PREALLOC_TOUR_BUFFER (3 * 1024)
#define PREALLOC_TOUR_IMG_BUFFER (65 * 1024)

#define NUM_FRAMES_DELAY_TO_SHOW_BUTTONS (5)
#define SC_MIN_ACCEPT_TIME (1000)
#define SC_MAX_ACCEPT_TIME (20000)

bool BaseSocialClubMenu::sm_isActive = false;
bool BaseSocialClubMenu::sm_flaggedForDeletion = false;
u8 BaseSocialClubMenu::sm_framesUntilDeletion = 0;

bool SocialClubMenu::sm_isTooYoung = false;
bool SocialClubMenu::sm_wasPMOpen = false;
bool SocialClubMenu::sm_shouldUnpauseGameplay = false;
u32 SocialClubMenu::sm_tourHash = 0;
u32 SocialClubMenu::sm_currentTourHash = 0;
eFRONTEND_MENU_VERSION SocialClubMenu::sm_currentMenuVersion;
eMenuScreen SocialClubMenu::sm_currentMenuScreen;

sysCriticalSectionToken SocialClubMenu::sm_renderingSection;
sysCriticalSectionToken SocialClubMenu::sm_networkSection;

SCTour::SCTour()
{
	m_hasCloudImage = false;
	m_hasDownloadFailed = false;
	m_TextureDictionarySlot = -1;
	m_requestHandle = CTextureDownloadRequest::INVALID_HANDLE;

	Shutdown();
}

void SCTour::Shutdown()
{
	ReleaseAndCancelTexture();

	m_hasCloudImage = false;
	m_hasDownloadFailed = false;
	m_tagHash = 0;
	m_header.Reset();
	m_body.Reset();
	m_filepath.Reset();
	m_filename.Reset();
}

void SCTour::StartDownload()
{
	m_requestDesc.m_Type = CTextureDownloadRequestDesc::GLOBAL_FILE;
	m_requestDesc.m_GamerIndex = 0;
	m_requestDesc.m_CloudFilePath = m_filepath.c_str();
	m_requestDesc.m_BufferPresize = PREALLOC_TOUR_IMG_BUFFER;  //arbitrary pre size of 22k
	m_requestDesc.m_TxtAndTextureHash = m_filename.c_str();
	m_requestDesc.m_CloudOnlineService = RL_CLOUD_ONLINE_SERVICE_NATIVE;

	CTextureDownloadRequest::Status retVal = CTextureDownloadRequest::REQUEST_FAILED;
	retVal = DOWNLOADABLETEXTUREMGR.RequestTextureDownload(m_requestHandle, m_requestDesc);

	m_TextureDictionarySlot = -1;

	m_hasCloudImage = false;
	m_hasDownloadFailed = false;
	uiAssertf(retVal == CTextureDownloadRequest::IN_PROGRESS || retVal == CTextureDownloadRequest::READY_FOR_USER, "Tour::StartDownload - retVal = %d", retVal);
}

void SCTour::UpdateDownload()
{
	if (m_requestHandle == CTextureDownloadRequest::INVALID_HANDLE)
	{
		return;
	}

	if (DOWNLOADABLETEXTUREMGR.HasRequestFailed(m_requestHandle))
	{
		ReleaseAndCancelTexture();
		m_hasDownloadFailed = true;
	}
	else if ( DOWNLOADABLETEXTUREMGR.IsRequestReady( m_requestHandle ) )
	{
		//Update the slot that the texture was slammed into
		strLocalIndex txdSlot = strLocalIndex(DOWNLOADABLETEXTUREMGR.GetRequestTxdSlot(m_requestHandle));
		if (uiVerifyf(g_TxdStore.IsValidSlot(txdSlot), "Tour::UpdateDownload - failed to get valid txd with name %s from DTM at slot %d", m_requestDesc.m_TxtAndTextureHash.GetCStr(), txdSlot.Get()))
		{
			g_TxdStore.AddRef(txdSlot, REF_OTHER);
			m_TextureDictionarySlot = txdSlot;
			uiDebugf1("Tour::UpdateDownload - Downloaded texture for Tour %s has been received in TXD slot %d",  m_filename.c_str(), txdSlot.Get());
		}

		m_hasCloudImage = true;

		//Tell the DTM that it can release it's hold on the TXD (since we've taken over the Reference to keep it in memory.
		DOWNLOADABLETEXTUREMGR.ReleaseTextureDownloadRequest( m_requestHandle );
		m_requestHandle = CTextureDownloadRequest::INVALID_HANDLE;
	}

}

void SCTour::ReleaseAndCancelTexture()
{
	if (m_TextureDictionarySlot.Get() >= 0)
	{
		g_TxdStore.RemoveRef(m_TextureDictionarySlot, REF_OTHER);
		m_TextureDictionarySlot = strLocalIndex(-1);
	}

	if (m_requestHandle != CTextureDownloadRequest::INVALID_HANDLE)
	{
		DOWNLOADABLETEXTUREMGR.ReleaseTextureDownloadRequest( m_requestHandle );
		m_requestHandle = CTextureDownloadRequest::INVALID_HANDLE;
	}
}

void SCTour::SetTag(const char* pTag)
{
	m_tagHash = atStringHash(pTag ? pTag : "");
}

void SCTour::SetImg(const char* pFullPath)
{
	if(pFullPath)
	{
		const char* startTag = "Cloud:/Global/";
		uiAssertf(stristr(pFullPath, startTag) == pFullPath, "Tour::SetImg - All Tour images should start with %s", startTag);

		int fullLen = int(strlen(pFullPath));
		int tagLen = int(strlen(startTag));
		if(tagLen < fullLen)
		{
			pFullPath = (pFullPath + tagLen);
		}

		m_filepath = pFullPath;
		int nameStartIndex = m_filepath.LastIndexOf('/') + 1;
		m_filename = m_filepath.c_str() + nameStartIndex;


		int extensionIndex = m_filename.IndexOf('.');
		if(extensionIndex != -1)
		{
			m_filename[extensionIndex] = 0;
		}
	}
}

void SCTourManager::Init(int gamerIndex, const char* pLangCode)
{
	m_gamerIndex = gamerIndex;

	if (!RL_IS_VALID_LOCAL_GAMER_INDEX(m_gamerIndex))
	{
		uiAssertf(0, "We've tried to initialize our SocialClubLegal with an invalid gamerIndx %d", m_gamerIndex);
		return;
	}

	m_tourFilename = "";
	m_tourFilename += "sc/Tours/";
	m_tourFilename += pLangCode;
	m_tourFilename += "/tour_";
	m_tourFilename += pLangCode;
	m_tourFilename += ".xml";
}

void SCTourManager::Update()
{
	if(m_downloadComplete)
	{
		for(int i=0; i<m_tours.size(); ++i)
		{
			m_tours[i].UpdateDownload();
		}
	}
	else if(m_fileRequestId == INVALID_CLOUD_REQUEST_ID && m_tourFilename.length() > 0)
	{
		// Not caching since it causes frame hitches, and the player should only enter this menu once.
		m_fileRequestId = CloudManager::GetInstance().RequestGetGlobalFile(m_tourFilename.c_str(), PREALLOC_TOUR_BUFFER, eRequest_CacheNone);
	}
}

void SCTourManager::Shutdown()
{
	m_hasError = false;
	m_downloadComplete = false;
	m_gamerIndex = -1;
	m_resultCode = RLSC_ERROR_NONE;
	m_tourFilename.Reset();
	m_fileRequestId = INVALID_CLOUD_REQUEST_ID;

	for(int i=0; i<m_tours.size(); ++i)
	{
		m_tours[i].Shutdown();
	}
	m_tours.Reset();
}

void SCTourManager::OnCloudEvent(const CloudEvent* pEvent)
{
	switch(pEvent->GetType())
	{
	case CloudEvent::EVENT_REQUEST_FINISHED:
		{
			// grab event data
			const CloudEvent::sRequestFinishedEvent* pEventData = pEvent->GetRequestFinishedData();

			// check if we're interested
			if(pEventData->nRequestID != m_fileRequestId)
				return;

			SocialClubMenu::LockNetwork();
			if(SSocialClubMenu::IsInstantiated())
			{
				m_fileRequestId = INVALID_CLOUD_REQUEST_ID;

				// did we get the file...
				if(pEventData->bDidSucceed)
				{
					char bufferFName[RAGE_MAX_PATH];
					fiDevice::MakeMemoryFileName(bufferFName, sizeof(bufferFName), pEventData->pData, pEventData->nDataSize, false, m_tourFilename.c_str());

					//Now load the text file.
					INIT_PARSER;
					{
						parTree* pTree = PARSER.LoadTree(bufferFName, "");
						if(pTree)
						{
							parTreeNode* pRootNode = pTree->GetRoot();

							if(uiVerify(pRootNode))
							{
								parTreeNode* pTourNode = NULL;
								const int numChildren = pRootNode->FindNumChildren();
								m_tours.Reserve(numChildren);

								for(int i=0; i<numChildren; ++i)
								{
									pTourNode = pRootNode->FindChildWithName("Tour", pTourNode);
									if(uiVerify(pTourNode))
									{
										SCTour& rTour = m_tours.Append();

										char buffer[50] = {0};
										rTour.SetTag(pTourNode->GetElement().FindAttributeStringValue("name", "", buffer, NELEM(buffer)));

										const parTreeNode* pNode = pTourNode->FindChildWithName("Header");
										if(pNode)
										{
											rTour.SetHeader(pNode->GetData());
										}

										pNode = pTourNode->FindChildWithName("Body");
										if(pNode)
										{
											rTour.SetBody(pNode->GetData());
										}

										pNode = pTourNode->FindChildWithName("Callout");
										if(pNode)
										{
											rTour.SetCallout(pNode->GetData());
										}

										pNode = pTourNode->FindChildWithName("img");
										if(pNode)
										{
											rTour.SetImg(pNode->GetData());
										}
									}
									else
									{
										break;
									}
								}
							}

							delete pTree;
						}
					}
					SHUTDOWN_PARSER;

					for(int i=0; i<m_tours.size(); ++i)
					{
						m_tours[i].StartDownload();
					}
				}
				else
				{
#if !__NO_OUTPUT
					if(pEventData->nResultCode == NET_HTTPSTATUS_NOT_FOUND)
					{
						uiDebugf1("TourManager::OnDownloadComplete - Failed to download %s; resultCode=404, going to force everything to show.", m_tourFilename.c_str());
					}
					else
					{
						uiDebugf1("TourManager::OnDownloadComplete - Failed to download %s, going to try again. isOnline=%d, resultCode=%d", m_tourFilename.c_str(), rlRos::IsOnline(m_gamerIndex), pEventData->nResultCode);
					}
#endif

					m_hasError = true;
					m_resultCode = pEventData->nResultCode;
				}

				m_downloadComplete = true; // Make sure we stop acting on stuff.
			}
			SocialClubMenu::UnlockNetwork();
		}
		break;

	case CloudEvent::EVENT_AVAILABILITY_CHANGED:
		break;
	};
}

bool SCTourManager::IsDownloading()
{
	for(int i=0; i<m_tours.size(); ++i)
	{
		if(m_tours[i].IsDownloading())
		{
			return true;
		}
	}

	return !m_downloadComplete;
}

int SCTourManager::GetStartingTour(u32 tourHash)
{
	for(int i=0; i<m_tours.size(); ++i)
	{
		if(m_tours[i].GetTagHash() == tourHash)
		{
			return i;
		}
	}

	return m_tours.empty() ? 0 : g_DrawRand.GetRanged(0, m_tours.size()-1);
}


const char* SCTourManager::GetError() const
{
	if(rlRos::GetCredentials(m_gamerIndex).HasPrivilege(RLROS_PRIVILEGEID_DEVELOPER))
	{
		return SocialClubMgr::GetErrorName(m_resultCode);
	}
	else
	{
		return "SOCIAL_CLUB_UNKNOWN";
	}
}

const char* SCTourManager::GetErrorTitle() const
{
	if(rlRos::GetCredentials(m_gamerIndex).HasPrivilege(RLROS_PRIVILEGEID_DEVELOPER))
	{
		return "DEV_SC_TOUR_FAILED";
	}
	else
	{
		return "SC_WARNING_TITLE";
	}
}


//////////////////////////////////////////////////////////////////////////////////////////////////////
void SocialClubMenu::Open( bool const isPolicyMenu, bool const ignoreLoadingScreens )
{
	uiDebugf1("SocialClubMenu::Open - isPolicyMenu=%d", isPolicyMenu);

	// if loading screens are active then shut them down. Temporary fix until we can deal with this properly
	if( !ignoreLoadingScreens && CLoadingScreens::AreActive())
	{
		CLoadingScreens::Shutdown(SHUTDOWN_CORE);
    }

	if(isPolicyMenu)
	{
		if(!SSocialClubLegalsMenu::IsInstantiated())
		{
			SSocialClubLegalsMenu::Instantiate();
			SSocialClubLegalsMenu::GetInstance().Init();
		}
	}
	else
	{
		if(!SSocialClubMenu::IsInstantiated())
		{
			SSocialClubMenu::Instantiate();
			SSocialClubMenu::GetInstance().Init();
		}
	}

	CPauseMenu::DeactivateSocialClubContext();

#if SUPPORT_MULTI_MONITOR
	CutSceneManager::SetManualBlinders(true);
	CutSceneManager::StartMultiheadFade(true, true);
#endif // SUPPORT_MULTI_MONITOR

	// switch off live streaming so that sign-in details are not broadcast
#if RSG_ORBIS
	NetworkInterface::EnableLiveStreaming(false);
#endif
}

void SocialClubMenu::Close()
{
	uiDebugf1("SocialClubMenu::Close - Closing the SC menu now.");

	gRenderThreadInterface.Flush();

	LockNetwork();
	LockRender();

	if(SSocialClubMenu::IsInstantiated())
	{
		SSocialClubMenu::Destroy();
	}
	if(SSocialClubLegalsMenu::IsInstantiated())
	{
		SSocialClubLegalsMenu::Destroy();
	}


	if(sm_wasPMOpen)
	{
		CPauseMenu::Open(sm_currentMenuVersion, true, sm_currentMenuScreen);
		CPauseMenu::ActivateSocialClubContext();
		CPauseMenu::RedrawInstructionalButtons();
	}

	UnlockRender();
	UnlockNetwork();

	if(sm_shouldUnpauseGameplay)
	{
		CPauseMenu::SetupCodeForUnPause();
		sm_shouldUnpauseGameplay = false;
	}

#if SUPPORT_MULTI_MONITOR
	CutSceneManager::StartMultiheadFade(false, false);
	CutSceneManager::SetManualBlinders(false);
#endif // SUPPORT_MULTI_MONITOR

	// switch on live streaming again
#if RSG_ORBIS
	NetworkInterface::EnableLiveStreaming(true);
#endif
}

void SocialClubMenu::UpdateWrapper()
{
	PF_AUTO_PUSH_TIMEBAR("SocialClubMenu UpdateWrapper");

	if(SSocialClubMenu::IsInstantiated())
	{
		SSocialClubMenu::GetInstance().Update();
	}
	if(SSocialClubLegalsMenu::IsInstantiated())
	{
		SSocialClubLegalsMenu::GetInstance().Update();
	}

	if(IsFlaggedForDeletion() && --sm_framesUntilDeletion == 0)
	{
		Close();
	}
}

void SocialClubMenu::RenderWrapper()
{
	LockRender();
	if(SSocialClubMenu::IsInstantiated())
	{
		SSocialClubMenu::GetInstance().Render();
	}
	if(SSocialClubLegalsMenu::IsInstantiated())
	{
		SSocialClubLegalsMenu::GetInstance().Render();
	}
	UnlockRender();
}

void SocialClubMenu::CacheOffPMState()
{
	sm_wasPMOpen = CPauseMenu::IsActive(PM_SkipSocialClub);
	if(sm_wasPMOpen)
	{
		sm_currentMenuVersion = CPauseMenu::GetCurrentMenuVersion();
		sm_currentMenuScreen = static_cast<eMenuScreen>(CPauseMenu::GetCurrentHighlightedTabData().MenuScreen.GetValue());
	}

	sm_currentTourHash = sm_tourHash;
	sm_tourHash = 0;
}

void SocialClubMenu::OnPresenceEvent(const rlPresenceEvent* evt)
{
	if(PRESENCE_EVENT_SIGNIN_STATUS_CHANGED == evt->GetId())
	{
		if((SSocialClubMenu::IsInstantiated() && !SSocialClubMenu::GetInstance().IsIdle()) ||
			(SSocialClubLegalsMenu::IsInstantiated() && !SSocialClubLegalsMenu::GetInstance().IsIdle())) // Hack for SC in loading screen.
		{
			uiDebugf1("SocialClubMenu::OnPresenceEvent - Closing Social Club menu due to signin status change.");

			if(sm_wasPMOpen)
			{
				CPauseMenu::Close();
				sm_shouldUnpauseGameplay = true;
				sm_wasPMOpen = false;
			}

			if(NetworkInterface::IsNetworkOpen() || SSocialClubLegalsMenu::IsInstantiated())
			{
				SocialClubMenu::FlagForDeletion();
			}
			else
			{
				const bool c_isShowingWebBrowser = CLiveManager::GetSocialClubMgr().IsShowingWebBrowser();
				if ( !c_isShowingWebBrowser )
				{
					SSocialClubMenu::GetInstance().SetGenericError( "HUD_DISCONNOLOGO", "SC_ERROR_TITLE" );
				}
			}
		}

		const int nControllerIndex = CControlMgr::GetMainPlayerIndex();
		const rlPresenceEventSigninStatusChanged* s = evt->m_SigninStatusChanged;
		if(s)
		{
			if(nControllerIndex == s->m_GamerInfo.GetLocalIndex())
			{
				if(!SPolicyVersions::IsInstantiated())
				{
					SPolicyVersions::Instantiate();
				}

				if(s->SignedIn() || s->SignedOut())
				{
					sm_isTooYoung = false;
					if( SSocialClubMenu::IsInstantiated() || SSocialClubLegalsMenu::IsInstantiated())
					{
						SocialClubMenu::FlagForDeletion();
					}
				}

				if(s->m_GamerInfo.IsOnline())
				{
					SPolicyVersions::GetInstance().Shutdown();
					SPolicyVersions::GetInstance().Init();
				}
			}
		}
	}
}

void SocialClubMenu::OnRosEvent(const rlRosEvent& rEvent)
{
	if(SSocialClubMenu::IsInstantiated())
	{
		switch(rEvent.GetId())
		{
		case RLROS_EVENT_GET_CREDENTIALS_RESULT:
			{
				const rlRosEventGetCredentialsResult& credEvent = static_cast<const rlRosEventGetCredentialsResult&>(rEvent);

				const rlGamerInfo* pActiveInfo = NetworkInterface::GetActiveGamerInfo();
				if(pActiveInfo && pActiveInfo->GetLocalIndex() == credEvent.m_LocalGamerIndex)
				{
					SSocialClubMenu::GetInstance().SetGotCredsResult();
				}
			}
			break;
		default:
			break;
		}
	}
}

SocialClubMenu::SocialClubMenu()
{
	Shutdown();

	const char* pMovie = "SOCIAL_CLUB2";
	m_scMovie.CreateMovieAndWaitForLoad(SF_BASE_CLASS_GENERIC, pMovie, Vector2(0.0f, 0.0f), Vector2(1.0f, 1.0f));
	CScaleformMgr::ForceMovieUpdateInstantly(m_scMovie.GetMovieID(), true);


	m_buttonMovie.CreateMovieAndWaitForLoad(SF_BASE_CLASS_GENERIC, SC_BUTTON_MOVIE_FILENAME, Vector2(0.0f, 0.0f), Vector2(1.0f, 1.0f));
	CBusySpinner::RegisterInstructionalButtonMovie(m_buttonMovie.GetMovieID());  // register this "instructional button" movie with the spinner system
	CScaleformMgr::ForceMovieUpdateInstantly(m_buttonMovie.GetMovieID(), true);
}

SocialClubMenu::~SocialClubMenu()
{
	Shutdown();

	// CLEAR the button movie's movie ref if the Pause Menu already had requested it
	// this is to prevent a total showstopper and in lieu of ref-counting these things
	if( !CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_INSTRUCTIONAL_BUTTONS).IsFree() )
	{
		CBusySpinner::UnregisterInstructionalButtonMovie(m_buttonMovie.GetMovieID());
		m_buttonMovie.Clear();
	}
}

void SocialClubMenu::Render()
{
	if(m_state != SC_IDLE)
	{
		m_scMovie.Render();
		m_buttonMovie.Render();
	}
}

void SocialClubMenu::Init()
{
	sm_isActive = true;

	if(!SPolicyVersions::GetInstance().IsInitialized())
	{
		SPolicyVersions::GetInstance().Init();
	}

	CLiveManager::GetSocialClubMgr().GetSocialClubInfo().Reset();

	sm_flaggedForDeletion = false;
	m_buttonMovie.CallMethod("CONSTRUCTION_INNARDS");

	if(CLiveManager::CheckIsAgeRestricted())
	{
		SetGenericError("HUD_AGERES", "CWS_WARNING");
	}
	else if(!CLiveManager::IsOnline())
	{
		SetGenericError("SC_SIGNIN");
	}
	else if(CNetwork::GetNetworkSession().IsActivitySession())
	{
		SetGenericError("SC_SIGNIN_JOB");
	}
	else
	{
		const rlGamerInfo* pGamerInfo = NetworkInterface::GetActiveGamerInfo();
		const rlRosCredentials& creds = rlRos::GetCredentials(pGamerInfo->GetLocalIndex());

		if(creds.IsValid())
		{
			if(!creds.HasPrivilege(RLROS_PRIVILEGEID_CREATE_TICKET) || creds.HasPrivilege(RLROS_PRIVILEGEID_BANNED))
			{
				SetGenericError("SC_ERROR_BANNED");
			}
			else if (!CLiveManager::GetSocialClubMgr().GetPasswordRequirements().bValid)
			{
				CLiveManager::GetSocialClubMgr().RetryPasswordRequirements();
				SetGenericError("SOCIAL_CLUB_UNKNOWN");
			}
			else if(NetworkInterface::IsCloudAvailable())
			{
				GoToState(SC_DOWNLOADING);

				m_policyManager.Init(pGamerInfo->GetLocalIndex(), NetworkUtils::GetLanguageCode());
				m_tourManager.Init(pGamerInfo->GetLocalIndex(), NetworkUtils::GetLanguageCode());

				m_scMovie.CallMethod("SET_GAMERNAME", pGamerInfo->GetDisplayName());

				// What am I supposed to do with this?
				const rlScAccountInfo* pInfo = CLiveManager::GetSocialClubMgr().IsConnectedToSocialClub() ? CLiveManager::GetSocialClubMgr().GetLocalGamerAccountInfo() : NULL;
				if (pInfo)
				{
					CLiveManager::GetSocialClubMgr().GetSocialClubInfo().SetNickname(pInfo->m_Nickname);
				}
				m_scMovie.CallMethod("SET_SOCIAL_CLUB_PRESENCE_ACTIVE", pInfo ? pInfo->m_Nickname : "");

				rlAgeGroup ageGroup = rlGamerInfo::GetAgeGroup(pGamerInfo->GetLocalIndex());
				int yearsOld = rlGamerInfo::GetAge(pGamerInfo->GetLocalIndex());
				uiDisplayf("SC - Years Old = %d, Age Group = %d", yearsOld, ageGroup);

				if(sm_isTooYoung
					|| (0 <= yearsOld && yearsOld < JOINABLE_AGE_YEARS)
					|| (RL_AGEGROUP_CHILD <= ageGroup && ageGroup < RL_AGEGROUP_TEEN)
#if !SC_COLLECT_AGE
					|| !CLiveManager::GetSocialClubMgr().GetSocialClubInfo().IsOldEnoughToJoinSC()
#endif
					)
				{
					SetGenericError("SC_ERROR_INVALID_ACCOUNT_AGE", "SC_WARNING_TITLE");
				}
			}
			else if(creds.HasPrivilege(RLROS_PRIVILEGEID_DEVELOPER))
			{
				SetGenericError("DEV_SC_NO_CLOUD");
			}
			else
			{
				SetGenericError("SOCIAL_CLUB_UNKNOWN");
			}
		}
		else
		{
#if !__NO_OUTPUT
			SetGenericError("DEV_SC_NO_TICKET");
#else
			SetGenericError("SOCIAL_CLUB_UNKNOWN");
#endif
		}
	}
}

void SocialClubMenu::Shutdown()
{
	BaseSocialClubMenu::Shutdown();

	sm_tourHash = 0;

	m_state = SC_IDLE;
	m_isKeyboardOpen = false;
	m_allowEmailSpam = false;
	m_isSpammable = true;
	m_isInSignInFlow = false;
	m_isPolicyUpdateFlow = false;
	m_gotCredsResult = false;

	m_welcomeTab = -1;
	m_currAlt = -1;
	m_altCount = 0;

	m_day = INVALID_DAY;
	m_month = INVALID_MONTH;
	m_year = INVALID_YEAR;

	m_dayBuffer[0] = '\0';
	m_monthBuffer[0] = '\0';
	m_yearBuffer[0] = '\0';
	m_originalNickname[0] = '\0';

	m_wOptions = WO_SIGNUP;
	m_dobOption = DOBO_START_DATE;
	m_suOption = SUO_NICKNAME;
	m_siOption = SIO_NICK_EMAIL;
	m_rpOption = RPO_EMAIL;




	m_policyManager.Shutdown();
	m_tourManager.Shutdown();

	if(m_emailStatus.Pending())
		rlSocialClub::Cancel(&m_emailStatus);
	m_emailStatus.Reset();

	if(m_nicknameStatus.Pending())
		rlSocialClub::Cancel(&m_nicknameStatus);
	m_nicknameStatus.Reset();

	if(m_alternateNicknameStatus.Pending())
		rlSocialClub::Cancel(&m_alternateNicknameStatus);
	m_alternateNicknameStatus.Reset();

	if(m_passwordStatus.Pending())
		rlSocialClub::Cancel(&m_passwordStatus);
	m_passwordStatus.Reset();

	if(m_netStatus.Pending())
		rlSocialClub::Cancel(&m_netStatus);
	m_netStatus.Reset();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
BaseSocialClubMenu::BaseSocialClubMenu()
	: m_hasPendingNicknameCheck(false)
	, m_buttonFlags(BUTTON_NONE)
	KEYBOARD_MOUSE_ONLY(, m_bLastInputWasKeyboard(true))
{
}

BaseSocialClubMenu::~BaseSocialClubMenu()
{
	sm_flaggedForDeletion = false;
}

void BaseSocialClubMenu::Shutdown()
{
	sm_isActive = false;
	sm_framesUntilDeletion	= 0;
	
	m_acceptPolicies = false;
	m_showDelayedButtonPrompts = 0;
	m_pendingButtons = BUTTON_NONE;
	m_opOption = OPO_ACCEPT;

	m_delayTimer.Zero();
	m_nicknameRequestTimes.Reset();
}

#define FRAME_DELAY_BEFORE_DELETION 3U;
void BaseSocialClubMenu::FlagForDeletion()
{
	// If we're already flagged for deletion, don't reset framesUntilDeletion in cases like url:bugstar:2001838, where script is spamming this every frame
	if(!sm_flaggedForDeletion)
	{
		uiDebugf1("BaseSocialClubMenu::FlagForDeletion");

		sm_flaggedForDeletion = true;
		// Add a very brief frame delay to handle the edge case where we shut down the social club menu and release textures just as scaleform receives the callback to add the texture
		sm_framesUntilDeletion = FRAME_DELAY_BEFORE_DELETION;
	}
}

CScaleformMovieWrapper* BaseSocialClubMenu::GetButtonWrapper()
{
	if(SSocialClubMenu::IsInstantiated())
	{
		return &SSocialClubMenu::GetInstance().m_buttonMovie;
	}
	if(SSocialClubLegalsMenu::IsInstantiated())
	{
		return &SSocialClubLegalsMenu::GetInstance().m_buttonMovie;
	}

	return NULL;
}

void BaseSocialClubMenu::DelayButtonCheck()
{
	if(m_showDelayedButtonPrompts < NUM_FRAMES_DELAY_TO_SHOW_BUTTONS)
	{
		++m_showDelayedButtonPrompts;

		if(m_pendingButtons &&
			m_showDelayedButtonPrompts >= NUM_FRAMES_DELAY_TO_SHOW_BUTTONS)
		{
			SetButtons(m_pendingButtons);
			m_pendingButtons = BUTTON_NONE;
		}
	}
}

void BaseSocialClubMenu::SetButtons(u32 buttonFlags)
{
	if (!m_buttonMovie.IsActive())
	{
		return;
	}

	if(m_showDelayedButtonPrompts < NUM_FRAMES_DELAY_TO_SHOW_BUTTONS)
	{
		m_pendingButtons = static_cast<eButtonFlags>(buttonFlags);
		m_buttonMovie.CallMethod("CLEAR_ALL"); 
	}
	else if(buttonFlags == 0)
	{
		m_buttonMovie.CallMethod("CLEAR_ALL"); 
	}
	else
	{
		m_buttonMovie.CallMethod("SET_CLEAR_SPACE"); 
		m_buttonMovie.CallMethod("SET_DATA_SLOT_EMPTY");

		CMenuButtonList buttonList;

		if(buttonFlags & BUTTON_SELECT)
			AddInstructionalButton(buttonList, INPUT_FRONTEND_ACCEPT, "IB_SELECT" );

		if(buttonFlags & BUTTON_READ)
			AddInstructionalButton(buttonList, INPUT_FRONTEND_ACCEPT, "SC_READ_TOS_BUTTON");

		if(buttonFlags & BUTTON_ACCEPT)
			AddInstructionalButton(buttonList, INPUT_FRONTEND_ACCEPT, "SC_ACCEPT_BUTTON");

		if(buttonFlags & BUTTON_UPLOAD)
			AddInstructionalButton(buttonList, INPUT_FRONTEND_ACCEPT, "IB_UPLOAD_CONFIRM");

		if(buttonFlags & BUTTON_BACK)
			AddInstructionalButton(buttonList, INPUT_FRONTEND_CANCEL, "IB_BACK" );

		if(buttonFlags & BUTTON_CANCEL)
			AddInstructionalButton(buttonList, INPUT_FRONTEND_CANCEL, "IB_CANCEL");

		if(buttonFlags & BUTTON_EXIT)
			AddInstructionalButton(buttonList, INPUT_FRONTEND_CANCEL, "FE_HLP16");

		if(buttonFlags & BUTTON_SUGGEST_NICKNAME)
			AddInstructionalButton(buttonList, INPUT_FRONTEND_Y, "SC_SUGGEST_BUTTON");

		if(buttonFlags & BUTTON_GOTO_SIGNIN)
			AddInstructionalButton(buttonList, INPUT_FRONTEND_X, "SC_SIGNIN_HEADER");

		if(buttonFlags & BUTTON_SCROLL_UPDOWN)
			AddInstructionalButton(buttonList, INPUT_FRONTEND_RIGHT_AXIS_Y, "FE_HLP5");

		if(buttonFlags & BUTTON_LR_TAB)
			AddInstructionalButton(buttonList, INPUTGROUP_FRONTEND_BUMPERS, "IB_TAB");

#if DEBUG_SC_MENU
		if(buttonFlags & BUTTON_AUTOFILL)
			AddInstructionalButton(buttonList, INPUT_FRONTEND_SELECT, "DEBUG_SC_AUTOFILL");
#endif

		buttonList.postLoad();
		buttonList.Draw(&m_buttonMovie);

		m_buttonFlags = buttonFlags;
	}
}

void BaseSocialClubMenu::AddInstructionalButton(CMenuButtonList& buttonList, InputGroup const inputGroup, const char* locLabel)
{
	CMenuButton btn;
	btn.m_ButtonInputGroup = inputGroup;
	btn.m_hButtonHash.SetFromString(locLabel); 

	buttonList.Add(btn);
}

void BaseSocialClubMenu::AddInstructionalButton(CMenuButtonList& buttonList, InputType const inputType, const char* locLabel)
{
	CMenuButton btn;
	btn.m_ButtonInput = inputType;
	btn.m_hButtonHash.SetFromString(locLabel); 

	buttonList.Add(btn);
}

CScaleformMovieWrapper::Param BaseSocialClubMenu::BuildLocTextParam(atHashWithStringBank aLabel, bool html /*= true*/)
{
	return CScaleformMovieWrapper::Param(TheText.Get(aLabel.GetHash(),""), html);
}

CScaleformMovieWrapper::Param BaseSocialClubMenu::BuildLocTextParam(const char* pLabel, bool html /*= true*/)
{
	return CScaleformMovieWrapper::Param(TheText.Get(pLabel), html);
}

CScaleformMovieWrapper::Param BaseSocialClubMenu::BuildNonLocTextParam(const char* pText, bool html /*= true*/)
{
	return CScaleformMovieWrapper::Param(pText, html);
}

void BaseSocialClubMenu::CallMovieWithPagedText(const char* pMovie, const PagedCloudText& rPagedText)
{
	if(m_scMovie.BeginMethod(pMovie))
	{
		for(int i=0; i<rPagedText.GetNumPages(); ++i)
		{
			CScaleformMgr::AddParamString((char*)rPagedText.GetPageOfText(i).ToString(), rPagedText.GetPageOfText(i).Length()+200, true);
		}
		m_scMovie.EndMethod();
	}
}

void BaseSocialClubMenu::CheckScroll()
{
	int iYAxis  = static_cast<s32>(CControlMgr::GetMainFrontendControl().GetFrontendRStickUpDown().GetNorm() * 127.5f);

	uiDebugf3("BaseSocialClubMenu::CheckScroll - iYAxis=%d", iYAxis);

	if(iYAxis < -CPauseMenu::FRONTEND_ANALOGUE_THRESHOLD)
	{
		m_scMovie.CallMethod("SCROLL_POLICY_TEXT", SO_UP);
	}
	else if(CPauseMenu::FRONTEND_ANALOGUE_THRESHOLD < iYAxis)
	{
		m_scMovie.CallMethod("SCROLL_POLICY_TEXT", SO_DOWN);
	}
}

void BaseSocialClubMenu::SetBusySpinner(bool isVisible)
{
	if(isVisible)
	{
		m_buttonMovie.CallMethod("SET_SAVING_TEXT", CSavingGameMessage::SAVEGAME_ICON_STYLE_LOADING, "");
	}
	else
	{
		m_buttonMovie.CallMethod("REMOVE_SAVING");
	}
}

void BaseSocialClubMenu::AcceptPolicyVersions(bool cloudVersion)
{
	m_scMovie.CallMethod("SET_SOCIAL_CLUB_PRESENCE_ACTIVE", CLiveManager::GetSocialClubMgr().GetSocialClubInfo().GetNickname());

	SPolicyVersions::GetInstance().AcceptPolicy(cloudVersion);
	CProfileSettings::GetInstance().Write(); // To save the profile settings of the accepted version.
}

#if KEYBOARD_MOUSE_SUPPORT
void SocialClubMenu::CheckMouseEvent()
{
	if(CMousePointer::HasSFMouseEvent(m_scMovie.GetMovieID()))
	{
		sScaleformMouseEvent evt = CMousePointer::GetLastMouseEvent();

		eMOUSE_EVT evtType = evt.evtType;

		switch(evtType)
		{
		case EVENT_TYPE_MOUSE_ROLL_OVER:
			m_opOption = static_cast<eOnlinePolicyOptions>(evt.iUID);
			UpdatePolicyHighlight();
			break;
		case EVENT_TYPE_MOUSE_RELEASE:
			SelectCurrentOption();
			break;
		default:
			break;
		}
	}
}
#endif // KEYBOARD_MOUSE_SUPPORT

bool BaseSocialClubMenu::UpdateOption(int& value, int maxValue, const bool* pValidOptions /*= NULL*/)
{
	int dir = 0;
	int oldValue = value;
	--maxValue;

	if(CheckInput(FRONTEND_INPUT_UP, false) || CheckInput(FRONTEND_INPUT_LEFT, false))
	{
		--dir;
	}
	else if(CheckInput(FRONTEND_INPUT_DOWN, false) || CheckInput(FRONTEND_INPUT_RIGHT, false))
	{
		++dir;
	}

	if(dir != 0)
	{
		CPauseMenu::PlayInputSound(FRONTEND_INPUT_UP);

		if(pValidOptions)
		{
			do 
			{
				value += dir;
				value = rage::Wrap(value, 0, maxValue);
			} while (!pValidOptions[value] && value != oldValue);
		}
		else
		{
			value += dir;
			value = rage::Wrap(value, 0, maxValue);
		}
	}

	return value != oldValue;
}

void BaseSocialClubMenu::Update()
{
#if KEYBOARD_MOUSE_SUPPORT
	// detect if we need to cycle instructional buttons
	if( m_bLastInputWasKeyboard != CControlMgr::GetPlayerMappingControl().WasKeyboardMouseLastKnownSource() )
	{
		m_bLastInputWasKeyboard = CControlMgr::GetPlayerMappingControl().WasKeyboardMouseLastKnownSource();
		SetButtons(m_buttonFlags); // Redraw our current buttons so Scaleform picks up the input device change
	}
#endif
}

void BaseSocialClubMenu::UpdateError()
{
	uiDebugf3("BaseSocialClubMenu::UpdateError");
	m_errorMessage.Update();
}


bool BaseSocialClubMenu::CheckInput(eFRONTEND_INPUT input, bool playSound /*=true*/)
{
	// Since this is a full screen menu, always check for disabled input in case Script is disabling
	if(CPauseMenu::CheckInput(input, playSound, CHECK_INPUT_OVERRIDE_FLAG_STORAGE_DEVICE, false, false, true))
	{
		uiDebugf1("BaseSocialClubMenu::CheckInput - Input (%d) pressed.", input);
		return true;
	}

	return false;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
SocialClubLegalsMenu::SocialClubLegalsMenu() 
{
	Shutdown();

	const char* pMovie = "ONLINE_POLICIES";
	
	m_scMovie.CreateMovieAndWaitForLoad(SF_BASE_CLASS_GENERIC, pMovie, Vector2(0.0f, 0.0f), Vector2(1.0f, 1.0f));
	CScaleformMgr::ForceMovieUpdateInstantly(m_scMovie.GetMovieID(), true);


	m_buttonMovie.CreateMovieAndWaitForLoad(SF_BASE_CLASS_GENERIC, SC_BUTTON_MOVIE_FILENAME, Vector2(0.0f, 0.0f), Vector2(1.0f, 1.0f));
	CScaleformMgr::ForceMovieUpdateInstantly(m_buttonMovie.GetMovieID(), true);
}

SocialClubLegalsMenu::~SocialClubLegalsMenu()
{
	Shutdown();

	m_buttonMovie.Clear();
}

void SocialClubLegalsMenu::Init()
{
	sm_isActive = true;

	if(!SPolicyVersions::GetInstance().IsInitialized())
	{
		uiDisplayf("SocialClubLegalsMenu::Init - Policy Versions aren't initialized.");
		SPolicyVersions::GetInstance().Init();
	}

	m_buttonMovie.CallMethod("CONSTRUCTION_INNARDS");

	const rlGamerInfo* pGamerInfo = NetworkInterface::GetActiveGamerInfo();

	if(!pGamerInfo || !pGamerInfo->IsSignedIn() || !pGamerInfo->IsOnline())
	{
		SocialClubMenu::FlagForDeletion();
	}
	else
	{
		m_policyManager.Init(pGamerInfo->GetLocalIndex(), NetworkUtils::GetLanguageCode());
		GoToState(SC_DOWNLOADING);
	}
}

void SocialClubLegalsMenu::Shutdown()
{
	BaseSocialClubMenu::Shutdown();
	m_state = SC_IDLE;
}

void SocialClubLegalsMenu::Render()
{
	if(m_state != SC_IDLE)
	{
		m_scMovie.Render();

#if RSG_PC
		// hide instructional buttons if SCUI is showing ...hints are confusing
		if (!g_rlPc.IsUiShowing())
#endif
		{
			m_buttonMovie.Render();
		}

	}
}

void SocialClubLegalsMenu::Update()
{
	if(m_state == SC_IDLE)
	{
		return;
	}

	BaseSocialClubMenu::Update();

	DelayButtonCheck();

	m_policyManager.Update();

	if(!IsFlaggedForDeletion())
	{
		switch(m_state)
		{
		case SC_IDLE:
			break;
		case SC_DOWNLOADING:
			UpdateDownloading();
			break;
		case SC_ONLINE_POLICIES:
			UpdateOnlinePolicies();
			break;
		case SC_EULA:
		case SC_PP:
		case SC_TOS:
			UpdateDocs();
			break;
		case SC_DECLINE_WARNING:
			break;
		case SC_DOWNLOAD_ERROR:
			UpdateDownloadError();
			break;
		case SC_ERROR:
			UpdateError();
			break;
		};
	}
}

void SocialClubLegalsMenu::UpdateDownloading()
{
	uiDebugf3("SocialClubLegalsMenu::UpdateDownloading, timerComplete=%d, policiesDownloaded=%d, versionsDownloaded=%d", m_delayTimer.IsComplete(SC_MIN_ACCEPT_TIME), !m_policyManager.IsDownloading(), !SPolicyVersions::GetInstance().IsDownloading());

	if(CheckInput(FRONTEND_INPUT_BACK))
	{
		m_policyManager.Shutdown();
		FlagForDeletion();
	}
	else if(m_delayTimer.IsComplete(SC_MIN_ACCEPT_TIME) && !m_policyManager.IsDownloading() && !SPolicyVersions::GetInstance().IsDownloading())
	{
		m_delayTimer.Zero();

		bool isUpToDate = SPolicyVersions::GetInstance().IsUpToDate();
#if __BANK
		if(!CLiveManager::GetSocialClubMgr().GetUpToDateToggleState())
		{
			isUpToDate = false;
		}
#endif

		if(m_policyManager.GetError())
		{
			GoToState(SC_DOWNLOAD_ERROR);
		}
		else if(isUpToDate)
		{
			FlagForDeletion();
		}
		else
		{
			GoToState(SC_ONLINE_POLICIES);
		}
	}
}

void SocialClubLegalsMenu::UpdateOnlinePolicies()
{
	uiDebugf3("SocialClubLegalsMenu::UpdateOnlinePolicies");

	if(CheckInput(FRONTEND_INPUT_ACCEPT))
	{
		SelectCurrentOption();
	}
	else if(CheckInput(FRONTEND_INPUT_BACK))
	{
		GoToState(SC_DECLINE_WARNING);
	}


	bool validStates[OPO_MAX] = {true};
	memset(validStates, true, sizeof(validStates));
	validStates[OPO_SUBMIT] = m_acceptPolicies;

	int selectedOption = static_cast<int>(m_opOption);
	if(UpdateOption(selectedOption, OPO_MAX, validStates))
	{
		m_opOption = static_cast<eOnlinePolicyOptions>(selectedOption);
		UpdatePolicyHighlight();
	}

}

void SocialClubLegalsMenu::UpdatePolicyHighlight()
{
	m_scMovie.CallMethod("SET_ONLINE_POLICY_LINK_1_HIGHLIGHT", m_opOption == OPO_EULA);
	m_scMovie.CallMethod("SET_ONLINE_POLICY_LINK_2_HIGHLIGHT", m_opOption == OPO_PP);
	m_scMovie.CallMethod("SET_ONLINE_POLICY_LINK_3_HIGHLIGHT", m_opOption == OPO_TOS);
	m_scMovie.CallMethod("SET_ONLINE_POLICY_ACCEPT_HIGHLIGHT", m_opOption == OPO_ACCEPT);
	m_scMovie.CallMethod("SET_ONLINE_POLICY_SUBMIT_HIGHLIGHT", m_acceptPolicies, m_opOption == OPO_SUBMIT);
}

void SocialClubLegalsMenu::SelectCurrentOption()
{
	switch(m_opOption)
	{
	case OPO_EULA:
		{
			GoToEula();
		}
		break;
	case OPO_PP:
		{
			GoToPrivacyPolicy();
		}
		break;
	case OPO_TOS:
		{
			GoToTOS();
		}
		break;
	case OPO_ACCEPT:
		{
			m_acceptPolicies = !m_acceptPolicies;
			m_scMovie.CallMethod("SET_ONLINE_POLICY_ACCEPT_RADIO_BUTTON_STATE", m_acceptPolicies);
			m_scMovie.CallMethod("SET_ONLINE_POLICY_SUBMIT_HIGHLIGHT", m_acceptPolicies, false);
		}
		break;
	case OPO_SUBMIT:
		{
			AcceptPolicyVersions(m_policyManager.AreTextsInSyncWithCloud());
			CLiveManager::GetSocialClubMgr().SetOnlinePolicyStatus(true);
			SocialClubMenu::FlagForDeletion();
		}
		break;
	default:
		uiAssertf(0, "SocialClubLegalsMenu::UpdateOnlinePolicies - Unhandled menu option %d", m_opOption);
		break;
	}
}

void SocialClubLegalsMenu::UpdateDocs()
{
	uiDebugf3("SocialClubLegalsMenu::UpdateDocs");

	if(CheckInput(FRONTEND_INPUT_BACK))
	{
		GoToState(SC_ONLINE_POLICIES);
	}

	CheckScroll();
}

void SocialClubLegalsMenu::AcceptBackoutWarning()
{
	uiDebugf3("SocialClubLegalsMenu::AcceptBackoutWarning");
	FlagForDeletion();
}

void SocialClubLegalsMenu::DeclineBackoutWarning()
{
	uiDebugf3("SocialClubLegalsMenu::DeclineBackoutWarning");

	if(m_policyManager.GetError())
	{
		GoToState(SC_DOWNLOAD_ERROR);
	}
	else if(m_policyManager.IsDownloading())
	{
		GoToState(SC_DOWNLOADING);
	}
	else
	{
		GoToState(SC_ONLINE_POLICIES);
	}
}

void SocialClubLegalsMenu::UpdateDownloadError()
{
	uiDebugf3("SocialClubLegalsMenu::UpdateDownloadError");

	if(CheckInput(FRONTEND_INPUT_ACCEPT))
	{
		FlagForDeletion();
	}
}

#if KEYBOARD_MOUSE_SUPPORT
void SocialClubLegalsMenu::UpdateMouseEvent(const GFxValue* args)
{
	const int SF_TYPE_PARAM = 2;
	//const int SF_NAME_PARAM = 3; // This is the name of the movie clip that triggered the event.  We can ignore this
	const int SF_OPTION_PARAM = 4;

	if (uiVerifyf(args[SF_TYPE_PARAM].IsNumber(), "SocialClubLegalsMenu::UpdateMouseEvent SF_TYPE_PARAM not compatible: %s", sfScaleformManager::GetTypeName(args[SF_TYPE_PARAM])) &&
		uiVerifyf(args[SF_OPTION_PARAM].IsNumber(), "SocialClubLegalsMenu::UpdateMouseEvent SF_OPTION_PARAM not compatible: %s", sfScaleformManager::GetTypeName(args[SF_OPTION_PARAM])) &&
		uiVerify(args[SF_TYPE_PARAM].GetNumber() < EVENT_TYPE_MOUSE_MAX) &&		
		uiVerify(args[SF_OPTION_PARAM].GetNumber() < OPO_MAX))
	{
		eMOUSE_EVT evtType = static_cast<eMOUSE_EVT>((int)args[SF_TYPE_PARAM].GetNumber());

		switch(evtType)
		{
			case EVENT_TYPE_MOUSE_ROLL_OVER:
				m_opOption = static_cast<eOnlinePolicyOptions>((int)args[SF_OPTION_PARAM].GetNumber());
				UpdatePolicyHighlight();
				break;
			case EVENT_TYPE_MOUSE_RELEASE:
				SelectCurrentOption();
				break;
			default:
				break;
		}
	}
}
#endif // KEYBOARD_MOUSE_SUPPORT

void SocialClubLegalsMenu::GoToState(eSCLegalStates newState)
{
	uiDebugf1("SocialClubLegalsMenu::GoToState - Old State=%d, New State=%d.", m_state, newState);
	SetBusySpinner(false);

	switch(newState)
	{
		case SC_IDLE:
			break;
		case SC_DOWNLOADING:
			GoToDownloading();
			break;
		case SC_ONLINE_POLICIES:
			GoToOnlinePolicies();
			break;
		case SC_EULA:
			GoToEula();
			break;
		case SC_PP:
			GoToPrivacyPolicy();
			break;
		case SC_TOS:
			GoToTOS();
			break;
		case SC_DECLINE_WARNING:
			GoToDeclineWarning();
			break;
		case SC_DOWNLOAD_ERROR:
			GoToDownloadError();
			break;
		case SC_ERROR:
			uiAssertf(0, "SocialClubMenu::GoToState - Going to State Error, but this state needs extra data and setup.");
			//GoToError();
			break;
	};
}

void SocialClubLegalsMenu::GoToDownloading()
{
	uiDebugf1("SocialClubLegalsMenu::GoToDownloading");
	SetBusySpinner(true);

	m_state = SC_DOWNLOADING;

	m_scMovie.CallMethod("DISPLAY_POLICY_UPDATE");
	m_scMovie.CallMethod("SET_POLICY_UPDATE_TITLE", BuildNonLocTextParam("ONLINE_POLICIES_TITLE"));
	m_scMovie.CallMethod("SET_POLICY_UPDATE_TEXT", BuildNonLocTextParam("ONLINE_POLICY_SYNC"));

	m_delayTimer.Start();
	SetButtons(BUTTON_CANCEL);
}

void SocialClubLegalsMenu::GoToOnlinePolicies()
{
	uiDebugf1("SocialClubLegalsMenu::GoToOnlinePolicies");

	m_state = SC_ONLINE_POLICIES;

	m_scMovie.CallMethod("DISPLAY_ONLINE_POLICY");

	m_scMovie.CallMethod("SET_ONLINE_POLICY_LINK_1_HIGHLIGHT", m_opOption == OPO_EULA);
	m_scMovie.CallMethod("SET_ONLINE_POLICY_LINK_2_HIGHLIGHT", m_opOption == OPO_PP);
	m_scMovie.CallMethod("SET_ONLINE_POLICY_LINK_3_HIGHLIGHT", m_opOption == OPO_TOS);
	m_scMovie.CallMethod("SET_ONLINE_POLICY_ACCEPT_HIGHLIGHT", m_opOption == OPO_ACCEPT);
	m_scMovie.CallMethod("SET_ONLINE_POLICY_ACCEPT_RADIO_BUTTON_STATE", m_acceptPolicies);
	m_scMovie.CallMethod("SET_ONLINE_POLICY_SUBMIT_HIGHLIGHT", m_acceptPolicies, m_opOption == OPO_SUBMIT);

	SetButtons(BUTTON_SELECT | BUTTON_BACK);
}

void SocialClubLegalsMenu::GoToEula()
{
	uiDebugf1("SocialClubLegalsMenu::GoToEula");

	m_state = SC_PP;

	m_scMovie.CallMethod("DISPLAY_DOWNLOADED_POLICY");
	m_scMovie.CallMethod("SET_POLICY_TITLE", BuildNonLocTextParam("ONLINE_POLICY_EULA_TITLE"));
	CallMovieWithPagedText("SET_POLICY_TEXT", m_policyManager.GetFile(SCF_EULA));
	m_scMovie.CallMethod("SCROLL_POLICY_TEXT", SO_TOP);

	SetButtons(BUTTON_BACK | BUTTON_SCROLL_UPDOWN);
}

void SocialClubLegalsMenu::GoToPrivacyPolicy()
{
	uiDebugf1("SocialClubLegalsMenu::GoToPrivacyPolicy");

	m_state = SC_PP;

	m_scMovie.CallMethod("DISPLAY_DOWNLOADED_POLICY");
	m_scMovie.CallMethod("SET_POLICY_TITLE", BuildNonLocTextParam("ONLINE_POLICY_PP_TITLE"));
	CallMovieWithPagedText("SET_POLICY_TEXT", m_policyManager.GetFile(SCF_PP));
	m_scMovie.CallMethod("SCROLL_POLICY_TEXT", SO_TOP);

	SetButtons(BUTTON_BACK | BUTTON_SCROLL_UPDOWN);
}

void SocialClubLegalsMenu::GoToTOS()
{
	uiDebugf1("SocialClubLegalsMenu::GoToTOS");

	m_state = SC_TOS;

	m_scMovie.CallMethod("DISPLAY_DOWNLOADED_POLICY");
	m_scMovie.CallMethod("SET_POLICY_TITLE", BuildNonLocTextParam("ONLINE_POLICY_TOS_TITLE"));
	CallMovieWithPagedText("SET_POLICY_TEXT", m_policyManager.GetFile(SCF_TOS));
	m_scMovie.CallMethod("SCROLL_POLICY_TEXT", SO_TOP);

	SetButtons(BUTTON_BACK | BUTTON_SCROLL_UPDOWN);
}

void SocialClubLegalsMenu::GoToDeclineWarning()
{
	uiDebugf1("SocialClubLegalsMenu::GoToDeclineWarning");

	m_state = SC_ERROR;

	m_errorMessageData.m_TextLabelHeading = "SC_WARNING_TITLE";
	m_errorMessageData.m_TextLabelBody = "ONLINE_POLICY_WARNING";
	m_errorMessageData.m_iFlags = static_cast<eWarningButtonFlags>(FE_WARNING_OK_CANCEL);
	m_errorMessageData.m_acceptPressed = datCallback(MFA(SocialClubLegalsMenu::AcceptBackoutWarning), (datBase*)this);
	m_errorMessageData.m_declinePressed = datCallback(MFA(SocialClubLegalsMenu::DeclineBackoutWarning), (datBase*)this);
	
	m_errorMessage.SetMessage(m_errorMessageData);
}

void SocialClubLegalsMenu::GoToDownloadError()
{
	uiDebugf1("SocialClubLegalsMenu::GoToDownloadError");

	m_state = SC_DOWNLOAD_ERROR;

	m_scMovie.CallMethod("DISPLAY_POLICY_UPDATE");
	m_scMovie.CallMethod("SET_POLICY_UPDATE_TITLE", BuildNonLocTextParam("SC_WARNING_TITLE"));
	m_scMovie.CallMethod("SET_POLICY_UPDATE_TEXT", BuildNonLocTextParam("ONLINE_POLICY_DL_FAILED"));

	SetButtons(BUTTON_SELECT);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
void SocialClubMenu::Update()
{
	if(m_state == SC_IDLE)
	{
		return;
	}

	BaseSocialClubMenu::Update();

	// If the virtual keyboard was open, and is now closed.
	if(CControlMgr::UpdateVirtualKeyboard() != ioVirtualKeyboard::kKBState_PENDING && m_isKeyboardOpen)
	{
		m_isKeyboardOpen = false;

		if(CControlMgr::GetVirtualKeyboardState() == ioVirtualKeyboard::kKBState_SUCCESS)
		{
			m_keyboardCallback.Call();
			m_keyboardCallback = NullCallback;
		}
	}

	DelayButtonCheck();

	m_policyManager.Update();
	m_tourManager.Update();
	m_nicknameStatus.Update();
	m_passwordStatus.Update();
	m_emailStatus.Update();
	m_netStatus.Update();
	m_alternateNicknameStatus.Update();

	if(!IsFlaggedForDeletion())
	{
		switch(m_state)
		{
		case SC_IDLE:
			break;
		case SC_DOWNLOADING:
			UpdateDownloading();
			break;
		case SC_WELCOME:
			UpdateWelcome();
			break;
#if SC_COLLECT_AGE
		case SC_DOB:
			UpdateDOB();
			break;
#endif
		case SC_ONLINE_POLICIES:
			UpdateOnlinePolicies();
			break;
		case SC_EULA:
			UpdateEula();
			break;
		case SC_PP:
			UpdatePrivacyPolicy();
			break;
		case SC_TOS:
			UpdateTOS();
			break;
		case SC_SIGNUP:
			UpdateSignUp();
			break;
		case SC_SIGNUP_CONFIRM:
			UpdateSignUpConfirm();
			break;
		case SC_SIGNING_UP:
			UpdateSigningUp();
			break;
		case SC_SIGNUP_DONE:
			UpdateSignUpDone();
			break;
		case SC_SIGNIN:
			UpdateSignIn();
			break;
		case SC_SIGNING_IN:
			UpdateSigningIn();
			break;
		case SC_SIGNIN_DONE:
			UpdateSignInDone();
			break;
		case SC_RESET_PASSWORD:
			UpdateResetPassword();
			break;
		case SC_RESETTING_PASSWORD:
			UpdateResettingPassword();
			break;
		case SC_RESET_PASSWORD_DONE:
			UpdateResetPasswordDone();
			break;
		case SC_POLICY_UPDATE_ACCEPTED:
			UpdatePolicyUpdateAccepted();
			break;
		case SC_AUTH_SIGNUP:
			UpdateScAuthSignup();
			break;
		case SC_AUTH_SIGNUP_DONE:
			UpdateScAuthSignupDone();
			break;
		case SC_AUTH_SIGNIN:
			UpdateScAuthSignin();
			break;
		case SC_AUTH_SIGNIN_DONE:
			UpdateScAuthSigninDone();
			break;
		case SC_ERROR:
			UpdateError();
			break;
		}
	}
}

void SocialClubMenu::UpdateDownloading()
{
	uiDebugf3("SocialClubMenu::UpdateDownloading, timerComplete=%d, policiesDownloaded=%d, toursDownloaded=%d, versionsDownloaded=%d", m_delayTimer.IsComplete(SC_MIN_ACCEPT_TIME), !m_policyManager.IsDownloading(), !m_tourManager.IsDownloading(), !SPolicyVersions::GetInstance().IsDownloading());

	if(CheckInput(FRONTEND_INPUT_BACK))
	{
		m_policyManager.Shutdown();
		m_tourManager.Shutdown();
		FlagForDeletion();
	}
	else if(m_delayTimer.IsComplete(SC_MIN_ACCEPT_TIME) && !m_policyManager.IsDownloading() && !m_tourManager.IsDownloading() && !SPolicyVersions::GetInstance().IsDownloading())
	{
		m_delayTimer.Zero();

		if(m_policyManager.GetError())
		{
			SetGenericError(m_policyManager.GetError(), m_policyManager.GetErrorTitle());
		}
		else if(m_tourManager.HasError())
		{
			SetGenericError(m_tourManager.GetError(), m_tourManager.GetErrorTitle());
		}
		else if(CLiveManager::GetSocialClubMgr().IsConnectedToSocialClub())
		{
			m_isPolicyUpdateFlow = true;
			GoToState(SC_ONLINE_POLICIES);
		}
		else
		{
			GoToState(SC_WELCOME);
		}
	}
}

void SocialClubMenu::UpdateWelcome()
{
	uiDebugf3("SocialClubMenu::UpdateWelcome");

	if(CheckInput(FRONTEND_INPUT_BACK))
	{
		FlagForDeletion();
	}
	else if(CheckInput(FRONTEND_INPUT_ACCEPT))
	{
		if (rlScAuth::IsEnabled())
		{
			if (m_wOptions == WO_SIGNUP)
			{
				m_isInSignInFlow = false;
				GoToState(SC_AUTH_SIGNUP);
			}
			else
			{
				m_isInSignInFlow = true;
				GoToState(SC_AUTH_SIGNIN);
			}
		}
		else if(m_wOptions == WO_SIGNUP)
		{
			m_isInSignInFlow = false;
			GoToState(SC_DOB);
		}
		else
		{
			m_isInSignInFlow = true;
			GoToState(SC_ONLINE_POLICIES);
		}
	}
	else
	{
		bool lbCheck = CheckInput(FRONTEND_INPUT_LB);
		if(lbCheck || CheckInput(FRONTEND_INPUT_RB))
		{
			if(0 < m_tourManager.NumTours())
			{
				m_welcomeTab += lbCheck ? -1 : 1;
				SetWelcomeTab(static_cast<s8>(rage::Wrap(static_cast<int>(m_welcomeTab), 0, m_tourManager.NumTours()-1)));
			}
		}
	}

	CheckAutoFill();


	int selectedOption = static_cast<int>(m_wOptions);
	if(UpdateOption(selectedOption, WO_MAX))
	{
		m_wOptions = static_cast<eWelcomOptions>(selectedOption);

		m_scMovie.CallMethod("SET_WELCOME_JOIN_HIGHLIGHT", m_wOptions == WO_SIGNUP);
		m_scMovie.CallMethod("SET_WELCOME_SIGN_IN_HIGHLIGHT", m_wOptions == WO_SIGNIN);
	}
}

#if SC_COLLECT_AGE
void SocialClubMenu::UpdateDOB()
{
	uiDebugf3("SocialClubMenu::UpdateDOB");

	if(CheckInput(FRONTEND_INPUT_ACCEPT))
	{
		switch(m_dobOption)
		{
		case DOBO_DAY:
			{
				SetupDateKeyboard("SC_DAY", m_dayBuffer, NUM_DAY_CHARACTERS);
			}
			break;
		case DOBO_MONTH:
			{
				SetupDateKeyboard("MONTH_0", m_monthBuffer, NUM_MONTH_CHARACTERS);
			}
			break;
		case DOBO_YEAR:
			{
				SetupDateKeyboard("SC_YEAR", m_yearBuffer, NUM_YEAR_CHARACTERS);
			}
			break;
		case DOBO_SUBMIT:
			{
				CLiveManager::GetSocialClubMgr().GetSocialClubInfo().SetAge(m_day, m_month, m_year);

				if(!SocialClubMgr::IsDateValid(m_day, m_month, m_year)) // How did we get here?
				{
					SetGenericWarning("SC_ERROR_INVALID_AGE");
				}
				else if(!CLiveManager::GetSocialClubMgr().GetSocialClubInfo().IsOldEnoughToJoinSC())
				{
					sm_isTooYoung = true;
					SetGenericError("SC_ERROR_INVALID_ACCOUNT_AGE", "SC_WARNING_TITLE");
				}
				else
				{
					GoToNext();
				}
			}
			break;
		default:
			uiAssertf(0, "SocialClubMenu::UpdateDOB - Unhandled menu option %d", m_dobOption);
			break;
		}
	}
	else if(CheckInput(FRONTEND_INPUT_BACK))
	{
		GoToPrevious();
	}
	else if(CheckInput(FRONTEND_INPUT_X))
	{
		m_isInSignInFlow = true;
		GoToState(SC_EULA);
	}

	CheckAutoFill();


	bool validStates[DOBO_MAX] = {true};
	memset(validStates, true, sizeof(validStates));
	validStates[DOBO_SUBMIT] = SocialClubMgr::IsDateValid(m_day, m_month, m_year);

	int selectedOption = static_cast<int>(m_dobOption);
	if(UpdateOption(selectedOption, DOBO_MAX, validStates))
	{
		m_dobOption = static_cast<eDOBOptions>(selectedOption);

		if(SocialClubMgr::IsDateValid(m_day, m_month, m_year))
		{
			m_scMovie.CallMethod("SET_DOB_SUBMIT_HIGHLIGHT", m_dobOption == DOBO_SUBMIT);
		}
		else
		{
			m_scMovie.CallMethod("SET_DOB_SUBMIT_DISABLED");
		}

		m_scMovie.CallMethod("SET_DOB_HIGHLIGHT", m_dobOption == DOBO_MONTH, DOBO_MONTH - DOBO_START_DATE);
		m_scMovie.CallMethod("SET_DOB_HIGHLIGHT", m_dobOption == DOBO_DAY, DOBO_DAY - DOBO_START_DATE);
		m_scMovie.CallMethod("SET_DOB_HIGHLIGHT", m_dobOption == DOBO_YEAR, DOBO_YEAR - DOBO_START_DATE);
	}
}
#endif

void SocialClubMenu::UpdateOnlinePolicies()
{
	uiDebugf3("SocialClubMenu::UpdateOnlinePolicies");

	KEYBOARD_MOUSE_ONLY( SocialClubMenu::CheckMouseEvent(); )

	if(CheckInput(FRONTEND_INPUT_ACCEPT))
	{
		SelectCurrentOption();
	}
	else if(CheckInput(FRONTEND_INPUT_BACK))
	{
		if(m_isPolicyUpdateFlow)
		{
			FlagForDeletion();
		}
		else if(m_isInSignInFlow)
		{
			GoToState(SC_WELCOME);
		}
		else
		{
			GoToState(SC_DOB);
		}
	}


	bool validStates[OPO_MAX] = {true};
	memset(validStates, true, sizeof(validStates));
	validStates[OPO_SUBMIT] = m_acceptPolicies;

	int selectedOption = static_cast<int>(m_opOption);
	if(UpdateOption(selectedOption, OPO_MAX, validStates))
	{
		m_opOption = static_cast<eOnlinePolicyOptions>(selectedOption);

		UpdatePolicyHighlight();
		uiDebugf3("SocialClubMenu::Update Policies.  m_acceptPolicies: %d, m_opOption == OPO_SUBMIT: %d", m_acceptPolicies, m_opOption == OPO_SUBMIT);
	}
}

bool SocialClubMenu::IsIdleWrapper()
{
    bool result = true;

    if(SSocialClubMenu::IsInstantiated())
    {
        result = SSocialClubMenu::GetInstance().IsIdle();
    }

    return result;
}

void SocialClubMenu::UpdatePolicyHighlight()
{
	m_scMovie.CallMethod("SET_ONLINE_POLICY_LINK_1_HIGHLIGHT", m_opOption == OPO_EULA);
	m_scMovie.CallMethod("SET_ONLINE_POLICY_LINK_2_HIGHLIGHT", m_opOption == OPO_PP);
	m_scMovie.CallMethod("SET_ONLINE_POLICY_LINK_3_HIGHLIGHT", m_opOption == OPO_TOS);
	m_scMovie.CallMethod("SET_ONLINE_POLICY_ACCEPT_HIGHLIGHT", m_opOption == OPO_ACCEPT);
	m_scMovie.CallMethod("SET_ONLINE_POLICY_SUBMIT_HIGHLIGHT", m_acceptPolicies, m_opOption == OPO_SUBMIT);
}

void SocialClubMenu::SelectCurrentOption()
{
	switch(m_opOption)
	{
	case OPO_EULA:
		{
			GoToEula();
		}
		break;
	case OPO_PP:
		{
			GoToPrivacyPolicy();
		}
		break;
	case OPO_TOS:
		{
			GoToTOS();
		}
		break;
	case OPO_ACCEPT:
		{
			m_acceptPolicies = !m_acceptPolicies;
			m_scMovie.CallMethod("SET_ONLINE_POLICY_ACCEPT_RADIO_BUTTON_STATE", m_acceptPolicies);
			m_scMovie.CallMethod("SET_ONLINE_POLICY_SUBMIT_HIGHLIGHT", m_acceptPolicies, false);
		}
		break;
	case OPO_SUBMIT:
		{
			if(m_isPolicyUpdateFlow)
			{
				AcceptPolicyVersions(m_policyManager.AreTextsInSyncWithCloud());
				GoToState(SC_POLICY_UPDATE_ACCEPTED);
			}
			else if(m_isInSignInFlow)
			{
				GoToState(SC_SIGNIN);
			}
			else
			{
				GoToState(SC_SIGNUP);
			}
		}
		break;
	default:
		uiAssertf(0, "SocialClubMenu::UpdateOnlinePolicies - Unhandled menu option %d", m_opOption);
		break;
	}
}

void SocialClubMenu::UpdateEula()
{
	uiDebugf3("SocialClubMenu::UpdateEula");

	if(CheckInput(FRONTEND_INPUT_ACCEPT) || CMousePointer::IsSFMouseReleased(m_scMovie.GetMovieID(), 0))
	{
		GoToState(SC_ONLINE_POLICIES);
	}

	CheckScroll();
}

void SocialClubMenu::UpdatePrivacyPolicy()
{
	uiDebugf3("SocialClubMenu::UpdatePrivacyPolicy");

	if(CheckInput(FRONTEND_INPUT_ACCEPT) || CMousePointer::IsSFMouseReleased(m_scMovie.GetMovieID(), 0))
	{
		GoToState(SC_ONLINE_POLICIES);
	}

	CheckScroll();
}

void SocialClubMenu::UpdateTOS()
{
	uiDebugf3("SocialClubMenu::UpdateTOS");

	if(CheckInput(FRONTEND_INPUT_ACCEPT) || CMousePointer::IsSFMouseReleased(m_scMovie.GetMovieID(), 0))
	{
		GoToState(SC_ONLINE_POLICIES);
	}

	CheckScroll();
}

void SocialClubMenu::UpdateSignUp()
{
	uiDebugf3("SocialClubMenu::UpdateSignUp");

	if(CheckInput(FRONTEND_INPUT_ACCEPT))
	{
		switch(m_suOption)
		{
		case SUO_NICKNAME:
			{
				SetupNicknameKeyboard();
			}
			break;
		case SUO_EMAIL:
			{
				SetupEmailKeyboard();
			}
			break;
		case SUO_PASSWORD:
			{
				SetupPasswordKeyboard();
			}
			break;
		case SUO_SPAM:
			{
				m_allowEmailSpam = !m_allowEmailSpam;
				m_scMovie.CallMethod("SET_SIGN_UP_RADIO_BUTTON_STATE", m_allowEmailSpam);
				CLiveManager::GetSocialClubMgr().GetSocialClubInfo().SetSpam(m_allowEmailSpam);
			}
			break;
		case SUO_SUBMIT:
			{
				GoToNext();
			}
			break;
		default:
			uiAssertf(0, "SocialClubMenu::UpdateSignup - Unhandled menu option %d", m_suOption);
			break;
		}
	}
	else if(CheckInput(FRONTEND_INPUT_BACK))
	{
		GoToState(SC_ONLINE_POLICIES);
	}
	else if(CheckInput(FRONTEND_INPUT_Y))
	{
		SuggestNickname(false);
	}
	else if(CheckInput(FRONTEND_INPUT_X))
	{
		m_isInSignInFlow = true;
		GoToState(SC_SIGNIN);
	}

	if(m_nicknameRequestTimes.GetCount() > 0 && m_nicknameRequestTimes.Top() + NICKNAME_CHECK_COOLDOWN_TIME <= fwTimer::GetSystemTimeInMilliseconds())
	{
		m_nicknameRequestTimes.Pop();

		// We've exceeded the allowed nick name checks within the allowed time period.  The timer is up now, so it's safe to check the last nickname entered
		if(m_hasPendingNicknameCheck)
		{
			m_hasPendingNicknameCheck = false;
			// Validate the last set nickname
			ValidateNickname(CLiveManager::GetSocialClubMgr().GetSocialClubInfo().GetNickname());
		}
	}

	CheckAutoFill();

	bool validStates[SUO_MAX] = {true};
	bool canSubmit = m_emailStatus.Succeeded() && m_nicknameStatus.Succeeded() && m_passwordStatus.Succeeded();
	memset(validStates, true, sizeof(validStates));
	validStates[SUO_SPAM] = m_isSpammable;
	validStates[SUO_SUBMIT] = canSubmit;

	int selectedOption = static_cast<int>(m_suOption);
	if(UpdateOption(selectedOption, SUO_MAX, validStates))
	{
		m_suOption = static_cast<eSignUpOptions>(selectedOption);

		m_scMovie.CallMethod("SET_SIGN_UP_NICKNAME_HIGHLIGHT", selectedOption == SUO_NICKNAME);
		m_scMovie.CallMethod("SET_SIGN_UP_EMAIL_ADDRESS_HIGHLIGHT", selectedOption == SUO_EMAIL);
		m_scMovie.CallMethod("SET_SIGN_UP_PASSWORD_HIGHLIGHT", selectedOption == SUO_PASSWORD);
		m_scMovie.CallMethod("SET_HIGLIGHT_INPUT_MAILING_LIST", selectedOption == SUO_SPAM);

		if(canSubmit)
		{
			m_scMovie.CallMethod("SET_SIGN_UP_SUBMIT_HIGHLIGHT", selectedOption == SUO_SUBMIT);
		}
		else
		{
			m_scMovie.CallMethod("SET_SIGN_UP_SUBMIT_DISABLED");
		}
	}
}

void SocialClubMenu::UpdateSignUpConfirm()
{
	uiDebugf3("SocialClubMenu::UpdateSignUpConfirm");

	CheckNextPrev();
}

void SocialClubMenu::UpdateSigningUp()
{
	uiDebugf3("SocialClubMenu::UpdateSigningUp, isSCMStatusPedning=%d, isConnected=%d, maxTimerComplete=%d %d", CLiveManager::GetSocialClubMgr().IsPending(), SPolicyVersions::GetInstance().IsUpToDate(), CLiveManager::GetSocialClubMgr().IsConnectedToSocialClub(), m_delayTimer.IsComplete(SC_MAX_ACCEPT_TIME));

	if(!CLiveManager::GetSocialClubMgr().IsPending() &&
		(m_gotCredsResult || m_delayTimer.IsComplete(SC_MAX_ACCEPT_TIME))) // If might take a while for the SC account info to be refreshed.
	{
		CLiveManager::GetSocialClubMgr().SetOnlinePolicyStatus(true);
		GoToNext();
	}
	else
	{
		const char* pError = SPolicyVersions::GetInstance().GetError();
		if(pError)
		{
			CLiveManager::GetSocialClubMgr().SetOnlinePolicyStatus(false);
			SetGenericError("SC_CREATED_BUT_FAILED");
		}
		else if(CheckInput(FRONTEND_INPUT_BACK))
		{
			GoToPrevious();
		}
	}
}

void SocialClubMenu::UpdateSignUpDone()
{
	uiDebugf3("SocialClubMenu::UpdateSignUpDone");

	if(CheckInput(FRONTEND_INPUT_ACCEPT) || CheckInput(FRONTEND_INPUT_BACK))
	{
		FlagForDeletion();
	}
}

void SocialClubMenu::UpdateSignIn()
{
	uiDebugf3("SocialClubMenu::UpdateSignIn");

	if(CheckInput(FRONTEND_INPUT_ACCEPT))
	{
		switch(m_siOption)
		{
		case SIO_NICK_EMAIL:
			{
				SetupNickEmailKeyboard();
			}
			break;
		case SIO_PASSWORD:
			{
				SetupPasswordKeyboard();
			}
			break;
		case SIO_SUBMIT:
			{
				GoToNext();
			}
			break;
		case SIO_RESET_PASSWORD:
			{
				GoToState(SC_RESET_PASSWORD);
			}
			break;
		default:
			uiAssertf(0, "SocialClubMenu::UpdateSignIn - Unhandled menu option %d", m_siOption);
			break;
		}
	}
	else if(CheckInput(FRONTEND_INPUT_BACK))
	{
		GoToState(SC_ONLINE_POLICIES);
	}

	bool canSubmit = (m_emailStatus.Succeeded() || m_nicknameStatus.Succeeded()) &&
			(CLiveManager::GetSocialClubMgr().GetSocialClubInfo().GetPassword()[0] != '\0');


	bool validStates[SIO_MAX] = {true};
	memset(validStates, true, sizeof(validStates));
	validStates[SIO_SUBMIT] = canSubmit;

	int selectedOption = static_cast<int>(m_siOption);
	if(UpdateOption(selectedOption, SIO_MAX, validStates))
	{
		m_siOption = static_cast<eSignInOptions>(selectedOption);

		m_scMovie.CallMethod("SET_SIGN_IN_EMAIL_ADDRESS_HIGHLIGHT", selectedOption == SIO_NICK_EMAIL);
		m_scMovie.CallMethod("SET_SIGN_IN_PASSWORD_HIGHLIGHT", selectedOption == SIO_PASSWORD);

		m_scMovie.CallMethod("SET_SIGN_IN_PASSWORD_RESET_HIGHLIGHT", selectedOption == SIO_RESET_PASSWORD);

		UpdateSignInSubmitState();
	}
}

void SocialClubMenu::UpdateSigningIn()
{
	uiDebugf3("SocialClubMenu::UpdateSigningIn, isSCMStatusPedning=%d, isConnected=%d, maxTimerComplete=%d", CLiveManager::GetSocialClubMgr().IsPending(), CLiveManager::GetSocialClubMgr().IsConnectedToSocialClub(), m_delayTimer.IsComplete(SC_MAX_ACCEPT_TIME));

	if(!CLiveManager::GetSocialClubMgr().IsPending() &&
		(m_gotCredsResult || m_delayTimer.IsComplete(SC_MAX_ACCEPT_TIME))) // If might take a while for the SC account info to be refreshed.
	{
		GoToNext();
	}
}

void SocialClubMenu::UpdateSignInDone()
{
	uiDebugf3("SocialClubMenu::UpdateSignInDone");

	if(CheckInput(FRONTEND_INPUT_ACCEPT) || CheckInput(FRONTEND_INPUT_BACK))
	{
		FlagForDeletion();
	}
}

void SocialClubMenu::UpdateResetPassword()
{
	uiDebugf3("SocialClubMenu::UpdateResetPassword");

	if(CheckInput(FRONTEND_INPUT_ACCEPT))
	{
		switch(m_rpOption)
		{
		case RPO_EMAIL:
			{
				SetupEmailKeyboard();
			}
			break;
		case RPO_SUBMIT:
			{
				GoToNext();
			}
			break;
		default:
			uiAssertf(0, "SocialClubMenu::UpdateResetPassword - Unhandled menu option %d", m_rpOption);
			break;
		}
	}
	else if(CheckInput(FRONTEND_INPUT_BACK))
	{
		GoToState(SC_SIGNIN);
	}

	bool validStates[RPO_MAX] = {true};
	memset(validStates, true, sizeof(validStates));
	validStates[RPO_SUBMIT] = m_emailStatus.Succeeded();

	int selectedOption = static_cast<int>(m_rpOption);
	if(UpdateOption(selectedOption, RPO_MAX, validStates))
	{
		m_rpOption = static_cast<eResetPasswordOptions>(selectedOption);

		m_scMovie.CallMethod("SET_FORGOT_PASSWORD_EMAIL_HIGHLIGHT", selectedOption == RPO_EMAIL);

		if(m_emailStatus.Succeeded())
		{
			m_scMovie.CallMethod("SET_FORGOT_PASSWORD_SUBMIT_HIGHLIGHT", selectedOption == RPO_SUBMIT);
		}
	}
}

void SocialClubMenu::UpdateResettingPassword()
{
	uiDebugf3("SocialClubMenu::UpdateResettingPassword");

	if(CheckInput(FRONTEND_INPUT_BACK))
	{
		m_netStatus.SetCanceled();
	}
}

void SocialClubMenu::UpdateResetPasswordDone()
{
	uiDebugf3("SocialClubMenu::UpdateResetPasswordDone");

	if(CheckInput(FRONTEND_INPUT_ACCEPT) || CheckInput(FRONTEND_INPUT_BACK))
	{
		GoToState(SC_SIGNIN);
	}
}

void SocialClubMenu::UpdatePolicyUpdateSummary()
{
	uiDebugf3("SocialClubMenu::UpdatePolicyUpdateSummary");

	if(CheckInput(FRONTEND_INPUT_ACCEPT) || CMousePointer::IsSFMouseReleased(m_scMovie.GetMovieID(), 0))
	{
		GoToState(SC_ONLINE_POLICIES);
	}
	else if(CheckInput(FRONTEND_INPUT_BACK))
	{
		FlagForDeletion();
	}
}

void SocialClubMenu::UpdatePolicyUpdateAccepted()
{
	uiDebugf3("SocialClubMenu::UpdatePolicyUpdateAccepted");

	if(CheckInput(FRONTEND_INPUT_ACCEPT) || CMousePointer::IsSFMouseReleased(m_scMovie.GetMovieID(), 0))
	{
		FlagForDeletion();
	}
}

void SocialClubMenu::UpdateScAuthSignup()
{
	if (CheckInput(FRONTEND_INPUT_BACK))
	{
		CLiveManager::GetSocialClubMgr().CancelTask();
	}
}

void SocialClubMenu::UpdateScAuthSignupDone()
{
	uiDebugf3("SocialClubMenu::UpdateSignInDone");

	if (CheckInput(FRONTEND_INPUT_ACCEPT))
	{
		FlagForDeletion();
	}
}

void SocialClubMenu::UpdateScAuthSignin()
{
	if (CheckInput(FRONTEND_INPUT_BACK))
	{
		CLiveManager::GetSocialClubMgr().CancelTask();
	}
}

void SocialClubMenu::UpdateScAuthSigninDone()
{
	uiDebugf3("SocialClubMenu::UpdateSignInDone");

	if (CheckInput(FRONTEND_INPUT_ACCEPT))
	{
		FlagForDeletion();
	}
}

void SocialClubMenu::UpdateError()
{
	uiDebugf3("SocialClubMenu::UpdateError");

	m_errorMessageData.UpdateCallbacks();
}



void SocialClubMenu::CheckNextPrev()
{
	uiDebugf3("SocialClubMenu::CheckNextPrev");

	if(CheckInput(FRONTEND_INPUT_ACCEPT))
	{
		GoToNext();
	}
	else if(CheckInput(FRONTEND_INPUT_BACK))
	{
		GoToPrevious();
	}
}

void SocialClubMenu::CheckAutoFill()
{
#if DEBUG_SC_MENU
	if(CheckInput(FRONTEND_INPUT_SELECT))
	{
		Debug_AutofillAccountInfo();
		GoToState(SC_SIGNUP);
		m_scMovie.CallMethod("SET_SIGN_UP_SUBMIT_HIGHLIGHT", m_suOption == SUO_SUBMIT);
	}
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
void SocialClubMenu::GoToDownloading()
{
	uiDebugf1("SocialClubMenu::GoToDownloading");
	SetBusySpinner(true);

	m_state = SC_DOWNLOADING;

	m_scMovie.CallMethod("DISPLAY_SYNC", BuildLocTextParam("SOCIAL_CLUB"), BuildLocTextParam("SC_SYNCSERVER"));

	m_delayTimer.Start();
	SetButtons(BUTTON_CANCEL);
}

void SocialClubMenu::GoToWelcome()
{
	uiDebugf1("SocialClubMenu::GoToWelcome");

	m_state = SC_WELCOME;

	m_scMovie.CallMethod("DISPLAY_WELCOME_PAGE");
	m_scMovie.CallMethod("SET_WELCOME_JOIN_HIGHLIGHT", m_wOptions == WO_SIGNUP);
	m_scMovie.CallMethod("SET_WELCOME_SIGN_IN_HIGHLIGHT", m_wOptions == WO_SIGNIN);
	m_scMovie.CallMethod("SETUP_WELCOME_TABS", m_tourManager.NumTours());

	if(m_welcomeTab == -1)
	{
		if(m_tourManager.NumTours() > 0)
		{
			SetWelcomeTab(static_cast<s8>(m_tourManager.GetStartingTour(sm_currentTourHash)));
		}
		else
		{
			m_welcomeTab = 0;
			m_scMovie.CallMethod("SET_WELCOME_TAB", m_welcomeTab);
		}
	}

	SetButtons(BUTTON_ACCEPT | BUTTON_EXIT | BUTTON_LR_TAB | BUTTON_AUTOFILL);
}

#if SC_COLLECT_AGE
void SocialClubMenu::GoToDOB()
{
	uiDebugf1("SocialClubMenu::GoToDOB");

	m_state = SC_DOB;

	m_scMovie.CallMethod("DISPLAY_DOB_PAGE");
	SetButtons(BUTTON_ACCEPT | BUTTON_CANCEL | BUTTON_AUTOFILL | BUTTON_GOTO_SIGNIN);
}
#endif

void SocialClubMenu::GoToOnlinePolicies()
{
	uiDebugf1("SocialClubMenu::GoToOnlinePolicies");

	m_state = SC_ONLINE_POLICIES;
	m_acceptPolicies = false;
	m_opOption = OPO_ACCEPT;

	m_scMovie.CallMethod("DISPLAY_ONLINE_POLICY");

	m_scMovie.CallMethod("SET_ONLINE_POLICY_LINK_1_HIGHLIGHT", m_opOption == OPO_EULA);
	m_scMovie.CallMethod("SET_ONLINE_POLICY_LINK_2_HIGHLIGHT", m_opOption == OPO_PP);
	m_scMovie.CallMethod("SET_ONLINE_POLICY_LINK_3_HIGHLIGHT", m_opOption == OPO_TOS);
	m_scMovie.CallMethod("SET_ONLINE_POLICY_ACCEPT_HIGHLIGHT", m_opOption == OPO_ACCEPT);
	m_scMovie.CallMethod("SET_ONLINE_POLICY_ACCEPT_RADIO_BUTTON_STATE", m_acceptPolicies);
	m_scMovie.CallMethod("SET_ONLINE_POLICY_SUBMIT_HIGHLIGHT", m_acceptPolicies, m_opOption == OPO_SUBMIT);

	SetButtons(BUTTON_SELECT | BUTTON_BACK);
}


void SocialClubMenu::GoToEula()
{
	uiDebugf1("SocialClubMenu::GoToEula");

	m_state = SC_EULA;

	m_scMovie.CallMethod("DISPLAY_DOWNLOADED_POLICY");
	m_scMovie.CallMethod("SET_POLICY_TITLE", BuildLocTextParam("ONLINE_POLICY_EULA_TITLE"));
	CallMovieWithPagedText("SET_POLICY_TEXT", m_policyManager.GetFile(SCF_EULA));
	m_scMovie.CallMethod("SCROLL_POLICY_TEXT", SO_TOP);

	SetButtons(BUTTON_SELECT | BUTTON_SCROLL_UPDOWN);
}

void SocialClubMenu::GoToPrivacyPolicy()
{
	uiDebugf1("SocialClubMenu::GoToPrivacyPolicy");

	m_state = SC_PP;

	m_scMovie.CallMethod("DISPLAY_DOWNLOADED_POLICY");
	m_scMovie.CallMethod("SET_POLICY_TITLE", BuildLocTextParam("ONLINE_POLICY_PP_TITLE"));
	CallMovieWithPagedText("SET_POLICY_TEXT", m_policyManager.GetFile(SCF_PP));
	m_scMovie.CallMethod("SCROLL_POLICY_TEXT", SO_TOP);

	SetButtons(BUTTON_SELECT | BUTTON_SCROLL_UPDOWN);
}

void SocialClubMenu::GoToTOS()
{
	uiDebugf1("SocialClubMenu::GoToTOS");

	m_state = SC_TOS;

	m_scMovie.CallMethod("DISPLAY_DOWNLOADED_POLICY");
	m_scMovie.CallMethod("SET_POLICY_TITLE", BuildLocTextParam("ONLINE_POLICY_TOS_TITLE"));
	CallMovieWithPagedText("SET_POLICY_TEXT", m_policyManager.GetFile(SCF_TOS));
	m_scMovie.CallMethod("SCROLL_POLICY_TEXT", SO_TOP);

	SetButtons(BUTTON_SELECT | BUTTON_SCROLL_UPDOWN);
}

void SocialClubMenu::GoToSignUp()
{
	uiDebugf1("SocialClubMenu::GoToSignUp");

	m_state = SC_SIGNUP;

	m_scMovie.CallMethod("DISPLAY_SIGN_UP");
	m_scMovie.CallMethod("SET_SIGN_UP_RADIO_BUTTON_STATE", m_allowEmailSpam);
	CLiveManager::GetSocialClubMgr().GetSocialClubInfo().SetSpam(m_allowEmailSpam);

	if(!CLiveManager::GetSocialClubMgr().GetSocialClubInfo().IsSpammable())
	{
		m_scMovie.CallMethod("SET_INPUT_MAILING_LIST", "");
		m_isSpammable = false;
	}

	m_emailStatus.SetCallback(datCallback(MFA2(SocialClubMenu::CheckSignUpNickEmailVerification), (datBase*)this, const_cast<char*>("SET_SIGN_UP_EMAIL_STATE")));
	m_nicknameStatus.SetCallback(datCallback(MFA2(SocialClubMenu::CheckSignUpNickEmailVerification), (datBase*)this, const_cast<char*>("SET_SIGN_UP_NICKNAME_STATE")));
	m_passwordStatus.SetCallback(datCallback(MFA2(SocialClubMenu::CheckPasswordVerification), (datBase*)this, const_cast<char*>("SET_SIGN_UP_PASSWORD_STATE")));

	UpdateSignUpSubmitState();

	const char* pPassword = CLiveManager::GetSocialClubMgr().GetSocialClubInfo().GetPassword();
	if(pPassword)
	{
		FillOutPassword(pPassword);
	}

	const char* pNickname = CLiveManager::GetSocialClubMgr().GetSocialClubInfo().GetNickname();
	if(pNickname)
	{
		FillOutNickname(pNickname);
	}

	const char* pEmail = CLiveManager::GetSocialClubMgr().GetSocialClubInfo().GetEmail();
	if(pEmail)
	{
		FillOutEmail(pEmail);
	}

	SetButtons(BUTTON_SELECT | BUTTON_BACK | BUTTON_AUTOFILL | BUTTON_SUGGEST_NICKNAME | BUTTON_GOTO_SIGNIN);
}

void SocialClubMenu::GoToSignUpConfirm()
{
	uiDebugf1("SocialClubMenu::GoToSignUpConfirm");

	m_state = SC_SIGNUP_CONFIRM;

	m_scMovie.CallMethod("DISPLAY_CONFIRM_PAGE");
	m_scMovie.CallMethod("SET_CONFIRM_TEXT_BLIPS", BuildLocTextParam("SC_CONFIRM_TEXT", false));
	m_scMovie.CallMethod("SET_CONFIRM_USER_EMAIL_TEXT", CLiveManager::GetSocialClubMgr().GetSocialClubInfo().GetEmail());
	m_scMovie.CallMethod("SET_CONFIRM_USER_NICKNAME_TEXT", CLiveManager::GetSocialClubMgr().GetSocialClubInfo().GetNickname());

	if(m_isSpammable)
	{
		// Purposely not using BuildLocTextParam, as this has font coloring in the string.
		m_scMovie.CallMethod("SET_CONFIRM_NEWSLETTER_TEXT", m_allowEmailSpam ? TheText.Get("SC_CONF_YES_SPAM") : TheText.Get("SC_CONF_NO_SPAM"));
	}

	SetButtons(BUTTON_SELECT | BUTTON_BACK);
}

void SocialClubMenu::GoToSigningUp()
{
	uiDebugf1("SocialClubMenu::GoToSigningUp");
	SetBusySpinner(true);

	m_state = SC_SIGNING_UP;

	m_scMovie.CallMethod("DISPLAY_SYNC", BuildLocTextParam("SC_SIGNING_UP_TITLE"), BuildLocTextParam("SC_SIGNING_UP_TEXT"));

	char countryCode[3];
	if (!CProfileSettings::GetInstance().GetCountry(countryCode))
	{
		//If we failed to retrieve the country, default to US
		//Could default to ROW or something similar, but not
		//supported by SC currently
		formatf(countryCode, COUNTOF(countryCode), "US");
	}

	uiDebugf1("Signing up for country = %s", countryCode);

	CLiveManager::GetSocialClubMgr().CreateAccount(countryCode, NetworkUtils::GetLanguageCode(),
		datCallback(MFA1(SocialClubMenu::AccountCreationCallback), (datBase*)this, NULL, true));

	m_delayTimer.Start();
	SetButtons(BUTTON_BACK);
}

void SocialClubMenu::GoToSignUpDone()
{
	uiDebugf1("SocialClubMenu::GoToSignUpDone");

	m_state = SC_SIGNUP_DONE;

	m_scMovie.CallMethod("DISPLAY_SIGN_UP_DONE_PAGE");

	SetButtons(BUTTON_SELECT);
}

void SocialClubMenu::GoToSignIn()
{
	uiDebugf1("SocialClubMenu::GoToSignIn");

	m_state = SC_SIGNIN;

	m_scMovie.CallMethod("DISPLAY_SIGN_IN");

	m_emailStatus.SetCallback(NullCallback);
	m_nicknameStatus.SetCallback(NullCallback);
	m_passwordStatus.SetCallback(NullCallback);

	if(m_emailStatus.Pending())
	{
		rlSocialClub::Cancel(&m_emailStatus);
	}
	m_emailStatus.Reset();

	if(m_nicknameStatus.Pending())
	{
		rlSocialClub::Cancel(&m_nicknameStatus);
	}
	m_nicknameStatus.Reset();

	if(m_passwordStatus.Pending())
	{
		rlSocialClub::Cancel(&m_passwordStatus);
	}
	m_passwordStatus.Reset();

	const char* pPassword = CLiveManager::GetSocialClubMgr().GetSocialClubInfo().GetPassword();
	if(pPassword)
	{
		FillOutPassword(pPassword);
	}

	const char* pNickname = CLiveManager::GetSocialClubMgr().GetSocialClubInfo().GetNickname();
	const char* pEmail = CLiveManager::GetSocialClubMgr().GetSocialClubInfo().GetEmail();
	if(pNickname && pEmail)
	{
		if(pEmail[0] != '\0')
		{
			FillOutEmail(pEmail);
			FillOutNickname("", false);
		}
		else if(pNickname[0] != '\0')
		{
			FillOutNickname(pNickname);
			FillOutEmail("", false);
		}
		else
		{
			m_scMovie.CallMethod("SET_NICKNAME", BuildLocTextParam("SC_EMAIL_NICKNAME_LABEL"));
		}
	}

	SetButtons(BUTTON_SELECT | BUTTON_BACK | BUTTON_AUTOFILL);
}

void SocialClubMenu::GoToSigningIn()
{
	uiDebugf1("SocialClubMenu::GoToSigningIn");
	SetBusySpinner(true);

	m_state = SC_SIGNING_IN;

	m_scMovie.CallMethod("DISPLAY_SYNC", BuildLocTextParam("SC_SIGNING_IN_TITLE"), BuildLocTextParam("SC_SIGNING_IN_TEXT"));
	
	bool bValidEmail = strlen(CLiveManager::GetSocialClubMgr().GetSocialClubInfo().GetEmail()) < RLSC_MAX_EMAIL_CHARS;
	bool bValidNickName = strlen(CLiveManager::GetSocialClubMgr().GetSocialClubInfo().GetNickname()) < RLSC_MAX_NICKNAME_CHARS;
	bool bValidPassword = strlen(CLiveManager::GetSocialClubMgr().GetSocialClubInfo().GetPassword()) < RLSC_MAX_PASSWORD_CHARS;
	bool bSuccess = false;

	if(bValidEmail && bValidNickName && bValidPassword)
	{
		bSuccess = CLiveManager::GetSocialClubMgr().LinkAccount(datCallback(MFA1(SocialClubMenu::AccountLinkCallback), (datBase*)this, NULL, true));
	}

	if(!bSuccess)
	{
		int iStatusCode = RLSC_ERROR_UNEXPECTED_RESULT;
		if(!bValidEmail)
			iStatusCode = RLSC_ERROR_INVALIDARGUMENT_EMAIL;
		if(!bValidNickName)
			iStatusCode = RLSC_ERROR_OUTOFRANGE_NICKNAMELENGTH;
		if(!bValidPassword)
			iStatusCode = RLSC_ERROR_OUTOFRANGE_PASSWORDLENGTH;
		netStatus status;
		status.ForceFailed(iStatusCode);
		SetGenericWarning(SocialClubMgr::GetErrorName(status), static_cast<eSocialClubStates>(m_state-1));
	}

	m_delayTimer.Start();
	SetButtons(BUTTON_NONE);
}

void SocialClubMenu::GoToSignInDone()
{
	uiDebugf1("SocialClubMenu::GoToSignInDone");

	m_state = SC_SIGNIN_DONE;

	m_scMovie.CallMethod("DISPLAY_SIGN_IN_DONE_PAGE");

	SetButtons(BUTTON_SELECT);
}

void SocialClubMenu::GoToResetPassword()
{
	uiDebugf1("SocialClubMenu::GoToResetPassword");

	m_state = SC_RESET_PASSWORD;

	m_scMovie.CallMethod("DISPLAY_FORGOT_PASSWORD_PAGE");
	m_emailStatus.SetCallback(datCallback(MFA2(SocialClubMenu::CheckSignInNickEmailVerification), (datBase*)this, const_cast<char*>("SET_FORGOT_PASSWORD_EMAIL_STATE")));

	SetButtons(BUTTON_SELECT | BUTTON_BACK);
}

void SocialClubMenu::GoToResettingPassword()
{
	uiDebugf1("SocialClubMenu::GoToResettingPassword");
	SetBusySpinner(true);

	m_state = SC_RESETTING_PASSWORD;

	m_scMovie.CallMethod("DISPLAY_SYNC", BuildLocTextParam("SC_FORGOT_PASSWORD_TITLE"), BuildLocTextParam("SC_RESETTING_PASSWORD"));
	m_netStatus.SetCallback(datCallback(MFA1(SocialClubMenu::AccountResetPasswordCallback), (datBase*)this, NULL, true));

	rlSocialClub::RequestResetPassword(CLiveManager::GetSocialClubMgr().GetSocialClubInfo().GetEmail(), &m_netStatus);

	SetButtons(BUTTON_CANCEL);
}

void SocialClubMenu::GoToResetPasswordDone()
{
	uiDebugf1("SocialClubMenu::GoToResetPasswordDone");

	m_state = SC_RESET_PASSWORD_DONE;

	m_scMovie.CallMethod("DISPLAY_FORGOT_PASSWORD_DONE_PAGE");

	SetButtons(BUTTON_SELECT);
}

void SocialClubMenu::GoToPolicyUpdateAccepted()
{
	uiDebugf1("SocialClubMenu::GoToPolicyUpdateAccepted");

	m_state = SC_POLICY_UPDATE_ACCEPTED;

	m_scMovie.CallMethod("DISPLAY_SIGN_IN_DONE_PAGE");
	m_scMovie.CallMethod("SET_SIGN_IN_DONE_TITLE", BuildLocTextParam("SC_POLICIES_UPDATE_TITLE"));
	m_scMovie.CallMethod("SET_SIGN_IN_DONE_TEXT", BuildLocTextParam("SC_POLICY_UPDATE_ACCEPTED"));

	SetButtons(BUTTON_SELECT);
}

void SocialClubMenu::GoToScAuthSignup()
{
	uiDebugf1("SocialClubMenu::GoToScAuthSignup");
	SetBusySpinner(true);
	m_state = SC_AUTH_SIGNUP;

	m_scMovie.CallMethod("DISPLAY_SYNC", BuildLocTextParam("SC_SIGNING_UP_TITLE"), BuildLocTextParam("SC_SIGNING_UP_TEXT"));

	CLiveManager::GetSocialClubMgr().LinkAccountScAuth(rlScAuth::SIGN_UP, datCallback(MFA1(SocialClubMenu::ScAuthSignupCallback), (datBase*)this, NULL, true));

	m_delayTimer.Start();
	SetButtons(BUTTON_BACK);
}

void SocialClubMenu::GoToScAuthSignUpDone()
{
	uiDebugf1("SocialClubMenu::GoToScAuthSignUpDone");

	m_state = SC_AUTH_SIGNUP_DONE;

	m_scMovie.CallMethod("DISPLAY_SIGN_UP_DONE_PAGE");

	SetButtons(BUTTON_SELECT);
}

void SocialClubMenu::GoToScAuthSignin()
{
	uiDebugf1("SocialClubMenu::GoToScAuthSignin");
	SetBusySpinner(true);
	m_state = SC_AUTH_SIGNIN;

	m_scMovie.CallMethod("DISPLAY_SYNC", BuildLocTextParam("SC_SIGNING_IN_TITLE"), BuildLocTextParam("SC_SIGNING_IN_TEXT"));

	CLiveManager::GetSocialClubMgr().LinkAccountScAuth(rlScAuth::SIGN_IN, datCallback(MFA1(SocialClubMenu::ScAuthSigninCallback), (datBase*)this, NULL, true));

	m_delayTimer.Start();
	SetButtons(BUTTON_BACK);
}

void SocialClubMenu::GoToScAuthSigninDone()
{
	uiDebugf1("SocialClubMenu::GoToScAuthSigninDone");

	m_state = SC_AUTH_SIGNIN_DONE;

	m_scMovie.CallMethod("DISPLAY_SIGN_IN_DONE_PAGE");

	SetButtons(BUTTON_SELECT);
}

void SocialClubMenu::GoToError(const CWarningMessage::Data& rMessage)
{
	uiDebugf1("SocialClubMenu::GoToError");
	SetBusySpinner(false);

	m_state = SC_ERROR;
	SetButtons(BUTTON_SELECT);

	m_errorMessageData = rMessage;

	m_scMovie.CallMethod("DISPLAY_ERROR_PAGE");
	m_scMovie.CallMethod("SET_ERROR_TITLE", BuildLocTextParam(m_errorMessageData.m_TextLabelHeading));
	m_scMovie.CallMethod("SET_ERROR_TEXT", BuildLocTextParam(m_errorMessageData.m_TextLabelBody));
	m_scMovie.CallMethod("SET_ERROR_BUTTON_TEXT", BuildLocTextParam(m_errorMessageData.m_TextLabelSubtext));
}

void SocialClubMenu::GoToNext()
{
	uiDebugf1("SocialClubMenu::GoToNext - Curr State=%d.", m_state);
	GoToState(static_cast<eSocialClubStates>(m_state+1));
}

void SocialClubMenu::GoToPrevious()
{
	uiDebugf1("SocialClubMenu::GoToPrevious - Curr State=%d.", m_state);
	GoToState(static_cast<eSocialClubStates>(m_state-1));
}

void SocialClubMenu::GoToState(eSocialClubStates newState)
{
	uiDebugf1("SocialClubMenu::GoToState - Old State=%d, New State=%d.", m_state, newState);
	SetBusySpinner(false);

	switch(newState)
	{
	case SC_IDLE:
		m_state = newState;
		break;
	case SC_DOWNLOADING:
		GoToDownloading();
		break;
	case SC_WELCOME:
		GoToWelcome();
		break;
#if SC_COLLECT_AGE
	case SC_DOB:
		GoToDOB();
		break;
#endif
	case SC_ONLINE_POLICIES:
		GoToOnlinePolicies();
		break;
	case SC_EULA:
		GoToEula();
		break;
	case SC_PP:
		GoToPrivacyPolicy();
		break;
	case SC_TOS:
		GoToTOS();
		break;
	case SC_SIGNUP:
		GoToSignUp();
		break;
	case SC_SIGNUP_CONFIRM:
		GoToSignUpConfirm();
		break;
	case SC_SIGNING_UP:
		GoToSigningUp();
		break;
	case SC_SIGNUP_DONE:
		GoToSignUpDone();
		break;
	case SC_SIGNIN:
		GoToSignIn();
		break;
	case SC_SIGNING_IN:
		GoToSigningIn();
		break;
	case SC_SIGNIN_DONE:
		GoToSignInDone();
		break;
	case SC_RESET_PASSWORD:
		GoToResetPassword();
		break;
	case SC_RESETTING_PASSWORD:
		GoToResettingPassword();
		break;
	case SC_RESET_PASSWORD_DONE:
		GoToResetPasswordDone();
		break;
	case SC_POLICY_UPDATE_ACCEPTED:
		GoToPolicyUpdateAccepted();
		break;
	case SC_AUTH_SIGNUP:
		GoToScAuthSignup();
		break;
	case SC_AUTH_SIGNUP_DONE:
		GoToScAuthSignUpDone();
		break;
	case SC_AUTH_SIGNIN:
		GoToScAuthSignin();
		break;
	case SC_AUTH_SIGNIN_DONE:
		GoToScAuthSigninDone();
		break;
	case SC_ERROR:
		uiAssertf(0, "SocialClubMenu::GoToState - Going to State Error, but this state needs extra data and setup.");
		//GoToError();
		break;
	}
}
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
void SocialClubMenu::SetWelcomeTab(s8 welcomeTab)
{
	m_welcomeTab = welcomeTab;
	m_scMovie.CallMethod("SET_WELCOME_TAB", m_welcomeTab);

	SCTour& rTour = m_tourManager.GetTour(m_welcomeTab);
	m_scMovie.CallMethod("SET_WELCOME_TITLE_TEXT", BuildNonLocTextParam(rTour.GetHeader()));
	m_scMovie.CallMethod("SET_WELCOME_INTRO_TEXT", BuildNonLocTextParam(rTour.GetBody()));
	m_scMovie.CallMethod("SET_WELCOME_CALLOUT_TEXT", BuildNonLocTextParam(rTour.GetCallout()));

	if(rTour.HasCloudImage())
	{
		CScaleformMovieWrapper::Param imageName = CScaleformMovieWrapper::Param(rTour.GetTextureName(), false);
		m_scMovie.CallMethod("SET_WELCOME_IMAGE", imageName, imageName);
		m_scMovie.CallMethod("SET_WELCOME_FALLBACK_IMAGE_VISIBILITY", false);
	}
	else
	{
		m_scMovie.CallMethod("SET_WELCOME_FALLBACK_IMAGE_VISIBILITY", true);
	}
}

void SocialClubMenu::SetupEmailKeyboard()
{
	SetupKeyboardHelper("SC_EMAIL_LABEL", CLiveManager::GetSocialClubMgr().GetSocialClubInfo().GetEmail(), ioVirtualKeyboard::kTextType_EMAIL);
	m_keyboardCallback = datCallback(MFA(SocialClubMenu::FillOutEmailFromKeyboard), (datBase*)this);
}

void SocialClubMenu::SetupNicknameKeyboard()
{
	SetupKeyboardHelper("SC_NICK_FORM", CLiveManager::GetSocialClubMgr().GetSocialClubInfo().GetNickname(), ioVirtualKeyboard::kTextType_ALPHABET);
	m_keyboardCallback = datCallback(MFA(SocialClubMenu::FillOutNicknameFromKeyboard), (datBase*)this);
}

void SocialClubMenu::SetupNickEmailKeyboard()
{
	const char* pEmail = CLiveManager::GetSocialClubMgr().GetSocialClubInfo().GetEmail();
	const char* pInitialStr = pEmail[0] != '\0' ? pEmail : CLiveManager::GetSocialClubMgr().GetSocialClubInfo().GetNickname();
	SetupKeyboardHelper("SC_EMAIL_NICK_FORM", pInitialStr, ioVirtualKeyboard::kTextType_EMAIL);
	m_keyboardCallback = datCallback(MFA(SocialClubMenu::FillOutNickEmailFromKeyboard), (datBase*)this);
}

void SocialClubMenu::SetupPasswordKeyboard()
{
	// Default values in case Social Club could not retrieve password requirements
	int minLength = 8;
	int maxLength = RLSC_MAX_PASSWORD_CHARS;

	// Retrieve password requirements from social club
	rlScPasswordReqs& passwordReqs = CLiveManager::GetSocialClubMgr().GetPasswordRequirements();
	if (passwordReqs.bValid)
	{
		minLength = passwordReqs.MinLength;
		maxLength = passwordReqs.MaxLength;
	}

	CNumberWithinMessage sizeBounds[2];
	sizeBounds[0].Set(minLength);
	sizeBounds[1].Set(maxLength);

	char msgBuffer[512] = {0};
	CMessages::InsertNumbersAndSubStringsIntoString(TheText.Get("SC_PW_FORM"), sizeBounds, NELEM(sizeBounds), NULL, 0, msgBuffer, NELEM(msgBuffer));

#if RSG_DURANGO
	// Durango's Virtual Keyboard's maxLength includes the null terminator... PS4 does not. If you pass the SCS password requirements
	// max length, you need to add 1 for the null terminator.
	maxLength++;
#endif

	SetupKeyboardHelper(msgBuffer, NULL, ioVirtualKeyboard::kTextType_PASSWORD, maxLength, true);
	m_keyboardCallback = datCallback(MFA(SocialClubMenu::FillOutPasswordFromKeyboard), (datBase*)this);
}

void SocialClubMenu::SetupDateKeyboard(const char* pTitle, const char* pInitialStr, int maxLength)
{
	SetupKeyboardHelper(pTitle, pInitialStr, ioVirtualKeyboard::kTextType_NUMERIC, maxLength);
	m_keyboardCallback = datCallback(MFA(SocialClubMenu::FillOutDateFromKeyboard), (datBase*)this);
}

void SocialClubMenu::SetupKeyboardHelper(const char* pTitle, const char* pInitialStr, ioVirtualKeyboard::eTextType keyboardType, int maxLength /*= -1*/, bool textEvaluated /* = false */)
{
#ifdef USE_UNICODE_FOR_TEXT_FILE
	CTextConversion::charStrcpy(m_keyboardLabel, textEvaluated ? pTitle : TheText.Get(pTitle), KEYBOARD_LABEL_SIZE);
	CTextConversion::charStrcpy(m_keyboardInitialStr, pInitialStr ? pInitialStr : "", ioVirtualKeyboard::MAX_STRING_LENGTH_FOR_VIRTUAL_KEYBOARD);
#else
	Utf8ToWide(m_keyboardLabel, textEvaluated ? pTitle : TheText.Get(pTitle), KEYBOARD_LABEL_SIZE);
	Utf8ToWide(m_keyboardInitialStr, pInitialStr ? pInitialStr : "", ioVirtualKeyboard::MAX_STRING_LENGTH_FOR_VIRTUAL_KEYBOARD);
#endif

	m_keyboardParams.Reset();

	m_keyboardParams.m_KeyboardType = keyboardType;
	m_keyboardParams.m_AlphabetType = ioVirtualKeyboard::kAlphabetType_ENGLISH;
	m_keyboardParams.m_Title = m_keyboardLabel;
	m_keyboardParams.m_Description = m_keyboardLabel;
	m_keyboardParams.m_InitialValue = m_keyboardInitialStr;
	m_keyboardParams.m_PlayerIndex = NetworkInterface::GetLocalGamerIndex();

	if(0 < maxLength)
	{
		m_keyboardParams.m_MaxLength = maxLength;
	}

	CControlMgr::ShowVirtualKeyboard(m_keyboardParams);

	m_isKeyboardOpen = true;
}

void SocialClubMenu::FillOutEmailFromKeyboard()
{
	const char* result = CControlMgr::GetVirtualKeyboardResult();

	FillOutEmail(result);
}

void SocialClubMenu::FillOutNicknameFromKeyboard()
{
	const char* result = CControlMgr::GetVirtualKeyboardResult();

	formatf(m_originalNickname, RLSC_MAX_NICKNAME_CHARS, "%s", result);
	FillOutNickname(m_originalNickname);
}

void SocialClubMenu::FillOutNickEmailFromKeyboard()
{
	const char* result = CControlMgr::GetVirtualKeyboardResult();
	if(result[0] != '\0')
	{
		if(strstr(result, "@") != NULL)
		{
			FillOutEmailFromKeyboard();
			FillOutNickname("", false);
		}
		else
		{
			FillOutNicknameFromKeyboard();
			FillOutEmail("", false);
		}
	}
	else
	{
		m_scMovie.CallMethod("SET_NICKNAME", BuildLocTextParam("SC_EMAIL_NICKNAME_LABEL"));
		m_scMovie.CallMethod("SET_SIGN_IN_EMAIL_STATE", "", static_cast<int>(VS_NONE));
		m_emailStatus.Reset();
		m_nicknameStatus.Reset();
	}

	UpdateSignInSubmitState();
}

void SocialClubMenu::FillOutEmail(const char* pEmail, bool canResetToDefault /*= true*/)
{
	CLiveManager::GetSocialClubMgr().GetSocialClubInfo().SetEmail(pEmail);

	if(pEmail)
	{
		if(m_emailStatus.Pending())
		{
			rlSocialClub::Cancel(&m_emailStatus);
		}
		m_emailStatus.Reset();

		if(pEmail[0] == '\0')
		{
			if(canResetToDefault)
			{
				m_scMovie.CallMethod("SET_EMAIL_ADDRESS", BuildLocTextParam("SC_EMAIL_LABEL"));
			}
		}
		else
		{
			m_scMovie.CallMethod("SET_EMAIL_ADDRESS", pEmail);

			if(m_emailStatus.HasValidCallback())
			{
				rlSocialClub::CheckEmail(NetworkInterface::GetLocalGamerIndex(), pEmail, &m_emailStatus);
			}
			else
			{
				m_emailStatus.ForceSucceeded();
			}
		}
	}
}

void SocialClubMenu::FillOutNickname(const char* pNickname, bool canResetToDefault /*= true*/)
{
	CLiveManager::GetSocialClubMgr().GetSocialClubInfo().SetNickname(pNickname);

	if(pNickname)
	{
		if(pNickname[0] == '\0')
		{
			if(canResetToDefault)
			{
				m_scMovie.CallMethod("SET_NICKNAME", BuildLocTextParam("SC_NICKNAME_LABEL"));
			}
		}
		else
		{
			m_scMovie.CallMethod("SET_NICKNAME", pNickname);

			ValidateNickname(pNickname);
		}
	}
}

void SocialClubMenu::ValidateNickname(const char* pNickname)
{
	if(m_nicknameStatus.Pending())
	{
		rlSocialClub::Cancel(&m_nicknameStatus);
	}

	m_nicknameStatus.Reset();

	if(m_nicknameStatus.HasValidCallback())
	{
		// Don't check the nickname here if we've reached our limit
		if(!m_nicknameRequestTimes.IsFull())
		{
			rlSocialClub::CheckNickname(NetworkInterface::GetLocalGamerIndex(), pNickname, &m_nicknameStatus);
			
			// Add the current time to the queue
			m_nicknameRequestTimes.Push(fwTimer::GetSystemTimeInMilliseconds());
		}
		else
		{
			// Show the nickname spinner until the cooldown period is up and it's safe to send another nickname check
			m_scMovie.CallMethod("SET_SIGN_UP_NICKNAME_STATE", "", static_cast<int>(VS_BUSY));
			m_hasPendingNicknameCheck = true;
		}
	}
	else
	{
		m_nicknameStatus.ForceSucceeded();
	}
}

void SocialClubMenu::FillOutPasswordFromKeyboard()
{
	const char* result = CControlMgr::GetVirtualKeyboardResult();

	FillOutPassword(result);
}

void SocialClubMenu::FillOutDateFromKeyboard()
{
	bool showWarning = false;
	bool success = false;
	int type = m_dobOption - DOBO_START_DATE;

	switch(m_dobOption)
	{
	case DOBO_DAY:
		{
			const char* result = CControlMgr::GetVirtualKeyboardResult();

			if(result[0] != '\0')
			{
				u8 day = (u8)atoi(result);

				formatf(m_dayBuffer, NELEM(m_dayBuffer), result);
				if(SocialClubMgr::IsDayValid(day))
				{
					success = true;
					m_day = day;
					m_scMovie.CallMethod("SET_DOB_TEXT", type, result);
				}
				else
				{
					m_day = 0;
					showWarning = true;
				}
			}

			if(!success)
			{
				m_scMovie.CallMethod("SET_DOB_TEXT", type, BuildLocTextParam("SC_DOB_DAY"));
			}
		}
		break;
	case DOBO_MONTH:
		{
			const char* result = CControlMgr::GetVirtualKeyboardResult();

			if(result[0] != '\0')
			{
				u8 month = (u8)atoi(result);

				formatf(m_monthBuffer, NELEM(m_monthBuffer), result);
				if(SocialClubMgr::IsMonthValid(month))
				{
					success = true;
					m_month = month;
					char monthBuffer[20] = {0};
					formatf(monthBuffer, "MONTH_%d", month);
					m_scMovie.CallMethod("SET_DOB_TEXT", type, monthBuffer);
				}
				else
				{
					m_month = 0;
					showWarning = true;
				}
			}

			if(!success)
			{
				m_scMovie.CallMethod("SET_DOB_TEXT", type, "SC_DOB_MONTH");
			}
		}
		break;
	case DOBO_YEAR:
		{
			const char* result = CControlMgr::GetVirtualKeyboardResult();
			if(result[0] != '\0')
			{
				u16 year = (u16)atoi(result);

				formatf(m_yearBuffer, NELEM(m_yearBuffer), result);
				if(SocialClubMgr::IsYearValid(year))
				{
					success = true;
					m_year = year;
					m_scMovie.CallMethod("SET_DOB_TEXT", type, result);
				}
				else
				{
					m_year = 0;
					showWarning = true;
				}
			}

			if(!success)
			{
				m_scMovie.CallMethod("SET_DOB_TEXT", type, BuildLocTextParam("SC_DOB_YEAR"));
			}
		}
		break;
	default:
		{
			uiAssertf(0, "SocialClubMenu::UpdateDOB - Recieved keyboard input while in an invalid state %d", m_dobOption);
		}
		break;
	}

	if(SocialClubMgr::IsDateValid(m_day, m_month, m_year))
	{
		m_scMovie.CallMethod("SET_DOB_SUBMIT_HIGHLIGHT", false);
	}
	else
	{
		if(m_day && m_month && m_year)
		{
			showWarning = true;
		}

		m_scMovie.CallMethod("SET_DOB_SUBMIT_DISABLED");
	}

	// Since warnings attempt to return to current state, make sure our current state isn't an error
	if(showWarning && m_state != SC_ERROR)
	{
		SetGenericWarning("SC_ERROR_INVALID_AGE");
	}
}

void SocialClubMenu::FillOutPassword(const char* pPassword)
{
	char buffer[ioVirtualKeyboard::MAX_STRING_LENGTH_FOR_VIRTUAL_KEYBOARD] = {0};
	size_t len = pPassword ? strlen(pPassword) : 0;
	len = MIN(ioVirtualKeyboard::MAX_STRING_LENGTH_FOR_VIRTUAL_KEYBOARD-1, len);

	CLiveManager::GetSocialClubMgr().GetSocialClubInfo().SetPassword(pPassword);

	for(int i=0; i<len; ++i)
	{
		buffer[i] = '*';
	}

	buffer[len] = 0;

	if(m_passwordStatus.Pending())
	{
		rlSocialClub::Cancel(&m_passwordStatus);
	}

	m_passwordStatus.Reset();

	if(buffer[0] == '\0')
	{
		m_scMovie.CallMethod("SET_SIGN_UP_PASSWORD", BuildLocTextParam("SC_PASSWORD_LABEL"));
		m_scMovie.CallMethod("SET_SIGN_IN_PASSWORD", BuildLocTextParam("SC_PASSWORD_LABEL"));
		m_scMovie.CallMethod("SET_SIGN_UP_PASSWORD_STATE", "", static_cast<int>(VS_NONE));
		m_scMovie.CallMethod("SET_SIGN_IN_PASSWORD_STATE", "", static_cast<int>(VS_NONE));
	}
	else
	{
		m_scMovie.CallMethod("SET_SIGN_UP_PASSWORD", buffer);
		m_scMovie.CallMethod("SET_SIGN_IN_PASSWORD", buffer);
	
		if(pPassword)
		{
			if(m_passwordStatus.HasValidCallback())
			{
				if(pPassword[0] != '\0')
				{
					rlSocialClub::CheckPassword(NetworkInterface::GetLocalGamerIndex(), pPassword, &m_passwordReqs, &m_passwordStatus);
				}
			}
			else
			{
				m_nicknameStatus.ForceSucceeded();
			}
		}
	}

	UpdateSignInSubmitState();
}

void SocialClubMenu::SuggestNickname(bool isAutoSuggest)
{
	if(!m_alternateNicknameStatus.Pending())
	{
		bool queryNicknames = isAutoSuggest;

		if(!queryNicknames)
		{
			++m_currAlt;
			if(m_currAlt < m_altCount)
			{
				FillOutNickname(m_alternateNicknames[m_currAlt]);
			}
			else
			{
				queryNicknames = true;
			}
		}

		if(queryNicknames)
		{
			m_currAlt = -1;

			if(m_alternateNicknameStatus.Pending())
			{
				rlSocialClub::Cancel(&m_alternateNicknameStatus);
			}
			m_alternateNicknameStatus.Reset();

			if(isAutoSuggest)
			{
				m_alternateNicknameStatus.SetCallback(datCallback(MFA(SocialClubMenu::AutoSuggestNicknameCallback), (datBase*)this));
			}
			else
			{
				m_alternateNicknameStatus.SetCallback(datCallback(MFA(SocialClubMenu::SuggestNicknameCallback), (datBase*)this));
			}

			uiVerify(rlSocialClub::GetAlternateNicknames(NetworkInterface::GetLocalGamerIndex(),
				m_originalNickname,
				m_alternateNicknames,
				MISCSC_MAX_ALT_NICKNAMES,
				&m_altCount,
				&m_alternateNicknameStatus));
		}
	}
}

void SocialClubMenu::SetGenericWarning(const char* pText, const char* pTitle /*= "SC_WARNING_TITLE"*/)
{
	SetGenericWarning(pText, m_state, pTitle);
}

void SocialClubMenu::SetGenericWarning(const char* pText, eSocialClubStates returnState, const char* pTitle /*= "SC_WARNING_TITLE"*/)
{
	uiDebugf1("SocialClubMenu::SetGenericWarning - text=%s, title=%s, returnState=%d", pText, pTitle, returnState);

	CWarningMessage::Data messageData;
	messageData.m_TextLabelHeading = pTitle;
	messageData.m_TextLabelBody = pText;
	messageData.m_TextLabelSubtext = "IB_OK";
	messageData.m_iFlags = FE_WARNING_OK;
	messageData.m_acceptPressed = datCallback(MFA1(SocialClubMenu::GoToState), (datBase*)this, (CallbackData)returnState);

	GoToError(messageData);
}

void SocialClubMenu::SetGenericError(const char* pText, const char* pTitle /*= "SC_ERROR_TITLE"*/)
{
	uiDebugf1("SocialClubMenu::SetGenericError - text=%s, title=%s", pText, pTitle);

	CWarningMessage::Data messageData;
	messageData.m_TextLabelHeading = pTitle;
	messageData.m_TextLabelBody = pText;
	messageData.m_TextLabelSubtext = "SC_RETURN_BUTTON";
	messageData.m_iFlags = FE_WARNING_OK;
	messageData.m_acceptPressed = datCallback(CFA(SocialClubMenu::FlagForDeletion));

	GoToError(messageData);
}

void SocialClubMenu::CheckSignUpNickEmailVerification(CallbackData function, void* pData)
{
	if(uiVerify(pData))
	{
		uiAssert(pData == &m_nicknameStatus || pData == &m_emailStatus);
		netStatus* pStatus = static_cast<netStatus*>(pData);
		const char* pASFunc = static_cast<const char*>(function);

		if(pStatus->Pending())
		{
			m_scMovie.CallMethod(pASFunc, "", static_cast<int>(VS_BUSY));
		}
		else if(pStatus->Succeeded())
		{
			m_scMovie.CallMethod(pASFunc, "", static_cast<int>(VS_VALID));
		}
		else if(pStatus->Failed())
		{
			if(pStatus == &m_nicknameStatus)
			{
				if(pStatus->GetResultCode() == RLSC_ERROR_ALREADYEXISTS_EMAILORNICKNAME || pStatus->GetResultCode() == RLSC_ERROR_ALREADYEXISTS_NICKNAME)
				{
					SuggestNickname(true);
				}
				else
				{
					m_scMovie.CallMethod(pASFunc, BuildLocTextParam(SocialClubMgr::GetErrorName(m_nicknameStatus)), static_cast<int>(VS_ERROR));
				}
			}
			else if(pStatus->GetResultCode() == RLSC_ERROR_ALREADYEXISTS_EMAILORNICKNAME || pStatus->GetResultCode() == RLSC_ERROR_ALREADYEXISTS_EMAIL)
			{
				pStatus->ForceSucceeded();
				m_scMovie.CallMethod(pASFunc, "", static_cast<int>(VS_VALID));
			}
			else
			{
				m_scMovie.CallMethod(pASFunc, BuildLocTextParam(SocialClubMgr::GetErrorName(*pStatus)), static_cast<int>(VS_ERROR));
			}
		}
		else
		{
			m_scMovie.CallMethod(pASFunc, "", static_cast<int>(VS_NONE));
		}

		UpdateSignUpSubmitState();
	}
}

void SocialClubMenu::CheckSignInNickEmailVerification(CallbackData function, void* pData)
{
	if(uiVerify(pData))
	{
		uiAssert(pData == &m_nicknameStatus || pData == &m_emailStatus);
		uiDisplayf("SocialClubMenu::CheckSignInNickEmailVerification - status=%s", (pData == &m_nicknameStatus) ? "Nickname" : "Email");

		netStatus* pStatus = static_cast<netStatus*>(pData);
		const char* pASFunc = static_cast<const char*>(function);

		if(pStatus->Pending())
		{
			uiDisplayf("SocialClubMenu::CheckSignInNickEmailVerification - Pending");
			m_scMovie.CallMethod(pASFunc, "", static_cast<int>(VS_BUSY));
		}
		else if(pStatus->Succeeded() || pStatus->Failed())
		{
			s32 resultCode = pStatus->GetResultCode();
			switch(resultCode)
			{
			case RLSC_ERROR_NONE:
				{
					uiDisplayf("SocialClubMenu::CheckSignInNickEmailVerification - RLSC_ERROR_NONE");
					m_scMovie.CallMethod(pASFunc, "", static_cast<int>(VS_VALID));
				}
				break;
			case RLSC_ERROR_ALREADYEXISTS_EMAILORNICKNAME:
			case RLSC_ERROR_ALREADYEXISTS_EMAIL:
			case RLSC_ERROR_ALREADYEXISTS_NICKNAME:
				{
					uiDisplayf("SocialClubMenu::CheckSignInNickEmailVerification - Already exists.");
					if(pStatus->Failed())
					{
						pStatus->ForceSucceeded(resultCode);
						m_scMovie.CallMethod(pASFunc, "", static_cast<int>(VS_VALID));
					}
				}
				break;
				// TODO: We should handle the following error messages for passwords by dynamically generating
				// password requirements and displaying them to the user from m_passwordReqs if
				// m_passwordReqs.bValid is true
			case RLSC_ERROR_INVALIDARGUMENT_PASSWORD:
			case RLSC_ERROR_OUTOFRANGE_PASSWORDLENGTH:
			default:
				{
					uiDisplayf("SocialClubMenu::CheckSignInNickEmailVerification - default, resultcode=%d", resultCode);
					m_scMovie.CallMethod(pASFunc, BuildLocTextParam(SocialClubMgr::GetErrorName(*pStatus)), static_cast<int>(VS_ERROR));
				}
				break;
			}
		}
		else
		{
			m_scMovie.CallMethod(pASFunc, "", static_cast<int>(VS_NONE));
		}
	}

	UpdateSignInSubmitState();
	UpdateResetPasswordSubmitState();
}

void SocialClubMenu::CheckPasswordVerification(CallbackData function, void* pData)
{
	if(uiVerify(pData))
	{
		uiAssert(pData == &m_passwordStatus);
		uiDisplayf("SocialClubMenu::CheckPasswordVerification");

		netStatus* pStatus = static_cast<netStatus*>(pData);
		const char* pASFunc = static_cast<const char*>(function);

		if(pStatus->Pending())
		{
			m_scMovie.CallMethod(pASFunc, "", static_cast<int>(VS_BUSY));
		}
		else if(pStatus->Succeeded())
		{
			m_scMovie.CallMethod(pASFunc, "", static_cast<int>(VS_VALID));
		}
		else if(pStatus->Failed())
		{
			if ((pStatus->GetResultCode() == RLSC_ERROR_OUTOFRANGE_PASSWORDLENGTH || pStatus->GetResultCode() == RLSC_ERROR_INVALIDARGUMENT_PASSWORD))
			{
				if(m_passwordReqs.bValid)
				{
					atString validSymbolsQuoted;
					size_t validSymLen = strlen(m_passwordReqs.ValidSymbols);

					for(int i = 0; i < validSymLen; ++i)
					{
						if (m_passwordReqs.ValidSymbols[i] == FONT_CHAR_TOKEN_DELIMITER)
						{
							validSymbolsQuoted += FONT_CHAR_TOKEN_ESCAPE;
							validSymbolsQuoted += m_passwordReqs.ValidSymbols[i];
						}
						else
						{
							validSymbolsQuoted += m_passwordReqs.ValidSymbols[i];
						}

						if(i < validSymLen - 1)
						{
							validSymbolsQuoted += ' ';
						}
					}

					CSubStringWithinMessage validCharactersQuoted;
					validCharactersQuoted.SetLiteralString(validSymbolsQuoted.c_str(), CSubStringWithinMessage::LITERAL_STRING_TYPE_FOR_IMMEDIATE_USE_LONG, false);

					CNumberWithinMessage sizeBounds[2];
					sizeBounds[0].Set(static_cast<int>(m_passwordReqs.MinLength));
					sizeBounds[1].Set(static_cast<int>(m_passwordReqs.MaxLength));

					char msgBuffer[1024] = {0};
					CMessages::InsertNumbersAndSubStringsIntoString(TheText.Get("SC_ERROR_INVALID_PASS_REGS"), sizeBounds, NELEM(sizeBounds), &validCharactersQuoted, 1, msgBuffer, NELEM(msgBuffer));
					m_scMovie.CallMethod(pASFunc, msgBuffer, static_cast<int>(VS_ERROR));
				}
				else
				{
					// The RLSC_ERROR_INVALIDARGUMENT_PASSWORD error isn't the right thing for this screen.
					m_scMovie.CallMethod(pASFunc, BuildLocTextParam(SocialClubMgr::GetErrorName(RLSC_ERROR_OUTOFRANGE_PASSWORDLENGTH)), static_cast<int>(VS_ERROR));
				}
			}
			else
			{
				m_scMovie.CallMethod(pASFunc, BuildLocTextParam(SocialClubMgr::GetErrorName(*pStatus)), static_cast<int>(VS_ERROR));
			}
		}
		else
		{
			m_scMovie.CallMethod(pASFunc, "", static_cast<int>(VS_NONE));
		}

		UpdateSignUpSubmitState();
		UpdateSignInSubmitState();
	}
}

void SocialClubMenu::UpdateSignUpSubmitState()
{
	bool canSubmit = m_emailStatus.Succeeded() && m_nicknameStatus.Succeeded() && m_passwordStatus.Succeeded();

	if(canSubmit)
	{
		m_scMovie.CallMethod("SET_SIGN_UP_SUBMIT_HIGHLIGHT", m_suOption == SUO_SUBMIT);
	}
	else
	{
		m_scMovie.CallMethod("SET_SIGN_UP_SUBMIT_DISABLED");
		if(m_suOption == SUO_SUBMIT)
		{
			m_scMovie.CallMethod("SET_SIGN_UP_NICKNAME_HIGHLIGHT", true);
			m_suOption = SUO_NICKNAME;
		}
	}
}

void SocialClubMenu::UpdateSignInSubmitState()
{
	bool canSubmit = (m_emailStatus.Succeeded() || m_nicknameStatus.Succeeded()) &&
		(CLiveManager::GetSocialClubMgr().GetSocialClubInfo().GetPassword()[0] != '\0');

	if(canSubmit)
	{
		m_scMovie.CallMethod("SET_SIGN_IN_SUBMIT_HIGHLIGHT", m_siOption == SIO_SUBMIT);
	}
	else
	{
		m_scMovie.CallMethod("SET_SIGN_IN_SUBMIT_DISABLED");
	}

}

void SocialClubMenu::UpdateResetPasswordSubmitState()
{
	if(m_emailStatus.Succeeded())
	{
		m_scMovie.CallMethod("SET_FORGOT_PASSWORD_SUBMIT_HIGHLIGHT", m_rpOption == RPO_SUBMIT);
	}
	else
	{
		m_scMovie.CallMethod("DISABLE_FORGOT_PASSWORD_SUBMIT_HIGHLIGHT");
		if(m_rpOption == RPO_SUBMIT)
		{
			m_scMovie.CallMethod("SET_FORGOT_PASSWORD_EMAIL_HIGHLIGHT", true);
			m_rpOption = RPO_EMAIL;
		}
	}
}

void SocialClubMenu::AccountCreationCallback(void* pData)
{
	if(uiVerify(pData))
	{
		netStatus* pStatus = static_cast<netStatus*>(pData);
		if(pStatus->Succeeded())
		{
			m_gotCredsResult = false;
			AcceptPolicyVersions(m_policyManager.AreTextsInSyncWithCloud());
		}
		else if(pStatus->Failed())
		{
			if(pStatus->GetResultCode() == RLSC_ERROR_INVALIDARGUMENT_EMAIL)
			{
				SetGenericWarning("SC_ERROR_INVALID_EMAIL_SUB", SC_SIGNUP);
			}
			else
			{
				SetGenericWarning(SocialClubMgr::GetErrorName(*pStatus), SC_SIGNUP);
			}
		}
		else if(pStatus->Canceled())
		{
			GoToPrevious();
		}
	}
}

void SocialClubMenu::AccountLinkCallback(void* pData)
{
	if(uiVerify(pData))
	{
		netStatus* pStatus = static_cast<netStatus*>(pData);
		if(pStatus->Succeeded())
		{
			m_gotCredsResult = false;
			AcceptPolicyVersions(m_policyManager.AreTextsInSyncWithCloud());
		}
		else if(pStatus->Failed())
		{
			SetGenericWarning(SocialClubMgr::GetErrorName(*pStatus), static_cast<eSocialClubStates>(m_state-1));
		}
		else if(pStatus->Canceled())
		{
			GoToPrevious();
		}
	}
}

void SocialClubMenu::ScAuthSignupCallback(void* pData)
{
	if (uiVerify(pData))
	{
		netStatus* pStatus = static_cast<netStatus*>(pData);
		if (pStatus->Succeeded())
		{
			uiDisplayf("ScAuth signup successful, accepting policy versions");

			// Get nickname from ROS credentials
			const rlRosCredentials& rosCreds = rlRos::GetCredentials(NetworkInterface::GetLocalGamerIndex());
			if (rosCreds.IsValid())
			{
				const rlScAccountInfo& accountInfo = rosCreds.GetRockstarAccount();

				// Set the SC Info
				CLiveManager::GetSocialClubMgr().GetSocialClubInfo().SetNickname(accountInfo.m_Nickname);
			}

			AcceptPolicyVersions(m_policyManager.AreTextsInSyncWithCloud());
			GoToState(SC_AUTH_SIGNUP_DONE);
		}
		else
		{
			// First, check to make sure we're not offline.  If we're offline, we want to set a generic error such 
			// that we exit on accept rather than go back to the "Join the Club" screen.
			const rlGamerInfo* pGamerInfo = NetworkInterface::GetActiveGamerInfo();
			bool isOnline = pGamerInfo && pGamerInfo->IsOnline();

			if (!isOnline)
			{
				SSocialClubMenu::GetInstance().SetGenericError( "HUD_DISCONNOLOGO", "SC_ERROR_TITLE" );
			}
			else if ( pStatus->Failed() )
			{
				// If the link failed, display a generic message about account details. We don't know
				// if the user simply did not choose to link their account.
				SetGenericWarning( SocialClubMgr::GetErrorName( RLSC_ERROR_INVALIDARGUMENT_ROCKSTARID ), SC_WELCOME );
			}
			else if (pStatus->Canceled())
			{
				GoToState( SC_WELCOME );
			}
		}
	}
}

void SocialClubMenu::ScAuthSigninCallback(void* pData)
{
	if (uiVerify(pData))
	{
		netStatus* pStatus = static_cast<netStatus*>(pData);
		if (pStatus->Succeeded())
		{
			uiDisplayf("ScAuth signin successful, accepting policy versions");

			// Get nickname from ROS credentials
			const rlRosCredentials& rosCreds = rlRos::GetCredentials(NetworkInterface::GetLocalGamerIndex());
			if (rosCreds.IsValid())
			{
				const rlScAccountInfo& accountInfo = rosCreds.GetRockstarAccount();

				// Set the SC Info
				CLiveManager::GetSocialClubMgr().GetSocialClubInfo().SetNickname(accountInfo.m_Nickname);
			}

			AcceptPolicyVersions(m_policyManager.AreTextsInSyncWithCloud());
			GoToState(SC_AUTH_SIGNIN_DONE);
		}
		else
		{
			// First, check to make sure we're not offline.  If we're offline, we want to set a generic error such 
			// that we exit on accept rather than go back to the "Join the Club" screen.
			const rlGamerInfo* pGamerInfo = NetworkInterface::GetActiveGamerInfo();
			bool isOnline = pGamerInfo && pGamerInfo->IsOnline();

			if (!isOnline)
			{
				SetGenericError( "HUD_DISCONNOLOGO", "SC_ERROR_TITLE" );
			}
			else if (pStatus->Failed())
			{
				// If the link failed, display a generic message about account details. We don't know
				// if the user simply did not choose to link their account.
				SetGenericWarning( SocialClubMgr::GetErrorName( RLSC_ERROR_INVALIDARGUMENT_ROCKSTARID ), SC_WELCOME );
			}
			else if (pStatus->Canceled())
			{
				GoToState( SC_WELCOME );
			}
		}
	}
}

void SocialClubMenu::AccountResetPasswordCallback(void* pData)
{
	if(uiVerify(pData))
	{
		netStatus* pStatus = static_cast<netStatus*>(pData);
		if(pStatus->Succeeded())
		{
			uiDisplayf("SocialClubMenu::AccountResetPasswordCallback - Succeeded");
			GoToNext();
		}
		else if(pStatus->Failed())
		{
			uiDisplayf("SocialClubMenu::AccountResetPasswordCallback - Failed, resultCode=%d", pStatus->GetResultCode());

			switch(pStatus->GetResultCode())
			{
			case RLSC_ERROR_DOESNOTEXIST_PLAYERACCOUNT:
			case RLSC_ERROR_DOESNOTEXIST_ROCKSTARACCOUNT:
				uiDisplayf("SocialClubMenu::AccountResetPasswordCallback - Account doesn't exist.");
				GoToNext();
				break;

			default:
				SetGenericWarning(SocialClubMgr::GetErrorName(*pStatus), static_cast<eSocialClubStates>(m_state-1));
				break;
			};
		}
		else if(pStatus->Canceled())
		{
			uiDisplayf("SocialClubMenu::AccountResetPasswordCallback - Canceled");
			GoToPrevious();
		}
	}
}

void SocialClubMenu::SuggestNicknameCallback()
{
	if(m_alternateNicknameStatus.Succeeded())
	{
		if(0 < m_altCount)
		{
			m_currAlt = 0;
			FillOutNickname(m_alternateNicknames[m_currAlt]);
		}
		else
		{
			m_scMovie.CallMethod("SET_SIGN_UP_NICKNAME_STATE", BuildLocTextParam("SC_NO_NICKNAMES"), static_cast<int>(VS_STATUS_QUO));
		}
	}
	else if(m_alternateNicknameStatus.Pending())
	{
		m_scMovie.CallMethod("SET_SIGN_UP_NICKNAME_STATE", "", static_cast<int>(VS_BUSY));
	}
	else if(m_alternateNicknameStatus.Failed() || m_alternateNicknameStatus.Canceled())
	{
		m_scMovie.CallMethod("SET_SIGN_UP_NICKNAME_STATE", BuildLocTextParam(SocialClubMgr::GetErrorName(m_alternateNicknameStatus)), static_cast<int>(VS_STATUS_QUO));
	}
}

void SocialClubMenu::AutoSuggestNicknameCallback()
{
	if((m_alternateNicknameStatus.Succeeded() || m_alternateNicknameStatus.Failed() || m_alternateNicknameStatus.Canceled()))
	{
		if(0 < m_altCount)
		{
			char msgBuffer[200] = {0};
			CSubStringWithinMessage sub[MISCSC_MAX_ALT_NICKNAMES];
			for(int i=0; i<m_altCount; ++i)
			{
				sub[i].SetLiteralString(m_alternateNicknames[i], CSubStringWithinMessage::LITERAL_STRING_TYPE_FOR_IMMEDIATE_USE, false);
			}

			CMessages::InsertNumbersAndSubStringsIntoString(TheText.Get("SC_ERROR_SUGGEST_NICK"), NULL, 0, sub, m_altCount, msgBuffer, NELEM(msgBuffer));
			m_scMovie.CallMethod("SET_SIGN_UP_NICKNAME_STATE", msgBuffer, static_cast<int>(VS_ERROR));
		}
		else
		{
			m_scMovie.CallMethod("SET_SIGN_UP_NICKNAME_STATE", BuildLocTextParam(SocialClubMgr::GetErrorName(m_nicknameStatus)), static_cast<int>(VS_ERROR));
		}
	}
}


#if DEBUG_SC_MENU
void SocialClubMenu::Debug_AutofillAccountInfo()
{
	CLiveManager::GetSocialClubMgr().Debug_AutofillAccountInfo();
	FillOutEmail(CLiveManager::GetSocialClubMgr().GetSocialClubInfo().GetEmail());
	FillOutNickname(CLiveManager::GetSocialClubMgr().GetSocialClubInfo().GetNickname());
	FillOutPassword(CLiveManager::GetSocialClubMgr().GetSocialClubInfo().GetPassword());

	m_day = 7;
	m_month = 5;
	m_year = 1980;
	CLiveManager::GetSocialClubMgr().GetSocialClubInfo().SetAge(m_day, m_month, m_year);

	formatf(m_originalNickname, RLSC_MAX_NICKNAME_CHARS, "%s", CLiveManager::GetSocialClubMgr().GetSocialClubInfo().GetNickname());
}
#endif

// eof
