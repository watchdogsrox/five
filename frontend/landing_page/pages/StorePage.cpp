/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : StorePage.h
// PURPOSE : Bespoke page for bringing up the store menu
//
// AUTHOR  : james.strain
// STARTED : July 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "StorePage.h"

#if GEN9_LANDING_PAGE_ENABLED

#include "StorePage_parser.h"

// game
#include "frontend/page_deck/IPageMessageHandler.h"
#include "frontend/page_deck/messages/uiPageDeckMessages.h"
#include "frontend/Store/StoreScreenMgr.h"
#include "Frontend/Store/StoreMainScreen.h"
#include "frontend/ui_channel.h"

FWUI_DEFINE_TYPE(CStorePage, 0x28D5738A);

void CStorePage::OnEnterCompleteDerived()
{
	if (m_productId == NULL)
	{
		//if no product id has been supplied, open the in-game store
		cStoreScreenMgr::Open(false);
	}
	else
	{
		//if product id has been supplied, attempt to go straight to checkout
		cStoreScreenMgr::CheckoutCommerceProduct(m_productId, 0, false);
	}
}

void CStorePage::UpdateDerived( float const UNUSED_PARAM(deltaMs) )
{
    cStoreScreenMgr::Update();

	bool bShouldExit = (!cStoreScreenMgr::IsStoreMenuOpen() || cStoreScreenMgr::IsExitRequested());

	// don't exit here if we're performing a quick checkout through the platform store
	// (as the store menu returns as closed to progress to the platform store)
	cStoreMainScreen* pStoreMainScreen = cStoreScreenMgr::GetStoreMainScreen();
	if (pStoreMainScreen && pStoreMainScreen->IsInQuickCheckout())
	{
		bShouldExit = false;
	}

    if(bShouldExit)
    {
        uiPageDeckBackMessage backMsg;
        IPageMessageHandler& messageHandler = GetMessageHandler();
        messageHandler.HandleMessage( backMsg );
    }
}

void CStorePage::OnExitStartDerived()
{
	if (cStoreScreenMgr::IsStoreMenuOpen())
	{
		cStoreScreenMgr::RequestExit();
	}

	// Need to force an update to ensure it goes away
	CBusySpinner::Update();
}

bool CStorePage::OnExitUpdateDerived(float const UNUSED_PARAM(deltaMs))
{
    cStoreScreenMgr::Update();
    return !cStoreScreenMgr::IsStoreMenuOpen();
}

#endif // GEN9_LANDING_PAGE_ENABLED
