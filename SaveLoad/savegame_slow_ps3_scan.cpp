
// Rage headers
#include "file/savegame.h"
#include "system/ipc.h"

// Game headers
#include "Core/game.h"
#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/savegame_list.h"
#include "SaveLoad/savegame_slow_ps3_scan.h"
#include "SaveLoad/savegame_users_and_devices.h"
#include "system/criticalsection.h"
#include "system/system.h"
#include "System/ThreadPriorities.h"

SAVEGAME_OPTIMISATIONS()


#if __PPU && __QUICK_SCAN_FOR_AUTOLOAD_AND_SLOW_SCAN_ON_ANOTHER_THREAD

bool CSavegameSlowPS3Scan::ms_bScanHasAlreadyBeenDone = false;
// NOTE: We set this to true to avoid a race condition with the commerce/facebook threads
// DO NOT set this value to false or you will introduce a startup crash on PS3!
bool CSavegameSlowPS3Scan::ms_bSavegameEnumerationInProgress = true;
volatile bool CSavegameSlowPS3Scan::sm_bHasThreadFinished = false;

static sysIpcThreadId ms_ReadNamesThreadId = sysIpcThreadIdInvalid;

static sysCriticalSectionToken s_SavegameEnumerationCritSecToken;

void CSavegameSlowPS3Scan::Init(unsigned initMode)
{
    if(initMode == INIT_CORE)
    {
	    ms_ReadNamesThreadId = sysIpcThreadIdInvalid;
	}
    else if(initMode == INIT_AFTER_MAP_LOADED)
    {
//		CSavegameSlowPS3Scan::WaitForThreadToFinish("CGenericGameStorage::Init", false);
	}
}


bool CSavegameSlowPS3Scan::IsSavegameEnumerationInProgress()
{
	SYS_CS_SYNC(s_SavegameEnumerationCritSecToken);
	return ms_bSavegameEnumerationInProgress;
}

void CSavegameSlowPS3Scan::SlowPS3Scan(void* )
{
	enum eSlowScanState
	{
		SLOW_SCAN_STATE_BEGIN_ENUMERATE_DEVICE,
		SLOW_SCAN_STATE_CHECK_ENUMERATE_DEVICE,
		SLOW_SCAN_STATE_SORT_SAVE_GAME_SLOTS,
		SLOW_SCAN_STATE_GET_FILE_TIMES,
//		SLOW_SCAN_STATE_DAMAGED_CHECKS,
		SLOW_SCAN_STATE_HANDLE_ERRORS		
	};

//	int CurrentSlotForDamagedCheck = 0;
	eSlowScanState s_SlowScanState = SLOW_SCAN_STATE_BEGIN_ENUMERATE_DEVICE;
	bool bFinished = false;

	while (!bFinished)
	{
		// Sleep to let other threads on this processor execute
		sysIpcSleep(1);

		if (CSystem::WantToExit())
			break;

		switch (s_SlowScanState)
		{
			case SLOW_SCAN_STATE_BEGIN_ENUMERATE_DEVICE :
				if (CLiveManager::GetCommerceMgr().IsThinDataPopulationInProgress())
				{
					savegameDebugf1("CSavegameSlowPS3Scan::SlowPS3Scan - SLOW_SCAN_STATE_BEGIN_ENUMERATE_DEVICE - waiting for IsThinDataPopulationInProgress to return false\n");
					break;
				}
				else
				{
					SYS_CS_SYNC(s_SavegameEnumerationCritSecToken);
					ms_bSavegameEnumerationInProgress = true;

					savegameDebugf1("CSavegameSlowPS3Scan::SlowPS3Scan - SLOW_SCAN_STATE_BEGIN_ENUMERATE_DEVICE\n");
					savegameDebugf1("Time = %d\n",sysTimer::GetSystemMsTime());
					CSavegameList::ClearSlotData();
					//			SAVEGAME.SetSelectedDeviceToAny(CSavegameUsers::GetUser());
					if (!CSavegameList::BeginEnumeration())
					{
						Assertf(0, "CSavegameSlowPS3Scan::SlowPS3Scan - BeginEnumeration failed");
						s_SlowScanState = SLOW_SCAN_STATE_HANDLE_ERRORS;

						//	Should I be calling SAVEGAME.EndEnumeration(CSavegameUsers::GetUser()); here?
						ms_bSavegameEnumerationInProgress = false;

						break;
					}
					s_SlowScanState = SLOW_SCAN_STATE_CHECK_ENUMERATE_DEVICE;
				}
	//			break;

			case SLOW_SCAN_STATE_CHECK_ENUMERATE_DEVICE :
				if (CSavegameList::CheckEnumeration())
				{	//	returns true for error or success, false for busy
					SYS_CS_SYNC(s_SavegameEnumerationCritSecToken);

					SAVEGAME.EndEnumeration(CSavegameUsers::GetUser());

					ms_bSavegameEnumerationInProgress = false;

					if (SAVEGAME.GetState(CSavegameUsers::GetUser()) == fiSaveGameState::HAD_ERROR)
					{
						s_SlowScanState = SLOW_SCAN_STATE_HANDLE_ERRORS;
						break;
					}

					if ( (SAVEGAME.GetError(CSavegameUsers::GetUser()) != fiSaveGame::SAVE_GAME_SUCCESS) && 
						(SAVEGAME.GetError(CSavegameUsers::GetUser()) != fiSaveGame::NO_FILES_EXIST) )
					{	//	don't treat "no files" as an error
						s_SlowScanState = SLOW_SCAN_STATE_HANDLE_ERRORS;
						break;
					}

					savegameDebugf1("CSavegameSlowPS3Scan::SlowPS3Scan - SLOW_SCAN_STATE_CHECK_ENUMERATE_DEVICE has finished\n");
					savegameDebugf1("ms_NumberOfSaveGameSlots = %d Time = %d\n", CSavegameList::GetNumberOfSaveGameSlots(), sysTimer::GetSystemMsTime());

					if ( (SAVEGAME.GetError(CSavegameUsers::GetUser()) == fiSaveGame::NO_FILES_EXIST)
						|| (CSavegameList::GetNumberOfSaveGameSlots() == 0) )
					{
						CSavegameList::SetSaveGamesHaveAlreadyBeenScanned(true);

						bFinished = true;
						savegameDebugf1("CSavegameSlowPS3Scan::SlowPS3Scan - SLOW_SCAN_STATE_CHECK_ENUMERATE_DEVICE - no files found\n");
					}
					else
					{
						s_SlowScanState = SLOW_SCAN_STATE_SORT_SAVE_GAME_SLOTS;
						savegameDebugf1("CSavegameSlowPS3Scan::SlowPS3Scan - SLOW_SCAN_STATE_CHECK_ENUMERATE_DEVICE - %d files found\n", CSavegameList::GetNumberOfSaveGameSlots());
#if !__NO_OUTPUT
						savegameDisplayf("CSavegameSlowPS3Scan::SlowPS3Scan - List of savegames before sorting");
						CSavegameList::PrintFilenames();
#endif	//	!__NO_OUTPUT
					}
				}
				break;

			case SLOW_SCAN_STATE_SORT_SAVE_GAME_SLOTS :
				//	Need to Sort the save games here
				//	then store the file times properly so that they can be used for the rest of the game

				//	Sorting the PS3 saves during autoload relies on the fact that there is only one storage device - the hard disk
				//	So the worst case should be 13 saves plus one profile file
				//	The sort function will be able to deal with this number of files

				if (CSavegameList::SortSaveGameSlots() == false)
				{
					s_SlowScanState = SLOW_SCAN_STATE_HANDLE_ERRORS;
				}
				else
				{
					Assertf(CSavegameList::GetNumberOfSaveGameSlots() > 0, "CSavegameSlowPS3Scan::SlowPS3Scan - no save games found");
					savegameDebugf1("CSavegameSlowPS3Scan::SlowPS3Scan - SLOW_SCAN_STATE_SORT_SAVE_GAME_SLOTS - about to call ResetFileTimeScan\n");
					CSavegameList::ResetFileTimeScan();
					s_SlowScanState = SLOW_SCAN_STATE_GET_FILE_TIMES;
					savegameDebugf1("CSavegameSlowPS3Scan::SlowPS3Scan - SLOW_SCAN_STATE_SORT_SAVE_GAME_SLOTS - sort has finished\n");
				}
				break;

			case SLOW_SCAN_STATE_GET_FILE_TIMES :
			{
				MemoryCardError GettingFileTimeStatus = CSavegameList::GetFileTimes();
				if (GettingFileTimeStatus == MEM_CARD_ERROR)
				{
					s_SlowScanState = SLOW_SCAN_STATE_HANDLE_ERRORS;
				}
				else if (GettingFileTimeStatus == MEM_CARD_COMPLETE)
				{
					bFinished = true;
					savegameDebugf1("CSavegameSlowPS3Scan::SlowPS3Scan - SLOW_SCAN_STATE_GET_FILE_TIMES - finished getting files times (and display names)\n");
					//	savegameDebugf1("ms_ReadNamesThreadId has finished - time = %d\n",sysTimer::GetSystemMsTime());

//					CurrentSlotForDamagedCheck = 0;
//					s_SlowScanState = SLOW_SCAN_STATE_DAMAGED_CHECKS;
//					savegameDebugf1("CSavegameSlowPS3Scan::SlowPS3Scan - about to start checking for damaged saves. Time = %d\n",sysTimer::GetSystemMsTime());
				}
			}
				break;


//	Graeme 12.10.12 - I'm going to try scanning for damaged saves in the background while the game is running. See CSavegameList::BackgroundScanForDamagedSavegames()
//	We now have 59 savegame files. Scanning one file for damage takes almost 2 seconds
/*
			case SLOW_SCAN_STATE_DAMAGED_CHECKS :

				if (savegameVerifyf(CurrentSlotForDamagedCheck < MAX_NUM_EXPECTED_SAVE_FILES, "CSavegameSlowPS3Scan::SlowPS3Scan - should only be checking if the first %d slots are damaged", MAX_NUM_EXPECTED_SAVE_FILES) )
				{
					if (!CSavegameList::IsSaveGameSlotEmpty(CurrentSlotForDamagedCheck))
					{
						switch (CSavegameList::CheckIfSlotIsDamaged(CurrentSlotForDamagedCheck))
						{
							case MEM_CARD_COMPLETE :
								savegameDebugf1("CSavegameSlowPS3Scan::SlowPS3Scan - finished checking slot %d for damage. Time = %d\n", CurrentSlotForDamagedCheck, sysTimer::GetSystemMsTime());
								CurrentSlotForDamagedCheck++;
								break;
							case MEM_CARD_BUSY :
								//	Don't do anything
								break;
							case MEM_CARD_ERROR :
								s_SlowScanState = SLOW_SCAN_STATE_HANDLE_ERRORS;
								break;
						}
					}
					else
					{
						CurrentSlotForDamagedCheck++;
					}
				}

				if (CurrentSlotForDamagedCheck == MAX_NUM_EXPECTED_SAVE_FILES)
				{
					bFinished = true;
					savegameDebugf1("CSavegameSlowPS3Scan::SlowPS3Scan - SLOW_SCAN_STATE_DAMAGED_CHECKS - finished checking for damaged savegames\n");
					savegameDebugf1("ms_ReadNamesThreadId has finished - time = %d\n",sysTimer::GetSystemMsTime());
				}
				break;
*/
			case SLOW_SCAN_STATE_HANDLE_ERRORS :
				CSavegameDialogScreens::ClearSaveGameError();
//				return MEM_CARD_ERROR;
				bFinished = true;
				savegameDebugf1("CSavegameSlowPS3Scan::SlowPS3Scan - SLOW_SCAN_STATE_HANDLE_ERRORS\n");
				break;
		}
	}	//	end of while

	sm_bHasThreadFinished = true;
}


#define GET_SAVE_GAME_NAMES_AND_DATES_THREAD_CORE		(0)			//	doesn't matter for PS3
#define GET_SAVE_GAME_NAMES_AND_DATES_THREAD_STACK		(16 * 1024)	//	16K should be enough

bool CSavegameSlowPS3Scan::CreateThreadToReadNamesAndDatesOfSaveGames()
{
	if (ms_bScanHasAlreadyBeenDone)
	{	//	Only want this to do something at the start of the game
		return false;
	}

	if (ms_ReadNamesThreadId != sysIpcThreadIdInvalid)
	{
		savegameDebugf1("CSavegameSlowPS3Scan::CreateThreadToReadNamesAndDatesOfSaveGames - thread has already been created so don't create another one\n");
		return false;
	}

	savegameDebugf1("CSavegameSlowPS3Scan::CreateThreadToReadNamesAndDatesOfSaveGames - about to create thread\n");
	sm_bHasThreadFinished = false;
	// Create the thread
	ms_ReadNamesThreadId = sysIpcCreateThread(SlowPS3Scan, NULL, GET_SAVE_GAME_NAMES_AND_DATES_THREAD_STACK, PRIO_GETSAVENAMES_THREAD, "[GTA] GetSaveNamesThread", GET_SAVE_GAME_NAMES_AND_DATES_THREAD_CORE);
#if __ASSERT
	Assertf(ms_ReadNamesThreadId != sysIpcThreadIdInvalid, "Could not create the thread to read the names and dates of save games - out of memory?");
#else
	if (ms_ReadNamesThreadId == sysIpcThreadIdInvalid)
		Quitf("Could not create the thread to read the names and dates of save games - out of memory?");
#endif

	savegameDebugf1("ms_ReadNamesThreadId - start time = %d\n",sysTimer::GetSystemMsTime());
	return true;
}

/*
void CSavegameSlowPS3Scan::WaitForThreadToFinish(const char *OUTPUT_ONLY(pNameOfCallingFunction), bool ASSERT_ONLY(bAssertIfNotFinished))
{
	if (ms_ReadNamesThreadId != sysIpcThreadIdInvalid)
	{
		Assertf(!bAssertIfNotFinished, "%s - expected ms_ReadNamesThreadId to be finished by now", pNameOfCallingFunction);
		savegameDebugf1("%s - waiting for ms_ReadNamesThreadId to end\n", pNameOfCallingFunction);
		sysIpcWaitThreadExit(ms_ReadNamesThreadId);
		ms_ReadNamesThreadId = sysIpcThreadIdInvalid;
		savegameDebugf1("%s - ms_ReadNamesThreadId has ended\n", pNameOfCallingFunction);
		savegameDebugf1("ms_ReadNamesThreadId - end time = %d\n",sysTimer::GetSystemMsTime());
	}
	ms_bScanHasAlreadyBeenDone = true;
}
*/

bool CSavegameSlowPS3Scan::HasThreadFinished()
{
	if (sm_bHasThreadFinished)
	{
		savegameDebugf1("CSavegameSlowPS3Scan::HasThreadFinished - ms_ReadNamesThreadId has ended\n");
//		savegameDebugf1("ms_ReadNamesThreadId - end time = %d\n",sysTimer::GetSystemMsTime());
		ms_ReadNamesThreadId = sysIpcThreadIdInvalid;
		ms_bScanHasAlreadyBeenDone = true;
		return true;
	}

	savegameDebugf1("CSavegameSlowPS3Scan::HasThreadFinished - waiting for ms_ReadNamesThreadId to end\n");
	return false;
}

#endif	//	__PPU && __QUICK_SCAN_FOR_AUTOLOAD_AND_SLOW_SCAN_ON_ANOTHER_THREAD


