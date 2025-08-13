/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : StoryItem.h
// PURPOSE : Card specialization for entering Story mode
//
// AUTHOR  : james.strain
// STARTED : May 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef STORY_ITEM_H
#define STORY_ITEM_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// game
#include "frontend/page_deck/items/ParallaxCardItem.h"

class uiPageDeckMessage;

class CStoryItem final : public CParallaxCardItem
{
    FWUI_DECLARE_DERIVED_TYPE(CStoryItem, CParallaxCardItem);

public: // Declarations and variables

    char const * GetItemTitleText() const final;
    char const * GetDescriptionText() const final;

public:
    CStoryItem() : superclass() {}
    virtual ~CStoryItem() {}

private: // declarations and variables
    atHashString m_storyUpsellTitle;
    atHashString m_storyUpsellDescription;
    PAR_PARSABLE;
    NON_COPYABLE( CStoryItem );


private: // methods
    static bool IsStoryModeAvailable();
	virtual void OnPageEventDerived(uiPageDeckMessageBase& msg) final;

	void ForceUpdateVisuals();
};

#endif // UI_PAGE_DECK_ENABLED

#endif // STORY_ITEM_H
