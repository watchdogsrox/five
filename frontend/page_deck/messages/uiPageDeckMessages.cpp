/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : uiPageDeckMessages.cpp
// PURPOSE : Contains the core messages used by the page deck system. Primary
//           intent is for view > controller communications
//
// NOTES   : If you have a specialized enough set of messages, don't dump them here.
//           Be a responsible developer and create a new h/cpp/psc in the correct domain
// 
// AUTHOR  : james.strain
// STARTED : November 2020
//
/////////////////////////////////////////////////////////////////////////////////
#include "uiPageDeckMessages.h"

#if UI_PAGE_DECK_ENABLED

#include "uiPageDeckMessages_parser.h"

FWUI_DEFINE_MESSAGE( uiPageDeckMessageBase, 0x45E674A4 );
FWUI_DEFINE_MESSAGE( uiGoToPageMessage, 0x6A1D7748 );
FWUI_DEFINE_MESSAGE( uiPageDeckAcceptMessage, 0xC2C12DEC );
FWUI_DEFINE_MESSAGE( uiPageDeckCanBackOutMessage, 0x318AA81B );
FWUI_DEFINE_MESSAGE( uiPageDeckBackMessage, 0x4DC9F81E );

uiPageDeckAcceptMessage::uiPageDeckAcceptMessage()
    : superclass( uiPageLink( uiPageId( ATSTRINGHASH( "next", 0x4BCD2941 ) ) ) )
{

}


uiPageDeckBackMessage::uiPageDeckBackMessage()
    : superclass( uiPageLink( uiPageConfig::GetControlCharacter_UpOneLevel() ) )
{

}

#endif // UI_PAGE_DECK_ENABLED
