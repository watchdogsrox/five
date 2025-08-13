/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : WindfallRecievedDecisionPage.cpp
// PURPOSE : Has this player already received the windfall bonus
//
// AUTHOR  : james.strain
// STARTED : November 2020
//
/////////////////////////////////////////////////////////////////////////////////
#include "WindfallRecievedDecisionPage.h"

#if GEN9_LANDING_PAGE_ENABLED

#include "WindfallRecievedDecisionPage_parser.h"

// game
#include "frontend/ui_channel.h"
#include "frontend/landing_page/LandingPageArbiter.h"

FWUI_DEFINE_TYPE( CWindfallRecievedDecisionPage, 0x2EDD616F );

bool CWindfallRecievedDecisionPage::GetDecisionResult() const
{
    return CLandingPageArbiter::HasPlayerRecievedWindfall();
}

#endif // GEN9_LANDING_PAGE_ENABLED
