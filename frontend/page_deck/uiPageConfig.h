/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : uiPageConfig.h
// PURPOSE : Config header for the page-deck system. Contains some common defines
//           and helper functions.
//
// AUTHOR  : james.strain
// STARTED : January 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef UI_PAGE_CONFIG_H
#define UI_PAGE_CONFIG_H

// framework
#include "fwutil/Gen9Settings.h"

#define UI_PAGE_DECK_ENABLED	( 1 && ( IS_GEN9_PLATFORM || GEN9_UI_SIMULATION_ENABLED ) )

#if UI_PAGE_DECK_ENABLED
#define UI_PAGE_DECK_ENABLED_ONLY(...)    __VA_ARGS__
#else // UI_PAGE_DECK_ENABLED
#define UI_PAGE_DECK_ENABLED_ONLY(...)
#endif // UI_PAGE_DECK_ENABLED

#if UI_PAGE_DECK_ENABLED

// rage
#include "atl/array.h"
#include "atl/hashstring.h"
#include "system/lambda.h"
#include "vectormath/vec2f.h"

// game
#include "frontend/page_deck/uiPageId.h"
#include "frontend/page_deck/uiPageInfo.h"
#include "frontend/scaleform/scaleformmgr.h"

class CPageBase;

#define UI_AUDIO_TRIGGER_NAV_LEFT_RIGHT "NAV_LEFT_RIGHT"
#define UI_AUDIO_TRIGGER_NAV_UP_DOWN "NAV_UP_DOWN"
#define UI_AUDIO_TRIGGER_SELECT "SELECT"
#define UI_AUDIO_TRIGGER_ERROR "ERROR"

namespace uiPageConfig
{
	typedef rage::atArray<uiPageInfo>			PageInfoCollection;
    typedef rage::atArray<CPageBase*>			PageCollection;
    typedef PageCollection::iterator			PageCollectionIterator;
    typedef PageCollection::const_iterator		ConstPageCollectionIterator;
	typedef rage::atArray<CPageBase const*>		ConstPageCollection;
	typedef rage::atMap< uiPageId, CPageBase*>	PageMap;
    typedef LambdaRef< void( uiPageId const, CPageBase& )>       PageVisitorLambda;
    typedef LambdaRef< void( uiPageId const, CPageBase const& )> PageVisitorConstLambda;

    inline uiPageId GetControlCharacter_UpOneLevel() { return uiPageId( ATSTRINGHASH( "^", 0xB51A6A1A ) ); }

    rage::Vec2f GetUIAreaDip();
    rage::Vec2f GetScreenArea();
    rage::Vec2f GetPhysicalArea();
    GFxMovieView::ScaleModeType GetScaleModeType();

    Vec2f GetDipScale();

    char const * GetLabel( atHashString const defaultLabel, atHashString const overrideLabel );
}
#endif // UI_PAGE_DECK_ENABLED

#endif // UI_PAGE_CONFIG_H
