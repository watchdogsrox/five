/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : Tooltip.cpp
// PURPOSE : Quick wrapper for the common tooltip interface
//
// AUTHOR  : james.strain
// STARTED : January 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "Tooltip.h"
#if UI_PAGE_DECK_ENABLED

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "frontend/Scaleform/ScaleFormComplexObjectMgr.h"
#include "frontend/ui_channel.h"

void CTooltip::SetString( atHashString const label, bool isRichText)
{
    char const * const c_literalText = TheText.Get( label );
    SetStringLiteral( c_literalText, isRichText);
}

void CTooltip::SetStringLiteral( char const * const textContent, bool isRichText)
{
    CComplexObject& displayObject = GetDisplayObject();
    if( displayObject.IsActive() )
    {
        CScaleformMgr::BeginMethodOnComplexObject( displayObject.GetId(), SF_BASE_CLASS_GENERIC, "SET_TEXT" );
        CScaleformMgr::AddParamString( textContent, false );
		CScaleformMgr::AddParamBool(isRichText);
        CScaleformMgr::EndMethod();
    }
}


#endif // UI_PAGE_DECK_ENABLED
