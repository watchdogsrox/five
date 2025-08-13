/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : PopulatablePageView.cpp
// PURPOSE : Base class for page views with default population mechanics
//
// AUTHOR  : james.strain
// STARTED : March 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "PopulatablePageView.h"

#if UI_PAGE_DECK_ENABLED

#include "PopulatablePageView_parser.h"

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/page_deck/IPageItemProvider.h"
#include "frontend/page_deck/PageItemCategoryBase.h"

FWUI_DEFINE_TYPE( CPopulatablePageView, 0xBFA1DA6C );

void CPopulatablePageView::OnEnterStartDerived()
{
    auto categoryFunc = []( CPageItemCategoryBase& category )
    {
        category.RequestItemData( IPageItemCollection::RequestOptions::DEFAULT );
    };

    IPageItemProvider& itemProvider = GetItemProvider();
    itemProvider.ForEachCategory( categoryFunc );
}

bool CPopulatablePageView::OnEnterUpdateDerived( float const deltaMs )
{
    bool canPopulate = superclass::OnEnterUpdateDerived( deltaMs );

    auto categoryFunc = [&canPopulate]( CPageItemCategoryBase& category )
    {
        IPageItemCollection::RequestStatus const c_status = category.UpdateRequest();
        canPopulate = canPopulate && ( c_status == IPageItemCollection::RequestStatus::SUCCESS || c_status == IPageItemCollection::RequestStatus::PENDING_RICH_DATA );
    };

    IPageItemProvider& itemProvider = GetItemProvider();
    itemProvider.ForEachCategory( categoryFunc );

    if( canPopulate )
    {
        Populate( itemProvider );
    }

    return canPopulate;
}

bool CPopulatablePageView::OnEnterTimeoutDerived()
{
    bool canPopulate = superclass::OnEnterTimeoutDerived();

    auto categoryFunc = [&canPopulate](CPageItemCategoryBase& category)
    {
        IPageItemCollection::RequestStatus const c_status = category.MarkAsTimedOut();
        canPopulate = canPopulate && (c_status == IPageItemCollection::RequestStatus::SUCCESS || c_status == IPageItemCollection::RequestStatus::PENDING_RICH_DATA);
    };

    IPageItemProvider& itemProvider = GetItemProvider();
    itemProvider.ForEachCategory(categoryFunc);

    if (canPopulate)
    {
        Populate(itemProvider);
    }

    return canPopulate;
}

void CPopulatablePageView::OnShutdown()
{
    Cleanup();
}

#endif // UI_PAGE_DECK_ENABLED
