

// Rage headers
#include "file/savegame.h"

// Game headers
#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/savegame_damaged_check.h"
#include "SaveLoad/savegame_messages.h"
#include "SaveLoad/savegame_users_and_devices.h"

SAVEGAME_OPTIMISATIONS()

CSavegameDamagedCheck::eProgress CSavegameDamagedCheck::ms_DamagedSlotProgressState = CSavegameDamagedCheck::DAMAGED_CHECK_PROGRESS_BEGIN;

void CSavegameDamagedCheck::ResetProgressState()
{
	ms_DamagedSlotProgressState = DAMAGED_CHECK_PROGRESS_BEGIN;
}

//	GetCreator doesn't do anything on PS3 so there's no point calling this function on PS3
MemoryCardError CSavegameDamagedCheck::CheckIfSlotIsDamaged(const char *pSavegameFilename, bool *pbDamagedFlag)
{
#if __PPU
	//	On PS3, a creator check doesn't seem to be enough to detect damaged savegames
	//	so I'm going to load the first 8 bytes of the file instead. Maybe reading the file will be enough to detect that the file is damaged.
	//	It's going to be slower though.
	static const u32 SIZE_OF_TEST_BUFFER = 8;
	static u8 testBuffer[SIZE_OF_TEST_BUFFER];
#endif	//	__PPU

	Assertf(pbDamagedFlag, "CSavegameDamagedCheck::CheckIfSlotIsDamaged - DamagedFlag pointer can't be NULL");
	switch (ms_DamagedSlotProgressState)
	{
		case DAMAGED_CHECK_PROGRESS_BEGIN :
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

			if (pbDamagedFlag)
			{
				*pbDamagedFlag = false;
			}
#if __PPU
			if (!SAVEGAME.BeginLoad(CSavegameUsers::GetUser(), CGenericGameStorage::GetContentType(), SAVEGAME.GetSelectedDevice(CSavegameUsers::GetUser()), pSavegameFilename, testBuffer, SIZE_OF_TEST_BUFFER) )
			{
				if (pbDamagedFlag)
				{
					*pbDamagedFlag = true;
				}
				return MEM_CARD_COMPLETE;
//				break;
			}
#elif RSG_ORBIS
			if(!SAVEGAME.BeginGetFileSize(CSavegameUsers::GetUser(), pSavegameFilename, false))
			{
				//assume savegame is corrupt
				if (pbDamagedFlag)
				{
					*pbDamagedFlag = true;
				}
				SAVEGAME.EndGetFileSize(CSavegameUsers::GetUser());
				return MEM_CARD_COMPLETE;
			}
#else
			if (CGenericGameStorage::DoCreatorCheck())
			{
				if (!SAVEGAME.BeginGetCreator(CSavegameUsers::GetUser(), CGenericGameStorage::GetContentType(), SAVEGAME.GetSelectedDevice(CSavegameUsers::GetUser()), pSavegameFilename) )
				{
					if (pbDamagedFlag)
					{
						*pbDamagedFlag = true;
					}
					return MEM_CARD_COMPLETE;
//					break;
				}
			}
#endif	//	__PPU

			ms_DamagedSlotProgressState = DAMAGED_CHECK_PROGRESS_UPDATE;
			//	break;

		case DAMAGED_CHECK_PROGRESS_UPDATE :
		{
#if __XENON
			SAVEGAME.UpdateClass();
#endif

#if __PPU
			u32 ActualBufferSizeLoaded = 0;
			bool validLoad = false;
			if (SAVEGAME.CheckLoad(CSavegameUsers::GetUser(), validLoad, ActualBufferSizeLoaded))
#elif RSG_ORBIS
			u64 fileSize;
			if(SAVEGAME.CheckGetFileSize(CSavegameUsers::GetUser(), fileSize))
#else
			bool bIsCreator = false;
			if (!CGenericGameStorage::DoCreatorCheck() || SAVEGAME.CheckGetCreator(CSavegameUsers::GetUser(), bIsCreator))
#endif	//	__PPU
			{	//	returns true for error or success, false for busy
				ms_DamagedSlotProgressState = DAMAGED_CHECK_PROGRESS_BEGIN;

#if __PPU
				SAVEGAME.EndLoad(CSavegameUsers::GetUser());
#elif RSG_ORBIS
				SAVEGAME.EndGetFileSize(CSavegameUsers::GetUser());
#else
				if (CGenericGameStorage::DoCreatorCheck())
				{
					SAVEGAME.EndGetCreator(CSavegameUsers::GetUser());
				}
#endif	//	__PPU

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

#if !__PPU && !RSG_ORBIS
				if (CGenericGameStorage::DoCreatorCheck())
#endif	//	!__PPU
				{
					if (SAVEGAME.GetState(CSavegameUsers::GetUser()) == fiSaveGameState::HAD_ERROR)
					{
						SAVEGAME.SetStateToIdle(CSavegameUsers::GetUser());
						if (pbDamagedFlag)
						{
							*pbDamagedFlag = true;
						}
						return MEM_CARD_COMPLETE;
					}

					if (SAVEGAME.GetError(CSavegameUsers::GetUser()) != fiSaveGame::SAVE_GAME_SUCCESS)
					{
						if (pbDamagedFlag)
						{
							*pbDamagedFlag = true;
						}
						return MEM_CARD_COMPLETE;
					}

#if __PPU
					if (!validLoad || (ActualBufferSizeLoaded != SIZE_OF_TEST_BUFFER) )
#elif RSG_ORBIS
					if (!fileSize)
#else
					if (!bIsCreator)
#endif	//	__PPU
					{
						if (pbDamagedFlag)
						{
							*pbDamagedFlag = true;
						}
						return MEM_CARD_COMPLETE;
					}
				}

				return MEM_CARD_COMPLETE;
			}
		}
			break;
	}

	return MEM_CARD_BUSY;
}

