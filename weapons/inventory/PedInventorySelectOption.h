//
// weapons/pedinventoryselectoption.h
//
// Copyright (C) 1999-2010 Rockstar North.  All Rights Reserved. 
//

#ifndef PED_INVENTORY_SELECT_OPTION_H
#define PED_INVENTORY_SELECT_OPTION_H

//////////////////////////////////////////////////////////////////////////
// ePedInventorySelectOption
//////////////////////////////////////////////////////////////////////////

enum ePedInventorySelectOption
{
	SET_INVALID = -1,
	SET_SPECIFIC_SLOT = 0,
	SET_SPECIFIC_ITEM,
	SET_NEXT,
	SET_PREVIOUS,
	SET_BEST,
	SET_BEST_MELEE,
	SET_BEST_NONLETHAL,
	SET_NEXT_IN_GROUP,
	SET_NEXT_CATEGORY,
	SET_PREVIOUS_CATEGORY
};

#endif // PED_INVENTORY_SELECT_OPTION_H
