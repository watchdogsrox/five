/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CashBalance.cpp
// PURPOSE : Quick wrapper for the cash total shown during the career builder
//
// AUTHOR  : Charalampos.Koundourakis
// STARTED : July 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "CashBalance.h"
#if UI_PAGE_DECK_ENABLED

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "frontend/Scaleform/ScaleFormComplexObjectMgr.h"
#include "frontend/ui_channel.h"

void CCashBalance::SetLiteral(char const * const cash, bool isRichText)
{
	CComplexObject& displayObject = GetDisplayObject();
	if (displayObject.IsActive())
	{
		CScaleformMgr::BeginMethodOnComplexObject(displayObject.GetId(), SF_BASE_CLASS_GENERIC, "SET_TEXT");
		CScaleformMgr::AddParamString(cash, false);
		CScaleformMgr::AddParamBool(isRichText);
		CScaleformMgr::EndMethod();
	}
}

#endif // UI_PAGE_DECK_ENABLED
