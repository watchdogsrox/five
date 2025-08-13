/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : StaticDataCategory.cpp
// PURPOSE : An item category made purely of static data
//           NOTE - Static may still mean async, if we are waiting on streaming
//
// AUTHOR  : james.strain
// STARTED : January 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "StaticDataCategory.h"
#if UI_PAGE_DECK_ENABLED
#include "StaticDataCategory_parser.h"

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/page_deck/PageItemBase.h"
#include "frontend/ui_channel.h"

CStaticDataCategory::~CStaticDataCategory()
{
    auto shutdownFunc = []( CPageItemBase& currentItem )
    {
        delete &currentItem;
    };
    superclass::ForEachItem( shutdownFunc );
    m_items.ResetCount();
}

CStaticDataCategory::RequestStatus CStaticDataCategory::RequestItemData( RequestOptions const UNUSED_PARAM(options) )
{
    return RequestStatus::SUCCESS;
}

CStaticDataCategory::RequestStatus CStaticDataCategory::UpdateRequest()
{
    return RequestStatus::SUCCESS;
}

CStaticDataCategory::RequestStatus CStaticDataCategory::MarkAsTimedOut()
{
    return RequestStatus::SUCCESS;
}

CStaticDataCategory::RequestStatus CStaticDataCategory::GetRequestStatus() const 
{
    return RequestStatus::SUCCESS;
}

void CStaticDataCategory::ForEachItemDerived( superclass::PerItemLambda action )
{
    for( ItemIterator itr = m_items.begin(); itr != m_items.end(); ++itr )
    {
        if( *itr )
        {
            action( **itr );
        }
    }
}

void CStaticDataCategory::ForEachItemDerived( superclass::PerItemConstLambda action ) const
{
    for( ConstItemIterator itr = m_items.begin(); itr != m_items.end(); ++itr )
    {
        if( *itr )
        {
            action( **itr );
        }
    }
}

#endif // UI_PAGE_DECK_ENABLED
