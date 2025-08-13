
//	#if __PPU
//	#include <cell/rtc.h>
//	#endif

// Rage headers
#include "rline/rl.h"

// Game headers
#include "control/replay/file/ReplayFileManager.h"
#include "frontend/PauseMenu.h"
#include "frontend/ProfileSettings.h"
#include "Peds/PlayerInfo.h"
#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/savegame_initial_checks.h"
#include "SaveLoad/savegame_list.h"
#include "SaveLoad/savegame_messages.h"
#include "SaveLoad/savegame_save.h"
#include "SaveLoad/savegame_users_and_devices.h"
#include "script/script.h"
#include "stats/StatsMgr.h"
#include "system/controlMgr.h"
#include "text/TextFile.h"

SAVEGAME_OPTIMISATIONS()

eGenericSaveState	CSavegameSave::ms_GenericSaveStatus = GENERIC_SAVE_DO_NOTHING;

char		*CSavegameSave::ms_pSaveGameIcon = NULL;
u32			CSavegameSave::ms_SizeOfSaveGameIcon = 0;

s32			CSavegameSave::ms_SlotNumber = -1;
bool		CSavegameSave::ms_bAutosave = false;

bool		CSavegameSave::ms_bSaveHasJustOccurred = false;
#if RSG_ORBIS
bool		CSavegameSave::ms_bAllowBackupSave = false;
bool		CSavegameSave::ms_bSavingABackupOnly = false;
bool		CSavegameSave::ms_savingBackupSave = false;
#endif

#if RSG_PC
char		CSavegameSave::ms_CloudSaveMetadataString[cMaxLengthOfCloudSaveMetadataString];
#endif	//	RSG_PC


eTypeOfSavegame CSavegameSave::ms_SaveGameType = SAVEGAME_SINGLE_PLAYER;

const u32 disableInputsDurationForAutoSave = 750;

void CSavegameSave::Init(unsigned initMode)
{
    if(initMode == INIT_CORE)
    {
	    ms_pSaveGameIcon = NULL;
	    ms_SizeOfSaveGameIcon = 0;
	    FileHandle fIdThumbNail = CFileMgr::OpenFile("platform:/DATA/icon.PNG");
	    Assert(CFileMgr::IsValidFileHandle(fIdThumbNail) );
	    if (CFileMgr::IsValidFileHandle(fIdThumbNail))
	    {
		    u32 size = CFileMgr::GetTotalSize(fIdThumbNail);

		    ms_pSaveGameIcon = rage_new char[size];
		    Assertf(ms_pSaveGameIcon, "Failed to allocate memory for savegame icon");
		    ms_SizeOfSaveGameIcon = size;

		    if (ms_pSaveGameIcon)
		    {
			    CFileMgr::Read(fIdThumbNail, ms_pSaveGameIcon, ms_SizeOfSaveGameIcon);
			    SAVEGAME.SetIcon(ms_pSaveGameIcon, ms_SizeOfSaveGameIcon);
		    }
		    CFileMgr::CloseFile(fIdThumbNail);
	    }

// #if __PPU
		// we need to init / set the icon for saved content, for bug 561 (GTA E1 PS3 database)
		// If the player fires up the ELC disc for the first time but exits back to XMB before selecting an episode
		// the profile is saved with a broken icon. This fix ensures that the icon is set to the ELC icon by default.
		//	InitDefaultIcon();
// #endif

    }
    else if(initMode == INIT_AFTER_MAP_LOADED)
    {
	    ms_GenericSaveStatus = GENERIC_SAVE_DO_NOTHING;

	    ms_bSaveHasJustOccurred = false;

	}
}

void CSavegameSave::Shutdown(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_CORE)
    {
	    if (ms_pSaveGameIcon)
	    {
		    delete[] ms_pSaveGameIcon;
		    ms_pSaveGameIcon = NULL;
	    }
	    ms_SizeOfSaveGameIcon = 0;
	}
    else if(shutdownMode == SHUTDOWN_WITH_MAP_LOADED)
    {

	}
}

MemoryCardError CSavegameSave::BeginGameSave(int SlotNumber, eTypeOfSavegame savegameType)
{
	if ( (SlotNumber < 0) && (!CPauseMenu::GetMenuPreference(PREF_AUTOSAVE)) )
	{	//	autosave has been disabled so return immediately
		return MEM_CARD_COMPLETE;
	}

	if (!savegameVerifyf(CGenericGameStorage::GetSaveOperation() == OPERATION_NONE, "CSavegameSave::BeginGameSave - operation is not OPERATION_NONE"))
	{
		return MEM_CARD_ERROR;
	}

	Assertf(ms_GenericSaveStatus == GENERIC_SAVE_DO_NOTHING, "CSavegameSave::BeginGameSave - ");

	if (ms_GenericSaveStatus == GENERIC_SAVE_DO_NOTHING)
	{
		ms_SaveGameType = savegameType;

#if RSG_ORBIS
		ms_bAllowBackupSave = false;
		ms_bSavingABackupOnly = false;
#endif	//	RSG_ORBIS

		ms_SlotNumber = SlotNumber;
		ms_bAutosave = false;

		if (SlotNumber < 0)
		{
			savegameAssertf( (ms_SaveGameType == SAVEGAME_SINGLE_PLAYER), "CSavegameSave::BeginGameSave - savegameType should be SINGLE_PLAYER for an autosave");
#if RSG_ORBIS
			CSavegameList::SetSlotToUpdateOnceSaveHasCompleted(CSavegameList::GetAutosaveSlotNumberForCurrentEpisode());

#if !USE_SAVE_DATA_MEMORY || USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP
			ms_bAllowBackupSave = true;	//	Allow backup saves for autosaves
#endif	//	!USE_SAVE_DATA_MEMORY || USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP

#endif
			ms_SlotNumber = CSavegameList::GetAutosaveSlotNumberForCurrentEpisode();
			ms_bAutosave = true;

			CGenericGameStorage::SetSaveOperation(OPERATION_AUTOSAVING);
		}
		else
		{
			CSavegameFilenames::MakeValidSaveNameForLocalFile(SlotNumber);

#if RSG_ORBIS
//	I'm not sure if I need to set this for OPERATION_SAVING_MPSTATS_SAVEGAME_TO_CONSOLE
			CSavegameList::SetSlotToUpdateOnceSaveHasCompleted(SlotNumber);
#endif
			eSavegameFileType savegameFileType = CSavegameList::FindSavegameFileTypeForThisSlot(SlotNumber);

#if RSG_ORBIS
			if (savegameFileType == SG_FILE_TYPE_SAVEGAME)
			{	//	Don't make backup saves for SG_FILE_TYPE_MISSION_REPEAT or SG_FILE_TYPE_REPLAY
				ms_bAllowBackupSave = true;
			}
			else if (savegameFileType == SG_FILE_TYPE_SAVEGAME_BACKUP)
			{
				ms_bSavingABackupOnly = true;
				ms_bAllowBackupSave = true;		//	I don't think this is essential when saving only a backup but I might as well set it anyway
			}
#endif	//	RSG_ORBIS

			switch (savegameFileType)
			{
				case SG_FILE_TYPE_MISSION_REPEAT :
					savegameAssertf( (ms_SaveGameType == SAVEGAME_SINGLE_PLAYER), "CSavegameSave::BeginGameSave - savegameType should be SINGLE_PLAYER for a repeat save");
					CGenericGameStorage::SetSaveOperation(OPERATION_SAVING_MISSION_REPEAT_SAVEGAME_TO_CONSOLE);
					break;

#if GTA_REPLAY
				case SG_FILE_TYPE_REPLAY :
					savegameAssertf( (ms_SaveGameType == SAVEGAME_SINGLE_PLAYER), "CSavegameSave::BeginGameSave - savegameType should be SINGLE_PLAYER for a replay save");
					CGenericGameStorage::SetSaveOperation(OPERATION_SAVING_REPLAY_SAVEGAME_TO_CONSOLE);
					break;
#endif	//	GTA_REPLAY

#if __ALLOW_LOCAL_MP_STATS_SAVES
				case SG_FILE_TYPE_MULTIPLAYER_STATS :
					savegameAssertf( (ms_SaveGameType == SAVEGAME_MULTIPLAYER_CHARACTER) || (ms_SaveGameType == SAVEGAME_MULTIPLAYER_COMMON), "CSavegameSave::BeginGameSave - savegameType should be CHARACTER or COMMON for an MP save");
					CGenericGameStorage::SetSaveOperation(OPERATION_SAVING_MPSTATS_SAVEGAME_TO_CONSOLE);
					break;
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES

				default :
					savegameAssertf( (ms_SaveGameType == SAVEGAME_SINGLE_PLAYER), "CSavegameSave::BeginGameSave - savegameType should be SINGLE_PLAYER for a single player save");
					CGenericGameStorage::SetSaveOperation(OPERATION_SAVING_SAVEGAME_TO_CONSOLE);
					break;
			}
		}

		ms_GenericSaveStatus = GENERIC_SAVE_CHECK_IF_THIS_IS_AN_AUTOSAVE;
		return MEM_CARD_COMPLETE;
	}

	return MEM_CARD_ERROR;
}




void CSavegameSave::EndGenericSave()
{
	ms_GenericSaveStatus = GENERIC_SAVE_DO_NOTHING;

#if __ALLOW_LOCAL_MP_STATS_SAVES
	savegameAssertf( (CGenericGameStorage::GetSaveOperation() == OPERATION_SAVING_SAVEGAME_TO_CONSOLE) 
		|| (CGenericGameStorage::GetSaveOperation() == OPERATION_SAVING_MISSION_REPEAT_SAVEGAME_TO_CONSOLE) 
		|| (CGenericGameStorage::GetSaveOperation() == OPERATION_SAVING_REPLAY_SAVEGAME_TO_CONSOLE)
		|| (CGenericGameStorage::GetSaveOperation() == OPERATION_AUTOSAVING)
		|| (CGenericGameStorage::GetSaveOperation() == OPERATION_SAVING_MPSTATS_SAVEGAME_TO_CONSOLE), "CSavegameSave::EndGenericSave - Operation should be OPERATION_SAVING_SAVEGAME_TO_CONSOLE, OPERATION_SAVING_MISSION_REPEAT_SAVEGAME_TO_CONSOLE, OPERATION_SAVING_REPLAY_SAVEGAME_TO_CONSOLE, OPERATION_AUTOSAVING or OPERATION_SAVING_MPSTATS_SAVEGAME_TO_CONSOLE but it's %d", (s32) CGenericGameStorage::GetSaveOperation());
#else	//	__ALLOW_LOCAL_MP_STATS_SAVES
	savegameAssertf( (CGenericGameStorage::GetSaveOperation() == OPERATION_SAVING_SAVEGAME_TO_CONSOLE) 
		|| (CGenericGameStorage::GetSaveOperation() == OPERATION_SAVING_MISSION_REPEAT_SAVEGAME_TO_CONSOLE) 
		|| (CGenericGameStorage::GetSaveOperation() == OPERATION_SAVING_REPLAY_SAVEGAME_TO_CONSOLE)
		|| (CGenericGameStorage::GetSaveOperation() == OPERATION_AUTOSAVING), "CSavegameSave::EndGenericSave - Operation should be OPERATION_SAVING_SAVEGAME_TO_CONSOLE, OPERATION_SAVING_MISSION_REPEAT_SAVEGAME_TO_CONSOLE, OPERATION_SAVING_REPLAY_SAVEGAME_TO_CONSOLE or OPERATION_AUTOSAVING but it's %d", (s32) CGenericGameStorage::GetSaveOperation());
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES

	CGenericGameStorage::SetSaveOperation(OPERATION_NONE);
}


MemoryCardError CSavegameSave::GenericSave(bool bDisplayErrors)
{
	bool ReturnSelection = false;

	MemoryCardError ReturnValue = MEM_CARD_BUSY;
	switch (ms_GenericSaveStatus)
	{
		case GENERIC_SAVE_DO_NOTHING :
			return MEM_CARD_COMPLETE;

		case GENERIC_SAVE_CHECK_IF_THIS_IS_AN_AUTOSAVE :
			{
				if (CGenericGameStorage::GetSaveOperation() == OPERATION_AUTOSAVING)
				{
					// If device is valid then just use the autosave slot for the current episode
					CSavegameFilenames::MakeValidSaveNameForLocalFile(CSavegameList::GetAutosaveSlotNumberForCurrentEpisode());
				}

				CSavegameInitialChecks::BeginInitialChecks(false);
				ms_GenericSaveStatus = GENERIC_SAVE_INITIAL_CHECKS;
			}
//			break;

		case GENERIC_SAVE_INITIAL_CHECKS :
			{
//	This gets displayed over the top of dialog messages in autosave so I've commented it out
//	Graeme - now that the message has moved to the top-right corner of the screen, I'll try putting this back in
				CSavingMessage::BeginDisplaying(CSavingMessage::STORAGE_DEVICE_MESSAGE_SAVING);

				MemoryCardError SelectionState = CSavegameInitialChecks::InitialChecks();
				if (SelectionState == MEM_CARD_ERROR)
				{
					if (SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT == CSavegameDialogScreens::GetMostRecentErrorCode())
					{
						ReturnValue = MEM_CARD_ERROR;
					}
					else
					{
						if (CGenericGameStorage::GetSaveOperation() == OPERATION_AUTOSAVING)
						{
							ms_GenericSaveStatus = GENERIC_SAVE_AUTOSAVE_FAILED_TIDY_UP;
						}
						else
						{
							ms_GenericSaveStatus = GENERIC_SAVE_HANDLE_ERRORS;
						}
					}
				}
				else if (SelectionState == MEM_CARD_COMPLETE)
				{
#if RSG_ORBIS
					if (ms_bSavingABackupOnly)
					{
						ms_GenericSaveStatus = GENERIC_SAVE_BEGIN_SAVE;
					}
					else
#endif	//	RSG_ORBIS
					{
						ms_GenericSaveStatus = GENERIC_SAVE_CREATE_SAVE_BUFFER;
					}

					if (CGenericGameStorage::GetSaveOperation() == OPERATION_AUTOSAVING)
					{
						// If device is valid then just use the autosave slot for the current episode
						CSavegameFilenames::MakeValidSaveNameForLocalFile(CSavegameList::GetAutosaveSlotNumberForCurrentEpisode());

						if (CGenericGameStorage::ShouldWeDisplayTheMessageAboutOverwritingTheAutosaveSlot())
						{
							ms_GenericSaveStatus = GENERIC_SAVE_CHECK_IF_THE_AUTOSAVE_TO_BE_OVERWRITTEN_IS_DAMAGED;
						}
						else
						{
							//	If the autosave slot is empty, clear this flag now so that the overwrite message
							//	doesn't appear for the next autosave
							CGenericGameStorage::SetFlagForAutosaveOverwriteMessage(false);
						}
					}
				}
			}
			break;

		case GENERIC_SAVE_CHECK_IF_THE_AUTOSAVE_TO_BE_OVERWRITTEN_IS_DAMAGED :
		{
			const int SlotNumber = CSavegameList::GetAutosaveSlotNumberForCurrentEpisode();
			MemoryCardError DamagedAutoSaveResult = MEM_CARD_ERROR;

			DamagedAutoSaveResult = CSavegameList::CheckIfSlotIsDamaged(SlotNumber);
			switch (DamagedAutoSaveResult)
			{
				case MEM_CARD_COMPLETE :
					if (CSavegameList::GetIsDamaged(SlotNumber))
					{	//	Don't need to find the name of the save game. The message will just say it is damaged.
						ms_GenericSaveStatus = GENERIC_SAVE_CHECK_THAT_PLAYER_WANTS_TO_OVERWRITE_EXISTING_AUTOSAVE;
						CControlMgr::GetMainPlayerControl(false).DisableAllInputs(disableInputsDurationForAutoSave, ioValue::DisableOptions(ioValue::DisableOptions::F_ALLOW_SUSTAINED));
					}
					else
					{	//	The save game is not damaged so find its name.
						ms_GenericSaveStatus = GENERIC_SAVE_FIND_THE_NAME_OF_THE_SLOT_THAT_WILL_BE_OVERWRITTEN_BY_THE_AUTOSAVE;
					}
					break;
				case MEM_CARD_BUSY :
					//	Don't do anything
					break;
				case MEM_CARD_ERROR :
					if (SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT == CSavegameDialogScreens::GetMostRecentErrorCode())
					{
						ReturnValue = MEM_CARD_ERROR;
					}
					else
					{
						ms_GenericSaveStatus = GENERIC_SAVE_AUTOSAVE_FAILED_TIDY_UP;
					}
					break;
			}
		}
			break;

		case GENERIC_SAVE_FIND_THE_NAME_OF_THE_SLOT_THAT_WILL_BE_OVERWRITTEN_BY_THE_AUTOSAVE :
		{
			MemoryCardError FileTimeReadStatus = CSavegameList::GetTimeAndMissionNameFromDisplayName(CSavegameList::GetAutosaveSlotNumberForCurrentEpisode());
			switch (FileTimeReadStatus)
			{
				case MEM_CARD_COMPLETE :
					ms_GenericSaveStatus = GENERIC_SAVE_CHECK_THAT_PLAYER_WANTS_TO_OVERWRITE_EXISTING_AUTOSAVE;
					CControlMgr::GetMainPlayerControl(false).DisableAllInputs(disableInputsDurationForAutoSave, ioValue::DisableOptions(ioValue::DisableOptions::F_ALLOW_SUSTAINED));
					break;
				case MEM_CARD_BUSY :
					//	Don't do anything
					break;
				case MEM_CARD_ERROR :
					if (SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT == CSavegameDialogScreens::GetMostRecentErrorCode())
					{
						ReturnValue = MEM_CARD_ERROR;
					}
					else
					{
						ms_GenericSaveStatus = GENERIC_SAVE_AUTOSAVE_FAILED_TIDY_UP;
					}
					break;
			}
		}
			break;

		case GENERIC_SAVE_CHECK_THAT_PLAYER_WANTS_TO_OVERWRITE_EXISTING_AUTOSAVE :
			if (CSavegameUsers::HasPlayerJustSignedOut())
			{
				ReturnValue = MEM_CARD_ERROR;
				break;
			}

#if RSG_DURANGO
			if (CLiveManager::SignedInIntoNewProfileAfterSuspendMode())
			{
				savegameDisplayf("CSavegameSave::GenericSave - player has signed into a new profile so cancel this save (1)");
				ReturnValue = MEM_CARD_ERROR;
				break;
			}
#endif	//	RSG_DURANGO

//			if (CSavegameDevices::HasSelectedDeviceJustBeenRemoved())
//			{
//				CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_SELECTED_DEVICE_HAS_BEEN_REMOVED);
//				return MEM_CARD_ERROR;
//			}

			if (!CGenericGameStorage::ShouldWeDisplayTheMessageAboutOverwritingTheAutosaveSlot())
			{	//	On PS3, it's possible (though unlikely) that the first autosave happens before the files have been scanned
				//		and the game will get this far before realising that the autosave slot actually contains a Blank Save.
				//		The display name of "Blank Save" will only be filled in by the GetTimeOfFileInThisSlot above.
				//		This can only happen if the game is run with -noautoload, the autosave slot already contains a Blank Save 
				//		and autosave is done before the Load or Save menu is viewed.
				ms_GenericSaveStatus = GENERIC_SAVE_CREATE_SAVE_BUFFER;
				//	Set this to false if the player says they don't mind the autosave slot being overwritten
				CGenericGameStorage::SetFlagForAutosaveOverwriteMessage(false);
			}
			else if (CSavegameDialogScreens::HandleSaveGameDialogBox(SAVE_GAME_DIALOG_OVERWRITE_EXISTING_AUTOSAVE, 0, &ReturnSelection))
			{	//	Do you want to overwrite the existing save game in the autosave slot?
				if (ReturnSelection)
				{
					ms_GenericSaveStatus = GENERIC_SAVE_CREATE_SAVE_BUFFER;
					//	Set this to false if the player says they don't mind the autosave slot being overwritten
					CGenericGameStorage::SetFlagForAutosaveOverwriteMessage(false);
				}
				else
				{
					ms_GenericSaveStatus = GENERIC_SAVE_CHECK_PLAYER_WANTS_TO_SWITCH_OFF_AUTOSAVE;
				}
			}
			break;

		case GENERIC_SAVE_CREATE_SAVE_BUFFER :
			{
#if RSG_DURANGO
				if (CLiveManager::SignedInIntoNewProfileAfterSuspendMode())
				{
					savegameDisplayf("CSavegameSave::GenericSave - player has signed into a new profile so cancel this save (2)");
					ReturnValue = MEM_CARD_ERROR;
					break;
				}
#endif	//	RSG_DURANGO

// 				if (CTheScripts::GetPlayerIsOnAMission())
// 				{	//	To fix bug 185794 where the player can start a new mission before the autosave has a chance to start.
// 					//	Will just have to abandon the save here rather than store all the script variables in an "on mission" state
// 					savegameDebugf1("CSavegameSave::GenericSave - abandoning the save because the player is on a mission\n");
// 					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_NONE);
// 					ms_GenericSaveStatus = GENERIC_SAVE_HANDLE_ERRORS;
// 					break;
// 				}
				CSavingMessage::BeginDisplaying(CSavingMessage::STORAGE_DEVICE_MESSAGE_SAVING);

#if __ALLOW_LOCAL_MP_STATS_SAVES
				if (savegameVerifyf( (CGenericGameStorage::GetSaveOperation() == OPERATION_SAVING_SAVEGAME_TO_CONSOLE) 
					|| (CGenericGameStorage::GetSaveOperation() == OPERATION_SAVING_MISSION_REPEAT_SAVEGAME_TO_CONSOLE) 
					|| (CGenericGameStorage::GetSaveOperation() == OPERATION_SAVING_REPLAY_SAVEGAME_TO_CONSOLE)
					|| (CGenericGameStorage::GetSaveOperation() == OPERATION_AUTOSAVING)
					|| (CGenericGameStorage::GetSaveOperation() == OPERATION_SAVING_MPSTATS_SAVEGAME_TO_CONSOLE), "CSavegameSave::GenericSave - No save game in progress"))
#else	//	__ALLOW_LOCAL_MP_STATS_SAVES
				if (savegameVerifyf( (CGenericGameStorage::GetSaveOperation() == OPERATION_SAVING_SAVEGAME_TO_CONSOLE) 
					|| (CGenericGameStorage::GetSaveOperation() == OPERATION_SAVING_MISSION_REPEAT_SAVEGAME_TO_CONSOLE) 
					|| (CGenericGameStorage::GetSaveOperation() == OPERATION_SAVING_REPLAY_SAVEGAME_TO_CONSOLE)
					|| (CGenericGameStorage::GetSaveOperation() == OPERATION_AUTOSAVING), "CSavegameSave::GenericSave - No save game in progress"))
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES
				{
					MemoryCardError AllocateBufferStatus = CGenericGameStorage::AllocateBuffer(ms_SaveGameType, true);
					switch (AllocateBufferStatus)
					{
						case MEM_CARD_COMPLETE :
							ms_GenericSaveStatus = GENERIC_SAVE_FILL_BUFFER;

							if (CGenericGameStorage::BeginSaveBlockData(ms_SaveGameType) == false)
							{
								CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_SAVE_BLOCK_DATA_FAILED);
								savegameDebugf1("CSavegameSave::GenericSave - BeginSaveBlockData failed for some reason \n");
								CGenericGameStorage::EndSaveBlockData(ms_SaveGameType);
								ms_GenericSaveStatus = GENERIC_SAVE_HANDLE_ERRORS;
							}
							break;
						case MEM_CARD_BUSY :
							//	Don't do anything - still waiting for memory to be allocated
							break;
						case MEM_CARD_ERROR :
							CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_ALLOCATE_MEMORY_FOR_SAVE_FAILED);
							ms_GenericSaveStatus = GENERIC_SAVE_HANDLE_ERRORS;
							break;
					}
				}
			}
			break;
		case GENERIC_SAVE_FILL_BUFFER:
			{
				switch (CGenericGameStorage::GetSaveBlockDataStatus(ms_SaveGameType))
				{
					case SAVEBLOCKDATA_PENDING:
						// Don't do anything - still waiting for data to be saved
						break;
					case SAVEBLOCKDATA_ERROR:
						{
							CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_SAVE_BLOCK_DATA_FAILED);
							savegameDebugf1("CSavegameSave::GenericSave - SaveBlockData failed for some reason \n");
							CGenericGameStorage::EndSaveBlockData(ms_SaveGameType);
							ms_GenericSaveStatus = GENERIC_SAVE_HANDLE_ERRORS;
						}
						break;
					case SAVEBLOCKDATA_SUCCESS:
						{
							ms_GenericSaveStatus = GENERIC_SAVE_BEGIN_SAVE;
							CGenericGameStorage::EndSaveBlockData(ms_SaveGameType);
						}
						break;
					default:
						FatalAssertf(0, "CSavegameSave::GenericSave - Unexpected return value from CGenericGameStorage::SaveBlockData");
						break;
				}
			}
			break;
		case GENERIC_SAVE_BEGIN_SAVE :
			{
#if RSG_ORBIS
				if (TheText.DoesTextLabelExist("SG_BACKUP"))
				{
					SAVEGAME.SetLocalisedBackupTitleString(TheText.Get("SG_BACKUP"));
				}

				if (ms_bSavingABackupOnly)
				{
					ms_savingBackupSave = true;
				}
				else
				{
					ms_savingBackupSave = false;
				}

#if USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP
				if (CGenericGameStorage::GetSaveOperation() == OPERATION_AUTOSAVING)
				{
#if GTA_REPLAY
					if (ReplayFileManager::CheckAvailableDiskSpaceForAutoSave())
					{
						savegameDisplayf("CSavegameSave::GenericSave - save the backup of the autosave to download0 before the main autosave");
						ms_savingBackupSave = true;		//	Save backup autosave first. Then save the main autosave.
					}
					else
					{
						savegameDisplayf("CSavegameSave::GenericSave - ReplayFileManager::CheckAvailableDiskSpaceForAutoSave() has returned false so skip the saving of the autosave backup to download0. Move on to saving the main autosave");
						ms_savingBackupSave = false;
					}
#else	//	GTA_REPLAY
					savegameDisplayf("CSavegameSave::GenericSave - save the backup of the autosave to download0 before the main autosave");
					ms_savingBackupSave = true;		//	Save backup autosave first. Then save the main autosave.
#endif	//	GTA_REPLAY
				}
#endif	//	USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP
#endif

#if RSG_DURANGO
				if (CLiveManager::SignedInIntoNewProfileAfterSuspendMode())
				{
					savegameDisplayf("CSavegameSave::GenericSave - player has signed into a new profile so cancel this save (3)");
					ReturnValue = MEM_CARD_ERROR;
					break;
				}
#endif	//	RSG_DURANGO

				ms_GenericSaveStatus = GENERIC_SAVE_DO_SAVE;
			}
			//	deliberately fall through here

		case GENERIC_SAVE_DO_SAVE :
		{
#if RSG_ORBIS
			MemoryCardError SaveResult = DoSave(ms_savingBackupSave);
#else
			MemoryCardError SaveResult = DoSave(false);
#endif

			switch (SaveResult)
			{
				case MEM_CARD_COMPLETE :
					{
#if USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP
						if(ms_savingBackupSave)
						{
							if (CGenericGameStorage::GetSaveOperation() == OPERATION_AUTOSAVING)
							{
								savegameDisplayf("CSavegameSave::GenericSave - saving of backup autosave to download0 has completed so move on to saving the main autosave");
								ms_savingBackupSave = false;
							}
							else
							{
								savegameDisplayf("CSavegameSave::GenericSave - backup save has completed so return MEM_CARD_COMPLETE");
								ms_bSaveHasJustOccurred = true;	//	The script is expected to clear this flag when CommandActivateSaveMenu is called
								ReturnValue = MEM_CARD_COMPLETE;
							}
						}
						else
						{
							if (CGenericGameStorage::GetSaveOperation() == OPERATION_AUTOSAVING)
							{
								savegameDisplayf("CSavegameSave::GenericSave - main autosave has completed. The backup autosave to download0 should already have been done so return MEM_CARD_COMPLETE");
								ms_bSaveHasJustOccurred = true;	//	The script is expected to clear this flag when CommandActivateSaveMenu is called
								ReturnValue = MEM_CARD_COMPLETE;
							}
							else if (!ms_bAllowBackupSave)
							{
								savegameDisplayf("CSavegameSave::GenericSave - main save has completed and no backup save has been requested so return MEM_CARD_COMPLETE");
								ms_bSaveHasJustOccurred = true;	//	The script is expected to clear this flag when CommandActivateSaveMenu is called
								ReturnValue = MEM_CARD_COMPLETE;
							}
							else
							{
								savegameDisplayf("CSavegameSave::GenericSave - main save has completed so move on to saving the backup autosave");
								ms_savingBackupSave = true;
							}
						}
#else	//	USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP
#if RSG_ORBIS
						if(ms_bAllowBackupSave && !ms_savingBackupSave)
						{
							ms_savingBackupSave = true;
						}
						else
						{
							ms_savingBackupSave = false;

							if (!ms_bSavingABackupOnly)
							{
								ms_bSaveHasJustOccurred = true;	//	The script is expected to clear this flag when CommandActivateSaveMenu is called
							}
							ReturnValue = MEM_CARD_COMPLETE;
						}
#else	//	RSG_ORBIS
						ms_bSaveHasJustOccurred = true;	//	The script is expected to clear this flag when CommandActivateSaveMenu is called
						ReturnValue = MEM_CARD_COMPLETE;
#endif	//	RSG_ORBIS
#endif	//	USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP
					}
					break;

				case MEM_CARD_BUSY :
					//	Do nothing - have to wait till it's complete
					break;
				case MEM_CARD_ERROR :
//					Assertf(0, "CSavegameSave::GenericSave - CSavegameSave::DoSave returned an error");
//					FreeAllData();
//					EndGenericSave();

#if USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP
					if(ms_savingBackupSave && (CGenericGameStorage::GetSaveOperation() == OPERATION_AUTOSAVING) )
					{	//	If saving a backup autosave to download0 fails, don't return an error. Instead we'll move on to saving the main autosave
						savegameDisplayf("CSavegameSave::GenericSave - saving of backup autosave to download0 has failed. Move on to saving the main autosave anyway");
						CSavegameDialogScreens::ClearSaveGameError();
						ms_savingBackupSave = false;
					}
					else
#endif	//	USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP
					{
						if (SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT == CSavegameDialogScreens::GetMostRecentErrorCode())
						{
							ReturnValue = MEM_CARD_ERROR;
						}
						else
						{
							if (CGenericGameStorage::GetSaveOperation() == OPERATION_AUTOSAVING)
							{
								ms_GenericSaveStatus = GENERIC_SAVE_AUTOSAVE_FAILED_TIDY_UP;
							}
							else
							{
								ms_GenericSaveStatus = GENERIC_SAVE_HANDLE_ERRORS;
							}
						}
					}
					break;

				default :
					Assertf(0, "CSavegameSave::GenericSave - unexpected return value from CSavegameSave::DoSave");
					break;
			}
//			return SaveResult;
		}
			break;

		case GENERIC_SAVE_HANDLE_ERRORS :

/*

TCRHERE	bug 64231
		CONFIRM_AUTOSAVE_DISABLE

//	Deal with the following cases
//	Inform the player why the autosave hasn't happened and give them the option to retry
//			sAutosaveDisableReason = NOT_SIGNED_IN;


//			player has signed out during this process - can maybe be treated as NOT_SIGNED_IN
//			storage device removed
//			free space

//			If the player doesn't want to retry then call
//			CPauseMenu::SetMenuPreference(PREF_AUTOSAVE, false);
//			and say that autosave has been turned off in the frontend

*/
			if(bDisplayErrors == false)
			{
				ReturnValue = MEM_CARD_ERROR;
				break;
			}

			CSavingMessage::Clear();

			if (CSavegameDialogScreens::HandleSaveGameErrorCode())
			{
				ReturnValue = MEM_CARD_ERROR;
			}
			break;

		case GENERIC_SAVE_AUTOSAVE_FAILED_TIDY_UP :
		{
			CSavingMessage::Clear();

			CSavegameDialogScreens::ClearSaveGameError();
			CGenericGameStorage::FreeBufferContainingContentsOfSavegameFile(ms_SaveGameType);

			ms_GenericSaveStatus = GENERIC_SAVE_AUTOSAVE_FAILED;
		}
//			break;	//	Fall through

		case GENERIC_SAVE_AUTOSAVE_FAILED :
			if (CSavegameDialogScreens::HandleSaveGameDialogBox(SAVE_GAME_DIALOG_AUTOSAVE_FAILED_TRY_AGAIN, 0, &ReturnSelection))
			{	//	Autosave failed. Do you want to retry?
				CGenericGameStorage::SetFlagForAutosaveOverwriteMessage(true);
				if (ReturnSelection)
				{
					ms_GenericSaveStatus = GENERIC_SAVE_CHECK_IF_THIS_IS_AN_AUTOSAVE;
				}
				else
				{
					ms_GenericSaveStatus = GENERIC_SAVE_CHECK_PLAYER_WANTS_TO_SWITCH_OFF_AUTOSAVE;
				}
			}
			break;

		case GENERIC_SAVE_CHECK_PLAYER_WANTS_TO_SWITCH_OFF_AUTOSAVE :
			if (CSavegameUsers::HasPlayerJustSignedOut())
			{
				ReturnValue = MEM_CARD_ERROR;
				break;
			}

//			if (CSavegameDevices::HasSelectedDeviceJustBeenRemoved())
//			{
//				CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_SELECTED_DEVICE_HAS_BEEN_REMOVED);
//				return MEM_CARD_ERROR;
//			}

			if (CSavegameDialogScreens::HandleSaveGameDialogBox(SAVE_GAME_DIALOG_AUTOSAVE_OFF_ARE_YOU_SURE, 0, &ReturnSelection))
			{	//	are you sure? Autosaving will be switched off.
				CGenericGameStorage::SetFlagForAutosaveOverwriteMessage(true);
				if (ReturnSelection)
				{
					ms_GenericSaveStatus = GENERIC_SAVE_SWITCH_OFF_AUTOSAVE;
				}
				else
				{
					ms_GenericSaveStatus = GENERIC_SAVE_CHECK_IF_THIS_IS_AN_AUTOSAVE;
				}
			}
			break;

		case GENERIC_SAVE_SWITCH_OFF_AUTOSAVE :
			if (CSavegameUsers::HasPlayerJustSignedOut())
			{
				ReturnValue = MEM_CARD_ERROR;
				break;
			}

//			if (CSavegameDevices::HasSelectedDeviceJustBeenRemoved())
//			{
//				CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_SELECTED_DEVICE_HAS_BEEN_REMOVED);
//				return MEM_CARD_ERROR;
//			}

			if (CSavegameDialogScreens::HandleSaveGameDialogBox(SAVE_GAME_DIALOG_AUTOSAVE_SWITCHED_OFF, 0, &ReturnSelection))
			{	//	single button
				//	display a message saying autosave has been switched off and can be switched on in the frontend
				ReturnValue = MEM_CARD_COMPLETE;
				CPauseMenu::SetMenuPreference(PREF_AUTOSAVE, false);
			}
			break;
	}

	if (ReturnValue != MEM_CARD_BUSY)
	{
		if (ReturnValue == MEM_CARD_ERROR)
		{
			CSavingMessage::Clear();
		}

//	These Free calls aren't really needed here. They should be called when the Manual/Auto Save QueuedOperation completes.
//	CGenericGameStorage::Shutdown(SHUTDOWN_SESSION) is still calling CSavegameSave::GenericSave
//	directly. I don't know if that will work at all.

#if RSG_ORBIS
		if (!ms_bSavingABackupOnly)
#endif	//	RSG_ORBIS
		{
			CGenericGameStorage::FreeBufferContainingContentsOfSavegameFile(ms_SaveGameType);
			CGenericGameStorage::FreeAllDataToBeSaved(ms_SaveGameType);
		}
		EndGenericSave();

		if (bDisplayErrors)
		{
			CSavegameDialogScreens::SetMessageAsProcessing(false);
		}
	}

	return ReturnValue;
}


MemoryCardError CSavegameSave::DoSave(bool ORBIS_ONLY(savingBackupSave))
{
	enum eSaveState
	{
		SAVE_STATE_BEGIN_SAVE,
		SAVE_STATE_CHECK_SAVE,
#if RSG_ORBIS
		SAVE_STATE_GET_MODIFIED_TIME,
#endif	//	RSG_ORBIS
		SAVE_STATE_BEGIN_ICON_SAVE,
		SAVE_STATE_CHECK_ICON_SAVE,

#if CREATE_BACKUP_SAVES
		SAVE_STATE_BEGIN_BACKUP_SAVE,
		SAVE_STATE_CHECK_BACKUP_SAVE,
#endif // CREATE_BACKUP_SAVES
	};

#if __XENON || __PPU || __WIN32PC || RSG_ORBIS || RSG_DURANGO
	static eSaveState SaveState = SAVE_STATE_BEGIN_SAVE;

#if __BANK
	char outFilename[CGenericGameStorage::ms_MaxLengthOfPcSaveFileName];	//	SAVE_GAME_MAX_FILENAME_LENGTH+8];
	FileHandle fileHandle = INVALID_FILE_HANDLE;
#endif	//	__BANK

	switch (SaveState)
	{
		case SAVE_STATE_BEGIN_SAVE :
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

#if __BANK
			if (CGenericGameStorage::ms_bSaveToPC)
			{
//				ASSET.PushFolder("common:/data");
				ASSET.PushFolder(CGenericGameStorage::ms_NameOfPCSaveFolder);

				if (strlen(CGenericGameStorage::ms_NameOfPCSaveFile) > 0)
				{
					strcpy(outFilename, CGenericGameStorage::ms_NameOfPCSaveFile);
				}
				else
				{
					strcpy(outFilename, CSavegameFilenames::GetFilenameOfLocalFile());
				}
				if (CGenericGameStorage::ms_bSaveAsPso)
				{
					strcat(outFilename, ".pso");
				}
				else
				{
					strcat(outFilename, ".xml");
				}
				fileHandle = CFileMgr::OpenFileForWriting(outFilename);
				if (savegameVerifyf(CFileMgr::IsValidFileHandle(fileHandle), "CSavegameSave::DoSave - failed to open savegame file %s on PC for writing. Maybe the folder X:/gta5/build/dev/saves doesn't exist", outFilename))
				{
					CFileMgr::Write(fileHandle, (const char *) CGenericGameStorage::GetBuffer(ms_SaveGameType), CGenericGameStorage::GetBufferSize(ms_SaveGameType));
					CFileMgr::CloseFile(fileHandle);
				}
				ASSET.PopFolder();
			}
#endif	//	__BANK

#if __BANK
			if (!CGenericGameStorage::ms_bSaveToPC || CGenericGameStorage::ms_bSaveToConsoleWhenSavingToPC)
#endif	//	__BANK
			{
				if (!SAVEGAME.BeginSave(CSavegameUsers::GetUser(), CGenericGameStorage::GetContentType(), CSavegameFilenames::GetNameToDisplay(),
#if RSG_ORBIS
					savingBackupSave ? CSavegameFilenames::GetFilenameOfBackupSaveFile() : 
#endif
					CSavegameFilenames::GetFilenameOfLocalFile(), CGenericGameStorage::GetBuffer(ms_SaveGameType), CGenericGameStorage::GetBufferSize(ms_SaveGameType), true) )	//	bool overwrite, CGameLogic::GetCurrentEpisodeIndex())
				{
					Assertf(0, "CSavegameSave::DoSave - BeginSave failed");
					savegameDebugf1("CSavegameSave::DoSave - BeginSave failed\n");
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_BEGIN_SAVE_FAILED);
					return MEM_CARD_ERROR;
				}
				SaveState = SAVE_STATE_CHECK_SAVE;
			}
#if __BANK
			else
			{
				savegameDisplayf("Finished saving to the PC and the widgets aren't set to save to the console so finish now\n");
				SaveState = SAVE_STATE_BEGIN_SAVE;
				return MEM_CARD_COMPLETE;
			}
#endif	//	__BANK

			//	break;

		case SAVE_STATE_CHECK_SAVE :
		{
			bool bOutIsValid = false;
			bool bFileExists = false;
#if __XENON
			SAVEGAME.UpdateClass();
#endif
			if (SAVEGAME.CheckSave(CSavegameUsers::GetUser(), bOutIsValid, bFileExists))
			{	//	returns true for error or success, false for busy

				// GSWTODO - bOutIsValid could be quite useful.
				//	I think it only returns TRUE if the save was finished successfully.
				//	So should be false if Memory card is removed half way through saving.

//				Assertf(bOutIsValid, "CSavegameSave::DoSave - CheckSave - bOutIsValid is false (whatever that means)");
//				Assertf(bFileExists, "CSavegameSave::DoSave - CheckSave - bFileExists is true. Is that good?");
				SaveState = SAVE_STATE_BEGIN_SAVE;
				SAVEGAME.EndSave(CSavegameUsers::GetUser());

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
					savegameDebugf1("CSavegameSave::DoSave - SAVE_GAME_DIALOG_CHECK_SAVE_FAILED1 \n");
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_SAVE_FAILED1);
					return MEM_CARD_ERROR;
				}

				if (SAVEGAME.GetError(CSavegameUsers::GetUser()) != fiSaveGame::SAVE_GAME_SUCCESS)
				{
					savegameDebugf1("CSavegameSave::DoSave - SAVE_GAME_DIALOG_CHECK_SAVE_FAILED2 \n");
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_SAVE_FAILED2);
					return MEM_CARD_ERROR;
				}

// 				#if __PPU
// 					CSavegameList::UpdateSlotDataAfterSave();
// 				#endif

#if ENABLE_SINGLE_PLAYER_CLOUD_SAVES
				RegisterCloudSaveFile();
#endif	//	RSG_PC

#if RSG_ORBIS
				if (!SAVEGAME.BeginGetFileModifiedTime(CSavegameUsers::GetUser(), savingBackupSave? CSavegameFilenames::GetFilenameOfBackupSaveFile() : CSavegameFilenames::GetFilenameOfLocalFile()))
				{
					Assertf(0, "CSavegameSave::DoSave - BeginGetFileModifiedTime failed");
					savegameDebugf1("CSavegameSave::DoSave - BeginGetFileModifiedTime failed\n");
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_BEGIN_GET_FILE_MODIFIED_TIME_FAILED);
					return MEM_CARD_ERROR;
				}
				SaveState = SAVE_STATE_GET_MODIFIED_TIME;
#else
				SaveState = SAVE_STATE_BEGIN_ICON_SAVE;
#endif	//	RSG_ORBIS
			}
		}
			break;

#if RSG_ORBIS
		case SAVE_STATE_GET_MODIFIED_TIME :
		{
// #if __XENON
// 			SAVEGAME.UpdateClass();
// #endif
			u32 ModifiedTimeHigh = 0;
			u32 ModifiedTimeLow = 0;
			if (SAVEGAME.CheckGetFileModifiedTime(CSavegameUsers::GetUser(), ModifiedTimeHigh, ModifiedTimeLow))
			{	//	returns true for error or success, false for busy

				SaveState = SAVE_STATE_BEGIN_SAVE;
				SAVEGAME.EndGetFileModifiedTime(CSavegameUsers::GetUser());

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
					savegameDebugf1("CSavegameSave::DoSave - SAVE_GAME_DIALOG_CHECK_GET_FILE_MODIFIED_TIME_FAILED1 \n");
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_GET_FILE_MODIFIED_TIME_FAILED1);
					return MEM_CARD_ERROR;
				}

				if (SAVEGAME.GetError(CSavegameUsers::GetUser()) != fiSaveGame::SAVE_GAME_SUCCESS)
				{
					savegameDebugf1("CSavegameSave::DoSave - SAVE_GAME_DIALOG_CHECK_GET_FILE_MODIFIED_TIME_FAILED2 \n");
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_GET_FILE_MODIFIED_TIME_FAILED2);
					return MEM_CARD_ERROR;
				}

				CSavegameList::UpdateSlotDataAfterSave(ModifiedTimeHigh, ModifiedTimeLow, savingBackupSave);

				SaveState = SAVE_STATE_BEGIN_ICON_SAVE;
			}
		}
			break;
#endif	//	RSG_ORBIS

		case SAVE_STATE_BEGIN_ICON_SAVE :
			if (ms_pSaveGameIcon)
			{
				if (!SAVEGAME.BeginIconSave(CSavegameUsers::GetUser(), CGenericGameStorage::GetContentType(), 
#if RSG_ORBIS
					savingBackupSave? CSavegameFilenames::GetFilenameOfBackupSaveFile() : 
#endif
					CSavegameFilenames::GetFilenameOfLocalFile(), static_cast<const void *>(ms_pSaveGameIcon), ms_SizeOfSaveGameIcon))
				{
					SaveState = SAVE_STATE_BEGIN_SAVE;
					Assertf(0, "CSavegameSave::DoSave - BeginIconSave failed");
					savegameDebugf1("CSavegameSave::DoSave - BeginIconSave failed\n");
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_BEGIN_ICON_SAVE_FAILED);
					return MEM_CARD_ERROR;
				}
				SaveState = SAVE_STATE_CHECK_ICON_SAVE;
			}
			else
			{

#if CREATE_BACKUP_SAVES
				SaveState = SAVE_STATE_BEGIN_BACKUP_SAVE;		
#else	//CREATE_BACKUP_SAVES
				SaveState = SAVE_STATE_BEGIN_SAVE;				// Save completed
				return MEM_CARD_COMPLETE;
#endif	//CREATE_BACKUP_SAVES

			}
			break;

		case SAVE_STATE_CHECK_ICON_SAVE :
#if __XENON
			SAVEGAME.UpdateClass();
#endif
			if (SAVEGAME.CheckIconSave(CSavegameUsers::GetUser()))
			{	//	returns true for error or success, false for busy
				SaveState = SAVE_STATE_BEGIN_SAVE;
				SAVEGAME.EndIconSave(CSavegameUsers::GetUser());

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
					savegameDebugf1("CSavegameSave::DoSave - SAVE_GAME_DIALOG_CHECK_ICON_SAVE_FAILED1 \n");
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_ICON_SAVE_FAILED1);
					return MEM_CARD_ERROR;
				}

				if (SAVEGAME.GetError(CSavegameUsers::GetUser()) != fiSaveGame::SAVE_GAME_SUCCESS)
				{
					savegameDebugf1("CSavegameSave::DoSave - SAVE_GAME_DIALOG_CHECK_ICON_SAVE_FAILED2 \n");
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_ICON_SAVE_FAILED2);
					return MEM_CARD_ERROR;
				}

#if CREATE_BACKUP_SAVES
				SaveState = SAVE_STATE_BEGIN_BACKUP_SAVE;		
#else	//CREATE_BACKUP_SAVES
				SaveState = SAVE_STATE_BEGIN_SAVE;
				return MEM_CARD_COMPLETE;
#endif	//CREATE_BACKUP_SAVES
			}
			break;

#if CREATE_BACKUP_SAVES
			case SAVE_STATE_BEGIN_BACKUP_SAVE:
				{
					if (!SAVEGAME.BeginBackupSave(CSavegameUsers::GetUser(), CGenericGameStorage::GetContentType(), CSavegameFilenames::GetNameToDisplay(), CSavegameFilenames::GetFilenameOfLocalFile(), CGenericGameStorage::GetBuffer(ms_SaveGameType), CGenericGameStorage::GetBufferSize(ms_SaveGameType), true) )	//	bool overwrite, CGameLogic::GetCurrentEpisodeIndex())
					{
						// What to do if the backup fails......
						SaveState = SAVE_STATE_BEGIN_SAVE;
						return MEM_CARD_COMPLETE;		// Say it's complete even if our backup hasn't saved correctly, if we got this far, we must have saved the original save OK.
					}
					SaveState = SAVE_STATE_CHECK_BACKUP_SAVE;
				}
				break;

			case SAVE_STATE_CHECK_BACKUP_SAVE:
				{
					bool bOutIsValid = false;
					bool bFileExists = false;

					if (SAVEGAME.CheckBackupSave(CSavegameUsers::GetUser(), bOutIsValid, bFileExists))
					{	//	returns true for error or success, false for busy
						SaveState = SAVE_STATE_BEGIN_SAVE;
						SAVEGAME.EndBackupSave(CSavegameUsers::GetUser());
						return MEM_CARD_COMPLETE;
					}
				}
				break;
#endif	//CREATE_BACKUP_SAVES

	}
#else	//	#if __XENON || __PPU || __WIN32PC || RSG_DURANGO
	Assertf(0, "CSavegameSave::DoSave - save game only implemented for 360, PS3, PC, Orbis and Durango so far");
#endif

	return MEM_CARD_BUSY;
}
 

#if ENABLE_SINGLE_PLAYER_CLOUD_SAVES
void CSavegameSave::RegisterCloudSaveFile()
{
	// Only register SAVEGAME type for saves.
	eSavegameFileType savegameFileType = CSavegameList::FindSavegameFileTypeForThisSlot(ms_SlotNumber);
	if (savegameFileType != SG_FILE_TYPE_SAVEGAME)
	{
		savegameDebugf1("Not registering cloud save file, type %d is not SG_FILE_TYPE_SAVEGAME.", savegameFileType);
		return;
	}

	memset(ms_CloudSaveMetadataString, 0, NELEM(ms_CloudSaveMetadataString));
	RsonWriter rsonWriter(ms_CloudSaveMetadataString, NELEM(ms_CloudSaveMetadataString), RSON_FORMAT_JSON);
	rsonWriter.Reset();

	bool bSuccess = rsonWriter.Begin(NULL, NULL);	//	Top-level structure to contain all the rest of the data
	savegameAssertf(bSuccess, "CSavegameSave::RegisterCloudSaveFile - rsonWriter.Begin failed");

	if (bSuccess)
	{
		bSuccess = rsonWriter.WriteInt("Autosave", ms_bAutosave ? 1 : 0);
		savegameAssertf(bSuccess, "CSavegameSave::RegisterCloudSaveFile - failed to write Autosave field");
	}

	if (bSuccess)
	{
		bSuccess = rsonWriter.WriteInt("Slot", ms_SlotNumber);
		savegameAssertf(bSuccess, "CSavegameSave::RegisterCloudSaveFile - failed to write Slot field");
	}

	if (bSuccess)
	{	//	UTF8 localized string. Should I store the text key instead?
		bSuccess = rsonWriter.WriteString("LastMission", CSavegameFilenames::GetNameOfLastMissionPassed());
		savegameAssertf(bSuccess, "CSavegameSave::RegisterCloudSaveFile - failed to write LastMission field");
	}

	if (bSuccess)
	{
		bSuccess = rsonWriter.WriteFloat("CompletionPct", CSavegameFilenames::GetMissionCompletionPercentage());
		savegameAssertf(bSuccess, "CSavegameSave::RegisterCloudSaveFile - failed to write completionPct field");
	}

	if (bSuccess)
	{
		u64 posixTime64 = rlGetPosixTime();
//		savegameAssertf( (posixTime >> 32) == 0, "CSavegameSave::RegisterCloudSaveFile - expected the top 32 bits of the current POSIX time to be 0");
//		u32 posixTime32 = (u32) (posixTime & 0xffffffff);

		bSuccess = rsonWriter.WriteUns64("PosixTime", posixTime64);
		savegameAssertf(bSuccess, "CSavegameSave::RegisterCloudSaveFile - failed to write PosixTime field");
	}

	if (bSuccess)
	{	//	0 = Michael, 1 = Franklin, 2 = Trevor, -1 = something went wrong
		s32 playerCharacterIndex = -1;
		CPed* pPlayerPed = FindPlayerPed();
		if(pPlayerPed != NULL)
		{
			playerCharacterIndex = CPlayerInfo::GetPlayerIndex(*pPlayerPed);
		}

		bSuccess = rsonWriter.WriteInt("PlayerCharacterIndex", playerCharacterIndex);
		savegameAssertf(bSuccess, "CSavegameSave::RegisterCloudSaveFile - failed to write playerCharacterIndex field");
	}

	if (bSuccess)
	{
		bSuccess = rsonWriter.End();	//	Close the top-level structure
		savegameAssertf(bSuccess, "CSavegameSave::RegisterCloudSaveFile - rsonWriter.End failed");
	}

//	Data that is still to be added - 
// 	1. Last map area - is this necessary?

//	Uncomment the following block when RegisterSinglePlayerCloudSaveFile() is ready to use
	if (bSuccess)
	{
		bSuccess = CLiveManager::RegisterSinglePlayerCloudSaveFile(CSavegameFilenames::GetFilenameOfLocalFile(), ms_CloudSaveMetadataString);
		savegameAssertf(bSuccess, "CSavegameSave::RegisterCloudSaveFile - RegisterSinglePlayerCloudSaveFile failed");
	}
}
#endif	//	ENABLE_SINGLE_PLAYER_CLOUD_SAVES


