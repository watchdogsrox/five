
// Rage headers
#include "system/userlist.h"

// Framework headers
//	#include "fwsys/timer.h"

// Game headers
#include "camera/CamInterface.h"
#include "Core/gamesessionstatemachine.h"
#include "frontend/loadingscreens.h"
#include "frontend/PauseMenu.h"
#include "frontend/WarningScreen.h"
#include "network/Live/livemanager.h"
#include "Peds/ped.h"
#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/savegame_autoload.h"
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/savegame_initial_checks.h"
#include "SaveLoad/savegame_list.h"
#include "SaveLoad/savegame_load.h"
#include "SaveLoad/savegame_messages.h"
#include "SaveLoad/savegame_photo_manager.h"
#include "SaveLoad/savegame_users_and_devices.h"
#include "scene/world/GameWorld.h"
#include "Text/Messages.h"
#include "Text/TextConversion.h"
#include "network/NetworkInterface.h"
#include "network/Live/NetworkProfileStats.h"

#include "control/replay/replay.h"

SAVEGAME_OPTIMISATIONS()

#if __PPU
const char *pTextLabelOfLoadingWarning = "SG_LOADING_PS3";
const char *pTextLabelOfLoadingSucceededTrophy = "SG_LOAD_SUC_T";
const char *pTextLabelOfLoadingSucceededNoTrophy = "SG_LOAD_SUC_NT";
const char *pTextLabelOfDeletingWarning = "SG_DELETING_PS3";
#elif __WIN32PC
const char *pTextLabelOfLoadingWarning = "SG_LOADING_PC";
const char *pTextLabelOfLoadingSucceeded = "SG_LOAD_SUC";
const char *pTextLabelOfDeletingWarning = "SG_DELETING_PC";
#else
const char *pTextLabelOfLoadingWarning = "SG_LOADING";
const char *pTextLabelOfLoadingSucceeded = "SG_LOAD_SUC";
const char *pTextLabelOfDeletingWarning = "SG_DELETING";
#endif

#if GTA_REPLAY
const char *pTextLabelOfSavingReplayWarning = "SG_REPLAY";
const char *pTextLabelOfReplaySavingVideoWarning = "SG_PROJECT";
const char *pTextLabelOfReplaySavingThumbWarning = "MO_THUMBNAILED";
#endif // GTA_REPLAY

sysTimer CSavingMessage::m_TimeDisplayedFor;
bool CSavingMessage::m_bDisplaySavingMessage = false;
CSavingMessage::eTypeOfMessage CSavingMessage::m_eTypeOfMessage = STORAGE_DEVICE_MESSAGE_INVALID;


SaveGameDialogCode CSavegameDialogScreens::ms_MostRecentErrorCode = SAVE_GAME_DIALOG_NONE;

s32 CLoadConfirmationMessage::ms_SlotForPlayerConfirmation = -1;
CLoadConfirmationMessage::eLoadConfirmationType CLoadConfirmationMessage::ms_TypeOfPlayerConfirmationCheck = LOAD_CONFIRMATION_NONE;
CLoadConfirmationMessage::eLoadMessageType CLoadConfirmationMessage::ms_LoadMessageType = LOAD_MESSAGE_LOAD;
bool CLoadConfirmationMessage::ms_bSlotIsDamaged = false;

s32 CSaveConfirmationMessage::ms_SlotForPlayerConfirmation = -1;
CSaveConfirmationMessage::eSaveConfirmationType CSaveConfirmationMessage::ms_TypeOfPlayerConfirmationCheck = SAVE_CONFIRMATION_NONE;
CSaveConfirmationMessage::eSaveMessageType CSaveConfirmationMessage::ms_SaveMessageType = SAVE_MESSAGE_SAVE;


char CSavegameDialogScreens::DialogBoxHeading[TEXT_KEY_SIZE];

u32 CSavegameDialogScreens::ms_NumberOfFramesToRenderBlackScreen = 0;

u32 CSavegameDialogScreens::ms_NumberOfFramesThatTheMessageHasBeenDisplayingFor = 0;

bool CSavegameDialogScreens::ms_bAllowSetMessageAsProcessingToBeCalled = false;

bool CSavegameDialogScreens::ms_bMessageIsProcessingThisFrame = false;
bool CSavegameDialogScreens::ms_bMessageWasProcessedLastFrame = false;


bool CSavegameDialogScreens::sm_bStartedUserPause = false;
bool CSavegameDialogScreens::sm_bScriptWasPaused = false;

bool CSavegameDialogScreens::sm_bLoadingScreensHaveBeenSuspended = false;


static const float MAX_TIME_TO_DISPLAY_FOR = 3000.0f;


void CSavingMessage::Clear()
{
#if !__NO_OUTPUT
	if (m_bDisplaySavingMessage)
	{
//		sysStack::PrintStackTrace();
		savegameDisplayf("CSavingMessage::Clear has been called");
	}
#endif	//	!__NO_OUTPUT
	m_bDisplaySavingMessage = false;
	m_TimeDisplayedFor.Reset();
	m_eTypeOfMessage = STORAGE_DEVICE_MESSAGE_INVALID;

	if (CSavingGameMessage::IsSavingMessageActive())
	{
		CSavingGameMessage::Clear();
	}
}

void CSavingMessage::BeginDisplaying(eTypeOfMessage MessageType)
{
#if !__NO_OUTPUT
	if (!m_bDisplaySavingMessage)
	{
//		sysStack::PrintStackTrace();
		savegameDisplayf("CSavingMessage::BeginDisplaying has been called");
	}
#endif	//	!__NO_OUTPUT
	Assertf(MessageType != STORAGE_DEVICE_MESSAGE_INVALID, "CSavingMessage::BeginDisplaying - can't call this with STORAGE_DEVICE_MESSAGE_INVALID");
	m_bDisplaySavingMessage = true;
	m_TimeDisplayedFor.Reset();
	m_eTypeOfMessage = MessageType;
}

bool CSavingMessage::ShouldDisplaySavingSpinner()
{
	if (m_bDisplaySavingMessage)
	{
		switch (m_eTypeOfMessage)
		{
			case STORAGE_DEVICE_MESSAGE_SAVING :
			case STORAGE_DEVICE_MESSAGE_LOCAL_DELETING :
#if GTA_REPLAY
			case STORAGE_DEVICE_MESSAGE_SAVING_REPLAY:
			case STORAGE_DEVICE_MESSAGE_SAVING_REPLAY_PROJECT:
			case STORAGE_DEVICE_MESSAGE_SAVING_REPLAY_PROJECT_NO_TEXT:
			case STORAGE_DEVICE_MESSAGE_SAVING_REPLAY_THUMBNAIL:
#endif // GTA_REPLAY
				return true;

			default :
				return false;
		}
	}
	return false;
}

void CSavingMessage::DisplaySavingMessage()
{
	static char StorageDeviceMessageSaving[] = "No Text for Local Saves"; 
	static char StorageDeviceMessageCloudSaving[] = "No Text for Cloud Saves";

	if (m_bDisplaySavingMessage)
	{
		const char *pString = NULL;
		CSavingGameMessage::eICON_STYLE iconToDisplay = CSavingGameMessage::SAVEGAME_ICON_STYLE_NONE;

		switch (m_eTypeOfMessage)
		{
			case STORAGE_DEVICE_MESSAGE_INVALID :
				Assertf(0, "CSavingMessage::DisplaySavingMessage - message type should never be STORAGE_DEVICE_MESSAGE_INVALID when m_bDisplaySavingMessage is TRUE");
				break;

			case STORAGE_DEVICE_MESSAGE_SAVING :
				pString = StorageDeviceMessageSaving;

#if __BANK
				if (CGenericGameStorage::ms_bDisplayLoadingSpinnerWhenSaving)
				{
					iconToDisplay = CSavingGameMessage::SAVEGAME_ICON_STYLE_LOADING;
				}
				else
#endif	//	__BANK
				{
					iconToDisplay = CSavingGameMessage::SAVEGAME_ICON_STYLE_SAVING;
				}
				break;

			case STORAGE_DEVICE_MESSAGE_LOADING :
				pString = TheText.Get(pTextLabelOfLoadingWarning);
				iconToDisplay = CSavingGameMessage::SAVEGAME_ICON_STYLE_LOADING;
				break;

			case STORAGE_DEVICE_MESSAGE_LOAD_SUCCESSFUL :
#if __PPU
				pString = CLiveManager::GetAchMgr().GetTrophiesEnabled()?TheText.Get(pTextLabelOfLoadingSucceededTrophy):TheText.Get(pTextLabelOfLoadingSucceededNoTrophy);
#else
				pString = TheText.Get(pTextLabelOfLoadingSucceeded);
#endif	//	__PPU
				break;

			case STORAGE_DEVICE_MESSAGE_CLOUD_SAVING :
				pString = StorageDeviceMessageCloudSaving;		//	The string isn't actually displayed now. I'll just set it to something in case it ever comes back.
				iconToDisplay = CSavingGameMessage::SAVEGAME_ICON_STYLE_CLOUD_WORKING;
				break;

			case STORAGE_DEVICE_MESSAGE_CLOUD_LOADING :
				pString = TheText.Get(pTextLabelOfLoadingWarning);		//	The string isn't actually displayed now. I'll just set it to something in case it ever comes back.
				iconToDisplay = CSavingGameMessage::SAVEGAME_ICON_STYLE_CLOUD_WORKING;
				break;

			case STORAGE_DEVICE_MESSAGE_LOCAL_DELETING :
			case STORAGE_DEVICE_MESSAGE_CLOUD_DELETING :
				pString = TheText.Get(pTextLabelOfDeletingWarning);
				if (m_eTypeOfMessage == STORAGE_DEVICE_MESSAGE_CLOUD_DELETING)
				{
					iconToDisplay = CSavingGameMessage::SAVEGAME_ICON_STYLE_CLOUD_WORKING;
				}
				else
				{
					iconToDisplay = CSavingGameMessage::SAVEGAME_ICON_STYLE_SAVING;
				}
				break;

#if GTA_REPLAY
			case STORAGE_DEVICE_MESSAGE_SAVING_REPLAY:
				pString = TheText.Get(pTextLabelOfSavingReplayWarning);
				iconToDisplay = CSavingGameMessage::SAVEGAME_ICON_STYLE_SAVING;
				break;
			case STORAGE_DEVICE_MESSAGE_SAVING_REPLAY_PROJECT:
				pString = TheText.Get(pTextLabelOfReplaySavingVideoWarning);
				iconToDisplay = CSavingGameMessage::SAVEGAME_ICON_STYLE_SAVING;
				break;
			case STORAGE_DEVICE_MESSAGE_SAVING_REPLAY_PROJECT_NO_TEXT:
				pString = "";
				iconToDisplay = CSavingGameMessage::SAVEGAME_ICON_STYLE_SAVING_NO_MESSAGE;
				break;
			case STORAGE_DEVICE_MESSAGE_SAVING_REPLAY_THUMBNAIL:
				pString = TheText.Get(pTextLabelOfReplaySavingThumbWarning);
				iconToDisplay = CSavingGameMessage::SAVEGAME_ICON_STYLE_SAVING;
				break;

#endif // GTA_REPLAY
		}
			
		Assertf(pString, "CSavingMessage::DisplaySavingMessage - Can't find SG_SAVING in the text file");
		
		CSavingGameMessage::SetMessageText(pString, iconToDisplay);

		savegameDebugf2("CSavingMessage::DisplaySavingMessage - TimeToDisplay = %.1f\n", m_TimeDisplayedFor.GetMsTime());

		bool bClearMessage = false;
		switch (m_eTypeOfMessage)
		{
			case STORAGE_DEVICE_MESSAGE_INVALID :
				break;

			case STORAGE_DEVICE_MESSAGE_SAVING :
			case STORAGE_DEVICE_MESSAGE_CLOUD_SAVING :
				if (m_TimeDisplayedFor.GetMsTime() >= MAX_TIME_TO_DISPLAY_FOR)
				{
					bool isMultiplayerProfileFlushing = (NetworkInterface::IsGameInProgress() 
															&& (CLiveManager::GetProfileStatsMgr().PendingFlush() || CLiveManager::GetProfileStatsMgr().PendingMultiplayerFlushRequest()));

					if (!CGenericGameStorage::IsSavegameSaveInProgress()
						&& !CGenericGameStorage::IsMpStatsSaveInProgress()
						&& !isMultiplayerProfileFlushing
						&& !CGenericGameStorage::IsPhotoSaveInProgress()
#if __ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
						&& !CGenericGameStorage::IsUploadingSavegameToCloud()
#endif	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
						)
					{
						bClearMessage = true;
					}
				}
				break;

			case STORAGE_DEVICE_MESSAGE_LOADING :
			case STORAGE_DEVICE_MESSAGE_CLOUD_LOADING :
				if (!CGenericGameStorage::IsSavegameLoadInProgress()
					&& !CGenericGameStorage::IsMpStatsLoadInProgress()
					&& !CGenericGameStorage::IsPhotoLoadInProgress())
				{	//	(m_TimeDisplayedFor >= 3000) && 
					bClearMessage = true;
				}
				break;

			case STORAGE_DEVICE_MESSAGE_LOAD_SUCCESSFUL :
#if __PPU
				if (m_TimeDisplayedFor.GetMsTime() >= (CLiveManager::GetAchMgr().GetTrophiesEnabled()?4000.0f:6000.0f))
#else
				if (m_TimeDisplayedFor.GetMsTime() >= 3000.0f)
#endif
				{
					bClearMessage = true;
				}
				break;

			case STORAGE_DEVICE_MESSAGE_LOCAL_DELETING :
				if (!CGenericGameStorage::IsLocalDeleteInProgress())
				{	//	(m_TimeDisplayedFor >= 3000) && 
					bClearMessage = true;
				}
				break;

			case STORAGE_DEVICE_MESSAGE_CLOUD_DELETING :
				if (!CGenericGameStorage::IsCloudDeleteInProgress())
				{	//	(m_TimeDisplayedFor >= 3000) && 
					bClearMessage = true;
				}
				break;

#if GTA_REPLAY
			case STORAGE_DEVICE_MESSAGE_SAVING_REPLAY:
			case STORAGE_DEVICE_MESSAGE_SAVING_REPLAY_PROJECT:
			case STORAGE_DEVICE_MESSAGE_SAVING_REPLAY_PROJECT_NO_TEXT:
			case STORAGE_DEVICE_MESSAGE_SAVING_REPLAY_THUMBNAIL:
				if ( (m_TimeDisplayedFor.GetMsTime() >= MAX_TIME_TO_DISPLAY_FOR) && (!CReplayMgr::IsPerformingFileOp()) )
				{
					bClearMessage = true;
				}
				break;
#endif // GTA_REPLAY
		}

		if (bClearMessage)
		{
			savegameDisplayf("CSavingMessage::DisplaySavingMessage - Clearing SavingMessage - TimeToDisplay = %.1f", m_TimeDisplayedFor.GetMsTime());
			m_bDisplaySavingMessage = false;

			CSavingGameMessage::Clear();
		}
	}
}

bool CSavingMessage::HasLoadingMessageDisplayedForLongEnough()
{
	//	Do I need to deal with STORAGE_DEVICE_MESSAGE_CLOUD_SAVING and/or STORAGE_DEVICE_MESSAGE_CLOUD_LOADING here?
	if (m_bDisplaySavingMessage && (m_eTypeOfMessage == STORAGE_DEVICE_MESSAGE_LOADING))
	{
		if (m_TimeDisplayedFor.GetMsTime() >= MAX_TIME_TO_DISPLAY_FOR)
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{	//	For safety, return true if the loading message is not displaying at all
		return true;
	}
}


void CSavegameDialogScreens::Init(unsigned initMode)
{
	if(initMode == INIT_CORE)
	{
	}
	else if(initMode == INIT_AFTER_MAP_LOADED)
	{
		ms_MostRecentErrorCode = SAVE_GAME_DIALOG_NONE;

// Should I be resetting CLoadConfirmationMessage and CSaveConfirmationMessage after the map has loaded?
//	    SlotForPlayerConfirmation = -1;
//	    TypeOfPlayerConfirmationCheck = PLAYER_CONFIRMATION_NONE;
//	    StatusOfPlayerConfirmation = 0;

		strncpy(DialogBoxHeading, "SG_HDNG", TEXT_KEY_SIZE);	//	SG_HDG_SAVE

	//	ms_bHasBeenPaused = false;
//		SetMessageAsProcessing(false);
		ms_bAllowSetMessageAsProcessingToBeCalled = false;
		ms_bMessageIsProcessingThisFrame = false;
//		ms_bMessageWasProcessedLastFrame = false;	//	Graeme - I think it might be safer if I don't reset this here. Despite being in INIT_AFTER_MAP_LOADED, it's called whenever a new session starts.
	}
}

void CSavegameDialogScreens::BeforeUpdatingTheSavegameQueue()
{
	ms_bAllowSetMessageAsProcessingToBeCalled = true;

	ms_bMessageIsProcessingThisFrame = false;
}



bool CSavegameDialogScreens::ShouldRenderBlackScreen()
{
	if (ms_NumberOfFramesToRenderBlackScreen > 0)
	{
		savegameDisplayf("CSavegameDialogScreens::ShouldRenderBlackScreen - returning true. About to decrement ms_NumberOfFramesToRenderBlackScreen from %u", ms_NumberOfFramesToRenderBlackScreen);
		ms_NumberOfFramesToRenderBlackScreen--;
		return true;
	}

	return false;
}

void CSavegameDialogScreens::RenderBlackScreen()
{
	GRCDEVICE.Clear(true, Color32(0x00000000), false, 0, false, 0);
}


void CSavegameDialogScreens::SetSaveGameError(SaveGameDialogCode NewErrorCode)
{
	Assertf(ms_MostRecentErrorCode == SAVE_GAME_DIALOG_NONE, "CSavegameDialogScreens::SetSaveGameError - trying to overwrite a save game error code");
	ms_MostRecentErrorCode = NewErrorCode;
}


bool CSavegameDialogScreens::HandleSaveGameErrorCode()
{
	bool ReturnSelection = false;

//	if (IsPerformingAutoLoadAtStartOfGame())
//	{
//		ms_MostRecentErrorCode = SAVE_GAME_DIALOG_NONE;
//		return true;
//	}
	if (HandleSaveGameDialogBox(ms_MostRecentErrorCode, 0, &ReturnSelection))
	{	//	number of buttons will be 1 for all of the error codes
		ms_MostRecentErrorCode = SAVE_GAME_DIALOG_NONE;
		return true;
	}

	return false;
}

void CSavegameDialogScreens::SetDialogBoxHeading(const char *pNewHeading)
{
	strncpy(DialogBoxHeading, pNewHeading, TEXT_KEY_SIZE);
}


void CSavegameDialogScreens::SetMessageAsProcessing(bool bProcessing)
{
	//	Special case for autoloading at start of game. It doesn't use the savegame queue
	//	AfterUpdatingTheSavegameQueue() won't be getting called during autoload so the player control won't be getting disabled and the game won't be paused. Not sure if that matters.
	if (savegameVerifyf(ms_bAllowSetMessageAsProcessingToBeCalled || CSavegameAutoload::IsPerformingAutoLoadAtStartOfGame() || (CGameSessionStateMachine::GetGameSessionState() == CGameSessionStateMachine::HANDLE_LOADED_SAVE_GAME), "CSavegameDialogScreens::SetMessageAsProcessing - can only call this within the update of the savegame queue"))
	{
		ms_bMessageIsProcessingThisFrame = bProcessing;
	}
}


void CSavegameDialogScreens::AfterUpdatingTheSavegameQueue()
{
	if (!CPauseMenu::IsActive())
	{
		if (ms_bMessageIsProcessingThisFrame)
		{
			if (!ms_bMessageWasProcessedLastFrame)
			{
				if (CGameWorld::FindLocalPlayer() && CGameWorld::FindLocalPlayer()->GetPlayerInfo())
				{
					CGameWorld::FindLocalPlayer()->GetPlayerInfo()->DisableControlsFrontend();
					savegameDisplayf("CSavegameDialogScreens::AfterUpdatingTheSavegameQueue - has called DisableControlsFrontend()");
				}

				if ( (CLoadingScreens::AreActive()) && (!CLoadingScreens::AreSuspended()) )
				{
					CLoadingScreens::Suspend();
					sm_bLoadingScreensHaveBeenSuspended = true;
				}
				else
				{
					sm_bLoadingScreensHaveBeenSuspended = false;
				}

				sm_bScriptWasPaused = false;
				sm_bStartedUserPause = false;

				ms_NumberOfFramesThatTheMessageHasBeenDisplayingFor = 0;
			}

			if (ms_NumberOfFramesThatTheMessageHasBeenDisplayingFor < 20)
			{	//	I'm only interested in the first few frames that the message is displayed
				ms_NumberOfFramesThatTheMessageHasBeenDisplayingFor++;
			}

			if (ms_NumberOfFramesThatTheMessageHasBeenDisplayingFor == CGenericGameStorage::ms_NumberOfFramesToDisplayWarningMessageForBeforePausingTheGame)
			{
				if(!NetworkInterface::IsNetworkOpen())
				{
					//Change Script Pause
					if (fwTimer::IsScriptPaused())
					{
						fwTimer::EndScriptPause();
						sm_bScriptWasPaused = true;
						savegameDisplayf("CSavegameDialogScreens::AfterUpdatingTheSavegameQueue - has called EndScriptPause()");
					}

					// Graeme - should I be checking fwTimer::IsUserPaused()
					//	and only calling fwTimer::EndUserPause() later if it's not already paused now

					//This is needed to restore the timers to what they were before the front end opened.
					//fwTimer::StoreCurrentTime();
					//Change User Pause
					fwTimer::StartUserPause();
					sm_bStartedUserPause = true;
					savegameDisplayf("CSavegameDialogScreens::AfterUpdatingTheSavegameQueue - has called StartUserPause()");
				}
			}
		}
		else //	if (!ms_bMessageIsProcessingThisFrame)
		{
			if (ms_bMessageWasProcessedLastFrame)
			{
				if (CGameWorld::FindLocalPlayer() && CGameWorld::FindLocalPlayer()->GetPlayerInfo())
				{
					CGameWorld::FindLocalPlayer()->GetPlayerInfo()->EnableControlsFrontend();
					savegameDisplayf("CSavegameDialogScreens::AfterUpdatingTheSavegameQueue - has called EnableControlsFrontend()");
				}

				//Check for User Pause
				if(sm_bStartedUserPause)
				{
					//This is needed to restore the timers when the frontend closes.
					//fwTimer::RestoreCurrentTime(); 
					fwTimer::EndUserPause();
					sm_bStartedUserPause = false;
					savegameDisplayf("CSavegameDialogScreens::AfterUpdatingTheSavegameQueue - has called EndUserPause()");
				}

				//Check for Script Pause
				if (sm_bScriptWasPaused)
				{
					fwTimer::StartScriptPause();
					sm_bScriptWasPaused = false;
					savegameDisplayf("CSavegameDialogScreens::AfterUpdatingTheSavegameQueue - has called StartScriptPause()");
				}

				if (sm_bLoadingScreensHaveBeenSuspended && (CLoadingScreens::AreSuspended()) )
				{
					CLoadingScreens::Resume();
				}
				sm_bLoadingScreensHaveBeenSuspended = false;
			}
		}
	}

	ms_bMessageWasProcessedLastFrame = ms_bMessageIsProcessingThisFrame;

	ms_bAllowSetMessageAsProcessingToBeCalled = false;
}

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
#if __NO_OUTPUT
#define SAVEGAME_MESSAGES_CHECK_TEXT_LABEL_EXISTS(textLabel)
#else
#define SAVEGAME_MESSAGES_CHECK_TEXT_LABEL_EXISTS(textLabel) \
if (!TheText.DoesTextLabelExist(textLabel))\
{\
	savegameErrorf("Can't find Key %s in the text file", textLabel);\
	savegameAssertf(0, "Can't find Key %s in the text file", textLabel);\
}
#endif
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES


bool CSavegameDialogScreens::HandleSaveGameDialogBox(u32 MessageToDisplay, const int NumberToInsertInMessage, bool *pReturnSelection)
{
	if( CLoadingScreens::IsDisplayingLegalScreen())
		return false;


	char cSaveGameName[SAVE_GAME_MAX_DISPLAY_NAME_LENGTH];
	char cSaveGameDate[MAX_MENU_ITEM_CHAR_LENGTH];

#if __REMOVE_TIME_FROM_DATE_TIME_STRINGS_IN_EXPORT_MESSAGES
	int LengthOfSaveGameDate = 0;
#endif // __REMOVE_TIME_FROM_DATE_TIME_STRINGS_IN_EXPORT_MESSAGES

	bool bSlotIsEmpty = false;
	bool bSlotIsDamaged = false;

	int NumberOfButtons = 1;
	bool bDisplayingTheRemovedDeviceMessage = false;

	SetMessageAsProcessing(true);

//	#define FE_WARNING_NONE		(1<<0)
//	#define FE_WARNING_ACCEPT	(1<<1)
//	#define FE_WARNING_BACK		(1<<2)
//	#define FE_WARNING_RETRY	(1<<3)

	switch (MessageToDisplay)
	{
		case SAVE_GAME_DIALOG_SIGN_IN :
		{
			//	You are not signed in. Do you want to sign in now?
			const char *pTextLabel = "SG_SIGNIN";
			switch (CGenericGameStorage::GetSaveOperation())
			{
				case OPERATION_NONE :
					savegameAssertf(0, "CSavegameDialogScreens::HandleSaveGameDialogBox - didn't expect OPERATION_NONE");
					break;

				case OPERATION_LOADING_REPLAY_SAVEGAME_FROM_CONSOLE :
					savegameAssertf(0, "CSavegameDialogScreens::HandleSaveGameDialogBox - didn't expect to reach here for OPERATION_LOADING_REPLAY_SAVEGAME_FROM_CONSOLE");
					break;

				case OPERATION_LOADING_MPSTATS_FROM_CLOUD:
				case OPERATION_SCANNING_CONSOLE_FOR_LOADING_SAVEGAMES :
				case OPERATION_LOADING_SAVEGAME_FROM_CONSOLE :
				case OPERATION_LOADING_PHOTO_FROM_CLOUD :
#if RSG_ORBIS
				case OPERATION_CHECKING_FOR_FREE_SPACE_AT_START_OF_GAME :
#endif
				case OPERATION_CHECKING_IF_FILE_EXISTS :
				case OPERATION_DELETING_LOCAL :
				case OPERATION_DELETING_CLOUD :
				case OPERATION_SAVING_LOCAL_PHOTO :
				case OPERATION_LOADING_LOCAL_PHOTO :
				case OPERATION_ENUMERATING_PHOTOS :
				case OPERATION_ENUMERATING_SAVEGAMES :
					break;

				case OPERATION_SAVING_MPSTATS_TO_CLOUD:
				case OPERATION_SCANNING_CONSOLE_FOR_SAVING_SAVEGAMES :
				case OPERATION_SAVING_SAVEGAME_TO_CONSOLE :
				case OPERATION_SAVING_PHOTO_TO_CLOUD :
#if __ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
				case OPERATION_UPLOADING_SAVEGAME_TO_CLOUD :
#endif	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
				case OPERATION_CHECKING_FOR_FREE_SPACE :
#if __ALLOW_LOCAL_MP_STATS_SAVES
				case OPERATION_LOADING_MPSTATS_SAVEGAME_FROM_CONSOLE :
				case OPERATION_SAVING_MPSTATS_SAVEGAME_TO_CONSOLE :
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES
					pTextLabel = "SG_SIGNIN_SV";
					break;

				case OPERATION_SAVING_MISSION_REPEAT_SAVEGAME_TO_CONSOLE :
					pTextLabel = "SG_SGN_SV_RPT";
					break;

				case OPERATION_SAVING_REPLAY_SAVEGAME_TO_CONSOLE :
					pTextLabel = "SG_SGN_SV_RPLY";
					break;

				case OPERATION_AUTOSAVING :
					pTextLabel = "SG_SIGNIN_ATSV";
					break;
			}

			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, pTextLabel, FE_WARNING_YES_NO);
			NumberOfButtons = 2;
//	Try no dialog before sign in blade
//			MessageToDisplay = SAVE_GAME_DIALOG_NONE;
		}
			break;

		case SAVE_GAME_DIALOG_DONT_SIGN_IN_TRY_AGAIN :
			{
				//	You have not signed in. You will need to sign in to save your progress and to be awarded Achievements. Do you want to sign in now?
				const char *pTextLabel = "SG_SIGNIN_AGN";
				if (CGenericGameStorage::GetSaveOperation() == OPERATION_SAVING_MISSION_REPEAT_SAVEGAME_TO_CONSOLE)
				{
					pTextLabel = "SG_SGN_SV_RPT_A";
				}
				else if (CGenericGameStorage::GetSaveOperation() == OPERATION_SAVING_REPLAY_SAVEGAME_TO_CONSOLE)
				{
					pTextLabel = "SG_SGN_SV_RPLY_A";
				}

				CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, pTextLabel, FE_WARNING_YES_NO);
				NumberOfButtons = 2;
			}
			break;

		case SAVE_GAME_DIALOG_DEVICE_SELECTION :
//	TCRHERE
//	if ms_bDisplayDeviceHasBeenRemovedMessageNextTime
//	The selected storage device has been removed. Do you want to select a new storage device?
//	else
//	You do not currently have a storage device selected. Do you want to select a storage device now?
			if (CSavegameDevices::GetDisplayDeviceHasBeenRemovedMessageNextTime())
			{
				bDisplayingTheRemovedDeviceMessage = true;
				//	The selected storage device has been removed. Do you want to select a new storage device?
				if (CGenericGameStorage::GetSaveOperation() == OPERATION_AUTOSAVING)
				{
					CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_DEV_REM_AUTO", FE_WARNING_YES_NO);
				}
				else
				{
					CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_DEVICE_REM", FE_WARNING_YES_NO);
				}
			}
			else
			{
/*
				if (CGenericGameStorage::GetSaveOperation() == OPERATION_AUTOSAVING)
				{
					CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_DEVICE_AUTO", FE_WARNING_YES_NO);
				}
				else
				{
					CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_DEVICE", FE_WARNING_YES_NO);
				}
*/
				//	Bug 109823 says it would be better if there was no dialog before the device selection blade appears
				//	I'll see if it's still okay to display the "device has been removed" message above
				MessageToDisplay = SAVE_GAME_DIALOG_NONE;
			}
			NumberOfButtons = 2;
			break;

		case SAVE_GAME_DIALOG_DONT_SELECT_DEVICE_TRY_AGAIN :
			//	You have not selected a storage device. You will need to select a storage device in order to save game progress. Do you want to select one now?
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_SEL_DEV_AGN", FE_WARNING_YES_NO);
			NumberOfButtons = 2;
			break;

		case SAVE_GAME_DIALOG_FREE_SPACE_CHECK :
		{
#if __PPU
			//	Change SG_SPACE_PS3 to say "You need an extra ~1~KB to save into this slot. Try overwriting an existing save game in another slot or quit the game and free the required space."
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "", "SG_SPACE_PS3", FE_WARNING_OK, true, NumberToInsertInMessage);
			NumberOfButtons = 1;
#else
			//	You need an extra ~1~KB to save the game. Do you want to select another storage device and attempt to save again?
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "", "SG_SPACE_360", FE_WARNING_YES_NO, true, NumberToInsertInMessage);
			NumberOfButtons = 2;
#endif
		}
			break;

		case SAVE_GAME_DIALOG_NO_FREE_SPACE_AT_START_OF_GAME :
		{
#if __PPU
			//	Change SG_NO_SPCE_PS3 to say "You need an extra ~1~KB of hard disk space for save game data. Please quit the game and free space on your hard disk."
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "", "SG_NO_SPCE_PS3", FE_WARNING_OK, true, NumberToInsertInMessage);
			NumberOfButtons = 1;
#else
			//	There is not enough space to save on the selected storage device. Do you want to select another storage device?
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "", "SG_NO_SPCE_360", FE_WARNING_YES_NO);
			NumberOfButtons = 2;
#endif
		}
			break;

// ******** User Cancelled ********
/*
		case SAVE_GAME_DIALOG_SCAN_FAILED :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "F_SCAN", FE_WARNING_ACCEPT);
			break;

		case SAVE_GAME_DIALOG_LOAD_FAILED :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "F_LOAD", FE_WARNING_ACCEPT);
			break;

		case SAVE_GAME_DIALOG_AUTOSAVE_FAILED :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "F_ASAVE", FE_WARNING_ACCEPT);
			break;

		case SAVE_GAME_DIALOG_SAVE_FAILED :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "F_SAVE", FE_WARNING_ACCEPT);
			break;
*/
//	******* Errors *******
		case SAVE_GAME_DIALOG_NONE :
//			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "ERROR_NONE", FE_WARNING_ACCEPT);
			break;
		case SAVE_GAME_DIALOG_DEVICE_SELECTION_FAILED :
		case SAVE_GAME_DIALOG_DEVICE_SELECTION_FAILED2 :
		case SAVE_GAME_DIALOG_DEVICE_SELECTION_FAILED3 :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_DEV_SEL_FAIL", FE_WARNING_OK);
			break;
		case SAVE_GAME_DIALOG_BEGIN_FREE_SPACE_CHECK_FAILED :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_SPCHK_FAILED", FE_WARNING_OK);
			break;
		case SAVE_GAME_DIALOG_CHECK_FREE_SPACE_CHECK_FAILED1 :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_SPCHK_FAILED", FE_WARNING_OK);
			break;
		case SAVE_GAME_DIALOG_CHECK_FREE_SPACE_CHECK_FAILED2 :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_SPCHK_FAILED", FE_WARNING_OK);
			break;
		case SAVE_GAME_DIALOG_DELETE_FROM_LIST_FAILED :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_DEL_LST_FAIL", FE_WARNING_OK);
			break;
/*
		case SAVE_GAME_DIALOG_PLAYER_DOESNT_WANT_TO_FREE_SPACE :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "DONT_FREE_SPACE", FE_WARNING_ACCEPT);
			break;
*/
		case SAVE_GAME_DIALOG_BEGIN_ENUMERATION_FAILED :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_ENUM_FAILED", FE_WARNING_OK);
			break;
		case SAVE_GAME_DIALOG_CHECK_ENUMERATION_FAILED1 :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_ENUM_FAILED", FE_WARNING_OK);
			break;
		case SAVE_GAME_DIALOG_CHECK_ENUMERATION_FAILED2 :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_ENUM_FAILED", FE_WARNING_OK);
			break;
		case SAVE_GAME_DIALOG_BEGIN_SAVE_FAILED :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_CHKSAVE_FAIL", FE_WARNING_OK);
			break;
		case SAVE_GAME_DIALOG_CHECK_SAVE_FAILED1 :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_CHKSAVE_FAIL", FE_WARNING_OK);
			break;
		case SAVE_GAME_DIALOG_CHECK_SAVE_FAILED2 :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_CHKSAVE_FAIL", FE_WARNING_OK);
			break;
		case SAVE_GAME_DIALOG_BEGIN_GET_FILE_MODIFIED_TIME_FAILED :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_BGNTIME_FAIL", FE_WARNING_OK);
			break;
		case SAVE_GAME_DIALOG_CHECK_GET_FILE_MODIFIED_TIME_FAILED1 :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_CHKTIME_FAIL", FE_WARNING_OK);
			break;
		case SAVE_GAME_DIALOG_CHECK_GET_FILE_MODIFIED_TIME_FAILED2 :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_CHKTIME_FAIL", FE_WARNING_OK);
			break;
		case SAVE_GAME_DIALOG_BEGIN_ICON_SAVE_FAILED :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_CHKSAVE_FAIL", FE_WARNING_OK);
			break;
		case SAVE_GAME_DIALOG_CHECK_ICON_SAVE_FAILED1 :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_CHKSAVE_FAIL", FE_WARNING_OK);
			break;
		case SAVE_GAME_DIALOG_CHECK_ICON_SAVE_FAILED2 :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_CHKSAVE_FAIL", FE_WARNING_OK);
			break;
		case SAVE_GAME_DIALOG_BEGIN_GET_CREATOR_FAILED :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_BGNLOAD_FAIL", FE_WARNING_OK);
			break;
		case SAVE_GAME_DIALOG_CHECK_GET_CREATOR_FAILED1 :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_BGNLOAD_FAIL", FE_WARNING_OK);
			break;
		case SAVE_GAME_DIALOG_CHECK_GET_CREATOR_FAILED2 :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_BGNLOAD_FAIL", FE_WARNING_OK);
			break;
		case SAVE_GAME_DIALOG_PLAYER_IS_NOT_CREATOR :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_NOT_CREATOR", FE_WARNING_OK);
			break;
		case SAVE_GAME_DIALOG_BEGIN_LOAD_FAILED :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_BGNLOAD_FAIL", FE_WARNING_OK);
			break;
		case SAVE_GAME_DIALOG_LOAD_BUFFER_SIZE_MISMATCH :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_BUFFSIZE_DIF", FE_WARNING_OK);
			break;
		case SAVE_GAME_DIALOG_CHECK_LOAD_FAILED1 :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_CHKLOAD_FAIL", FE_WARNING_OK);
			break;
		case SAVE_GAME_DIALOG_CHECK_LOAD_FAILED2 :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_CHKLOAD_FAIL", FE_WARNING_OK);
			break;
/*
		case SAVE_GAME_DIALOG_BEGIN_GET_FILE_TIME_FAILED :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "GET_TIME_FAILED", FE_WARNING_ACCEPT);
			break;
		case SAVE_GAME_DIALOG_CHECK_GET_FILE_TIME_FAILED1 :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "GET_TIME_FAILED1", FE_WARNING_ACCEPT);
			break;
		case SAVE_GAME_DIALOG_CHECK_GET_FILE_TIME_FAILED2 :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "GET_TIME_FAILED2", FE_WARNING_ACCEPT);
			break;
*/

		case SAVE_GAME_DIALOG_BEGIN_GET_FILE_SIZE_FAILED :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "GET_SIZE_FAILED", FE_WARNING_OK);
			break;
		case SAVE_GAME_DIALOG_CHECK_GET_FILE_SIZE_FAILED1 :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "GET_SIZE_FAILED", FE_WARNING_OK);
			break;
		case SAVE_GAME_DIALOG_CHECK_GET_FILE_SIZE_FAILED2 :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "GET_SIZE_FAILED", FE_WARNING_OK);
			break;

		case SAVE_GAME_DIALOG_ALLOCATE_MEMORY_FOR_LOAD_FAILED :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_BGNLOAD_FAIL", FE_WARNING_OK);		//	Load failed. Please check your storage device and try again.
			break;

		case SAVE_GAME_DIALOG_ALLOCATE_MEMORY_FOR_SAVE_FAILED :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_CHKSAVE_FAIL", FE_WARNING_OK);		//	Save failed. Please check your storage device and try again.
			break;

		case SAVE_GAME_DIALOG_SAVE_BLOCK_DATA_FAILED :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_CHKSAVE_FAIL", FE_WARNING_OK);
			break;

		case SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT :	//	maybe should remove this completely
			// If player signs out then the game will restart immediately so don't bother
			//	displaying a warning message and giving the player another chance to load/save
			MessageToDisplay = SAVE_GAME_DIALOG_NONE;
//			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_SIGN_OUT", FE_WARNING_OK);
			break;

		case SAVE_GAME_DIALOG_SELECTED_DEVICE_HAS_BEEN_REMOVED :

			if ( (CGenericGameStorage::GetSaveOperation() == OPERATION_SCANNING_CONSOLE_FOR_LOADING_SAVEGAMES) 
				|| (CGenericGameStorage::GetSaveOperation() == OPERATION_SCANNING_CONSOLE_FOR_SAVING_SAVEGAMES) 
				|| (CGenericGameStorage::GetSaveOperation() == OPERATION_ENUMERATING_PHOTOS)
				|| (CGenericGameStorage::GetSaveOperation() == OPERATION_ENUMERATING_SAVEGAMES) )
			{
				// This message is immediately followed by another message saying 
				//	"The selected storage device has been removed. Do you want to select a new storage device?"
				//	so remove this one
				MessageToDisplay = SAVE_GAME_DIALOG_NONE;
			}
			else
			{
				CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_DEV_REM", FE_WARNING_OK);
			}
			break;

		case SAVE_GAME_DIALOG_OVERWRITE_EXISTING_AUTOSAVE :
			CSavegameList::GetDisplayNameAndDateForThisSaveGameItem(CSavegameList::GetAutosaveSlotNumberForCurrentEpisode(), cSaveGameName, cSaveGameDate, &bSlotIsEmpty, &bSlotIsDamaged);
			Assertf(!bSlotIsEmpty, "CSavegameDialogScreens::HandleSaveGameDialogBox - SAVE_GAME_DIALOG_OVERWRITE_EXISTING_AUTOSAVE - shouldn't be trying to display this message if the slot is empty");
			if (bSlotIsEmpty || bSlotIsDamaged)
			{
				CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_AUTO_SL_OVR", FE_WARNING_YES_NO);
			}
			else
			{
				//	The autosave slot already contains "~a~" saved on ~a~. Do you want to overwrite the data in this slot with your current game progress?
				CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_AUTO_SL_OVR2", FE_WARNING_YES_NO, false, -1, cSaveGameName, cSaveGameDate);
			}
			NumberOfButtons = 2;
			break;

		case SAVE_GAME_DIALOG_AUTOSAVE_FAILED_TRY_AGAIN :
			//	Autosave failed. Do you want to retry?
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_AUTO_FAILED", FE_WARNING_YES_NO);
			NumberOfButtons = 2;
			break;

		case SAVE_GAME_DIALOG_AUTOSAVE_OFF_ARE_YOU_SURE :
			//	Are you sure? Autosaving will be turned off.
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_AUTOOFF_SURE", FE_WARNING_YES_NO);
			NumberOfButtons = 2;
			break;

		case SAVE_GAME_DIALOG_AUTOSAVE_SWITCHED_OFF :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_AUTO_OFF", FE_WARNING_OK);
			break;

		case SAVE_GAME_DIALOG_CANT_LOAD_A_DAMAGED_SAVE :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_LOAD_DAM", FE_WARNING_OK);
			break;

		case SAVE_GAME_DIALOG_LOAD_WILL_OVERWRITE :
			//	Are you sure you want to load this save game data? Doing so will lose any unsaved game progress.
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_LOAD_OVRWRT", FE_WARNING_YES_NO);
			NumberOfButtons = 2;
			break;
/*
		case SAVE_GAME_DIALOG_LOAD_ARE_YOU_SURE :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_LOADOVR_SURE", FE_WARNING_YES_NO);
			NumberOfButtons = 2;
			break;
*/
		case SAVE_GAME_DIALOG_DELETE_ARE_YOU_SURE :
#if RSG_ORBIS
			Assertf( (NumberToInsertInMessage >= 1) && (NumberToInsertInMessage <= TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES+NUM_BACKUPSAVE_SLOTS), "CSavegameDialogScreens::HandleSaveGameDialogBox - SAVE_GAME_DIALOG_DELETE_ARE_YOU_SURE - expected the slot number to be between 1 and %d (inclusive)", TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES+NUM_BACKUPSAVE_SLOTS);
#else
			Assertf( (NumberToInsertInMessage >= 1) && (NumberToInsertInMessage <= TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES), "CSavegameDialogScreens::HandleSaveGameDialogBox - SAVE_GAME_DIALOG_DELETE_ARE_YOU_SURE - expected the slot number to be between 1 and %d (inclusive)", TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES);
#endif
			CSavegameList::GetDisplayNameAndDateForThisSaveGameItem( (NumberToInsertInMessage-1), cSaveGameName, cSaveGameDate, &bSlotIsEmpty, &bSlotIsDamaged);
			Assertf(!bSlotIsEmpty, "CSavegameDialogScreens::HandleSaveGameDialogBox - SAVE_GAME_DIALOG_DELETE_ARE_YOU_SURE - shouldn't be trying to display this message if the slot is empty");	//	&& !bSlotIsDamaged

			if (bSlotIsDamaged)
			{
				CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_DEL_DAM", FE_WARNING_YES_NO, false, -1);
			}
			else
			{
				//	The selected slot contains "~a~" saved on ~a~. Do you want to delete the save game data in this slot?
				CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_DEL", FE_WARNING_YES_NO, false, -1, cSaveGameName, cSaveGameDate );
			}

			NumberOfButtons = 2;
			break;

#if __ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
		case SAVE_GAME_DIALOG_UPLOAD_ARE_YOU_SURE :
			Assertf( (NumberToInsertInMessage >= 1) && (NumberToInsertInMessage <= TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES), "CSavegameDialogScreens::HandleSaveGameDialogBox - expected the slot number to be between 1 and %d (inclusive)", TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES);
			CSavegameList::GetDisplayNameAndDateForThisSaveGameItem( (NumberToInsertInMessage-1), cSaveGameName, cSaveGameDate, &bSlotIsEmpty, &bSlotIsDamaged);
			Assertf(!bSlotIsEmpty && !bSlotIsDamaged, "CSavegameDialogScreens::HandleSaveGameDialogBox - SAVE_GAME_DIALOG_UPLOAD_ARE_YOU_SURE - shouldn't be trying to display this message if the slot is empty or damaged");

			//	The selected slot contains "~a~" saved on ~a~. Do you want to upload the save game data in this slot to the Social Club?
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_UPLD", FE_WARNING_YES_NO, false, -1, cSaveGameName, cSaveGameDate );

			NumberOfButtons = 2;
			break;
#endif	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
		case SAVE_GAME_DIALOG_CANT_EXPORT_A_DAMAGED_SAVE :
			// I still need to add this text to the american text 
			//	maybe x:\gta5\assets_ng\gametext\NG_TU_Sub_35_0\American\americanCode.txt
			//	or whatever the latest TU is.
			// The selected save game data cannot be exported/migrated because it is damaged.
			SAVEGAME_MESSAGES_CHECK_TEXT_LABEL_EXISTS("SG_PROC_DAM")
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_PROC_DAM", FE_WARNING_OK);
			break;

		case SAVE_GAME_DIALOG_CONFIRM_EXPORT_OF_SAVEGAME :
			Assertf( (NumberToInsertInMessage >= 1) && (NumberToInsertInMessage <= TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES), "CSavegameDialogScreens::HandleSaveGameDialogBox - expected the slot number to be between 1 and %d (inclusive)", TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES);
			CSavegameList::GetDisplayNameAndDateForThisSaveGameItem( (NumberToInsertInMessage-1), cSaveGameName, cSaveGameDate, &bSlotIsEmpty, &bSlotIsDamaged);
			Assertf(!bSlotIsEmpty && !bSlotIsDamaged, "CSavegameDialogScreens::HandleSaveGameDialogBox - SAVE_GAME_DIALOG_CONFIRM_EXPORT_OF_SAVEGAME - shouldn't be trying to display this message if the slot is empty or damaged");

#if __REMOVE_TIME_FROM_DATE_TIME_STRINGS_IN_EXPORT_MESSAGES
			//	Remove the time from the date/time string
			LengthOfSaveGameDate = istrlen(cSaveGameDate);
			if (savegameVerifyf(LengthOfSaveGameDate == 14, "CSavegameDialogScreens::HandleSaveGameDialogBox - expected the string containing the date/time ('%s') to be 14 characters long", cSaveGameDate))
			{
				if (savegameVerifyf(cSaveGameDate[8] == ' ', "CSavegameDialogScreens::HandleSaveGameDialogBox - expected the 9th character of the string containing the date/time ('%s') to be a space", cSaveGameDate))
				{
					cSaveGameDate[8] = '\0';
				}
			}
#endif // __REMOVE_TIME_FROM_DATE_TIME_STRINGS_IN_EXPORT_MESSAGES

			SAVEGAME_MESSAGES_CHECK_TEXT_LABEL_EXISTS("SG_PROC")
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_PROC", FE_WARNING_YES_NO, false, -1, cSaveGameName, cSaveGameDate );

			NumberOfButtons = 2;

			break;
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES


#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
		case SAVE_GAME_DIALOG_IMPORT_WILL_OVERWRITE_SLOT :
#if RSG_ORBIS
			Assertf( (NumberToInsertInMessage >= 1) && (NumberToInsertInMessage <= TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES+NUM_BACKUPSAVE_SLOTS), "CSavegameDialogScreens::HandleSaveGameDialogBox - expected the slot number to be between 1 and %d (inclusive)", TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES+NUM_BACKUPSAVE_SLOTS);
#else
			Assertf( (NumberToInsertInMessage >= 1) && (NumberToInsertInMessage <= TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES), "CSavegameDialogScreens::HandleSaveGameDialogBox - expected the slot number to be between 1 and %d (inclusive)", TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES);
#endif
			CSavegameList::GetDisplayNameAndDateForThisSaveGameItem( (NumberToInsertInMessage-1), cSaveGameName, cSaveGameDate, &bSlotIsEmpty, &bSlotIsDamaged);
			Assertf(!bSlotIsEmpty, "CSavegameDialogScreens::HandleSaveGameDialogBox - SAVE_GAME_DIALOG_IMPORT_WILL_OVERWRITE_SLOT - shouldn't be trying to display this message if the slot is empty");
			if (bSlotIsEmpty || bSlotIsDamaged)
			{	// I still need to add this text to americanError.txt or americanCode.txt
				// The selected slot contains damaged save game data. Do you want to overwrite the data in this slot with the imported save game data?
				CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_IMPORT_OVRWRT", FE_WARNING_YES_NO, false, -1);
			}
			else
			{	// I still need to add this text to americanCode.txt
				// The selected slot already contains "~a~" saved on ~a~. Do you want to overwrite the data in this slot with the imported save game data?
				CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_IMPORT_OVRWRT2", FE_WARNING_YES_NO, false, -1, cSaveGameName, cSaveGameDate );
			}
			NumberOfButtons = 2;
			break;

		case SAVE_GAME_DIALOG_IMPORT_ARE_YOU_SURE :
			// I still need to add this text to americanCode.txt
			// Are you sure you want to import the save game data in to a new slot?
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_IMPORT_NEW", FE_WARNING_YES_NO, false, -1);
			NumberOfButtons = 2;
			break;

		case SAVE_GAME_DIALOG_IMPORT_OF_SAVEGAME_SUCCEEDED :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_IMPORT_SUCCESS", FE_WARNING_OK);
			break;

		case SAVE_GAME_DIALOG_IMPORT_OF_SAVEGAME_FAILED :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_IMPORT_FAILED", FE_WARNING_OK);
			break;

		case SAVE_GAME_DIALOG_LOADING_OF_IMPORTED_SAVEGAME_FAILED :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_IMPORT_LOAD_FAILED", FE_WARNING_OK);
			break;
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES


		case SAVE_GAME_DIALOG_SAVE_WILL_OVERWRITE_SLOT :
#if RSG_ORBIS
			Assertf( (NumberToInsertInMessage >= 1) && (NumberToInsertInMessage <= TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES+NUM_BACKUPSAVE_SLOTS), "CSavegameDialogScreens::HandleSaveGameDialogBox - expected the slot number to be between 1 and %d (inclusive)", TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES+NUM_BACKUPSAVE_SLOTS);
#else
			Assertf( (NumberToInsertInMessage >= 1) && (NumberToInsertInMessage <= TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES), "CSavegameDialogScreens::HandleSaveGameDialogBox - expected the slot number to be between 1 and %d (inclusive)", TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES);
#endif
			CSavegameList::GetDisplayNameAndDateForThisSaveGameItem( (NumberToInsertInMessage-1), cSaveGameName, cSaveGameDate, &bSlotIsEmpty, &bSlotIsDamaged);
			Assertf(!bSlotIsEmpty, "CSavegameDialogScreens::HandleSaveGameDialogBox - SAVE_GAME_DIALOG_SAVE_WILL_OVERWRITE_SLOT - shouldn't be trying to display this message if the slot is empty");
			if (bSlotIsEmpty || bSlotIsDamaged)
			{
				CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_SAVE_OVRWRT", FE_WARNING_YES_NO, false, -1);
			}
			else
			{
				//	Slot ~1~ already contains "~a~" saved on ~a~. Do you want to overwrite the data in this slot with your current game progress?
				CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_SAVE_OVRWRT2", FE_WARNING_YES_NO, false, -1, cSaveGameName, cSaveGameDate );
			}
			NumberOfButtons = 2;
			break;

		case SAVE_GAME_DIALOG_SAVE_ARE_YOU_SURE :
			//	Are you sure you want to save your current game progress in slot ~1~?
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_SAVEOVR_SURE", FE_WARNING_YES_NO, false, -1);
			NumberOfButtons = 2;
			break;

		case SAVE_GAME_DIALOG_TOO_MANY_SAVE_FILES_IN_FOLDER :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_TOOMANYFILES", FE_WARNING_OK);
			break;

		case SAVE_GAME_DIALOG_UNEXPECTED_FILENAME :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_FILENAME_ERR", FE_WARNING_OK);
			break;

		case SAVE_GAME_DIALOG_TWO_SAVE_FILES_WITH_SAME_NAME	:
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_FILENAME_DUP", FE_WARNING_OK);
			break;

		case SAVE_GAME_DIALOG_WRONG_VERSION :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_VERS_ERR", FE_WARNING_OK);
			break;

		case SAVE_GAME_DIALOG_EXTRA_CONTENT_ERROR :
//			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_EX_CONT_ERR", FE_WARNING_OK, false, -1, true);	//	Last flag true means list missing extra content
			if (CSavegameAutoload::IsPerformingAutoLoadAtStartOfGame())
			{	//	The downloadable content required for this autoload is not available.~n~Press ~PAD_A~ to start a new game of GTA IV.
				CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "FE_NODLC3", FE_WARNING_OK);
			}
			else
			{
				CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "FE_NODLC2", FE_WARNING_OK);
			}
			break;
//	******* AutoLoad *******
		case SAVE_GAME_DIALOG_AUTOLOAD_SIGN_IN :
			
#if RSG_DURANGO
			if(sysUserList::GetInstance().GetNumberOfDetectedUsersInRoom() != 0)
			{
				// You are not paired with a controller. Please ensure the controller is visible to Kinect or select an Account from the Picker. Do you want to select an Account now?
				CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_ALD_PAIRPAD", FE_WARNING_YES_NO, true, NumberToInsertInMessage);
			}
			else if(sysUserList::GetInstance().GetNumberOfUsersLoggedInOnConsole() != 0)
			{
				// You are not paired with a controller. You will need to pair a controller to save your progress and to be awarded Achievements. Do want you pair a controller now?
				CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_ALD_PAIRPAD_MANUAL", FE_WARNING_YES_NO, true, NumberToInsertInMessage);
			}
			else
#endif // RSG_DURANGO
			{
				//	You are not signed in. You will need to sign in to save your progress and to be awarded Achievements. Do you want to sign in now?
				CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_ALD_SIGNIN", FE_WARNING_YES_NO, true, NumberToInsertInMessage);
			}
			NumberOfButtons = 2;
			break;

//		case SAVE_GAME_DIALOG_AUTOLOAD_DONT_SIGN_IN_SURE :
//			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_ALD_SIGNSURE", FE_WARNING_YES_NO, true, NumberToInsertInMessage);
//			NumberOfButtons = 2;
//			break;

		case SAVE_GAME_DIALOG_AUTOLOAD_DONT_SIGN_IN_OK :
//			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_ALD_SIGN_OK", FE_WARNING_OK, true, NumberToInsertInMessage);
			MessageToDisplay = SAVE_GAME_DIALOG_NONE;
			break;

		case SAVE_GAME_DIALOG_AUTOLOAD_NOT_SIGNED_IN_TRY_AGAIN :
#if RSG_DURANGO
			if(sysUserList::GetInstance().GetNumberOfDetectedUsersInRoom() != 0)
			{
				// You are not paired with a controller. Please ensure the controller is visible to Kinect or select an Account from the Picker. Do you want to select an Account now?
				CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_ALD_PAIRPAD_AGN", FE_WARNING_YES_NO, true, NumberToInsertInMessage);
			}
			else if(sysUserList::GetInstance().GetNumberOfUsersLoggedInOnConsole() != 0)
			{
				// You are not paired with a controller. You will need to pair a controller to save your progress and to be awarded Achievements. Do want you pair a controller now?
				CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_ALD_PAIRPAD_MANUAL_AGN", FE_WARNING_YES_NO, true, NumberToInsertInMessage);
			}
			else
#endif // RSG_DURANGO
			{
				//	You have not signed in. You will need to sign in to save your progress and to be awarded Achievements. Do you want to sign in now?
				CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_ALD_SIGN_AGN", FE_WARNING_YES_NO, true, NumberToInsertInMessage);
			}
			NumberOfButtons = 2;
			break;

		case SAVE_GAME_DIALOG_MOST_RECENT_SAVE_IS_DAMAGED :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_ALD_DAM", FE_WARNING_OK);
			break;

#if RSG_PC
		case SAVE_GAME_DIALOG_MOST_RECENT_PC_SAVE_IS_DAMAGED :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_ALD_DAM_PC", FE_WARNING_OK);
			break;

		case SAVE_GAME_DIALOG_CANT_LOAD_A_DAMAGED_PC_SAVE :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_LOAD_DAM_PC", FE_WARNING_OK);
			break;
#endif	//	RSG_PC

		case SAVE_GAME_DIALOG_AUTOLOAD_NO_PAD_CONNECTED :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "NO_PAD", FE_WARNING_NONE);
			NumberOfButtons = 0;
			break;

		case SAVE_GAME_DIALOG_AUTOLOAD_MULTIPLE_PROFILES :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "MULT_PROF", FE_WARNING_NONE);
			NumberOfButtons = 0;
			break;
/*
#if __PPU
		case SAVE_GAME_DIALOG_FLASHING_HDD_LIGHT_WARNING :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "PS3_HDD_LIGHT", FE_WARNING_NONE);
			NumberOfButtons = 0;
			break;
#endif
*/
		case SAVE_GAME_DIALOG_BEGIN_LOCAL_DELETE_FAILED :
		case SAVE_GAME_DIALOG_CHECK_LOCAL_DELETE_FAILED1 :
		case SAVE_GAME_DIALOG_CHECK_LOCAL_DELETE_FAILED2 :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "SG_HDNG", "SG_DEL_FAIL", FE_WARNING_OK);	//	SG_HDG_DEL
			break;

		case SAVE_GAME_DIALOG_BEGIN_CLOUD_DELETE_FAILED :
		case SAVE_GAME_DIALOG_CHECK_CLOUD_DELETE_FAILED :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "SG_HDNG", "SG_DEL_C_FAIL", FE_WARNING_OK);	//	SG_HDG_DEL
			break;

		case SAVE_GAME_DIALOG_BEGIN_CLOUD_LOAD_PHOTO_LIST_FAILED :
		case SAVE_GAME_DIALOG_CHECK_CLOUD_LOAD_PHOTO_LIST_FAILED :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_PH_LS_CL_FL", FE_WARNING_OK);
			break;
		case SAVE_GAME_DIALOG_BEGIN_CLOUD_LOAD_JPEG_FAILED :
		case SAVE_GAME_DIALOG_CHECK_CLOUD_LOAD_JPEG_FAILED :
		case SAVE_GAME_DIALOG_BEGIN_CLOUD_LOAD_JSON_FAILED :
		case SAVE_GAME_DIALOG_CHECK_CLOUD_LOAD_JSON_FAILED :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "SG_HDNG", "SG_PH_LD_CL_FL", FE_WARNING_OK);	//	SG_HDG_L_PH_CL
			break;

		case SAVE_GAME_DIALOG_BEGIN_LOCAL_LOAD_JPEG_FAILED :
		case SAVE_GAME_DIALOG_CHECK_LOCAL_LOAD_JPEG_FAILED :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "SG_HDNG", "SG_PH_LD_LC_FL", FE_WARNING_OK);
			break;

		case SAVE_GAME_DIALOG_BEGIN_CLOUD_SAVE_JPEG_FAILED :
		case SAVE_GAME_DIALOG_CHECK_CLOUD_SAVE_JPEG_FAILED :
		case SAVE_GAME_DIALOG_BEGIN_CLOUD_SAVE_JSON_FAILED :
		case SAVE_GAME_DIALOG_CHECK_CLOUD_SAVE_JSON_FAILED :
		case SAVE_GAME_DIALOG_BEGIN_UGC_PHONE_PHOTO_SAVE_FAILED :
		case SAVE_GAME_DIALOG_CHECK_UGC_PHONE_PHOTO_SAVE_FAILED :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "SG_HDNG", "SG_PH_SV_CL_FL", FE_WARNING_OK);	//	SG_HDG_S_PH_CL
			break;

		case SAVE_GAME_DIALOG_LIMELIGHT_UGC_CREATE_ALREADY_PENDING :
			DisplayRockstarDevWarning("SG_PH_UGC_PN");
			break;

		case SAVE_GAME_DIALOG_LIMELIGHT_UGC_CREATION_FAILED_TO_START :
			DisplayRockstarDevWarning("SG_PH_UGC_CR1");
			break;

		case SAVE_GAME_DIALOG_LIMELIGHT_UGC_CREATION_FAILED :
			DisplayRockstarDevWarning("SG_PH_UGC_CR2", NULL, NULL, true, CPhotoManager::GetNetStatusResultCodeForUploadLocalPhoto());
			break;

		case SAVE_GAME_DIALOG_LIMELIGHT_GET_UGC_CREATE_CONTENT_ID_FAILED :
			DisplayRockstarDevWarning("SG_PH_UGC_ID");
			break;

		case SAVE_GAME_DIALOG_LIMELIGHT_GET_UGC_CREATE_RESULT_FAILED :
			DisplayRockstarDevWarning("SG_PH_UGC_RES", CPhotoManager::GetUGCContentIdForUploadLocalPhoto());
			break;

		case SAVE_GAME_DIALOG_LIMELIGHT_UGC_METADATA_PARSE_ERROR :
			DisplayRockstarDevWarning("SG_PH_UGC_PRS", CPhotoManager::GetUGCContentIdForUploadLocalPhoto());
			break;

		case SAVE_GAME_DIALOG_LIMELIGHT_CREATE_TASK_FAILED :
			DisplayRockstarDevWarning("SG_PH_LL_TSK", CPhotoManager::GetUGCContentIdForUploadLocalPhoto());
			break;

		case SAVE_GAME_DIALOG_LIMELIGHT_CONFIGURE_FAILED :
			DisplayRockstarDevWarning("SG_PH_LL_CFG", CPhotoManager::GetUGCContentIdForUploadLocalPhoto(), CPhotoManager::GetLimelightAuthTokenForUploadLocalPhoto());
			break;

		case SAVE_GAME_DIALOG_LIMELIGHT_ADD_PARALLEL_TASK_FAILED :
			DisplayRockstarDevWarning("SG_PH_LL_PRL", CPhotoManager::GetUGCContentIdForUploadLocalPhoto(), CPhotoManager::GetLimelightAuthTokenForUploadLocalPhoto());
			break;

		case SAVE_GAME_DIALOG_LIMELIGHT_UPLOAD_FAILED :
			DisplayRockstarDevWarning("SG_PH_LL_UP", CPhotoManager::GetUGCContentIdForUploadLocalPhoto(), CPhotoManager::GetLimelightAuthTokenForUploadLocalPhoto(), true, CPhotoManager::GetNetStatusResultCodeForUploadLocalPhoto());
			break;

		case SAVE_GAME_DIALOG_LIMELIGHT_UGC_PUBLISH_FAILED_TO_START :
			DisplayRockstarDevWarning("SG_PH_UGC_PB1", CPhotoManager::GetUGCContentIdForUploadLocalPhoto());
			break;

		case SAVE_GAME_DIALOG_LIMELIGHT_UGC_PUBLISH_FAILED :
			DisplayRockstarDevWarning("SG_PH_UGC_PB2", CPhotoManager::GetUGCContentIdForUploadLocalPhoto(), NULL, true, CPhotoManager::GetNetStatusResultCodeForUploadLocalPhoto());
			break;

		case SAVE_GAME_DIALOG_MISSION_REPEAT_MAKES_A_SAVE :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_RPT_SAVES", FE_WARNING_YES_NO);
			NumberOfButtons = 2;
			break;

		case SAVE_GAME_DIALOG_PC_BENCHMARK_TEST_MAKES_A_SAVE :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_BNCH_SAVES", FE_WARNING_YES_NO);
			NumberOfButtons = 2;
			break;

#if __ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
		case SAVE_GAME_DIALOG_LOADING_LOCAL_SAVEGAME_BEFORE_UPLOADING_TO_CLOUD :
			//	Loading the local save game in order to upload it to the Social Club.
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_UPLD_LD", FE_WARNING_NONE);
			NumberOfButtons = 0;
			break;
		case SAVE_GAME_DIALOG_UPLOADING_SAVEGAME_TO_CLOUD :
			//	Uploading the save game data to the Social Club.
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_UPLD_SV", FE_WARNING_NONE);
			NumberOfButtons = 0;
			break;

		case SAVE_GAME_DIALOG_UPLOAD_OF_SAVEGAME_SUCCEEDED :
			//	The save game data has been successfully uploaded to the Social Club.
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "SG_HDNG", "SG_UPLD_SUCCESS", FE_WARNING_OK);
			break;

		case SAVE_GAME_DIALOG_BEGIN_CLOUD_SAVE_SP_SAVEGAME_FAILED :
		case SAVE_GAME_DIALOG_CHECK_CLOUD_SAVE_SP_SAVEGAME_FAILED :
			//	Failed to save single player save game to the Social Club. The Rockstar game services are unavailable right now. Please try again later.
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "SG_HDNG", "SG_UPLD_FAILED", FE_WARNING_OK);
			break;
#endif	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME

		case SAVE_GAME_DIALOG_DELETION_OF_SAVEGAME_SUCCEEDED :
			//	The save game data has been successfully deleted.
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "SG_HDNG", "SG_DEL_SUCCESS", FE_WARNING_OK);
			break;

		case SAVE_GAME_DIALOG_PLAYER_HAS_SKIPPED_AUTOLOAD :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "SG_HDNG", "SG_AUTO_LD_SKIP", FE_WARNING_OK);
			break;

		case SAVE_GAME_DIALOG_OLDEST_UPLOADED_PHOTO_WILL_BE_DELETED :
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, DialogBoxHeading, "SG_PH_OLDEST", FE_WARNING_YES_NO, true, NumberToInsertInMessage);
			NumberOfButtons = 2;
			break;

		default :
			Assertf(0, "CSavegameDialogScreens::HandleSaveGameDialogBox - unexpected MessageToDisplay value");
			break;
	}

#if RSG_ORBIS
	if (CGenericGameStorage::GetSaveOperation() == OPERATION_CHECKING_FOR_FREE_SPACE_AT_START_OF_GAME)
	{
		savegameDebugf1("CSavegameDialogScreens::HandleSaveGameDialogBox - Error message is %d but can't display it now so setting to SAVE_GAME_DIALOG_NONE\n", MessageToDisplay);
		savegameWarningf("CSavegameDialogScreens::HandleSaveGameDialogBox - don't display save game error messages during free space checks at start of game");
		MessageToDisplay = SAVE_GAME_DIALOG_NONE;
	}
#endif

	if (SAVE_GAME_DIALOG_NONE == MessageToDisplay)
	{
		*pReturnSelection = true;

		CWarningScreen::Remove();
		return true;
	}

	if (NumberOfButtons == 0)
	{	//	Doesn't really matter what is returned here
		//	This is for the cases where the game displays a warning and doesn't expect the player to press A or B
		*pReturnSelection = false;
		return false;
	}

	eWarningButtonFlags result = CWarningScreen::CheckAllInput(true, CHECK_INPUT_OVERRIDE_FLAG_RESTART_SAVED_GAME_STATE);

	if (result == FE_WARNING_OK || result == FE_WARNING_YES || result == FE_WARNING_SELECT)
	{
		if (bDisplayingTheRemovedDeviceMessage)
		{	//	Only display the removed device message the first time after the device has been removed
			CSavegameDevices::SetAllowDeviceHasBeenRemovedMessageToBeDisplayed(false);
		}

		*pReturnSelection = true;

		CWarningScreen::Remove();
		ms_NumberOfFramesToRenderBlackScreen = CGenericGameStorage::ms_NumberOfFramesToDisplayBlackScreenAfterWarningMessage;
		return true;
	}

	if (NumberOfButtons == 2)
	{
		if (result == FE_WARNING_NO)
		{
			if (bDisplayingTheRemovedDeviceMessage)
			{	//	Only display the removed device message the first time after the device has been removed
				CSavegameDevices::SetAllowDeviceHasBeenRemovedMessageToBeDisplayed(false);
			}
			*pReturnSelection = false;

			CWarningScreen::Remove();
			ms_NumberOfFramesToRenderBlackScreen = CGenericGameStorage::ms_NumberOfFramesToDisplayBlackScreenAfterWarningMessage;
			return true;
		}
	}
	
	return false;
}


void CSavegameDialogScreens::DisplayRockstarDevWarning(const char *pStringToDisplay, const char *pSubString1, const char *pSubString2, bool bInsertNumber, int numberToInsert)
{
	if (!NetworkInterface::IsRockstarDev())
	{
		pStringToDisplay = "SG_PH_SV_CL_FL";
		pSubString1 = NULL;
		pSubString2 = NULL;
		bInsertNumber = false;
		numberToInsert = -1;
	}
	CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "SG_HDNG", pStringToDisplay, FE_WARNING_OK, bInsertNumber, numberToInsert, pSubString1, pSubString2);
}


void CLoadConfirmationMessage::Clear()
{
	ms_SlotForPlayerConfirmation = -1;
	ms_bSlotIsDamaged = false;
	ms_TypeOfPlayerConfirmationCheck = LOAD_CONFIRMATION_NONE;
	ms_LoadMessageType = LOAD_MESSAGE_LOAD;
}

void CLoadConfirmationMessage::Begin(s32 SlotNumber, eLoadMessageType loadMessageType)
{
	ms_SlotForPlayerConfirmation = SlotNumber;
	ms_bSlotIsDamaged = CSavegameList::GetIsDamaged(ms_SlotForPlayerConfirmation);
	ms_TypeOfPlayerConfirmationCheck = LOAD_CONFIRMATION_DISPLAY_MESSAGE;
	ms_LoadMessageType = loadMessageType;
}

ePlayerConfirmationReturn CLoadConfirmationMessage::Process()
{
	bool ReturnSelection = false;
	ePlayerConfirmationReturn ReturnValue = PLAYER_CONFIRMATION_CHECK_IN_PROGRESS;

	if (CSavegameUsers::HasPlayerJustSignedOut())
	{
		ms_SlotForPlayerConfirmation = -1;
		ms_TypeOfPlayerConfirmationCheck = LOAD_CONFIRMATION_PLAYER_HAS_SIGNED_OUT;
	}

	if (CSavegameDevices::HasSelectedDeviceJustBeenRemoved())
	{
		ms_SlotForPlayerConfirmation = -1;
		ms_TypeOfPlayerConfirmationCheck = LOAD_CONFIRMATION_STORAGE_DEVICE_REMOVED;
	}

	switch (ms_TypeOfPlayerConfirmationCheck)
	{
		case LOAD_CONFIRMATION_NONE :
			return NO_PLAYER_CONFIRMATION_CHECK_IN_PROGRESS;

		case LOAD_CONFIRMATION_DISPLAY_MESSAGE :
			{
				s32 slotNumberToDisplay = 0;

				u32 messageToDisplay = SAVE_GAME_DIALOG_LOAD_WILL_OVERWRITE;
				switch (ms_LoadMessageType)
				{
					case LOAD_MESSAGE_LOAD :
						if (ms_bSlotIsDamaged)
						{
							messageToDisplay = SAVE_GAME_DIALOG_CANT_LOAD_A_DAMAGED_SAVE;
						}
						else
						{
							messageToDisplay = SAVE_GAME_DIALOG_LOAD_WILL_OVERWRITE;
						}
						break;
					case LOAD_MESSAGE_DELETE :
						messageToDisplay = SAVE_GAME_DIALOG_DELETE_ARE_YOU_SURE;
						slotNumberToDisplay = ms_SlotForPlayerConfirmation + 1;	//	 - FIRST_MANUAL_SLOT;
						Assertf(slotNumberToDisplay > 0, "CLoadConfirmationMessage::Process - delete slot to display should be greater than 0");
						break;
#if __ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
					case LOAD_MESSAGE_UPLOAD :
						messageToDisplay = SAVE_GAME_DIALOG_UPLOAD_ARE_YOU_SURE;
						slotNumberToDisplay = ms_SlotForPlayerConfirmation + 1;	//	 - FIRST_MANUAL_SLOT;
						Assertf(slotNumberToDisplay > 0, "CLoadConfirmationMessage::Process - upload slot to display should be greater than 0");
						break;
#endif	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
					case LOAD_MESSAGE_EXPORT :
						if (ms_bSlotIsDamaged)
						{
							messageToDisplay = SAVE_GAME_DIALOG_CANT_EXPORT_A_DAMAGED_SAVE;
						}
						else
						{
							messageToDisplay = SAVE_GAME_DIALOG_CONFIRM_EXPORT_OF_SAVEGAME;
							slotNumberToDisplay = ms_SlotForPlayerConfirmation + 1;	//	 - FIRST_MANUAL_SLOT;
							Assertf(slotNumberToDisplay > 0, "CLoadConfirmationMessage::Process - export slot to display should be greater than 0");
						}
						break;
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES
				}

				if (CLiveManager::GetInviteMgr().HasPendingAcceptedInvite())
				{
					ms_TypeOfPlayerConfirmationCheck = LOAD_CONFIRMATION_NONE;
					ReturnValue = NO_PLAYER_CONFIRMATION_CHECK_IN_PROGRESS;
				}
#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
				else if ( (messageToDisplay == SAVE_GAME_DIALOG_CANT_LOAD_A_DAMAGED_SAVE)
						|| (messageToDisplay == SAVE_GAME_DIALOG_CANT_EXPORT_A_DAMAGED_SAVE) )
#else // __ALLOW_EXPORT_OF_SP_SAVEGAMES
				else if (messageToDisplay == SAVE_GAME_DIALOG_CANT_LOAD_A_DAMAGED_SAVE)
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES
				{
					if (CSavegameDialogScreens::HandleSaveGameDialogBox(messageToDisplay, slotNumberToDisplay, &ReturnSelection))
					{
						ms_TypeOfPlayerConfirmationCheck = LOAD_CONFIRMATION_NONE;
						ReturnValue = NO_PLAYER_CONFIRMATION_CHECK_IN_PROGRESS;
					}
				}
				else if (CSavegameDialogScreens::HandleSaveGameDialogBox(messageToDisplay, slotNumberToDisplay, &ReturnSelection))
				{
					ms_TypeOfPlayerConfirmationCheck = LOAD_CONFIRMATION_NONE;

					if (LOAD_MESSAGE_DELETE == ms_LoadMessageType)
					{
						// Default to load in the load game menu after any delete attempt, so if the player clicks on an item loading will be assumed
						CPauseMenu::SetPlayerWantsToLoadASavegame();
					}

					if (ReturnSelection)
					{
						switch (ms_LoadMessageType)
						{
							case LOAD_MESSAGE_LOAD :
								ReturnValue = PLAYER_CONFIRMATION_CHECK_FINISHED_BEGIN_LOADING;
								break;
							case LOAD_MESSAGE_DELETE :
								ReturnValue = PLAYER_CONFIRMATION_CHECK_FINISHED_BEGIN_DELETING_FROM_LOAD_MENU;
								break;
#if __ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
							case LOAD_MESSAGE_UPLOAD :
								ReturnValue = PLAYER_CONFIRMATION_CHECK_FINISHED_BEGIN_UPLOADING;
								break;
#endif	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
							case LOAD_MESSAGE_EXPORT :
								ReturnValue = PLAYER_CONFIRMATION_CHECK_FINISHED_BEGIN_EXPORTING;
								break;
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES
						}
					}
					else
					{
						ReturnValue = NO_PLAYER_CONFIRMATION_CHECK_IN_PROGRESS;
					}
				}
			}
			break;

		case LOAD_CONFIRMATION_PLAYER_HAS_SIGNED_OUT :
			ms_TypeOfPlayerConfirmationCheck = LOAD_CONFIRMATION_NONE;
			ReturnValue = PLAYER_CONFIRMATION_CHECK_INTERRUPTED_PLAYER_HAS_SIGNED_OUT;
			break;

		case LOAD_CONFIRMATION_STORAGE_DEVICE_REMOVED :
			if (CSavegameDialogScreens::HandleSaveGameDialogBox(SAVE_GAME_DIALOG_SELECTED_DEVICE_HAS_BEEN_REMOVED, 0, &ReturnSelection))
			{
				ms_TypeOfPlayerConfirmationCheck = LOAD_CONFIRMATION_NONE;
				ReturnValue = PLAYER_CONFIRMATION_CHECK_INTERRUPTED_NEED_TO_RESCAN;
			}
			break;

		default :
			Assertf(0, "CLoadConfirmationMessage::Process - unknown ms_TypeOfPlayerConfirmationCheck");
			break;
	}

	return ReturnValue;
}



void CSaveConfirmationMessage::Clear()
{
	ms_SlotForPlayerConfirmation = -1;
	ms_TypeOfPlayerConfirmationCheck = SAVE_CONFIRMATION_NONE;
	ms_SaveMessageType = SAVE_MESSAGE_SAVE;
}

void CSaveConfirmationMessage::Begin(s32 SlotNumber, eSaveMessageType saveMessageType)
{
	ms_SlotForPlayerConfirmation = SlotNumber;
	ms_TypeOfPlayerConfirmationCheck = SAVE_CONFIRMATION_DISPLAY_MESSAGE;
	ms_SaveMessageType = saveMessageType;
}

//	If the MU is removed during this function then if the player cancels and goes back to the list of slots
//	then the display will still be displaying the contents of the MU. Should probably do a full rescan to update
//	the list
ePlayerConfirmationReturn CSaveConfirmationMessage::Process()
{
	bool bPlayerHasSignedOut = false;

	bool ReturnSelection = false;
	ePlayerConfirmationReturn ReturnValue = PLAYER_CONFIRMATION_CHECK_IN_PROGRESS;

	if (ms_TypeOfPlayerConfirmationCheck == SAVE_CONFIRMATION_DO_INITIAL_CHECKS_FOR_FREE_SPACE)
	{	//	Leave it to InitialChecks to test whether the player has signed out or removed the storage device
	}
	else
	{
		if (CSavegameUsers::HasPlayerJustSignedOut())
		{
			ms_SlotForPlayerConfirmation = -1;
			ms_TypeOfPlayerConfirmationCheck = SAVE_CONFIRMATION_PLAYER_HAS_SIGNED_OUT;
		}

		if (CSavegameDevices::HasSelectedDeviceJustBeenRemoved())
		{
			ms_SlotForPlayerConfirmation = -1;
			ms_TypeOfPlayerConfirmationCheck = SAVE_CONFIRMATION_STORAGE_DEVICE_REMOVED;
		}
	}

	switch (ms_TypeOfPlayerConfirmationCheck)
	{
		case SAVE_CONFIRMATION_NONE :
			return NO_PLAYER_CONFIRMATION_CHECK_IN_PROGRESS;

		case SAVE_CONFIRMATION_DISPLAY_MESSAGE :
		{
			u32 MessageToDisplay = SAVE_GAME_DIALOG_SAVE_WILL_OVERWRITE_SLOT;
			switch (ms_SaveMessageType)
			{
				case SAVE_MESSAGE_SAVE :
					if (CSavegameList::IsSaveGameSlotEmpty(ms_SlotForPlayerConfirmation))
					{
						MessageToDisplay = SAVE_GAME_DIALOG_SAVE_ARE_YOU_SURE;
					}
					else
					{
						MessageToDisplay = SAVE_GAME_DIALOG_SAVE_WILL_OVERWRITE_SLOT;
					}
					break;

				case SAVE_MESSAGE_DELETE :
					MessageToDisplay = SAVE_GAME_DIALOG_DELETE_ARE_YOU_SURE;
					break;

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
				case SAVE_MESSAGE_IMPORT :
					if (CSavegameList::IsSaveGameSlotEmpty(ms_SlotForPlayerConfirmation))
					{
						MessageToDisplay = SAVE_GAME_DIALOG_IMPORT_ARE_YOU_SURE;
					}
					else
					{
						MessageToDisplay = SAVE_GAME_DIALOG_IMPORT_WILL_OVERWRITE_SLOT;
					}
					break;
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES
			}

			s32 slotNumberToDisplay = ms_SlotForPlayerConfirmation + 1;	//	 - FIRST_MANUAL_SLOT;
			Assertf(slotNumberToDisplay > 0, "CSaveConfirmationMessage::Process - slot to display should be greater than 0");
			if (CSavegameDialogScreens::HandleSaveGameDialogBox(MessageToDisplay, slotNumberToDisplay, &ReturnSelection))
			{
				if (ReturnSelection)
				{
					switch (ms_SaveMessageType)
					{
						case SAVE_MESSAGE_DELETE :
							ms_TypeOfPlayerConfirmationCheck = SAVE_CONFIRMATION_NONE;
							ReturnValue = PLAYER_CONFIRMATION_CHECK_FINISHED_BEGIN_DELETING_FROM_SAVE_MENU;
							break;

						case SAVE_MESSAGE_SAVE :
							{
								CPauseMenu::SetRenderMenus(false);
#if RSG_ORBIS
								CSavegameFilenames::MakeValidSaveNameForLocalFile(CSavegameList::ConvertBackupSlotToSavegameSlot(ms_SlotForPlayerConfirmation));
#else
								CSavegameFilenames::MakeValidSaveNameForLocalFile(ms_SlotForPlayerConfirmation);
#endif
								CGenericGameStorage::SetSaveOperation(OPERATION_CHECKING_FOR_FREE_SPACE);
								CSavegameInitialChecks::BeginInitialChecks(false);
								ms_TypeOfPlayerConfirmationCheck = SAVE_CONFIRMATION_DO_INITIAL_CHECKS_FOR_FREE_SPACE;
							}
							break;

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
						case SAVE_MESSAGE_IMPORT :
#if RSG_ORBIS
							CSavegameFilenames::MakeValidSaveNameForLocalFile(CSavegameList::ConvertBackupSlotToSavegameSlot(ms_SlotForPlayerConfirmation));
#else
							CSavegameFilenames::MakeValidSaveNameForLocalFile(ms_SlotForPlayerConfirmation);
#endif
							// The real display name for the imported savegame gets set later when 
							// CSaveGameImport::BeginInitialChecks() calls SetDisplayNameForSavegame()

							// The SAVE_MESSAGE_SAVE case above checks that there is enough free space for the new save
							// I'm not sure if I know the size of the imported savegame at this stage
							// So just return here to allow the import to proceed
							ms_TypeOfPlayerConfirmationCheck = SAVE_CONFIRMATION_NONE;
							ReturnValue = PLAYER_CONFIRMATION_CHECK_FINISHED_BEGIN_IMPORTING;
							break;
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES
					}
				}
				else
				{
					ms_TypeOfPlayerConfirmationCheck = SAVE_CONFIRMATION_NONE;
					ReturnValue = NO_PLAYER_CONFIRMATION_CHECK_IN_PROGRESS;

					if (SAVE_MESSAGE_DELETE == ms_SaveMessageType)
					{
						// Default to save in the quick save menu, so if the player doesn't delete the save and tries to save over it immediately after, it'll work
						CPauseMenu::SetPlayerWantsToSaveASavegame();
					}
				}
			}
		}
			break;

		case SAVE_CONFIRMATION_DO_INITIAL_CHECKS_FOR_FREE_SPACE :
		{
			MemoryCardError SelectionState = CSavegameInitialChecks::InitialChecks();
			if (SelectionState == MEM_CARD_ERROR)
			{
				CPauseMenu::SetRenderMenus(true);
				if (SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT == CSavegameDialogScreens::GetMostRecentErrorCode())
				{
					bPlayerHasSignedOut = true;
				}
				else
				{
					ms_TypeOfPlayerConfirmationCheck = SAVE_CONFIRMATION_HANDLE_ERRORS;
				}
			}
			else if (SelectionState == MEM_CARD_COMPLETE)
			{	//	There is enough free space
//				*pSlotToUse = ms_SlotForPlayerConfirmation;
				ms_TypeOfPlayerConfirmationCheck = SAVE_CONFIRMATION_NONE;
				CGenericGameStorage::SetSaveOperation(OPERATION_NONE);
				ReturnValue = PLAYER_CONFIRMATION_CHECK_FINISHED_BEGIN_SAVING;
			}
		}
		break;

		case SAVE_CONFIRMATION_HANDLE_ERRORS :
			if (CSavegameDialogScreens::HandleSaveGameErrorCode())
			{
				ms_TypeOfPlayerConfirmationCheck = SAVE_CONFIRMATION_NONE;
				CGenericGameStorage::SetSaveOperation(OPERATION_NONE);
				ReturnValue = PLAYER_CONFIRMATION_CHECK_INTERRUPTED_NEED_TO_RESCAN;
			}
			break;

		case SAVE_CONFIRMATION_PLAYER_HAS_SIGNED_OUT :
			bPlayerHasSignedOut = true;
			break;

		case SAVE_CONFIRMATION_STORAGE_DEVICE_REMOVED :
			if (CSavegameDialogScreens::HandleSaveGameDialogBox(SAVE_GAME_DIALOG_SELECTED_DEVICE_HAS_BEEN_REMOVED, 0, &ReturnSelection))
			{
				ms_TypeOfPlayerConfirmationCheck = SAVE_CONFIRMATION_NONE;
				CGenericGameStorage::SetSaveOperation(OPERATION_NONE);
				ReturnValue = PLAYER_CONFIRMATION_CHECK_INTERRUPTED_NEED_TO_RESCAN;
			}
			break;

		default :
			Assertf(0, "CSaveConfirmationMessage::Process - unknown ms_TypeOfPlayerConfirmationCheck");
			break;
	}

	if (bPlayerHasSignedOut)
	{
		ms_TypeOfPlayerConfirmationCheck = SAVE_CONFIRMATION_NONE;
		CGenericGameStorage::SetSaveOperation(OPERATION_NONE);
		ReturnValue = PLAYER_CONFIRMATION_CHECK_INTERRUPTED_PLAYER_HAS_SIGNED_OUT;
	}

	return ReturnValue;
}


