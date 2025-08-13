/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : SummaryLayoutItem.cpp
// PURPOSE : Quick wrapper for the item used in the windfall shop summary page
//
// AUTHOR  : Charalampos.Koundourakis
// STARTED : June 2021
//
/////////////////////////////////////////////////////////////////////////////////

#include "SummaryLayoutItem.h"

#if UI_PAGE_DECK_ENABLED

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "frontend/Scaleform/ScaleFormComplexObjectMgr.h"
#include "frontend/ui_channel.h"

void CSummaryLayoutItem::Set(atHashString const title, atHashString const description, char const * const price, bool isRichText)
{
	// Write these to separate buffers so they are not replaced by subsequent calls to TheText.Get if the text labels cannot be found
	const int c_displayLength = 64;
	char titleLiteral[c_displayLength];
	safecpy(titleLiteral, TheText.Get(title), c_displayLength);
	char descriptionLiteral[c_displayLength];
	safecpy(descriptionLiteral, TheText.Get(description), c_displayLength);

	SetLiteral(titleLiteral, descriptionLiteral, price, isRichText);
}

void CSummaryLayoutItem::SetLiteral(char const * const title, char const * const description, char const * const price, bool isRichText)
{
	CComplexObject& displayObject = GetDisplayObject();
	if (displayObject.IsActive())
	{
		CScaleformMgr::BeginMethodOnComplexObject(displayObject.GetId(), SF_BASE_CLASS_GENERIC, "SET_SUMMARY_ITEM_DETAILS");
		CScaleformMgr::AddParamString(title, false);
		CScaleformMgr::AddParamString(description, false);
		CScaleformMgr::AddParamString(price, false);
		CScaleformMgr::AddParamBool(isRichText);
		CScaleformMgr::EndMethod();
	}
}

#endif // UI_PAGE_DECK_ENABLED
