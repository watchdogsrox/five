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
#include "StaticDataPage.h"
#if UI_PAGE_DECK_ENABLED
#include "StaticDataPage_parser.h"

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/ui_channel.h"
#include "frontend/page_deck/PageItemBase.h"

FWUI_DEFINE_TYPE( CStaticDataPage, 0x5DF31CF1 );

CStaticDataPage::~CStaticDataPage()
{
    auto shutdownFunc = []( CPageItemCategoryBase& currentCategory )
    {
        delete &currentCategory;
    };
    ForEachCategory( shutdownFunc );
    m_categories.ResetCount();
}

void CStaticDataPage::ForEachCategoryDerived( PerCategoryLambda action )
{
    for( CategoryIterator itr = m_categories.begin(); itr != m_categories.end(); ++itr )
    {
        if( *itr )
        {
            action( **itr );
        }
    }
}

void CStaticDataPage::ForEachCategoryDerived( PerCategoryConstLambda action ) const 
{
    for( ConstCategoryIterator itr = m_categories.begin(); itr != m_categories.end(); ++itr )
    {
        if( *itr )
        {
            action( **itr );
        }
    }
}

#endif // UI_PAGE_DECK_ENABLED
