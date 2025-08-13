/////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef INC_LOADINGSCREEN_CONTEXT_H_
#define INC_LOADINGSCREEN_CONTEXT_H_

// --- Enums ----------------------------------------------------------------
enum LoadingScreenContext
{
	LOADINGSCREEN_CONTEXT_NONE,
	LOADINGSCREEN_CONTEXT_INTRO_MOVIE,				// renders the bink intro movie
	
	LOADINGSCREEN_CONTEXT_LEGALSPLASH,				// Legal non-scaleform splash screen. ( Startup context, can be passed in ::Init() )

    LOADINGSCREEN_CONTEXT_LEGALMAIN,				// Legal scaleform screen ( Startup context, can be passed in ::Init() )
    LOADINGSCREEN_CONTEXT_SWAP_UNUSED,				// Gen7 Legacy - Waiting for the disc swap. Exists just to keep hardcoded script values in sync with this enum
	LOADINGSCREEN_CONTEXT_LANDING,				    // PC & Gen9 Landing
	LOADINGSCREEN_CONTEXT_LOADGAME,					// Game loading scaleform screen ( Selected from within CLoadingScreens only)
	LOADINGSCREEN_CONTEXT_INSTALL,					// Game installing scaleform screen ( Selected from within CLoadingScreens only)

	LOADINGSCREEN_CONTEXT_LOADLEVEL,				// Level loading scaleform screen ( Startup context, can be passed in ::Init() )
	LOADINGSCREEN_CONTEXT_MAPCHANGE,				// Load screen when loading from SP to MP and vice versa through CExtraContentManager
	LOADINGSCREEN_CONTEXT_LAST_FRAME // Use the last rendered frame as a background with a spinner as a loading screen
};

#endif // !INC_LOADINGSCREEN_CONTEXT_H_
