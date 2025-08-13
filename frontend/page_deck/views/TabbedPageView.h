/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : TabbedPageView.h
// PURPOSE : Base for views that want a tab controller.
//
// NOTES   : Implements a tab controller but requires derive types to fill in 
//           blanks.
//
// AUTHOR  : james.strain
// STARTED : January 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef TABBED_PAGE_VIEW_H
#define TABBED_PAGE_VIEW_H

#include "frontend/landing_page/LandingPageConfig.h"

#if GEN9_LANDING_PAGE_ENABLED

// game
#include "frontend/page_deck/layout/PageLayoutItemParams.h"
#include "frontend/page_deck/PageViewBase.h"
#include "frontend/page_deck/views/PopulatablePageView.h"
#include "frontend/page_deck/views/TabController.h"

class CPageLayoutBase;

class CTabbedPageView : public CPopulatablePageView
{
    FWUI_DECLARE_DERIVED_TYPE( CTabbedPageView, CPopulatablePageView );
public:
    CTabbedPageView();
    virtual ~CTabbedPageView();

protected:
    void UpdateDerived( float const deltaMs ) override;
	void UpdateInputDerived() override;
    void OnExitCompleteDerived() override;

    void InitializeTabController( CComplexObject& parentObject, CPageLayoutBase& layout, CPageLayoutItemParams const params );
    void ShutdownTabController();

    u32 GetDefaultTabFocus() const;
    void AddTabItem( atHashString const label );
    void GoToTab( u32 const index );

    float GetTransitionDeltaNormalized() const;
    bool IsTransitioning() const { return m_transitionCountdown > 0.f; }
    int GetPreviousTabIndex() const { return m_previousTabIndex; }
    int GetActiveTabIndex() const { return m_tabController.GetActiveTabIndex(); }

    void PlayEnterAnimation() override;

private: // declarations and variables
    CTabController  m_tabController;
    int             m_previousTabIndex;
    float           m_transitionCountdown;
    float           m_transitionDuration;
    PAR_PARSABLE;
    NON_COPYABLE( CTabbedPageView );

private: // methods
    float GetTabTransitionDuration() const { return m_transitionDuration; }
    void OnTransitionToTabStart( u32 const index );
    void OnTransitionToTabEnd( u32 const index );
    void OnGoToTab( u32 const index );

    virtual u32 GetTabSourceCount() const = 0;
    virtual void OnTransitionToTabStartDerived( u32 const tabLeavingIdx ) = 0;
    virtual void OnTransitionToTabEndDerived( u32 const tabEnteringIdx ) = 0;
    virtual void OnGoToTabDerived( u32 const index ) = 0;

    void SwapToNextTab();
    void SwapToPreviousTab();
};

#endif // GEN9_LANDING_PAGE_ENABLED

#endif // TABBED_PAGE_VIEW_H
