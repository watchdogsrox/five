/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : PillarPageView.cpp
// PURPOSE : Page view that implements N selectable pillar items
//
// AUTHOR  : james.strain
// STARTED : March 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "PillarPageView.h"

#if UI_PAGE_DECK_ENABLED
#include "PillarPageView_parser.h"

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/ui_channel.h"
#include "frontend/page_deck/IPageItemCollection.h"
#include "frontend/page_deck/IPageItemProvider.h"
#include "frontend/page_deck/IPageViewHost.h"
#include "frontend/page_deck/layout/DynamicLayoutContext.h"
#include "frontend/page_deck/PageItemBase.h"
#include "frontend/page_deck/PageItemCategoryBase.h"
#include "frontend/PauseMenu.h"

FWUI_DEFINE_TYPE( CPillarPageView, 0x24A46128 );

CPillarPageView::CPillarPageView()
    : superclass()
    , m_topLevelLayout( CPageGridSimple::GRID_3x8 )
    , m_contentLayout(CPageGridSimple::GRID_9x1, "careersItem" )
{

}

CPillarPageView::~CPillarPageView()
{
    Cleanup();
}

void CPillarPageView::RecalculateGrids()
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

    m_contentObject.SetPositionInPixels( contentPos );
    m_contentLayout.RecalculateLayout( contentPos, contentSize );
    m_contentLayout.NotifyOfLayoutUpdate();
}

void CPillarPageView::GetContentArea( Vec2f_InOut pos, Vec2f_InOut size ) const
{
    m_topLevelLayout.GetCell( pos, size, 0, 3, 3, 3 );
}

void CPillarPageView::PopulateDerived( IPageItemProvider& itemProvider )
{
    if( !IsPopulated() )
    {
        CComplexObject& pageRoot = GetPageRoot();

        CPageLayoutItemParams const c_tooltipParams( 1, 6, 1, 1 );
        InitializeTooltip( pageRoot, m_topLevelLayout, c_tooltipParams );

        CPageLayoutItemParams const c_titleItemParams( 1, 1, 1, 1 );
        InitializeTitle( pageRoot, m_topLevelLayout, c_titleItemParams );
        SetTitleLiteral( itemProvider.GetTitleString() );

        m_contentObject = pageRoot.CreateEmptyMovieClip( "contentArea", -1 );
        InitializeLayoutContext( itemProvider, m_contentObject, m_contentLayout );
        RecalculateGrids();
    }
}

void CPillarPageView::CleanupDerived()
{
    m_topLevelLayout.UnregisterAll();
    m_contentObject.Release();

    superclass::CleanupDerived();
}

#endif // UI_PAGE_DECK_ENABLED
