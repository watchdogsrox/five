//
// Inventory.h
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
// Stores the inventory of a player
//
#ifndef INC_NET_INVENTORY_H_
#define INC_NET_INVENTORY_H_

#include "network/NetworkTypes.h"
#if __NET_SHOP_ACTIVE

// Rage headers
#include "atl/hashstring.h"
#include "atl/array.h"
#include "net/status.h"
#include "parser/macros.h"
#include "string/stringhash.h"

// Game headers
#include "stats/StatsTypes.h"

namespace rage { class RsonReader; };

//PURPOSE: Store inventory item and amount of it
class netInventoryBaseItem
{
public:
	netInventoryBaseItem() {}
	netInventoryBaseItem(atHashString& itemKey, const s64 amount, const s64 statvalue, const s64 pricepaid) 
		: m_itemKey(itemKey)
		, m_amount(amount) 
		, m_statvalue(statvalue) 
		, m_pricepaid(pricepaid) 
	{}

	virtual ~netInventoryBaseItem() {}

	const atHashString& GetItemKey() const {return m_itemKey;}
	s64 GetAmount() const {return m_amount;}
 	s64 GetStatValue() const {return m_statvalue;}

	virtual bool Serialize(RsonWriter& wr) const;
	virtual bool Deserialize(const RsonReader& rr);

protected:
	atHashString  m_itemKey;
	s64           m_amount;
	s64           m_statvalue;
	s64           m_pricepaid;

	PAR_PARSABLE;
};

//PURPOSE: Inventory items for packed stats.
class netInventoryPackedStatsItem : public netInventoryBaseItem
{
public:
	netInventoryPackedStatsItem() {}
	netInventoryPackedStatsItem(atHashString& itemKey, const s64 amount, const s64 statvalue, const s64 pricepaid, const int charSlot)
		: netInventoryBaseItem(itemKey, amount, statvalue, pricepaid)
		, m_charSlot(charSlot)
	{}

	virtual ~netInventoryPackedStatsItem() {}
	int GetCharacterSlot() const { return m_charSlot; }

	//PURPOSE: Returns the class hash value.
	static u32 GetTypeHash() { return ATSTRINGHASH("netInventoryPackedStatsItem", 0x50f4151b); }

	virtual bool Serialize(RsonWriter& wr) const;
	virtual bool Deserialize(const RsonReader& rr);

	void  SetSlot(const int slot) { m_charSlot = slot; }

private:
	int m_charSlot;

	PAR_PARSABLE;
};

#endif //__NET_SHOP_ACTIVE

#endif //INC_NET_INVENTORY_H_

