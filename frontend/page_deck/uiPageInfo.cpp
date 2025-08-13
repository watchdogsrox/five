/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : uiPageInfo.cpp
// PURPOSE : Info of a page for use when transitioning
// 
// AUTHOR  : stephen.phillips
// STARTED : September 2021
//
/////////////////////////////////////////////////////////////////////////////////

#include "uiPageInfo.h"

#if UI_PAGE_DECK_ENABLED

#include "uiPageInfo_parser.h"

FRONTEND_OPTIMISATIONS();

uiPageInfo::uiPageInfo( uiPageId const pageId ) :
	m_pageId( pageId )
{
}

bool uiPageInfo::operator ==( const class uiPageInfo& op ) const
{
	// We currently only care about the page IDs matching
	const bool c_areEqual = m_pageId == op.m_pageId;
	return c_areEqual;
}

#endif // UI_PAGE_DECK_ENABLED
