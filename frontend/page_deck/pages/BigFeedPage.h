/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : BigFeedPage.h
// PURPOSE : Bespoke page for bringing up the big feed
//
// AUTHOR  : james.strain
// STARTED : June 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef BIG_FEED_PAGE_H
#define BIG_FEED_PAGE_H

#include "frontend/landing_page/LandingPageConfig.h"

#if GEN9_LANDING_PAGE_ENABLED

// game
#include "frontend/page_deck/pages/DecisionPageBase.h"

class CBigFeedPage final : public CCategorylessPageBase
{
    FWUI_DECLARE_DERIVED_TYPE( CBigFeedPage, CCategorylessPageBase );
public:
    CBigFeedPage();
    virtual ~CBigFeedPage() {}

private: // declarations and variables
    bool m_isPriorityFeed;
    PAR_PARSABLE;
    NON_COPYABLE( CBigFeedPage );

private: // methods
    bool OnEnterUpdateDerived( float const deltaMs ) final;
    void OnEnterStartDerived() final;

    void OnFocusLostDerived() final;
    void OnExitCompleteDerived() final;

    bool IsTransitoryPageDerived() const final { return false; }

    atHashString GetPageContext() const;
};

#endif // GEN9_LANDING_PAGE_ENABLED

#endif // BIG_FEED_PAGE_H
