/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : SummaryButton.h
// PURPOSE : Quick wrapper for the button used in the windfall shop summary page
//
// AUTHOR  : Charalampos.Koundourakis
// STARTED : June 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef UI_SUMMARY_BUTTON_H
#define UI_SUMMARY_BUTTON_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// game
#include "frontend/page_deck/layout/PageLayoutItem.h"

class CSummaryButton final : public CPageLayoutItem
{
public:
	CSummaryButton()
	{}
	~CSummaryButton() {}

	void Set(atHashString const label);
	void SetLiteral(char const * const label);

private: // declarations and variables
	NON_COPYABLE(CSummaryButton);

private: // methods
	char const * GetSymbol() const override { return "summaryButton"; }
};

#endif // UI_PAGE_DECK_ENABLED

#endif // UI_SUMMARY_BUTTON_H
