
// network includes
#include "network/Live/livemanager.h"

// Game headers
#include "game/cheat.h"
#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/savegame_autoload.h"
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/savegame_filenames.h"
#include "SaveLoad/savegame_get_file_size.h"
#include "SaveLoad/savegame_initial_checks.h"
#include "SaveLoad/savegame_load.h"
#include "SaveLoad/savegame_users_and_devices.h"
#include "SaveLoad/SavegameMigration/savegame_export.h"

#if __WIN32PC || RSG_DURANGO
XPARAM(showProfileBypass);
#endif // __WIN32PC || RSG_DURANGO

SAVEGAME_OPTIMISATIONS()

eGenericLoadState	CSavegameLoad::ms_GenericLoadStatus = GENERIC_LOAD_DO_NOTHING;
eTypeOfSavegame		CSavegameLoad::ms_typeOfSavegame = SAVEGAME_SINGLE_PLAYER;

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
bool CSavegameLoad::ms_bLoadingForExport = false;
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES


#if __PPU
bool CSavegameLoad::ms_bLoadingAtEndOfNetworkGame = false;	//	if this is true then don't display any loading error messages
#endif


void CSavegameLoad::Init(unsigned initMode)
{
    if(initMode == INIT_CORE)
    {
    #if __PPU
	    ms_bLoadingAtEndOfNetworkGame = false;
    #endif

    }
    else if(initMode == INIT_AFTER_MAP_LOADED)
    {
	    ms_GenericLoadStatus = GENERIC_LOAD_DO_NOTHING;
		ms_typeOfSavegame = SAVEGAME_SINGLE_PLAYER;
#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
		ms_bLoadingForExport = false;
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES
	}
}


void CSavegameLoad::EndGenericLoad()
{
	ms_GenericLoadStatus = GENERIC_LOAD_DO_NOTHING;
#if __ALLOW_LOCAL_MP_STATS_SAVES
	Assertf( (CGenericGameStorage::GetSaveOperation() == OPERATION_LOADING_SAVEGAME_FROM_CONSOLE)
		|| (CGenericGameStorage::GetSaveOperation() == OPERATION_LOADING_REPLAY_SAVEGAME_FROM_CONSOLE)
		|| (CGenericGameStorage::GetSaveOperation() == OPERATION_LOADING_MPSTATS_SAVEGAME_FROM_CONSOLE), 
		"CSavegameLoad::EndGenericLoad - SaveOperation should be OPERATION_LOADING_SAVEGAME_FROM_CONSOLE, OPERATION_LOADING_REPLAY_SAVEGAME_FROM_CONSOLE or OPERATION_LOADING_MPSTATS_SAVEGAME_FROM_CONSOLE");
#else	//	__ALLOW_LOCAL_MP_STATS_SAVES
	Assertf( (CGenericGameStorage::GetSaveOperation() == OPERATION_LOADING_SAVEGAME_FROM_CONSOLE) || (CGenericGameStorage::GetSaveOperation() == OPERATION_LOADING_REPLAY_SAVEGAME_FROM_CONSOLE), "CSavegameLoad::EndGenericLoad - SaveOperation should be OPERATION_LOADING_SAVEGAME_FROM_CONSOLE or OPERATION_LOADING_REPLAY_SAVEGAME_FROM_CONSOLE");
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES
	CGenericGameStorage::SetSaveOperation(OPERATION_NONE);
}


void CSavegameLoad::BeginLoad(eTypeOfSavegame typeOfSavegame)
{
	ms_GenericLoadStatus = GENERIC_LOAD_BEGIN_INITIAL_CHECKS;
	ms_typeOfSavegame = typeOfSavegame;
#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	ms_bLoadingForExport = false;
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

	switch (typeOfSavegame)
	{
		case SAVEGAME_SINGLE_PLAYER :
			if (CSavegameFilenames::IsThisTheNameOfAReplaySaveFile(CSavegameFilenames::GetFilenameOfLocalFile()))
			{
				CGenericGameStorage::SetSaveOperation(OPERATION_LOADING_REPLAY_SAVEGAME_FROM_CONSOLE);
			}
			else
			{
				CGenericGameStorage::SetSaveOperation(OPERATION_LOADING_SAVEGAME_FROM_CONSOLE);
			}
			break;

		case SAVEGAME_MULTIPLAYER_CHARACTER :
		case SAVEGAME_MULTIPLAYER_COMMON :
#if __ALLOW_LOCAL_MP_STATS_SAVES
			CGenericGameStorage::SetSaveOperation(OPERATION_LOADING_MPSTATS_SAVEGAME_FROM_CONSOLE);
#else	//	__ALLOW_LOCAL_MP_STATS_SAVES
			savegameAssertf(0, "CSavegameLoad::BeginLoad - Didn't expect this function to be called with typeOfSavegame set to SAVEGAME_MULTIPLAYER_CHARACTER or SAVEGAME_MULTIPLAYER_COMMON");
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES
		break;

		case SAVEGAME_MAX_TYPES :
			savegameAssertf(0, "CSavegameLoad::BeginLoad - Didn't expect this function to be called with typeOfSavegame set to SAVEGAME_MAX_TYPES");
			break;
	}
}

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
void CSavegameLoad::BeginLoadForExport()
{
	ms_GenericLoadStatus = GENERIC_LOAD_BEGIN_INITIAL_CHECKS;
	ms_typeOfSavegame = SAVEGAME_SINGLE_PLAYER;
	ms_bLoadingForExport = true;

	CGenericGameStorage::SetSaveOperation(OPERATION_LOADING_SAVEGAME_FROM_CONSOLE);
}
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES


// *********************************************************************************************
// FUNCTION: GenericLoad()
//	
// Description: 
// *********************************************************************************************
MemoryCardError CSavegameLoad::GenericLoad()
{
#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	const bool bLoadingForExport = ms_bLoadingForExport;
#else // __ALLOW_EXPORT_OF_SP_SAVEGAMES
	const bool bLoadingForExport = false;
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

	bool bReturnError = false;
	switch (ms_GenericLoadStatus)
	{
		case GENERIC_LOAD_DO_NOTHING :
			return MEM_CARD_COMPLETE;

		case GENERIC_LOAD_BEGIN_INITIAL_CHECKS :
			{
#if __ALLOW_LOCAL_MP_STATS_SAVES
				savegameAssertf( (CGenericGameStorage::GetSaveOperation() == OPERATION_LOADING_SAVEGAME_FROM_CONSOLE) || (CGenericGameStorage::GetSaveOperation() == OPERATION_LOADING_REPLAY_SAVEGAME_FROM_CONSOLE) || (CGenericGameStorage::GetSaveOperation() == OPERATION_LOADING_MPSTATS_SAVEGAME_FROM_CONSOLE), "CSavegameLoad::GenericLoad - No loading of a save game is in progress");
#else	//	__ALLOW_LOCAL_MP_STATS_SAVES
				savegameAssertf( (CGenericGameStorage::GetSaveOperation() == OPERATION_LOADING_SAVEGAME_FROM_CONSOLE) || (CGenericGameStorage::GetSaveOperation() == OPERATION_LOADING_REPLAY_SAVEGAME_FROM_CONSOLE), "CSavegameLoad::GenericLoad - No loading of a save game is in progress");
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES
				CSavegameInitialChecks::BeginInitialChecks(false);
				ms_GenericLoadStatus = GENERIC_LOAD_INITIAL_CHECKS;
			}
//			break;

		case GENERIC_LOAD_INITIAL_CHECKS :
			{
#if __PPU
				if (!CSavegameAutoload::IsPerformingAutoLoadAtStartOfGame() && !ms_bLoadingAtEndOfNetworkGame)
#else
				if (!CSavegameAutoload::IsPerformingAutoLoadAtStartOfGame())
#endif
				{
					CSavingMessage::BeginDisplaying(CSavingMessage::STORAGE_DEVICE_MESSAGE_LOADING);
				}

				MemoryCardError SelectionState = CSavegameInitialChecks::InitialChecks();
				if (SelectionState == MEM_CARD_ERROR)
				{
//					EndGenericLoad();
//					return MEM_CARD_ERROR;
					if (SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT == CSavegameDialogScreens::GetMostRecentErrorCode())
					{
						bReturnError = true;
					}
					else
					{
						ms_GenericLoadStatus = GENERIC_LOAD_HANDLE_ERRORS;
					}
				}
				else if (SelectionState == MEM_CARD_COMPLETE)
				{
//					CGenericGameStorage::ClearBufferToStoreFirstFewBytesOfSaveFile();
					ms_GenericLoadStatus = GENERIC_LOAD_GET_FILE_SIZE;	//	GENERIC_LOAD_READ_SIZES_FROM_START_OF_FILE;	//	GENERIC_LOAD_ALLOCATE_BUFFER;	//	
				}
			}
			break;

		case GENERIC_LOAD_GET_FILE_SIZE :
		{
			u64 FileSize = 0;
			MemoryCardError GetFileSizeStatus = CSavegameGetFileSize::GetFileSize(CSavegameFilenames::GetFilenameOfLocalFile(), false, FileSize);

			switch (GetFileSizeStatus)
			{
				case MEM_CARD_COMPLETE :
				{
					if (FileSize == 0)
					{
						CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_GET_FILE_SIZE_FAILED1);
						ms_GenericLoadStatus = GENERIC_LOAD_HANDLE_ERRORS;						
					}
					else
					{
#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
						if (bLoadingForExport)
						{
							CSaveGameExport::SetBufferSizeForLoading(static_cast<u32>(FileSize));
						}
						else
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES
						{
							CGenericGameStorage::SetBufferSize(ms_typeOfSavegame, static_cast<u32>(FileSize));
						}
						ms_GenericLoadStatus = GENERIC_LOAD_ALLOCATE_BUFFER;	//	GENERIC_LOAD_READ_SIZES_FROM_START_OF_FILE;
					}
				}
				break;

				case MEM_CARD_BUSY :
					// Don't do anything
					break;

				case MEM_CARD_ERROR :
					if (SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT == CSavegameDialogScreens::GetMostRecentErrorCode())
					{
						bReturnError = true;
					}
					else
					{
						ms_GenericLoadStatus = GENERIC_LOAD_HANDLE_ERRORS;
					}

					if ( (ms_typeOfSavegame == SAVEGAME_SINGLE_PLAYER) && !bLoadingForExport)
					{
						CGenericGameStorage::ResetForEpisodeChange();	//	not sure if this is needed here
					}
					break;

				default :
					Assertf(0, "CSavegameLoad::GenericLoad - unexpected return value from CSavegameGetFileSize::GetFileSize");
					break;
			}
		}
			break;

/*
		case GENERIC_LOAD_READ_SIZES_FROM_START_OF_FILE :
		{
//	Maybe the first call is enough since this message doesn't time out
//			if (!CSavegameAutoload::IsPerformingAutoLoadAtStartOfGame() && !ms_bLoadingAtEndOfNetworkGame)
//			{
//				CSavingMessage::BeginDisplaying(CSavingMessage::STORAGE_DEVICE_MESSAGE_LOADING);
//			}

			MemoryCardError LoadStatus = DoLoad(false);

			switch (LoadStatus)
			{
				case MEM_CARD_COMPLETE :
				{
//	#if !__FINAL
//	CDebug::SetVersionNumber is not called in __FINAL builds so there's no point in checking the version number here
//					int SaveGameVersionNumber = CDebug::GetSavegameVersionNumber();
//					int VersionNumberInFile = CGenericGameStorage::GetVersionNumberFromStartOfSaveFile();
//					if ( (SaveGameVersionNumber > 0) && (VersionNumberInFile > 0) && (SaveGameVersionNumber != VersionNumberInFile) )
//					{
//						savegameDebugf1("CSavegameLoad::GenericLoad - SaveGameVersion mismatch - expected version %d differs from version number read from file %d\n", SaveGameVersionNumber, VersionNumberInFile);
//						CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_WRONG_VERSION);
//						ms_GenericLoadStatus = GENERIC_LOAD_HANDLE_ERRORS;
//					}
//					else
//	#endif	//	#if !__FINAL
					{
						CSaveGameData::LoadSizeFromStartOfSaveFile();
						ms_GenericLoadStatus = GENERIC_LOAD_ALLOCATE_BUFFER;
					}

//					if (!CheckSaveStringAtStartOfSaveFile((const char *) (&ms_BufferToStoreFirstFewBytesOfSaveFile[FIRST_FEW_BYTES_OFFSET_OF_SAVE_LABEL])))
//					if (!CGenericGameStorage::CheckSaveStringAtStartOfSaveFile())
//					{
//						savegameDebugf1("CSavegameLoad::GenericLoad - expected to find SAVE in the 8th to 11th bytes of save file\n");
//						CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_LOAD_FAILED1);	//	"The game data file appears to be damaged and cannot be loaded."
//						ms_GenericLoadStatus = GENERIC_LOAD_HANDLE_ERRORS;
//					}
				}
					break;

				case MEM_CARD_BUSY :
					// Don't do anything
					break;

				case MEM_CARD_ERROR :
//					Assertf(0, "CSavegameLoad::GenericLoad - DoLoad returned an error");
//					CGenericGameStorage::FreeAllData();
//					EndGenericLoad();
					if (SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT == CSavegameDialogScreens::GetMostRecentErrorCode())
					{
						bReturnError = true;
					}
					else
					{
						ms_GenericLoadStatus = GENERIC_LOAD_HANDLE_ERRORS;
					}
					CGenericGameStorage::ResetForEpisodeChange();
					break;

				default :
					Assertf(0, "CSavegameLoad::GenericLoad - unexpected return value from DoLoad");
					break;
			}
		}
			break;
*/
		case GENERIC_LOAD_ALLOCATE_BUFFER :
			{
//	Maybe the first call is enough since this message doesn't time out
//			if (!CSavegameAutoload::IsPerformingAutoLoadAtStartOfGame() && !ms_bLoadingAtEndOfNetworkGame)
//			{
//				CSavingMessage::BeginDisplaying(CSavingMessage::STORAGE_DEVICE_MESSAGE_LOADING);
//			}

#if __ALLOW_LOCAL_MP_STATS_SAVES
				if (savegameVerifyf( (CGenericGameStorage::GetSaveOperation() == OPERATION_LOADING_SAVEGAME_FROM_CONSOLE) || (CGenericGameStorage::GetSaveOperation() == OPERATION_LOADING_REPLAY_SAVEGAME_FROM_CONSOLE) || (CGenericGameStorage::GetSaveOperation() == OPERATION_LOADING_MPSTATS_SAVEGAME_FROM_CONSOLE), "CSavegameLoad::GenericLoad - No loading of a save game is in progress"))
#else	//	__ALLOW_LOCAL_MP_STATS_SAVES
				if (savegameVerifyf( (CGenericGameStorage::GetSaveOperation() == OPERATION_LOADING_SAVEGAME_FROM_CONSOLE) || (CGenericGameStorage::GetSaveOperation() == OPERATION_LOADING_REPLAY_SAVEGAME_FROM_CONSOLE), "CSavegameLoad::GenericLoad - No loading of a save game is in progress"))
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES
				{
					MemoryCardError AllocateBufferStatus = MEM_CARD_ERROR;
#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
					if (bLoadingForExport)
					{
						AllocateBufferStatus = CSaveGameExport::AllocateBufferForLoading();
					}
					else
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES
					{
						AllocateBufferStatus = CGenericGameStorage::AllocateBuffer(ms_typeOfSavegame, false);
					}

					switch (AllocateBufferStatus)
					{
						case MEM_CARD_COMPLETE :
							if ( (ms_typeOfSavegame == SAVEGAME_SINGLE_PLAYER) && !bLoadingForExport)
							{
								CCheat::ResetCheats();
							}
							ms_GenericLoadStatus = GENERIC_LOAD_BEGIN_LOAD;
							break;

						case MEM_CARD_BUSY :
							// Don't do anything - still waiting for memory to be allocated
							break;

						case MEM_CARD_ERROR :
							CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_ALLOCATE_MEMORY_FOR_LOAD_FAILED);
							ms_GenericLoadStatus = GENERIC_LOAD_HANDLE_ERRORS;
							break;
					}
				}
			}
			break;

		case GENERIC_LOAD_BEGIN_LOAD :
			{
				ms_GenericLoadStatus = GENERIC_LOAD_DO_LOAD;
			}
			//	deliberately fall through here

		case GENERIC_LOAD_DO_LOAD :
		{
//	Maybe the first call is enough since this message doesn't time out
//			if (!CSavegameAutoload::IsPerformingAutoLoadAtStartOfGame() && !ms_bLoadingAtEndOfNetworkGame)
//			{
//				CSavingMessage::BeginDisplaying(CSavingMessage::STORAGE_DEVICE_MESSAGE_LOADING);
//			}

			MemoryCardError LoadStatus = DoLoad();

			switch (LoadStatus)
			{
				case MEM_CARD_COMPLETE :
//	Can't call FreeAllData() and EndGenericLoad() for Loading Game Data until the end of DeSerialize
/*
					CSavegameDialogScreens::SetMessageAsProcessing(false);	//	GSWTODO - not sure if this should be in here or in DeSerialize()
					ms_UniqueIDOfGamerWhoStartedTheLoad = NetworkInterface::GetActiveGamerInfo()->GetGamerId();
					return MEM_CARD_COMPLETE;
*/
					ms_GenericLoadStatus = GENERIC_LOAD_WAIT_FOR_LOADING_MESSAGE_TO_FINISH;
					break;

				case MEM_CARD_BUSY :
					// Don't do anything
					break;

				case MEM_CARD_ERROR :
//					Assertf(0, "CSavegameLoad::GenericLoad - DoLoad returned an error");
//					CGenericGameStorage::FreeAllData();
//					EndGenericLoad();
					if (SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT == CSavegameDialogScreens::GetMostRecentErrorCode())
					{
						bReturnError = true;
					}
					else
					{
						ms_GenericLoadStatus = GENERIC_LOAD_HANDLE_ERRORS;
					}

					if ( (ms_typeOfSavegame == SAVEGAME_SINGLE_PLAYER) && !bLoadingForExport)
					{
						CGenericGameStorage::ResetForEpisodeChange();
					}
					break;

				default :
					Assertf(0, "CSavegameLoad::GenericLoad - unexpected return value from DoLoad");
					break;
			}
//			return LoadStatus;
		}
			break;

		case GENERIC_LOAD_WAIT_FOR_LOADING_MESSAGE_TO_FINISH :
#if __PPU
			if (ms_bLoadingAtEndOfNetworkGame || (CSavingMessage::HasLoadingMessageDisplayedForLongEnough()) )
#else
			if (CSavingMessage::HasLoadingMessageDisplayedForLongEnough())
#endif
			{
				CSavegameDialogScreens::SetMessageAsProcessing(false);	//	GSWTODO - not sure if this should be in here or in DeSerialize()

#if __WIN32PC || RSG_DURANGO
				// We do not yet support user accounts/live/social club etc. so we are faking it for now.
				Assertf(!PARAM_showProfileBypass.Get(), "User Profiles is not yet supported on PC. This WILL need update once Social Club is added! For now we will mimic a valid user profile and id!");
				rlGamerId id;
				id.SetId(0);
				CSavegameUsers::SetUniqueIDOfGamerWhoStartedTheLoad(id);
				return MEM_CARD_COMPLETE;
#else
				if (AssertVerify(NetworkInterface::GetActiveGamerInfo()))
				{
					CSavegameUsers::SetUniqueIDOfGamerWhoStartedTheLoad(NetworkInterface::GetActiveGamerInfo()->GetGamerId());
					return MEM_CARD_COMPLETE;
				}
				else
				{
					CSavegameUsers::ClearUniqueIDOfGamerWhoStartedTheLoad();
					bReturnError = true;
				}
#endif // __WIN32PC || RSG_DURANGO
			}
			break;

		case GENERIC_LOAD_HANDLE_ERRORS :
			CSavingMessage::Clear();

			if (CSavegameDialogScreens::HandleSaveGameErrorCode())
			{
				bReturnError = true;
//				if (GetPointerToBuffer())
/*
				CGenericGameStorage::FreeAllData();
				EndGenericLoad();
				CSavegameDialogScreens::SetMessageAsProcessing(false);
				return MEM_CARD_ERROR;
*/
			}
			break;
	}

	if (bReturnError)
	{
		CSavingMessage::Clear();

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
		if (bLoadingForExport)
		{
			CSaveGameExport::FreeBufferForLoading();
		}
		else
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES
		{
			CGenericGameStorage::FreeBufferContainingContentsOfSavegameFile(ms_typeOfSavegame);
		}
		EndGenericLoad();
		CSavegameDialogScreens::SetMessageAsProcessing(false);
		return MEM_CARD_ERROR;
	}

	return MEM_CARD_BUSY;
}



// GSWTODO - Maybe pass a flag into this function to say whether it must complete in a single frame (network load)
//	or if it should display error messages (e.g. corrupt file) and wait for the player to press OK (memory card load)
MemoryCardError CSavegameLoad::DeSerialize()
{
//	Should probably assert that GenericLoad has already finished successfully

	if (savegameVerifyf( (CGenericGameStorage::GetSaveOperation() == OPERATION_LOADING_SAVEGAME_FROM_CONSOLE) || (CGenericGameStorage::GetSaveOperation() == OPERATION_LOADING_REPLAY_SAVEGAME_FROM_CONSOLE), "CSavegameLoad::DeSerialize - not loading a save game"))
	{
		if (CGenericGameStorage::LoadBlockData(SAVEGAME_SINGLE_PLAYER) == false)
		{
//	SAVE GAME ERROR MESSAGE HERE?
			savegameDebugf1("CSavegameLoad::DeSerialize - LoadBlockData failed for some reason\n");
			CGenericGameStorage::FreeBufferContainingContentsOfSavegameFile(SAVEGAME_SINGLE_PLAYER);
			EndGenericLoad();
			return MEM_CARD_ERROR;
		}

		CSavingMessage::BeginDisplaying(CSavingMessage::STORAGE_DEVICE_MESSAGE_LOAD_SUCCESSFUL);
		savegameDebugf1("CSavegameLoad::DeSerialize - Game successfully loaded \n");
	}

	CGenericGameStorage::FreeBufferContainingContentsOfSavegameFile(SAVEGAME_SINGLE_PLAYER);
	EndGenericLoad();

	return MEM_CARD_COMPLETE;
}


MemoryCardError CSavegameLoad::DoLoad()
{
#if __XENON || __PPU || __WIN32PC || RSG_DURANGO || RSG_ORBIS
	static int LoadState = 0;

	switch (LoadState)
	{
		case 0 :
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

			if (CGenericGameStorage::DoCreatorCheck())
			{
				if (!SAVEGAME.BeginGetCreator(CSavegameUsers::GetUser(), CGenericGameStorage::GetContentType(), SAVEGAME.GetSelectedDevice(CSavegameUsers::GetUser()), CSavegameFilenames::GetFilenameOfLocalFile()) )
				{
					Assertf(0, "CSavegameLoad::DoLoad - BeginGetCreator failed");
					savegameDebugf1("CSavegameLoad::DoLoad - BeginGetCreator failed \n");
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_BEGIN_GET_CREATOR_FAILED);
					return MEM_CARD_ERROR;
				}
			}

			LoadState = 1;

		case 1 :
		{
			bool bIsCreator = false;
#if __PPU
			bool bHasCreator = false;
#endif // __PPU
#if __XENON
			SAVEGAME.UpdateClass();
#endif
			if (!CGenericGameStorage::DoCreatorCheck() || SAVEGAME.CheckGetCreator(CSavegameUsers::GetUser(), bIsCreator
#if __PPU
				,bHasCreator
#endif // __PPU
				))
			{	//	returns true for error or success, false for busy
				LoadState = 0;

				if (CGenericGameStorage::DoCreatorCheck())
				{
					SAVEGAME.EndGetCreator(CSavegameUsers::GetUser());
				}

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

				if (CGenericGameStorage::DoCreatorCheck())
				{
					if (SAVEGAME.GetState(CSavegameUsers::GetUser()) == fiSaveGameState::HAD_ERROR)
					{
						savegameDebugf1("CSavegameLoad::DoLoad - SAVE_GAME_DIALOG_CHECK_GET_CREATOR_FAILED1 \n");
						CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_GET_CREATOR_FAILED1);
						return MEM_CARD_ERROR;
					}

					if (SAVEGAME.GetError(CSavegameUsers::GetUser()) != fiSaveGame::SAVE_GAME_SUCCESS)
					{
						savegameDebugf1("CSavegameLoad::DoLoad - SAVE_GAME_DIALOG_CHECK_GET_CREATOR_FAILED2 \n");
						CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_GET_CREATOR_FAILED2);
						return MEM_CARD_ERROR;
					}

#if __PPU
					CLiveManager::GetAchMgr().SetOwnedSave(bHasCreator);

					if (bHasCreator && !bIsCreator)
#else
					if (!bIsCreator)
#endif // __PPU
					{
						CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_PLAYER_IS_NOT_CREATOR);
						return MEM_CARD_ERROR;
					}
				}

				u8 *pBuffer = CGenericGameStorage::GetBuffer(ms_typeOfSavegame);
				u32 bufferSize = CGenericGameStorage::GetBufferSize(ms_typeOfSavegame);
#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
				if (ms_bLoadingForExport)
				{
					pBuffer = CSaveGameExport::GetBufferForLoading();
					bufferSize = CSaveGameExport::GetSizeOfBufferForLoading();
				}
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

				if (!SAVEGAME.BeginLoad(CSavegameUsers::GetUser(), CGenericGameStorage::GetContentType(), SAVEGAME.GetSelectedDevice(CSavegameUsers::GetUser()), CSavegameFilenames::GetFilenameOfLocalFile(), pBuffer, bufferSize) )
				{
					Assertf(0, "CSavegameLoad::DoLoad - BeginLoad failed");
					savegameDebugf1("CSavegameLoad::DoLoad - BeginLoad failed \n");
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_BEGIN_LOAD_FAILED);
					return MEM_CARD_ERROR;
				}
				LoadState = 2;
			}
		}
			break;

		case 2 :
#if __XENON
			SAVEGAME.UpdateClass();
#endif
			u32 ActualBufferSizeLoaded = 0;
			bool valid;
			if (SAVEGAME.CheckLoad(CSavegameUsers::GetUser(), valid, ActualBufferSizeLoaded))
			{	//	returns true for error or success, false for busy
				LoadState = 0;
				SAVEGAME.EndLoad(CSavegameUsers::GetUser());

				if(!valid)
				{
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_LOAD_FAILED2);
					return MEM_CARD_ERROR;
				}

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

#if __PPU && __ASSERT
				if ( (ActualBufferSizeLoaded != 0) )
				{
					u32 ExpectedSize = CGenericGameStorage::GetBufferSize(ms_typeOfSavegame);
					s32 sizeDiff = (s32) ExpectedSize - (s32) ActualBufferSizeLoaded;
					Assertf( (sizeDiff >= 0) && (sizeDiff <= 1024), "CSavegameLoad::DoLoad - Expected the allocated buffer (%u) to be no more than 1024 bytes greater than the Buffer size returned by SAVEGAME.CheckLoad (%u) but the size difference is %d bytes", ExpectedSize, ActualBufferSizeLoaded, sizeDiff);
				}
#endif	//	__PPU && __ASSERT

#if __XENON
				u32 ExpectedSize = CGenericGameStorage::GetBufferSize(ms_typeOfSavegame);
				if ( (ActualBufferSizeLoaded != 0) && (ActualBufferSizeLoaded != ExpectedSize) )
				{
					Assertf(0, "CSavegameLoad::DoLoad - Buffer size returned by SAVEGAME.CheckLoad %d does not match buffer size calculated by game %d", ActualBufferSizeLoaded, ExpectedSize);
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_LOAD_BUFFER_SIZE_MISMATCH);
					return MEM_CARD_ERROR;
				}
#endif	//	__XENON

				if (SAVEGAME.GetState(CSavegameUsers::GetUser()) == fiSaveGameState::HAD_ERROR)
				{
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_LOAD_FAILED1);
					return MEM_CARD_ERROR;
				}

				if (SAVEGAME.GetError(CSavegameUsers::GetUser()) != fiSaveGame::SAVE_GAME_SUCCESS)
				{
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_LOAD_FAILED2);
					return MEM_CARD_ERROR;
				}

				return MEM_CARD_COMPLETE;
			}
			break;
	}
#else	//	#if __XENON || __PPU || __WIN32PC || RSG_DURANGO
	Assertf(0, "CSavegameLoad::DoLoad - save game only implemented for 360, PS3, PC, Orbis and Durango so far");
#endif
	return MEM_CARD_BUSY;
}




