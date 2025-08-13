/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : LandingPageConfig.h
// PURPOSE : Configuration and defines for the UI Landing Page
//
// AUTHOR  : james.strain
// STARTED : October 2020
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef LANDING_PAGE_CONFIG_H
#define LANDING_PAGE_CONFIG_H

#include "fwutil/Gen9Settings.h"

#define UI_LANDING_PAGE_ENABLED ( 1 && ( RSG_PC || IS_GEN9_PLATFORM || GEN9_UI_SIMULATION_ENABLED ) )

#if UI_LANDING_PAGE_ENABLED
#define UI_LANDING_PAGE_ONLY(...)    __VA_ARGS__
#else
#define UI_LANDING_PAGE_ONLY(...)
#endif

#define GEN9_LANDING_PAGE_ENABLED	( 1 && UI_LANDING_PAGE_ENABLED && ( IS_GEN9_PLATFORM || GEN9_UI_SIMULATION_ENABLED ) )

#if GEN9_LANDING_PAGE_ENABLED
#define GEN9_LANDING_PAGE_ONLY(...)    __VA_ARGS__
#else
#define GEN9_LANDING_PAGE_ONLY(...)
#endif

#if GEN9_LANDING_PAGE_ENABLED
GEN9_UI_SIMULATION_ONLY(XPARAM(gen9BootFlow));

// Runtime check to allow querying gen9BootFlow commandline arg.
// ** Always use this instead of referencing PARAM_gen9BootFlow directly **
inline bool Gen9LandingPageEnabled()
{
#if IS_GEN9_PLATFORM
	return true;
#elif GEN9_UI_SIMULATION_ENABLED
	return PARAM_gen9BootFlow.Get();
#else // IS_GEN9_PLATFORM
	return false;
#endif // IS_GEN9_PLATFORM
}
#endif // GEN9_LANDING_PAGE_ENABLED

#if UI_LANDING_PAGE_ENABLED

namespace LandingPageConfig
{
    enum class eEntryPoint
    {
        ENTRY_FROM_BOOT,
        ENTRY_FROM_PAUSE,
		SINGLEPLAYER_MIGRATION,
		MULTIPLAYER_REVIEW_CHARACTERS,
		STORYMODE_UPSELL,

        ENTRY_COUNT
    };
}

#define BIG_FEED_WHATS_NEW_PAGE_CONTEXT ATSTRINGHASH( "OnWhatsNewPage", 0xF14081C8)
#define BIG_FEED_PRIORITY_FEED_CONTEXT ATSTRINGHASH( "OnPriorityFeedPage", 0x2DAD085D)

#endif // UI_LANDING_PAGE_ENABLED

#endif // LANDING_PAGE_CONFIG_H
