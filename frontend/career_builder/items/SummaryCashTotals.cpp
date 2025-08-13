/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : SummaryCashTotals.cpp
// PURPOSE : Quick wrapper for the cash total used in the windfall shop summary page
//
// AUTHOR  : Charalampos.Koundourakis
// STARTED : June 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "SummaryCashTotals.h"
#if UI_PAGE_DECK_ENABLED

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "frontend/Scaleform/ScaleFormComplexObjectMgr.h"
#include "frontend/ui_channel.h"

void CSummaryCashTotals::Set( atHashString const title, atHashString const price, bool isRichText)
{
	char const * const c_titleLiteral = TheText.Get(title);	
	char const * const c_priceLiteral = TheText.Get(price);

	SetLiteral(c_titleLiteral, c_priceLiteral, isRichText);
}

void CSummaryCashTotals::SetLiteral( char const * const title, char const * const price, bool isRichText)
{
    CComplexObject& displayObject = GetDisplayObject();
    if( displayObject.IsActive() )
    {
        CScaleformMgr::BeginMethodOnComplexObject( displayObject.GetId(), SF_BASE_CLASS_GENERIC, "SET_SUMMARY_CASH_TOTALS" );
        CScaleformMgr::AddParamString( title, false );
		CScaleformMgr::AddParamString( price, false);
		CScaleformMgr::AddParamBool(isRichText);
        CScaleformMgr::EndMethod();
    }
}

#endif // UI_PAGE_DECK_ENABLED
