/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : StoryModeSaveMigrationSaveGameItem.h
// PURPOSE : PageLayoutItem wrapper to populate migrationSaveGame Scaleform.
//
// AUTHOR  : stephen.phillips
// STARTED : August 2021
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef STORY_MODE_SAVE_MIGRATION_SAVE_GAME_ITEM
#define STORY_MODE_SAVE_MIGRATION_SAVE_GAME_ITEM

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// framework
#include "atl/array.h"

// game
#include "frontend/page_deck/layout/PageLayoutItem.h"
#include "frontend/save_migration/SaveMigrationDefs.h"

class CStoryModeSaveMigrationSaveGameItem final : public CPageLayoutItem
{
public:
	CStoryModeSaveMigrationSaveGameItem();

	void SetDetails(atHashString titleHash, atHashString descriptionHash);
	void SetDetailsLiteral(const char* title, const char* description);
	void AddSaveGame(const SaveMigrationDefs::StoryModeSaveGameEntry& saveGameData);
	int GetFocusedIndex() const { return m_focusedIndex; }
	void ChangeFocus(int delta);
	void SetFocusedEntry(int index);
	void Reset();

private: // declarations and variables
	NON_COPYABLE(CStoryModeSaveMigrationSaveGameItem);

	rage::atArray<SaveMigrationDefs::StoryModeSaveGameEntry> m_saveGames;
	int m_focusedIndex;

private: // methods
	char const * GetSymbol() const override { return "migrationSaveGame"; }

	void PopulateSaveDescription(const SaveMigrationDefs::StoryModeSaveGameEntry& entry);
	void SetTabName(int index, const char* name);
	void SetEntryState(int index, bool enabled);

	void ShutdownDerived() final;
};

#endif // UI_PAGE_DECK_ENABLED

#endif // STORY_MODE_SAVE_MIGRATION_SAVE_GAME_ITEM
