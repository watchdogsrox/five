
#include "ReportMenu.h"
//#include "ReportMenuData_parser.h"
#include "rline/rlgamerinfo.h"
#include "rline/rltitleid.h"
#include "Network/Live/livemanager.h"
#include "parser/manager.h"
#include "parser/macros.h"
#include "parsercore/settings.h"
#include "net/status.h"
#include "text/text.h"

#include "Frontend/ui_channel.h"
#include "Frontend/PauseMenu.h"
#include "Frontend/ContextMenu.h"
#include "system/controlMgr.h"
#include "script/script.h"
#include "stats/StatsInterface.h"
#include "Network/Cloud/Tunables.h"
#include "Network/Live/NetworkTelemetry.h"

#define REPORTMENU_DATA_XML_FILENAME	"common:/data/ui/reportplayer.xml"
#define REPORTMENU_MOVIE_NAME			"MP_RESULTS_PANEL"

#define MAX_CUSTOM_COMPLAINT_LENGTH (64)

netStatus CReportMenu::sm_NetStatusPool[MAX_SIMULTANEOUS_COMPLAINT];

CReportMenu::CReportMenu(): m_bAllowCancel(true),
m_bActive(false),
m_bInactiveThisFrame(false),
m_bWasPauseMenuActive(false),
m_bInSystemKeyboard(false),
m_iSelectedIndex(0),
m_iSelectedMenu(0),
m_iStoredCallbackIndex(0),
m_iSelectedMissionIndex(0),
m_iReportAbuseCountThreshold(0),
m_iReportAbuseTimeThreshold(0),
m_eReportType(ReportType_None),
m_eMenuType(eReportMenu_Invalid),
m_NetStatusCallback(NULL),
m_iInputDisableDuration(500U) // disable all inputs for half a second when the warning screen exits!
{
	m_ButtonFlags = static_cast<eWarningButtonFlags>(FE_WARNING_CONFIRM | FE_WARNING_BACK);
	m_szCustomReportInfo = rage_new char[MAX_CUSTOM_COMPLAINT_LENGTH];
	m_Name = rage_new char[RL_MAX_DISPLAY_NAME_BUF_SIZE];
	memset(m_szCustomReportInfo,'\0',sizeof(char)*MAX_CUSTOM_COMPLAINT_LENGTH);
	memset(m_Name,'\0',sizeof(char)*RL_MAX_DISPLAY_NAME_BUF_SIZE);
	
	m_GamerHandle.Clear();
	m_AbuseArray.Reset();

	// bind query delegate
	m_QueryMissionsDelegate.Bind(this, &CReportMenu::OnRecentMissionsContentResult);
	// reset the status member
	m_QueryMissionsStatus.Reset();

	m_QueryMissionsOp = eUgcMissionsOp_Idle;
}

CReportMenu::~CReportMenu()
{
	delete m_szCustomReportInfo;
	m_szCustomReportInfo = NULL;

	if (m_Name)
	{
		delete m_Name;
		m_Name = NULL;
	}

	if (m_NetStatusCallback)
	{
		delete m_NetStatusCallback;
		m_NetStatusCallback = NULL;
	}
	
}

void CReportMenu::Open(const rlGamerHandle& rlHandle, eMenuType eType)
{
	reportDisplayf("CReportMenu::Open");

	m_HistoryStack.Reset();
	m_GamerHandle.Clear();

	m_GamerHandle = rlHandle;

	CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerHandle(m_GamerHandle);
	
	if (pPlayer)
	{
		#if RSG_DURANGO
		if (pPlayer->GetGamerInfo().HasDisplayName())
		{
			formatf(m_Name, RL_MAX_DISPLAY_NAME_BUF_SIZE,"%s",pPlayer->GetGamerInfo().GetDisplayName());
		}
		else
		#endif
		{
			formatf(m_Name, RL_MAX_DISPLAY_NAME_BUF_SIZE,"%s",pPlayer->GetGamerInfo().GetName());
		}

	}
	
	m_iReportAbuseCountThreshold = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("REPORT_ABUSE_COUNT_THRESHOLD", 0x082623a4), 2);
	
	LoadDataXMLFile(eType);
	

	#if RSG_PC
	CMousePointer::SetMouseCursorStyle(MOUSE_CURSOR_STYLE_ARROW);
	CMousePointer::SetMouseCursorVisible(true);
	#endif

	m_ButtonFlags = static_cast<eWarningButtonFlags>(FE_WARNING_CONFIRM | FE_WARNING_BACK);
}

void CReportMenu::Close()
{
	reportDisplayf("CReportMenu::Close");

	
	#if RSG_PC
	CMousePointer::SetMouseCursorVisible(false);
	#endif
	m_iStoredCallbackIndex = 0;
	memset(m_szCustomReportInfo,'0',sizeof(char)*MAX_CUSTOM_COMPLAINT_LENGTH);

	RemoveReportMovie();
}
//****************************************************************************************
// LoadDataXMLFile - Loads reportplayer.xml into our menu structure
//****************************************************************************************
bool CReportMenu::LoadDataXMLFile(eMenuType eType)
{
	reportDisplayf("CReportMenu::LoadDataXMLFile");

	// initialize any arrays:
	m_eMenuType = eType;

	parSettings settings = PARSER.Settings();
	DEV_ONLY( settings.SetFlag(parSettings::READ_SAFE_BUT_SLOW, true); )
	settings.SetFlag(parSettings::CULL_OTHER_PLATFORM_DATA, true);
	
	if( !PARSER.LoadObject(REPORTMENU_DATA_XML_FILENAME, "", m_MenuArray, &settings) )
			return false;

	if (m_eMenuType == eReportMenu_CommendMenu)
	{
		m_HistoryStack.PushAndGrow(ReportLink_None);
		UpdateMenuOptions(ReportLink_CommendRoot);
	}
	else if ( m_eMenuType == eReportMenu_ReportMenu)
	{
		m_HistoryStack.PushAndGrow(ReportLink_None);
		UpdateMenuOptions(ReportLink_ReportRoot);
	}
	else
	{
		m_HistoryStack.PushAndGrow(ReportLink_None);
		UpdateMenuOptions(ReportLink_UGCReportRoot);
	}

	return true;
}

//****************************************************************************************
// UpdateMenuOptions - Moves to the appropriate page based on the link
//****************************************************************************************
void CReportMenu::UpdateMenuOptions(eReportLink eToMoveTo)
{
	// These menus are dynamically created.  Populate depending on the screen
	if (eToMoveTo == ReportLink_UGCReportRoot )
	{
		reportDisplayf("CReportMenu::UpdateMenuOptions - setting m_iSelectedMenu to ReportLink_UGCReportRoot");
		m_iSelectedMenu = ReportLink_UGCReportRoot;

		if (m_QueryMissionsOp != eUgcMissionsOp_Pending)
		{
			m_NumberOfMissionsInArray = 0;
			m_nContentTotal = 0;

			m_QueryMissionsStatus.Reset();
			m_QueryMissionsOp = eUgcMissionsOp_Idle;

			reportDisplayf("CReportMenu::UpdateMenuOptions - about to call GetMyRecentlyPlayedContent");

            char szQueryParams[256];
            formatf(szQueryParams, sizeof(szQueryParams), "{lang:[%s]}", CText::GetStringOfSupportedLanguagesFromCurrentLanguageSetting());

			if (UserContentManager::GetInstance().QueryContent(RLUGC_CONTENT_TYPE_GTA5MISSION, "GetMyRecentlyPlayedContent", szQueryParams, 0, MAX_WARNING_MISSIONS, &m_nContentTotal, NULL, m_QueryMissionsDelegate, &m_QueryMissionsStatus))
			{
				reportDisplayf("CReportMenu::UpdateMenuOptions - GetMyRecentlyPlayedContent has successfully started. Setting m_QueryMissionsOp to eUgcMissionsOp_Pending");
				m_QueryMissionsOp = eUgcMissionsOp_Pending;
			}
		}
		else
		{
			reportDisplayf("CReportMenu::UpdateMenuOptions - m_QueryMissionsOp is %d so GetMyRecentlyPlayedContent has been skipped", (int) m_QueryMissionsOp);
		}

		LoadReportMovie();
	}
	else
	{
		int iMenuScreensCount = m_MenuArray.m_ReportLists.GetCount();

		for (int i = 0; i < iMenuScreensCount; i++)
		{
			if (m_MenuArray.m_ReportLists[i].ReportLinkId == eToMoveTo)
			{
				Populate(m_MenuArray.m_ReportLists[i]);
				m_iSelectedMenu = i;
			}
		}
	}
}

//****************************************************************************************
// Populate - Fills out the report page according to the list information
//****************************************************************************************
void CReportMenu::Populate(CReportList& list)
{
	m_NameHashArray.Reset();
	m_CallbackArray.Reset();

	m_ButtonFlags = static_cast<eWarningButtonFlags>(FE_WARNING_CONFIRM | FE_WARNING_BACK);

	for (int i = 0; i < list.ReportOptions.GetCount(); i++)
	{
		if (list.ReportOptions[i].ReportLink == ReportLink_Crew)
		{
			const rlClanDesc* pClanDesc = CLiveManager::GetNetworkClan().GetPrimaryClan(m_GamerHandle);
			if (pClanDesc && !pClanDesc->IsValid())
			{
				continue;
			}
		}

		if (list.ReportOptions[i].ReportLink == ReportLink_PlayerMadeMissions)
		{
			if (strcmp(CTheScripts::GetContentIdOfCurrentUGCMission(),"") == 0)
				continue;
		}

		m_NameHashArray.PushAndGrow(list.ReportOptions[i].LabelHash.GetHash());

		switch (list.ReportOptions[i].ReportType)
		{
			// Negative report callbacks
			case ReportType_Exploit:
			{
				m_CallbackArray.PushAndGrow(datCallback(CContextMenuHelper::clbReportForExploit));
				break;
			}
			case ReportType_OffensiveUGC:
			{
				m_CallbackArray.PushAndGrow(datCallback(CContextMenuHelper::clbReportForOffensiveUGC));
				break;
			}
			case ReportType_OffensiveLicensePlate:
			{
				m_CallbackArray.PushAndGrow(datCallback(CContextMenuHelper::clbReportForOffensiveTagPlate));
				break;
			}
			case ReportType_OffensiveLanguage:
			{
				m_CallbackArray.PushAndGrow(datCallback(CContextMenuHelper::clbReportForOffensiveLang));
				break;
			}
			case ReportType_Griefing:
			{
				m_CallbackArray.PushAndGrow(datCallback(CContextMenuHelper::clbReportForGriefing));
				break;
			}
			case ReportType_GameExploit:
			{
				m_CallbackArray.PushAndGrow(datCallback(CContextMenuHelper::clbReportForGameExploit));
				break;
			}
			case ReportType_None:
			{
				
				m_CallbackArray.PushAndGrow(datCallback(CContextMenuHelper::clbReportForGriefing));
				break;
			}
			case ReportType_OffensiveCrewName:
			case ReportType_OffensiveCrewMotto:
			case ReportType_OffensiveCrewStatus:
			case ReportType_OffensiveCrewEmblem:
			case ReportType_PlayerMade_Title:
			case ReportType_PlayerMade_Description:
			case ReportType_PlayerMade_Photo:
			case ReportType_PlayerMade_Content:
			{
				m_CallbackArray.PushAndGrow(datCallback(CContextMenuHelper::clbReportForUGCContent));
				break;
			}
			case ReportType_VoiceChat_Annoying:
			{
				m_CallbackArray.PushAndGrow(datCallback(CContextMenuHelper::clbReportForVCAnnoying));
				break;
			}
			case ReportType_VoiceChat_Hate:
			{
				m_CallbackArray.PushAndGrow(datCallback(CContextMenuHelper::clbReportForVCHate));
				break;
			}
			case ReportType_TextChat_Annoying:
			{
				m_CallbackArray.PushAndGrow(datCallback(CContextMenuHelper::clbReportForTextChatAnnoying));
				break;
			}
			case ReportType_TextChat_Hate:
			{
				m_CallbackArray.PushAndGrow(datCallback(CContextMenuHelper::clbReportForTextChatHate));
				break;
			}
			case ReportType_Other:
			{
				m_CallbackArray.PushAndGrow(datCallback(CContextMenuHelper::clbReportForVCHate));
				break;
			}

			//Positive 
			case ReportType_Friendly:
			{
				m_CallbackArray.PushAndGrow(datCallback(CContextMenuHelper::clbCommendForFriendly));
				break;
			}
			case ReportType_Helpful:
			{
				m_CallbackArray.PushAndGrow(datCallback(CContextMenuHelper::clbCommendForHelpful));
				break;
			}
		}
	}


	ShowReportScreen(list.SubtitleHash);
}

//****************************************************************************************
// ResetAbuseList - 
//****************************************************************************************
void CReportMenu::ResetAbuseList()
{
		SReportMenu::GetInstance().ResetMenuAbuseList();
}

void CReportMenu::ResetMenuAbuseList()
{
	m_AbuseArray.Reset();
}
//****************************************************************************************
// Update - A stub update that encapsulates the singleton so we can use gameskeleton.
//****************************************************************************************
void CReportMenu::Update()
{
	SReportMenu::GetInstance().UpdateMenu();

    // This class doesnt handle the result of the requests (fire and forget) so we can reset the status, no one is checking for it
    for(int i=0; i<MAX_SIMULTANEOUS_COMPLAINT; i++)
    {
        if(sm_NetStatusPool[i].GetStatus() != netStatus::NET_STATUS_PENDING)
        {
            sm_NetStatusPool[i].Reset();
        }
    }
}

bool CReportMenu::IsReportAbusive()
{
	s8 uIndex = -1;

	for (s8 i = 0; i < m_AbuseArray.GetCount(); i++)
	{
		if (m_AbuseArray[i].handle == m_GamerHandle)
		{
			uIndex = i;
			break;
		}
	}

	if (uIndex != -1)
	{
		m_AbuseArray[uIndex].reportCount++;

		if (m_AbuseArray[uIndex].reportCount >= m_iReportAbuseCountThreshold)
		{
			char szUserId[64] = {0};
			char szReportedId[64] = {0};
			
			#if RSG_XENON || RSG_DURANGO
			s64 sUserId = m_GamerHandle.GetXuid();
			s64 sReportedId = NetworkInterface::GetLocalGamerHandle().GetXuid();

			formatf(szUserId,64, "%" I64FMT "u",sUserId );
			formatf(szReportedId, 64,  "%" I64FMT "u",sReportedId );

			#elif RSG_PS3 || RSG_ORBIS
			formatf(szUserId,64, "%s",m_GamerHandle.GetNpOnlineId().data );
			formatf(szReportedId,64, "%s",NetworkInterface::GetLocalGamerHandle().GetNpOnlineId().data );

			#elif RSG_PC
			s64 sUserId = m_GamerHandle.GetRockstarId();
			s64 sReportedId = NetworkInterface::GetLocalGamerHandle().GetRockstarId();

			formatf(szUserId,64, "%" I64FMT "u",sUserId );
			formatf(szReportedId,64, "%" I64FMT "u",sReportedId );

			#else
			Assert(0);
			#endif
			CNetworkTelemetry::AppendMetricReporter(szUserId, szReportedId, m_iStoredCallbackIndex , m_AbuseArray[uIndex].reportCount);
		}
	}
	else
	{
		sAbuseInfo info;
		info.Init(m_GamerHandle, 1);

		m_AbuseArray.PushAndGrow(info);

		return false;
	}

	return uIndex == -1;
}

//****************************************************************************************
// UpdateMenu - Updates our report system.
//****************************************************************************************
void CReportMenu::UpdateMenu()
{
	PF_AUTO_PUSH_TIMEBAR("CReportMenu Update");

	if (IsActive() && m_Movie.IsActive())
	{
		if (m_bWasPauseMenuActive && (!CPauseMenu::IsActive() || CLiveManager::GetInviteMgr().IsAwaitingInviteConfirm()))
		{
			RemoveReportMovie();
			m_bWasPauseMenuActive = false;
			return;
		}

		m_bInactiveThisFrame = true;

		CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_REPORT_SCREEN, "",  "", m_ButtonFlags, false, -1, NULL, NULL, false);  // no background

		UpdateUGCFlow();
		UpdateInput();

	}
	else if(m_bInactiveThisFrame)
	{
		m_bInactiveThisFrame = false;
		CControlMgr::GetMainPlayerControl(false).DisableAllInputs(m_iInputDisableDuration);
	}

	m_bWasPauseMenuActive = CPauseMenu::IsActive();
}

#if RSG_PC
void CReportMenu::UpdateMouseEvents(const GFxValue* args)
{
	if (args[4].IsNumber() && (s32)args[4].GetNumber() != m_iSelectedIndex)
	{
		if ( (s32)args[4].GetNumber() > m_iSelectedIndex )
		{
			CPauseMenu::PlayInputSound(FRONTEND_INPUT_UP);
		}

		if ( (s32)args[4].GetNumber() < m_iSelectedIndex )
		{
			CPauseMenu::PlayInputSound(FRONTEND_INPUT_DOWN);
		}


		if(m_Movie.BeginMethod("SET_SLOT_STATE"))
		{
			m_Movie.AddParam(m_iSelectedIndex);
			m_Movie.AddParam(0);
			m_Movie.EndMethod();
		}

		m_iSelectedIndex = (s32)args[4].GetNumber();
	}

	if(m_Movie.BeginMethod("SET_SLOT_STATE"))
	{
		m_Movie.AddParam(m_iSelectedIndex);
		m_Movie.AddParam((int)(BIT(0)));
		m_Movie.EndMethod();
	}

	const sScaleformMouseEvent evt = CMousePointer::GetLastMouseEvent();

	if(evt.iMovieID != SF_INVALID_MOVIE )
	{
		if (evt.evtType == eMOUSE_EVT::EVENT_TYPE_MOUSE_RELEASE)
		{
			OnAcceptInput();
		}
	}
}
#endif
//***********************************************************************************************
// UpdateUGCFlow - Updates UGC flow for report menu when we need to sent complaint to SC backend.
//***********************************************************************************************
void CReportMenu::UpdateUGCFlow()
{

	if (CControlMgr::UpdateVirtualKeyboard() != ioVirtualKeyboard::kKBState_PENDING && m_bInSystemKeyboard)
	{
		if (CControlMgr::GetVirtualKeyboardState() == ioVirtualKeyboard::kKBState_SUCCESS)
		{
			formatf(m_szCustomReportInfo, MAX_CUSTOM_COMPLAINT_LENGTH,"%s",CControlMgr::GetVirtualKeyboardResult());

			// Store which callback to use
			m_bInSystemKeyboard = false;
			m_iStoredCallbackIndex = m_iSelectedIndex;
			
			if (m_eReportType == ReportType_None)
				m_eReportType = ReportType_Other;

			m_HistoryStack.PushAndGrow(m_MenuArray.m_ReportLists[m_iSelectedMenu].ReportLinkId);
			UpdateConfirmationScreen();
		}
	}

	if (m_NetStatus.GetStatus() != netStatus::NET_STATUS_NONE)
	{
		if (m_NetStatus.GetStatus() == netStatus::NET_STATUS_SUCCEEDED || m_NetStatus.GetStatus() == netStatus::NET_STATUS_FAILED)
		{

			if (m_NetStatusCallback)
			{
				m_NetStatusCallback->Call();
				delete m_NetStatusCallback;
				m_NetStatusCallback = NULL;
			}

			m_NetStatus.Reset();
		}
	}

	if(m_QueryMissionsOp == eUgcMissionsOp_Pending && !m_QueryMissionsStatus.Pending())
	{
		if (m_iSelectedMenu == ReportLink_UGCReportRoot)
		{
			if(m_QueryMissionsStatus.Succeeded())
			{
				reportDisplayf("CReportMenu::UpdateUGCFlow - Recent mission request has succeeded. m_NumberOfMissionsInArray is %u", m_NumberOfMissionsInArray);

				PopulateMissionPage();
			}
			else
			{
				reportDisplayf("CReportMenu::UpdateUGCFlow - Recent mission request has failed");

				UpdateThankYouScreen();
			}
		}
		else
		{
			reportDisplayf("CReportMenu::UpdateUGCFlow - Recent mission request has finished but m_iSelectedMenu isn't ReportLink_UGCReportRoot so don't try to populate the list of mission names");
		}

		m_QueryMissionsOp = eUgcMissionsOp_Finished;
	}
}


//****************************************************************************************
// Render - A stub render that encapsulates the singleton so we can use gameskeleton.
//****************************************************************************************
void CReportMenu::Render()
{
		SReportMenu::GetInstance().RenderMenu();
}

//****************************************************************************************
// Render - A stub render that encapsulates the singleton so we can use gameskeleton.
//****************************************************************************************
void CReportMenu::RenderMenu()
{
	if (IsActive() && m_Movie.IsActive())
	{
		if (CWarningScreen::CanRender())
			CWarningScreen::Render();

		m_Movie.Render(true);

	}


}

//****************************************************************************************
// LoadReportMovie
//****************************************************************************************
void CReportMenu::LoadReportMovie()
{
	if (!m_bActive)
	{
		m_Movie.CreateMovieAndWaitForLoad(SF_BASE_CLASS_GENERIC, REPORTMENU_MOVIE_NAME, Vector2(0.0f,0.0f), Vector2(1.0f, 1.0f));
		CScaleformMgr::ForceMovieUpdateInstantly(m_Movie.GetMovieID(), true);
		m_bActive = true;
	}
}

//****************************************************************************************
// RemoveReportMovie
//****************************************************************************************
void CReportMenu::RemoveReportMovie()
{
	reportDisplayf("CReportMenu::RemoveReportMovie");

	if(m_Movie.IsActive())
	{ 
		m_Movie.RemoveMovie();

		m_bActive = false;

		if(CPauseMenu::IsActive() && CPauseMenu::GetDisplayPed() && CPauseMenu::GetCurrentScreenData().GetDynamicMenu() && CPauseMenu::GetCurrentScreenData().GetDynamicMenu()->GetMenuScreenId() != MENU_UNIQUE_ID_MISSION_CREATOR)
		{
			CPauseMenu::GetDisplayPed()->SetVisible(true);
		}
	}

//	m_QueryMissionsOp = eUgcMissionsOp_Idle;		//	Graeme - I'll try without this just now.
}

//****************************************************************************************
// OnAcceptInput
//****************************************************************************************
void CReportMenu::OnAcceptInput()
{

		// ReportLink_UGCReportRoot is a special flow - the screen is created dynamically via UGC results thus doesn't exist in the ReportOptions.
		// We need to check specifically for this to handle proper flow in code.
		if (m_iSelectedMenu == ReportLink_UGCReportRoot)
		{
			if (m_QueryMissionsOp == eUgcMissionsOp_Finished)
			{
				if ( (m_iSelectedIndex >= 0) && (m_iSelectedIndex < m_NumberOfMissionsInArray) )
				{
					reportDisplayf("CReportMenu::UpdateInput - player has pressed Accept so move on to ReportLink_PlayerMadeMissions. m_iSelectedIndex=%d", m_iSelectedIndex);

					m_HistoryStack.PushAndGrow(ReportLink_UGCReportRoot);
					m_iSelectedMissionIndex = m_iSelectedIndex;
					UpdateMenuOptions(ReportLink_PlayerMadeMissions);
				}
				else
				{
					reportDisplayf("CReportMenu::UpdateInput - player has pressed Accept but m_iSelectedIndex=%d. It's out of the range 0 to %d so do nothing", m_iSelectedIndex, m_NumberOfMissionsInArray);
				}
			}
		}
		else
		{
			eReportLink eNewLink = m_MenuArray.m_ReportLists[m_iSelectedMenu].ReportOptions[m_iSelectedIndex].ReportLink;
		
			if (eNewLink == ReportLink_None)
			{
				m_iStoredCallbackIndex = 0;
				memset(m_szCustomReportInfo,'0',sizeof(char)*MAX_CUSTOM_COMPLAINT_LENGTH);

				RemoveReportMovie();
			}
			else if (eNewLink == ReportLink_Confirmation)
			{
				// Store which callback to use
				m_iStoredCallbackIndex = m_iSelectedIndex;
				m_eReportType = m_MenuArray.m_ReportLists[m_iSelectedMenu].ReportOptions[m_iSelectedIndex].ReportType;

				m_HistoryStack.PushAndGrow(m_MenuArray.m_ReportLists[m_iSelectedMenu].ReportLinkId);
				UpdateConfirmationScreen();
			}
			else if (eNewLink == ReportLink_Other)
			{
				ShowKeyboard("RP_CUSTOM_TITLE");
			}
			else if (eNewLink == ReportLink_Content)
			{
				ShowKeyboard("RP_CUSTOM_TITLE");
				m_eReportType = ReportType_PlayerMade_Content;
			}
			else if (eNewLink == ReportLink_Successful)
			{
				if (m_eReportType == ReportType_OffensiveCrewName || m_eReportType == ReportType_OffensiveCrewMotto || m_eReportType == ReportType_OffensiveCrewStatus ||
					m_eReportType ==  ReportType_OffensiveCrewEmblem || m_eReportType == ReportType_Other)
				{
					if (m_NetStatus.GetStatus() == netStatus::NET_STATUS_NONE)
					{
						GenerateReport();
					}
				}
				else if ( m_eReportType == ReportType_PlayerMade_Title || m_eReportType == ReportType_PlayerMade_Description || m_eReportType == ReportType_PlayerMade_Content)
				{
					if (!m_QueryMissionsStatus.Pending() && (m_iSelectedMissionIndex >= 0) && (m_iSelectedMissionIndex < m_NumberOfMissionsInArray))
					{
						if (m_NetStatus.GetStatus() == netStatus::NET_STATUS_NONE)
						{
							reportDisplayf("CReportMenu::UpdateInput - player has pressed Accept so call GenerateMissionReport");
							CReportMenu::GenerateMissionReport();
						}
						else
						{
							reportDisplayf("CReportMenu::UpdateInput - player has pressed Accept but we can't call GenerateMissionReport because an earlier call to rlSocialClub::RegisterComplaint hasn't finished yet");
						}
					}
				}
				else
				{

					if (m_MenuArray.m_ReportLists[m_iSelectedMenu].ReportOptions[m_iSelectedIndex].ReportType == ReportType_Friendly || m_MenuArray.m_ReportLists[m_iSelectedMenu].ReportOptions[m_iSelectedIndex].ReportType == ReportType_Helpful )
					{
						m_iStoredCallbackIndex = m_iSelectedIndex;
					}

					if (m_eMenuType != eReportMenu_CommendMenu  && m_iStoredCallbackIndex < ReportType_Friendly)
					{
						if (!IsReportAbusive())
						{
							m_CallbackArray[m_iStoredCallbackIndex].Call();
						}
					}
					else
					{
						m_CallbackArray[m_iStoredCallbackIndex].Call();
					}

					UpdateThankYouScreen();
					m_eReportType = ReportType_None;
				}

				m_NameHashArray.Reset();
				m_CallbackArray.Reset();
				m_bAllowCancel = false;
			}
			else
			{
				if (eNewLink == ReportLink_Crew)
				{
					const rlClanDesc* pClanDesc = CLiveManager::GetNetworkClan().GetPrimaryClan(m_GamerHandle);
					eReportLink eLink = eNewLink;
					if (pClanDesc && !pClanDesc->IsValid())
					{
						eLink = ReportLink_PlayerMadeMissions;		//	Graeme - ask Geoff what this does
					}
				
					m_HistoryStack.PushAndGrow(m_MenuArray.m_ReportLists[m_iSelectedMenu].ReportLinkId);
					UpdateMenuOptions(eLink);

				}
				else
				{
					m_HistoryStack.PushAndGrow(m_MenuArray.m_ReportLists[m_iSelectedMenu].ReportLinkId);
					UpdateMenuOptions(eNewLink);
				}

			}
		}

		CPauseMenu::PlayInputSound(FRONTEND_INPUT_ACCEPT);

}

//****************************************************************************************
// UpdateInput - Controls input and navigation between pages.
//****************************************************************************************
bool CReportMenu::UpdateInput()
{
	if (!IsActive())
	{
		return false;
	}

	#if RSG_PC
	if (CControlMgr::GetVirtualKeyboardState() == ioVirtualKeyboard::kKBState_PENDING )
		return false;
	#endif

	
	if (CPauseMenu::CheckInput(FRONTEND_INPUT_ACCEPT, false, CHECK_INPUT_OVERRIDE_FLAG_STORAGE_DEVICE | CHECK_INPUT_OVERRIDE_FLAG_WARNING_MESSAGE))
	{

		OnAcceptInput();
		return true;
	}

	if (CPauseMenu::CheckInput(FRONTEND_INPUT_BACK, false, CHECK_INPUT_OVERRIDE_FLAG_STORAGE_DEVICE | CHECK_INPUT_OVERRIDE_FLAG_WARNING_MESSAGE))
	{
		if (m_bAllowCancel && (m_HistoryStack.Top() == ReportLink_None ))
		{
			RemoveReportMovie();
		}
		else
		{
			eReportLink eLink = m_HistoryStack.Top();
			m_eReportType = ReportType_None;
			memset(m_szCustomReportInfo,'0',sizeof(char)*MAX_CUSTOM_COMPLAINT_LENGTH);

			if(eLink != ReportLink_None)
			{
				m_HistoryStack.Pop();
				UpdateMenuOptions(eLink);
				CPauseMenu::PlayInputSound(FRONTEND_INPUT_BACK);
			}
		}
		
		return true;
	}

	if (CPauseMenu::CheckInput(FRONTEND_INPUT_UP, false, CHECK_INPUT_OVERRIDE_FLAG_STORAGE_DEVICE | CHECK_INPUT_OVERRIDE_FLAG_WARNING_MESSAGE))
	{

		if ( m_NameHashArray.GetCount() == 0 && m_iSelectedMenu != ReportLink_UGCReportRoot)
			return false;

		int newIndex = m_iSelectedIndex-1;

		if(m_Movie.BeginMethod("SET_SLOT_STATE"))
		{
			m_Movie.AddParam(m_iSelectedIndex);
			m_Movie.AddParam(0);
			m_Movie.EndMethod();
		}

		if(newIndex < 0)
		{
			if (m_iSelectedMenu == ReportLink_UGCReportRoot)
			{
				m_iSelectedIndex = m_NumberOfMissionsInArray - 1;
			}
			else
			{
				m_iSelectedIndex = m_CallbackArray.GetCount() - 1;
			}
		}
		else
		{
			m_iSelectedIndex = newIndex;
		}
		
		if(m_Movie.BeginMethod("SET_SLOT_STATE"))
		{
			m_Movie.AddParam(m_iSelectedIndex);
			m_Movie.AddParam((int)(BIT(0)));
			m_Movie.EndMethod();
		}

		CPauseMenu::PlayInputSound(FRONTEND_INPUT_UP);
	}

	if (CPauseMenu::CheckInput(FRONTEND_INPUT_DOWN, false, CHECK_INPUT_OVERRIDE_FLAG_STORAGE_DEVICE | CHECK_INPUT_OVERRIDE_FLAG_WARNING_MESSAGE))
	{

		if ( m_NameHashArray.GetCount() == 0 && m_iSelectedMenu != ReportLink_UGCReportRoot)
			return false;

		int newIndex = m_iSelectedIndex+1;

		if(m_Movie.BeginMethod("SET_SLOT_STATE"))
		{
			m_Movie.AddParam(m_iSelectedIndex);
			m_Movie.AddParam(0);
			m_Movie.EndMethod();
		}

		s32 numberOfMenuItems = 0;
		if (m_iSelectedMenu == ReportLink_UGCReportRoot)
		{
			numberOfMenuItems = m_NumberOfMissionsInArray;
		}
		else
		{
			numberOfMenuItems = m_CallbackArray.GetCount();
		}

		if(newIndex == numberOfMenuItems)
		{
			m_iSelectedIndex = 0;
		}
		else
		{
			m_iSelectedIndex = newIndex;
		}


		if(m_Movie.BeginMethod("SET_SLOT_STATE"))
		{
			m_Movie.AddParam(m_iSelectedIndex);
			m_Movie.AddParam((int)(BIT(0)));
			m_Movie.EndMethod();
		}

		CPauseMenu::PlayInputSound(FRONTEND_INPUT_DOWN);
	}

	return false;
}

void CReportMenu::ShowKeyboard(const char* szTitle)
{
	const int titleSize = 50;

	static char16 s_TitleForKeyboardUI[titleSize];
	static char16 s_InitialValueForKeyboard[MAX_CUSTOM_COMPLAINT_LENGTH];

	Utf8ToWide(s_TitleForKeyboardUI, TheText.Get(szTitle), titleSize);
	Utf8ToWide(s_InitialValueForKeyboard,"", MAX_CUSTOM_COMPLAINT_LENGTH);

	ioVirtualKeyboard::Params params;
	params.m_KeyboardType = ioVirtualKeyboard::kTextType_ALPHABET;
	params.m_MaxLength = MAX_CUSTOM_COMPLAINT_LENGTH;
	params.m_Title = s_TitleForKeyboardUI;
	params.m_InitialValue = s_InitialValueForKeyboard;
	CControlMgr::ShowVirtualKeyboard(params);

	m_bInSystemKeyboard = true;
}

void CReportMenu::PopulateMissionPage()
{	
	reportDisplayf("CReportMenu::PopulateMissionPage");

	// I bet this'll turn out to somehow be incredibly evil...
	LoadReportMovie();
	SetTitle(ATSTRINGHASH("CM_REPORT", 0xE8442257));

	if(m_Movie.BeginMethod("CLEAR_ALL_SLOTS"))
	{
		m_Movie.EndMethod();
	}

	if (m_NumberOfMissionsInArray == 0)
	{
		m_ButtonFlags = static_cast<eWarningButtonFlags>(FE_WARNING_BACK);
	}

	// Set the report subtext.
	if(m_Movie.BeginMethod("SET_SUBTITLE"))
	{
		if (m_NumberOfMissionsInArray == 0)
		{
			m_Movie.AddParamLocString(ATSTRINGHASH("RP_NO_MISSION", 0x64548BBF));
		}
		else
		{
			m_Movie.AddParamLocString(ATSTRINGHASH("RP_SEL_MISSION", 0xbed413e0));
		}
		m_Movie.EndMethod();
	}

	m_bAllowCancel = true;

	// Arguments 0-5
	int i;
	for(i = 0; (i < m_NumberOfMissionsInArray) && (i<MAX_WARNING_MISSIONS); ++i)
	{
		if(m_Movie.BeginMethod("SET_SLOT"))
		{
			m_Movie.AddParam(i);
			m_Movie.AddParam(i == 0? (int)(BIT(0)) : 0);
			m_Movie.AddParamString(m_ArrayOfMissions[i].m_ContentName);
			m_Movie.EndMethod();
		}
	}
	m_iSelectedIndex = 0;

	if(CPauseMenu::GetDisplayPed())
	{
		CPauseMenu::GetDisplayPed()->SetVisible(false);
	}

}
//****************************************************************************************
// UpdateConfirmationScreen - Controls input and navigation between pages.
//****************************************************************************************
void CReportMenu::UpdateThankYouScreen()
{
	reportDisplayf("CReportMenu::UpdateThankYouScreen");

	int iMenuScreensCount = m_MenuArray.m_ReportLists.GetCount();

	m_ButtonFlags = static_cast<eWarningButtonFlags>(FE_WARNING_OK);
	for (int i = 0; i < iMenuScreensCount; i++)
	{
		if (m_MenuArray.m_ReportLists[i].ReportLinkId == ReportLink_Successful)
		{
			m_iSelectedMenu = i;
			m_iSelectedIndex = 0;
		}
	} 

	if (m_eMenuType == eReportMenu_CommendMenu)
	{
		SetTitle(ATSTRINGHASH("CM_COMMENDED", 0xFFA09734));
	}
	else
	{
		SetTitle(ATSTRINGHASH("CM_REPORTED", 0x80483FC9));
	}

	if(m_Movie.BeginMethod("CLEAR_ALL_SLOTS"))
	{
		m_Movie.EndMethod();
	}

	// Set the report subtext.
	if(m_Movie.BeginMethod("SET_SUBTITLE"))
	{
		if (m_eMenuType == eReportMenu_CommendMenu)
			m_Movie.AddParamLocString(ATSTRINGHASH("RP_COM_THANKYOU", 0x9F2B20D2));
		else
			m_Movie.AddParamLocString(ATSTRINGHASH("RP_REP_THANKYOU", 0xE64CF8DC));
		m_Movie.EndMethod();
	}

	m_HistoryStack.Reset();
	m_HistoryStack.PushAndGrow(ReportLink_None);
}

//****************************************************************************************
// UpdateConfirmationScreen - Controls input and navigation between pages.
//****************************************************************************************
void CReportMenu::UpdateConfirmationScreen()
{
	reportDisplayf("CReportMenu::UpdateConfirmationScreen");

	bool bReportingAMission = false;
	switch (m_eReportType)
	{
		case ReportType_PlayerMade_Title :
		case ReportType_PlayerMade_Description :
		case ReportType_PlayerMade_Content :
		case ReportType_PlayerMade_Photo:
			bReportingAMission = true;
			break;

		default :
			break;
	}

	m_NameHashArray.Reset();

	int iMenuScreensCount = m_MenuArray.m_ReportLists.GetCount();

	m_ButtonFlags = static_cast<eWarningButtonFlags>(FE_WARNING_CONFIRM | FE_WARNING_BACK);

	for (int i = 0; i < iMenuScreensCount; i++)
	{
		if (m_MenuArray.m_ReportLists[i].ReportLinkId == ReportLink_Confirmation)
		{
			m_iSelectedMenu = i;
			m_iSelectedIndex = 0;
		}
	}
	if (m_eMenuType == eReportMenu_CommendMenu)
	{
		SetTitle(ATSTRINGHASH("CWS_COMMEND", 0x42546832));
	}
	else
	{
		SetTitle(ATSTRINGHASH("CM_REPORT", 0xE8442257));
	}

	if(m_Movie.BeginMethod("CLEAR_ALL_SLOTS"))
	{
		m_Movie.EndMethod();
	}

	const char* pGamerTag = m_Name;


	// Set the report subtext.
	if (m_Movie.BeginMethod("SET_SUBTITLE"))
	{
		char* pOriginalChar;
		if(m_eMenuType == eReportMenu_CommendMenu)
		{
			pOriginalChar = TheText.Get(ATSTRINGHASH("CM_WARN_HDR", 0x1FCC23E4),"CM_WARN_HDR");
		}
		else
		{
			if (bReportingAMission)	//	Maybe I could have checked if (m_eMenuType == eReportMenu_UGCMenu) here
			{
				pOriginalChar = TheText.Get(ATSTRINGHASH("RP_WARN_MISS_HDR", 0x3924c267),"RP_WARN_MISS_HDR");
			}
			else
			{
				pOriginalChar = TheText.Get(ATSTRINGHASH("RP_WARN_HDR", 0xD5A865A2),"RP_WARN_HDR");
			}
		}

		char newString[512];

		CSubStringWithinMessage substrings[2];

		if (bReportingAMission)	//	Maybe I could have checked if (m_eMenuType == eReportMenu_UGCMenu) here
		{
			substrings[0].SetLiteralString(m_ArrayOfMissions[m_iSelectedMissionIndex].m_ContentName, CSubStringWithinMessage::LITERAL_STRING_TYPE_FOR_IMMEDIATE_USE, false);
		}
		else
		{
			substrings[0].SetLiteralString(pGamerTag, CSubStringWithinMessage::LITERAL_STRING_TYPE_FOR_IMMEDIATE_USE, false);
		}
		substrings[1].SetTextLabel(FillReportInfo(m_eReportType));

		CMessages::InsertNumbersAndSubStringsIntoString( pOriginalChar, NULL,0,substrings,2,newString, 512);
		m_Movie.AddParamString(newString);
		m_Movie.EndMethod();
	}

	if(m_Movie.BeginMethod("SET_SLOT"))
	{
		m_Movie.AddParam(0);
		m_Movie.AddParam(0x000000f0);
		if (m_eMenuType == eReportMenu_CommendMenu)
			m_Movie.AddParamLocString(ATSTRINGHASH("RP_WARNING", 0x3B37753B));
		else
			m_Movie.AddParamLocString(ATSTRINGHASH("RP_WARNING", 0x3B37753B));

		m_Movie.EndMethod();
	}
}

//****************************************************************************************
// ShowReportScreen - Controls input and navigation between pages.
//****************************************************************************************
void CReportMenu::ShowReportScreen(u32 uSubtitleHash, bool bAllowCancel)
{
	const int MAX_WARNING_ITEMS = 10;
	uiAssertf(m_NameHashArray.GetCount() == m_CallbackArray.GetCount(), "Name (%d) and Callback (%d) arrays passed to CustomWarningScreen do not match sizes!", m_NameHashArray.GetCount(), m_CallbackArray.GetCount());
	uiAssertf(m_NameHashArray.GetCount() < MAX_WARNING_ITEMS, "Name/Callback array size (%d) is far larger than our allowance of (%d)", m_NameHashArray.GetCount(), MAX_WARNING_ITEMS);

	// I bet this'll turn out to somehow be incredibly evil...
	LoadReportMovie();

	if (m_eMenuType == eReportMenu_CommendMenu)
	{
		SetTitle(ATSTRINGHASH("CWS_COMMEND", 0x42546832));
	}
	else
	{
		SetTitle(ATSTRINGHASH("CM_REPORT", 0xE8442257));
	}

	if(m_Movie.BeginMethod("CLEAR_ALL_SLOTS"))
	{
		m_Movie.EndMethod();
	}

	// Set the report subtext.
	if(m_Movie.BeginMethod("SET_SUBTITLE"))
	{
		m_Movie.AddParamLocString(uSubtitleHash);
		m_Movie.EndMethod();
	}

	m_bAllowCancel = bAllowCancel;

	// Arguments 0-5
	int i;
	for(i = 0; i < m_NameHashArray.GetCount() && i<MAX_WARNING_ITEMS; ++i)
	{
		if(m_Movie.BeginMethod("SET_SLOT"))
		{
			m_Movie.AddParam(i);
			m_Movie.AddParam(i == 0? (int)(BIT(0)) : 0);
			m_Movie.AddParamLocString(m_NameHashArray[i]);
			m_Movie.EndMethod();
		}
	}

		m_iSelectedIndex = 0;

		if(CPauseMenu::GetDisplayPed())
		{
			CPauseMenu::GetDisplayPed()->SetVisible(false);
		}

}

void CReportMenu::GenerateReport()
{

	if (m_eReportType == ReportType_OffensiveCrewName || m_eReportType == ReportType_OffensiveCrewMotto ||
		m_eReportType == ReportType_OffensiveCrewStatus || m_eReportType == ReportType_OffensiveCrewEmblem )
	{
		GenerateCrewReport();
	}
	else if (m_eReportType == ReportType_Other)
	{
		GenerateCustomReport();
	}
}


void CReportMenu::GenerateMPPlayerReport(const netPlayer &fromPlayer, eReportType eReportType)
{

	char szUserId[64] = {0};
	char szReportedId[64] = {0};
	char szSessionId[64] = {0};
	char szOsvcName[8] = {0};
	char szPlatform[8] = {0};

	s64 sessionId = 0;
	const rlRosCredentials& cred = rlRos::GetCredentials(NetworkInterface::GetLocalGamerIndex());

	if (cred.IsValid())
	{
		sessionId = cred.GetSessionId();
		formatf(szSessionId,"%" I64FMT "u",sessionId);
	}

#if __XENON
	s64 sUserId = fromPlayer.GetGamerInfo().GetGamerHandle().GetXuid();
	s64 sReportedId = NetworkInterface::GetLocalGamerHandle().GetXuid();

	formatf(szUserId,64, "%" I64FMT "u",sUserId );
	formatf(szReportedId, 64,  "%" I64FMT "u",sReportedId );
	formatf(szOsvcName,8, "xbl");
	formatf(szPlatform, 8, "xbox");
#elif RSG_DURANGO
	s64 sUserId = fromPlayer.GetGamerInfo().GetGamerHandle().GetXuid();
	s64 sReportedId = NetworkInterface::GetLocalGamerHandle().GetXuid();

	formatf(szUserId,64, "%" I64FMT "u",sUserId );
	formatf(szReportedId, 64,  "%" I64FMT "u", sReportedId );
	formatf(szOsvcName,8, "xbl");
	formatf(szPlatform, 8, rlRosTitleId::GetPlatformName());
#elif RSG_NP

	formatf(szUserId,64, "%s",fromPlayer.GetGamerInfo().GetGamerHandle().GetNpOnlineId().data );
	formatf(szReportedId,64, "%s",NetworkInterface::GetLocalGamerHandle().GetNpOnlineId().data );

	safecpy(szOsvcName, "np");
	safecpy(szPlatform, rlRosTitleId::GetPlatformName());
#elif RSG_PC
	s64 sUserId = fromPlayer.GetGamerInfo().GetGamerHandle().GetRockstarId();
	s64 sReportedId = NetworkInterface::GetLocalGamerHandle().GetRockstarId();

	formatf(szUserId,64, "%" I64FMT "u",sUserId );
	formatf(szReportedId,64, "%" I64FMT "u",sReportedId );

	sprintf(szOsvcName, "sc");
	sprintf(szPlatform, "pc");
#endif


	u64 uPosixTime = rlGetPosixTime();

	const CNetGamePlayer& fromNetGamePlayer = static_cast<const CNetGamePlayer&>(fromPlayer);
	const PlayerAccountId reporterPlayerAccountId = fromNetGamePlayer.GetPlayerAccountId();
	const PlayerAccountId playerAccountId = cred.GetPlayerAccountId();

	RsonWriter rw;
	char buff[2096] = {0};
	rw.Init(buff, sizeof(buff),RSON_FORMAT_JSON);
	rw.Begin(NULL, NULL);
	rw.WriteString("titleName","gta5");
	rw.WriteString("osvcname",szOsvcName);
	rw.WriteString("reporterId",szUserId);
	rw.WriteInt("reporterPlayerAccountId", reporterPlayerAccountId);
	rw.WriteString("reporterNickname", fromPlayer.GetGamerInfo().GetName());
	rw.WriteString("reportType","GTAV_MP_PLAYER");

	if (eReportType == ReportType_VoiceChat_Hate || eReportType == ReportType_TextChat_Hate)
		rw.WriteString("reportReason", "hate");
	else if (eReportType == ReportType_TextChat_Annoying)
		rw.WriteString("reportReason", "harass");
	else if (eReportType == ReportType_Exploit)
		rw.WriteString("reportReason", "exploit");
	else
		rw.WriteString("reportReason", "gameexploit");

	rw.WriteString("reportReasonDesc", "Custom player report.");
	rw.WriteString("reportMoreInfo", "");
	rw.WriteString("status", "new");
	rw.WriteInt64("created", uPosixTime);
	rw.WriteInt64("updated", uPosixTime);
	rw.Begin("data", NULL);
		rw.WriteString("id",szReportedId);
		rw.WriteInt("playerAccountId", playerAccountId);
		rw.WriteString("gtag", NetworkInterface::GetLocalGamerName());
		rw.WriteString("platform", szPlatform);
		rw.WriteString("sessionId",szSessionId);
		if (eReportType == ReportType_TextChat_Annoying || eReportType == ReportType_TextChat_Hate)
		{
	#if RSG_PC
			NetworkInterface::GetTextChat().GetChatHistoryWithPlayer(fromPlayer.GetGamerInfo().GetGamerHandle().GetRockstarId(), &rw);
	#else
			Assertf(0, "TextChat report not supported on this platform");
	#endif
		}
	rw.End();
	rw.End();

	netStatus* NetStatus = GetNetStatus();
    if(NetStatus)
    {
        rlSocialClub::RegisterComplaint(NetworkInterface::GetLocalGamerIndex(), rw.ToString(), NetStatus);
    }
}

void CReportMenu::GenerateCrewReport()
{
	char szOsvcName[8] = {0};
	char szUserId[64] = {0};
	char szReportType[64] = {0};
	char szReportReason[64] = {0};
	char szReportMoreInfo[64] = {0};
	char szReportReasonDesc[64] = {0};
	char szReportedId[64] = {0};
	char szCrewId[32] = {0};
	char szCrewName[32] = {0};
	char szContentId[64] = {0};
	char szCrewMotto[RL_CLAN_MOTTO_MAX_CHARS] = {0};

		
#if __XENON || RSG_DURANGO
	s64 sUserId = NetworkInterface::GetLocalGamerHandle().GetXuid();
	s64 sReportedId = m_GamerHandle.GetXuid();
	
	formatf(szOsvcName,8, "xbl");
	formatf(szUserId,64, "%" I64FMT "u",sUserId );
	formatf(szReportedId, 64,  "%" I64FMT "u",sReportedId );
#elif RSG_NP
	formatf(szOsvcName,8, "np");
	formatf(szUserId,64, "%s",NetworkInterface::GetLocalGamerHandle().GetNpOnlineId().data );
	formatf(szReportedId,64, "%s",m_GamerHandle.GetNpOnlineId().data );
#else
	s64 sUserId = NetworkInterface::GetLocalGamerHandle().GetRockstarId();
	s64 sReportedId = m_GamerHandle.GetRockstarId();

	formatf(szOsvcName,8, "sc");
	formatf(szUserId,64, "%" I64FMT "u",sUserId );
	formatf(szReportedId,64, "%" I64FMT "u",sReportedId );
#endif


	const rlClanDesc* pClanDesc = CLiveManager::GetNetworkClan().GetPrimaryClan(m_GamerHandle);
	if (pClanDesc)
	{
		formatf(szCrewId,32, "%d", pClanDesc->m_Id);
		formatf(szCrewName, 32, "%s", pClanDesc->m_ClanName);
		formatf(szCrewMotto, RL_CLAN_MOTTO_MAX_CHARS, "%s", pClanDesc->m_ClanMotto);
	}
	else
	{
#if __ASSERT
		char ghBuf[RL_MAX_GAMER_HANDLE_CHARS];
		uiAssertf(0, "CReportMenu::GenerateCrewReport -- Unable to find rlClanDesc for gamer handle: %s", m_GamerHandle.ToString(ghBuf));
#endif // __ASSERT
		formatf(szCrewId,32, "%d", 0);
		formatf(szCrewName, 32, "%s", "--NONE--");
		formatf(szCrewMotto, RL_CLAN_MOTTO_MAX_CHARS, "%s", "--NONE--");
	}


	if (m_eReportType == ReportType_OffensiveCrewEmblem)
	{
		formatf(szReportType,64,"CREW_ACTIVE_EMBLEM");
		formatf(szReportReason,64,"Offensive Emblem");
		formatf(szReportMoreInfo,64,"Crew emblem is offensive.");
		formatf(szReportReasonDesc,64,"Crew emblem is offensive.");
		formatf(szContentId,64,"CREW.%s.ACTIVE_EMBLEM",szCrewId);

	}
	else if (m_eReportType == ReportType_OffensiveCrewMotto)
	{
		formatf(szReportType,64,"CREW_MOTTO");
		formatf(szReportReason,64,"Offensive Motto");
		formatf(szReportMoreInfo,64,"Crew motto is offensive.");
		formatf(szReportReasonDesc,64,"Crew motto is offensive.");
		formatf(szContentId,64,"CREW.%s.MOTTO",szCrewId);
	}
	else if (m_eReportType == ReportType_OffensiveCrewName)
	{
		formatf(szReportType,64,"CREW_NAME");
		formatf(szReportReason,64,"Offensive Crew Name");
		formatf(szReportMoreInfo,64,"Crew name is offensive.");
		formatf(szReportReasonDesc,64,"Crew name is offensive.");
		formatf(szContentId,64,"CREW.%s.NAME",szCrewId);
	}
	else
	{
		formatf(szReportType,64,"CREW_STATUS");
		formatf(szReportReason,64,"Offensive Crew Status");
		formatf(szReportMoreInfo,64,"Crew status is offensive.");
		formatf(szReportReasonDesc,64,"Crew status is offensive.");
		formatf(szContentId,64,"CREW.%s.STATUS",szCrewId);

	}
	u64 uPosixTime = rlGetPosixTime();

	const rlRosCredentials& cred = rlRos::GetCredentials(NetworkInterface::GetLocalGamerIndex());
	const PlayerAccountId playerAccountId = cred.GetPlayerAccountId();

	RsonWriter rw;
	char buff[2096] = {0};
	rw.Init(buff, sizeof(buff), RSON_FORMAT_JSON);
	rw.Begin(NULL, NULL);
	rw.WriteString("titleName","gta5");
	rw.WriteString("osvcname",szOsvcName);
	rw.WriteString("reporterId",szUserId);
	rw.WriteInt("reporterPlayerAccountId", playerAccountId);
	rw.WriteString("reporterNickname", NetworkInterface::GetLocalGamerName());
	rw.WriteString("reportType",szReportType);
	rw.WriteString("reportReason", szReportReason);
	rw.WriteString("reportReasonDesc", szReportReasonDesc);
	rw.WriteString("reportMoreInfo", szReportMoreInfo);
	rw.WriteString("status", "new");
	rw.WriteInt64("created", uPosixTime);
	rw.WriteInt64("updated", uPosixTime);
	rw.Begin("data", NULL);
		rw.WriteString("ContentId", szContentId );
		rw.WriteString("gtag", m_Name);
		rw.WriteString("crewid", szCrewId);
		rw.WriteString("crewname", szCrewName);
		rw.WriteString("motto", szCrewMotto);
		rw.WriteString("ipForm","");
		rw.End();
	rw.End();
	
	rlSocialClub::RegisterComplaint(NetworkInterface::GetLocalGamerIndex(), rw.ToString(), &m_NetStatus);

	if (!m_NetStatusCallback)
	{
		m_NetStatusCallback = rage_new datCallback(MFA(CReportMenu::UpdateThankYouScreen),(datBase*)this);
	}

	CContextMenuHelper::SetLastReportTimer(sysTimer::GetSystemMsTime());

}

void CReportMenu::GenerateMissionReport()
{
	reportDisplayf("CReportMenu::GenerateMissionReport for %s %s", m_ArrayOfMissions[m_iSelectedMissionIndex].m_ContentId.c_str(), m_ArrayOfMissions[m_iSelectedMissionIndex].m_ContentName.c_str());

	char szUserId[64] = {0};
	char szReportedId[64] = {0};
	char szSessionId[64] = {0};
	char szOsvcName[8] = {0};
	char szPlatform[8] = {0};
	//	char szAuthorId[64] = {0};
	char szReportType[64] = {0};

	if (m_eReportType == ReportType_PlayerMade_Title)
	{
		formatf(szReportType,64,"JOB_TITLE");
	}
	else if (m_eReportType == ReportType_PlayerMade_Description)
	{
		formatf(szReportType,64,"JOB_DESCRIPTION");
	}
	else if (m_eReportType == ReportType_PlayerMade_Photo)
	{
		formatf(szReportType,64,"JOB_PHOTO");
	}
	else
	{
		formatf(szReportType,64,"GTAV_UGC_CONTENT");
	}

	s64 sessionId = 0;
	const rlRosCredentials& cred = rlRos::GetCredentials(NetworkInterface::GetLocalGamerIndex());

	if (cred.IsValid())
	{
		sessionId = cred.GetSessionId();
		formatf(szSessionId,"%" I64FMT "u",sessionId);
	}

	#if __XENON
	s64 sUserId = NetworkInterface::GetLocalGamerHandle().GetXuid();
	s64 sReportedId = m_GamerHandle.GetXuid();

	formatf(szUserId,64, "%" I64FMT "u",sUserId );
	formatf(szReportedId, 64,  "%" I64FMT "u",sReportedId );
	formatf(szOsvcName,8, "xbl");
	formatf(szPlatform, 8, "xbox");

	#elif RSG_NP

	formatf(szUserId,64, "%s",NetworkInterface::GetLocalGamerHandle().GetNpOnlineId().data );
	formatf(szReportedId,64, "%s",m_GamerHandle.GetNpOnlineId().data );

	safecpy(szOsvcName, "np");
	safecpy(szPlatform, rlRosTitleId::GetPlatformName());

	#elif RSG_DURANGO

	s64 sUserId = NetworkInterface::GetLocalGamerHandle().GetXuid();
	s64 sReportedId = m_GamerHandle.GetXuid();

	formatf(szUserId,64, "%" I64FMT "u",sUserId );
	formatf(szReportedId, 64,  "%" I64FMT "u", sReportedId );
	formatf(szOsvcName,8, "xbl");
	formatf(szPlatform, 8, rlRosTitleId::GetPlatformName());


	#elif RSG_PC
	s64 sUserId = NetworkInterface::GetLocalGamerHandle().GetRockstarId();
	s64 sReportedId = m_GamerHandle.GetRockstarId();

	formatf(szUserId,64, "%" I64FMT "u",sUserId );
	formatf(szReportedId,64, "%" I64FMT "u",sReportedId );

	sprintf(szOsvcName, "sc");
	sprintf(szPlatform, "pc");
	#endif

	u64 uPosixTime = rlGetPosixTime();

	RsonWriter rw;
	char buff[2096] = {0};
	rw.Init(buff, sizeof(buff),RSON_FORMAT_JSON);
	rw.Begin(NULL, NULL);
	rw.WriteString("titleName","gta5");
	rw.WriteString("osvcname",szOsvcName);
	rw.WriteString("reporterId",szUserId);
	rw.WriteInt("reporterPlayerAccountId", cred.GetPlayerAccountId());
	rw.WriteString("reporterNickname", NetworkInterface::GetLocalGamerName());
	rw.WriteString("reportType",szReportType);
	rw.WriteString("reportReason", "harass");
	rw.WriteString("reportReasonDesc", "Custom player report.");
	if (m_eReportType == ReportType_PlayerMade_Content )
		rw.WriteString("reportMoreInfo", m_szCustomReportInfo);
	else
		rw.WriteString("reportMoreInfo", "");
	rw.WriteString("status", "new");
	rw.WriteInt64("created",uPosixTime);
	rw.WriteInt64("updated",uPosixTime);
	rw.Begin("data", NULL);

	reportAssertf(m_ArrayOfMissions[m_iSelectedMissionIndex].m_ContentId.length() > 0, "CReportMenu::GenerateMissionReport - Empty m_ContentId");
	reportAssertf(m_ArrayOfMissions[m_iSelectedMissionIndex].m_ContentName.length() > 0, "CReportMenu::GenerateMissionReport - Empty m_ContentName");
	reportAssertf(m_ArrayOfMissions[m_iSelectedMissionIndex].m_ContentUserName.length() > 0, "CReportMenu::GenerateMissionReport - Empty m_ContentUserName");
	reportAssertf(m_ArrayOfMissions[m_iSelectedMissionIndex].m_ContentUserId.length() > 0, "CReportMenu::GenerateMissionReport - Empty m_ContentUserId");

	rw.WriteString("ContentId", m_ArrayOfMissions[m_iSelectedMissionIndex].m_ContentId);
	rw.WriteString("ContentType","gta5mission");
	rw.WriteString("ContentName", m_ArrayOfMissions[m_iSelectedMissionIndex].m_ContentName);
	rw.WriteString("AuthorNickname", m_ArrayOfMissions[m_iSelectedMissionIndex].m_ContentUserName);
//	rw.WriteString("AuthorId", szAuthorId);
	rw.WriteString("AuthorId", m_ArrayOfMissions[m_iSelectedMissionIndex].m_ContentRockstarId);
	rw.WriteString("AuthorGamerTag", m_ArrayOfMissions[m_iSelectedMissionIndex].m_ContentUserName);
	rw.WriteString("AuthorUserId", m_ArrayOfMissions[m_iSelectedMissionIndex].m_ContentUserId);
	rw.WriteString("sessionId",szSessionId);
	rw.WriteString("platform",szPlatform);
	rw.End();
	rw.End();

	rlSocialClub::RegisterComplaint(NetworkInterface::GetLocalGamerIndex(), rw.ToString(), &m_NetStatus);

	if (!m_NetStatusCallback)
	{
		m_NetStatusCallback = rage_new datCallback(MFA(CReportMenu::UpdateThankYouScreen),(datBase*)this);
	}

}

void CReportMenu::GenerateCustomReport()
{
	char szUserId[64] = {0};
	char szReportedId[64] = {0};
	char szSessionId[64] = {0};
	char szOsvcName[8] = {0};
	char szPlatform[8] = {0};

	s64 sessionId = 0;
	const rlRosCredentials& cred = rlRos::GetCredentials(NetworkInterface::GetLocalGamerIndex());

	if (cred.IsValid())
	{
		sessionId = cred.GetSessionId();
		formatf(szSessionId,"%" I64FMT "u",sessionId);
	}

#if __XENON
	s64 sUserId = NetworkInterface::GetLocalGamerHandle().GetXuid();
	s64 sReportedId = m_GamerHandle.GetXuid();

	formatf(szUserId,64, "%" I64FMT "u",sUserId );
	formatf(szReportedId, 64,  "%" I64FMT "u",sReportedId );
	formatf(szOsvcName,8, "xbl");
	formatf(szPlatform, 8, "xbox");
#elif RSG_DURANGO
	s64 sUserId = NetworkInterface::GetLocalGamerHandle().GetXuid();
	s64 sReportedId = m_GamerHandle.GetXuid();

	formatf(szUserId,64, "%" I64FMT "u",sUserId );
	formatf(szReportedId, 64,  "%" I64FMT "u", sReportedId );
	formatf(szOsvcName,8, "xbl");
	formatf(szPlatform, 8, rlRosTitleId::GetPlatformName());
#elif RSG_NP
	//s64 sUserId = NetworkInterface::GetLocalGamerHandle().GetXuid();
	//s64 sReportedId = m_GamerHandle.GetXuid();

	formatf(szUserId,64, "%" I64FMT "u",NetworkInterface::GetLocalGamerHandle().GetNpOnlineId().data );
	formatf(szReportedId,64, "%" I64FMT "u",m_GamerHandle.GetNpOnlineId().data );

	formatf(szOsvcName,8, "np");
	formatf(szPlatform,8, rlRosTitleId::GetPlatformName());
#elif RSG_PC
	s64 sUserId = NetworkInterface::GetLocalGamerHandle().GetRockstarId();
	s64 sReportedId = m_GamerHandle.GetRockstarId();

	formatf(szUserId,64, "%" I64FMT "u",sUserId );
	formatf(szReportedId,64, "%" I64FMT "u",sReportedId );

	formatf(szOsvcName, 8, "sc");
	formatf(szPlatform, 8, "pc");
#endif

	u64 uPosixTime = rlGetPosixTime();

	RsonWriter rw;
	char buff[2096] = {0};
	rw.Init(buff, sizeof(buff),RSON_FORMAT_JSON);
	rw.Begin(NULL, NULL);
	rw.WriteString("titleName","gta5");
	rw.WriteString("osvcname",szOsvcName);
	rw.WriteString("reporterId",szUserId);
	rw.WriteInt("reporterPlayerAccountId", cred.GetPlayerAccountId());
	rw.WriteString("reporterNickname", NetworkInterface::GetLocalGamerName());
	rw.WriteString("reportType","GTAV_MP_PLAYER");
	rw.WriteString("reportReason", "harass");
	rw.WriteString("reportReasonDesc", "Custom player report.");
	rw.WriteString("reportMoreInfo", m_szCustomReportInfo);
	rw.WriteString("status", "new");
	rw.WriteInt64("created",uPosixTime);
	rw.WriteInt64("updated",uPosixTime);
	rw.Begin("data", NULL);
	rw.WriteString("id",szReportedId);
	rw.WriteString("gtag", m_Name);
	rw.WriteString("platform",szPlatform);
	rw.WriteString("sessionId",szSessionId);
	rw.End();
	rw.End();


	rlSocialClub::RegisterComplaint(NetworkInterface::GetLocalGamerIndex(), rw.ToString(), &m_NetStatus);

	if (!m_NetStatusCallback)
	{
		m_NetStatusCallback = rage_new datCallback(MFA(CReportMenu::UpdateThankYouScreen),(datBase*)this);
	}

	CContextMenuHelper::SetLastReportTimer(sysTimer::GetSystemMsTime());
}

const char* CReportMenu::FillReportInfo(eReportType eReportType)
{
	
	switch(eReportType)
	{
		case ReportType_None:
			return "SM_LCNONE";
		case ReportType_Griefing:
			return "CWS_GRIEFING";
		case ReportType_OffensiveLanguage:
			return "RP_OFFENSIVE_LANGUAGE";
		case ReportType_OffensiveLicensePlate:
			return "RP_OFFENSIVE_PLATE";
		case ReportType_Exploit:
			return "RP_EXPLOITS";
		case ReportType_GameExploit:
			return "RP_GAME_EXPLOIT";
		case ReportType_OffensiveUGC:
			return "RP_OFFENSIVE_UGC";
		case ReportType_Other:
			return "RP_NET_OTHER";
		case ReportType_OffensiveCrewName:
			return "RP_CREW_NAME";
		case ReportType_OffensiveCrewMotto:
			return "RP_MOTTO";
		case ReportType_OffensiveCrewStatus:
			return "RP_STATUS";
		case ReportType_OffensiveCrewEmblem:
			return "RP_EMBLEM";
		case ReportType_PlayerMade_Title:
			return "RP_PLAYER_MADE_TITLE";
		case ReportType_PlayerMade_Description:
			return "RP_PLAYER_MADE_DESC";
		case ReportType_PlayerMade_Photo:
			return "RP_PLAYER_MADE_PHOTO";
		case ReportType_PlayerMade_Content:
			return "RP_PLAYER_MADE_CONTENT";
		case ReportType_VoiceChat_Annoying:
			return "RP_VC_ANNOY";
		case ReportType_VoiceChat_Hate:
			return "RP_VC_HATE";
		case ReportType_TextChat_Annoying:
			return "RP_TC_ANNOY";
		case ReportType_TextChat_Hate:
			return "RP_TC_HATE";
		case ReportType_Friendly:
			return "CWS_FRIENDLY";
		case ReportType_Helpful:
			return "CWS_HELPFUL";
		default:
			return "Undefined";
	}
}

void CReportMenu::SetTitle(u32 uTitleHash)
{

	// Set the report title.
	if(m_Movie.BeginMethod("SET_TITLE"))
	{
		m_Movie.AddParamLocString(uTitleHash);
		m_Movie.EndMethod();
	}

}


void CReportMenu::OnRecentMissionsContentResult(const int UNUSED_PARAM(nIndex),
											  const char* szContentID,
											  const rlUgcMetadata* pMetadata,
											  const rlUgcRatings* UNUSED_PARAM(pRatings),
											  const char* UNUSED_PARAM(szStatsJSON),
											  const unsigned UNUSED_PARAM(nPlayers),
											  const unsigned UNUSED_PARAM(nPlayerIndex),
											  const rlUgcPlayer* UNUSED_PARAM(pPlayer))
{
	if (m_NumberOfMissionsInArray < MAX_WARNING_MISSIONS)
	{
		if (reportVerifyf(pMetadata, "CReportMenu::OnRecentMissionsContentResult - pMetadata is NULL"))
		{
			switch (pMetadata->GetCategory())
			{
				case RLUGC_CATEGORY_ROCKSTAR_CREATED :
					reportDisplayf("CReportMenu::OnRecentMissionsContentResult - I've not added %s %s to the array of recent missions because it is ROCKSTAR_CREATED", szContentID, pMetadata->GetContentName());
					break;

				case RLUGC_CATEGORY_ROCKSTAR_CREATED_CANDIDATE : 
					reportDisplayf("CReportMenu::OnRecentMissionsContentResult - I've not added %s %s to the array of recent missions because it is ROCKSTAR_CREATED_CANDIDATE", szContentID, pMetadata->GetContentName());
					break;

//	Bug 1718433 requested that Rockstar Verified jobs also appear in the Report Job list
// 				case RLUGC_CATEGORY_ROCKSTAR_VERIFIED :
// 					reportDisplayf("CReportMenu::OnRecentMissionsContentResult - I've not added %s %s to the array of recent missions because it is ROCKSTAR_VERIFIED", szContentID, pMetadata->GetContentName());
// 					break;

				default :
					{
						
						if (strcmp(NetworkInterface::GetActiveGamerInfo()->GetName(),pMetadata->GetUsername()) != 0)
						{
							formatf(m_ArrayOfMissions[m_NumberOfMissionsInArray].m_ContentRockstarId, 64, "%" I64FMT "u", pMetadata->GetRockstarId());
							m_ArrayOfMissions[m_NumberOfMissionsInArray].m_ContentId = szContentID;
							m_ArrayOfMissions[m_NumberOfMissionsInArray].m_ContentName = pMetadata->GetContentName();
							m_ArrayOfMissions[m_NumberOfMissionsInArray].m_ContentUserName = pMetadata->GetUsername();
							m_ArrayOfMissions[m_NumberOfMissionsInArray].m_ContentUserId = pMetadata->GetUserId();

							reportAssertf(m_ArrayOfMissions[m_NumberOfMissionsInArray].m_ContentId.length() > 0, "CReportMenu::OnRecentMissionsContentResult - Empty m_ContentId");
							reportAssertf(m_ArrayOfMissions[m_NumberOfMissionsInArray].m_ContentName.length() > 0, "CReportMenu::OnRecentMissionsContentResult - Empty m_ContentName");
							reportAssertf(m_ArrayOfMissions[m_NumberOfMissionsInArray].m_ContentUserName.length() > 0, "CReportMenu::OnRecentMissionsContentResult - Empty m_ContentUserName");
							reportAssertf(m_ArrayOfMissions[m_NumberOfMissionsInArray].m_ContentUserId.length() > 0, "CReportMenu::OnRecentMissionsContentResult - Empty m_ContentUserId");

							reportDisplayf("CReportMenu::OnRecentMissionsContentResult - Added content. %u Content Id: %s, Content Name: %s, User Id: %s, User Name: %s, Total: %u", m_NumberOfMissionsInArray, 
								szContentID, pMetadata->GetContentName(), pMetadata->GetUserId(), pMetadata->GetUsername(), m_nContentTotal);
							m_NumberOfMissionsInArray++;
						}
					}
					break;
			}
		}
	}

}

netStatus* CReportMenu::GetNetStatus()
{
    for(int i=0; i<MAX_SIMULTANEOUS_COMPLAINT; i++)
    {
        if(sm_NetStatusPool[i].GetStatus() == netStatus::NET_STATUS_NONE)
        {
            return &sm_NetStatusPool[i];
        }
    }
    reportAssertf(0, "Max simultaneous Complaint requests reached (%d)", MAX_SIMULTANEOUS_COMPLAINT);
    return nullptr;
}
