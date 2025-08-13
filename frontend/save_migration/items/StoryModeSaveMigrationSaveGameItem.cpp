#include "StoryModeSaveMigrationSaveGameItem.h"

#if UI_PAGE_DECK_ENABLED

// game
#include "frontend/page_deck/items/uiCloudSupport.h"
#include "frontend/ui_channel.h"

CStoryModeSaveMigrationSaveGameItem::CStoryModeSaveMigrationSaveGameItem() :
	m_focusedIndex(-1)
{
}

void CStoryModeSaveMigrationSaveGameItem::SetDetails(atHashString titleHash, atHashString descriptionHash)
{
	char const * const c_titleLiteral = TheText.Get(titleHash);
	char const * const c_descriptionLiteral = TheText.Get(descriptionHash);
	SetDetailsLiteral(c_titleLiteral, c_descriptionLiteral);
}

void CStoryModeSaveMigrationSaveGameItem::SetDetailsLiteral(const char* title, const char* description)
{
	CComplexObject& displayObject = GetDisplayObject();
	if (displayObject.IsActive())
	{
		CScaleformMgr::BeginMethodOnComplexObject(displayObject.GetId(), SF_BASE_CLASS_GENERIC, "SET_ITEM_DETAILS");
		CScaleformMgr::AddParamString(title, false);					// Title
		CScaleformMgr::AddParamString(description, false);				// Description
		CScaleformMgr::AddParamString("FRONTEND_LANDING_BASE", false);	// Tag TXD
		CScaleformMgr::AddParamString("MIGRATION_STORY_LOGO", false);	// Tag Texture
		CScaleformMgr::AddParamBool(true);								// Is Rich text
		CScaleformMgr::EndMethod();
	}
}

void CStoryModeSaveMigrationSaveGameItem::AddSaveGame(const SaveMigrationDefs::StoryModeSaveGameEntry& saveGameData)
{
	const int c_saveIndex = m_saveGames.GetCount();

	m_saveGames.PushAndGrow(saveGameData);

	SetTabName(c_saveIndex, saveGameData.GetAccountName());

	// Populate and set focus to first entry
	if (m_focusedIndex == -1)
	{
		SetFocusedEntry(c_saveIndex);
	}
}

void CStoryModeSaveMigrationSaveGameItem::ChangeFocus(int delta)
{
	int targetIndex = m_focusedIndex + delta;
	if (targetIndex < 0)
	{
		targetIndex = 0;
	}
	else if (targetIndex >= m_saveGames.GetCount())
	{
		targetIndex = m_saveGames.GetCount() - 1;
	}

	SetFocusedEntry(targetIndex);
}

void CStoryModeSaveMigrationSaveGameItem::SetFocusedEntry(int index)
{
	if (m_focusedIndex != index
		&& !m_saveGames.empty()
		&& uiVerifyf(index >= 0 && index < m_saveGames.GetCount(), "Attempting to set focus on invalid index %d", index))
	{
		if (m_focusedIndex != -1)
		{
			// Disable previous entry (if any)
			SetEntryState(m_focusedIndex, false);
		}

		m_focusedIndex = index;

		// Enable new entry
		SetEntryState(m_focusedIndex, true);

		const SaveMigrationDefs::StoryModeSaveGameEntry& c_entry = m_saveGames[m_focusedIndex];
		PopulateSaveDescription(c_entry);
	}
}

void CStoryModeSaveMigrationSaveGameItem::Reset()
{
	// Clear account tab labels
	const int c_saveGamesCount = m_saveGames.GetCount();
	for (int i = 0; i < c_saveGamesCount; ++i)
	{
		SetTabName(i, "");
	}

	// Clear focus
	SetEntryState(m_focusedIndex, false);
	m_focusedIndex = -1;

	// Clear description
	SaveMigrationDefs::StoryModeSaveGameEntry emptySave;
	PopulateSaveDescription(emptySave);

	m_saveGames.Reset();
}

void CStoryModeSaveMigrationSaveGameItem::PopulateSaveDescription(const SaveMigrationDefs::StoryModeSaveGameEntry& entry)
{
	CComplexObject& displayObject = GetDisplayObject();
	if (displayObject.IsActive())
	{
		// Graeme - entry.GetPlayerDisplayName() could be used here.
		CScaleformMgr::BeginMethodOnComplexObject(displayObject.GetId(), SF_BASE_CLASS_GENERIC, "SET_ITEM_INFO");
		CScaleformMgr::AddParamString(TheText.Get("UI_MIGRATE_SP_RECENT"), false);		// row1Label
		CScaleformMgr::AddParamString(entry.GetRecentMissionName(), false);				// row1Value
		CScaleformMgr::AddParamString(TheText.Get("UI_MIGRATE_SP_COMPLETION"), false);	// row2Label
		CScaleformMgr::AddParamString(entry.GetTotalCompletion(), false);				// row2Value
		CScaleformMgr::AddParamString(TheText.Get("UI_MIGRATE_SP_TIMESTAMP"), false);	// row3Label
		CScaleformMgr::AddParamString(entry.GetUploadTimestamp(), false);				// row3Value
		// Trophy information is no longer displayed
		CScaleformMgr::AddParamString("", false);										// row4Label
		CScaleformMgr::AddParamString("", false);										// row4Value
		CScaleformMgr::EndMethod();
	}
}

void CStoryModeSaveMigrationSaveGameItem::SetTabName(int index, const char* name)
{
	CComplexObject& displayObject = GetDisplayObject();
	if (displayObject.IsActive())
	{
		CScaleformMgr::BeginMethodOnComplexObject(displayObject.GetId(), SF_BASE_CLASS_GENERIC, "SET_ACCOUNT_NAME");
		CScaleformMgr::AddParamInt(index);			// Index
		CScaleformMgr::AddParamString(name);		// Name
		CScaleformMgr::EndMethod();
	}
}

void CStoryModeSaveMigrationSaveGameItem::SetEntryState(int index, bool enabled)
{
	CComplexObject& displayObject = GetDisplayObject();
	if (displayObject.IsActive())
	{
		CScaleformMgr::BeginMethodOnComplexObject(displayObject.GetId(), SF_BASE_CLASS_GENERIC, "SET_TAB_STATE");
		CScaleformMgr::AddParamInt(index);			// Index
		CScaleformMgr::AddParamInt(1);				// State
		CScaleformMgr::AddParamBool(enabled);		// Enabled
		CScaleformMgr::EndMethod();
	}
}

void CStoryModeSaveMigrationSaveGameItem::ShutdownDerived()
{
	m_saveGames.Reset();
	m_focusedIndex = -1;
}

#endif // UI_PAGE_DECK_ENABLED
