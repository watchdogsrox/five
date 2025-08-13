/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CashBalance.h
// PURPOSE : Quick wrapper for the cash total used in the windfall shop summary page
//
// AUTHOR  : Charalampos.Koundourakis
// STARTED : July 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef UI_CASH_BALANCE_H
#define UI_CASH_BALANCE_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// game
#include "frontend/page_deck/layout/PageLayoutItem.h"

class CCashBalance final : public CPageLayoutItem
{
public:
	CCashBalance()
	{}
	~CCashBalance() {}

	void SetLiteral(char const * const cash, bool isRichText);

private: // declarations and variables
	NON_COPYABLE(CCashBalance);

private: // methods
	char const * GetSymbol() const override { return "cash"; }
};

#endif // UI_PAGE_DECK_ENABLED

#endif // UI_CASH_BALANCE_H
