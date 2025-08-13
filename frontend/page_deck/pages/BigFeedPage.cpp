/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : BigFeedPage.cpp
// PURPOSE : Bespoke page for bringing up the big feed
//
// AUTHOR  : james.strain
// STARTED : June 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "BigFeedPage.h"

#if GEN9_LANDING_PAGE_ENABLED

#include "BigFeedPage_parser.h"

// game
#include "frontend/HudTools.h"
#include "frontend/landing_page/LandingPageArbiter.h"
#include "frontend/page_deck/IPageMessageHandler.h"
#include "frontend/page_deck/messages/uiPageDeckMessages.h"
#include "frontend/PauseMenu.h"
#include "frontend/ui_channel.h"
#include "network/SocialClubServices/SocialClubNewsStoryMgr.h"

FWUI_DEFINE_TYPE( CBigFeedPage, 0xC118BF34 );

CBigFeedPage::CBigFeedPage() 
    : superclass()
    , m_isPriorityFeed( false )
{

}

bool CBigFeedPage::OnEnterUpdateDerived( float const UNUSED_PARAM(deltaMs) )
{
    CNetworkTransitionNewsController& newsController = CNetworkTransitionNewsController::Get();
    bool const c_complete = newsController.IsDisplayingNews();
    return c_complete;
}

void CBigFeedPage::OnEnterStartDerived()
{
    atHashString const c_context = GetPageContext();
    SUIContexts::Activate( c_context );
}

void CBigFeedPage::OnFocusLostDerived()
{
    // The transition isn't a backward transition so the
    // StepTransition doesn't close the page automatically.
    if (m_isPriorityFeed)
    {
        ExitPage(false);
    }
}

void CBigFeedPage::OnExitCompleteDerived()
{
    atHashString const c_context = GetPageContext();
    SUIContexts::Deactivate( c_context );
}

atHashString CBigFeedPage::GetPageContext() const
{
    return m_isPriorityFeed ? BIG_FEED_PRIORITY_FEED_CONTEXT : BIG_FEED_WHATS_NEW_PAGE_CONTEXT;
}

#endif // GEN9_LANDING_PAGE_ENABLED
