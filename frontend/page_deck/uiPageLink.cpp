/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : uiPageLink.cpp
// PURPOSE : Info of how a page is linked when transitioning
// 
// AUTHOR  : james.strain
// STARTED : December 2020
//
/////////////////////////////////////////////////////////////////////////////////
#include "uiPageLink.h"
#if UI_PAGE_DECK_ENABLED
#include "uiPageLink_parser.h"

FRONTEND_OPTIMISATIONS();

namespace
{
	const uiPageInfo nullPageInfo;
}

uiPageLink::uiPageLink( uiPageId const pageId )
{
	uiPageInfo targetPage( pageId );
	m_transitionSteps.PushAndGrow( targetPage );
}

uiPageInfo const& uiPageLink::GetTarget() const
{
	return IsValid() ? m_transitionSteps[m_transitionSteps.GetCount() - 1] : nullPageInfo;
}

bool uiPageLink::operator ==( const class uiPageLink& op ) const
{
	bool areEqual = true;

	int const c_transitionStepsCount = m_transitionSteps.GetCount();
	if( areEqual && c_transitionStepsCount != op.m_transitionSteps.GetCount() )
	{
		areEqual = false;
	}

	for( int i = 0; areEqual && i < c_transitionStepsCount; ++i )
	{
		if ( !( op.m_transitionSteps[i] == m_transitionSteps[i] ) )
		{
			areEqual = false;
		}
	}

	return areEqual;
}

#endif // UI_PAGE_DECK_ENABLED
