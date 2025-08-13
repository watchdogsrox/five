/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : IPageItemCollection.h
// PURPOSE : Interface used for walking a collection of items for a given page.
//
// NOTES   : It is not guaranteed that the items exist yet. Cloud content, for
//           example, may need requested. Requesting is split into base data
//           and rich data such as textures.
//
// AUTHOR  : james.strain
// STARTED : November 2020
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef I_PAGE_ITEM_COLLECTION_H
#define I_PAGE_ITEM_COLLECTION_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// rage
#include "atl/hashstring.h"
#include "system/bit.h"
#include "system/lambda.h"

// framework
#include "fwui/Interfaces/Interface.h"

class CPageItemBase;

class IPageItemCollection
{
    FWUI_DECLARE_INTERFACE( IPageItemCollection );
public: // declarations
    typedef LambdaRef< void( CPageItemBase& )>          PerItemLambda;
    typedef LambdaRef< void( CPageItemBase const& )>    PerItemConstLambda;

    enum class RequestOptions : rage::u32
    {
        INVALID   = 0,
        BASE_DATA = BIT0,               // Request just the base data
        RICH_DATA = BIT1,               // Request just the rich data
        DEFAULT = (BASE_DATA|RICH_DATA)
    };

    enum class RequestStatus : rage::u32
    {
        NONE,               // No request has been made yet
        FAILED,             // We were unable to satisfy this request
        CANCELLED,          // Request terminated before completion
        BUSY,               // Request could not be made, we are waiting on an existing request
        PENDING,            // Request was made, and we are pending results
        PENDING_RICH_DATA,  // We have received enough data to iterate items, but are still waiting on rich content like images
        SUCCESS,            // Request complete
    };

public: // methods
    void ForEachItem( PerItemLambda action ) { ForEachItemDerived( action ); }
    void ForEachItem( PerItemConstLambda action ) const { ForEachItemDerived( action ); }

    virtual RequestStatus RequestItemData( RequestOptions const options ) = 0;
    virtual RequestStatus UpdateRequest() = 0;
    virtual RequestStatus MarkAsTimedOut() = 0;
    virtual RequestStatus GetRequestStatus() const = 0;
    // TODO - No release baked in yet, or refresh. Will need to build interface further once network 
    // are involved in populating the landing page.

private:
    virtual void ForEachItemDerived( PerItemLambda action ) = 0;
    virtual void ForEachItemDerived( PerItemConstLambda action ) const = 0;
};

FWUI_DEFINE_INTERFACE( IPageItemCollection );

#endif // UI_PAGE_DECK_ENABLED

#endif // I_PAGE_ITEM_COLLECTION_H
