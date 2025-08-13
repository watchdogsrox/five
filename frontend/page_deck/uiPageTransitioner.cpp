/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : uiPageTransitioner.cpp
// PURPOSE : Handles moving between CPageBases and updating a stack of
//           active parentage.
// 
// AUTHOR  : james.strain
// STARTED : December 2020
//
/////////////////////////////////////////////////////////////////////////////////
#include "uiPageTransitioner.h"

#if UI_PAGE_DECK_ENABLED

// game
#include "frontend/page_deck/PageBase.h"
#include "frontend/ui_channel.h"

FRONTEND_OPTIMISATIONS();

uiPageTransitioner::uiPageTransitioner()
    : m_pageStack()
    , m_transitionTimeout( 0 )
{

}

void uiPageTransitioner::AbandonTransition()
{
    if( HasTransition() )
    {
        CPageBase * const currentTarget = GetCurrentTransitionTarget();
        if( !IsStacked( currentTarget ) )
        {
            currentTarget->ExitPage( true );
        }
    }

    CleanupTransitionInfo();
}

void uiPageTransitioner::Shutdown()
{
    m_transitionCollection.ResetCount();
    AbandonTransition();
    UnstackAllPages();
}

bool uiPageTransitioner::PushTransition( uiPageLink const& pageLink, uiPageConfig::PageMap const& pageMap )
{
    bool result = false;

	// Retrieve list of pages in transition
	uiPageConfig::ConstPageCollection transitionSteps;
	uiPageConfig::PageInfoCollection const& c_transitionStepsInfo = pageLink.GetTransitionSteps();
	int const c_transitionStepsCount = c_transitionStepsInfo.GetCount();
	for( int i = 0; i < c_transitionStepsCount; ++i )
	{
		uiPageInfo const& c_pageInfo = c_transitionStepsInfo[i];
		uiPageId c_pageId = c_pageInfo.GetId();
		CPageBase* const * const nestedTransitionPageEntry = pageMap.Access( c_pageId );
		if( IsPageValidForTransition( nestedTransitionPageEntry, c_pageId ) )
		{
			transitionSteps.PushAndGrow( *nestedTransitionPageEntry );
		}
	}

	// Only add this transition if all pages were found and valid
	if ( transitionSteps.GetCount() == c_transitionStepsCount )
	{
		CPageBase * const sourcePage = GetSourceForTransition();
		m_transitionCollection.PushAndGrow( TransitionInfo( sourcePage, transitionSteps, pageLink ) );
	}

    result = HasTransition();

    return result;
}

bool uiPageTransitioner::TransitionToParent()
{
    bool result = false;

    CPageBase * const sourcePage = m_pageStack.GetCount() > 0 ? m_pageStack.Top() : nullptr;
    int const c_pageCount = m_pageStack.GetCount();
    if( c_pageCount > 1 )
    {
        CPageBase* parentPage = nullptr;
        int steps = 1;
        for( int index = c_pageCount - 2; parentPage == nullptr && index >= 0; --index )
        {
            CPageBase* const potentialParent = m_pageStack[index];
            if( !potentialParent->IsTransitoryPage() )
            {
                parentPage = potentialParent;
                break;
            }

            ++steps;
        }

        if( uiVerifyf( parentPage, "Unable to find suitable parent page for transition request" ) )
        {
			uiPageLink const c_parentPageLink(parentPage->GetId());
            m_transitionCollection.PushAndGrow( TransitionInfo( sourcePage, parentPage, c_parentPageLink, steps ) );
            result = true;
        }
    }

    return result;
}


bool uiPageTransitioner::StepTransition( float const deltaMs )
{
    CPageBase * currentTargetPage = GetCurrentTransitionTarget();

    // People visiting from the future, this function is a cutdown version of fwuiNodeTransitionHandler::StepTransition
    // from future projects. It was ratified in RDR development, but sliced down here to just the logic we need for this backport
    if( currentTargetPage == nullptr )
    {
        return false;
    }

    CPageBase * currentSourcePage = GetCurrentTransitionSource();
    bool const c_isBackwardTransition = IsTransitionToParent();

    m_transitionTimeout += deltaMs;

    const float c_sourceExitTimeout = currentSourcePage && !currentSourcePage->IsExitComplete() ? currentSourcePage->GetTransitionTimeout() : 0.0f;
    const float c_targetEnterTimeout = currentTargetPage && !currentTargetPage->IsEnteringComplete() ? currentTargetPage->GetTransitionTimeout() : 0.0f;
    const float c_timeoutLimit = rage::Max( c_sourceExitTimeout, c_targetEnterTimeout );

    bool const c_forceTransitionThisFrame = c_timeoutLimit > 0.0f && m_transitionTimeout >= c_timeoutLimit;
    bool const c_immediateLink = c_forceTransitionThisFrame || IsImmediateLink();

    ASSERT_ONLY( uiPageId const c_sourceID = currentSourcePage ? currentSourcePage->GetId() : uiPageId() );
    ASSERT_ONLY( uiPageId const c_targetID = currentTargetPage ? currentTargetPage->GetId() : uiPageId() );
    uiAssertf( !c_forceTransitionThisFrame, "Forcing transition this frame from '" HASHFMT "', to '" HASHFMT  "', after %.1fms timeout.", 
        HASHOUT( c_sourceID ), HASHOUT( c_targetID ), c_timeoutLimit );

    bool exitedThisFrame = c_forceTransitionThisFrame;
    bool enteredThisFrame = c_forceTransitionThisFrame;

    // We check m_currentTransitionTarget and m_currentTransitionSource as exit/update/enter/update occurs as the 
    // states may be re-entrant and interact with the flow, so we may have been cleaned up mid-transition

    if( currentSourcePage )
    {
        bool const c_wasExited = currentSourcePage->IsExitComplete();

        if( currentSourcePage->CanExit() || c_forceTransitionThisFrame )
        {
            if( currentSourcePage->IsFocused() )
            {
                currentSourcePage->LoseFocus();
            }

            if( c_isBackwardTransition )
            {
                currentSourcePage->ExitPage( c_immediateLink );
            }
        }

        if( currentSourcePage->IsExiting() )
        {
            currentSourcePage->Update( deltaMs );
        }

        exitedThisFrame = !currentSourcePage || !c_isBackwardTransition || 
            ( !c_wasExited && currentSourcePage->IsExitComplete() ) || c_forceTransitionThisFrame;
    }
    else
    {
        exitedThisFrame = true;
    }

    if( currentTargetPage && 
        ( !currentSourcePage || currentSourcePage->IsExitComplete() || !c_isBackwardTransition || c_forceTransitionThisFrame ) )
    {
        if( currentTargetPage->ShouldEnter() )
        {
            currentTargetPage->EnterPage( c_immediateLink );
        }

        if( currentTargetPage && ( currentTargetPage->IsEntering() || !c_isBackwardTransition ) )
        {
            currentTargetPage->Update( deltaMs );
        }

        if( currentTargetPage && ( currentTargetPage->IsEnteringComplete() || c_forceTransitionThisFrame ) )
        {
            if( !currentTargetPage->IsEnteringComplete() )
            {
                // If we didn't finish entering, give a final nudge to the state. This will help figuring out what state to transition next.
                // Note that this nudge is not important as important when OnExiting, since for that case we will have decided a transition target.
                currentTargetPage->OnEnterTimedOut();

            }

            UpdateTransitionTarget();
            enteredThisFrame = true;
        }
    }
    else
    {
        enteredThisFrame = true;

        // If we exited the source node this frame and there's no target, means we finished transitioning
        if( exitedThisFrame && !HasTransition() )
        {
            CleanupTransitionInfo();
        }
    }

    // We want externals to treat our transition as immediate if we entered and exited on the same frame,
    // or the link type says so
    bool const c_wasImmediateTransitionThisFrame = ( exitedThisFrame && enteredThisFrame ) || c_immediateLink;
    return c_wasImmediateTransitionThisFrame;
}

bool uiPageTransitioner::HasTransition() const
{
    return m_transitionCollection.size() > 0;
}

bool uiPageTransitioner::HasTransition( uiPageLink const& pageLink ) const
{
    bool found = false;

    int const c_count = m_transitionCollection.GetCount();
    for( int index = 0; found == false && index < c_count; ++index )
    {
        TransitionInfo const * const c_info = &m_transitionCollection[index];
        if( c_info->m_pageLink == pageLink )
        {
            found = true;
			break;
        }
    }

    return found;
}

bool uiPageTransitioner::CanBackOut() const
{
    bool result = false;

    auto func = [&result]( uiPageId const UNUSED_PARAM(id), CPageBase const& currentPage )
    {
        if( !result )
        {
            result = !currentPage.IsTransitoryPage();
        }
    };

    ForEachStackedPage_ExceptFocused( func );

    return result;
}

void uiPageTransitioner::ForEachStackedPage_ExceptFocused( uiPageConfig::PageVisitorLambda action )
{
    int const c_count = m_pageStack.GetCount();
    for( int index = 0; index < c_count - 1; ++index )
    {
        CPageBase * const currentPage = m_pageStack[index];
        action( currentPage->GetId(), *currentPage );
    }
}

void uiPageTransitioner::ForEachStackedPage_ExceptFocused( uiPageConfig::PageVisitorConstLambda action ) const
{
    int const c_count = m_pageStack.GetCount();
    for( int index = 0; index < c_count - 1; ++index )
    {
        CPageBase const * const c_currentPage = m_pageStack[index];
        action( c_currentPage->GetId(), *c_currentPage );
    }
}

void uiPageTransitioner::CleanupTransitionInfo()
{
    if( m_transitionCollection.GetCount() > 0 )
    {
        m_transitionCollection.Delete( 0 );
    }

    // Validate any remaining transitions can still be acted on
    for( int index = 0; index < m_transitionCollection.GetCount(); )
    {
        TransitionInfo const * const c_info = &m_transitionCollection[index];
        if( !IsStacked( c_info->m_transitionSource ))
        {
            m_transitionCollection.Delete( index );
        }
        else
        {
            ++index;
        }
    }

    m_transitionTimeout = 0;
}

CPageBase* uiPageTransitioner::GetSourceForTransition()
{
    // If we have no pages stacked, we've likely requested a transition during the enter
    // of our first page (before it's stacked)
    // so use the transition target instead
    CPageBase* result = m_pageStack.GetCount() > 0 ? m_pageStack.Top() : nullptr;
    if( result == nullptr )
    {
        result = GetCurrentTransitionTarget();
    }

    return result;
}

CPageBase const * uiPageTransitioner::GetCurrentTransitionSource() const
{
    CPageBase const * result = nullptr;

    if( m_transitionCollection.GetCount() > 0 )
    {
        TransitionInfo const& c_info = m_transitionCollection[0];
        if( c_info.IsTransitionToParent() )
        {
            result = m_pageStack.Top();
        }
        else
        {
            // If not going to parent, we just use it as is
            result = c_info.m_transitionSource;
        }
    }

    return result;
}

CPageBase const * uiPageTransitioner::GetCurrentTransitionTarget() const
{
    CPageBase const * result = nullptr;

    if( m_transitionCollection.GetCount() > 0 )
    {
        TransitionInfo const& c_info = m_transitionCollection[0];
        if( c_info.IsTransitionToParent() )
        {
            // We are after the _current_ target, not the _final_ target,
            // and since we pop nodes off the stack as we move up, a parent transition
            // will always be from lastItem to lastItem - 1
            int const c_count = m_pageStack.GetCount();
            result = c_count > 1 ? m_pageStack[c_count - 2] : nullptr;
        }
        else
        {
            // If not going to parent, follow nested transitions
			int const c_transitionStepsCount = c_info.m_transitionSteps.GetCount();
			if ( uiVerifyf( c_info.m_stepsRemaining <= c_transitionStepsCount, "Invalid number of steps in nested page transition: %d / %d", c_info.m_stepsRemaining, c_transitionStepsCount ) )
			{
				int const c_pageIndex = c_transitionStepsCount - c_info.m_stepsRemaining;
				result = c_info.m_transitionSteps[c_pageIndex];
			}
        }
    }

    return result;
}

bool uiPageTransitioner::IsImmediateLink() const
{
	bool const c_result = !m_transitionCollection.empty() ?
		m_transitionCollection[0].IsNestedTransition() : false;

	return c_result;
}

bool uiPageTransitioner::IsTransitionToParent() const
{
    bool const c_result = !m_transitionCollection.empty() ?
        m_transitionCollection[0].IsTransitionToParent() : false;

    return c_result;
}

int uiPageTransitioner::GetStackedIndex( CPageBase const * const page ) const
{
    int const c_index = m_pageStack.Find( const_cast<CPageBase*>(page) );
    return c_index;
}

bool uiPageTransitioner::IsOnTopOfStack( CPageBase const * const page ) const
{
    bool const c_top = !m_pageStack.empty() && m_pageStack.Top() == page;
    return c_top;
}

bool uiPageTransitioner::IsParent( CPageBase const * const potentialParent, CPageBase const * const potentialChild ) const
{
    int const c_parentIndex = m_pageStack.Find( const_cast<CPageBase*>(potentialParent) );
    int const c_childIndex = m_pageStack.Find( const_cast<CPageBase*>(potentialChild) );
    return c_parentIndex >= 0 && ( c_parentIndex == c_childIndex - 1);
}

void uiPageTransitioner::UpdateTransitionTarget()
{
    if( HasTransition() )
    {
        TransitionInfo& transitionInfo = m_transitionCollection[0];

        // Pull these first before we manipulate the stack
        CPageBase * const currentTransitionSource = GetCurrentTransitionSource();
        CPageBase * const currentTransitionTarget = GetCurrentTransitionTarget();

        bool const c_isForwardTransition = !transitionInfo.IsTransitionToParent();

        // If we just left the top of the stack, remove the item
        if( currentTransitionSource )
        {
            if( !c_isForwardTransition )
            {
                m_pageStack.Pop();
            }
        }

        if( currentTransitionTarget )
        {
            if( !IsStacked( currentTransitionTarget) )
            {
                m_pageStack.PushAndGrow( currentTransitionTarget );
            }

            bool const c_isFinalDestination = transitionInfo.IsFinalDestination();
            if( c_isFinalDestination )
            {
                currentTransitionTarget->GainFocus( c_isForwardTransition );
            }
        }

        if( transitionInfo.StepTransition() )
        {
            CleanupTransitionInfo();
        }
    }
}

void uiPageTransitioner::UnstackAllPages()
{
    while( m_pageStack.GetCount() )
    {
        CPageBase * currentPage = m_pageStack.Pop();
        if( currentPage->IsFocused() )
        {
            currentPage->LoseFocus();
        }

        if( currentPage->ShouldExit() )
        {
            currentPage->ExitPage( true );
        }
    }
}

bool uiPageTransitioner::IsPageValidForTransition(CPageBase* const * const page, uiPageId pageId)
{
#if !__ASSERT
	(void)pageId;
#endif
	bool const c_isValid = uiVerifyf( page != nullptr, "Requested page '" HASHFMT "' but it doesn't exist", HASHOUT( pageId ) )
		&& uiVerifyf( *page != nullptr, "Requested page '" HASHFMT "' but it is null", HASHOUT( pageId ) )
		&& uiVerifyf( !IsStacked( **page ), "Page '" HASHFMT "' is already active. We do not support instancing at this time.", HASHOUT( pageId ) );
	return c_isValid;
}

#endif // UI_PAGE_DECK_ENABLED
