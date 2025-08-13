/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : PausePageView.h
// PURPOSE : Page view that renders the GTAV pause menu.
//
// AUTHOR  : james.strain
// STARTED : December 2020
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef PAUSE_PAGE_VIEW_H
#define PAUSE_PAGE_VIEW_H

#include "frontend/landing_page/LandingPageConfig.h"

#if GEN9_LANDING_PAGE_ENABLED

// game
#include "frontend/page_deck/PageViewBase.h"

class CPausePageView final : public CPageViewBase
{
    FWUI_DECLARE_DERIVED_TYPE( CPausePageView, CPageViewBase );
public:
    CPausePageView()
		: superclass()
        , m_blendStateHandle(grcStateBlock::BS_Invalid)
	{}
	virtual ~CPausePageView() {}

private: // declarations and variables
    grcBlendStateHandle m_blendStateHandle;
    mutable CSprite2d m_background; // Needs mutable for const render, as rendering changes state :(
    PAR_PARSABLE;
    NON_COPYABLE( CPausePageView );

private: // methods
    bool HasCustomRenderDerived() const final { return true; }
    void RenderDerived() const final;

    void OnEnterStartDerived() final;
    void OnExitStartDerived() final;

    void OnShutdown() final;
    void InitSprites();
    void CleanupSprites();
};

#endif // GEN9_LANDING_PAGE_ENABLED

#endif // PAUSE_PAGE_VIEW_H
