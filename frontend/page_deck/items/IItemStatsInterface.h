/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : IItemStatsInterface.h
// PURPOSE : Interface for working with display stats on items
//
// AUTHOR  : stephen.phillips
// STARTED : July 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef I_ITEM_STATS_INTERFACE
#define I_ITEM_STATS_INTERFACE

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// framework
#include "fwui/Interfaces/Interface.h"

class CFeaturedItem;

class IItemStatsInterface
{
	FWUI_DECLARE_INTERFACE(IItemStatsInterface);
public: // methods
	virtual void AssignStats(CFeaturedItem& featuredItem) const = 0;
};

FWUI_DEFINE_INTERFACE(IItemStatsInterface);

#endif // UI_PAGE_DECK_ENABLED

#endif // I_ITEM_TEXTURE_INTERFACE_H
