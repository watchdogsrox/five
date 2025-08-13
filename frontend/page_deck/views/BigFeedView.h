/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : BigFeedView.h
// PURPOSE : Page view that renders the GTAV big feed.
//
// AUTHOR  : james.strain
// STARTED : June 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef BIG_FEED_VIEW_H
#define BIG_FEED_VIEW_H

#include "frontend/landing_page/LandingPageConfig.h"

#if GEN9_LANDING_PAGE_ENABLED

// game
#include "frontend/page_deck/PageViewBase.h"
#include "frontend/page_deck/views/PipCounter.h"

class CBigFeedView final : public CPageViewBase
{
    FWUI_DECLARE_DERIVED_TYPE( CBigFeedView, CPageViewBase );
public:
    CBigFeedView();
    virtual ~CBigFeedView();

    static void Render();

private: // declarations and variables
    uiPageDeckMessageBase* m_skipAction;
    CPageGridSimple m_topLevelLayout; 
    CComplexObject  m_newsMovieRoot;
    CPipCounter     m_pipCounter;
    float           m_skipDelayMs;
    u32             m_skipDelayTotalTimeMs;
    u32             m_viewStartTimeMs;
    s32             m_newsMovieId;
    bool            m_currentStoryHasLink;
    bool            m_metricNavReported;
    PAR_PARSABLE;
    NON_COPYABLE( CBigFeedView );

private: // methods
    void RecalculateGrids();

    bool IsPriorityFeedMode() const;
    bool CanActionFeeds() const;
    bool IsCurrentStoryWithLink() const;
    bool HasCustomRenderDerived() const final { return true; }
    void RenderDerived() const final;

    void OnEnterStartDerived() final;
    bool OnEnterUpdateDerived( float const deltaMs ) final;
    void OnEnterCompleteDerived() final;

    void UpdateDerived( float const deltaMs ) final;
    void UpdateInputDerived() final;
    void UpdateInstructionalButtonsDerived() final;
    void UpdateVisualsDerived() final;

    void OnExitCompleteDerived();
    void UpdateAcceptButton( float const deltaMs );

    void OnNewsMovieFirstLoaded();
    void OnNewsMovieShutdown();

    void SetupPipCounter();
    void ReportMetricNav(const u32 trackingId, const atHashString leftBy);
};

#endif // GEN9_LANDING_PAGE_ENABLED

#endif // BIG_FEED_VIEW_H
