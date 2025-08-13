#include "StoryModeSaveMigrationView.h"

#if UI_PAGE_DECK_ENABLED
#include "StoryModeSaveMigrationView_parser.h"

#include "SaveMigration/SaveMigration.h"

FWUI_DEFINE_TYPE(CStoryModeSaveMigrationView, 0x5FE5EBD0);

CStoryModeSaveMigrationView::CStoryModeSaveMigrationView() :
	superclass(),
	m_layout(CPageGridSimple::GRID_3x6),
	m_onSelectMessage(nullptr)
{
}

void CStoryModeSaveMigrationView::RecalculateGrids()
{
	m_layout.Reset();
	Vec2f const c_screenExtents = uiPageConfig::GetScreenArea();
	m_layout.RecalculateLayout(Vec2f(Vec2f::ZERO), c_screenExtents);
	m_layout.NotifyOfLayoutUpdate();
}

void CStoryModeSaveMigrationView::UpdateDerived(float const UNUSED_PARAM(deltaMs))
{
	UpdateInput();
}

void CStoryModeSaveMigrationView::PopulateDerived(IPageItemProvider& itemProvider)
{
	if (!IsPopulated())
	{
		CComplexObject& pageRoot = GetPageRoot();

		m_parallaxImage.Initialize(pageRoot);
		CPageLayoutItemParams const c_parallaxImageParams(1, 2, 1, 1);
		m_layout.RegisterItem(m_parallaxImage, c_parallaxImageParams);
		m_parallaxImage.StartNewParallax("FRONTEND_LANDING_BASE", "MIGRATION_STORY_SAVEDATA_MASKS_BG", "FRONTEND_LANDING_BASE", "MIGRATION_STORY_SAVEDATA_MASKS_CHAR", eMotionType::MIGRATION_MOTION_TYPE);
		m_saveGameItem.Initialize(pageRoot);
		CPageLayoutItemParams const c_layoutParams(1, 2, 1, 1);
		m_layout.RegisterItem(m_saveGameItem, c_layoutParams);
		m_saveGameItem.SetDetailsLiteral(itemProvider.GetTitleString(), TheText.Get("UI_MIGRATE_SP_SAVE_D"));

		FillSaveSlots();

		RecalculateGrids();
	}
}

fwuiInput::eHandlerResult CStoryModeSaveMigrationView::HandleSaveSlotSelectedAction(IPageMessageHandler& messageHandler)
{
	fwuiInput::eHandlerResult result(fwuiInput::ACTION_NOT_HANDLED);

	const int c_saveIndex = m_saveGameItem.GetFocusedIndex();

	// TODO_UI SAVE_MIGRATION: Add confirmation prompt here

	const CSaveMigrationGetAvailableSingleplayerSaves::MigrationRecordCollection& c_saveData = CSaveMigration::GetSingleplayer().GetAvailableSingleplayerSaves();
	if (uiVerifyf(c_saveIndex >= 0 && c_saveIndex < c_saveData.GetCount(), "Attempting to select invalid save index %d", c_saveIndex)
		&& uiVerifyf(m_onSelectMessage != nullptr, "Missing onSelectMessage"))
	{
		const rlSaveMigrationRecordMetadata& c_record = c_saveData[c_saveIndex];
		CSaveMigration::GetSingleplayer().SetChosenSingleplayerSave(c_record);
		messageHandler.HandleMessage(*m_onSelectMessage);

		result = fwuiInput::ACTION_HANDLED;
	}

	return result;
}

void CStoryModeSaveMigrationView::UpdateInputDerived()
{
	if (IsPopulated())
	{
		fwuiInput::eHandlerResult result(fwuiInput::ACTION_NOT_HANDLED);
		if (CPauseMenu::CheckInput(FRONTEND_INPUT_ACCEPT))
		{
			result = HandleSaveSlotSelectedAction(GetViewHost());
			TriggerInputAudio(result, UI_AUDIO_TRIGGER_SELECT, UI_AUDIO_TRIGGER_ERROR);
		}
		else if (CPauseMenu::CheckInput(FRONTEND_INPUT_LEFT))
		{
			m_saveGameItem.ChangeFocus(-1);
			result = fwuiInput::ACTION_HANDLED;
			TriggerInputAudio(result, UI_AUDIO_TRIGGER_NAV_LEFT_RIGHT, UI_AUDIO_TRIGGER_ERROR);
		}
		else if (CPauseMenu::CheckInput(FRONTEND_INPUT_RIGHT))
		{
			m_saveGameItem.ChangeFocus(1);
			result = fwuiInput::ACTION_HANDLED;
			TriggerInputAudio(result, UI_AUDIO_TRIGGER_NAV_LEFT_RIGHT, UI_AUDIO_TRIGGER_ERROR);
		}
	}
}

void CStoryModeSaveMigrationView::UpdateInstructionalButtonsDerived()
{
	atHashString const c_acceptLabelOverride = GetAcceptPromptLabelOverride();
	char const * const c_selectLabel = uiPageConfig::GetLabel(ATSTRINGHASH("IB_SELECT", 0xD7ED7F0C), c_acceptLabelOverride);
	CVideoEditorInstructionalButtons::AddButton(INPUT_FRONTEND_ACCEPT, c_selectLabel, true);
	m_layout.UpdateInstructionalButtons(c_acceptLabelOverride);

	superclass::UpdateInstructionalButtonsDerived();
}

bool CStoryModeSaveMigrationView::IsPopulatedDerived() const
{
	return m_saveGameItem.IsInitialized();
}

void CStoryModeSaveMigrationView::CleanupDerived()
{
	m_layout.UnregisterAll();
	m_parallaxImage.Shutdown();
	m_saveGameItem.Shutdown();
}

void CStoryModeSaveMigrationView::FillSaveSlots()
{
	const CSaveMigrationGetAvailableSingleplayerSaves::MigrationRecordCollection& c_saveData = CSaveMigration::GetSingleplayer().GetAvailableSingleplayerSaves();
	const int c_saveDataCount = c_saveData.GetCount();
	rage::atArray<SaveMigrationDefs::StoryModeSaveGameEntry> saveGames;
	saveGames.Reserve(c_saveDataCount);

	for (int i = 0; i < c_saveDataCount; ++i)
	{
		const rlSaveMigrationRecordMetadata& c_record = c_saveData[i];
		SaveMigrationDefs::StoryModeSaveGameEntry saveGameData = SaveMigrationDefs::CreateFormattedSaveGameEntry(c_record);
		saveGames.Push(saveGameData);
	}

	// Sort by timestamp before adding to UI
	saveGames.QSort(0, -1, SaveMigrationDefs::SortByUploadTimeDescending);

	for (int i = 0; i < c_saveDataCount; ++i)
	{
		m_saveGameItem.AddSaveGame(saveGames[i]);
	}
}

#if RSG_BANK

void CStoryModeSaveMigrationView::DebugRenderDerived() const
{
	m_layout.DebugRender();
}

#endif // RSG_BANK

#endif // UI_PAGE_DECK_ENABLED
