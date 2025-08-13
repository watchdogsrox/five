/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : MigrateProfileItem.cpp
// PURPOSE : PageLayoutItem wrapper for the migrate profile scaleform element 
//
// AUTHOR  : Charalampos.Koundourakis
// STARTED : August 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "MigrateProfileItem.h"
#if UI_PAGE_DECK_ENABLED

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "frontend/Scaleform/ScaleFormComplexObjectMgr.h"

void CMigrateProfileItem::ShowAsLoading(atHashString titleHash)
{
	char const * const c_titleLiteral = TheText.Get(titleHash);
	SetDetailsLiteral(c_titleLiteral, "Fetching your data");
}

void CMigrateProfileItem::SetDetails(atHashString titleHash, atHashString descriptionHash)
{
	char const * const c_titleLiteral = TheText.Get(titleHash);
	char const * const c_descriptionLiteral = TheText.Get(descriptionHash);
	SetDetailsLiteral(c_titleLiteral, c_descriptionLiteral);
}

void CMigrateProfileItem::SetDetailsLiteral(char const * const title, char const * const description)
{
	CComplexObject& displayObject = GetDisplayObject();
	if (displayObject.IsActive())
	{
		CScaleformMgr::BeginMethodOnComplexObject(displayObject.GetId(), SF_BASE_CLASS_GENERIC, "SET_ITEM_DETAILS");
		CScaleformMgr::AddParamString(title, false);
		CScaleformMgr::AddParamString(description, false);
		CScaleformMgr::AddParamString("FRONTEND_LANDING_BASE", false);	// Tag TXD
		CScaleformMgr::AddParamString("MIGRATION_ONLINE_LOGO", false);	// Tag Texture
		CScaleformMgr::AddParamBool(true); // Is Rich text
		CScaleformMgr::EndMethod();
	}
}

void CMigrateProfileItem::SetItemInfo(char const * const lastPlayedValue, char const * const missionsValue, char const * const bankedValue, char const * const fundsValue)
{
	CComplexObject& displayObject = GetDisplayObject();
	if (displayObject.IsActive())
	{
		CScaleformMgr::BeginMethodOnComplexObject(displayObject.GetId(), SF_BASE_CLASS_GENERIC, "SET_ITEM_INFO");
		CScaleformMgr::AddParamString(TheText.Get("UI_MIGRATE_MP_RECENT"), true);
		CScaleformMgr::AddParamString(lastPlayedValue, false);
		CScaleformMgr::AddParamString(TheText.Get("UI_MIGRATE_MP_UGCPUB"), true);
		CScaleformMgr::AddParamString(missionsValue, false);
		CScaleformMgr::AddParamString(TheText.Get("UI_MIGRATE_MP_BANK"), true);
		CScaleformMgr::AddParamString(bankedValue, false);
		CScaleformMgr::AddParamString(TheText.Get("UI_MIGRATE_MP_FUNDS"), true);
		CScaleformMgr::AddParamString(fundsValue, false);
		CScaleformMgr::EndMethod();
	}
}

void CMigrateProfileItem::SetProfileInfo(int profileID, char const * const profileName, char const * const rpEarned, char const * const headshotTexture, int rank)
{
	CComplexObject& displayObject = GetDisplayObject();
	if (displayObject.IsActive())
	{
		CScaleformMgr::BeginMethodOnComplexObject(displayObject.GetId(), SF_BASE_CLASS_GENERIC, "SET_PROFILE_INFO");
		CScaleformMgr::AddParamInt(profileID);
		CScaleformMgr::AddParamString(profileName, false);
		CScaleformMgr::AddParamString(rpEarned, false);
		CScaleformMgr::AddParamString(headshotTexture, false);
		CScaleformMgr::AddParamInt(rank);
		CScaleformMgr::EndMethod();
	}
}

void CMigrateProfileItem::SetAccountName(int accountNumber, char const * const name)
{
	CComplexObject& displayObject = GetDisplayObject();
	if (displayObject.IsActive())
	{
		CScaleformMgr::BeginMethodOnComplexObject(displayObject.GetId(), SF_BASE_CLASS_GENERIC, "SET_ACCOUNT_NAME");
		CScaleformMgr::AddParamInt(accountNumber);
		CScaleformMgr::AddParamString(name);
		CScaleformMgr::EndMethod();
	}
}

void CMigrateProfileItem::SetTabState(int tabNumber, int state, bool enabled)
{
	CComplexObject& displayObject = GetDisplayObject();
	if (displayObject.IsActive())
	{
		CScaleformMgr::BeginMethodOnComplexObject(displayObject.GetId(), SF_BASE_CLASS_GENERIC, "SET_TAB_STATE");
		CScaleformMgr::AddParamInt(tabNumber);
		CScaleformMgr::AddParamInt(state);
		CScaleformMgr::AddParamBool(enabled);		
		CScaleformMgr::EndMethod();
	}
}

#endif // UI_PAGE_DECK_ENABLED


