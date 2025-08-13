/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : MigrateAlertPageView.cpp
// PURPOSE : The view that manages the migrate profile  character pages. They consist of the migrate profiler item, a tooltip and the buttons 
//
// AUTHOR  : Charalampos.Koundourakis
// STARTED : August 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "MigrateAlertPageView.h"

#if UI_PAGE_DECK_ENABLED
#include "MigrateAlertPageView_parser.h"

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

FWUI_DEFINE_TYPE(CMigrateAlertPageView, 0xC9A49227);

CMigrateAlertPageView::CMigrateAlertPageView()
	: superclass()
	, m_topLevelLayout(CPageGridSimple::GRID_3x5)	
{	

}

CMigrateAlertPageView::~CMigrateAlertPageView()
{	
	Cleanup();
}


void CMigrateAlertPageView::UpdateDerived(float const UNUSED_PARAM(deltaMs))
{
	if (CSaveMigration::GetMultiplayer().GetMigrationStatus() == SM_STATUS_OK)
	{
		uiGoToPageMessage message(m_targetPageLink);
		GetViewHost().HandleMessage(message);		
	}
	UpdateInput();
}

fwuiInput::eHandlerResult CMigrateAlertPageView::HandleBackoutAction(eFRONTEND_INPUT const inputAction, IPageMessageHandler& messageHandler)
{
	fwuiInput::eHandlerResult result(fwuiInput::ACTION_NOT_HANDLED);

	if (inputAction == FRONTEND_INPUT_BACK)
	{
		uiPageDeckBackMessage message;
		messageHandler.HandleMessage(message);		
		result = fwuiInput::ACTION_HANDLED;
	}	

	return result;
}

fwuiInput::eHandlerResult CMigrateAlertPageView::HandleMigrateAction(eFRONTEND_INPUT const inputAction)
{
	fwuiInput::eHandlerResult result(fwuiInput::ACTION_NOT_HANDLED);

	if (inputAction == FRONTEND_INPUT_ACCEPT)
	{		
		if (CSaveMigration::GetMultiplayer().GetMigrationStatus() == SM_STATUS_NONE && m_hasUpdatedAccountDetails)
		{
			if (m_saves.GetCount() > m_selectedProfile)
			{
				if (CSaveMigration::GetMultiplayer().MigrateAccount(m_Accounts[m_selectedProfile].m_AccountId, m_Accounts[m_selectedProfile].m_Platform))
				{
					result = fwuiInput::ACTION_HANDLED;
				}
			}
		}
	}
	   	 
	return result;
}

void CMigrateAlertPageView::UpdateInputDerived()
{
	if (IsPopulated())
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
	}
}




bool CMigrateAlertPageView::IsPopulatedDerived() const
{
	return true;
}

void CMigrateAlertPageView::CleanupDerived()
{	
	m_topLevelLayout.UnregisterAll();		
	m_parallaxImage.Shutdown();
	m_migrateProfile.Shutdown(); 
	ShutdownTooltip();
}

void CMigrateAlertPageView::GetContentArea(Vec2f_InOut pos, Vec2f_InOut size) const
{
	m_topLevelLayout.GetCell(pos, size, 0, 2, 3, 3);
}

#if RSG_BANK

void CMigrateAlertPageView::DebugRenderDerived() const
{
	m_topLevelLayout.DebugRender();	
}

#endif // RSG_BANK

#endif // UI_PAGE_DECK_ENABLED
