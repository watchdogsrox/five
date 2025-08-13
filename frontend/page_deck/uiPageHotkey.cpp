/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : uiPageHotkey.cpp
// PURPOSE : Container object for an input type, tooltip and action to perform.
//           Used as top-level interactions on a given page.
// 
// AUTHOR  : james.strain
// STARTED : March 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "uiPageHotkey.h"
#if UI_PAGE_DECK_ENABLED
#include "uiPageHotkey_parser.h"

#include "frontend/page_deck/messages/uiPageDeckMessages.h"

FRONTEND_OPTIMISATIONS();

uiPageHotkey::uiPageHotkey()
    : m_message( nullptr )
    , m_label()
    , m_inputType( UNDEFINED_INPUT )
{

}

uiPageHotkey::~uiPageHotkey()
{
    delete m_message;
}

eFRONTEND_INPUT uiPageHotkey::GetFrontendInputType() const
{
    return GetFrontendInputType( m_inputType );
}

// Blame the long departed for these kind of shenanigans
eFRONTEND_INPUT uiPageHotkey::GetFrontendInputType( rage::InputType const inputType )
{
    eFRONTEND_INPUT result = FRONTEND_INPUT_MAX;

    switch( inputType )
    {
    case rage::INPUT_FRONTEND_ACCEPT:
        {
            result = FRONTEND_INPUT_ACCEPT;
            break;
        }
    case rage::INPUT_FRONTEND_CANCEL:
        {
            result = FRONTEND_INPUT_BACK;
            break;
        }
    case rage::INPUT_FRONTEND_X:
        {
            result = FRONTEND_INPUT_X;
            break;
        }
    case rage::INPUT_FRONTEND_Y:
        {
            result = FRONTEND_INPUT_Y;
            break;
        }
    case rage::INPUT_FRONTEND_PAUSE:
        {
            result = FRONTEND_INPUT_START;
            break;
        }
    case rage::INPUT_FRONTEND_SELECT:
        {
            result = FRONTEND_INPUT_SELECT;
            break;
        }
	case rage::INPUT_FRONTEND_LS:
		{
			result = FRONTEND_INPUT_L3;
			break;
		}
	case rage::INPUT_FRONTEND_RS:
		{
			result = FRONTEND_INPUT_R3;
			break;
		}
    default:
        {
            // nop
            uiAssertf( false, "Unsupported hotkey %d specified", inputType );
        }
    }

    return result;
}

bool uiPageHotkey::IsValid() const
{
    return m_message != nullptr && GetFrontendInputType() != FRONTEND_INPUT_MAX;
}

#endif // UI_PAGE_DECK_ENABLED
