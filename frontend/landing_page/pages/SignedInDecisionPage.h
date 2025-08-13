/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : SignedInDecisionPage.h
// PURPOSE : Is the main player signed in
//
// AUTHOR  : james.strain
// STARTED : November 2020
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef SIGNED_IN_DECISION_PAGE_H
#define SIGNED_IN_DECISION_PAGE_H

#include "frontend/landing_page/LandingPageConfig.h"

#if GEN9_LANDING_PAGE_ENABLED

// game
#include "frontend/page_deck/pages/DecisionPageBase.h"

class CSignedInDecisionPage : public CDecisionPageBase
{
    FWUI_DECLARE_DERIVED_TYPE( CSignedInDecisionPage, CDecisionPageBase );
public:
    CSignedInDecisionPage() : superclass() {}
    virtual ~CSignedInDecisionPage() {}

private: // declarations and variables
    PAR_PARSABLE;
    NON_COPYABLE( CSignedInDecisionPage );

private: // methods
    virtual bool GetDecisionResult() const;
};

#endif // GEN9_LANDING_PAGE_ENABLED

#endif // SIGNED_IN_DECISION_PAGE_H
