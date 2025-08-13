/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : StaticDataPage.h
// PURPOSE : Page that contains a parGen defined set of categories.
//           NOTE - Static just refers to the fact the categories are pre-defined
//           on disc. A category may be cloud populated itself.
//
// AUTHOR  : james.strain
// STARTED : October 2020
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef STATIC_DATA_PAGE_H
#define STATIC_DATA_PAGE_H


#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// rage
#include "atl/array.h"

// game
#include "frontend/page_deck/PageBase.h"

class CPageItemCategoryBase;

class CStaticDataPage : public CPageBase
{
    FWUI_DECLARE_DERIVED_TYPE( CStaticDataPage, CPageBase );
public:
    CStaticDataPage() : superclass() {}
    virtual ~CStaticDataPage();

private: // declarations and variables
    typedef rage::atArray<CPageItemCategoryBase*>   CategoryCollection;
    typedef CategoryCollection::iterator            CategoryIterator;
    typedef CategoryCollection::const_iterator      ConstCategoryIterator;
    CategoryCollection m_categories;

    PAR_PARSABLE;
    NON_COPYABLE( CStaticDataPage );

private: // methods

    size_t GetCategoryCount() const final { return m_categories.size(); }
    void ForEachCategoryDerived( PerCategoryLambda action ) final;
    void ForEachCategoryDerived( PerCategoryConstLambda action ) const final;

    bool IsTransitoryPageDerived() const final { return false; }
};

#endif // UI_PAGE_DECK_ENABLED

#endif // STATIC_DATA_PAGE_H
