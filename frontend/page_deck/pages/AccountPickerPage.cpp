/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : AccountPickerPage.cpp
// PURPOSE : Page underpinning the account picker

// AUTHOR  : james.strain
// STARTED : August 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "AccountPickerPage.h"

#if UI_PAGE_DECK_ENABLED
#include "AccountPickerPage_parser.h"

// game
#include "frontend/ui_channel.h"
#include "frontend/page_deck/messages/uiPageDeckMessages.h"
#include "frontend/page_deck/IPageMessageHandler.h"
#include "network/Live/livemanager.h"

FWUI_DEFINE_TYPE( CAccountPickerPage, 0xAA4F9C96 );

void CAccountPickerPage::OnEnterStartDerived()
{
	CLiveManager::ShowSigninUi();
}

void CAccountPickerPage::UpdateDerived( float const UNUSED_PARAM(deltaMs) )
{
    if( !CLiveManager::IsSystemUiShowing())
    {
		IPageMessageHandler& messageHandler = GetMessageHandler();
		uiPageDeckBackMessage backMsg;

		messageHandler.HandleMessage(backMsg);
    }
}

#endif // UI_PAGE_DECK_ENABLED