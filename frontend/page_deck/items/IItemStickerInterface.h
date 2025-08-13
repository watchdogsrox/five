/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : IItemStickerInterface.h
// PURPOSE : Interface for working with stickers from different sources on items
//
// AUTHOR  : james.strain
// STARTED : August 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef I_ITEM_STICKER_INTERFACE_H
#define I_ITEM_STICKER_INTERFACE_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// framework
#include "fwui/Interfaces/Interface.h"

class IItemStickerInterface
{
    FWUI_DECLARE_INTERFACE( IItemStickerInterface );
public: // declarations and variables
    enum class StickerType : rage::u32
    {
        None = 0,
        Primary,
        PrimarySale,
        Secondary,
        SecondaryGift,
        Tertiary,
        TertiaryRockstar,
        PlatformSpecific, // PlayStation / Xbox, etc...
        Twitch,
        TickText,
        Tick,
        LockText,
        Lock
    };

public: // methods
    virtual char const * GetStickerText() const = 0;
    virtual StickerType GetItemStickerType() const = 0;
	virtual StickerType GetPageStickerType() const = 0;
private:
};

FWUI_DEFINE_INTERFACE( IItemStickerInterface );

#endif // UI_PAGE_DECK_ENABLED

#endif // I_ITEM_STICKER_INTERFACE_H
