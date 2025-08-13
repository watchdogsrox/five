/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : SummaryButton.cpp
// PURPOSE : Quick wrapper for the cash total used in the windfall shop summary page
//
// AUTHOR  : Charalampos.Koundourakis
// STARTED : June 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "SummaryButton.h"
#if UI_PAGE_DECK_ENABLED

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "frontend/Scaleform/ScaleFormComplexObjectMgr.h"
#include "frontend/ui_channel.h"

void CSummaryButton::Set(atHashString const label)
{
	char const * const c_labelLiteral = TheText.Get(label);
	SetLiteral(c_labelLiteral);
}

void CSummaryButton::SetLiteral(char const * const label)
{
	CComplexObject& displayObject = GetDisplayObject();
	if (displayObject.IsActive())
	{
		CScaleformMgr::BeginMethodOnComplexObject(displayObject.GetId(), SF_BASE_CLASS_GENERIC, "SET_BUTTON_TEXT");
		CScaleformMgr::AddParamString(label, false);
		CScaleformMgr::EndMethod();
	}
}

#endif // UI_PAGE_DECK_ENABLED
