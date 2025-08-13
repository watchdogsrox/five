/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : Tooltip.h
// PURPOSE : Quick wrapper for the common tooltip interface
//
// AUTHOR  : james.strain
// STARTED : January 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef UI_TOOLTIP_H
#define UI_TOOLTIP_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// rage 
#include "atl/array.h"

// game
#include "frontend/page_deck/layout/PageLayoutItem.h"

class CTooltip final : public CPageLayoutItem
{
public:
    CTooltip()
    {}
    ~CTooltip() {}

	void SetString(atHashString const label, bool isRichText);
	void SetStringLiteral(char const * const textContent, bool isRichText);

private: // declarations and variables
    NON_COPYABLE( CTooltip );

private: // methods
    char const * GetSymbol() const final { return "toolTip"; }	
};

#endif // UI_PAGE_DECK_ENABLED

#endif // UI_TOOLTIP_H
