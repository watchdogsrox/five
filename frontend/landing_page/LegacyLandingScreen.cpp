/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : LegacyLandingScreen.cpp
// PURPOSE : Legacy landing screen, used on PC builds
//
// AUTHOR  : james.strain
// STARTED : 2015, relocated October 2020
//
/////////////////////////////////////////////////////////////////////////////////
#include "LegacyLandingScreen.h"

// rage
#include "system/param.h"

// game
#include "frontend/landing_page/LandingPageConfig.h"
#include "frontend/PauseMenu.h"
#include "frontend/NewHud.h"
#include "frontend/UIContexts.h"

#if RSG_PC

#if UI_LANDING_PAGE_ENABLED && !__FINAL
XPARAM(skipLandingPage);
#endif

NOSTRIP_PC_XPARAM(StraightIntoFreemode);

CLegacyLandingScreen::CLegacyLandingScreen()
	: m_context( Context::INITIAL )
	, m_currentSelection(-1)
{
	// Create Landing Movie
	m_movie.CreateMovieAndWaitForLoad(SF_BASE_CLASS_GENERIC, "LANDING_PAGE", Vector2(0.0f, 0.0f), Vector2(1.0f,1.0f), false);
	s32 movieID = m_movie.GetMovieID();

	if(uiVerifyf(movieID != INVALID_MOVIE_ID, "Failed to load landing screen movie"))
	{
		CScaleformMgr::ForceMovieUpdateInstantly( movieID, true);  // this movie requires instant updating
		UpdateDisplayConfig();
	}

	DrawButtons();

	CLoadingScreens::GoToLanding();

	SUIContexts::Activate("OnLandingPage");
}

CLegacyLandingScreen::~CLegacyLandingScreen()
{
	SUIContexts::Deactivate("OnLandingPage");
	SUIContexts::Deactivate("HadMappingConflict");
}

void CLegacyLandingScreen::Init()
{
	SLegacyLandingScreen::Instantiate();
}

bool CLegacyLandingScreen::IsLandingPageInitialized()
{
    return SLegacyLandingScreen::IsInstantiated();
}

void CLegacyLandingScreen::Shutdown()
{
	CLoadingScreens::ExitLanding();

	if( SLegacyLandingScreen::IsInstantiated() )
	{
		SLegacyLandingScreen::Destroy();
	}
}

void CLegacyLandingScreen::DeviceReset()
{
	if( SLegacyLandingScreen::IsInstantiated() )
	{
		SLegacyLandingScreen::GetInstance().UpdateDisplayConfig();

		if(CNetworkTransitionNewsController::Get().IsActive())
		{
			CNetworkTransitionNewsController::Get().UpdateDisplayConfig();
			CNetworkTransitionNewsController::Get().SetAlignment(CHudTools::GetWideScreen());
		}
	}
}

bool CLegacyLandingScreen::IsLandingPageEnabled()
{
	const bool c_userMappingFileNeedsUpdating = CControlMgr::GetPlayerMappingControl().DoesUsersMappingFileNeedUpdating();
	const bool c_localPlayerIsOnline = NetworkInterface::IsLocalPlayerOnline();
	const bool c_landingPageMenuPreferenceEnabled = CPauseMenu::GetMenuPreference(PREF_LANDING_PAGE) == 1;

#if UI_LANDING_PAGE_ENABLED && !__FINAL
	const bool c_skipLandingPage = PARAM_skipLandingPage.Get();
#else
	const bool c_skipLandingPage = false;
#endif

	const bool c_isLandingPageEnabled = c_userMappingFileNeedsUpdating
		|| (!c_skipLandingPage && c_localPlayerIsOnline && c_landingPageMenuPreferenceEnabled);

	return c_isLandingPageEnabled;
}

bool CLegacyLandingScreen::Update()
{
    bool bExit = false;

    PF_FRAMEINIT_TIMEBARS(0);

    CApp::CheckExit();

    sysIpcSleep(33);
    fwTimer::Update();

    // need to update the PC SCUI at all times
    CNetwork::Update(true);

    g_SysService.UpdateClass();

    // FlushAndDeviceCheck needed for window resizes, alt tabs, etc
    gRenderThreadInterface.FlushAndDeviceCheck();

    CControlMgr::StartUpdateThread();
    CControlMgr::WaitForUpdateThread();

    // Call before CLoadingScreens::KeepAliveUpdate (where SF mouse events come in)
    CMousePointer::ResetMouseInput();

    if(!CWarningScreen::IsActive())
    {
        if(!CLoadingScreens::AreSuspended())
        {
            // Warning Screen handles CScaleformMgr::UpdateAtEndOfFrame itself during initial boot
            CLoadingScreens::KeepAliveUpdate(); // Updates Network, News, Scaleform, Texture requests, and Streaming
        }
    }

    CMousePointer::Update();

    switch(m_context)
    {
    case Context::INITIAL:
        if( CControlMgr::GetPlayerMappingControl().DoesUsersMappingFileNeedUpdating() )
        {
            SetContext(Context::KEYMAPPING);

            CPauseMenu::Open(ATSTRINGHASH("FE_MENU_VERSION_LANDING_KEYMAPPING_MENU",0xbd328ec));
            SUIContexts::Activate("OnLandingPage");
            SUIContexts::Activate("HadMappingConflict");

            CWarningMessage::Data messageData;
            messageData.m_TextLabelHeading = "CWS_WARNING";
            messageData.m_TextLabelBody = "MAPPING_TU_CONFLICTS";
            messageData.m_iFlags = FE_WARNING_OK;
            messageData.m_bCloseAfterPress = true;
            CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage().SetMessage(messageData);

            if(CNetworkTransitionNewsController::Get().IsActive())
            {
                // Hide News
                CNetworkTransitionNewsController::Get().SetPaused(true);
            }
            break;
        }
        else
        {
            SetContext(Context::MAIN);
            // fallthru on purpose 
        }

    case Context::MAIN:

        if(!CLoadingScreens::IsCommitedToMpSp())
        {
            BtnType buttonPressed = CheckInput();

            switch(buttonPressed)
            {
            case BtnType::STORY_MODE: 
                CNetwork::SetGoStraightToMultiplayer(false);

                CLoadingScreens::SetScreenOrder(true);
                CLoadingScreens::SetNewsScreenOrder(true);

                gnetDebug3("CLandingScreen::Update -- Exiting Landing page due to Story Mode selected");
                bExit = true;
                break;
            case BtnType::ONLINE: 
                //if(PARAM_StraightIntoFreemode.Get())
                {
                    CNetwork::SetGoStraightToMultiplayer(true);
                    CNetwork::SetGoStraightToMPEvent(false);
                    CNetwork::SetGoStraightToMPRandomJob(false);
                    CLoadingScreens::SetScreenOrder(false);
                    CLoadingScreens::SetNewsScreenOrder(false);

                    gnetDebug3("UpdateLandingScreen :: Exiting Landing page due to Online selected %s", PARAM_StraightIntoFreemode.Get() ? "-StraightIntoFreeMode" : "");
                    bExit = true;
                }
                break;
            case BtnType::RANDOM_JOB: 
                CNetwork::SetGoStraightToMultiplayer(true);
                CNetwork::SetGoStraightToMPEvent(false);
                CNetwork::SetGoStraightToMPRandomJob(true);
                CLoadingScreens::SetScreenOrder(false);
                CLoadingScreens::SetNewsScreenOrder(false);

                gnetDebug3("UpdateLandingScreen :: Exiting Landing page due to Random Job selected");
                bExit = true;
                break;
            case BtnType::EVENT:
                CNetwork::SetGoStraightToMultiplayer(true);
                CNetwork::SetGoStraightToMPEvent(true);
                CNetwork::SetGoStraightToMPRandomJob(false);
                CLoadingScreens::SetScreenOrder(false);
                CLoadingScreens::SetNewsScreenOrder(false);

                gnetDebug3("UpdateLandingScreen :: Exiting Landing page due to FRONTEND_INPUT_RB (MP Event)");
                bExit = true;
                break;
            case BtnType::SETTINGS:
                SetContext( Context::SETTINGS);

                CPauseMenu::Open(FE_MENU_VERSION_LANDING_MENU);
                SUIContexts::Activate("OnLandingPage");

                if(CNetworkTransitionNewsController::Get().IsActive())
                {
                    // Hide News
                    CNetworkTransitionNewsController::Get().SetPaused(true);
                }
                break;
            case BtnType::QUIT:
                SetContext(Context::CONFIRM_QUIT);
                CLoadingScreens::Suspend();
                break;
            case BtnType::NONE:
            default:
                break;
            }
        }
        break;

    case Context::KEYMAPPING:
    case Context::SETTINGS:
        if (CPauseMenu::IsActive())
        {
            CPauseMenu::Update();

            if( m_context == Context::SETTINGS)
            {
                CSettingsManager::GetInstance().SafeUpdate();

#if COLLECT_END_USER_BENCHMARKS
                // if pause menu is no longer active after this frame's update and triggered the benchmark tests, exit landing
                if(!CPauseMenu::IsActive() && EndUserBenchmarks::GetWasStartedFromPauseMenu(FE_MENU_VERSION_LANDING_MENU))
                {
                    CNetwork::SetGoStraightToMultiplayer(false);
                    gnetDebug3("CLandingScreen::Update -- Exiting Landing page due to Benchmark Tests selected");
                    bExit = true;
                }
#endif
            }
        }
        else
        {
            // We've exited the pause menu, go back to main landing screen
            SetContext(Context::MAIN);

            if(CNetworkTransitionNewsController::Get().IsActive())
            {
                // Unpause News
                CNetworkTransitionNewsController::Get().SetPaused(false);
            }
        }

        break;
    case Context::CONFIRM_QUIT:
        {
            CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "WARNING_EXIT_WINDOWS2", "EXIT_SURE_2", FE_WARNING_YES_NO);
            eWarningButtonFlags result = CWarningScreen::CheckAllInput();

            if(result == FE_WARNING_YES)
            {
                CPauseMenu::SetGameWantsToExitToWindows(true);
                NetworkInterface::GetNetworkExitFlow().StartShutdownTasks();
            }
            else if (result == FE_WARNING_NO)
            {
                SetContext(Context::MAIN);
                CLoadingScreens::Resume();
            }

            break;
        }
    default:
        break;
    }

    // Warning Screens are updated after everything that could set it
    // (as done during normal gameplay)
    if(CWarningScreen::IsActive())
    {
        CWarningScreen::Update();
    }

    bool bPlayAudio = bExit;

    if(CLoadingScreens::IsCommitedToMpSp())
    {
        // If something else has committed to an action (i.e. presence event invites) let's exit the landing
        gnetDebug3("UpdateLandingScreen :: Exiting Landing page due to IsCommitedToMpSp");
        bExit = true;
    }

#if RSG_ORBIS || RSG_PC
    if (PARAM_StraightIntoFreemode.Get())
    {
        // If something else has committed to an action (i.e. -StraightIntoFreemode) let's exit the landing
        gnetDebug3("UpdateLandingScreen :: Exiting Landing page due to PARAM_StraightIntoFreemode");
        bExit = true;
    }
#endif

    // A user has signed out, we must exit the landing page flow and return to the process require entitlement state
    if (CGame::IsCheckingEntitlement())
    {
        gnetDebug3("UpdateLandingScreen :: Exiting Landing page due to IsCheckingEntitlement");

        // early out
        m_movie.RemoveMovie();
        PF_FRAMEEND_TIMEBARS();
        return true;
    }

    if(bExit)
    {
        // Clear Buttons
        m_movie.RemoveMovie();

        if(bPlayAudio)
        {
            g_FrontendAudioEntity.PlaySound("Select", "UI_LANDING_SCREEN_SOUNDS");
        }
        g_FrontendAudioEntity.StartLoadingTune();

        // Reset mouse input, and fake a key press so that the mouse stops rendering
        // This is needed because the mouse pointer update will no longer invoke after exiting the landing page
        CMousePointer::ResetMouseInput();
        CMousePointer::SetKeyPressed();
    }

    PF_FRAMEEND_TIMEBARS();
    return bExit;
}

// Render Thread
void CLegacyLandingScreen::Render()
{
    // Render the pause menu ourselves when viewing landing settings
    if((m_context == Context::SETTINGS || m_context == Context::KEYMAPPING) && CPauseMenu::IsActive())
    {
        PauseMenuRenderDataExtra newData;
        CPauseMenu::SetRenderData(newData);

        CPauseMenu::RenderBackgroundContent(newData);
        CPauseMenu::RenderForegroundContent(newData);

        // SafeZone rendering
        if(CNewHud::ShouldDrawSafeZone())
        {
            CNewHud::DrawSafeZone();
        }
    }
    else
    {
        // Render landing movie when not rendering the pause menu
        m_movie.Render();
    }
}

void CLegacyLandingScreen::UpdateDisplayConfig()
{
	m_movie.SetDisplayConfig(CScaleformMgr::SDC::ForceWidescreen | CScaleformMgr::SDC::UseFakeSafeZoneOnBootup);
	CScaleformMgr::UpdateMovieParams(m_movie.GetMovieID(), CScaleformMgr::GetRequiredScaleMode(m_movie.GetMovieID(), true));
}

CLegacyLandingScreen::BtnType CLegacyLandingScreen::CheckInput()
{
    s32 movieID = m_movie.GetMovieID();

    if(m_currentSelection != -1 && CPauseMenu::CheckInput(FRONTEND_INPUT_ACCEPT))
    {
        return m_buttons[m_currentSelection];
    }
    else
    {
        if(CPauseMenu::CheckInput(FRONTEND_INPUT_LEFT))
        {
            if(m_currentSelection > 0)
            {
                SetSelection(m_currentSelection - 1);
            }
        }
        else if(CPauseMenu::CheckInput(FRONTEND_INPUT_RIGHT))
        {
            if(m_currentSelection < m_buttons.GetCount() - 1)
            {
                SetSelection(m_currentSelection + 1);
            }
        }

        for(int i = 0; i < m_buttons.GetCount(); ++i)
        {
            if(CMousePointer::IsSFMouseReleased(movieID, i))
            {
                return m_buttons[i];
            }
            else if(CMousePointer::IsSFMouseRollOver(movieID, i))
            {
                SetSelection(i);
            }
        }

        return BtnType::NONE;
    }
}

void CLegacyLandingScreen::DrawButtons()
{
	m_buttons.Reset();

	if(m_movie.BeginMethod("INIT_LANDING_PAGE"))
	{
		AddButton(BTN_LABEL_QUIT,		BtnType::QUIT);
		AddButton(BTN_LABEL_SETTINGS,	BtnType::SETTINGS);

		// Online Options
		if(CanSelectOnline())
		{
			if(SocialClubEventMgr::Get().HasFeaturedEvent() ORBIS_ONLY(|| ShouldShowPlusUpsell()))
			{
				AddButton(GetEventButtonText(false), BtnType::EVENT, false);
			}

			if(SocialClubEventMgr::Get().GetDataReceived())
			{
				AddButton(GetJobButtonText(false), BtnType::RANDOM_JOB, false);
			}

			AddButton(BTN_LABEL_ONLINE, BtnType::ONLINE);
		}

 		AddButton(BTN_LABEL_STORY, BtnType::STORY_MODE);

		m_movie.EndMethod();
	}
}

// Assumes we're building the Scaleform movie button function arguments
void CLegacyLandingScreen::AddButton(const char* label, BtnType type, bool bIsLocString)
{
	m_movie.AddParam(bIsLocString ? TheText.Get(label) : label);
	m_buttons.PushAndGrow(type);
}

void CLegacyLandingScreen::SetSelection(int i)
{
	if(uiVerify(i >= 0 && i < m_buttons.GetCount()))
	{
		m_movie.CallMethod("SET_BUTTON_SELECTED", i);
		m_currentSelection = i;
	}
}

#endif // RSG_PC
