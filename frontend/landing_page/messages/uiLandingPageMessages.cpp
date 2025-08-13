/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : uiLandingPageMessages.cpp
// PURPOSE : Contains the core messages used by the landing page. We have
//           some generic messages in the page deck system, but these all
//           slant to functionality that does not genericise
// 
// AUTHOR  : james.strain
// STARTED : November 2020
//
/////////////////////////////////////////////////////////////////////////////////
#include "uiLandingPageMessages.h"

#if GEN9_LANDING_PAGE_ENABLED

#include "uiLandingPageMessages_parser.h"

FWUI_DEFINE_MESSAGE( uiLandingPage_GoToStoryMode, 0xEA08D1CE );
FWUI_DEFINE_MESSAGE( uiLandingPage_GoToOnlineMode, 0x321437B4 );
FWUI_DEFINE_MESSAGE( uiLandingPage_GoToEditorMode, 0xD340FE9D );
FWUI_DEFINE_MESSAGE( uiLandingPage_Dismiss, 0xAB84F144 );
FWUI_DEFINE_MESSAGE( uiRefreshStoryItemMsg, 0x8CC4423E );

void uiLandingPage_GoToOnlineMode::CleanupOnConsumed()
{
	m_scriptRouterArg.Clear();
}

#endif // GEN9_LANDING_PAGE_ENABLED
