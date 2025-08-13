#include "SaveMigrationWaitingForProcessPages.h"

#if GEN9_LANDING_PAGE_ENABLED

#include "SaveMigrationWaitingForProcessPages_parser.h"

// rage
#include "file/savegame.h"

// game
#include "SaveLoad/savegame_autoload.h"
#include "SaveLoad/savegame_users_and_devices.h"
#include "SaveMigration/SaveMigration.h"

FWUI_DEFINE_TYPE(CFetchingMultiplayerAccountPage, 0xD69D2FFF);
FWUI_DEFINE_TYPE(CProcessingMultiplayerAccountMigrationPage, 0xC1AF26F5);
FWUI_DEFINE_TYPE(CLoadingLocalSingleplayerSavesPage, 0x9486A827);
FWUI_DEFINE_TYPE(CFetchingSingleplayerSavesPage, 0xEB30307E);
FWUI_DEFINE_TYPE(CDownloadingSingleplayerSavePage, 0xB9C88630);
FWUI_DEFINE_TYPE(CCompletingSingleplayerMigrationPage, 0xDB3BB671);

void CFetchingMultiplayerAccountPage::OnEnterStartDerived()
{
	CSaveMigration::GetMultiplayer().RequestAccounts();
}

bool CFetchingMultiplayerAccountPage::HasProcessCompletedDerived() const
{
	const bool c_isComplete = (CSaveMigration::GetMultiplayer().GetAccountsStatus() != SM_STATUS_PENDING) && (CSaveMigration::GetMultiplayer().GetAccountsStatus() != SM_STATUS_INPROGRESS);
	return c_isComplete;
}

//////////////////////////////////////////////////////////////////////////

bool CProcessingMultiplayerAccountMigrationPage::HasProcessCompletedDerived() const
{
	const bool c_isComplete = CSaveMigration::GetMultiplayer().GetMigrationStatus() == SM_STATUS_OK;
	return c_isComplete;
}

//////////////////////////////////////////////////////////////////////////

int CLoadingLocalSingleplayerSavesPage::sm_saveGameSlotsCount = 0;

int CLoadingLocalSingleplayerSavesPage::GetNumberOfEnumeratedSaves()
{
	return sm_saveGameSlotsCount;
}

CLoadingLocalSingleplayerSavesPage::CLoadingLocalSingleplayerSavesPage() :
	superclass(),
	m_isRunning(false)
{
}

void CLoadingLocalSingleplayerSavesPage::OnEnterStartDerived()
{
	// Lifted from CSavegameAutoload::AutoLoadEnumerateContent()
	const int c_userIndex = CSavegameUsers::GetUser();
	SAVEGAME.SetSelectedDeviceToAny(c_userIndex);
	SAVEGAME.BeginEnumeration(c_userIndex, CGenericGameStorage::GetContentType(), &m_saveGameSlots[0], sm_maxNumberOfSaveFilesToEnumerate);
	m_isRunning = true;
}

void CLoadingLocalSingleplayerSavesPage::UpdateDerived(const float msTick)
{
	superclass::UpdateDerived(msTick);

	// returns true for error or success, false for busy
	if (SAVEGAME.CheckEnumeration(CSavegameUsers::GetUser(), sm_saveGameSlotsCount))
	{
		SAVEGAME.EndEnumeration(CSavegameUsers::GetUser());
		DiscardIrrelevantResults();
		m_isRunning = false;
	}
}

bool CLoadingLocalSingleplayerSavesPage::HasProcessCompletedDerived() const
{
	const bool c_isComplete = !m_isRunning;
	return c_isComplete;
}

void CLoadingLocalSingleplayerSavesPage::DiscardIrrelevantResults()
{
	const char *pProfileFilename = "Profile";
#if USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP
	const char *pAutosaveBackupFilename = "SBGTA50015";
#endif // USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP

	const int loopEnd = sm_saveGameSlotsCount;
	for (int loop = 0; loop < loopEnd; loop++)
	{
		if (!strnicmp(m_saveGameSlots[loop].Filename, pProfileFilename, strlen(pProfileFilename)))
		{
			sm_saveGameSlotsCount--;
		}

#if USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP
		if (!strnicmp(m_saveGameSlots[loop].Filename, pAutosaveBackupFilename, strlen(pAutosaveBackupFilename)))
		{
			sm_saveGameSlotsCount--;
		}
#endif	//	USE_DOWNLOAD0_FOR_AUTOSAVE_BACKUP
	}
}

//////////////////////////////////////////////////////////////////////////

void CFetchingSingleplayerSavesPage::OnEnterStartDerived()
{
	CSaveMigration::GetSingleplayer().RequestAvailableSingleplayerSaves();
}

bool CFetchingSingleplayerSavesPage::HasProcessCompletedDerived() const
{
	const bool c_isComplete = CSaveMigration::GetSingleplayer().HasCompletedRequestAvailableSingleplayerSaves();
	return c_isComplete;
}

//////////////////////////////////////////////////////////////////////////

void CDownloadingSingleplayerSavePage::OnEnterStartDerived()
{
	const rlSaveMigrationRecordMetadata& c_recordMetadata = CSaveMigration::GetSingleplayer().GetChosenSingleplayerSave();
	CSaveMigration::GetSingleplayer().RequestDownloadSingleplayerSave(c_recordMetadata);
}

bool CDownloadingSingleplayerSavePage::HasProcessCompletedDerived() const
{
	const bool c_isComplete = CSaveMigration::GetSingleplayer().HasCompletedRequestDownloadSingleplayerSave();
	return c_isComplete;
}

//////////////////////////////////////////////////////////////////////////

void CCompletingSingleplayerMigrationPage::OnEnterStartDerived()
{
	const rlSaveMigrationRecordMetadata& c_recordMetadata = CSaveMigration::GetSingleplayer().GetChosenSingleplayerSave();
	CSaveMigration::GetSingleplayer().RequestCompleteSingleplayerMigration(c_recordMetadata);
}

bool CCompletingSingleplayerMigrationPage::HasProcessCompletedDerived() const
{
	const bool c_isComplete = CSaveMigration::GetSingleplayer().HasCompletedRequestCompleteSingleplayerMigration();
	return c_isComplete;
}

#endif // GEN9_LANDING_PAGE_ENABLED
