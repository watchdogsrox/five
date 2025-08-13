/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : MigrateProfileItem.h
// PURPOSE : PageLayoutItem wrapper for the migrate profile scaleform element 
//
// AUTHOR  : Charalampos.Koundourakis
// STARTED : August 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef UI_MIGRATE_PROFILE_ITEM_H
#define UI_MIGRATE_PROFILE_ITEM_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// game
#include "frontend/page_deck/layout/PageLayoutItem.h"
#include "frontend/save_migration/SaveMigrationDefs.h"

class CMigrateProfileItem final : public CPageLayoutItem
{
public:
	CMigrateProfileItem()
	{}
	~CMigrateProfileItem() {}

	void ShowAsLoading(atHashString titleHash);
	void SetDetails(atHashString titleHash, atHashString descriptionHash);
	void SetDetailsLiteral(char const * const title, char const * const descriptione);	
	void SetItemInfo(char const * const lastPlayedValue, char const * const missionsValue, char const * const bankedValue, char const * const fundsValue);
	void SetProfileInfo(int profileID, char const * const profileName, char const * const rpEarned, char const * const headshotTexture, int rank);
	void SetAccountName(int accountNumber, char const * const name);
	void SetTabState(int tabNumber, int state, bool enabled);

private: // declarations and variables
	NON_COPYABLE(CMigrateProfileItem);

private: // methods
	char const * GetSymbol() const override { return "migration"; }
};

#endif // UI_PAGE_DECK_ENABLED

#endif // UI_MIGRATE_PROFILE_ITEM_H
