/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : TabbedPageView.cpp
// PURPOSE : Base for views that want a tab controller.
//
// NOTES   : Implements a tab controller but requires derive types to fill in 
//           blanks.
//
// AUTHOR  : james.strain
// STARTED : January 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "TabbedPageView.h"

#if GEN9_LANDING_PAGE_ENABLED

#include "TabbedPageView_parser.h"

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/page_deck/layout/PageLayoutBase.h"
#include "frontend/ui_channel.h"
#include "system/control.h"
#include "system/controlMgr.h"

FWUI_DEFINE_TYPE( CTabbedPageView, 0x1B33E980 );

CTabbedPageView::CTabbedPageView()
    : superclass()
    , m_previousTabIndex( INDEX_NONE )
    , m_transitionCountdown( 0.f )
    , m_transitionDuration( 250.f )
{
    
}

CTabbedPageView::~CTabbedPageView()
{
    ShutdownTabController();
}

void CTabbedPageView::UpdateDerived( float const deltaMs )
{
    if( IsTransitioning() )
    {
        m_transitionCountdown -= deltaMs;
        if( m_transitionCountdown <= 0.f )
        {
            OnTransitionToTabEnd( m_tabController.GetActiveTabIndex() );
        }
    }
}

void CTabbedPageView::UpdateInputDerived()
{
    if( !IsTransitioning() )
    {
        CControl& frontendControl = CControlMgr::GetMainFrontendControl();

        if( frontendControl.GetFrontendLB().IsReleased() )
        {
            SwapToPreviousTab();
        }
        else if( frontendControl.GetFrontendRB().IsReleased() )
        {
            SwapToNextTab();
        }
    }
}

void CTabbedPageView::OnExitCompleteDerived()
{
    ShutdownTabController();
    superclass::OnExitCompleteDerived();
}

void CTabbedPageView::InitializeTabController( CComplexObject& parentObject, CPageLayoutBase& layout, CPageLayoutItemParams const params )
{
    m_tabController.Initialize( parentObject );
    layout.RegisterItem( m_tabController, params );
}

void CTabbedPageView::ShutdownTabController()
{
    m_tabController.Shutdown();
}

u32 CTabbedPageView::GetDefaultTabFocus() const
{
    return 0;
}

void CTabbedPageView::AddTabItem( atHashString const label )
{
    m_tabController.AddTabItem( label );
}

void CTabbedPageView::GoToTab( u32 const index )
{
    u32 const c_tabCount = GetTabSourceCount();
    if( index < c_tabCount )
    {
        m_tabController.ActivateTab( index );
        OnGoToTabDerived( index );
    }
}

float CTabbedPageView::GetTransitionDeltaNormalized() const
{
    float const c_fragment = m_transitionDuration > 0.f ? ( 1.f / m_transitionDuration) : 0.f;
    float const c_result = 1.f - ( c_fragment * m_transitionCountdown );
    return c_result;
}

void CTabbedPageView::PlayEnterAnimation()
{
    m_tabController.PlayRevealAnimation(0);
}

void CTabbedPageView::OnTransitionToTabStart( u32 const index )
{
    uiFatalAssertf( !IsTransitioning(), "Logic error, double-transition not supported" );
    m_transitionCountdown = GetTabTransitionDuration();
    OnTransitionToTabStartDerived( index );
}

void CTabbedPageView::OnTransitionToTabEnd( u32 const index )
{
    m_transitionCountdown = 0.f;
    OnTransitionToTabEndDerived( index );
    m_previousTabIndex = INDEX_NONE;
}

void CTabbedPageView::OnGoToTab( u32 const index )
{
    m_transitionCountdown = 0.f;
    OnGoToTabDerived( index );
}

void CTabbedPageView::SwapToNextTab()
{
    int activeTab = m_tabController.GetActiveTabIndex();
    if( activeTab >= 0 )
    {
        if( m_tabController.SwapToNextTab() )
        {
            m_previousTabIndex = activeTab;
            activeTab = m_tabController.GetActiveTabIndex();
            OnTransitionToTabStart( (u32)m_previousTabIndex );
        }
    }
}

void CTabbedPageView::SwapToPreviousTab()
{
    int activeTab = m_tabController.GetActiveTabIndex();
    if( activeTab >= 0 )
    {
        if( m_tabController.SwapToPreviousTab() )
        {
            m_previousTabIndex = activeTab;
            activeTab = m_tabController.GetActiveTabIndex();
            OnTransitionToTabStart( (u32)m_previousTabIndex );
        }
    }
}

#endif // GEN9_LANDING_PAGE_ENABLED
