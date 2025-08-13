/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : MigrateProfilePageView.cpp
// PURPOSE : The view that manages the migrate profile  character pages. They consist of the migrate profiler item, a tooltip and the buttons 
//
// AUTHOR  : Charalampos.Koundourakis
// STARTED : August 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "MigrateProfilePageView.h"

#if UI_PAGE_DECK_ENABLED
#include "MigrateProfilePageView_parser.h"

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/ui_channel.h"
#include "frontend/page_deck/IPageItemCollection.h"
#include "frontend/page_deck/IPageItemProvider.h"
#include "frontend/page_deck/IPageViewHost.h"
#include "frontend/page_deck/PageItemCategoryBase.h"
#include "frontend/PauseMenu.h"
#include "SaveMigration/SaveMigration.h"

FWUI_DEFINE_TYPE(CMigrateProfilePageView, 0xC9A49227);

CMigrateProfilePageView::CMigrateProfilePageView()
	: superclass()
	, m_topLevelLayout(CPageGridSimple::GRID_3x5)	
	, m_selectedProfile(-1)
	, m_hasUpdatedAccountDetails(false)
{		
}

CMigrateProfilePageView::~CMigrateProfilePageView()
{	
	Cleanup();
}

void CMigrateProfilePageView::RecalculateGrids()
{
	// Order is important here, as some of the sizing cascades
	// So if you adjust the order TEST IT!
	m_topLevelLayout.Reset();

	// Screen area is acceptable as our grids are all using star sizing
	Vec2f const c_screenExtents = uiPageConfig::GetScreenArea();
	m_topLevelLayout.RecalculateLayout(Vec2f(Vec2f::ZERO), c_screenExtents);
	m_topLevelLayout.NotifyOfLayoutUpdate();
}

void CMigrateProfilePageView::OnFocusGainedDerived()
{	
	GetPageRoot().SetVisible(true);
	char const * const c_literalTooltipText = TheText.Get("UI_MIGRATE_MP_CASH_TIP");
	SetTooltipLiteral(c_literalTooltipText, false);
	
}

void CMigrateProfilePageView::UpdateDerived(float const UNUSED_PARAM(deltaMs))
{
	eSaveMigrationStatus currentStatus = CSaveMigration::GetMultiplayer().GetAccountsStatus();
	if (currentStatus == SM_STATUS_OK)
	{
		if (!m_hasUpdatedAccountDetails)
		{
			m_accounts = CSaveMigration::GetMultiplayer().GetAccounts();
			FillProfileSlots();			
			m_hasUpdatedAccountDetails = true;
		}
	}
	else if (currentStatus == SM_STATUS_ERROR)
	{		
		// TODO_UI: Add try-again-later alert.
	}

	UpdateInput();
}

void CMigrateProfilePageView::FillProfileSlots()
{	
	const int c_requestRecordMetadataCount = m_accounts.GetCount();
	m_saves.Reserve(c_requestRecordMetadataCount);

	for (int i = 0; i < c_requestRecordMetadataCount; ++i)
	{
		const rlSaveMigrationMPAccount& c_record = m_accounts[i];
		SaveMigrationDefs::MultiplayerModeSaveGameEntry profileData = SaveMigrationDefs::CreateFormattedMPSaveGameEntry(c_record);
		m_saves.Push(profileData);
		m_migrateProfile.SetAccountName(i, m_saves[i].GetGamerTag());
	}	
	m_migrateProfile.SetDetails("UI_MIGRATE_MP_SAVE", "UI_MIGRATE_MP_SAVE_D");	
	SwapSelectedProfile(0);
}

void CMigrateProfilePageView::OnFocusLostDerived()
{	
	GetPageRoot().SetVisible(false);
	ClearPromptsAndTooltip();
}

void CMigrateProfilePageView::PopulateDerived(IPageItemProvider& UNUSED_PARAM(itemProvider))
{	
	if (!IsPopulated())
	{
		CComplexObject& pageRoot = GetPageRoot();

		m_parallaxImage.Initialize(pageRoot);
		CPageLayoutItemParams const c_parallaxImageParams(1, 2, 1, 1);
		m_topLevelLayout.RegisterItem(m_parallaxImage, c_parallaxImageParams);
		m_parallaxImage.StartNewParallax("FRONTEND_LANDING_BASE", "MIGRATION_STORY_SAVEDATA_BANK_BG", "FRONTEND_LANDING_BASE", "MIGRATION_STORY_SAVEDATA_BANK_CHAR", eMotionType::MIGRATION_MOTION_TYPE);
		
		CPageLayoutItemParams const c_tooltipParams(1, 4, 1, 1);
		InitializeTooltip(pageRoot, m_topLevelLayout, c_tooltipParams);
		char const * const c_literalTooltipText = TheText.Get("UI_MIGRATE_MP_CASH_TIP");
		SetTooltipLiteral(c_literalTooltipText, false);

		CPageLayoutItemParams const c_migrateProfileParams(1, 2, 1, 1);
		m_migrateProfile.Initialize(pageRoot);
		m_topLevelLayout.RegisterItem(m_migrateProfile, c_migrateProfileParams);	
		m_migrateProfile.SetTabState(m_selectedProfile, 1, true);		
		RecalculateGrids();		
	}
}

fwuiInput::eHandlerResult CMigrateProfilePageView::HandleBackoutAction(eFRONTEND_INPUT const inputAction, IPageMessageHandler& messageHandler)
{
	fwuiInput::eHandlerResult result(fwuiInput::ACTION_NOT_HANDLED);

	if (inputAction == FRONTEND_INPUT_BACK)
	{
		uiLandingPage_GoToOnlineMode message;
		messageHandler.HandleMessage(message);
		result = fwuiInput::ACTION_HANDLED;
	}	

	return result;
}

fwuiInput::eHandlerResult CMigrateProfilePageView::HandleMigrateAction(eFRONTEND_INPUT const inputAction)
{
	fwuiInput::eHandlerResult result(fwuiInput::ACTION_NOT_HANDLED);

	if (inputAction == FRONTEND_INPUT_ACCEPT)
	{	
		eSaveMigrationStatus currentMigrationStatus = CSaveMigration::GetMultiplayer().GetMigrationStatus();
		if (uiVerifyf(currentMigrationStatus == SM_STATUS_NONE, "Tried to migrate but migration status is: %d", currentMigrationStatus))
		{
			if (uiVerifyf(m_accounts.GetCount() > m_selectedProfile && m_selectedProfile != -1, "Invalid Account Selection: %d", m_selectedProfile))
			{
				CSaveMigration::GetMultiplayer().SetChosenAccount(m_accounts[m_selectedProfile]);
				uiGoToPageMessage message(m_targetPageLink);
				GetViewHost().HandleMessage(message);
				result = fwuiInput::ACTION_HANDLED;
			}
		}
	}
	   	 
	return result;
}

void CMigrateProfilePageView::UpdateInputDerived()
{
	if (IsPopulated() && m_hasUpdatedAccountDetails)
	{
		fwuiInput::eHandlerResult result(fwuiInput::ACTION_NOT_HANDLED);
		if (CPauseMenu::CheckInput(FRONTEND_INPUT_ACCEPT))
		{
			result = HandleMigrateAction(FRONTEND_INPUT_ACCEPT);
			TriggerInputAudio(result, UI_AUDIO_TRIGGER_SELECT, UI_AUDIO_TRIGGER_ERROR);
		}
		else if (CPauseMenu::CheckInput(FRONTEND_INPUT_BACK))
		{
			result = HandleBackoutAction(FRONTEND_INPUT_BACK, GetViewHost());
			TriggerInputAudio(result, UI_AUDIO_TRIGGER_SELECT, UI_AUDIO_TRIGGER_ERROR);
		}
		else
		{
			CControl& frontendControl = CControlMgr::GetMainFrontendControl();
			if (CPauseMenu::CheckInput(FRONTEND_INPUT_LEFT) || frontendControl.GetFrontendLB().IsReleased())
			{
				result = SwapSelectedProfile(0);
				TriggerInputAudio(result, UI_AUDIO_TRIGGER_NAV_LEFT_RIGHT, UI_AUDIO_TRIGGER_ERROR);
			}
			else if (CPauseMenu::CheckInput(FRONTEND_INPUT_RIGHT) || frontendControl.GetFrontendRB().IsReleased())
			{
				result = SwapSelectedProfile(1);
				TriggerInputAudio(result, UI_AUDIO_TRIGGER_NAV_LEFT_RIGHT, UI_AUDIO_TRIGGER_ERROR);
			}
		}
	}
}

fwuiInput::eHandlerResult CMigrateProfilePageView::SwapSelectedProfile(int targetProfile)
{
	fwuiInput::eHandlerResult result(fwuiInput::ACTION_NOT_HANDLED);
	if (m_selectedProfile != targetProfile)
	{
		if (m_saves.GetCount() > targetProfile)
		{
			m_migrateProfile.SetTabState(m_selectedProfile, 0, true);
			m_migrateProfile.SetTabState(targetProfile, 1, true);
			m_selectedProfile = targetProfile;
			m_migrateProfile.SetItemInfo(m_saves[m_selectedProfile].GetLastPlayedTime(), m_saves[m_selectedProfile].GetUGCPublishedMissionNumber(), m_saves[m_selectedProfile].GetBankedCash(), m_saves[m_selectedProfile].GetEligibleFunds());
			SaveMigrationDefs::MultiplayerCharacterEntry character = m_saves[m_selectedProfile].GetCharacter(0);
			SaveMigrationDefs::MultiplayerCharacterEntry secondCharacter = m_saves[m_selectedProfile].GetCharacter(1);
			m_migrateProfile.SetProfileInfo(0, character.GetProfileName(), character.GetRpEarned(), character.GetCharacterHeadshotTexture(), character.GetRank());
			m_migrateProfile.SetProfileInfo(1, secondCharacter.GetProfileName(), secondCharacter.GetRpEarned(), secondCharacter.GetCharacterHeadshotTexture(), secondCharacter.GetRank());
			result = fwuiInput::ACTION_HANDLED;
		}		
	}		
	return result;
}

void CMigrateProfilePageView::UpdateInstructionalButtonsDerived()
{
	atHashString const c_acceptLabelOverride = GetAcceptPromptLabelOverride();
	char const * const c_label = uiPageConfig::GetLabel(ATSTRINGHASH("IB_SELECT", 0xD7ED7F0C), c_acceptLabelOverride);
	CVideoEditorInstructionalButtons::AddButton(INPUT_FRONTEND_ACCEPT, c_label, true);
	m_topLevelLayout.UpdateInstructionalButtons(c_acceptLabelOverride);
	CVideoEditorInstructionalButtons::AddButton(rage::INPUT_FRONTEND_CANCEL, TheText.Get(ATSTRINGHASH("IB_MIGRATE_MP_FREEROAM", 0x19E98E30)), true);
	superclass::UpdateInstructionalButtonsDerived();
}


bool CMigrateProfilePageView::IsPopulatedDerived() const
{	
	return m_migrateProfile.IsInitialized();
}

void CMigrateProfilePageView::CleanupDerived()
{	
	m_topLevelLayout.UnregisterAll();		
	m_parallaxImage.Shutdown();
	m_migrateProfile.Shutdown(); 
	ShutdownTooltip();
}

void CMigrateProfilePageView::GetContentArea(Vec2f_InOut pos, Vec2f_InOut size) const
{
	m_topLevelLayout.GetCell(pos, size, 0, 2, 3, 3);
}

#if RSG_BANK

void CMigrateProfilePageView::DebugRenderDerived() const
{
	m_topLevelLayout.DebugRender();	
}

#endif // RSG_BANK

#endif // UI_PAGE_DECK_ENABLED
