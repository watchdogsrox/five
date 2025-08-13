/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : SocialClubPageView.cpp
// PURPOSE : Page view that renders the GTAV Social Club menu.
//
// AUTHOR  : james.strain
// STARTED : July 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "SocialClubPageView.h"

#if GEN9_LANDING_PAGE_ENABLED

#include "SocialClubPageView_parser.h"

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/ui_channel.h"
#include "frontend/NewHud.h"
#include "frontend/page_deck/IPageViewHost.h"
#include "frontend/PauseMenu.h"
#include "frontend/SocialClubMenu.h"

FWUI_DEFINE_TYPE(CSocialClubPageView, 0x5EB7526D);

void CSocialClubPageView::RenderDerived() const
{
    SocialClubMenu::RenderWrapper();
}

#endif //  GEN9_LANDING_PAGE_ENABLED
