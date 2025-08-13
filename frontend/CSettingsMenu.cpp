#include "Frontend/CSettingsMenu.h"

// rage headers:
#if __BANK
#include "bank/bkmgr.h"
#endif // __BANK
#include "net/nethardware.h"
#include "parsercore/element.h"
#include "Network/Facebook/Facebook.h"

// game stuff
#include "audio/northaudioengine.h"
#include "audio/radiostation.h"
#include "Network/Live/liveManager.h"
#include "Frontend/DisplayCalibration.h"
#include "Frontend/ControllerLabelMgr.h"
#include "frontend/landing_page/LegacyLandingScreen.h"
#include "Frontend/ProfileSettings.h"
#include "Frontend/ui_channel.h"
#include "Frontend/ButtonEnum.h"
#include "Frontend/WarningScreen.h"
#include "script/script_hud.h"
#include "streaming/streaming.h"
#include "Peds/PlayerInfo.h"
#include "Peds/ped.h"
#include "system/SettingsDefaults.h"
#if RSG_PC
#include "script/script.h"
#include "script/streamedscripts.h"
#include "script/thread.h"
#include "Frontend/PCGamepadCalibration.h"
#endif // RSG_PC

#if COMMERCE_CONTAINER
#include <cell/sysmodule.h>
#endif

FRONTEND_MENU_OPTIMISATIONS()

#if __DEV
#define UIATSTRINGHASH(str,code) str
#else
#define UIATSTRINGHASH(str,code) ATSTRINGHASH(str,code)
#endif

#define HIDE_ACCEPTBUTTON UIATSTRINGHASH("HIDE_ACCEPTBUTTON",0x14211b54)

#define ARBITRARY_OFFSET_TO_FAKE_HISTORY 10000

CSettingsMenu::CSettingsMenu(CMenuScreen& owner)
	: CMenuBase(owner)
	, m_eState(SS_OffPage)
	, m_bOnFacebookTab(false)
	, m_bOnControllerTab(false)
	, m_bInsideSystemUI(false)
	, m_bInSettingsColumn(false)
	, m_bHaveNavigatedToTheRightMenu(false)
#if RSG_PC
	, m_uGfxApplyTimestamp(0)
	, m_bFailedApply(false)
	, m_bWriteFailed(false)
	, m_bOnVoiceTab(false)
	, m_bDisplayRestartWarning(false)
	, m_bDisplayOnlineWarning(false)
	, m_bHasShownOnlineGfxWarning(false)
#endif // RSG_PC
#if COMMERCE_CONTAINER
	, m_bHaveLoadedPRX(false)
	, m_bWasWirelessHeadsetAvailable(false)
#endif
#if __BANK
	, m_iFakedStatus(SS_OffPage)
	, m_pDebugWidget(NULL)
#endif
{
	int temp;
	owner.FindDynamicData("fbookColumnHeight", temp, 403);
	m_fbookPageHeight = s16(temp);

	CPauseMenu::SetMenuPrefSelected(PREF_INVALID);
}


CSettingsMenu::~CSettingsMenu()
{
	CPauseMenu::SetMenuPrefSelected(PREF_INVALID);
#if __BANK
	if( m_pDebugWidget )
	{
		m_pDebugWidget->Destroy();
		m_pDebugWidget = NULL;
	}
#endif

	COMMERCE_CONTAINER_ONLY(PreparePRX(false));
}


#if COMMERCE_CONTAINER
void CSettingsMenu::PreparePRX(bool bLoadOrNot)
{
	if( bLoadOrNot != m_bHaveLoadedPRX )
	{
		m_bHaveLoadedPRX = bLoadOrNot;
		if( bLoadOrNot )
		{
			CLiveManager::GetCommerceMgr().ContainerLoadGeneric(CELL_SYSMODULE_AVCONF_EXT, "Audio Pulse Wireless Detection");

			audNorthAudioEngine::CheckForWirelessHeadset();
			m_bWasWirelessHeadsetAvailable = audNorthAudioEngine::IsWirelessHeadsetConnected();
			CPauseMenu::HandleWirelessHeadsetContextChange();
		}
		else
		{
			CLiveManager::GetCommerceMgr().ContainerUnloadGeneric(CELL_SYSMODULE_AVCONF_EXT, "Audio Pulse Wireless Detection");
		}
	}
}
#endif

void CSettingsMenu::LoseFocus()
{
	SUIContexts::Deactivate("FB_Disabled");
	SUIContexts::Deactivate("FB_NeedsSignUp");
	SUIContexts::Deactivate("FB_Ready");
	CPauseMenu::SetMenuPrefSelected(PREF_INVALID);

#if __XENON
#if __BANK
	if (CProfileSettings::GetUpdateProfileWhenNavigatingSettingsMenu())
#endif	//	__BANK
	{
		CPauseMenu::UpdateProfileFromMenuOptions();
	}
#endif	//	__XENON

	m_bHaveNavigatedToTheRightMenu = false;
	m_bInSettingsColumn = false;
	COMMERCE_CONTAINER_ONLY(PreparePRX(false));
}

void CSettingsMenu::PopulateWithMessage(const char* pHeader, const char* pBody, bool bLeaveRoom)
{
	if(m_bInSettingsColumn && !bLeaveRoom)
	{
		m_bInSettingsColumn = false;
		CPauseMenu::MENU_SHIFT_DEPTH(kMENUCEPT_SHALLOWER, false, true );
	}
	
	ShowColumnWarning_EX(PM_COLUMN_MIDDLE_RIGHT, pBody, pHeader, bLeaveRoom?m_fbookPageHeight:0);
	if( bLeaveRoom )
		SHOW_COLUMN(PM_COLUMN_MIDDLE,true);
}

void CSettingsMenu::Update()
{
#if RSG_PS3
	if(m_bHaveLoadedPRX)
	{
		audNorthAudioEngine::CheckForWirelessHeadset();
		if(m_bWasWirelessHeadsetAvailable != audNorthAudioEngine::IsWirelessHeadsetConnected())
		{		
			CPauseMenu::HandleWirelessHeadsetContextChange();
			m_bWasWirelessHeadsetAvailable = audNorthAudioEngine::IsWirelessHeadsetConnected();
		}
	}
#endif // RSG_PS3

#if RSG_PC
	HandlePCWarningScreens();
#endif // RSG_PC

	if( !m_bOnFacebookTab )
	{
		if(m_bOnControllerTab)
		{
			CControllerLabelMgr::Update();
			if(CControllerLabelMgr::GetLabelsChanged())
			{
				PopulateControllerLabels(GetControllerImageHeader());
				CControllerLabelMgr::SetLabelsChanged(false);
			}
		}

#if RSG_PC
		if(m_bOnVoiceTab)
		{
			CPauseMenu::UpdateVoiceBar(true);
		}
#endif // RSG_PC

		return;
	}

	RL_FACEBOOK_ONLY(CFacebook& facebook = CLiveManager::GetFacebookMgr());
	int localGamerIndex = NetworkInterface::GetLocalGamerIndex();

    bool hasFacebookPermission = 
#if RSG_NP
		// on PS4, sub-accounts are blocked from linking to a Facebook account. 
		// We can't explicitly detect a sub-account with no privilege restrictions so for consistency, allow linking in all
		// cases and let the system UI handle the error messaging (this states that this is due to sub-account)
		true; 
#else
		NetworkInterface::IsLocalPlayerSignedIn() && CLiveManager::GetSocialNetworkingSharingPrivileges();
#endif

	// taking the cue from somewhere else, can't use HasPrivilege for Banned because it'll return true if they're not valid... though we probably should just fix that...
	bool bBanned = RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex) 
		&& (rlRos::GetCredentials(localGamerIndex).HasPrivilege(RLROS_PRIVILEGEID_BANNED)
		|| 	!rlRos::HasPrivilege(localGamerIndex, RLROS_PRIVILEGEID_CREATE_TICKET));

#if __BANK
#define CHECK_STATUS(orig, faked) CheckStatus(orig, faked)
#else
#define CHECK_STATUS(orig, faked) orig
#endif

	
	SignupStatus stateAtStart = m_eState;

	bool isFacebookBusy = RL_FACEBOOK_SWITCH(facebook.IsBusy(), false);
	bool hasFacebookAppToken = RL_FACEBOOK_SWITCH(facebook.HasAccessToken(), false);

	// no cable plugged in
	if( CHECK_STATUS( !netHardware::IsLinkConnected(), SS_Offline ) )
	{
		if( ChangeState( SS_Offline ) )
		{
			PopulateWithMessage("CWS_WARNING", "HUD_NONETWORK", false);
		}
	}
	// no internet connection
	else if( CHECK_STATUS( !NetworkInterface::IsLocalPlayerOnline(), SS_Offline ) )
	{
#if RSG_ORBIS
		// doubles down a bit on what happens in the last else here, but "seems" safer?
		if( !g_rlNp.IsNpAvailable( NetworkInterface::GetLocalGamerIndex() ) )
		{
			const rlNpAuth::NpUnavailabilityReason c_npReason = g_rlNp.GetNpAuth().GetNpUnavailabilityReason( NetworkInterface::GetLocalGamerIndex() );
			uiDisplayf("CPlayerListMenu::HandleNoPlayers - Network is not available. Reason code %d", c_npReason );

			if( c_npReason != rlNpAuth::NP_REASON_INVALID )
			{
				PopulateWithMessage("WARNING_NOT_CONNECTED_TITLE", CPauseMenu::GetOfflineReasonAsLocKey(), false);
			}
		}
		else
#endif
// 		if (NetworkInterface::IsLocalPlayerSignedIn())
// 		{
// 			if( ChangeState( SS_Offline ) )
// 			{
// 				PopulateWithMessage("WARNING_NOT_CONNECTED_TITLE", "NOT_IN_OFFLINE_MODE", false);
// 			}
// 		}
// 		else
//		{
		if( ChangeState( SS_Offline ) )
		{
			PopulateWithMessage("WARNING_NOT_CONNECTED_TITLE", CPauseMenu::GetOfflineReasonAsLocKey(), false);
		}
//		}
		
	}
	// you're blocked/banned
	else if( CHECK_STATUS( bBanned, SS_Banned ) )
	{
		if( ChangeState( SS_Banned ) )
		{
			SUIContexts::Deactivate("FB_NeedsSignUp");
			SUIContexts::Deactivate("FB_Ready");

			PopulateWithMessage("CWS_WARNING", "HUD_SCSBANNED", false);
		}
		m_bInsideSystemUI = false;
	}
	// facebook is down/cloud/whatever
	else if( CHECK_STATUS( !NetworkInterface::IsOnlineRos(), SS_FacebookDown ) )
	{
		if( ChangeState( SS_FacebookDown ) )
		{
			PopulateWithMessage("CWS_WARNING", "ERROR_FB_DOWN", false);
		}
	}
	else if( CHECK_STATUS( !hasFacebookPermission, SS_NoPermission ) )
	{
		if( ChangeState( SS_NoPermission ) )
		{
			SUIContexts::Deactivate("FB_NeedsSignUp");
			SUIContexts::Deactivate("FB_Ready");

			PopulateWithMessage("PM_PANE_FB", "FB_NOPERMISSION", true);
		}
		m_bInsideSystemUI = false;
	}
	// no social club (connection?)
	else if( CHECK_STATUS( !NetworkInterface::HasValidRockstarId(), SS_NoSocialClub ) )
	{
		if( ChangeState( SS_NoSocialClub ) )
		{
			PopulateWithMessage("WARNING_NO_SC_TITLE", "CRW_JOINSC", false);
		}
	}
	// policy is old (why do we need this here...?)
	else if( CHECK_STATUS( !CLiveManager::GetSocialClubMgr().IsOnlinePolicyUpToDate(), SS_OldPolicy ) )
	{
		if( ChangeState( SS_OldPolicy ) )
		{
			PopulateWithMessage("WARNING_UPDATE_SC_TITLE", "CRW_UPDATESC", false);
		}
	}
	else if( CHECK_STATUS(isFacebookBusy, SS_Busy ) )
	{
		if( ChangeState( SS_Busy ) )
		{
			PopulateWithMessage("PM_PANE_FB", "FB_BUSY", false);
		}
	}
	else if( CHECK_STATUS( hasFacebookAppToken, SS_TotallyFine ) )
	{
		if( ChangeState( SS_TotallyFine ) )
		{
			SUIContexts::Deactivate("FB_NeedsSignUp");
			SUIContexts::Activate("FB_Ready");

			if( CPauseMenu::GenerateMenuData(MENU_UNIQUE_ID_SETTINGS_FACEBOOK) > 0 && m_bInSettingsColumn )
			{
				CScaleformMenuHelper::SET_COLUMN_FOCUS(PM_COLUMN_MIDDLE,true);
				// fake the shit out of this
				int iOnScreen = 0;
				
				if( m_Owner.FindItemIndex(MENU_UNIQUE_ID_SETTINGS_FACEBOOK, &iOnScreen, true) != -1 )
				{
					m_bInSettingsColumn = false;
					TriggerEvent(MENU_UNIQUE_ID_SETTINGS_FACEBOOK, iOnScreen );
					CScaleformMenuHelper::SET_COLUMN_FOCUS(PM_COLUMN_MIDDLE,true);
				}
			}
			else
			{
				CScaleformMenuHelper::SET_COLUMN_FOCUS(PM_COLUMN_LEFT,true);
			}
			PopulateWithMessage("PM_PANE_FB", "FB_CONNECTED", true);
		}
		m_bInsideSystemUI = false;
	}
	else
	{
		// if we think we prompted the screen and it didn't succeed, throw up a warning.
		bool didSucceed = RL_FACEBOOK_SWITCH(facebook.DidSucceed(), false);
		if( m_bInsideSystemUI 
			&& !didSucceed
#if RL_FACEBOOK_ENABLED
			&& facebook.GetLastResult() != RL_FB_GET_TOKEN_FAILED_USER_CANCELLED
#if __WIN32PC || RSG_DURANGO // due to the external web browser on PC and XB1, we don't get a callback we can track fro cancelled, so we'll get this
			&& facebook.GetLastResult() != RL_FB_DOESNOTEXIST_ROCKSTARACCOUNT
#endif
			&& (int)facebook.GetLastResult() != RL_FB_ERROR_AUTHENTICATIONFAILED_OAUTHEXCEPTION
#endif // RL_FACEBOOK_ENABLED
			)
		{
			CWarningMessage& rMessage = CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage();

			CWarningMessage::Data messageData;
			messageData.m_TextLabelBody = "ERROR_FB_DOWN";
			messageData.m_iFlags = FE_WARNING_OK;
			messageData.m_bCloseAfterPress = true;

			rMessage.SetMessage(messageData);
		}

		if( ChangeState( SS_NoFacebook ) )
		{
			SUIContexts::Activate("FB_NeedsSignUp");
			SUIContexts::Deactivate("FB_Ready");

			if( CPauseMenu::GenerateMenuData(MENU_UNIQUE_ID_SETTINGS_FACEBOOK) > 0 && m_bInSettingsColumn )
			{
				CScaleformMenuHelper::SET_COLUMN_FOCUS(PM_COLUMN_MIDDLE,true);
			}
			else
			{
				CScaleformMenuHelper::SET_COLUMN_FOCUS(PM_COLUMN_LEFT,true);
			}
			PopulateWithMessage("PM_PANE_FB", "FB_JOINNOW", true);
		}

		m_bInsideSystemUI = false;
	}


	if( m_eState != stateAtStart )
	{
		SUIContexts::SetActive(HIDE_ACCEPTBUTTON, !IsInStateWithAButton() );
		CPauseMenu::RedrawInstructionalButtons();
	}
}

bool CSettingsMenu::ShouldPlayNavigationSound(bool UNUSED_PARAM(bNavUp))
{
	if(!m_bInSettingsColumn)
		return true;

	return CPauseMenu::GetCurrentActivePanelData().WouldShowAtLeastTwoItems();
}

bool CSettingsMenu::TriggerEvent(MenuScreenId MenuId, s32 iUniqueId)
{
	if (MenuId == MENU_UNIQUE_ID_BRIGHTNESS_CALIBRATION)
	{
		CDisplayCalibration::LoadCalibrationMovie(CPauseMenu::GetMenuPreference(PREF_GAMMA));
		CPauseMenu::sm_bWaitOnDisplayCalibrationScreen = true;
		CDisplayCalibration::SetActive(true);
		return true;
	}
	else if(CPauseMenu::GetMenuPrefSelected() == PREF_RESTORE_DEFAULTS)
	{
		CPauseMenu::RestoreDefaults(CPauseMenu::GetCurrentActivePanelData().MenuScreen);
		return true;
	}

#if RSG_ORBIS
	else if( u32(MenuId.GetValue()) == ATSTRINGHASH("PREF_SAFEZONE_SIZE",0xc9fbb259))
	{
		CPauseMenu::ShowDisplaySafeAreaSettings();
		return true;
	}
#endif
#if RSG_PC
	else if (u32(MenuId.GetValue()) == ATSTRINGHASH("PREF_PCGAMEPAD",0xf9aa1b46))
	{
		CPCGamepadCalibration::LoadCalibrationMovie();
		CPauseMenu::sm_bWaitOnPCGamepadCalibrationScreen = CPCGamepadCalibration::IsLoading() || CPCGamepadCalibration::IsActive() ;
		return true;
	}
	else if(u32(MenuId.GetValue()) == ATSTRINGHASH("UR_COMPLETESCAN", 0xB49544B8))
	{
		audRadioStation::GetUserRadioTrackManager()->StartMediaScan(true);
		SUIContexts::SetActive(UIATSTRINGHASH("AUD_FSCANNING",0x64E3F10A), audRadioStation::GetUserRadioTrackManager()->IsScanning());
		CPauseMenu::GenerateMenuData(CPauseMenu::GetCurrentActivePanelData().MenuScreen, true);
		CPauseMenu::RedrawInstructionalButtons();
		return true;
	}
	else if(u32(MenuId.GetValue()) == ATSTRINGHASH("UR_QUICKSCAN", 0xAB72249C))
	{
		audRadioStation::GetUserRadioTrackManager()->StartMediaScan(false);
		SUIContexts::SetActive(UIATSTRINGHASH("AUD_QSCANNING",0x54348A17), audRadioStation::GetUserRadioTrackManager()->IsScanning());
		CPauseMenu::GenerateMenuData(CPauseMenu::GetCurrentActivePanelData().MenuScreen, true);
		CPauseMenu::RedrawInstructionalButtons();
		return true;
	}
#endif
	if( u32(MenuId.GetValue()) == ATSTRINGHASH("LINK_FACEBOOK",0xbb2566c4) )
	{
		bool isBusy = RL_FACEBOOK_SWITCH(CLiveManager::GetFacebookMgr().IsBusy(), false);
		if(!m_bInsideSystemUI && !isBusy)
		{
			m_bInsideSystemUI = true;
			RL_FACEBOOK_ONLY(CLiveManager::GetFacebookMgr().GetAccessToken());
		}
		return true;
	}
	else if( MenuId == MENU_UNIQUE_ID_SETTINGS_LIST )
	{
		// if this isn't a special item
		if( !CPauseMenu::HandlePrefOverride(static_cast<eMenuPref>(iUniqueId), 1 ) )
		{
			// treat it as though we've just hit DPAD_RIGHT on it to cycle to the next option
			CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod( "SET_INPUT_EVENT", PAD_DPADRIGHT );
		}
		SUIContexts::Activate(ATSTRINGHASH("HACK_SKIP_SELECT_SOUND",0x9fb52674));
	}
	else if( MenuId != MENU_UNIQUE_ID_SETTINGS )
	{
#if __PS3
		if( MenuId == MENU_UNIQUE_ID_SETTINGS_AUDIO )
		{
			COMMERCE_CONTAINER_ONLY(PreparePRX(true));
			audNorthAudioEngine::CheckForWirelessHeadset();
		}

#endif

		if( static_cast<s32>(CPauseMenu::GetLastValidPref()) >= ARBITRARY_OFFSET_TO_FAKE_HISTORY)
		{
			CPauseMenu::SetMenuPrefSelected( eMenuPref(CPauseMenu::GetLastValidPref() - ARBITRARY_OFFSET_TO_FAKE_HISTORY) );
			CPauseMenu::SetLastValidPref(PREF_INVALID);
		}
		// first time entering a menu, set this up.
		else if( (CPauseMenu::GetMenuPrefSelected() == PREF_INVALID && !m_bHaveNavigatedToTheRightMenu) ||
			CPauseMenu::GetMenuPrefSelected() - ARBITRARY_OFFSET_TO_FAKE_HISTORY < -1)
		{
			// find the index of the item we just left
			int iIndex = m_Owner.FindOnscreenItemIndex(iUniqueId);

			if( uiVerifyf(iIndex != -1, "Somehow found an invalid menu index for MenuId %s, UniqueID %i. This is handled, but is totally weird.", MenuId.GetParserName(), iUniqueId ) )
			{
				// now find the proper child we're entering, and get its menu pref
				const MenuScreenId& linkedMenu = m_Owner.MenuItems[iIndex].MenuUniqueId;
				const CMenuScreen& linkedScreen = CPauseMenu::GetScreenData(linkedMenu);

				int iLinkedIndex = linkedScreen.FindOnscreenItemIndex(0, MENU_OPTION_ACTION_PREF_CHANGE);
				if( iLinkedIndex != -1 )
					CPauseMenu::SetMenuPrefSelected( eMenuPref(linkedScreen.MenuItems[iLinkedIndex].MenuPref) );
			}
		}
		else
		{
			CPauseMenu::SetMenuPrefSelected( eMenuPref(CPauseMenu::GetMenuPrefSelected() - ARBITRARY_OFFSET_TO_FAKE_HISTORY) );
		}

		if(m_bOnFacebookTab )
		{
			SUIContexts::SetActive("SETTINGS_PrefSwitch", false );
			SUIContexts::SetActive(HIDE_ACCEPTBUTTON,  !IsInStateWithAButton() );
		}
		else
		{
			SUIContexts::SetActive("SETTINGS_PrefSwitch", IsPrefControledBySlider(CPauseMenu::GetMenuPrefSelected())/*(CPauseMenu::GetMenuPrefSelected() + ARBITRARY_OFFSET_TO_FAKE_HISTORY != PREF_INVALID*/);
			SUIContexts::SetActive(HIDE_ACCEPTBUTTON,  IsPrefControledBySlider(CPauseMenu::GetMenuPrefSelected()));
		}
		CPauseMenu::RedrawInstructionalButtons(iUniqueId);

		m_bHaveNavigatedToTheRightMenu = true;
		m_bInSettingsColumn = true;
	}

	return false;
}

bool CSettingsMenu::IsInStateWithAButton() const
{
	if( !m_bOnFacebookTab )
		return true;

	switch( m_eState )
	{
		case SS_FacebookDown:
		case SS_NoPermission:
		case SS_Banned:
		case SS_Busy:
			return false;

		default:
			return true;
	}
}

bool CSettingsMenu::UpdateInput(s32 eInput)
{
	if( m_bOnFacebookTab && CPauseMenu::IsNavigatingContent() )
	{
		if( eInput == PAD_CROSS )
		{
			switch( m_eState )
			{
			// block progression for these three menu states
			case SS_OldPolicy:
			case SS_NoSocialClub:
				CPauseMenu::EnterSocialClub();
				return true;

			case SS_Offline:
				CLiveManager::ShowSigninUi();
				return true;

			case SS_FacebookDown:
			case SS_Busy:
				return true;

			default:
				break;
			}
		}

		// to prevent general weirdness, just don't allow you to navigate up/down in the facebook menu
		else if (m_bInSettingsColumn && (eInput == PAD_DPADUP || eInput == PAD_DPADDOWN))
		{
			return true;
		}
	}

#if RSG_PC
	MenuScreenId activePanel = CPauseMenu::GetCurrentActivePanel();

	if(eInput == PAD_SQUARE)
	{
		if((activePanel == MENU_UNIQUE_ID_SETTINGS_GRAPHICS || activePanel == MENU_UNIQUE_ID_SETTINGS_ADVANCED_GFX) && SUIContexts::IsActive("GFX_Dirty"))
		{
			if(!m_bHasShownOnlineGfxWarning && CPauseMenu::IsMP())
			{
				m_bHasShownOnlineGfxWarning = true;
				m_bDisplayOnlineWarning = true;
			}
			else
			{
				ApplyGfxSettings();
				CPauseMenu::PlaySound("SELECT");
			}
		}
	}

#if COLLECT_END_USER_BENCHMARKS
	if(eInput == PAD_TRIANGLE)
	{
		if( (activePanel == MENU_UNIQUE_ID_SETTINGS_GRAPHICS || activePanel == MENU_UNIQUE_ID_SETTINGS_ADVANCED_GFX) &&
			(CPauseMenu::IsSP() || SLegacyLandingScreen::IsInstantiated()) && SUIContexts::IsActive("NOT_ON_A_MISSION"))
		{
			CPauseMenu::PlaySound("SELECT");

			if(!SLegacyLandingScreen::IsInstantiated())
			{
				// No confirmation needed here, forge ahead
				OnConfirmBenchmarTests();
				StartBenchmarkScript();
			}
			else
			{
				// Confirmation requested on landing page -- url:bugstar:2314710
				CWarningMessage& rMessage = CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage();

				CWarningMessage::Data messageData;
				messageData.m_TextLabelBody = "WARN_BENCHMARK_QUIT";
				messageData.m_iFlags = FE_WARNING_YES_NO;
				messageData.m_bCloseAfterPress = true;
				messageData.m_acceptPressed = datCallback(MFA(CSettingsMenu::OnConfirmBenchmarTests),this);

				rMessage.SetMessage(messageData);
			}

		}
	}
#endif // COLLECT_END_USER_BENCHMARKS
#endif // RSG_PC

	return false;
}

#if COLLECT_END_USER_BENCHMARKS
void CSettingsMenu::OnConfirmBenchmarTests()
{
	EndUserBenchmarks::SetWasStartedFromPauseMenu(CPauseMenu::GetCurrentMenuVersion());
	CPauseMenu::Close();
}
#endif

bool CSettingsMenu::Populate(MenuScreenId newScreenId)
{
	if( newScreenId == MENU_UNIQUE_ID_SETTINGS )
	{
#if RL_FACEBOOK_ENABLED
		SUIContexts::SetActive("FB_Disabled", rlFacebook::IsKillSwitchEnabled());
#else
		SUIContexts::SetActive("FB_Disabled", true);
#endif

		return true;
	}
	return false;
}

void CSettingsMenu::LayoutChanged( MenuScreenId iPreviousLayout, MenuScreenId iNewLayout, s32 iUniqueId, eLAYOUT_CHANGED_DIR WIN32PC_ONLY( eDir ) )
{
// 	SUIContexts::SetActive("SETTINGS_PrefSwitch", iNewLayout == MENU_UNIQUE_ID_SETTINGS_LIST 
// 		|| iNewLayout == MENU_UNIQUE_ID_BRIGHTNESS_CALIBRATION);

	bool bOnControllerTab = false;

#if __XENON
#if __BANK
	if (CProfileSettings::GetUpdateProfileWhenNavigatingSettingsMenu())
#endif	//	__BANK
	{
		CPauseMenu::UpdateProfileFromMenuOptions();
	}
#endif	//	__XENON

#if RSG_PC
	bool bOnGfxScreen = false;
	bool bOnVoiceScreen = false;

	if(iNewLayout == MENU_UNIQUE_ID_SETTINGS_GRAPHICS || iNewLayout == MENU_UNIQUE_ID_SETTINGS_ADVANCED_GFX)
	{
		bOnGfxScreen = true;
		if(CPauseMenu::DirtyGfxSettings() || CPauseMenu::DirtyAdvGfxSettings())
		{
			// If our context is active, confirm settings change
			if(SUIContexts::IsActive("GFX_Dirty"))
			{
				CPauseMenu::SetBackedWithGraphicChanges(true);
			}
			else
			{
				// The graphics settings have changed without any user input, so just refresh the menu
				CPauseMenu::GenerateMenuData(CPauseMenu::GetCurrentActivePanel(), true);
			}
		}
	}

	if(iNewLayout == MENU_UNIQUE_ID_SETTINGS_VOICE_CHAT)
	{
		bOnVoiceScreen = true;

		// This ensures that everything gets enabled/disabled correctly
		// every time we first open the screen.
		CPauseMenu::SetValueBasedOnPreference(PREF_VOICE_ENABLE, CPauseMenu::UPDATE_PREFS_FROM_MENU);

		// Display an alert on the Voice Chat settings screen that chat output is not being transmitted.
		if((eDir != LAYOUT_CHANGED_DIR_BACKWARDS) &&
			CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).BeginMethod("SET_VIDEO_MEMORY_BAR"))
		{
			CScaleformMgr::AddParamBool(true);
			CScaleformMgr::AddParamString(TheText.Get("MO_VOICE_ALERT"));
			CScaleformMgr::AddParamInt(-1);
			CScaleformMgr::AddParamInt(0);
			CScaleformMgr::EndMethod();
		}
	}

	MenuScreenId activePanel = CPauseMenu::GetCurrentActivePanel();
	if(!bOnGfxScreen && iPreviousLayout != MENU_UNIQUE_ID_SETTINGS_GRAPHICS && iPreviousLayout != MENU_UNIQUE_ID_SETTINGS_ADVANCED_GFX)
		bOnGfxScreen = (activePanel == MENU_UNIQUE_ID_SETTINGS_GRAPHICS || activePanel == MENU_UNIQUE_ID_SETTINGS_ADVANCED_GFX);
	if(!bOnVoiceScreen && iPreviousLayout != MENU_UNIQUE_ID_SETTINGS_VOICE_CHAT)
		bOnVoiceScreen = activePanel == MENU_UNIQUE_ID_SETTINGS_VOICE_CHAT;

	if( iNewLayout == MENU_UNIQUE_ID_SETTINGS_GRAPHICS || iNewLayout == MENU_UNIQUE_ID_SETTINGS_ADVANCED_GFX)
	{
		if( iPreviousLayout != MENU_UNIQUE_ID_SETTINGS_LIST && 
			CPauseMenu::GetMenuPrefSelected() != PREF_RESTORE_DEFAULTS)
		{
			CPauseMenu::UpdateMemoryBar(true);
		}
	}

	SUIContexts::SetActive("GFX_Dirty", (CPauseMenu::DirtyGfxSettings()|| CPauseMenu::DirtyAdvGfxSettings()) && bOnGfxScreen);

	SUIContexts::SetActive("ON_VID_GFX_MENU", bOnGfxScreen);
	SUIContexts::SetActive("NOT_ON_A_MISSION", (!CTheScripts::GetPlayerIsOnAMission() && !CTheScripts::GetPlayerIsInAnimalForm()));
	m_bOnVoiceTab = bOnVoiceScreen;
#endif // RSG_PC

	if( iNewLayout == MENU_UNIQUE_ID_SETTINGS_LIST )
	{
		CPauseMenu::SetMenuPrefSelected( eMenuPref(iUniqueId) );
		SUIContexts::SetActive(HIDE_ACCEPTBUTTON,  IsPrefControledBySlider(eMenuPref(iUniqueId)));
	}
	else if(iNewLayout == MENU_UNIQUE_ID_BRIGHTNESS_CALIBRATION)
	{
		CPauseMenu::SetMenuPrefSelected( PREF_GAMMA );
		SUIContexts::SetActive(HIDE_ACCEPTBUTTON,  false);
	}
	else if( (u32)iNewLayout.GetValue() == ATSTRINGHASH("RESTORE_DEFAULTS_AUDIO",0xC8D6A761) ||
		(u32)iNewLayout.GetValue() == ATSTRINGHASH("RESTORE_DEFAULTS_DISPLAY",0x5BAEBC8B) ||
		(u32)iNewLayout.GetValue() ==  ATSTRINGHASH("RESTORE_DEFAULTS_CONTROLS",0x56CA350) ||
		(u32)iNewLayout.GetValue() ==  ATSTRINGHASH("RESTORE_DEFAULTS_VOICE_CHAT", 0xF93B9677) ||
		(u32)iNewLayout.GetValue() ==  ATSTRINGHASH("RESTORE_DEFAULTS_GRAPHICS",0xC66511CE) ||
		(u32)iNewLayout.GetValue() ==  ATSTRINGHASH("RESTORE_DEFAULTS_FIRST_PERSON", 0x4809CCD7) ||
		(u32)iNewLayout.GetValue() ==  ATSTRINGHASH("RESTORE_DEFAULTS_MISC_CONTROLS", 0x60E04BA1) ||
		(u32)iNewLayout.GetValue() ==  ATSTRINGHASH("RESTORE_DEFAULTS_ADVANCED_GFX", 0x335F04C8) ||
		(u32)iNewLayout.GetValue() ==  ATSTRINGHASH("RESTORE_DEFAULTS_CAMERA", 0xA8E5BCD1))
	{
		CPauseMenu::SetMenuPrefSelected( PREF_RESTORE_DEFAULTS );
		SUIContexts::SetActive(HIDE_ACCEPTBUTTON,  IsPrefControledBySlider(CPauseMenu::GetMenuPrefSelected()));
	}
#if RSG_ORBIS
	else if ( (u32)iNewLayout.GetValue() ==  ATSTRINGHASH("PREF_SAFEZONE_SIZE", 0xc9fbb259) )
	{
		CPauseMenu::SetMenuPrefSelected( PREF_SAFEZONE_SIZE );
		SUIContexts::SetActive(HIDE_ACCEPTBUTTON,  false);
	}
#endif // RSG_ORBIS
#if RSG_PC
	else if ( (u32)iNewLayout.GetValue() ==  ATSTRINGHASH("PREF_PCGAMEPAD", 0xf9aa1b46) )
	{
		CPauseMenu::SetMenuPrefSelected( PREF_PCGAMEPAD );
		SUIContexts::SetActive(HIDE_ACCEPTBUTTON,  false);
	}
#endif
	// if backing out to any menu on the primary screen
	else if( iPreviousLayout == MENU_UNIQUE_ID_SETTINGS_LIST ||
		iPreviousLayout == MENU_UNIQUE_ID_BRIGHTNESS_CALIBRATION)
	{
		CPauseMenu::SetMenuPrefSelected( eMenuPref(CPauseMenu::GetMenuPrefSelected() + ARBITRARY_OFFSET_TO_FAKE_HISTORY) );
		SUIContexts::SetActive(HIDE_ACCEPTBUTTON,   IsPrefControledBySlider(CPauseMenu::GetMenuPrefSelected()));
		m_bInSettingsColumn = false;

#if __PS3
		COMMERCE_CONTAINER_ONLY(PreparePRX(false));
#endif
	}
	else
	{
		// If we're going between side bar menus we have to clear the
		// previously valid value.
		if(IsSideBarLayout(iNewLayout) && IsSideBarLayout(iPreviousLayout))
		{
			CPauseMenu::SetLastValidPref(PREF_INVALID);
		}

		CPauseMenu::SetMenuPrefSelected(PREF_INVALID);
		SUIContexts::SetActive(HIDE_ACCEPTBUTTON,  false);
		m_bInSettingsColumn = false;
	}

	// due to the way scaleform works, we need to rejigger our visibility states on layout changed
	m_columnVisibility.Set(PM_COLUMN_LEFT, true);
	m_columnVisibility.Set(PM_COLUMN_MIDDLE, true);

#if RSG_PC
	if( iNewLayout == MENU_UNIQUE_ID_INCEPT_TRIGGER && iUniqueId == MENU_UNIQUE_ID_KEYMAP )
	{
		ShowColumnWarning_EX(PM_COLUMN_MIDDLE_RIGHT, "MAPPING_HINT", "PM_PANE_KEYS");
	}
	else if ( eDir != LAYOUT_CHANGED_DIR_BACKWARDS )
	{
		ClearWarningColumn_EX(PM_COLUMN_MIDDLE_RIGHT);
	}
#endif

	// Handle necessary logic when switching on/off the facebook tab
	if(iNewLayout == MENU_UNIQUE_ID_SETTINGS_FACEBOOK)
	{
		m_bOnFacebookTab = true;
		if( iPreviousLayout != MENU_UNIQUE_ID_SETTINGS_LIST 
			&& static_cast<u32>(iPreviousLayout.GetValue()) != ATSTRINGHASH("LINK_FACEBOOK",0xbb2566c4) )
		{
			ChangeState(SS_OffPage);
			SUIContexts::Deactivate("FB_NeedsSignUp");
			SUIContexts::Deactivate("FB_Ready");
			SUIContexts::Activate("IS_FACEBOOK");
			Update(); // Force an update to handle the SS logic immediately
		}
	}
	else if( m_bOnFacebookTab && iNewLayout != MENU_UNIQUE_ID_SETTINGS) // and not backing out to the header
	{
		ChangeState(SS_OffPage);
		m_bOnFacebookTab = false;
		SUIContexts::Deactivate("FB_NeedsSignUp");
		SUIContexts::Deactivate("FB_Ready");
		SUIContexts::Deactivate("IS_FACEBOOK");
		ClearWarningColumn_EX(PM_COLUMN_MIDDLE_RIGHT);
	}

	if(iNewLayout == MENU_UNIQUE_ID_SETTINGS_CONTROLS)
	{
		int iPrefValue = CPauseMenu::GetMenuPreference(PREF_CONTROLS_CONTEXT);
		u32 iModePref = iPrefValue < 3 ? PREF_CONTROL_CONFIG : PREF_CONTROL_CONFIG_FPS;
		CControllerLabelMgr::SetLabelScheme(CPauseMenu::GetControllerContext(), CPauseMenu::GetMenuPreference(iModePref));
		PopulateControllerLabels(GetControllerImageHeader());
		CControllerLabelMgr::SetLabelsChanged(false);
		bOnControllerTab = true;
	}
	
	if(!bOnControllerTab)
	{
		bOnControllerTab = IsOnControllerPref(CPauseMenu::GetMenuPrefSelected()) || (u32)iNewLayout.GetValue() == ATSTRINGHASH("PREF_PCGAMEPAD", 0xF9AA1B46);
	}

	m_bOnControllerTab = bOnControllerTab;

	SUIContexts::SetActive("SETTINGS_PrefSwitch", IsPrefControledBySlider(CPauseMenu::GetMenuPrefSelected()));
	CPauseMenu::RedrawInstructionalButtons(iUniqueId);
}

bool CSettingsMenu::IsOnControllerPref(eMenuPref currentPref)
{
	switch(currentPref)
	{
	case PREF_CONTROLS_CONTEXT:
	case PREF_TARGET_CONFIG:
	case PREF_VIBRATION:
	case PREF_INVERT_LOOK:
	case PREF_CONTROL_CONFIG:
	case PREF_LOOK_AROUND_SENSITIVITY:
	case PREF_CONTROLLER_SENSITIVITY:
	case PREF_CONTROL_CONFIG_FPS:
	case PREF_FPS_LOOK_SENSITIVITY:
	case PREF_FPS_AIM_SENSITIVITY:
	case PREF_FPS_AIM_DEADZONE:
	case PREF_FPS_AIM_ACCELERATION:
	case PREF_AIM_DEADZONE:
	case PREF_AIM_ACCELERATION:
	case PREF_SNIPER_ZOOM:
	case PREF_ALTERNATE_HANDBRAKE:
	case PREF_ALTERNATE_DRIVEBY:
	case PREF_CONTROLLER_LIGHT_EFFECT:
		return true;
	default:
		return false;
	}
}

bool CSettingsMenu::IsSideBarLayout(MenuScreenId screenID)
{
	return m_Owner.FindItemIndex(screenID) != -1;
}

bool CSettingsMenu::IsPrefControledBySlider(eMenuPref ePref)
{
	if( ePref == PREF_INVALID )
		return false;

	CMenuScreen& curData = CPauseMenu::GetCurrentActivePanelData();
	int iIndex = curData.FindItemIndex(ePref);
	if(iIndex != -1 )
	{
		return curData.MenuItems[iIndex].MenuOption == MENU_OPTION_SLIDER;
	}
	return false;
}


char* CSettingsMenu::GetControllerImageHeader()
{
	char* gxtHeader = 0;
	u32 uCurrentContext = CPauseMenu::GetMenuPreference(PREF_CONTROLS_CONTEXT);

	switch (uCurrentContext)
	{
		// On foot
		case 0:
		case 3:
			gxtHeader = TheText.Get("MO_FOT");
			break;
		// In vehicle
		case 1:
		case 4:
			gxtHeader = TheText.Get("MO_VEH");
			break;
		// In aircarft
		case 2:
		case 5:
			gxtHeader = TheText.Get("MO_AIRCRAFT");
			break;
		// mission creator
		case 6:
			gxtHeader = TheText.Get("MO_ONLINECREATOR");
			break;
		default:
			gxtHeader = TheText.Get("MO_FOT");
			break;
	}

	return gxtHeader;
}

void CSettingsMenu::PopulateControllerLabels(char* controllerHeader)
{
	CScaleformMovieWrapper& controllerImage = CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT);
	if( controllerImage.BeginMethod("SET_CONTROL_LABELS") )
	{
		for(int i = 0; i < CControllerLabelMgr::GetCurrentLabels().GetCount(); ++i)
		{
			controllerImage.AddParamString(TheText.Get(CControllerLabelMgr::GetCurrentLabels()[i].m_LabelHash, "CONTROLLER_LABEL"));
		}

		// Header
		controllerImage.AddParamString(controllerHeader);
		controllerImage.EndMethod();
	}
}

#if RSG_PC
void CSettingsMenu::WriteGfxSettings()
{
	Settings settings = CPauseMenu::GetMenuGraphicsSettings(true);
	m_bWriteFailed = !CSettingsManager::GetInstance().Save(settings);
}

void CSettingsMenu::ApplyGfxSettings()
{
	if(CPauseMenu::GetCanApplySettings())
	{
		Settings gfxSettings = CPauseMenu::GetMenuGraphicsSettings();

		if( CSettingsManager::GetInstance().DoNewSettingsRequireRestart(gfxSettings))
		{
			m_bDisplayRestartWarning = true;
			CPauseMenu::GenerateMenuData(CPauseMenu::GetCurrentActivePanel(), true);
			CPauseMenu::RedrawInstructionalButtons();
		}
		else
		{
			CSettingsManager::GetInstance().RequestNewSettings(gfxSettings);
			if(CPauseMenu::GfxScreenNeedCountDownWarning())
			{
				m_uGfxApplyTimestamp = sysTimer::GetSystemMsTime();
			}
			else
			{
				WriteGfxSettings();
				SUIContexts::Deactivate("GFX_Dirty");
				CPauseMenu::GenerateMenuData(CPauseMenu::GetCurrentActivePanel(), true);
				CPauseMenu::RedrawInstructionalButtons();
			}
		}
	}
	else
	{
		m_bFailedApply = true;
	}
}

void CSettingsMenu::StartBenchmarkScript()
{
	scrThreadId debugScriptID = THREAD_INVALID;
	const strLocalIndex scriptID = g_StreamedScripts.FindSlot("benchmark");
	if(gnetVerifyf(scriptID != -1, "Graphics benchmarking script does not exist"))
	{
		g_StreamedScripts.StreamingRequest(scriptID, STRFLAG_PRIORITY_LOAD|STRFLAG_MISSION_REQUIRED);
		CStreaming::LoadAllRequestedObjects();

		if(gnetVerifyf(g_StreamedScripts.HasObjectLoaded(scriptID), "Failed to stream in benchmarking script"))
		{
			debugScriptID = CTheScripts::GtaStartNewThreadOverride("benchmark", NULL, 0, GtaThread::GetDefaultStackSize());
			if(gnetVerifyf(debugScriptID != 0, "Failed to start benchmarking script"))
			{
				gnetDebug1("Started running train debug script");
			}
		}
	}
}

void CSettingsMenu::HandlePCWarningScreens()
{
	// A restart is required for the changes to fully take effect
	if(m_bDisplayRestartWarning)
	{
		CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD,
			ATSTRINGHASH("GFX_RESTART_HEADER", 0x41318EFB),
			ATSTRINGHASH("GFX_RESTART_BODY", 0x25DE0ACD),
			FE_WARNING_YES_NO,
			false,
			0,
			NULL,
			NULL,
			true);

		if (CWarningScreen::CheckAllInput() == FE_WARNING_YES)
		{
			CPauseMenu::UpdateProfileFromMenuOptions(false);
			WriteGfxSettings();
			CPauseMenu::sm_bWantsToRestartGame = !m_bWriteFailed;
			m_bDisplayRestartWarning = false;
		}

		if (CWarningScreen::CheckAllInput() == FE_WARNING_NO)
		{
			CPauseMenu::BackoutGraphicalChanges(false, true);
			CPauseMenu::SetBackedWithGraphicChanges(false);
			m_bDisplayRestartWarning = false;
		}
	}

	// User Applied Resolution Changes.
	if(m_uGfxApplyTimestamp != 0)
	{
		CMenuScreen& activePanel = CPauseMenu::GetCurrentActivePanelData();
		const int GFX_COUNTDOWN_TIME = 15;
		int restoreSeconds = GFX_COUNTDOWN_TIME - ((sysTimer::GetSystemMsTime() - m_uGfxApplyTimestamp) / 1000);

		if(restoreSeconds < 0)
		{
			CPauseMenu::BackoutGraphicalChanges();
			m_uGfxApplyTimestamp = 0;
			CPauseMenu::SetBackedWithGraphicChanges(false);
			m_bDisplayRestartWarning = false;
		}
		else
		{
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD,
				ATSTRINGHASH("GFX_WARNING_HEADER", 0x27C36328),
				ATSTRINGHASH("GFX_WARNING_BODY", 0xE3A55663),
				FE_WARNING_YES_NO,
				true,
				restoreSeconds,
				NULL,
				NULL,
				true);

			if (CWarningScreen::CheckAllInput() == FE_WARNING_YES)
			{
				WriteGfxSettings();
				m_uGfxApplyTimestamp = 0;
				SUIContexts::SetActive("GFX_Dirty", CPauseMenu::DirtyGfxSettings() || CPauseMenu::DirtyAdvGfxSettings());
				CPauseMenu::GenerateMenuData(activePanel.MenuScreen, true);
				CPauseMenu::RedrawInstructionalButtons();
				CPauseMenu::SetBackedWithGraphicChanges(false);
			}
			else if (CWarningScreen::CheckAllInput() == FE_WARNING_NO)
			{
				CPauseMenu::BackoutGraphicalChanges();
				m_uGfxApplyTimestamp = 0;
				CPauseMenu::SetBackedWithGraphicChanges(false);
				m_bDisplayRestartWarning = false;
			}
		}
	}

	if(m_bWriteFailed)
	{
		CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD,
			ATSTRINGHASH("CELL_CAM_ALERT", 0x413B4109),
			ATSTRINGHASH("GFX_WRITE_FAILED", 0xA57AF0C2),
			FE_WARNING_OK,
			false,
			0,
			NULL,
			NULL,
			true);

		if (CWarningScreen::CheckAllInput() == FE_WARNING_OK)
		{
			m_bWriteFailed = false;
		}
	}

	// Attempted to apply changes beyond the memory limits of the graphics card.
	if(m_bFailedApply)
	{
		CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD,
			ATSTRINGHASH("CELL_CAM_ALERT", 0x413B4109),
			ATSTRINGHASH("GFX_NO_APPLY_BODY", 0xCC5B6C80),
			FE_WARNING_OK,
			false,
			0,
			NULL,
			NULL,
			true);

		if (CWarningScreen::CheckAllInput() == FE_WARNING_OK)
		{
			m_bFailedApply = false;

			if(CPauseMenu::GetBackedWithGraphicChanges())
			{
				CPauseMenu::BackoutGraphicalChanges(false, true);
				CPauseMenu::SetBackedWithGraphicChanges(false);
				m_bDisplayRestartWarning = false;
			}
		}
	}

	// User backed out of the menu without applying changes
	if(CPauseMenu::GetBackedWithGraphicChanges() && m_uGfxApplyTimestamp == 0)
	{
		CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD,
				ATSTRINGHASH("CELL_CAM_ALERT", 0x413B4109),
				ATSTRINGHASH("GFX_BACK_OUT_BODY", 0x76CDEF83),
				FE_WARNING_YES_NO_CANCEL,
				false,
				0,
				NULL,
				NULL,
				true);

			if (CWarningScreen::CheckAllInput() == FE_WARNING_YES)
			{
				ApplyGfxSettings();
				CPauseMenu::SetBackedWithGraphicChanges(false);
			}
			else if (CWarningScreen::CheckAllInput() == FE_WARNING_NO_ON_X)
			{
				CPauseMenu::BackoutGraphicalChanges(false, true);
				CPauseMenu::SetBackedWithGraphicChanges(false);
				m_bDisplayRestartWarning = false;
			}
			else if (CWarningScreen::CheckAllInput() == FE_WARNING_CANCEL_ON_B)
			{
				CPauseMenu::MENU_SHIFT_DEPTH(kMENUCEPT_DEEPER);
				CPauseMenu::SetBackedWithGraphicChanges(false);
				m_bDisplayRestartWarning = false;
			}
	}

	// Show a warning if the user is trying to apply changes while online.
	if(m_bDisplayOnlineWarning)
	{
		CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD,
			ATSTRINGHASH("CELL_CAM_ALERT", 0x413B4109),
			ATSTRINGHASH("GFX_WARNING_MP", 0x2636DDA8),
			FE_WARNING_YES_NO,
			false,
			0,
			NULL,
			NULL,
			true);

		if (CWarningScreen::CheckAllInput() == FE_WARNING_YES)
		{
			ApplyGfxSettings();
			m_bDisplayOnlineWarning = false;
		}
		else if (CWarningScreen::CheckAllInput() == FE_WARNING_NO)
		{
			m_bDisplayOnlineWarning = false;
		}
	}
}
#endif // RSG_PC

#if __BANK
bool CSettingsMenu::CheckStatus(bool bOriginalValue, int iCheckStatus) const
{
	if( m_iFakedStatus == SS_OffPage )
		return bOriginalValue;

	return m_iFakedStatus == iCheckStatus;
}

void CSettingsMenu::AddWidgets(bkBank* ToThisBank)
{
	m_pDebugWidget = ToThisBank->AddGroup("Settings Menu");
	const char* pszDebugReasons[] = {
		"SS_OffPage",
		"SS_Offline",
		"SS_FacebookDown",
		"SS_NoSocialClub",
		"SS_OldPolicy",
		"SS_NoFacebook",
		"SS_TotallyFine",
		"SS_NoPermission",
		"SS_Banned",
		"SS_Busy"
	};

	m_pDebugWidget->AddCombo("Fake Reasons", &m_iFakedStatus, COUNTOF(pszDebugReasons), pszDebugReasons);
}

#endif


