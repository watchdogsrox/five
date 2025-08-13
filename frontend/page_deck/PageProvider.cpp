/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : PageProvider.cpp
// PURPOSE : Class that wraps a collection of pages and runs book-keeping logic
//           for them.
//
// AUTHOR  : james.strain
// STARTED : October 2020
//
/////////////////////////////////////////////////////////////////////////////////
#include "PageProvider.h"
#if UI_PAGE_DECK_ENABLED
#include "PageProvider_parser.h"

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/ui_channel.h"
#include "frontend/page_deck/PageBase.h"

CPageProvider::CPageProvider()
    : m_transitioner()
{
}

void CPageProvider::PostLoadInitialize( IPageViewHost& viewHost )
{
    auto validationFunc = [this, &viewHost]( uiPageId const id, CPageBase& currentPage )
    {
        InitializationLambdaHelper( id, currentPage, viewHost );
    };

    ForEachPage( validationFunc );
}

void CPageProvider::Shutdown()
{
    Dismiss();
    UnloadInternal();
}

void CPageProvider::LaunchToPage( uiEntrypointId const entrypointId )
{
    if( uiVerifyf( !IsRunning(), "Attempting to launch to '" HASHFMT "' but we are already running", HASHOUT(entrypointId) ) )
    {
        uiPageLink const& startingPageLink = GetLinkFromEntrypoint( entrypointId );
		if( uiVerifyf( startingPageLink.IsValid(), "Unable to find requested starting page '" HASHFMT "'", HASHOUT(entrypointId) ) )
        {
            GoToPage( startingPageLink );
        }
    }
}

bool CPageProvider::IsRunning() const
{
    return m_transitioner.HasStackedPages() || m_transitioner.HasTransition();
}

void CPageProvider::Update( float const deltaMs )
{
    // We update in order, active but un-focused...
    Update_Active( deltaMs );

    // ...then focused
    Update_Focused( deltaMs );

    // Step the transition, if we have one
    bool stepTransitions = m_transitioner.HasTransition();
    while( stepTransitions )
    {
        bool const c_wasInstantTransition = m_transitioner.StepTransition( deltaMs );
        stepTransitions = c_wasInstantTransition && m_transitioner.HasTransition();
    }
}

void CPageProvider::Dismiss()
{
    if( IsRunning() )
    {
        m_transitioner.Shutdown();
    }
}

void CPageProvider::RequestTransition( uiPageLink const& pageLink )
{
    if( IsRunning() )
    {
        GoToPage( pageLink );
    }
}

bool CPageProvider::CanBackOut() const
{
    bool const c_result = m_transitioner.CanBackOut();
    return c_result;
}

void CPageProvider::UnloadInternal()
{
    m_transitioner.Shutdown();

    auto shutdownFunc = []( uiPageId const UNUSED_PARAM(id), CPageBase& currentPage )
    {
        currentPage.Shutdown();
        delete &currentPage;
    };

    ForEachPage( shutdownFunc );
    m_pageMap.Reset();
    m_entrypointMap.Reset();
}

void CPageProvider::Update_Active( float const deltaMs )
{
    auto updateFunc = [deltaMs]( uiPageId const UNUSED_PARAM(id), CPageBase& currentPage )
    {
        currentPage.Update( deltaMs );
    };

    m_transitioner.ForEachStackedPage_ExceptFocused( updateFunc );
}

void CPageProvider::Update_Focused( float const deltaMs )
{
    CPageBase * const focusedPage = GetFocusedPage();
    if( focusedPage )
    {
        focusedPage->Update( deltaMs );
    }
}

void CPageProvider::ForEachPage( uiPageConfig::PageVisitorLambda action )
{
    int const c_count = m_pageMap.GetNumSlots();
    for( int index = 0; index < c_count; ++index )
    {
        PageMapEntry * entry = m_pageMap.GetEntry( index );
        while( entry )
        {
            uiAssertf( entry->key.IsNotNull(), "Page entry with null key found" );

            if( uiVerifyf( entry->data, "Null page type for entry '" HASHFMT "', your code and data is likely out of sync", HASHOUT(entry->key) ) )
            {
                action( entry->key, *(entry->data) );
            }

            entry = entry->next;
        }
    }
}

void CPageProvider::ForEachPage( uiPageConfig::PageVisitorConstLambda action ) const
{
    int const c_count = m_pageMap.GetNumSlots();
    for( int index = 0; index < c_count; ++index )
    {
        PageMapEntry const * entry = m_pageMap.GetEntry( index );
        while( entry )
        {
            uiAssertf( entry->key.IsNotNull(), "Page entry with null key found" );

            if( uiVerifyf( entry->data, "Null page type for entry '" HASHFMT "', your code and data is likely out of sync", HASHOUT(entry->key) ) )
            {
                action( entry->key, *(entry->data) );
            }

            entry = entry->next;
        }
    }
}

CPageBase const * CPageProvider::GetFocusedPage() const
{
    CPageBase const * const c_result = m_transitioner.GetFocusedPage();
    return c_result;
}

bool CPageProvider::IsActive( CPageBase const& targetPage ) const
{
    bool const c_active = m_transitioner.IsStacked( targetPage );
    return c_active;
}

bool CPageProvider::IsFocused( CPageBase const& targetPage ) const
{
    CPageBase const * const c_focused = GetFocusedPage();
    return &targetPage == c_focused;
}

void CPageProvider::InitializationLambdaHelper( uiPageId const pageId, CPageBase& currentPage, IPageViewHost& viewHost ) const
{
    // Initialize the page
    currentPage.Initialize( pageId, viewHost );
}

uiPageLink const& CPageProvider::GetLinkFromEntrypoint( uiEntrypointId const entrypointId ) const
{
    // Default to the default
    uiPageLink const * result = &m_defaultEntry;

    if( entrypointId.IsNotNull() )
    {
        // If we have an entry point, utilize that
        uiPageLink const * const targetEntryPoint = m_entrypointMap.Access( entrypointId );
        if( uiVerifyf( targetEntryPoint != nullptr, "Requested entrypoint '" HASHFMT "' but it doesn't exist", HASHOUT( entrypointId ) ) )
        {
            result = targetEntryPoint;
        }
    }

    return *result;
}

void CPageProvider::GoToPage( uiPageLink const& targetPageLink )
{
	if ( targetPageLink.IsValid() )
	{
		uiPageInfo const& c_targetPage = targetPageLink.GetTarget();

		if (c_targetPage.GetId() == uiPageConfig::GetControlCharacter_UpOneLevel())
		{
			m_transitioner.TransitionToParent();
		}
		else
		{
			m_transitioner.PushTransition(targetPageLink, m_pageMap);
		}
	}
}

#endif // UI_PAGE_DECK_ENABLED
