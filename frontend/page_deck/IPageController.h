/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : IPageController.h
// PURPOSE : Interface to the controller, primarily for message sending
//           so the controller can action any results
//
// AUTHOR  : james.strain
// STARTED : October 2020
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef I_PAGE_CONTROLLER_H
#define I_PAGE_CONTROLLER_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// rage
#include "atl/hashstring.h"

// game
#include "frontend/page_deck/IPageMessageHandler.h"

class IPageController : public IPageMessageHandler
{
    FWUI_DECLARE_INTERFACE( IPageController );
public:
    
};

FWUI_DEFINE_INTERFACE( IPageController );

#endif // UI_PAGE_DECK_ENABLED

#endif // I_PAGE_CONTROLLER_H
