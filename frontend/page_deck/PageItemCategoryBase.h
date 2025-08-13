/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : PageItemCategoryBase.h
// PURPOSE : Extension of an item collection. Has meta-data about the collection
//           used for further display
//
// AUTHOR  : james.strain
// STARTED : January 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef C_PAGE_ITEM_CATEGORY_BASE_H
#define C_PAGE_ITEM_CATEGORY_BASE_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// rage
#include "atl/hashstring.h"
#include "parser/macros.h"

// game
#include "frontend/page_deck/IPageItemCollection.h"
#include "frontend/page_deck/layout/PageLayoutBase.h"

class CPageLayoutBase;

class CPageItemCategoryBase : public IPageItemCollection
{
public: // methods
    CPageItemCategoryBase();
    ~CPageItemCategoryBase();

    atHashString GetTitle() const { return m_title; }
    CPageLayoutBase* GetLayout() { return m_layout; }
    CPageLayoutBase const* GetLayout() const { return m_layout; }
    atHashString GetId() const { return m_id; }
    void SetTitle( atHashString const title) { m_title = title; }

    bool RefreshContent();

protected:
    void SetLayout(CPageLayoutBase*& layout);

    template< typename t_derivedType>
    t_derivedType* GetLayout() { return m_layout ? m_layout->AsClass<t_derivedType>() : nullptr; }

    template< typename t_derivedType>
    t_derivedType const* GetLayout() const { return m_layout ? m_layout->AsClass<t_derivedType>() : nullptr; }

    virtual bool RefreshContentDerived() { return false; }

    void DestroyLayout();

private:
    CPageLayoutBase*    m_layout;
    atHashString        m_title;
    atHashString        m_id;
    PAR_PARSABLE;
};

#endif // UI_PAGE_DECK_ENABLED

#endif // C_PAGE_ITEM_CATEGORY_BASE_H
