/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : PageTitle.h
// PURPOSE : Quick wrapper for the common featured title interface
//
// AUTHOR  : james.strain
// STARTED : March 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef UI_PAGE_TITLE_H
#define UI_PAGE_TITLE_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// game
#include "frontend/page_deck/layout/PageLayoutItem.h"

class CPageTitle final : public CPageLayoutItem
{
public:
    CPageTitle()
    {}
    ~CPageTitle() {}

    void Set( atHashString const label );
    void SetLiteral( char const * const label );

private: // declarations and variables
    NON_COPYABLE( CPageTitle );

private: // methods
    char const * GetSymbol() const override { return "title"; }
};

#endif // UI_PAGE_DECK_ENABLED

#endif // UI_PAGE_TITLE_H
