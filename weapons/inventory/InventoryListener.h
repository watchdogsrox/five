//
// weapons/inventorylistener.h
//
// Copyright (C) 1999-2010 Rockstar North.  All Rights Reserved. 
//

#ifndef INVENTORY_LISTENER_H
#define INVENTORY_LISTENER_H

// Game headers
#include "fwtl/RegdRefs.h"

//////////////////////////////////////////////////////////////////////////
// CInventoryListener
//////////////////////////////////////////////////////////////////////////

class CInventoryListener : public fwRefAwareBase
{
public:

	CInventoryListener() {}
	virtual ~CInventoryListener() {}

	// Called when an item is added to the inventory
	virtual void ItemAdded(u32 UNUSED_PARAM(uItemNameHash)) {}

	// Called when an item is removed from the inventory
	virtual void ItemRemoved(u32 UNUSED_PARAM(uItemNameHash)) {}

	// Called when all items are removed from the inventory
	virtual void AllItemsRemoved() {}
};

#endif // INVENTORY_LISTENER_H
