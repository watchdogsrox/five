/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : LandingPageView.cpp
// PURPOSE : Represents the landing page overview, which is comprised of a tab
//           control and some sub-sections comprised of grids.
//
// NOTES   : This isn't strictly a landing page view, but would work for any
//           other views that have a tab controller that slides in/out further
//           modal content selections.
//
//           I couldn't think of a better name :)
//
// AUTHOR  : james.strain
// STARTED : January 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "LandingPageView.h"

#if GEN9_LANDING_PAGE_ENABLED

#include "LandingPageView_parser.h"

// rage
#include "math/amath.h"
#include "scaleform/tweenstar.h"

// framework
#include "fwui/Input/fwuiInputQuantization.h"
#include "fwutil/xmacro.h"

// game
#include "frontend/ui_channel.h"
#include "frontend/page_deck/IPageItemCollection.h"
#include "frontend/page_deck/IPageItemProvider.h"
#include "frontend/page_deck/IPageViewHost.h"
#include "frontend/page_deck/messages/uiPageDeckMessages.h"
#include "frontend/page_deck/PageItemBase.h"
#include "frontend/page_deck/PageItemCategoryBase.h"
#include "frontend/VideoEditor/ui/InstructionalButtons.h"
#include "network/live/NetworkTelemetry.h"

#if RSG_BANK
#include "grcore/quads.h"
#endif // RSG_BANK

#include "system/control.h"
#include "system/controlMgr.h"

FWUI_DEFINE_TYPE( CLandingPageView, 0x56AD051E );

CLandingPageView::CLandingPageView()
    : superclass()
    , m_topLevelLayout( CPageGridSimple::GRID_3x5 )
{
    
}

CLandingPageView::~CLandingPageView()
{
    Cleanup();
}

void CLandingPageView::LayoutCategoryPair::Cleanup()
{
    if( m_dynamicLayoutContext )
    {
        m_dynamicLayoutContext->Shutdown();
        delete m_dynamicLayoutContext;
    }
}

bool CLandingPageView::LayoutCategoryPair::Refresh()
{
    bool changed = false;

    if( m_dynamicLayoutContext )
    {
        // There is not currently a callback for network status changing,
        // so we poll the category for changes
        changed = m_category && m_category->RefreshContent();
        if( changed )
        {
            m_dynamicLayoutContext->RemoveItems();
            m_dynamicLayoutContext->AddCategory( *m_category );
        }
    }

    return changed;
}

void CLandingPageView::RecalculateGrids()
{
    // Order is important here, as some of the sizing cascades
    // So if you adjust the order TEST IT!
    m_topLevelLayout.Reset();

    // Screen area is acceptable as our grids are all using star sizing
    Vec2f const c_screenExtents = uiPageConfig::GetScreenArea(); 
    m_topLevelLayout.RecalculateLayout( Vec2f( Vec2f::ZERO ), c_screenExtents );
    m_topLevelLayout.NotifyOfLayoutUpdate();

    Vec2f contentPos( Vec2f::ZERO );
    Vec2f contentSize( Vec2f::ZERO );
    GetContentArea( contentPos, contentSize );
    
    int const c_categoryCount = (int)GetTabSourceCount();
    Vec2f const c_contentExtents = contentSize * Vec2f( (float)c_categoryCount, 1.f );
    m_bgScrollLayout.RecalculateLayout( contentPos, c_contentExtents );
    m_scrollerObject.SetPositionInPixels( contentPos );
    m_bgScrollLayout.NotifyOfLayoutUpdate();
}

bool CLandingPageView::OnEnterUpdateDerived( float const deltaMs )
{
    bool isComplete = superclass::OnEnterUpdateDerived( deltaMs );

    // url:bugstar:7021748 - [UILanding] - Views currently wait for all textures to be loaded
    // Still waiting on animation assets from Gareth, so for now I'm still blocking the landing page on all assets being available
    isComplete = isComplete && AreAllItemTexturesAvailable();

    if( !isComplete )
    {
        // Keep the page invisible until required
        // Change this to an animation and spinners when(if) designs arrive.
        GetPageRoot().SetVisible( false );

        if( IsTransitionTimeoutExpired() )
        {
            // url:bugstar:7021703 covers implementing fallback views/missing assets
            // for now just continue with the missing textures.
            OnEnterTimeout();
            isComplete = true;
        }
    }

    return isComplete;
}

void CLandingPageView::OnFocusGainedDerived()
{
    CDynamicLayoutContext* const activeTabLayout = GetActiveTabLayout();
    if( activeTabLayout )
    {
		// RM_DND - bugstar://7231864
		// may want to revisit how we generate this page event but for now it's here to force refresh following a possible entitlement change.				
		uiRefreshStoryItemMsg refreshStoryItemMessage;
		activeTabLayout->OnPageEvent(refreshStoryItemMessage);

        activeTabLayout->RefreshFocusVisuals();
	}

    GetPageRoot().SetVisible( true );
    UpdatePromptsAndTooltip();
}

void CLandingPageView::UpdateDerived( float const deltaMs )
{
    superclass::UpdateDerived( deltaMs );

    if( IsTransitioning() )
    {
        float const c_normalizedDelta = GetTransitionDeltaNormalized();
        UpdateTabTransition( c_normalizedDelta );
    }
    else
    {
        UpdateInput();
        
        bool didAnyRefresh = false;
        auto func = [&didAnyRefresh]( LayoutCategoryPair& currentPair )
        {
            didAnyRefresh = currentPair.Refresh() || didAnyRefresh;
        };
        ForEachLayoutPair( func );

        if( didAnyRefresh )
        {
            RecalculateGrids();
            OnFocusGained( false );
        }
    }
}

void CLandingPageView::OnFocusLostDerived()
{
    CDynamicLayoutContext* const activeTabLayout = GetActiveTabLayout();
    if( activeTabLayout )
    {
        activeTabLayout->ClearFocusVisuals();
    }

    GetPageRoot().SetVisible( false );
    ClearPromptsAndTooltip();
}

void CLandingPageView::PopulateDerived( IPageItemProvider& itemProvider )
{
    if( !IsPopulated() )
    {
        CComplexObject& pageRoot = GetPageRoot();

        // Initialize our tab controller
        CPageLayoutItemParams const c_tabControllerParams( 1, 1, 1, 1 );
        InitializeTabController( pageRoot, m_topLevelLayout, c_tabControllerParams );

        m_scrollerObject = pageRoot.CreateEmptyMovieClip( "landingViewScroller", -1 );

        CPageLayoutItemParams const c_tooltipParams( 1, 4, 1, 1 );
        InitializeTooltip( pageRoot, m_topLevelLayout, c_tooltipParams );

        // Manually reset rows/columns, as we don't want a starting 1x1 grid
        m_bgScrollLayout.ResetRows();
        m_bgScrollLayout.ResetColumns();
        m_bgScrollLayout.AddRow( uiPageDeck::SIZING_TYPE_STAR, 1.f );

        auto categoryFunc = [this]( CPageItemCategoryBase& category )
        {
            atHashString const c_title = category.GetTitle();
            CPageLayoutBase * const c_layout = category.GetLayout();
            if( uiVerifyf( c_layout, "Category " HASHFMT " lacks a valid layout", HASHOUT(c_title ) ) )
            {
                CDynamicLayoutContext * const dynamicContext = rage_new CDynamicLayoutContext( m_scrollerObject, category );
                m_perTabInfo.PushAndGrow( LayoutCategoryPair( dynamicContext, &category ) );

                AddTabItem( c_title );

                int const c_columnCount = (int)m_bgScrollLayout.GetColumnCount();
                m_bgScrollLayout.AddCol( uiPageDeck::SIZING_TYPE_STAR, 1.f );
                m_bgScrollLayout.RegisterItem( *dynamicContext, CPageLayoutItemParams( c_columnCount, 0, 1, 1 ) );
            }
        };
        itemProvider.ForEachCategory( categoryFunc );

        u32 const c_tabFocus = GetDefaultTabFocus();
        GoToTab( c_tabFocus );
        RecalculateGrids();
    }
}

void CLandingPageView::CleanupDerived()
{
    m_topLevelLayout.UnregisterAll();
    ShutdownTabController();
    ShutdownTooltip();

    m_bgScrollLayout.UnregisterAll();
    m_scrollerObject.Release();


    auto cleanupFunc = []( LayoutCategoryPair& tabInfo )
    {
        tabInfo.Cleanup();
    };    
    ForEachLayoutPair( cleanupFunc );
    m_perTabInfo.ResetCount();

    // TODO_UI - Release rich/base data here? Do underlying request systems have their own caching?
}

void CLandingPageView::GetContentArea( Vec2f_InOut pos, Vec2f_InOut size ) const
{
    m_topLevelLayout.GetCell( pos, size, 0, 2, 3, 1 );
}

CDynamicLayoutContext* CLandingPageView::GetActiveTabLayout()
{
    int const c_currentTabIdx = GetActiveTabIndex();

    CDynamicLayoutContext* const result = c_currentTabIdx >= 0 ? GetTabLayout( (u32)c_currentTabIdx ) : nullptr;
    return result;
}

CDynamicLayoutContext const* CLandingPageView::GetActiveTabLayout() const
{
    int const c_currentTabIdx = GetActiveTabIndex();

    CDynamicLayoutContext const * const c_result = c_currentTabIdx >= 0 ? GetTabLayout( (u32)c_currentTabIdx ) : nullptr;
    return c_result;
}

CDynamicLayoutContext* CLandingPageView::GetTabLayout( u32 const index )
{
    CDynamicLayoutContext* const result = index < m_perTabInfo.GetCount() ? m_perTabInfo[index].m_dynamicLayoutContext : nullptr;
    return result;
}

CDynamicLayoutContext const* CLandingPageView::GetTabLayout( u32 const index ) const
{
    CDynamicLayoutContext const * const c_result = index < m_perTabInfo.GetCount() ? m_perTabInfo[index].m_dynamicLayoutContext : nullptr;
    return c_result;
}

atHashString CLandingPageView::GetTabId( u32 const index ) const
{
    CPageItemCategoryBase const * const c_category = index < (u32)m_perTabInfo.GetCount() ? m_perTabInfo[index].m_category : nullptr;
    return c_category ? c_category->GetId() : atHashString();
}

void CLandingPageView::OnTransitionToTabStartDerived( u32 const tabLeavingIdx )
{
    CDynamicLayoutContext* const tabLeaving = GetTabLayout( tabLeavingIdx );
    if( tabLeaving )
    {
        tabLeaving->InvalidateFocus();
        ClearTooltip();
    }
}

void CLandingPageView::OnTransitionToTabEndDerived( u32 const tabEnteringIdx )
{
    // GoToTab sets focus, and will ensure we get a last good tap of our position animation as it snaps
    GoToTab( tabEnteringIdx );
}

void CLandingPageView::OnGoToTabDerived( u32 const index )
{
    Vec2f contentPos( Vec2f::ZERO );
    Vec2f contentSize( Vec2f::ZERO );
    GetContentArea( contentPos, contentSize );

    float const c_contentWidth = contentSize.GetX();
    float const c_endPosition = c_contentWidth * index;

    m_scrollerObject.SetXPositionInPixels( -c_endPosition );

    CDynamicLayoutContext* const activeTabLayout = GetActiveTabLayout();
    if( activeTabLayout )
    {
        activeTabLayout->SetDefaultFocus();
        UpdatePromptsAndTooltip();

        atHashString const c_tabId = GetTabId(index);

        if ( c_tabId.IsNotNull() )
        {
            CNetworkTelemetry::LandingPageTabEntered(c_tabId);
        }
    }
}

u32 CLandingPageView::GetTabSourceCount() const
{
    return (u32)m_perTabInfo.GetCount();
}

void CLandingPageView::UpdateTabTransition( float const normalizedDelta )
{
    int const c_previousTabIndex = GetPreviousTabIndex();
    int const c_activeTabIndex = GetActiveTabIndex();

    Vec2f contentPos( Vec2f::ZERO );
    Vec2f contentSize( Vec2f::ZERO );
    GetContentArea( contentPos, contentSize );

    float const c_perPageOffset = 0.f - contentSize.GetX();

    float const c_startPosition = c_perPageOffset * c_previousTabIndex;
    float const c_endPosition = c_perPageOffset * c_activeTabIndex;

    float const c_quarticDelta = rage::TweenStar::ComputeEasedValue( normalizedDelta, rage::TweenStar::EASE_quartic_EO );
    float const c_finalX = rage::Lerp( c_quarticDelta, c_startPosition, c_endPosition );
    m_scrollerObject.SetXPositionInPixels( c_finalX );
}

void CLandingPageView::UpdateInputDerived()
{
    superclass::UpdateInputDerived();

    CDynamicLayoutContext* const activeTabLayout = GetActiveTabLayout();
    if( activeTabLayout )
    {
        fwuiInput::eHandlerResult result( fwuiInput::ACTION_NOT_HANDLED );

        if( CPauseMenu::CheckInput( FRONTEND_INPUT_ACCEPT ) )
        {
            result = activeTabLayout->HandleInputAction( FRONTEND_INPUT_ACCEPT, GetViewHost() );
            TriggerInputAudio( result, UI_AUDIO_TRIGGER_SELECT, UI_AUDIO_TRIGGER_ERROR );
        }
        else
        {
            int deltaX = 0;
            int deltaY = 0;

            // If CPauseMenu navigation results for analogue input are not accurate enough,
            // consider using the fwuiInput Quantization and direct reads of the analogue values
            if( CPauseMenu::CheckInput( FRONTEND_INPUT_LEFT ) )
            {
                deltaX = -1;
            }
            else if( CPauseMenu::CheckInput( FRONTEND_INPUT_RIGHT ) )
            {
                deltaX = 1;
            }

            if( CPauseMenu::CheckInput( FRONTEND_INPUT_UP ) )
            {
                deltaY = -1;
            }
            else if( CPauseMenu::CheckInput( FRONTEND_INPUT_DOWN ) )
            {
                deltaY = 1;
            }

            if( deltaX != 0 || deltaY != 0 )
            {
                result = activeTabLayout->HandleDigitalNavigation( deltaX, deltaY );
                char const * const c_positiveTrigger = ( deltaX != 0 ) ? UI_AUDIO_TRIGGER_NAV_LEFT_RIGHT : UI_AUDIO_TRIGGER_NAV_UP_DOWN;
                TriggerInputAudio( result, c_positiveTrigger, nullptr );

                // Focus changed, so we need to re-calculate our tool-tip and prompts
                if( result == fwuiInput::ACTION_HANDLED )
                {
                    UpdatePromptsAndTooltip();
                }
            }
        }
    }
}

void CLandingPageView::UpdateInstructionalButtonsDerived()
{
    CDynamicLayoutContext const * const c_activeTabLayout = GetActiveTabLayout();
    if( c_activeTabLayout )
    {
        c_activeTabLayout->UpdateInstructionalButtons();
    }

    superclass::UpdateInstructionalButtonsDerived();
}

void CLandingPageView::UpdateVisualsDerived()
{
    CDynamicLayoutContext * const activeTabLayout = GetActiveTabLayout();
    if( activeTabLayout )
    {
        activeTabLayout->UpdateVisuals();
    }
}

void CLandingPageView::UpdatePromptsAndTooltip()
{
    CDynamicLayoutContext* const activeTabLayout = GetActiveTabLayout();
    if( activeTabLayout )
    {
        char const * const c_tooltip = activeTabLayout->GetFocusTooltip();
        if( c_tooltip )
        {
            SetTooltipLiteral( c_tooltip, true );
        }
        else
        {
            ClearTooltip();
        }

        RefreshPrompts();
    }
}

void CLandingPageView::PlayEnterAnimation()
{
    superclass::PlayEnterAnimation();

    CDynamicLayoutContext* const activeTabLayout = GetActiveTabLayout();
    if( activeTabLayout )
    {
        activeTabLayout->PlayEnterAnimation();
    }
}

void CLandingPageView::ForEachLayoutPair( PerLayoutCategoryPairLambda action )
{
    int const c_count = m_perTabInfo.GetCount();
    for( int index = 0; index < c_count; ++index )
    {
        LayoutCategoryPair& currentItem = m_perTabInfo[index];
        action( currentItem );
    }
}

void CLandingPageView::ForEachLayoutPair( PerLayoutCategoryPairConstLambda action) const
{
    int const c_count = m_perTabInfo.GetCount();
    for( int index = 0; index < c_count; ++index )
    {
        LayoutCategoryPair const& c_currentItem = m_perTabInfo[index];
        action( c_currentItem );
    }
}

#if RSG_BANK

void CLandingPageView::DebugRenderDerived() const 
{
    m_topLevelLayout.DebugRender();
    m_bgScrollLayout.DebugRender();
}

#endif // RSG_BANK

#endif // GEN9_LANDING_PAGE_ENABLED
