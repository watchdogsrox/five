/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : uiPageInfo.h
// PURPOSE : Info of a page for use when transitioning
// 
// AUTHOR  : stephen.phillips
// STARTED : September 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef UI_PAGE_INFO_H
#define UI_PAGE_INFO_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// rage
#include "parser/macros.h"

class uiPageInfo final
{
public:
	uiPageInfo() { }
	explicit uiPageInfo( uiPageId const pageId );
	~uiPageInfo() { }

	uiPageId const& GetId() const { return m_pageId; }

	bool operator ==( const class uiPageInfo& op ) const;

private:
	uiPageId m_pageId;

	PAR_SIMPLE_PARSABLE;
};

#endif // UI_PAGE_DECK_ENABLED

#endif // UI_PAGE_INFO_H
