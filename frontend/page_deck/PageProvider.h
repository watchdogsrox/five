/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : PageProvider.h
// PURPOSE : Class that wraps a collection of pages and runs book-keeping logic
//           for them.
//
// NOTES   : Rename PageManager? It does more than just "provide" them...
//
// AUTHOR  : james.strain
// STARTED : October 2020
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef PAGE_PROVIDER_H
#define PAGE_PROVIDER_H

#include "frontend/page_deck/uiPageConfig.h"

#if UI_PAGE_DECK_ENABLED

// rage
#include "atl/array.h"
#include "atl/hashstring.h"
#include "atl/map.h"
#include "parser/macros.h"

// game
#include "frontend/page_deck/uiEntrypointId.h"
#include "frontend/page_deck/uiPageTransitioner.h"


class CPageBase;
class IPageViewHost;

class CPageProvider /* final */
{
public:
    CPageProvider();
    ~CPageProvider() { Shutdown(); }

    void PostLoadInitialize( IPageViewHost& viewHost );
    void Shutdown();

    void LaunchToPage( uiEntrypointId const entrypointId );
    bool IsRunning() const;
    void Update( float const deltaMs );
    void Dismiss();

    void RequestTransition( uiPageLink const& pageLink );
    bool CanBackOut() const;

private: // declarations and variables
    typedef rage::atMap< uiEntrypointId, uiPageLink>  EntrypointMap;
    typedef uiPageConfig::PageMap::Entry PageMapEntry;

    EntrypointMap                       m_entrypointMap;
	uiPageConfig::PageMap               m_pageMap;
    uiPageTransitioner                  m_transitioner;
    uiPageLink                          m_defaultEntry;
    PAR_SIMPLE_PARSABLE;

private: // methods
    void UnloadInternal();

    void Update_Active( float const deltaMs );
    void Update_Focused( float const deltaMs );

    void ForEachPage( uiPageConfig::PageVisitorLambda action );
    void ForEachPage( uiPageConfig::PageVisitorConstLambda action ) const;

    CPageBase * GetFocusedPage()
    {
        CPageProvider const * const c_constThis = const_cast<CPageProvider const*>(this);
        return const_cast<CPageBase*>( c_constThis->GetFocusedPage() );
    }
    CPageBase const * GetFocusedPage() const;

    bool IsActive( CPageBase const& targetPage ) const;
    bool IsFocused( CPageBase const& targetPage ) const;

    void InitializationLambdaHelper( uiPageId const pageId, CPageBase& currentPage, IPageViewHost& viewHost ) const;

    uiPageLink const& GetLinkFromEntrypoint( uiEntrypointId const entrypointId ) const;
    void GoToPage( uiPageLink const& targetPageLink );
};

#endif // UI_PAGE_DECK_ENABLED

#endif // PAGE_PROVIDER_H
