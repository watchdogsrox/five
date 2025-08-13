/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : BigFeedDecisionPage.h
// PURPOSE : Are there any big feed items to view?
//
// AUTHOR  : james.strain
// STARTED : August 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef BIG_FEED_DECISION_PAGE_H
#define BIG_FEED_DECISION_PAGE_H

#include "frontend/landing_page/LandingPageConfig.h"

#if GEN9_LANDING_PAGE_ENABLED

// game
#include "frontend/page_deck/pages/DecisionPageBase.h"

class CBigFeedDecisionPage : public CDecisionPageBase
{
    FWUI_DECLARE_DERIVED_TYPE(CBigFeedDecisionPage, CDecisionPageBase );
public:
	CBigFeedDecisionPage() : superclass() {}
    virtual ~CBigFeedDecisionPage() {}

private: // declarations and variables
    PAR_PARSABLE;
    NON_COPYABLE(CBigFeedDecisionPage);

private: // methods
    bool HasDecisionResult() const final;
    bool GetDecisionResult() const final;
    float GetTransitionTimeout() const final;
};

#endif // GEN9_LANDING_PAGE_ENABLED

#endif // BIG_FEED_DECISION_PAGE_H
