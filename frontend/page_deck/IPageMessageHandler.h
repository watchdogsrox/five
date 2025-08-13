/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : IPageMessageHandler.h
// PURPOSE : Interface for classes that handle page messages. 
//
// AUTHOR  : james.strain
// STARTED : October 2020
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef I_PAGE_MESSAGE_HANDLER_H
#define I_PAGE_MESSAGE_HANDLER_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// framework
#include "fwui/Interfaces/Interface.h"

class uiPageDeckMessageBase;

class IPageMessageHandler
{
    FWUI_DECLARE_INTERFACE( IPageMessageHandler );
public:
    virtual bool HandleMessage( uiPageDeckMessageBase& msg ) = 0;
};

FWUI_DEFINE_INTERFACE( IPageMessageHandler );

#endif // UI_PAGE_DECK_ENABLED

#endif // I_PAGE_MESSAGE_HANDLER_H
