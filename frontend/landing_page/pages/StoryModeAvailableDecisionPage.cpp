/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : StoryModeAvailableDecisionPage.cpp
// PURPOSE : Checks access to story mode, and executes appropriate yes/no actions
//
// AUTHOR  : james.strain
// STARTED : November 2020
//
/////////////////////////////////////////////////////////////////////////////////
#include "StoryModeAvailableDecisionPage.h"

#if GEN9_LANDING_PAGE_ENABLED

#include "StoryModeAvailableDecisionPage_parser.h"

// game
#include "frontend/ui_channel.h"
#include "frontend/landing_page/LandingPageArbiter.h"

FWUI_DEFINE_TYPE( CStoryModeAvailableDecisionPage, 0x507AAE7F );

bool CStoryModeAvailableDecisionPage::GetDecisionResult() const
{
    return CLandingPageArbiter::IsStoryModeAvailable();
}

#endif // GEN9_LANDING_PAGE_ENABLED
