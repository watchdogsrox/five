/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : WarningScreenPage.cpp
// PURPOSE : Page that specifically hosts a legacy warning message.
//
// NOTE    : We are not reproducing the alert as a new view here, so this page
//           works directly with the old system. Hence it does not need a view.
//           Makes a bit odd that it's a viewless "screen", but that's how
//           it goes when you patch a 7 year old game :)
//
// AUTHOR  : james.strain
// STARTED : November 2020
//
/////////////////////////////////////////////////////////////////////////////////
#include "WarningScreenPage.h"
#if UI_PAGE_DECK_ENABLED
#include "WarningScreenPage_parser.h"

// game
#include "frontend/ui_channel.h"
#include "frontend/page_deck/messages/uiPageDeckMessages.h"
#include "frontend/page_deck/IPageMessageHandler.h"
#include "frontend/WarningScreen.h"

FWUI_DEFINE_TYPE( CWarningScreenPage, 0x3C3AAC26 );

void CWarningScreenPage::UpdateDerived( float const UNUSED_PARAM(deltaMs) )
{
    if( IsFocused() )
    {
        atHashString const c_title = GetTitle();
        if( c_title.IsNotNull() )
        {
            CWarningScreen::SetMessageWithHeader( WARNING_MESSAGE_STANDARD, c_title, m_description, (eWarningButtonFlags)m_buttonFlags.GetAllFlags() );
        }
        else
        {
            CWarningScreen::SetMessage( WARNING_MESSAGE_STANDARD, m_description, (eWarningButtonFlags)m_buttonFlags.GetAllFlags() );
        }

        eWarningButtonFlags const c_result = CWarningScreen::CheckAllInput();
        if( c_result != FE_WARNING_NONE )
        {
            // Because eWarningButtonFlags is not parsable, we can't use it as the map key. So
            // we need this contrived conversion back to the parsable flags (which are a bit index, rather than a bitmask value)
            eParsableWarningButtonFlags const c_parsableFlags = CWarningScreen::GetParsableFlagsFromFlag( c_result );

            uiPageDeckMessageBase* const * const resultAction = m_resultMap.Access( c_parsableFlags );
            if( uiVerifyf( resultAction && *resultAction, "Warning screen got result '%u' but we have no action mapped to it", (u32)c_parsableFlags ) )
            {
                IPageMessageHandler& messageHandler = GetMessageHandler();
                messageHandler.HandleMessage( **resultAction );
            }
        }
    }
}

void CWarningScreenPage::Shutdown()
{
    int const c_count = m_resultMap.GetNumSlots();
    for( int index = 0; index < c_count; ++index )
    {
        ResultMapEntry * const entry = m_resultMap.GetEntry( index );
        if( entry && entry->data )
        {
            delete entry->data;
        }
    }

    m_resultMap.Reset();
}

#endif // UI_PAGE_DECK_ENABLED