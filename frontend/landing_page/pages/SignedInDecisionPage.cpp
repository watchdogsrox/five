/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : SignedInDecisionPage.cpp
// PURPOSE : Is the main player signed in
//
// AUTHOR  : james.strain
// STARTED : November 2020
//
/////////////////////////////////////////////////////////////////////////////////
#include "SignedInDecisionPage.h"

#if GEN9_LANDING_PAGE_ENABLED

#include "SignedInDecisionPage_parser.h"

// game
#include "frontend/ui_channel.h"
#include "network/live/livemanager.h"

FWUI_DEFINE_TYPE( CSignedInDecisionPage, 0x19C78702 );

bool CSignedInDecisionPage::GetDecisionResult() const
{
    return CLiveManager::IsSignedIn();
}

#endif // GEN9_LANDING_PAGE_ENABLED
