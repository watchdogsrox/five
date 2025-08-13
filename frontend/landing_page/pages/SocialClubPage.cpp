/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : SocialClubPage.cpp
// PURPOSE : Bespoke page for bringing up the social club menu
//
// AUTHOR  : james.strain
// STARTED : July 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "SocialClubPage.h"

#if GEN9_LANDING_PAGE_ENABLED

#include "SocialClubPage_parser.h"

// game
#include "frontend/SocialClubMenu.h"
#include "frontend/page_deck/IPageMessageHandler.h"
#include "frontend/page_deck/messages/uiPageDeckMessages.h"
#include "frontend/ui_channel.h"

FWUI_DEFINE_TYPE(CSocialClubPage, 0x6652115B);

CSocialClubPage::CSocialClubPage() :
	superclass(),
	m_onDismissNotSignedInAction(nullptr)
{
}

void CSocialClubPage::OnEnterCompleteDerived()
{
    SocialClubMenu::Open( false, true );
}

void CSocialClubPage::UpdateDerived( float const UNUSED_PARAM(deltaMs) )
{
    SocialClubMenu::UpdateWrapper();

    if( SocialClubMenu::IsIdleWrapper() && !CLiveManager::GetSocialClubMgr().IsPending())
    {
        const bool c_isSignedIn = CLiveManager::GetSocialClubMgr().IsConnectedToSocialClub();
        IPageMessageHandler& messageHandler = GetMessageHandler();
        if (!c_isSignedIn && m_onDismissNotSignedInAction)
        {
            messageHandler.HandleMessage(*m_onDismissNotSignedInAction);
        }
        else 
        {
            uiPageDeckBackMessage backMsg;
            messageHandler.HandleMessage(backMsg);
        }
    }
}

#endif // GEN9_LANDING_PAGE_ENABLED
