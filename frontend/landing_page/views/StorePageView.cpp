/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : StorePageView.cpp
// PURPOSE : Page view that renders the store menu.
//
// AUTHOR  : james.strain
// STARTED : July 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "StorePageView.h"

#if GEN9_LANDING_PAGE_ENABLED

#include "StorePageView_parser.h"

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/ui_channel.h"
#include "frontend/NewHud.h"
#include "frontend/page_deck/IPageViewHost.h"
#include "frontend/PauseMenu.h"
#include "frontend/Store/StoreScreenMgr.h"

FWUI_DEFINE_TYPE(CStorePageView, 0x91A7F53D);

void CStorePageView::UpdateDerived(float const UNUSED_PARAM(deltaMs))
{
	// Special case - During initial loading of the store screen we may be stuck waiting on the asset load
	// So we need to handle backing out ourselves
	if( cStoreScreenMgr::IsLoadingAssets() )
	{
		UpdateInput();
	}
}

void CStorePageView::RenderDerived() const
{
    cStoreScreenMgr::Render();
}

#endif //  GEN9_LANDING_PAGE_ENABLED
