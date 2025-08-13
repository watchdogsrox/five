//
// Catalog.h
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
// Stores the contents of the transaction server catalog
//

#ifndef INC_NET_CATALOG_H_
#define INC_NET_CATALOG_H_


#include "network/NetworkTypes.h"
#if __NET_SHOP_ACTIVE

// Rage headers
#include "atl/hashstring.h"
#include "atl/map.h"
#include "atl/string.h"
#include "net/status.h"
#include "parser/macros.h"
#include "atl/singleton.h"

#include "fwnet/netchannel.h"

// Game headers
#include "stats/StatsInterface.h"
#include "stats/StatsTypes.h"
#include "network/NetworkTypes.h"
#include "network/cloud/CloudManager.h"
#include "system/threadpool.h"

class netInventory;
class netInventoryBaseItem;

static const int CATALOG_MAX_NUM_OPEN_ITEMS      = 10;
static const int CATALOG_MAX_NUM_DELETE_ITEMS    = 10;
static const int CATALOG_MAX_KEY_LABEL_SIZE      = 128;

#define SHOP_CATEGORY_LIST \
	CATEGORY_ITEM(CATEGORY_CLOTH,0x492B4A93)\
	CATEGORY_ITEM(CATEGORY_WEAPON,0x92F0DBBF)   \
	CATEGORY_ITEM(CATEGORY_WEAPON_MOD,0x63A336E1)   \
	CATEGORY_ITEM(CATEGORY_MART,0xB2491B6E)   \
	CATEGORY_ITEM(CATEGORY_VEHICLE,0x497c6262)   \
	CATEGORY_ITEM(CATEGORY_VEHICLE_MOD,0x5930c2e0)   \
	CATEGORY_ITEM(CATEGORY_INVENTORY_VEHICLE_MOD,0x81855910)   \
	CATEGORY_ITEM(CATEGORY_PROPERTIE,0x196af398)   \
	CATEGORY_ITEM(CATEGORY_SERVICE,0xe62a3aa1)   \
	CATEGORY_ITEM(CATEGORY_SERVICE_WITH_THRESHOLD	,0x57de404e)   \
	CATEGORY_ITEM(CATEGORY_WEAPON_AMMO,0x3fa29128)   \
	CATEGORY_ITEM(CATEGORY_BEARD,0x70750496)   \
	CATEGORY_ITEM(CATEGORY_MKUP,0x982f68ea)   \
	CATEGORY_ITEM(CATEGORY_INVENTORY_ITEM,0xf2c77e1d)   \
	CATEGORY_ITEM(CATEGORY_INVENTORY_VEHICLE,0xb6fd233f)   \
	CATEGORY_ITEM(CATEGORY_INVENTORY_PROPERTIE,0x531cef2d)   \
	CATEGORY_ITEM(CATEGORY_INVENTORY_BEARD,0x85a48ae6)   \
	CATEGORY_ITEM(CATEGORY_INVENTORY_MKUP,0x65679d34)   \
	CATEGORY_ITEM(CATEGORY_HAIR,0x3fae47e2)   \
	CATEGORY_ITEM(CATEGORY_TATTOO,0x4e6baf79)   \
	CATEGORY_ITEM(CATEGORY_INVENTORY_HAIR,0x94eb09e5)   \
	CATEGORY_ITEM(CATEGORY_INVENTORY_EYEBROWS,0x8d08cfa8)   \
	CATEGORY_ITEM(CATEGORY_INVENTORY_CHEST_HAIR,0x22a9f688)   \
	CATEGORY_ITEM(CATEGORY_INVENTORY_CONTACTS,0x015faf63)   \
	CATEGORY_ITEM(CATEGORY_INVENTORY_FACEPAINT,0x7e349b56)   \
	CATEGORY_ITEM(CATEGORY_INVENTORY_BLUSHER,0xaddf33d9)   \
	CATEGORY_ITEM(CATEGORY_INVENTORY_LIPSTICK,0xe1eb447e)   \
	CATEGORY_ITEM(CATEGORY_CONTACTS,0xd6d190e7)   \
	CATEGORY_ITEM(CATEGORY_PRICE_MODIFIER,0x372897dd)   \
	CATEGORY_ITEM(CATEGORY_PRICE_OVERRIDE,0xa14400be)   \
	CATEGORY_ITEM(CATEGORY_SERVICE_UNLOCKED,0x92257108)\
	CATEGORY_ITEM(CATEGORY_EYEBROWS,0x2ef790d3)\
	CATEGORY_ITEM(CATEGORY_CHEST_HAIR,0x7c781f5e)\
	CATEGORY_ITEM(CATEGORY_FACEPAINT,0xc43b0e2e)\
	CATEGORY_ITEM(CATEGORY_BLUSHER,0x5bb5fc38)\
	CATEGORY_ITEM(CATEGORY_LIPSTICK,0x3e732aa5)\
	CATEGORY_ITEM(CATEGORY_SERVICE_WITH_LIMIT,0xbc5b83ba)\
	CATEGORY_ITEM(CATEGORY_SYSTEM,0xfbb57f65)\
	CATEGORY_ITEM(CATEGORY_VEHICLE_UPGRADE,0xeb1662d4)   \
	CATEGORY_ITEM(CATEGORY_INVENTORY_PROPERTY_INTERIOR,0x71a98dca)   \
	CATEGORY_ITEM(CATEGORY_PROPERTY_INTERIOR,0xee15ef9c)   \
	CATEGORY_ITEM(CATEGORY_INVENTORY_WAREHOUSE,0x9fa465a5)   \
	CATEGORY_ITEM(CATEGORY_WAREHOUSE,0x954c746d)   \
	CATEGORY_ITEM(CATEGORY_INVENTORY_CONTRABAND_MISSION,0x94bd7097) \
	CATEGORY_ITEM(CATEGORY_CONTRABAND_MISSION,0x100ea9c3) \
	CATEGORY_ITEM(CATEGORY_CONTRABAND_QNTY,0x0426e041) \
	CATEGORY_ITEM(CATEGORY_CONTRABAND_FLAGS,0xd3e58772) \
	CATEGORY_ITEM(CATEGORY_INVENTORY_WAREHOUSE_INTERIOR,0xf9ceeab6) \
	CATEGORY_ITEM(CATEGORY_WAREHOUSE_INTERIOR,0x7732fe80) \
	CATEGORY_ITEM(CATEGORY_WAREHOUSE_VEHICLE_INDEX,0xa6e56d90) \
    CATEGORY_ITEM(CATEGORY_INVENTORY_PRICE_PAID,0x7D8CAF5) \
	CATEGORY_ITEM(CATEGORY_BONUS,0xAE04310C) \
	CATEGORY_ITEM(CATEGORY_CASINO_CHIPS,0x7442b428) \
	CATEGORY_ITEM(CATEGORY_DECORATION,0xe45b956f) \
	CATEGORY_ITEM(CATEGORY_DATA_STORAGE,0x7092c1af) \
	CATEGORY_ITEM(CATEGORY_CASINO_CHIP_REASON,0x6E06ACCC)\
	CATEGORY_ITEM(CATEGORY_CURRENCY_TYPE,0x336094bf)\
	CATEGORY_ITEM(CATEGORY_INVENTORY_CURRENCY,0x983af4ce)\
	CATEGORY_ITEM(CATEGORY_EARN_CURRENCY,0x22b8f1cf)\
	CATEGORY_ITEM(CATEGORY_UNLOCK,0x35a571b3)\


enum eNetworkShopCategories
{
#define CATEGORY_ITEM(a,b) a = b,
	SHOP_CATEGORY_LIST
#undef CATEGORY_ITEM
};

//PURPOSE: Base Class for individual entry in the transaction server catalog.
class netCatalogBaseItem
{
	friend class netCatalogCloudDataFile;

public:
	netCatalogBaseItem(): m_price(0) {}
	netCatalogBaseItem(const int keyhash, const int category, const int price);
	netCatalogBaseItem(const netCatalogBaseItem& other) 
		: m_keyhash(other.m_keyhash)
		, m_category(other.m_category)
		, m_price(other.m_price)
#if __BANK
		, m_openitems(other.m_openitems)
		, m_deleteitems(other.m_deleteitems)
#endif
	{;}
	netCatalogBaseItem(const netCatalogBaseItem* other) 
		: m_keyhash(other->m_keyhash)
		, m_category(other->m_category)
		, m_price(other->m_price)
#if __BANK
		, m_openitems(other->m_openitems)
		, m_deleteitems(other->m_deleteitems)
#endif
	{;}

	virtual ~netCatalogBaseItem()
	{
#if __BANK
		m_openitems.clear();
		m_deleteitems.clear();
#endif
	}

	enum StorageType
	{
		INT,
		INT64,
		BITMASK
	};

	u32                  GetCategory( ) const {return m_category.GetHash();}
	int                     GetPrice( ) const {return m_price;}
	const atHashString&   GetKeyHash( ) const {return m_keyhash;}

	BANK_ONLY( void SetPrice(const int price) {m_price = price;} )

	OUTPUT_ONLY(const char* GetCategoryName() const { return m_category.TryGetCStr(); } );
	OUTPUT_ONLY(const char*  GetItemKeyName() const { return m_keyhash.TryGetCStr(); } );

	//PURPOSE: Link another catalog item.
	BANK_ONLY( bool        Link(atHashString& key); )
	BANK_ONLY( bool  RemoveLink(atHashString& key); )
	BANK_ONLY( bool    IsLinked(atHashString& key) const; )

	//PURPOSE: Link another catalog item for selling purpose.
	BANK_ONLY( bool        LinkForSelling(atHashString& key); )
	BANK_ONLY( bool  RemoveLinkForSelling(atHashString& key); )
	BANK_ONLY( bool    IsLinkedForSelling(atHashString& key) const; )

	//PURPOSE: Set a stat value in local stats.
	virtual void SetStatValue(const netInventoryBaseItem* item) const = 0;

	//PURPOSE: Common set stat value.
	static void OnSetStatValue(StatId& statkey, const s64 value);

	//PURPOSE: Return true if this catalog entry has a static stat value.
	virtual bool HasStatValue() const {return false;}

	//PURPOSE: Retrieve the static stat value.
	virtual int  GetStatValue() const {gnetAssert(0); return 0;}

	virtual bool Deserialize(const RsonReader& rr);

	void SetCatagory(atHashString  category);

	//PURPOSE: Return the valid Storage type.
	static int FindStorageType(atHashString type);


	static bool LoadHashArrayParam(const rage::RsonReader& rr, const char* pMemberName, atArray<atHashString>& param);

	template<typename T>
	static bool LoadParam(const RsonReader& rr, T& param)
	{
		RsonReader valuereader = rr;
		if(valuereader.HasMember("value"))
		{
			valuereader.GetMember("value", &valuereader);
		}

		if (!valuereader.AsValue(param))
			return false;

		return true;
	}

	const netCatalogBaseItem& operator=(const netCatalogBaseItem* other)
	{
		m_keyhash     = other->m_keyhash;
		m_category    = other->m_category;
		m_price       = other->m_price;
#if __BANK
		m_openitems   = other->m_openitems;
		m_deleteitems = other->m_deleteitems;
#endif
		return *this;
	}
	const netCatalogBaseItem& operator=(const netCatalogBaseItem& other)
	{
		netCatalogBaseItem::operator=(&other);
		return *this;
	}

	virtual bool             IsValid( ) const {return true;}
	virtual bool SetupInventoryStats( )       {return true;}

#if __BANK
	void  SetLabel(const char* textTag) {if(textTag) m_textTag.SetFromString(textTag);}
	void  SetName(const char* name) {gnetAssert(name); if(name) m_name = name;}
#endif //__BANK

	virtual StatId  GetStat() const = 0;

private:
	virtual u32     GetHash(int slot) const = 0;

#if __BANK

protected:
	//Text label for the catalog item.
	atHashString  m_textTag;

	//Name of the catalog item
	atString  m_name;

#endif //__BANK

private:
	//Item Category.
	atHashString  m_keyhash;

	//Item Category.
	atHashString  m_category;

	//Item price
	int  m_price;

#if __BANK

	//Array of item keys that this item also opens.
	atArray< atHashString > m_openitems;

	//Array of item keys that this item will delete if its sold.
	atArray< atHashString > m_deleteitems;

#endif //__BANK

	PAR_PARSABLE;
};

typedef   atMap< atHashString, netCatalogBaseItem* > netCatalogMap;
typedef atArray< const netCatalogBaseItem* > netCatalogItemList;

//PURPOSE: Catalog Entries for SERVICES.
class netCatalogServiceItem : public netCatalogBaseItem
{
	typedef netCatalogBaseItem Base;

public:
	netCatalogServiceItem() {}
	netCatalogServiceItem(const int keyhash, const int category, const int price) 
		: Base(keyhash, category, price) 
	{ }

	void     SetStatValue(const netInventoryBaseItem*) const {;}

private:
	StatId        GetStat(   ) const {return StatId();}
	u32           GetHash(int) const {return 0;}

	PAR_PARSABLE;
};

//PURPOSE: Catalog Entries for SERVICES with a threshold.
class netCatalogServiceWithThresholdItem : public netCatalogBaseItem
{
	typedef netCatalogBaseItem Base;

public:
	netCatalogServiceWithThresholdItem() {}
	netCatalogServiceWithThresholdItem(const int keyhash, const int category, const int price) 
		: Base(keyhash, category, price) 
	{ }

	void     SetStatValue(const netInventoryBaseItem*) const  {;}

private:
	StatId        GetStat(   ) const {return StatId();}
	u32           GetHash(int) const {return 0;}

	PAR_PARSABLE;
};

//PURPOSE: Catalog Entries for SERVICES with limited use.
class netCatalogServiceLimitedItem : public netCatalogBaseItem
{
	typedef netCatalogBaseItem Base;

public:
	netCatalogServiceLimitedItem() {}
	netCatalogServiceLimitedItem(const int keyhash, const int category, const int price) 
		: Base(keyhash, category, price) 
	{ }

	void     SetStatValue(const netInventoryBaseItem*) const  {;}

private:
	StatId        GetStat(   ) const {return StatId();}
	u32           GetHash(int) const {return 0;}

	PAR_PARSABLE;
};

//PURPOSE: Catalog Entries for items using packed stats.
class netCatalogPackedStatInventoryItem : public netCatalogBaseItem
{
	typedef netCatalogBaseItem Base;

public:
	netCatalogPackedStatInventoryItem();

#if !__FINAL
	netCatalogPackedStatInventoryItem(const int keyhash
										, const int category
										, const int price
										, const u32 statenumvalue);
#endif //!__FINAL

	//PURPOSE: Set a stat value in local stats.
	void     SetStatValue(const netInventoryBaseItem* item) const;

	virtual bool Deserialize(const RsonReader& rr);

	void PostLoad();

	const netCatalogPackedStatInventoryItem& operator=(const Base& other)
	{
		Base::operator=(other);
		return *this;
	}
	const netCatalogPackedStatInventoryItem& operator=(const netCatalogPackedStatInventoryItem& other)
	{
		Base::operator=(&other);

		m_statEnumValue = other.m_statEnumValue;

#if !__FINAL
		m_isPlayerStat = other.m_isPlayerStat;
		m_statName = other.m_statName;
		m_bitShift = other.m_bitShift;
		m_bitSize = other.m_bitSize;
#endif // !__FINAL

		return *this;
	}

	virtual bool             IsValid( ) const;
	virtual bool SetupInventoryStats( );
	virtual int    GetStatEnum(        ) const { return m_statEnumValue; }

private:
	virtual StatId     GetStat(        ) const;
	virtual u32        GetHash(int slot) const;
	
	//Setup stats mapping values from the stats enum value for catalog export.
	void SetupStatsMapping();

private:
	//Stat name.
	NOTFINAL_ONLY( atString m_statName; );

	//Enum value of the stat used to construct the stat name/hash using namespace catalog_vehicle_mod.
	int  m_statEnumValue;

	//Bit shift and number of bits when the stat is set.
	NOTFINAL_ONLY( u8 m_bitShift; );
	NOTFINAL_ONLY( u8 m_bitSize; );

	//If true it means the stat is a Player stat and will NOT have "MP0" to "MP4"
	NOTFINAL_ONLY( bool  m_isPlayerStat; );

	PAR_PARSABLE;
};

//PURPOSE: Catalog Entries for inventory properties.
class netCatalogInventoryItem : public netCatalogBaseItem
{
	typedef netCatalogBaseItem Base;

public:
	netCatalogInventoryItem() : m_isPlayerStat(false) {}
	netCatalogInventoryItem(const int keyhash, const int category, const int price, const char* statname, const bool isPlayerStat) 
		: Base(keyhash, category, price)
		, m_isPlayerStat(isPlayerStat)
	{
		m_statName = statname;
	}

	//PURPOSE: Set a stat value in local stats.
	void     SetStatValue(const netInventoryBaseItem* item) const;

	static u32 GetTypeHash() { return ATSTRINGHASH("netCatalogInventoryItem", 0xf3272615); }
	virtual bool Deserialize(const RsonReader& rr);

	const netCatalogInventoryItem& operator=(const Base& other)
	{
		Base::operator=(other);
		return *this;
	}
	const netCatalogInventoryItem& operator=(const netCatalogInventoryItem& other)
	{
		Base::operator=(&other);
		m_statName = other.m_statName;
		m_isPlayerStat = other.m_isPlayerStat;
		return *this;
	}

	virtual bool             IsValid( ) const;
	virtual bool SetupInventoryStats( );

	bool             IsCharacterStat( ) const { return !IsPlayerStat(); }
	bool                IsPlayerStat( ) const { return m_isPlayerStat; }

private:
	virtual StatId  GetStat(        ) const;
	virtual u32     GetHash(int slot) const;

private:
	//Vehicle Stat name.
	atString  m_statName;

	//If true it means the stat is a Player stat and will NOT have "MP0" to "MP4"
	bool  m_isPlayerStat;

	PAR_PARSABLE;
};

//PURPOSE: Catalog Entries that represent only catalog items. Usually for prices only.
class netCatalogOnlyItem : public netCatalogBaseItem
{
	typedef netCatalogBaseItem Base;

public:
	netCatalogOnlyItem() {}
	netCatalogOnlyItem(const int keyhash, const int category, const int price) 
		: Base(keyhash, category, price)
	{ }

	virtual void SetStatValue(const netInventoryBaseItem*) const  {;}

	static u32 GetTypeHash() { return ATSTRINGHASH("netCatalogOnlyItem", 0x10bcb2fc); }

private:
	StatId  GetStat(   ) const {return StatId();}
	u32     GetHash(int) const {return 0;}

	PAR_PARSABLE;
};

//PURPOSE: Catalog Entries that represent only catalog items. Usually for prices only but need also a stat value.
class netCatalogOnlyItemWithStat : public netCatalogBaseItem
{
	typedef netCatalogBaseItem Base;

public:
	netCatalogOnlyItemWithStat() {}
	netCatalogOnlyItemWithStat(const int keyhash, const int category, const int price, const int statvalue) 
		: Base(keyhash, category, price)
		, m_statValue(statvalue)
	{ }

	virtual void SetStatValue(const netInventoryBaseItem*) const  {;}

	virtual int  GetStatValue() const {return m_statValue;}
	virtual bool HasStatValue() const {return true;}
	virtual bool Deserialize(const RsonReader& rr);

	static u32 GetTypeHash() { return ATSTRINGHASH("netCatalogOnlyItemWithStat", 0xf413c951); }

private:
	StatId  GetStat(   ) const {return StatId();}
	u32     GetHash(int) const {return 0;}

private:
	int m_statValue;

	PAR_PARSABLE;
};

//PURPOSE: Multipurpose Catalog Entries for items with direct stats information.
class netCatalogGeneralItem : public netCatalogBaseItem
{
	typedef netCatalogBaseItem Base;

	static const u32 MAX_TEXT_TAG = 16;

public:
	netCatalogGeneralItem();
	netCatalogGeneralItem(const int keyhash 
							,const int category
							,const char* textTag
							,const int price
							,const char* statname
							,const int storagetype
							,const int bitshift
							,const int bitsize
							,const bool isPlayerStat);

	//PURPOSE: Set a stat value in local stats.
	void     SetStatValue(const netInventoryBaseItem* item) const;

	virtual bool Deserialize(const RsonReader& rr);

#if !__NO_OUTPUT
	const char* GetStatName() const { return m_statName.c_str();} 
#endif

	const netCatalogGeneralItem& operator=(const Base& other)
	{
		Base::operator=(other);
		return *this;
	}
	const netCatalogGeneralItem& operator=(const netCatalogGeneralItem& other)
	{
		Base::operator=(&other);

#if __BANK

		SetLabel(other.m_textTag.TryGetCStr());

		if (other.m_name.c_str())
		{
			SetName(other.m_name.c_str());
		}

#endif //__BANK

		m_statName    = other.m_statName;
		m_storageType = other.m_storageType;
		m_bitShift    = other.m_bitShift;
		m_bitSize     = other.m_bitSize;
		m_isPlayerStat  = other.m_isPlayerStat;
		return *this;
	}

	virtual bool             IsValid( ) const;
	virtual bool SetupInventoryStats( );

	u8         GetBitShift() const { return m_bitShift; }
	u8          GetBitSize() const { return m_bitSize; }
	bool   IsCharacterStat() const { return !IsPlayerStat(); }
	bool      IsPlayerStat() const { return m_isPlayerStat; }

public:
	virtual StatId GetStat(        ) const;
	virtual u32    GetHash(int slot) const;

private:

	//Stat name.
	atString m_statName;

	//Type of storage for the stat.
	u8 m_storageType;

	//Bit shift and number of bits when the stat is set.
	u8 m_bitShift;
	u8 m_bitSize;

	//If true it means the stat is a Player stat and will NOT have "MP0" to "MP4"
	bool  m_isPlayerStat;

	PAR_PARSABLE;
};

//PURPOSE: Parsable class for the catalog cache.
class CacheCatalogParsableInfo
{
public:
	CacheCatalogParsableInfo() {}
	virtual ~CacheCatalogParsableInfo() 
	{
		for (int i = 0; i < m_Items1.GetCount(); i++)
		{
			delete m_Items1[i];
		}
		m_Items1.Reset();
	}

public:
	atArray < netCatalogBaseItem* > m_Items1;
	atArray < netCatalogBaseItem* > m_Items2;
	int m_Version;
	u32 m_CRC;
	int m_LatestServerVersion;
	PAR_SIMPLE_PARSABLE;
};

//PURPOSE: Cache listener, called when we successfully load the catalog.
class CatalogCacheListener
{
protected:
	CatalogCacheListener(){;}
	virtual ~CatalogCacheListener(){;}
public:
	virtual void OnCacheFileLoaded(CacheCatalogParsableInfo& /*catalog*/) = 0;
};

//PURPOSE: Work thread to save the catalog into the cache.
class netCatalogCacheSaveWorker : public sysThreadPool::WorkItem
{
	virtual void DoWork();

private:
	//PURPOSE: Builds the pso with the catalog items.
	void  BuildPsoData(CacheCatalogParsableInfo& catalog, psoBuilder& builder);
};

//PURPOSE: Manage the catalog cache and responsible for loading/saving it.
class netCatalogCache : CloudCacheListener
{
public:
	static const u32 HEADER_SIZE = 4; //4 byte header for LZ4 compression/decompression.
	static const u32 MAX_UMCOMPRESSED_SIZE = 15000*1024; //Max uncompressed buffer size.

public:
	netCatalogCache(CatalogCacheListener* listener);
	~netCatalogCache( );

	void       Init( );
	bool       Load( );
	bool       Save( );
	void CancelWork( );
	bool     Pending( ) const;

	virtual void   OnCacheFileLoaded(const CacheRequestID requestID, bool cacheLoaded);
	virtual void OnCacheStringLoaded(const CacheRequestID /*requestID*/, unsigned /*totalsize*/) {}

private:
	CatalogCacheListener*  m_cachelistener;
	CacheRequestID         m_requestId;

	//Stuff for the background processing thread.
	netCatalogCacheSaveWorker    m_saveWorker;
	static sysThreadPool::Thread s_catalogCacheThread;
};

//PURPOSE: Used to permit stats update to packed stats that are shared between the game server and script.
struct sPackedStatsScriptExceptions
{
private:
	atArray< int >  m_bitShift;
	int             m_bits;
public:
	sPackedStatsScriptExceptions( const bool isBooleanType ) 
	{
		if (isBooleanType)
		{
			m_bits = 1;
			m_bitShift.Reserve(64);
			for (int i=0; i<64; i+=m_bits)
				m_bitShift.Push(i);
		}
		else
		{
			m_bits = 8;
			m_bitShift.Reserve(8);
			for (int i=0; i<=56; i+=m_bits)
				m_bitShift.Push(i);
		}
	}

	~sPackedStatsScriptExceptions()
	{
		m_bitShift.clear();
	}

	bool     Find(const int value) const { return (m_bitShift.Find(value) > -1); }
	int  GetCount(               ) const { return (m_bitShift.GetCount()); }

	bool Remove(const int value)
	{
		for (int i=0; i<m_bitShift.GetCount(); i++)
		{
			if (value == m_bitShift[i])
			{
				m_bitShift.Delete(i);
				return true;
			}
		}

		return false;
	}

	void SetExceptions(const StatId& key, const u64 value) const;

	OUTPUT_ONLY( void SpewBitShifts( ) const; )
};

//PURPOSE: Work thread to save the catalog into the cache.
class netCatalogDownloadWorker : public sysThreadPool::WorkItem
{
	static const u32 BUFFER_SIZE_MULTIPLIER = 15;

public:
	netCatalogDownloadWorker();
	bool Configure(const void* pData, unsigned nDataSize, bool isAligned);

	virtual void DoWork();
	virtual void OnFinished();

private:
	void*    m_data;
	u32      m_dataSize;
	bool     m_isAligned;
	u32      m_bufferSize;
};

//PURPOSE: transaction server catalog store. 
class netCatalog : CatalogCacheListener
{
	//Number of times we will accept failures in the catalog.
	static const u32 MAX_CATALOG_FAILURES = 3; 

public:
	netCatalog()
		: m_crc(0)
		, m_version(0)
		, m_latestServerVersion(0)
		, m_catalogCache(this)
		, m_writeToCacheCount(0)
		, m_catalogMaxFailures(MAX_CATALOG_FAILURES)
		, m_catalogFailedTimes(0)
		, m_catalogWasSuccessful(false)
	{
	}
	~netCatalog();

	static netCatalog& GetInstance() {return sm_Instance;}

	// PURPOSE: Access to catalog tunables
	static void SetCheckForDuplicatesOnAddItem(const bool checkForDuplicates);

	// PURPOSE: Cancel Worker thread that deals with updating the catalog Cache.
	void  CancelSaveWorker();

	// PURPOSE: Initialize everything.
	void Init();

	// PURPOSE: Shutdown everything.
	void Shutdown();

#if !__FINAL
	// PURPOSE: Read local catalog file.
	void Read();

	// PURPOSE: Write local catalog file.
	void Write();
#endif

	//PURPOSE: Clear current catalog download status.
	void ClearDownloadStatus();

	//PURPOSE: Returns true when the request is successful.
	bool HasReadSucceeded() const;

	//PURPOSE: Returns true when the request has failed.
	bool HasReadFailed() const;

	//PURPOSE: Set amount of catalog item to profile stats
	bool SetStat(const netInventoryBaseItem* item) const;

	//PURPOSE: Iterator fro catalog items
	netCatalogMap::ConstIterator GetCatalogIterator( ) const;

	//PURPOSE: Accessor for a catalog item
	const netCatalogBaseItem* Find(atHashString& key) const;

	//PURPOSE: Add items to out catalog database.
	bool  AddItem(const atHashString& key, netCatalogBaseItem* item);
	
	//PURPOSE: remove items to out catalog database.
	bool  RemoveItem(atHashString& key);
	
	//PURPOSE: Reset all data and prepare to add new json file.
	void  Clear( );

	//PURPOSE: accessors to catalog read status.
	netStatus& GetStatus() {return m_UpdateStatus;}

	//PURPOSE: Control server catalog requests.
	void RequestServerFile();

	//PURPOSE: Deals with catalog download failures.
	void CatalogDownloadDone();

	//PURPOSE: Link 2 items n the catalog, ie, buying one item opens the other item.
	BANK_ONLY( bool           LinkItems(atHashString& keyA, atHashString& keyB); )
	BANK_ONLY( bool      AreItemsLinked(atHashString& keyA, atHashString& keyB) const; )
	BANK_ONLY( bool   RemoveLinkedItems(atHashString& keyA, atHashString& keyB); )
	BANK_ONLY( bool ChangeItemTextLabel(atHashString& key, const char* label); )
	BANK_ONLY( bool     ChangeItemPrice(atHashString& key, const int price); )

	//PURPOSE: Link 2 items so that when item with keyA is sold will delete the reference keyB from the inventory.
	BANK_ONLY( bool          LinkItemsForSelling(atHashString& keyA, atHashString& keyB); )
	BANK_ONLY( bool     AreItemsLinkedForSelling(atHashString& keyA, atHashString& keyB) const; )
	BANK_ONLY( bool  RemoveLinkedItemsForSelling(atHashString& keyA, atHashString& keyB); )

	// PURPOSE: make sure the packed stats offsets are matching between script and code.
	BANK_ONLY(static void CheckPackedStatsEndvalue(int value));

	u32              GetNumItems(                 ) const;
	u32                   GetCRC(                 ) const {return m_crc;}
	u32               GetVersion(                 ) const {return m_version;}
	u32   GetLatestServerVersion(                 ) const {return m_latestServerVersion;}
	bool         IsLatestVersion(                 ) const {return (m_version == m_latestServerVersion && 0 < m_version);}
	void        SetLatestVersion(const u32 latestVersion, const u32 crc);

	//PURPOSE: Called when the catalog is downloaded.
	bool OnCatalogDownload(const void* pData, unsigned nDataSize, bool isAligned);

	//PURPOSE: Load catalog from cache file.
	void OnCacheFileLoaded(CacheCatalogParsableInfo& catalog);

	//PURPOSE: Load catalog from cache file.
	void  LoadFromCache();

	//PURPOSE: Write catalog from cache file.
	void  WriteToCache();

	const atMap < int, sPackedStatsScriptExceptions >& GetScriptExceptions() {return m_packedExceptions;}

	//Force catalog load Succeeded. 
	void  ForceSucceeded( )
	{
		m_ReadStatus.Reset();
		m_ReadStatus.SetPending();
		m_ReadStatus.SetSucceeded();
	}

	//Force catalog load Failed. 
	void  ForceFailed( )
	{
		m_ReadStatus.Reset();
		m_ReadStatus.SetPending();
		m_ReadStatus.SetFailed();
	}

	//PURPOSE: Returns TRUE when the catalog load is in progress.
	bool  GetLoadWorkerInProgress() const { return m_downloadWorker.Pending(); }

#if __BANK
	void  InvalidateCatalog() { m_latestServerVersion = m_version+1; }
#endif //__BANK

public:
	//PURPOSE: Load data from a json file.
	bool LoadFromJSON(const void* pData, unsigned nDataSize, unsigned& crcvalue);

	//PURPOSE: Return the ID of a random Item.
	u32 GetRandomItem() const;

private:
	bool LoadFromJSON(const rage::RsonReader& rr, unsigned& crcvalue);
	bool LoadItems(const rage::RsonReader& rr, unsigned& crcvalue);
	void CalculatePackedExceptions( );

	//PURPOSE: return the crc value of an item.
	static u32 GetItemCRC(netCatalogBaseItem* item);

private:
	static netCatalog sm_Instance;

	static bool sm_CheckForDuplicatesOnAddingItems;

	atMap < int, sPackedStatsScriptExceptions > m_packedExceptions;
	netCatalogCache     m_catalogCache;
	netCatalogMap       m_catalog;
	netStatus           m_ReadStatus;
	netStatus           m_UpdateStatus;
	u32                 m_crc;
	u32                 m_version;
	u32                 m_latestServerVersion;
	u32                 m_writeToCacheCount;
	u32                 m_catalogMaxFailures;
	u32                 m_catalogFailedTimes;
	bool                m_catalogWasSuccessful;

	//Stuff for the background processing thread.
	netCatalogDownloadWorker      m_downloadWorker;
	static sysThreadPool::Thread  s_catalogThread;

	PAR_SIMPLE_PARSABLE;
};

//PURPOSE: Download catalog from the cloud. 
class netCatalogServerData
{
private:
	//The json string is reasonable size, unlike the catalog.
	static const u32 MAX_DATA_SIZE = 3000;

public:
	netCatalogServerData(){;}
	~netCatalogServerData() { Cancel(); }

	//PURPOSE: Trigger request.
	bool RequestServerFile();

	//PURPOSE: Cancel any pending request.
	void Cancel();

	//PURPOSE: Called when the operation finishes.
	bool OnServerSuccess(const void* pData, unsigned nDataSize);
};
typedef atSingleton< netCatalogServerData > CatalogServerDataInst;
#define CATALOG_SERVER_INST CatalogServerDataInst::GetInstance()

#endif //__NET_SHOP_ACTIVE

#endif //INC_NET_CATALOG_H_