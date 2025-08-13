/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : SaveMigrationWaitingForProcessPages.h
// PURPOSE : Contains the implementations of WaitingForProcessPage
// 
// AUTHOR  : stephen.phillips
// STARTED : August 2021
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef SAVE_MIGRATION_WAITING_FOR_PROCESS_PAGES
#define SAVE_MIGRATION_WAITING_FOR_PROCESS_PAGES

#include "frontend/landing_page/LandingPageConfig.h"
#include "frontend/save_migration/pages/WaitingForProcessPage.h"

#if GEN9_LANDING_PAGE_ENABLED

class CFetchingMultiplayerAccountPage : public CWaitingForProcessPage
{
	FWUI_DECLARE_DERIVED_TYPE(CFetchingMultiplayerAccountPage, CWaitingForProcessPage);
public:
	CFetchingMultiplayerAccountPage() : superclass() { }
private: // declarations and variables		
	void OnEnterStartDerived() final;
	bool HasProcessCompletedDerived() const final;

	NON_COPYABLE(CFetchingMultiplayerAccountPage);
	PAR_PARSABLE;
};

class CProcessingMultiplayerAccountMigrationPage final : public CWaitingForProcessPage
{
	FWUI_DECLARE_DERIVED_TYPE(CProcessingMultiplayerAccountMigrationPage, CWaitingForProcessPage);
public:
	CProcessingMultiplayerAccountMigrationPage() : superclass() { }
private: // declarations and variables		
	bool HasProcessCompletedDerived() const final;
	NON_COPYABLE(CProcessingMultiplayerAccountMigrationPage);
	PAR_PARSABLE;
};

class CLoadingLocalSingleplayerSavesPage final : public CWaitingForProcessPage
{
	FWUI_DECLARE_DERIVED_TYPE(CLoadingLocalSingleplayerSavesPage, CWaitingForProcessPage);
public:
	CLoadingLocalSingleplayerSavesPage();

	static int GetNumberOfEnumeratedSaves();

private: // declarations and variables
	static int sm_saveGameSlotsCount;

	// Set this to 3 so that we still catch the existence of at least one valid save in the case where the player also has the following - 
	// 1. A savegame file called "Profile". It contains various per-player settings.
	//		On PS5, it looks like SAVEGAME.BeginEnumeration() doesn't add "Profile" to the array of results.
	//		On XBSX, "Profile" is added to the array of results.
	// 2. On PS5, a backup of their autosave. This is stored in download0. It's not in the same folder as the other savegame files.
	static const int sm_maxNumberOfSaveFilesToEnumerate = 3;
	fiSaveGame::Content m_saveGameSlots[sm_maxNumberOfSaveFilesToEnumerate];
	bool m_isRunning;

	PAR_PARSABLE;
	NON_COPYABLE(CLoadingLocalSingleplayerSavesPage);

private: // methods
	void OnEnterStartDerived() final;
	void UpdateDerived(const float UNUSED_PARAM(msTick)) final;
	bool HasProcessCompletedDerived() const final;

	void DiscardIrrelevantResults();
};

class CFetchingSingleplayerSavesPage final : public CWaitingForProcessPage
{
	FWUI_DECLARE_DERIVED_TYPE(CFetchingSingleplayerSavesPage, CWaitingForProcessPage);
public:
	CFetchingSingleplayerSavesPage() : superclass() { }

private: // declarations and variables
	PAR_PARSABLE;
	NON_COPYABLE(CFetchingSingleplayerSavesPage);

private: // methods
	void OnEnterStartDerived() final;
	bool HasProcessCompletedDerived() const final;
};

class CDownloadingSingleplayerSavePage final : public CWaitingForProcessPage
{
	FWUI_DECLARE_DERIVED_TYPE(CDownloadingSingleplayerSavePage, CWaitingForProcessPage);
public:
	CDownloadingSingleplayerSavePage() : superclass() { }

private: // declarations and variables
	PAR_PARSABLE;
	NON_COPYABLE(CDownloadingSingleplayerSavePage);

private: // methods
	void OnEnterStartDerived() final;
	bool HasProcessCompletedDerived() const final;
};

class CCompletingSingleplayerMigrationPage final : public CWaitingForProcessPage
{
	FWUI_DECLARE_DERIVED_TYPE(CCompletingSingleplayerMigrationPage, CWaitingForProcessPage);
public:
	CCompletingSingleplayerMigrationPage() : superclass() { }

private: // declarations and variables
	PAR_PARSABLE;
	NON_COPYABLE(CCompletingSingleplayerMigrationPage);

private: // methods
	void OnEnterStartDerived() final;
	bool HasProcessCompletedDerived() const final;
};

#endif // GEN9_LANDING_PAGE_ENABLED

#endif // SAVE_MIGRATION_WAITING_FOR_PROCESS_PAGES
