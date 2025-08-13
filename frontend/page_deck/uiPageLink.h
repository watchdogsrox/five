/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : uiPageLink.h
// PURPOSE : Info of how a page is linked when transitioning
// 
// AUTHOR  : james.strain
// STARTED : December 2020
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef UI_PAGE_LINK_H
#define UI_PAGE_LINK_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// framework
#include "atl/array.h"

// rage
#include "parser/macros.h"

// game
#include "frontend/page_deck/uiPageInfo.h"

class CPageBase;

class uiPageLink final
{
public:
    uiPageLink() { }
    explicit uiPageLink( uiPageId const pageId );
    ~uiPageLink() { }

	bool IsValid() const { return !m_transitionSteps.empty(); }
	uiPageInfo const& GetTarget() const;
	uiPageConfig::PageInfoCollection const& GetTransitionSteps() const { return m_transitionSteps; }

	bool operator ==( const class uiPageLink& op ) const;

private:
	// TODO_UI: Consider merging fwuiHashPath support from RDR to traverse a tree of page hashes
	uiPageConfig::PageInfoCollection m_transitionSteps;

    PAR_SIMPLE_PARSABLE;

private:
};

#endif // UI_PAGE_DECK_ENABLED

#endif // UI_PAGE_LINK_H
