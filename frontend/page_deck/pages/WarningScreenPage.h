/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : WarningScreenPage.h
// PURPOSE : Page that specifically hosts a legacy warning message.
//
// NOTE    : We are not reproducing the alert as a new view here, so this page
//           works directly with the old system. Hence it does not need a view.
//           Makes a bit odd that it's a viewless "screen", but that's how
//           it goes when you patch a 7 year old game :)
//
// AUTHOR  : james.strain
// STARTED : November 2020
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef WARNING_SCREEN_PAGE_H
#define WARNING_SCREEN_PAGE_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// rage
#include "atl/bitset.h"
#include "atl/map.h"

// framework
#include "fwutil/flags.h"

// game
#include "frontend/WarningScreenEnums.h"
#include "frontend/page_deck/PageBase.h"

class uiPageDeckMessageBase;

class CWarningScreenPage : public CCategorylessPageBase
{
    FWUI_DECLARE_DERIVED_TYPE( CWarningScreenPage, CCategorylessPageBase );
public:
    CWarningScreenPage()
        : superclass()
    {

    }
    virtual ~CWarningScreenPage() { Shutdown(); }

private: // declarations and variables
    typedef rage::atMap< eParsableWarningButtonFlags, uiPageDeckMessageBase*> ResultMap;
    typedef ResultMap::Entry ResultMapEntry;

    ResultMap       m_resultMap;
    atHashString    m_description;
    fwFlags32       m_buttonFlags;
    PAR_PARSABLE;
    NON_COPYABLE( CWarningScreenPage );

private: // methods
    virtual void UpdateDerived( float const deltaMs );

    void Shutdown();

    bool IsTransitoryPageDerived() const final { return true; }
};

#endif // UI_PAGE_DECK_ENABLED

#endif // WARNING_SCREEN_PAGE_H
