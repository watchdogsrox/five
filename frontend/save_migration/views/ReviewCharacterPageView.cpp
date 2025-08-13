/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : ReviewCharacterPageView.cpp
// PURPOSE : The view that manages the review character pages. They consist of tthe review character item, a tooltip and the buttons
//
// AUTHOR  : Charalampos.Koundourakis
// STARTED : August 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "ReviewCharacterPageView.h"

#if UI_PAGE_DECK_ENABLED
#include "ReviewCharacterPageView_parser.h"

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/ui_channel.h"
#include "frontend/page_deck/IPageItemCollection.h"
#include "frontend/page_deck/IPageItemProvider.h"
#include "frontend/page_deck/IPageViewHost.h"
#include "frontend/page_deck/PageItemBase.h"
#include "frontend/page_deck/PageItemCategoryBase.h"
#include "frontend/PauseMenu.h"
#include "frontend/career_builder/messages/uiCareerBuilderMessages.h"

FWUI_DEFINE_TYPE(CReviewCharacterPageView, 0xC49A7D62);

CReviewCharacterPageView::CReviewCharacterPageView()
	: superclass()
	, m_topLevelLayout(CPageGridSimple::GRID_3x5)
	, m_hasUpdatedAccountDetails(false)
{
}

CReviewCharacterPageView::~CReviewCharacterPageView()
{
	Cleanup();
}

void CReviewCharacterPageView::RecalculateGrids()
{
	// Order is important here, as some of the sizing cascades
	// So if you adjust the order TEST IT!
	m_topLevelLayout.Reset();

	// Screen area is acceptable as our grids are all using star sizing
	Vec2f const c_screenExtents = uiPageConfig::GetScreenArea();
	m_topLevelLayout.RecalculateLayout(Vec2f(Vec2f::ZERO), c_screenExtents);
	m_topLevelLayout.NotifyOfLayoutUpdate();

	Vec2f contentPos(Vec2f::ZERO);
	Vec2f contentSize(Vec2f::ZERO);
	GetContentArea(contentPos, contentSize);
}

void CReviewCharacterPageView::OnFocusGainedDerived()
{
	GetPageRoot().SetVisible(true);
	ClearPromptsAndTooltip();
}

void CReviewCharacterPageView::UpdateDerived(float const UNUSED_PARAM(deltaMs))
{
	if (CSaveMigration::GetMultiplayer().GetAccountsStatus() == SM_STATUS_OK)
	{
		if (!m_hasUpdatedAccountDetails)
		{
			const rlSaveMigrationMPAccount& c_record = CSaveMigration::GetMultiplayer().GetChosenAccount();
			SaveMigrationDefs::MultiplayerModeSaveGameEntry profileData = SaveMigrationDefs::CreateFormattedMPSaveGameEntry(c_record);
			FillCharacterSlots(profileData);
			m_reviewCharacter.SetDetails("UI_MIGRATE_MP_REVIEW", "UI_MIGRATE_MP_WINDFALL_D", "UI_MIGRATE_MP_STARTLATER");
			m_hasUpdatedAccountDetails = true;
		}
	}

	UpdateInput();
}

void CReviewCharacterPageView::FillCharacterSlots(SaveMigrationDefs::MultiplayerModeSaveGameEntry profileData)
{		
	m_characters.Reset();
	m_characters.PushAndGrow(profileData.GetCharacter(0));
	m_characters.PushAndGrow(profileData.GetCharacter(1));
	m_characters.QSort(0, -1, SaveMigrationDefs::SortByNonActiveAccountFirst);
	
	for (int i = 0; i < m_characters.GetCount(); ++i)
	{
		const SaveMigrationDefs::MultiplayerCharacterEntry& character = m_characters[i];
		if (character.GetIsActive())
		{
			m_reviewCharacter.SetProfileInfo(i, character.GetProfileName(), character.GetRpEarned(), character.GetCharacterHeadshotTexture());
		}
		else
		{
			FillEmptyCharacterSlot(i);
		}
	}
}

void CReviewCharacterPageView::FillEmptyCharacterSlot(int profileSLot)
{
	char const* const c_literalTitleText = TheText.Get("UI_MIGRATE_MP_NEWCHAR");
	char const* const c_literalDescriptionText = TheText.Get("UI_MIGRATE_MP_SLOT");
	m_reviewCharacter.SetProfileInfo(profileSLot, c_literalTitleText, c_literalDescriptionText, "");
}

void CReviewCharacterPageView::OnFocusLostDerived()
{
	GetPageRoot().SetVisible(false);
	ClearPromptsAndTooltip();
}

void CReviewCharacterPageView::PopulateDerived(IPageItemProvider& UNUSED_PARAM(itemProvider))
{
	if (!IsPopulated())
	{
		CComplexObject& pageRoot = GetPageRoot();
		m_parallaxImage.Initialize(pageRoot);
		CPageLayoutItemParams const c_parallaxImageParams(1, 2, 1, 1);
		m_topLevelLayout.RegisterItem(m_parallaxImage, c_parallaxImageParams);
		m_parallaxImage.StartNewParallax("FRONTEND_LANDING_BASE", "MIGRATION_ONLINE_PROFILE_LAMAR_BG", "FRONTEND_LANDING_BASE", "MIGRATION_ONLINE_PROFILE_LAMAR_CHAR", eMotionType::MIGRATION_MOTION_TYPE);

		CPageLayoutItemParams const c_reviewCharacterParams(1, 2, 1, 1);
		m_reviewCharacter.Initialize(pageRoot);
		m_topLevelLayout.RegisterItem(m_reviewCharacter, c_reviewCharacterParams);
		m_reviewCharacter.SetState(0, 1);
		RecalculateGrids();
	}
}

fwuiInput::eHandlerResult CReviewCharacterPageView::HandleCharacterSlotSelectedAction(eFRONTEND_INPUT const inputAction, IPageMessageHandler& messageHandler)
{
	fwuiInput::eHandlerResult result(fwuiInput::ACTION_NOT_HANDLED);

	if (inputAction == FRONTEND_INPUT_ACCEPT)
	{				
		uiSelectMpCharacterSlotMessage message(m_characters[m_selectedCharacter].GetSlotIndex(), m_targetPageLink);
		messageHandler.HandleMessage(message);
		result = fwuiInput::ACTION_HANDLED;		
	}

	return result;
}

void CReviewCharacterPageView::UpdateInputDerived()
{
	if (IsPopulated())
	{
		fwuiInput::eHandlerResult result(fwuiInput::ACTION_NOT_HANDLED);
		if (CPauseMenu::CheckInput(FRONTEND_INPUT_ACCEPT))
		{
			result = HandleCharacterSlotSelectedAction(FRONTEND_INPUT_ACCEPT, GetViewHost());
			TriggerInputAudio(result, UI_AUDIO_TRIGGER_SELECT, UI_AUDIO_TRIGGER_ERROR);
		}

		if (CPauseMenu::CheckInput(FRONTEND_INPUT_UP))
		{
			SetSelectedCharacter(0);
		}

		if (CPauseMenu::CheckInput(FRONTEND_INPUT_DOWN))
		{
			SetSelectedCharacter(1);
		}
	}
}

void CReviewCharacterPageView::SetSelectedCharacter(int targetCharacter)
{
	m_reviewCharacter.SetState(m_selectedCharacter, 0);
	m_selectedCharacter = targetCharacter;
	m_reviewCharacter.SetState(m_selectedCharacter, 1);
}


void CReviewCharacterPageView::UpdateInstructionalButtonsDerived()
{
	atHashString const c_acceptLabelOverride = GetAcceptPromptLabelOverride();
	char const * const c_label = uiPageConfig::GetLabel(ATSTRINGHASH("IB_MIGRATE_MP_STARTCAREER", 0x92A0892E), c_acceptLabelOverride);
	CVideoEditorInstructionalButtons::AddButton(INPUT_FRONTEND_ACCEPT, c_label, true);
	m_topLevelLayout.UpdateInstructionalButtons(c_acceptLabelOverride);
	superclass::UpdateInstructionalButtonsDerived();
}

bool CReviewCharacterPageView::IsPopulatedDerived() const
{
	return m_reviewCharacter.IsInitialized();
}

void CReviewCharacterPageView::CleanupDerived()
{
	m_characters.clear();
	m_topLevelLayout.UnregisterAll();
	m_parallaxImage.Shutdown();
	m_reviewCharacter.Shutdown();
	ShutdownTooltip();
}

void CReviewCharacterPageView::GetContentArea(Vec2f_InOut pos, Vec2f_InOut size) const
{
	m_topLevelLayout.GetCell(pos, size, 0, 2, 3, 3);
}

#if RSG_BANK

void CReviewCharacterPageView::DebugRenderDerived() const
{
	m_topLevelLayout.DebugRender();
}

#endif // RSG_BANK

#endif // UI_PAGE_DECK_ENABLED
