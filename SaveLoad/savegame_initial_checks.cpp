
// Rage headers
#include "file/savegame.h"

// network includes
#include "network/Live/livemanager.h"

// Game headers
#include "frontend/ProfileSettings.h"
#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/savegame_autoload.h"
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/savegame_free_space_check.h"
#include "SaveLoad/savegame_initial_checks.h"
#include "SaveLoad/savegame_list.h"
#include "SaveLoad/savegame_users_and_devices.h"

SAVEGAME_OPTIMISATIONS()

enum
{
	BEGIN_PLAYER_SIGN_IN,
	PLAYER_SIGN_IN_DIALOG,
	PLAYER_DOESNT_WANT_TO_SIGN_IN_TRY_AGAIN,
	CHECK_PLAYER_SIGNED_IN,
	BEGIN_DEVICE_SELECTION,
	DEVICE_SELECTION_DIALOG,
	DISPLAY_DEVICE_SELECTION_UI_ONCE_NO_OTHER_UIS_ARE_SHOWING,
	PLAYER_DOESNT_WANT_TO_SELECT_DEVICE_TRY_AGAIN,
	CHECK_DEVICE_SELECTION,
	BEGIN_ENUMERATION,
	CHECK_ENUMERATION,
	SORT_SAVE_GAME_SLOTS,
	GET_FILE_TIMES,
	BEGIN_FREE_SPACE,
	CHECK_FREE_SPACE,
	FREE_SPACE_DIALOG
//	CHECK_FILE_DELETION
};


int			CSavegameInitialChecks::ms_InitialChecksState = BEGIN_PLAYER_SIGN_IN;

u32			CSavegameInitialChecks::ms_SizeOfBufferToBeSaved = 0;

bool 		CSavegameInitialChecks::ms_bManageStorage = true;
//	GSWTODO - do i need to add a forceshow flag here too?

bool		CSavegameInitialChecks::ms_bPlayerChoseNotToSignIn = false;
bool		CSavegameInitialChecks::ms_bPlayerChoseNotToSelectADevice = false;

bool		CSavegameInitialChecks::ms_bForcePlayerToChooseADevice = false;	//	Needed for the Select Device button on the frontend
																			//	Brings up the Select Device UI even if the player has already selected a storage device
																			//	Also bypasses the "Do you want to select a device?" dialog box


bool CSavegameInitialChecks::ShouldTryAgainMessageBeDisplayed()
{
	bool bMessageShouldBeDisplayed = true;
	switch (CGenericGameStorage::GetSaveOperation())
	{
		case OPERATION_NONE :
			savegameAssertf(0, "CSavegameInitialChecks::ShouldTryAgainMessageBeDisplayed - didn't expect SaveOperation to be OPERATION_NONE");
			bMessageShouldBeDisplayed = false;
			break;

		case OPERATION_SCANNING_CONSOLE_FOR_LOADING_SAVEGAMES :
			bMessageShouldBeDisplayed = false;
			break;

		case OPERATION_SCANNING_CONSOLE_FOR_SAVING_SAVEGAMES :
			bMessageShouldBeDisplayed = true;
			break;

		case OPERATION_LOADING_SAVEGAME_FROM_CONSOLE :
		case OPERATION_LOADING_REPLAY_SAVEGAME_FROM_CONSOLE :
			savegameAssertf(0, "CSavegameInitialChecks::ShouldTryAgainMessageBeDisplayed - shouldn't get here for loading");
			bMessageShouldBeDisplayed = false;
			break;

		case OPERATION_LOADING_PHOTO_FROM_CLOUD :
			bMessageShouldBeDisplayed = false;
			break;

		case OPERATION_SAVING_SAVEGAME_TO_CONSOLE :
			savegameAssertf(0, "CSavegameInitialChecks::ShouldTryAgainMessageBeDisplayed - shouldn't get here for a manual save");
			bMessageShouldBeDisplayed = true;
			break;

		case OPERATION_SAVING_MISSION_REPEAT_SAVEGAME_TO_CONSOLE :
		case OPERATION_SAVING_REPLAY_SAVEGAME_TO_CONSOLE :
			bMessageShouldBeDisplayed = true;
			break;

		case OPERATION_AUTOSAVING :
			bMessageShouldBeDisplayed = true;
			break;

		case OPERATION_SAVING_PHOTO_TO_CLOUD :
#if __ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
		case OPERATION_UPLOADING_SAVEGAME_TO_CLOUD :
#endif	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
			bMessageShouldBeDisplayed = true;
			break;
#if RSG_ORBIS
		case OPERATION_CHECKING_FOR_FREE_SPACE_AT_START_OF_GAME :
			bMessageShouldBeDisplayed = true;	//	not sure whether this should be true or false
			break;
#endif

		case OPERATION_CHECKING_FOR_FREE_SPACE :
			bMessageShouldBeDisplayed = true;
			break;

		case OPERATION_CHECKING_IF_FILE_EXISTS :
			bMessageShouldBeDisplayed = false;
			break;

		case OPERATION_LOADING_MPSTATS_FROM_CLOUD :
#if __ALLOW_LOCAL_MP_STATS_SAVES
		case OPERATION_LOADING_MPSTATS_SAVEGAME_FROM_CONSOLE :
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES
			bMessageShouldBeDisplayed = false;
			break;

		case OPERATION_SAVING_MPSTATS_TO_CLOUD :
#if __ALLOW_LOCAL_MP_STATS_SAVES
		case OPERATION_SAVING_MPSTATS_SAVEGAME_TO_CONSOLE :
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES
			bMessageShouldBeDisplayed = true;
			break;

		case OPERATION_DELETING_LOCAL :
		case OPERATION_DELETING_CLOUD :
			bMessageShouldBeDisplayed = false;
			break;

		case OPERATION_SAVING_LOCAL_PHOTO :
		case OPERATION_LOADING_LOCAL_PHOTO :
			bMessageShouldBeDisplayed = false;
			break;

		case OPERATION_ENUMERATING_PHOTOS :
		case OPERATION_ENUMERATING_SAVEGAMES :
			bMessageShouldBeDisplayed = false;
			break;
	}

	return bMessageShouldBeDisplayed;
}

u32 CSavegameInitialChecks::GetSizeOfBufferToBeSaved()
{
	switch (CGenericGameStorage::GetSaveOperation())
	{
	case OPERATION_NONE :
		savegameAssertf(0, "CSavegameInitialChecks::GetSizeOfBufferToBeSaved - didn't expect OPERATION_NONE");
		break;

	case OPERATION_LOADING_MPSTATS_FROM_CLOUD :
		savegameAssertf(0, "CSavegameInitialChecks::GetSizeOfBufferToBeSaved - Graeme - just checking if it ever gets here for OPERATION_LOADING_MPSTATS_FROM_CLOUD");
		break;

	case OPERATION_LOADING_PHOTO_FROM_CLOUD :
		savegameAssertf(0, "CSavegameInitialChecks::GetSizeOfBufferToBeSaved - Graeme - shouldn't get here for OPERATION_LOADING_PHOTO_FROM_CLOUD");
		break;

	case OPERATION_SCANNING_CONSOLE_FOR_LOADING_SAVEGAMES :
	case OPERATION_LOADING_SAVEGAME_FROM_CONSOLE :
	case OPERATION_LOADING_REPLAY_SAVEGAME_FROM_CONSOLE :
#if __ALLOW_LOCAL_MP_STATS_SAVES
	case OPERATION_LOADING_MPSTATS_SAVEGAME_FROM_CONSOLE :
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES
	case OPERATION_SCANNING_CONSOLE_FOR_SAVING_SAVEGAMES :
#if RSG_ORBIS
	case OPERATION_CHECKING_FOR_FREE_SPACE_AT_START_OF_GAME :
#endif
	case OPERATION_CHECKING_IF_FILE_EXISTS :
	case OPERATION_DELETING_LOCAL :
	case OPERATION_DELETING_CLOUD :
	case OPERATION_LOADING_LOCAL_PHOTO :
	case OPERATION_ENUMERATING_PHOTOS :
	case OPERATION_ENUMERATING_SAVEGAMES :
		break;

	case OPERATION_SAVING_MPSTATS_TO_CLOUD :
		savegameAssertf(0, "CSavegameInitialChecks::GetSizeOfBufferToBeSaved - Graeme - just checking if it ever gets here for OPERATION_SAVING_MPSTATS_TO_CLOUD");
		break;

	case OPERATION_SAVING_PHOTO_TO_CLOUD :
		savegameAssertf(0, "CSavegameInitialChecks::GetSizeOfBufferToBeSaved - Graeme - just checking if it ever gets here for OPERATION_SAVING_PHOTO_TO_CLOUD");
		break;

#if __ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
	case OPERATION_UPLOADING_SAVEGAME_TO_CLOUD :
		savegameAssertf(0, "CSavegameInitialChecks::GetSizeOfBufferToBeSaved - Graeme - just checking if it ever gets here for OPERATION_UPLOADING_SAVEGAME_TO_CLOUD");
		break;
#endif	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME

	case OPERATION_SAVING_SAVEGAME_TO_CONSOLE :
	case OPERATION_SAVING_MISSION_REPEAT_SAVEGAME_TO_CONSOLE :
	case OPERATION_SAVING_REPLAY_SAVEGAME_TO_CONSOLE :
	case OPERATION_AUTOSAVING :
	case OPERATION_CHECKING_FOR_FREE_SPACE :
#if __ALLOW_LOCAL_MP_STATS_SAVES
	case OPERATION_SAVING_MPSTATS_SAVEGAME_TO_CONSOLE :
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES
	case OPERATION_SAVING_LOCAL_PHOTO :
		return ms_SizeOfBufferToBeSaved;
	}

	return 0;
}

void CSavegameInitialChecks::BeginInitialChecks(bool bForcePlayerToChooseADevice)
{
	ms_bManageStorage = true;
// GSWTODO - have a force show flag too?
	savegameAssertf(CGenericGameStorage::IsStorageDeviceBeingAccessed(), "CSavegameInitialChecks::BeginInitialChecks - didn't expect operation to be OPERATION_NONE at this stage");
	ms_bForcePlayerToChooseADevice = bForcePlayerToChooseADevice;

	ms_bPlayerChoseNotToSignIn = false;
	ms_bPlayerChoseNotToSelectADevice = false;

	ms_InitialChecksState = BEGIN_PLAYER_SIGN_IN;
}

bool CSavegameInitialChecks::ShowLoggedSignInUi()
{
	savegameDisplayf("ShowSigninUi from CSavegameInitialChecks");
	return CLiveManager::ShowSigninUi();
}

MemoryCardError CSavegameInitialChecks::InitialChecks()
{
	static int SpaceRequired = 0;

	ms_bPlayerChoseNotToSignIn = false;
	ms_bPlayerChoseNotToSelectADevice = false;

	bool ReturnSelection;

	switch (ms_InitialChecksState)
	{
		case BEGIN_PLAYER_SIGN_IN :
#if RSG_DURANGO
			if (CSavegameUsers::GetSignedInUser() && !CLiveManager::SignedInIntoNewProfileAfterSuspendMode())
#else
			if (CSavegameUsers::GetSignedInUser())
#endif
			{
				ms_InitialChecksState = BEGIN_DEVICE_SELECTION;	//	CALCULATE_SAVEGAME_SIZE;
				//	Kevin Baca added code that restarted the session if the player is signed into a profile and another player on a different Xbox signs in to the same profile.
				//	It was possible for this to happen while the storage device was in use so I've added the code below as a safeguard.
				//	The particular case in Bug 173335 shouldn't happen any more as Kevin has changed the code so that only network sessions are affected by Xbox Live disconnects.
				savegameAssertf( (SAVEGAME.GetState(CSavegameUsers::GetUser()) == fiSaveGameState::IDLE) || (SAVEGAME.GetState(CSavegameUsers::GetUser()) == fiSaveGameState::HAD_ERROR), "CSavegameInitialChecks::InitialChecks - expected save game state to be IDLE or HAD_ERROR but it's %d", (s32) SAVEGAME.GetState(CSavegameUsers::GetUser()));
				SAVEGAME.SetStateToIdle(CSavegameUsers::GetUser());
			}
			else
			{
				if (CGenericGameStorage::GetSaveOperation() == OPERATION_LOADING_REPLAY_SAVEGAME_FROM_CONSOLE)
				{
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_NONE);
					ms_bPlayerChoseNotToSignIn = true;
					return MEM_CARD_ERROR;
				}
				else
				{
					ms_InitialChecksState = PLAYER_SIGN_IN_DIALOG;
				}
			}
			break;

		case PLAYER_SIGN_IN_DIALOG :
			if (CLiveManager::IsSystemUiShowing())
			{
				ShowLoggedSignInUi();
				ms_InitialChecksState = CHECK_PLAYER_SIGNED_IN;
			}
			else if (CSavegameUsers::GetSignedInUser())
			{	//	If the player brings up the System UI before starting the game then it can reach here
				ms_InitialChecksState = CHECK_PLAYER_SIGNED_IN;
			}
// TCRHERE - You are not signed in. Do you want to sign in now?
			else if (CSavegameDialogScreens::HandleSaveGameDialogBox(SAVE_GAME_DIALOG_SIGN_IN, 0, &ReturnSelection))
			{
				if (ReturnSelection)
				{
					ShowLoggedSignInUi();
					ms_InitialChecksState = CHECK_PLAYER_SIGNED_IN;
				}
				else
				{	//	player doesn't want to sign in
					ms_InitialChecksState = PLAYER_DOESNT_WANT_TO_SIGN_IN_TRY_AGAIN;
				}
			}
			break;

		case PLAYER_DOESNT_WANT_TO_SIGN_IN_TRY_AGAIN :
		{
			bool bShouldMessageBeDisplayed = ShouldTryAgainMessageBeDisplayed();

			ReturnSelection = false;
			if (CLiveManager::IsSystemUiShowing())
			{
				ShowLoggedSignInUi();
				ms_InitialChecksState = CHECK_PLAYER_SIGNED_IN;
			}
			else if (CSavegameUsers::GetSignedInUser())
			{	//	If the player brings up the System UI before starting the game then it can reach here
				ms_InitialChecksState = CHECK_PLAYER_SIGNED_IN;
			}
			else if (!bShouldMessageBeDisplayed || CSavegameDialogScreens::HandleSaveGameDialogBox(SAVE_GAME_DIALOG_DONT_SIGN_IN_TRY_AGAIN, 0, &ReturnSelection))
			{
				//	You have not signed in. You will need to sign in to save your progress 
				//	and to be awarded Achievements. Do you want to sign in now?
				//	(Should I mention that you need a live enabled profile to play multiplayer games)
				if (ReturnSelection && bShouldMessageBeDisplayed)
				{
					ShowLoggedSignInUi();
					ms_InitialChecksState = CHECK_PLAYER_SIGNED_IN;
				}
				else
				{
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_NONE);
					ms_bPlayerChoseNotToSignIn = true;
					return MEM_CARD_ERROR;
				}
			}
		}
			break;

		case CHECK_PLAYER_SIGNED_IN :
#if RSG_DURANGO
			if(CLiveManager::SignedInIntoNewProfileAfterSuspendMode())
			{
				CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT);
				ms_bPlayerChoseNotToSignIn = true;
				ms_InitialChecksState = PLAYER_DOESNT_WANT_TO_SIGN_IN_TRY_AGAIN;
				return MEM_CARD_ERROR;
			}
#endif
			if (CLiveManager::IsSystemUiShowing() == false)
			{
				if (CSavegameUsers::GetSignedInUser())
				{	//	If player has just signed in then force device selection
//					SetDeviceIsInvalid(false);
					CSavegameUsers::ClearVariablesForNewlySignedInPlayer();
					ms_InitialChecksState = BEGIN_DEVICE_SELECTION;	//	CALCULATE_SAVEGAME_SIZE;
				}
				else
				{
					ms_InitialChecksState = PLAYER_DOESNT_WANT_TO_SIGN_IN_TRY_AGAIN;
				}
//				ms_InitialChecksState = BEGIN_PLAYER_SIGN_IN;	//	start from the beginning
			}
			break;
// *************************************************************************************
		case BEGIN_DEVICE_SELECTION :
			if (CSavegameUsers::HasPlayerJustSignedOut())
			{
				CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT);
				return MEM_CARD_ERROR;
			}

			if ( (CGenericGameStorage::GetSaveOperation() == OPERATION_SAVING_PHOTO_TO_CLOUD)
				|| (CGenericGameStorage::GetSaveOperation() == OPERATION_LOADING_PHOTO_FROM_CLOUD)
#if __ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
				|| (CGenericGameStorage::GetSaveOperation() == OPERATION_UPLOADING_SAVEGAME_TO_CLOUD)
#endif	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
				)

			{
				CSavegameDialogScreens::SetMessageAsProcessing(false);
				return MEM_CARD_COMPLETE;
			}
			
			if (ms_bForcePlayerToChooseADevice)
			{
				ms_InitialChecksState = DEVICE_SELECTION_DIALOG;
			}
			else if (!CSavegameDevices::IsDeviceValid())
			{
				bool bDeviceRemovalError = true;
				switch (CGenericGameStorage::GetSaveOperation())
				{	//	Allow player to reselect storage device for autosaving and scanning.
					case OPERATION_NONE :
						savegameAssertf(0, "CSavegameInitialChecks::InitialChecks - didn't expect OPERATION_NONE");
						break;

					//	For loading and saving we have to return an error because the player 
					//	will have already selected a slot and we can't allow the player to
					//	change storage device without reselecting the slot.

					//	OPERATION_CHECKING_FOR_FREE_SPACE is used for saving and autosaving but returning
					//	an error for autosaving should still return to a state where the player
					//	can try to select a device and slot again. If ms_bManageStorage is false
					//	then allow the device selection UI to show again so that the player can choose
					//	a different device or try to free space on the current device but always return
					//	an error to force a rescan at the end in case the device is changed.
					case OPERATION_SCANNING_CONSOLE_FOR_LOADING_SAVEGAMES :
					case OPERATION_SCANNING_CONSOLE_FOR_SAVING_SAVEGAMES :
#if RSG_ORBIS
					case OPERATION_CHECKING_FOR_FREE_SPACE_AT_START_OF_GAME :
#endif
					case OPERATION_ENUMERATING_PHOTOS :
					case OPERATION_ENUMERATING_SAVEGAMES :
						bDeviceRemovalError = false;
						break;

					case OPERATION_LOADING_SAVEGAME_FROM_CONSOLE :
					case OPERATION_LOADING_REPLAY_SAVEGAME_FROM_CONSOLE :
						bDeviceRemovalError = true;
						break;

					case OPERATION_LOADING_MPSTATS_FROM_CLOUD:
						savegameAssertf(0, "CSavegameInitialChecks::InitialChecks - BEGIN_DEVICE_SELECTION - Graeme - just checking if it ever gets here for OPERATION_LOADING_MPSTATS_FROM_CLOUD");
						bDeviceRemovalError = false;
						break;

					case OPERATION_SAVING_MPSTATS_TO_CLOUD:
						savegameAssertf(0, "CSavegameInitialChecks::InitialChecks - BEGIN_DEVICE_SELECTION - Graeme - just checking if it ever gets here for OPERATION_SAVING_MPSTATS_TO_CLOUD");
						bDeviceRemovalError = false;
						break;

					case OPERATION_LOADING_PHOTO_FROM_CLOUD :
						savegameAssertf(0, "CSavegameInitialChecks::InitialChecks - BEGIN_DEVICE_SELECTION - Graeme - shouldn't get here for OPERATION_LOADING_PHOTO_FROM_CLOUD");
						bDeviceRemovalError = false;	//	I'll try false here
						break;

					case OPERATION_SAVING_SAVEGAME_TO_CONSOLE :
						bDeviceRemovalError = true;
						break;

					case OPERATION_SAVING_MISSION_REPEAT_SAVEGAME_TO_CONSOLE :
					case OPERATION_SAVING_REPLAY_SAVEGAME_TO_CONSOLE :
						bDeviceRemovalError = false;
						break;

					case OPERATION_AUTOSAVING :
						bDeviceRemovalError = false;
						if (!ms_bManageStorage)						
						{	//	If trying to free space for the autosave slot then don't display any "do you want to select a device" message
							ms_bForcePlayerToChooseADevice = true;
						}
						break;

					case OPERATION_SAVING_PHOTO_TO_CLOUD :
						savegameAssertf(0, "CSavegameInitialChecks::InitialChecks - BEGIN_DEVICE_SELECTION - Graeme - shouldn't get here for OPERATION_SAVING_PHOTO_TO_CLOUD");
						bDeviceRemovalError = false;
						break;

#if __ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
					case OPERATION_UPLOADING_SAVEGAME_TO_CLOUD :
						savegameAssertf(0, "CSavegameInitialChecks::InitialChecks - BEGIN_DEVICE_SELECTION - Graeme - shouldn't get here for OPERATION_UPLOADING_SAVEGAME_TO_CLOUD");
						bDeviceRemovalError = false;
						break;
#endif	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME

					case OPERATION_CHECKING_FOR_FREE_SPACE :
						if (ms_bManageStorage)
						{	//	The first time BEGIN_DEVICE_SELECTION is reached for OPERATION_CHECKING_FOR_FREE_SPACE, ms_bManageStorage will be true.
							//	If the device is invalid already then return an error
							bDeviceRemovalError = true;
						}
						else
						{	//	ms_bManageStorage is set to false if the player wants to free space
							//	For this to have any effect, we must let the game call SAVEGAME.BeginSelectDevice again
							//	so don't return an error here
							//	Will return an error later if ms_bManageStorage is false where MEM_CARD_COMPLETE would have been returned
							//	to force the player to choose a slot again in case they've selected a different storage device
							bDeviceRemovalError = false;
							ms_bForcePlayerToChooseADevice = true;
						}
						break;

					case OPERATION_CHECKING_IF_FILE_EXISTS :
						bDeviceRemovalError = false;
						break;

					case OPERATION_DELETING_LOCAL :
					case OPERATION_DELETING_CLOUD :
						bDeviceRemovalError = true;
						break;

#if __ALLOW_LOCAL_MP_STATS_SAVES
					case OPERATION_SAVING_MPSTATS_SAVEGAME_TO_CONSOLE :
					case OPERATION_LOADING_MPSTATS_SAVEGAME_FROM_CONSOLE :
						bDeviceRemovalError = true;
						break;
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES

					case OPERATION_SAVING_LOCAL_PHOTO :
					case OPERATION_LOADING_LOCAL_PHOTO :
						bDeviceRemovalError = true;
						break;
				}

				if (bDeviceRemovalError)
				{
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_SELECTED_DEVICE_HAS_BEEN_REMOVED);
					return MEM_CARD_ERROR;
				}
				else
				{	//	Allow the player to select a storage device
					ms_InitialChecksState = DEVICE_SELECTION_DIALOG;
				}
			}
			else
			{
				ms_InitialChecksState = BEGIN_ENUMERATION;
			}
			break;

		case DEVICE_SELECTION_DIALOG :
		{
			if (CSavegameUsers::HasPlayerJustSignedOut())
			{
				CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT);
				return MEM_CARD_ERROR;
			}

			bool bPlayerWantsToSelectADevice = false;
			if (ms_bForcePlayerToChooseADevice)
			{
				bPlayerWantsToSelectADevice = true;
				ReturnSelection = true;

				ms_bForcePlayerToChooseADevice = false;
			}
			else if (CSavegameDialogScreens::HandleSaveGameDialogBox(SAVE_GAME_DIALOG_DEVICE_SELECTION, 0, &ReturnSelection))
			{
				bPlayerWantsToSelectADevice = true;
			}

			if (bPlayerWantsToSelectADevice)
			{
				CSavegameDevices::SetDisplayDeviceHasBeenRemovedMessageNextTime(false);
				if (ReturnSelection)
				{	//	start device selection
					ms_InitialChecksState = DISPLAY_DEVICE_SELECTION_UI_ONCE_NO_OTHER_UIS_ARE_SHOWING;
/*
//	GSWTODO - change forceshow to true - do I need a variable to store this or will it always be true?
//	TCRHERE
					if (!SAVEGAME.BeginSelectDevice(CSavegameUsers::GetUser(), ContentType, sTotalBufferSize, true, ms_bManageStorage) )	//	bool forceShow is true)
					{
						savegameAssertf(0, "CSavegameInitialChecks::InitialChecks - BeginSelectDevice failed");
						CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_DEVICE_SELECTION_FAILED);
						return MEM_CARD_ERROR;
					}
					ms_InitialChecksState = CHECK_DEVICE_SELECTION;
*/
				}
				else
				{	//	player doesn't want to choose a storage device
					ms_InitialChecksState = PLAYER_DOESNT_WANT_TO_SELECT_DEVICE_TRY_AGAIN;
				}
			}
		}
			break;

		case DISPLAY_DEVICE_SELECTION_UI_ONCE_NO_OTHER_UIS_ARE_SHOWING :
			if (CSavegameUsers::HasPlayerJustSignedOut())
			{
				CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT);
				return MEM_CARD_ERROR;
			}

			if (CLiveManager::IsSystemUiShowing() == false)
			{
//	GSWTODO - change forceshow to true - do I need a variable to store this or will it always be true?
//	TCRHERE
				if (!SAVEGAME.BeginSelectDevice(CSavegameUsers::GetUser(), CGenericGameStorage::GetContentType(), GetSizeOfBufferToBeSaved(), true, ms_bManageStorage) )	//	bool forceShow is true)
				{
					savegameAssertf(0, "CSavegameInitialChecks::InitialChecks - BeginSelectDevice failed");
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_DEVICE_SELECTION_FAILED);
					return MEM_CARD_ERROR;
				}
				ms_InitialChecksState = CHECK_DEVICE_SELECTION;
			}
			break;

		case PLAYER_DOESNT_WANT_TO_SELECT_DEVICE_TRY_AGAIN :
		{
			if (CSavegameUsers::HasPlayerJustSignedOut())
			{
				CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT);
				return MEM_CARD_ERROR;
			}

			bool bShouldMessageBeDisplayed = ShouldTryAgainMessageBeDisplayed();

			//	You have not selected a storage device. You will need to select a storage device in order to 
			//	save game progress. Do you want to select one now?
			ReturnSelection = false;
			if (!bShouldMessageBeDisplayed || CSavegameDialogScreens::HandleSaveGameDialogBox(SAVE_GAME_DIALOG_DONT_SELECT_DEVICE_TRY_AGAIN, 0, &ReturnSelection))
			{
				if (ReturnSelection && bShouldMessageBeDisplayed)
				{
					ms_InitialChecksState = DISPLAY_DEVICE_SELECTION_UI_ONCE_NO_OTHER_UIS_ARE_SHOWING;
/*
					//	start device selection
					//	GSWTODO - change forceshow to true - do I need a variable to store this or will it always be true?
					//	TCRHERE
					if (!SAVEGAME.BeginSelectDevice(CSavegameUsers::GetUser(), ContentType, sTotalBufferSize, true, ms_bManageStorage) )	//	bool forceShow is true)
					{
						savegameAssertf(0, "CSavegameInitialChecks::InitialChecks - BeginSelectDevice failed");
						CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_DEVICE_SELECTION_FAILED);
						return MEM_CARD_ERROR;
					}
					ms_InitialChecksState = CHECK_DEVICE_SELECTION;
*/
				}
				else
				{
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_NONE);
					ms_bPlayerChoseNotToSelectADevice = true;
					return MEM_CARD_ERROR;
				}
			}
		}
			break;

		case CHECK_DEVICE_SELECTION :
			if (SAVEGAME.CheckSelectDevice(CSavegameUsers::GetUser()))
			{	//	returns true for error or success, false for busy
				SAVEGAME.EndSelectDevice(CSavegameUsers::GetUser());

				if (CSavegameUsers::HasPlayerJustSignedOut())
				{
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT);
					return MEM_CARD_ERROR;
				}

				if (SAVEGAME.GetState(CSavegameUsers::GetUser()) == fiSaveGameState::HAD_ERROR)
				{
					if (SAVEGAME.GetError(CSavegameUsers::GetUser()) == fiSaveGame::USER_CANCELLED_DEVICE_SELECTION)
					{
						ms_InitialChecksState = PLAYER_DOESNT_WANT_TO_SELECT_DEVICE_TRY_AGAIN;
						break;
					}
					else
					{
						CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_DEVICE_SELECTION_FAILED2);
						return MEM_CARD_ERROR;
					}
				}

				if (SAVEGAME.GetError(CSavegameUsers::GetUser()) != fiSaveGame::SAVE_GAME_SUCCESS)
				{
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_DEVICE_SELECTION_FAILED3);
					return MEM_CARD_ERROR;
				}

				if (CSavegameDevices::IsDeviceValid())	//	only reason for calling this is so that ms_bAllowDeviceHasBeenRemovedMessageToBeDisplayed is set correctly
				{
					ms_InitialChecksState = BEGIN_ENUMERATION;
				}
				else
				{
					ms_InitialChecksState = BEGIN_PLAYER_SIGN_IN;	//	start from the beginning
				}
			}
			break;

		case BEGIN_ENUMERATION :
			if (CSavegameUsers::HasPlayerJustSignedOut())
			{
				CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT);
				return MEM_CARD_ERROR;
			}

			if (CSavegameDevices::HasSelectedDeviceJustBeenRemoved())
			{
				CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_SELECTED_DEVICE_HAS_BEEN_REMOVED);
				return MEM_CARD_ERROR;
			}

#if RSG_ORBIS
			if (CGenericGameStorage::GetSaveOperation() == OPERATION_CHECKING_FOR_FREE_SPACE_AT_START_OF_GAME)
			{	//	NewGameChecks have their own fiSaveGame::Content array so there's no point 
				//	enumerating the fiSaveGame::Content array inside CSavegameList in this function
				return MEM_CARD_COMPLETE;
			}
#endif

			if (CSavegameAutoload::IsPerformingAutoLoadAtStartOfGame())
			{	//	CSavegameAutoload has its own fiSaveGame::Content array so there's no point 
				//	enumerating the fiSaveGame::Content array inside CSavegameList in this function
				savegameAssertf(OPERATION_LOADING_SAVEGAME_FROM_CONSOLE == CGenericGameStorage::GetSaveOperation(), "CSavegameInitialChecks::InitialChecks - expected operation to be OPERATION_LOADING_SAVEGAME_FROM_CONSOLE during autoload");
				return MEM_CARD_COMPLETE;
			}

#if RSG_ORBIS
			if (!CSavegameList::GetSaveGamesHaveAlreadyBeenScanned())
#endif
			{
				CSavegameList::ClearSlotData();

				savegameDebugf1("CSavegameInitialChecks::InitialChecks - BeginEnumeration\n");
				savegameDebugf1("Time = %d\n",sysTimer::GetSystemMsTime());
				if (!CSavegameList::BeginEnumeration())
				{
					savegameAssertf(0, "CSavegameInitialChecks::InitialChecks - BeginEnumeration failed");
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_BEGIN_ENUMERATION_FAILED);
					return MEM_CARD_ERROR;
				}
			}
			ms_InitialChecksState = CHECK_ENUMERATION;
			//	break;

		case CHECK_ENUMERATION :
		{
			bool bEnumerationIsComplete = false;

#if RSG_ORBIS
			if (CSavegameList::GetSaveGamesHaveAlreadyBeenScanned())
			{
				bEnumerationIsComplete = true;
			}
			else
#endif
			{
				if (CSavegameList::CheckEnumeration())
				{	//	returns true for error or success, false for busy
					SAVEGAME.EndEnumeration(CSavegameUsers::GetUser());

					if (CSavegameUsers::HasPlayerJustSignedOut())
					{
						CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT);
						return MEM_CARD_ERROR;
					}

					if (CSavegameDevices::HasSelectedDeviceJustBeenRemoved())
					{
						CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_SELECTED_DEVICE_HAS_BEEN_REMOVED);
						return MEM_CARD_ERROR;
					}

					if (SAVEGAME.GetState(CSavegameUsers::GetUser()) == fiSaveGameState::HAD_ERROR)
					{
						CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_ENUMERATION_FAILED1);
						return MEM_CARD_ERROR;
					}


					savegameDebugf1("CSavegameInitialChecks::InitialChecks - CheckEnumeration has finished\n");
#if !__NO_OUTPUT
					savegameDisplayf("CSavegameInitialChecks::InitialChecks - List of savegames before sorting");
					CSavegameList::PrintFilenames();
#endif	//	!__NO_OUTPUT
					savegameDebugf1("ms_NumberOfSaveGameSlots = %d Time = %d\n", CSavegameList::GetNumberOfSaveGameSlots(), sysTimer::GetSystemMsTime());

	// GSWTODO - what other errors can be returned?
					if ( (SAVEGAME.GetError(CSavegameUsers::GetUser()) != fiSaveGame::SAVE_GAME_SUCCESS) && 
						(SAVEGAME.GetError(CSavegameUsers::GetUser()) != fiSaveGame::NO_FILES_EXIST) )
					{	//	don't treat "no files" as an error
						CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_ENUMERATION_FAILED2);
						return MEM_CARD_ERROR;
					}

					bEnumerationIsComplete = true;
				}
			}

			if (bEnumerationIsComplete)
			{
				ms_InitialChecksState = SORT_SAVE_GAME_SLOTS;
			}
		}
			break;

// *************************************************************************************
		case SORT_SAVE_GAME_SLOTS :
		{
			bool bSortingIsComplete = false;

			if (CSavegameUsers::HasPlayerJustSignedOut())
			{
				CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT);
				return MEM_CARD_ERROR;
			}

			if (CSavegameDevices::HasSelectedDeviceJustBeenRemoved())
			{
				CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_SELECTED_DEVICE_HAS_BEEN_REMOVED);
				return MEM_CARD_ERROR;
			}

#if RSG_ORBIS
			if (CSavegameList::GetSaveGamesHaveAlreadyBeenScanned())
			{
				bSortingIsComplete = true;
			}
			else
#endif
			{
				if (CSavegameList::SortSaveGameSlots() == false)
				{
				// GSWTODO - set errors in SortSaveGameSlots or just a general error here
					return MEM_CARD_ERROR;
				}
				else
				{
					bSortingIsComplete = true;
				}
			}

			if (bSortingIsComplete)
			{
				switch (CGenericGameStorage::GetSaveOperation())
				{
					case OPERATION_CHECKING_IF_FILE_EXISTS :
					case OPERATION_DELETING_LOCAL :
					case OPERATION_DELETING_CLOUD :
					case OPERATION_ENUMERATING_PHOTOS :
						CSavegameDialogScreens::SetMessageAsProcessing(false);
						return MEM_CARD_COMPLETE;
//						break;

					default :
						ms_InitialChecksState = GET_FILE_TIMES;
						CSavegameList::ResetFileTimeScan();
						break;
				}
			}
		}
			break;

		case GET_FILE_TIMES :
		{
			if (CSavegameUsers::HasPlayerJustSignedOut())
			{
				CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT);
				return MEM_CARD_ERROR;
			}

			if (CSavegameDevices::HasSelectedDeviceJustBeenRemoved())
			{
				CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_SELECTED_DEVICE_HAS_BEEN_REMOVED);
				return MEM_CARD_ERROR;
			}

			switch (CSavegameList::GetFileTimes())
			{
				case MEM_CARD_COMPLETE :
				{
					switch (CGenericGameStorage::GetSaveOperation())
					{
					case OPERATION_SAVING_SAVEGAME_TO_CONSOLE :
					case OPERATION_SAVING_MISSION_REPEAT_SAVEGAME_TO_CONSOLE :
					case OPERATION_SAVING_REPLAY_SAVEGAME_TO_CONSOLE :
					case OPERATION_AUTOSAVING :
					case OPERATION_CHECKING_FOR_FREE_SPACE:
#if __ALLOW_LOCAL_MP_STATS_SAVES
					case OPERATION_SAVING_MPSTATS_SAVEGAME_TO_CONSOLE :
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES
					case OPERATION_SAVING_LOCAL_PHOTO :
						ms_InitialChecksState = BEGIN_FREE_SPACE;
						break;

					case OPERATION_SCANNING_CONSOLE_FOR_LOADING_SAVEGAMES :
					case OPERATION_SCANNING_CONSOLE_FOR_SAVING_SAVEGAMES :
					case OPERATION_LOADING_SAVEGAME_FROM_CONSOLE :
					case OPERATION_LOADING_REPLAY_SAVEGAME_FROM_CONSOLE :
#if __ALLOW_LOCAL_MP_STATS_SAVES
					case OPERATION_LOADING_MPSTATS_SAVEGAME_FROM_CONSOLE :
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES
					case OPERATION_LOADING_LOCAL_PHOTO :
					case OPERATION_ENUMERATING_SAVEGAMES :
						CSavegameDialogScreens::SetMessageAsProcessing(false);
						return MEM_CARD_COMPLETE;
//						break;

					case OPERATION_LOADING_PHOTO_FROM_CLOUD :
						savegameAssertf(0, "CSavegameInitialChecks::InitialChecks - GET_FILE_TIMES - Graeme - shouldn't get here for OPERATION_LOADING_PHOTO_FROM_CLOUD");
						break;

					case OPERATION_LOADING_MPSTATS_FROM_CLOUD:
						savegameAssertf(0, "CSavegameInitialChecks::InitialChecks - GET_FILE_TIMES - Graeme - just checking if it ever gets here for OPERATION_LOADING_MPSTATS_FROM_CLOUD");
						break;

					case OPERATION_SAVING_MPSTATS_TO_CLOUD:
						savegameAssertf(0, "CSavegameInitialChecks::InitialChecks - GET_FILE_TIMES - Graeme - just checking if it ever gets here for OPERATION_SAVING_MPSTATS_TO_CLOUD");
						break;

					case OPERATION_SAVING_PHOTO_TO_CLOUD :
						savegameAssertf(0, "CSavegameInitialChecks::InitialChecks - GET_FILE_TIMES - Graeme - shouldn't get here for OPERATION_SAVING_PHOTO_TO_CLOUD");
						break;

#if __ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
					case OPERATION_UPLOADING_SAVEGAME_TO_CLOUD :
						savegameAssertf(0, "CSavegameInitialChecks::InitialChecks - GET_FILE_TIMES - Graeme - shouldn't get here for OPERATION_UPLOADING_SAVEGAME_TO_CLOUD");
						break;
#endif	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME

					case OPERATION_NONE :
#if RSG_ORBIS
					case OPERATION_CHECKING_FOR_FREE_SPACE_AT_START_OF_GAME :
#endif
					case OPERATION_CHECKING_IF_FILE_EXISTS :
					case OPERATION_DELETING_LOCAL :
					case OPERATION_DELETING_CLOUD :
					case OPERATION_ENUMERATING_PHOTOS :
						savegameAssertf(0, "CSavegameInitialChecks::InitialChecks - GET_FILE_TIMES - unexpected value for ms_Operation");
						break;
					}
				}
					break;

				case MEM_CARD_BUSY :
					//	Do nothing
					break;

				case MEM_CARD_ERROR :
					//	CSavegameDialogScreens::SetSaveGameError();
					savegameAssertf(0, "CSavegameInitialChecks::InitialChecks - CSavegameList::GetFileTimes() failed");
					return MEM_CARD_ERROR;
//					break;
			}
		}
			break;

		case BEGIN_FREE_SPACE :
#if !RSG_ORBIS
			if (CSavegameFreeSpaceCheck::CheckForFreeSpaceForCurrentFilename(true, GetSizeOfBufferToBeSaved(), SpaceRequired) == MEM_CARD_ERROR)
			{
				return MEM_CARD_ERROR;
			}

			ms_InitialChecksState = CHECK_FREE_SPACE;
//			break;

#endif

		case CHECK_FREE_SPACE :
#if !RSG_ORBIS
		{
			MemoryCardError FreeSpaceResult = CSavegameFreeSpaceCheck::CheckForFreeSpaceForCurrentFilename(false, GetSizeOfBufferToBeSaved(), SpaceRequired);

			if (FreeSpaceResult == MEM_CARD_ERROR)
			{
				return MEM_CARD_ERROR;
			}
			else if (FreeSpaceResult == MEM_CARD_COMPLETE)
			{
				if (SpaceRequired >= 0)
				{
					if (!ms_bManageStorage)
					{	//	If ms_bManageStorage is false then the player could have changed device due to lack of space on the first device chosen
						if (CGenericGameStorage::GetSaveOperation() == OPERATION_AUTOSAVING)
						{	//	The player cannot choose the autosave slot so just allow them
							//	to save in the autosave slot on whichever device has been selected
							if (CGenericGameStorage::ShouldWeDisplayTheMessageAboutOverwritingTheAutosaveSlot() )
							{	//	Don't unpause here as we will need to immediately display the autosave overwrite message

							}
							else
							{
								CSavegameDialogScreens::SetMessageAsProcessing(false);
							}
							return MEM_CARD_COMPLETE;
						}
						//	Return an error so that the player has to choose the slot again
						return MEM_CARD_ERROR;
					}
					else
					{
						if ( (CGenericGameStorage::GetSaveOperation() == OPERATION_AUTOSAVING) 
							&& CGenericGameStorage::ShouldWeDisplayTheMessageAboutOverwritingTheAutosaveSlot() )
						{	//	Don't unpause here as we will need to immediately display the autosave overwrite message

						}
						else
						{
							CSavegameDialogScreens::SetMessageAsProcessing(false);
						}
						return MEM_CARD_COMPLETE;
					}
				}
				else
				{
					ms_InitialChecksState = FREE_SPACE_DIALOG;
				}
			}
		}
#else		
		return MEM_CARD_COMPLETE;
#endif

		break;

		case FREE_SPACE_DIALOG :
		{
			if (CSavegameUsers::HasPlayerJustSignedOut())
			{
				CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT);
				return MEM_CARD_ERROR;
			}

			if (CSavegameDevices::HasSelectedDeviceJustBeenRemoved())
			{
				CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_SELECTED_DEVICE_HAS_BEEN_REMOVED);
				return MEM_CARD_ERROR;
			}

			if (CSavegameDialogScreens::HandleSaveGameDialogBox(SAVE_GAME_DIALOG_FREE_SPACE_CHECK, -SpaceRequired, &ReturnSelection))
			{
				if (ReturnSelection)
				{
#if __PPU
/*		We can't afford the memory container for BeginDeleteFromList
					if (!SAVEGAME.BeginDeleteFromList(CSavegameUsers::GetUser()))
					{	//	start delete from list
						savegameAssertf(0, "CSavegameInitialChecks::InitialChecks - BeginDeleteFromList failed");
						CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_DELETE_FROM_LIST_FAILED);
						return MEM_CARD_ERROR;
					}

					ms_InitialChecksState = CHECK_FILE_DELETION;
*/
					//	Try this so that the player can continue the game and attempt to save in to another slot
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_NONE);
					return MEM_CARD_ERROR;
#else
					ms_bManageStorage = false;
					//	Force device selection again
					CSavegameDevices::SetAllowDeviceHasBeenRemovedMessageToBeDisplayed(false);
					CSavegameDevices::SetDeviceIsInvalid(false);

					ms_InitialChecksState = BEGIN_PLAYER_SIGN_IN;	//	start from the beginning
#endif
				}
				else
				{	//	player doesn't want to free space so give up now
//					SetFailureErrorMessage(ms_Operation, CGenericGameStorage::GetPerformingAnAutoSave());
//					maybe just call SetSaveGameError here
//					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_PLAYER_DOESNT_WANT_TO_FREE_SPACE);
#if __PPU
					savegameAssertf(0, "CSavegameInitialChecks::InitialChecks - should only be able to say OK to PS3 No Free Space dialog");
#endif
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_NONE);
					return MEM_CARD_ERROR;
				}
			}
		}
			break;
// *************************************************************************************
/*		We can't afford the memory container for CheckDeleteFromList
		case CHECK_FILE_DELETION :
#if __PPU
			if (SAVEGAME.CheckDeleteFromList(CSavegameUsers::GetUser()))
			{	//	GSWTODO - still need to deal with errors here
				SAVEGAME.EndDeleteFromList(CSavegameUsers::GetUser());

// These might not actually be required on PS3 since you can't remove the storage device
//	and I don't think you can sign out either
				if (CSavegameUsers::HasPlayerJustSignedOut())
				{
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT);
					return MEM_CARD_ERROR;
				}

				if (CSavegameDevices::HasSelectedDeviceJustBeenRemoved())
				{
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_SELECTED_DEVICE_HAS_BEEN_REMOVED);
					return MEM_CARD_ERROR;
				}
// End of These might not actually be required on PS3 since you can't remove the storage device
//	and I don't think you can sign out either

				ms_InitialChecksState = BEGIN_PLAYER_SIGN_IN;	//	start from the beginning
			}
#else
			savegameAssertf(0, "CSavegameInitialChecks::InitialChecks - CHECK_FILE_DELETION - should only happen for PS3");
#endif
			break;
*/
		default :
			savegameAssertf(0, "CSavegameInitialChecks::InitialChecks - unexpected value for ms_InitialChecksState");
			break;
	}

	return MEM_CARD_BUSY;
}

