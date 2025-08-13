
#if RSG_ORBIS

// Rage headers
#include "system/param.h"

// Game headers
#include "frontend/ProfileSettings.h"
#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/savegame_free_space_check.h"
#include "SaveLoad/savegame_initial_checks.h"
#include "SaveLoad/savegame_list.h"
#include "SaveLoad/savegame_new_game_checks.h"
#include "SaveLoad/savegame_users_and_devices.h"

SAVEGAME_OPTIMISATIONS()

PARAM(nosignincheck, "[savegame_new_game_checks] Don't ask the player to sign in or select a device at the start of the game");


bool CSavegameNewGameChecks::ms_bShouldNewGameChecksBePerformed = true;
int CSavegameNewGameChecks::ms_nFreeSpaceRequiredForSaveGames = 0;

void CSavegameNewGameChecks::Init(unsigned initMode)
{
    if(initMode == INIT_CORE)
    {
		 ms_bShouldNewGameChecksBePerformed = true;
    #if !__FINAL	  
	    if (PARAM_nosignincheck.Get())
	    {
		    ms_bShouldNewGameChecksBePerformed = false;
	    }
    #endif
	    ms_nFreeSpaceRequiredForSaveGames = 0;
    }
    else if(initMode == INIT_AFTER_MAP_LOADED)
    {
	}
}

void CSavegameNewGameChecks::BeginNewGameChecks()
{
	if (ms_bShouldNewGameChecksBePerformed)
	{
		CGenericGameStorage::SetSaveOperation(OPERATION_CHECKING_FOR_FREE_SPACE_AT_START_OF_GAME);
	}
}

MemoryCardError CSavegameNewGameChecks::ChecksAtStartOfNewGame()
{
	enum eNewGameCheckState
	{
		NEW_GAME_STATE_BEGIN_CHECKS,
		NEW_GAME_STATE_DO_CHECKS,
		NEW_GAME_STATE_BEGIN_FREE_SPACE_CHECK,
		NEW_GAME_STATE_FREE_SPACE_CHECK,
		NEW_GAME_STATE_HANDLE_ERRORS,
	};
	static eNewGameCheckState s_NewGameChecksState = NEW_GAME_STATE_BEGIN_CHECKS;	
	//early exit for debug
	if (!ms_bShouldNewGameChecksBePerformed)
	{
		if (OPERATION_CHECKING_FOR_FREE_SPACE_AT_START_OF_GAME == CGenericGameStorage::GetSaveOperation())
		{
			CGenericGameStorage::SetSaveOperation(OPERATION_NONE);
		}
		return MEM_CARD_COMPLETE;
	}

	switch (s_NewGameChecksState)
	{
		case NEW_GAME_STATE_BEGIN_CHECKS :
			CSavegameInitialChecks::BeginInitialChecks(false);
			s_NewGameChecksState = NEW_GAME_STATE_DO_CHECKS;
			break;

		case NEW_GAME_STATE_DO_CHECKS :
		{
			MemoryCardError InitialChecksStatus = CSavegameInitialChecks::InitialChecks();
			if (InitialChecksStatus == MEM_CARD_ERROR)
			{
				s_NewGameChecksState = NEW_GAME_STATE_HANDLE_ERRORS;
			}
			else if (InitialChecksStatus == MEM_CARD_COMPLETE)
				s_NewGameChecksState = NEW_GAME_STATE_BEGIN_FREE_SPACE_CHECK;
		}
			break;

		case NEW_GAME_STATE_BEGIN_FREE_SPACE_CHECK :
		{
			if (HandleFreeSpaceChecksAtStartOfGame() == MEM_CARD_ERROR)
			{
				s_NewGameChecksState = NEW_GAME_STATE_HANDLE_ERRORS;
			}
			else
				s_NewGameChecksState = NEW_GAME_STATE_FREE_SPACE_CHECK;
		}
			break;

		case NEW_GAME_STATE_FREE_SPACE_CHECK :
		{
			switch (HandleFreeSpaceChecksAtStartOfGame())
			{
				case MEM_CARD_COMPLETE :
					ms_bShouldNewGameChecksBePerformed = false;
					if (OPERATION_CHECKING_FOR_FREE_SPACE_AT_START_OF_GAME == CGenericGameStorage::GetSaveOperation())
					{
						CGenericGameStorage::SetSaveOperation(OPERATION_NONE);
					}
					return MEM_CARD_COMPLETE;
					break;

				case MEM_CARD_BUSY :
					break;

				case MEM_CARD_ERROR :
					s_NewGameChecksState = NEW_GAME_STATE_HANDLE_ERRORS;
					break;
			}
		}
			break;

		case NEW_GAME_STATE_HANDLE_ERRORS :			
			ms_bShouldNewGameChecksBePerformed = false;
			if (OPERATION_CHECKING_FOR_FREE_SPACE_AT_START_OF_GAME == CGenericGameStorage::GetSaveOperation())
			{
				CGenericGameStorage::SetSaveOperation(OPERATION_NONE);
			}
			return MEM_CARD_COMPLETE;
			break;
		
	}

	return MEM_CARD_BUSY;
}

/*
#1 Deletes and save game check files if there are any
#2 enumerates through SaveSlots and check for at least one manual save (0~14) and at least one of the each of the other slots
## for each missing file the size of a save game is added to the total minimum space needed
#3 tries to allocate a space check and lets the platform API show that we don't have enough space
## this should also delete the save game check file after it is created

*/
MemoryCardError CSavegameNewGameChecks::HandleFreeSpaceChecksAtStartOfGame( )
{
	enum eReserveSpaceProgress
	{
		BEGIN_DELETE_CHECK,
		CONTINUE_DELETE_CHECK,

		BEGIN_CHECK_SAVE_SLOT,
		END_CHECK_SAVE_SLOT,

		BEGIN_FREE_SPACE_CHECK_FOR_SAVE_GAMES,
		CONTINUE_FREE_SPACE_CHECK_FOR_SAVEGAME
	};
	static eReserveSpaceProgress s_eProgress = BEGIN_DELETE_CHECK;

	MemoryCardError ReturnStatus = MEM_CARD_BUSY;

	static int s_SpaceRequiredForFile = 0;
	static int s_TotalFreeHDDSpace = 0;
	static int s_SizeOfSystemFiles = 0;
	static int s_SaveSlotIdx = 0;
	static char s_filename[SAVE_GAME_MAX_FILENAME_LENGTH_OF_LOCAL_FILE] = {'\0'};

	//0 is the first manual save game, we only need one of the indices 0~14, so we skip to INDEX_OF_PS3_PROFILE_SLOT once we have found one.
	const int SaveSlots[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, INDEX_OF_PS3_PROFILE_SLOT, AUTOSAVE_SLOT_FOR_EPISODE_ZERO, INDEX_OF_MISSION_REPEAT_SAVE_SLOT};
	const int last_manual_save_slot_idx = 14;
	bool exists = false;

	switch (s_eProgress)
	{
		//just in case it hasn't been deleted, delete the check save game 
		case BEGIN_DELETE_CHECK:
			if(SAVEGAME.BeginDeleteMount(CSavegameUsers::GetUser(), "CHECK", true))
				s_eProgress = CONTINUE_DELETE_CHECK;
			break;
		case CONTINUE_DELETE_CHECK:						
			if(SAVEGAME.CheckDeleteMount(CSavegameUsers::GetUser()))
			{
				SAVEGAME.EndDeleteMount(CSavegameUsers::GetUser());
				s_eProgress = BEGIN_CHECK_SAVE_SLOT;
			}
			break;

		//check all the saves in SaveSlots
		case BEGIN_CHECK_SAVE_SLOT:
			CSavegameFilenames::CreateNameOfLocalFile(s_filename, SAVE_GAME_MAX_FILENAME_LENGTH_OF_LOCAL_FILE, SaveSlots[s_SaveSlotIdx]);
			if(SAVEGAME.BeginGetFileExists(CSavegameUsers::GetUser(), s_filename))
				s_eProgress = END_CHECK_SAVE_SLOT;
			break;
		case END_CHECK_SAVE_SLOT:						
			if(SAVEGAME.CheckGetFileExists(CSavegameUsers::GetUser(), exists))
			{
				if( SAVEGAME.GetState( CSavegameUsers::GetUser() ) == fiSaveGameState::HAVE_GOT_FILE_EXISTS )
				{
					if(s_SaveSlotIdx - last_manual_save_slot_idx > 0)
					{
						if(!exists)
							ms_nFreeSpaceRequiredForSaveGames += SCE_SAVE_DATA_BLOCK_SIZE * SCE_SAVE_DATA_BLOCKS_MIN;					
					}
					else
					{
						if(!exists && s_SaveSlotIdx == last_manual_save_slot_idx)
							ms_nFreeSpaceRequiredForSaveGames += SCE_SAVE_DATA_BLOCK_SIZE * SCE_SAVE_DATA_BLOCKS_MIN;					
						else if(exists)
						{
							s_SaveSlotIdx = last_manual_save_slot_idx;
						}
					}
				}
				SAVEGAME.EndGetFileExists( CSavegameUsers::GetUser() );
				if( ++s_SaveSlotIdx >= NELEM(SaveSlots) )
				{
					if(ms_nFreeSpaceRequiredForSaveGames != 0)
						s_eProgress = BEGIN_FREE_SPACE_CHECK_FOR_SAVE_GAMES;
					else
					{
						s_SpaceRequiredForFile = 0;
						return MEM_CARD_COMPLETE;
					}
				}
				else
					s_eProgress = BEGIN_CHECK_SAVE_SLOT;
			}
			break;
	
		//Check that we have enough space for the above
		case BEGIN_FREE_SPACE_CHECK_FOR_SAVE_GAMES :
			if (CSavegameFreeSpaceCheck::CheckForFreeSpaceForNamedFile(FREE_SPACE_CHECK, true, "CHECK", ms_nFreeSpaceRequiredForSaveGames, s_SpaceRequiredForFile, s_TotalFreeHDDSpace, s_SizeOfSystemFiles) == MEM_CARD_ERROR)
			{
				ReturnStatus = MEM_CARD_ERROR;
				break;
			}			
			Displayf("We need %d bytes\n", ms_nFreeSpaceRequiredForSaveGames);
			s_eProgress = CONTINUE_FREE_SPACE_CHECK_FOR_SAVEGAME;
			break;
		case CONTINUE_FREE_SPACE_CHECK_FOR_SAVEGAME :
		{		
			MemoryCardError FreeSpaceStatus = CSavegameFreeSpaceCheck::CheckForFreeSpaceForNamedFile(FREE_SPACE_CHECK, false, "CHECK", ms_nFreeSpaceRequiredForSaveGames, s_SpaceRequiredForFile, s_TotalFreeHDDSpace, s_SizeOfSystemFiles);
			switch (FreeSpaceStatus)
			{
				case MEM_CARD_COMPLETE :
					if (s_SpaceRequiredForFile != 0)
					{	
						CProfileSettings::GetInstance().SetWritePending(false);
						savegameDebugf1("CSavegameNewGameChecks::HandleFreeSpaceChecksAtStartOfGame - CONTINUE_FREE_SPACE_CHECK_FOR_SAVEGAME - we needed more memory and the system should have reported this\n");
					}
					return MEM_CARD_COMPLETE;
					break;
			
				case MEM_CARD_BUSY :
						//Do nothing
					break;
				case MEM_CARD_ERROR :
					ReturnStatus = MEM_CARD_ERROR;
					break;
			}
		}
		break;		
	}
	return ReturnStatus;
}
#endif	//	RSG_ORBIS


