
#include "savegame_queued_operations.h"

// Rage headers
//	#include "file/savegame.h"
#include "rline/rlhttptask.h"
#include "system/param.h"

// Game headers
#include "audio/northaudioengine.h"
#include "camera/CamInterface.h"
#include "control/replay/File/ReplayFileManager.h"
#include "control/gamelogic.h"
#include "core/game.h"
#include "frontend/loadingscreens.h"
#include "frontend/ProfileSettings.h"

#include "Network/Cloud/UserContentManager.h"

#include "network/NetworkInterface.h"
#include "renderer/rendertargets.h"				//	for VideoResManager::GetNativeWidth() and GetNativeHeight()
#include "renderer/ScreenshotManager.h"
#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/savegame_check_file_exists.h"
#include "SaveLoad/savegame_damaged_check.h"
#include "SaveLoad/savegame_delete.h"
#include "SaveLoad/savegame_frontend.h"
#include "SaveLoad/savegame_get_file_size.h"
#include "SaveLoad/savegame_initial_checks.h"
#include "SaveLoad/savegame_list.h"
#include "SaveLoad/savegame_load.h"
#include "SaveLoad/savegame_load_block.h"
#include "SaveLoad/savegame_messages.h"
#include "SaveLoad/savegame_new_game_checks.h"
#include "SaveLoad/savegame_photo_buffer.h"
#include "SaveLoad/savegame_photo_cloud_deleter.h"
#include "SaveLoad/savegame_photo_cloud_list.h"
#include "SaveLoad/savegame_photo_load_local.h"
#include "SaveLoad/savegame_photo_local_list.h"
#include "SaveLoad/savegame_photo_local_metadata.h"
#include "SaveLoad/savegame_photo_manager.h"
#include "SaveLoad/savegame_photo_save_local.h"
#include "SaveLoad/savegame_save.h"
#include "SaveLoad/savegame_save_block.h"
#include "SaveLoad/savegame_slow_ps3_scan.h"
#include "SaveLoad/savegame_users_and_devices.h"
#include "SaveLoad/SavegameMigration/savegame_export.h"
#include "SaveLoad/SavegameMigration/savegame_import.h"
#include "SaveMigration/SaveMigration.h"
#include "script/script.h"
#include "streaming/streaming.h"
#include "streaming/streamingvisualize.h"
#include "system/controlMgr.h"
#include "Stats/StatsInterface.h"

SAVEGAME_OPTIMISATIONS()

RAGE_DEFINE_SUBCHANNEL(savegame, queueop, DIAG_SEVERITY_DEBUG3)
#undef __savegame_channel 
#define __savegame_channel savegame_queueop

#if __ALLOW_LOCAL_MP_STATS_SAVES
XPARAM(enableLocalMPSaveCache);
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES

XPARAM(outputCloudFiles);

PARAM(useNewStagingUrl, "[savegame_queued_operations] ignore the staging url returned by the UGC upload");

#if RSG_ORBIS


void CSavegameQueuedOperation_SpaceCheck::Init()
{
	// Results
	m_MemoryRequiredForBasicSaveGame = 0;
	m_SpaceChecksProgress = SPACE_CHECKS_BEGIN;
}

//	Return true to Pop() from queue

bool CSavegameQueuedOperation_SpaceCheck::Update()
{
	switch (m_SpaceChecksProgress)
	{
		case SPACE_CHECKS_BEGIN :
			CSavegameNewGameChecks::BeginNewGameChecks();
			m_SpaceChecksProgress = SPACE_CHECKS_PROCESS;
			break;

		case SPACE_CHECKS_PROCESS :
			switch(CSavegameNewGameChecks::ChecksAtStartOfNewGame())
			{
				case MEM_CARD_COMPLETE :
					m_MemoryRequiredForBasicSaveGame = CSavegameNewGameChecks::GetFreeSpaceRequiredForSaveGames();
					m_Status = MEM_CARD_COMPLETE;
					return true;
					break;
				case MEM_CARD_BUSY :
					break;
				case MEM_CARD_ERROR :
					m_Status = MEM_CARD_ERROR;
					return true;
					break;
			}
			break;
	}
	return false;
}

#endif // RSG_ORBIS

#if USE_PROFILE_SAVEGAME

#if USE_DOWNLOAD0_FOR_PROFILE_BACKUP

void CSavegameQueuedOperation_ProfileLoad::Init(s32 localIndex, void *pProfileBuffer, s32 BufferSize)
{
	m_localIndex = localIndex;
	m_pProfileBuffer = pProfileBuffer;
	m_SizeOfProfileBuffer = BufferSize;

	m_MainProfileSlot = INDEX_OF_PS3_PROFILE_SLOT;
	m_BackupProfileSlot = INDEX_OF_BACKUP_PROFILE_SLOT;

	m_MainProfileModTime = 0;
	m_BackupProfileModTime = 0;

	// Results
	m_SizeOfLoadedProfile = 0;

	m_ProfileLoadProgress = PROFILE_LOAD_PROGRESS_BEGIN_GET_MAIN_PROFILE_MOD_TIME;
}

//	Return true to Pop() from queue
bool CSavegameQueuedOperation_ProfileLoad::Update()
{
	switch (m_ProfileLoadProgress)
	{
		case PROFILE_LOAD_PROGRESS_BEGIN_GET_MAIN_PROFILE_MOD_TIME :
			CSavegameDialogScreens::SetDialogBoxHeading("SG_HDNG");

			if (BeginGetModifiedTime(m_MainProfileSlot, "Main Profile"))
			{
				m_ProfileLoadProgress = PROFILE_LOAD_PROGRESS_CHECK_GET_MAIN_PROFILE_MOD_TIME;
			}
			else
			{
				m_Status = MEM_CARD_ERROR;
			}
			break;

		case PROFILE_LOAD_PROGRESS_CHECK_GET_MAIN_PROFILE_MOD_TIME :
			{
				u32 mainModTimeHigh = 0;
				u32 mainModTimeLow = 0;
				switch (CheckGetModifiedTime(mainModTimeHigh, mainModTimeLow, "Main Profile"))
				{
				case MEM_CARD_COMPLETE :
					m_MainProfileModTime = ((u64) mainModTimeHigh) << 32;
					m_MainProfileModTime |= mainModTimeLow;

					if (m_MainProfileModTime == 0)
					{
						savegameDisplayf("CSavegameQueuedOperation_ProfileLoad::Update - mod time for main profile is 0. Maybe the file doesn't exist. Don't attempt to load the backup profile either");
						m_Status = MEM_CARD_ERROR;
					}
					else
					{
						m_ProfileLoadProgress = PROFILE_LOAD_PROGRESS_BEGIN_GET_BACKUP_PROFILE_MOD_TIME;
					}
					break;

				case MEM_CARD_BUSY :
					break;

				case MEM_CARD_ERROR :
					savegameDisplayf("CSavegameQueuedOperation_ProfileLoad::Update - CheckGetModifiedTime returned MEM_CARD_ERROR for the main profile. Maybe the file doesn't exist. Don't attempt to load the backup profile either");
					m_Status = MEM_CARD_ERROR;
					break;
				}
			}
			break;

		case PROFILE_LOAD_PROGRESS_BEGIN_GET_BACKUP_PROFILE_MOD_TIME :
			if (BeginGetModifiedTime(m_BackupProfileSlot, "Backup Profile"))
			{
				m_ProfileLoadProgress = PROFILE_LOAD_PROGRESS_CHECK_GET_BACKUP_PROFILE_MOD_TIME;
			}
			else
			{
				savegameDisplayf("CSavegameQueuedOperation_ProfileLoad::Update - BeginGetModifiedTime failed for the backup profile. Skip straight to BEGIN_LOAD_MOST_RECENT_PROFILE to attempt to load the main profile");
				m_ProfileLoadProgress = PROFILE_LOAD_PROGRESS_BEGIN_LOAD_MOST_RECENT_PROFILE;
			}
			break;

		case PROFILE_LOAD_PROGRESS_CHECK_GET_BACKUP_PROFILE_MOD_TIME :
			{
				u32 backupModTimeHigh = 0;
				u32 backupModTimeLow = 0;
				switch (CheckGetModifiedTime(backupModTimeHigh, backupModTimeLow, "Backup Profile"))
				{
				case MEM_CARD_COMPLETE :
					m_BackupProfileModTime = ((u64) backupModTimeHigh) << 32;
					m_BackupProfileModTime |= backupModTimeLow;

					m_ProfileLoadProgress = PROFILE_LOAD_PROGRESS_BEGIN_LOAD_MOST_RECENT_PROFILE;
					break;

				case MEM_CARD_BUSY :
					break;

				case MEM_CARD_ERROR :
					savegameDisplayf("CSavegameQueuedOperation_ProfileLoad::Update - CheckGetModifiedTime failed for the backup profile. Skip straight to BEGIN_LOAD_MOST_RECENT_PROFILE to attempt to load the main profile");
					m_ProfileLoadProgress = PROFILE_LOAD_PROGRESS_BEGIN_LOAD_MOST_RECENT_PROFILE;
					break;
				}
			}
			break;

		case PROFILE_LOAD_PROGRESS_BEGIN_LOAD_MOST_RECENT_PROFILE :
			{
				s32 slotToLoad = m_MainProfileSlot;

				if (m_BackupProfileModTime > m_MainProfileModTime)
				{
					savegameDisplayf("CSavegameQueuedOperation_ProfileLoad::Update - backup profile is more recent than the main profile so load the backup");
					slotToLoad = m_BackupProfileSlot;
				}

				CSavegameFilenames::MakeValidSaveNameForLocalFile(slotToLoad);
				if(SAVEGAME.BeginLoad(m_localIndex, 0, 0, CSavegameFilenames::GetFilenameOfLocalFile(), m_pProfileBuffer, m_SizeOfProfileBuffer))
				{
					m_ProfileLoadProgress = PROFILE_LOAD_PROGRESS_CHECK_LOAD;
				}
				else
				{
					savegameDisplayf("CSavegameQueuedOperation_ProfileLoad::Update - BeginLoad failed");
					SAVEGAME.EndLoad(m_localIndex);
					m_Status = MEM_CARD_ERROR;
				}
			}
			break;

		case PROFILE_LOAD_PROGRESS_CHECK_LOAD :
			{
				bool valid;
				if (SAVEGAME.CheckLoad(m_localIndex, valid, m_SizeOfLoadedProfile))
				{
					SAVEGAME.EndLoad(m_localIndex);

					if(valid)
					{
						m_Status = MEM_CARD_COMPLETE;
					}
					else
					{
						savegameDisplayf("CSavegameQueuedOperation_ProfileLoad::Update - SAVEGAME.CheckLoad failed");
						m_Status = MEM_CARD_ERROR;
					}
				}
			}
			break;
	}

	if ( (m_Status == MEM_CARD_COMPLETE) || (m_Status == MEM_CARD_ERROR) )
	{
		return true;
	}

	return false;
}


bool CSavegameQueuedOperation_ProfileLoad::BeginGetModifiedTime(s32 slotIndex, const char* OUTPUT_ONLY(pDebugString))
{
	CSavegameFilenames::MakeValidSaveNameForLocalFile(slotIndex);
	if (SAVEGAME.BeginGetFileModifiedTime(m_localIndex, CSavegameFilenames::GetFilenameOfLocalFile()))
	{
		return true;
	}
	else
	{
		savegameAssertf(0, "CSavegameQueuedOperation_ProfileLoad::BeginGetModifiedTime - BeginGetFileModifiedTime failed for %s", pDebugString);
		savegameDebugf1("CSavegameQueuedOperation_ProfileLoad::BeginGetModifiedTime - BeginGetFileModifiedTime failed for %s\n", pDebugString);
		return false;
	}
}


MemoryCardError CSavegameQueuedOperation_ProfileLoad::CheckGetModifiedTime(u32 &modTimeHigh, u32 &modTimeLow, const char* OUTPUT_ONLY(pDebugString))
{
	if (SAVEGAME.CheckGetFileModifiedTime(m_localIndex, modTimeHigh, modTimeLow))
	{	//	returns true for error or success, false for busy
		SAVEGAME.EndGetFileModifiedTime(m_localIndex);

		if (CSavegameUsers::HasPlayerJustSignedOut())
		{
//			CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT);
			return MEM_CARD_ERROR;
		}

		if (CSavegameDevices::HasSelectedDeviceJustBeenRemoved())
		{
//			CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_SELECTED_DEVICE_HAS_BEEN_REMOVED);
			return MEM_CARD_ERROR;
		}

		if (SAVEGAME.GetState(m_localIndex) == fiSaveGameState::HAD_ERROR)
		{
			savegameDisplayf("CSavegameQueuedOperation_ProfileLoad::CheckGetModifiedTime - SAVEGAME.GetState returned an error for %s", pDebugString);
//			CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_GET_FILE_MODIFIED_TIME_FAILED1);
			return MEM_CARD_ERROR;
		}

		if (SAVEGAME.GetError(m_localIndex) != fiSaveGame::SAVE_GAME_SUCCESS)
		{
			savegameDisplayf("CSavegameQueuedOperation_ProfileLoad::CheckGetModifiedTime - SAVEGAME.GetError returned an error for %s", pDebugString);
//			CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_GET_FILE_MODIFIED_TIME_FAILED2);
			return MEM_CARD_ERROR;
		}

		return MEM_CARD_COMPLETE;
	}

	return MEM_CARD_BUSY;
}


bool CSavegameQueuedOperation_ProfileLoad::Shutdown()
{
	//	This Profile Load has been interrupted so request another attempt
	CProfileSettings::GetInstance().Read();
	return true;
}



void CSavegameQueuedOperation_ProfileSave::Init(s32 localIndex, void *pProfileBuffer, s32 BufferSize, eProfileSaveDestination saveProfileDestination)
{
	m_localIndex = localIndex;
	m_pProfileBuffer = pProfileBuffer;
	m_SizeOfProfileBuffer = BufferSize;

	m_SaveProfileDestination = saveProfileDestination;

	if (m_SaveProfileDestination == SAVE_BACKUP_PROFILE_ONLY)
	{
		m_SlotToSaveTo = INDEX_OF_BACKUP_PROFILE_SLOT;
	}
	else
	{
		m_SlotToSaveTo = INDEX_OF_PS3_PROFILE_SLOT;
	}

	m_ProfileSaveProgress = PROFILE_SAVE_PROGRESS_BEGIN_SAVE;
}


bool CSavegameQueuedOperation_ProfileSave::Update()
{
	switch (m_ProfileSaveProgress)
	{
	case PROFILE_SAVE_PROGRESS_BEGIN_SAVE :
		CSavegameDialogScreens::SetDialogBoxHeading("SG_HDNG");	//	SG_HDG_PROF_SV	//	Not sure if this will ever be displayed

		CSavingMessage::BeginDisplaying(CSavingMessage::STORAGE_DEVICE_MESSAGE_SAVING);

		CSavegameFilenames::MakeValidSaveNameForLocalFile(m_SlotToSaveTo);
		if (!SAVEGAME.BeginSave(m_localIndex, 0, CSavegameFilenames::GetNameToDisplay(), CSavegameFilenames::GetFilenameOfLocalFile(), m_pProfileBuffer, m_SizeOfProfileBuffer, true) )	//	bool overwrite)
		{
			savegameDisplayf("CSavegameQueuedOperation_ProfileSave::Update - BeginSave failed");
			SAVEGAME.EndSave(m_localIndex);

			CSavingMessage::Clear();

			m_Status = MEM_CARD_ERROR;
			return true;
		}

		m_ProfileSaveProgress = PROFILE_SAVE_PROGRESS_CHECK_SAVE;
		break;

	case PROFILE_SAVE_PROGRESS_CHECK_SAVE :
		{
			bool bOutIsValid;
			bool bFileExists;

			if (SAVEGAME.CheckSave(m_localIndex, bOutIsValid, bFileExists))
			{
				SAVEGAME.EndSave(m_localIndex);

				savegameDisplayf("CSavegameQueuedOperation_ProfileSave::Update - about to call CSavingMessage::Clear()");
				CSavingMessage::Clear();

				if (m_SaveProfileDestination == SAVE_BOTH_PROFILES)
				{
					if (m_SlotToSaveTo == INDEX_OF_PS3_PROFILE_SLOT)
					{
						savegameDisplayf("CSavegameQueuedOperation_ProfileSave::Update - finished saving the main profile. The flag is set to save both the main and the backup profile now so move on to saving the backup profile");
						m_SlotToSaveTo = INDEX_OF_BACKUP_PROFILE_SLOT;
						m_ProfileSaveProgress = PROFILE_SAVE_PROGRESS_BEGIN_SAVE;
					}
					else
					{
						savegameDisplayf("CSavegameQueuedOperation_ProfileSave::Update - finished saving the backup profile");
						m_Status = MEM_CARD_COMPLETE;
						return true;
					}
				}
				else
				{
					if (m_SaveProfileDestination == SAVE_BACKUP_PROFILE_ONLY)
					{
						savegameDisplayf("CSavegameQueuedOperation_ProfileSave::Update - finished saving the backup profile so request a save of the main profile when the streaming isn't busy");

						CProfileSettings::GetInstance().RequestMainProfileSave();
					}
					else if (savegameVerifyf(m_SaveProfileDestination == SAVE_MAIN_PROFILE_ONLY, "CSavegameQueuedOperation_ProfileSave::Update - expected m_SaveProfileDestination to be SAVE_MAIN_PROFILE_ONLY"))
					{
						savegameDisplayf("CSavegameQueuedOperation_ProfileSave::Update - finished saving the main profile so set status to MEM_CARD_COMPLETE");
					}

					m_Status = MEM_CARD_COMPLETE;
					return true;
				}
			}
		}
		break;
	}

	return false;
}

bool CSavegameQueuedOperation_ProfileSave::Shutdown()
{
	//	I don't think I need to do anything here. I'll just rely on CGenericGameStorage::FinishAllUnfinishedSaveGameOperations()
	return false;
}


#else	//	USE_DOWNLOAD0_FOR_PROFILE_BACKUP

void CSavegameQueuedOperation_ProfileLoad::Init(s32 localIndex, char *pFilenameOfPS3Profile, void *pProfileBuffer, s32 BufferSize)
{
	m_localIndex = localIndex;
	m_FilenameOfPS3Profile = pFilenameOfPS3Profile;
	m_pProfileBuffer = pProfileBuffer;
	m_SizeOfProfileBuffer = BufferSize;

	// Results
	m_SizeOfLoadedProfile = 0;

	m_ProfileLoadProgress = PROFILE_LOAD_PROGRESS_BEGIN_CREATOR_CHECK;
}

bool CSavegameQueuedOperation_ProfileLoad::Update()
{
	switch (m_ProfileLoadProgress)
	{
		case PROFILE_LOAD_PROGRESS_BEGIN_CREATOR_CHECK :

			CSavegameDialogScreens::SetDialogBoxHeading("SG_HDNG");	//	SG_HDG_PROF_LD	//	Not sure if this will ever be displayed

			if (CGenericGameStorage::DoCreatorCheck())
			{
				if (!SAVEGAME.BeginGetCreator(m_localIndex, 0, 0, m_FilenameOfPS3Profile) )
				{
					Assertf(0, "CSavegameQueuedOperation_ProfileLoad::Update - BeginGetCreator failed");
					savegameWarningf("CSavegameQueuedOperation_ProfileLoad::Update - BeginGetCreator failed\n");
					m_Status = MEM_CARD_ERROR;
					return true;
				}
				else
				{
					m_ProfileLoadProgress = PROFILE_LOAD_PROGRESS_CHECK_CREATOR_CHECK;
				}
			}
			else
			{
				m_ProfileLoadProgress = PROFILE_LOAD_PROGRESS_BEGIN_LOAD;
			}
			break;

		case PROFILE_LOAD_PROGRESS_CHECK_CREATOR_CHECK :
			{
				bool bIsCreator = false;
#if __PPU
				bool bHasCreator = false;
				if (SAVEGAME.CheckGetCreator(m_localIndex, bIsCreator, bHasCreator))
#else
				if (SAVEGAME.CheckGetCreator(m_localIndex, bIsCreator))
#endif	//	__PPU
				{	//	returns true for error or success, false for busy
					SAVEGAME.EndGetCreator(m_localIndex);

					m_ProfileLoadProgress = PROFILE_LOAD_PROGRESS_BEGIN_LOAD;

					if (SAVEGAME.GetState(m_localIndex) == fiSaveGameState::HAD_ERROR)
					{
						savegameWarningf("CSavegameQueuedOperation_ProfileLoad::Update - SAVE_GAME_DIALOG_CHECK_GET_CREATOR_FAILED1\n");
						m_Status = MEM_CARD_ERROR;
						return true;
					}
					else
					{
						if (SAVEGAME.GetError(m_localIndex) != fiSaveGame::SAVE_GAME_SUCCESS)
						{
							savegameWarningf("CSavegameQueuedOperation_ProfileLoad::Update - SAVE_GAME_DIALOG_CHECK_GET_CREATOR_FAILED2\n");
							m_Status = MEM_CARD_ERROR;
							return true;
						}
						else
						{
#if __PPU
							if (!bHasCreator || !bIsCreator)
#else
							if (!bIsCreator)
#endif	//	__PPU
							{
								savegameWarningf("CSavegameQueuedOperation_ProfileLoad::Update - SAVE_GAME_DIALOG_PLAYER_IS_NOT_CREATOR\n");
								m_Status = MEM_CARD_ERROR;
								return true;
							}
						}
					}
				}
			}
			break;

		case PROFILE_LOAD_PROGRESS_BEGIN_LOAD :

			CSavegameDialogScreens::SetDialogBoxHeading("SG_HDNG");	//	SG_HDG_PROF_LD	//	Not sure if this will ever be displayed

			if(!SAVEGAME.BeginLoad(m_localIndex, 0, 0, m_FilenameOfPS3Profile, m_pProfileBuffer, m_SizeOfProfileBuffer))
			{
				Warningf("CSavegameQueuedOperation_ProfileLoad::Update - BeginLoad failed\n");
				SAVEGAME.EndLoad(m_localIndex);
				m_Status = MEM_CARD_ERROR;
				return true;
			}
			m_ProfileLoadProgress = PROFILE_LOAD_PROGRESS_CHECK_LOAD;
			break;

		case PROFILE_LOAD_PROGRESS_CHECK_LOAD :
		{
			bool valid;
			if (SAVEGAME.CheckLoad(m_localIndex, valid, m_SizeOfLoadedProfile))
			{
				SAVEGAME.EndLoad(m_localIndex);

				if(!valid)
				{
					savegameWarningf("CSavegameQueuedOperation_ProfileLoad::Update - SAVE_GAME_DIALOG_CHECK_LOAD_FAILED2\n");
					m_Status = MEM_CARD_ERROR;
					return true;
				}

				m_Status = MEM_CARD_COMPLETE;

				return true;
			}
			break;
		}
	}

	return false;
}

bool CSavegameQueuedOperation_ProfileLoad::Shutdown()
{
	//	This Profile Load has been interrupted so request another attempt
	CProfileSettings::GetInstance().Read();
	return true;
}


void CSavegameQueuedOperation_ProfileSave::Init(s32 localIndex, char16 ProfileName[LengthOfProfileName], char *pFilenameOfPS3Profile, void *pProfileBuffer, s32 BufferSize)
{
	m_localIndex = localIndex;
	wcsncpy(m_ProfileName, ProfileName, LengthOfProfileName);
	m_FilenameOfPS3Profile = pFilenameOfPS3Profile;
	m_pProfileBuffer = pProfileBuffer;
	m_SizeOfProfileBuffer = BufferSize;

	m_ProfileSaveProgress = PROFILE_SAVE_PROGRESS_BEGIN_SAVE;
}

bool CSavegameQueuedOperation_ProfileSave::Update()
{
	switch (m_ProfileSaveProgress)
	{
		case PROFILE_SAVE_PROGRESS_BEGIN_SAVE :
			CSavegameDialogScreens::SetDialogBoxHeading("SG_HDNG");	//	SG_HDG_PROF_SV	//	Not sure if this will ever be displayed

			CSavingMessage::BeginDisplaying(CSavingMessage::STORAGE_DEVICE_MESSAGE_SAVING);

			if (!SAVEGAME.BeginSave(m_localIndex, 0, m_ProfileName, m_FilenameOfPS3Profile, m_pProfileBuffer, m_SizeOfProfileBuffer, true) )	//	bool overwrite)
			{
				Warningf("CProfileSettings::StartWrite - BeginSave failed\n");
				SAVEGAME.EndSave(m_localIndex);

				CSavingMessage::Clear();

				m_Status = MEM_CARD_ERROR;
				return true;
			}

			m_ProfileSaveProgress = PROFILE_SAVE_PROGRESS_CHECK_SAVE;
			break;

		case PROFILE_SAVE_PROGRESS_CHECK_SAVE :
		{
			bool bOutIsValid;
			bool bFileExists;

			if (SAVEGAME.CheckSave(m_localIndex, bOutIsValid, bFileExists))
			{
				SAVEGAME.EndSave(m_localIndex);

				savegameDisplayf("CSavegameQueuedOperation_ProfileSave::Update - about to call CSavingMessage::Clear()");
				CSavingMessage::Clear();

				m_Status = MEM_CARD_COMPLETE;
				return true;
			}
		}
		break;
	}

	return false;
}

bool CSavegameQueuedOperation_ProfileSave::Shutdown()
{
//	I don't think I need to do anything here. I'll just rely on CGenericGameStorage::FinishAllUnfinishedSaveGameOperations()
	return false;
}

#endif	//	USE_DOWNLOAD0_FOR_PROFILE_BACKUP

#endif // USE_PROFILE_SAVEGAME

#if RSG_ORBIS

// *********************** PS3DamagedCheck *****************************

void CSavegameQueuedOperation_PS4DamagedCheck::Init(s32 slotIndex)
{
	//	Set m_slotIndex even if it's out of range
	//	CSavegameQueuedOperation_PS3DamagedCheck::Update() will handle the slot being out of range and will return MEM_CARD_ERROR
	savegameAssertf((slotIndex >= 0) && (slotIndex < MAX_NUM_EXPECTED_SAVE_FILES), "CSavegameQueuedOperation_PS3DamagedCheck::Init - slot index %d is out of range 0 to %d", slotIndex, MAX_NUM_EXPECTED_SAVE_FILES);
	m_slotIndex = slotIndex;
	savegameDebugf1("CSavegameQueuedOperation_PS3DamagedCheck::Init called for slot %d. Time = %d\n", slotIndex, sysTimer::GetSystemMsTime());
}

bool CSavegameQueuedOperation_PS4DamagedCheck::Shutdown()
{
	CSavegameDamagedCheck::ResetProgressState();

	return true;
}

//	Return true to Pop() from queue
bool CSavegameQueuedOperation_PS4DamagedCheck::Update()
{
	//	I could call CSavegameDialogScreens::SetDialogBoxHeading() to set a heading here but I don't think it would ever be displayed

	if ( (m_slotIndex < 0) || (m_slotIndex >= MAX_NUM_EXPECTED_SAVE_FILES) )
	{
		savegameAssertf(0, "CSavegameQueuedOperation_PS3DamagedCheck::Update - slot index %d is out of range 0 to %d", m_slotIndex, MAX_NUM_EXPECTED_SAVE_FILES);
		m_Status = MEM_CARD_ERROR;
		return true;
	}

	if (CSavegameList::IsSaveGameSlotEmpty(m_slotIndex))
	{
		savegameDebugf1("CSavegameQueuedOperation_PS3DamagedCheck::Update - slot %d is empty. Time = %d\n", m_slotIndex, sysTimer::GetSystemMsTime());
		m_Status = MEM_CARD_COMPLETE;
		return true;
	}

	switch (CSavegameList::CheckIfSlotIsDamaged(m_slotIndex))
	{
		case MEM_CARD_COMPLETE :
			savegameDebugf1("CSavegameQueuedOperation_PS3DamagedCheck::Update - completed for slot %d. Time = %d. Slot is %sdamaged\n", m_slotIndex, sysTimer::GetSystemMsTime(), CSavegameList::GetIsDamaged(m_slotIndex)?"":"not ");
			m_Status = MEM_CARD_COMPLETE;
			return true;
//			break;

		case MEM_CARD_BUSY :
			break;

		case MEM_CARD_ERROR :
			m_Status = MEM_CARD_ERROR;
			return true;
//			break;
	}

	return false;
}

#endif	//	__PPU



// Photos *********************************************************************************************************
#if __LOAD_LOCAL_PHOTOS
void CSavegameQueuedOperation_LoadLocalPhoto::Init(const CSavegamePhotoUniqueId &uniquePhotoId, CPhotoBuffer *pPhotoBuffer)
{
	savegameDisplayf("CSavegameQueuedOperation_LoadLocalPhoto::Init called for unique photo Id %d", uniquePhotoId.GetValue());

	m_UniquePhotoId = uniquePhotoId;

	// check that pPhotoBuffer isn't NULL
	m_pBufferToStoreLoadedPhoto = pPhotoBuffer;

	m_SizeOfJpegBuffer = CPhotoBuffer::GetDefaultSizeOfJpegBuffer();

	m_TextureDownloadRequestHandle = CTextureDownloadRequest::INVALID_HANDLE;

	m_LocalPhotoLoadProgress = LOCAL_PHOTO_LOAD_PROGRESS_BEGIN_LOAD;
}

void CSavegameQueuedOperation_LoadLocalPhoto::EndLoadLocalPhoto()
{
//	I think the save operation will have already been set back to OPERATION_NONE after LOCAL_PHOTO_LOAD_PROGRESS_CHECK_LOAD 
//	photoAssertf(CGenericGameStorage::GetSaveOperation() == OPERATION_LOADING_LOCAL_PHOTO, "CSavegameQueuedOperation_LoadLocalPhoto::EndLoadLocalPhoto - SaveOperation should be OPERATION_LOADING_LOCAL_PHOTO");
	CGenericGameStorage::SetSaveOperation(OPERATION_NONE);
}

//    Return true to Pop() from queue
bool CSavegameQueuedOperation_LoadLocalPhoto::Update()
{
	switch (m_LocalPhotoLoadProgress)
	{
	case LOCAL_PHOTO_LOAD_PROGRESS_BEGIN_LOAD :
		{
			CSavegameDialogScreens::SetDialogBoxHeading("SG_HDNG");

//			CGenericGameStorage::SetSaveOperation(OPERATION_LOADING_PHOTO_FROM_CLOUD);
//			CSavegameInitialChecks::BeginInitialChecks(false);

			// check that uniquePhotoId is valid
			// check that m_pBufferToStoreLoadedPhoto isn't NULL

			switch (CSavegamePhotoLoadLocal::BeginLocalPhotoLoad(m_UniquePhotoId, m_pBufferToStoreLoadedPhoto))
			{
				case MEM_CARD_COMPLETE :
					m_LocalPhotoLoadProgress = LOCAL_PHOTO_LOAD_PROGRESS_CHECK_LOAD;
					break;
				case MEM_CARD_BUSY :
					photoAssertf(0, "CSavegameQueuedOperation_LoadLocalPhoto::Update - didn't expect CSavegamePhotoLoadLocal::BeginLocalPhotoLoad to ever return MEM_CARD_BUSY");
					break;
				case MEM_CARD_ERROR :
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_BEGIN_LOCAL_LOAD_JPEG_FAILED);
					m_LocalPhotoLoadProgress = LOCAL_PHOTO_LOAD_PROGRESS_HANDLE_ERRORS;
					break;
			}
		}
		break;

	case LOCAL_PHOTO_LOAD_PROGRESS_CHECK_LOAD :
		{
			CSavingMessage::BeginDisplaying(CSavingMessage::STORAGE_DEVICE_MESSAGE_CLOUD_LOADING);

			MemoryCardError LoadState = CSavegamePhotoLoadLocal::DoLocalPhotoLoad();
			if (LoadState == MEM_CARD_ERROR)
			{
				if (SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT == CSavegameDialogScreens::GetMostRecentErrorCode())
				{
					photoDisplayf("CSavegameQueuedOperation_LoadLocalPhoto::Update - player has signed out");
					m_Status = MEM_CARD_ERROR;
				}
				else
				{
					photoDisplayf("CSavegameQueuedOperation_LoadLocalPhoto::Update - CSavegamePhotoLoadLocal::DoLocalPhotoLoad() returned an error");
					m_LocalPhotoLoadProgress = LOCAL_PHOTO_LOAD_PROGRESS_HANDLE_ERRORS;
				}
			}
			else if (LoadState == MEM_CARD_COMPLETE)
			{
				photoDisplayf("CSavegameQueuedOperation_LoadLocalPhoto::Update - CSavegamePhotoLoadLocal::DoLocalPhotoLoad() succeeded");

				m_LocalPhotoLoadProgress = LOCAL_PHOTO_LOAD_PROGRESS_REQUEST_TEXTURE_DECODE;
			}
		}
		break;

	case LOCAL_PHOTO_LOAD_PROGRESS_REQUEST_TEXTURE_DECODE :
		{
//			if (!CSavegameUsers::HasPlayerJustSignedOut() && NetworkInterface::IsCloudAvailable())
			{
//				if (photoVerifyf(CPhotoManager::GetHasValidMetadata(m_IndexWithinMergedPhotoList), "CSavegameQueuedOperation_LoadUGCPhoto::Update - invalid metadata!"))
				{
					char textureName[CTextureDictionariesForGalleryPhotos::ms_MaxLengthOfTextureName];
					CTextureDictionariesForGalleryPhotos::GetNameOfPhotoTextureDictionary(m_UniquePhotoId, textureName, CTextureDictionariesForGalleryPhotos::ms_MaxLengthOfTextureName);

					CTextureDecodeRequestDesc requestDesc;

					requestDesc.m_TxtAndTextureHash	= textureName;
					requestDesc.m_Type				= CTextureDecodeRequestDesc::JPEG_BUFFER;	//	CTextureDownloadRequestDesc::UGC_FILE;

					requestDesc.m_BufferPtr			= m_pBufferToStoreLoadedPhoto->GetJpegBuffer();			// Buffer with the data blob (memory is owned by the user and must guarantee it will be valid until the request completes).

// Is the size in bytes or KBytes?
//	Should I be using GetSizeOfAllocatedJpegBuffer()?
//	Do I not need m_SizeOfJpegBuffer?
					requestDesc.m_BufferSize		= m_pBufferToStoreLoadedPhoto->GetExactSizeOfJpegData();			// Size for the buffer

					if (m_UniquePhotoId.GetIsMaximised())
					{
						requestDesc.m_JPEGScalingFactor = VideoResManager::DOWNSCALE_ONE;
					}
					else
					{
						requestDesc.m_JPEGScalingFactor = CHighQualityScreenshot::GetDownscaleForNoneMaximizedView();
					}

//					bool								m_JPEGEncodeAsDXT;		// JPEG-only: allows specifying whether the JPEG should be encoded as a DXT1 texture, rather than uncompressed RGBA8.

					// Issue the request
					TextureDownloadRequestHandle handle;
					CTextureDownloadRequest::Status retVal;
					retVal = DOWNLOADABLETEXTUREMGR.RequestTextureDecode(handle, requestDesc);

					// Check the return value
					if ( (retVal == CTextureDownloadRequest::IN_PROGRESS) || (retVal == CTextureDownloadRequest::READY_FOR_USER) )
					{
						photoDisplayf("CSavegameQueuedOperation_LoadLocalPhoto::Update - RequestTextureDecode succeeded. Return value was %d", retVal);

						// Cache the handle for querying the state of the request
						m_TextureDownloadRequestHandle = handle;
						m_LocalPhotoLoadProgress = LOCAL_PHOTO_LOAD_PROGRESS_WAIT_FOR_TEXTURE_DECODE;
					}
					else
					{
						photoDisplayf("CSavegameQueuedOperation_LoadLocalPhoto::Update - RequestTextureDecode failed. Return value was %d", retVal);

						CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_BEGIN_LOCAL_LOAD_JPEG_FAILED);
						m_LocalPhotoLoadProgress = LOCAL_PHOTO_LOAD_PROGRESS_HANDLE_ERRORS;
					}
				}
// 				else
// 				{
// 					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_BEGIN_LOCAL_LOAD_JPEG_FAILED);
// 					m_LocalPhotoLoadProgress = LOCAL_PHOTO_LOAD_PROGRESS_HANDLE_ERRORS;
// 				}
			}
// 			else
// 			{
// 				photoDisplayf("CSavegameQueuedOperation_LoadUGCPhoto::Update - REQUEST_TEXTURE_DOWNLOAD - player signed out or cloud not available");
// 
// 				CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_BEGIN_LOCAL_LOAD_JPEG_FAILED);
// 				m_LocalPhotoLoadProgress = LOCAL_PHOTO_LOAD_PROGRESS_HANDLE_ERRORS;
// 			}
		}
		break;


	case LOCAL_PHOTO_LOAD_PROGRESS_WAIT_FOR_TEXTURE_DECODE :
		if (m_TextureDownloadRequestHandle == CTextureDownloadRequest::INVALID_HANDLE)
		{
			photoAssertf(0, "CSavegameQueuedOperation_LoadLocalPhoto::Update - didn't expect TextureDownloadRequestHandle to be invalid inside LOCAL_PHOTO_LOAD_PROGRESS_WAIT_FOR_TEXTURE_DECODE");
			//			m_Status = MEM_CARD_ERROR;
			CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_LOCAL_LOAD_JPEG_FAILED);
			m_LocalPhotoLoadProgress = LOCAL_PHOTO_LOAD_PROGRESS_HANDLE_ERRORS;
		}
		else
		{
			// Reset our handle if the request has failed
			if (DOWNLOADABLETEXTUREMGR.HasRequestFailed(m_TextureDownloadRequestHandle))
			{
				photoDisplayf("CSavegameQueuedOperation_LoadLocalPhoto::Update - WAIT_FOR_TEXTURE_DECODE - request failed");

				DOWNLOADABLETEXTUREMGR.ReleaseTextureDownloadRequest(m_TextureDownloadRequestHandle);
				m_TextureDownloadRequestHandle = CTextureDownloadRequest::INVALID_HANDLE;
				//				m_Status = MEM_CARD_ERROR;
				CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_LOCAL_LOAD_JPEG_FAILED);
				m_LocalPhotoLoadProgress = LOCAL_PHOTO_LOAD_PROGRESS_HANDLE_ERRORS;
			}
			// Check if our request is ready
			else if (DOWNLOADABLETEXTUREMGR.IsRequestReady(m_TextureDownloadRequestHandle))
			{
				photoDisplayf("CSavegameQueuedOperation_LoadLocalPhoto::Update - WAIT_FOR_TEXTURE_DECODE - request is ready");

				strLocalIndex txdSlotForRequest = DOWNLOADABLETEXTUREMGR.GetRequestTxdSlot(m_TextureDownloadRequestHandle);

				// Add a reference for us before releasing the request
				g_TxdStore.AddRef(txdSlotForRequest, REF_OTHER);

				m_LocalPhotoLoadProgress = LOCAL_PHOTO_LOAD_PROGRESS_READ_METADATA;
			}
		}
		break;

	case LOCAL_PHOTO_LOAD_PROGRESS_READ_METADATA :
		{
			CSavegamePhotoMetadata metadataForLoadedPhoto;

			if (photoVerifyf(metadataForLoadedPhoto.ReadMetadataFromString(m_pBufferToStoreLoadedPhoto->GetJsonBuffer()), "CSavegameQueuedOperation_LoadLocalPhoto::Update - failed to read metadata from json string"))
			{
				atString titleString(m_pBufferToStoreLoadedPhoto->GetTitleString());
				atString descriptionString(m_pBufferToStoreLoadedPhoto->GetDescriptionString());
				CSavegamePhotoLocalMetadata::Add(m_UniquePhotoId, metadataForLoadedPhoto, titleString, descriptionString);

				eImageSignatureResult validateResult = CPhotoManager::ValidateCRCForImage(metadataForLoadedPhoto.GetImageSignature(), m_pBufferToStoreLoadedPhoto->GetJpegBuffer(), m_pBufferToStoreLoadedPhoto->GetExactSizeOfJpegData());
				if (photoVerifyf(IMAGE_SIGNATURE_MISMATCH != validateResult, "CSavegameQueuedOperation_LoadLocalPhoto::Update - image signature validation failed. Can't use this image."))
				{
					CMetadataForOnePhoto *pMetadata = CSavegamePhotoLocalMetadata::GetMetadataForUniquePhotoId(m_UniquePhotoId);
					if (photoVerifyf(pMetadata, "CSavegameQueuedOperation_LoadLocalPhoto::Update - failed to find metadata for unique photo Id %d", m_UniquePhotoId.GetValue()))
					{
						if (validateResult == IMAGE_SIGNATURE_VALID)
						{
							pMetadata->SetHasValidSignature(true);
						}
						else
						{
							pMetadata->SetHasValidSignature(false);
						}
					}

					m_LocalPhotoLoadProgress = LOCAL_PHOTO_LOAD_PROGRESS_WAIT_FOR_LOADING_MESSAGE_TO_FINISH;
				}
				else
				{
					strLocalIndex txdSlotForRequest = DOWNLOADABLETEXTUREMGR.GetRequestTxdSlot(m_TextureDownloadRequestHandle);
					g_TxdStore.RemoveRef(txdSlotForRequest, REF_OTHER);

					//	Should I display an error message, continue without a warning or set m_Status to MEM_CARD_ERROR?
					m_Status = MEM_CARD_ERROR;
				}
			}
			else
			{
				//	Should I display an error message, continue without a warning or set m_Status to MEM_CARD_ERROR?
				m_Status = MEM_CARD_ERROR;
			}
		}
		break;

	case LOCAL_PHOTO_LOAD_PROGRESS_WAIT_FOR_LOADING_MESSAGE_TO_FINISH :
		if (CSavingMessage::HasLoadingMessageDisplayedForLongEnough())
		{
			// The following is called for savegames. I'm not sure if it's needed for photos / MP stats.
			CSavegameDialogScreens::SetMessageAsProcessing(false);	//	GSWTODO - not sure if this should be in here or in DeSerialize()

			EndLoadLocalPhoto();

			m_Status = MEM_CARD_COMPLETE;
		}
		break;


	case LOCAL_PHOTO_LOAD_PROGRESS_HANDLE_ERRORS :
		{
			CSavingMessage::Clear();

			bool bDisplayingMessageAboutFailingToLoadAThumbnail = false;
			if (!m_UniquePhotoId.GetIsMaximised())
			{	//	if loading a thumbnail photo
				switch (CSavegameDialogScreens::GetMostRecentErrorCode())
				{
				case SAVE_GAME_DIALOG_BEGIN_LOCAL_LOAD_JPEG_FAILED :
				case SAVE_GAME_DIALOG_CHECK_LOCAL_LOAD_JPEG_FAILED :
//				case SAVE_GAME_DIALOG_BEGIN_LOCAL_LOAD_JSON_FAILED :
//				case SAVE_GAME_DIALOG_CHECK_LOCAL_LOAD_JSON_FAILED :
					bDisplayingMessageAboutFailingToLoadAThumbnail = true;
					break;
				default :
					break;
				}
			}

			if (bDisplayingMessageAboutFailingToLoadAThumbnail && CPhotoManager::GetWarningMessageForGalleryPhotoLoadFailureHasAlreadyBeenSeen())
			{
				CSavegameDialogScreens::ClearSaveGameError();
				m_Status = MEM_CARD_ERROR;
			}
			else
			{
				if (CSavegameDialogScreens::HandleSaveGameErrorCode())
				{
					if (bDisplayingMessageAboutFailingToLoadAThumbnail)
					{
						CPhotoManager::SetWarningMessageForGalleryPhotoLoadFailureHasAlreadyBeenSeen(true);
					}
					m_Status = MEM_CARD_ERROR;
				}
			}
		}
		break;
	}


	if ( (m_Status == MEM_CARD_ERROR) || (m_Status == MEM_CARD_COMPLETE) )
	{
		if (m_Status == MEM_CARD_ERROR)
		{
			//		savegameWarningf("Error in CalculateSizeOfRequiredBuffer: code=%d", CSavegameDialogScreens::GetMostRecentErrorCode());
			photoDisplayf("CSavegameQueuedOperation_LoadLocalPhoto::Update - failed");

			CSavingMessage::Clear();

			EndLoadLocalPhoto();

			// The following is called for savegames. I'm not sure if it's needed for photos / MP stats.
			CSavegameDialogScreens::SetMessageAsProcessing(false);

			CPhotoManager::SetTextureDownloadFailed(m_UniquePhotoId);
		}
		else if (m_Status == MEM_CARD_COMPLETE)
		{
			photoDisplayf("CSavegameQueuedOperation_LoadLocalPhoto::Update - succeeded");

			CPhotoManager::SetTextureDownloadSucceeded(m_UniquePhotoId);
		}

		if (m_TextureDownloadRequestHandle != CTextureDownloadRequest::INVALID_HANDLE)
		{
			// We have the txd slot, we can now release our request
			DOWNLOADABLETEXTUREMGR.ReleaseTextureDownloadRequest(m_TextureDownloadRequestHandle);
			m_TextureDownloadRequestHandle = CTextureDownloadRequest::INVALID_HANDLE;
		}

		m_pBufferToStoreLoadedPhoto->FreeBuffer();

		return true;
	}

	return false;
}
#endif	//	__LOAD_LOCAL_PHOTOS

#if !__LOAD_LOCAL_PHOTOS
void CSavegameQueuedOperation_LoadUGCPhoto::Init(const CIndexWithinMergedPhotoList &indexWithinMergedPhotoList, CIndexOfPhotoOnCloud indexOfPhotoFile)
{
// 	if (m_pMetadataString)
// 	{
// 		photoAssertf(0, "CSavegameQueuedOperation_LoadUGCPhoto::Init - expected metadata string to be NULL here");
// 		delete [] m_pMetadataString;
// 		m_pMetadataString = NULL;
// 	}

	m_IndexWithinMergedPhotoList = indexWithinMergedPhotoList;
	m_IndexOfPhotoFile = indexOfPhotoFile;

	m_SizeOfJpegBuffer = CPhotoBuffer::GetDefaultSizeOfJpegBuffer();
//	m_SizeOfJsonBuffer = CPhotoBuffer::GetDefaultSizeOfJsonBuffer();

//	m_TxdSlotForRequest = -1;
	m_TextureDownloadRequestHandle = CTextureDownloadRequest::INVALID_HANDLE;

	m_UGCPhotoLoadProgress = UGC_PHOTO_LOAD_PROGRESS_BEGIN_PLAYER_IS_SIGNED_IN;
}

void CSavegameQueuedOperation_LoadUGCPhoto::EndLoadBlockCloud()
{
	photoAssertf(CGenericGameStorage::GetSaveOperation() == OPERATION_LOADING_PHOTO_FROM_CLOUD, "CSavegameQueuedOperation_LoadUGCPhoto::EndLoadBlock - SaveOperation should be OPERATION_LOADING_PHOTO_FROM_CLOUD");
	CGenericGameStorage::SetSaveOperation(OPERATION_NONE);
}


//    Return true to Pop() from queue
bool CSavegameQueuedOperation_LoadUGCPhoto::Update()
{
	switch (m_UGCPhotoLoadProgress)
	{
	case UGC_PHOTO_LOAD_PROGRESS_BEGIN_PLAYER_IS_SIGNED_IN :
		{
			CSavegameDialogScreens::SetDialogBoxHeading("SG_HDNG");	//	SG_HDG_L_PH_CL

			CSavegameFilenames::SetFilenameOfCloudFile("");	//	Hopefully the InitialChecks don't rely on a valid savegame filename

			CGenericGameStorage::SetSaveOperation(OPERATION_LOADING_PHOTO_FROM_CLOUD);
			CSavegameInitialChecks::BeginInitialChecks(false);

			m_UGCPhotoLoadProgress = UGC_PHOTO_LOAD_PROGRESS_CHECK_PLAYER_IS_SIGNED_IN;
		}
		break;
	case UGC_PHOTO_LOAD_PROGRESS_CHECK_PLAYER_IS_SIGNED_IN :
		{
			CSavingMessage::BeginDisplaying(CSavingMessage::STORAGE_DEVICE_MESSAGE_CLOUD_LOADING);

			MemoryCardError SelectionState = CSavegameInitialChecks::InitialChecks();
			if (SelectionState == MEM_CARD_ERROR)
			{
				if (SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT == CSavegameDialogScreens::GetMostRecentErrorCode())
				{
					photoDisplayf("CSavegameQueuedOperation_LoadUGCPhoto::Update - player has signed out");
					m_Status = MEM_CARD_ERROR;
				}
				else
				{
					photoDisplayf("CSavegameQueuedOperation_LoadUGCPhoto::Update - InitialChecks() returned an error");
					m_UGCPhotoLoadProgress = UGC_PHOTO_LOAD_PROGRESS_HANDLE_ERRORS;
				}
			}
			else if (SelectionState == MEM_CARD_COMPLETE)
			{
				photoDisplayf("CSavegameQueuedOperation_LoadUGCPhoto::Update - InitialChecks() succeeded");

				if (CPhotoManager::GetHasValidMetadata(m_IndexWithinMergedPhotoList))
				{
					m_UGCPhotoLoadProgress = UGC_PHOTO_LOAD_PROGRESS_REQUEST_TEXTURE_DOWNLOAD;
				}
				else
				{
//					m_Status = MEM_CARD_ERROR;
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_BEGIN_CLOUD_LOAD_JPEG_FAILED);
					m_UGCPhotoLoadProgress = UGC_PHOTO_LOAD_PROGRESS_HANDLE_ERRORS;
				}
			}
		}
		break;

	case UGC_PHOTO_LOAD_PROGRESS_REQUEST_TEXTURE_DOWNLOAD :
		{
			if (!CSavegameUsers::HasPlayerJustSignedOut() && NetworkInterface::IsCloudAvailable())
			{
				if (photoVerifyf(CPhotoManager::GetHasValidMetadata(m_IndexWithinMergedPhotoList), "CSavegameQueuedOperation_LoadUGCPhoto::Update - invalid metadata!"))
				{
					char textureName[CTextureDictionariesForGalleryPhotos::ms_MaxLengthOfTextureName];
					CTextureDictionariesForGalleryPhotos::GetNameOfPhotoTextureDictionary(m_IndexWithinMergedPhotoList, textureName, CTextureDictionariesForGalleryPhotos::ms_MaxLengthOfTextureName);

					CTextureDownloadRequestDesc requestDesc;
					requestDesc.m_TxdName			= textureName;
					requestDesc.m_TextureName		= textureName;
					requestDesc.m_Type				= CTextureDownloadRequestDesc::UGC_FILE;

                    requestDesc.m_CloudFilePath		= CPhotoManager::GetContentIdOfPhotoOnCloud(m_IndexWithinMergedPhotoList);
					requestDesc.m_BufferPresize		= m_SizeOfJpegBuffer;
					requestDesc.m_GamerIndex		= NetworkInterface::GetLocalGamerIndex();
					requestDesc.m_CloudMemberId		= rlCloudMemberId();
					requestDesc.m_UseCacheFile		= true;

                    requestDesc.m_nFileID		    = CPhotoManager::GetFileID(m_IndexWithinMergedPhotoList);
                    requestDesc.m_nFileVersion		= CPhotoManager::GetFileVersion(m_IndexWithinMergedPhotoList);
                    requestDesc.m_nLanguage		    = CPhotoManager::GetLanguage(m_IndexWithinMergedPhotoList);

                    if (m_IndexWithinMergedPhotoList.GetIsMaximised())
					{
						requestDesc.m_JPEGScalingFactor = VideoResManager::DOWNSCALE_ONE;
					}
					else
					{
						requestDesc.m_JPEGScalingFactor = CHighQualityScreenshot::GetDownscaleForNoneMaximizedView();
					}


					// Issue the request
					TextureDownloadRequestHandle handle;
					CTextureDownloadRequest::Status retVal;
					retVal = DOWNLOADABLETEXTUREMGR.RequestTextureDownload(handle, requestDesc);

					// Check the return value
					if ( (retVal == CTextureDownloadRequest::IN_PROGRESS) || (retVal == CTextureDownloadRequest::READY_FOR_USER) )
					{
						photoDisplayf("CSavegameQueuedOperation_LoadUGCPhoto::Update - RequestTextureDownload succeeded. Return value was %d", retVal);

	//					m_bWaitingForTextureDownload = true;
	//					m_bTextureDownloadSucceeded = false;

						// Cache the handle for querying the state of the request
						m_TextureDownloadRequestHandle = handle;
						m_UGCPhotoLoadProgress = UGC_PHOTO_LOAD_PROGRESS_WAIT_FOR_TEXTURE_DOWNLOAD;
					}
					else
					{
						photoDisplayf("CSavegameQueuedOperation_LoadUGCPhoto::Update - RequestTextureDownload failed. Return value was %d", retVal);

						//	dlcWarningf("CDownloadableTextureManagerDebug::OnLoadTexture: request failed before being issued");
//						m_Status = MEM_CARD_ERROR;
						CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_BEGIN_CLOUD_LOAD_JPEG_FAILED);
						m_UGCPhotoLoadProgress = UGC_PHOTO_LOAD_PROGRESS_HANDLE_ERRORS;
					}
				}
				else
				{
//					m_Status = MEM_CARD_ERROR;
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_BEGIN_CLOUD_LOAD_JPEG_FAILED);
					m_UGCPhotoLoadProgress = UGC_PHOTO_LOAD_PROGRESS_HANDLE_ERRORS;
				}
			}
			else
			{
				photoDisplayf("CSavegameQueuedOperation_LoadUGCPhoto::Update - REQUEST_TEXTURE_DOWNLOAD - player signed out or cloud not available");

//				m_Status = MEM_CARD_ERROR;
				CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_BEGIN_CLOUD_LOAD_JPEG_FAILED);
				m_UGCPhotoLoadProgress = UGC_PHOTO_LOAD_PROGRESS_HANDLE_ERRORS;
			}
		}
		break;


	case UGC_PHOTO_LOAD_PROGRESS_WAIT_FOR_TEXTURE_DOWNLOAD :
		if (m_TextureDownloadRequestHandle == CTextureDownloadRequest::INVALID_HANDLE)
		{
			photoAssertf(0, "CSavegameQueuedOperation_LoadUGCPhoto::Update - didn't expect TextureDownloadRequestHandle to be invalid inside UGC_PHOTO_LOAD_PROGRESS_WAIT_FOR_TEXTURE_DOWNLOAD");
//			m_Status = MEM_CARD_ERROR;
			CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_CLOUD_LOAD_JPEG_FAILED);
			m_UGCPhotoLoadProgress = UGC_PHOTO_LOAD_PROGRESS_HANDLE_ERRORS;
		}
		else
		{
			// Reset our handle if the request has failed
			if (DOWNLOADABLETEXTUREMGR.HasRequestFailed(m_TextureDownloadRequestHandle))
			{
				photoDisplayf("CSavegameQueuedOperation_LoadUGCPhoto::Update - WAIT_FOR_TEXTURE_DOWNLOAD - request failed");

				DOWNLOADABLETEXTUREMGR.ReleaseTextureDownloadRequest(m_TextureDownloadRequestHandle);
				m_TextureDownloadRequestHandle = CTextureDownloadRequest::INVALID_HANDLE;
//				m_Status = MEM_CARD_ERROR;
				CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_CLOUD_LOAD_JPEG_FAILED);
				m_UGCPhotoLoadProgress = UGC_PHOTO_LOAD_PROGRESS_HANDLE_ERRORS;
			}
			// Check if our request is ready
			else if (DOWNLOADABLETEXTUREMGR.IsRequestReady(m_TextureDownloadRequestHandle))
			{
				photoDisplayf("CSavegameQueuedOperation_LoadUGCPhoto::Update - WAIT_FOR_TEXTURE_DOWNLOAD - request is ready");

					strLocalIndex txdSlotForRequest = DOWNLOADABLETEXTUREMGR.GetRequestTxdSlot(m_TextureDownloadRequestHandle);

					// Add a reference for us before releasing the request
					g_TxdStore.AddRef(txdSlotForRequest, REF_OTHER);

					m_UGCPhotoLoadProgress = UGC_PHOTO_LOAD_PROGRESS_WAIT_FOR_LOADING_MESSAGE_TO_FINISH;
			}
		}
		break;

	case UGC_PHOTO_LOAD_PROGRESS_WAIT_FOR_LOADING_MESSAGE_TO_FINISH :
		if (CSavingMessage::HasLoadingMessageDisplayedForLongEnough())
		{
			// The following is called for savegames. I'm not sure if it's needed for photos / MP stats.
			CSavegameDialogScreens::SetMessageAsProcessing(false);	//	GSWTODO - not sure if this should be in here or in DeSerialize()

			EndLoadBlockCloud();

			m_Status = MEM_CARD_COMPLETE;
		}
		break;


	case UGC_PHOTO_LOAD_PROGRESS_HANDLE_ERRORS :
		{
			CSavingMessage::Clear();

			bool bDisplayingMessageAboutFailingToLoadAThumbnail = false;
			if (!m_IndexWithinMergedPhotoList.GetIsMaximised())
			{	//	if loading a thumbnail photo
				switch (CSavegameDialogScreens::GetMostRecentErrorCode())
				{
				case SAVE_GAME_DIALOG_BEGIN_CLOUD_LOAD_JPEG_FAILED :
				case SAVE_GAME_DIALOG_CHECK_CLOUD_LOAD_JPEG_FAILED :
				case SAVE_GAME_DIALOG_BEGIN_CLOUD_LOAD_JSON_FAILED :
				case SAVE_GAME_DIALOG_CHECK_CLOUD_LOAD_JSON_FAILED :
					bDisplayingMessageAboutFailingToLoadAThumbnail = true;
					break;
				default :
					break;
				}
			}

			if (bDisplayingMessageAboutFailingToLoadAThumbnail && CPhotoManager::GetWarningMessageForGalleryPhotoLoadFailureHasAlreadyBeenSeen())
			{
				CSavegameDialogScreens::ClearSaveGameError();
				m_Status = MEM_CARD_ERROR;
			}
			else
			{
				if (CSavegameDialogScreens::HandleSaveGameErrorCode())
				{
					if (bDisplayingMessageAboutFailingToLoadAThumbnail)
					{
						CPhotoManager::SetWarningMessageForGalleryPhotoLoadFailureHasAlreadyBeenSeen(true);
					}
					m_Status = MEM_CARD_ERROR;
				}
			}
		}
		break;
	}


	if ( (m_Status == MEM_CARD_ERROR) || (m_Status == MEM_CARD_COMPLETE) )
	{
		if (m_Status == MEM_CARD_ERROR)
		{
			//		savegameWarningf("Error in CalculateSizeOfRequiredBuffer: code=%d", CSavegameDialogScreens::GetMostRecentErrorCode());
			photoDisplayf("CSavegameQueuedOperation_LoadUGCPhoto::Update - failed");

			CSavingMessage::Clear();

			EndLoadBlockCloud();

			// The following is called for savegames. I'm not sure if it's needed for photos / MP stats.
			CSavegameDialogScreens::SetMessageAsProcessing(false);

			CPhotoManager::SetTextureDownloadFailed(m_IndexWithinMergedPhotoList);
		}
		else if (m_Status == MEM_CARD_COMPLETE)
		{
			photoDisplayf("CSavegameQueuedOperation_LoadUGCPhoto::Update - succeeded");

			CPhotoManager::SetTextureDownloadSucceeded(m_IndexWithinMergedPhotoList);
		}

		if (m_TextureDownloadRequestHandle != CTextureDownloadRequest::INVALID_HANDLE)
		{
			// We have the txd slot, we can now release our request
			DOWNLOADABLETEXTUREMGR.ReleaseTextureDownloadRequest(m_TextureDownloadRequestHandle);
			m_TextureDownloadRequestHandle = CTextureDownloadRequest::INVALID_HANDLE;
		}

// 		if (m_pMetadataString)
// 		{
// 			delete [] m_pMetadataString;
// 			m_pMetadataString = NULL;
// 		}

		//	Temporary function until the proper code to create a photo buffer is ready to use
		//		FreePhotoBuffer();
		return true;
	}

	return false;
}
#endif	//	!__LOAD_LOCAL_PHOTOS


void CSavegameQueuedOperation_PhotoSave::Init(u8 *pJpegBuffer, u32 exactSizeOfJpegBuffer, CSavegamePhotoMetadata const &metadataToSave, const char* pTitle, const char* pDescription)
{
	m_pJpegBuffer = pJpegBuffer;
	m_ExactSizeOfJpegBuffer = exactSizeOfJpegBuffer;

//	safecpy(m_DisplayName, pTitle, NELEM(m_DisplayName));
//	safecpy(m_Description, pDescription, NELEM(m_Description));
	m_DisplayName = pTitle;
	m_Description = pDescription;
	m_MetadataToSave = metadataToSave;

	m_pMetadataStringForUploadingToUGC = NULL;
	
	m_PhotoSaveProgress = PHOTO_SAVE_PROGRESS_CREATE_METADATA_STRING;
}


//    Return true to Pop() from queue
bool CSavegameQueuedOperation_PhotoSave::Update()
{
	switch (m_PhotoSaveProgress)
	{
		case PHOTO_SAVE_PROGRESS_CREATE_METADATA_STRING :
			{
				CSavegameDialogScreens::SetDialogBoxHeading("SG_HDNG");	//	SG_HDG_S_PH_CL

				m_Status = MEM_CARD_ERROR;

				m_pMetadataStringForUploadingToUGC = rage_new char[CPhotoBuffer::GetDefaultSizeOfJsonBuffer()];
				if (photoVerifyf(m_pMetadataStringForUploadingToUGC, "CSavegameQueuedOperation_PhotoSave::Update - PHOTO_SAVE_PROGRESS_CREATE_METADATA_STRING - failed to allocate memory for metadata string"))
				{
					if (photoVerifyf(m_MetadataToSave.WriteMetadataToString(m_pMetadataStringForUploadingToUGC, CPhotoBuffer::GetDefaultSizeOfJsonBuffer(), true), "CSavegameQueuedOperation_PhotoSave::Update - PHOTO_SAVE_PROGRESS_CREATE_METADATA_STRING - WriteMetadataToString() failed"))
					{
						photoDisplayf("CSavegameQueuedOperation_PhotoSave::Update - PHOTO_SAVE_PROGRESS_CREATE_METADATA_STRING - WriteMetadataToString() succeeded");

						m_Status = MEM_CARD_BUSY;
						m_PhotoSaveProgress = PHOTO_SAVE_PROGRESS_BEGIN_CREATE_UGC;
					}
				}
			}
			break;

//	Photos are no longer saved to member space first. They now go straight to UGC
/*
		case PHOTO_SAVE_PROGRESS_BEGIN_CLOUD_SAVE :
			{
				MemoryCardError resultOfBeginCall = CSavegameSaveBlock::BeginCloudPhotoSave(m_pJpegBuffer, m_ExactSizeOfJpegBuffer);
			
				switch (resultOfBeginCall)
				{
				case MEM_CARD_COMPLETE :
					photoDisplayf("CSavegameQueuedOperation_PhotoSave::Update - BEGIN_CLOUD_SAVE - BeginSaveBlock succeeded");
					m_PhotoSaveProgress = PHOTO_SAVE_PROGRESS_CHECK_CLOUD_SAVE;
					break;

				case MEM_CARD_BUSY :
					photoAssertf(0, "CSavegameQueuedOperation_PhotoSave::Update - BEGIN_CLOUD_SAVE - didn't expect CSavegameSaveBlock::BeginSaveBlock() to ever return MEM_CARD_BUSY");
					m_Status = MEM_CARD_ERROR;
					break;
				case MEM_CARD_ERROR :
					photoAssertf(0, "CSavegameQueuedOperation_PhotoSave::Update - BEGIN_CLOUD_SAVE - CSavegameSaveBlock::BeginSaveBlock() returned MEM_CARD_ERROR for cloud save");
					m_Status = MEM_CARD_ERROR;
					break;
				}
			}
			break;

		case PHOTO_SAVE_PROGRESS_CHECK_CLOUD_SAVE :
			switch (CSavegameSaveBlock::SaveBlock(true))
			{
			case MEM_CARD_COMPLETE :
				photoDisplayf("CSavegameQueuedOperation_PhotoSave::Update - CHECK_CLOUD_SAVE - CSavegameSaveBlock succeeded");
				m_PhotoSaveProgress = PHOTO_SAVE_PROGRESS_BEGIN_CREATE_UGC;
				break;

			case MEM_CARD_BUSY :    // the save is still in progress
				break;

			case MEM_CARD_ERROR :
				photoDisplayf("CSavegameQueuedOperation_PhotoSave::Update - CHECK_CLOUD_SAVE - CSavegameSaveBlock failed");
				m_Status = MEM_CARD_ERROR;
				break;
			}
			break;
*/

		case PHOTO_SAVE_PROGRESS_BEGIN_CREATE_UGC :
			{
				if (photoVerifyf(!UserContentManager::GetInstance().IsCreatePending(), "CSavegameQueuedOperation_PhotoSave::Update - BEGIN_CREATE_UGC - a Create is already pending"))
				{
//					to get the title and description, I could declare an instance of the metadata class and call
//						bool CSavegamePhotoMetadata::ReadMetadataFromString(const char *pString)

                    rlUgc::SourceFile srcFile(0, m_pJpegBuffer, m_ExactSizeOfJpegBuffer);
                    
					const bool bPublish = true;	//	John Hynd says that this means "share so that others can see your photos"
					photoDisplayf("CSavegameQueuedOperation_PhotoSave::Update - BEGIN_CREATE_UGC - Title is %s, sizeOfJpegBuffer=%u, metadata string is %s", m_DisplayName.c_str(), m_ExactSizeOfJpegBuffer, m_pMetadataStringForUploadingToUGC);
					
                    if (UserContentManager::GetInstance().Create(m_DisplayName, 
                                                                 m_Description, 
                                                                 &srcFile,
                                                                 1,
                                                                 NULL, //tags
                                                                 m_pMetadataStringForUploadingToUGC, 
                                                                 RLUGC_CONTENT_TYPE_GTA5PHOTO, 
                                                                 bPublish))
					{
						photoDisplayf("CSavegameQueuedOperation_PhotoSave::Update - BEGIN_CREATE_UGC - UGC creation was successfully started");
						m_PhotoSaveProgress = PHOTO_SAVE_PROGRESS_CHECK_CREATE_UGC;
					}
					else
					{
						photoDisplayf("CSavegameQueuedOperation_PhotoSave::Update - BEGIN_CREATE_UGC - UGC creation failed to start");
						CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_BEGIN_UGC_PHONE_PHOTO_SAVE_FAILED);
						m_PhotoSaveProgress = PHOTO_SAVE_PROGRESS_DISPLAY_UGC_ERROR;
					}
				}
			}
			break;

		case PHOTO_SAVE_PROGRESS_CHECK_CREATE_UGC :
			if (UserContentManager::GetInstance().HasCreateFinished())
			{
				if (UserContentManager::GetInstance().DidCreateSucceed())
				{
					photoDisplayf("CSavegameQueuedOperation_PhotoSave::Update - CHECK_CREATE_UGC - UGC creation has completed successfully");
					m_Status = MEM_CARD_COMPLETE;
				}
				else
				{
					photoDisplayf("CSavegameQueuedOperation_PhotoSave::Update - CHECK_CREATE_UGC - UGC creation has failed");
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_UGC_PHONE_PHOTO_SAVE_FAILED);
					m_PhotoSaveProgress = PHOTO_SAVE_PROGRESS_DISPLAY_UGC_ERROR;
				}
			}
			break;

		case PHOTO_SAVE_PROGRESS_DISPLAY_UGC_ERROR :
			if (CSavegameDialogScreens::HandleSaveGameErrorCode())
			{
				m_Status = MEM_CARD_ERROR;
			}
			break;
	}

	if ( (m_Status == MEM_CARD_ERROR) || (m_Status == MEM_CARD_COMPLETE) )
	{
		if (m_pMetadataStringForUploadingToUGC)
		{
			delete [] m_pMetadataStringForUploadingToUGC;
			m_pMetadataStringForUploadingToUGC = NULL;
		}

//		CHighQualityScreenshot::ResetState();
//		FreePhotoBuffer();
		return true;
	}

	return false;
}

bool CSavegameQueuedOperation_PhotoSave::Shutdown()
{
//	I'm not sure what to do here. If I've started uploading a photo, do I just let it finish by itself?
	return false;
}

// End of CSavegameQueuedOperation_PhotoSave *********************************************************************************************


// CSavegameQueuedOperation_SavePhotoForMissionCreator *********************************************************************************************


void CSavegameQueuedOperation_SavePhotoForMissionCreator::Init(const char *pFilename, CPhotoBuffer *pPhotoBuffer)
{
	m_pPhotoBuffer = pPhotoBuffer;

	savegameAssertf(strlen(pFilename) < MAX_LENGTH_OF_PATH_TO_MISSION_CREATOR_PHOTO, "CSavegameQueuedOperation_SavePhotoForMissionCreator::Init - filename %s is too long. MAX_LENGTH_OF_PATH_TO_MISSION_CREATOR_PHOTO is %d. Increase it", pFilename, MAX_LENGTH_OF_PATH_TO_MISSION_CREATOR_PHOTO);
	safecpy(m_Filename, pFilename, MAX_LENGTH_OF_PATH_TO_MISSION_CREATOR_PHOTO);

	m_PhotoSaveProgress = MISSION_CREATOR_PHOTO_SAVE_PROGRESS_BEGIN_CLOUD_SAVE;

}

//    Return true to Pop() from queue
bool CSavegameQueuedOperation_SavePhotoForMissionCreator::Update()
{
	switch (m_PhotoSaveProgress)
	{
	case MISSION_CREATOR_PHOTO_SAVE_PROGRESS_BEGIN_CLOUD_SAVE :
		{
			CSavegameDialogScreens::SetDialogBoxHeading("SG_HDNG");	//	SG_HDG_S_PH_CL	//	I could have a different message here to differentiate it from the gallery photos

			MemoryCardError resultOfBeginCall = MEM_CARD_ERROR;

			resultOfBeginCall = CSavegameSaveBlock::BeginSaveOfMissionCreatorPhoto(m_Filename, m_pPhotoBuffer->GetJpegBuffer(), m_pPhotoBuffer->GetExactSizeOfJpegData());

			switch (resultOfBeginCall)
			{
			case MEM_CARD_COMPLETE :
				photoDisplayf("CSavegameQueuedOperation_SavePhotoForMissionCreator::Update - BEGIN_CLOUD_SAVE - BeginSaveOfMissionCreatorPhoto succeeded");
				m_PhotoSaveProgress = MISSION_CREATOR_PHOTO_SAVE_PROGRESS_CHECK_CLOUD_SAVE;
				break;

			case MEM_CARD_BUSY :
				photoAssertf(0, "CSavegameQueuedOperation_SavePhotoForMissionCreator::Update - BEGIN_CLOUD_SAVE - didn't expect CSavegameSaveBlock::BeginSaveOfMissionCreatorPhoto() to ever return MEM_CARD_BUSY");
				m_Status = MEM_CARD_ERROR;
				break;
			case MEM_CARD_ERROR :
				photoAssertf(0, "CSavegameQueuedOperation_SavePhotoForMissionCreator::Update - BEGIN_CLOUD_SAVE - CSavegameSaveBlock::BeginSaveOfMissionCreatorPhoto() returned MEM_CARD_ERROR");
				m_Status = MEM_CARD_ERROR;
				break;
			}
		}
		break;

	case MISSION_CREATOR_PHOTO_SAVE_PROGRESS_CHECK_CLOUD_SAVE :
		switch (CSavegameSaveBlock::SaveBlock(true))
		{
		case MEM_CARD_COMPLETE :
			photoDisplayf("CSavegameQueuedOperation_SavePhotoForMissionCreator::Update - CHECK_CLOUD_SAVE - CSavegameSaveBlock succeeded");
			m_Status = MEM_CARD_COMPLETE;
			break;

		case MEM_CARD_BUSY :    // the save is still in progress
			break;

		case MEM_CARD_ERROR :
			photoDisplayf("CSavegameQueuedOperation_SavePhotoForMissionCreator::Update - CHECK_CLOUD_SAVE - CSavegameSaveBlock failed");
			m_Status = MEM_CARD_ERROR;
			break;
		}
		break;
	}

	if ( (m_Status == MEM_CARD_ERROR) || (m_Status == MEM_CARD_COMPLETE) )
	{
		//		CHighQualityScreenshot::ResetState();
		//		FreePhotoBuffer();
		return true;
	}

	return false;
}

// End of CSavegameQueuedOperation_SavePhotoForMissionCreator *********************************************************************************************

// CSavegameQueuedOperation_LoadPhotoForMissionCreator *********************************************************************************************

void CSavegameQueuedOperation_LoadPhotoForMissionCreator::Init(const sRequestData &requestData, CPhotoBuffer *pPhotoBuffer, u32 maxSizeOfBuffer)
{
	m_pPhotoBuffer = pPhotoBuffer;
	m_MaxSizeOfBuffer = maxSizeOfBuffer;
    m_RequestData = requestData;

	m_PhotoLoadProgress = MISSION_CREATOR_PHOTO_LOAD_PROGRESS_BEGIN_CLOUD_LOAD;
}

//    Return true to Pop() from queue
bool CSavegameQueuedOperation_LoadPhotoForMissionCreator::Update()
{
	switch (m_PhotoLoadProgress)
	{
	case MISSION_CREATOR_PHOTO_LOAD_PROGRESS_BEGIN_CLOUD_LOAD :
		{
			CSavegameDialogScreens::SetDialogBoxHeading("SG_HDNG");	//	SG_HDG_L_PH_CL	//	I could have a different message here to differentiate it from the gallery photos

			MemoryCardError resultOfBeginCall = MEM_CARD_ERROR;

			resultOfBeginCall = CSavegameLoadJpegFromCloud::BeginLoadJpegFromCloud(m_RequestData, m_pPhotoBuffer, m_MaxSizeOfBuffer, RL_CLOUD_NAMESPACE_UGC, false);

			switch (resultOfBeginCall)
			{
			case MEM_CARD_COMPLETE :
				photoDisplayf("CSavegameQueuedOperation_LoadPhotoForMissionCreator::Update - BEGIN_CLOUD_LOAD - CSavegameLoadJpegFromCloud::BeginLoadJpegFromCloud() succeeded");
				m_PhotoLoadProgress = MISSION_CREATOR_PHOTO_LOAD_PROGRESS_CHECK_CLOUD_LOAD;
				break;

			case MEM_CARD_BUSY :
				photoAssertf(0, "CSavegameQueuedOperation_LoadPhotoForMissionCreator::Update - BEGIN_CLOUD_LOAD - didn't expect CSavegameLoadJpegFromCloud::BeginLoadJpegFromCloud() to ever return MEM_CARD_BUSY");
				m_Status = MEM_CARD_ERROR;
				break;
			case MEM_CARD_ERROR :
				photoAssertf(0, "CSavegameQueuedOperation_LoadPhotoForMissionCreator::Update - BEGIN_CLOUD_LOAD - CSavegameLoadJpegFromCloud::BeginLoadJpegFromCloud() returned MEM_CARD_ERROR");
				m_Status = MEM_CARD_ERROR;
				break;
			}
		}
		break;

	case MISSION_CREATOR_PHOTO_LOAD_PROGRESS_CHECK_CLOUD_LOAD :
		switch (CSavegameLoadJpegFromCloud::LoadJpegFromCloud())
		{
		case MEM_CARD_COMPLETE :
			photoDisplayf("CSavegameQueuedOperation_LoadPhotoForMissionCreator::Update - CHECK_CLOUD_LOAD - CSavegameLoadJpegFromCloud::LoadJpegFromCloud() succeeded");
			m_Status = MEM_CARD_COMPLETE;
			break;

		case MEM_CARD_BUSY :    // the save is still in progress
			break;

		case MEM_CARD_ERROR :
			photoDisplayf("CSavegameQueuedOperation_LoadPhotoForMissionCreator::Update - CHECK_CLOUD_LOAD - CSavegameLoadJpegFromCloud::LoadJpegFromCloud() failed");
			m_Status = MEM_CARD_ERROR;
			break;
		}
		break;
	}

	if ( (m_Status == MEM_CARD_ERROR) || (m_Status == MEM_CARD_COMPLETE) )
	{
		//		CHighQualityScreenshot::ResetState();
		//		FreePhotoBuffer();
		return true;
	}

	return false;
}


// End of Photos *********************************************************************************************

// MP Stats *******************************************************************************************

void CSavegameQueuedOperation_MPStats_Load::Init(const u32 saveCategory)
{
	m_IndexOfMpSave = saveCategory;
#if __ALLOW_LOCAL_MP_STATS_SAVES
	m_IndexOfSavegameFile = saveCategory + CSavegameList::GetBaseIndexForSavegameFileType(SG_FILE_TYPE_MULTIPLAYER_STATS);
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES

	if (saveCategory == 0)
	{
		m_SavegameType = SAVEGAME_MULTIPLAYER_COMMON;
	}
	else
	{
		m_SavegameType = SAVEGAME_MULTIPLAYER_CHARACTER;
	}

	m_OpProgress = MP_STATS_LOAD_PROGRESS_BEGIN_CLOUD_LOAD;
}

bool CSavegameQueuedOperation_MPStats_Load::Shutdown()
{
	m_OpProgress = MP_STATS_LOAD_PROGRESS_CLOUD_FINISH;

//	CGenericGameStorage::SetCloudSaveResultcode( HTTP_CODE_CLIENTCLOSEDREQUEST );
	CGenericGameStorage::SetReasonForFailureToLoadSavegame(LOAD_FAILED_REASON_FAILED_TO_LOAD);

	m_Status = MEM_CARD_ERROR;

	CSavingMessage::Clear();

	m_Cloud.Shutdown();

	CGenericGameStorage::SetSaveOperation(OPERATION_NONE);

	return true;
}

void CSavegameQueuedOperation_MPStats_Load::Cancel()
{
	savegameAssertf(MEM_CARD_BUSY == m_Status, "We are not currently loading the mp savegame.");
	if (MEM_CARD_BUSY == m_Status)
	{
		m_OpProgress = MP_STATS_LOAD_PROGRESS_CLOUD_FINISH;

		//	CGenericGameStorage::SetCloudSaveResultcode( HTTP_CODE_CLIENTCLOSEDREQUEST );
		CGenericGameStorage::SetReasonForFailureToLoadSavegame(LOAD_FAILED_REASON_FAILED_TO_LOAD);

		m_Status = MEM_CARD_ERROR;

		CSavingMessage::Clear();

		m_Cloud.Shutdown();

		CGenericGameStorage::SetSaveOperation(OPERATION_NONE);
	}
}


#if __BANK
void OutputContentsOfCloudFile(void *pBuffer, u32 BufferLength)
{
	if(PARAM_outputCloudFiles.Get())
	{
		const fiDevice* device = fiDevice::GetDevice("cloudsave0.bak", false);

		int newFileIndex = 0;
		if (device)
		{
			u64 oldestTime = (u64)-1;

			const int numberOfBackups = 6;
			for(int i = 0; i < numberOfBackups; i++)
			{
				char newFileName[RAGE_MAX_PATH];
				formatf(newFileName, "cloudsave%d.bak", i);

				if (!ASSET.Exists(newFileName, ""))
				{
					newFileIndex = i;
					break;
				}
				else 
				{
					char fullNewFileName[RAGE_MAX_PATH];
					ASSET.FullWritePath(fullNewFileName, RAGE_MAX_PATH, newFileName, "");
					u64 fileTime = device->GetFileTime(fullNewFileName);

					if (fileTime < oldestTime)
					{
						oldestTime = fileTime;
						newFileIndex = i;
					}
				}

			}
		}

		char newFileName[RAGE_MAX_PATH];
		formatf(newFileName, "cloudsave%d", newFileIndex);

		fiStream* pStream(ASSET.Create(newFileName, "bak"));
		if (pStream)
		{
			pStream->Write(pBuffer, BufferLength);
			pStream->Close();
		}
	}
}
#endif	//	__BANK

#if __ALLOW_LOCAL_MP_STATS_SAVES
bool CSavegameQueuedOperation_MPStats_Load::ReadPosixTimeFromCloudFile(u64 &timeStamp)
{
	CGenericGameStorage::SetSaveOperation(OPERATION_LOADING_MPSTATS_FROM_CLOUD);
	//Setup Pointers to buffers and size
	CGenericGameStorage::SetBuffer((u8*)m_Cloud.GetBuffer(), m_Cloud.GetBufferLength(), m_SavegameType);
	CGenericGameStorage::SetOnlyReadTimestampFromMPSave(true);

	bool returnValue = CGenericGameStorage::LoadBlockData(m_SavegameType, StatsInterface::CanUseEncrytionInMpSave());

	CGenericGameStorage::SetOnlyReadTimestampFromMPSave(false);
	//Clear Pointers to buffers and size
	CGenericGameStorage::SetBuffer(NULL, 0, m_SavegameType);
	CGenericGameStorage::SetSaveOperation(OPERATION_NONE);

	if (returnValue)
	{
		CGenericGameStorage::SetReasonForFailureToLoadSavegame(LOAD_FAILED_REASON_NONE);
		timeStamp = CGenericGameStorage::GetTimestampReadFromMpSave();
		savegameDisplayf("CSavegameQueuedOperation_MPStats_Load::ReadPosixTimeFromCloudFile - timestamp of cloud file is %lu", timeStamp);
	}
	else
	{
		CGenericGameStorage::SetReasonForFailureToLoadSavegame(LOAD_FAILED_REASON_FAILED_TO_LOAD);
		timeStamp = 0;
		savegameDisplayf("CSavegameQueuedOperation_MPStats_Load::ReadPosixTimeFromCloudFile - LoadBlockData failed when reading time stamp from cloud file");		
	}

	return returnValue;
}

void CSavegameQueuedOperation_MPStats_Load::ReadPosixTimeFromLocalFile(u64 &timeStamp)
{
	timeStamp = 0;
	if (m_bLocalFileHasLoaded)
	{
		CGenericGameStorage::SetSaveOperation(OPERATION_LOADING_MPSTATS_SAVEGAME_FROM_CONSOLE);
		CGenericGameStorage::SetOnlyReadTimestampFromMPSave(true);
		CGenericGameStorage::AllowReasonForFailureToBeSet(false);

		if (CGenericGameStorage::LoadBlockData(m_SavegameType))
		{
			timeStamp = CGenericGameStorage::GetTimestampReadFromMpSave();
			savegameDisplayf("CSavegameQueuedOperation_MPStats_Load::ReadPosixTimeFromLocalFile - timestamp of local file is %lu", timeStamp);
		}
		else
		{
			savegameDisplayf("CSavegameQueuedOperation_MPStats_Load::ReadPosixTimeFromLocalFile - LoadBlockData failed when reading time stamp from local file");
		}

		CGenericGameStorage::AllowReasonForFailureToBeSet(true);
		CGenericGameStorage::SetOnlyReadTimestampFromMPSave(false);
		CGenericGameStorage::SetSaveOperation(OPERATION_NONE);
	}
}

void CSavegameQueuedOperation_MPStats_Load::ReadFullDataFromLocalFile()
{
	CGenericGameStorage::SetSaveOperation(OPERATION_LOADING_MPSTATS_SAVEGAME_FROM_CONSOLE);
	CGenericGameStorage::AllowReasonForFailureToBeSet(false);

	//	Pass false for useEncryption. If the buffer needed to be decrypted then it would have been done when reading the timestamp above.
	if (CGenericGameStorage::LoadBlockData(m_SavegameType, false))
	{
		savegameDisplayf("CSavegameQueuedOperation_MPStats_Load::ReadFullDataFromLocalFile - LoadBlockData succeeded when reading data from local file");
	}
	else
	{
		m_bFailedToReadFromLocalFile = true;
		savegameDisplayf("CSavegameQueuedOperation_MPStats_Load::ReadFullDataFromLocalFile - LoadBlockData failed when reading data from local file");
	}

	CGenericGameStorage::AllowReasonForFailureToBeSet(true);
	CGenericGameStorage::SetSaveOperation(OPERATION_NONE);
}
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES


void CSavegameQueuedOperation_MPStats_Load::ReadFullDataFromCloudFile()
{
	m_bFailedToReadFromCloudFile = false;
	CGenericGameStorage::SetSaveOperation(OPERATION_LOADING_MPSTATS_FROM_CLOUD);
	//Setup Pointers to buffers and size
	CGenericGameStorage::SetBuffer((u8*)m_Cloud.GetBuffer(), m_Cloud.GetBufferLength(), m_SavegameType);

	bool bDecryptBuffer = StatsInterface::CanUseEncrytionInMpSave();
#if __ALLOW_LOCAL_MP_STATS_SAVES
	bDecryptBuffer = false;	//	Pass false for useEncryption. If the buffer needed to be decrypted then it would have been done when reading the timestamp above.
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES

	//Default error is Failure to load
	CGenericGameStorage::SetReasonForFailureToLoadSavegame(LOAD_FAILED_REASON_FAILED_TO_LOAD);

	if (CGenericGameStorage::LoadBlockData(m_SavegameType, bDecryptBuffer))
	{
		CGenericGameStorage::SetReasonForFailureToLoadSavegame(LOAD_FAILED_REASON_NONE);
		savegameDisplayf("CSavegameQueuedOperation_MPStats_Load::ReadFullDataFromCloudFile - LoadBlockData succeeded when reading data from cloud file");
	}
	else
	{
		m_bFailedToReadFromCloudFile = true;
		savegameAssertf(0, "LoadBlockData failed when reading data from cloud file '%s'", CSavegameFilenames::GetFilenameOfCloudFile());
		savegameErrorf("CSavegameQueuedOperation_MPStats_Load::ReadFullDataFromCloudFile - LoadBlockData failed when reading data from cloud file");
	}

	//Clear Pointers to buffers and size
	CGenericGameStorage::SetBuffer(NULL, 0, m_SavegameType);
	CGenericGameStorage::SetSaveOperation(OPERATION_NONE);
}


// If an error occurs when loading or reading the cloud file then return an error (even if the local file is read successfully)
//    Return true to Pop() from queue
bool CSavegameQueuedOperation_MPStats_Load::Update()
{
	switch (m_OpProgress)
	{
		case MP_STATS_LOAD_PROGRESS_BEGIN_CLOUD_LOAD :
			{
				CSavegameDialogScreens::SetDialogBoxHeading("SG_HDNG");	//	SG_HDG_LOAD_MP

				CGenericGameStorage::SetReasonForFailureToLoadSavegame(LOAD_FAILED_REASON_FAILED_TO_LOAD);
				m_OpProgress = MP_STATS_LOAD_PROGRESS_FREE_BOTH_BUFFERS;

#if __ALLOW_LOCAL_MP_STATS_SAVES
				m_bLocalFileExists = false;
				m_bLocalFileIsDamaged = false;

				m_bLocalFileHasLoaded = false;
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES

				m_bCloudFileHasLoaded = false;

				if (savegameVerifyf(CGenericGameStorage::GetSaveOperation() == OPERATION_NONE, "CSavegameQueuedOperation_MPStats_Load::Update - MP_STATS_LOAD_PROGRESS_BEGIN_LOAD - operation is not OPERATION_NONE"))
				{
					// this was getting called when the player had signed in, then signed out without closing the sign-in UI in between.
					if(NetworkInterface::GetActiveGamerInfo() && NetworkInterface::IsCloudAvailable())
					{
						CSavegameUsers::SetUniqueIDOfGamerWhoStartedTheLoad(NetworkInterface::GetActiveGamerInfo()->GetGamerId());
						CGenericGameStorage::SetSaveOperation(OPERATION_LOADING_MPSTATS_FROM_CLOUD);

						CSavegameFilenames::MakeValidSaveNameForMpStatsCloudFile(m_IndexOfMpSave);

						CSavingMessage::BeginDisplaying(CSavingMessage::STORAGE_DEVICE_MESSAGE_CLOUD_LOADING);

						if (m_Cloud.RequestFile(CSavegameFilenames::GetFilenameOfCloudFile(), NetworkInterface::GetLocalGamerIndex(), rlCloudMemberId(), StatsInterface::GetCloudSavegameService()))
						{
							CGenericGameStorage::SetReasonForFailureToLoadSavegame(LOAD_FAILED_REASON_NONE);
							m_OpProgress = MP_STATS_LOAD_PROGRESS_CHECK_CLOUD_LOAD;

#if __ALLOW_LOCAL_MP_STATS_SAVES
#if !__FINAL
							if (PARAM_enableLocalMPSaveCache.Get())
#endif
							{
								m_OpProgress = MP_STATS_LOAD_PROGRESS_BEGIN_CHECK_LOCAL_FILE_EXISTS;
							}
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES

							m_Status = MEM_CARD_BUSY;	//	Graeme - I think m_Status will already be MEM_CARD_BUSY at this stage
						}
					}
				}
			}
			break;

#if __ALLOW_LOCAL_MP_STATS_SAVES
		case MP_STATS_LOAD_PROGRESS_BEGIN_CHECK_LOCAL_FILE_EXISTS :
			CGenericGameStorage::SetSaveOperation(OPERATION_NONE);
			switch (CSavegameCheckFileExists::BeginCheckFileExists(m_IndexOfSavegameFile))
			{
			case MEM_CARD_COMPLETE :
				m_OpProgress = MP_STATS_LOAD_PROGRESS_CHECK_LOCAL_FILE_EXISTS;
				break;
			case MEM_CARD_BUSY :
				savegameAssertf(0, "CSavegameQueuedOperation_MPStats_Load::Update - didn't expect CSavegameCheckFileExists::BeginCheckFileExists() to ever return MEM_CARD_BUSY");
				m_OpProgress = MP_STATS_LOAD_PROGRESS_CHECK_CLOUD_LOAD;
				break;
			case MEM_CARD_ERROR :
				savegameAssertf(0, "CSavegameQueuedOperation_MPStats_Load::Update - CSavegameCheckFileExists::BeginCheckFileExists() returned MEM_CARD_ERROR");
				m_OpProgress = MP_STATS_LOAD_PROGRESS_CHECK_CLOUD_LOAD;
				break;
			}
			break;

		case MP_STATS_LOAD_PROGRESS_CHECK_LOCAL_FILE_EXISTS :
			switch (CSavegameCheckFileExists::CheckFileExists())
			{
				case MEM_CARD_COMPLETE :
					CSavegameCheckFileExists::GetFileStatus(m_bLocalFileExists, m_bLocalFileIsDamaged);
					savegameDebugf1("CSavegameQueuedOperation_MPStats_Load::Update - completed for local slot %d. Time = %d. Slot %s. Slot is %sdamaged\n", m_IndexOfSavegameFile, sysTimer::GetSystemMsTime(), m_bLocalFileExists?"exists":"doesn't exist", m_bLocalFileIsDamaged?"":"not ");
					
					if (m_bLocalFileExists && !m_bLocalFileIsDamaged)
					{
						m_OpProgress = MP_STATS_LOAD_PROGRESS_BEGIN_LOCAL_LOAD;
					}
					else
					{
						m_OpProgress = MP_STATS_LOAD_PROGRESS_CHECK_CLOUD_LOAD;
					}
					break;

				case MEM_CARD_BUSY :    // the load is still in progress
					break;

				case MEM_CARD_ERROR :
					m_bLocalFileExists = false;
					m_bLocalFileIsDamaged = false;
					savegameDebugf1("CSavegameQueuedOperation_MPStats_Load::Update - failed for slot %d. Time = %d. Slot %s. Slot is %sdamaged\n", m_IndexOfSavegameFile, sysTimer::GetSystemMsTime(), m_bLocalFileExists?"exists":"doesn't exist", m_bLocalFileIsDamaged?"":"not ");
					m_OpProgress = MP_STATS_LOAD_PROGRESS_CHECK_CLOUD_LOAD;
					break;
			}
			break;

		case MP_STATS_LOAD_PROGRESS_BEGIN_LOCAL_LOAD :
			if (savegameVerifyf(CSavegameLoad::GetLoadStatus() == GENERIC_LOAD_DO_NOTHING, "CSavegameQueuedOperation_MPStats_Load::Update - expected LoadStatus to be GENERIC_LOAD_DO_NOTHING"))
			{
				CSavegameFilenames::MakeValidSaveNameForLocalFile(m_IndexOfSavegameFile);
				CSavegameLoad::BeginLoad(m_SavegameType);

				m_OpProgress = MP_STATS_LOAD_PROGRESS_CHECK_LOCAL_LOAD;
			}
			else
			{
				m_OpProgress = MP_STATS_LOAD_PROGRESS_CHECK_CLOUD_LOAD;
			}
			break;

		case MP_STATS_LOAD_PROGRESS_CHECK_LOCAL_LOAD :
			{
				switch (CSavegameLoad::GenericLoad())
				{
					case MEM_CARD_COMPLETE:
						m_bLocalFileHasLoaded = true;
						m_OpProgress = MP_STATS_LOAD_PROGRESS_CHECK_CLOUD_LOAD;
						break;
					case MEM_CARD_BUSY: // the load is still in progress
						break;
					case MEM_CARD_ERROR:
						m_bLocalFileHasLoaded = false;
						m_OpProgress = MP_STATS_LOAD_PROGRESS_CHECK_CLOUD_LOAD;
						break;
				}
			}
			break;
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES

		case MP_STATS_LOAD_PROGRESS_CHECK_CLOUD_LOAD :
			{
				CGenericGameStorage::SetSaveOperation(OPERATION_NONE);
				CGenericGameStorage::SetSaveOperation(OPERATION_LOADING_MPSTATS_FROM_CLOUD);
				if (!m_Cloud.Pending())
				{
					m_bCloudFileHasLoaded = false;
					m_OpProgress = MP_STATS_LOAD_PROGRESS_FREE_BOTH_BUFFERS;

					m_Cloud.CheckBufferSize();

					if (m_Cloud.Succeeded())
					{
#if __BANK
						OutputContentsOfCloudFile(m_Cloud.GetBuffer(), m_Cloud.GetBufferLength());
#endif // __BANK

						savegameAssertf(m_Cloud.GetBuffer(), "CSavegameQueuedOperation_MPStats_Load::Update - Buffer from cloud load is empty.");
						if (m_Cloud.GetBuffer())
						{
							m_bCloudFileHasLoaded = true;
							m_OpProgress = MP_STATS_LOAD_PROGRESS_DESERIALIZE_FROM_MOST_RECENT_FILE;
						}
					}
					else
					{
						savegameWarningf("CSavegameQueuedOperation_MPStats_Load::Update - Cloud load failed");

						const int resultCode = m_Cloud.GetResultCode(); //See http result codes

						CGenericGameStorage::SetCloudSaveResultcode( resultCode );
						CGenericGameStorage::SetReasonForFailureToLoadSavegame(LOAD_FAILED_REASON_FAILED_TO_LOAD);

						if (resultCode == HTTP_CODE_FILENOTFOUND) // File Not Found
						{
							CGenericGameStorage::SetReasonForFailureToLoadSavegame(LOAD_FAILED_REASON_FILE_NOT_FOUND);
						}
						else if (resultCode == HTTP_CODE_REQUESTTIMEOUT) // Request has timed out
						{
							CGenericGameStorage::SetReasonForFailureToLoadSavegame(LOAD_FAILED_REASON_SERVER_TIMEOUT);
						}
						else
						{
							if (resultCode >= HTTP_CODE_ANYSERVERERROR)
							{
								CGenericGameStorage::SetReasonForFailureToLoadSavegame(LOAD_FAILED_REASON_SERVER_ERROR);
							}

							savegameAssertf(0, "CSavegameQueuedOperation_MPStats_Load::Update - Cloud LOAD failed - http result code = %d", resultCode);
						}
					}

					CGenericGameStorage::SetSaveOperation(OPERATION_NONE);
				}
			}
			break;

		case MP_STATS_LOAD_PROGRESS_DESERIALIZE_FROM_MOST_RECENT_FILE :
			{
				if (savegameVerifyf(m_bCloudFileHasLoaded, "CSavegameQueuedOperation_MPStats_Load::Update - didn't expect to reach MP_STATS_LOAD_PROGRESS_DESERIALIZE_FROM_MOST_RECENT_FILE if the cloud load failed"))
				{
#if __ALLOW_LOCAL_MP_STATS_SAVES
					u64 timeStampOfCloudSave = 0;
					u64 timeStampOfLocalSave = 0;
			
					m_bFailedToReadFromCloudFile = false;
					m_bFailedToReadFromLocalFile = false;

					if (!ReadPosixTimeFromCloudFile(timeStampOfCloudSave))
					{
						m_bFailedToReadFromCloudFile = true;
						m_OpProgress = MP_STATS_LOAD_PROGRESS_FREE_BOTH_BUFFERS;
						break;
					}

					ReadPosixTimeFromLocalFile(timeStampOfLocalSave);

// Deserialize from most recent file
					if (timeStampOfCloudSave >= timeStampOfLocalSave)
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES
					{
						ReadFullDataFromCloudFile();
					}
#if __ALLOW_LOCAL_MP_STATS_SAVES
					else
					{
						ReadFullDataFromLocalFile();
					}
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES
				}

				m_OpProgress = MP_STATS_LOAD_PROGRESS_FREE_BOTH_BUFFERS;
			}
			break;

		case MP_STATS_LOAD_PROGRESS_FREE_BOTH_BUFFERS :
			{
				CSavingMessage::Clear();

				m_Cloud.Shutdown();
				CGenericGameStorage::SetSaveOperation(OPERATION_LOADING_MPSTATS_FROM_CLOUD);
				CGenericGameStorage::FreeBufferContainingContentsOfSavegameFile(m_SavegameType);	//	Graeme - Does this only need to be called for local MP Stats files?

				CGenericGameStorage::SetSaveOperation(OPERATION_NONE);

#if __ALLOW_LOCAL_MP_STATS_SAVES
				CGenericGameStorage::SetSaveOperation(OPERATION_LOADING_MPSTATS_SAVEGAME_FROM_CONSOLE);
				CGenericGameStorage::FreeBufferContainingContentsOfSavegameFile(m_SavegameType);
//				CGenericGameStorage::FreeAllDataToBeSaved(m_SavegameType);
				CSavegameLoad::EndGenericLoad();
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES

				if (!m_bCloudFileHasLoaded || m_bFailedToReadFromCloudFile
#if __ALLOW_LOCAL_MP_STATS_SAVES
					|| m_bFailedToReadFromLocalFile
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES
					)
				{
					m_Status = MEM_CARD_ERROR;
				}
				else
				{
					m_Status = MEM_CARD_COMPLETE;
				}
			}
			break;

		case MP_STATS_LOAD_PROGRESS_CLOUD_FINISH :
			break;
	}

	if ( (m_Status == MEM_CARD_ERROR) || (m_Status == MEM_CARD_COMPLETE) )
	{
		return true;
	}

	return false;
}

void CSavegameQueuedOperation_MPStats_Save::Init(const u32 saveCategory
#if __ALLOW_LOCAL_MP_STATS_SAVES
	, eMPStatsSaveDestination saveDestination
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES
	)
{
	m_SlotIndex = saveCategory;

	if (saveCategory == 0)
	{
		m_SavegameType = SAVEGAME_MULTIPLAYER_COMMON;
	}
	else
	{
		m_SavegameType = SAVEGAME_MULTIPLAYER_CHARACTER;
	}

#if __ALLOW_LOCAL_MP_STATS_SAVES
	m_bSaveToCloud = false;
	m_bSaveToConsole = false;

	switch (saveDestination)
	{
		case MP_STATS_SAVE_TO_CLOUD :
			m_bSaveToCloud = true;
			break;
		case MP_STATS_SAVE_TO_CONSOLE :
			m_bSaveToConsole = true;
			break;
		case MP_STATS_SAVE_TO_CLOUD_AND_CONSOLE :
			m_bSaveToCloud = true;
			m_bSaveToConsole = true;
			break;
	}

	if (m_bSaveToConsole)
	{
		m_OpProgress = MP_STATS_SAVE_PROGRESS_CREATE_LOCAL_DATA_TO_BE_SAVED;
	}
	else
	{
		savegameAssertf(m_bSaveToCloud, "CSavegameQueuedOperation_MPStats_Save::Init - both bSaveToCloud and bSaveToConsole have been specified as false");
		m_OpProgress = MP_STATS_SAVE_PROGRESS_CREATE_CLOUD_DATA_TO_BE_SAVED;
	}

	m_bLocalSaveSucceeded = false;
#else	//	__ALLOW_LOCAL_MP_STATS_SAVES
	m_OpProgress = MP_STATS_SAVE_PROGRESS_CREATE_CLOUD_DATA_TO_BE_SAVED;
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES

	m_bCloudSaveSucceeded = false;
}

bool CSavegameQueuedOperation_MPStats_Save::Shutdown()
{
	m_OpProgress = MP_STATS_SAVE_PROGRESS_FINISH;

	CGenericGameStorage::SetCloudSaveResultcode( HTTP_CODE_CLIENTCLOSEDREQUEST );
//	CGenericGameStorage::SetReasonForFailureToLoadSavegame(LOAD_FAILED_REASON_FAILED_TO_LOAD);

	m_Status = MEM_CARD_ERROR;

	CSavingMessage::Clear();

	m_Cloud.Shutdown();

	CGenericGameStorage::SetSaveOperation(OPERATION_NONE);

	return true;
}

void CSavegameQueuedOperation_MPStats_Save::Cancel()
{
	savegameAssertf(m_Status == MEM_CARD_BUSY, "We are NOT currently saving the game to the cloud.");
	if (m_Status == MEM_CARD_BUSY)
	{
		CGenericGameStorage::FreeBufferContainingContentsOfSavegameFile(m_SavegameType);
		CGenericGameStorage::FreeAllDataToBeSaved(m_SavegameType);

		m_OpProgress = MP_STATS_SAVE_PROGRESS_FINISH;

		CGenericGameStorage::SetCloudSaveResultcode( HTTP_CODE_CLIENTCLOSEDREQUEST );
		//	CGenericGameStorage::SetReasonForFailureToLoadSavegame(LOAD_FAILED_REASON_FAILED_TO_LOAD);

		m_Status = MEM_CARD_ERROR;

		CSavingMessage::Clear();

		m_Cloud.Shutdown();

		CGenericGameStorage::SetSaveOperation(OPERATION_NONE);
	}
}

//    Return true to Pop() from queue
bool CSavegameQueuedOperation_MPStats_Save::Update()
{
	switch (m_OpProgress)
	{
#if __ALLOW_LOCAL_MP_STATS_SAVES
// ****************** Local Save ******************
		case MP_STATS_SAVE_PROGRESS_CREATE_LOCAL_DATA_TO_BE_SAVED :
			{
				CSavegameDialogScreens::SetDialogBoxHeading("SG_HDNG");	//	SG_HDG_SAVE_MP

				CGenericGameStorage::SetSaveOperation(OPERATION_SAVING_MPSTATS_SAVEGAME_TO_CONSOLE);

				MemoryCardError CalculateSizeStatus = CGenericGameStorage::CreateSaveDataAndCalculateSize(m_SavegameType, false);
				switch (CalculateSizeStatus)
				{
				case MEM_CARD_COMPLETE :
					savegameAssertf(CGenericGameStorage::GetBufferSize(m_SavegameType) > 0, "CSavegameQueuedOperation_MPStats_Save::Update - size of local MP save game buffer has not been calculated");
					CSavegameInitialChecks::SetSizeOfBufferToBeSaved(CGenericGameStorage::GetBufferSize(m_SavegameType));
					m_OpProgress = MP_STATS_SAVE_PROGRESS_BEGIN_LOCAL_SAVE;
					break;

				case MEM_CARD_BUSY :
					// Don't do anything - still calculating size - it might be that we're waiting for memory to be available to create the save data
					break;

				case MEM_CARD_ERROR :
					savegameAssertf(0, "CSavegameQueuedOperation_MPStats_Save::Update - CGenericGameStorage::CreateSaveDataAndCalculateSize failed for local buffer");
					m_OpProgress = MP_STATS_SAVE_PROGRESS_FREE_LOCAL_SAVE_BUFFER;
					break;
				}
			}
			break;

		case MP_STATS_SAVE_PROGRESS_BEGIN_LOCAL_SAVE :
			{
				CGenericGameStorage::SetSaveOperation(OPERATION_NONE);
				switch (CSavegameSave::BeginGameSave(CSavegameList::GetBaseIndexForSavegameFileType(SG_FILE_TYPE_MULTIPLAYER_STATS) + m_SlotIndex, m_SavegameType))
				{
					case MEM_CARD_COMPLETE :
						m_OpProgress = MP_STATS_SAVE_PROGRESS_LOCAL_SAVE_UPDATE;
						break;
					case MEM_CARD_BUSY :
						savegameAssertf(0, "CSavegameQueuedOperation_MPStats_Save::Update - didn't expect CSavegameSave::BeginGameSave to ever return MEM_CARD_BUSY");
						m_OpProgress = MP_STATS_SAVE_PROGRESS_FREE_LOCAL_SAVE_BUFFER;
						break;
					case MEM_CARD_ERROR :
						savegameAssertf(0, "CSavegameQueuedOperation_MPStats_Save::Update - CSavegameSave::BeginGameSave failed");
						m_OpProgress = MP_STATS_SAVE_PROGRESS_FREE_LOCAL_SAVE_BUFFER;
						break;
				}
			}
			break;

		case MP_STATS_SAVE_PROGRESS_LOCAL_SAVE_UPDATE :
			{
				switch (CSavegameSave::GenericSave())
				{
					case MEM_CARD_COMPLETE :
						m_bLocalSaveSucceeded = true;
						m_OpProgress = MP_STATS_SAVE_PROGRESS_FREE_LOCAL_SAVE_BUFFER;
						break;

					case MEM_CARD_BUSY :	 // the save is still in progress
						break;

					case MEM_CARD_ERROR :
						m_OpProgress = MP_STATS_SAVE_PROGRESS_FREE_LOCAL_SAVE_BUFFER;
						break;
				}
			}
			break;

		case MP_STATS_SAVE_PROGRESS_FREE_LOCAL_SAVE_BUFFER :
			
			CGenericGameStorage::SetSaveOperation(OPERATION_SAVING_MPSTATS_SAVEGAME_TO_CONSOLE);

			CGenericGameStorage::FreeBufferContainingContentsOfSavegameFile(m_SavegameType);
			CGenericGameStorage::FreeAllDataToBeSaved(m_SavegameType);

			CGenericGameStorage::SetSaveOperation(OPERATION_NONE);

			if (m_bSaveToCloud)
			{
				CGenericGameStorage::SetSaveOperation(OPERATION_SAVING_MPSTATS_TO_CLOUD);

				m_OpProgress = MP_STATS_SAVE_PROGRESS_CREATE_CLOUD_DATA_TO_BE_SAVED;
			}
			else
			{
				m_OpProgress = MP_STATS_SAVE_PROGRESS_FINISH;
			}
			break;
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES

// ****************** Cloud Save ******************

		case MP_STATS_SAVE_PROGRESS_CREATE_CLOUD_DATA_TO_BE_SAVED :
			{
				CSavegameDialogScreens::SetDialogBoxHeading("SG_HDNG");	//	SG_HDG_SAVE_MP

				CGenericGameStorage::SetSaveOperation(OPERATION_SAVING_MPSTATS_TO_CLOUD);

				MemoryCardError CalculateSizeStatus = CGenericGameStorage::CreateSaveDataAndCalculateSize(m_SavegameType, false);
				switch (CalculateSizeStatus)
				{
					case MEM_CARD_COMPLETE :
						savegameAssertf(CGenericGameStorage::GetBufferSize(m_SavegameType) > 0, "CSavegameQueuedOperation_MPStats_Save::Update - size of cloud MP save game buffer has not been calculated");
						CSavegameInitialChecks::SetSizeOfBufferToBeSaved(CGenericGameStorage::GetBufferSize(m_SavegameType));
						m_OpProgress = MP_STATS_SAVE_PROGRESS_ALLOCATE_CLOUD_SAVE_BUFFER;
						break;

					case MEM_CARD_BUSY :
						// Don't do anything - still calculating size - it might be that we're waiting for memory to be available to create the save data
						break;

					case MEM_CARD_ERROR :
						savegameAssertf(0, "CSavegameQueuedOperation_MPStats_Save::Update - CGenericGameStorage::CreateSaveDataAndCalculateSize failed for cloud");
						m_OpProgress = MP_STATS_SAVE_PROGRESS_FREE_CLOUD_SAVE_BUFFER;
						break;
				}
			}
			break;

		case MP_STATS_SAVE_PROGRESS_ALLOCATE_CLOUD_SAVE_BUFFER :
			{
				MemoryCardError AllocateBufferStatus = CGenericGameStorage::AllocateBuffer(m_SavegameType, true);
				switch (AllocateBufferStatus)
				{
					case MEM_CARD_COMPLETE :
						m_OpProgress = MP_STATS_SAVE_PROGRESS_BEGIN_FILL_CLOUD_SAVE_BUFFER;
 						break;
 					case MEM_CARD_BUSY :
 						//	Don't do anything - still waiting for memory to be allocated
 						break;
 					case MEM_CARD_ERROR :
						savegameAssertf(0, "CSavegameQueuedOperation_MPStats_Save::Update - CGenericGameStorage::AllocateBuffer failed");
						m_OpProgress = MP_STATS_SAVE_PROGRESS_FREE_CLOUD_SAVE_BUFFER;
 						break;
				}
			}
			break;

		case MP_STATS_SAVE_PROGRESS_BEGIN_FILL_CLOUD_SAVE_BUFFER:
			{
				if (CGenericGameStorage::BeginSaveBlockData(m_SavegameType, StatsInterface::CanUseEncrytionInMpSave()))
				{
					m_OpProgress = MP_STATS_SAVE_PROGRESS_FILL_CLOUD_SAVE_BUFFER;
				}
				else
				{
	//				CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_SAVE_BLOCK_DATA_FAILED);
					savegameAssertf(0, "CSavegameQueuedOperation_MPStats_Save::Update - CGenericGameStorage::BeginSaveBlockData failed for cloud buffer");
					CGenericGameStorage::EndSaveBlockData(m_SavegameType);
					m_OpProgress = MP_STATS_SAVE_PROGRESS_FREE_CLOUD_SAVE_BUFFER;
				}
			}
			break;

		case MP_STATS_SAVE_PROGRESS_FILL_CLOUD_SAVE_BUFFER :
			{
				switch (CGenericGameStorage::GetSaveBlockDataStatus(m_SavegameType))
				{
				case SAVEBLOCKDATA_PENDING:
					// Don't do anything - still waiting for data to be saved
					break;
				case SAVEBLOCKDATA_ERROR:
					{
		//				CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_SAVE_BLOCK_DATA_FAILED);
						savegameAssertf(0, "CSavegameQueuedOperation_MPStats_Save::Update - CGenericGameStorage::SaveBlockData failed for cloud buffer");
						CGenericGameStorage::EndSaveBlockData(m_SavegameType);
						m_OpProgress = MP_STATS_SAVE_PROGRESS_FREE_CLOUD_SAVE_BUFFER;
					}
					break;
				case SAVEBLOCKDATA_SUCCESS:
					{
						CGenericGameStorage::EndSaveBlockData(m_SavegameType);
						m_OpProgress = MP_STATS_SAVE_PROGRESS_BEGIN_CLOUD_SAVE;
					}
					break;
				default:
					FatalAssertf(0, "CSavegameQueuedOperation_MPStats_Save::Update - Unexpected return value from CGenericGameStorage::SaveBlockData");
					break;
				}

			}
			break;

		case MP_STATS_SAVE_PROGRESS_BEGIN_CLOUD_SAVE:
			{
				// this was getting called when the player had signed in, then signed out without closing the sign-in UI in between.
				if(NetworkInterface::GetActiveGamerInfo() && NetworkInterface::IsCloudAvailable())
				{
					CSavegameFilenames::MakeValidSaveNameForMpStatsCloudFile(m_SlotIndex);

					CSavingMessage::BeginDisplaying(CSavingMessage::STORAGE_DEVICE_MESSAGE_CLOUD_SAVING);

					m_Cloud.PostFile(CGenericGameStorage::GetBuffer(m_SavegameType)
						,CGenericGameStorage::GetBufferSize(m_SavegameType)
						,CSavegameFilenames::GetFilenameOfCloudFile()
						,NetworkInterface::GetLocalGamerIndex()
						,StatsInterface::GetCloudSavegameService());

					m_OpProgress = MP_STATS_SAVE_PROGRESS_CLOUD_SAVE_UPDATE;

					m_Status = MEM_CARD_BUSY;	//	Graeme - I think m_Status will already be MEM_CARD_BUSY at this stage
				}
				else
				{
					CGenericGameStorage::SetCloudSaveResultcode( HTTP_CODE_CLIENTCLOSEDREQUEST );
					m_OpProgress = MP_STATS_SAVE_PROGRESS_FREE_CLOUD_SAVE_BUFFER;
				}
			}
			break;

		case MP_STATS_SAVE_PROGRESS_CLOUD_SAVE_UPDATE:
			{
				if (!m_Cloud.Pending())
				{
					CGenericGameStorage::SetCloudSaveResultcode( m_Cloud.GetResultCode() );

					if (m_Cloud.Succeeded())
					{
						m_bCloudSaveSucceeded = true;
					}
					else
					{
						savegameWarningf("CSavegameQueuedOperationCloud::Update - Cloud save failed");

						const int resultCode = m_Cloud.GetResultCode(); //See http result codes

						if (resultCode == HTTP_CODE_REQUESTTIMEOUT 
							|| resultCode == HTTP_CODE_TOOMANYREQUESTS 
							|| resultCode == HTTP_CODE_GATEWAYTIMEOUT 
							NOTFINAL_ONLY( || resultCode==HTTP_CODE_ANYSERVERERROR )
							)
						{
							savegameWarningf("CSavegameQueuedOperationCloud::Update - Cloud SAVE failed - http result code = %d", resultCode);
						}
						else
						{
							savegameAssertf(0, "CSavegameQueuedOperationCloud::Update - Cloud SAVE failed - http result code = %d", resultCode);
						}
					}

					m_OpProgress = MP_STATS_SAVE_PROGRESS_FREE_CLOUD_SAVE_BUFFER;
				}
			}
			break;

		case MP_STATS_SAVE_PROGRESS_FREE_CLOUD_SAVE_BUFFER :
			CGenericGameStorage::FreeBufferContainingContentsOfSavegameFile(m_SavegameType);
			CGenericGameStorage::FreeAllDataToBeSaved(m_SavegameType);

			CGenericGameStorage::SetSaveOperation(OPERATION_NONE);

			m_OpProgress = MP_STATS_SAVE_PROGRESS_FINISH;
			break;

		case MP_STATS_SAVE_PROGRESS_FINISH :
			break;
	}

	if (m_OpProgress == MP_STATS_SAVE_PROGRESS_FINISH)
	{
		if (m_bCloudSaveSucceeded
#if __ALLOW_LOCAL_MP_STATS_SAVES
			|| m_bLocalSaveSucceeded
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES
			)
		{	//	Set status to complete if either save succeeded
			m_Status = MEM_CARD_COMPLETE;
		}
		else
		{
			m_Status = MEM_CARD_ERROR;
		}

		CSavingMessage::Clear();
		m_Cloud.Shutdown();

		return true;
	}

	return false;
}

// End of MP Stats *******************************************************************************************


void CSavegameQueuedOperation_ManualLoad::Init()
{
	m_ManualLoadProgress = MANUAL_LOAD_PROGRESS_BEGIN_SAVEGAME_LIST;

	m_bQuitAsSoonAsNoSavegameOperationsAreRunning = false;
#if RSG_ORBIS
	deleteBackupSave = false;
#endif
}

bool CSavegameQueuedOperation_ManualLoad::Update()
{
#if __ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
	bool bReturnSelection = false;
#endif	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME

	switch (m_ManualLoadProgress)
	{
		case MANUAL_LOAD_PROGRESS_BEGIN_SAVEGAME_LIST :
			CSavegameDialogScreens::SetDialogBoxHeading("SG_HDNG");	//	SG_HDG_LOAD

			if (!CSavegameFrontEnd::BeginSaveGameList())
			{
				Assertf(0, "CSavegameQueuedOperation_ManualLoad::Update - CSavegameFrontEnd::BeginSaveGameList failed");
				CPauseMenu::Close();
				savegameDisplayf("CSavegameQueuedOperation_ManualLoad::Update - setting status to MEM_CARD_ERROR (1)");
				m_Status = MEM_CARD_ERROR;
			}
			else
			{
				if (!CPauseMenu::GetNoValidSaveGameFiles())	//	sm_bNoValidSaveGameFiles will be true if the PS3 hard disk has been 
															//	scanned previously and there are no valid save games on it
				{
					//CPauseMenu::LockMenuControl();n

				}
				m_ManualLoadProgress = MANUAL_LOAD_PROGRESS_CHECK_SAVEGAME_LIST;
			}
			break;

		case MANUAL_LOAD_PROGRESS_CHECK_SAVEGAME_LIST :
			{
				switch (CSavegameFrontEnd::SaveGameMenuCheck(m_bQuitAsSoonAsNoSavegameOperationsAreRunning))
				{
					case MEM_CARD_COMPLETE :

						if (CSavegameFrontEnd::ShouldDeleteSavegame())
						{
							m_ManualLoadProgress = MANUAL_LOAD_PROGRESS_BEGIN_DELETE;
						}
						else
						{
#if __ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
							if (CSavegameFrontEnd::ShouldUploadFileToCloud())
							{
								savegameAssertf(!CSavegameList::IsSaveGameSlotEmpty(CSavegameFrontEnd::GetSaveGameSlotToUpload()), "CSavegameQueuedOperation_ManualLoad::Update - didn't expect slot %d to be empty", CSavegameFrontEnd::GetSaveGameSlotToUpload());
								savegameAssertf(!CSavegameList::GetIsDamaged(CSavegameFrontEnd::GetSaveGameSlotToUpload()), "CSavegameQueuedOperation_ManualLoad::Update - didn't expect slot %d to be damaged", CSavegameFrontEnd::GetSaveGameSlotToUpload());
								m_ManualLoadProgress = MANUAL_LOAD_PROGRESS_BEGIN_LOAD_FOR_CLOUD_UPLOAD;
							}
							else
#endif	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
							{
								savegameAssertf(!CSavegameList::IsSaveGameSlotEmpty(CSavegameFrontEnd::GetSaveGameSlotToLoad()), "CSavegameQueuedOperation_ManualLoad::Update - didn't expect slot %d to be empty", CSavegameFrontEnd::GetSaveGameSlotToLoad());
								savegameAssertf(!CSavegameList::GetIsDamaged(CSavegameFrontEnd::GetSaveGameSlotToLoad()), "CSavegameQueuedOperation_ManualLoad::Update - didn't expect slot %d to be damaged", CSavegameFrontEnd::GetSaveGameSlotToLoad());
								m_ManualLoadProgress = MANUAL_LOAD_PROGRESS_BEGIN_LOAD;

								STRVIS_SET_MARKER_TEXT("LOAD_PROGRESS_CHECK_SAVEGAME_LIST - Complete");

								// We just pressed the "Yes, we want to load a saved game"
								camInterface::FadeOut(0);	// Fade out now (B*939357)
								CLoadingScreens::Init(LOADINGSCREEN_CONTEXT_LOADLEVEL, 0);

								// pause streaming any other misc processing
								CStreaming::SetIsPlayerPositioned(false);
							}
						}
						break;

					case MEM_CARD_BUSY :	//	still processing, do nothing
						break;

					case MEM_CARD_ERROR :
						CPauseMenu::BackOutOfLoadGamePanes();
						savegameDisplayf("CSavegameQueuedOperation_ManualLoad::Update - setting status to MEM_CARD_ERROR (2)");
						m_Status = MEM_CARD_ERROR;
						break;
				}
			}
			break;

		case MANUAL_LOAD_PROGRESS_BEGIN_LOAD :
			if (MEM_CARD_ERROR == CSavegameList::BeginGameLoad(CSavegameFrontEnd::GetSaveGameSlotToLoad()))
			{
				camInterface::FadeIn(0);	//	CPauseMenu::CheckWhatToDoWhenClosed() will be calling FadeOut() at about the same time as this because bCloseAndStartSavedGame will be set
				CPauseMenu::AbandonLoadingOfSavedGame();	//	so clear bCloseAndStartSavedGame
				CGame::SetStateToIdle();
				CLoadingScreens::Shutdown(SHUTDOWN_CORE);
				savegameDisplayf("CSavegameQueuedOperation_ManualLoad::Update - setting status to MEM_CARD_ERROR (3)");
				m_Status = MEM_CARD_ERROR;
			}
			else
			{
				m_ManualLoadProgress = MANUAL_LOAD_PROGRESS_CHECK_LOAD;
			}
			break;

		case MANUAL_LOAD_PROGRESS_CHECK_LOAD :
			{
				switch (CSavegameLoad::GenericLoad())
				{
				case MEM_CARD_COMPLETE:
					{
						STRVIS_SET_MARKER_TEXT("LOAD_PROGRESS_CHECK_LOAD - complete");
						camInterface::FadeOut(0);  // THIS FADE IS OK AS WE ARE STARTING A NEW GAME or loading

						CGame::HandleLoadedSaveGame();
						savegameDisplayf("CSavegameQueuedOperation_ManualLoad::Update - setting status to MEM_CARD_COMPLETE (1)");
						m_Status = MEM_CARD_COMPLETE;
					}
					break;
				case MEM_CARD_BUSY: // the load is still in progress
					break;
				case MEM_CARD_ERROR:
					{
						camInterface::FadeIn(0);	//	CPauseMenu::CheckWhatToDoWhenClosed() will be calling FadeOut() at about the same time as this because bCloseAndStartSavedGame will be set
						CPauseMenu::AbandonLoadingOfSavedGame();	//	so clear bCloseAndStartSavedGame
						CGame::SetStateToIdle();
						CLoadingScreens::Shutdown(SHUTDOWN_CORE);
						savegameDisplayf("CSavegameQueuedOperation_ManualLoad::Update - setting status to MEM_CARD_ERROR (4)");
						m_Status = MEM_CARD_ERROR;
					}
					break;
				}
			}
			break;

		case MANUAL_LOAD_PROGRESS_BEGIN_DELETE :
			{
				s32 correctSlot = CSavegameFrontEnd::GetSaveGameSlotToDelete();
#if RSG_ORBIS
				if(deleteBackupSave)
				{
					//make sure we get the correct slot numbers for the main save and the backup save
					if(CSavegameFrontEnd::GetSaveGameSlotToDelete() < TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES)
						correctSlot = CSavegameFrontEnd::GetSaveGameSlotToDelete() + INDEX_OF_BACKUPSAVE_SLOT;
				}
#endif
				CSavegameFilenames::CreateNameOfLocalFile(m_FilenameOfFileToDelete, SAVE_GAME_MAX_FILENAME_LENGTH_OF_LOCAL_FILE, correctSlot);

#if RSG_ORBIS
				CSavegameList::SetSlotToUpdateOnceDeleteHasCompleted(correctSlot);
#endif	//	RSG_ORBIS

				if (CSavegameDelete::Begin(m_FilenameOfFileToDelete, false, false))	//	m_bCloudFile))
				{
					savegameDisplayf("CSavegameQueuedOperation_ManualLoad::Update - CSavegameDelete::Begin succeeded for %s", m_FilenameOfFileToDelete);
					m_ManualLoadProgress = MANUAL_LOAD_PROGRESS_CHECK_DELETE;
				}
				else
				{
					savegameDisplayf("CSavegameQueuedOperation_ManualLoad::Update - CSavegameDelete::Begin failed for %s", m_FilenameOfFileToDelete);
					savegameDisplayf("CSavegameQueuedOperation_ManualLoad::Update - setting status to MEM_CARD_ERROR (5)");
					m_Status = MEM_CARD_ERROR;
//					return true;
				}
			}
			break;

		case MANUAL_LOAD_PROGRESS_CHECK_DELETE :
			{
				switch (CSavegameDelete::Update())
				{
				case MEM_CARD_COMPLETE :
					savegameDisplayf("CSavegameQueuedOperation_ManualLoad::Update - CSavegameDelete::Update succeeded for %s", m_FilenameOfFileToDelete);
					
#if ENABLE_SINGLE_PLAYER_CLOUD_SAVES
					CLiveManager::UnregisterSinglePlayerCloudSaveFile(m_FilenameOfFileToDelete);
#endif	//	RSG_PC

#if RSG_ORBIS
					//set progress back to start of delete to delete appropriate backup save
					if(!deleteBackupSave && CSavegameFrontEnd::GetSaveGameSlotToDelete() < TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES)
					{
						deleteBackupSave = true;
						m_ManualLoadProgress = MANUAL_LOAD_PROGRESS_BEGIN_DELETE;
					}
					else
#endif
					{
#if RSG_ORBIS
						deleteBackupSave = false;

#if USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP && GTA_REPLAY
						if (CSavegameFilenames::IsThisTheNameOfAnAutosaveBackupFile(m_FilenameOfFileToDelete))
						{
							savegameDisplayf("CSavegameQueuedOperation_ManualLoad::Update - CSavegameDelete::Update succeeded for an autosave backup in download0 so call ReplayFileManager::RefreshNonClipDirectorySizes()");
							ReplayFileManager::RefreshNonClipDirectorySizes();
						}
#endif	//	USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP && GTA_REPLAY
#endif	//	RSG_ORBIS

						m_ManualLoadProgress = MANUAL_LOAD_PROGRESS_DISPLAY_DELETE_SUCCESS_MESSAGE;
					}

#if RSG_ORBIS
					CSavegameList::UpdateSlotDataAfterDelete();
#endif	//	RSG_ORBIS
//					m_Status = MEM_CARD_COMPLETE;
//					return true;
					break;

				case MEM_CARD_BUSY :
					break;

				case MEM_CARD_ERROR :
					savegameDisplayf("CSavegameQueuedOperation_ManualLoad::Update - CSavegameDelete::Update failed for %s", m_FilenameOfFileToDelete);
					savegameDisplayf("CSavegameQueuedOperation_ManualLoad::Update - setting status to MEM_CARD_ERROR (6)");
					m_Status = MEM_CARD_ERROR;
//					return true;
					break;
				}
			}
			break;

		case MANUAL_LOAD_PROGRESS_DISPLAY_DELETE_SUCCESS_MESSAGE :
			{
//				CSavingMessage::Clear();

				bool ReturnSelection = false;
				if (CSavegameDialogScreens::HandleSaveGameDialogBox(SAVE_GAME_DIALOG_DELETION_OF_SAVEGAME_SUCCEEDED, 0, &ReturnSelection))
				{
					savegameDisplayf("CSavegameQueuedOperation_ManualLoad::Update - setting status to MEM_CARD_COMPLETE (2)");
					m_Status = MEM_CARD_COMPLETE;
					CPauseMenu::SetSavegameListIsBeingRedrawnAfterDeletingASavegame(true);
				}
			}
			break;


#if __ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
		case MANUAL_LOAD_PROGRESS_BEGIN_LOAD_FOR_CLOUD_UPLOAD :
			{
				FillJsonString(CSavegameFrontEnd::GetSaveGameSlotToUpload());

				if (MEM_CARD_ERROR == CSavegameList::BeginGameLoad(CSavegameFrontEnd::GetSaveGameSlotToUpload()))
				{
//					camInterface::FadeIn(0);	//	CPauseMenu::CheckWhatToDoWhenClosed() will be calling FadeOut() at about the same time as this because bCloseAndStartSavedGame will be set
//					CPauseMenu::AbandonLoadingOfSavedGame();	//	so clear bCloseAndStartSavedGame
//					CGame::SetStateToIdle();
//					CLoadingScreens::Shutdown(SHUTDOWN_CORE);
					savegameDisplayf("CSavegameQueuedOperation_ManualLoad::Update - setting status to MEM_CARD_ERROR (7)");
					m_Status = MEM_CARD_ERROR;
				}
				else
				{
					m_ManualLoadProgress = MANUAL_LOAD_PROGRESS_CHECK_LOAD_FOR_CLOUD_UPLOAD;
				}
			}
			break;
	
		case MANUAL_LOAD_PROGRESS_CHECK_LOAD_FOR_CLOUD_UPLOAD :
			{
				switch (CSavegameLoad::GenericLoad())
				{
				case MEM_CARD_COMPLETE:
					{
//						STRVIS_SET_MARKER_TEXT("LOAD_PROGRESS_CHECK_LOAD - complete");
//						camInterface::FadeOut(0);  // THIS FADE IS OK AS WE ARE STARTING A NEW GAME or loading

//						CGame::HandleLoadedSaveGame();
//						m_Status = MEM_CARD_COMPLETE;
						CSavegameLoad::EndGenericLoad();
						m_ManualLoadProgress = MANUAL_LOAD_PROGRESS_BEGIN_CLOUD_UPLOAD;
					}
					break;
				case MEM_CARD_BUSY: // the load is still in progress
					if (SAVE_GAME_DIALOG_NONE == CSavegameDialogScreens::GetMostRecentErrorCode())
					{
						CSavegameDialogScreens::HandleSaveGameDialogBox(SAVE_GAME_DIALOG_LOADING_LOCAL_SAVEGAME_BEFORE_UPLOADING_TO_CLOUD, 0, &bReturnSelection);
					}
					break;
				case MEM_CARD_ERROR:
					{
//						camInterface::FadeIn(0);	//	CPauseMenu::CheckWhatToDoWhenClosed() will be calling FadeOut() at about the same time as this because bCloseAndStartSavedGame will be set
//						CPauseMenu::AbandonLoadingOfSavedGame();	//	so clear bCloseAndStartSavedGame
//						CGame::SetStateToIdle();
//						CLoadingScreens::Shutdown(SHUTDOWN_CORE);
						CSavegameLoad::EndGenericLoad();
						savegameDisplayf("CSavegameQueuedOperation_ManualLoad::Update - setting status to MEM_CARD_ERROR (8)");
						m_Status = MEM_CARD_ERROR;
					}
					break;
				}
			}
			break;

		case MANUAL_LOAD_PROGRESS_BEGIN_CLOUD_UPLOAD :
			{
				CSavegameDialogScreens::SetDialogBoxHeading("SG_HDNG");	//	SG_HDG_S_PH_CL	//	I could have a different message here to differentiate it from the gallery photos

				MemoryCardError resultOfBeginCall = MEM_CARD_ERROR;

				resultOfBeginCall = CSavegameSaveBlock::BeginUploadOfSavegameToCloud("GTA5/saves/PreviousGenSave.save", CGenericGameStorage::GetBuffer(SAVEGAME_SINGLE_PLAYER), CGenericGameStorage::GetBufferSize(SAVEGAME_SINGLE_PLAYER), m_JsonString);

				switch (resultOfBeginCall)
				{
				case MEM_CARD_COMPLETE :
					savegameDisplayf("CSavegameQueuedOperation_ManualLoad::Update - BEGIN_CLOUD_UPLOAD - BeginUploadOfSavegameToCloud succeeded");
					m_ManualLoadProgress = MANUAL_LOAD_PROGRESS_CHECK_CLOUD_UPLOAD;
					break;

				case MEM_CARD_BUSY :
					savegameAssertf(0, "CSavegameQueuedOperation_ManualLoad::Update - BEGIN_CLOUD_UPLOAD - didn't expect CSavegameSaveBlock::BeginUploadOfSavegameToCloud() to ever return MEM_CARD_BUSY");
					savegameDisplayf("CSavegameQueuedOperation_ManualLoad::Update - setting status to MEM_CARD_ERROR (9)");
					m_Status = MEM_CARD_ERROR;
					break;
				case MEM_CARD_ERROR :
					savegameAssertf(0, "CSavegameQueuedOperation_ManualLoad::Update - BEGIN_CLOUD_UPLOAD - CSavegameSaveBlock::BeginUploadOfSavegameToCloud() returned MEM_CARD_ERROR");
					savegameDisplayf("CSavegameQueuedOperation_ManualLoad::Update - setting status to MEM_CARD_ERROR (10)");
					m_Status = MEM_CARD_ERROR;
					break;
				}

			}
			break;

		case MANUAL_LOAD_PROGRESS_CHECK_CLOUD_UPLOAD :
			{
				switch (CSavegameSaveBlock::SaveBlock(true))
				{
				case MEM_CARD_COMPLETE :
					photoDisplayf("CSavegameQueuedOperation_ManualLoad::Update - CHECK_CLOUD_UPLOAD - CSavegameSaveBlock succeeded");
					m_ManualLoadProgress = MANUAL_LOAD_PROGRESS_DISPLAY_UPLOAD_SUCCESS_MESSAGE;
					break;

				case MEM_CARD_BUSY :    // the save is still in progress
					if (SAVE_GAME_DIALOG_NONE == CSavegameDialogScreens::GetMostRecentErrorCode())
					{
						CSavegameDialogScreens::HandleSaveGameDialogBox(SAVE_GAME_DIALOG_UPLOADING_SAVEGAME_TO_CLOUD, 0, &bReturnSelection);
					}
					break;

				case MEM_CARD_ERROR :
					photoDisplayf("CSavegameQueuedOperation_ManualLoad::Update - CHECK_CLOUD_UPLOAD - CSavegameSaveBlock failed");
					savegameDisplayf("CSavegameQueuedOperation_ManualLoad::Update - setting status to MEM_CARD_ERROR (11)");
					m_Status = MEM_CARD_ERROR;
					break;
				}
			}
			break;

		case MANUAL_LOAD_PROGRESS_DISPLAY_UPLOAD_SUCCESS_MESSAGE :
			{
//				CSavingMessage::Clear();

				bool ReturnSelection = false;
				if (CSavegameDialogScreens::HandleSaveGameDialogBox(SAVE_GAME_DIALOG_UPLOAD_OF_SAVEGAME_SUCCEEDED, 0, &ReturnSelection))
				{
					savegameDisplayf("CSavegameQueuedOperation_ManualLoad::Update - setting status to MEM_CARD_COMPLETE (3)");
					m_Status = MEM_CARD_COMPLETE;
				}
			}
			break;
#endif	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
	}


	if ( (m_Status == MEM_CARD_ERROR) || (m_Status == MEM_CARD_COMPLETE) )
	{
		switch (m_ManualLoadProgress)
		{
			case MANUAL_LOAD_PROGRESS_BEGIN_DELETE :
			case MANUAL_LOAD_PROGRESS_CHECK_DELETE :
			case MANUAL_LOAD_PROGRESS_DISPLAY_DELETE_SUCCESS_MESSAGE :
				{
					if (m_bQuitAsSoonAsNoSavegameOperationsAreRunning)
					{
						return true;
					}
					else
					{
						m_ManualLoadProgress = MANUAL_LOAD_PROGRESS_BEGIN_SAVEGAME_LIST;
						savegameDisplayf("CSavegameQueuedOperation_ManualLoad::Update - setting status to MEM_CARD_BUSY (1)");
						m_Status = MEM_CARD_BUSY;
					}
				}
				break;

#if __ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
			case MANUAL_LOAD_PROGRESS_BEGIN_LOAD_FOR_CLOUD_UPLOAD :
			case MANUAL_LOAD_PROGRESS_CHECK_LOAD_FOR_CLOUD_UPLOAD :
			case MANUAL_LOAD_PROGRESS_BEGIN_CLOUD_UPLOAD :
			case MANUAL_LOAD_PROGRESS_CHECK_CLOUD_UPLOAD :
			case MANUAL_LOAD_PROGRESS_DISPLAY_UPLOAD_SUCCESS_MESSAGE :
				{
					CGenericGameStorage::FreeBufferContainingContentsOfSavegameFile(SAVEGAME_SINGLE_PLAYER);
					//			CSavegameLoad::EndGenericLoad();	//	this line should maybe be moved so that it only gets called in UPLOAD_PROGRESS_CHECK_LOAD for pass or fail

					//			CPauseMenu::BackOutOfLoadGamePanes();

					if (m_bQuitAsSoonAsNoSavegameOperationsAreRunning)
					{
						return true;
					}
					else
					{
						m_ManualLoadProgress = MANUAL_LOAD_PROGRESS_BEGIN_SAVEGAME_LIST;
						savegameDisplayf("CSavegameQueuedOperation_ManualLoad::Update - setting status to MEM_CARD_BUSY (2)");
						m_Status = MEM_CARD_BUSY;
					}
				}
				break;
#endif	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME

			default :
				{
					if (m_Status == MEM_CARD_COMPLETE)
					{
						savegameDisplayf("CSavegameQueuedOperation_ManualLoad::Update - m_Status is MEM_CARD_COMPLETE so returning TRUE");

						return true;
					}
					else if (m_Status == MEM_CARD_ERROR)
					{
						// We won't be starting a new session so unpause the streaming
						CStreaming::SetIsPlayerPositioned(true);

						// Un-mute the audio
						audNorthAudioEngine::CancelStartNewGame();

						savegameDisplayf("CSavegameQueuedOperation_ManualLoad::Update - m_Status is MEM_CARD_ERROR so returning TRUE");

						return true;
					}
				}
				break;
		}
	}

	return false;
}

#if __ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME
bool CSavegameQueuedOperation_ManualLoad::FillJsonString(s32 savegameSlotNumber)
{
	memset(m_JsonString, 0, LENGTH_OF_JSON_STRING);
	RsonWriter rsonWriter(m_JsonString, LENGTH_OF_JSON_STRING, RSON_FORMAT_JSON);
	rsonWriter.Reset();

	bool bSuccess = rsonWriter.Begin(NULL, NULL);	//	Top-level structure to contain all the rest of the data

	if (bSuccess)
	{
		bool SlotIsEmpty = false;
		bool SlotIsDamaged = false;

		char cSaveGameName[SAVE_GAME_MAX_DISPLAY_NAME_LENGTH];
		char cSaveGameDate[MAX_MENU_ITEM_CHAR_LENGTH];

		CSavegameList::GetDisplayNameAndDateForThisSaveGameItem(savegameSlotNumber, cSaveGameName, cSaveGameDate, &SlotIsEmpty, &SlotIsDamaged);
		bSuccess = rsonWriter.WriteString("disp", cSaveGameName);
	}

	if (bSuccess)
	{
		bSuccess = rsonWriter.End();	//	Close the top-level structure
	}

	return bSuccess;
}
#endif	//	__ALLOW_CLOUD_UPLOAD_OF_LOCAL_SAVEGAME



void CSavegameQueuedOperation_ManualSave::Init()
{
	m_ManualSaveProgress = MANUAL_SAVE_PROGRESS_CREATE_SAVE_BUFFER;

	m_bQuitAsSoonAsNoSavegameOperationsAreRunning = false;
#if RSG_ORBIS
	deleteBackupSave = false;
#endif
}

bool CSavegameQueuedOperation_ManualSave::Update()
{
	switch (m_ManualSaveProgress)
	{
		case MANUAL_SAVE_PROGRESS_CREATE_SAVE_BUFFER :
			{
				CSavegameDialogScreens::SetDialogBoxHeading("SG_HDNG");	//	SG_HDG_SAVE

				MemoryCardError CalculateSizeStatus = CGenericGameStorage::CreateSaveDataAndCalculateSize(SAVEGAME_SINGLE_PLAYER, false);
				switch (CalculateSizeStatus)
				{
				case MEM_CARD_COMPLETE :
					Assertf(CGenericGameStorage::GetBufferSize(SAVEGAME_SINGLE_PLAYER) > 0, "CSavegameQueuedOperation_ManualSave::Update - size of save game buffer has not been calculated");
					CSavegameInitialChecks::SetSizeOfBufferToBeSaved(CGenericGameStorage::GetBufferSize(SAVEGAME_SINGLE_PLAYER));
					m_ManualSaveProgress = MANUAL_SAVE_PROGRESS_BEGIN_SAVEGAME_LIST;
					break;

				case MEM_CARD_BUSY :
					// Don't do anything - still calculating size - it might be that we're waiting for memory to be available to create the save data
					break;

				case MEM_CARD_ERROR :
//					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_ALLOCATE_MEMORY_FAILED);
//					return MEM_CARD_ERROR;
					Assertf(0, "CSavegameQueuedOperation_ManualSave::Update - CGenericGameStorage::CreateSaveDataAndCalculateSize failed");
					CPauseMenu::Close();
					m_Status = MEM_CARD_ERROR;
					break;
				}
			}
			break;
		case MANUAL_SAVE_PROGRESS_BEGIN_SAVEGAME_LIST :
			if (!CSavegameFrontEnd::BeginSaveGameList())
			{
				Assertf(0, "CSavegameQueuedOperation_ManualSave::Update - CSavegameFrontEnd::BeginSaveGameList failed");
				CPauseMenu::Close();
				m_Status = MEM_CARD_ERROR;
			}
			else
			{
				if (!CPauseMenu::GetNoValidSaveGameFiles())	//	sm_bNoValidSaveGameFiles will be true if the PS3 hard disk has been 
															//	scanned previously and there are no valid save games on it
				{
					CPauseMenu::LockMenuControl(true);
				}
				m_ManualSaveProgress = MANUAL_SAVE_PROGRESS_CHECK_SAVEGAME_LIST;
			}
			break;
		case MANUAL_SAVE_PROGRESS_CHECK_SAVEGAME_LIST :
			switch (CSavegameFrontEnd::SaveGameMenuCheck(m_bQuitAsSoonAsNoSavegameOperationsAreRunning))
			{
			case MEM_CARD_COMPLETE :
				if (CSavegameFrontEnd::ShouldDeleteSavegame())
				{
					m_ManualSaveProgress = MANUAL_SAVE_PROGRESS_BEGIN_DELETE;
				}
				else
				{
#if __MANUAL_SAVE_CLOSE_PAUSE_MENU_AS_SOON_AS_POSSIBLE
					CPauseMenu::Close();
					m_ManualSaveProgress = MANUAL_SAVE_PROGRESS_WAIT_UNTIL_PAUSE_MENU_IS_CLOSED;
					m_SafetyTimeOut.Reset();
#else	//	__MANUAL_SAVE_CLOSE_PAUSE_MENU_AS_SOON_AS_POSSIBLE
					m_ManualSaveProgress = MANUAL_SAVE_PROGRESS_CHECK_SAVE;
#endif	//	__MANUAL_SAVE_CLOSE_PAUSE_MENU_AS_SOON_AS_POSSIBLE
				}
				break;

			case MEM_CARD_BUSY :	//	still processing, do nothing
				break;

			case MEM_CARD_ERROR :
				CPauseMenu::UnlockMenuControl();	//	or should this be CPauseMenu::BackOutOfSaveGameList()?
				CPauseMenu::Close();
				m_Status = MEM_CARD_ERROR;
				break;
			}
			break;

#if __MANUAL_SAVE_CLOSE_PAUSE_MENU_AS_SOON_AS_POSSIBLE
		case MANUAL_SAVE_PROGRESS_WAIT_UNTIL_PAUSE_MENU_IS_CLOSED :
			{
				bool bMoveOnToNextStage = false;

				if (!CPauseMenu::IsActive())
				{
					savegameDisplayf("CSavegameQueuedOperation_ManualSave::Update - MANUAL_SAVE_PROGRESS_WAIT_UNTIL_PAUSE_MENU_IS_CLOSED - CPauseMenu::IsActive() has returned false so move on to MANUAL_SAVE_PROGRESS_WAIT_UNTIL_SPINNER_CAN_BE_DISPLAYED. m_SafetyTimeOut=%f", m_SafetyTimeOut.GetMsTime());
					bMoveOnToNextStage = true;
				}
				else if (m_SafetyTimeOut.GetMsTime() > 3000.0f)
				{
					savegameDisplayf("CSavegameQueuedOperation_ManualSave::Update - MANUAL_SAVE_PROGRESS_WAIT_UNTIL_PAUSE_MENU_IS_CLOSED - CPauseMenu::IsActive() has returned true but timing out and moving on to MANUAL_SAVE_PROGRESS_WAIT_UNTIL_SPINNER_CAN_BE_DISPLAYED anyway");
					bMoveOnToNextStage = true;
				}

				if (bMoveOnToNextStage)
				{
					m_ManualSaveProgress = MANUAL_SAVE_PROGRESS_WAIT_UNTIL_SPINNER_CAN_BE_DISPLAYED;
					m_SafetyTimeOut.Reset();
				}
			}
			break;

		case MANUAL_SAVE_PROGRESS_WAIT_UNTIL_SPINNER_CAN_BE_DISPLAYED :
			if (camInterface::IsFadedOut())
			{
				if (CGtaOldLoadingScreen::IsUsingLoadingRenderFunction())
				{
					m_ManualSaveProgress = MANUAL_SAVE_PROGRESS_CHECK_SAVE;
					savegameDisplayf("CSavegameQueuedOperation_ManualSave::Update - MANUAL_SAVE_PROGRESS_WAIT_UNTIL_SPINNER_CAN_BE_DISPLAYED - CGtaOldLoadingScreen::IsUsingLoadingRenderFunction() is true. m_SafetyTimeOut=%f", m_SafetyTimeOut.GetMsTime());
				}
				else
				{
					savegameDisplayf("CSavegameQueuedOperation_ManualSave::Update - MANUAL_SAVE_PROGRESS_WAIT_UNTIL_SPINNER_CAN_BE_DISPLAYED - camInterface::IsFadedOut() but CGtaOldLoadingScreen::IsUsingLoadingRenderFunction() is false. m_SafetyTimeOut=%f", m_SafetyTimeOut.GetMsTime());
					if (m_SafetyTimeOut.GetMsTime() > 3000.0f)
					{
						savegameDisplayf("CSavegameQueuedOperation_ManualSave::Update - safety time out");
						m_ManualSaveProgress = MANUAL_SAVE_PROGRESS_CHECK_SAVE;
					}
				}
			}
			else
			{
				m_ManualSaveProgress = MANUAL_SAVE_PROGRESS_CHECK_SAVE;
			}
			break;
#endif	//	__MANUAL_SAVE_CLOSE_PAUSE_MENU_AS_SOON_AS_POSSIBLE

		case MANUAL_SAVE_PROGRESS_CHECK_SAVE :
			
			switch (CSavegameSave::GenericSave())
			{
				case MEM_CARD_COMPLETE :
#if !__MANUAL_SAVE_CLOSE_PAUSE_MENU_AS_SOON_AS_POSSIBLE
					CPauseMenu::Close();
#endif	//	!__MANUAL_SAVE_CLOSE_PAUSE_MENU_AS_SOON_AS_POSSIBLE
					m_Status = MEM_CARD_COMPLETE;
					break;

				case MEM_CARD_BUSY :	 // the save is still in progress
					break;

				case MEM_CARD_ERROR :
#if !__MANUAL_SAVE_CLOSE_PAUSE_MENU_AS_SOON_AS_POSSIBLE
					CPauseMenu::Close();
#endif	//	!__MANUAL_SAVE_CLOSE_PAUSE_MENU_AS_SOON_AS_POSSIBLE
					m_Status = MEM_CARD_ERROR;
					break;
			}
			break;

//	DELETING A SAVEGAME

		case MANUAL_SAVE_PROGRESS_BEGIN_DELETE :
			{
				s32 correctSlot = CSavegameFrontEnd::GetSaveGameSlotToDelete();
#if RSG_ORBIS
				if(deleteBackupSave)
				{
					//make sure we get the correct slot numbers for the main save and the backup save
					if(CSavegameFrontEnd::GetSaveGameSlotToDelete() < TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES)
						correctSlot = CSavegameFrontEnd::GetSaveGameSlotToDelete() + INDEX_OF_BACKUPSAVE_SLOT;
				}
#endif
				CSavegameFilenames::CreateNameOfLocalFile(m_FilenameOfFileToDelete, SAVE_GAME_MAX_FILENAME_LENGTH_OF_LOCAL_FILE, correctSlot);
			
#if RSG_ORBIS
				CSavegameList::SetSlotToUpdateOnceDeleteHasCompleted(correctSlot);
#endif	//	RSG_ORBIS

				if (CSavegameDelete::Begin(m_FilenameOfFileToDelete, false, false))	//	m_bCloudFile))
				{
					savegameDisplayf("CSavegameQueuedOperation_ManualSave::Update - CSavegameDelete::Begin succeeded for %s", m_FilenameOfFileToDelete);
					m_ManualSaveProgress = MANUAL_SAVE_PROGRESS_CHECK_DELETE;
				}
				else
				{
					savegameDisplayf("CSavegameQueuedOperation_ManualSave::Update - CSavegameDelete::Begin failed for %s", m_FilenameOfFileToDelete);
					m_Status = MEM_CARD_ERROR;
//					return true;
				}
			}
			break;

		case MANUAL_SAVE_PROGRESS_CHECK_DELETE :
			{
				switch (CSavegameDelete::Update())
				{
				case MEM_CARD_COMPLETE :
					savegameDisplayf("CSavegameQueuedOperation_ManualSave::Update - CSavegameDelete::Update succeeded for %s", m_FilenameOfFileToDelete);
					
#if ENABLE_SINGLE_PLAYER_CLOUD_SAVES
					CLiveManager::UnregisterSinglePlayerCloudSaveFile(m_FilenameOfFileToDelete);
#endif	//	RSG_PC

#if RSG_ORBIS
					//set progress back to start of delete to delete appropriate backup save
					if(!deleteBackupSave)
					{
						deleteBackupSave = true;
						m_ManualSaveProgress = MANUAL_SAVE_PROGRESS_BEGIN_DELETE;
					}
					else
#endif
					{
#if RSG_ORBIS
						deleteBackupSave = false;

#if USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP && GTA_REPLAY
						if (CSavegameFilenames::IsThisTheNameOfAnAutosaveBackupFile(m_FilenameOfFileToDelete))
						{	// We probably can't actually reach here since the autosave isn't displayed in the Save menu
							savegameDisplayf("CSavegameQueuedOperation_ManualSave::Update - CSavegameDelete::Update succeeded for an autosave backup in download0 so call ReplayFileManager::RefreshNonClipDirectorySizes()");
							ReplayFileManager::RefreshNonClipDirectorySizes();
						}
#endif	//	USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP && GTA_REPLAY
#endif	//	RSG_ORBIS
						m_ManualSaveProgress = MANUAL_SAVE_PROGRESS_DISPLAY_DELETE_SUCCESS_MESSAGE;
					}

#if RSG_ORBIS
					CSavegameList::UpdateSlotDataAfterDelete();
#endif	//	RSG_ORBIS

//					m_Status = MEM_CARD_COMPLETE;
//					return true;
					break;

				case MEM_CARD_BUSY :
					break;

				case MEM_CARD_ERROR :
					savegameDisplayf("CSavegameQueuedOperation_ManualSave::Update - CSavegameDelete::Update failed for %s", m_FilenameOfFileToDelete);
					m_Status = MEM_CARD_ERROR;
//					return true;
					break;
				}
			}
			break;

		case MANUAL_SAVE_PROGRESS_DISPLAY_DELETE_SUCCESS_MESSAGE :
			{
//				CSavingMessage::Clear();

				bool ReturnSelection = false;
				if (CSavegameDialogScreens::HandleSaveGameDialogBox(SAVE_GAME_DIALOG_DELETION_OF_SAVEGAME_SUCCEEDED, 0, &ReturnSelection))
				{
					m_Status = MEM_CARD_COMPLETE;
				}
			}
			break;
	}

	if ( (m_Status == MEM_CARD_ERROR) || (m_Status == MEM_CARD_COMPLETE) )
	{
		bool bRedisplayListAfterDeletingASavegame = false;
		switch (m_ManualSaveProgress)
		{
			case MANUAL_SAVE_PROGRESS_BEGIN_DELETE :
			case MANUAL_SAVE_PROGRESS_CHECK_DELETE :
			case MANUAL_SAVE_PROGRESS_DISPLAY_DELETE_SUCCESS_MESSAGE :
				if (!m_bQuitAsSoonAsNoSavegameOperationsAreRunning)
				{
					CPauseMenu::SetSavegameListIsBeingRedrawnAfterDeletingASavegame(true);

					m_ManualSaveProgress = MANUAL_SAVE_PROGRESS_BEGIN_SAVEGAME_LIST;
					m_Status = MEM_CARD_BUSY;

					bRedisplayListAfterDeletingASavegame = true;
				}
				break;

			default :
				break;
		}

		if (!bRedisplayListAfterDeletingASavegame)
		{
			CGenericGameStorage::FreeBufferContainingContentsOfSavegameFile(SAVEGAME_SINGLE_PLAYER);
			CGenericGameStorage::FreeAllDataToBeSaved(SAVEGAME_SINGLE_PLAYER);

			//	Do this for all successful saves
			if (m_Status == MEM_CARD_COMPLETE)
			{
				CTheScripts::GetMissionReplayStats().ClearReplayStatsStructure();
				savegameDisplayf("CSavegameQueuedOperation_ManualSave::Update has cleared the replay stats structure after a successful save");
			}

			return true;
		}
	}

	return false;
}


bool CSavegameQueuedOperation_ManualSave::Shutdown()
{
//	I don't think I need to do anything here. I'll just rely on CGenericGameStorage::FinishAllUnfinishedSaveGameOperations()
	return false;
}


void CSavegameQueuedOperation_Autosave::Init()
{
	m_AutosaveProgress = AUTOSAVE_PROGRESS_CREATE_SAVE_BUFFER;
}

	//	Return true to Pop() from queue
bool CSavegameQueuedOperation_Autosave::Update()
{
	switch (m_AutosaveProgress)
	{
		case AUTOSAVE_PROGRESS_CREATE_SAVE_BUFFER :
			{
				CSavegameDialogScreens::SetDialogBoxHeading("SG_HDNG");	//	SG_HDG_AUTO

				MemoryCardError CalculateSizeStatus = CGenericGameStorage::CreateSaveDataAndCalculateSize(SAVEGAME_SINGLE_PLAYER, true);
				switch (CalculateSizeStatus)
				{
				case MEM_CARD_COMPLETE :
					Assertf(CGenericGameStorage::GetBufferSize(SAVEGAME_SINGLE_PLAYER) > 0, "CSavegameQueuedOperation_Autosave::Update - size of save game buffer has not been calculated");
					CSavegameInitialChecks::SetSizeOfBufferToBeSaved(CGenericGameStorage::GetBufferSize(SAVEGAME_SINGLE_PLAYER));
					CSavegameSave::BeginGameSave(-1, SAVEGAME_SINGLE_PLAYER);
					m_AutosaveProgress = AUTOSAVE_PROGRESS_CHECK_SAVE;
					break;

				case MEM_CARD_BUSY :
					// Don't do anything - still calculating size - it might be that we're waiting for memory to be available to create the save data
					break;

				case MEM_CARD_ERROR :
//					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_ALLOCATE_MEMORY_FAILED);
//					return MEM_CARD_ERROR;
					Assertf(0, "CSavegameQueuedOperation_Autosave::Update - CGenericGameStorage::CreateSaveDataAndCalculateSize failed");
//					CPauseMenu::Close();
					m_Status = MEM_CARD_ERROR;
					break;
				}
			}
			break;

		case AUTOSAVE_PROGRESS_CHECK_SAVE :
			switch (CSavegameSave::GenericSave())
			{
			case MEM_CARD_COMPLETE :
				m_Status = MEM_CARD_COMPLETE;
				break;

			case MEM_CARD_BUSY :	 // the save is still in progress
				break;

			case MEM_CARD_ERROR :
				m_Status = MEM_CARD_ERROR;
				break;
			}
			break;
	}

	if ( (m_Status == MEM_CARD_ERROR) || (m_Status == MEM_CARD_COMPLETE) )
	{
		CGenericGameStorage::FreeBufferContainingContentsOfSavegameFile(SAVEGAME_SINGLE_PLAYER);
		CGenericGameStorage::FreeAllDataToBeSaved(SAVEGAME_SINGLE_PLAYER);

		//	Do this for all successful saves
		if (m_Status == MEM_CARD_COMPLETE)
		{
#if USE_SAVE_DATA_MEMORY && !USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP
			CGenericGameStorage::RequestBackupOfAutosave();
#endif	//	USE_SAVE_DATA_MEMORY && !USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP

			CTheScripts::GetMissionReplayStats().ClearReplayStatsStructure();
			savegameDisplayf("CSavegameQueuedOperation_Autosave::Update has cleared the replay stats structure after a successful save");
		}

		return true;
	}

	return false;
}

bool CSavegameQueuedOperation_Autosave::Shutdown()
{
//	I don't think I need to do anything here. I'll just rely on CGenericGameStorage::FinishAllUnfinishedSaveGameOperations()
	return false;
}


#if GTA_REPLAY

#if USE_SAVE_SYSTEM
void CSavegameQueuedOperation_ReplayLoad::Init()
{
	m_ManualLoadProgress = REPLAY_LOAD_PROGRESS_BEGIN_LOAD;
}

bool CSavegameQueuedOperation_ReplayLoad::Update()
{
	switch (m_ManualLoadProgress)
	{
	case REPLAY_LOAD_PROGRESS_BEGIN_LOAD :

		CSavegameDialogScreens::SetDialogBoxHeading("SG_HDNG");	//	SG_HDG_LD_RPL

		if (MEM_CARD_ERROR == CSavegameList::BeginGameLoad(INDEX_OF_REPLAYSAVE_SLOT))
		{
			//	should I call CPauseMenu::Close(); here?

			CGame::SetStateToIdle();

			camInterface::FadeOut(0);  // THIS FADE IS OK AS WE ARE STARTING A NEW GAME or loading
			CGame::StartNewGame(CGameLogic::GetCurrentLevelIndex());

			m_Status = MEM_CARD_ERROR;
			return true;
		}

		camInterface::FadeOut(0);  // fade out the screen
		CGame::LoadSaveGame();

		m_ManualLoadProgress = REPLAY_LOAD_PROGRESS_CHECK_LOAD;
		break;

	case REPLAY_LOAD_PROGRESS_CHECK_LOAD :
		{
			switch (CSavegameLoad::GenericLoad())
			{
			case MEM_CARD_COMPLETE:
				{
					camInterface::FadeOut(0);  // THIS FADE IS OK AS WE ARE STARTING A NEW GAME or loading

					CGame::HandleLoadedSaveGame();
					m_Status = MEM_CARD_COMPLETE;
					return true;
				}
//				break;
			case MEM_CARD_BUSY: // the load is still in progress
				break;
			case MEM_CARD_ERROR:
				{
					// allow the player to choose a different slot
					//	Try the following rather than calling BeginSaveGameList() here
					//m_ManualLoadProgress = MANUAL_LOAD_PROGRESS_BEGIN_SAVEGAME_LIST;

					CGame::SetStateToIdle();

					camInterface::FadeOut(0);  // THIS FADE IS OK AS WE ARE STARTING A NEW GAME or loading
					CGame::StartNewGame(CGameLogic::GetCurrentLevelIndex());

					m_Status = MEM_CARD_ERROR;
					return true;
				}
//				break;
			}
		}
		break;
	}

	return false;
}



void CSavegameQueuedOperation_ReplaySave::Init()
{
	gnetAssertf( !NetworkInterface::IsGameInProgress(), "PLEASE YOU CAN NOT MAKE A SINGLE PLAYER SAVE WHILE IN MULTIPLAYER - THIS WILL BREAK THE SINGLE PLAYER SAVE" );
	m_AutosaveProgress = REPLAY_SAVE_PROGRESS_CREATE_SAVE_BUFFER;
}

//	Return true to Pop() from queue
bool CSavegameQueuedOperation_ReplaySave::Update()
{
	switch (m_AutosaveProgress)
	{
	case REPLAY_SAVE_PROGRESS_CREATE_SAVE_BUFFER :
		{
			CSavegameDialogScreens::SetDialogBoxHeading("SG_HDNG");	//	SG_HDG_SV_RPL

			MemoryCardError CalculateSizeStatus = CGenericGameStorage::CreateSaveDataAndCalculateSize(SAVEGAME_SINGLE_PLAYER, false);
			switch (CalculateSizeStatus)
			{
			case MEM_CARD_COMPLETE :
				Assertf(CGenericGameStorage::GetBufferSize(SAVEGAME_SINGLE_PLAYER) > 0, "CSavegameQueuedOperation_ReplaySave::Update - size of save game buffer has not been calculated");
				CSavegameInitialChecks::SetSizeOfBufferToBeSaved(CGenericGameStorage::GetBufferSize(SAVEGAME_SINGLE_PLAYER));
				CSavegameSave::BeginGameSave(INDEX_OF_REPLAYSAVE_SLOT, SAVEGAME_SINGLE_PLAYER);
				m_AutosaveProgress = REPLAY_SAVE_PROGRESS_CHECK_SAVE;
				break;

			case MEM_CARD_BUSY :
				// Don't do anything - still calculating size - it might be that we're waiting for memory to be available to create the save data
				break;

			case MEM_CARD_ERROR :
				//					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_ALLOCATE_MEMORY_FAILED);
				//					return MEM_CARD_ERROR;
				Assertf(0, "CSavegameQueuedOperation_ReplaySave::Update - CGenericGameStorage::CreateSaveDataAndCalculateSize failed");
				//					CPauseMenu::Close();
				m_Status = MEM_CARD_ERROR;
				break;
			}
		}
		break;

	case REPLAY_SAVE_PROGRESS_CHECK_SAVE :
		switch (CSavegameSave::GenericSave())
		{
		case MEM_CARD_COMPLETE :
			m_Status = MEM_CARD_COMPLETE;
			break;

		case MEM_CARD_BUSY :	 // the save is still in progress
			break;

		case MEM_CARD_ERROR :
			m_Status = MEM_CARD_ERROR;
			break;
		}
		break;
	}

	if ( (m_Status == MEM_CARD_ERROR) || (m_Status == MEM_CARD_COMPLETE) )
	{
		CGenericGameStorage::FreeBufferContainingContentsOfSavegameFile(SAVEGAME_SINGLE_PLAYER);
		CGenericGameStorage::FreeAllDataToBeSaved(SAVEGAME_SINGLE_PLAYER);
		return true;
	}

	return false;
}
#endif	//	USE_SAVE_SYSTEM

// TransferReplayFile ************************************************************************************************

#if RSG_ORBIS

void CSavegameQueuedOperation_TransferReplayFile::Init(const char* fileName, void* pSrc, u32 bufferSize, bool UNUSED_PARAM(saveIcon), bool overwrite)
{
	safecpy(m_FileName, fileName, REPLAYPATHLENGTH);

	m_localIndex = CSavegameUsers::GetUser();
	
	//ensure any mounts display name uses the 
	//.clip suffix
	atString displayName(fileName);
	displayName.Replace(".jpg", ".clip");
	safecpy(m_DisplayName, displayName.c_str(), REPLAYPATHLENGTH);

	m_Buffer = pSrc;
	m_BufferSize = bufferSize;

	m_OverWrite = overwrite;

	m_Progress = REPLAY_OP_PROGRESS_BEGIN_TRANSFER;
}

bool CSavegameQueuedOperation_TransferReplayFile::Update()
{
	switch(m_Progress)
	{	
	case REPLAY_OP_PROGRESS_BEGIN_TRANSFER:
		{
			CSavegameDialogScreens::SetDialogBoxHeading("SG_HDNG");	//	SG_HDG_PROF_SV	//	Not sure if this will ever be displayed

			if (!SAVEGAME.BeginSave(m_localIndex, CGenericGameStorage::GetContentType(), m_DisplayName, m_FileName, (void*)m_Buffer, m_BufferSize, m_OverWrite) )	//	bool overwrite)
			{
				Warningf("ReplayClip::StartWrite - BeginSave failed\n");
				SAVEGAME.EndSave(m_localIndex);
				SAVEGAME.SetStateToIdle(m_localIndex);
				m_Status = MEM_CARD_ERROR;
				return true;
			}

			m_Progress = REPLAY_OP_PROGRESS_CHECK_TRANSFER;
			break;
		}
	case REPLAY_OP_PROGRESS_CHECK_TRANSFER:
		{
			bool bOutIsValid;
			bool bFileExists;

			if (SAVEGAME.CheckSave(m_localIndex, bOutIsValid, bFileExists))
			{
				SAVEGAME.EndSave(m_localIndex);
				
				if (SAVEGAME.GetState(m_localIndex) == fiSaveGameState::HAD_ERROR || !bOutIsValid)
				{
					SAVEGAME.SetStateToIdle(CSavegameUsers::GetUser());

					m_Status = MEM_CARD_ERROR;
					return true;
				}
				
				m_Status = MEM_CARD_COMPLETE;
				return true;
			}
			break;
		}
	}

	return false;
}

#endif // RSG_ORBIS

// End of TransferReplayFile ****************************************************************************************

// EnumerateReplayFile *********************************************************************************************

void CSavegameQueuedOperation_EnumerateReplayFiles::Init(eReplayEnumerationTypes enumType, FileDataStorage* dataStorage, const char* filter)
{
	m_Enumtype = enumType;
	safecpy(m_Filter, filter, REPLAYPATHLENGTH);
	m_Progress = REPLAY_OP_PROGRESS_BEGIN_ENUMERATE;
	m_FileList = dataStorage;
}

//    Return true to Pop() from queue
bool CSavegameQueuedOperation_EnumerateReplayFiles::Update()
{
	switch(m_Progress)
	{
	case REPLAY_OP_PROGRESS_BEGIN_ENUMERATE:
		{
#if RSG_ORBIS
			// Worth running this before we enumerate files
			// to make sure this local user Id is still the same for the PSN user
			if(ReplayFileManager::UpdateUserIDDirectoriesOnChange())
#endif
			{
				if ( !ReplayFileManager::EnumerateFiles(m_Enumtype, *m_FileList, m_Filter) )
				{
					savegameDisplayf("CSavegameQueuedOperation_EnumerateReplayFiles::Update - BEGIN_ENUMERATE failed - FileOp is already running ...keep trying until it is free");
				}
				else
				{
					m_Progress = REPLAY_OP_PROGRESS_CHECK_ENUMERATE;
				}
			}
			break;
		}
	case REPLAY_OP_PROGRESS_CHECK_ENUMERATE:
		{
			if( !ReplayFileManager::IsPerformingFileOp() )
			{
				if( ReplayFileManager::GetLastErrorCode() == REPLAY_ERROR_SUCCESS )
				{
					savegameDisplayf("CSavegameQueuedOperation_EnumerateReplayFiles::Update - CHECK_ENUMERATE succeeded");
					m_Status = MEM_CARD_COMPLETE;
				}
				else
				{
					savegameDisplayf("CSavegameQueuedOperation_EnumerateReplayFiles::Update - CHECK_ENUMERATE failed");
					m_Status = MEM_CARD_ERROR;
				}
			}
			break;
		}
	}

	if ( (m_Status == MEM_CARD_ERROR) || (m_Status == MEM_CARD_COMPLETE) )
	{
		if (m_Status == MEM_CARD_COMPLETE)
		{
			savegameDisplayf("CSavegameQueuedOperation_EnumerateReplayFiles::Update - succeeded");
		}
		else
		{
			savegameDisplayf("CSavegameQueuedOperation_EnumerateReplayFiles::Update - failed");
		}
		return true;
	}

	return false;
}

// End of EnumerateReplayFiles ****************************************************************************************

// ReplayUpdateFavourites *********************************************************************************************

void CSavegameQueuedOperation_ReplayUpdateFavourites::Init()
{
	m_Progress = REPLAY_OP_PROGRESS_BEGIN_UPDATE_FAVOURITES;
}

//    Return true to Pop() from queue
bool CSavegameQueuedOperation_ReplayUpdateFavourites::Update()
{
	switch(m_Progress)
	{
	case REPLAY_OP_PROGRESS_BEGIN_UPDATE_FAVOURITES:
		{
			if ( !ReplayFileManager::UpdateFavourites() )
			{
				savegameDisplayf("CSavegameQueuedOperation_ReplayUpdateFavourites::Update - REPLAY_OP_PROGRESS_BEGIN_UPDATE_FAVOURITES failed");
				m_Status = MEM_CARD_ERROR;
			}
			else
			{
				m_Progress = REPLAY_OP_PROGRESS_CHECK_UPDATE_FAVOURITES;
			}
			break;
		}
	case REPLAY_OP_PROGRESS_CHECK_UPDATE_FAVOURITES:
		{
			if( !ReplayFileManager::IsPerformingFileOp() )
			{
				if( ReplayFileManager::GetLastErrorCode() == REPLAY_ERROR_SUCCESS )
				{
					savegameDisplayf("CSavegameQueuedOperation_ReplayUpdateFavourites::Update - REPLAY_OP_PROGRESS_CHECK_UPDATE_FAVOURITES succeeded");
					m_Status = MEM_CARD_COMPLETE;
				}
				else
				{
					savegameDisplayf("CSavegameQueuedOperation_ReplayUpdateFavourites::Update - REPLAY_OP_PROGRESS_CHECK_UPDATE_FAVOURITES failed");
					m_Status = MEM_CARD_ERROR;
				}

				return true;
			}
			break;
		}
	}

	return false;
}

// End of ReplayUpdateFavourites **************************************************************************************

// ReplayMultiDelete *********************************************************************************************

void CSavegameQueuedOperation_ReplayMultiDelete::Init(const char* filter)
{
	m_Progress = REPLAY_OP_PROGRESS_BEGIN_REPLAY_MULTI_DELETE;
	safecpy(m_Filter, filter, REPLAYPATHLENGTH);
}

//    Return true to Pop() from queue
bool CSavegameQueuedOperation_ReplayMultiDelete::Update()
{
	switch(m_Progress)
	{
	case REPLAY_OP_PROGRESS_BEGIN_REPLAY_MULTI_DELETE:
		{
			if ( !ReplayFileManager::ProcessMultiDelete(m_Filter) )
			{
				savegameDisplayf("CSavegameQueuedOperation_ReplayMultiDelete::Update - REPLAY_OP_PROGRESS_BEGIN_REPLAY_MULTI_DELETE failed");
				m_Status = MEM_CARD_ERROR;
			}
			else
			{
				m_Progress = REPLAY_OP_PROGRESS_CHECK_REPLAY_MULTI_DELETE;
			}
			break;
		}
	case REPLAY_OP_PROGRESS_CHECK_REPLAY_MULTI_DELETE:
		{
			if( !ReplayFileManager::IsPerformingFileOp() )
			{
				if( ReplayFileManager::GetLastErrorCode() == REPLAY_ERROR_SUCCESS )
				{
					savegameDisplayf("CSavegameQueuedOperation_ReplayMultiDelete::Update - REPLAY_OP_PROGRESS_CHECK_REPLAY_MULTI_DELETE succeeded");
					m_Status = MEM_CARD_COMPLETE;
				}
				else
				{
					savegameDisplayf("CSavegameQueuedOperation_ReplayMultiDelete::Update - REPLAY_OP_PROGRESS_CHECK_REPLAY_MULTI_DELETE failed");
					m_Status = MEM_CARD_ERROR;
				}

				return true;
			}
			break;
		}
	}

	return false;
}

// End of ReplayMultiDelete **************************************************************************************


// DeleteReplayClip *********************************************************************************************

void CSavegameQueuedOperation_DeleteReplayClips::Init(const char* filePath)
{
	m_Progress = REPLAY_OP_PROGRESS_BEGIN_DELETE_CLIP;

	safecpy(m_Path, filePath, REPLAYPATHLENGTH);
}

//    Return true to Pop() from queue
bool CSavegameQueuedOperation_DeleteReplayClips::Update()
{
	switch(m_Progress)
	{
	case REPLAY_OP_PROGRESS_BEGIN_DELETE_CLIP:
		{
			if( !ReplayFileManager::DeleteFile(m_Path) )
			{
				savegameDisplayf("CSavegameQueuedOperation_DeleteReplayClips::Update - BEGIN_DELETE_CLIP failed");
				m_Status = MEM_CARD_ERROR;
			}
			else
			{
				m_Progress = REPLAY_OP_PROGRESS_CHECK_DELETE_CLIP;
			}
			break;
		}
	case REPLAY_OP_PROGRESS_CHECK_DELETE_CLIP:
		{
			if( !ReplayFileManager::IsPerformingFileOp() )
			{
				if( ReplayFileManager::GetLastErrorCode() == REPLAY_ERROR_SUCCESS )
				{
					savegameDisplayf("CSavegameQueuedOperation_DeleteReplayClips::Update - CHECK_DELETE_CLIP succeeded");
					
					//continue to the next file
					m_Progress = REPLAY_OP_PROGRESS_BEGIN_DELETE_JPG;
				}
				else
				{
					savegameDisplayf("CSavegameQueuedOperation_DeleteReplayClips::Update - CHECK_DELETE_CLIP failed");
					m_Status = MEM_CARD_ERROR;
				}
			}
			break;
		}
	case REPLAY_OP_PROGRESS_BEGIN_DELETE_JPG:
		{
			atString path(m_Path);
			path.Replace(".clip", ".jpg");

			if( !ReplayFileManager::DeleteFile(path.c_str()) )
			{
				savegameDisplayf("CSavegameQueuedOperation_DeleteReplayClips::Update - BEGIN_DELETE_JPG failed");
				m_Status = MEM_CARD_ERROR;
			}
			else
			{
				m_Progress = REPLAY_OP_PROGRESS_CHECK_DELETE_JPG;
			}
			break;
		}
	case REPLAY_OP_PROGRESS_CHECK_DELETE_JPG:
		{
			if( !ReplayFileManager::IsPerformingFileOp() )
			{
				if( ReplayFileManager::GetLastErrorCode() == REPLAY_ERROR_SUCCESS )
				{
					savegameDisplayf("CSavegameQueuedOperation_DeleteReplayClips::Update - CHECK_DELETE_JPG succeeded");
				}
				else
				{
					savegameDisplayf("CSavegameQueuedOperation_DeleteReplayClips::Update - CHECK_DELETE_JPG failed");
				}

#if RSG_PC
				// try to delete any logfile
				m_Progress = REPLAY_OP_PROGRESS_BEGIN_DELETE_LOGFILE;
#else

#if DO_REPLAY_OUTPUT_XML
				//continue to the next file
				m_Progress = REPLAY_OP_PROGRESS_BEGIN_DELETE_XML;
#else
				m_Status = MEM_CARD_COMPLETE;
#endif

#endif	//RSG_PC
				
			}
			break;
		}

	case REPLAY_OP_PROGRESS_BEGIN_DELETE_LOGFILE:
		{
			atString path(m_Path);
			path.Replace(".clip", ".txt");

			if( !ReplayFileManager::DeleteFile(path.c_str()) )
			{

#if DO_REPLAY_OUTPUT_XML
				//continue to the next file
				m_Progress = REPLAY_OP_PROGRESS_BEGIN_DELETE_XML;
#else
				m_Status = MEM_CARD_ERROR;
#endif
			}
			else
			{
				m_Progress = REPLAY_OP_PROGRESS_CHECK_DELETE_LOGFILE;
			}
			break;
		}
	case REPLAY_OP_PROGRESS_CHECK_DELETE_LOGFILE:
		{
			if( !ReplayFileManager::IsPerformingFileOp() )
			{
				if( ReplayFileManager::GetLastErrorCode() == REPLAY_ERROR_SUCCESS )
				{
					savegameDisplayf("CSavegameQueuedOperation_DeleteReplayClips::Update - CHECK_DELETE_LOGFILE succeeded");
				}
				else
				{
					savegameDisplayf("CSavegameQueuedOperation_DeleteReplayClips::Update - CHECK_DELETE_LOGFILE failed");
				}

#if DO_REPLAY_OUTPUT_XML
				//continue to the next file
				m_Progress = REPLAY_OP_PROGRESS_BEGIN_DELETE_XML;
#else
				m_Status = MEM_CARD_COMPLETE;
#endif

			}
			break;
		}


#if DO_REPLAY_OUTPUT_XML
	case REPLAY_OP_PROGRESS_BEGIN_DELETE_XML:
		{
			atString path(m_Path);
			path.Replace(".clip", ".xml");

			if( !ReplayFileManager::DeleteFile(path.c_str()) )
			{
				savegameDisplayf("CSavegameQueuedOperation_DeleteReplayClips::Update - BEGIN_DELETE_XML failed");
				m_Status = MEM_CARD_ERROR;
			}
			else
			{
				m_Progress = REPLAY_OP_PROGRESS_CHECK_DELETE_XML;
			}
			break;
		}
	case REPLAY_OP_PROGRESS_CHECK_DELETE_XML:
		{
			if( !ReplayFileManager::IsPerformingFileOp() )
			{
				if( ReplayFileManager::GetLastErrorCode() == REPLAY_ERROR_SUCCESS )
				{
					savegameDisplayf("CSavegameQueuedOperation_DeleteReplayClips::Update - CHECK_DELETE_XML succeeded");
				}
				else
				{
					savegameDisplayf("CSavegameQueuedOperation_DeleteReplayClips::Update - CHECK_DELETE_XML failed- in ReplayDebug on PC, this XML file  may have not existed");
				}

				// always return a success, as debug XML file doesn't always exist
				m_Status = MEM_CARD_COMPLETE;
			}
			break;
		}
#endif
	}

	if ( (m_Status == MEM_CARD_ERROR) || (m_Status == MEM_CARD_COMPLETE) )
	{
		// if any of the main stages fails, it quits out with MEM_CARD_ERROR ...which brings up the prompt saying it didn't work
		if (m_Status == MEM_CARD_COMPLETE)
		{
			savegameDisplayf("CSavegameQueuedOperation_DeleteReplayClips::Update - success - MEM_CARD_COMPLETE");
		}
		else
		{
			savegameDisplayf("CSavegameQueuedOperation_DeleteReplayClips::Update -  fail - MEM_CARD_ERROR");
		}
		return true;
	}

	return false;
}

// End of DeleteReplayClip **************************************************************************************

// DeleteReplayFile *********************************************************************************************

void CSavegameQueuedOperation_DeleteReplayFile::Init(const char* filePath)
{
	m_Progress = REPLAY_OP_PROGRESS_BEGIN_DELETE_FILE;
	safecpy(m_Path, filePath, REPLAYPATHLENGTH);
}

//    Return true to Pop() from queue
bool CSavegameQueuedOperation_DeleteReplayFile::Update()
{
	switch(m_Progress)
	{
	case REPLAY_OP_PROGRESS_BEGIN_DELETE_FILE:
		{
			if( !ReplayFileManager::DeleteFile(m_Path) )
			{
				m_Status = MEM_CARD_ERROR;
				return true;
			}

			m_Progress = REPLAY_OP_PROGRESS_CHECK_DELETE_FILE;
			break;
		}
	case REPLAY_OP_PROGRESS_CHECK_DELETE_FILE:
		{
			if( !ReplayFileManager::IsPerformingFileOp() )
			{
				if( ReplayFileManager::GetLastErrorCode() == REPLAY_ERROR_SUCCESS )
				{
					savegameDisplayf("CSavegameQueuedOperation_DeleteReplayFile::Update - succeeded");
					m_Status = MEM_CARD_COMPLETE;
				}
				else
				{
					savegameDisplayf("CSavegameQueuedOperation_DeleteReplayFile::Update - failed");
					m_Status = MEM_CARD_ERROR;
				}
				return true;
			}
			break;
		}
	}

	return false;
}

// End of DeleteReplayFile ************************************************************************************

// LoadReplayClip *********************************************************************************************

void CSavegameQueuedOperation_LoadReplayClip::Init(const char* filePath, CBufferInfo& bufferInfo, tLoadStartFunc loadStartFunc)
{
	safecpy(m_Path, filePath, REPLAYPATHLENGTH);
	m_BufferInfo = &bufferInfo;
	m_LoadStartFunc = loadStartFunc;
	m_Progress = REPLAY_OP_PROGRESS_BEGIN_LOAD;
}

//    Return true to Pop() from queue
bool CSavegameQueuedOperation_LoadReplayClip::Update()
{
	switch(m_Progress)
	{
	case REPLAY_OP_PROGRESS_BEGIN_LOAD:
		{
			if( !ReplayFileManager::StartLoad(m_Path, m_BufferInfo, m_LoadStartFunc) )
			{
				m_Status = MEM_CARD_ERROR;
				return true;
			}

			m_Progress = REPLAY_OP_PROGRESS_CHECK_LOAD;
			break;
		}
	case REPLAY_OP_PROGRESS_CHECK_LOAD:
		{
			if( !ReplayFileManager::IsPerformingFileOp() )
			{
				m_Status = MEM_CARD_ERROR;

				if( ReplayFileManager::GetLastErrorCode() == REPLAY_ERROR_SUCCESS )
				{
					m_Status = MEM_CARD_COMPLETE;
				}

				return true;
			}
		}
	}

	return false;
}

// End of LoadReplayClip ************************************************************************************

// LoadMontage *********************************************************************************************

void CSavegameQueuedOperation_LoadMontage::Init(const char* filePath, CMontage* montage)
{
	safecpy(m_Path, filePath, REPLAYPATHLENGTH);
	m_Montage = montage;
	m_Progress = REPLAY_OP_PROGRESS_BEGIN_LOAD;
}

//    Return true to Pop() from queue
bool CSavegameQueuedOperation_LoadMontage::Update()
{
	switch(m_Progress)
	{
	case REPLAY_OP_PROGRESS_BEGIN_LOAD:
		{
			if( !ReplayFileManager::LoadMontage(m_Path, m_Montage) )
			{
				m_Status = MEM_CARD_ERROR;
				return true;
			}

			m_Progress = REPLAY_OP_PROGRESS_CHECK_LOAD;
			break;
		}
	case REPLAY_OP_PROGRESS_CHECK_LOAD:
		{
			if( !ReplayFileManager::IsPerformingFileOp() )
			{
				m_Status = MEM_CARD_ERROR;

				if( ReplayFileManager::GetLastErrorCode() == REPLAY_ERROR_SUCCESS )
				{
					m_Status = MEM_CARD_COMPLETE;
				}

				return true;
			}
		}
	}

	return false;
}

// End of LoadMontage ************************************************************************************

// SaveMontage *********************************************************************************************

void CSavegameQueuedOperation_SaveMontage::Init(const char* filePath, CMontage* montage)
{
	safecpy(m_Path, filePath, REPLAYPATHLENGTH);
	m_Montage = montage;
	m_Progress = REPLAY_OP_PROGRESS_BEGIN_SAVE;
}

//    Return true to Pop() from queue
bool CSavegameQueuedOperation_SaveMontage::Update()
{
	switch(m_Progress)
	{
	case REPLAY_OP_PROGRESS_BEGIN_SAVE:
		{
			if( !ReplayFileManager::SaveMontage(m_Path, m_Montage) )
			{
				m_Status = MEM_CARD_ERROR;
				return true;
			}

			m_Progress = REPLAY_OP_PROGRESS_CHECK_SAVE;
			break;
		}
	case REPLAY_OP_PROGRESS_CHECK_SAVE:
		{
			if( !ReplayFileManager::IsPerformingFileOp() )
			{
				m_Status = MEM_CARD_ERROR;

				if( ReplayFileManager::GetLastErrorCode() == REPLAY_ERROR_SUCCESS )
				{
					m_Status = MEM_CARD_COMPLETE;
				}

				return true;
			}
		}
	}

	return false;
}

// End of LoadMontage ************************************************************************************

// LoadReplayHeader *****************************************************************************************

void CSavegameQueuedOperation_LoadReplayHeader::Init(const char* filepath)
{
	// REPLAY_CLIPS_PATH change
	char path[REPLAYPATHLENGTH];
	ReplayFileManager::getClipDirectory(path);
	formatf(m_Path, REPLAYPATHLENGTH, "%s%s", path, filepath);
	
	m_Progress = REPLAY_OP_PROGRESS_BEGIN_LOAD;
}

//    Return true to Pop() from queue
bool CSavegameQueuedOperation_LoadReplayHeader::Update()
{
	switch(m_Progress)
	{
	case REPLAY_OP_PROGRESS_BEGIN_LOAD:
		{
			if( !ReplayFileManager::StartLoadReplayHeader(m_Path) )
			{
				m_Status = MEM_CARD_ERROR;
				return true;
			}

			m_Progress = REPLAY_OP_PROGRESS_CHECK_LOAD;
			break;
		}
	case REPLAY_OP_PROGRESS_CHECK_LOAD:
		{
			if( !ReplayFileManager::IsPerformingFileOp() )
			{
				m_Header = ReplayFileManager::GetHeader(m_Path);

				m_Status = MEM_CARD_ERROR;

				if( m_Header != NULL )
				{
					m_Status = MEM_CARD_COMPLETE;
				}

				return true;
			}
			break;
		}
	}

	return false;
}

// End of LoadReplayHeader ***************************************************************************************

#endif // GTA_REPLAY

// MissionRepeatLoad *********************************************************************************************

void CSavegameQueuedOperation_MissionRepeatLoad::Init()
{
	m_MissionRepeatLoadProgress = MISSION_REPEAT_LOAD_PROGRESS_PRE_LOAD;
}

//	Return true to Pop() from queue
bool CSavegameQueuedOperation_MissionRepeatLoad::Update()
{
	switch (m_MissionRepeatLoadProgress)
	{
		case MISSION_REPEAT_LOAD_PROGRESS_PRE_LOAD :

#if GTA_REPLAY
			if(CReplayMgr::IsSaving())
			{
				return false;
			}
#endif // GTA_REPLAY

			m_MissionRepeatLoadProgress = MISSION_REPEAT_LOAD_PROGRESS_BEGIN_LOAD;
			break;
		case MISSION_REPEAT_LOAD_PROGRESS_BEGIN_LOAD :
			CSavegameDialogScreens::SetDialogBoxHeading("SG_HDNG");	//	SG_HDG_LD_RPT

			if (MEM_CARD_ERROR == CSavegameList::BeginGameLoad(INDEX_OF_MISSION_REPEAT_SAVE_SLOT))
			{
				CTheScripts::GetMissionReplayStats().ClearReplayStatsStructure();	//	It doesn't make sense to apply any improved stats for the replayed mission if we've been forced to start a new session

				CGame::SetStateToIdle();

				camInterface::FadeOut(0);  // THIS FADE IS OK AS WE ARE STARTING A NEW GAME or loading
				CGame::StartNewGame(CGameLogic::GetCurrentLevelIndex());

				m_Status = MEM_CARD_ERROR;
				return true;
			}

			camInterface::FadeOut(0);  // fade out the screen
			CGame::LoadSaveGame();

			m_MissionRepeatLoadProgress = MISSION_REPEAT_LOAD_PROGRESS_CHECK_LOAD;
			break;

		case MISSION_REPEAT_LOAD_PROGRESS_CHECK_LOAD :
			{
				switch (CSavegameLoad::GenericLoad())
				{
				case MEM_CARD_COMPLETE:
					{
						camInterface::FadeOut(0);  // THIS FADE IS OK AS WE ARE STARTING A NEW GAME or loading

						CGame::HandleLoadedSaveGame();
						m_Status = MEM_CARD_COMPLETE;
						return true;
					}
//					break;
				case MEM_CARD_BUSY: // the load is still in progress
					break;
				case MEM_CARD_ERROR:
					{
						CTheScripts::GetMissionReplayStats().ClearReplayStatsStructure();	//	It doesn't make sense to apply any improved stats for the replayed mission if we've been forced to start a new session

						CGame::SetStateToIdle();

						camInterface::FadeOut(0);  // THIS FADE IS OK AS WE ARE STARTING A NEW GAME or loading
						CGame::StartNewGame(CGameLogic::GetCurrentLevelIndex());

						m_Status = MEM_CARD_ERROR;
						return true;
					}
//					break;
				}
			}
			break;
	}

	return false;
}


// MissionRepeatSave *********************************************************************************************

void CSavegameQueuedOperation_MissionRepeatSave::Init(bool bBenchmarkTest)
{
	m_MissionRepeatSaveProgress = MISSION_REPEAT_SAVE_PROGRESS_DISPLAY_WARNING_SCREEN;
	m_bBenchmarkTest = bBenchmarkTest;
}

//	Return true to Pop() from queue
bool CSavegameQueuedOperation_MissionRepeatSave::Update()
{
	switch (m_MissionRepeatSaveProgress)
	{
		case MISSION_REPEAT_SAVE_PROGRESS_DISPLAY_WARNING_SCREEN :
		{
			CSavegameDialogScreens::SetDialogBoxHeading("SG_HDNG");	//	SG_HDG_SV_RPT

			//	Only display this warning if the player is signed in and has chosen a device?
			//	That would avoid multiple warning screens

			bool bReturnSelection = false;

			SaveGameDialogCode messageToDisplay = SAVE_GAME_DIALOG_MISSION_REPEAT_MAKES_A_SAVE;
			if (m_bBenchmarkTest)
			{
				messageToDisplay = SAVE_GAME_DIALOG_PC_BENCHMARK_TEST_MAKES_A_SAVE;
			}
			if (CSavegameDialogScreens::HandleSaveGameDialogBox(messageToDisplay, 0, &bReturnSelection))
			{
				if (bReturnSelection)
				{
					m_MissionRepeatSaveProgress = MISSION_REPEAT_SAVE_PROGRESS_CREATE_SAVE_BUFFER;
				}
				else
				{
					m_Status = MEM_CARD_ERROR;
				}
			}
		}
		break;

		case MISSION_REPEAT_SAVE_PROGRESS_CREATE_SAVE_BUFFER :
		{
			MemoryCardError CalculateSizeStatus = CGenericGameStorage::CreateSaveDataAndCalculateSize(SAVEGAME_SINGLE_PLAYER, false);
			switch (CalculateSizeStatus)
			{
			case MEM_CARD_COMPLETE :
				Assertf(CGenericGameStorage::GetBufferSize(SAVEGAME_SINGLE_PLAYER) > 0, "CSavegameQueuedOperation_MissionRepeatSave::Update - size of save game buffer has not been calculated");
				CSavegameInitialChecks::SetSizeOfBufferToBeSaved(CGenericGameStorage::GetBufferSize(SAVEGAME_SINGLE_PLAYER));
				CSavegameSave::BeginGameSave(INDEX_OF_MISSION_REPEAT_SAVE_SLOT, SAVEGAME_SINGLE_PLAYER);
				m_MissionRepeatSaveProgress = MISSION_REPEAT_SAVE_PROGRESS_CHECK_SAVE;
				break;

			case MEM_CARD_BUSY :
				// Don't do anything - still calculating size - it might be that we're waiting for memory to be available to create the save data
				break;

			case MEM_CARD_ERROR :
				//					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_ALLOCATE_MEMORY_FAILED);
				//					return MEM_CARD_ERROR;
				Assertf(0, "CSavegameQueuedOperation_MissionRepeatSave::Update - CGenericGameStorage::CreateSaveDataAndCalculateSize failed");
				//					CPauseMenu::Close();
				m_Status = MEM_CARD_ERROR;
				break;
			}
		}
		break;

		case MISSION_REPEAT_SAVE_PROGRESS_CHECK_SAVE :
			switch (CSavegameSave::GenericSave())
			{
			case MEM_CARD_COMPLETE :
				m_Status = MEM_CARD_COMPLETE;
				break;

			case MEM_CARD_BUSY :	 // the save is still in progress
				break;

			case MEM_CARD_ERROR :
				m_Status = MEM_CARD_ERROR;
				break;
			}
			break;
	}

	if ( (m_Status == MEM_CARD_ERROR) || (m_Status == MEM_CARD_COMPLETE) )
	{
		CGenericGameStorage::FreeBufferContainingContentsOfSavegameFile(SAVEGAME_SINGLE_PLAYER);
		CGenericGameStorage::FreeAllDataToBeSaved(SAVEGAME_SINGLE_PLAYER);
		return true;
	}

	return false;
}


// CheckFileExists *********************************************************************************************

void CSavegameQueuedOperation_CheckFileExists::Init(s32 SlotIndex)
{
	if (savegameVerifyf( (SlotIndex >= 0) && (SlotIndex < MAX_NUM_EXPECTED_SAVE_FILES), 
		"CSavegameQueuedOperation_CheckFileExists::Init - Slot Index %d is out of the range 0 to %d", 
		SlotIndex, MAX_NUM_EXPECTED_SAVE_FILES))
	{
		m_SlotIndex = SlotIndex;
		m_bFileExists = false;
		m_bFileIsDamaged = false;
		m_CheckFileExistsProgress = CHECK_FILE_EXISTS_PROGRESS_BEGIN;
	}
}

//    Return true to Pop() from queue
bool CSavegameQueuedOperation_CheckFileExists::Update()
{
	switch (m_CheckFileExistsProgress)
	{
		case CHECK_FILE_EXISTS_PROGRESS_BEGIN :
			CSavegameDialogScreens::SetDialogBoxHeading("SG_HDNG");	//	SG_HDG_EXISTS	//	Graeme - I don't think this is actually used just now. Nothing calls CGenericGameStorage::QueueCheckFileExists()

			switch (CSavegameCheckFileExists::BeginCheckFileExists(m_SlotIndex))
			{
				case MEM_CARD_COMPLETE :
					m_CheckFileExistsProgress = CHECK_FILE_EXISTS_PROGRESS_CHECK;
					break;
				case MEM_CARD_BUSY :
					savegameAssertf(0, "CSavegameQueuedOperation_CheckFileExists::Update - didn't expect CSavegameCheckFileExists::BeginCheckFileExists() to ever return MEM_CARD_BUSY");
					m_Status = MEM_CARD_ERROR;
					break;
				case MEM_CARD_ERROR :
					savegameAssertf(0, "CSavegameQueuedOperation_CheckFileExists::Update - CSavegameCheckFileExists::BeginCheckFileExists() returned MEM_CARD_ERROR");
					m_Status = MEM_CARD_ERROR;
					break;
			}
			break;
		case CHECK_FILE_EXISTS_PROGRESS_CHECK :
			switch (CSavegameCheckFileExists::CheckFileExists())
			{
			case MEM_CARD_COMPLETE :
				CSavegameCheckFileExists::GetFileStatus(m_bFileExists, m_bFileIsDamaged);
				savegameDebugf1("CSavegameQueuedOperation_CheckFileExists::Update - completed for slot %d. Time = %d. Slot %s. Slot is %sdamaged\n", m_SlotIndex, sysTimer::GetSystemMsTime(), m_bFileExists?"exists":"doesn't exist", m_bFileIsDamaged?"":"not ");
				m_Status = MEM_CARD_COMPLETE;
				break;

			case MEM_CARD_BUSY :    // the load is still in progress
				break;

			case MEM_CARD_ERROR :
				m_bFileExists = false;
				m_bFileIsDamaged = false;
				savegameDebugf1("CSavegameQueuedOperation_CheckFileExists::Update - failed for slot %d. Time = %d. Slot %s. Slot is %sdamaged\n", m_SlotIndex, sysTimer::GetSystemMsTime(), m_bFileExists?"exists":"doesn't exist", m_bFileIsDamaged?"":"not ");
				m_Status = MEM_CARD_ERROR;
				break;
			}
			break;
	}

	if ( (m_Status == MEM_CARD_ERROR) || (m_Status == MEM_CARD_COMPLETE) )
	{
		return true;
	}

	return false;
}

// End of CheckFileExists *********************************************************************************************


// CreateSortedListOfSavegames *********************************************************************************************

void CSavegameQueuedOperation_CreateSortedListOfSavegames::Init(bool bDisplayWarningScreens)
{
	m_bDisplayWarningScreens = bDisplayWarningScreens;
	m_CreateSortedListOfSavegamesProgress = CREATE_SORTED_LIST_OF_SAVEGAMES_BEGIN;
}

//    Return true to Pop() from queue
bool CSavegameQueuedOperation_CreateSortedListOfSavegames::Update()
{
	switch (m_CreateSortedListOfSavegamesProgress)
	{
		case CREATE_SORTED_LIST_OF_SAVEGAMES_BEGIN :
			CSavegameDialogScreens::SetDialogBoxHeading("SG_HDNG");

			CGenericGameStorage::SetSaveOperation(OPERATION_ENUMERATING_SAVEGAMES);
			CSavegameInitialChecks::BeginInitialChecks(false);

			m_CreateSortedListOfSavegamesProgress = CREATE_SORTED_LIST_OF_SAVEGAMES_CHECK;
			break;

		case CREATE_SORTED_LIST_OF_SAVEGAMES_CHECK :
			{
				MemoryCardError EnumerationState = CSavegameInitialChecks::InitialChecks();
				if (EnumerationState == MEM_CARD_ERROR)
				{
					if (SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT == CSavegameDialogScreens::GetMostRecentErrorCode())
					{
						photoDisplayf("CSavegameQueuedOperation_CreateSortedListOfSavegames::Update - player has signed out");
						m_Status = MEM_CARD_ERROR;
					}
					else
					{
						photoDisplayf("CSavegameQueuedOperation_CreateSortedListOfSavegames::Update - InitialChecks() returned an error");
						m_CreateSortedListOfSavegamesProgress = CREATE_SORTED_LIST_OF_SAVEGAMES_HANDLE_ERRORS;
					}
				}
				else if (EnumerationState == MEM_CARD_COMPLETE)
				{
					photoDisplayf("CSavegameQueuedOperation_CreateSortedListOfSavegames::Update - InitialChecks() succeeded");

					m_Status = MEM_CARD_COMPLETE;
				}
			}
			break;

		case CREATE_SORTED_LIST_OF_SAVEGAMES_HANDLE_ERRORS :
			if (!m_bDisplayWarningScreens || CSavegameDialogScreens::HandleSaveGameErrorCode())
			{
				CSavegameDialogScreens::ClearSaveGameError();
				m_Status = MEM_CARD_ERROR;
			}
			break;
	}

	if ( (m_Status == MEM_CARD_ERROR) || (m_Status == MEM_CARD_COMPLETE) )
	{
		EndCreatedSortedList();

		return true;
	}

	return false;
}

void CSavegameQueuedOperation_CreateSortedListOfSavegames::EndCreatedSortedList()
{
	photoAssertf(CGenericGameStorage::GetSaveOperation() == OPERATION_ENUMERATING_SAVEGAMES, "CSavegameQueuedOperation_CreateSortedListOfSavegames::EndCreatedSortedList - SaveOperation should be OPERATION_ENUMERATING_SAVEGAMES");
	CGenericGameStorage::SetSaveOperation(OPERATION_NONE);
}


// End of CreateSortedListOfSavegames *********************************************************************************************

#if __LOAD_LOCAL_PHOTOS

void CSavegameQueuedOperation_CreateSortedListOfLocalPhotos::Init(bool bDisplayWarningScreens)
{
	m_bDisplayWarningScreens = bDisplayWarningScreens;

	m_bCloudEnumerationHasBeenStarted = false;
	m_bLocalEnumerationHasBeenStarted = false;

	m_bHadCloudError = false;
	m_bHadLocalError = false;

	m_CreateSortedListOfLocalPhotosProgress = CREATE_SORTED_LIST_OF_LOCAL_PHOTOS_BEGIN_CLOUD_ENUMERATION;
}

//    Return true to Pop() from queue
bool CSavegameQueuedOperation_CreateSortedListOfLocalPhotos::Update()
{
	switch (m_CreateSortedListOfLocalPhotosProgress)
	{
	case CREATE_SORTED_LIST_OF_LOCAL_PHOTOS_BEGIN_CLOUD_ENUMERATION :

		CSavegameDialogScreens::SetDialogBoxHeading("SG_HDNG");

		m_CreateSortedListOfLocalPhotosProgress = CREATE_SORTED_LIST_OF_LOCAL_PHOTOS_BEGIN_LOCAL_ENUMERATION;

		m_bCloudEnumerationHasBeenStarted = false;

		if (CPhotoManager::IsListOfPhotosUpToDate(false))
		{
		}
		else
		{
			if (NetworkInterface::IsCloudAvailable())
			{	//	I need to check IsCloudAvailable() here to prevent a "Cloud not available" Verifyf inside CListOfUGCPhotosOnCloud::RequestListOfDirectoryContents()
				if (CPhotoManager::RequestListOfCloudPhotos())
				{
					m_bCloudEnumerationHasBeenStarted = true;
				}
				else
				{
					photoDisplayf("CSavegameQueuedOperation_CreateSortedListOfLocalPhotos::Update - RequestListOfCloudPhotos() failed");
					
	//	The player should still be allowed to view their local photos even if the cloud photos can't be enumerated here
	//				CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_BEGIN_CLOUD_LOAD_PHOTO_LIST_FAILED);
	//				m_CreateSortedListOfLocalPhotosProgress = CREATE_SORTED_LIST_OF_LOCAL_PHOTOS_HANDLE_ERRORS;
				}
			}
			else
			{
				photoDisplayf("CSavegameQueuedOperation_CreateSortedListOfLocalPhotos::Update - NetworkInterface::IsCloudAvailable() returned false");
			}
		}
		break;
	case CREATE_SORTED_LIST_OF_LOCAL_PHOTOS_BEGIN_LOCAL_ENUMERATION :

		if (CPhotoManager::IsListOfPhotosUpToDate(true))
		{
			m_bLocalEnumerationHasBeenStarted = false;

			if (m_bCloudEnumerationHasBeenStarted)
			{
				m_CreateSortedListOfLocalPhotosProgress = CREATE_SORTED_LIST_OF_LOCAL_PHOTOS_CHECK_CLOUD_ENUMERATION;
			}
			else
			{
				m_Status = MEM_CARD_COMPLETE;
			}
		}
		else
		{
			CGenericGameStorage::SetSaveOperation(OPERATION_ENUMERATING_PHOTOS);
			CSavegameInitialChecks::BeginInitialChecks(false);

			m_bLocalEnumerationHasBeenStarted = true;
			m_CreateSortedListOfLocalPhotosProgress = CREATE_SORTED_LIST_OF_LOCAL_PHOTOS_CHECK_LOCAL_ENUMERATION;
		}
		break;

	case CREATE_SORTED_LIST_OF_LOCAL_PHOTOS_CHECK_LOCAL_ENUMERATION :
		{
			MemoryCardError EnumerationState = CSavegameInitialChecks::InitialChecks();
			if (EnumerationState == MEM_CARD_ERROR)
			{	//	What should I do about the cloud enumeration if the local enumeration fails?
				m_bHadLocalError = true;

				if (SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT == CSavegameDialogScreens::GetMostRecentErrorCode())
				{
					photoDisplayf("CSavegameQueuedOperation_CreateSortedListOfLocalPhotos::Update - player has signed out");
					m_Status = MEM_CARD_ERROR;
				}
				else
				{
					photoDisplayf("CSavegameQueuedOperation_CreateSortedListOfLocalPhotos::Update - InitialChecks() returned an error");
					m_CreateSortedListOfLocalPhotosProgress = CREATE_SORTED_LIST_OF_LOCAL_PHOTOS_HANDLE_ERRORS;
				}
			}
			else if (EnumerationState == MEM_CARD_COMPLETE)
			{
				photoDisplayf("CSavegameQueuedOperation_CreateSortedListOfLocalPhotos::Update - InitialChecks() succeeded");

#if !__NO_OUTPUT
				CSavegamePhotoLocalList::DisplayContentsOfList();
#endif	//	!__NO_OUTPUT

				if (m_bCloudEnumerationHasBeenStarted)
				{
					m_CreateSortedListOfLocalPhotosProgress = CREATE_SORTED_LIST_OF_LOCAL_PHOTOS_CHECK_CLOUD_ENUMERATION;
				}
				else
				{
					m_Status = MEM_CARD_COMPLETE;
				}
			}
		}
		break;

	case CREATE_SORTED_LIST_OF_LOCAL_PHOTOS_CHECK_CLOUD_ENUMERATION :
		photoDisplayf("CSavegameQueuedOperation_CreateSortedListOfLocalPhotos::Update - CHECK_CLOUD_ENUMERATION");
		if (CPhotoManager::GetIsCloudPhotoRequestPending() == false)
		{
			if (CPhotoManager::GetHasCloudPhotoRequestSucceeded())
			{
				photoDisplayf("CSavegameQueuedOperation_CreateSortedListOfLocalPhotos::Update - cloud request has completed");
				CPhotoManager::FillMergedListOfPhotos();
				m_Status = MEM_CARD_COMPLETE;
			}
			else
			{
				if (CPhotoManager::ShouldDisplayCloudPhotoEnumerationError())
				{
					CPhotoManager::SetDisplayCloudPhotoEnumerationError(false);

					photoDisplayf("CSavegameQueuedOperation_CreateSortedListOfLocalPhotos::Update - cloud request has failed");
//					m_Status = MEM_CARD_ERROR;
					m_bHadCloudError = true;
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_CLOUD_LOAD_PHOTO_LIST_FAILED);
					m_CreateSortedListOfLocalPhotosProgress = CREATE_SORTED_LIST_OF_LOCAL_PHOTOS_HANDLE_ERRORS;
				}
				else
				{
					photoDisplayf("CSavegameQueuedOperation_CreateSortedListOfLocalPhotos::Update - cloud request has failed but ShouldDisplayCloudPhotoEnumerationError() returned FALSE so set m_Status to MEM_CARD_COMPLETE");
					m_Status = MEM_CARD_COMPLETE;
				}
			}
		}
		break;

	case CREATE_SORTED_LIST_OF_LOCAL_PHOTOS_HANDLE_ERRORS :
		if (!m_bDisplayWarningScreens || CSavegameDialogScreens::HandleSaveGameErrorCode())
		{
			CSavegameDialogScreens::ClearSaveGameError();
			m_Status = MEM_CARD_ERROR;
		}
		break;
	}

	if ( (m_Status == MEM_CARD_ERROR) || (m_Status == MEM_CARD_COMPLETE) )
	{
		EndCreatedSortedList();

		if (m_Status == MEM_CARD_ERROR)
		{
			if (m_bHadCloudError && !m_bHadLocalError)
			{	//	Allow the player to proceed even if the cloud photos couldn't be read. They should still be able to view their local photos in the gallery.
				//	They shouldn't be allowed to upload them though since we won't be able to tell if they've already been uploaded
				m_Status = MEM_CARD_COMPLETE;
			}
		}

		return true;
	}

	return false;
}

void CSavegameQueuedOperation_CreateSortedListOfLocalPhotos::EndCreatedSortedList()
{
	if (m_bLocalEnumerationHasBeenStarted)
	{
		photoAssertf(CGenericGameStorage::GetSaveOperation() == OPERATION_ENUMERATING_PHOTOS, "CSavegameQueuedOperation_CreateSortedListOfLocalPhotos::EndCreatedSortedList - SaveOperation should be OPERATION_ENUMERATING_PHOTOS");
	}
	CGenericGameStorage::SetSaveOperation(OPERATION_NONE);
}




void CSavegameQueuedOperation_UpdateMetadataOfLocalAndCloudPhoto::Init(const CSavegamePhotoUniqueId &uniquePhotoId, CPhotoBuffer *pPhotoBuffer)
{
	photoDisplayf("CSavegameQueuedOperation_UpdateMetadataOfLocalAndCloudPhoto::Init called for unique photo Id %d", uniquePhotoId.GetValue());

	if (photoVerifyf(pPhotoBuffer, "CSavegameQueuedOperation_UpdateMetadataOfLocalAndCloudPhoto::Init - pPhotoBuffer is NULL"))
	{
		m_UniquePhotoId = uniquePhotoId;
		m_pBufferToStoreLoadedPhoto = pPhotoBuffer;

		m_NewTitle.Reset();

		CMetadataForOnePhoto *pMetadata = CSavegamePhotoLocalMetadata::GetMetadataForUniquePhotoId(m_UniquePhotoId);

		if (photoVerifyf(pMetadata, "CSavegameQueuedOperation_UpdateMetadataOfLocalAndCloudPhoto::Init - failed to find metadata for unique photo Id %d", m_UniquePhotoId.GetValue()))
		{
			m_NewTitle = pMetadata->GetTitleOfPhoto();
		}

		m_UpdateMetadataProgress = UPDATE_METADATA_PROGRESS_BEGIN_LOAD;
	}
}

//    Return true to Pop() from queue
bool CSavegameQueuedOperation_UpdateMetadataOfLocalAndCloudPhoto::Update()
{
	switch (m_UpdateMetadataProgress)
	{
	case UPDATE_METADATA_PROGRESS_BEGIN_LOAD :
		{
			CSavegameDialogScreens::SetDialogBoxHeading("SG_HDNG");

			switch (CSavegamePhotoLoadLocal::BeginLocalPhotoLoad(m_UniquePhotoId, m_pBufferToStoreLoadedPhoto))
			{
			case MEM_CARD_COMPLETE :
				m_UpdateMetadataProgress = UPDATE_METADATA_PROGRESS_CHECK_LOAD;
				break;
			case MEM_CARD_BUSY :
				photoAssertf(0, "CSavegameQueuedOperation_UpdateMetadataOfLocalAndCloudPhoto::Update - didn't expect CSavegamePhotoLoadLocal::BeginLocalPhotoLoad to ever return MEM_CARD_BUSY");
				break;
			case MEM_CARD_ERROR :
				CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_BEGIN_LOCAL_LOAD_JPEG_FAILED);
				m_UpdateMetadataProgress = UPDATE_METADATA_PROGRESS_HANDLE_ERRORS;
				break;
			}
		}
		break;

	case UPDATE_METADATA_PROGRESS_CHECK_LOAD :
		{
			CSavingMessage::BeginDisplaying(CSavingMessage::STORAGE_DEVICE_MESSAGE_CLOUD_LOADING);

			MemoryCardError LoadState = CSavegamePhotoLoadLocal::DoLocalPhotoLoad();
			if (LoadState == MEM_CARD_ERROR)
			{
				if (SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT == CSavegameDialogScreens::GetMostRecentErrorCode())
				{
					photoDisplayf("CSavegameQueuedOperation_UpdateMetadataOfLocalAndCloudPhoto::Update - player has signed out");
					m_Status = MEM_CARD_ERROR;
				}
				else
				{
					photoDisplayf("CSavegameQueuedOperation_UpdateMetadataOfLocalAndCloudPhoto::Update - CSavegamePhotoLoadLocal::DoLocalPhotoLoad() returned an error");
					m_UpdateMetadataProgress = UPDATE_METADATA_PROGRESS_HANDLE_ERRORS;
				}
			}
			else if (LoadState == MEM_CARD_COMPLETE)
			{
				photoDisplayf("CSavegameQueuedOperation_UpdateMetadataOfLocalAndCloudPhoto::Update - CSavegamePhotoLoadLocal::DoLocalPhotoLoad() succeeded. Updating title to %s", m_NewTitle.c_str());

				if (m_pBufferToStoreLoadedPhoto->ReadAndVerifyLoadedBuffer())
				{
					//	We could also overwrite the metadata and description if we need to in the future
					if (m_pBufferToStoreLoadedPhoto->SetTitleString(m_NewTitle.c_str()))
					{
						m_UpdateMetadataProgress = UPDATE_METADATA_PROGRESS_BEGIN_SAVE;
					}
					else
					{
						photoDisplayf("CSavegameQueuedOperation_UpdateMetadataOfLocalAndCloudPhoto::Update - SetTitleString() returned an error");
						photoAssertf(0, "CSavegameQueuedOperation_UpdateMetadataOfLocalAndCloudPhoto::Update - SetTitleString() returned an error");
						m_UpdateMetadataProgress = UPDATE_METADATA_PROGRESS_HANDLE_ERRORS;
					}
				}
				else
				{
					photoDisplayf("CSavegameQueuedOperation_UpdateMetadataOfLocalAndCloudPhoto::Update - ReadAndVerifyLoadedBuffer() returned an error");
					photoAssertf(0, "CSavegameQueuedOperation_UpdateMetadataOfLocalAndCloudPhoto::Update - ReadAndVerifyLoadedBuffer() returned an error");
					m_UpdateMetadataProgress = UPDATE_METADATA_PROGRESS_HANDLE_ERRORS;
				}
			}
		}
		break;

	case UPDATE_METADATA_PROGRESS_BEGIN_SAVE :
		switch (CSavegamePhotoSaveLocal::BeginSavePhotoLocal(m_UniquePhotoId, m_pBufferToStoreLoadedPhoto))
		{
		case MEM_CARD_COMPLETE :
			m_UpdateMetadataProgress = UPDATE_METADATA_PROGRESS_CHECK_SAVE;
			break;
		case MEM_CARD_BUSY :
			savegameAssertf(0, "CSavegameQueuedOperation_UpdateMetadataOfLocalAndCloudPhoto::Update - didn't expect CSavegamePhotoSaveLocal::BeginSavePhotoLocal() to ever return MEM_CARD_BUSY");
			m_Status = MEM_CARD_ERROR;
			break;
		case MEM_CARD_ERROR :
			savegameAssertf(0, "CSavegameQueuedOperation_UpdateMetadataOfLocalAndCloudPhoto::Update - CSavegamePhotoSaveLocal::BeginSavePhotoLocal() returned MEM_CARD_ERROR");
			m_Status = MEM_CARD_ERROR;
			break;
		}
		break;

	case UPDATE_METADATA_PROGRESS_CHECK_SAVE :
		switch (CSavegamePhotoSaveLocal::DoSavePhotoLocal())
		{
		case MEM_CARD_COMPLETE :
//	For some reason on PS4 resaving the photo doesn't lead to a new ModTimeLow when enumerating the photos so the resaved photo maintains its position in the sorted list
#if __UPDATING_LOCAL_PHOTO_TITLE_AFFECTS_MOD_TIME
			//	This photo is now the most-recently saved photo so move it to the top of the list of local photos
			CSavegamePhotoLocalList::Delete(m_UniquePhotoId);
			CSavegamePhotoLocalList::InsertAtTopOfList(m_UniquePhotoId);

#if !__NO_OUTPUT
			photoDisplayf("CSavegameQueuedOperation_UpdateMetadataOfLocalAndCloudPhoto::Update - list of local photos after re-inserting unique photo Id %d is", m_UniquePhotoId.GetValue());
			CSavegamePhotoLocalList::DisplayContentsOfList();
#endif	//	!__NO_OUTPUT

#endif	//	__UPDATING_LOCAL_PHOTO_TITLE_AFFECTS_MOD_TIME

			m_UpdateMetadataProgress = UPDATE_METADATA_PROGRESS_BEGIN_UPDATE_UGC_TITLE;		//	UPDATE_METADATA_PROGRESS_WAIT_FOR_LOADING_MESSAGE_TO_FINISH;
			break;

		case MEM_CARD_BUSY :	 // the save is still in progress
			break;

		case MEM_CARD_ERROR :
			m_Status = MEM_CARD_ERROR;
			break;
		}
		break;

	case UPDATE_METADATA_PROGRESS_BEGIN_UPDATE_UGC_TITLE :
		{
			//	I won't return MEM_CARD_ERROR if the cloud rename fails. As long as the local rename succeeds, I'll return MEM_CARD_COMPLETE

			m_UpdateMetadataProgress = UPDATE_METADATA_PROGRESS_WAIT_FOR_LOADING_MESSAGE_TO_FINISH;

			if (CSavegamePhotoCloudList::GetListIsUpToDate())
			{
				if (CSavegamePhotoCloudList::DoesCloudPhotoExistWithThisUniqueId(m_UniquePhotoId))
				{
					CSavegamePhotoCloudList::SetTitleOfPhoto(m_UniquePhotoId, m_NewTitle.c_str());

					const char *pContentId = CSavegamePhotoCloudList::GetContentIdOfPhotoOnCloud(m_UniquePhotoId);

					photoDisplayf("CSavegameQueuedOperation_UpdateMetadataOfLocalAndCloudPhoto::Update - a cloud photo exists with unique photo Id %d (content Id is %s) so I'll call UpdateContent() with the new Title %s", m_UniquePhotoId.GetValue(), pContentId, m_NewTitle.c_str());

					rlUgc::SourceFile aUgcFile[1];

					if (photoVerifyf(UserContentManager::GetInstance().UpdateContent(pContentId, 
						m_NewTitle.c_str(), 
						NULL,			// szDesc
						NULL,			// szDataJson
						aUgcFile,		// pFiles
						0,				// nFiles
						NULL,			// szTags
						RLUGC_CONTENT_TYPE_GTA5PHOTO), 
						"CSavegameQueuedOperation_UpdateMetadataOfLocalAndCloudPhoto::Update - UpdateContent for UGC metadata has failed to start"))
					{
						photoDisplayf("CSavegameQueuedOperation_UpdateMetadataOfLocalAndCloudPhoto::Update - UpdateContent for UGC metadata has been started");
						m_UpdateMetadataProgress = UPDATE_METADATA_PROGRESS_CHECK_UPDATE_UGC_TITLE;
					}
				}
				else
				{
					photoDisplayf("CSavegameQueuedOperation_UpdateMetadataOfLocalAndCloudPhoto::Update - no photo with unique photo Id %d exists on the cloud that needs its Title updated", m_UniquePhotoId.GetValue());
				}
			}
			else
			{
				photoDisplayf("CSavegameQueuedOperation_UpdateMetadataOfLocalAndCloudPhoto::Update - list of cloud photos is not up to date so I can't check whether there is a corresponding cloud photo that needs its Title updated");
			}
		}
		break;

	case UPDATE_METADATA_PROGRESS_CHECK_UPDATE_UGC_TITLE :
		if (UserContentManager::GetInstance().HasModifyFinished())
		{
			//	I won't return MEM_CARD_ERROR if the cloud rename fails. As long as the local rename succeeds, I'll return MEM_CARD_COMPLETE

			if (UserContentManager::GetInstance().DidModifySucceed())
			{
				photoDisplayf("CSavegameQueuedOperation_UpdateMetadataOfLocalAndCloudPhoto::Update - UpdateContent for UGC succeeded");
			}
			else
			{
				photoDisplayf("CSavegameQueuedOperation_UpdateMetadataOfLocalAndCloudPhoto::Update - UpdateContent for UGC failed");
			}
			m_UpdateMetadataProgress = UPDATE_METADATA_PROGRESS_WAIT_FOR_LOADING_MESSAGE_TO_FINISH;
		}
		break;

	case UPDATE_METADATA_PROGRESS_WAIT_FOR_LOADING_MESSAGE_TO_FINISH :
		if (CSavingMessage::HasLoadingMessageDisplayedForLongEnough())
		{
			// The following is called for savegames. I'm not sure if it's needed for photos / MP stats.
			CSavegameDialogScreens::SetMessageAsProcessing(false);	//	GSWTODO - not sure if this should be in here or in DeSerialize()

			m_Status = MEM_CARD_COMPLETE;
		}
		break;

	case UPDATE_METADATA_PROGRESS_HANDLE_ERRORS :
		{
			CSavingMessage::Clear();

			if (CSavegameDialogScreens::HandleSaveGameErrorCode())
			{
				m_Status = MEM_CARD_ERROR;
			}
		}
		break;
	}


	if ( (m_Status == MEM_CARD_ERROR) || (m_Status == MEM_CARD_COMPLETE) )
	{
		if (m_Status == MEM_CARD_ERROR)
		{
			photoDisplayf("CSavegameQueuedOperation_UpdateMetadataOfLocalAndCloudPhoto::Update - failed");

			CSavingMessage::Clear();

			// The following is called for savegames. I'm not sure if it's needed for photos / MP stats.
			CSavegameDialogScreens::SetMessageAsProcessing(false);
		}
		else if (m_Status == MEM_CARD_COMPLETE)
		{
			photoDisplayf("CSavegameQueuedOperation_UpdateMetadataOfLocalAndCloudPhoto::Update - succeeded");
		}

		m_pBufferToStoreLoadedPhoto->FreeBuffer();

		return true;
	}

	return false;
}


// UploadLocalPhotoToCloud *********************************************************************************************

void CSavegameQueuedOperation_UploadLocalPhotoToCloud::Init(const CSavegamePhotoUniqueId &uniquePhotoId, CPhotoBuffer *pPhotoBuffer)
{
	if (photoVerifyf(pPhotoBuffer, "CSavegameQueuedOperation_UploadLocalPhotoToCloud::Init - pPhotoBuffer is NULL"))
	{
		m_UniquePhotoId = uniquePhotoId;
		m_pBufferContainingLocalPhoto = pPhotoBuffer;

		safecpy(m_ContentIdOfUGCPhoto, "");

		safecpy(m_LimelightStagingFileName, "");
		safecpy(m_LimelightStagingAuthToken, "");

		m_NetStatusResultCode = 0;

		m_UploadPosixTime = 0;

		m_LimelightUploadStatus.Reset();

		safecpy(m_CdnUploadResponseHeaderValueBuffer, "");

		m_UploadLocalPhotoProgress = LOCAL_PHOTO_UPLOAD_PROGRESS_CHECK_NUMBER_OF_UPLOADED_PHOTOS;
	}
}

void CSavegameQueuedOperation_UploadLocalPhotoToCloud::EndLocalPhotoUpload()
{
//	photoAssertf(CGenericGameStorage::GetSaveOperation() == OPERATION_LOADING_LOCAL_PHOTO, "CSavegameQueuedOperation_UploadLocalPhotoToCloud::EndLoadLocalPhoto - SaveOperation should be OPERATION_LOADING_LOCAL_PHOTO");
	CGenericGameStorage::SetSaveOperation(OPERATION_NONE);
}

//    Return true to Pop() from queue
bool CSavegameQueuedOperation_UploadLocalPhotoToCloud::Update()
{
    // This is the header we want to look for in the CDN upload request response.
    // We forward its value to the UGC Publish request in a header of the same name.
    static const char* cdnUploadHeaderResponsePath = "X-Akamai-RSGShardPath";

	switch (m_UploadLocalPhotoProgress)
	{
	case LOCAL_PHOTO_UPLOAD_PROGRESS_CHECK_NUMBER_OF_UPLOADED_PHOTOS :
		m_UploadLocalPhotoProgress = LOCAL_PHOTO_UPLOAD_PROGRESS_BEGIN_LOAD;
		if (photoVerifyf(CSavegamePhotoCloudList::GetListIsUpToDate(), "CSavegameQueuedOperation_UploadLocalPhotoToCloud::Update - the list of cloud photos is not up to date so we can't get the number of uploaded photos"))
		{
			photoDisplayf("CSavegameQueuedOperation_UploadLocalPhotoToCloud::Update - number of uploaded photos is %u", CSavegamePhotoCloudList::GetNumberOfPhotos());
			if (CSavegamePhotoCloudList::GetNumberOfPhotos() >= MAX_PHOTOS_TO_LOAD_FROM_CLOUD)
			{
				m_UploadLocalPhotoProgress = LOCAL_PHOTO_UPLOAD_PROGRESS_DISPLAY_WARNING_OLDEST_DELETE;
			}
		}
		break;

	case LOCAL_PHOTO_UPLOAD_PROGRESS_DISPLAY_WARNING_OLDEST_DELETE :
		{
			bool bReturnSelection = false;
			if (CSavegameDialogScreens::HandleSaveGameDialogBox(SAVE_GAME_DIALOG_OLDEST_UPLOADED_PHOTO_WILL_BE_DELETED, CSavegamePhotoCloudList::GetNumberOfPhotos(), &bReturnSelection))
			{
				if (bReturnSelection)
				{
					photoDisplayf("CSavegameQueuedOperation_UploadLocalPhotoToCloud::Update - player has chosen to replace their oldest uploaded photo");

					m_UploadLocalPhotoProgress = LOCAL_PHOTO_UPLOAD_PROGRESS_BEGIN_LOAD;	//	If the deletion of the oldest cloud photo fails for any reason then continue to upload the local photo anyway

					CSavegamePhotoUniqueId uniqueIdOfOldestCloudPhoto = CSavegamePhotoCloudList::GetUniqueIdOfOldestPhoto();
					if (photoVerifyf(!uniqueIdOfOldestCloudPhoto.GetIsFree(), "CSavegameQueuedOperation_UploadLocalPhotoToCloud::Update - unique id of oldest cloud photo is %d", uniqueIdOfOldestCloudPhoto.GetValue()))
					{
						const char *pContentId = CSavegamePhotoCloudList::GetContentIdOfPhotoOnCloud(uniqueIdOfOldestCloudPhoto);

						photoDisplayf("CSavegameQueuedOperation_UploadLocalPhotoToCloud::Update - the oldest cloud photo has unique photo Id %d (content Id is %s). I'll add it to the cloud delete queue", uniqueIdOfOldestCloudPhoto.GetValue(), pContentId);

						if (photoVerifyf(CSavegamePhotoCloudDeleter::AddContentIdToQueue(pContentId), "CSavegameQueuedOperation_UploadLocalPhotoToCloud::Update - failed to queue the deletion of the oldest cloud photo"))
						{
							CSavegamePhotoCloudList::RemovePhotoFromList(uniqueIdOfOldestCloudPhoto);	//	I'm assuming that the cloud delete will succeed
							photoDisplayf("CSavegameQueuedOperation_UploadLocalPhotoToCloud::Update - setting m_UploadLocalPhotoProgress to LOCAL_PHOTO_UPLOAD_PROGRESS_WAIT_FOR_DELETION_TO_FINISH");
							m_UploadLocalPhotoProgress = LOCAL_PHOTO_UPLOAD_PROGRESS_WAIT_FOR_DELETION_TO_FINISH;
						}
					}
				}
				else
				{
					photoDisplayf("CSavegameQueuedOperation_UploadLocalPhotoToCloud::Update - player has chosen not to replace their oldest uploaded photo so give up now");
					m_Status = MEM_CARD_ERROR;
				}
			}
		}
		break;

	case LOCAL_PHOTO_UPLOAD_PROGRESS_WAIT_FOR_DELETION_TO_FINISH :
		photoDisplayf("CSavegameQueuedOperation_UploadLocalPhotoToCloud::Update - waiting for queue of cloud photo deletes to be empty");
		if (CSavegamePhotoCloudDeleter::IsQueueEmpty())		//	I'm not actually checking whether the cloud delete succeeded or not. I'd have to expand CSavegamePhotoCloudDeleter to record that.
		{
			photoDisplayf("CSavegameQueuedOperation_UploadLocalPhotoToCloud::Update - queue of cloud photo deletes is now empty");
			m_UploadLocalPhotoProgress = LOCAL_PHOTO_UPLOAD_PROGRESS_BEGIN_LOAD;
		}
		break;

	case LOCAL_PHOTO_UPLOAD_PROGRESS_BEGIN_LOAD :
		{
			CSavegameDialogScreens::SetDialogBoxHeading("SG_HDNG");

			switch (CSavegamePhotoLoadLocal::BeginLocalPhotoLoad(m_UniquePhotoId, m_pBufferContainingLocalPhoto))
			{
			case MEM_CARD_COMPLETE :
				m_UploadLocalPhotoProgress = LOCAL_PHOTO_UPLOAD_PROGRESS_CHECK_LOAD;
				break;
			case MEM_CARD_BUSY :
				photoAssertf(0, "CSavegameQueuedOperation_UploadLocalPhotoToCloud::Update - didn't expect CSavegamePhotoLoadLocal::BeginLocalPhotoLoad to ever return MEM_CARD_BUSY");
				break;
			case MEM_CARD_ERROR :
				CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_BEGIN_LOCAL_LOAD_JPEG_FAILED);
				m_UploadLocalPhotoProgress = LOCAL_PHOTO_UPLOAD_PROGRESS_HANDLE_ERRORS;
				break;
			}
		}
		break;

	case LOCAL_PHOTO_UPLOAD_PROGRESS_CHECK_LOAD :
		{
			CSavingMessage::BeginDisplaying(CSavingMessage::STORAGE_DEVICE_MESSAGE_CLOUD_LOADING);

			MemoryCardError LoadState = CSavegamePhotoLoadLocal::DoLocalPhotoLoad();
			if (LoadState == MEM_CARD_ERROR)
			{
				if (SAVE_GAME_DIALOG_PLAYER_HAS_SIGNED_OUT == CSavegameDialogScreens::GetMostRecentErrorCode())
				{
					photoDisplayf("CSavegameQueuedOperation_UploadLocalPhotoToCloud::Update - player has signed out");
					m_Status = MEM_CARD_ERROR;
				}
				else
				{
					photoDisplayf("CSavegameQueuedOperation_UploadLocalPhotoToCloud::Update - CSavegamePhotoLoadLocal::DoLocalPhotoLoad() returned an error");
					m_UploadLocalPhotoProgress = LOCAL_PHOTO_UPLOAD_PROGRESS_HANDLE_ERRORS;
				}
			}
			else if (LoadState == MEM_CARD_COMPLETE)
			{
				photoDisplayf("CSavegameQueuedOperation_UploadLocalPhotoToCloud::Update - CSavegamePhotoLoadLocal::DoLocalPhotoLoad() succeeded");

				m_UploadLocalPhotoProgress = LOCAL_PHOTO_UPLOAD_PROGRESS_BEGIN_CREATE_UGC;
			}
		}
		break;

	case LOCAL_PHOTO_UPLOAD_PROGRESS_BEGIN_CREATE_UGC :
		{
			if (photoVerifyf(!UserContentManager::GetInstance().IsCreatePending(), "CSavegameQueuedOperation_UploadLocalPhotoToCloud::Update - BEGIN_CREATE_UGC - a Create is already pending"))
			{
				if (CSavegamePhotoMetadata::ShouldCorrectAkamaiOnUpload())
				{
					m_pBufferContainingLocalPhoto->CorrectAkamaiTag();
				}

				u8 *pJpegBuffer = m_pBufferContainingLocalPhoto->GetJpegBuffer();
				u32 sizeOfJpegBuffer = m_pBufferContainingLocalPhoto->GetExactSizeOfJpegData();
				const char *pTitle = m_pBufferContainingLocalPhoto->GetTitleString();
				const char *pDescription = m_pBufferContainingLocalPhoto->GetDescriptionString();
				const char *pMetadataString = m_pBufferContainingLocalPhoto->GetJsonBuffer();

				rlUgc::SourceFile srcFile(0, pJpegBuffer, sizeOfJpegBuffer);

				rlUgc::SourceFile *pSourceFile = &srcFile;
				s32 numberOfFiles = 1;

				// setting bPublish=false and numberOfFiles=0 as the first stage in uploading to CDN
				bool bPublish = false;	//	John Hynd says that this means "share so that others can see your photos"
				pSourceFile = NULL;
				numberOfFiles = 0;

				photoDisplayf("CSavegameQueuedOperation_UploadLocalPhotoToCloud::Update - BEGIN_CREATE_UGC - Title is %s, sizeOfJpegBuffer=%u, metadata string is %s", pTitle, sizeOfJpegBuffer, pMetadataString);

				if (UserContentManager::GetInstance().Create(pTitle, 
					pDescription, 
					pSourceFile,
					numberOfFiles,
					NULL, //tags
					pMetadataString, 
					RLUGC_CONTENT_TYPE_GTA5PHOTO, 
					bPublish))
				{
					photoDisplayf("CSavegameQueuedOperation_UploadLocalPhotoToCloud::Update - BEGIN_CREATE_UGC - UGC creation was successfully started");
					m_UploadLocalPhotoProgress = LOCAL_PHOTO_UPLOAD_PROGRESS_CHECK_CREATE_UGC;
				}
				else
				{
					photoDisplayf("CSavegameQueuedOperation_UploadLocalPhotoToCloud::Update - BEGIN_CREATE_UGC - UGC creation failed to start");
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_LIMELIGHT_UGC_CREATION_FAILED_TO_START);
					m_UploadLocalPhotoProgress = LOCAL_PHOTO_UPLOAD_PROGRESS_HANDLE_ERRORS;
				}
			}
			else
			{
				CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_LIMELIGHT_UGC_CREATE_ALREADY_PENDING);
				m_UploadLocalPhotoProgress = LOCAL_PHOTO_UPLOAD_PROGRESS_HANDLE_ERRORS;
			}
		}
		break;

	case LOCAL_PHOTO_UPLOAD_PROGRESS_CHECK_CREATE_UGC :
		if (UserContentManager::GetInstance().HasCreateFinished())
		{
#if __BANK
			if (CPhotoManager::GetDelayPhotoUpload())
			{

			}
			else
#endif	//	__BANK
			{
				if (UserContentManager::GetInstance().DidCreateSucceed())
				{
					m_UploadPosixTime = UserContentManager::GetInstance().GetCreateCreatedDate();

					photoDisplayf("CSavegameQueuedOperation_UploadLocalPhotoToCloud::Update - CHECK_CREATE_UGC - UGC creation has completed successfully so parse content results for Limelight info");

					SaveGameDialogCode beginUploadErrorCode = SAVE_GAME_DIALOG_NONE;
					if (BeginCdnUpload(beginUploadErrorCode, cdnUploadHeaderResponsePath))
					{
						m_UploadLocalPhotoProgress = LOCAL_PHOTO_UPLOAD_CHECK_LIMELIGHT_UPLOAD;
					}
					else
					{
						photoDisplayf("CSavegameQueuedOperation_UploadLocalPhotoToCloud::Update - CHECK_CREATE_UGC - BeginLimelightUpload() failed so delete the newly uploaded photo from UGC");

						const char *pContentId = UserContentManager::GetInstance().GetCreateContentID();
						if (photoVerifyf(pContentId, "CSavegameQueuedOperation_UploadLocalPhotoToCloud::Update - CHECK_CREATE_UGC - failed to get the Content Id for the newly uploaded photo. I wanted to delete the content because BeginLimelightUpload() failed"))
						{
							UserContentManager::GetInstance().SetDeleted(pContentId, true, RLUGC_CONTENT_TYPE_GTA5PHOTO);
						}

						if (beginUploadErrorCode == SAVE_GAME_DIALOG_NONE)
						{
							CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_UGC_PHONE_PHOTO_SAVE_FAILED);
						}
						else
						{
							CSavegameDialogScreens::SetSaveGameError(beginUploadErrorCode);
						}
						m_UploadLocalPhotoProgress = LOCAL_PHOTO_UPLOAD_PROGRESS_HANDLE_ERRORS;
					}
				}
				else
				{
					m_NetStatusResultCode = UserContentManager::GetInstance().GetCreateResultCode();

					photoDisplayf("CSavegameQueuedOperation_UploadLocalPhotoToCloud::Update - CHECK_CREATE_UGC - UGC creation has failed with result code %d", m_NetStatusResultCode);
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_LIMELIGHT_UGC_CREATION_FAILED);
					m_UploadLocalPhotoProgress = LOCAL_PHOTO_UPLOAD_PROGRESS_HANDLE_ERRORS;
				}
			}
		}
		break;

	case LOCAL_PHOTO_UPLOAD_CHECK_LIMELIGHT_UPLOAD :
		{
			if (!m_LimelightUploadStatus.Pending())
			{
				if (m_LimelightUploadStatus.Succeeded())
				{
					photoDisplayf("CSavegameQueuedOperation_UploadLocalPhotoToCloud::Update - CHECK_LIMELIGHT_UPLOAD - Limelight upload has succeeded so I'll publish the new photo");

                    // m_CdnUploadResponseHeaderValueBuffer was filled in with the value of the response header with the name 
                    // cdnUploadHeaderResponsePath as part of the photo CDN upload task.
                    // Now we forward the value to the Publish task as an extra request header.
                    char cdnUploadPathHeader[256] = {0};

                    if (m_CdnUploadResponseHeaderValueBuffer[0] != '\0')
                    {
                        formatf(cdnUploadPathHeader, "%s: %s\r\n", cdnUploadHeaderResponsePath, m_CdnUploadResponseHeaderValueBuffer);
                    }

					//	What should I pass in for szBaseContentID?
					if (UserContentManager::GetInstance().Publish(m_ContentIdOfUGCPhoto, "", RLUGC_CONTENT_TYPE_GTA5PHOTO, cdnUploadPathHeader[0] != '\0' ? cdnUploadPathHeader : nullptr))
					{
						photoDisplayf("CSavegameQueuedOperation_UploadLocalPhotoToCloud::Update - CHECK_LIMELIGHT_UPLOAD - publish has been successfully started");
						m_UploadLocalPhotoProgress = LOCAL_PHOTO_UPLOAD_PROGRESS_CHECK_PUBLISH;
					}
					else
					{
						photoDisplayf("CSavegameQueuedOperation_UploadLocalPhotoToCloud::Update - CHECK_LIMELIGHT_UPLOAD - publish failed to start");

						UserContentManager::GetInstance().SetDeleted(m_ContentIdOfUGCPhoto, true, RLUGC_CONTENT_TYPE_GTA5PHOTO);

						CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_LIMELIGHT_UGC_PUBLISH_FAILED_TO_START);
						m_UploadLocalPhotoProgress = LOCAL_PHOTO_UPLOAD_PROGRESS_HANDLE_ERRORS;
					}
				}
				else
				{
					m_NetStatusResultCode = m_LimelightUploadStatus.GetResultCode();

					photoDisplayf("CSavegameQueuedOperation_UploadLocalPhotoToCloud::Update - CHECK_LIMELIGHT_UPLOAD - Limelight upload has failed with result code %d so I'll delete the uploaded UGC", m_NetStatusResultCode);

					UserContentManager::GetInstance().SetDeleted(m_ContentIdOfUGCPhoto, true, RLUGC_CONTENT_TYPE_GTA5PHOTO);

					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_LIMELIGHT_UPLOAD_FAILED);
					m_UploadLocalPhotoProgress = LOCAL_PHOTO_UPLOAD_PROGRESS_HANDLE_ERRORS;
				}
			}
		}
		break;

	case LOCAL_PHOTO_UPLOAD_PROGRESS_CHECK_PUBLISH :
		{
			if (UserContentManager::GetInstance().HasModifyFinished())
			{
				if (UserContentManager::GetInstance().DidModifySucceed())
				{
					photoDisplayf("CSavegameQueuedOperation_UploadLocalPhotoToCloud::Update - CHECK_PUBLISH - publish has succeeded so I'll add it to the cloud list");

					const char *pTitle = m_pBufferContainingLocalPhoto->GetTitleString();
					const char *pMetadataString = m_pBufferContainingLocalPhoto->GetJsonBuffer();

					CSavegamePhotoCloudList::AddToArray(m_ContentIdOfUGCPhoto, pTitle, pMetadataString, m_UploadPosixTime);

					m_Status = MEM_CARD_COMPLETE;
				}
				else
				{
					m_NetStatusResultCode = UserContentManager::GetInstance().GetModifyResultCode();

					photoDisplayf("CSavegameQueuedOperation_UploadLocalPhotoToCloud::Update - CHECK_PUBLISH - publish has failed with result code %d so I'll delete the uploaded UGC", m_NetStatusResultCode);

					UserContentManager::GetInstance().SetDeleted(m_ContentIdOfUGCPhoto, true, RLUGC_CONTENT_TYPE_GTA5PHOTO);

					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_LIMELIGHT_UGC_PUBLISH_FAILED);
					m_UploadLocalPhotoProgress = LOCAL_PHOTO_UPLOAD_PROGRESS_HANDLE_ERRORS;
				}
			}
		}
		break;

//		LOCAL_PHOTO_UPLOAD_PROGRESS_WAIT_FOR_LOADING_MESSAGE_TO_FINISH,
	case LOCAL_PHOTO_UPLOAD_PROGRESS_HANDLE_ERRORS :
		{
			CSavingMessage::Clear();

			if (CSavegameDialogScreens::HandleSaveGameErrorCode())
			{
				m_Status = MEM_CARD_ERROR;
			}
		}
		break;
	}

	if ( (m_Status == MEM_CARD_ERROR) || (m_Status == MEM_CARD_COMPLETE) )
	{
		if (m_Status == MEM_CARD_ERROR)
		{
//			savegameWarningf("Error in CalculateSizeOfRequiredBuffer: code=%d", CSavegameDialogScreens::GetMostRecentErrorCode());
			photoDisplayf("CSavegameQueuedOperation_UploadLocalPhotoToCloud::Update - failed");
		}
		else if (m_Status == MEM_CARD_COMPLETE)
		{
			photoDisplayf("CSavegameQueuedOperation_UploadLocalPhotoToCloud::Update - succeeded");
		}

		m_pBufferContainingLocalPhoto->FreeBuffer();

		CSavingMessage::Clear();

		EndLocalPhotoUpload();

		// The following is called for savegames. I'm not sure if it's needed for photos / MP stats.
		CSavegameDialogScreens::SetMessageAsProcessing(false);

		return true;
	}

	return false;
}


bool CSavegameQueuedOperation_UploadLocalPhotoToCloud::BeginCdnUpload(SaveGameDialogCode &returnErrorCode, const char* requestResponseHeaderValueToGet)
{
	returnErrorCode = SAVE_GAME_DIALOG_NONE;
	const char *pNewContentId = UserContentManager::GetInstance().GetCreateContentID();

	if (pNewContentId)
	{
		photoAssertf(strlen(pNewContentId) < RLUGC_MAX_CONTENTID_CHARS, "CSavegameQueuedOperation_UploadLocalPhotoToCloud::BeginLimelightUpload - pNewContentId has length %d. Expected it to be less than %u", (s32) strlen(pNewContentId), RLUGC_MAX_CONTENTID_CHARS);

		safecpy(m_ContentIdOfUGCPhoto, pNewContentId);

		const rlUgcMetadata *pUgcMetadata = UserContentManager::GetInstance().GetCreateResult();
		if (pUgcMetadata)
		{
			rlUgcGta5PhotoMetadataData photoMetadata;

			if (photoMetadata.SetMetadata(*pUgcMetadata))
			{
				u8 *pJpegBuffer = m_pBufferContainingLocalPhoto->GetJpegBuffer();
				u32 sizeOfJpegBuffer = m_pBufferContainingLocalPhoto->GetExactSizeOfJpegData();

				rlHttpCdnUploadPhotoTask *pTask = NULL;
				rlGetTaskManager()->CreateTask(&pTask);
				if(pTask)
				{
					char m_StagingUrl[512];
//					formatf(m_StagingUrl, "http://global.llp.lldns.net:8080/post/raw/%s", photoMetadata.GetStagingFileName());
					formatf(m_StagingUrl, "http://global.llp.lldns.net:8080/post/raw");

					const char *pStagingUrl = photoMetadata.GetStagingUrl();
					if (PARAM_useNewStagingUrl.Get())
					{
						pStagingUrl = m_StagingUrl;
					}

					safecpy(m_LimelightStagingFileName, photoMetadata.GetStagingFileName());
					safecpy(m_LimelightStagingAuthToken, photoMetadata.GetStagingAuthToken());

					//	Should I call m_LimelightUploadStatus.Reset(); here?
					if (rlTaskBase::Configure(pTask, "name", 
						pStagingUrl,
						photoMetadata.GetStagingAuthToken(), photoMetadata.GetStagingDirName(),
						photoMetadata.GetStagingFileName(), pJpegBuffer, sizeOfJpegBuffer,
						requestResponseHeaderValueToGet, m_CdnUploadResponseHeaderValueBuffer, (unsigned)sizeof(m_CdnUploadResponseHeaderValueBuffer), photoMetadata.UsesAkamai(),
						&m_LimelightUploadStatus))
					{
						if (rlGetTaskManager()->AddParallelTask(pTask))
						{
							return true;
						}
						else
						{
							returnErrorCode = SAVE_GAME_DIALOG_LIMELIGHT_ADD_PARALLEL_TASK_FAILED;
							photoDisplayf("CSavegameQueuedOperation_UploadLocalPhotoToCloud::BeginLimelightUpload - AddParallelTask failed for rlHttpLlnwUploadPhotoTask");
							photoAssertf(0, "CSavegameQueuedOperation_UploadLocalPhotoToCloud::BeginLimelightUpload - AddParallelTask failed for rlHttpLlnwUploadPhotoTask");
						}
					}
					else
					{
						returnErrorCode = SAVE_GAME_DIALOG_LIMELIGHT_CONFIGURE_FAILED;
						photoDisplayf("CSavegameQueuedOperation_UploadLocalPhotoToCloud::BeginLimelightUpload - Configure failed for rlHttpLlnwUploadPhotoTask");
						photoAssertf(0, "CSavegameQueuedOperation_UploadLocalPhotoToCloud::BeginLimelightUpload - Configure failed for rlHttpLlnwUploadPhotoTask");
					}
				}
				else
				{
					returnErrorCode = SAVE_GAME_DIALOG_LIMELIGHT_CREATE_TASK_FAILED;
					photoDisplayf("CSavegameQueuedOperation_UploadLocalPhotoToCloud::BeginLimelightUpload - failed to create a rlHttpLlnwUploadPhotoTask");
					photoAssertf(0, "CSavegameQueuedOperation_UploadLocalPhotoToCloud::BeginLimelightUpload - failed to create a rlHttpLlnwUploadPhotoTask");
				}
			}
			else
			{
				returnErrorCode = SAVE_GAME_DIALOG_LIMELIGHT_UGC_METADATA_PARSE_ERROR;
				photoDisplayf("CSavegameQueuedOperation_UploadLocalPhotoToCloud::BeginLimelightUpload - failed to parse Limelight photo data from metadata");
				photoAssertf(0, "CSavegameQueuedOperation_UploadLocalPhotoToCloud::BeginLimelightUpload - failed to parse Limelight photo data from metadata");
			}
		}
		else
		{
			returnErrorCode = SAVE_GAME_DIALOG_LIMELIGHT_GET_UGC_CREATE_RESULT_FAILED;
			photoDisplayf("CSavegameQueuedOperation_UploadLocalPhotoToCloud::BeginLimelightUpload - GetCreateResult() returned NULL");
			photoAssertf(0, "CSavegameQueuedOperation_UploadLocalPhotoToCloud::BeginLimelightUpload - GetCreateResult() returned NULL");
		}
	}
	else
	{
		returnErrorCode = SAVE_GAME_DIALOG_LIMELIGHT_GET_UGC_CREATE_CONTENT_ID_FAILED;
		photoDisplayf("CSavegameQueuedOperation_UploadLocalPhotoToCloud::BeginLimelightUpload - GetCreateContentID() returned NULL");
		photoAssertf(0, "CSavegameQueuedOperation_UploadLocalPhotoToCloud::BeginLimelightUpload - GetCreateContentID() returned NULL");
	}

	return false;
}

#else	//	__LOAD_LOCAL_PHOTOS

// CreateSortedListOfPhotos *********************************************************************************************

void CSavegameQueuedOperation_CreateSortedListOfPhotos::Init(bool bOnlyGetNumberOfPhotos, bool bDisplayWarningScreens)
{
	m_bOnlyGetNumberOfPhotos = bOnlyGetNumberOfPhotos;
	m_bDisplayWarningScreens = bDisplayWarningScreens;
	m_CreateSortedListOfPhotosProgress = CREATE_SORTED_LIST_OF_PHOTOS_REQUEST_CLOUD_DIRECTORY_CONTENTS;
}

//    Return true to Pop() from queue
bool CSavegameQueuedOperation_CreateSortedListOfPhotos::Update()
{
	switch (m_CreateSortedListOfPhotosProgress)
	{
		case CREATE_SORTED_LIST_OF_PHOTOS_REQUEST_CLOUD_DIRECTORY_CONTENTS :

			CSavegameDialogScreens::SetDialogBoxHeading("SG_HDNG");	//	SG_HDG_PH_RTRV	//	Retrieving Photos from Social Club

			if (CPhotoManager::RequestListOfCloudPhotos(m_bOnlyGetNumberOfPhotos))
			{
				m_CreateSortedListOfPhotosProgress = CREATE_SORTED_LIST_OF_PHOTOS_WAIT_FOR_CLOUD_DIRECTORY_REQUEST;
			}
			else
			{
				photoAssertf(0, "CSavegameQueuedOperation_CreateSortedListOfPhotos::Update - REQUEST_CLOUD_DIRECTORY_CONTENTS failed");
//				m_Status = MEM_CARD_ERROR;
				CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_BEGIN_CLOUD_LOAD_PHOTO_LIST_FAILED);
				m_CreateSortedListOfPhotosProgress = CREATE_SORTED_LIST_OF_PHOTOS_HANDLE_ERRORS;
			}
			break;

		case CREATE_SORTED_LIST_OF_PHOTOS_WAIT_FOR_CLOUD_DIRECTORY_REQUEST :
			photoDisplayf("CSavegameQueuedOperation_CreateSortedListOfPhotos::Update - WAIT_FOR_CLOUD_DIRECTORY_REQUEST");
			if (CPhotoManager::GetIsCloudPhotoRequestPending(m_bOnlyGetNumberOfPhotos) == false)
			{
				if (CPhotoManager::GetHasCloudPhotoRequestSucceeded(m_bOnlyGetNumberOfPhotos))
				{
					photoDisplayf("CSavegameQueuedOperation_CreateSortedListOfPhotos::Update - cloud request has completed");
					m_CreateSortedListOfPhotosProgress = CREATE_SORTED_LIST_OF_PHOTOS_CREATE_MERGED_LIST_OF_LOCAL_AND_CLOUD_PHOTOS;
				}
				else
				{
					photoDisplayf("CSavegameQueuedOperation_CreateSortedListOfPhotos::Update - cloud request has failed");
//					m_Status = MEM_CARD_ERROR;
					CSavegameDialogScreens::SetSaveGameError(SAVE_GAME_DIALOG_CHECK_CLOUD_LOAD_PHOTO_LIST_FAILED);
					m_CreateSortedListOfPhotosProgress = CREATE_SORTED_LIST_OF_PHOTOS_HANDLE_ERRORS;
				}
			}
			break;

		case CREATE_SORTED_LIST_OF_PHOTOS_CREATE_MERGED_LIST_OF_LOCAL_AND_CLOUD_PHOTOS :
			CPhotoManager::FillMergedListOfPhotos(m_bOnlyGetNumberOfPhotos);
			m_Status = MEM_CARD_COMPLETE;
			break;

		case CREATE_SORTED_LIST_OF_PHOTOS_HANDLE_ERRORS :
			if (!m_bDisplayWarningScreens || CSavegameDialogScreens::HandleSaveGameErrorCode())
			{
				CSavegameDialogScreens::ClearSaveGameError();
				m_Status = MEM_CARD_ERROR;
			}
			break;
	}

	if ( (m_Status == MEM_CARD_ERROR) || (m_Status == MEM_CARD_COMPLETE) )
	{
		// Copied from CSavegameCheckFileExists::CheckFileExists which in turned copied it from CSavegameLoadBlock::CalculateSizeOfRequiredBuffer(). Not sure if it's necessary here.
// 		CSavegameDialogScreens::SetMessageAsProcessing(false);
// 
// 		Assertf( (m_bCreateListForSaving && (CGenericGameStorage::GetSaveOperation() == OPERATION_SCANNING_CONSOLE_FOR_SAVING_SAVEGAMES) ) 
// 			|| (!m_bCreateListForSaving && (CGenericGameStorage::GetSaveOperation() == OPERATION_SCANNING_CONSOLE_FOR_LOADING_SAVEGAMES) ), "CSavegameQueuedOperation_CreateSortedListOfPhotos::Update - SaveOperation should be either OPERATION_SCANNING_CONSOLE_FOR_SAVING_SAVEGAMES or OPERATION_SCANNING_CONSOLE_FOR_LOADING_SAVEGAMES");
// 
// 		CGenericGameStorage::SetSaveOperation(OPERATION_NONE);
		return true;
	}

	return false;
}

// End of CreateSortedListOfPhotos *********************************************************************************************

#endif	//	__LOAD_LOCAL_PHOTOS


// UploadMugshot *********************************************************************************************

void CSavegameQueuedOperation_UploadMugshot::Init(CPhotoBuffer *pPhotoBuffer, s32 mugshotCharacterIndex)
{
	if (photoVerifyf(pPhotoBuffer, "CSavegameQueuedOperation_UploadMugshot::Init - pPhotoBuffer is NULL"))
	{
		m_pBufferContainingMugshot = pPhotoBuffer;
		m_MugshotCharacterIndex = mugshotCharacterIndex;

		m_UploadMugshotProgress = MUGSHOT_UPLOAD_PROGRESS_BEGIN;		
	}
}

//    Return true to Pop() from queue
bool CSavegameQueuedOperation_UploadMugshot::Update()
{
	switch (m_UploadMugshotProgress)
	{
	case MUGSHOT_UPLOAD_PROGRESS_BEGIN :
		if (CPhotoManager::RequestUploadOfMugshotPhoto(m_pBufferContainingMugshot, m_MugshotCharacterIndex))
		{
			savegameDisplayf("CSavegameQueuedOperation_UploadMugshot::Update - RequestUploadOfMugshotPhoto returned TRUE");
			m_UploadMugshotProgress = MUGSHOT_UPLOAD_PROGRESS_CHECK;
		}
		else
		{
			savegameDisplayf("CSavegameQueuedOperation_UploadMugshot::Update - RequestUploadOfMugshotPhoto returned FALSE");
			m_Status = MEM_CARD_ERROR;
		}
		break;

	case MUGSHOT_UPLOAD_PROGRESS_CHECK :
		m_Status = CPhotoManager::GetStatusOfUploadOfMugshotPhoto();
		break;
	}

	if ( (m_Status == MEM_CARD_ERROR) || (m_Status == MEM_CARD_COMPLETE) )
	{
		if (m_Status == MEM_CARD_ERROR)
		{
			photoDisplayf("CSavegameQueuedOperation_UploadMugshot::Update - failed");
		}
		else if (m_Status == MEM_CARD_COMPLETE)
		{
			photoDisplayf("CSavegameQueuedOperation_UploadMugshot::Update - succeeded");
		}

		CSavingMessage::Clear();

//		EndMugshotUpload();

		// The following is called for savegames. I'm not sure if it's needed for photos / MP stats.
		CSavegameDialogScreens::SetMessageAsProcessing(false);

		return true;
	}

	return false;
}

// End of UploadMugshot *********************************************************************************************


// DeleteFile *********************************************************************************************

void CSavegameQueuedOperation_DeleteFile::Init(const char *pFilename, bool bCloudFile, bool bIsAPhoto)
{
	s32 maxStringLength = SAVE_GAME_MAX_FILENAME_LENGTH_OF_LOCAL_FILE;
	if (bCloudFile)
	{
		maxStringLength = SAVE_GAME_MAX_FILENAME_LENGTH_OF_CLOUD_FILE;
	}

	if (photoVerifyf( (strlen(pFilename) < maxStringLength), "CSavegameQueuedOperation_DeleteFile::Init - filename %s has more than %d characters", pFilename, maxStringLength))
	{
		safecpy(m_FilenameOfFileToDelete, pFilename, maxStringLength);
		m_bCloudFile = bCloudFile;
		m_bIsAPhoto = bIsAPhoto;
		m_DeleteFileProgress = DELETE_FILE_PROGRESS_BEGIN;
	}
}

//    Return true to Pop() from queue
bool CSavegameQueuedOperation_DeleteFile::Update()
{
	switch (m_DeleteFileProgress)
	{
		case DELETE_FILE_PROGRESS_BEGIN :
			{
				CSavegameDialogScreens::SetDialogBoxHeading("SG_HDNG");	//	SG_HDG_DEL	//	It doesn't look like anything is using the Delete operation now

				if (CSavegameDelete::Begin(m_FilenameOfFileToDelete, m_bCloudFile, m_bIsAPhoto))
				{
					photoDisplayf("CSavegameQueuedOperation_DeleteFile::Update - CSavegameDelete::Begin succeeded for %s", m_FilenameOfFileToDelete);
					m_DeleteFileProgress = DELETE_FILE_PROGRESS_CHECK;
				}
				else
				{
					photoDisplayf("CSavegameQueuedOperation_DeleteFile::Update - CSavegameDelete::Begin failed for %s", m_FilenameOfFileToDelete);
					m_Status = MEM_CARD_ERROR;
					return true;
				}
			}
			break;

		case DELETE_FILE_PROGRESS_CHECK :
			{
				switch (CSavegameDelete::Update())
				{
					case MEM_CARD_COMPLETE :
						photoDisplayf("CSavegameQueuedOperation_DeleteFile::Update - CSavegameDelete::Update succeeded for %s", m_FilenameOfFileToDelete);
						m_Status = MEM_CARD_COMPLETE;
						return true;
//						break;

					case MEM_CARD_BUSY :
						break;

					case MEM_CARD_ERROR :
						photoDisplayf("CSavegameQueuedOperation_DeleteFile::Update - CSavegameDelete::Update failed for %s", m_FilenameOfFileToDelete);
						m_Status = MEM_CARD_ERROR;
						return true;
//						break;
				}
			}
			break;
	}

	return false;
}

// End of DeleteFile *********************************************************************************************


// SaveLocalPhoto *********************************************************************************************

void CSavegameQueuedOperation_SaveLocalPhoto::Init(const CSavegamePhotoUniqueId &uniqueId, const CPhotoBuffer *pPhotoBuffer)
{
//	if (photoVerifyf(pUniqueId, "CSavegameQueuedOperation_SaveLocalPhoto::Init - pUniqueId is NULL"))
	{
		if (photoVerifyf(pPhotoBuffer, "CSavegameQueuedOperation_SaveLocalPhoto::Init - pPhotoBuffer is NULL"))
		{
			m_UniqueIdForFilename = uniqueId;
			m_pBufferToBeSaved = pPhotoBuffer;

			m_SaveLocalPhotoProgress = SAVE_LOCAL_PHOTO_PROGRESS_BEGIN;
		}
	}
}


//    Return true to Pop() from queue
bool CSavegameQueuedOperation_SaveLocalPhoto::Update()
{
	switch (m_SaveLocalPhotoProgress)
	{
		case SAVE_LOCAL_PHOTO_PROGRESS_BEGIN :
			switch (CSavegamePhotoSaveLocal::BeginSavePhotoLocal(m_UniqueIdForFilename, m_pBufferToBeSaved))
			{
			case MEM_CARD_COMPLETE :
				m_SaveLocalPhotoProgress = SAVE_LOCAL_PHOTO_PROGRESS_CHECK;
				break;
			case MEM_CARD_BUSY :
				savegameAssertf(0, "CSavegameQueuedOperation_SaveLocalPhoto::Update - didn't expect CSavegamePhotoSaveLocal::BeginSavePhotoLocal() to ever return MEM_CARD_BUSY");
				m_Status = MEM_CARD_ERROR;
				break;
			case MEM_CARD_ERROR :
				savegameAssertf(0, "CSavegameQueuedOperation_SaveLocalPhoto::Update - CSavegamePhotoSaveLocal::BeginSavePhotoLocal() returned MEM_CARD_ERROR");
				m_Status = MEM_CARD_ERROR;
				break;
			}

			break;

		case SAVE_LOCAL_PHOTO_PROGRESS_CHECK :
			switch (CSavegamePhotoSaveLocal::DoSavePhotoLocal())
			{
			case MEM_CARD_COMPLETE :
				CSavegamePhotoLocalList::InsertAtTopOfList(m_UniqueIdForFilename);

#if !__NO_OUTPUT
				photoDisplayf("CSavegameQueuedOperation_SaveLocalPhoto::Update - list of local photos after inserting unique photo Id %d is", m_UniqueIdForFilename.GetValue());
				CSavegamePhotoLocalList::DisplayContentsOfList();
#endif	//	!__NO_OUTPUT

				m_Status = MEM_CARD_COMPLETE;
				break;

			case MEM_CARD_BUSY :	 // the save is still in progress
				break;

			case MEM_CARD_ERROR :
				m_Status = MEM_CARD_ERROR;
				break;
			}

			break;
	}

	if ( (m_Status == MEM_CARD_ERROR) || (m_Status == MEM_CARD_COMPLETE) )
	{
		return true;
	}

	return false;
}

// End of SaveLocalPhoto *********************************************************************************************


// LoadMostRecentSave *********************************************************************************************
void CSavegameQueuedOperation_LoadMostRecentSave::Init()
{
	m_LoadMostRecentSaveProgress = LOAD_PROGRESS_CHECK_IF_SIGNED_IN;
}

//    Return true to Pop() from queue
bool CSavegameQueuedOperation_LoadMostRecentSave::Update()
{
	switch (m_LoadMostRecentSaveProgress)
	{
		case LOAD_PROGRESS_CHECK_IF_SIGNED_IN :
		{
#if RSG_DURANGO
			if (CSavegameUsers::GetSignedInUser() && !CLiveManager::SignedInIntoNewProfileAfterSuspendMode())
#else
			if (CSavegameUsers::GetSignedInUser())
#endif
			{
				CSavegameDialogScreens::SetDialogBoxHeading("SG_HDNG");

				CGenericGameStorage::SetSaveOperation(OPERATION_ENUMERATING_SAVEGAMES);
				CSavegameInitialChecks::BeginInitialChecks(false);

				m_LoadMostRecentSaveProgress = LOAD_PROGRESS_ENUMERATE_SAVEGAMES;
			}
			else
			{
				savegameDisplayf("CSavegameQueuedOperation_LoadMostRecentSave::Update - player is not signed in so start a new session");
				m_Status = MEM_CARD_ERROR;
			}
		}
		break;

		case LOAD_PROGRESS_ENUMERATE_SAVEGAMES :
		{
			MemoryCardError EnumerationState = CSavegameInitialChecks::InitialChecks();
			if (EnumerationState == MEM_CARD_ERROR)
			{
				savegameDisplayf("CSavegameQueuedOperation_LoadMostRecentSave::Update - enumeration of savegames has failed so start a new session");

				CGenericGameStorage::SetSaveOperation(OPERATION_NONE);

				m_Status = MEM_CARD_ERROR;
			}
			else if (EnumerationState == MEM_CARD_COMPLETE)
			{
				savegameDisplayf("CSavegameQueuedOperation_LoadMostRecentSave::Update - enumeration of savegames has succeeded so move on to finding the most recent save");

				CGenericGameStorage::SetSaveOperation(OPERATION_NONE);

				m_LoadMostRecentSaveProgress = LOAD_PROGRESS_FIND_MOST_RECENT_SAVE;
			}
		}
		break;

	case LOAD_PROGRESS_FIND_MOST_RECENT_SAVE :
		{
			m_IndexOfMostRecentSave = CSavegameList::FindMostRecentSavegame();

			if (m_IndexOfMostRecentSave >= 0)
			{
				savegameDisplayf("CSavegameQueuedOperation_LoadMostRecentSave::Update - most recent savegame slot is %d", m_IndexOfMostRecentSave);
				m_LoadMostRecentSaveProgress = LOAD_PROGRESS_BEGIN_LOAD;
			}
			else
			{
				savegameDisplayf("CSavegameQueuedOperation_LoadMostRecentSave::Update - CSavegameList::FindMostRecentSavegame() has failed to find a savegame so start a new session");
				m_Status = MEM_CARD_ERROR;
			}
		}
		break;

	case LOAD_PROGRESS_BEGIN_LOAD :
		{
			CSavegameDialogScreens::SetDialogBoxHeading("SG_HDNG");	//	SG_HDG_LD_RPL

			if (MEM_CARD_ERROR == CSavegameList::BeginGameLoad(m_IndexOfMostRecentSave))
			{
				savegameDisplayf("CSavegameQueuedOperation_LoadMostRecentSave::Update - CSavegameList::BeginGameLoad() has failed so start a new session");
				m_Status = MEM_CARD_ERROR;
			}
			else
			{
				camInterface::FadeOut(0);  // fade out the screen
				CGame::LoadSaveGame();

				m_LoadMostRecentSaveProgress = LOAD_PROGRESS_CHECK_LOAD;
			}
		}
		break;

	case LOAD_PROGRESS_CHECK_LOAD :
		{
			switch (CSavegameLoad::GenericLoad())
			{
			case MEM_CARD_COMPLETE:
				{
					camInterface::FadeOut(0);  // THIS FADE IS OK AS WE ARE STARTING A NEW GAME or loading

					CGame::HandleLoadedSaveGame();
					savegameDisplayf("CSavegameQueuedOperation_LoadMostRecentSave::Update - setting status to MEM_CARD_COMPLETE");
					m_Status = MEM_CARD_COMPLETE;
				}
				break;

			case MEM_CARD_BUSY: // the load is still in progress
				break;

			case MEM_CARD_ERROR:
				savegameDisplayf("CSavegameQueuedOperation_LoadMostRecentSave::Update - CSavegameLoad::GenericLoad() has failed so start a new session");
				m_Status = MEM_CARD_ERROR;
				break;
			}
		}
		break;
	}

	if (m_Status == MEM_CARD_ERROR)
	{
		CGame::SetStateToIdle();

		camInterface::FadeOut(0);  // THIS FADE IS OK AS WE ARE STARTING A NEW GAME or loading
		CGame::StartNewGame(CGameLogic::GetCurrentLevelIndex());
		return true;
	}

	if (m_Status == MEM_CARD_COMPLETE)
	{
		return true;
	}

	return false;
}

// End of LoadMostRecentSave *********************************************************************************************


// CreateBackupOfAutosaveSDM *********************************************************************************************

#if USE_SAVE_DATA_MEMORY && !USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP
void CSavegameQueuedOperation_CreateBackupOfAutosaveSDM::Init()
{
	m_SlotToLoadFrom = AUTOSAVE_SLOT_FOR_EPISODE_ZERO;
	m_SlotToSaveTo = (m_SlotToLoadFrom + TOTAL_NUMBER_OF_SLOTS_FOR_SAVEGAMES);

	m_BackupProgress = AUTOSAVE_BACKUP_PROGRESS_BEGIN_LOAD;
}

//    Return true to Pop() from queue
bool CSavegameQueuedOperation_CreateBackupOfAutosaveSDM::Update()
{
	switch (m_BackupProgress)
	{
		case AUTOSAVE_BACKUP_PROGRESS_BEGIN_LOAD :
			{
				if (MEM_CARD_ERROR == CSavegameList::BeginGameLoad(m_SlotToLoadFrom))
				{
					m_Status = MEM_CARD_ERROR;
				}
				else
				{
					m_BackupProgress = AUTOSAVE_BACKUP_PROGRESS_CHECK_LOAD;
				}
			}
			break;

		case AUTOSAVE_BACKUP_PROGRESS_CHECK_LOAD :
			{
				switch (CSavegameLoad::GenericLoad())
				{
				case MEM_CARD_COMPLETE:
					{
						savegameDisplayf("CSavegameQueuedOperation_CreateBackupOfAutosaveSDM::Update - load has succeeded");
						CSavegameLoad::EndGenericLoad();
						m_BackupProgress = AUTOSAVE_BACKUP_PROGRESS_BEGIN_SAVE;
					}
					break;
				case MEM_CARD_BUSY: // the load is still in progress
					break;
				case MEM_CARD_ERROR:
					{
						savegameDisplayf("CSavegameQueuedOperation_CreateBackupOfAutosaveSDM::Update - load has failed");
						m_Status = MEM_CARD_ERROR;
					}
					break;
				}
			}
			break;

		case AUTOSAVE_BACKUP_PROGRESS_BEGIN_SAVE :
			{
				switch (CSavegameSave::BeginGameSave(m_SlotToSaveTo, SAVEGAME_SINGLE_PLAYER))
				{
				case MEM_CARD_COMPLETE :
					m_BackupProgress = AUTOSAVE_BACKUP_PROGRESS_CHECK_SAVE;
					break;
				case MEM_CARD_BUSY :
					savegameAssertf(0, "CSavegameQueuedOperation_CreateBackupOfAutosaveSDM::Update - didn't expect CSavegameSave::BeginGameSave to ever return MEM_CARD_BUSY");
					m_Status = MEM_CARD_ERROR;
					break;
				case MEM_CARD_ERROR :
					savegameAssertf(0, "CSavegameQueuedOperation_CreateBackupOfAutosaveSDM::Update - CSavegameSave::BeginGameSave failed");
					m_Status = MEM_CARD_ERROR;
					break;
				}
			}
			break;

		case AUTOSAVE_BACKUP_PROGRESS_CHECK_SAVE :
			{
				switch (CSavegameSave::GenericSave())
				{
				case MEM_CARD_COMPLETE :
					savegameDisplayf("CSavegameQueuedOperation_CreateBackupOfAutosaveSDM::Update - save has succeeded");
					m_Status = MEM_CARD_COMPLETE;
					break;

				case MEM_CARD_BUSY :	 // the save is still in progress
					break;

				case MEM_CARD_ERROR :
					savegameDisplayf("CSavegameQueuedOperation_CreateBackupOfAutosaveSDM::Update - save has failed");
					m_Status = MEM_CARD_ERROR;
					break;
				}
			}
			break;

// 		case AUTOSAVE_BACKUP_PROGRESS_HANDLE_ERRORS :
// 			{
// 				if (CSavegameDialogScreens::HandleSaveGameErrorCode())
// 				{
// 					CSavegameDialogScreens::ClearSaveGameError();
// 					m_Status = MEM_CARD_ERROR;
// 				}
// 			}
// 			break;
	}

	if ( (m_Status == MEM_CARD_ERROR) || (m_Status == MEM_CARD_COMPLETE) )
	{
		CGenericGameStorage::FreeBufferContainingContentsOfSavegameFile(SAVEGAME_SINGLE_PLAYER);

		return true;
	}

	return false;
}

#endif	//	USE_SAVE_DATA_MEMORY && !USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP

// End of CreateBackupOfAutosaveSDM *********************************************************************************************



#if __ALLOW_EXPORT_OF_SP_SAVEGAMES

// ExportSinglePlayerSavegame *********************************************************************************************

// Based on CSavegameQueuedOperation_ManualLoad
void CSavegameQueuedOperation_ExportSinglePlayerSavegame::Init()
{
	m_ExportSpSaveProgress = EXPORT_SP_SAVE_PROGRESS_BEGIN_SAVEGAME_LIST;

	m_bQuitAsSoonAsNoSavegameOperationsAreRunning = false;
	m_bNeedToCallEndGenericLoad = false;
}


bool CSavegameQueuedOperation_ExportSinglePlayerSavegame::Update()
{
	switch (m_ExportSpSaveProgress)
	{
	case EXPORT_SP_SAVE_PROGRESS_BEGIN_SAVEGAME_LIST :
		CSavegameDialogScreens::SetDialogBoxHeading("SG_HDNG");	//	SG_HDG_LOAD

		CSaveGameExport::SetIsExporting(true);

		if (!CSavegameFrontEnd::BeginSaveGameList())
		{
			Assertf(0, "CSavegameQueuedOperation_ExportSinglePlayerSavegame::Update - CSavegameFrontEnd::BeginSaveGameList failed");
			CPauseMenu::Close();
			savegameDisplayf("CSavegameQueuedOperation_ExportSinglePlayerSavegame::Update - setting status to MEM_CARD_ERROR (1)");
			m_Status = MEM_CARD_ERROR;
		}
		else
		{
			m_ExportSpSaveProgress = EXPORT_SP_SAVE_PROGRESS_CHECK_SAVEGAME_LIST;
		}
		break;

	case EXPORT_SP_SAVE_PROGRESS_CHECK_SAVEGAME_LIST :
		{
			switch (CSavegameFrontEnd::SaveGameMenuCheck(m_bQuitAsSoonAsNoSavegameOperationsAreRunning))
			{
			case MEM_CARD_COMPLETE :
				{
					savegameAssertf(!CSavegameList::IsSaveGameSlotEmpty(CSavegameFrontEnd::GetSaveGameSlotToExport()), "CSavegameQueuedOperation_ExportSinglePlayerSavegame::Update - didn't expect slot %d to be empty", CSavegameFrontEnd::GetSaveGameSlotToExport());
					savegameAssertf(!CSavegameList::GetIsDamaged(CSavegameFrontEnd::GetSaveGameSlotToExport()), "CSavegameQueuedOperation_ExportSinglePlayerSavegame::Update - didn't expect slot %d to be damaged", CSavegameFrontEnd::GetSaveGameSlotToExport());
					m_ExportSpSaveProgress = EXPORT_SP_SAVE_PROGRESS_BEGIN_LOAD;
				}
				break;

			case MEM_CARD_BUSY :	//	still processing, do nothing
				break;

			case MEM_CARD_ERROR :
				CPauseMenu::BackOutOfUploadSavegamePanes();
				savegameDisplayf("CSavegameQueuedOperation_ExportSinglePlayerSavegame::Update - setting status to MEM_CARD_ERROR (2)");
				m_Status = MEM_CARD_ERROR;
				break;
			}
		}
		break;

	case EXPORT_SP_SAVE_PROGRESS_BEGIN_LOAD :
		if (CSavegameList::BeginGameLoadForExport(CSavegameFrontEnd::GetSaveGameSlotToExport()) == MEM_CARD_ERROR)
		{
			CPauseMenu::AbandonLoadingOfSavedGame();	//	so clear bCloseAndStartSavedGame
			savegameDisplayf("CSavegameQueuedOperation_ExportSinglePlayerSavegame::Update - setting status to MEM_CARD_ERROR (3)");
			m_Status = MEM_CARD_ERROR;
		}
		else
		{
			m_bNeedToCallEndGenericLoad = true;
			m_ExportSpSaveProgress = EXPORT_SP_SAVE_PROGRESS_CHECK_LOAD;
		}
		break;

	case EXPORT_SP_SAVE_PROGRESS_CHECK_LOAD :
		{
			switch (CSavegameLoad::GenericLoad())
			{
			case MEM_CARD_COMPLETE:
				{
					savegameDisplayf("CSavegameQueuedOperation_ExportSinglePlayerSavegame::Update - load complete - moving on to BEGIN_EXPORT");
					//	Should I call CSavegameLoad::EndGenericLoad(); here? The disabled code in MANUAL_LOAD_PROGRESS_CHECK_LOAD_FOR_CLOUD_UPLOAD does.
					m_ExportSpSaveProgress = EXPORT_SP_SAVE_PROGRESS_BEGIN_EXPORT;
				}
				break;
			case MEM_CARD_BUSY: // the load is still in progress
				break;
			case MEM_CARD_ERROR:
				{
					CPauseMenu::AbandonLoadingOfSavedGame();	//	so clear bCloseAndStartSavedGame
					//	Should I call CSavegameLoad::EndGenericLoad(); here? The disabled code in MANUAL_LOAD_PROGRESS_CHECK_LOAD_FOR_CLOUD_UPLOAD does.
					savegameDisplayf("CSavegameQueuedOperation_ExportSinglePlayerSavegame::Update - setting status to MEM_CARD_ERROR (4)");
					m_Status = MEM_CARD_ERROR;
				}
				break;
			}
		}
		break;

	case EXPORT_SP_SAVE_PROGRESS_BEGIN_EXPORT :
		{
			if (CSaveGameExport::BeginExport())
			{
				savegameDisplayf("CSavegameQueuedOperation_ExportSinglePlayerSavegame::Update - BEGIN_EXPORT complete - moving on to CHECK_EXPORT");
				m_ExportSpSaveProgress = EXPORT_SP_SAVE_PROGRESS_CHECK_EXPORT;
			}
			else
			{
				CPauseMenu::AbandonLoadingOfSavedGame();	//	so clear bCloseAndStartSavedGame
				savegameDisplayf("CSavegameQueuedOperation_ExportSinglePlayerSavegame::Update - setting status to MEM_CARD_ERROR (5)");
				m_Status = MEM_CARD_ERROR;
			}
		}
		break;

	case EXPORT_SP_SAVE_PROGRESS_CHECK_EXPORT :
		{
			switch (CSaveGameExport::UpdateExport())
			{
			case MEM_CARD_COMPLETE:
				{
					savegameDisplayf("CSavegameQueuedOperation_ExportSinglePlayerSavegame::Update - CHECK_EXPORT - setting MEM_CARD_COMPLETE");
					m_Status = MEM_CARD_COMPLETE;
				}
				break;
			case MEM_CARD_BUSY: // the export is still in progress
				break;
			case MEM_CARD_ERROR:
				{
					CPauseMenu::AbandonLoadingOfSavedGame();	//	so clear bCloseAndStartSavedGame
					savegameDisplayf("CSavegameQueuedOperation_ExportSinglePlayerSavegame::Update - setting status to MEM_CARD_ERROR (6)");
					m_Status = MEM_CARD_ERROR;
				}
				break;
			}
		}
		break;
	}


	if ( (m_Status == MEM_CARD_ERROR) || (m_Status == MEM_CARD_COMPLETE) )
	{
		CSaveGameExport::SetIsExporting(false);

		CSaveGameExport::FreeAllData();

		if (m_bNeedToCallEndGenericLoad)
		{
			m_bNeedToCallEndGenericLoad = false;
			CSavegameLoad::EndGenericLoad();
		}

		if (m_bQuitAsSoonAsNoSavegameOperationsAreRunning)
		{
			return true;
		}
		else
		{
			m_ExportSpSaveProgress = EXPORT_SP_SAVE_PROGRESS_BEGIN_SAVEGAME_LIST;
			savegameDisplayf("CSavegameQueuedOperation_ExportSinglePlayerSavegame::Update - setting status to MEM_CARD_BUSY (1)");
			m_Status = MEM_CARD_BUSY;
		}
	}

	return false;
}

#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

// End of ExportSinglePlayerSavegame *********************************************************************************************

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES

// ImportSinglePlayerSavegame *********************************************************************************************

void CSavegameQueuedOperation_ImportSinglePlayerSavegame::Init()
{
	m_ImportSpSaveProgress = IMPORT_SP_SAVE_PROGRESS_BEGIN_SAVEGAME_LIST;
	m_bImportSucceeded = false;

	m_bQuitAsSoonAsNoSavegameOperationsAreRunning = false;
}

//	Return true to Pop() from queue
bool CSavegameQueuedOperation_ImportSinglePlayerSavegame::Update()
{
	switch (m_ImportSpSaveProgress)
	{
	case IMPORT_SP_SAVE_PROGRESS_BEGIN_SAVEGAME_LIST :
		{
			CSavegameDialogScreens::SetDialogBoxHeading("SG_HDNG");	//	SG_HDG_SAVE

			CSaveGameImport::SetIsImporting(true);

			if (!CSavegameFrontEnd::BeginSaveGameList())
			{
				savegameAssertf(0, "CSavegameQueuedOperation_ImportSinglePlayerSavegame::Update - CSavegameFrontEnd::BeginSaveGameList failed");
				CPauseMenu::Close();
				savegameDisplayf("CSavegameQueuedOperation_ImportSinglePlayerSavegame::Update - setting status to MEM_CARD_ERROR (1)");
				m_Status = MEM_CARD_ERROR;
			}
			else
			{
				if (!CPauseMenu::GetNoValidSaveGameFiles())	//	sm_bNoValidSaveGameFiles will be true if the PS3 hard disk has been 
					//	scanned previously and there are no valid save games on it
				{
					CPauseMenu::LockMenuControl(true);
				}
				m_ImportSpSaveProgress = IMPORT_SP_SAVE_PROGRESS_CHECK_SAVEGAME_LIST;
			}
		}
		break;

	case IMPORT_SP_SAVE_PROGRESS_CHECK_SAVEGAME_LIST :
		{
			switch (CSavegameFrontEnd::SaveGameMenuCheck(m_bQuitAsSoonAsNoSavegameOperationsAreRunning))
			{
			case MEM_CARD_COMPLETE :
				{
					m_ImportSpSaveProgress = IMPORT_SP_SAVE_PROGRESS_BEGIN_IMPORT;
				}
				break;

			case MEM_CARD_BUSY :	//	still processing, do nothing
				break;

			case MEM_CARD_ERROR :
				CPauseMenu::UnlockMenuControl();	//	or should this be CPauseMenu::BackOutOfSaveGameList()?
				CPauseMenu::Close();
				savegameDisplayf("CSavegameQueuedOperation_ImportSinglePlayerSavegame::Update - setting status to MEM_CARD_ERROR (2)");
				m_Status = MEM_CARD_ERROR;
				break;
			}
		}
		break;

	case IMPORT_SP_SAVE_PROGRESS_BEGIN_IMPORT :
		{
			if (CSaveGameImport::BeginImport())
			{
				savegameDisplayf("CSavegameQueuedOperation_ImportSinglePlayerSavegame::Update - BEGIN_IMPORT complete - moving on to CHECK_IMPORT");
				m_ImportSpSaveProgress = IMPORT_SP_SAVE_PROGRESS_CHECK_IMPORT;
			}
			else
			{
				savegameErrorf("CSavegameQueuedOperation_ImportSinglePlayerSavegame::Update - CSaveGameImport::BeginImport() failed - Display an error message");
				m_ImportSpSaveProgress = IMPORT_SP_SAVE_PROGRESS_DISPLAY_RESULT_MESSAGE;
			}
		}
		break;

	case IMPORT_SP_SAVE_PROGRESS_CHECK_IMPORT :
		{
			switch (CSaveGameImport::UpdateImport())
			{
			case MEM_CARD_COMPLETE:
				{
					savegameDisplayf("CSavegameQueuedOperation_ImportSinglePlayerSavegame::Update - CSaveGameImport::UpdateImport() completed - moving on to DISPLAY_RESULT_MESSAGE");
					m_bImportSucceeded = true;
					m_ImportSpSaveProgress = IMPORT_SP_SAVE_PROGRESS_DISPLAY_RESULT_MESSAGE;

#if GEN9_STANDALONE_ENABLED
					CSaveMigration::GetSingleplayer().SetShouldMarkSingleplayerMigrationAsComplete();
#endif // GEN9_STANDALONE_ENABLED
				}
				break;
			case MEM_CARD_BUSY: // the import is still in progress
				break;
			case MEM_CARD_ERROR:
				{
					savegameErrorf("CSavegameQueuedOperation_ImportSinglePlayerSavegame::Update - CSaveGameImport::UpdateImport() failed - Display an error message");
					m_ImportSpSaveProgress = IMPORT_SP_SAVE_PROGRESS_DISPLAY_RESULT_MESSAGE;
				}
				break;
			}
		}
		break;

	case IMPORT_SP_SAVE_PROGRESS_DISPLAY_RESULT_MESSAGE :
		{
			bool ReturnSelection = false;
			if (m_bImportSucceeded)
			{
				if (CSavegameDialogScreens::HandleSaveGameDialogBox(SAVE_GAME_DIALOG_IMPORT_OF_SAVEGAME_SUCCEEDED, 0, &ReturnSelection))
				{
					CPauseMenu::Close();
					m_Status = MEM_CARD_COMPLETE;
				}
			}
			else
			{
				if (CSavegameDialogScreens::HandleSaveGameDialogBox(SAVE_GAME_DIALOG_IMPORT_OF_SAVEGAME_FAILED, 0, &ReturnSelection))
				{
					CPauseMenu::Close();
					m_Status = MEM_CARD_ERROR;
				}
			}
		}
		break;
	}


	if ( (m_Status == MEM_CARD_ERROR) || (m_Status == MEM_CARD_COMPLETE) )
	{
		CSaveGameImport::SetIsImporting(false);

		// CSaveGameImport::UpdateImport() should have dealt with freeing any data that it had allocated

		return true;
	}

	return false;
}

// End of ImportSinglePlayerSavegame *********************************************************************************************

// ImportSinglePlayerSavegameIntoFixedSlot *********************************************************************************************

void CSavegameQueuedOperation_ImportSinglePlayerSavegameIntoFixedSlot::Init()
{
	m_ImportSpSaveIntoFixedSlotProgress = IMPORT_SP_SAVE_INTO_FIXED_SLOT_PROGRESS_BEGIN_IMPORT;
	m_bImportSucceeded = false;
	m_bLoadingOfImportedSaveSucceeded = false;

	m_bQuitAsSoonAsNoSavegameOperationsAreRunning = false;
}

bool CSavegameQueuedOperation_ImportSinglePlayerSavegameIntoFixedSlot::CopyDataImmediately(const u8 *pData, u32 dataSize)
{
	return CSaveGameImport::CreateCopyOfDownloadedData(pData, dataSize);
}

//	Return true to Pop() from queue
bool CSavegameQueuedOperation_ImportSinglePlayerSavegameIntoFixedSlot::Update()
{
	const s32 slotToUseForImportedSave = (NUM_MANUAL_SAVE_SLOTS - 1);
	switch (m_ImportSpSaveIntoFixedSlotProgress)
	{
		case IMPORT_SP_SAVE_INTO_FIXED_SLOT_PROGRESS_BEGIN_IMPORT:
		{
			CSaveGameImport::SetIsImporting(true);

			// Rather than allowing the player to choose a slot, or finding a free slot, we just use the last manual 
			//		slot (even if it already contains a savegame)
			CSavegameFrontEnd::SetSaveGameSlotForImporting(slotToUseForImportedSave);

			if (CSaveGameImport::BeginImportOfDownloadedData())
			{
				savegameDisplayf("CSavegameQueuedOperation_ImportSinglePlayerSavegameIntoFixedSlot::Update - BEGIN_IMPORT complete - moving on to CHECK_IMPORT");
				m_ImportSpSaveIntoFixedSlotProgress = IMPORT_SP_SAVE_INTO_FIXED_SLOT_PROGRESS_CHECK_IMPORT;
			}
			else
			{
				savegameErrorf("CSavegameQueuedOperation_ImportSinglePlayerSavegameIntoFixedSlot::Update - CSaveGameImport::BeginImportOfDownloadedData() failed - Display an error message");
				m_ImportSpSaveIntoFixedSlotProgress = IMPORT_SP_SAVE_INTO_FIXED_SLOT_PROGRESS_DISPLAY_ERROR_MESSAGE;
			}
		}
		break;

		case IMPORT_SP_SAVE_INTO_FIXED_SLOT_PROGRESS_CHECK_IMPORT:
		{
			switch (CSaveGameImport::UpdateImport())
			{
				case MEM_CARD_COMPLETE:
				{
					CSaveGameImport::SetIsImporting(false);

					savegameDisplayf("CSavegameQueuedOperation_ImportSinglePlayerSavegameIntoFixedSlot::Update - CSaveGameImport::UpdateImport() completed - moving on to BEGIN_LOAD");
					m_bImportSucceeded = true;
					m_ImportSpSaveIntoFixedSlotProgress = IMPORT_SP_SAVE_INTO_FIXED_SLOT_PROGRESS_BEGIN_LOAD;

#if GEN9_STANDALONE_ENABLED
					CSaveMigration::GetSingleplayer().SetShouldMarkSingleplayerMigrationAsComplete();
#endif // GEN9_STANDALONE_ENABLED
				}
				break;

				case MEM_CARD_BUSY: // the import is still in progress
					break;

				case MEM_CARD_ERROR:
				{
					CSaveGameImport::SetIsImporting(false);

					savegameErrorf("CSavegameQueuedOperation_ImportSinglePlayerSavegameIntoFixedSlot::Update - CSaveGameImport::UpdateImport() failed - Display an error message");
					m_ImportSpSaveIntoFixedSlotProgress = IMPORT_SP_SAVE_INTO_FIXED_SLOT_PROGRESS_DISPLAY_ERROR_MESSAGE;
				}
				break;
			}
		}
		break;

		case IMPORT_SP_SAVE_INTO_FIXED_SLOT_PROGRESS_BEGIN_LOAD :
		{
			camInterface::FadeOut(0);
			CLoadingScreens::Init(LOADINGSCREEN_CONTEXT_LOADLEVEL, 0);

			// pause streaming any other misc processing
			CStreaming::SetIsPlayerPositioned(false);

			CSavegameDialogScreens::SetDialogBoxHeading("SG_HDNG");

			if (CSavegameList::BeginGameLoad(slotToUseForImportedSave) == MEM_CARD_ERROR)
			{
				savegameDisplayf("CSavegameQueuedOperation_ImportSinglePlayerSavegameIntoFixedSlot::Update - CSavegameList::BeginGameLoad() has failed so display an error message and start a new session");
				m_ImportSpSaveIntoFixedSlotProgress = IMPORT_SP_SAVE_INTO_FIXED_SLOT_PROGRESS_DISPLAY_ERROR_MESSAGE;
			}
			else
			{
				CGame::LoadSaveGame();

				m_ImportSpSaveIntoFixedSlotProgress = IMPORT_SP_SAVE_INTO_FIXED_SLOT_PROGRESS_CHECK_LOAD;
			}
		}
		break;

		case IMPORT_SP_SAVE_INTO_FIXED_SLOT_PROGRESS_CHECK_LOAD :
		{
			switch (CSavegameLoad::GenericLoad())
			{
			case MEM_CARD_COMPLETE:
			{
				m_bLoadingOfImportedSaveSucceeded = true;

				camInterface::FadeOut(0);

				CGame::HandleLoadedSaveGame();
				savegameDisplayf("CSavegameQueuedOperation_ImportSinglePlayerSavegameIntoFixedSlot::Update - setting status to MEM_CARD_COMPLETE");
				m_Status = MEM_CARD_COMPLETE;
			}
			break;

			case MEM_CARD_BUSY: // the load is still in progress
				break;

			case MEM_CARD_ERROR:
				savegameDisplayf("CSavegameQueuedOperation_ImportSinglePlayerSavegameIntoFixedSlot::Update - CSavegameLoad::GenericLoad() has failed so display an error message and start a new session");
				m_ImportSpSaveIntoFixedSlotProgress = IMPORT_SP_SAVE_INTO_FIXED_SLOT_PROGRESS_DISPLAY_ERROR_MESSAGE;
				break;
			}
		}
		break;

		case IMPORT_SP_SAVE_INTO_FIXED_SLOT_PROGRESS_DISPLAY_ERROR_MESSAGE :
		{
			bool ReturnSelection = false;
			if (m_bImportSucceeded && !m_bLoadingOfImportedSaveSucceeded)
			{
				if (CSavegameDialogScreens::HandleSaveGameDialogBox(SAVE_GAME_DIALOG_LOADING_OF_IMPORTED_SAVEGAME_FAILED, 0, &ReturnSelection))
				{
//					CPauseMenu::Close();
					m_Status = MEM_CARD_ERROR;
				}
			}
			else
			{
				if (CSavegameDialogScreens::HandleSaveGameDialogBox(SAVE_GAME_DIALOG_IMPORT_OF_SAVEGAME_FAILED, 0, &ReturnSelection))
				{
//					CPauseMenu::Close();
					m_Status = MEM_CARD_ERROR;
				}
			}

			if (m_Status == MEM_CARD_ERROR)
			{
				CGame::SetStateToIdle();

				camInterface::FadeOut(0);
				CGame::StartNewGame(CGameLogic::GetCurrentLevelIndex());
			}
		}
		break;
	}


	if ((m_Status == MEM_CARD_ERROR) || (m_Status == MEM_CARD_COMPLETE))
	{
		CSaveGameImport::SetIsImporting(false);

		// CSaveGameImport::UpdateImport() should have dealt with freeing any data that it had allocated

		return true;
	}

	return false;
}

// End of ImportSinglePlayerSavegameIntoFixedSlot *********************************************************************************************

#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES


