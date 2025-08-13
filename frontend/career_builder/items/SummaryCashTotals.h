/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : SummaryCashTotals.h
// PURPOSE : Quick wrapper for the cash total used in the windfall shop summary page
//
// AUTHOR  : Charalampos.Koundourakis
// STARTED : June 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef UI_SUMMARY_CASH_TOTALS_H
#define UI_SUMMARY_CASH_TOTALS_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// game
#include "frontend/page_deck/layout/PageLayoutItem.h"

class CSummaryCashTotals final : public CPageLayoutItem
{
public:
	CSummaryCashTotals()
	{}
	~CSummaryCashTotals() {}

	void Set(atHashString const title, atHashString const price, bool isRichText);
	void SetLiteral(char const * const title, char const * const price, bool isRichText);

private: // declarations and variables
	NON_COPYABLE(CSummaryCashTotals);

private: // methods
	char const * GetSymbol() const override { return "summaryCashTotals"; }
	
};

#endif // UI_PAGE_DECK_ENABLED

#endif // UI_SUMMARY_CASH_TOTALS_H
