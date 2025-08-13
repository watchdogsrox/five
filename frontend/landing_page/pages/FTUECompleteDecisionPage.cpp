/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : FTUECompleteDecisionPage.cpp
// PURPOSE : Has the main player already completed the first time user experience
//
// AUTHOR  : james.strain
// STARTED : November 2020
//
/////////////////////////////////////////////////////////////////////////////////
#include "FTUECompleteDecisionPage.h"

#if GEN9_LANDING_PAGE_ENABLED

#include "FTUECompleteDecisionPage_parser.h"

// game
#include "frontend/ui_channel.h"
#include "frontend/landing_page/LandingPageArbiter.h"


FWUI_DEFINE_TYPE( CFTUECompleteDecisionPage, 0xD84D7D2 );

bool CFTUECompleteDecisionPage::GetDecisionResult() const
{
    return  CLandingPageArbiter::IsFTUEComplete();
}

#endif // GEN9_LANDING_PAGE_ENABLED
