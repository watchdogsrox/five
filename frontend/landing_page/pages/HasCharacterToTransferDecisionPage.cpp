/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : HasCharacterToTransferDecisionPage.cpp
// PURPOSE : Has the main player got a character to transfer the first time user experience
//
// AUTHOR  : james.strain
// STARTED : November 2020
//
/////////////////////////////////////////////////////////////////////////////////
#include "HasCharacterToTransferDecisionPage.h"

#if GEN9_LANDING_PAGE_ENABLED

#include "HasCharacterToTransferDecisionPage_parser.h"

// game
#include "frontend/ui_channel.h"
#include "frontend/landing_page/LandingPageArbiter.h"


FWUI_DEFINE_TYPE( CHasCharacterToTransferDecisionPage, 0xA33D2F1A );

bool CHasCharacterToTransferDecisionPage::GetDecisionResult() const
{
    return CLandingPageArbiter::DoesPlayerHaveCharacterToTransfer();
}

#endif // GEN9_LANDING_PAGE_ENABLED
