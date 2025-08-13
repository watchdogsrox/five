/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : PageTitle.cpp
// PURPOSE : Quick wrapper for the common featured title interface
//
// AUTHOR  : james.strain
// STARTED : March 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "PageTitle.h"
#if UI_PAGE_DECK_ENABLED

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "frontend/Scaleform/ScaleFormComplexObjectMgr.h"
#include "frontend/ui_channel.h"

void CPageTitle::Set( atHashString const label )
{
    char const * const c_labelLiteral = TheText.Get( label );
    SetLiteral( c_labelLiteral );
}

void CPageTitle::SetLiteral( char const * const label )
{
    CComplexObject& displayObject = GetDisplayObject();
    if( displayObject.IsActive() )
    {
        CScaleformMgr::BeginMethodOnComplexObject( displayObject.GetId(), SF_BASE_CLASS_GENERIC, "SET_TEXT" );
        CScaleformMgr::AddParamString( label, true );
        CScaleformMgr::EndMethod();
    }
}

#endif // UI_PAGE_DECK_ENABLED
