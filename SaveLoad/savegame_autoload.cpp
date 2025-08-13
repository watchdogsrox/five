
#include "rline/rlpresence.h"

#include "camera/CamInterface.h"
#include "control/gamelogic.h"
#include "frontend/PauseMenu.h"	//	for CPauseMenu::SetMenuPreference
#include "frontend/ProfileSettings.h"
#include "Frontend/loadingscreens.h"
#include "frontend/WarningScreen.h"
#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/savegame_autoload.h"
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/savegame_damaged_check.h"
#include "SaveLoad/savegame_filenames.h"
#include "SaveLoad/savegame_load.h"
#include "SaveLoad/savegame_users_and_devices.h"
#include "Network/Live/NetworkTelemetry.h"
#include "system/controlMgr.h"
#include "text/messages.h"
#include "text/TextConversion.h"

#if RSG_DURANGO
#include "rline/durango/rlxbl_interface.h"
#endif

#if __WIN32PC
XPARAM(showProfileBypass);
#endif // __WIN32PC

SAVEGAME_OPTIMISATIONS()

#if __XENON
//	Allow for 4 storage devices on the XBox360
//	The only time that all devices should be enumerated is when the game is first started
//	to find the most recent save
//	A bit lazy but allow extra slots so that SortSaveGameSlots has some free slots to work with
#define MAX_NUM_SAVE_FILES_TO_ENUMERATE_FOR_AUTOLOAD	(MAX_NUM_EXPECTED_SAVE_FILES * 5)
#else
// PS3 *and* PC.
//	A bit lazy but allow extra slots so that SortSaveGameSlots has some free slots to work with
#define MAX_NUM_SAVE_FILES_TO_ENUMERATE_FOR_AUTOLOAD	(MAX_NUM_EXPECTED_SAVE_FILES * 2)
#endif


CSavegameAutoload::eAutoLoadSignInState CSavegameAutoload::ms_AutoLoadSignInState = CSavegameAutoload::AUTOLOAD_STATE_CHECK_IF_PLAYER_IS_SIGNED_IN;

#if !__PPU || !__QUICK_SCAN_FOR_AUTOLOAD_AND_SLOW_SCAN_ON_ANOTHER_THREAD
CSavegameAutoload::eAutoLoadFileEnumerationState CSavegameAutoload::ms_AutoLoadFileEnumerationState = CSavegameAutoload::AUTOLOAD_STATE_BEGIN_ENUMERATE_ALL_DEVICES;
#endif

CSavegameAutoload::eAutoLoadInviteState CSavegameAutoload::ms_AutoLoadInviteState = CSavegameAutoload::AUTOLOAD_STATE_BEGIN_CHECK_FOR_INVITE;

CSavegameAutoload::eAutoLoadDecisionState CSavegameAutoload::ms_AutoLoadDecisionState = CSavegameAutoload::AUTOLOAD_STATE_DECIDE_ON_COURSE_OF_ACTION;

fiSaveGame::Content *CSavegameAutoload::ms_pSaveGameSlotsForAutoload;
s32		CSavegameAutoload::ms_NumberOfSaveGameSlotsForAutoload = 0;


int CSavegameAutoload::ms_FirstCharacterOfAutoloadName = 0;
int CSavegameAutoload::ms_LengthOfAutoloadName = 0;

int CSavegameAutoload::ms_AutoLoadEpisodeForDiscBuild = -1;

bool CSavegameAutoload::ms_bEpisodicMsgAlreadySeen = false;

bool CSavegameAutoload::ms_bPerformingAutoLoadAtStartOfGame = true;

#if __PPU
bool CSavegameAutoload::ms_bAutoloadEnumerationHasCompleted = false;
#endif	//	__PPU

bool CSavegameAutoload::ms_InviteConfirmationBlockedDuringAutoload = true;

CDate CSavegameAutoload::ms_MostRecentSaveDate;
int CSavegameAutoload::ms_MostRecentSaveSlot = -1;

int CSavegameAutoload::ms_EpisodeNumberForSaveToBeLoaded = INDEX_OF_FIRST_LEVEL;


//	Could all this be moved inside the INIT_CORE block of CSavegameAutoload::Init
void CSavegameAutoload::InitialiseAutoLoadAtStartOfGame()
{
#if __PPU
		SAVEGAME.SetMaxNumberOfSaveGameFilesToEnumerate(MAX_NUM_SAVE_FILES_TO_ENUMERATE_FOR_AUTOLOAD);
#endif

	ClearMostRecentSaveData();

	ms_AutoLoadEpisodeForDiscBuild = -1;

	//	Make sure this function is only called once in game::Init
	ms_EpisodeNumberForSaveToBeLoaded = CGameLogic::GetRequestedLevelIndex();
	ms_FirstCharacterOfAutoloadName = 0;
	ms_LengthOfAutoloadName = 0;

	CSavegameDialogScreens::SetDialogBoxHeading("SG_HDNG");	//	 SG_HDG_AUTO_LD");

//	PauseIfNecessary();
//	Might need to open frontend here
//	CFrontEnd::RequestState(FRONTEND_STATE_OPEN_AUTOLOAD);
}

void CSavegameAutoload::Init(unsigned initMode)
{
    if(initMode == INIT_CORE)
    {
	    ms_bPerformingAutoLoadAtStartOfGame = true;

#if __PPU
		ms_bAutoloadEnumerationHasCompleted = false;
#endif	//	__PPU

	    ms_InviteConfirmationBlockedDuringAutoload = true;
    }
    else if(initMode == INIT_AFTER_MAP_LOADED)
    {
		ms_AutoLoadEpisodeForDiscBuild = -1;

		ms_FirstCharacterOfAutoloadName = 0;
	    ms_LengthOfAutoloadName = 0;
	}
}

bool CSavegameAutoload::ShowLoggedSignInUi()
{
	savegameDisplayf("ShowSigninUi from CSavegameAutoload");
	return CLiveManager::ShowSigninUi();
}

#define WAIT_FOR_LEGALS() \
	if( CLoadingScreens::IsDisplayingLegalScreen() )\
		return AUTOLOAD_SIGN_IN_RETURN_STILL_CHECKING

eAutoLoadSignInReturnValue CSavegameAutoload::AutoLoadCheckForSignedInPlayer()
{
#if RSG_NP
	// On PS4 the online state machine can be a lengthy process on startup, and will fire a signout/signin event. 
	// Simplify this by earlying out in this state to let the lower level signin finish before proceeding.
	// We also don't know the NP environment until NP auth has completed for one player, so our gamer has an unknown environment
	// and is essentially useless. If nobody is signed in, we'll bail here anyway.
	static bool IsPendingOnlineMsg = true;
	if (g_rlNp.IsAnyPendingOnline())
	{
		// We want to log this out once per signin flow. Do our output logging, and then
		//	clear the msg flag until we succesfully get past IsAnyPendingOnline
		if (IsPendingOnlineMsg)
		{
			gnetDebug1("Waiting for pending online user before checking signin state.");
			IsPendingOnlineMsg = false;
		}

		return AUTOLOAD_SIGN_IN_RETURN_STILL_CHECKING;
	}
	IsPendingOnlineMsg = true;
#endif

	bool bReturnSelection = false;
//	static eAutoLoadStatus ReturnAutoLoadStatus = AUTOLOAD_STATUS_STILL_CHECKING;
//	static eAutoLoadSignInReturnValue ReturnAutoLoadSignInStatus = AUTOLOAD_SIGN_IN_RETURN_STILL_CHECKING;

	int number_of_signed_in_profiles = 0;
	int first_signed_in_profile = -1;
	//@@: location CSAVEGAMEAUTOLOAD_AUTOLOADCHECKFORSIGNEDINPLAYER
	if (CControlMgr::GetMainPlayerIndex() == -1)
	{
// Loop through profiles 0 to 3 and check how many are signed in
//	if none are signed in then display the message asking player to sign in
//	if one is signed in then call CControlMgr::SetMainPlayerIndex() for it
//	if more than one is signed in then display the message asking for one pad to press a button

// If no pads are connected display the "Insert a Controller" error message

//	What if pads 0 and 1 are connected but profiles 2 and 3 are signed in?

//	What should happen if the player removes the pad when the sign-in blade is being displayed?

		// count the number of profiles that are signed in
		for (int profile_loop = RL_MAX_LOCAL_GAMERS-1; profile_loop >= 0; profile_loop--)
		{
			// Number of signed in, non-guest profiles
			if (rlPresence::IsSignedIn(profile_loop) 
				DURANGO_ONLY(&& g_rlXbl.IsInitialized())
				&& !rlPresence::IsGuest(profile_loop))
			{
				number_of_signed_in_profiles++;
				first_signed_in_profile = profile_loop;
			}
		}

		if (number_of_signed_in_profiles == 1)
		{
			CControlMgr::SetMainPlayerIndex(first_signed_in_profile);
		}
	}
	else
	{
		number_of_signed_in_profiles = 1;
	}

	{
#if !__DEV && !RSG_PC
		if (CControlMgr::GetNumberOfConnectedPads() == 0)
		{
			WAIT_FOR_LEGALS();

			CSavegameDialogScreens::HandleSaveGameDialogBox(SAVE_GAME_DIALOG_AUTOLOAD_NO_PAD_CONNECTED, 0, &bReturnSelection);
//			CFrontEnd::SetDisplayingReconnectControllerMessage(true);	//	Do I need this here? I don't think anything else is running that needs to check it

			return AUTOLOAD_SIGN_IN_RETURN_STILL_CHECKING;
		}
#endif // !__DEV && !RSG_PC
	}

	switch (ms_AutoLoadSignInState)
	{
		case AUTOLOAD_STATE_CHECK_IF_PLAYER_IS_SIGNED_IN :
			if (CSavegameUsers::GetSignedInUser())
			{
/*
#if __QUICK_SCAN_FOR_AUTOLOAD_AND_SLOW_SCAN_ON_ANOTHER_THREAD
					return AUTOLOAD_STATE_BEGIN_FIND_MOST_RECENT_SAVE_QUICK;
#else
					return AUTOLOAD_STATE_BEGIN_ENUMERATE_ALL_DEVICES;	//	AUTOLOAD_STATE_DECIDE_WHETHER_TO_JOIN_NETWORK_GAME_OR_AUTOLOAD;
#endif
*/
				//	Do I need a separate return value for both cases above
				return AUTOLOAD_SIGN_IN_RETURN_PLAYER_IS_SIGNED_IN;
			}
			else
			{
				if (number_of_signed_in_profiles > 1)
				{
					ms_AutoLoadSignInState = AUTOLOAD_STATE_WAIT_FOR_BUTTON_PRESS_ON_ONE_OF_THE_PADS;
				}
				else
				{
					ms_AutoLoadSignInState = AUTOLOAD_STATE_ASK_IF_PLAYER_WANTS_TO_SIGN_IN;
				}
//				ShowLoggedSignInUi();
//				ms_AutoLoadSignInState = AUTOLOAD_STATE_WAIT_FOR_PLAYER_TO_SIGN_IN;
			}
			break;

		case AUTOLOAD_STATE_ASK_IF_PLAYER_WANTS_TO_SIGN_IN :
			WAIT_FOR_LEGALS();
			//@@: location CSAVEGAMEAUTOLOAD_AUTOLOADCHECKFORSIGNEDINPLAYER_ASK_TO_SIGN_IN
			if (CLiveManager::IsSystemUiShowing())
			{
				ShowLoggedSignInUi();
				ms_AutoLoadSignInState = AUTOLOAD_STATE_WAIT_FOR_PLAYER_TO_SIGN_IN;
			}
			else if (CSavegameUsers::GetSignedInUser())
			{	//	If the player brings up the System UI before starting the game then it can reach here
				ms_AutoLoadSignInState = AUTOLOAD_STATE_WAIT_FOR_PLAYER_TO_SIGN_IN;
			}
//	You are not signed in...
			else if (CSavegameDialogScreens::HandleSaveGameDialogBox(SAVE_GAME_DIALOG_AUTOLOAD_SIGN_IN, 0, &bReturnSelection))
			{
				if (bReturnSelection)
				{
					ms_AutoLoadSignInState = AUTOLOAD_STATE_HIDE_WARNING_SCREEN_AND_SHOW_SYSTEM_UI;
					CWarningScreen::Remove();
				}
				else
				{
//	Seems a bit silly to ask the player if they want to sign in twice in a row
//					ms_AutoLoadSignInState = AUTOLOAD_STATE_DOESNT_WANT_TO_SIGN_IN_TRY_AGAIN;
					ms_AutoLoadSignInState = AUTOLOAD_STATE_DONT_WANT_TO_SIGN_IN_OK;
				}
			}
			break;
		case AUTOLOAD_STATE_HIDE_WARNING_SCREEN_AND_SHOW_SYSTEM_UI:
			ShowLoggedSignInUi();
			ms_AutoLoadSignInState = AUTOLOAD_STATE_WAIT_FOR_PLAYER_TO_SIGN_IN;
			break;
/*
		case AUTOLOAD_STATE_DONT_WANT_TO_SIGN_IN_ARE_YOU_SURE :
			if (HandleSaveGameDialogBox(SAVE_GAME_DIALOG_AUTOLOAD_DONT_SIGN_IN_SURE, 0, &bReturnSelection))
			{
				if (bReturnSelection)
				{
					ms_AutoLoadSignInState = AUTOLOAD_STATE_DONT_WANT_TO_SIGN_IN_OK;
				}
				else
				{
					ShowLoggedSignInUi();
					ms_AutoLoadSignInState = AUTOLOAD_STATE_WAIT_FOR_PLAYER_TO_SIGN_IN;
				}
			}
			break;
*/
		case AUTOLOAD_STATE_DOESNT_WANT_TO_SIGN_IN_TRY_AGAIN :
			WAIT_FOR_LEGALS();

			if (CLiveManager::IsSystemUiShowing())
			{
				ShowLoggedSignInUi();
				ms_AutoLoadSignInState = AUTOLOAD_STATE_WAIT_FOR_PLAYER_TO_SIGN_IN;
			}
			else if (CSavegameUsers::GetSignedInUser())
			{	//	If the player brings up the System UI before starting the game then it can reach here
				ms_AutoLoadSignInState = AUTOLOAD_STATE_WAIT_FOR_PLAYER_TO_SIGN_IN;
			}
			else if (CSavegameDialogScreens::HandleSaveGameDialogBox(SAVE_GAME_DIALOG_AUTOLOAD_NOT_SIGNED_IN_TRY_AGAIN, 0, &bReturnSelection))
			{
				if (bReturnSelection)
				{
					ShowLoggedSignInUi();
					ms_AutoLoadSignInState = AUTOLOAD_STATE_WAIT_FOR_PLAYER_TO_SIGN_IN;
				}
				else
				{
					ms_AutoLoadSignInState = AUTOLOAD_STATE_DONT_WANT_TO_SIGN_IN_OK;	//	AUTOLOAD_STATE_DONT_WANT_TO_SIGN_IN_ARE_YOU_SURE;
				}
			}
			break;

		case AUTOLOAD_STATE_DONT_WANT_TO_SIGN_IN_OK :
			WAIT_FOR_LEGALS();

			if (CSavegameDialogScreens::HandleSaveGameDialogBox(SAVE_GAME_DIALOG_AUTOLOAD_DONT_SIGN_IN_OK, 0, &bReturnSelection))
			{
//				ms_AutoLoadSignInState = AUTOLOAD_STATE_DECIDE_ON_COURSE_OF_ACTION;	//	AUTOLOAD_STATE_START_NEW_GAME;
				return AUTOLOAD_SIGN_IN_RETURN_PLAYER_NOT_SIGNED_IN;
			}
			break;

		case AUTOLOAD_STATE_WAIT_FOR_PLAYER_TO_SIGN_IN :
			if (CLiveManager::IsSystemUiShowing() == false)
			{
				if (CSavegameUsers::GetSignedInUser())
				{
					CSavegameUsers::ClearVariablesForNewlySignedInPlayer();
/*
#if __QUICK_SCAN_FOR_AUTOLOAD_AND_SLOW_SCAN_ON_ANOTHER_THREAD
						ms_AutoLoadSignInState = AUTOLOAD_STATE_BEGIN_FIND_MOST_RECENT_SAVE_QUICK;
#else
						ms_AutoLoadSignInState = AUTOLOAD_STATE_BEGIN_ENUMERATE_ALL_DEVICES;	//	AUTOLOAD_STATE_DECIDE_WHETHER_TO_JOIN_NETWORK_GAME_OR_AUTOLOAD;
#endif
*/
					//	Do I need a separate return value for both cases above
					//@@: location CSAVEGAMEAUTOLOAD_AUTOLOADCHECKFORSIGNEDINPLAYER_SIGNED_IN
					return AUTOLOAD_SIGN_IN_RETURN_PLAYER_IS_SIGNED_IN;
				}
				else
				{
					ms_AutoLoadSignInState = AUTOLOAD_STATE_DOESNT_WANT_TO_SIGN_IN_TRY_AGAIN;
//					ms_AutoLoadSignInState = AUTOLOAD_STATE_START_NEW_GAME;
				}
			}
			break;

		case AUTOLOAD_STATE_WAIT_FOR_BUTTON_PRESS_ON_ONE_OF_THE_PADS :
			WAIT_FOR_LEGALS();

			if (CLiveManager::IsSystemUiShowing())
			{
				ShowLoggedSignInUi();
				ms_AutoLoadSignInState = AUTOLOAD_STATE_WAIT_FOR_BUTTON_PRESS_ON_ONE_OF_THE_PADS_SIGN_IN_UI_IS_DISPLAYED;
			}
			else if (CControlMgr::GetMainPlayerIndex() != -1)
			{	//	If the player brings up the System UI before starting the game 
				//	or presses a button on one of the pads to select that pad
				//	then it can reach here
				ms_AutoLoadSignInState = AUTOLOAD_STATE_WAIT_FOR_PLAYER_TO_SIGN_IN;
			}
			else
			{
				CSavegameDialogScreens::HandleSaveGameDialogBox(SAVE_GAME_DIALOG_AUTOLOAD_MULTIPLE_PROFILES, 0, &bReturnSelection);
				//	This relies on the code in CControlMgr::Update that calls SetMainPlayerIndex
				//	on the first pad that has input (whether the profile for that pad is signed in or not).
			}
			break;

		case AUTOLOAD_STATE_WAIT_FOR_BUTTON_PRESS_ON_ONE_OF_THE_PADS_SIGN_IN_UI_IS_DISPLAYED :
			if (CLiveManager::IsSystemUiShowing() == false)
			{
				if (CControlMgr::GetMainPlayerIndex() != -1)
				{
					ms_AutoLoadSignInState = AUTOLOAD_STATE_WAIT_FOR_PLAYER_TO_SIGN_IN;
				}
				else
				{
					ms_AutoLoadSignInState = AUTOLOAD_STATE_WAIT_FOR_BUTTON_PRESS_ON_ONE_OF_THE_PADS;
				}
			}
			break;

		default :
			Assertf(0, "CSavegameAutoload::AutoLoadCheckForSignedInPlayer - unexpected state");
			break;
	}

	return AUTOLOAD_SIGN_IN_RETURN_STILL_CHECKING;	//	ReturnAutoLoadStatus;
}



#if !__PPU || !__QUICK_SCAN_FOR_AUTOLOAD_AND_SLOW_SCAN_ON_ANOTHER_THREAD
eAutoLoadEnumerationReturnValue CSavegameAutoload::AutoLoadEnumerateContent()
{
	switch (ms_AutoLoadFileEnumerationState)
	{
		case AUTOLOAD_STATE_BEGIN_ENUMERATE_ALL_DEVICES :
			if (CSavegameUsers::HasPlayerJustSignedOut() || -1 == CSavegameUsers::GetUser())
			{
//				SetSaveGameError(SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT);
//				return MEM_CARD_ERROR;
				return AUTOLOAD_ENUMERATION_RETURN_ENUMERATION_FAILED;
			}

			ClearSlotData();

			SAVEGAME.SetSelectedDeviceToAny(CSavegameUsers::GetUser());

			savegameDebugf1("CSavegameAutoload::AutoLoadEnumerateContent - BeginEnumeration\n");
			savegameDebugf1("Time = %d\n",sysTimer::GetSystemMsTime());
			savegameAssertf(ms_pSaveGameSlotsForAutoload, "CSavegameAutoload::AutoLoadEnumerateContent - ms_pSaveGameSlotsForAutoload buffer hasn't been allocated");
			if (!SAVEGAME.BeginEnumeration(CSavegameUsers::GetUser(), CGenericGameStorage::GetContentType(), ms_pSaveGameSlotsForAutoload, MAX_NUM_SAVE_FILES_TO_ENUMERATE_FOR_AUTOLOAD))
			{
				Assertf(0, "CSavegameAutoload::AutoLoadEnumerateContent - BeginEnumeration failed");
//				SetSaveGameError(SAVE_GAME_DIALOG_BEGIN_ENUMERATION_FAILED);
//				return MEM_CARD_ERROR;
				return AUTOLOAD_ENUMERATION_RETURN_ENUMERATION_FAILED;
			}
			ms_AutoLoadFileEnumerationState = AUTOLOAD_STATE_CHECK_ENUMERATE_ALL_DEVICES;
//			break;

		case AUTOLOAD_STATE_CHECK_ENUMERATE_ALL_DEVICES :
			if (SAVEGAME.CheckEnumeration(CSavegameUsers::GetUser(), ms_NumberOfSaveGameSlotsForAutoload))
			{	//	returns true for error or success, false for busy
				SAVEGAME.EndEnumeration(CSavegameUsers::GetUser());

				if (CSavegameUsers::HasPlayerJustSignedOut())
				{
//					SetSaveGameError(SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT);
//					return MEM_CARD_ERROR;
					return AUTOLOAD_ENUMERATION_RETURN_ENUMERATION_FAILED;
				}
/*
				if (HasSelectedDeviceJustBeenRemoved())
				{
					SetSaveGameError(SAVE_GAME_DIALOG_SELECTED_DEVICE_HAS_BEEN_REMOVED);
					return MEM_CARD_ERROR;
				}
*/
				if (SAVEGAME.GetState(CSavegameUsers::GetUser()) == fiSaveGameState::HAD_ERROR)
				{
//					SetSaveGameError(SAVE_GAME_DIALOG_CHECK_ENUMERATION_FAILED1);
//					return MEM_CARD_ERROR;
					return AUTOLOAD_ENUMERATION_RETURN_ENUMERATION_FAILED;
				}

// GSWTODO - what other errors can be returned?
				if ( (SAVEGAME.GetError(CSavegameUsers::GetUser()) != fiSaveGame::SAVE_GAME_SUCCESS) && 
					(SAVEGAME.GetError(CSavegameUsers::GetUser()) != fiSaveGame::NO_FILES_EXIST) )
				{	//	don't treat "no files" as an error
//					SetSaveGameError(SAVE_GAME_DIALOG_CHECK_ENUMERATION_FAILED2);
//					return MEM_CARD_ERROR;
					return AUTOLOAD_ENUMERATION_RETURN_ENUMERATION_FAILED;
				}

				savegameDebugf1("CSavegameAutoload::AutoLoadEnumerateContent - CheckEnumeration has finished\n");
				savegameDebugf1("ms_NumberOfSaveGameSlotsForAutoload = %d Time = %d\n", ms_NumberOfSaveGameSlotsForAutoload, sysTimer::GetSystemMsTime());

				if ( (SAVEGAME.GetError(CSavegameUsers::GetUser()) == fiSaveGame::NO_FILES_EXIST)
					|| (ms_NumberOfSaveGameSlotsForAutoload == 0) )
				{
					return AUTOLOAD_ENUMERATION_RETURN_ENUMERATION_COMPLETE;
				}
				else
				{
					return AUTOLOAD_ENUMERATION_RETURN_ENUMERATION_COMPLETE;
				}
			}
			break;

		default :
			Assertf(0, "CSavegameAutoload::AutoloadEnumerateContent - unexpected state");
			break;
	}

	return AUTOLOAD_ENUMERATION_RETURN_STILL_CHECKING;	//	ReturnAutoLoadStatus;
}
#endif	//	#if !__PPU || !__QUICK_SCAN_FOR_AUTOLOAD_AND_SLOW_SCAN_ON_ANOTHER_THREAD


eAutoLoadInviteReturnValue CSavegameAutoload::AutoLoadCheckForAcceptingInvite()
{
	switch (ms_AutoLoadInviteState)
	{
	case AUTOLOAD_STATE_BEGIN_CHECK_FOR_INVITE :
		ms_InviteConfirmationBlockedDuringAutoload = true;

		if (CSavegameUsers::GetSignedInUser())
		{
			if (CLiveManager::ShouldSkipBootUi(OUTPUT_ONLY(true)))
			{
				ms_AutoLoadInviteState = AUTOLOAD_STATE_CONTINUE_CHECK_FOR_INVITE;
				break;
			}
		}

		return AUTOLOAD_INVITE_RETURN_NO_INVITE;
//		break;
		// *************************************************************************************
	case AUTOLOAD_STATE_CONTINUE_CHECK_FOR_INVITE :
		ms_InviteConfirmationBlockedDuringAutoload = false;

		if (CSavegameUsers::HasPlayerJustSignedOut())
		{
			return AUTOLOAD_INVITE_RETURN_NO_INVITE;
		}
		else
		{
			if (CLiveManager::ShouldSkipBootUi())
			{
				if (CLiveManager::GetInviteMgr().HasConfirmedAcceptedInvite())
				{
					//ReturnAutoLoadStatus = AUTOLOAD_STATUS_JOIN_NETWORK_GAME;
					return AUTOLOAD_INVITE_RETURN_INVITE_ACCEPTED;
				}
			}
			else
			{
				return AUTOLOAD_INVITE_RETURN_NO_INVITE;
			}
		}
		break;

	default :
		Assertf(0, "CSavegameAutoload::AutoLoadCheckForAcceptingInvite - unexpected state");
		break;
	}

	return AUTOLOAD_INVITE_RETURN_STILL_CHECKING_INVITE;
}


#if __PPU
void CSavegameAutoload::SetAutoloadEnumerationHasCompleted()
{
#if !__NO_OUTPUT
	Displayf("Callstack for CSavegameAutoload::SetAutoloadEnumerationHasCompleted is ");
	sysStack::PrintStackTrace();
#endif	//	!__NO_OUTPUT

	ms_bAutoloadEnumerationHasCompleted = true;
}
#endif	//	__PPU


eAutoLoadDecisionReturnValue CSavegameAutoload::AutoLoadDecideWhetherToLoadOrStartANewGame()
{
//	#if __PPU
//	#if __QUICK_SCAN_FOR_AUTOLOAD_AND_SLOW_SCAN_ON_ANOTHER_THREAD
//		static u32 s_EpisodeIndexArray[MAX_NUM_SAVE_FILES_TO_ENUMERATE];
//	#endif
//	#endif

	switch (ms_AutoLoadDecisionState)
	{
	case AUTOLOAD_STATE_DECIDE_ON_COURSE_OF_ACTION :
		//			ms_InviteConfirmationBlockedDuringAutoload = true;

//		if (EXTRACONTENT.GetIsDLCFromDisc())
//		{
//			if (ms_AutoLoadEpisodeForDiscBuild == -1)
//			{
//				ASSERTMSG(0, "CSavegameAutoload::AutoLoadDecideWhetherToLoadOrStartANewGame - expected ms_AutoLoadEpisodeForDiscBuild to always have a valid value at this stage - Graeme");
//				ms_EpisodeNumberForSaveToBeLoaded = EXTRACONTENT.GetDiscDefaultEpisode();
//			}
//			else
//			{
//				ms_EpisodeNumberForSaveToBeLoaded = ms_AutoLoadEpisodeForDiscBuild;
//			}
//		}

		//	Decide whether to (in this order of importance) - 
		//	1. accept an invite
		//	2. Display message about new episode
		//	3. Load the most recent save
		//	4. Start a new single player session
 		if (!CSavegameUsers::GetSignedInUser()) 	//	Do I need to check HasPlayerJustSignedOut() too?
		{
			return AUTOLOAD_DECISION_RETURN_START_NEW_GAME;
		}
		else
		{
// if both left and right triggers are pressed then skip the autoload. This will allow the player into the game to delete the most recent save if it is corrupt.
			if(CControlMgr::GetPlayerPad() && CControlMgr::GetPlayerPad()->GetLeftShoulder2() && CControlMgr::GetPlayerPad()->GetRightShoulder2())
			{
				OUTPUT_ONLY(printf("Bypassing autoload (and disabling autosave)\n"));

				CPauseMenu::SetMenuPreference(PREF_AUTOSAVE, false);

				CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_PLAYER_HAS_SKIPPED_AUTOLOAD);
				ms_AutoLoadDecisionState = AUTOLOAD_STATE_DISPLAY_ERROR_MESSAGE;
			}
			else
			{
#if __PPU && __QUICK_SCAN_FOR_AUTOLOAD_AND_SLOW_SCAN_ON_ANOTHER_THREAD
				ms_AutoLoadDecisionState = AUTOLOAD_STATE_BEGIN_FIND_MOST_RECENT_SAVE_QUICK;
#else
				ms_AutoLoadDecisionState = AUTOLOAD_STATE_BEGIN_FIND_MOST_RECENT_SAVE;
#endif
			}
		}
		break;




#if !__PPU || !__QUICK_SCAN_FOR_AUTOLOAD_AND_SLOW_SCAN_ON_ANOTHER_THREAD
		case AUTOLOAD_STATE_BEGIN_FIND_MOST_RECENT_SAVE :
//			ms_AutoLoadMostRecentSaveDate.Initialise();
//			ms_AutoLoadMostRecentSaveSlot = -1;



//			will have to deal with no saves here
//				or no saves for the selected disc episode

			if (ms_NumberOfSaveGameSlotsForAutoload == 0)
			{
				return AUTOLOAD_DECISION_RETURN_START_NEW_GAME;
			}

			ms_AutoLoadDecisionState = AUTOLOAD_STATE_FIND_MOST_RECENT_SAVE;
			savegameDebugf1("CSavegameAutoload::AutoLoadDecideWhetherToLoadOrStartANewGame - AUTOLOAD_STATE_BEGIN_FIND_MOST_RECENT_SAVE - about to call ResetFileTimeScan\n");
			ResetFileTimeScan();
//			break;

		case AUTOLOAD_STATE_FIND_MOST_RECENT_SAVE :
		{
			bool bFinishedComparingDates = false;
//			CDate DateForCurrentSlot;
			MemoryCardError GettingFileTimeStatus = GetFileTimes();
			
			if (GettingFileTimeStatus == MEM_CARD_ERROR)
			{
				return AUTOLOAD_DECISION_RETURN_START_NEW_GAME;
			}
			else if (GettingFileTimeStatus == MEM_CARD_COMPLETE)
			{
				bFinishedComparingDates = true;
			}

			if (bFinishedComparingDates)
			{
				if (ms_MostRecentSaveSlot == -1)
				{
//					ReturnAutoLoadStatus = AUTOLOAD_STATUS_START_NEW_GAME;
					return AUTOLOAD_DECISION_RETURN_START_NEW_GAME;
				}
				else
				{
					ms_AutoLoadDecisionState = AUTOLOAD_STATE_CHECK_IF_MOST_RECENT_SAVE_IS_DAMAGED;
				}
			}
		}
			break;
#endif	//	#if !__PPU || !__QUICK_SCAN_FOR_AUTOLOAD_AND_SLOW_SCAN_ON_ANOTHER_THREAD

#if __PPU && __QUICK_SCAN_FOR_AUTOLOAD_AND_SLOW_SCAN_ON_ANOTHER_THREAD
//	On PS3, use the fact that cellSaveDataListAutoLoad orders the results by date to find the most recent save
//	Move the slow scanning of GetFileTimes to a separate thread once the autoload has started (function is called SlowPS3Scan)
//	This new function will have to set ms_bSaveGamesHaveAlreadyBeenScanned at the end

//	With this new method, we can bypass 
//	AUTOLOAD_STATE_BEGIN_ENUMERATE_ALL_DEVICES
//	AUTOLOAD_STATE_CHECK_ENUMERATE_ALL_DEVICES,
//	AUTOLOAD_STATE_BEGIN_FIND_MOST_RECENT_SAVE,
//	AUTOLOAD_STATE_FIND_MOST_RECENT_SAVE

		case AUTOLOAD_STATE_BEGIN_FIND_MOST_RECENT_SAVE_QUICK :
			if (CSavegameUsers::HasPlayerJustSignedOut())
			{
				return AUTOLOAD_DECISION_RETURN_START_NEW_GAME;
//				break;
			}

			ClearSlotData();

			SAVEGAME.SetSelectedDeviceToAny(CSavegameUsers::GetUser());
			savegameAssertf(ms_pSaveGameSlotsForAutoload, "CSavegameAutoload::AutoLoadDecideWhetherToLoadOrStartANewGame - ms_pSaveGameSlotsForAutoload buffer hasn't been allocated");
			if (!SAVEGAME.BeginEnumeration(CSavegameUsers::GetUser(), CGenericGameStorage::GetContentType(), ms_pSaveGameSlotsForAutoload, MAX_NUM_SAVE_FILES_TO_ENUMERATE_FOR_AUTOLOAD))
			{
				Assertf(0, "CSavegameAutoload::AutoLoadDecideWhetherToLoadOrStartANewGame - BeginEnumeration failed");
				return AUTOLOAD_DECISION_RETURN_START_NEW_GAME;
//				break;
			}
			ms_AutoLoadDecisionState = AUTOLOAD_STATE_FIND_MOST_RECENT_SAVE_QUICK;
//			break;

		case AUTOLOAD_STATE_FIND_MOST_RECENT_SAVE_QUICK :
			if (SAVEGAME.CheckEnumeration(CSavegameUsers::GetUser(), ms_NumberOfSaveGameSlotsForAutoload))
			{	//	returns true for error or success, false for busy
				SAVEGAME.EndEnumeration(CSavegameUsers::GetUser());

#if __PPU
				SetAutoloadEnumerationHasCompleted();
#endif	//	__PPU

				if (CSavegameUsers::HasPlayerJustSignedOut())
				{
					return AUTOLOAD_DECISION_RETURN_START_NEW_GAME;
//					break;
				}

				if (SAVEGAME.GetState(CSavegameUsers::GetUser()) == fiSaveGameState::HAD_ERROR)
				{
					return AUTOLOAD_DECISION_RETURN_START_NEW_GAME;
//					break;
				}

				if ( (SAVEGAME.GetError(CSavegameUsers::GetUser()) != fiSaveGame::SAVE_GAME_SUCCESS) && 
					(SAVEGAME.GetError(CSavegameUsers::GetUser()) != fiSaveGame::NO_FILES_EXIST) )
				{	//	don't treat "no files" as an error
					return AUTOLOAD_DECISION_RETURN_START_NEW_GAME;
//					break;
				}

				if ( (SAVEGAME.GetError(CSavegameUsers::GetUser()) == fiSaveGame::NO_FILES_EXIST)
					|| (ms_NumberOfSaveGameSlotsForAutoload == 0) )
				{
					savegameDebugf1("CSavegameAutoload::AutoLoadDecideWhetherToLoadOrStartANewGame - AUTOLOAD_STATE_FIND_MOST_RECENT_SAVE_QUICK - no save files found (not even a profile)\n");
					return AUTOLOAD_DECISION_RETURN_START_NEW_GAME;
				}
				else
				{
					ms_MostRecentSaveSlot = 0;
					bool bFoundAValidFilename = false;
					//	Some of the later code expects a slot number < MAX_NUM_SAVE_GAMES_TO_DISPLAY
					//	so quit looking for the most recent save if we've not found one in the first MAX_NUM_SAVE_GAMES_TO_DISPLAY.
					//	This isn't strictly correct for ELC because the first 15 slots could contain a profile and 14 saves for another episode
					//	and the 16th save could be the one save that applies to the episode that the player has chosen to play.

//	This could be worse now that the photos, profile and mp stats might appear before any savegames in the array

					//	I'll try removing this MAX_NUM_SAVE_GAMES_TO_DISPLAY check now
					while ( (ms_MostRecentSaveSlot < ms_NumberOfSaveGameSlotsForAutoload)
//						&& (ms_MostRecentSaveSlot < MAX_NUM_SAVE_GAMES_TO_DISPLAY)
						&& (!IsSaveGameSlotEmpty(ms_MostRecentSaveSlot))
						&& !bFoundAValidFilename)
					{
						if (CSavegameFilenames::IsThisTheNameOfASavegameFile(GetFilename(ms_MostRecentSaveSlot)))
						{
//							if (!EXTRACONTENT.GetIsDLCFromDisc())
							{
								bFoundAValidFilename = true;
							}
/*
							else
							{	//	ELC build so we should find the most recent save for the episode chosen by the player
								if ( (ms_AutoLoadEpisodeForDiscBuild == 1) || (ms_AutoLoadEpisodeForDiscBuild == 2) )
								{
									if (s_EpisodeIndexArray[ms_MostRecentSaveSlot] == ((u32) ms_AutoLoadEpisodeForDiscBuild) )
									{
										bFoundAValidFilename = true;
									}
								}
								else
								{	//	ms_AutoLoadEpisodeForDiscBuild should always be 1 or 2 at this stage to indicate which episode the player has chosen to play.
									//	If for some reason it's not set then just pick the most recent save that belongs to either of the valid episodes.
									ASSERTMSG(0, "CSavegameAutoload::AutoLoadDecideWhetherToLoadOrStartANewGame - expected ms_AutoLoadEpisodeForDiscBuild to always have a valid value at this stage - Graeme");
									if ( (s_EpisodeIndexArray[ms_MostRecentSaveSlot] == 1) || (s_EpisodeIndexArray[ms_MostRecentSaveSlot] == 2) )
									{
										bFoundAValidFilename = true;
									}
								}
							}
*/
						}

						if (!bFoundAValidFilename)
						{
							ms_MostRecentSaveSlot += 1;
						}
					}

					if (bFoundAValidFilename)
					{
						savegameDebugf1("CSavegameAutoload::AutoLoadDecideWhetherToLoadOrStartANewGame - AUTOLOAD_STATE_FIND_MOST_RECENT_SAVE_QUICK - valid save game in ordered slot %d (Slots ordered by modified time)\n", ms_MostRecentSaveSlot);
						ms_AutoLoadDecisionState = AUTOLOAD_STATE_GET_NAME_OF_MOST_RECENT_SAVE_QUICK;
					}
					else
					{
						ms_MostRecentSaveSlot = -1;
						savegameDebugf1("CSavegameAutoload::AutoLoadDecideWhetherToLoadOrStartANewGame - AUTOLOAD_STATE_FIND_MOST_RECENT_SAVE_QUICK - didn't find a valid save game filename\n");
						return AUTOLOAD_DECISION_RETURN_START_NEW_GAME;
					}
				}
			}
			break;

		case AUTOLOAD_STATE_GET_NAME_OF_MOST_RECENT_SAVE_QUICK :
		{
			MemoryCardError FileTimeReadStatus = GetTimeAndMissionNameFromDisplayName(ms_MostRecentSaveSlot);
			if (FileTimeReadStatus == MEM_CARD_ERROR)
			{
				savegameDebugf1("CSavegameAutoload::AutoLoadDecideWhetherToLoadOrStartANewGame - AUTOLOAD_STATE_GET_NAME_OF_MOST_RECENT_SAVE_QUICK - GetTimeAndMissionNameFromDisplayName returned an error\n");
				ms_MostRecentSaveSlot = -1;
				return AUTOLOAD_DECISION_RETURN_START_NEW_GAME;
			}
			else if (FileTimeReadStatus == MEM_CARD_COMPLETE)
			{
				ms_AutoLoadDecisionState = AUTOLOAD_STATE_CHECK_IF_MOST_RECENT_SAVE_IS_DAMAGED;
			}
		}
			break;
#endif	//	__PPU && __QUICK_SCAN_FOR_AUTOLOAD_AND_SLOW_SCAN_ON_ANOTHER_THREAD

//	#if __XENON
		case AUTOLOAD_STATE_DISPLAY_EPISODIC_CONTENT_MESSAGE :
		{
			return AUTOLOAD_DECISION_RETURN_START_NEW_GAME;
		}
		break;
//	#endif // __XENON
// *************************************************************************************

		case AUTOLOAD_STATE_CHECK_IF_MOST_RECENT_SAVE_IS_DAMAGED :
		{
			bool bMostRecentSaveIsDamaged = false;
			MemoryCardError DamagedSlotResult = MEM_CARD_ERROR;

			if (CSavegameUsers::HasPlayerJustSignedOut())
			{
//				ReturnAutoLoadStatus = AUTOLOAD_STATUS_START_NEW_GAME;
				return AUTOLOAD_DECISION_RETURN_START_NEW_GAME;
//				break;
			}

			//	Have to set the selected device to the one that contains the most recent save
			SAVEGAME.SetSelectedDevice(CSavegameUsers::GetUser(), GetDeviceId(ms_MostRecentSaveSlot));

			bMostRecentSaveIsDamaged = false;
			DamagedSlotResult = CSavegameDamagedCheck::CheckIfSlotIsDamaged(GetFilename(ms_MostRecentSaveSlot), &bMostRecentSaveIsDamaged);

			switch (DamagedSlotResult)
			{
				case MEM_CARD_COMPLETE :
					if (bMostRecentSaveIsDamaged)
					{
						CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_MOST_RECENT_SAVE_IS_DAMAGED);
						ms_AutoLoadDecisionState = AUTOLOAD_STATE_DISPLAY_ERROR_MESSAGE;
					}
					else
					{
						ms_AutoLoadDecisionState = AUTOLOAD_STATE_BEGIN_LOAD;
					}
					break;
				case MEM_CARD_BUSY :
					//	do nothing
					break;
				case MEM_CARD_ERROR :
//					ReturnAutoLoadStatus = AUTOLOAD_STATUS_START_NEW_GAME;
					return AUTOLOAD_DECISION_RETURN_START_NEW_GAME;
//					break;
			}
		}
			break;

		case AUTOLOAD_STATE_DISPLAY_ERROR_MESSAGE :
			if (CSavegameDialogScreens::HandleSaveGameErrorCode())
			{
//				ReturnAutoLoadStatus = AUTOLOAD_STATUS_START_NEW_GAME;
				return AUTOLOAD_DECISION_RETURN_START_NEW_GAME;
			}
			break;


		case AUTOLOAD_STATE_BEGIN_LOAD :
		{
			if (CSavegameUsers::HasPlayerJustSignedOut())
			{
//				SetSaveGameError(SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT);
//				return MEM_CARD_ERROR;
//				ReturnAutoLoadStatus = AUTOLOAD_STATUS_START_NEW_GAME;
				return AUTOLOAD_DECISION_RETURN_START_NEW_GAME;
//				break;
			}

//	#if __ASSERT
//			if (EXTRACONTENT.GetIsDLCFromDisc())
//			{
//				ASSERTMSG(ms_EpisodeNumberForSaveToBeLoaded == (int) s_EpisodeIndexArray[ms_MostRecentSaveSlot], "CSavegameAutoload::AutoLoadDecideWhetherToLoadOrStartANewGame - episode index in listParam does not match the one in the display name");
//				ASSERTMSG(ms_EpisodeNumberForSaveToBeLoaded == ms_AutoLoadEpisodeForDiscBuild, "CSavegameAutoload::AutoLoadDecideWhetherToLoadOrStartANewGame - episode index in the display name for the savegame does not match the episode selected by the player");
//			}
//	#endif

			savegameDebugf1("CSavegameAutoload::AutoLoadDecideWhetherToLoadOrStartANewGame - AUTOLOAD_STATE_BEGIN_LOAD - beginning to load save file in slot %d\n", ms_MostRecentSaveSlot);
			SAVEGAME.SetSelectedDevice(CSavegameUsers::GetUser(), GetDeviceId(ms_MostRecentSaveSlot));
			camInterface::FadeIn(0);
//			CFrontEnd::RequestState(FRONTEND_STATE_RESTART_SAVED_GAME_FADE_OUT);

//			BeginGameLoad(AUTOLOAD_SLOT);
//			ReturnAutoLoadStatus = AUTOLOAD_STATUS_DO_AUTOLOAD;
			return AUTOLOAD_DECISION_RETURN_DO_AUTOLOAD;
		}
//			break;

//		case AUTOLOAD_STATE_DO_NOTHING :
//			break;

		default :
			Assertf(0, "CSavegameAutoload::AutoloadDecideWhetherToLoadOrStartANewGame - unexpected state");
			break;
	}

	return AUTOLOAD_DECISION_RETURN_STILL_CHECKING;	//	ReturnAutoLoadStatus;
}

bool CSavegameAutoload::IsInviteConfirmationBlockedDuringAutoload()
{
	return ms_bPerformingAutoLoadAtStartOfGame && ms_InviteConfirmationBlockedDuringAutoload;
}

void CSavegameAutoload::ResetAfterAutoloadAtStartOfGame()
{
//	ms_SaveGameBuffer.SetCalculatingBufferSizeForFirstTime(true);
	CSavegameDevices::SetAllowDeviceHasBeenRemovedMessageToBeDisplayed(false);

	if (ms_pSaveGameSlotsForAutoload)
	{
		delete[] ms_pSaveGameSlotsForAutoload;
		ms_pSaveGameSlotsForAutoload = NULL;
	}

	ms_AutoLoadEpisodeForDiscBuild = -1;

//	CGenericGameStorage::ClearBufferToStoreFirstFewBytesOfSaveFile();

	if (CSavegameUsers::GetSignedInUser())
	{
		CSavegameDevices::SetDeviceIsInvalid(false);
	}
}


MemoryCardError CSavegameAutoload::GetTimeAndMissionNameFromDisplayName(int SaveGameSlot)
{
	if (!IsSaveGameSlotEmpty(SaveGameSlot))
	{
#if __DEV
		static char TempAsciiBuffer[128];
		CSavegameFilenames::CopyWideStringToUtf8String(GetDisplayNameForAutoload(SaveGameSlot), (int)wcslen(GetDisplayNameForAutoload(SaveGameSlot)), TempAsciiBuffer, NELEM(TempAsciiBuffer));
		savegameDebugf1("CSavegameAutoload::GetTimeAndMissionNameFromDisplayName - slot %d has name %s\n", SaveGameSlot, TempAsciiBuffer);
#endif
		savegameDebugf1("CSavegameAutoload::GetTimeAndMissionNameFromDisplayName - slot %d has file name %s\n", SaveGameSlot, GetFilename(SaveGameSlot));

		CDate DateForCurrentSlot;
		bool bDateCanBeChecked = false;

		int AutoLoadEpisodeNumber = 0;
		int FirstCharOfAutoloadName = 0;
		int LengthOfAutoloadName = 0;

#if RSG_ORBIS
		u64 fileTime;
		GetModificationTime(SaveGameSlot, fileTime);
		time_t SaveTimeForThisSlot = static_cast<time_t>(fileTime);
		bDateCanBeChecked = DateForCurrentSlot.ExtractDateFromTimeT(SaveTimeForThisSlot);
#else
		char SaveTimeString[32];
		SaveTimeString[0] = '\0';
#endif

		CSavegameFilenames::ExtractInfoFromDisplayName(GetDisplayNameForAutoload(SaveGameSlot), &AutoLoadEpisodeNumber, 
#if RSG_ORBIS
			NULL, 
#else
			&SaveTimeString[0], 
#endif
			&FirstCharOfAutoloadName, &LengthOfAutoloadName);

#if !RSG_ORBIS
		bDateCanBeChecked = DateForCurrentSlot.ExtractDateFromString(&SaveTimeString[0]);
#endif

//		if (EXTRACONTENT.GetIsDLCFromDisc() && (ms_AutoLoadEpisodeForDiscBuild != -1) && (ms_AutoLoadEpisodeForDiscBuild != AutoLoadEpisodeNumber))
//		{
//			bDateCanBeChecked = false;
//		}

		if (bDateCanBeChecked)
		{
			if (DateForCurrentSlot > ms_MostRecentSaveDate)
			{
				ms_MostRecentSaveDate = DateForCurrentSlot;
				ms_MostRecentSaveSlot = SaveGameSlot;
				savegameDebugf1("CSavegameAutoload::GetTimeAndMissionNameFromDisplayName - updating ms_MostRecentSaveDate with slot %d\n", SaveGameSlot);
//	PS3 used to set episode number to this instead
//				ms_EpisodeNumberForSaveToBeLoaded = ms_EpisodeNumber[SaveGameSlot];
				ms_EpisodeNumberForSaveToBeLoaded = AutoLoadEpisodeNumber;
//	The following two variables were only set on the 360
				ms_FirstCharacterOfAutoloadName = FirstCharOfAutoloadName;
				ms_LengthOfAutoloadName = LengthOfAutoloadName;
			}
		}
	}

	return MEM_CARD_COMPLETE;
}


void CSavegameAutoload::ResetFileTimeScan()
{
	ClearMostRecentSaveData();

	//	Make sure this doesn't happen half-way through the load
	ms_EpisodeNumberForSaveToBeLoaded = INDEX_OF_FIRST_LEVEL;
	// establish the default episode for this disc - if no suitable autoload is found then boot into this episode
//	ms_EpisodeNumberForSaveToBeLoaded = EXTRACONTENT.GetDiscDefaultEpisode();
//	if (EXTRACONTENT.GetIsDLCFromDisc_())
//	{
//		if (ms_AutoLoadEpisodeForDiscBuild != -1)
//		{
//			ms_EpisodeNumberForSaveToBeLoaded = ms_AutoLoadEpisodeForDiscBuild;
//		}
//	}

	ms_FirstCharacterOfAutoloadName = 0;
	ms_LengthOfAutoloadName = 0;
}

MemoryCardError CSavegameAutoload::GetFileTimes()
{
	s32 CurrentSlotForGetFileTime = 0;

#if USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP
	#define NUMBER_OF_AUTOSAVE_EPISODES	(3)

	bool bMainAutosaveExists[NUMBER_OF_AUTOSAVE_EPISODES];
	s32 slotIndexOfBackupAutosave[NUMBER_OF_AUTOSAVE_EPISODES];

	u32 episodeLoop = 0;
	while (episodeLoop < NUMBER_OF_AUTOSAVE_EPISODES)
	{
		bMainAutosaveExists[episodeLoop] = false;
		slotIndexOfBackupAutosave[episodeLoop] = -1;
		episodeLoop++;
	}

//	Scan through slots once to see if the autosave slot exists (for each episode)
//	Record the index of the backup autosave slot (for each episode)
	while (CurrentSlotForGetFileTime < MAX_NUM_SAVE_FILES_TO_ENUMERATE_FOR_AUTOLOAD)
	{
		if (!IsSaveGameSlotEmpty(CurrentSlotForGetFileTime))
		{
			const char *pFilenameForThisSlot = GetFilename(CurrentSlotForGetFileTime);
			s32 episodeIndexOfAutosave = -1;
			if (CSavegameFilenames::IsThisTheNameOfAnAutosaveFile(pFilenameForThisSlot))
			{
				episodeIndexOfAutosave = CSavegameFilenames::GetEpisodeIndexFromAutosaveFilename(pFilenameForThisSlot);
				if (savegameVerifyf( (episodeIndexOfAutosave >= 0) && (episodeIndexOfAutosave < NUMBER_OF_AUTOSAVE_EPISODES), "CSavegameAutoload::GetFileTimes - episodeIndex of %s is %d. Expected it to be >= 0 and < %d", pFilenameForThisSlot, episodeIndexOfAutosave, NUMBER_OF_AUTOSAVE_EPISODES))
				{
					bMainAutosaveExists[episodeIndexOfAutosave] = true;
				}
			}
			else if (CSavegameFilenames::IsThisTheNameOfAnAutosaveBackupFile(pFilenameForThisSlot))
			{
				episodeIndexOfAutosave = CSavegameFilenames::GetEpisodeIndexFromAutosaveBackupFilename(pFilenameForThisSlot);
				if (savegameVerifyf( (episodeIndexOfAutosave >= 0) && (episodeIndexOfAutosave < NUMBER_OF_AUTOSAVE_EPISODES), "CSavegameAutoload::GetFileTimes - episodeIndex of %s is %d. Expected it to be >= 0 and < %d", pFilenameForThisSlot, episodeIndexOfAutosave, NUMBER_OF_AUTOSAVE_EPISODES))
				{
					slotIndexOfBackupAutosave[episodeIndexOfAutosave] = CurrentSlotForGetFileTime;
				}
			}
		}

		CurrentSlotForGetFileTime++;
	}

//	If the main autosave slot doesn't exist then empty the backup autosave slot
	episodeLoop = 0;
	while (episodeLoop < NUMBER_OF_AUTOSAVE_EPISODES)
	{
		if ( (slotIndexOfBackupAutosave[episodeLoop] != -1) && (!bMainAutosaveExists[episodeLoop]) )
		{
			ClearSlot(slotIndexOfBackupAutosave[episodeLoop]);
		}

		episodeLoop++;
	}
#endif	//	USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP


	CurrentSlotForGetFileTime = 0;
	while (CurrentSlotForGetFileTime < MAX_NUM_SAVE_FILES_TO_ENUMERATE_FOR_AUTOLOAD)
	{
		if (!IsSaveGameSlotEmpty(CurrentSlotForGetFileTime))
		{
			if (CSavegameFilenames::IsThisTheNameOfASavegameFile(GetFilename(CurrentSlotForGetFileTime))
#if USE_SAVE_DATA_MEMORY || USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP
				|| CSavegameFilenames::IsThisTheNameOfAnAutosaveBackupFile(GetFilename(CurrentSlotForGetFileTime))
#endif	//	USE_SAVE_DATA_MEMORY || USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP
				)
			{
				MemoryCardError FileTimeReadStatus = GetTimeAndMissionNameFromDisplayName(CurrentSlotForGetFileTime);
				if (FileTimeReadStatus == MEM_CARD_ERROR)
				{
					return MEM_CARD_ERROR;
				}
				else if (FileTimeReadStatus == MEM_CARD_BUSY)
				{
					savegameAssertf(0, "CSavegameAutoload::GetFileTimes - didn't expect GetTimeAndMissionNameFromDisplayName to return MEM_CARD_BUSY");
				}
			}
		}

		CurrentSlotForGetFileTime++;
	}

	return MEM_CARD_COMPLETE;
}

MemoryCardError CSavegameAutoload::BeginGameAutoload()
{
	if (!savegameVerifyf(CGenericGameStorage::GetSaveOperation() == OPERATION_NONE, "CSavegameAutoload::BeginGameAutoload - operation is not OPERATION_NONE"))
	{
		return MEM_CARD_ERROR;
	}

	if (savegameVerifyf(CSavegameLoad::GetLoadStatus() == GENERIC_LOAD_DO_NOTHING, "CSavegameAutoload::BeginGameAutoload - expected LoadStatus to be GENERIC_LOAD_DO_NOTHING"))
	{
		CSavegameFilenames::SetFilenameOfLocalFile(GetFilename(ms_MostRecentSaveSlot));

		CSavegameLoad::BeginLoad(SAVEGAME_SINGLE_PLAYER);
		CNetworkTelemetry::LoadGame();
	}

	return MEM_CARD_COMPLETE;
}


char* CSavegameAutoload::GetNameOfSavedGameForMenu(int SlotNumber)
{
	static char TempAsciiBuffer[MAX_MENU_ITEM_CHAR_LENGTH];
	TempAsciiBuffer[0] = '\0';

	if (savegameVerifyf( (SlotNumber >= 0) && (SlotNumber < MAX_NUM_SAVE_FILES_TO_ENUMERATE_FOR_AUTOLOAD), "CSavegameAutoload::GetNameOfSavedGameForMenu - During autoload, slot number should be >= 0 and < MAX_NUM_SAVE_FILES_TO_ENUMERATE_FOR_AUTOLOAD"))
	{
		if (SlotNumber != ms_MostRecentSaveSlot)
		{
			Assertf(0, "CSavegameAutoload::GetNameOfSavedGameForMenu - SlotNumber should be ms_MostRecentSaveSlot for autoloading");
			SlotNumber = ms_MostRecentSaveSlot;
		}

		int FirstCharacter = ms_FirstCharacterOfAutoloadName;
		int MaxLength = MIN(MAX_MENU_ITEM_CHAR_LENGTH, ms_LengthOfAutoloadName+1);

		if (savegameVerifyf(ms_pSaveGameSlotsForAutoload, "CSavegameAutoload::GetNameOfSavedGameForMenu - ms_pSaveGameSlotsForAutoload buffer hasn't been allocated"))
		{
			CSavegameFilenames::CopyWideStringToUtf8String(&ms_pSaveGameSlotsForAutoload[SlotNumber].DisplayName[FirstCharacter], MaxLength, TempAsciiBuffer, NELEM(TempAsciiBuffer));
		}
	}

	return TempAsciiBuffer;
}


void CSavegameAutoload::ClearSlot(s32 slotIndex)
{
	if (savegameVerifyf( (slotIndex >= 0) && (slotIndex < MAX_NUM_SAVE_FILES_TO_ENUMERATE_FOR_AUTOLOAD), "CSavegameAutoload::ClearSlot - slotIndex is %d. Expected it to be >= 0 and < %d", slotIndex, MAX_NUM_SAVE_FILES_TO_ENUMERATE_FOR_AUTOLOAD))
	{
		ms_pSaveGameSlotsForAutoload[slotIndex].DeviceId = 0;
		ms_pSaveGameSlotsForAutoload[slotIndex].ContentType = 0;
		ms_pSaveGameSlotsForAutoload[slotIndex].DisplayName[0] = '\0';	//	_TCHAR
		ms_pSaveGameSlotsForAutoload[slotIndex].Filename[0] = '\0';
	}
}

void CSavegameAutoload::ClearSlotData(void)
{
	if (ms_pSaveGameSlotsForAutoload)
	{
		savegameAssertf(0, "CSavegameAutoload::ClearSlotData - didn't expect ms_pSaveGameSlotsForAutoload to have already been allocated at this stage");
		delete[] ms_pSaveGameSlotsForAutoload;
		ms_pSaveGameSlotsForAutoload = NULL;
	}

	ms_pSaveGameSlotsForAutoload = rage_new fiSaveGame::Content[MAX_NUM_SAVE_FILES_TO_ENUMERATE_FOR_AUTOLOAD];

	for (int loop = 0; loop < MAX_NUM_SAVE_FILES_TO_ENUMERATE_FOR_AUTOLOAD; loop++)
	{
		ClearSlot(loop);
	}
}


#if RSG_ORBIS
void CSavegameAutoload::GetModificationTime(int SlotIndex, u64 &ModificationTime)
{
	ModificationTime = 0;
	if (Verifyf((SlotIndex >= 0) && (SlotIndex < MAX_NUM_SAVE_FILES_TO_ENUMERATE_FOR_AUTOLOAD), "CSavegameAutoload::GetModificationTime - slot index is out of range"))
	{
		if (savegameVerifyf(ms_pSaveGameSlotsForAutoload, "CSavegameAutoload::GetModificationTime - ms_pSaveGameSlotsForAutoload buffer hasn't been allocated"))
		{
			ModificationTime = static_cast<u64>(ms_pSaveGameSlotsForAutoload[SlotIndex].ModificationTimeHigh) << 32;
			ModificationTime += static_cast<u64>(ms_pSaveGameSlotsForAutoload[SlotIndex].ModificationTimeLow);
		}
	}
}
#endif

u32 CSavegameAutoload::GetDeviceId(int SlotIndex)
{
	if (Verifyf((SlotIndex >= 0) && (SlotIndex < MAX_NUM_SAVE_FILES_TO_ENUMERATE_FOR_AUTOLOAD), "CSavegameAutoload::GetDeviceId - slot index is out of range"))
	{
		if (savegameVerifyf(ms_pSaveGameSlotsForAutoload, "CSavegameAutoload::GetDeviceId - ms_pSaveGameSlotsForAutoload buffer hasn't been allocated"))
		{
			return ms_pSaveGameSlotsForAutoload[SlotIndex].DeviceId;
		}
	}
	return 0;
}


const char *CSavegameAutoload::GetFilename(int SlotIndex)
{
	if (Verifyf((SlotIndex >= 0) && (SlotIndex < MAX_NUM_SAVE_FILES_TO_ENUMERATE_FOR_AUTOLOAD), "CSavegameAutoload::GetFilename - slot index is out of range"))
	{
		if (savegameVerifyf(ms_pSaveGameSlotsForAutoload, "CSavegameAutoload::GetFilename - ms_pSaveGameSlotsForAutoload buffer hasn't been allocated"))
		{
			return &(ms_pSaveGameSlotsForAutoload[SlotIndex].Filename[0]);
		}
	}
	return NULL;
}

const char16 *CSavegameAutoload::GetDisplayNameForAutoload(int SlotIndex)
{
	if (Verifyf((SlotIndex >= 0) && (SlotIndex < MAX_NUM_SAVE_FILES_TO_ENUMERATE_FOR_AUTOLOAD), "CSavegameAutoload::GetDisplayNameForAutoload - slot index is out of range"))
	{
		if (savegameVerifyf(ms_pSaveGameSlotsForAutoload, "CSavegameAutoload::GetDisplayNameForAutoload - ms_pSaveGameSlotsForAutoload buffer hasn't been allocated"))
		{
			return &(ms_pSaveGameSlotsForAutoload[SlotIndex].DisplayName[0]);
		}
	}
	return NULL;
}


bool CSavegameAutoload::IsSaveGameSlotEmpty(int SlotIndex)
{
	if (Verifyf((SlotIndex >= 0) && (SlotIndex < MAX_NUM_SAVE_FILES_TO_ENUMERATE_FOR_AUTOLOAD), "CSavegameAutoload::IsSaveGameSlotEmpty - slot index is out of range") )
	{
		if (savegameVerifyf(ms_pSaveGameSlotsForAutoload, "CSavegameAutoload::IsSaveGameSlotEmpty - ms_pSaveGameSlotsForAutoload buffer hasn't been allocated"))
		{
			if (ms_pSaveGameSlotsForAutoload[SlotIndex].Filename[0] == '\0')
			{
				Assertf(ms_pSaveGameSlotsForAutoload[SlotIndex].ContentType == 0, "CSavegameAutoload::IsSaveGameSlotEmpty - Filename is empty for this slot, expected ContentType to be 0 too");
				Assertf(ms_pSaveGameSlotsForAutoload[SlotIndex].DeviceId == 0, "CSavegameAutoload::IsSaveGameSlotEmpty - Filename is empty for this slot, expected DeviceId to be 0 too");
				Assertf(ms_pSaveGameSlotsForAutoload[SlotIndex].DisplayName[0] == '\0', "CSavegameAutoload::IsSaveGameSlotEmpty - Filename is empty for this slot, expected DisplayName to be empty too");
				return true;
			}
		}
	}

	return false;
}

void CSavegameAutoload::ClearMostRecentSaveData()
{
	ms_MostRecentSaveDate.Initialise();
	ms_MostRecentSaveSlot = -1;
}


