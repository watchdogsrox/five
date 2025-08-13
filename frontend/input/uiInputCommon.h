/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : uiInputCommon.h
// PURPOSE : Common types or data to share between the input system
//
// AUTHOR  : james.strain
// STARTED : September 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef UI_INPUT_COMMON_H
#define UI_INPUT_COMMON_H

// framework
#include "fwui/Input/fwuiInputCommon.h"

#if FW_UI_ENABLED

namespace uiInput
{
    void TriggerInputAudio( fwuiInput::eHandlerResult const actionResult, char const * const soundset,
        char const * const actionedTrigger, char const * const ignoredTrigger );
}

#endif // FW_UI_ENABLED

#endif // UI_INPUT_COMMON_H