/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CarouselPageView.h
// PURPOSE : Page view that implements a linear carousel for showing N items
//
// AUTHOR  : james.strain
// STARTED : March 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef CAROUSEL_PAGE_VIEW_H
#define CAROUSEL_PAGE_VIEW_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// game
#include "frontend/page_deck/layout/PageGridSimple.h"
#include "frontend/page_deck/views/Carousel.h"
#include "frontend/page_deck/views/FeaturedImage.h"
#include "frontend/page_deck/views/FeaturedItem.h"
#include "frontend/page_deck/views/PageTitle.h"
#include "frontend/page_deck/views/PopulatablePageView.h"

class CDynamicLayoutContext;

class CCarouselPageView final : public CPopulatablePageView
{
    FWUI_DECLARE_DERIVED_TYPE( CCarouselPageView, CPopulatablePageView );
public:
    CCarouselPageView();
    virtual ~CCarouselPageView();

private: // declarations and variables
    CPageGridSimple         m_topLevelLayout;
    CCarousel               m_carousel;
    CPageTitle              m_pageTitle;
    CFeaturedImage          m_featuredImage;
    CFeaturedItem           m_featuredItem;
    PAR_PARSABLE;
    NON_COPYABLE( CCarouselPageView );

private: // methods
    void RecalculateGrids();

    void UpdateDerived( float const deltaMs ) final;

    void OnFocusGainedDerived() final;
    void OnFocusLostDerived() final;

    void PopulateDerived( IPageItemProvider& itemProvider ) final;
    bool IsPopulatedDerived() const final;
    void CleanupDerived() final;

    void UpdateInputDerived() final;
    void UpdateInstructionalButtonsDerived() final;
    void UpdateVisualsDerived() final;

    void UpdateFeaturedItem();
    void ClearFeaturedItem();

    void UpdatePromptsAndTooltip();

    void PlayEnterAnimation() final;
};

#endif // UI_PAGE_DECK_ENABLED

#endif // CAROUSEL_PAGE_VIEW_H
