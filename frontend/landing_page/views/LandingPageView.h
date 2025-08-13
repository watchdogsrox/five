/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : LandingPageView.h
// PURPOSE : Represents the landing page overview, which is comprised of a tab
//           control and some sub-sections comprised of grids.
//
// AUTHOR  : james.strain
// STARTED : January 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef LANDING_PAGE_VIEW_H
#define LANDING_PAGE_VIEW_H

#include "frontend/landing_page/LandingPageConfig.h"

#if GEN9_LANDING_PAGE_ENABLED

// rage
#include "atl/array.h"

// game
#include "frontend/page_deck/layout/DynamicLayoutContext.h"
#include "frontend/page_deck/layout/PageGrid.h"
#include "frontend/page_deck/layout/PageGridSimple.h"
#include "frontend/page_deck/PageViewBase.h"
#include "frontend/page_deck/views/TabbedPageView.h"

class CLandingPageView final : public CTabbedPageView
{
    FWUI_DECLARE_DERIVED_TYPE( CLandingPageView, CTabbedPageView );
public:
    CLandingPageView();
    virtual ~CLandingPageView();

private: // declarations and variables
    struct LayoutCategoryPair
    {
        CDynamicLayoutContext* m_dynamicLayoutContext;
        CPageItemCategoryBase* m_category;

        LayoutCategoryPair()
            : m_dynamicLayoutContext( nullptr )
            , m_category( nullptr )
        {

        }

        LayoutCategoryPair( CDynamicLayoutContext* const dynamicLayoutContext, CPageItemCategoryBase* const category )
            : m_dynamicLayoutContext( dynamicLayoutContext )
            , m_category( category )
        {

        }

        bool Refresh();
        void Cleanup();
    };

    typedef LambdaRef< void( LayoutCategoryPair& )>          PerLayoutCategoryPairLambda;
    typedef LambdaRef< void( LayoutCategoryPair const& )>    PerLayoutCategoryPairConstLambda;

    CPageGridSimple                     m_topLevelLayout; 
    CPageGrid                           m_bgScrollLayout;
    CComplexObject                      m_scrollerObject;

    atArray<LayoutCategoryPair>         m_perTabInfo;
    PAR_PARSABLE;
    NON_COPYABLE( CLandingPageView );

private: // methods
    void RecalculateGrids();

    bool OnEnterUpdateDerived( float const deltaMs ) final;
    void OnFocusGainedDerived() final;
    void UpdateDerived( float const deltaMs ) final;
    void OnFocusLostDerived() final;

    void PopulateDerived( IPageItemProvider& itemProvider ) final;
    bool IsPopulatedDerived() const { return m_perTabInfo.GetCount() > 0; }
    void CleanupDerived() final;

    void GetContentArea( Vec2f_InOut pos, Vec2f_InOut size ) const;
    CDynamicLayoutContext* GetActiveTabLayout();
    CDynamicLayoutContext const* GetActiveTabLayout() const;
    CDynamicLayoutContext* GetTabLayout( u32 const index );
    CDynamicLayoutContext const* GetTabLayout( u32 const index ) const;
    atHashString GetTabId( u32 const index ) const ;

    u32 GetTabSourceCount() const final;
    void OnTransitionToTabStartDerived( u32 const tabLeavingIdx ) final;
    void OnTransitionToTabEndDerived( u32 const tabEnteringIdx ) final;
    void OnGoToTabDerived( u32 const index ) final;
    void UpdateTabTransition( float const normalizedDelta );

    void UpdateInputDerived() final;
    void UpdateInstructionalButtonsDerived() final;
    void UpdateVisualsDerived() final;

    void UpdatePromptsAndTooltip();

    void PlayEnterAnimation() final;

    void ForEachLayoutPair( PerLayoutCategoryPairLambda action );
    void ForEachLayoutPair( PerLayoutCategoryPairConstLambda action ) const;

#if RSG_BANK
    void DebugRenderDerived() const final;
#endif 
};

#endif // GEN9_LANDING_PAGE_ENABLED

#endif // LANDING_PAGE_VIEW_H
