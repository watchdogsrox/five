//
// Inventory.cpp
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
// Stores the contents of the inventory
//

#include "Network/NetworkTypes.h"

#if __NET_SHOP_ACTIVE

//rage headers
#include "data/rson.h"
#include "diag/seh.h"
#include "parser/manager.h"

// framework headers
#include "fwnet/netchannel.h"

//game headers
#include "Inventory.h"
#include "Inventory_parser.h"
#include "network/Shop/Catalog.h"
#include "network/Shop/NetworkGameTransactions.h"
#include "network/Shop/GameTransactionsSessionMgr.h"
#include "network/NetworkInterface.h"
#include "stats/MoneyInterface.h"

NETWORK_OPTIMISATIONS();
NETWORK_SHOP_OPTIMISATIONS();

PARAM(inventoryNotEncrypted, "If present, game understands that inventory file is not encrypted.");
PARAM(inventoryFile, "If present, setup the inventory fine name.");


RAGE_DEFINE_SUBCHANNEL(net, shop_inventory, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_shop_inventory

using namespace NetworkGameTransactions;

// --- netInventoryBaseItem ------------------------------------------------------------------------------------

bool netInventoryBaseItem::Serialize( RsonWriter& wr ) const
{
	return wr.WriteString("ItemKey", m_itemKey.TryGetCStr())
		&& wr.WriteInt64("Amount", m_amount)
		&& wr.WriteInt64("StatValue", m_statvalue)
		&& wr.WriteInt64("PricePaid", m_pricepaid);
}

bool netInventoryBaseItem::Deserialize( const RsonReader& rr )
{
	char buffer[128];
	bool success = rr.ReadString("ItemKey", buffer)
				&& rr.ReadInt64("Amount", m_amount)
				&& rr.ReadInt64("StatValue", m_statvalue)
				&& rr.ReadInt64("PricePaid", m_pricepaid);

	m_itemKey = buffer;

	return success;
}

// --- netInventoryPackedStatsItem ------------------------------------------------------------------------------------

bool netInventoryPackedStatsItem::Serialize( RsonWriter& wr ) const
{
	return netInventoryBaseItem::Serialize( wr ) 
		&& wr.WriteInt("CharSlot", m_charSlot);
}

bool netInventoryPackedStatsItem::Deserialize( const RsonReader& rr )
{
	return netInventoryBaseItem::Deserialize( rr );
	/* && rr.ReadInt("CharSlot", m_charSlot); */
}

#endif // __NET_SHOP_ACTIVE

//eof

