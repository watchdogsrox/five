#include "Frontend/Store/PauseStoreMenu.h"
#include "Frontend/Store/StoreUIChannel.h"


#include "Frontend/ButtonEnum.h"
#include "Frontend/PauseMenu.h"
#include "frontend/Store/StoreScreenMgr.h"
#include "network/Live/livemanager.h"
#include "network/NetworkInterface.h"
#include "Network/Sessions/NetworkSession.h"
#include "rline/rlgamerinfo.h"
#include "Stats/MoneyInterface.h"
#include "net/nethardware.h"


FRONTEND_STORE_OPTIMISATIONS()

XPARAM(leavempforstore);

const unsigned int STORE_UNAVAILABLE_DISPLAY_FRAME_THRESHOLD = 90;

CPauseStoreMenu::CPauseStoreMenu( CMenuScreen& owner ): CMenuBase(owner),
	m_storeState(STATE_NONE),
	m_StoreSigninFrameCount(0),
    m_WaitingForLoginScreenReturn(false),
    m_WaitingForLoginScreenEntrance(false),
	m_bIsDisplayingNews(false),
	m_bStoryTextPosted(false)
{
    
}

CPauseStoreMenu::~CPauseStoreMenu()
{

}

void CPauseStoreMenu::Init()
{
    m_WaitingForLoginScreenReturn = false;    
    m_WaitingForLoginScreenEntrance = false;
	m_StoreSigninFrameCount = 0;
}
 
void CPauseStoreMenu::Update()
{
    CMenuBase::Update();


    if ( m_WaitingForLoginScreenEntrance  && CLiveManager::IsSystemUiShowing() )
    {
        m_WaitingForLoginScreenEntrance = false;
        m_WaitingForLoginScreenReturn = true;

    }
    else if ( m_WaitingForLoginScreenReturn && !CLiveManager::IsSystemUiShowing())
    {
        m_WaitingForLoginScreenReturn = false;

        if( CPauseMenu::IsStoreAvailable() && !CLiveManager::GetCommerceMgr().IsThinDataPopulationInProgress() )
        {
			//We close the pause menu immediately unless we are in an MP game AND the leave mp for store param is set
			bool closePauseMenuImmediately = !(NetworkInterface::IsGameInProgress() && PARAM_leavempforstore.Get());
			if (closePauseMenuImmediately)
			{
				CPauseMenu::TriggerStore();
			}
			else
			{
				cStoreScreenMgr::RequestMPStore();

				//Lock the store screen early to avoid glitches when leaving the session.
				CPauseMenu::TogglePauseRenderPhases(false, OWNER_STORE, __FUNCTION__ );
			}
        }
    }

	//We use this to stop flickers of unavailability, and a visible progression of states on sign in.
	if ( CPauseMenu::IsStoreAvailable() )
	{
		m_StoreSigninFrameCount = 0;

		// If we are displaying news, let's check for any updates
		if(	m_bIsDisplayingNews)
		{
			CNetworkPauseMenuNewsController& newsController = CNetworkPauseMenuNewsController::Get();
			if(newsController.IsPendingStoryReady())
			{
				PostStoreStory(newsController.GetPendingStory(), true);
				UpdateStoreColumnScroll(newsController.GetNumStories(), newsController.GetPendingStoryIndex());
				CNetworkPauseMenuNewsController::Get().MarkPendingStoryAsShown();
				m_bStoryTextPosted = false;
			}
			else if(!m_bStoryTextPosted && newsController.IsPendingStoryDataRcvd())
			{
				PostStoreStory(newsController.GetPendingStory(), false);
				UpdateStoreColumnScroll(newsController.GetNumStories(), newsController.GetPendingStoryIndex());
				m_bStoryTextPosted = true;
			}
		}
	}
	
	//We use this point to delay the DC message after sign in. Hopefully I can get a more reliable method to check if sign in is in progress.
	if (m_StoreSigninFrameCount > 0)
	{
		m_StoreSigninFrameCount--;
	}

	if (IsNotConnected())
	{
		m_StoreSigninFrameCount = STORE_UNAVAILABLE_DISPLAY_FRAME_THRESHOLD;
	}
	 

	if(m_storeState != GetStoreState())
	{
		Populate(MENU_UNIQUE_ID_STORE); // We don't want to do this every frame, only when the state has changed
		CPauseMenu::RedrawInstructionalButtons();
	}

	if(CPauseMenu::DynamicMenuExists() && 
		CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage().IsActive() &&
		CLiveManager::GetInviteMgr().HasPendingAcceptedInvite())
	{
		// If we've started an invite let's kill the warning screen
		CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage().Clear();
	}
}

// void CPauseStoreMenu::Render(const PauseMenuRenderDataExtra* renderData)
// {
//     CMenuBase::Render(renderData);
// }

bool CPauseStoreMenu::UpdateInput( s32 sInput )
{
	if ( cStoreScreenMgr::IsPendingNetworkShutdownToOpen() )
	{
		//Dont allow the screen to exit if we are trying to leave a network session and open the store.
		return true;
	}


    //BIG HONKING NOTE OF FUTURE PROTECTION-NESS! WHEN THIS MENU PANEL IS PRETTIED UP THE FUNCTIONALITY 
    //HERE NEEDS TO BE PRESERVED LEST WE ANGER THE QA ELDER ONES WITH TRC BREACHES.
    //(That's right. Lovecraftian warnings.)
    if( sInput == PAD_CROSS  )
    {

#if !RSG_ORBIS // B*1817634 - Cannot show Sign-in UI on PS4
		if ( !CLiveManager::IsOnline() )
		{
			CLiveManager::ShowSigninUi();
			m_WaitingForLoginScreenEntrance = true;
		}
		else
#endif
		if ( 
#if !RSG_PC
			(NetworkInterface::IsGameInProgress() && CNetwork::GetNetworkSession().IsActivitySession()) || 
#endif			
			!CPauseMenu::IsStoreAvailable())
		{
			//We do not open the store in this case
		}
		else if (CPauseMenu::IsStoreAvailable() && !CLiveManager::GetCommerceMgr().IsThinDataPopulationInProgress())
        {
			const rlGamerInfo* pGamerInfo = NetworkInterface::GetActiveGamerInfo();
			if (!rlGamerInfo::HasStorePrivileges(pGamerInfo->GetLocalIndex()))
			{
				//Display the warning screen
				DisplayUnderageWarning();
				return true;
			}

			

            atString baseCat("TOP_DEFAULT");
            cStoreScreenMgr::SetBaseCategory(baseCat);

			//We no longer leave the game to access the store in NG.
			//We close the pause menu immediately unless we are in an MP game AND the leave mp for store param is set
            bool closePauseMenuImmediately = !(NetworkInterface::IsGameInProgress() && PARAM_leavempforstore.Get());
			if (closePauseMenuImmediately)
			{
				CPauseMenu::TriggerStore();
			}
			else
			{
				//We populate here for MP since a lot of data is lost during the session exit.
				cStoreScreenMgr::PopulateDisplayValues();
				DisplayLeaveSessionWarning();
			}
			
			CPauseMenu::PlaySound("SELECT");
         
            return true;
        }
		else if(!CPauseMenu::IsStoreAvailable())
		{
			// Display Warning
			DisplayStoreUnavailableWarning();
		}
    }

	if ( sInput == PAD_CIRCLE && cStoreScreenMgr::IsPendingNetworkShutdownToOpen() )
	{
		//Dont allow the screen to exit if we are trying to leave a network session and open the store.
		return true;
	}

	if (SUIContexts::IsActive("STORE_CanScroll") && m_bIsDisplayingNews)
	{
		if( CPauseMenu::CheckInput(FRONTEND_INPUT_RUP, true))
		{
			CNetworkPauseMenuNewsController::Get().RequestStory(-1);
			ShowColumnWarning(PM_COLUMN_LEFT, 3, "");
			CScaleformMenuHelper::SET_BUSY_SPINNER(PM_COLUMN_MIDDLE, true);
		}
		else if( CPauseMenu::CheckInput(FRONTEND_INPUT_RDOWN, true))
		{
			CNetworkPauseMenuNewsController::Get().RequestStory(1);
			ShowColumnWarning(PM_COLUMN_LEFT, 3, "");
			CScaleformMenuHelper::SET_BUSY_SPINNER(PM_COLUMN_MIDDLE, true);
		}
	}

    return CMenuBase::UpdateInput( sInput );
}

bool CPauseStoreMenu::Populate(MenuScreenId newScreenId)
{
	bool bResult = false;

	bool showAcceptButton = true;

	if(newScreenId.GetValue() == MENU_UNIQUE_ID_STORE)
	{
		m_storeState = GetStoreState();

		switch(m_storeState)
		{
		case STATE_DISCONNECTED:
			ShowColumnWarning_Loc(PM_COLUMN_LEFT, 3, "NOT_CONNECTED", "WARNING_NOT_CONNECTED_TITLE");
#if !__XENON
			showAcceptButton = false;
#endif
			break;
		case STATE_UNDERAGE:
			ShowColumnWarning_Loc(PM_COLUMN_LEFT, 3, "HUD_PERM", "CWS_WARNING");
			break;
		case STATE_NOCONNECTION:
			ShowColumnWarning_Loc(PM_COLUMN_LEFT, 3, "HUD_NONETWORK", "CWS_WARNING");
#if !RSG_PC // if no connection, we'll bring up the sign-in page that will say "retry connection" ...so need accept for that
			showAcceptButton = false;
#endif
			break;
#if RSG_ORBIS
		case STATE_NP_UNAVAILABLE:
			ShowColumnWarning_Loc(PM_COLUMN_LEFT, 3, CPauseMenu::GetOfflineReasonAsLocKey(), "CWS_WARNING");
			break;
#endif // RSG_ORBIS
		case STATE_UNAVAILABLE_MP:
			ShowColumnWarning_Loc(PM_COLUMN_LEFT, 3, "STORE_UNAVAIL_MP", "WARNING_STORE_UNAVAIL_TITLE");
			showAcceptButton = false;
			break;
		case STATE_UNAVAILABLE:
#if __XENON
            ShowColumnWarning_Loc(PM_COLUMN_LEFT, 3, "SCLB_NO_ROS", "WARNING_STORE_UNAVAIL_TITLE");
#else
            ShowColumnWarning_Loc(PM_COLUMN_LEFT, 3, "STORE_UNAVAIL", "WARNING_STORE_UNAVAIL_TITLE");
#endif
			showAcceptButton = false;
			break;
		case STATE_NONE:
		default:
			bResult = PopulateStoreNews();
			break;
		}

		if (m_storeState != STATE_NONE)
		{
			bResult = false;
			m_bIsDisplayingNews = false;
#if RSG_ORBIS
			showAcceptButton = false; // Orbis only shows accept button when everything is fine. so override any other prev logic
#endif
		}

	}

	if (showAcceptButton)
	{
		SUIContexts::Deactivate("HIDE_ACCEPTBUTTON");
	}
	else
	{
		SUIContexts::Activate("HIDE_ACCEPTBUTTON");
	}


	return bResult;
}

eStoreState CPauseStoreMenu::GetStoreState()
{
	eStoreState storeState = STATE_NONE;
 
	if (IsLinkNotConnected())
	{
		storeState = STATE_NOCONNECTION;
	}
 	else if(IsNotConnected())
 	{
 #if RSG_ORBIS
 		const rlGamerInfo* pGamerInfo = NetworkInterface::GetActiveGamerInfo();
 		if( !g_rlNp.GetNpAuth().IsNpAvailable( pGamerInfo->GetLocalIndex() ) )
 		{
			storeState = STATE_NP_UNAVAILABLE;
		}
 		else
 #endif
 		{
 			storeState = STATE_DISCONNECTED;
 		}
 	}
 	else if(IsStoreUnavailableMP())
 	{
 		storeState = STATE_UNAVAILABLE_MP;
 	}
 	else if(IsStoreUnavailable())
 	{
 		//We do this to account for log in time.
 		if (m_StoreSigninFrameCount > 0)
 		{
 			storeState = STATE_DISCONNECTED;
 		}
 		else
 		{
 			storeState = STATE_UNAVAILABLE;
 		}
 		
 	}

	else if(CLiveManager::CheckIsAgeRestricted())
	{
		storeState = STATE_UNDERAGE;
	}
 
 	return storeState;
}

bool CPauseStoreMenu::IsLinkNotConnected()
{
#if RSG_PC
	// just check against not connected... can't be running without a SC account on PC
	// this function overrides IsNotConnected in conditions so use here so rest of code works
	return IsNotConnected();
#else
	return !netHardware::IsLinkConnected();
#endif
}

bool CPauseStoreMenu::IsNotConnected()
{
	return !CLiveManager::IsOnline();
}

bool CPauseStoreMenu::IsStoreUnavailableMP()
{
	// we're happy opening the store on PC, as it's on the SCUI menu
#if RSG_PC
	return false;
#else
	return NetworkInterface::IsGameInProgress() && CNetwork::GetNetworkSession().IsActivitySession();
#endif
}

bool CPauseStoreMenu::IsStoreUnavailable()
{
#if !RSG_PC
	// B*2413863 - moved this cloud save check to general unavailable, to display the correct generic "try again later" string
	if (NetworkInterface::IsNetworkOpen())
	{
		if (!MoneyInterface::SavegameCanBuyCashProducts())
			return true;
	}
#endif

	return !CPauseMenu::IsStoreAvailable();
}

bool CPauseStoreMenu::PopulateStoreNews()
{
	CNetworkPauseMenuNewsController& newsController = CNetworkPauseMenuNewsController::Get();

	if(!m_bIsDisplayingNews && newsController.InitControl(NEWS_TYPE_STORE_HASH))
	{
		int iNumStories = newsController.GetNumStories();
		if(iNumStories > 1)
		{
			if(!SUIContexts::IsActive("STORE_CanScroll"))
			{
				SUIContexts::Activate("STORE_CanScroll");
				CPauseMenu::RedrawInstructionalButtons();
			}
			UpdateStoreColumnScroll(iNumStories, newsController.GetPendingStoryIndex());
		}

		m_bIsDisplayingNews = true;
		ShowColumnWarning(PM_COLUMN_LEFT, 3, "");
		CScaleformMenuHelper::SET_BUSY_SPINNER(PM_COLUMN_MIDDLE, true);
	}
	else
	{
		ShowColumnWarning_Loc(PM_COLUMN_LEFT, 3, "", "CMRC_STORE_OPEN");
	}

	return true;
}

void CPauseStoreMenu::PostStoreStory(CNetworkSCNewsStoryRequest* pNewsItem, bool bUpdateTexture)
{
	CScaleformMenuHelper::SET_BUSY_SPINNER(PM_COLUMN_MIDDLE_RIGHT, false);

	if(bUpdateTexture)
	{
		ShowColumnWarning(PM_COLUMN_LEFT, 3, pNewsItem->GetContent(), pNewsItem->GetHeadline(), 0, pNewsItem->GetTxdName(), pNewsItem->GetTxdName(), IMG_ALIGN_RIGHT);
	}
	else
	{
		ShowColumnWarning(PM_COLUMN_LEFT, 3, pNewsItem->GetContent(), pNewsItem->GetHeadline(), 0, "", "", IMG_ALIGN_RIGHT);
	}

	CScaleformMenuHelper::SET_BUSY_SPINNER(PM_COLUMN_MIDDLE, false);

}

void CPauseStoreMenu::UpdateStoreColumnScroll(int iNumStories, int iSelectedStory)
{
	char pszCaption[64] = {0};
	CNumberWithinMessage pArrayOfNumbers[2];
	pArrayOfNumbers[0].Set(iSelectedStory+1);
	pArrayOfNumbers[1].Set(iNumStories);
	if ( iNumStories > 1 )
	{
		CMessages::InsertNumbersAndSubStringsIntoString(TheText.Get("NEWS_NUM_STORIES"),pArrayOfNumbers,2,NULL,0,pszCaption,64);
		CScaleformMenuHelper::SET_COLUMN_SCROLL(PM_COLUMN_LEFT, pszCaption);
	}
	
}

void CPauseStoreMenu::LoseFocus()
{
	uiDebugf3("CPauseStoreMenu::LoseFocus");
	SUIContexts::Deactivate("STORE_CanScroll");
	m_bIsDisplayingNews = false;

	// Cleanup Newswire
	CNetworkPauseMenuNewsController::Get().Reset();
}


void CPauseStoreMenu::DisplayUnderageWarning()
{
#if RSG_DURANGO
	// Display the Xbox system UI telling the user they can't access the store.
	uiVerifyf(
		CLiveManager::ResolvePlatformPrivilege(
			NetworkInterface::GetLocalGamerIndex(), 
			rlPrivileges::PrivilegeTypes::PRIVILEGE_PURCHASE_CONTENT, 
			true),
		"CLiveManager::ResolvePlatformPrivilege with attemptResolution = true failed, so the system UI won't be shown");
#else
	CWarningMessage& rMessage = CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage();
	CWarningMessage::Data messageData;
	
	messageData.m_TextLabelBody = "HUD_AGERES";
	messageData.m_iFlags = FE_WARNING_OK;
	messageData.m_acceptPressed = datCallback(MFA(CMenuScreen::BackOutOfWarningMessageToContextMenu), (datBase*)&m_Owner);

	rMessage.SetMessage(messageData);
#endif
}

void CPauseStoreMenu::DisplayLeaveSessionWarning()
{
	CWarningMessage& rMessage = CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage();
	CWarningMessage::Data messageData;
	messageData.m_TextLabelBody = "GOTO_STORE_CONFIRM";
	messageData.m_TextLabelSubtext = "PM_QUIT_WARN11";
	messageData.m_iFlags = FE_WARNING_YES_NO;
	messageData.m_acceptPressed = datCallback(MFA(CPauseStoreMenu::ConfirmDestructiveActionCallback), (datBase*)&m_Owner);
	messageData.m_declinePressed = datCallback(MFA(CMenuScreen::BackOutOfWarningMessageToContextMenu), (datBase*)&m_Owner);

	rMessage.SetMessage(messageData);
}

void CPauseStoreMenu::DisplayStoreUnavailableWarning()
{
	CWarningMessage& rMessage = CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage();
	CWarningMessage::Data messageData;

#if __XENON || RSG_DURANGO
	if ( !CLiveManager::IsSignedIn() || CLiveManager::IsOnline() )
	{
		messageData.m_TextLabelBody = "STORE_UNAVAIL";	
	}
	else
	{
		messageData.m_TextLabelBody = "SCLB_NO_ROS";	
	}
#else
	messageData.m_TextLabelBody = "STORE_UNAVAIL";
#endif

	
	messageData.m_iFlags = FE_WARNING_OK;
	messageData.m_acceptPressed = datCallback(MFA(CWarningMessage::Clear), &CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage());

	rMessage.SetMessage(messageData);
}

void CPauseStoreMenu::ConfirmDestructiveActionCallback()
{
	// remove the warning message
	CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage().Clear();
	CPauseMenu::TriggerStore(true);
	//cStoreScreenMgr::RequestMPStore();
}
