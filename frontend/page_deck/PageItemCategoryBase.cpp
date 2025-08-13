/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : PageItemCategoryBase.cpp
// PURPOSE : Extension of an item collection. Has meta-data about the collection
//           used for further display
//
// AUTHOR  : james.strain
// STARTED : January 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "PageItemCategoryBase.h"

#if UI_PAGE_DECK_ENABLED
#include "PageItemCategoryBase_parser.h"

CPageItemCategoryBase::CPageItemCategoryBase() 
    : m_layout(nullptr)
{
}

CPageItemCategoryBase::~CPageItemCategoryBase()
{
    DestroyLayout();
}

bool CPageItemCategoryBase::RefreshContent()
{
    return RefreshContentDerived();
}

void CPageItemCategoryBase::SetLayout(CPageLayoutBase*& layout)
{
    DestroyLayout();
    m_layout = layout;
    layout = nullptr;
}

void CPageItemCategoryBase::DestroyLayout()
{
    if (m_layout)
    {
        delete m_layout;
        m_layout = nullptr;
    }
}

#endif // UI_PAGE_DECK_ENABLED
