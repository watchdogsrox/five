/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : FTUECompleteDecisionPage.h
// PURPOSE : Has the main player already completed the first time user experience
//
// AUTHOR  : james.strain
// STARTED : November 2020
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef FTUE_COMPLETE_DECISION_PAGE_H
#define FTUE_COMPLETE_DECISION_PAGE_H

#include "frontend/landing_page/LandingPageConfig.h"

#if GEN9_LANDING_PAGE_ENABLED

// game
#include "frontend/page_deck/pages/DecisionPageBase.h"

class CFTUECompleteDecisionPage : public CDecisionPageBase
{
    FWUI_DECLARE_DERIVED_TYPE( CFTUECompleteDecisionPage, CDecisionPageBase );
public:
    CFTUECompleteDecisionPage() : superclass() {}
    virtual ~CFTUECompleteDecisionPage() {}

private: // declarations and variables
    PAR_PARSABLE;
    NON_COPYABLE( CFTUECompleteDecisionPage );

private: // methods
    virtual bool GetDecisionResult() const;
};

#endif // GEN9_LANDING_PAGE_ENABLED

#endif // FTUE_COMPLETE_DECISION_PAGE_H
