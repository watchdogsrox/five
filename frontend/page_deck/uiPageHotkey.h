/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : uiPageHotkey.h
// PURPOSE : Container object for an input type, tooltip and action to perform.
//           Used as top-level interactions on a given page.
// 
// AUTHOR  : james.strain
// STARTED : March 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef UI_PAGE_HOTKEY_H
#define UI_PAGE_HOTKEY_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// rage
#include "atl/hashstring.h"
#include "parser/macros.h"

// game
#include "frontend/input/uiInputEnums.h"
#include "frontend/page_deck/uiPageConfig.h"
#include "system/control_mapping.h"

class uiPageDeckMessageBase;

class uiPageHotkey final
{
public:
    uiPageHotkey();
    ~uiPageHotkey();

    uiPageDeckMessageBase* const GetMessage() { return m_message; }
    uiPageDeckMessageBase const * const GetMessage() const { return m_message; }

    atHashString GetLabel() const { return m_label; }
    rage::InputType GetHotkeyInputType() const { return m_inputType; }
    eFRONTEND_INPUT GetFrontendInputType() const;

    bool IsValid() const;

private:
    uiPageDeckMessageBase * const   m_message;
    atHashString                    m_label;
    rage::InputType                 m_inputType;
    PAR_SIMPLE_PARSABLE;

private:
    static eFRONTEND_INPUT GetFrontendInputType( rage::InputType const inputType );
};

#endif // UI_PAGE_DECK_ENABLED

#endif // UI_PAGE_HOTKEY_H
