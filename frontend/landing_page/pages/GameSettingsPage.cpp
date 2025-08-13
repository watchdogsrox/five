/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : GameSettingsPage.h
// PURPOSE : Bespoke page for bringing up the game settings menu
//
// AUTHOR  : james.strain
// STARTED : December 2020
//
/////////////////////////////////////////////////////////////////////////////////
#include "GameSettingsPage.h"

#if GEN9_LANDING_PAGE_ENABLED

#include "GameSettingsPage_parser.h"

// game
#include "frontend/landing_page/LandingPageArbiter.h"
#include "frontend/page_deck/IPageMessageHandler.h"
#include "frontend/page_deck/messages/uiPageDeckMessages.h"
#include "frontend/PauseMenu.h"
#include "frontend/ui_channel.h"
#include "system/SettingsManager.h"

FWUI_DEFINE_TYPE( CGameSettingsPage, 0x9C5A22E6 );

void CGameSettingsPage::OnEnterCompleteDerived()
{
    CPauseMenu::Open(FE_MENU_VERSION_LANDING_MENU);
    SUIContexts::Activate("OnLandingPage");
}

void CGameSettingsPage::UpdateDerived( float const UNUSED_PARAM(deltaMs) )
{
    if( CPauseMenu::IsActive() )
    {
#if RSG_PC // Only available on PC right now
        CSettingsManager::GetInstance().SafeUpdate();
#endif // RSG_PC
    }
    else
    {
        uiPageDeckBackMessage backMsg;
        IPageMessageHandler& messageHandler = GetMessageHandler();
        messageHandler.HandleMessage( backMsg );
    }
}

#endif // GEN9_LANDING_PAGE_ENABLED
