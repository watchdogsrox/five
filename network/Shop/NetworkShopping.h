//
// NetworkShopping.h
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//

#ifndef NETWORKSHOPPINGMGR_H
#define NETWORKSHOPPINGMGR_H

#include "network/NetworkTypes.h"
#if __NET_SHOP_ACTIVE

// Rage headers
#include "net/status.h"
#include "data/base.h"
#include "atl/dlist.h"
#include "atl/singleton.h"
#include "atl/hashstring.h"
#include "atl/array.h"
#include "string/stringhash.h"
#include "parser/macros.h"

// Framework headers
#include "fwtl/pool.h"

// Game headers
#include "Network/Shop/NetworkGameTransactions.h"

//Return the friendly name of the transaction type.
OUTPUT_ONLY( const char*     NETWORK_SHOP_GET_TRANS_TYPENAME(const NetShopTransactionType type);     )
OUTPUT_ONLY( const char*   NETWORK_SHOP_GET_TRANS_ACTIONNAME(const NetShopTransactionAction action); )
OUTPUT_ONLY( const char* NETWORK_SHOP_GET_TRANS_CATEGORYNAME(const NetShopCategory category);        )

//Forward definitions
namespace rage { class RsonWriter; class sysMemAllocator; };

//Defines for the flags that can be set when adding items to the basket or adding services.
enum eCATALOG_ITEM_FLAGS
{
	 CATALOG_ITEM_FLAG_WALLET_ONLY       = BIT(0) // Use only the WALLET in the transaction.
	,CATALOG_ITEM_FLAG_BANK_ONLY         = BIT(1) // Use only the BANK in the transaction.
	,CATALOG_ITEM_FLAG_BANK_THEN_WALLET  = BIT(2) // Use the BANK then after that the WALLET in the transaction.
	,CATALOG_ITEM_FLAG_WALLET_THEN_BANK  = BIT(3) // Use the WALLET then after that the BANK in the transaction.
	,CATALOG_ITEM_FLAG_EVC_ONLY          = BIT(4) // Use EVC Only.
	,CATALOG_ITEM_FLAG_TOKEN             = BIT(5) // Using tokens.
};

//PURPOSE:
//  Keep track of pending cash reductions.
class PendingCashReductionsHelper
{
private:
	struct CashAmountHelper
	{
	public:
		CashAmountHelper() : m_id(0), m_bank(0), m_wallet(0), m_EvcOnly(false) {;}
		CashAmountHelper (const int id, const s64 wallet, const s64 bank, const bool evconly)
			: m_id(id)
			, m_bank(bank)
			, m_wallet(wallet)
			, m_EvcOnly(evconly)
		{;}

	public:
		int m_id;
		s64 m_bank;
		s64 m_wallet;
		bool m_EvcOnly;
	};

public:
	PendingCashReductionsHelper() : m_totalbank(0), m_totalwallet(0), m_totalbankEvcOnly(0), m_totalwalletEvcOnly(0) {;}

	bool   Finished(const int id);
	void   Register(const int id, const s64 wallet, const s64 bank, const bool evconly);

	//Accessors to amounts
	s64     GetTotalEvc(  ) const;
	s64      GetTotalVc(  ) const { return m_totalbank + m_totalwallet + m_totalbankEvcOnly + m_totalwalletEvcOnly; }
	s64  GetTotalWallet(  ) const { return m_totalwallet + m_totalwalletEvcOnly; }
	s64    GetTotalBank(  ) const { return m_totalbank + m_totalbankEvcOnly; }

private:
	bool  Exists(const int id, int& index) const;
	void  IncrementTotals(const s64 wallet, const s64 bank, const bool evconly);
	void  DecrementTotals(const s64 wallet, const s64 bank, const bool evconly);

private:
	atArray< CashAmountHelper >  m_transactions;
	s64                          m_totalbank;
	s64                          m_totalwallet;
	s64                          m_totalbankEvcOnly;
	s64                          m_totalwalletEvcOnly;
};

//PURPOSE
//  Item's information that needs to be sent.
class CNetShopItem
{
public:
	NetShopItemId     m_Id;
	NetShopItemId     m_ExtraInventoryId;
	int               m_Price;
	int               m_StatValue;
	u32               m_Quantity;

public:
	CNetShopItem() { Clear(); }
	CNetShopItem(const NetShopItemId id, const NetShopItemId catalogId, const int price, const int statValue, const u32 quantity = 1) 
		: m_Id(id)
		, m_ExtraInventoryId(catalogId)
		, m_Price(price)
		, m_StatValue(statValue)
		, m_Quantity(quantity) 
	{;}

	void Clear();
	bool IsValid() const { return (m_Id != NET_SHOP_INVALID_ITEM_ID); }

	const CNetShopItem& operator=(const CNetShopItem& other);
};

//PURPOSE
//structure to pass back containing info about server data that was applied.
class CNetShopTransactionServerDataInfo
{
public:
	CNetShopTransactionServerDataInfo()
	{

	}

	NetworkGameTransactions::PlayerBalanceApplicationInfo m_playerBalanceApplyInfo;
	NetworkGameTransactions::InventoryDataApplicationInfo m_inventoryApplyInfo;
};


//PURPOSE
//  Base class for shopping transactions.
class CNetShopTransactionBase
{
protected:
	static const u32 MAX_NUMBER_OF_ATTEMPTS = 2;

	//Magic Number for character slot signifying a Player stat.
	static const int SLOT_ID_FOR_PLAYER_STATS = 5;

private:
	static u32 sm_TransactionBaseId;
	static u32 NEXT_AUTO_ID();

public:
	FW_REGISTER_CLASS_POOL( CNetShopTransactionBase );

	CNetShopTransactionBase()
		: m_RequestId(0)
		, m_Type(0)
		, m_Checkout(false)
		, m_Category(0)
		, m_Action(0)
		, m_Flags(0)
		, m_IsProcessing(false)
		, m_TimeEnd(0)
#if !__NO_OUTPUT
		, m_attempts(0)
		, m_FrameStart(0)
		, m_FrameEnd(0)
		, m_TimeStart(0)
#endif // !__NO_OUTPUT
	{
	}

	CNetShopTransactionBase(const NetShopTransactionType type, const NetShopCategory category, const NetShopTransactionAction action, const int flags) 
		: m_RequestId(0)
		, m_Type(type)
		, m_Checkout(false)
		, m_NullTransaction(false)
		, m_Category(category)
		, m_Action(action)
		, m_Flags(flags)
		, m_IsProcessing(false)
#if !__NO_OUTPUT
		, m_attempts(0)
		, m_FrameStart(0)
		, m_FrameEnd(0)
		, m_TimeStart(0)
#endif // !__NO_OUTPUT
	{
		Init();
	}

	CNetShopTransactionBase(const CNetShopTransactionBase& other)
	{
		m_Type                     = other.m_Type;
		m_RequestId                = other.m_RequestId;
		m_Category                 = other.m_Category;
		m_Status                   = other.m_Status;
		m_Checkout                 = other.m_Checkout;
		m_NullTransaction          = other.m_NullTransaction;
		m_Action                   = other.m_Action;
		m_Flags                    = other.m_Flags;
		m_PlayerBalanceResponse    = other.m_PlayerBalanceResponse;
		m_InventoryItemSetResponse = other.m_InventoryItemSetResponse;
		m_TelemetryNonceResponse           = other.m_TelemetryNonceResponse;
		m_IsProcessing             = other.m_IsProcessing;
		m_attempts                 = other.m_attempts;

#if !__NO_OUTPUT
		m_FrameStart               = other.m_FrameStart;
		m_FrameEnd                 = other.m_FrameEnd;
		m_TimeStart                = other.m_TimeStart;
#endif // !__NO_OUTPUT
	}

	virtual ~CNetShopTransactionBase()
	{
		Shutdown();
	}

	virtual void             Init( );
	virtual void           Cancel( );
	virtual void         Shutdown( );
	virtual void           Update( );
	virtual void            Clear( );

	//Should be called to Finish call by ProcessingStart();
	bool    FinishProcessingStart(NetworkGameTransactions::SpendEarnTransaction& transactionObj);

	//Should be called to register pending cash transactions when they get added to the queue.
	bool    RegisterPendingCash( ) const;

	virtual void     ProcessSuccess( ) = 0;
	virtual bool    ProcessingStart( ) = 0;
	virtual bool  GetTransactionObj(NetworkGameTransactions::SpendEarnTransaction& transactionObj) const = 0;
	virtual void     ProcessFailure( );

	//Return True if the Action is Earn.
	bool  GetIsEarnAction( ) const;
	// Return True if it's a Bonus Action, and hasn't already been reported
	__forceinline bool  AddApplicableBonusReport(CNetShopItem) const;

	//Force Failure because Finish FinishProcessingStart() FAILED.
	void   ForceFailure( );

	NetShopTransactionId  GetId() const { return m_Id; }

	bool                      GetIsType(const NetShopTransactionType type) const { return (type == m_Type); }
	NetShopTransactionType      GetType( )                                 const { return (m_Type); }

	OUTPUT_ONLY( const char*      GetTypeName( ) const { return NETWORK_SHOP_GET_TRANS_TYPENAME(m_Type); } )
	OUTPUT_ONLY( const char*    GetActionName( ) const { return NETWORK_SHOP_GET_TRANS_ACTIONNAME(m_Action); } )
	OUTPUT_ONLY( const char*  GetCategoryName( ) const { return NETWORK_SHOP_GET_TRANS_CATEGORYNAME(m_Category); } )
	OUTPUT_ONLY( virtual void       SpewItems( ) const = 0; )

	bool             IsProcessing( ) const { return m_IsProcessing; }
	bool                     None( ) const { return m_Status.None(); }
	bool                   Failed( ) const { return m_Status.Failed(); }
	bool                  Pending( ) const { return m_Status.Pending(); }
	bool                 Canceled( ) const { return m_Status.Canceled(); }
	bool                Succeeded( ) const { return m_Status.Succeeded(); }
	bool            FailedExpired( ) const;
	const char*   GetStatusString( ) const { return (Pending() ? "Pending" : (None() ? "None" : (Failed() ? "Failed" : (Canceled() ? "Canceled" : "Succeeded")))); }

	netStatus::StatusCode      GetStatus( ) const { return m_Status.GetStatus(); }
	int                    GetResultCode( ) const { return m_Status.GetResultCode(); }
	virtual int GetServiceId() const { return 0; };

	NetShopRequestId GetRequestId() const { gnetAssert(m_RequestId); return m_RequestId; }

	bool     Checkout( );
	bool  CanCheckout( ) const;

	bool ApplyDataToStats( CNetShopTransactionServerDataInfo& outInfo, const bool checkProcessing = true );

	NetShopCategory  GetItemsCategory() const { return m_Category; }

	NetShopTransactionAction   GetAction( ) const { return m_Action; }

	const CNetShopTransactionBase& operator=(const CNetShopTransactionBase& other);

	bool WasNullTransaction() const { return m_NullTransaction; }

	OUTPUT_ONLY( virtual void  GrcDebugDraw(); )

	s64 GetTelemetryNonce() const {return m_TelemetryNonceResponse.GetNonce();}

protected:
	//Support for just doing a NULL transaction.
	bool ShouldDoNullTransaction() const;
	void DoNullServerTransaction();

	void SendResultEventToScript() const;

	//Given a catalog item id - returns the character slot id used 
	// in the serialization of the member "slot" to the server. Server uses magic number '5'
	// to identify inventory Player stats ( vs the large majority that is Character stats ).
	int GetSlotIdForCatalogItem(const u32 catalogItemId) const;

protected:

	NetShopTransactionId                        m_Id;
	NetShopTransactionType                      m_Type;
	NetShopRequestId                            m_RequestId;
	NetShopCategory                             m_Category;
	netStatus                                   m_Status;
	NetShopTransactionAction                    m_Action;
	int                                         m_Flags;
	bool                                        m_Checkout;
	bool                                        m_NullTransaction;
	bool                                        m_IsProcessing;
	u32                                         m_TimeEnd;
	NetworkGameTransactions::PlayerBalance      m_PlayerBalanceResponse;
	NetworkGameTransactions::InventoryItemSet   m_InventoryItemSetResponse;
	NetworkGameTransactions::TelemetryNonce     m_TelemetryNonceResponse;
	u32                                         m_attempts;

#if !__NO_OUTPUT
	u32  m_FrameStart;
	u32  m_FrameEnd;
	u32  m_TimeStart;
#endif // !__NO_OUTPUT
};


//PURPOSE
//  This class is responsible for shopping items - part of the player inventory.
class CNetShopTransactionBasket : public CNetShopTransactionBase
{
public:
	//Maximum number of items a basket can hold.
	static const u8 MAX_NUMBER_ITEMS = 71;

public:
	CNetShopTransactionBasket()
		: CNetShopTransactionBase( )
		, m_Size(0) 
	{
	}

	CNetShopTransactionBasket(const NetShopTransactionType type, const NetShopCategory category, const NetShopTransactionAction action, const int flags) 
		: CNetShopTransactionBase(type, category, action, flags)
		, m_Size(0) 
	{
	}

	virtual ~CNetShopTransactionBasket()
	{
		Shutdown();
	}

	virtual void       Init( );
	virtual void     Cancel( );
	virtual void   Shutdown( );
	virtual void      Clear( );

	virtual void     ProcessSuccess( );
	virtual bool    ProcessingStart( );
	virtual bool  GetTransactionObj(NetworkGameTransactions::SpendEarnTransaction& transactionObj) const;

	u8               Find( const NetShopItemId itemId ) const;
	bool              Add( const CNetShopItem& itemId );
	bool           Remove( const NetShopItemId itemId );
	int          GetPrice( const NetShopItemId itemId ) const;
	int     GetTotalPrice(bool& propertieTradeIsCredit, int& numProperties, bool& hasOverrideItem) const;
	u8     GetNumberItems( ) const { return m_Size; }

	const CNetShopTransactionBasket& operator=(const CNetShopTransactionBasket& other);

	OUTPUT_ONLY( virtual void  GrcDebugDraw( ); )
	OUTPUT_ONLY( virtual void     SpewItems( ) const; )

private:
	CNetShopItem   m_Items[MAX_NUMBER_ITEMS];
	u8             m_Size;
};


//PURPOSE
//  This class is responsible for a single service transaction.
class CNetShopTransaction : public CNetShopTransactionBase
{

public:
	CNetShopTransaction( ) : CNetShopTransactionBase () {}
	CNetShopTransaction( const NetShopTransactionType type, const NetShopCategory category, const NetShopItemId id, const int price, const NetShopTransactionAction action, const int flags) 
		: CNetShopTransactionBase (type, category, action, flags)
	{
		m_Item.m_Id        = id;
		m_Item.m_Price     = price;
		m_Item.m_StatValue = 0;
		m_Item.m_Quantity  = 1;
	}

	virtual ~CNetShopTransaction()
	{
		Shutdown();
	}

	virtual void             Init( );
	virtual void           Cancel( );
	virtual void         Shutdown( );
	virtual void            Clear( );

	virtual void     ProcessSuccess( );
	virtual bool    ProcessingStart( );
	virtual bool  GetTransactionObj(NetworkGameTransactions::SpendEarnTransaction& transactionObj) const;

	int                  GetPrice( ) const { return m_Item.m_Price; }
	NetShopItemId       GetItemId( ) const { return m_Item.m_Id; }
	virtual int GetServiceId() const { return (int)GetItemId(); };

	const CNetShopTransaction& operator=(const CNetShopTransaction& other);

	OUTPUT_ONLY( virtual void  GrcDebugDraw( ); )
	OUTPUT_ONLY( virtual void     SpewItems( ) const; )

private:
	CNetShopItem  m_Item;
};

//PURPOSE
//  Represents a transaction service.
class CNetTransactionItemKey
{
public:
	CNetTransactionItemKey() {}
	CNetTransactionItemKey(atHashString key) :
	m_key(key) {}

#if __DEV
	const char* TryGetCStr() const {return m_key.TryGetCStr();}
	u32 GetKeyHash() const {return m_key.GetHash();}
#else
	const char* TryGetCStr() const {return NULL;}
	u32 GetKeyHash() const {return static_cast<u32>(m_key);}
#endif

private:
#if __DEV
	atHashString m_key;
#else
	int m_key;
#endif // __DEV

	PAR_SIMPLE_PARSABLE;
};

#if !__NO_OUTPUT

//PURPOSE
//  Debug draw standalone game transactions.
struct sStandaloneGameTransactionStatus
{
public:
	const char*       m_name;
	const netStatus*  m_status;

	sStandaloneGameTransactionStatus( ) 
		: m_name(0)
		, m_status(0)
	{;}

	sStandaloneGameTransactionStatus(const char* name, const netStatus* status) 
		: m_name(name)
		, m_status(status)
	{;}

	void  GrcDebugDraw();
};

#endif // !__NO_OUTPUT

//PURPOSE
//  Class responsible to manage all shopping transactions.
class CNetworkShoppingMgr
{
public:
	enum { MAX_NUMBER_TRANSACTIONS = 15 };

private:

	//PURPOSE
	// This is a convenience class for allocating the list nodes from a pool
	class atDTransactionNode : public atDNode<CNetShopTransactionBase*, datBase>
	{
	public:
		FW_REGISTER_CLASS_POOL(atDTransactionNode);
	};

public:

	CNetworkShoppingMgr();
	~CNetworkShoppingMgr();

	void InitCatalog( );
	void Init(sysMemAllocator* pAllocator);
	void Shutdown(const u32 shutdownMode);
	void Update( );
	OUTPUT_ONLY( void UpdateGrcDebugDraw(); )
	OUTPUT_ONLY( static void AddToGrcDebugDraw(sStandaloneGameTransactionStatus& debug); )

	//Returns the friendly name of services/categories/transaction types.
	OUTPUT_ONLY( void              SpewServicesInfo(const NetShopTransactionType type, const NetShopItemId id) const; )
	OUTPUT_ONLY( const char*        GetCategoryName(const NetShopCategory type) const; )
	OUTPUT_ONLY( const char*	GetTransactionTypeName(const NetShopTransactionType type) const; )
	OUTPUT_ONLY( const char*      GetActionTypeName(const NetShopTransactionAction type) const; )

	//Returns TRUE is the services/categories/transaction types are valid.
	bool        GetCategoryIsValid(const NetShopCategory category)    const;
	bool GetTransactionTypeIsValid(const NetShopTransactionType type) const;
	bool      GetActionTypeIsValid(const NetShopTransactionAction action)    const;
	bool ActionAllowsNegativeOrZeroValue(const NetShopTransactionAction action) const;
	/* -------------------------------------- */
	/* BASKET                                 */

	bool                 CreateBasket(NetShopTransactionId& id, const NetShopCategory category, const NetShopTransactionAction action, const int flags);
	bool                 DeleteBasket( );
	bool                      AddItem(CNetShopItem& item);
	bool                   RemoveItem(const NetShopItemId itemId);
	bool                     FindItem(const NetShopItemId itemId) const;
	bool                  ClearBasket( );
	bool                IsBasketEmpty( ) const;
	bool                 IsBasketFull( ) const;
	u32                GetNumberItems( ) const;


	/* -------------------------------------- */
	/* SERVICES                               */

	bool                 BeginService(NetShopTransactionId& id, const NetShopTransactionType type, const NetShopCategory category, const NetShopItemId service, const NetShopTransactionAction action, const int price, const int flags);
	bool                   EndService( const NetShopTransactionId id );


	/* -------------------------------------- */
	/* BASKET + SERVICES                      */

	bool                StartCheckout(const NetShopTransactionId id);
	bool                    GetStatus(const NetShopTransactionId id, netStatus::StatusCode& code) const;
	bool                GetFailedCode(const NetShopTransactionId id, int& code) const;


	//Find our basket for the inventory.
	const CNetShopTransactionBase*  FindBasketConst(NetShopTransactionId& transId) const { return FindBasket(transId); }

	//Accessor's for services/categories/transaction types.
	const atArray< CNetTransactionItemKey >& GetTransactionTypes( ) const {return m_transactiontypes;};

	//Return TRUE if we should make a NULL transaction.
	bool ShouldDoNullTransaction() const;

	//Return TRUE if a transaction is in Progress.
	bool TransactionInProgress() const;

	//Find any transaction given a transaction id.
	const CNetShopTransactionBase*  FindTransaction(const NetShopTransactionId id) const;
	CNetShopTransactionBase*        FindTransaction(const NetShopTransactionId id);

	//Find any transaction given a transaction type and a service id.
	CNetShopTransaction*        FindService( const NetShopTransactionType type, const NetShopItemId id );
	const CNetShopTransaction*  FindService( const NetShopTransactionType type, const NetShopItemId id ) const;

	//Accessor for pending cash reductions
	PendingCashReductionsHelper& GetCashReductions() {return m_cashreductions;}

private:

	//Cancel and cleanup in flight transactions.
	void CancelTransactions( );

	//Returns if there are free spaces in the pool or if we have managed to create free spaces.
	bool  CreateFreeSpaces(const bool removeAlsofailed = false);

	//Append a new node.
	bool  AppendNewNode(CNetShopTransactionBase* transaction);

	//Find our basket for the inventory.
	CNetShopTransactionBase*        FindBasket(NetShopTransactionId& transId);
	const CNetShopTransactionBase*  FindBasket(NetShopTransactionId& transId) const;


#if __BANK

public:
	void            Bank_InitWidgets(class rage::bkBank* bank);
	void            Bank_WriteToCache( );
	void            Bank_ClearCatalog( );
	void            Bank_ClearVisualDebug( );
	void            Bank_LoadFromCache( );
	void            Bank_ReadCatalog( );
	void            Bank_WriteCatalog( );
	void			Bank_CreateBasket( );
	void			Bank_AddItem();
	void			Bank_Checkout();
	void			Bank_ApplyDataToStats();
	void            Bank_SpewPackedStatsExceptions();

	u32                  Bank_GetLatency( ) const { return m_bankLatency; };
	bool         Bank_GetOverrideLatency( ) const { return m_bankOverrideLatency; };
	WIN32PC_ONLY( bool  Bank_GetTestAsynchTransactions( ) const { return m_bankTestAsynchTransactions; }; )

private:

	u32   m_bankLatency;
	bool  m_bankOverrideLatency;
	WIN32PC_ONLY( bool  m_bankTestAsynchTransactions; )

#endif // __BANK

private:
	
	//Debug Draw transaction information.
	OUTPUT_ONLY( atFixedArray< CNetShopTransactionBase*, MAX_NUMBER_TRANSACTIONS > m_TransactionListHistory; )

	//Valid list of services/categories/transaction types
	atArray< CNetTransactionItemKey > m_transactiontypes;
	atArray< CNetTransactionItemKey > m_actiontypes;

	//A list of transactions waiting for processing.
	atDList< CNetShopTransactionBase*, datBase > m_TransactionList;

	//Memory allocator
	sysMemAllocator* m_Allocator;

	//Flag init Catalog from cache
	bool  m_LoadCatalogFromCache;

	//Flag transaction in progress.
	bool  m_transactionInProgress;

	//Keep track of cash reductions
	PendingCashReductionsHelper m_cashreductions;

	PAR_SIMPLE_PARSABLE;
};

typedef atSingleton< CNetworkShoppingMgr > NetworkShoppingMgrSingleton;
#define NETWORK_SHOPPING_MGR NetworkShoppingMgrSingleton::GetInstance()

#endif //__NET_SHOP_ACTIVE

#endif // NETWORKSHOPPINGMGR_H

// EOF

