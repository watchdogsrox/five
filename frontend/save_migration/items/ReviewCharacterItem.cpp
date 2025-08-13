/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : ReviewCharacterItem.cpp
// PURPOSE : Quick wrapper for the review character element 
//
// AUTHOR  : Charalampos.Koundourakis
// STARTED : August 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "ReviewCharacterItem.h"
#if UI_PAGE_DECK_ENABLED

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "frontend/Scaleform/ScaleFormComplexObjectMgr.h"


void CReviewCharacterItem::ShowAsLoading(atHashString titleHash)
{
	char const * const c_titleLiteral = TheText.Get(titleHash);
	SetDetailsLiteral(c_titleLiteral, "Fetching your data", "");
}

void CReviewCharacterItem::SetDetails(atHashString titleHash, atHashString descriptionHash, atHashString strapLineHash)
{
	char const * const c_titleLiteral = TheText.Get(titleHash);
	char const * const c_descriptionLiteral = TheText.Get(descriptionHash);
	char const * const c_strapLineLiteral = TheText.Get(strapLineHash);
	SetDetailsLiteral(c_titleLiteral, c_descriptionLiteral, c_strapLineLiteral);
}

void CReviewCharacterItem::SetDetailsLiteral(char const * const title, char const * const description, char const * const strapLine)
{
	CComplexObject& displayObject = GetDisplayObject();
	if (displayObject.IsActive())
	{
		CScaleformMgr::BeginMethodOnComplexObject(displayObject.GetId(), SF_BASE_CLASS_GENERIC, "SET_ITEM_DETAILS");
		CScaleformMgr::AddParamString(title, false);
		CScaleformMgr::AddParamString(description, false);
		CScaleformMgr::AddParamString(strapLine, false);
		CScaleformMgr::AddParamString("FRONTEND_LANDING_BASE", false);	// Tag TXD
		CScaleformMgr::AddParamString("MIGRATION_ONLINE_LOGO", false);	// Tag Texture
		CScaleformMgr::AddParamBool(true); // Is Rich text
		CScaleformMgr::EndMethod();
	}
}

void CReviewCharacterItem::SetProfileInfo(int profileNumber, char const * const profileName, char const * const rpEarned, char const * const headshotTexture)
{
	CComplexObject& displayObject = GetDisplayObject();
	if (displayObject.IsActive())
	{
		CScaleformMgr::BeginMethodOnComplexObject(displayObject.GetId(), SF_BASE_CLASS_GENERIC, "SET_PROFILE_INFO");
		CScaleformMgr::AddParamInt(profileNumber);
		CScaleformMgr::AddParamString(profileName, false);
		CScaleformMgr::AddParamString(rpEarned, false);
		CScaleformMgr::AddParamString(headshotTexture, false);	
		CScaleformMgr::EndMethod();
	}
}

void CReviewCharacterItem::SetState(int number, int state)
{
	CComplexObject& displayObject = GetDisplayObject();
	if (displayObject.IsActive())
	{
		CScaleformMgr::BeginMethodOnComplexObject(displayObject.GetId(), SF_BASE_CLASS_GENERIC, "SET_STATE");
		CScaleformMgr::AddParamInt(number);
		CScaleformMgr::AddParamInt(state);	
		CScaleformMgr::EndMethod();
	}
}

#endif // UI_PAGE_DECK_ENABLED


