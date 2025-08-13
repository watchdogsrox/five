/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : IPageItemProvider.h
// PURPOSE : Top level interface for iterating content we want on a page
//
// NOTES   : Has support for categories of items, and walking all items within a 
//           a category.
//
//           Note that some derived classes may have virtualized sets of items,
//           so there may be costs to walking large sets.
//
// AUTHOR  : james.strain
// STARTED : November 2020
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef I_PAGE_ITEM_PROVIDER_H
#define I_PAGE_ITEM_PROVIDER_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// rage
#include "atl/hashstring.h"
#include "system/lambda.h"

// framework
#include "fwui/Interfaces/Interface.h"

// game
#include "frontend/page_deck/PageItemCategoryBase.h"

class IPageItemCategory;

class IPageItemProvider
{
    FWUI_DECLARE_INTERFACE( IPageItemProvider );
public: // declarations
    typedef LambdaRef< void( CPageItemCategoryBase& )>          PerCategoryLambda;
    typedef LambdaRef< void( CPageItemCategoryBase const& )>    PerCategoryConstLambda;

public: // methods
    virtual char const * GetTitleString() const = 0;
    virtual size_t GetCategoryCount() const = 0;

    void ForEachCategory( PerCategoryLambda action ) { ForEachCategoryDerived( action ); }
    void ForEachCategory( PerCategoryConstLambda action ) const { ForEachCategoryDerived( action ); }

private:
    virtual void ForEachCategoryDerived( PerCategoryLambda action ) = 0;
    virtual void ForEachCategoryDerived( PerCategoryConstLambda action ) const = 0;
};

FWUI_DEFINE_INTERFACE( IPageItemProvider );

#endif // UI_PAGE_DECK_ENABLED

#endif // I_PAGE_ITEM_PROVIDER_H
