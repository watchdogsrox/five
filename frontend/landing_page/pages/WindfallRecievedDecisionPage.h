/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : WindfallRecievedDecisionPage.h
// PURPOSE : Has this player already received the windfall bonus
//
// AUTHOR  : james.strain
// STARTED : November 2020
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef WINDFALL_RECIEVED_DECISION_PAGE_H
#define WINDFALL_RECIEVED_DECISION_PAGE_H

#include "frontend/landing_page/LandingPageConfig.h"

#if GEN9_LANDING_PAGE_ENABLED

// game
#include "frontend/page_deck/pages/DecisionPageBase.h"

class CWindfallRecievedDecisionPage : public CDecisionPageBase
{
    FWUI_DECLARE_DERIVED_TYPE( CWindfallRecievedDecisionPage, CDecisionPageBase );
public:
    CWindfallRecievedDecisionPage() : superclass() {}
    virtual ~CWindfallRecievedDecisionPage() {}

private: // declarations and variables
    PAR_PARSABLE;
    NON_COPYABLE( CWindfallRecievedDecisionPage );

private: // methods
    virtual bool GetDecisionResult() const;
};

#endif // GEN9_LANDING_PAGE_ENABLED

#endif // WINDFALL_RECIEVED_DECISION_PAGE_H
