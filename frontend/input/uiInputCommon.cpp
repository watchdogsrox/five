/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : uiInputCommon.cpp
// PURPOSE : Common types or data to share between the input system
//
// AUTHOR  : james.strain
// STARTED : September 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "uiInputCommon.h"

#if FW_UI_ENABLED

// game
#include "audio/frontendaudioentity.h"

namespace uiInput
{

    void TriggerInputAudio( fwuiInput::eHandlerResult const actionResult, char const * const soundset, 
                           char const * const actionedTrigger, char const * const ignoredTrigger)
    {
        if( actionResult == fwuiInput::ACTION_HANDLED && actionedTrigger )
        {
            g_FrontendAudioEntity.PlaySound( actionedTrigger, soundset );
            g_FrontendAudioEntity.NotifyFrontendInput();
        }
        else if( actionResult == fwuiInput::ACTION_IGNORED && ignoredTrigger )
        {
            g_FrontendAudioEntity.PlaySound( ignoredTrigger, soundset );
            g_FrontendAudioEntity.NotifyFrontendInput();
        }
    }
}

#endif // FW_UI_ENABLED
