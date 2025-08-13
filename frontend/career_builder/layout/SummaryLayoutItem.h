/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : SummaryLayoutItem.h
// PURPOSE : Quick wrapper for the item used in the windfall shop summary page
//
// AUTHOR  : Charalampos.Koundourakis
// STARTED : June 2021
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef UI_SUMMARY_LAYOUT_ITEM_H
#define UI_SUMMARY_LAYOUT_ITEM_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// game
#include "frontend/page_deck/layout/PageLayoutItem.h"

class CSummaryLayoutItem final : public CPageLayoutItem
{
public:
	CSummaryLayoutItem() {}
	~CSummaryLayoutItem() {}

	void Set(atHashString const title, atHashString const description, char const * const price, bool isRichText);
	void SetLiteral(char const * const title, char const * const description, char const * const price, bool isRichText);
private: // declarations and variables
	NON_COPYABLE(CSummaryLayoutItem);

private: // methods
	char const * GetSymbol() const override { return "summaryItem"; }
};

#endif // UI_PAGE_DECK_ENABLED

#endif // UI_SUMMARY_LAYOUT_ITEM_H
