/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : BinkPageView.h
// PURPOSE : Page view that renders a configured bink.
//
// AUTHOR  : brian.beacom
// STARTED : September 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef BINK_PAGE_VIEW_H
#define BINK_PAGE_VIEW_H

#include "frontend/landing_page/LandingPageConfig.h"

#if GEN9_LANDING_PAGE_ENABLED

// game
#include "frontend/page_deck/PageViewBase.h"

namespace rage
{
    class bwMovie;
    class audSound;
};

class CBinkPageView final : public CPageViewBase
{
    FWUI_DECLARE_DERIVED_TYPE(CBinkPageView, CPageViewBase);
public:
    CBinkPageView()
        : superclass(), 
          m_movieHandle(0),
          m_movie(nullptr),
          m_pSound(nullptr),
          m_binkPlaybackScene(nullptr),
          m_isCleanedUp(false)
    {}
    virtual ~CBinkPageView() {}

private: // declarations and variables
    audScene* m_binkPlaybackScene;
    audSound* m_pSound;

    ConstString m_movieName;
    bwMovie* m_movie;
    u32 m_movieHandle;

    uiPageDeckMessageBase* m_onFinishMessage;
    bool m_isCleanedUp;
    PAR_PARSABLE;
    NON_COPYABLE(CBinkPageView);

private: // methods
    bool HasCustomRenderDerived() const final { return true; }

    virtual void OnEnterStartDerived() final;
    virtual void UpdateDerived(float const deltaMs) final;
    virtual void RenderDerived() const final;

    void CreateSound();
};

#endif // GEN9_LANDING_PAGE_ENABLED

#endif // BINK_PAGE_VIEW_H
