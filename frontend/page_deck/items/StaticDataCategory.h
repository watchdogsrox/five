/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : StaticDataCategory.h
// PURPOSE : An item category made purely of static data
//           NOTE - Static may still mean async, if we are waiting on streaming
//
// AUTHOR  : james.strain
// STARTED : January 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef STATIC_DATA_CATEGORY_H
#define STATIC_DATA_CATEGORY_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// rage
#include "atl/array.h"

// game
#include "frontend/page_deck/PageItemCategoryBase.h"

class CPageItemBase;

class CStaticDataCategory : public CPageItemCategoryBase
{
    typedef CPageItemCategoryBase superclass;
public:
    CStaticDataCategory() {}
    virtual ~CStaticDataCategory();

    RequestStatus RequestItemData( RequestOptions const options ) final;
    RequestStatus UpdateRequest() final;
    RequestStatus MarkAsTimedOut() final;
    RequestStatus GetRequestStatus() const final;

private: // declarations and variables
    PAR_PARSABLE;
    typedef rage::atArray<CPageItemBase*>   ItemCollection;
    typedef ItemCollection::iterator        ItemIterator;
    typedef ItemCollection::const_iterator  ConstItemIterator;

    ItemCollection  m_items;

private: // methods
    void ForEachItemDerived( superclass::PerItemLambda action ) final;
    void ForEachItemDerived( superclass::PerItemConstLambda action ) const final;
};

#endif // UI_PAGE_DECK_ENABLED

#endif // STATIC_DATA_CATEGORY_H
