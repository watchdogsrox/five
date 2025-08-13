/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : StorePageView.h
// PURPOSE : Page view that renders the store menu.
//
// AUTHOR  : james.strain
// STARTED : July 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef STORE_PAGE_VIEW_H
#define STORE_PAGE_VIEW_H

#include "frontend/landing_page/LandingPageConfig.h"

#if GEN9_LANDING_PAGE_ENABLED

// game
#include "frontend/page_deck/PageViewBase.h"

class CStorePageView final : public CPageViewBase
{
    FWUI_DECLARE_DERIVED_TYPE(CStorePageView, CPageViewBase );
public:
    CStorePageView()
        : superclass()
    {}
    virtual ~CStorePageView() {}
    
private: // declarations and variables
    PAR_PARSABLE;
    NON_COPYABLE(CStorePageView);

private: // methods
    bool HasCustomRenderDerived() const final { return true; }
	void UpdateDerived( float const deltaMs ) final;
    void RenderDerived() const final;
};

#endif // GEN9_LANDING_PAGE_ENABLED

#endif // STORE_PAGE_VIEW_H
