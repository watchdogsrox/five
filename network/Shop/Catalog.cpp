//
// Catalog.cpp
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
// Stores the contents of the transaction server catalog
//

#include "Network/NetworkTypes.h"

#if __NET_SHOP_ACTIVE

#include "Catalog.h"
#include "Catalog_parser.h"

// rage headers
#include "parser/manager.h"
#include "system/param.h"
#include "data/aes.h"
#include "paging/streamer.h"
#include "string/stringhash.h"
#include "system/timer.h"
#include "parser/psofile.h"
#include "parser/psoparserbuilder.h"
#include "zlib/lz4.h"
#include "fwmaths/random.h"

// framework headers
#include "fwnet/netchannel.h"
#include "fwnet/netcloudfiledownloadhelper.h"

// game headers
#include "network/shop/inventory.h"
#include "network/shop/NetworkShopping.h"
#include "network/shop/VehicleCatalog.h"
#include "network/shop/GameTransactionsSessionMgr.h"
#include "network/cloud/Tunables.h"
#include "network/live/livemanager.h"
#include "script/script.h"
#include "stats/StatsInterface.h"
#include "stats/StatsMgr.h"
#include "stats/StatsDataMgr.h"

#define __USE_ZLIB_STREAM 1

#if __USE_ZLIB_STREAM
#include "rline/rltelemetry.h"
#include "system/simpleallocator.h"
#endif // __USE_ZLIB_STREAM

NETWORK_OPTIMISATIONS();
NETWORK_SHOP_OPTIMISATIONS();

PARAM(catalogNotEncrypted, "If present, game understands that catalog file is not encrypted on cloud.");
PARAM(catalogCloudFile, "If present, setup the catalog fine name.");
PARAM(catalogParseOpenDeleteItems, "If present, we will also parse Open/Delete Items.");
PARAM(nocatalogcache, "If present, we will dont parse catalog cache.");
PARAM(forceCatalogCacheSave, "If present, we will force saving of the catalog cache.");
XPARAM(sc_UseBasketSystem);
#if !__FINAL
XPARAM(catalogVersion);
#endif //!__FINAL

RAGE_DEFINE_SUBCHANNEL(net, shop_catalog, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_shop_catalog

netCatalog netCatalog::sm_Instance;

#if RSG_DURANGO || RSG_ORBIS || RSG_XENON
static const int THREAD_CPU_ID = 5;
#else
static const int THREAD_CPU_ID = 0;
#endif // RSG_DURANGO || RSG_ORBIS || RSG_XENON

static sysThreadPool s_catalogThreadPool;

// --- Memory ------------------------------------------------------------------------------------

u8* TryAllocateMemory(const u32 size, const u32 alignment = 16)
{
	sysMemAllocator& oldAllocator = sysMemAllocator::GetCurrent();
	sysMemAllocator::SetCurrent(sysMemAllocator::GetMaster());
	u8* data = reinterpret_cast<u8*>(sysMemAllocator::GetCurrent().TryAllocate(size, alignment));
	sysMemAllocator::SetCurrent(oldAllocator);

	return data; 
}

void TryDeAllocateMemory(void* address)
{
	if(!address)
		return;

	sysMemAllocator& oldAllocator = sysMemAllocator::GetCurrent();
	sysMemAllocator::SetCurrent(sysMemAllocator::GetMaster());
	sysMemAllocator::GetCurrent().Free(address);
	sysMemAllocator::SetCurrent(oldAllocator);
}

// --- netCatalogBaseItem ------------------------------------------------------------------------------------

#if !__FINAL
const char* GetNonFinalCategoryName( const int category )
{
	switch (category)
	{
	case CATEGORY_DATA_STORAGE: return "CATEGORY_DATA_STORAGE"; break;
	case CATEGORY_DECORATION: return "CATEGORY_DECORATION"; break;
	case CATEGORY_CLOTH: return "CATEGORY_CLOTH"; break;
	case CATEGORY_WEAPON: return "CATEGORY_WEAPON"; break;
	case CATEGORY_WEAPON_MOD: return "CATEGORY_WEAPON_MOD"; break;
	case CATEGORY_MART: return "CATEGORY_MART"; break;
	case CATEGORY_VEHICLE: return "CATEGORY_VEHICLE"; break;
	case CATEGORY_VEHICLE_MOD: return "CATEGORY_VEHICLE_MOD"; break;
	case CATEGORY_INVENTORY_VEHICLE_MOD: return "CATEGORY_INVENTORY_VEHICLE_MOD"; break;
	case CATEGORY_PROPERTIE: return "CATEGORY_PROPERTIE"; break;
	case CATEGORY_SERVICE: return "CATEGORY_SERVICE"; break;
	case CATEGORY_SERVICE_WITH_THRESHOLD: return "CATEGORY_SERVICE_WITH_THRESHOLD"; break;
	case CATEGORY_WEAPON_AMMO: return "CATEGORY_WEAPON_AMMO"; break;
	case CATEGORY_BEARD: return "CATEGORY_BEARD"; break;
	case CATEGORY_MKUP: return "CATEGORY_MKUP"; break;
	case CATEGORY_INVENTORY_ITEM: return "CATEGORY_INVENTORY_ITEM"; break;
	case CATEGORY_INVENTORY_VEHICLE: return "CATEGORY_INVENTORY_VEHICLE"; break;
	case CATEGORY_INVENTORY_PROPERTIE: return "CATEGORY_INVENTORY_PROPERTIE"; break;
	case CATEGORY_INVENTORY_BEARD: return "CATEGORY_INVENTORY_BEARD"; break;
	case CATEGORY_INVENTORY_MKUP: return "CATEGORY_INVENTORY_MKUP"; break;
	case CATEGORY_HAIR: return "CATEGORY_HAIR"; break;
	case CATEGORY_TATTOO: return "CATEGORY_TATTOO"; break;
	case CATEGORY_INVENTORY_HAIR: return "CATEGORY_INVENTORY_HAIR"; break;
	case CATEGORY_INVENTORY_EYEBROWS: return "CATEGORY_INVENTORY_EYEBROWS"; break;
	case CATEGORY_INVENTORY_CHEST_HAIR: return "CATEGORY_INVENTORY_CHEST_HAIR"; break;
	case CATEGORY_INVENTORY_CONTACTS: return "CATEGORY_INVENTORY_CONTACTS"; break;
	case CATEGORY_INVENTORY_FACEPAINT: return "CATEGORY_INVENTORY_FACEPAINT"; break;
	case CATEGORY_INVENTORY_BLUSHER: return "CATEGORY_INVENTORY_BLUSHER"; break;
	case CATEGORY_INVENTORY_LIPSTICK: return "CATEGORY_INVENTORY_LIPSTICK"; break;
	case CATEGORY_CONTACTS: return "CATEGORY_CONTACTS"; break;
	case CATEGORY_PRICE_MODIFIER: return "CATEGORY_PRICE_MODIFIER"; break;
	case CATEGORY_PRICE_OVERRIDE: return "CATEGORY_PRICE_OVERRIDE"; break;
	case CATEGORY_SERVICE_UNLOCKED: return "CATEGORY_SERVICE_UNLOCKED"; break;
	case CATEGORY_EYEBROWS: return "CATEGORY_EYEBROWS"; break;
	case CATEGORY_CHEST_HAIR: return "CATEGORY_CHEST_HAIR"; break;
	case CATEGORY_FACEPAINT: return "CATEGORY_FACEPAINT"; break;
	case CATEGORY_BLUSHER: return "CATEGORY_BLUSHER"; break;
	case CATEGORY_LIPSTICK: return "CATEGORY_LIPSTICK"; break;
	case CATEGORY_SERVICE_WITH_LIMIT: return "CATEGORY_SERVICE_WITH_LIMIT"; break;
	case CATEGORY_SYSTEM: return "CATEGORY_SYSTEM"; break;
	case CATEGORY_VEHICLE_UPGRADE: return "CATEGORY_VEHICLE_UPGRADE"; break;
	case CATEGORY_INVENTORY_PROPERTY_INTERIOR: return "CATEGORY_INVENTORY_PROPERTY_INTERIOR"; break;
	case CATEGORY_PROPERTY_INTERIOR: return "CATEGORY_PROPERTY_INTERIOR"; break;
	case CATEGORY_INVENTORY_WAREHOUSE: return "CATEGORY_INVENTORY_WAREHOUSE"; break;
	case CATEGORY_WAREHOUSE: return "CATEGORY_WAREHOUSE"; break;
	case CATEGORY_INVENTORY_CONTRABAND_MISSION: return "CATEGORY_INVENTORY_CONTRABAND_MISSION"; break;
	case CATEGORY_CONTRABAND_MISSION: return "CATEGORY_CONTRABAND_MISSION"; break;
	case CATEGORY_CONTRABAND_QNTY: return "CATEGORY_CONTRABAND_QNTY"; break;
	case CATEGORY_CONTRABAND_FLAGS: return "CATEGORY_CONTRABAND_FLAGS"; break;
	case CATEGORY_INVENTORY_WAREHOUSE_INTERIOR: return "CATEGORY_INVENTORY_WAREHOUSE_INTERIOR"; break;
	case CATEGORY_WAREHOUSE_INTERIOR: return "CATEGORY_WAREHOUSE_INTERIOR"; break;
	case CATEGORY_WAREHOUSE_VEHICLE_INDEX: return "CATEGORY_WAREHOUSE_VEHICLE_INDEX"; break;
    case CATEGORY_INVENTORY_PRICE_PAID: return "CATEGORY_INVENTORY_PRICE_PAID"; break;
	case CATEGORY_CASINO_CHIPS: return "CATEGORY_CASINO_CHIP"; break;
	case CATEGORY_CASINO_CHIP_REASON: return "CATEGORY_CASINO_CHIP_REASON"; break;
	case CATEGORY_CURRENCY_TYPE: return "CATEGORY_CURRENCY_TYPE"; break;
	case CATEGORY_INVENTORY_CURRENCY: return "CATEGORY_INVENTORY_CURRENCY"; break;
	case CATEGORY_EARN_CURRENCY: return "CATEGORY_EARN_CURRENCY"; break;
	case CATEGORY_UNLOCK: return "CATEGORY_UNLOCK"; break;
	}

	gnetAssertf(0, "GetNonFinalCategoryName - Invalid category='%d'.", category);

	return "INVALID";
}
#endif //!__FINAL

netCatalogBaseItem::netCatalogBaseItem(const int keyhash, const int category, const int price) 
{
	if (!gnetVerify(NETWORK_SHOPPING_MGR.GetCategoryIsValid((NetShopCategory)category)))
		return;

	m_keyhash  = keyhash;

#if !__FINAL
	m_category = atHashString( GetNonFinalCategoryName( category ) );
#else
	m_category = category;
#endif //!__FINAL

	m_price    = price;
}

void netCatalogBaseItem::OnSetStatValue(StatId& statkey, const s64 value)
{
	gnetDebug1("Change Stat key=\"%s\", hash=\"%d\", value=\"%" I64FMT "d\".", statkey.GetName(), statkey.GetHash(), value);

	gnetAssert(StatsInterface::GetStatDesc(statkey)->GetSynched());

	switch (StatsInterface::GetStatType(statkey))
	{
	case STAT_TYPE_INT:    StatsInterface::SetStatData(statkey,   static_cast<int>(value), STATUPDATEFLAG_DEFAULT_GAMER_SERVER); break;
	case STAT_TYPE_INT64:  StatsInterface::SetStatData(statkey,                     value, STATUPDATEFLAG_DEFAULT_GAMER_SERVER); break;
	case STAT_TYPE_FLOAT:  StatsInterface::SetStatData(statkey, static_cast<float>(value), STATUPDATEFLAG_DEFAULT_GAMER_SERVER); break;
	case STAT_TYPE_UINT8:  StatsInterface::SetStatData(statkey,    static_cast<u8>(value), STATUPDATEFLAG_DEFAULT_GAMER_SERVER); break;
	case STAT_TYPE_UINT16: StatsInterface::SetStatData(statkey,   static_cast<u16>(value), STATUPDATEFLAG_DEFAULT_GAMER_SERVER); break;
	case STAT_TYPE_UINT32: StatsInterface::SetStatData(statkey,   static_cast<u32>(value), STATUPDATEFLAG_DEFAULT_GAMER_SERVER); break;
	case STAT_TYPE_TIME:   StatsInterface::SetStatData(statkey,   static_cast<u64>(value), STATUPDATEFLAG_DEFAULT_GAMER_SERVER); break;
	case STAT_TYPE_UINT64: StatsInterface::SetStatData(statkey,   static_cast<u64>(value), STATUPDATEFLAG_DEFAULT_GAMER_SERVER); break;

	default:
		gnetAssertf(0, "Invalid type %d", StatsInterface::GetStatType(statkey));
		StatsInterface::SetStatData(statkey,   static_cast<int>(value), STATUPDATEFLAG_DEFAULT_GAMER_SERVER); break;
		break;
	}
}

#if __BANK

//PURPOSE: Link another catalog item.
bool netCatalogBaseItem::Link(atHashString& key)
{
	if (IsLinked(key))
	{
		gnetWarning("key='%d' is already linked", key.GetHash());
		return false;
	}

	if (gnetVerify(m_openitems.GetCount() < CATALOG_MAX_NUM_OPEN_ITEMS))
	{
		gnetDebug1("Add link key='%d'.", key.GetHash());

		m_openitems.PushAndGrow( key );
	}

	return true;
}

//PURPOSE: Remove an existing Linked item.
bool netCatalogBaseItem::RemoveLink(atHashString& key)
{
	if (IsLinked(key))
	{
		gnetDebug1("Remove link key='%d'.", key.GetHash());

		m_openitems.Delete(key);

		return true;
	}

	return false;
}

//PURPOSE: Return true if the link exists.
bool netCatalogBaseItem::IsLinked(atHashString& key) const 
{
	for (int i=0; i<m_openitems.GetCount(); i++)
	{
		if (m_openitems[i] == key)
			return true;
	}

	return false;
}

//PURPOSE: Link another catalog item.
bool netCatalogBaseItem::LinkForSelling(atHashString& key)
{
	for (int i=0; i<m_deleteitems.GetCount(); i++)
	{
		if (m_deleteitems[i] == key)
			return false;
	}

	if (gnetVerify(m_deleteitems.GetCount() < CATALOG_MAX_NUM_DELETE_ITEMS))
	{
		gnetDebug1("Add link for selling. key='%d'.", key.GetHash());

		m_deleteitems.PushAndGrow( key );
	}


	return true;
}

//PURPOSE: Remove an existing Linked item.
bool netCatalogBaseItem::RemoveLinkForSelling(atHashString& key)
{
	if (IsLinkedForSelling(key))
	{
		gnetDebug1("Remove link for selling key='%d'.", key.GetHash());

		m_deleteitems.Delete(key);

		return true;
	}

	return false;
}

//PURPOSE: Return true if the link exists.
bool netCatalogBaseItem::IsLinkedForSelling(atHashString& key) const 
{
	for (int i=0; i<m_deleteitems.GetCount(); i++)
	{
		if (m_deleteitems[i] == key)
			return true;
	}

	return false;
}

#endif //__BANK

bool netCatalogBaseItem::Deserialize(const RsonReader& rr)
{
	bool retVal = true;
	char buffer[CATALOG_MAX_KEY_LABEL_SIZE];

	RsonReader itemreader;
	if(gnetVerify(rr.GetMember("key", &itemreader) && itemreader.AsString(buffer)))
	{
		m_keyhash = buffer;
	}
	else
	{
		retVal = false;
	}

	if(rr.GetMember("price", &itemreader))
	{
		LoadParam(itemreader, m_price);
	}

#if __BANK
	if (rr.GetMember("textTag", &itemreader) && gnetVerify(itemreader.AsString(buffer)))
	{
		m_textTag = buffer;
	}

	if (rr.GetMember("name", &itemreader) && gnetVerify(itemreader.AsString(buffer)))
	{
		m_name = buffer;
	}

	if (PARAM_catalogParseOpenDeleteItems.Get())
	{
		LoadHashArrayParam(rr, "openItems", m_openitems);
		LoadHashArrayParam(rr, "deleteItems", m_deleteitems);
	}
#endif // __BANK

	return retVal;
}

void netCatalogBaseItem::SetCatagory(atHashString  category)
{
	m_category = category;
	gnetAssert(NETWORK_SHOPPING_MGR.GetCategoryIsValid((NetShopCategory)m_category.GetHash()));
}

int netCatalogBaseItem::FindStorageType(atHashString type)
{
	static u32 typeInt = (u32)ATSTRINGHASH("INT", 0x212acca2);
	if (type.GetHash() == typeInt)
		return 0;

	static u32 typeInt64 = (u32)ATSTRINGHASH("INT64", 0xef7a434d);
	if (type.GetHash() == typeInt64)
		return 1;

	static u32 typeBitField = (u32)ATSTRINGHASH("BITFIELD", 0x101cbfda);
	if (type.GetHash() == typeBitField)
		return 2;

	return 0;
}

bool netCatalogBaseItem::LoadHashArrayParam(const RsonReader& rr, const char* pMemberName, atArray<atHashString>& param)
{
	RsonReader currItem;
	bool foundMemeber = rr.GetMember(pMemberName, &currItem);
	if (foundMemeber)
	{
		if(currItem.HasMember("item"))
		{
			currItem.GetMember("item", &currItem);
		}

		foundMemeber = currItem.GetFirstMember(&currItem);
		if (foundMemeber)
		{
			for(; foundMemeber; foundMemeber = currItem.GetNextSibling(&currItem))
			{
				char arrayOfOpenItemsToRead[CATALOG_MAX_KEY_LABEL_SIZE];
				currItem.AsString( arrayOfOpenItemsToRead );

				atHashString openitemkeyhash( arrayOfOpenItemsToRead );
				param.PushAndGrow( openitemkeyhash );
			}
		}
		else
		{
			char buffer[CATALOG_MAX_KEY_LABEL_SIZE];
			currItem.AsString( buffer );

			atHashString itemHash( buffer );
			param.PushAndGrow( itemHash );
		}

		return true;
	}

	return false;
}

// --- netCatalogPackedStatInventoryItem ------------------------------------------------------------------------------------

StatId netCatalogPackedStatInventoryItem::GetStat() const
{
	//Remove CHAR_MP0 - because we are not calling GetOnlineStatId() and we are
        // using Multiplayer slots as indexes. 0 to max number of possible characters.
	const int charSlot = packed_stats::GetIsCharacterStat(GetStatEnum()) ?  GameTransactionSessionMgr::Get().GetCharacterSlot() - CHAR_MP0 : -1;

	StatId statkey;
	packed_stats::GetStatKey(statkey, GetStatEnum(), charSlot);

	return statkey;
}

u32 netCatalogPackedStatInventoryItem::GetHash(int slot) const
{
	//Remove CHAR_MP0 - because we are not calling GetOnlineStatId() and we are
        // using Multiplayer slots as indexes. 0 to max number of possible characters.
	const int charSlot = packed_stats::GetIsCharacterStat(GetStatEnum()) ?  slot : -1;

	StatId statkey;
	packed_stats::GetStatKey(statkey, GetStatEnum(), charSlot);

	return statkey.GetHash( );
}

bool netCatalogPackedStatInventoryItem::IsValid( ) const 
{
	bool result = false;

	StatId statkey;
	if (packed_stats::GetIsCharacterStat(m_statEnumValue))
	{
		result = packed_stats::GetStatKey(statkey, m_statEnumValue, 0);
	}
	else
	{
		result = packed_stats::GetStatKey(statkey, m_statEnumValue, -1);
	}

	if (gnetVerify( result ))
	{
		if (StatsInterface::IsKeyValid(statkey))
		{
			return true;
		}
	}

	gnetError("netCatalogPackedStatInventoryItem::IsValid():: Invalid key='%d', statName='%s:%d", GetKeyHash().GetHash(), statkey.GetName(), statkey.GetHash());

	return false;
}

bool netCatalogPackedStatInventoryItem::SetupInventoryStats( )
{
	bool result = IsValid();

	if (result)
	{
		CStatsDataMgr& statsDataMgr = CStatsMgr::GetStatsDataMgr();

		if (packed_stats::GetIsCharacterStat(m_statEnumValue))
		{
			for (int i=0; i<MAX_NUM_MP_CHARS && result; i++)
			{
				StatId statkey;
				result &= packed_stats::GetStatKey(statkey, m_statEnumValue, i);
				if (gnetVerify(statkey.IsValid()))
				{
					CStatsDataMgr::StatsMap::Iterator statIter;
					statsDataMgr.StatIterFind(statkey, statIter);
					result &= statsDataMgr.SetIsPlayerInventory(statIter);
				}
			}
		}
		else
		{
			StatId statkey;
			result &= packed_stats::GetStatKey(statkey, m_statEnumValue, -1);
			if (gnetVerify(statkey.IsValid()))
			{
				CStatsDataMgr::StatsMap::Iterator statIter;
				statsDataMgr.StatIterFind(statkey, statIter);
				result &= statsDataMgr.SetIsPlayerInventory(statIter);
			}
		}

	}

	return result;
}

netCatalogPackedStatInventoryItem::netCatalogPackedStatInventoryItem() 
	: m_statEnumValue(0)
#if !__FINAL
	, m_isPlayerStat(false)
	, m_bitSize(0)
	, m_bitShift(0)
#endif //!__FINAL
{}

#if !__FINAL
netCatalogPackedStatInventoryItem::netCatalogPackedStatInventoryItem(const int keyhash
																	, const int category
																	, const int price
																	, const u32 statenumvalue)
	: Base(keyhash, category, price)
	, m_statEnumValue(statenumvalue)
{
	SetupStatsMapping();
}
#endif //!__FINAL

void netCatalogPackedStatInventoryItem::PostLoad()
{
	SetupStatsMapping();
}

void netCatalogPackedStatInventoryItem::SetupStatsMapping()
{
#if !__FINAL
	m_isPlayerStat = packed_stats::GetIsPlayerStat(m_statEnumValue);

	StatId statkey;
	int shift;

	const bool hasStatKey = packed_stats::GetStatKey(statkey, m_statEnumValue, m_isPlayerStat ? -1 : 0);
	const bool hasStatShiftValue = packed_stats::GetStatShiftValue(shift, m_statEnumValue);
	//We pass character slot 0 because its not important to get the stat name here.
	if (gnetVerify(hasStatKey) && gnetVerify(hasStatShiftValue))
	{
		int numBits = NUM_INT_PER_STAT;
		if (packed_stats::GetIsBooleanType(m_statEnumValue))
		{
			numBits = NUM_BOOL_PER_STAT;
		}

		m_bitSize = (u8)numBits;
		m_bitShift = (u8)shift;

		//Remove the character prefix if its a character stat - server will take care of filling in the correct character prefix.
		m_statName.Set(statkey.GetName(), (int)strlen(statkey.GetName()), m_isPlayerStat ? 0 : 4);
	}
#if !__NO_OUTPUT
	else
	{
		if (!hasStatKey)
		{
			gnetError("packed_stats::GetStatKey - Failed for key='%s : %d', m_statEnumValue='%d'.", GetItemKeyName(), GetKeyHash().GetHash(), m_statEnumValue);
		}

		if (!hasStatShiftValue)
		{
			gnetError("packed_stats::GetStatShiftValue - Failed for key='%s : %d', m_statEnumValue='%d'.", GetItemKeyName(), GetKeyHash().GetHash(), m_statEnumValue);
		}
	}
#endif // !__NO_OUTPUT
#endif // !__FINAL
}

//PURPOSE: Set amount of catalog item to stats
void netCatalogPackedStatInventoryItem::SetStatValue(const netInventoryBaseItem* item) const
{
	if (gnetVerify(item))
	{
		const netInventoryPackedStatsItem* moditem = static_cast< const netInventoryPackedStatsItem* >(item);

		StatId statkey;
		int shift;

		const int characterSlot = packed_stats::GetIsCharacterStat(m_statEnumValue) ? moditem->GetCharacterSlot() : -1;

		if (gnetVerify( packed_stats::GetStatKey(statkey, m_statEnumValue, characterSlot) ) &&
			gnetVerify( packed_stats::GetStatShiftValue(shift, m_statEnumValue) ))
		{
			gnetAssert(StatsInterface::GetStatDesc(statkey)->GetSynched());

			int numBits   = NUM_INT_PER_STAT;
			int statValue = (int)moditem->GetStatValue();

			if (packed_stats::GetIsBooleanType(m_statEnumValue))
			{
				numBits = NUM_BOOL_PER_STAT;

				//This hack sorts out an issue with the server returning a count of items 
				//  instead of the value 1 for packed boolean stats.
				if (statValue > 1)
					statValue = 1;
			}

			gnetDebug1("Change Stat key=\"%s\", hash=\"%d\", value=\"%d\", enumValue=\"%d\", numBits=\"%d\", shift=\"%d\"."
							,statkey.GetName()
							,statkey.GetHash()
							,statValue
							,m_statEnumValue
							,numBits
							,shift);

			ASSERT_ONLY(const bool result = ) StatsInterface::SetMaskedInt(statkey, statValue, shift, numBits, false, STATUPDATEFLAG_DEFAULT_GAMER_SERVER);
			ASSERT_ONLY(gnetAssertf(result, "netCatalogPackedStatInventoryItem::SetStatValue() - Failed to set value for key='%s', enumvalue='%d', slot='%d', value='%" I64FMT "d'.", moditem->GetItemKey().TryGetCStr(), m_statEnumValue, moditem->GetCharacterSlot(), moditem->GetStatValue());)
		}
	}
}

bool netCatalogPackedStatInventoryItem::Deserialize(const RsonReader& rr)
{
	RsonReader itemReader;
	if(rr.GetMember("statEnumValue", &itemReader))
	{
		LoadParam(itemReader, m_statEnumValue);

		SetupStatsMapping();
	}

	return Base::Deserialize(rr);
}

// --- netCatalogOnlyItemWithStat ------------------------------------------------------------------------------------

bool netCatalogOnlyItemWithStat::Deserialize(const RsonReader& rr)
{
	return Base::Deserialize(rr) 
			&& gnetVerify(rr.ReadInt("statValue", m_statValue));
}


// --- netCatalogInventoryItem ------------------------------------------------------------------------------------

StatId netCatalogInventoryItem::GetStat() const
{
	if (IsCharacterStat())
	{
		gnetAssert(!GameTransactionSessionMgr::Get().IsSlotInvalid());
		StatId_char charStat(m_statName.c_str());
		return charStat.GetOnlineStatId(GameTransactionSessionMgr::Get().GetCharacterSlot());
	}

	StatId stat(m_statName);
	return stat;
}

u32 netCatalogInventoryItem::GetHash(int slot) const
{
	if (IsCharacterStat())
	{
		gnetAssert(slot+CHAR_MP_START < MAX_STATS_CHAR_INDEXES);
		StatId_char charStat(m_statName.c_str());
		return charStat.GetHash(slot+CHAR_MP_START);
	}

	StatId stat(m_statName);
	return stat.GetHash( );
}

bool netCatalogInventoryItem::IsValid( ) const 
{
	if (IsCharacterStat())
	{
		StatId_char charStat(m_statName.c_str());
		if (StatsInterface::IsKeyValid(charStat.GetHash(CHAR_MP0)))
		{
			return true;
		}
	}
	else 
	{
		StatId stat(m_statName);
		if (StatsInterface::IsKeyValid(stat.GetHash()))
		{
			return true;
		}
	}

	OUTPUT_ONLY( gnetError("netCatalogInventoryItem::IsValid():: Catalog item with key='%s : %d' has an invalid stat name='%s'", GetItemKeyName(), GetKeyHash().GetHash(), m_statName.c_str()); );

	return false;
}

bool netCatalogInventoryItem::SetupInventoryStats( )
{
	bool result = IsValid();

	if (result)
	{
		CStatsDataMgr& statsDataMgr = CStatsMgr::GetStatsDataMgr();

		if (IsPlayerStat())
		{
			StatId statkey(m_statName.c_str());
			CStatsDataMgr::StatsMap::Iterator statIter;
			statsDataMgr.StatIterFind(statkey.GetHash(), statIter);
			result &= statsDataMgr.SetIsPlayerInventory(statIter);
		}
		else
		{
			StatId_char statkey(m_statName.c_str());
			for (int i=0; i<MAX_NUM_MP_CHARS && result; i++)
			{
				CStatsDataMgr::StatsMap::Iterator statIter;
				statsDataMgr.StatIterFind(statkey.GetHash(CHAR_MP0+i), statIter);
				result &= statsDataMgr.SetIsPlayerInventory(statIter);
			}
		}

	}

	return result;
}

//PURPOSE: Set amount of catalog item to stats.
void netCatalogInventoryItem::SetStatValue(const netInventoryBaseItem* item) const
{
	if (gnetVerify(item))
	{
		StatId statkey = GetStat();

		const bool statKeyIsValid = StatsInterface::IsKeyValid(statkey);
		OUTPUT_ONLY( gnetAssertf(statKeyIsValid, "netCatalogInventoryItem::SetStatValue() - Catalog item with key='%s: %d' and stat name='%s' has an invalid stat key='%d'.", GetItemKeyName(), GetKeyHash().GetHash(), m_statName.c_str(), statkey.GetHash()) );

		if (statKeyIsValid)
		{
			netCatalogBaseItem::OnSetStatValue(statkey, item->GetStatValue());
		}
	}
}

bool netCatalogInventoryItem::Deserialize(const RsonReader& rr)
{
	bool result = Base::Deserialize(rr) 
		&& gnetVerify(rr.ReadString("statName", m_statName));

	m_isPlayerStat = false;

	RsonReader itemreader;
	if (rr.GetMember("isPlayerStat", &itemreader))
	{
		result = gnetVerify( LoadParam(itemreader, m_isPlayerStat) );
	}
	else if(m_statName.length() > 0)
	{
		if (strncmp(m_statName.c_str(), "MPPLY_", 6) == 0 || strncmp(m_statName.c_str(), "MP_CNCPSTAT_", 12) == 0)
		{
			m_isPlayerStat = true;
		}
	}

	return result;		
}

// --- netCatalogGeneralItem ------------------------------------------------------------------------------------

netCatalogGeneralItem::netCatalogGeneralItem()
	: Base()
	, m_storageType(0)
	, m_bitShift(0)
	, m_bitSize(0)
	, m_isPlayerStat(false)
{
}

netCatalogGeneralItem::netCatalogGeneralItem(const int keyhash
												,const int category
												,const char* textTag
												,const int price
												,const char* statname
												,const int storagetype
												,const int bitshift
												,const int bitsize
												,const bool isPlayerStat) : Base(keyhash, category, price)
{
	if (!gnetVerify(NETWORK_SHOPPING_MGR.GetCategoryIsValid((NetShopCategory)category)))
		return;

	gnetAssert(textTag);
	if (!textTag)
		return;

	gnetAssert(statname);
	if (!statname)
		return;

	if (textTag)
	{
#if __BANK
		SetLabel(textTag);
#endif
	}

	m_storageType = (u8)storagetype;
	m_bitShift = (u8)bitshift;
	m_bitSize = (u8)bitsize;
	m_statName = statname;
	m_isPlayerStat = isPlayerStat;
}

StatId netCatalogGeneralItem::GetStat() const
{
	if (IsCharacterStat())
	{
		gnetAssert(!GameTransactionSessionMgr::Get().IsSlotInvalid());
		StatId_char charStat(m_statName.c_str());
		return charStat.GetOnlineStatId(GameTransactionSessionMgr::Get().GetCharacterSlot());
	}

	StatId stat(m_statName);
	return stat;
}

u32 netCatalogGeneralItem::GetHash(int slot) const
{
	if (IsCharacterStat())
	{
		gnetAssert(slot+CHAR_MP_START < MAX_STATS_CHAR_INDEXES);
		StatId_char charStat(m_statName.c_str());
		return charStat.GetHash(slot+CHAR_MP_START);
	}

	StatId stat(m_statName);
	return stat.GetHash( );
}

bool netCatalogGeneralItem::IsValid( ) const 
{
	if (IsCharacterStat())
	{
		StatId_char charStat(m_statName.c_str());
		if (StatsInterface::IsKeyValid(charStat.GetHash(CHAR_MP0)))
		{
			return true;
		}
	}
	else
	{
		StatId stat(m_statName);
		if (StatsInterface::IsKeyValid(stat))
		{
			return true;
		}
	}

	OUTPUT_ONLY( gnetError("netCatalogGeneralItem::IsValid():: Catalog item with key='%s : %d' has invalid STAT with name='%s'", GetItemKeyName(), GetKeyHash().GetHash(), m_statName.c_str()); );
	return false;
}

bool netCatalogGeneralItem::SetupInventoryStats( )
{
	bool result = IsValid();

	if (result)
	{
		CStatsDataMgr& statsDataMgr = CStatsMgr::GetStatsDataMgr();

		if (IsCharacterStat())
		{
			StatId_char statkey(m_statName.c_str());
			for (int i=0; i<MAX_NUM_MP_CHARS && result; i++)
			{
				CStatsDataMgr::StatsMap::Iterator statIter;
				statsDataMgr.StatIterFind(statkey.GetHash(CHAR_MP0+i), statIter);

				result &= statsDataMgr.SetIsPlayerInventory(statIter);
			}
		}
		else
		{
			StatId statkey(m_statName.c_str());

			CStatsDataMgr::StatsMap::Iterator statIter;
			statsDataMgr.StatIterFind(statkey, statIter);

			result &= statsDataMgr.SetIsPlayerInventory(statIter);
		}
	}

	return result;
}


//PURPOSE: Set amount of catalog item to stats
void netCatalogGeneralItem::SetStatValue(const netInventoryBaseItem* item) const
{
	if (gnetVerify(item))
	{
		StatId statkey = GetStat();
		if ( !gnetVerifyf(StatsInterface::IsKeyValid(statkey), "netCatalogGeneralItem::SetStatValue(%d) - Invalid stat key='%s','%d'.  Statname=%s", GetKeyHash().GetHash(), statkey.GetName(), statkey.GetHash(), GetStatName()) )
			return;

		const s64 statvalue = item->GetStatValue();

		switch(m_storageType)
		{
		case INT64:
		case INT: 
			netCatalogBaseItem::OnSetStatValue(statkey, statvalue); 
			break;

		case BITMASK:
			{
				int newvalue = Assign(newvalue, statvalue);

				if (m_bitSize == NUM_BOOL_PER_STAT && newvalue > 1)
				{
					gnetDebug1("Change Stat key=\"%s\", hash=\"%d\", with wrong newvalue=\"%d\", reset value to 1, m_bitShift=\"%d\", m_bitSize=\"%d\".", statkey.GetName(), statkey.GetHash(), newvalue, m_bitShift, m_bitSize);
					newvalue = 1;
				}
				else if (m_bitSize == NUM_INT_PER_STAT && newvalue > 255)
				{
					gnetError("FAILED to Change Stat key=\"%s\", hash=\"%d\", newvalue=\"%d\", m_bitShift=\"%d\", m_bitSize=\"%d\".", statkey.GetName(), statkey.GetHash(), newvalue, m_bitShift, m_bitSize);
					gnetError("Invalid value=%d for 8 bits.", newvalue);
					gnetAssertf(0, "Invalid value=%d for 8 bits.", newvalue);
				}
				else
				{
					gnetDebug1("Change Stat key=\"%s\", hash=\"%d\", newvalue=\"%d\", m_bitShift=\"%d\", m_bitSize=\"%d\".", statkey.GetName(), statkey.GetHash(), newvalue, m_bitShift, m_bitSize);
				}

				gnetAssert(StatsInterface::GetStatDesc(statkey)->GetSynched());

				StatsInterface::SetMaskedInt(statkey, newvalue, m_bitShift, m_bitSize, false, STATUPDATEFLAG_DEFAULT_GAMER_SERVER);
			}
			break;

		default:
			gnetAssert("netCatalogGeneralItem::SetStatValue: Unrecognised catalog item storage type");
			break;
		}
	}
}

bool netCatalogGeneralItem::Deserialize(const RsonReader& rr)
{
	bool retVal = Base::Deserialize(rr);
	char buffer[CATALOG_MAX_KEY_LABEL_SIZE];

	RsonReader itemreader;

	if (gnetVerify((rr.GetMember("storageType", &itemreader)) && itemreader.AsString(buffer)))
	{
		m_storageType = (u8)FindStorageType(atHashString(buffer));
	}
	else
	{
		retVal = false;
	}

	if (gnetVerify(rr.GetMember("statName", &itemreader) && itemreader.AsString(buffer)))
	{
		m_statName = buffer;
	}
	else
	{
		retVal = false;
	}

	rr.GetMember("bitShift", &itemreader) && LoadParam(itemreader, m_bitShift);
	rr.GetMember("bitSize", &itemreader) && LoadParam(itemreader, m_bitSize);

	m_isPlayerStat = false;
	if (rr.GetMember("isPlayerStat", &itemreader))
	{
		retVal = LoadParam(itemreader, m_isPlayerStat);
	}
	else if(m_statName.length() > 0)
	{
		if (strncmp(m_statName.c_str(), "MPPLY_", 6) == 0 || strncmp(m_statName.c_str(), "MP_CNCPSTAT_", 12) == 0)
		{
			m_isPlayerStat = true;
		}
	}

	return retVal;
}

// --- sPackedStatsScriptExceptions ----------------------------------------------------------------------------------------

#if !__NO_OUTPUT

void sPackedStatsScriptExceptions::SpewBitShifts( ) const
{
	for (int i=0; i<m_bitShift.GetCount(); i++)
		gnetDebug1("               exc='%d'", m_bitShift[i]);
}

#endif // !__NO_OUTPUT

void sPackedStatsScriptExceptions::SetExceptions(const StatId& key, const u64 value) const 
{
	//Create our mask. 0x1 or 0xFF
	const u64 mask = (1 << m_bits) - 1;

	for (int i=0; i<m_bitShift.GetCount(); i++)
	{
		const u64 data       = (value >> m_bitShift[i]); //Shift the bits we want to the right
		const int finalvalue = (int)(data&mask);         //Grab the bits we want that are set.

		//Now update the value.
		StatsInterface::SetMaskedInt(key, finalvalue, m_bitShift[i], m_bits, false, STATUPDATEFLAG_DEFAULT_GAMER_SERVER);
	}
}


// --- netCatalog ----------------------------------------------------------------------------------------

sysThreadPool::Thread  netCatalog::s_catalogThread;

bool netCatalog::sm_CheckForDuplicatesOnAddingItems = false;

void netCatalog::SetCheckForDuplicatesOnAddItem(const bool checkForDuplicates)
{
	if(sm_CheckForDuplicatesOnAddingItems != checkForDuplicates)
	{
		gnetDebug1("SetCheckForDuplicatesOnAddItem :: %s -> %s", checkForDuplicates ? "True" : "False", sm_CheckForDuplicatesOnAddingItems ? "True" : "False");
		sm_CheckForDuplicatesOnAddingItems = checkForDuplicates;
	}
}

netCatalog::~netCatalog()
{
	Shutdown();
}

void netCatalog::Init() 
{
	gnetDebug1("InitialiseThreadPool - s_catalogThreadPool");
	s_catalogThreadPool.Init();
	s_catalogThreadPool.AddThread(&s_catalogThread, "CatalogLoad", THREAD_CPU_ID, (32 * 1024) << __64BIT);
	m_catalogCache.Init();
}

void  netCatalog::CancelSaveWorker()
{
	if (m_catalogCache.Pending())
		m_catalogCache.CancelWork();

	while(m_catalogCache.Pending())
	{
		gnetDebug1("Waiting for catalog save worker finish.");
		sysIpcSleep(5);
	}
}

void  netCatalog::Shutdown()
{
	CancelSaveWorker();

	atMap< atHashString, netCatalogBaseItem* >::Iterator it = m_catalog.CreateIterator();
	for (it.Start(); !it.AtEnd(); it.Next())
	{
		netCatalogBaseItem** item = it.GetDataPtr();
		if (item)
			delete *item;
	}
	m_catalog.Kill();

	m_ReadStatus.Reset();

	gnetDebug1("ShutdownThreadPool - s_catalogThreadPool");
	s_catalogThreadPool.Shutdown();
}

#if !__FINAL
// PURPOSE: Read local catalog file.
void netCatalog::Read()
{
	m_ReadStatus.Reset();
	m_ReadStatus.SetPending();
	PARSER.LoadObject("common:/data/debug/catalog", "meta", *this);
	m_ReadStatus.SetSucceeded();
}

// PURPOSE: Write local catalog file.
void netCatalog::Write()
{
	netCatalogMap::ConstIterator entry = GetCatalogIterator();
	for (entry.Start(); !entry.AtEnd(); entry.Next())
	{
		netCatalogBaseItem* item = entry.GetData();
		if (item)
		{
			gnetAssertf(item->GetCategoryName(), "Missing category name - this will fail Write and crash.");
			gnetAssertf(item->GetItemKeyName(), "Missing Key name - this will fail Write and crash.");
		}
	}

	gnetDebug1("Catalog Map Write - GetNumUsed='%d'.", m_catalog.GetNumUsed());

	parSettings settings;
	settings.SetFlag(parSettings::WRITE_IGNORE_DEFAULTS, true);

	PARSER.SaveObject("common:/data/debug/catalog", "meta", this, parManager::XML, &settings);
}
#endif

//PURPOSE: Clear current catalog download status.
void netCatalog::ClearDownloadStatus()
{
	m_ReadStatus.Reset();
}

//PURPOSE: Returns true when the request is successful.
bool netCatalog::HasReadSucceeded() const
{
	return m_ReadStatus.Succeeded();
}

//PURPOSE: Returns true when the request has failed.
bool netCatalog::HasReadFailed() const
{
	return m_ReadStatus.Failed();
}

//PURPOSE: Set inventory data in stats.
bool netCatalog::SetStat(const netInventoryBaseItem* item) const
{
	if (gnetVerify(item))
	{
		const atHashString& itemId = item->GetItemKey();

		const netCatalogBaseItem* const* ppItem = m_catalog.Access(itemId);
		gnetAssertf(ppItem, "netCatalog::AdjustStat: Trying to adjust stat for an item %s that doesnt exist.", itemId.TryGetCStr());

		if (ppItem)
		{
			const netCatalogBaseItem* pItem = *ppItem;

			gnetAssertf(pItem, "netCatalog::SetStat: Trying to set stat for an item %s that doesnt exist", itemId.TryGetCStr());

			if(pItem)
			{
				gnetDebug1("Set Inventory: key='%s', amount='%ld'", itemId.TryGetCStr(), item->GetAmount());

				pItem->SetStatValue( item );
				return true;
			}
		}
	}

	return false;
}

//PURPOSE: Iterator fro catalog items
netCatalogMap::ConstIterator netCatalog::GetCatalogIterator( ) const
{
	return m_catalog.CreateIterator();
}

//PURPOSE: Accessor for a catalog item
const netCatalogBaseItem* netCatalog::Find(atHashString& key) const
{
	netCatalogBaseItem*const* ppItem = m_catalog.Access(key);
	return ppItem ? *ppItem : NULL;
}

//PURPOSE: Reset all data and prepare to add new json file.
void  netCatalog::Clear( )
{
	CancelSaveWorker();

	atMap< atHashString, netCatalogBaseItem* >::Iterator it = m_catalog.CreateIterator();
	for (it.Start(); !it.AtEnd(); it.Next())
	{
		netCatalogBaseItem** item = it.GetDataPtr();
		if (item)
			delete *item;
	}

	m_catalog.Kill( );
	m_packedExceptions.Kill();
}

void netCatalog::RequestServerFile()
{
	if (NetworkInterface::IsGameInProgress() && GetVersion() > 0)
	{
		m_catalogWasSuccessful = m_UpdateStatus.Succeeded();

		int tunevalue = 0;
		if(!Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("MAX_CATALOG_FAILURES", 0xe084c459), tunevalue))
		{
			m_catalogMaxFailures = MAX_CATALOG_FAILURES;
		}
		else
		{
			m_catalogMaxFailures = (u32)tunevalue;
		}
	}

	m_UpdateStatus.Reset();
	NetworkGameTransactions::GetCatalog(NetworkInterface::GetActiveGamerInfo()->GetLocalIndex(), &netCatalog::GetInstance().GetStatus());
}

void netCatalog::CatalogDownloadDone()
{
	gnetAssert(!m_UpdateStatus.Pending());

	//Deal with catalog download stuff.
	if (m_UpdateStatus.Failed() && m_catalogWasSuccessful)
	{
		m_catalogFailedTimes += 1;

		gnetError("Catalog ::CatalogDownloadDone() - FAILED - m_catalogFailedTimes='%u', m_catalogMaxFailures='%u'.", m_catalogFailedTimes, m_catalogMaxFailures);

		if (m_catalogWasSuccessful && m_catalogFailedTimes < m_catalogMaxFailures)
		{
			//back to being Succeeded
			m_UpdateStatus.Reset();
			m_UpdateStatus.ForceSucceeded();
			gnetError("Catalog ::CatalogDownloadDone() - FAILED - Forcing Successfull.");
		}
		else
		{
			m_catalogFailedTimes = 0;
		}
	}
	else 
	{
		m_catalogFailedTimes = 0;
	}

	m_catalogWasSuccessful = false;
}

#if __BANK
void netCatalog::CheckPackedStatsEndvalue(int NET_ASSERTS_ONLY(value))
{
	gnetDebug1("Script and Code packed stats indices : Script %d - Code %d", value, END_LAST_ENUM_VALUE_ON_SCRIPT);
	gnetAssertf(value == END_LAST_ENUM_VALUE_ON_SCRIPT, "Script and Code packed stats indices are out of sync - Script %d != Code %d", value, END_LAST_ENUM_VALUE_ON_SCRIPT);
}
#endif // __BANK

//PURPOSE: Returns total number of items in the catalog.
u32  netCatalog::GetNumItems( ) const
{
	return ((u32)m_catalog.GetNumUsed());
}

//PURPOSE: Add items to out catalog database.
bool netCatalog::AddItem(const atHashString& key, netCatalogBaseItem* item)
{
	if (gnetVerify(item))
	{
		if (sm_CheckForDuplicatesOnAddingItems)
		{
			if (m_catalog.Access(key) != nullptr)
			{
				NOTFINAL_ONLY(gnetAssertf(false, "Catalog ::AddItem() - FAILED - Key='%s:%d' already exists", item->GetItemKeyName(), key.GetHash());)
				gnetError("Catalog ::AddItem() - FAILED - Key='%s:%d' already exists", item->GetItemKeyName(), key.GetHash());
				return false;
			}
		}

		if (item->SetupInventoryStats())
		{
			m_catalog.Insert(key, item);
			gnetDebug3("Catalog ::AddItem() - key='%s:%d', price='%d', category='%s'.", item->GetItemKeyName(), key.GetHash(), item->GetPrice(), item->GetCategoryName());
			return true;
		}
		else
		{
			gnetWarning("Catalog ::AddItem() - FAILED - Key='%s:%d' is not a VALID catalog item.", item->GetItemKeyName(), key.GetHash());
			NOTFINAL_ONLY(gnetAssertf(false, "Key='%s:%d' is not a VALID catalog item.", item->GetItemKeyName(), key.GetHash());)
		}
	}
	else
	{
		gnetError("Catalog ::AddItem() - FAILED - NULL Item - key='%d'.", key.GetHash());
	}

	return false;
}

//PURPOSE: Remove items to out catalog database.
bool netCatalog::RemoveItem(atHashString& key)
{
	return m_catalog.Delete( key );
}

//PURPOSE: return the crc value of an item.
u32 netCatalog::GetItemCRC(netCatalogBaseItem* item)
{
	u32 crcvalue = 0;

	if (gnetVerify(item))
	{
		const u32 keyvalue = item->GetKeyHash().GetHash();
		crcvalue += atDataHash(&keyvalue, sizeof(u32), 0);

		const u32 pricevalue = item->GetPrice();
		crcvalue += atDataHash(&pricevalue, sizeof(u32), 0);
	}

	return crcvalue;
}

//PURPOSE: Load catalog from cache file.
void netCatalog::OnCacheFileLoaded(CacheCatalogParsableInfo& catalog)
{
	gnetAssert(m_version == 0);
	if (m_version > 0)
		return;

	//start parsing
	Clear( );

	gnetDebug1("netCatalogCache::OnCacheFileLoaded - m_Version='%d'.", catalog.m_Version);
	gnetDebug1("netCatalogCache::OnCacheFileLoaded - m_CRC='%u'.", catalog.m_CRC);
	gnetDebug1("netCatalogCache::OnCacheFileLoaded - m_LatestServerVersion='%d'.", catalog.m_LatestServerVersion);
	gnetDebug1("netCatalog::OnCacheFileLoaded - total m_Items1='%d'.", catalog.m_Items1.GetCount());
	gnetDebug1("netCatalog::OnCacheFileLoaded - total m_Items2='%d'.", catalog.m_Items2.GetCount());
	OUTPUT_ONLY(const unsigned startTime = sysTimer::GetSystemMsTime();)

	//crc calculated
	u32 crcvalue = 0;

	u32 numberOfItems = 0;
	
	atArray< netCatalogBaseItem* > listOfOrphanItems;
	for (int i = 0; i < catalog.m_Items1.GetCount(); i++)
	{
		netCatalogBaseItem* pItem = catalog.m_Items1[i];
		if (pItem)
		{
			const bool itemAdded = netCatalog::GetInstance().AddItem(pItem->GetKeyHash(), pItem);
			if (itemAdded)
			{
				crcvalue += netCatalog::GetItemCRC(pItem);
				++numberOfItems;
			}
			else
			{
				listOfOrphanItems.PushAndGrow(pItem);
			}
		}
	}
	for (int i = 0; i < catalog.m_Items2.GetCount(); i++)
	{
		netCatalogBaseItem* pItem = catalog.m_Items2[i];
		if (pItem)
		{
			const bool itemAdded = netCatalog::GetInstance().AddItem(pItem->GetKeyHash(), pItem);
			if (itemAdded)
			{
				crcvalue += netCatalog::GetItemCRC(pItem);
				++numberOfItems;
			}
			else
			{
				listOfOrphanItems.PushAndGrow(pItem);
			}
		}
	}
	//Pointers are owned by the main catalog now or added to listOfOrphanItems.
	catalog.m_Items1.Reset(false);
	catalog.m_Items2.Reset(false);

	//Free memory for all orphan items
	gnetAssertf(listOfOrphanItems.empty(), "netCatalog::OnCacheFileLoaded - total number of ophan items='%d'", listOfOrphanItems.GetCount());
	for (int i = 0; i < listOfOrphanItems.GetCount(); i++)
	{
		delete listOfOrphanItems[i];
	}
	listOfOrphanItems.Reset();

	//Finish up and make sure crc matches.
	if (catalog.m_CRC == crcvalue)
	{
		GameTransactionSessionMgr::Get().UnFlagCatalogForRefresh();
		gnetDebug1("UnFlag Catalog Refresh - Successfull LOAD from the catalog cache, numberOfItems='%d'", numberOfItems);

		m_version             = catalog.m_Version;
		m_crc                 = catalog.m_CRC;
		m_latestServerVersion = catalog.m_LatestServerVersion;

		gnetDebug1("Catalog cache, numberOfItems='%d', version='%u', crc='%u'", numberOfItems, catalog.m_Version, crcvalue);

		m_ReadStatus.Reset();
		m_ReadStatus.ForceSucceeded();

		CalculatePackedExceptions();
	}
	else
	{
		GameTransactionSessionMgr::Get().FlagCatalogForRefresh();
		gnetDebug1("Flag Catalog Refresh - Failed to load from the catalog cache, numberOfItems='%d'.", numberOfItems);
		Clear();
	}

	gnetDebug1("netCatalog::OnCacheFileLoaded - time: %u ms", sysTimer::GetSystemMsTime() - startTime);
}

//PURPOSE: Load catalog from cache file.
void  netCatalog::LoadFromCache()
{
	//Load catalog from cache.
	if (CloudCache::IsCacheEnabled() && !NETWORK_SHOPPING_MGR.ShouldDoNullTransaction() && !PARAM_nocatalogcache.Get())
	{
		m_catalogCache.Load();
	}
}
//PURPOSE: Write catalog from cache file.
void  netCatalog::WriteToCache()
{
	bool allowedToSaveCatalogCache = true;
	if (Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("CATALOG_CACHE_WRITE_FIRST_TRANSITION", 0x0e2ac7b9), allowedToSaveCatalogCache))
	{
#if !__FINAL
		//Param that forces using the catalog cache and bypasses the existence of tunable 'CATALOG_CACHE_WRITE_FIRST_TRANSITION'
		if (PARAM_forceCatalogCacheSave.Get() && !PARAM_nocatalogcache.Get())
		{
			allowedToSaveCatalogCache = true;
		}
		else
#endif //!__FINAL
		{
			allowedToSaveCatalogCache = false;
		}
	}

	GameTransactionSessionMgr& rSession = GameTransactionSessionMgr::Get();

	gnetDebug1("netCatalog::WriteToCache() :: allowedToSaveCatalogCache='%s', IsAnySessionActive='%s'.", allowedToSaveCatalogCache ? "true" : "false", rSession.IsAnySessionActive() ? "true" : "false");

	if (allowedToSaveCatalogCache || !rSession.IsAnySessionActive())
	{
		int writeToCacheCount = 0;
		if (Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("CATALOG_CACHE_WRITE_COUNT", 0x6c117b0b), writeToCacheCount))
		{
#if !__FINAL
			//Param that forces using the catalog cache and bypasses the existence of tunable 'CATALOG_CACHE_WRITE_FIRST_TRANSITION'
			if (PARAM_forceCatalogCacheSave.Get() && !PARAM_nocatalogcache.Get())
			{
				allowedToSaveCatalogCache = true;
			}
			else
#endif //!__FINAL
			{
				allowedToSaveCatalogCache = false;
			}
		}

		//Load catalog from cache.
		if (CloudCache::IsCacheEnabled() && !NETWORK_SHOPPING_MGR.ShouldDoNullTransaction())
		{
			if (allowedToSaveCatalogCache || (m_writeToCacheCount < (u32)writeToCacheCount))
			{
				m_writeToCacheCount += 1;
				m_catalogCache.Save();
			}
		}
	}
}

void netCatalogDownloadWorker::DoWork()
{
	bool result = false;

#if !__NO_OUTPUT
	unsigned curTime = sysTimer::GetSystemMsTime();
#endif

#if __USE_ZLIB_STREAM

	netCatalog& catalog = netCatalog::GetInstance();
	m_bufferSize = m_dataSize*BUFFER_SIZE_MULTIPLIER;

	int multiplier = 0;
	const bool tuneExists = Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("BUFFER_SIZE_MULTIPLIER", 0xf644352a), multiplier);
	if (tuneExists)
	{
		m_bufferSize = (u32)m_dataSize*multiplier;
	}

	int maxattempts = 2;
	Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("BUFFER_MAX_ATTEMPTS", 0xa66f9826), maxattempts);

	unsigned totalConsumed = 0;
	unsigned totalProduced = 0; //skip the mode byte.
	unsigned numConsumed = 0;
	unsigned numProduced = 0;

	int attempt = 0;
	while (attempt < maxattempts)
	{
		u8* uncompressed = TryAllocateMemory(m_bufferSize);
		if (!gnetVerify(uncompressed))
			return;

		sysMemSet(uncompressed, 0, m_bufferSize);

		static const int ZLIB_HEAP_SIZE = 10 * 1024; //Zlib needs a crap ton of memory to do it's work.
		u8 zlibHeap[ZLIB_HEAP_SIZE];

		sysMemSimpleAllocator allocator(zlibHeap, COUNTOF(zlibHeap), sysMemSimpleAllocator::HEAP_APP_2);

		zlibStream zstrm;
		zstrm.BeginDecode(&allocator, m_isAligned ? 16+MAX_WBITS : 0);

		totalConsumed = 0;
		totalProduced = 0; //skip the mode byte.
		numConsumed = 0;
		numProduced = 0;

		u8* source = (u8*)m_data;

		while(totalConsumed < m_dataSize && totalProduced < m_bufferSize && !zstrm.HasError() && !WasCanceled())
		{
			zstrm.Decode(&source[totalConsumed]
						,m_dataSize - totalConsumed
						,&uncompressed[totalProduced]
						,m_bufferSize - totalProduced
						,&numConsumed
						,&numProduced);

			totalConsumed += numConsumed;
			totalProduced += numProduced;
		}

#if !__NO_OUTPUT
		gnetDebug1("Catalog :: zlib uncompress time is '%dms'.", (sysTimer::GetSystemMsTime() | 0x01) - curTime);
		curTime = sysTimer::GetSystemMsTime();
#endif //

		gnetAssertf(totalProduced <= m_bufferSize, "Destination buffer size='%d' is to small for size='%d'.", m_bufferSize, totalProduced);
		gnetAssertf(!zstrm.HasError(), "Error's found on inflating cloud file. deflated size=%d", m_dataSize);

		if (!zstrm.HasError() && totalProduced <= m_bufferSize && !WasCanceled())
		{
			u32 crc = 0;
			result = catalog.LoadFromJSON(uncompressed, totalProduced, crc);
		}

		TryDeAllocateMemory(uncompressed);

		//Up number of attempts
		attempt += 1;

		//We havent managed to consume the full compressed buffer 
		//  JSON is invalid.
		if(!result && totalConsumed < m_dataSize)
		{
			m_bufferSize += 1024*1024;

			//We are done
			if (attempt == maxattempts)
			{
				gnetDebug1("Catalog :: JSON is invalid - m_bufferSize='%u', totalConsumed='%u', m_dataSize='%u'.", m_bufferSize, totalConsumed, m_dataSize);

				gnetDebug1("HeartBeatGameTransactionHttpTask::CheckFailure - FAILURE TO COMMUNICATE WITH SERVER.");
				GameTransactionSessionMgr::Get().Bail(GameTransactionSessionMgr::FAILED_CATALOG, BAIL_CATALOG_BUFFER_TOO_SMALL, BAIL_CTX_NONE, true);

				catalog.ForceFailed();
			}
		}
		else
		{
			break;
		}
	}

#else //__USE_ZLIB_STREAM

	if (!WasCanceled())
	{
		result = LoadFromJSON(pData, nDataSize, catalog.GetCRC());
	}

#endif // __USE_ZLIB_STREAM

#if !__NO_OUTPUT
	gnetDebug1("Catalog :: cloud catalog load from json time is '%dms'.", (sysTimer::GetSystemMsTime() | 0x01) - curTime);
#endif //!__NO_OUTPUT

	gnetDebug1("Catalog :: Loaded cloud catalog. Generated CRC [%dx]", catalog.GetCRC());
	
	gnetDebug1("Catalog :: DataSize: %u, BufferSize: %u, TotalConsumed: %u, TotalProduced: %u", m_dataSize, m_bufferSize, totalConsumed, totalProduced);

	gnetDebug1("Catalog :: Load result [%s]", result ? "Success" : "Failed");

	if (result && !WasCanceled())
	{
		catalog.ForceSucceeded();

		//Update catalog cache.
		if (CloudCache::IsCacheEnabled())
		{
			catalog.WriteToCache();
		}
	}

	TryDeAllocateMemory(m_data);
	m_data      = 0;
	m_dataSize  = 0;
	m_isAligned = false;
}

void netCatalogDownloadWorker::OnFinished()
{
	gnetDebug1("Catalog :: worker state [%s]", Finished() ? "Finished" : ( Pending() ? "Pending" : "None"));
}

netCatalogDownloadWorker::netCatalogDownloadWorker() 
	: m_bufferSize(0)
{
}

bool  netCatalogDownloadWorker::Configure(const void* pData, unsigned nDataSize, bool isAligned)
{
	if (m_data)
		return false;

	m_dataSize  = nDataSize;
	m_isAligned = isAligned;
	m_data      = TryAllocateMemory(m_dataSize, isAligned ? 16+MAX_WBITS : 0);

	if (m_data)
	{
		gnetDebug1("netCatalogDownloadWorker - Received download buffer size='%u'", nDataSize);

		sysMemCpy(m_data, pData, m_dataSize);
		return true;
	}

	return false;
}

bool netCatalog::OnCatalogDownload(const void* pData, unsigned nDataSize, bool isAligned)
{
	bool success = false;

	rtry
	{
		rverify(!m_downloadWorker.Pending()
			,catchall
			,gnetError("OnCatalogDownload - m_downloadWorker is Active."));

		rverify(m_downloadWorker.Configure(pData, nDataSize, isAligned)
			,catchall
			,gnetError("OnCatalogDownload - FAILED Configure."));

		rverify(s_catalogThreadPool.QueueWork(&m_downloadWorker)
			,catchall
			,gnetError("OnCatalogDownload - Failed to QueueWork."));

		gnetDebug1("OnCatalogDownload - m_downloadWorker QueueWork.");

		success = true;
	}
	rcatchall
	{
	}

	return success;

}

bool netCatalog::LoadFromJSON(const void* pData, unsigned nDataSize, unsigned& crcvalue)
{
	OUTPUT_ONLY(const unsigned startTime = sysTimer::GetSystemMsTime();)

	//validate data
	bool success = RsonReader::ValidateJson(static_cast<const char*>(pData), nDataSize);
	if(!gnetVerifyf(success, "Catalog :: JSON data invalid! %s", pData))
	{

#if !__NO_OUTPUT
		//Log to file
		fiStream* pStream(ASSET.Create("Catalog_ValidationFail.txt", ""));
		if(pStream)
		{
			pStream->Write(pData, nDataSize);
			pStream->Close();
		}
#endif // !__NO_OUTPUT

		return false;
	}

	//Initialize reader
	RsonReader tReader;
	success = tReader.Init(static_cast<const char*>(pData), 0, nDataSize);
	if(!gnetVerifyf(success, "Catalog :: Cannot initialise JSON reader!"))
	{
		return false;
	}

	success = LoadFromJSON(tReader, crcvalue);

	gnetDebug1("LoadFromJSON - size: %u bytes, time: %u ms", nDataSize, sysTimer::GetSystemMsTime() - startTime);

	return success;
}

bool netCatalog::LoadFromJSON(const rage::RsonReader& rr, unsigned& crcvalue)
{
	//Grab the version
	u32 version = 0;
	RsonReader tVersion;
	gnetVerifyf(rr.GetMember("version", &tVersion) && tVersion.AsUns(version), "Catalog :: No version data!");

	m_version             = version;
	m_latestServerVersion = version;

	u32 catalogCrc = 0;
	RsonReader tCRC;
	gnetVerifyf(rr.GetMember("catalogCrc", &tCRC) && tCRC.AsUns(catalogCrc), "Catalog :: No catalogCrc data!");

	m_crc = catalogCrc;

	//Load all items from json file.
	LoadItems(rr, crcvalue);
	gnetAssertf(m_crc == crcvalue, "netCatalog::LoadFromJSON() - Calculated crc=%u != download crc=%u", crcvalue, m_crc);

	gnetDebug1("netCatalog::LoadFromJSON() - Catalog version=%u", m_version);
	gnetDebug1("netCatalog::LoadFromJSON() - Calculated crc=%u != download crc=%u", crcvalue, m_crc);

	return true; 
}

u32 netCatalog::GetRandomItem() const
{
	if (0 == m_catalog.GetNumUsed() || 0 == m_catalog.GetNumSlots())
	{
		return 0;
	}

	const int item = fwRandom::GetRandomNumberInRange(0, m_catalog.GetNumUsed());

	const atMap< atHashString, netCatalogBaseItem* >::Entry* entry = m_catalog.GetEntry(item);

	if (entry)
	{
		return entry->data->GetKeyHash().GetHash();
	}

	return 0;
}


bool netCatalog::LoadItems(const rage::RsonReader& rr, unsigned& crcvalue)
{
	bool success = false;
	RsonReader tCatalog = rr;

	success |= tCatalog.GetMember("items", &tCatalog);

	if(!gnetVerifyf(success, "Catalog :: Failed to get member 'catalog'."))
	{
		return false;
	}

	char buffer[CATALOG_MAX_KEY_LABEL_SIZE];

	//start parsing
	netCatalog::GetInstance().Clear( );
	int numItems = 0;

	//grab contexts
	RsonReader catalogItem;
	for(success = tCatalog.GetFirstMember(&catalogItem); success; success = catalogItem.GetNextSibling(&catalogItem))
	{
		//See what rank it is.
		RsonReader itemreader;
		RsonReader paramValue;

		numItems++;

		buffer[0] = '\0';

		atArray< atHashString > catagories;
		if(!gnetVerify(netCatalogBaseItem::LoadHashArrayParam(catalogItem, "category", catagories)) || !gnetVerify(!catagories.empty()))
		{
			continue;
		}

		gnetAssert(catagories.size() == 1);
		atHashString categoryHash = catagories[0];

		netCatalogBaseItem* newcatalogitem = NULL;

		switch ((int)categoryHash.GetHash())
		{
		case CATEGORY_SERVICE:
			newcatalogitem = rage_new netCatalogServiceItem(); 
			break;

		case CATEGORY_SERVICE_WITH_THRESHOLD:
		case CATEGORY_EARN_CURRENCY:
			newcatalogitem = rage_new netCatalogServiceWithThresholdItem(); 
			break;

		case CATEGORY_SERVICE_WITH_LIMIT:
			newcatalogitem = rage_new netCatalogServiceLimitedItem(); 
			break;

		case CATEGORY_INVENTORY_VEHICLE_MOD:  
		case CATEGORY_INVENTORY_BEARD:
		case CATEGORY_INVENTORY_MKUP:
		case CATEGORY_INVENTORY_EYEBROWS:
		case CATEGORY_INVENTORY_CHEST_HAIR:
		case CATEGORY_INVENTORY_FACEPAINT:
		case CATEGORY_INVENTORY_BLUSHER:
		case CATEGORY_INVENTORY_LIPSTICK:
		case CATEGORY_UNLOCK:
			newcatalogitem = rage_new netCatalogPackedStatInventoryItem();
			break;

		case CATEGORY_WEAPON_AMMO:
		case CATEGORY_MART:
		case CATEGORY_PRICE_MODIFIER:
		case CATEGORY_PRICE_OVERRIDE:
		case CATEGORY_SYSTEM:
		case CATEGORY_VEHICLE_UPGRADE:
		case CATEGORY_CONTRABAND_FLAGS:
		case CATEGORY_CASINO_CHIP_REASON:
		case CATEGORY_CURRENCY_TYPE:
			newcatalogitem = rage_new netCatalogOnlyItem();
			break;

		case CATEGORY_EYEBROWS:
		case CATEGORY_CHEST_HAIR:
		case CATEGORY_HAIR:
		case CATEGORY_VEHICLE_MOD:
		case CATEGORY_VEHICLE:
		case CATEGORY_PROPERTIE: 
		case CATEGORY_BEARD: 
		case CATEGORY_MKUP:
		case CATEGORY_CONTACTS:
		case CATEGORY_FACEPAINT:
		case CATEGORY_BLUSHER:
		case CATEGORY_LIPSTICK:
		case CATEGORY_PROPERTY_INTERIOR:
		case CATEGORY_WAREHOUSE:
		case CATEGORY_CONTRABAND_MISSION: 
		case CATEGORY_WAREHOUSE_INTERIOR:
        case CATEGORY_INVENTORY_PRICE_PAID:
			newcatalogitem = rage_new netCatalogOnlyItemWithStat();
			break;

		case CATEGORY_INVENTORY_ITEM:
		case CATEGORY_INVENTORY_HAIR:
		case CATEGORY_INVENTORY_VEHICLE:
		case CATEGORY_INVENTORY_PROPERTIE:
		case CATEGORY_INVENTORY_CONTACTS:
		case CATEGORY_INVENTORY_PROPERTY_INTERIOR:
		case CATEGORY_INVENTORY_WAREHOUSE:
		case CATEGORY_INVENTORY_CONTRABAND_MISSION:
		case CATEGORY_INVENTORY_WAREHOUSE_INTERIOR:
		case CATEGORY_INVENTORY_CURRENCY:
			newcatalogitem = rage_new netCatalogInventoryItem(); 
			break;

		//case CATEGORY_SERVICE_UNLOCKED:
		//case CATEGORY_CLOTH:
		//case CATEGORY_WEAPON:
		//case CATEGORY_TATTOO:
		//case CATEGORY_CONTRABAND_QNTY:
		//case CATEGORY_WAREHOUSE_VEHICLE_INDEX:
		//case CATEGORY_CASINO_CHIPS:
		//case CATEGORY_DECORATION:
		//case CATEGORY_DATA_STORAGE:
		default:
			newcatalogitem = rage_new netCatalogGeneralItem();
			break;
		}

		if(newcatalogitem)
		{
			const bool failedToDeserialize = !newcatalogitem->Deserialize(catalogItem);
			const bool failedToAddItem = (failedToDeserialize || !gnetVerify(netCatalog::GetInstance().AddItem(newcatalogitem->GetKeyHash(), newcatalogitem)));
			if(failedToDeserialize || failedToAddItem)
			{
				gnetError("Failed to %s - key='%s', Reader:'%s'.", failedToDeserialize ? "Deserialize" : "AddItem", newcatalogitem->GetItemKeyName(), catalogItem.GetBuffer());
				delete newcatalogitem;
			}
			else
			{
				const u32 keyvalue = newcatalogitem->GetKeyHash().GetHash();
				crcvalue += atDataHash(&keyvalue, sizeof(u32), 0);
				const u32 pricevalue = newcatalogitem->GetPrice();
				crcvalue += atDataHash(&pricevalue, sizeof(u32), 0);

				newcatalogitem->SetCatagory(categoryHash);
			}
		}
	}

	CalculatePackedExceptions( );

	return true;
}

void netCatalog::CalculatePackedExceptions( )
{
	m_packedExceptions.Kill();

	atMap< int, sPackedStatsScriptExceptions > tempExceptions;

	netCatalogMap::ConstIterator entry = GetCatalogIterator();
	for (entry.Start(); !entry.AtEnd(); entry.Next())
	{
		netCatalogBaseItem* item = entry.GetData();
		if (item)
		{
			switch (item->GetCategory())
			{
			//Fix B*2403992 - Beer hats gifted for playlist completion are not awarded on PC.
			//I am adding a stupid hack because its the only exception needed and we need to keep these numbers down, 
			//also this code is to dump in the future so we can do something ugly as this.
			case CATEGORY_TATTOO:
				{
					const netCatalogGeneralItem* pitem = static_cast<const netCatalogGeneralItem*>(item);

					if (gnetVerify(pitem))
					{
						const int shift = pitem->GetBitShift();

						if (pitem->IsCharacterStat())
						{
							for (int i=0; i<MAX_NUM_MP_CHARS; i++)
							{
								StatId statkey(pitem->GetHash(i));

								static StatId MP0_TUPSTAT_BOOL7("MP0_TUPSTAT_BOOL7");
								static StatId MP1_TUPSTAT_BOOL7("MP1_TUPSTAT_BOOL7");
								static StatId MP2_TUPSTAT_BOOL7("MP2_TUPSTAT_BOOL7");
								static StatId MP3_TUPSTAT_BOOL7("MP3_TUPSTAT_BOOL7");

								if (statkey.GetHash() == MP0_TUPSTAT_BOOL7
									|| statkey.GetHash() == MP1_TUPSTAT_BOOL7
									|| statkey.GetHash() == MP2_TUPSTAT_BOOL7
									|| statkey.GetHash() == MP3_TUPSTAT_BOOL7)
								{
									sPackedStatsScriptExceptions* exc = tempExceptions.Access(pitem->GetHash(i));

									if (exc)
									{
										exc->Remove(shift);
									}
									else
									{
										sPackedStatsScriptExceptions statexc(true);
										statexc.Remove(shift);
										tempExceptions.Insert(statkey.GetHash(), statexc);
									}
								}
							}
						}
					}
				}
				break;

			case CATEGORY_INVENTORY_VEHICLE_MOD:
			case CATEGORY_INVENTORY_BEARD:
			case CATEGORY_INVENTORY_MKUP:
			case CATEGORY_INVENTORY_EYEBROWS:
			case CATEGORY_INVENTORY_CHEST_HAIR:
			case CATEGORY_INVENTORY_FACEPAINT:
			case CATEGORY_INVENTORY_BLUSHER:
			case CATEGORY_INVENTORY_LIPSTICK:
				{
					netCatalogPackedStatInventoryItem* pitem = static_cast<netCatalogPackedStatInventoryItem*>(item);
					if (gnetVerify(pitem))
					{
						int shift = 0;
						packed_stats::GetStatShiftValue(shift, pitem->GetStatEnum());

						if (packed_stats::GetIsCharacterStat(pitem->GetStatEnum()))
						{
							for (int i=0; i<MAX_NUM_MP_CHARS; i++)
							{
								StatId statkey;
								if (packed_stats::GetStatKey(statkey, pitem->GetStatEnum(), i) && gnetVerify(StatsInterface::IsKeyValid(statkey)))
								{
									sPackedStatsScriptExceptions* exc = tempExceptions.Access(statkey.GetHash());
									if (exc)
									{
										if (!gnetVerify(exc->Remove(shift)))
										{
											gnetError("Item='%s', Duplicate Stat key='%s', Enum='%d', Shift='%d'", pitem->GetItemKeyName(), StatsInterface::GetKeyName(statkey.GetHash()), pitem->GetStatEnum(), shift);
										}
									}
									else
									{
										sPackedStatsScriptExceptions statexc(packed_stats::GetIsBooleanType(pitem->GetStatEnum()));

										if (!gnetVerify(statexc.Remove(shift)))
										{
											gnetError("Item='%s', Shift value doesnt exist for Stat key='%s', Enum='%d', Shift='%d'", pitem->GetItemKeyName(), StatsInterface::GetKeyName(statkey.GetHash()), pitem->GetStatEnum(), shift);
										}

										tempExceptions.Insert(statkey.GetHash(), statexc);
									}
								}
							}
						}
						else
						{
							StatId statkey;
							if (packed_stats::GetStatKey(statkey, pitem->GetStatEnum(), -1) && gnetVerify(StatsInterface::IsKeyValid(statkey)))
							{
								sPackedStatsScriptExceptions* exc = tempExceptions.Access(statkey.GetHash());
								if (exc)
								{
									if (!gnetVerify(exc->Remove(shift)))
									{
										gnetError("Item='%s', Duplicate Stat key='%s', Enum='%d', Shift='%d'", pitem->GetItemKeyName(), StatsInterface::GetKeyName(statkey.GetHash()), pitem->GetStatEnum(), shift);
									}
								}
								else
								{
									sPackedStatsScriptExceptions statexc(packed_stats::GetIsBooleanType(pitem->GetStatEnum()));
									gnetVerify( statexc.Remove(shift) );
									tempExceptions.Insert(statkey.GetHash(), statexc);
								}
							}
						}
					}
				}
				break;

			default:
				break;
			}
		}
	}

	atMap< int, sPackedStatsScriptExceptions >::Iterator it = tempExceptions.CreateIterator();
	for (it.Start(); !it.AtEnd(); it.Next())
	{
		if ( it.GetKey() == 0 )
			continue;

		if (it.GetData().GetCount() > 0)
		{
			gnetDebug1("Exception added for Stat='%s',", StatsInterface::GetKeyName(it.GetKey()));
			OUTPUT_ONLY( it.GetData().SpewBitShifts() );

			m_packedExceptions.Insert(it.GetKey(), it.GetData());
		}
	}

	tempExceptions.Kill();
}

#if __BANK

//PURPOSE: Link 2 items so that when one is bought opens up access to the other.
bool netCatalog::LinkItems(atHashString& keyA, atHashString& keyB)
{
	netCatalogBaseItem** catalogitemA = m_catalog.Access( keyA );
	netCatalogBaseItem** catalogitemB = m_catalog.Access( keyB );

	if (!gnetVerify(catalogitemA) || !gnetVerify(catalogitemB) || !gnetVerify(*catalogitemA) || !gnetVerify(*catalogitemB))
		return false;

	return ((*catalogitemA)->Link(keyB));
}

//PURPOSE: Returns TRUE is 2 items are already linked.
bool netCatalog::AreItemsLinked(atHashString& keyA, atHashString& keyB) const
{
	netCatalogBaseItem*const* catalogitemA = m_catalog.Access( keyA );
	netCatalogBaseItem*const* catalogitemB = m_catalog.Access( keyB );

	if (!gnetVerify(catalogitemA) || !gnetVerify(catalogitemB) || !gnetVerify(*catalogitemA) || !gnetVerify(*catalogitemB))
		return false;

	return ((*catalogitemA)->IsLinked(keyB));
}

//PURPOSE: Change the text label for a catalog item.
bool netCatalog::ChangeItemTextLabel(atHashString& key, const char* label)
{
	bool result = false;

	netCatalogBaseItem** catalogitem = m_catalog.Access( key );

	if (!gnetVerify(catalogitem))
		return result;

	(*catalogitem)->SetLabel(label);

	result = true;

	return result;
}

//PURPOSE: Change the price for a catalog item.
bool netCatalog::ChangeItemPrice(atHashString& key, const int price)
{
	bool result = false;

	netCatalogBaseItem** catalogitem = m_catalog.Access( key );

	if (!gnetVerify(catalogitem))
		return result;

	(*catalogitem)->SetPrice(price);
	result = true;

	return result;
}

//PURPOSE: Remove the Link between 2 items.
bool netCatalog::RemoveLinkedItems(atHashString& keyA, atHashString& keyB)
{
	bool result = false;

	netCatalogBaseItem** catalogitemA = m_catalog.Access( keyA );
	netCatalogBaseItem** catalogitemB = m_catalog.Access( keyB );

	if (!gnetVerify(catalogitemA) || !gnetVerify(catalogitemB) || !gnetVerify(*catalogitemA) || !gnetVerify(*catalogitemB))
		return result;

	result = true;

	if (AreItemsLinked(keyA, keyB))
	{
		result = result && (*catalogitemA)->RemoveLink(keyB);
	}

	if (AreItemsLinked(keyB, keyA))
	{
		result = result && (*catalogitemB)->RemoveLink(keyA);
	}

	return result;
}

//PURPOSE: Link 2 items so that when item with keyA is sold will delete the reference keyB from the inventory.
bool netCatalog::LinkItemsForSelling(atHashString& keyA, atHashString& keyB)
{
	bool result = false;

	netCatalogBaseItem** catalogitemA = m_catalog.Access( keyA );
	netCatalogBaseItem** catalogitemB = m_catalog.Access( keyB );

	if (!gnetVerify(catalogitemA) || !gnetVerify(catalogitemB) || !gnetVerify(*catalogitemA) || !gnetVerify(*catalogitemB))
		return result;

	result = (*catalogitemA)->LinkForSelling(keyB);

	return result;
}

//PURPOSE: Returns TRUE is 2 items are already linked.
bool netCatalog::AreItemsLinkedForSelling(atHashString& keyA, atHashString& keyB) const
{
	bool result = false;

	netCatalogBaseItem*const* catalogitemA = m_catalog.Access( keyA );
	netCatalogBaseItem*const* catalogitemB = m_catalog.Access( keyB );

	if (!gnetVerify(catalogitemA) || !gnetVerify(catalogitemB) || !gnetVerify(*catalogitemA) || !gnetVerify(*catalogitemB))
		return result;

	result = (*catalogitemA)->IsLinkedForSelling(keyB);
	result = result && (*catalogitemB)->IsLinkedForSelling(keyA);

	return result;
}

//PURPOSE: Remove the Link between 2 items.
bool netCatalog::RemoveLinkedItemsForSelling(atHashString& keyA, atHashString& keyB)
{
	netCatalogBaseItem** catalogitemA = m_catalog.Access( keyA );
	netCatalogBaseItem** catalogitemB = m_catalog.Access( keyB );

	if (!gnetVerify(catalogitemA) || !gnetVerify(catalogitemB) || !gnetVerify(*catalogitemA) || !gnetVerify(*catalogitemB))
		return false;

	bool result = true;

	if (AreItemsLinked(keyA, keyB))
	{
		result =  result && (*catalogitemA)->RemoveLinkForSelling(keyB);
	}

	if (AreItemsLinked(keyB, keyA))
	{
		result = result && (*catalogitemB)->RemoveLinkForSelling(keyA);
	}

	return result;
}

#endif // __BANK

void netCatalog::SetLatestVersion(const u32 latestVersion, const u32 crc)
{
	m_latestServerVersion = latestVersion;

#if !__FINAL
	int catalogVersion = 0;
	if(!PARAM_catalogVersion.Get(catalogVersion))
		catalogVersion = 0;
#endif //!__FINAL

	//New version of the catalog
	if (m_latestServerVersion != m_version 
		|| m_crc != crc 
		NOTFINAL_ONLY(|| (catalogVersion > 0 && ((int)latestVersion) != catalogVersion)))
	{
		gnetDebug1("SetLatestVersion() - FlagCatalogForRefresh() - m_latestServerVersion(%u) != m_version(%u) || m_crc(%u) != crc(%u)", m_latestServerVersion, m_version, m_crc, crc);

#if !__FINAL
		if (catalogVersion > 0 && ((int)latestVersion) != catalogVersion)
			gnetDebug1("SetLatestVersion() - PARAM_catalogVersion='%d'.", catalogVersion);
#endif //!__FINAL

		m_crc = crc;
		GameTransactionSessionMgr::Get().FlagCatalogForRefresh();
	}
	//We still have the latest version.
	else if (m_latestServerVersion == m_version && m_version > 0 && m_crc == crc)
	{
		if (!m_ReadStatus.Succeeded())
			m_ReadStatus.ForceSucceeded();
	}
}

// --- netCatalogCache ------------------------------------------------------------------------------------

template<typename _Type> bool LoadDataObject(const char* name, psoStruct& root, _Type& obj)
{
	psoMember mem = root.GetMemberByName(atLiteralHashValue(name));
	if (!mem.IsValid())
	{
		return false;
	}

	psoStruct str = mem.GetSubStructure();
	if (!str.IsValid())
	{
		return false;
	}

	psoLoadObject(str, obj);

	return true;
}

template<typename _Type> void AddDataObject(psoBuilder& builder, psoBuilderInstance& parent, _Type* var, const char* name)
{
	// Now add the new object to the PSO file we're constructing
	psoBuilderInstance* childInst = psoAddParsableObject(builder, var);

	// Get the parent object's instance data, and add a new fixup so that the parent member will point to the child object
	// we just created
	parent.AddFixup(0, name, childInst);
}

u32  CompressData(u8* source, u32 sourceSize)
{
	int compSize = 0;
	u32 destSize = LZ4_compressBound(sourceSize)+netCatalogCache::HEADER_SIZE;

	u8* dest = TryAllocateMemory(destSize);
	if(dest)
	{
		compSize = LZ4_compress(reinterpret_cast<const char*>(source), reinterpret_cast<char*>(dest), sourceSize);

		if (0 < compSize)
		{
			sysMemSet(source, 0, sourceSize);
			memcpy(source, &sourceSize, netCatalogCache::HEADER_SIZE);
			memcpy(source+netCatalogCache::HEADER_SIZE, dest, compSize);

			TryDeAllocateMemory(dest);

			gnetAssertf(sourceSize < netCatalogCache::MAX_UMCOMPRESSED_SIZE, "Max size reached '%u', MAX_UMCOMPRESSED_SIZE='%u'", sourceSize, netCatalogCache::MAX_UMCOMPRESSED_SIZE);

			return (compSize+netCatalogCache::HEADER_SIZE);
		}

		TryDeAllocateMemory(dest);
	}

	gnetError("netCatalogCache::CompressData() FAILED");

	return 0;
}

bool  DecompressData(u8** source, const u32 OUTPUT_ONLY(sourcesize), u8* dest, const u32 destinationSize)
{
	int returnValue = LZ4_decompress_fast(reinterpret_cast<const char*>(*source), reinterpret_cast<char*>(dest), destinationSize);
	gnetAssert(static_cast<int>(sourcesize-netCatalogCache::HEADER_SIZE) == returnValue);

	if(returnValue <= 0)
	{
		TryDeAllocateMemory(dest);
		
		gnetError("DecompressData() - FAILED - LZ4_decompress_fast FAILED - returnValue='%d', sourcesize='%u'", returnValue, sourcesize);
		return false;
	}

	return true;
}

// --- netCatalogCacheSaveWorker ------------------------------------------------------------------------------------

void netCatalogCacheSaveWorker::DoWork( )
{
	psoBuilder builder;

	if(!WasCanceled())
	{
		CacheCatalogParsableInfo* catalog = rage_new CacheCatalogParsableInfo( );
		if (catalog)
		{
			if (!WasCanceled())
			{
				BuildPsoData(*catalog, builder);

				const u32 totalSize = builder.GetFileSize();
				gnetDebug1("netCatalogCache::Save - Total PSO Size='%u'", totalSize);

				if(!WasCanceled())
				{
					u8* buffer = TryAllocateMemory(totalSize);
					if (buffer)
					{
						//Finally get the fucking PSO buffer.
						if (gnetVerifyf(builder.SaveToBuffer(reinterpret_cast<char*>(buffer), totalSize), "netCatalogCache::Save( ) - builder.SaveToBuffer() - FAILED"))
						{
							//Add compression.
							u32 compressedSize = CompressData(buffer, totalSize);
							if (gnetVerifyf(0 < compressedSize, "netCatalogCache::Save( ) - CompressData() - FAILED"))
							{
								//Add Encryption.
								AES aes(AES_KEY_ID_SAVEGAME);
								if (gnetVerifyf(aes.Encrypt(buffer, compressedSize), "netCatalogCache::Save( ) - aes.Encrypt() - FAILED"))
								{
									if(!WasCanceled())
									{
										const u64 semantics = (u64)ATSTRINGHASH("catalog_cached_4", 0x8315344A);
										const u32 timestamp = (u32)rlGetPosixTime();

										OUTPUT_ONLY(CacheRequestID requestId =) CloudCache::AddCacheFile(semantics, timestamp, false, CachePartition::Default, buffer, compressedSize);

										gnetDebug1("netCatalogCache:: PSO File generated: result='%s', totalSize='%u', compressedSize='%u'"
																					,(requestId != INVALID_CACHE_REQUEST_ID) ? "true":"false"
																					,totalSize
																					,compressedSize);

										gnetAssertf((requestId != INVALID_CACHE_REQUEST_ID), "netCatalogCache:: PSO File generated: result='%s', totalSize='%u', compressedSize='%u'"
																					,(requestId != INVALID_CACHE_REQUEST_ID) ? "true":"false"
																					,totalSize
																					,compressedSize);
									}
								}
							}
						}

						TryDeAllocateMemory(buffer);
					}
				}
			}

			//Items are owned by the main catalog.
			catalog->m_Items1.Reset(false);
			catalog->m_Items2.Reset(false);

			delete catalog;
		}
	}
}

void  netCatalogCacheSaveWorker::BuildPsoData(CacheCatalogParsableInfo& catalog, psoBuilder& builder)
{
	netCatalog& catalogInst = netCatalog::GetInstance();

	catalog.m_Version             = static_cast<int>(catalogInst.GetVersion());
	catalog.m_CRC                 = static_cast<int>(catalogInst.GetCRC());
	catalog.m_LatestServerVersion = static_cast<int>(catalogInst.GetLatestServerVersion());

	const u32 MAX_ITEMS_PER_ARRAY = 60000;
	catalog.m_Items1.Reserve(MAX_ITEMS_PER_ARRAY);
	catalog.m_Items2.Reserve(MAX_ITEMS_PER_ARRAY);

	ASSERT_ONLY(u32 itemCount = 0;);
	netCatalogMap::ConstIterator entry = catalogInst.GetCatalogIterator();
	for (entry.Start(); !entry.AtEnd(); entry.Next())
	{
		netCatalogBaseItem* item = entry.GetData();
		if (item)
		{
			ASSERT_ONLY(itemCount += 1;);

			if (catalog.m_Items1.GetCount() < catalog.m_Items1.GetCapacity())
			{
				catalog.m_Items1.Push(item);
			}
			else if (catalog.m_Items2.GetCount() < catalog.m_Items2.GetCapacity())
			{
				catalog.m_Items2.Push(item);
			}
		}
	}

	ASSERT_ONLY(gnetAssertf(itemCount == (u32)(catalog.m_Items1.GetCount() + catalog.m_Items2.GetCount()),
		"Not enough space for all '%u' catalog items.", itemCount));

	gnetDebug1("netCatalogCache::BuildPsoData - m_Version='%d'.", catalog.m_Version);
	gnetDebug1("netCatalogCache::BuildPsoData - m_CRC='%u'.", catalog.m_CRC);
	gnetDebug1("netCatalogCache::BuildPsoData - m_LatestServerVersion='%d'.", catalog.m_LatestServerVersion);
	gnetDebug1("netCatalogCache::BuildPsoData - total num m_Items1='%d'.", catalog.m_Items1.GetCount());
	gnetDebug1("netCatalogCache::BuildPsoData - total num m_Items2='%d'.", catalog.m_Items2.GetCount());

	builder.IncludeChecksum();

	psoBuilderStructSchema& schema = builder.CreateStructSchema(atLiteralHashValue("netCatalog"));
	schema.AddMemberPointer(atLiteralHashValue("catalog_cached"), parMemberStructSubType::SUBTYPE_POINTER);

	schema.FinishBuilding();

	psoBuilderInstance& builderInst = builder.CreateStructInstances(schema);
	AddDataObject(builder, builderInst, &catalog, "catalog_cached");
	builder.SetRootObject(builderInst);

	builder.FinishBuilding();
};

// --- netCatalogCache ------------------------------------------------------------------------------------

sysThreadPool::Thread  netCatalogCache::s_catalogCacheThread;

netCatalogCache::netCatalogCache(CatalogCacheListener* listener) 
	: m_cachelistener(listener)
	, m_requestId(INVALID_CACHE_REQUEST_ID)
{
}

void netCatalogCache::Init()
{
	s_catalogThreadPool.AddThread(&s_catalogCacheThread, "CatalogCache", THREAD_CPU_ID, (32 * 1024) << __64BIT);
}

netCatalogCache::~netCatalogCache( ) 
{
	m_cachelistener = 0;
}

bool netCatalogCache::Load( )
{
	if (m_requestId == INVALID_CACHE_REQUEST_ID)
	{
		const u64 semantics = (u64)ATSTRINGHASH("catalog_cached_4", 0x8315344A);
		m_requestId = CloudCache::OpenCacheFile(semantics, CachePartition::Persistent);
		return (m_requestId != INVALID_CACHE_REQUEST_ID);
}

	return false;
};

bool netCatalogCache::Save()
{
	bool success = false;

	rtry
	{
		rverify(!m_saveWorker.Active()
			,catchall
			,gnetError("StartSaveWorkerThread - m_saveWorker is Active."));

		rverify(s_catalogThreadPool.QueueWork(&m_saveWorker)
			,catchall
			,gnetError("StartSaveWorkerThread - Failed to QueueWork."));

		success = true;
	}
	rcatchall
	{
	}

	return success;
}

void  netCatalogCache::CancelWork( )
{
	if (m_saveWorker.Pending())
	{
		s_catalogThreadPool.CancelWork(m_saveWorker.GetId());
	}
}

bool netCatalogCache::Pending() const
{
	return m_saveWorker.Pending();
}

void netCatalogCache::OnCacheFileLoaded(const CacheRequestID requestID, bool cacheLoaded)
{
	if (m_requestId == INVALID_CACHE_REQUEST_ID)
		return;

	if (requestID != m_requestId)
		return;

	if (!cacheLoaded)
	{
		m_requestId = INVALID_CACHE_REQUEST_ID;
		return;
	}

	sCacheHeader header;
	bool success = CloudCache::GetCacheHeader(requestID, &header);
	if(!gnetVerify(success))
	{
		gnetError("[%d]\t netCatalogCache::OnCacheFileLoaded :: Failed to open header file!", requestID);
	}
	else
	{
		const u32 totalsize = header.nSize;

		u8* data = TryAllocateMemory(totalsize);
		if (data)
		{
			//data[totalsize] = 0;

			if (gnetVerify(CloudCache::GetCacheData(requestID, data, totalsize)))
			{
				//Decrypt the buffer.
				AES aes(AES_KEY_ID_SAVEGAME);
				if (gnetVerifyf(aes.Decrypt(data, totalsize), "netCatalogCache::OnCacheFileLoaded() - aes.Decrypt - FAILED."))
				{
					u8* psodata = 0;
					u32 psoSize = 0;

					u8* lz4data = data + HEADER_SIZE;

					memcpy(&psoSize, data, HEADER_SIZE);
					gnetAssertf(psoSize < MAX_UMCOMPRESSED_SIZE, "Uncompressed size is too big '%u', max size is '%u'", psoSize, MAX_UMCOMPRESSED_SIZE);

					psodata = TryAllocateMemory(psoSize);
					gnetAssertf(psodata, "Failed to allocate dest - size is too big '%u', max size is '%u'", psoSize, MAX_UMCOMPRESSED_SIZE);

					if (psodata)
					{
						//Decompress the full buffer.
						if (gnetVerifyf(DecompressData(&lz4data, totalsize, psodata, psoSize), "netCatalogCache::OnCacheFileLoaded() - DecompressData - FAILED."))
						{
							psoFile* psofile = NULL;

							//Load the pso struct
							psoLoadFileStatus result;

							psofile = psoLoadFileFromBuffer(reinterpret_cast<char*>(psodata), psoSize, "netCatalog", PSOLOAD_PREP_FOR_PARSER_LOADING, false, atFixedBitSet32().Set(psoFile::IGNORE_CHECKSUM), &result);
							if (gnetVerifyf(result == PSOLOAD_SUCCESS, "netCatalogCache::OnCacheFileLoaded() - psoLoadFileFromBuffer - FAILED - result='%d'.", result))
							{
								psoStruct root = psofile->GetRootInstance();

								CacheCatalogParsableInfo* catalog = rage_new CacheCatalogParsableInfo( );
								if (catalog)
								{
									const bool result = LoadDataObject< CacheCatalogParsableInfo >("catalog_cached", root, *catalog);
									if (gnetVerifyf(result, "netCatalogCache::OnCacheFileLoaded() - LoadDataObject - FAILED."))
									{
										//listener callback
										if (gnetVerifyf(m_cachelistener, "netCatalogCache::OnCacheFileLoaded() - m_cachelistener - FAILED."))
										{
											m_cachelistener->OnCacheFileLoaded(*catalog);
										}
									}

									delete catalog;
								}

								delete psofile;
							}
							else
							{
								gnetError("netCatalogCache::OnCacheFileLoaded - psoLoadFileFromBuffer - FAILED - result='%d'.", result);
							}
						}

						TryDeAllocateMemory(psodata);
					}
				}
			}

			TryDeAllocateMemory(data);
		}
	}

	m_requestId = INVALID_CACHE_REQUEST_ID;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// netCatalogServerData
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool netCatalogServerData::RequestServerFile()
{
	if (gnetVerify(!netCatalog::GetInstance().GetStatus().Pending()))
	{
		netCatalog::GetInstance().RequestServerFile();
		return true;
	}

	return false;
}

bool netCatalogServerData::OnServerSuccess(const void* pData, const unsigned nDataSize)
{
	gnetAssert( netCatalog::GetInstance().GetStatus().Pending() );

	bool success = true;

	if(gnetVerify(pData) && gnetVerify(0 < nDataSize) && nDataSize < MAX_DATA_SIZE)
	{
		RsonReader tReader;
		if(gnetVerifyf(tReader.Init(static_cast<const char*>(pData), 0, nDataSize), "netCatalogServerData::OnServerSuccess - Cannot initialise JSON reader!"))
		{
			if(gnetVerifyf(tReader.HasMember("Error"), "netCatalogServerData::OnServerSuccess - Found a rar Json string that's not an error."))
			{
				success = false;
			}
		}
	}

	if(success)
	{
		gnetDebug1("netCatalogServerData::OnServerSuccess - OnCatalogDownload.");
		success = netCatalog::GetInstance().OnCatalogDownload(pData, nDataSize, false);
	}
	else if (netCatalog::GetInstance().HasReadSucceeded() && netCatalog::GetInstance().IsLatestVersion())
	{
		gnetDebug1("netCatalogServerData::OnServerSuccess - Avoid failure - we have a valid catalog already.");
		success = true;
	}

	return success;
}

void netCatalogServerData::Cancel()
{
	gnetDebug1(" netCatalogServerData::Cancel() ");

	netStatus* pStatus = &netCatalog::GetInstance().GetStatus();
	if(pStatus->Pending())
	{
		NetworkGameTransactions::Cancel(pStatus);
		pStatus->SetCanceled();
	}
}

#endif // __NET_SHOP_ACTIVE

//eof 
