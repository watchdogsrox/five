/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : ReviewCharacterItem.h
// PURPOSE : Quick wrapper for the review character element 
//
// AUTHOR  : Charalampos.Koundourakis
// STARTED : August 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef UI_REVIEW_CHARACTER_PROFILE_ITEM_H
#define UI_REVIEW_CHARACTER_PROFILE_ITEM_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// game
#include "frontend/page_deck/layout/PageLayoutItem.h"

class CReviewCharacterItem final : public CPageLayoutItem
{
public:
	CReviewCharacterItem()
	{}
	~CReviewCharacterItem() {}	
	void ShowAsLoading(atHashString titleHash);
	void SetDetails(atHashString titleHash, atHashString descriptionHash, atHashString strapLineHash);
	void SetDetailsLiteral(char const * const title, char const * const description, char const * const strapLine);
	void SetProfileInfo(int profileNumber, char const * const profileName, char const * const rpEarned, char const * const headshotTexture);	
	void SetState(int number, int state);
private: // declarations and variables
	NON_COPYABLE(CReviewCharacterItem);

private: // methods
	char const * GetSymbol() const override { return "migrationCharacterReview"; }
	
};

#endif // UI_PAGE_DECK_ENABLED

#endif // UI_REVIEW_CHARACTER_PROFILE_ITEM_H
