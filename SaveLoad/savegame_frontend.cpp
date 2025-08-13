
// Game headers
#include "SaveLoad/savegame_frontend.h"

#include "audio/frontendaudioentity.h"
#include "camera/viewports/ViewportManager.h"
#include "frontend/PauseMenu.h"
#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/savegame_filenames.h"
#include "SaveLoad/savegame_initial_checks.h"
#include "SaveLoad/savegame_list.h"
#include "SaveLoad/savegame_messages.h"
#include "SaveLoad/savegame_save.h"
#include "SaveLoad/savegame_users_and_devices.h"
#include "Text/Messages.h"
#include "Text/TextConversion.h"


SAVEGAME_OPTIMISATIONS()


s32 CSavegameFrontEnd::ms_SaveGameSlotToLoad = -1;
s32 CSavegameFrontEnd::ms_SaveGameSlotToDelete = -1;

CSavegameFrontEnd::eSavegameMenuFunction CSavegameFrontEnd::ms_SavegameMenuFunction = SAVEGAME_MENU_FUNCTION_LOAD;

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
s32 CSavegameFrontEnd::ms_SaveGameSlotToExport = -1;
char CSavegameFrontEnd::sm_MigrationInfoTextBuffer[MAX_CHARS_IN_EXTENDED_MESSAGE];
bool CSavegameFrontEnd::sm_bAllowAcceptButtonOnMigrationInfoScreen = true;
CSaveGameExport::eNetworkProblemsForSavegameExport CSavegameFrontEnd::sm_NetworkProblemForSavegameExport = CSaveGameExport::SAVEGAME_EXPORT_NETWORK_PROBLEM_NONE;
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
s32 CSavegameFrontEnd::ms_SaveGameSlotToImport = -1;
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

#define MAX_SAVEGAMES_PER_FRONTEND_PAGE	(TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES)

const int NumberOfMenuItemsBeforeSavegameList = 1;	//	The first menu item displays the current and maximum number of saves. In the Save menu, it can be selected to create a new save.
													//	If NumberOfMenuItemsBeforeSavegameList gets set to 0 then I'll still need to
													//	deal with problems with displaying the Save menu when there are no saves.




void CSavegameFrontEnd::SetLoadMenuContext(eLoadMenuInstructionalButtonContexts loadMenuContext)
{

	s8 ColumnForSpinner = CPauseMenu::GetColumnForSpinner();
	switch (loadMenuContext)
	{
	case BUTTON_CONTEXT_WAITING_FOR_SIGN_IN :
		SUIContexts::Deactivate("SELECT_STORAGE_DEVICE");
		SUIContexts::Deactivate(DELETE_SAVEGAME_CONTEXT);
		SUIContexts::Deactivate("HIDE_ACCEPTBUTTON");

		CPauseMenu::SetBusySpinner(false, ColumnForSpinner);
		break;
	case BUTTON_CONTEXT_WAITING_FOR_DEVICE_SELECT :
		SUIContexts::Activate("SELECT_STORAGE_DEVICE");
		SUIContexts::Deactivate(DELETE_SAVEGAME_CONTEXT);
		SUIContexts::Activate("HIDE_ACCEPTBUTTON");

		CPauseMenu::SetBusySpinner(false, ColumnForSpinner);
		break;
	case BUTTON_CONTEXT_LOAD_GAME_MENU :
		SUIContexts::Activate("SELECT_STORAGE_DEVICE");

		if (CPauseMenu::IsInLoadGamePanel())
		{
			SUIContexts::Activate(DELETE_SAVEGAME_CONTEXT);
		}
		else
		{
			SUIContexts::Deactivate(DELETE_SAVEGAME_CONTEXT);
		}
		SUIContexts::Deactivate("HIDE_ACCEPTBUTTON");

		CPauseMenu::SetBusySpinner(true, ColumnForSpinner);
		break;

	case BUTTON_CONTEXT_LOAD_GAME_MENU_DELETE_IN_PROGRESS :
		SUIContexts::Deactivate("SELECT_STORAGE_DEVICE");
		SUIContexts::Deactivate(DELETE_SAVEGAME_CONTEXT);
		SUIContexts::Activate("HIDE_ACCEPTBUTTON");
		break;

	case BUTTON_CONTEXT_SAVE_GAME_MENU_WAITING_FOR_MENU_TO_DISPLAY :
		SUIContexts::Deactivate("SELECT_STORAGE_DEVICE");
		SUIContexts::Deactivate(DELETE_SAVEGAME_CONTEXT);
		SUIContexts::Activate("HIDE_ACCEPTBUTTON");
		break;

	case BUTTON_CONTEXT_SAVE_GAME_MENU_IS_DISPLAYING :
		SUIContexts::Activate("SELECT_STORAGE_DEVICE");

		if (ms_CurrentlyHighlightedSlot < 1 ||												// There will never be a valid save file above slot 1 - don't show delete
			ms_CurrentlyHighlightedSlot > ms_SavegameSlotsArrangedByTime.GetCount())		// When a save is deleted, ms_CurrentlyHighlighted slot won't update right away. This check fixes the edge case of deleting the last save - otherwise delete will show over "create new save"
		{
			SUIContexts::Deactivate(DELETE_SAVEGAME_CONTEXT);
		}
		else
		{
			SUIContexts::Activate(DELETE_SAVEGAME_CONTEXT);
		}

		SUIContexts::Deactivate("HIDE_ACCEPTBUTTON");
		break;

	case BUTTON_CONTEXT_SAVE_GAME_MENU_DELETE_IN_PROGRESS :
		SUIContexts::Deactivate("SELECT_STORAGE_DEVICE");
		SUIContexts::Deactivate(DELETE_SAVEGAME_CONTEXT);
		SUIContexts::Activate("HIDE_ACCEPTBUTTON");
		break;

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	case BUTTON_CONTEXT_UPLOAD_MENU_IS_DISPLAYING :
		{
			SUIContexts::Deactivate(DELETE_SAVEGAME_CONTEXT);

			// Copied these two from BUTTON_CONTEXT_LOAD_GAME_MENU
			SUIContexts::Deactivate("HIDE_ACCEPTBUTTON");
			CPauseMenu::SetBusySpinner(true, ColumnForSpinner);
		}
		break;

	case BUTTON_CONTEXT_MIGRATION_INFO_SCREEN :
		{
			SUIContexts::Deactivate("SELECT_STORAGE_DEVICE");
			SUIContexts::Deactivate(DELETE_SAVEGAME_CONTEXT);
			SUIContexts::Deactivate("HIDE_ACCEPTBUTTON");

			CPauseMenu::SetBusySpinner(false, ColumnForSpinner);
		}
		break;

	case BUTTON_CONTEXT_MIGRATION_INFO_SCREEN_NO_ACCEPT :
		{
			SUIContexts::Deactivate("SELECT_STORAGE_DEVICE");
			SUIContexts::Deactivate(DELETE_SAVEGAME_CONTEXT);
			SUIContexts::Activate("HIDE_ACCEPTBUTTON");

			CPauseMenu::SetBusySpinner(false, ColumnForSpinner);
		}
		break;
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES
	}
	CPauseMenu::RedrawInstructionalButtons();
}


CSavegameFrontEnd::CSavegameFrontEnd(CMenuScreen& owner)
	: CMenuBase(owner)
{
}

CSavegameFrontEnd::~CSavegameFrontEnd()
{
}

void CSavegameFrontEnd::LayoutChanged( MenuScreenId OUTPUT_ONLY(iPreviousLayout), MenuScreenId OUTPUT_ONLY(iNewLayout), s32 iUniqueId, eLAYOUT_CHANGED_DIR OUTPUT_ONLY(eDir))
{
	uiDebugf3("CSavegameFrontEnd::LayoutChanged - Prev: %i, New: %i, Unique: %i, Dir: %i", iPreviousLayout.GetValue(), iNewLayout.GetValue(), iUniqueId, eDir);
	SetHighlightedMenuItem(iUniqueId);

	if (CPauseMenu::IsInSaveGameMenus())
	{
		bool hasDeleteButtonVisibilityChanged = false;
		if (iUniqueId == 0)
		{
			// Make sure the Delete button is hidden when the "Create new save" element is highlighted
			if (SUIContexts::IsActive(DELETE_SAVEGAME_CONTEXT))
			{
				SUIContexts::Deactivate(DELETE_SAVEGAME_CONTEXT);
				hasDeleteButtonVisibilityChanged = true;
			}
		}
		else
		{
			if (!SUIContexts::IsActive(DELETE_SAVEGAME_CONTEXT))
			{
				SUIContexts::Activate(DELETE_SAVEGAME_CONTEXT);
				hasDeleteButtonVisibilityChanged = true;
			}
		}

		if (hasDeleteButtonVisibilityChanged)
		{
			CPauseMenu::RedrawInstructionalButtons();
		}

		CPauseMenu::SetPlayerWantsToSaveASavegame();
	}
}

void CSavegameFrontEnd::SetHighlightedMenuItem( s32 iMenuItem )
{
#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	if ( CPauseMenu::IsInSaveGameOrLoadGameMenus() 
	 || CPauseMenu::IsUploadSavegameOptionHighlighted()
	 || CPauseMenu::IsInUploadSavegamePanel() )
#else // __ALLOW_EXPORT_OF_SP_SAVEGAMES
	if ( CPauseMenu::IsInSaveGameOrLoadGameMenus() )
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES
	{
		ms_CurrentlyHighlightedSlot = iMenuItem;
	}
}

s32 CSavegameFrontEnd::GetNumberOfLinesToDisplayInSavegameList(bool /*bSaveMenu*/)
{
	return (MAX_SAVEGAMES_PER_FRONTEND_PAGE + 1);
}


bool CSavegameFrontEnd::BeginSaveGameList()
{
	if (SaveGameListScreenSetup(true, false, false) == MEM_CARD_ERROR)
	{
		return false;
	}

	return true;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	SaveGameMenuCheck
// PURPOSE:	checks for input and handles the new input into the menu list
/////////////////////////////////////////////////////////////////////////////////////
MemoryCardError CSavegameFrontEnd::SaveGameMenuCheck(bool bQuitAsSoonAsNoSavegameOperationsAreRunning)
{
	MemoryCardError ScanStatus = SaveGameListScreenSetup(false, false, bQuitAsSoonAsNoSavegameOperationsAreRunning);
	switch (ScanStatus)
	{
	case MEM_CARD_COMPLETE :
		//	list of save games has been displayed so just carry on
		break;
	case MEM_CARD_BUSY :
		return MEM_CARD_BUSY;	//	still scanning the memory card

	case MEM_CARD_ERROR :
		{
			if (bQuitAsSoonAsNoSavegameOperationsAreRunning)
			{
				savegameDisplayf("CSavegameFrontEnd::SaveGameMenuCheck - backed out of savegame list after SaveGameListScreenSetup() returned MEM_CARD_ERROR");
				CPauseMenu::ResetMenuItemTriggered();
			}
			return MEM_CARD_ERROR;
		}

	default :
		Assertf(0, "CSavegameFrontEnd::SaveGameMenuCheck - unexpected return value from SaveGameListScreenSetup");
		return MEM_CARD_ERROR;
	}

	ePlayerConfirmationReturn PlayerConfirmationResult = NO_PLAYER_CONFIRMATION_CHECK_IN_PROGRESS;
	if (CPauseMenu::IsInSaveGameMenus())
	{
		Assertf(!CLoadConfirmationMessage::IsThereAMessageToProcess(), "CSavegameFrontEnd::SaveGameMenuCheck - there shouldn't be a load confirmation message to process within the save menu");
		PlayerConfirmationResult = CSaveConfirmationMessage::Process();
	}
	else
	{ // GraemeSW - would it be better to have a new CUploadSavegameConfirmationMessage class instead of shoe-horning the Upload code into CLoadConfirmationMessage?
		Assertf(!CSaveConfirmationMessage::IsThereAMessageToProcess(), "CSavegameFrontEnd::SaveGameMenuCheck - there shouldn't be a save confirmation message to process within the load menu");
		PlayerConfirmationResult = CLoadConfirmationMessage::Process();
	}

	switch (PlayerConfirmationResult)
	{
	case NO_PLAYER_CONFIRMATION_CHECK_IN_PROGRESS :
		break;

	case PLAYER_CONFIRMATION_CHECK_IN_PROGRESS :
		return MEM_CARD_BUSY;

	case PLAYER_CONFIRMATION_CHECK_FINISHED_BEGIN_LOADING :
		
		CPauseMenu::CloseAndLoadSavedGame();

		ms_SavegameMenuFunction = SAVEGAME_MENU_FUNCTION_LOAD;

		ms_SaveGameSlotToLoad = CLoadConfirmationMessage::GetSlotIndex();
		CSavegameList::SetEpisodeNumberForTheSaveGameToBeLoadedToThisSlot(ms_SaveGameSlotToLoad);

		return MEM_CARD_COMPLETE;

	case PLAYER_CONFIRMATION_CHECK_FINISHED_BEGIN_DELETING_FROM_LOAD_MENU :

		SetLoadMenuContext(BUTTON_CONTEXT_LOAD_GAME_MENU_DELETE_IN_PROGRESS);

		ms_SavegameMenuFunction = SAVEGAME_MENU_FUNCTION_DELETE;
#if RSG_ORBIS
		ms_SaveGameSlotToDelete = CSavegameList::ConvertBackupSlotToSavegameSlot(CLoadConfirmationMessage::GetSlotIndex());
#else
		ms_SaveGameSlotToDelete = CLoadConfirmationMessage::GetSlotIndex();
#endif

		return MEM_CARD_COMPLETE;

	case PLAYER_CONFIRMATION_CHECK_FINISHED_BEGIN_DELETING_FROM_SAVE_MENU :

		SetLoadMenuContext(BUTTON_CONTEXT_SAVE_GAME_MENU_DELETE_IN_PROGRESS);

		ms_SavegameMenuFunction = SAVEGAME_MENU_FUNCTION_DELETE;
#if RSG_ORBIS
		ms_SaveGameSlotToDelete = CSavegameList::ConvertBackupSlotToSavegameSlot(CSaveConfirmationMessage::GetSlotIndex());
#else
		ms_SaveGameSlotToDelete = CSaveConfirmationMessage::GetSlotIndex();
#endif

		return MEM_CARD_COMPLETE;

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	case PLAYER_CONFIRMATION_CHECK_FINISHED_BEGIN_EXPORTING :
		{
			ms_SavegameMenuFunction = SAVEGAME_MENU_FUNCTION_EXPORT;
			ms_SaveGameSlotToExport = CLoadConfirmationMessage::GetSlotIndex();

			return MEM_CARD_COMPLETE;
		}
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

	case PLAYER_CONFIRMATION_CHECK_FINISHED_BEGIN_SAVING :

		ms_SavegameMenuFunction = SAVEGAME_MENU_FUNCTION_SAVE;
#if RSG_ORBIS
		CSavegameSave::BeginGameSave(CSavegameList::ConvertBackupSlotToSavegameSlot(CSaveConfirmationMessage::GetSlotIndex()), SAVEGAME_SINGLE_PLAYER);
#else
		CSavegameSave::BeginGameSave(CSaveConfirmationMessage::GetSlotIndex(), SAVEGAME_SINGLE_PLAYER);
#endif

		//CPauseMenu::LockMenuControl();
		return MEM_CARD_COMPLETE;

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
	case PLAYER_CONFIRMATION_CHECK_FINISHED_BEGIN_IMPORTING :
		{
			ms_SavegameMenuFunction = SAVEGAME_MENU_FUNCTION_IMPORT;
#if RSG_ORBIS
			ms_SaveGameSlotToImport = CSavegameList::ConvertBackupSlotToSavegameSlot(CSaveConfirmationMessage::GetSlotIndex());
#else
			ms_SaveGameSlotToImport = CSaveConfirmationMessage::GetSlotIndex();
#endif
			return MEM_CARD_COMPLETE;
		}
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

	case PLAYER_CONFIRMATION_CHECK_INTERRUPTED_NEED_TO_RESCAN :
		if (SaveGameListScreenSetup(true, false, false) == MEM_CARD_ERROR)
		{
			Assertf(0, "CSavegameFrontEnd::SaveGameMenuCheck - error occurred in the first call to SaveGameListScreenSetup");
			return MEM_CARD_ERROR;
		}
		return MEM_CARD_BUSY;

	case PLAYER_CONFIRMATION_CHECK_INTERRUPTED_PLAYER_HAS_SIGNED_OUT :
		return MEM_CARD_ERROR;

	default :
		Assertf(0, "CSavegameFrontEnd::SaveGameMenuCheck - unexpected return value from ConfirmationMessage::Process()");
		return MEM_CARD_ERROR;
	}

	//	This function might get called for one extra frame after FRONTEND_MENU_SAVEGAME_LIST has been disabled
	//	by the player cancelling the menu so return before anything attempts to access the menu
	if (CPauseMenu::IsPauseMenuControlDisabled())
		return MEM_CARD_ERROR;	//	not sure what should be returned here yet

	// item has been accepted:
	bool bItemTriggered = CPauseMenu::HasMenuItemBeenTriggered();

	if (bItemTriggered || bQuitAsSoonAsNoSavegameOperationsAreRunning)
	{
		s32 menu_item = CPauseMenu::GetMenuItemTriggered();
		CPauseMenu::ResetMenuItemTriggered();  // maybe rework this?

		if (bQuitAsSoonAsNoSavegameOperationsAreRunning)
		{
			savegameDisplayf("CSavegameFrontEnd::SaveGameMenuCheck - backed out of savegame list");
			return MEM_CARD_ERROR;
		}
		else
		{
			if ( menu_item == 0 )	//	&& (NumberOfMenuItemsBeforeSavegameList == 1) )
			{
				// If there are only damaged saves in this screen of the Load menu then the first item will be
				//	highlighted. Don't do anything if the player selects it though.
				if (CPauseMenu::IsInSaveGameMenus())
				{
					s32 FreeSaveSlot = CSavegameList::FindFreeManualSaveSlot();
					if (savegameVerifyf(FreeSaveSlot >= 0, "CSavegameFrontEnd::SaveGameMenuCheck - failed to find a free manual save slot"))
					{
#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
						if (CPauseMenu::PlayerHasChosenToImportASavegame())
						{
							CSaveConfirmationMessage::Begin(FreeSaveSlot, CSaveConfirmationMessage::SAVE_MESSAGE_IMPORT);
						}
						else
#endif	//	__ALLOW_IMPORT_OF_SP_SAVEGAMES
						{
							CSaveConfirmationMessage::Begin(FreeSaveSlot, CSaveConfirmationMessage::SAVE_MESSAGE_SAVE);
						}
					}
				}
			}
			else
			{
				if (CPauseMenu::IsInSaveGameMenus())
				{
					if (CPauseMenu::PlayerHasChosenToDeleteASavegame())
					{
						CSaveConfirmationMessage::Begin(ConvertMenuItemToSlotIndex(menu_item), CSaveConfirmationMessage::SAVE_MESSAGE_DELETE);
					}
					else
					{
#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
						if (CPauseMenu::PlayerHasChosenToImportASavegame())
						{
							CSaveConfirmationMessage::Begin(ConvertMenuItemToSlotIndex(menu_item), CSaveConfirmationMessage::SAVE_MESSAGE_IMPORT);
						}
						else
#endif	//	__ALLOW_IMPORT_OF_SP_SAVEGAMES
						{
							CSaveConfirmationMessage::Begin(ConvertMenuItemToSlotIndex(menu_item), CSaveConfirmationMessage::SAVE_MESSAGE_SAVE);
						}
					}
				}
				else
				{
					if (CPauseMenu::PlayerHasChosenToDeleteASavegame())
					{
						CLoadConfirmationMessage::Begin(ConvertMenuItemToSlotIndex(menu_item), CLoadConfirmationMessage::LOAD_MESSAGE_DELETE);
					}
					else
					{
#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
						if (CPauseMenu::PlayerHasChosenToExportASavegame())
						{
							CLoadConfirmationMessage::Begin(ConvertMenuItemToSlotIndex(menu_item), CLoadConfirmationMessage::LOAD_MESSAGE_EXPORT);
						}
						else
#endif	//	__ALLOW_EXPORT_OF_SP_SAVEGAMES
						{
							s32 slotToLoad = ConvertMenuItemToSlotIndex(menu_item);
#if __BANK && RSG_ORBIS
							if (CGenericGameStorage::ms_bAlwaysLoadBackupSaves)
							{
								if (slotToLoad < TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES)
								{
									if (!CSavegameList::IsSaveGameSlotEmpty(slotToLoad + TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES))
									{
										slotToLoad += TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES;
									}
								}
								else
								{
									savegameAssertf( (slotToLoad >= TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES) && (slotToLoad < (TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES + NUM_BACKUPSAVE_SLOTS) ), "CSavegameFrontEnd::SaveGameMenuCheck - expected slot to be between 0 and %u", (TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES + NUM_BACKUPSAVE_SLOTS));
								}
							}
#endif	//	__BANK && RSG_ORBIS
							CLoadConfirmationMessage::Begin(slotToLoad, CLoadConfirmationMessage::LOAD_MESSAGE_LOAD);
						}
					}
				}
			}
		}
	}

	return MEM_CARD_BUSY;
}


atFixedArray<s32, TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES> CSavegameFrontEnd::ms_SavegameSlotsArrangedByTime;
s32 CSavegameFrontEnd::ms_BaseMenuItem = 0;
s32 CSavegameFrontEnd::ms_CurrentlyHighlightedSlot = 0;

bool CSavegameFrontEnd::ms_bNeedToRestartDamagedCheck = true;

// CSavegameFrontEnd::CreateListSortedByDate - returns false if we're in the Load menu and no valid saves were found to display
bool CSavegameFrontEnd::CreateListSortedByDate()
{
	ms_SavegameSlotsArrangedByTime.Reset();

	ms_bNeedToRestartDamagedCheck = true;

	//	We could have an array of flags to remember which files we'd already checked for damage
	//	so that the damaged check doesn't need to be run again if the same page of the list is displayed again.
	//  This array could be reset here.

	s32 MaxSlotsToSort = TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES;
	if (CPauseMenu::IsInSaveGameMenus())
	{	//	Ignore the autosave slots in the save menu
		MaxSlotsToSort = NUM_MANUAL_SAVE_SLOTS;
	}

	const u8 ALREADY_ADDED_TO_SORTED_ARRAY = 1 << 0;	//	Originally I thought I might flag damaged slots too
	u8 FlagsForSlots[TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES];	//	so I thought I'd make FlagsForSlots an array of bit-fields instead of an array of bools
	sysMemSet(FlagsForSlots, 0, TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES);

	CDate MostRecentSaveDate;
	s32 MostRecentSaveSlot = -1;

	bool bFinished = false;

	//	Add slot indices of saves to array ordered by time with most recent save in array index 0
	while (!bFinished)
	{
		MostRecentSaveDate.Initialise();
		MostRecentSaveSlot = -1;

		s32 slot_loop = 0;
		while (slot_loop < MaxSlotsToSort)
		{
			if ( (FlagsForSlots[slot_loop] & ALREADY_ADDED_TO_SORTED_ARRAY) == 0)
			{
				if (!CSavegameList::IsSaveGameSlotEmpty(slot_loop) 
#if RSG_ORBIS
					|| !CSavegameList::IsSaveGameSlotEmpty(slot_loop + INDEX_OF_BACKUPSAVE_SLOT)
#endif
					)
				{
					//possible that only a backup save exists if the main save was corrupt and deleted
					//in that case, use the backup slot
					s32 activeSlot = slot_loop;
					bool bConsiderThisSlot = true;
#if RSG_ORBIS
					if(CSavegameList::IsSaveGameSlotEmpty(activeSlot))
					{
#if USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP
						if (CSavegameList::IsThisAnAutosaveSlot(activeSlot))
						{	//	Ignore download0 backups of autosaves if the main autosave doesn't exist
							savegameDisplayf("CSavegameFrontEnd::CreateListSortedByDate - autosave slot %d is empty so don't consider the download0 backup", activeSlot);
							bConsiderThisSlot = false;
						}
						else
#endif	//	USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP
						{
							activeSlot += INDEX_OF_BACKUPSAVE_SLOT;
						}
					}
#endif
					if (bConsiderThisSlot)
					{
//						if (!CSavegameList::GetIsDamaged(slot_loop))	no point checking this since I've not checked if the slot is damaged yet
						{
							if (CSavegameList::GetSaveTime(activeSlot) > MostRecentSaveDate)
							{
								MostRecentSaveDate = CSavegameList::GetSaveTime(activeSlot);
								MostRecentSaveSlot = activeSlot;
							}
						}
					}
				}
			}

			slot_loop++;
		}

		bFinished = true;
		if (MostRecentSaveSlot >= 0)
		{
			bFinished = false;
			ms_SavegameSlotsArrangedByTime.Push(MostRecentSaveSlot);
#if RSG_ORBIS
			FlagsForSlots[CSavegameList::ConvertBackupSlotToSavegameSlot(MostRecentSaveSlot)] |= ALREADY_ADDED_TO_SORTED_ARRAY;
#else
			FlagsForSlots[MostRecentSaveSlot] |= ALREADY_ADDED_TO_SORTED_ARRAY;
#endif
		}
	}

	if (!CPauseMenu::IsInSaveGameMenus())
	{	//	We are in the Load menu or the Upload Savegame menu
		if (ms_SavegameSlotsArrangedByTime.GetCount() == 0)
		{	//	haven't found any good saves so display the error
 			CPauseMenu::SetNoValidSaveGameFiles(true);

			//CPauseMenu::Close();
//			CFrontEnd::DisplayNoSaveGamesMessageInMilliseconds = 5000;
			g_FrontendAudioEntity.PlaySound("ERROR","HUD_FRONTEND_DEFAULT_SOUNDSET");

			CSavegameDevices::SetAllowDeviceHasBeenRemovedMessageToBeDisplayed(false);
			CSavegameDevices::SetDeviceIsInvalid(false);

			return false;
		}
// 		else
// 		{
// 			CFrontEnd::DisplayNoSaveGamesMessageInMilliseconds = 0;
// 		}
	}

	return true;
}


bool CSavegameFrontEnd::AddOneItemToSaveGameList(s32 MenuItem)
{
	bool bFoundValidData = false;

	bool SlotIsEmpty = false;
	bool SlotIsDamaged = false;

	char cSaveGameName[SAVE_GAME_MAX_DISPLAY_NAME_LENGTH];
	char cSaveGameDate[MAX_MENU_ITEM_CHAR_LENGTH];

	if ( (MenuItem < NumberOfMenuItemsBeforeSavegameList) || (MenuItem > (MAX_SAVEGAMES_PER_FRONTEND_PAGE + NumberOfMenuItemsBeforeSavegameList) ) )
	{
		savegameAssertf(0, "CSavegameFrontEnd::AddOneItemToSaveGameList should be called with an int in the range %d to %d", NumberOfMenuItemsBeforeSavegameList, (MAX_SAVEGAMES_PER_FRONTEND_PAGE + NumberOfMenuItemsBeforeSavegameList) );
		return false;
	}

	s32 SlotIndex = ConvertMenuItemToSlotIndex(MenuItem);

	CSavegameList::GetDisplayNameAndDateForThisSaveGameItem(SlotIndex, cSaveGameName, cSaveGameDate, &SlotIsEmpty, &SlotIsDamaged);

	if (savegameVerifyf(!SlotIsEmpty, "CSavegameFrontEnd::AddOneItemToSaveGameList - sorting by time should ignore empty slots so shouldn't find any at this stage"))
	{
		if (SlotIsDamaged)
		{
			char gxtSlotIsDamagedText[MAX_MENU_ITEM_CHAR_LENGTH];
			char *pGxtFromTextFile = TheText.Get("SG_DAM");

			CNumberWithinMessage NumbersArray[1];
			NumbersArray[0].Set((ms_BaseMenuItem + MenuItem + (1 - NumberOfMenuItemsBeforeSavegameList) ));

			CMessages::InsertNumbersAndSubStringsIntoString(pGxtFromTextFile, 
				NumbersArray, 1, 
				NULL, 0,
				gxtSlotIsDamagedText, NELEM(gxtSlotIsDamagedText));

			bool bActivated = true;

//	Bug 2006658 - The Load menu now has a Delete button. Don't grey out damaged savegames so that they can be selected for deletion.
//					If the player attempts to load a damaged save, they'll get a "The selected save game data cannot be loaded because it is damaged." warning message.
// 			if (!CPauseMenu::IsInSaveGameMenus())
// 			{	//	damaged slots should be greyed out in the load menu but not in the save menu
// 				bActivated = false;
// 			}

			CPauseMenu::InsertSaveGameIntoMenu(MenuItem, gxtSlotIsDamagedText, NULL, bActivated);
		}
		else
		{
			bFoundValidData = true;

			// lets cap the amount of characters at run time:
			s32 MaxCharactersToDisplay = 64;

			char cSaveGameNameWithMenuItemNumber[SAVE_GAME_MAX_DISPLAY_NAME_LENGTH];
			sprintf(cSaveGameNameWithMenuItemNumber, "%02d - ", (MenuItem + ms_BaseMenuItem) );

			s32 numberOfCharactersInMenuItemNumber = CountUtf8Characters(cSaveGameNameWithMenuItemNumber);
			s32 numberOfBytesInMenuItemNumber = istrlen(cSaveGameNameWithMenuItemNumber);	//	There shouldn't be any multi-byte characters in this part so numberOfBytesInMenuItemNumber should equal numberOfCharactersInMenuItemNumber

			s32 numberOfCharactersInSaveGameName = CountUtf8Characters(cSaveGameName);
			s32 numberOfBytesInSaveGameName = istrlen(cSaveGameName);

			bool bStringDoesntFit = false;
			if ((numberOfCharactersInMenuItemNumber + numberOfCharactersInSaveGameName) > MaxCharactersToDisplay)
			{
				bStringDoesntFit = true;
			}

			if ((numberOfBytesInMenuItemNumber + numberOfBytesInSaveGameName) >= NELEM(cSaveGameNameWithMenuItemNumber))
			{
				bStringDoesntFit = true;
			}

			s32 maxBytesToCopy = NELEM(cSaveGameNameWithMenuItemNumber) - numberOfBytesInMenuItemNumber;
			if (bStringDoesntFit)
			{
				maxBytesToCopy -= 3;		//	for the three '.' characters
			}

			s32 maxCharactersToCopy = (MaxCharactersToDisplay - numberOfCharactersInMenuItemNumber);

			if ( (maxCharactersToCopy > 0) && (maxBytesToCopy > 0) )
			{
				CTextConversion::StrcpyWithCharacterAndByteLimits(&cSaveGameNameWithMenuItemNumber[numberOfBytesInMenuItemNumber], cSaveGameName, maxCharactersToCopy, maxBytesToCopy);
			}

			if (bStringDoesntFit)
			{
				s32 numberOfBytesInFullString = istrlen(cSaveGameNameWithMenuItemNumber);

				if (Verifyf(numberOfBytesInFullString < (NELEM(cSaveGameNameWithMenuItemNumber)-3), 
					"CSavegameFrontEnd::AddOneItemToSaveGameList - expected there to still be space to append three dot characters at the end of this string. Current length is %d. Maximum length is %d", 
					numberOfBytesInFullString, NELEM(cSaveGameNameWithMenuItemNumber)))
				{
					strcat(cSaveGameNameWithMenuItemNumber, "...");
				}
			}

//			CMenuSystem::InsertOneMenuItem(CFrontEnd::GetMenuID(FRONTEND_MENU_SAVEGAME_LIST), 0, MenuItem, cSaveGameNameWithMenuItemNumber, false);
//			CMenuSystem::InsertOneMenuItem(CFrontEnd::GetMenuID(FRONTEND_MENU_SAVEGAME_LIST), 1, MenuItem, cSaveGameDate, false);

			CPauseMenu::InsertSaveGameIntoMenu(MenuItem, cSaveGameNameWithMenuItemNumber, cSaveGameDate, true);
		}
	}

	return bFoundValidData;
}

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
#if __NO_OUTPUT
#define SAVEGAME_FRONTEND_CHECK_TEXT_LABEL_EXISTS(textLabel)
#else
#define SAVEGAME_FRONTEND_CHECK_TEXT_LABEL_EXISTS(textLabel) \
if (!TheText.DoesTextLabelExist(textLabel))\
{\
	savegameErrorf("Can't find Key %s in the text file", textLabel);\
	savegameAssertf(0, "Can't find Key %s in the text file", textLabel);\
}
#endif
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	SaveGameListScreenSetup
// PURPOSE:	sets up the savegame list menu
/////////////////////////////////////////////////////////////////////////////////////
MemoryCardError CSavegameFrontEnd::SaveGameListScreenSetup(bool bRescanMemoryCard, bool bForceDeviceSelection, bool bForceCloseWhenSafe)
{
	enum eScreenSetupProgress
	{
		SCREEN_SETUP_PROGRESS_DO_NOTHING,
#if RSG_PC || RSG_DURANGO
		SCREEN_SETUP_PROGRESS_WAIT_FOR_USER_TO_PRESS_A_BUTTON,
#endif	// RSG_PC || RSG_DURANGO

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
		SCREEN_SETUP_PROGRESS_DISPLAY_MIGRATION_INFO_SCREEN,
		SCREEN_SETUP_PROGRESS_WAIT_ON_MIGRATION_INFO_SCREEN,
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

		SCREEN_SETUP_PROGRESS_BEGIN_INITIAL_CHECKS_FOR_LOADING,
		SCREEN_SETUP_PROGRESS_DO_INITIAL_CHECKS,
		SCREEN_SETUP_PROGRESS_SCAN_FOR_DAMAGES,
		SCREEN_SETUP_PROGRESS_HANDLE_ERRORS
	};
	static eScreenSetupProgress SetupProgress = SCREEN_SETUP_PROGRESS_DO_NOTHING;

	MemoryCardError returnStatus = MEM_CARD_BUSY;

	if (bRescanMemoryCard)
	{
		if (!bForceCloseWhenSafe)
		{
			if (ms_SavegameSlotsArrangedByTime.GetCount() > 0)
			{
				// Only notify the pause menu that there are valid save games if that is the case. Otherwise, a flicker will occur in the instructional buttons when selecting
				// load game, as CPauseMenu::SetNoValidSaveGameFiles now updates the SUIContexts/instructional buttons to reflect not having save games.
				CPauseMenu::SetNoValidSaveGameFiles(false);
			}

			CSavegameList::ClearSlotData();
			CSavegameList::ClearNumberOfSaveGameSlots();
			CSavegameList::ResetFileTimeScan();

			CPauseMenu::ClearSaveGameMenu();

			CLoadConfirmationMessage::Clear();
			CSaveConfirmationMessage::Clear();

			if (CPauseMenu::IsInSaveGameMenus())
			{
				CGenericGameStorage::SetSaveOperation(OPERATION_SCANNING_CONSOLE_FOR_SAVING_SAVEGAMES);

				SetLoadMenuContext(BUTTON_CONTEXT_SAVE_GAME_MENU_WAITING_FOR_MENU_TO_DISPLAY);

				CSavegameInitialChecks::BeginInitialChecks(bForceDeviceSelection);
				SetupProgress = SCREEN_SETUP_PROGRESS_DO_INITIAL_CHECKS;
			}
			else
			{
				CGenericGameStorage::SetSaveOperation(OPERATION_SCANNING_CONSOLE_FOR_LOADING_SAVEGAMES);

				bool bPlayerIsSignedIn = false;
#if RSG_PC || RSG_DURANGO
				if (CSavegameUsers::GetSignedInUser())
				{
					bPlayerIsSignedIn = true;
				}
				else
				{
					SetLoadMenuContext(BUTTON_CONTEXT_WAITING_FOR_SIGN_IN);

					CScaleformMenuHelper::SHOW_WARNING_MESSAGE(PM_COLUMN_MIDDLE, 2, TheText.Get("PM_SIGN_IN"), TheText.Get("PM_PANE_LOA"));
					SetupProgress = SCREEN_SETUP_PROGRESS_WAIT_FOR_USER_TO_PRESS_A_BUTTON;
				}
#else	//	do the following if not RSG_PC or RSG_DURANGO
				bPlayerIsSignedIn = true;
#endif	//	RSG_PC || RSG_DURANGO

				if (bPlayerIsSignedIn)
				{
#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
					if (CPauseMenu::IsUploadSavegameOptionHighlighted() || CPauseMenu::IsInUploadSavegamePanel())
					{
						SetupProgress = SCREEN_SETUP_PROGRESS_DISPLAY_MIGRATION_INFO_SCREEN;
					}
					else
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES
					{
						SetupProgress = SCREEN_SETUP_PROGRESS_BEGIN_INITIAL_CHECKS_FOR_LOADING;
					}
				}
			}
		}
	}
	else
	{
		switch (SetupProgress)
		{
		case SCREEN_SETUP_PROGRESS_DO_NOTHING :
			return MEM_CARD_COMPLETE;	//	Don't set returnStatus to MEM_CARD_COMPLETE. Just return here to avoid any of the extra code that is done at the bottom of this function when it has just completed.
			break;

#if RSG_PC || RSG_DURANGO
		case SCREEN_SETUP_PROGRESS_WAIT_FOR_USER_TO_PRESS_A_BUTTON :
			if ( (CSavegameUsers::GetSignedInUser()) 
				|| (CPauseMenu::CheckInput(FRONTEND_INPUT_ACCEPT, true, (CHECK_INPUT_OVERRIDE_FLAG_WARNING_MESSAGE | CHECK_INPUT_OVERRIDE_FLAG_STORAGE_DEVICE | CHECK_INPUT_OVERRIDE_FLAG_RESTART_SAVED_GAME_STATE) )) )
			{
				CScaleformMenuHelper::SET_WARNING_MESSAGE_VISIBILITY(false);

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
				if (CPauseMenu::IsUploadSavegameOptionHighlighted() || CPauseMenu::IsInUploadSavegamePanel())
				{
					SetupProgress = SCREEN_SETUP_PROGRESS_DISPLAY_MIGRATION_INFO_SCREEN;
				}
				else
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES
				{
					SetupProgress = SCREEN_SETUP_PROGRESS_BEGIN_INITIAL_CHECKS_FOR_LOADING;
				}
			}
			break;
#endif	//	RSG_PC || RSG_DURANGO


#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
		case SCREEN_SETUP_PROGRESS_DISPLAY_MIGRATION_INFO_SCREEN :
			{
				bool bDisplayInfoMessage = true;
				sm_bAllowAcceptButtonOnMigrationInfoScreen = true;

				sm_NetworkProblemForSavegameExport = CSaveGameExport::CheckForNetworkProblems();

				if (sm_NetworkProblemForSavegameExport != CSaveGameExport::SAVEGAME_EXPORT_NETWORK_PROBLEM_NONE)
				{
					savegameDebugf1("SaveGameListScreenSetup - DISPLAY_MIGRATION_INFO_SCREEN - network problem is %s", 
						CSaveGameExport::GetNetworkProblemName(sm_NetworkProblemForSavegameExport));

					switch (sm_NetworkProblemForSavegameExport)
					{
					case CSaveGameExport::SAVEGAME_EXPORT_NETWORK_PROBLEM_NONE :
						// Can't reach this state
						break;

					case CSaveGameExport::SAVEGAME_EXPORT_NETWORK_PROBLEM_SIGNED_OUT_LOCALLY :
						{
							SAVEGAME_FRONTEND_CHECK_TEXT_LABEL_EXISTS("SG_PROC_NET_1")
							safecpy(sm_MigrationInfoTextBuffer, TheText.Get(ATSTRINGHASH("SG_PROC_NET_1", 0x1AF4BAA2), ""));
							sm_bAllowAcceptButtonOnMigrationInfoScreen = false;
						}
						break;

					case CSaveGameExport::SAVEGAME_EXPORT_NETWORK_PROBLEM_NO_NETWORK_CONNECTION :
						{
							SAVEGAME_FRONTEND_CHECK_TEXT_LABEL_EXISTS("SG_PROC_NET_2")
							safecpy(sm_MigrationInfoTextBuffer, TheText.Get(ATSTRINGHASH("SG_PROC_NET_2", 0x29850C), ""));
							sm_bAllowAcceptButtonOnMigrationInfoScreen = false;
						}
						break;

					case CSaveGameExport::SAVEGAME_EXPORT_NETWORK_PROBLEM_AGE_RESTRICTED :
						{
							SAVEGAME_FRONTEND_CHECK_TEXT_LABEL_EXISTS("SG_PROC_NET_3")
							safecpy(sm_MigrationInfoTextBuffer, TheText.Get(ATSTRINGHASH("SG_PROC_NET_3", 0x2DE1607B), ""));
							sm_bAllowAcceptButtonOnMigrationInfoScreen = false;
						}
						break;

					case CSaveGameExport::SAVEGAME_EXPORT_NETWORK_PROBLEM_SIGNED_OUT_ONLINE :
						{
							SAVEGAME_FRONTEND_CHECK_TEXT_LABEL_EXISTS("SG_PROC_NET_4")
							safecpy(sm_MigrationInfoTextBuffer, TheText.Get(ATSTRINGHASH("SG_PROC_NET_4", 0x240F4CD7), ""));
							sm_bAllowAcceptButtonOnMigrationInfoScreen = false;
						}
						break;

					case CSaveGameExport::SAVEGAME_EXPORT_NETWORK_PROBLEM_SOCIAL_CLUB_UNAVAILABLE :
						{
							SAVEGAME_FRONTEND_CHECK_TEXT_LABEL_EXISTS("SG_PROC_NET_5")
							safecpy(sm_MigrationInfoTextBuffer, TheText.Get(ATSTRINGHASH("SG_PROC_NET_5", 0xD7F734A8), ""));
							sm_bAllowAcceptButtonOnMigrationInfoScreen = false;
						}
						break;

					case CSaveGameExport::SAVEGAME_EXPORT_NETWORK_PROBLEM_NOT_SIGNED_IN_TO_SOCIAL_CLUB :
						{
							SAVEGAME_FRONTEND_CHECK_TEXT_LABEL_EXISTS("SG_PROC_NET_6")
							safecpy(sm_MigrationInfoTextBuffer, TheText.Get(ATSTRINGHASH("SG_PROC_NET_6", 0xC5C59045), ""));
							sm_bAllowAcceptButtonOnMigrationInfoScreen = false;
						}
						break;

					case CSaveGameExport::SAVEGAME_EXPORT_NETWORK_PROBLEM_SOCIAL_CLUB_POLICIES_OUT_OF_DATE :
						{
							SAVEGAME_FRONTEND_CHECK_TEXT_LABEL_EXISTS("SG_PROC_NET_7")
							safecpy(sm_MigrationInfoTextBuffer, TheText.Get(ATSTRINGHASH("SG_PROC_NET_7", 0xFC9CFDF3), ""));
							sm_bAllowAcceptButtonOnMigrationInfoScreen = false;
						}
						break;
					}
				}
				else if (CSaveGameExport::GetTransferAlreadyCompleted())
				{
					SAVEGAME_FRONTEND_CHECK_TEXT_LABEL_EXISTS("SG_PROC_INTRO3")
					safecpy(sm_MigrationInfoTextBuffer, TheText.Get(ATSTRINGHASH("SG_PROC_INTRO3", 0xD04E3B52), ""));
					sm_bAllowAcceptButtonOnMigrationInfoScreen = false;
				}
				else if (CSaveGameExport::GetHaveCheckedForPreviousUpload())
				{
					if (CSaveGameExport::GetHasASavegameBeenPreviouslyUploaded())
					{
						SAVEGAME_FRONTEND_CHECK_TEXT_LABEL_EXISTS("SG_PROC_INTRO2")
						char *pMainString = TheText.Get(ATSTRINGHASH("SG_PROC_INTRO2", 0x12F8C0A6), "");

 						CSubStringWithinMessage SubStringArray[2];
 						u32 NumberOfSubStringsToInsert = 2;
						SubStringArray[0].SetCharPointer(CSaveGameExport::GetMissionNameOfPreviousUpload());
						SubStringArray[1].SetCharPointer(CSaveGameExport::GetSaveTimeStringOfPreviousUpload());

						CMessages::InsertNumbersAndSubStringsIntoString(pMainString, 
							NULL, 0,		// no numbers to insert
							SubStringArray, NumberOfSubStringsToInsert, 
							sm_MigrationInfoTextBuffer, MAX_CHARS_IN_EXTENDED_MESSAGE);
					}
					else
					{
						SAVEGAME_FRONTEND_CHECK_TEXT_LABEL_EXISTS("SG_PROC_INTRO1")
						safecpy(sm_MigrationInfoTextBuffer, TheText.Get(ATSTRINGHASH("SG_PROC_INTRO1", 0x28C2EC3A), ""));
					}
				}
				else
				{ // If we haven't yet checked for a previously-uploaded savegame then don't display any info message.
					// Should I instead display SG_PROC_INTRO1 as if there is no previous upload?
					// Or should I run the check now?
					bDisplayInfoMessage = false;
				}


				if (bDisplayInfoMessage)
				{
					if (sm_bAllowAcceptButtonOnMigrationInfoScreen)
					{
						SetLoadMenuContext(BUTTON_CONTEXT_MIGRATION_INFO_SCREEN);
					}
					else
					{
						SetLoadMenuContext(BUTTON_CONTEXT_MIGRATION_INFO_SCREEN_NO_ACCEPT);
					}

					u32 hashOfTitleTextKey = ATSTRINGHASH("PM_PANE_PROC", 0x92FB6209);

					CScaleformMenuHelper::SHOW_WARNING_MESSAGE(PM_COLUMN_MIDDLE, 2, sm_MigrationInfoTextBuffer, TheText.Get(hashOfTitleTextKey, ""));
					SetupProgress = SCREEN_SETUP_PROGRESS_WAIT_ON_MIGRATION_INFO_SCREEN;
				}
				else
				{
					SetupProgress = SCREEN_SETUP_PROGRESS_BEGIN_INITIAL_CHECKS_FOR_LOADING;
				}
			}
			break;

		case SCREEN_SETUP_PROGRESS_WAIT_ON_MIGRATION_INFO_SCREEN :
			{
				CSaveGameExport::eNetworkProblemsForSavegameExport currentNetworkProblem = CSaveGameExport::CheckForNetworkProblems();

				if (currentNetworkProblem != sm_NetworkProblemForSavegameExport)
				{
					savegameDebugf1("SaveGameListScreenSetup - WAIT_ON_MIGRATION_INFO_SCREEN - network problem was %s. Now it's %s. Return to DISPLAY_MIGRATION_INFO_SCREEN", 
						CSaveGameExport::GetNetworkProblemName(sm_NetworkProblemForSavegameExport), CSaveGameExport::GetNetworkProblemName(currentNetworkProblem));

					CScaleformMenuHelper::SET_WARNING_MESSAGE_VISIBILITY(false);

					SetupProgress = SCREEN_SETUP_PROGRESS_DISPLAY_MIGRATION_INFO_SCREEN;
				}
				else
				{
					if (sm_bAllowAcceptButtonOnMigrationInfoScreen)
					{
						if (CPauseMenu::CheckInput(FRONTEND_INPUT_ACCEPT, true, (CHECK_INPUT_OVERRIDE_FLAG_WARNING_MESSAGE | CHECK_INPUT_OVERRIDE_FLAG_STORAGE_DEVICE) ))	//	 | CHECK_INPUT_OVERRIDE_FLAG_RESTART_SAVED_GAME_STATE
						{
							CScaleformMenuHelper::SET_WARNING_MESSAGE_VISIBILITY(false);

							SetupProgress = SCREEN_SETUP_PROGRESS_BEGIN_INITIAL_CHECKS_FOR_LOADING;
						}
					}
				}
			}
			break;
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

		case SCREEN_SETUP_PROGRESS_BEGIN_INITIAL_CHECKS_FOR_LOADING :
			{
#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
				if (CPauseMenu::IsUploadSavegameOptionHighlighted() || CPauseMenu::IsInUploadSavegamePanel())
				{
					SetLoadMenuContext(BUTTON_CONTEXT_UPLOAD_MENU_IS_DISPLAYING);
				}
				else
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES
				{
					SetLoadMenuContext(BUTTON_CONTEXT_LOAD_GAME_MENU);
				}

				CSavegameInitialChecks::BeginInitialChecks(bForceDeviceSelection);
				SetupProgress = SCREEN_SETUP_PROGRESS_DO_INITIAL_CHECKS;
			}
//			break;	// Deliberately fall through to SCREEN_SETUP_PROGRESS_DO_INITIAL_CHECKS

		case SCREEN_SETUP_PROGRESS_DO_INITIAL_CHECKS :
			{
				MemoryCardError InitialChecksReturnStatus = CSavegameInitialChecks::InitialChecks();
				switch (InitialChecksReturnStatus)
				{
				case MEM_CARD_BUSY :
					bForceCloseWhenSafe = false;	//	This can't be interrupted
					break;

				case MEM_CARD_COMPLETE :
					if (CreateListSortedByDate())
					{
						ms_BaseMenuItem	= 0;
					}

					SetupProgress = SCREEN_SETUP_PROGRESS_SCAN_FOR_DAMAGES;
					break;

				case MEM_CARD_ERROR :
					if (SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT == CSavegameDialogScreens::GetMostRecentErrorCode())
					{
						returnStatus = MEM_CARD_ERROR;
					}
					else
					{
						SetupProgress = SCREEN_SETUP_PROGRESS_HANDLE_ERRORS;
					}
					break;
				}	//	switch (InitialChecksReturnStatus)
			}
			break;	//	case SCREEN_SETUP_PROGRESS_DO_INITIAL_CHECKS

		case SCREEN_SETUP_PROGRESS_SCAN_FOR_DAMAGES :
			{
				MemoryCardError ScanStatus = CheckVisibleFilesForDamage(bForceCloseWhenSafe);
				switch (ScanStatus)
				{
				case MEM_CARD_COMPLETE :
					if (CPauseMenu::IsInSaveGameMenus())
					{
						SetLoadMenuContext(BUTTON_CONTEXT_SAVE_GAME_MENU_IS_DISPLAYING);
					}

					returnStatus = MEM_CARD_COMPLETE;
					break;

				case MEM_CARD_BUSY :
					bForceCloseWhenSafe = false;	//	This can't be interrupted
					break;

				case MEM_CARD_ERROR :
					returnStatus = MEM_CARD_ERROR;
					break;
				}
			}
			break;

		case SCREEN_SETUP_PROGRESS_HANDLE_ERRORS :
			if (CSavegameDialogScreens::HandleSaveGameErrorCode())
			{
				returnStatus = MEM_CARD_ERROR;
			}
			break;
		}	//	switch (SetupProgress)
	}	//	end of else for if (bRescanMemoryCard)

	if ( (returnStatus != MEM_CARD_BUSY) || bForceCloseWhenSafe)
	{
		SetupProgress = SCREEN_SETUP_PROGRESS_DO_NOTHING;
		CGenericGameStorage::SetSaveOperation(OPERATION_NONE);
		CSavegameDialogScreens::SetMessageAsProcessing(false);
	}

	if (bForceCloseWhenSafe)
	{
		savegameDisplayf("CSavegameFrontEnd::SaveGameListScreenSetup - bForceCloseWhenSafe is set so returning MEM_CARD_ERROR");
		returnStatus = MEM_CARD_ERROR;
	}

	return returnStatus;
}

s32 CSavegameFrontEnd::ConvertMenuItemToSlotIndex(s32 menu_item)
{
	if (savegameVerifyf( (menu_item >= NumberOfMenuItemsBeforeSavegameList) && (menu_item < (MAX_SAVEGAMES_PER_FRONTEND_PAGE + NumberOfMenuItemsBeforeSavegameList) ), "CSavegameFrontEnd::ConvertMenuItemToSlotIndex - expected menu_item to be between %d and %d", NumberOfMenuItemsBeforeSavegameList, (MAX_SAVEGAMES_PER_FRONTEND_PAGE + NumberOfMenuItemsBeforeSavegameList) ) )
	{
		menu_item -= NumberOfMenuItemsBeforeSavegameList;
		if ((ms_BaseMenuItem + menu_item) < ms_SavegameSlotsArrangedByTime.GetCount())
		{
			return ms_SavegameSlotsArrangedByTime[ms_BaseMenuItem + menu_item];
		}
	}

	return -1;
}

#if RSG_ORBIS
void CSavegameFrontEnd::ReplaceMenuItemWithSlotIndex(s32 menu_item, s32 slotIndex)
{
	if (savegameVerifyf( (menu_item >= NumberOfMenuItemsBeforeSavegameList) && (menu_item < (MAX_SAVEGAMES_PER_FRONTEND_PAGE + NumberOfMenuItemsBeforeSavegameList) ), "CSavegameFrontEnd::ReplaceMenuItemWithSlotIndex - expected menu_item to be between %d and %d", NumberOfMenuItemsBeforeSavegameList, (MAX_SAVEGAMES_PER_FRONTEND_PAGE + NumberOfMenuItemsBeforeSavegameList) ) )
	{
		menu_item -= NumberOfMenuItemsBeforeSavegameList;
		if ((ms_BaseMenuItem + menu_item) < ms_SavegameSlotsArrangedByTime.GetCount())
		{
			ms_SavegameSlotsArrangedByTime[ms_BaseMenuItem + menu_item] = slotIndex;
		}
	}

}
#endif

MemoryCardError CSavegameFrontEnd::CheckVisibleFilesForDamage(bool bForceCloseWhenSafe)
{
	static s32 sMenuItemToHighlight = 0;
	static s32 ms_MenuItemForDamagedCheck = NumberOfMenuItemsBeforeSavegameList;
	static bool s_bFinishedCheckingMenuItems = false;

	if (ms_bNeedToRestartDamagedCheck)
	{
		if (bForceCloseWhenSafe)
		{
			savegameDisplayf("CSavegameFrontEnd::CheckVisibleFilesForDamage - ms_bNeedToRestartDamagedCheck is TRUE but returning MEM_CARD_ERROR because bForceCloseWhenSafe is set");
			return MEM_CARD_ERROR;
		}
		else
		{
			sMenuItemToHighlight = -1;

			if (NumberOfMenuItemsBeforeSavegameList == 1)
			{
		//	Menu item 0 displays the current and maximum number of savegames
		//	In the save menu it can be selected to create a new save file if the maximum number of saves hasn't already been reached

				char gxtNumberOfSavesText[MAX_MENU_ITEM_CHAR_LENGTH];
				char *pGxtFromTextFile = TheText.Get("MO_NUM_SAVES");
				bool bActivated = false;
				int currentNumberOfSaveGames = ms_SavegameSlotsArrangedByTime.GetCount();
				int maximumNumberOfSaveGames = TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES;
				CNumberWithinMessage NumbersArray[2];
				int numberOfStringArgument = 1;

				if (CPauseMenu::IsInSaveGameMenus())
				{
					maximumNumberOfSaveGames = NUM_MANUAL_SAVE_SLOTS;

					if (ms_SavegameSlotsArrangedByTime.GetCount() < TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES - 1)
					{
						pGxtFromTextFile = TheText.Get("MO_NEW_SAVE");
						currentNumberOfSaveGames = ms_SavegameSlotsArrangedByTime.GetCount()+1;
						numberOfStringArgument = 2;
						bActivated = true;
					}
				}

				NumbersArray[0].Set(currentNumberOfSaveGames);
				NumbersArray[1].Set(maximumNumberOfSaveGames);

				if(ms_SavegameSlotsArrangedByTime.GetCount() == 0 && !CPauseMenu::IsInSaveGameMenus())
				{
					pGxtFromTextFile = TheText.Get("MO_NO_SAVES");
					strcpy(gxtNumberOfSavesText, pGxtFromTextFile);
				}
				else
				{
					CMessages::InsertNumbersAndSubStringsIntoString(pGxtFromTextFile, 
						NumbersArray, numberOfStringArgument, 
						NULL, 0, 
						gxtNumberOfSavesText, NELEM(gxtNumberOfSavesText));
				}
				CPauseMenu::InsertSaveGameIntoMenu(0, gxtNumberOfSavesText, NULL, bActivated);
			}

			ms_MenuItemForDamagedCheck = NumberOfMenuItemsBeforeSavegameList;

			s_bFinishedCheckingMenuItems = false;

			ms_bNeedToRestartDamagedCheck = false;
		}	//	end of else for if (bForceCloseWhenSafe)
	}	//	if (ms_bNeedToRestartDamagedCheck)

	if (s_bFinishedCheckingMenuItems)
	{
		return MEM_CARD_COMPLETE;
	}

	bool bAllMenuItemsDisplayed = false;

	s32 SlotIndex = ConvertMenuItemToSlotIndex(ms_MenuItemForDamagedCheck);
	if (SlotIndex >= 0)
	{
#if USE_SAVE_DATA_MEMORY || USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP
		if (CSavegameList::IsThisAnAutosaveSlot(SlotIndex))
		{
			const s32 slotIndexOfBackupSave = SlotIndex + INDEX_OF_BACKUPSAVE_SLOT;
			if (!CSavegameList::IsSaveGameSlotEmpty(slotIndexOfBackupSave) && !CSavegameList::HasBeenCheckedForDamage(slotIndexOfBackupSave))
			{
				switch (CSavegameList::CheckIfSlotIsDamaged(slotIndexOfBackupSave))
				{
					case MEM_CARD_COMPLETE :
						{
							if (bForceCloseWhenSafe)
							{
								savegameDisplayf("CSavegameFrontEnd::CheckVisibleFilesForDamage - damaged check for autosave backup slot %d has just finished. Returning MEM_CARD_ERROR because bForceCloseWhenSafe is set", slotIndexOfBackupSave);
								return MEM_CARD_ERROR;
							}
							else
							{
								savegameDisplayf("CSavegameFrontEnd::CheckVisibleFilesForDamage - finished checking autosave backup slot %d for damage. Damaged = %s. I'll move on to checking the main autosave slot now", slotIndexOfBackupSave, CSavegameList::GetIsDamaged(slotIndexOfBackupSave)?"true":"false");
							}
						}
						break;

					case MEM_CARD_BUSY :
						return MEM_CARD_BUSY;
//						break;
					case MEM_CARD_ERROR :
						return MEM_CARD_ERROR;
				}
			}
		}
#endif	//	USE_SAVE_DATA_MEMORY || USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP

		switch (CSavegameList::CheckIfSlotIsDamaged(SlotIndex))
		{
			case MEM_CARD_COMPLETE :

				if (bForceCloseWhenSafe)
				{
					savegameDisplayf("CSavegameFrontEnd::CheckVisibleFilesForDamage - damaged check for slot %d has just finished. Returning MEM_CARD_ERROR because bForceCloseWhenSafe is set", SlotIndex);
					return MEM_CARD_ERROR;
				}
				else
				{
#if RSG_ORBIS
					bool bAddCurrentItemToList = true;

					//if main slot and is damaged, check backup
					if(SlotIndex < TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES && CSavegameList::GetIsDamaged(SlotIndex) && !CSavegameList::IsSaveGameSlotEmpty(SlotIndex + INDEX_OF_BACKUPSAVE_SLOT))
					{
						ReplaceMenuItemWithSlotIndex(ms_MenuItemForDamagedCheck, SlotIndex + INDEX_OF_BACKUPSAVE_SLOT);
						bAddCurrentItemToList = false;
					}
#if USE_SAVE_DATA_MEMORY || USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP
					else if (CSavegameList::IsThisAnAutosaveSlot(SlotIndex))
					{
						if (!CSavegameList::IsSaveGameSlotEmpty(SlotIndex) && !CSavegameList::GetIsDamaged(SlotIndex))
						{
							const s32 backupSlotIndex = SlotIndex + INDEX_OF_BACKUPSAVE_SLOT;
							if (!CSavegameList::IsSaveGameSlotEmpty(backupSlotIndex) && !CSavegameList::GetIsDamaged(backupSlotIndex))
							{
								if (CSavegameList::GetSaveTime(backupSlotIndex) > CSavegameList::GetSaveTime(SlotIndex))
								{
									savegameDisplayf("CSavegameFrontEnd::CheckVisibleFilesForDamage - backup slot %d is more recent than main slot %d so display the backup", backupSlotIndex, SlotIndex);
									ReplaceMenuItemWithSlotIndex(ms_MenuItemForDamagedCheck, backupSlotIndex);
									bAddCurrentItemToList = false;
								}
							}
						}
					}
#endif	//	USE_SAVE_DATA_MEMORY || USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP

					if (!bAddCurrentItemToList)
					{
						return MEM_CARD_BUSY;
					}
#endif
					AddOneItemToSaveGameList(ms_MenuItemForDamagedCheck);

					int iNumSavegameSlots = ms_SavegameSlotsArrangedByTime.GetCount();
					if (ms_CurrentlyHighlightedSlot > iNumSavegameSlots)
					{
						// The end slot has been removed - move the highlight to the last save
						ms_CurrentlyHighlightedSlot = iNumSavegameSlots;
					}

					sMenuItemToHighlight = ms_CurrentlyHighlightedSlot;

					ms_MenuItemForDamagedCheck++;
					if (ms_MenuItemForDamagedCheck == (MAX_SAVEGAMES_PER_FRONTEND_PAGE + NumberOfMenuItemsBeforeSavegameList))
					{
						bAllMenuItemsDisplayed = true;
					}
				}
				break;
			case MEM_CARD_BUSY :
				break;
			case MEM_CARD_ERROR :
				return MEM_CARD_ERROR;
		}
	}
	else
	{
		bAllMenuItemsDisplayed = true;
	}

	if (bAllMenuItemsDisplayed)
	{
// 		CMenuSystem::SetSelectedMenuItem(CFrontEnd::GetMenuID(FRONTEND_MENU_SAVEGAME_LIST), sMenuItemToHighlight);
// 		CMenuSystem::HighlightOneItem(CFrontEnd::GetMenuID(FRONTEND_MENU_SAVEGAME_LIST), sMenuItemToHighlight, true);
// 		CMenuSystem::Enable(CFrontEnd::GetMenuID(FRONTEND_MENU_SAVEGAME_LIST));

		// Do I need to return MEM_CARD_ERROR here if bForceCloseWhenSafe is true?

		CPauseMenu::FlagSaveGameMenuAsFinished(sMenuItemToHighlight);

		s8 ColumnForSpinner = CPauseMenu::GetColumnForSpinner();
		CPauseMenu::SetBusySpinner(false, ColumnForSpinner);

		s_bFinishedCheckingMenuItems = true;

		return MEM_CARD_COMPLETE;
	}

	return MEM_CARD_BUSY;
}

