//
// NetworkShopping.cpp
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//

#include "Network/NetworkTypes.h"

#if __NET_SHOP_ACTIVE

//Rage Headers
#include "net/nethardware.h"
#include "rline/rl.h"
#include "data/callback.h"
#include "system/memory.h"
#include "system/param.h"
#include "parser/manager.h"

#if __BANK
#include "bank/bank.h"
#endif // __BANK

#if !__NO_OUTPUT
#include "grcore/debugdraw.h"
#endif // !__NO_OUTPUT

//Framework headers
#include "fwnet/netchannel.h"
#include "fwsys/gameskeleton.h"

//Network Headers
#include "NetworkShopping.h"
#include "Network/Shop/NetworkShopping_parser.h"
#include "Network/Shop/Catalog.h"
#include "Network/Shop/Inventory.h"
#include "Network/Shop/GameTransactionsSessionMgr.h"
#include "Network/Network.h"
#include "Network/Debug/NetworkDebug.h"
#include "Event/EventNetwork.h"
#include "Event/EventGroup.h"
#include "Security/ragesecgameinterface.h"
#include "Stats/MoneyInterface.h"

NETWORK_OPTIMISATIONS()
NETWORK_SHOP_OPTIMISATIONS()

PARAM(netshoplatency, "[net_shopping] Set latency on network shopping transactions.");
PARAM(netshoptransactionnull, "[net_shopping] use the null transaction.");
PARAM(sc_UseBasketSystem, "[net_shopping] if not present the game will do NULL transactions.");
PARAM(netshoptransactiondebug, "[net_shopping] if present extra debug info is rendered.");


RAGE_DEFINE_SUBCHANNEL(net, shop, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_shop
#define gameserverDebugf1(_fmt, ...)          netDebug1("[%u] [%" I64FMT "d] " _fmt, GetId(), GetRequestId(), ##__VA_ARGS__)
#define gameserverDebugf3(_fmt, ...)          netDebug3("[%u] [%" I64FMT "d] " _fmt, GetId(), GetRequestId(), ##__VA_ARGS__)
#define gameserverErrorf(_fmt, ...)           netError("[%u] [%" I64FMT "d] " _fmt, GetId(), GetRequestId(), ##__VA_ARGS__)
#define gameserverAssertf(_cond, _fmt, ...)   netAssertf(_cond, "[%u] [%" I64FMT "d] " _fmt, GetId(), GetRequestId(), ##__VA_ARGS__)


#define LARGEST_NETSHOPTRANS_CLASS (ALIGN_TO_16(sizeof(CNetShopTransactionBasket)))

#if __NO_OUTPUT
#define TRANSACTION_POOL_SIZE (CNetworkShoppingMgr::MAX_NUMBER_TRANSACTIONS)
#else
//Need to be always *2 because of the history fixed array so we can create space when its full.
#define TRANSACTION_POOL_SIZE (CNetworkShoppingMgr::MAX_NUMBER_TRANSACTIONS*2)
#endif // __NO_OUTPUT

FW_INSTANTIATE_BASECLASS_POOL(CNetShopTransactionBase, TRANSACTION_POOL_SIZE, atHashString("NetShopTransactions",0x5f91f738), LARGEST_NETSHOPTRANS_CLASS);
FW_INSTANTIATE_CLASS_POOL(CNetworkShoppingMgr::atDTransactionNode, TRANSACTION_POOL_SIZE, atHashString("atDTransactionNode",0x74ab575b));

//Defines for transaction type's.
static const u32 NET_SHOP_TTYPE_INVALID  = ATSTRINGHASH("NET_SHOP_TTYPE_INVALID", 0xa10aed30);
static const u32 NET_SHOP_TTYPE_BASKET   = ATSTRINGHASH("NET_SHOP_TTYPE_BASKET", 0x04a9a0ae);
static const u32 NET_SHOP_TTYPE_SERVICE  = ATSTRINGHASH("NET_SHOP_TTYPE_SERVICE", 0xbc537e0d);

static const u32 NET_SHOP_ACTION_INVALID                  = ATSTRINGHASH("NET_SHOP_ACTION_INVALID", 0xbabfd2a5);
static const u32 NET_SHOP_ACTION_EARN                     = ATSTRINGHASH("NET_SHOP_ACTION_EARN", 0x562592bb);
static const u32 NET_SHOP_ACTION_SPEND                    = ATSTRINGHASH("NET_SHOP_ACTION_SPEND", 0x2005d9a9);
static const u32 NET_SHOP_ACTION_ALLOT                    = ATSTRINGHASH("NET_SHOP_ACTION_ALLOT", 0x53ed6cdd);
static const u32 NET_SHOP_ACTION_RECOUP                   = ATSTRINGHASH("NET_SHOP_ACTION_RECOUP", 0x9f7ef588);
static const u32 NET_SHOP_ACTION_PURCH                    = ATSTRINGHASH("NET_SHOP_ACTION_PURCH", 0xfe387a6b);
static const u32 NET_SHOP_ACTION_GIVE                     = ATSTRINGHASH("NET_SHOP_ACTION_GIVE", 0x5208d875);
static const u32 NET_SHOP_ACTION_USE                      = ATSTRINGHASH("NET_SHOP_ACTION_USE", 0xd4457774);
static const u32 NET_SHOP_ACTION_ACQUIRE                  = ATSTRINGHASH("NET_SHOP_ACTION_ACQUIRE", 0x3b6b7024);
static const u32 NET_SHOP_ACTION_DELETE_CHAR              = ATSTRINGHASH("NET_SHOP_ACTION_DELETE_CHAR", 0xaaf14924);
static const u32 NET_SHOP_ACTION_BUY_ITEM                 = ATSTRINGHASH("NET_SHOP_ACTION_BUY_ITEM", 0x1c8315fb);
static const u32 NET_SHOP_ACTION_BUY_VEHICLE              = ATSTRINGHASH("NET_SHOP_ACTION_BUY_VEHICLE", 0xca8729fa);
static const u32 NET_SHOP_ACTION_BUY_PROPERTY             = ATSTRINGHASH("NET_SHOP_ACTION_BUY_PROPERTY", 0x51d518f6);
static const u32 NET_SHOP_ACTION_SELL_VEHICLE             = ATSTRINGHASH("NET_SHOP_ACTION_SELL_VEHICLE", 0xb1866901);
static const u32 NET_SHOP_ACTION_BUY_VEHICLE_MODS         = ATSTRINGHASH("NET_SHOP_ACTION_BUY_VEHICLE_MODS", 0x8bd840b3);
static const u32 NET_SHOP_ACTION_CREATE_PLAYER_APPEARANCE = ATSTRINGHASH("NET_SHOP_ACTION_CREATE_PLAYER_APPEARANCE", 0x04df35a2);
static const u32 NET_SHOP_ACTION_SPEND_LIMITED_SERVICE    = ATSTRINGHASH("NET_SHOP_ACTION_SPEND_LIMITED_SERVICE", 0x3297c803);
static const u32 NET_SHOP_ACTION_EARN_LIMITED_SERVICE     = ATSTRINGHASH("NET_SHOP_ACTION_EARN_LIMITED_SERVICE", 0xbe1d1918);
static const u32 NET_SHOP_ACTION_BUY_WAREHOUSE            = ATSTRINGHASH("NET_SHOP_ACTION_BUY_WAREHOUSE", 0x0e0475fc);
static const u32 NET_SHOP_ACTION_BUY_CONTRABAND_MISSION   = ATSTRINGHASH("NET_SHOP_ACTION_BUY_CONTRABAND_MISSION", 0xc344bd09);
static const u32 NET_SHOP_ACTION_ADD_CONTRABAND           = ATSTRINGHASH("NET_SHOP_ACTION_ADD_CONTRABAND", 0x7da91a2c);
static const u32 NET_SHOP_ACTION_REMOVE_CONTRABAND        = ATSTRINGHASH("NET_SHOP_ACTION_REMOVE_CONTRABAND", 0xd0fc92cb);
static const u32 NET_SHOP_ACTION_UPDATE_BUSINESS_GOODS	  = ATSTRINGHASH("NET_SHOP_ACTION_UPDATE_BUSINESS_GOODS", 0xCB2B161D);
static const u32 NET_SHOP_ACTION_RESET_BUSINESS_PROGRESS  = ATSTRINGHASH("NET_SHOP_ACTION_RESET_BUSINESS_PROGRESS", 0x2EC621C0);
static const u32 NET_SHOP_ACTION_UPDATE_WAREHOUSE_VEHICLE = ATSTRINGHASH("NET_SHOP_ACTION_UPDATE_WAREHOUSE_VEHICLE", 0xD548DED3);
static const u32 NET_SHOP_ACTION_BONUS					  = ATSTRINGHASH("NET_SHOP_ACTION_BONUS", 0xFCFA31AD);
static const u32 NET_SHOP_ACTION_BUY_CASINO_CHIPS         = ATSTRINGHASH("NET_SHOP_ACTION_BUY_CASINO_CHIPS", 0xf8720a1a);
static const u32 NET_SHOP_ACTION_SELL_CASINO_CHIPS        = ATSTRINGHASH("NET_SHOP_ACTION_SELL_CASINO_CHIPS", 0xfeae09e5);
static const u32 NET_SHOP_ACTION_UPDATE_STORAGE_DATA      = ATSTRINGHASH("NET_SHOP_ACTION_UPDATE_STORAGE_DATA", 0x63797c2f);
static const u32 NET_SHOP_ACTION_BUY_UNLOCK               = ATSTRINGHASH("NET_SHOP_ACTION_BUY_UNLOCK", 0x9701c6d4);

u32 CNetShopTransactionBase::sm_TransactionBaseId = 1;

u32 CNetShopTransactionBase::NEXT_AUTO_ID( )
{
	const u32 transactionBaseId = sm_TransactionBaseId;
	++sm_TransactionBaseId;
	return transactionBaseId;
}

/////////////////////////////////////////////////////
///////////////// Transaction Types /////////////////
/////////////////////////////////////////////////////

#if !__NO_OUTPUT

//For grcDebugDraw Functions
static int   s_TextLine             = 0;
static float s_VerticalDist         = 0.013f;
static float s_HorizontalBorder     = 0.250f;
static float s_HorizontalItemBorder = 0.260f;
static float s_VerticalBorder       = 0.04f;
static float s_textScale            = 1.02f;
static bool  s_drawBGQuad           = true;
static bool  s_ActivateDebugDraw    = false;

const char* NETWORK_SHOP_GET_TRANS_TYPENAME(const NetShopTransactionType type)
{
	return NETWORK_SHOPPING_MGR.GetTransactionTypeName(type);
}

const char* NETWORK_SHOP_GET_TRANS_ACTIONNAME(const NetShopTransactionAction action)
{
	return NETWORK_SHOPPING_MGR.GetActionTypeName(action);
}

const char* NETWORK_SHOP_GET_TRANS_CATEGORYNAME(const NetShopCategory category)
{
	return NETWORK_SHOPPING_MGR.GetCategoryName(category);
}

struct sDebugNames
{
public:
	const char* m_name;
	int m_id;
	sDebugNames(const char* name, const int id) 
		: m_name(name)
		, m_id(id)
	{;}
};

#define DEBUG_NAME(name, id) sDebugNames(name, id)

enum eGAMESERVERRESULTS
{
	GAMESERVER_SUCCESS,
	GAMESERVER_ERROR_UNDEFINED,
	GAMESERVER_ERROR_NOT_IMPLEMENTED,
	GAMESERVER_ERROR_SYSTEM_ERROR,
	GAMESERVER_ERROR_INVALID_DATA,
	GAMESERVER_ERROR_INVALID_PRICE,
	GAMESERVER_ERROR_INSUFFICIENT_FUNDS,
	GAMESERVER_ERROR_INSUFFICIENT_INVENTORY,
	GAMESERVER_ERROR_INCORRECT_FUNDS,
	GAMESERVER_ERROR_INVALID_ITEM,
	GAMESERVER_ERROR_INVALID_PERMISSIONS,
	GAMESERVER_ERROR_INVALID_CATALOG_VERSION,
	GAMESERVER_ERROR_INVALID_TOKEN,
	GAMESERVER_ERROR_INVALID_REQUEST,
	GAMESERVER_ERROR_INVALID_ACCOUNT,
	GAMESERVER_ERROR_INVALID_NONCE,
	GAMESERVER_ERROR_EXPIRED_TOKEN,
	GAMESERVER_ERROR_CACHE_OPERATION,
	GAMESERVER_ERROR_INVALID_LIMITER_DEFINITION,
	GAMESERVER_ERROR_GAME_TRANSACTION_EXCEPTION,
	GAMESERVER_ERROR_EXCEED_LIMIT,
	GAMESERVER_ERROR_EXCEED_LIMIT_MESSAGE,
	GAMESERVER_ERROR_AUTH_FAILED,
	GAMESERVER_ERROR_STALE_DATA,
	GAMESERVER_ERROR_BAD_SIGNATURE,
	GAMESERVER_ERROR_INVALID_CONFIGURATION,
	GAMESERVER_ERROR_NET_MAINTENANCE,
	GAMESERVER_ERROR_USER_FORCE_BAIL,
	GAMESERVER_ERROR_CLIENT_FORCE_FAILED,
    GAMESERVER_ERROR_INVALID_ENTITLEMENT,
	GAMESERVER_ERROR_MAX
};
const sDebugNames s_GameServerErrors [] = 
{
	 DEBUG_NAME("SUCCESS",	                        GAMESERVER_SUCCESS)
	,DEBUG_NAME("ERROR_UNDEFINED",	                GAMESERVER_ERROR_UNDEFINED)
	,DEBUG_NAME("ERROR_NOT_IMPLEMENTED",	        GAMESERVER_ERROR_NOT_IMPLEMENTED)
	,DEBUG_NAME("ERROR_SYSTEM_ERROR",	            GAMESERVER_ERROR_SYSTEM_ERROR)
	,DEBUG_NAME("ERROR_INVALID_DATA",	            GAMESERVER_ERROR_INVALID_DATA)
	,DEBUG_NAME("ERROR_INVALID_PRICE",	            GAMESERVER_ERROR_INVALID_PRICE)
	,DEBUG_NAME("ERROR_INSUFFICIENT_FUNDS",	        GAMESERVER_ERROR_INSUFFICIENT_FUNDS)
	,DEBUG_NAME("ERROR_INSUFFICIENT_INVENTORY",	    GAMESERVER_ERROR_INSUFFICIENT_INVENTORY)
	,DEBUG_NAME("ERROR_INCORRECT_FUNDS",	        GAMESERVER_ERROR_INCORRECT_FUNDS)
	,DEBUG_NAME("ERROR_INVALID_ITEM",	            GAMESERVER_ERROR_INVALID_ITEM)
	,DEBUG_NAME("ERROR_INVALID_PERMISSIONS",	    GAMESERVER_ERROR_INVALID_PERMISSIONS)
	,DEBUG_NAME("ERROR_INVALID_CATALOG_VERSION",	GAMESERVER_ERROR_INVALID_CATALOG_VERSION)
	,DEBUG_NAME("ERROR_INVALID_TOKEN",	            GAMESERVER_ERROR_INVALID_TOKEN)
	,DEBUG_NAME("ERROR_INVALID_REQUEST",	        GAMESERVER_ERROR_INVALID_REQUEST)
	,DEBUG_NAME("ERROR_INVALID_ACCOUNT",	        GAMESERVER_ERROR_INVALID_ACCOUNT)
	,DEBUG_NAME("ERROR_INVALID_NONCE",	            GAMESERVER_ERROR_INVALID_NONCE)
	,DEBUG_NAME("ERROR_EXPIRED_TOKEN",	            GAMESERVER_ERROR_EXPIRED_TOKEN)
	,DEBUG_NAME("ERROR_CACHE_OPERATION",	        GAMESERVER_ERROR_CACHE_OPERATION)
	,DEBUG_NAME("ERROR_INVALID_LIMITER_DEFINITION",	GAMESERVER_ERROR_INVALID_LIMITER_DEFINITION)
	,DEBUG_NAME("ERROR_GAME_TRANSACTION_EXCEPTION",	GAMESERVER_ERROR_GAME_TRANSACTION_EXCEPTION)
	,DEBUG_NAME("ERROR_EXCEED_LIMIT",	            GAMESERVER_ERROR_EXCEED_LIMIT)
	,DEBUG_NAME("ERROR_EXCEED_LIMIT_MESSAGE",	    GAMESERVER_ERROR_EXCEED_LIMIT_MESSAGE)
	,DEBUG_NAME("ERROR_AUTH_FAILED",	            GAMESERVER_ERROR_AUTH_FAILED)
	,DEBUG_NAME("ERROR_STALE_DATA",	                GAMESERVER_ERROR_STALE_DATA)
	,DEBUG_NAME("ERROR_BAD_SIGNATURE",	            GAMESERVER_ERROR_BAD_SIGNATURE)
	,DEBUG_NAME("ERROR_INVALID_CONFIGURATION",	    GAMESERVER_ERROR_INVALID_CONFIGURATION)
	,DEBUG_NAME("ERROR_NET_MAINTENANCE",	        GAMESERVER_ERROR_NET_MAINTENANCE)
	,DEBUG_NAME("ERROR_USER_FORCE_BAIL",	        GAMESERVER_ERROR_USER_FORCE_BAIL)
	,DEBUG_NAME("ERROR_CLIENT_FORCE_FAILED",	    GAMESERVER_ERROR_CLIENT_FORCE_FAILED)
    ,DEBUG_NAME("ERROR_INVALID_ENTITLEMENT",        GAMESERVER_ERROR_INVALID_ENTITLEMENT)
};
static const u32 SIZE_OF_GAMERSERVER_ERRORS = COUNTOF(s_GameServerErrors);
CompileTimeAssert(SIZE_OF_GAMERSERVER_ERRORS == GAMESERVER_ERROR_MAX);
#endif // !__NO_OUTPUT



//////////////////////////////////////////////////////////////
///////////////// PendingCashReductionsHelper ////////////////
//////////////////////////////////////////////////////////////

bool PendingCashReductionsHelper::Finished(const int id)
{
	int index = 0;
	if (Exists(id, index))
	{
		gnetDebug1("PendingCashReductionsHelper - Finished - id='%d'", id);

		DecrementTotals(m_transactions[index].m_wallet, m_transactions[index].m_bank, m_transactions[index].m_EvcOnly);
		m_transactions.Delete(index);
		return true;
	}

	return false;
}

void  PendingCashReductionsHelper::IncrementTotals(const s64 wallet, const s64 bank, const bool evconly)
{
	if (evconly)
	{
		m_totalbankEvcOnly += bank;
		m_totalwalletEvcOnly += wallet;
	}
	else
	{
		m_totalbank += bank;
		m_totalwallet += wallet;
	}
}

void  PendingCashReductionsHelper::DecrementTotals(const s64 wallet, const s64 bank, const bool evconly)
{
	if (evconly)
	{
		m_totalbankEvcOnly -= bank;
		m_totalwalletEvcOnly -= wallet;
	}
	else
	{
		m_totalbank -= bank;
		m_totalwallet -= wallet;
	}
}

void PendingCashReductionsHelper::Register(const int id, const s64 wallet, const s64 bank, const bool evconly)
{
	int index = 0;

	//Update existing entry
	if (Exists(id, index))
	{
		gnetDebug1("PendingCashReductionsHelper - Update - id='%d', evcOnly='%s', old < b='%" I64FMT "d', w='%" I64FMT "d'>, new=< b='%" I64FMT "d', w='%" I64FMT "d' >", id, m_transactions[index].m_EvcOnly ? "true" : "false", m_transactions[index].m_bank, m_transactions[index].m_wallet, bank, wallet);

		//Remove current totals
		DecrementTotals(m_transactions[index].m_wallet, m_transactions[index].m_bank, m_transactions[index].m_EvcOnly);

		//Set new values
		m_transactions[index].m_bank   = bank;
		m_transactions[index].m_wallet = wallet;
		m_transactions[index].m_EvcOnly = evconly;
	}
	//add new entry
	else
	{
		gnetDebug1("PendingCashReductionsHelper - Register - id='%d', b='%" I64FMT "d', w='%" I64FMT "d', evcOnly='%s'.", id, bank, wallet, evconly?"true":"false");
		CashAmountHelper m(id, wallet, bank, evconly);
		m_transactions.PushAndGrow(m);
	}

	//Update Totals
	IncrementTotals(wallet, bank, evconly);
}

bool PendingCashReductionsHelper::Exists(const int id, int& index) const
{
	index = 0;

	for (int i=0; i<m_transactions.GetCount(); i++)
	{
		if (id == m_transactions[i].m_id)
		{
			index = i;
			return true;
		}
	}

	return false;
}

s64
PendingCashReductionsHelper::GetTotalEvc( ) const
{
	const s64 pvc = MoneyInterface::GetPVCBalance();

	if (pvc > (m_totalbank + m_totalwallet))
	{
		return (m_totalbankEvcOnly + m_totalwalletEvcOnly);
	}

	return (m_totalbankEvcOnly + m_totalwalletEvcOnly + m_totalbank + m_totalwallet - pvc);
}

//////////////////////////////////////////////////////////////
///////////////// NetShopTransBase::ItemInfo /////////////////
//////////////////////////////////////////////////////////////

void CNetShopItem::Clear()
{
	m_Id = NET_SHOP_INVALID_ITEM_ID;
	m_ExtraInventoryId = NET_SHOP_INVALID_ITEM_ID;
	m_Price       = 0;
	m_StatValue   = 0;
	m_Quantity    = 0;
}

const CNetShopItem& CNetShopItem::operator=(const CNetShopItem& other)
{
	m_Id = other.m_Id;
	m_ExtraInventoryId = other.m_ExtraInventoryId;
	m_Price       = other.m_Price;
	m_StatValue   = other.m_StatValue;
	m_Quantity    = other.m_Quantity;

	return *this;
}

////////////////////////////////////////////////////
///////////////// NetShopTransBase /////////////////
////////////////////////////////////////////////////

void CNetShopTransactionBase::Init() 
{
	rlCreateUUID(&m_RequestId);
	m_Id = NEXT_AUTO_ID( );

	gameserverDebugf1(" CNetShopTransactionBase::Init ");

	gnetAssert(m_Type != NET_SHOP_TTYPE_INVALID);
	gnetAssert(m_RequestId);
	gnetAssert(!m_IsProcessing);

	m_TimeEnd = 0;
	m_attempts = 0;

#if !__NO_OUTPUT
	m_FrameStart = 0;
	m_FrameEnd = 0;
	m_TimeStart = 0;
#endif // !__NO_OUTPUT
}

void CNetShopTransactionBase::Shutdown( )
{
	gnetAssert( !Pending() );

	if (Pending())
		Cancel();

	NETWORK_SHOPPING_MGR.GetCashReductions().Finished( static_cast< int >(GetId()) );
}

void CNetShopTransactionBase::Cancel( )
{
	if (gnetVerify(Pending()))
	{
		NetworkGameTransactions::Cancel(&m_Status);
	}
}

void CNetShopTransactionBase::Clear( )
{
	gnetAssert( !Pending() );

	if (Pending())
		Cancel();

	NETWORK_SHOPPING_MGR.GetCashReductions().Finished( static_cast< int >(GetId()) );

	m_Status.Reset();
	m_Checkout = false;

	gnetAssert( !IsProcessing() );
	m_IsProcessing = false;

	m_TimeEnd = 0;
	m_attempts = 0;

#if !__NO_OUTPUT
	m_FrameStart = 0;
	m_FrameEnd = 0;
	m_TimeStart = 0;
#endif // !__NO_OUTPUT

	Init();
}

bool CNetShopTransactionBase::FinishProcessingStart(NetworkGameTransactions::SpendEarnTransaction& transactionObj)
{
	bool result = true;

	gameserverDebugf1("FinishProcessingStart - Clear Nonce='%" I64FMT "d'.", m_TelemetryNonceResponse.GetNonce());

	m_TelemetryNonceResponse.Clear();
	m_InventoryItemSetResponse.Clear();
	m_PlayerBalanceResponse.Clear();

	//
	// Spend Events
	//
	if (m_Action == NET_SHOP_ACTION_SPEND)
	{
		gameserverDebugf1("FinishProcessingStart sending NetworkGameTransactions::SendSpendEvent");
		result = NetworkGameTransactions::SendSpendEvent(NetworkInterface::GetLocalGamerIndex(), transactionObj, &m_PlayerBalanceResponse, &m_InventoryItemSetResponse, &m_TelemetryNonceResponse, &m_Status, 0);
	}
	else if (m_Action == NET_SHOP_ACTION_BUY_ITEM)
	{
		gameserverDebugf1("FinishProcessingStart sending NetworkGameTransactions::SendBuyItemEvent");
		result = NetworkGameTransactions::SendBuyItemEvent(NetworkInterface::GetLocalGamerIndex(), transactionObj, &m_PlayerBalanceResponse, &m_InventoryItemSetResponse, &m_TelemetryNonceResponse, &m_Status);
	}
	else if (m_Action == NET_SHOP_ACTION_BUY_VEHICLE)
	{
		gameserverDebugf1("FinishProcessingStart sending NetworkGameTransactions::SendBuyVehicleEvent");
		result = NetworkGameTransactions::SendBuyVehicleEvent(NetworkInterface::GetLocalGamerIndex(), transactionObj, &m_PlayerBalanceResponse, &m_InventoryItemSetResponse, &m_TelemetryNonceResponse, &m_Status, 0);
	}
	else if (m_Action == NET_SHOP_ACTION_BUY_PROPERTY)
	{
		gameserverDebugf1("FinishProcessingStart sending NetworkGameTransactions::SendBuyPropertyEvent");
		result = NetworkGameTransactions::SendBuyPropertyEvent(NetworkInterface::GetLocalGamerIndex(), transactionObj, &m_PlayerBalanceResponse, &m_InventoryItemSetResponse, &m_TelemetryNonceResponse, &m_Status, 0);
	}
	else if (m_Action == NET_SHOP_ACTION_BUY_VEHICLE_MODS)
	{
		gameserverDebugf1("FinishProcessingStart sending NetworkGameTransactions::SendSpendVehicleModEvent");
		result = NetworkGameTransactions::SendSpendVehicleModEvent(NetworkInterface::GetLocalGamerIndex(), transactionObj, &m_PlayerBalanceResponse, &m_TelemetryNonceResponse, &m_Status);
	}
	else if (m_Action == NET_SHOP_ACTION_SPEND_LIMITED_SERVICE)
	{
		gameserverDebugf1("FinishProcessingStart sending NetworkGameTransactions::SendSpendLimitedService");
		result = NetworkGameTransactions::SendSpendLimitedService(NetworkInterface::GetLocalGamerIndex(), transactionObj, &m_PlayerBalanceResponse, &m_InventoryItemSetResponse, &m_TelemetryNonceResponse, &m_Status);
	}
	else if (m_Action == NET_SHOP_ACTION_BUY_WAREHOUSE)
	{
		gameserverDebugf1("FinishProcessingStart sending NetworkGameTransactions::SendBuyWarehouseEvent");
		result = NetworkGameTransactions::SendBuyWarehouseEvent(NetworkInterface::GetLocalGamerIndex(), transactionObj, &m_PlayerBalanceResponse, &m_InventoryItemSetResponse, &m_TelemetryNonceResponse, &m_Status, 0);
	}
	else if (m_Action == NET_SHOP_ACTION_BUY_CONTRABAND_MISSION)
	{
		gameserverDebugf1("FinishProcessingStart sending NetworkGameTransactions::SendBuyContrabandMissionEvent");
		result = NetworkGameTransactions::SendBuyContrabandMissionEvent(NetworkInterface::GetLocalGamerIndex(), transactionObj, &m_PlayerBalanceResponse, &m_InventoryItemSetResponse, &m_TelemetryNonceResponse, &m_Status, 0);
	}
	else if (m_Action == NET_SHOP_ACTION_ADD_CONTRABAND)
	{
		gameserverDebugf1("FinishProcessingStart sending NetworkGameTransactions::SendAddContrabandEvent");
		result = NetworkGameTransactions::SendAddContrabandEvent(NetworkInterface::GetLocalGamerIndex(), transactionObj, &m_PlayerBalanceResponse, &m_InventoryItemSetResponse, &m_TelemetryNonceResponse, &m_Status, 0);
	}
	else if (m_Action == NET_SHOP_ACTION_RESET_BUSINESS_PROGRESS)
	{
		gameserverDebugf1("FinishProcessingStart sending NetworkGameTransactions::SendResetBusinessProgressEvent");
		result = NetworkGameTransactions::SendResetBusinessProgressEvent(NetworkInterface::GetLocalGamerIndex(), transactionObj, &m_PlayerBalanceResponse, &m_InventoryItemSetResponse, &m_TelemetryNonceResponse, &m_Status, 0);
	}
	else if (m_Action == NET_SHOP_ACTION_UPDATE_BUSINESS_GOODS)
	{
		gameserverDebugf1("FinishProcessingStart sending NetworkGameTransactions::SendUpdateBusinessGoodsEvent");
		result = NetworkGameTransactions::SendUpdateBusinessGoodsEvent(NetworkInterface::GetLocalGamerIndex(), transactionObj, &m_PlayerBalanceResponse, &m_InventoryItemSetResponse, &m_TelemetryNonceResponse, &m_Status, 0);
	}
	else if (m_Action == NET_SHOP_ACTION_UPDATE_WAREHOUSE_VEHICLE)
	{
		gameserverDebugf1("FinishProcessingStart sending NetworkGameTransactions::SendUpdateUpdateWarehouseVehicle");
		result = NetworkGameTransactions::SendUpdateUpdateWarehouseVehicle(NetworkInterface::GetLocalGamerIndex(), transactionObj, &m_PlayerBalanceResponse, &m_InventoryItemSetResponse, &m_TelemetryNonceResponse, &m_Status, 0);
	}
	else if (m_Action == NET_SHOP_ACTION_BUY_CASINO_CHIPS)
	{
		gameserverDebugf1("FinishProcessingStart sending NetworkGameTransactions::SendBuyCasinoChips");
		result = NetworkGameTransactions::SendBuyCasinoChips(NetworkInterface::GetLocalGamerIndex(), transactionObj, &m_PlayerBalanceResponse, &m_InventoryItemSetResponse, &m_TelemetryNonceResponse, &m_Status, 0);
	}
	else if (m_Action == NET_SHOP_ACTION_UPDATE_STORAGE_DATA)
	{
		gameserverDebugf1("FinishProcessingStart sending NetworkGameTransactions::SendUpdateStorageData");
		result = NetworkGameTransactions::SendUpdateStorageData(NetworkInterface::GetLocalGamerIndex(), transactionObj, &m_PlayerBalanceResponse, &m_InventoryItemSetResponse, &m_TelemetryNonceResponse, &m_Status, 0);
	}
	else if (m_Action == NET_SHOP_ACTION_BUY_UNLOCK)
	{
		gameserverDebugf1("FinishProcessingStart sending NetworkGameTransactions::SendBuyUnlock");
		result = NetworkGameTransactions::SendBuyUnlock(NetworkInterface::GetLocalGamerIndex(), transactionObj, &m_PlayerBalanceResponse, &m_InventoryItemSetResponse, &m_TelemetryNonceResponse, &m_Status, 0);
	}

	//
	// Earn Events
	//
	else if (m_Action == NET_SHOP_ACTION_EARN)
	{
		gameserverDebugf1("FinishProcessingStart sending NetworkGameTransactions::SendEarnEvent");
		result = NetworkGameTransactions::SendEarnEvent(NetworkInterface::GetLocalGamerIndex(), transactionObj, &m_PlayerBalanceResponse, &m_InventoryItemSetResponse, &m_TelemetryNonceResponse, &m_Status, 0);
	}
	else if (m_Action == NET_SHOP_ACTION_SELL_VEHICLE)
	{
		gameserverDebugf1("FinishProcessingStart sending NetworkGameTransactions::SendSellVehicleEvent");
		result = NetworkGameTransactions::SendSellVehicleEvent(NetworkInterface::GetLocalGamerIndex(), transactionObj, &m_PlayerBalanceResponse, &m_InventoryItemSetResponse, &m_TelemetryNonceResponse, &m_Status, 0);
	}
	else if (m_Action == NET_SHOP_ACTION_CREATE_PLAYER_APPEARANCE)
	{
		gameserverDebugf1("FinishProcessingStart sending NetworkGameTransactions::CreatePlayerAppearance");
		result = NetworkGameTransactions::CreatePlayerAppearance(NetworkInterface::GetLocalGamerIndex(), transactionObj, &m_PlayerBalanceResponse, &m_InventoryItemSetResponse, &m_TelemetryNonceResponse, &m_Status);
	}
	else if (m_Action == NET_SHOP_ACTION_EARN_LIMITED_SERVICE)
	{
		gameserverDebugf1("FinishProcessingStart sending NetworkGameTransactions::SendEarnLimitedService");
		result = NetworkGameTransactions::SendEarnLimitedService(NetworkInterface::GetLocalGamerIndex(), transactionObj, &m_PlayerBalanceResponse, &m_InventoryItemSetResponse, &m_TelemetryNonceResponse, &m_Status);
	}
	else if (m_Action == NET_SHOP_ACTION_REMOVE_CONTRABAND)
	{
		gameserverDebugf1("FinishProcessingStart sending NetworkGameTransactions::SendRemoveContrabandEvent");
		result = NetworkGameTransactions::SendRemoveContrabandEvent(NetworkInterface::GetLocalGamerIndex(), transactionObj, &m_PlayerBalanceResponse, &m_InventoryItemSetResponse, &m_TelemetryNonceResponse, &m_Status, 0);
	}
	else if (m_Action == NET_SHOP_ACTION_SELL_CASINO_CHIPS)
	{
		gameserverDebugf1("FinishProcessingStart sending NetworkGameTransactions::SendSellCasinoChips");
		result = NetworkGameTransactions::SendSellCasinoChips(NetworkInterface::GetLocalGamerIndex(), transactionObj, &m_PlayerBalanceResponse, &m_InventoryItemSetResponse, &m_TelemetryNonceResponse, &m_Status, 0);
	}

	//
	// Misc Events
	//
	else if (m_Action == NET_SHOP_ACTION_BONUS)
	{
		gameserverDebugf1("FinishProcessingStart sending NetworkGameTransactions::SentBonusEvent");
		result = NetworkGameTransactions::SendBonusEvent(NetworkInterface::GetLocalGamerIndex(), transactionObj, &m_PlayerBalanceResponse, &m_InventoryItemSetResponse, &m_TelemetryNonceResponse, &m_Status, 0);
	}

	//
	// Default
	//
	else
	{
		result = false;
		gameserverDebugf1("FinishProcessingStart - Invalid Action \"0x%08X\"", m_Action);
		gameserverAssertf(0, "FinishProcessingStart - Invalid Action \"0x%08X\"", m_Action);
	}

	m_Checkout = false;
	m_IsProcessing = true;

	return result;
}

bool CNetShopTransactionBase::AddApplicableBonusReport (CNetShopItem item) const
{
	if(m_Action == NET_SHOP_ACTION_BONUS)
	{
		if(rageSecReportCache::GetInstance()->HasBeenReported(rageSecReportCache::eReportCacheBucket::RCB_GAMESERVER, item.m_Id))
		{
			return false;
		}
		else
		{
			rageSecReportCache::GetInstance()->AddToReported(rageSecReportCache::eReportCacheBucket::RCB_GAMESERVER, item.m_Id);
			return true;
		}
	} 
	return true;
}

bool  CNetShopTransactionBase::GetIsEarnAction( ) const
{
	if (m_Action == NET_SHOP_ACTION_EARN)
	{
		return true;
	}
	else if (m_Action == NET_SHOP_ACTION_SELL_VEHICLE)
	{
		return true;
	}
	else if (m_Action == NET_SHOP_ACTION_CREATE_PLAYER_APPEARANCE)
	{
		return true;
	}
	else if (m_Action == NET_SHOP_ACTION_EARN_LIMITED_SERVICE)
	{
		return true;
	}
	else if (m_Action == NET_SHOP_ACTION_REMOVE_CONTRABAND)
	{
		return true;
	}
	else if (m_Action == NET_SHOP_ACTION_SELL_CASINO_CHIPS)
	{
		return true;
	}

	return false;
}

bool CNetShopTransactionBase::RegisterPendingCash() const
{
	bool result = true;

	if (m_Action == NET_SHOP_ACTION_SPEND
		|| m_Action == NET_SHOP_ACTION_BUY_ITEM
		|| m_Action == NET_SHOP_ACTION_BUY_VEHICLE
		|| m_Action == NET_SHOP_ACTION_BUY_PROPERTY
		|| m_Action == NET_SHOP_ACTION_BUY_VEHICLE_MODS
		|| m_Action == NET_SHOP_ACTION_BUY_CONTRABAND_MISSION
		|| m_Action == NET_SHOP_ACTION_SPEND_LIMITED_SERVICE
		|| m_Action == NET_SHOP_ACTION_BUY_WAREHOUSE
		|| m_Action == NET_SHOP_ACTION_BUY_CONTRABAND_MISSION
		|| m_Action == NET_SHOP_ACTION_ADD_CONTRABAND
		|| m_Action == NET_SHOP_ACTION_BUY_CASINO_CHIPS)
	{
		result = false;

		NetworkGameTransactions::SpendEarnTransaction transactionObj;
		if (GetTransactionObj(transactionObj))
		{
			NETWORK_SHOPPING_MGR.GetCashReductions().Register(static_cast<int>(GetId()), transactionObj.GetWallet(), transactionObj.GetBank(), transactionObj.IsEVCOnly());
			result = true;
		}
	}

	return result;
}

void CNetShopTransactionBase::ForceFailure( )
{
	if (!m_Status.None())
		m_Status.Reset();

	m_Status.ForceFailed((int)ATSTRINGHASH("ERROR_CLIENT_FORCE_FAILED", 0xdd64b5fe));
	gameserverAssertf(m_Status.Failed(), "Failed :: ForceFailure( )");

	//Make sure we can go trough the ProcessFailure
	m_IsProcessing = true;
	Update( );
}

void CNetShopTransactionBase::ProcessFailure( )
{
	//If this was a failure due to needing to do a sessionRefresh, let's reset and try again.
	GameTransactionSessionMgr& rSession = GameTransactionSessionMgr::Get();
	if ((rSession.IsRefreshPending() || rSession.IsRefreshCatalogPending()) && Failed() && m_attempts < MAX_NUMBER_OF_ATTEMPTS)
	{
		gameserverDebugf1("CNetShopTransaction::ProcessFailure - Clearing failed due to RefreshSession pending");
		Checkout();
	}
	else
	{
		SendResultEventToScript();
		NETWORK_SHOPPING_MGR.GetCashReductions().Finished(static_cast<int>(GetId()));
	}
}

void CNetShopTransactionBase::Update( )
{
	if (None())
		return;

	if (!Pending() && IsProcessing())
	{
#if !__NO_OUTPUT
		m_FrameEnd = fwTimer::GetFrameCount();
#endif // !__NO_OUTPUT

		m_IsProcessing = false;
		m_TimeEnd = fwTimer::GetSystemTimeInMilliseconds();

		if (Succeeded())
		{
			gameserverDebugf3("CNetShopTransactionBase::Update:: - Succeeded.");
			ProcessSuccess();
		}
		else if(Failed() || Canceled() )
		{
			gameserverDebugf3("CNetShopTransactionBase::Update:: - Failed or Cancelled.");
			ProcessFailure();
		}
	}
}

bool CNetShopTransactionBase::FailedExpired( ) const 
{
	static const u32 FAILED_EXPIRE_TIME = 1000;

	//If this was a failure due to needing to do a sessionRefresh, let's reset and try again.
	GameTransactionSessionMgr& rSession = GameTransactionSessionMgr::Get();
	if (!rSession.IsRefreshPending() && !rSession.IsRefreshCatalogPending())
	{
		if (m_TimeEnd > 0)
		{
			return (m_Status.Failed() && (fwTimer::GetSystemTimeInMilliseconds( ) > (m_TimeEnd + FAILED_EXPIRE_TIME)));
		}
	}

	return false;
}

bool CNetShopTransactionBase::Checkout( )
{
	gnetAssertf(!Pending(), "This transaction is pending. type=\"%s\", id=\"%u\", Current status=\"%s\"", GetTypeName(), GetId(), GetStatusString());
	if (Pending())
		return false;

	gnetAssertf(!Succeeded(), "This transaction is already completed. type=\"%s\", id=\"%u\", Current status=\"%s\"", GetTypeName(), GetId(), GetStatusString());
	if (Succeeded())
		return false;

	gnetAssertf(!Canceled(), "This transaction has been Canceled. type=\"%s\", id=\"%u\", Current status=\"%s\"", GetTypeName(), GetId(), GetStatusString());
	if (Canceled())
		return false;

	gnetAssertf(!IsProcessing(), "This transaction is Processing. type=\"%s\", id=\"%u\", Current status=\"%s\"", GetTypeName(), GetId(), GetStatusString());
	if (IsProcessing())
		return false;

	m_Checkout = true;

	m_Status.Reset();

	m_TimeEnd = 0;

	m_attempts++;
	gnetAssert(m_attempts <= MAX_NUMBER_OF_ATTEMPTS);

#if !__NO_OUTPUT
	m_FrameStart = fwTimer::GetFrameCount();
	m_FrameEnd   = 0;
	m_TimeStart  = fwTimer::GetSystemTimeInMilliseconds();
#endif // !__NO_OUTPUT

	gameserverDebugf1("Basket checkout.  type=\"%s\", Current status=\"%s\", Attempts=\"%d\", FrameStart=\"%u\", TimeStart=\"%u\"", GetTypeName(), GetStatusString(), m_attempts, m_FrameStart, m_TimeStart);

	return true;
}

bool CNetShopTransactionBase::CanCheckout( ) const 
{
	return m_Checkout;
}

bool CNetShopTransactionBase::ApplyDataToStats(CNetShopTransactionServerDataInfo& outInfo, const bool checkProcessing)
{
	if (Pending())
		return false;

	if (checkProcessing && IsProcessing())
		return false;

	if (!Succeeded())
		return false;

	if (WasNullTransaction())
		return true;

#if !__NO_OUTPUT
	GameTransactionSessionMgr& rSession = GameTransactionSessionMgr::Get();
	gnetAssertf(rSession.HaveStatsFinishedLoading(), "Failed HaveStatsFinishedLoading()");
	if (!rSession.HaveStatsFinishedLoading())
	{
		gameserverErrorf("Failed HaveStatsFinishedLoading()");
	}
#endif

	int charSlot = StatsInterface::GetCurrentMultiplayerCharaterSlot();

	bool result = m_PlayerBalanceResponse.ApplyDataToStats(charSlot, outInfo.m_playerBalanceApplyInfo);
	if (result)
	{
		result = m_InventoryItemSetResponse.ApplyDataToStats(outInfo.m_inventoryApplyInfo);
		if (!result)
		{
			gameserverErrorf("CNetShopTransactionBase::ApplyDataToStats() - FAILED - m_InventoryItemSetResponse.ApplyDataToStats()");
		}
	}
	else
	{
		gameserverErrorf("CNetShopTransactionBase::ApplyDataToStats() - FAILED - m_PlayerBalanceResponse.ApplyDataToStats()");
	}

	return result;
}

const CNetShopTransactionBase& CNetShopTransactionBase::operator=(const CNetShopTransactionBase& other)
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
	m_TelemetryNonceResponse   = other.m_TelemetryNonceResponse;
	m_IsProcessing             = other.m_IsProcessing;
	m_TimeEnd                  = other.m_TimeEnd;
	m_attempts                 = other.m_attempts;

#if !__NO_OUTPUT
	m_FrameStart               = other.m_FrameStart;
	m_FrameEnd                 = other.m_FrameEnd;
	m_TimeStart                = other.m_TimeStart;
#endif // !__NO_OUTPUT

	return *this;
}

void CNetShopTransactionBase::DoNullServerTransaction()
{
	if (gnetVerify(!Pending()))
	{
		m_NullTransaction = true;
		NetworkGameTransactions::SendNULLEvent(NetworkInterface::GetLocalGamerIndex(), &m_Status);
	}
}

bool CNetShopTransactionBase::ShouldDoNullTransaction() const
{	
	return NETWORK_SHOPPING_MGR.ShouldDoNullTransaction();
}

void CNetShopTransactionBase::SendResultEventToScript() const
{
	mthRandom rnd((int)m_RequestId);
	const int mask1 = rnd.GetRanged(1, 63) << 26;
	const int mask2 = rnd.GetRanged(1, 63) << 26;
	
	const int bankCashDifference = m_PlayerBalanceResponse.m_applyDataToStatsInfo.m_bankCashDifference < 0 ? (int)m_PlayerBalanceResponse.m_applyDataToStatsInfo.m_bankCashDifference*-1 : (int)m_PlayerBalanceResponse.m_applyDataToStatsInfo.m_bankCashDifference;
	const int walletCashDifference = (int)m_PlayerBalanceResponse.m_applyDataToStatsInfo.m_walletCashDifference < 0 ? (int)m_PlayerBalanceResponse.m_applyDataToStatsInfo.m_walletCashDifference*-1 : (int)m_PlayerBalanceResponse.m_applyDataToStatsInfo.m_walletCashDifference;

	//Trigger Network Script Event so they can process.
	OUTPUT_ONLY(const char* result = GetResultCode() == 0 ? "Success" : "Failure");
	gameserverDebugf1( "SendResultEventToScript - Sending %s event to script - Nonce='%" I64FMT "d'", result, GetTelemetryNonce());
	CEventNetworkShopTransaction scrEvent(
		GetId()
		, GetType()
		, GetAction()
		, GetResultCode()
		, GetServiceId()
		, (mask1 | bankCashDifference)
		, (mask2 | walletCashDifference));

	GetEventScriptNetworkGroup()->Add(scrEvent);
}

int CNetShopTransactionBase::GetSlotIdForCatalogItem(const u32 catalogItemId) const
{
	//By default SLOT ID is the current character slot used by the player (From 0 to MAX_NUM_MP_CHARS)
	int slotId = StatsInterface::GetCurrentMultiplayerCharaterSlot();

	//Check if the catalog item has a valid stat and the type.
	const netCatalog& catalog = netCatalog::GetInstance();
	atHashString itemIdHashStr(catalogItemId);
	const netCatalogBaseItem* catItemBase = catalog.Find(itemIdHashStr);
	if (catItemBase)
	{
		const StatId stathash = catItemBase->GetStat();

		//Must be a valid stat id.
		if (stathash.IsValid() && StatsInterface::IsKeyValid(stathash))
		{
			const sStatDescription* statDesc = StatsInterface::GetStatDesc(stathash);
			if (statDesc && !statDesc->GetIsCharacterStat())
			{
				//Override Character slot value with magic number 5 signifying a Player Stat.
				slotId = SLOT_ID_FOR_PLAYER_STATS;
			}
		}
	}

	return slotId;
}

#if !__NO_OUTPUT
void  CNetShopTransactionBase::GrcDebugDraw( )
{
#if DEBUG_DRAW

	bool saveDebug = grcDebugDraw::GetDisplayDebugText();

	grcDebugDraw::SetDisplayDebugText(TRUE);

	static const u32 TEXT_SIZE = 256;

	const char* errorcode = "UNKNOWN";
	for (int i=0; i<SIZE_OF_GAMERSERVER_ERRORS; i++)
	{
		if (m_Status.GetResultCode() == s_GameServerErrors[i].m_id)
		{
			errorcode = s_GameServerErrors[i].m_name;
			break;
		}
	}

	Color32 color = Color_white;
	if (Failed( ))
		color = Color_red;
	else if (Pending( ))
		color = Color_yellow;
	else if (Canceled( ))
		color = Color_pink;
	else if (Succeeded( ))
		color = Color_green;

	const char* type     = GetTypeName();
	const char* category = GetCategoryName();
	const char* action   = GetActionName();

	Vector2 vTextRenderPos(s_HorizontalBorder, s_VerticalBorder + s_TextLine*s_VerticalDist);

	char mainInfo[TEXT_SIZE];
	formatf(mainInfo, sizeof(mainInfo), "[ TRANS - '%u' ] - [att:'%u', f='%u:%u', t='%ums'] [%s : %d], [%s], [%s], [%s]"
										,GetId()
										,m_attempts
										,m_FrameStart, m_FrameEnd
										,m_TimeEnd-m_TimeStart
										,errorcode, m_Status.GetResultCode()
										,strlen(type) > 16    ? &type[15]    : type      //Remove NET_SHOP_TTYPE_
										,strlen(category) > 10 ? &category[9] : category //Remove CATEGORY_
										,strlen(action) > 17  ? &action[16]  : action);  //Remove NET_SHOP_ACTION_);

	grcDebugDraw::Text(vTextRenderPos, color, mainInfo, s_drawBGQuad, s_textScale);
	s_TextLine += 1;
	vTextRenderPos.y = s_VerticalBorder + s_TextLine*s_VerticalDist;

	grcDebugDraw::SetDisplayDebugText( saveDebug );

#endif //DEBUG_DRAW
}
#endif // !__NO_OUTPUT


////////////////////////////////////////////////////////
///////////////// NetShopTransProducts /////////////////
////////////////////////////////////////////////////////

void CNetShopTransactionBasket::Init()
{
	CNetShopTransactionBase::Init();
}

void CNetShopTransactionBasket::Shutdown()
{
	CNetShopTransactionBase::Shutdown();
}

void CNetShopTransactionBasket::Cancel()
{
	CNetShopTransactionBase::Cancel();
}

u8  CNetShopTransactionBasket::Find(const NetShopItemId itemId) const
{
	for (u8 i=0; i<m_Size; i++)
	{
		if (itemId == m_Items[i].m_Id)
			return i;
	}

	return MAX_NUMBER_ITEMS;
}

bool CNetShopTransactionBasket::Add(const CNetShopItem& item)
{
	if (gnetVerify(None()) && gnetVerify(m_Size < MAX_NUMBER_ITEMS))
	{
		m_Items[m_Size] = item;
		m_Size++;
		return true;
	}

	return false;
}

bool CNetShopTransactionBasket::Remove(const NetShopItemId itemId)
{
	const u8 idx = Find( itemId );

	if (gnetVerify(None()) && gnetVerify(MAX_NUMBER_ITEMS > idx))
	{
		m_Size--;

		m_Items[idx].Clear();

		//Shift left
		for (u8 i=idx; i+1<MAX_NUMBER_ITEMS; i++)
			m_Items[i] = m_Items[i+1];

		return true;
	}

	return false;
}

void CNetShopTransactionBasket::Clear( )
{
	if ( gnetVerify(!Pending()) )
	{
		CNetShopTransactionBase::Clear();

		m_Size = 0;

		for (u8 i=0; i+1<MAX_NUMBER_ITEMS; i++)
			m_Items[i].Clear();
	}
}

int CNetShopTransactionBasket::GetPrice(const NetShopItemId itemId) const
{
	const u8 idx = Find( itemId );

	if (gnetVerify(MAX_NUMBER_ITEMS > idx))
	{
		return m_Items[idx].m_Price;
	}

	return 0;
}

int CNetShopTransactionBasket::GetTotalPrice(bool& propertieTradeIsCredit, int& numProperties, bool& hasOverrideItem) const
{
	hasOverrideItem = false;

	int totalprice = 0;

	propertieTradeIsCredit = false;
	numProperties = 0;

	const netCatalog& catalogue = netCatalog::GetInstance();

	//NET_SHOP_ACTION_BUY_UNLOCK - Will only happen in CnC
	//
	//  0: unlock being purchased
	//		{
	//				item: <CATEGORY_UNLOCK itemId>,
	//				price: <number> (cost of item (in the appropriate currency)),
	//				value: 1
	//		}
	//  1: the currency to use to purchase it
	//		{
	//			item: <CATEGORY_INVENTORY_CURRENCY itemId> The currency slot we want to pay for it out of.
	//			price: <number> the amount of currency we’re spending (should be the same as the item price),
	//			value: <CATEGORY_CURRENCY_TYPE, the type of currency being used>
	//		}
	if (m_Action == NET_SHOP_ACTION_BUY_UNLOCK)
	{
		if (m_Size == 2)
		{
			atHashString keyUnlock((u32) m_Items[0].m_Id);
			const netCatalogBaseItem* itemUnlock = catalogue.Find(keyUnlock);

			atHashString keyCurrency((u32) m_Items[1].m_Id);
			const netCatalogBaseItem* itemCurrency = catalogue.Find(keyCurrency);

			if (gnetVerify(itemUnlock) && gnetVerify(itemCurrency))
			{
				const int TYPE_TOKEN = ATSTRINGHASH("CNC_PURCHASE_TYPE_TOKEN", 0xd3818eed);
				// Tokens - should always be just one
				if (itemCurrency->GetKeyHash() == TYPE_TOKEN)
				{
				}
				else
				{
					//Add the cost of the unlock
					totalprice += m_Items[0].m_Price;
				}
			}
		}
	}
	//NET_SHOP_ACTION_BUY_PROPERTY
	//NET_SHOP_ACTION_BUY_WAREHOUSE
	//  0: Property to buy -- catalog price
	//  1: Property to sell -- 50% of catalog price
	else if (m_Action == NET_SHOP_ACTION_BUY_PROPERTY || m_Action == NET_SHOP_ACTION_BUY_WAREHOUSE)
	{
		for (int i=0; i<m_Size; i++)
		{
			atHashString key((u32) m_Items[i].m_Id);
			const netCatalogBaseItem* item = catalogue.Find(key);

			if (gnetVerify(item))
			{
				if (CATEGORY_INVENTORY_PROPERTIE == item->GetCategory() || CATEGORY_INVENTORY_WAREHOUSE == item->GetCategory())
				{
					numProperties += 1;

					if (i == 0)
						totalprice += m_Items[i].m_Price;
					else
						totalprice -= m_Items[i].m_Price;
				}
				else if (CATEGORY_INVENTORY_PROPERTY_INTERIOR == item->GetCategory() || CATEGORY_INVENTORY_WAREHOUSE_INTERIOR == item->GetCategory())
				{
					totalprice += m_Items[i].m_Price;
				}
			}
		}

		if (totalprice < 0)
		{
			propertieTradeIsCredit = true;
			totalprice *= -1;
		}
	}
	//Default is to add up all 
	else
	{
		for (u8 i=0; i<m_Size; i++)
		{
			atHashString key((u32) m_Items[i].m_Id );

			const netCatalogBaseItem* item = catalogue.Find( key );
			if (gnetVerify( item ))
			{
				//Don't add up items with modifiers.
				if (item->GetCategory() == CATEGORY_PRICE_MODIFIER)
				{
					hasOverrideItem = true;
					continue;
				}

				//Don't add up items with overrides.
				if (item->GetCategory() == CATEGORY_PRICE_OVERRIDE)
				{
					hasOverrideItem = true;
					continue;
				}

				totalprice += m_Items[i].m_Price * m_Items[i].m_Quantity;
			}
		}
	}

	return totalprice;
}

void CNetShopTransactionBasket::ProcessSuccess( )
{
	//Done
	NETWORK_SHOPPING_MGR.GetCashReductions().Finished(static_cast<int>(GetId()));

	//Apply Server Data to Stats
	CNetShopTransactionServerDataInfo outInfo;
	ApplyDataToStats(outInfo, false);

	//Trigger Network Script Event so they can process the success.
	SendResultEventToScript();
}

const CNetShopTransactionBasket& CNetShopTransactionBasket::operator=(const CNetShopTransactionBasket& other)
{
	for (int i=0; i<MAX_NUMBER_ITEMS; i++)
		m_Items[i] = other.m_Items[i];

	m_Size = other.m_Size;

	return *this;
}

bool CNetShopTransactionBasket::ProcessingStart( )
{
	gameserverDebugf1("CNetShopTransactionBasket::ProcessingStart.");

	if (Pending())
	{
		gameserverErrorf("ProcessingStart() - FAILED - Tried to start CNetShopTransactionBasket while pending");
		return false;
	}

	if (ShouldDoNullTransaction())
	{
		gameserverDebugf1("Starting CNetShopTransactionBasket as NULL transaction");
		DoNullServerTransaction();
		return true;
	}

	PlayerAccountId acctId = InvalidPlayerAccountId;
	const rlRosCredentials& cred = rlRos::GetCredentials( NetworkInterface::GetLocalGamerIndex() );
	if(gnetVerify(cred.IsValid() && InvalidPlayerAccountId != cred.GetPlayerAccountId()))
	{
		acctId = cred.GetPlayerAccountId();
	}
	else
	{
		gameserverErrorf("ProcessingStart() - FAILED - to get the account id.");
		return false;
	}

	//Fill out the transaction object.
	NetworkGameTransactions::SpendEarnTransaction transactionObj;
	if (!GetTransactionObj(transactionObj))
	{
		gameserverErrorf("GetTransactionObj() - FAILED.");
		return false;
	}

	return FinishProcessingStart( transactionObj );
}

bool CNetShopTransactionBasket::GetTransactionObj(NetworkGameTransactions::SpendEarnTransaction& transactionObj) const
{
	int charSlot = StatsInterface::GetCurrentMultiplayerCharaterSlot();

	//When we trade properties we might end up in credit.
	bool hasOverrideItem = false;
	bool isPropertyTradeCredit = false;
	int numProperties = 0;
	s64 totalPrice = GetTotalPrice(isPropertyTradeCredit, numProperties, hasOverrideItem);

	s64 amtWallet = 0;
	s64 amtBank = 0;

	//Calculate the amounts
	if (CATALOG_ITEM_FLAG_EVC_ONLY&m_Flags && totalPrice > MoneyInterface::GetEVCBalance())
	{
		gameserverErrorf("GetTransactionObj() - FAILED - EVC_ONLY - Not enough cash in EVC balance.");
		OUTPUT_ONLY( SpewItems(); )
		return false;
	}

	if (CATALOG_ITEM_FLAG_TOKEN&m_Flags)
	{
		/* using Tokens on Validation */
	}
	else if (CATALOG_ITEM_FLAG_WALLET_ONLY&m_Flags)
	{
		const s64 walletCash = MoneyInterface::GetVCWalletBalance();
		if ( GetIsEarnAction() || hasOverrideItem || gnetVerify(totalPrice <= walletCash) )
		{
			amtWallet = totalPrice;
		}
		else
		{
			gameserverErrorf("GetTransactionObj() - FAILED - Not enough cash.");
			OUTPUT_ONLY( SpewItems(); )
			return false;
		}
	}
	else if (CATALOG_ITEM_FLAG_BANK_ONLY&m_Flags || (numProperties > 1 && isPropertyTradeCredit))
	{
		const s64 bankCash = MoneyInterface::GetVCBankBalance();
		if ( GetIsEarnAction() || (numProperties > 1 && isPropertyTradeCredit) || hasOverrideItem || gnetVerify(totalPrice <= bankCash) )
		{
			amtBank = totalPrice;
		}
		else
		{
			gameserverErrorf("GetTransactionObj() - FAILED - Not enough cash.");
			OUTPUT_ONLY( SpewItems(); )
			return false;
		}
	}
	else if (CATALOG_ITEM_FLAG_BANK_THEN_WALLET&m_Flags)
	{
		const s64 bankCash   = MoneyInterface::GetVCBankBalance();
		const s64 walletCash = MoneyInterface::GetVCWalletBalance();

		if ( GetIsEarnAction() || (totalPrice <= bankCash) )
		{
			amtBank = totalPrice;
		}
		else if ( hasOverrideItem || gnetVerify(totalPrice <= (bankCash + walletCash)) )
		{
			amtBank = bankCash;
			amtWallet = totalPrice - bankCash;
		}
		else
		{
			gameserverErrorf("GetTransactionObj() - FAILED - Not enough cash.");
			OUTPUT_ONLY( SpewItems(); )
			return false;
		}
	}
	else if (gnetVerify(CATALOG_ITEM_FLAG_WALLET_THEN_BANK&m_Flags))
	{
		const s64 bankCash   = MoneyInterface::GetVCBankBalance();
		const s64 walletCash = MoneyInterface::GetVCWalletBalance();

		if ( GetIsEarnAction() || totalPrice <= walletCash )
		{
			amtWallet = totalPrice;
		}
		else if ( hasOverrideItem || gnetVerify(totalPrice <= (bankCash + walletCash)) )
		{
			amtWallet = walletCash;
			amtBank = totalPrice - walletCash;
		}
		else
		{
			gameserverErrorf("GetTransactionObj() - FAILED - Not enough cash.");
			OUTPUT_ONLY( SpewItems(); )
			return false;
		}
	}
	else
	{
		gameserverErrorf("GetTransactionObj() - FAILED - Invalid flags='%d'.", m_Flags);
		OUTPUT_ONLY( SpewItems(); )
		return false;
	}

	//Override Character slot value with magic number 5 signifying a Player Stat.
	for (int i=0; i<MAX_NUMBER_ITEMS; i++)
	{
		if(m_Items[i].IsValid())
		{
			if (GetSlotIdForCatalogItem((u32)m_Items[i].m_Id) == SLOT_ID_FOR_PLAYER_STATS)
			{
				charSlot = SLOT_ID_FOR_PLAYER_STATS;
				break;
			}
		}
	}

	//Fill out the transaction object.
	transactionObj.Reset(charSlot, (long)amtBank, (long)amtWallet);

	for (int i=0; i<MAX_NUMBER_ITEMS; i++)
	{
		if(m_Items[i].IsValid())
		{
			long value = m_Items[i].m_ExtraInventoryId != NET_SHOP_INVALID_ITEM_ID ? m_Items[i].m_ExtraInventoryId : m_Items[i].m_Quantity;
			transactionObj.PushItem(m_Items[i].m_Id, value, m_Items[i].m_Price, CATALOG_ITEM_FLAG_EVC_ONLY&m_Flags ? true : false);
		}
	}

	return true;
}

#if !__NO_OUTPUT

void  CNetShopTransactionBasket::GrcDebugDraw( )
{
#if DEBUG_DRAW

	CNetShopTransactionBase::GrcDebugDraw( );

	bool saveDebug = grcDebugDraw::GetDisplayDebugText();

	grcDebugDraw::SetDisplayDebugText(TRUE);

	static const u32 TEXT_SIZE = 256;

	Color32 color = Color_white;
	if (Failed( ))
		color = Color_red;
	else if (Pending( ))
		color = Color_yellow;
	else if (Canceled( ))
		color = Color_pink;
	else if (Succeeded( ))
		color = Color_green;

	char mainInfo[TEXT_SIZE];

	Vector2 vTextRenderPos(s_HorizontalItemBorder, s_VerticalBorder + s_TextLine*s_VerticalDist);

	for (int i=0; i<m_Size; i++)
	{
		if (m_Items[i].IsValid())
		{
			formatf(mainInfo, sizeof(mainInfo), "[Item '%d' - p='%d', id='%d', e_id='%d', val='%d', qtt='%d'"
				,i
				,m_Items[i].m_Price
				,m_Items[i].m_Id
				,m_Items[i].m_ExtraInventoryId
				,m_Items[i].m_StatValue
				,m_Items[i].m_Quantity);

			grcDebugDraw::Text(vTextRenderPos, color, mainInfo, s_drawBGQuad, s_textScale);
			s_TextLine += 1;
			vTextRenderPos.y = s_VerticalBorder + s_TextLine*s_VerticalDist;
		}
	}

	grcDebugDraw::SetDisplayDebugText( saveDebug );

#endif // DEBUG_DRAW
}


void  CNetShopTransactionBasket::SpewItems( ) const
{
	OUTPUT_ONLY( bool propertieTradeIsCredit = false; )
	OUTPUT_ONLY( int numProperties = 0; )
	OUTPUT_ONLY( bool hasOverrideItem = false; )

	gameserverDebugf1("Basket - TotalPrice='%d', action='%u', IsEarnAction='%s', hasOverrideItem='%s'.", GetTotalPrice(propertieTradeIsCredit, numProperties, hasOverrideItem), GetAction(), GetIsEarnAction() ? "True" : "False", hasOverrideItem ? "True" : "False" );

	for (int i=0; i<m_Size; i++)
	{
		if (m_Items[i].IsValid())
		{
			gameserverDebugf1("... Item '%d' - p='%d', id='%d', e_id='%d', val='%d', qtt='%d'"
				,i
				,m_Items[i].m_Price
				,m_Items[i].m_Id
				,m_Items[i].m_ExtraInventoryId
				,m_Items[i].m_StatValue
				,m_Items[i].m_Quantity);
		}
	}
}

#endif // !__NO_OUTPUT

////////////////////////////////////////////////////////////
///////////////// NetShopTransSpendService /////////////////
////////////////////////////////////////////////////////////

void CNetShopTransaction::Init()
{
	CNetShopTransactionBase::Init();
	gnetAssert(GetItemId() != NET_SHOP_INVALID_ITEM_ID);
}

void CNetShopTransaction::Shutdown()
{
	CNetShopTransactionBase::Shutdown();
}

void CNetShopTransaction::Cancel()
{
	CNetShopTransactionBase::Cancel();
}

void CNetShopTransaction::Clear()
{
	gameserverAssertf(false, "Can not clear this transaction.");
}

bool CNetShopTransaction::ProcessingStart( )
{
	if (Pending())
	{
		gameserverErrorf("ProcessingStart() - FAILED - Tried to start CNetShopTransactionBasket while pending");
		return false;
	}

	if (ShouldDoNullTransaction())
	{
		gameserverDebugf1("Starting CNetShopTransactionBasket as NULL transaction");
		DoNullServerTransaction();
		return true;
	}

	PlayerAccountId acctId = InvalidPlayerAccountId;
	const rlRosCredentials& cred = rlRos::GetCredentials( NetworkInterface::GetLocalGamerIndex() );
	if(gnetVerify(cred.IsValid() && InvalidPlayerAccountId != cred.GetPlayerAccountId()))
	{
		acctId = cred.GetPlayerAccountId();
	}
	else
	{
		gameserverErrorf("ProcessingStart() - FAILED - failed to get the account id.");
		return false;
	}

	//Fill out the transaction object.
	NetworkGameTransactions::SpendEarnTransaction transactionObj;
	if (!GetTransactionObj(transactionObj))
	{
		gameserverErrorf("GetTransactionObj() - FAILED.");
		return false;
	}

#if __FINAL
	const NetShopItemId itemidver = (NetShopItemId)ATSTRINGHASH("SERVICE_EARN_DEBUG", 0x762d6bf6);
	if (m_Item.m_Id == itemidver)
	{
		MetricLAST_VEH metric;
		metric.m_fields[0] = (int)itemidver;
		metric.m_fields[1] = (int)ATSTRINGHASH("Cash", 0x69e6cd7e);
		metric.m_fields[2] = (int)(m_Item.m_Price*m_Item.m_Quantity); 

		const rlGamerHandle& fromGH = NetworkInterface::GetLocalGamerHandle();
		fromGH.ToString(metric.m_FromGH, COUNTOF(metric.m_FromGH));
		fromGH.ToString(metric.m_ToGH, COUNTOF(metric.m_ToGH));

		CNetworkTelemetry::AppendSecurityMetric(metric);

		return false;
	}
#endif // __FINAL

	return FinishProcessingStart( transactionObj );
}

bool CNetShopTransaction::GetTransactionObj(NetworkGameTransactions::SpendEarnTransaction& transactionObj) const
{
	//Default all stats are character stats.
	int charSlot = StatsInterface::GetCurrentMultiplayerCharaterSlot();

	s64 totalPrice = m_Item.m_Quantity * m_Item.m_Price;

	s64 amtWallet = 0;
	s64 amtBank = 0;

	//Calculate the amounts
	if (CATALOG_ITEM_FLAG_EVC_ONLY&m_Flags && totalPrice > MoneyInterface::GetEVCBalance())
	{
		gameserverErrorf("GetTransactionObj() - FAILED - EVC_ONLY - Not enough cash in EVC balance.");
		OUTPUT_ONLY( SpewItems(); );
		return false;
	}

	//Calculate the amounts
	if (CATALOG_ITEM_FLAG_TOKEN&m_Flags)
	{
		/* using Tokens on Validation */
	}
	else if (CATALOG_ITEM_FLAG_WALLET_ONLY&m_Flags)
	{
		const s64 walletCash = MoneyInterface::GetVCWalletBalance();
		if ( GetIsEarnAction() || gnetVerify(totalPrice <= walletCash) )
		{
			amtWallet = totalPrice;
		}
		else
		{
			gameserverErrorf("GetTransactionObj() - FAILED - not enough cash.");
			OUTPUT_ONLY( SpewItems(); )
			return false;
		}
	}
	else if (CATALOG_ITEM_FLAG_BANK_ONLY&m_Flags)
	{
		const s64 bankCash = MoneyInterface::GetVCBankBalance();
		if ( GetIsEarnAction() || gnetVerify(totalPrice <= bankCash) )
		{
			amtBank = totalPrice;
		}
		else
		{
			gameserverErrorf("GetTransactionObj() - FAILED - not enough cash.");
			OUTPUT_ONLY( SpewItems(); )
				return false;
		}
	}
	else if (CATALOG_ITEM_FLAG_BANK_THEN_WALLET&m_Flags)
	{
		const s64 bankCash   = MoneyInterface::GetVCBankBalance();
		const s64 walletCash = MoneyInterface::GetVCWalletBalance();

		if ( GetIsEarnAction() || totalPrice <= bankCash )
		{
			amtBank = totalPrice;
		}
		else if ( gnetVerify(totalPrice <= (bankCash + walletCash)) )
		{
			amtBank = bankCash;
			amtWallet = totalPrice - bankCash;
		}
		else
		{
			gameserverErrorf("GetTransactionObj() - FAILED - not enough cash.");
			OUTPUT_ONLY( SpewItems(); )
			return false;
		}
	}
	else if (gnetVerify(CATALOG_ITEM_FLAG_WALLET_THEN_BANK&m_Flags))
	{
		const s64 bankCash   = MoneyInterface::GetVCBankBalance();
		const s64 walletCash = MoneyInterface::GetVCWalletBalance();

		if ( GetIsEarnAction() || totalPrice <= walletCash)
		{
			amtWallet = totalPrice;
		}
		else if (gnetVerify(totalPrice <= (bankCash + walletCash)))
		{
			amtWallet = walletCash;
			amtBank = totalPrice - walletCash;
		}
		else
		{
			gameserverErrorf("GetTransactionObj() - FAILED - not enough cash.");
			OUTPUT_ONLY( SpewItems(); )
			return false;
		}
	}
	else
	{
		gameserverErrorf("GetTransactionObj() - FAILED - Invalid flags='%d'.", m_Flags);
		OUTPUT_ONLY( SpewItems(); )
		return false;
	}

	if(!AddApplicableBonusReport(m_Item))
	{
		gameserverErrorf("GetTransactionObj() - FAILED - Duplicate / already reported bonus metric.");
		OUTPUT_ONLY( SpewItems(); )
		return false;
	}

	//Override Character slot value with magic number 5 signifying a Player Stat.
	charSlot = GetSlotIdForCatalogItem((u32)m_Item.m_Id);

	//Fill out the transaction object.
	transactionObj.Reset(charSlot, (long)amtBank, (long)amtWallet);

	long value = m_Item.m_ExtraInventoryId != NET_SHOP_INVALID_ITEM_ID ? m_Item.m_ExtraInventoryId : m_Item.m_Quantity;
	transactionObj.PushItem(m_Item.m_Id, value == 0 ? 1 : value, m_Item.m_Price, CATALOG_ITEM_FLAG_EVC_ONLY&m_Flags ? true : false);

	return true;
}

void CNetShopTransaction::ProcessSuccess( )
{
	//Done
	NETWORK_SHOPPING_MGR.GetCashReductions().Finished(static_cast<int>(GetId()));

	//Apply Server Data to Stats
	CNetShopTransactionServerDataInfo outInfo;
	ApplyDataToStats(outInfo, false);

	//Trigger Network Script Event so they can process the success.
	SendResultEventToScript();
}

const CNetShopTransaction& CNetShopTransaction::operator=(const CNetShopTransaction& other)
{
	m_Item = other.m_Item;
	return *this;
}

#if !__NO_OUTPUT
#define MAX_NUMBER_TRANSACTION_STATUS 5
atFixedArray< sStandaloneGameTransactionStatus, MAX_NUMBER_TRANSACTION_STATUS > s_StandaloneTransactionStatusHistory;

void  sStandaloneGameTransactionStatus::GrcDebugDraw( )
{
#if DEBUG_DRAW

	if (!m_status)
		return;

	bool saveDebug = grcDebugDraw::GetDisplayDebugText();

	grcDebugDraw::SetDisplayDebugText(TRUE);

	static const u32 TEXT_SIZE = 256;

	const char* errorcode = "UNKNOWN";
	for (int i=0; i<SIZE_OF_GAMERSERVER_ERRORS; i++)
	{
		if (m_status->GetResultCode() == s_GameServerErrors[i].m_id)
		{
			errorcode = s_GameServerErrors[i].m_name;
			break;
		}
	}

	Color32 color = Color_white;
	if (m_status->Failed( ))
		color = Color_red;
	else if (m_status->Pending( ))
		color = Color_yellow;
	else if (m_status->Canceled( ))
		color = Color_pink;
	else if (m_status->Succeeded( ))
		color = Color_green;

	Vector2 vTextRenderPos(s_HorizontalBorder, s_VerticalBorder + s_TextLine*s_VerticalDist);

	char mainInfo[TEXT_SIZE];
	formatf(mainInfo, sizeof(mainInfo), "[ TRANS - '%s' ] - [%s : %d]"
		,m_name
		,errorcode
		,m_status->GetResultCode());

	grcDebugDraw::Text(vTextRenderPos, color, mainInfo, s_drawBGQuad, s_textScale);
	s_TextLine += 1;
	vTextRenderPos.y = s_VerticalBorder + s_TextLine*s_VerticalDist;

	grcDebugDraw::SetDisplayDebugText( saveDebug );

#endif //DEBUG_DRAW
}



void  CNetShopTransaction::GrcDebugDraw( )
{
#if DEBUG_DRAW

	CNetShopTransactionBase::GrcDebugDraw( );

	bool saveDebug = grcDebugDraw::GetDisplayDebugText();

	grcDebugDraw::SetDisplayDebugText(TRUE);

	static const u32 TEXT_SIZE = 256;

	Color32 color = Color_white;
	if (Failed( ))
		color = Color_red;
	else if (Pending( ))
		color = Color_yellow;
	else if (Canceled( ))
		color = Color_pink;
	else if (Succeeded( ))
		color = Color_green;

	char mainInfo[TEXT_SIZE];

	//netCatalog& catalogue = netCatalog::GetInstance();
	//atHashString key((u32)m_Item.m_Id);
	//const netCatalogBaseItem* item = catalogue.Find(key);

	Vector2 vTextRenderPos(s_HorizontalItemBorder, s_VerticalBorder + s_TextLine*s_VerticalDist);

	formatf(mainInfo, sizeof(mainInfo), "[Item - p='%d', id='%d', e_id='%d', val='%d', qtt='%d'"
		,m_Item.m_Price
		,m_Item.m_Id
		,m_Item.m_ExtraInventoryId
		,m_Item.m_StatValue
		,m_Item.m_Quantity);

	grcDebugDraw::Text(vTextRenderPos, color, mainInfo, s_drawBGQuad, s_textScale);
	s_TextLine += 1;
	vTextRenderPos.y = s_VerticalBorder + s_TextLine*s_VerticalDist;

	grcDebugDraw::SetDisplayDebugText( saveDebug );

#endif // DEBUG_DRAW
}

void  CNetShopTransaction::SpewItems( ) const
{
	gameserverDebugf1("Service - TotalPrice='%d', action='%u', actionIsEarn='%s'", GetPrice(), GetAction(), GetIsEarnAction() ? "True" : "False");
	gameserverDebugf1("... Item - p='%d', id='%d', e_id='%d', val='%d', qtt='%d'"
		,m_Item.m_Price
		,m_Item.m_Id
		,m_Item.m_ExtraInventoryId
		,m_Item.m_StatValue
		,m_Item.m_Quantity);
}

#endif // !__NO_OUTPUT

//////////////////////////////////////////////////////
///////////////// NetworkShoppingMgr /////////////////
//////////////////////////////////////////////////////

CNetworkShoppingMgr::CNetworkShoppingMgr( ) 
	: m_Allocator(NULL)
	, m_LoadCatalogFromCache(true)
	, m_transactionInProgress(false)
{
	m_TransactionList.Empty();
	OUTPUT_ONLY( m_TransactionListHistory.Reset( ); )
	OUTPUT_ONLY( s_StandaloneTransactionStatusHistory.Reset( ); )

#if __BANK
	m_bankLatency = 0;
	m_bankOverrideLatency = false;
	WIN32PC_ONLY( m_bankTestAsynchTransactions = false; )
#endif // __BANK
}

CNetworkShoppingMgr::~CNetworkShoppingMgr()
{
	Shutdown(SHUTDOWN_CORE);
}

void CNetworkShoppingMgr::InitCatalog( )
{
	if (m_LoadCatalogFromCache)
	{
		m_LoadCatalogFromCache = false;
		if(!NETWORK_SHOPPING_MGR.ShouldDoNullTransaction())
		{
			netCatalog::GetInstance().Init();
			netCatalog::GetInstance().LoadFromCache();
		}
	}
}

void CNetworkShoppingMgr::Init(sysMemAllocator* allocator)
{
	gnetAssert(allocator);
	if(!allocator)
		return;

	m_Allocator = allocator;

	sysMemAllocator* previousallocator = &sysMemAllocator::GetCurrent();
	sysMemAllocator::SetCurrent(*m_Allocator);

	CNetShopTransactionBase::InitPool( MEMBUCKET_NETWORK );
	atDTransactionNode::InitPool( MEMBUCKET_NETWORK );

	sysMemAllocator::SetCurrent(*previousallocator);

#if	__DEV
	PARSER.LoadObject("common:/data/debug/networkshop_debug", "meta", *this);
#else
	PARSER.LoadObject("common:/data/networkshop", "meta", *this);
#endif // __DEV

#if __BANK

	m_bankLatency          = 0;
	m_bankOverrideLatency  = false;

	WIN32PC_ONLY( m_bankTestAsynchTransactions = false; )

	PARAM_netshoplatency.Get( m_bankLatency );

	if (m_bankLatency > 0)
		m_bankOverrideLatency = true;

#endif // __BANK

	OUTPUT_ONLY( s_ActivateDebugDraw = PARAM_netshoptransactiondebug.Get(); )
}

void CNetworkShoppingMgr::CancelTransactions ()
{
	atDNode< CNetShopTransactionBase*, datBase >* node     = m_TransactionList.GetHead();
	atDNode< CNetShopTransactionBase*, datBase >* nextNode = node;
	while (node)
	{
		nextNode = node->GetNext();

		CNetShopTransactionBase* transaction = node->Data;
		if (transaction)
		{
			if (transaction->Pending())
				transaction->Cancel();
		}

		node = nextNode;
	}
}
void CNetworkShoppingMgr::Shutdown(const u32 shutdownMode)
{
	if (shutdownMode != SHUTDOWN_CORE)
	{
		//Cleanup any in-flight transactions.
		if (shutdownMode == SHUTDOWN_SESSION)
		{
			CancelTransactions();
			BANK_ONLY( Bank_ClearVisualDebug( ); )
		}

		return;
	}

	//Delete all
	atDNode< CNetShopTransactionBase*, datBase >* node = m_TransactionList.GetHead();
	while (node)
	{
		if (node->Data)
		{
			delete node->Data;
			node->Data = NULL;
		}
		node = node->GetNext();
	}
	m_TransactionList.DeleteAll();

	BANK_ONLY( Bank_ClearVisualDebug( ); )

	CNetwork::UseNetworkHeap(NET_MEM_LIVE_MANAGER_AND_SESSIONS);
	CNetShopTransactionBase::ShutdownPool( );
	atDTransactionNode::ShutdownPool( );
	CNetwork::StopUsingNetworkHeap( );
}

void CNetworkShoppingMgr::Update()
{
	if(!NetworkShoppingMgrSingleton::IsInstantiated())
		return;

	gnetAssertf(sysThreadType::IsUpdateThread(), "CNetworkShoppingMgr::Update() used from another thread than the main thread, this may not be safe.");

	m_transactionInProgress = false;

	CNetShopTransactionBase* pendingTransaction = 0;

	//Check for pending requests.
	atDNode< CNetShopTransactionBase*, datBase >* node     = m_TransactionList.GetHead();
	atDNode< CNetShopTransactionBase*, datBase >* nextNode = node;
	while (node)
	{
		nextNode = node->GetNext();
		CNetShopTransactionBase* transaction = node->Data;
		if (transaction)
		{
			if (transaction->Pending())
			{
				pendingTransaction = transaction;
				m_transactionInProgress = true;
			}
		}

		if(!TransactionInProgress())
			node = nextNode;
		else
			node = 0;
	}

	//Update the pending transaction.
	if (pendingTransaction)
	{
		pendingTransaction->Update();

		//Transactions that have Succeeded.
		// NOTE: Remove the failed delete because we need to deal with this differently.
		if (pendingTransaction->Succeeded( ))
		{
			gnetDebug1("[%u] [%" I64FMT "d] Transaction SUCCEEDED. type=\"%s\", status=\"%s\"", pendingTransaction->GetId(), pendingTransaction->GetRequestId(), pendingTransaction->GetTypeName(), pendingTransaction->GetStatusString());
		}
		else if (pendingTransaction->Canceled( ))
		{
			gnetError("[%u] [%" I64FMT "d] Transaction CANCELED. type=\"%s\", status=\"%s\"", pendingTransaction->GetId(), pendingTransaction->GetRequestId(), pendingTransaction->GetTypeName(), pendingTransaction->GetStatusString());
		}
		//Deal with failed transactions - Either code/script needs to retry.
		else if (pendingTransaction->Failed( ))
		{
			gnetError("[%u] [%" I64FMT "d] Transaction FAILED. SCRIPT CAN RETRY. Not Removing Transaction type=\"%s\",  status=\"%s\"", pendingTransaction->GetId(), pendingTransaction->GetRequestId(), pendingTransaction->GetTypeName(), pendingTransaction->GetStatusString());
		}
	}

	//No transactions pending start a new one.
	if (!NETWORK_SHOPPING_MGR.TransactionInProgress())
	{
		atDNode< CNetShopTransactionBase*, datBase >* node     = m_TransactionList.GetHead();
		atDNode< CNetShopTransactionBase*, datBase >* nextNode = node;

		//Check for pending requests.
		while (node)
		{
			nextNode = node->GetNext();

			CNetShopTransactionBase* transaction = node->Data;
			if (transaction)
			{
				//Update the transaction
				transaction->Update();

				//Start the transaction checkout process.
				GameTransactionSessionMgr& rSession = GameTransactionSessionMgr::Get();
				if (!rSession.IsQueueBusy() && rSession.IsReady())
				{
					if (transaction->None( ) && transaction->CanCheckout( ) && !rSession.IsRefreshPending() && !rSession.IsRefreshCatalogPending())
					{
						gnetDebug1("[%u] [%" I64FMT "d] Checkout Transaction type=\"%s\".", transaction->GetId(), transaction->GetRequestId(), transaction->GetTypeName());

						if (transaction->ProcessingStart())
						{
							gnetDebug1("[%u] [%" I64FMT "d] Started Checkout of Transaction type=\"%s\".", transaction->GetId(), transaction->GetRequestId(), transaction->GetTypeName());
						}

						//Flag startup.
						if (transaction->Pending())
						{
							m_transactionInProgress = true;
						}
						else
						{
							gnetDebug1("[%u] [%" I64FMT "d] Failed - Transaction type=\"%s\".", transaction->GetId(), transaction->GetRequestId(), transaction->GetTypeName());
							OUTPUT_ONLY( transaction->SpewItems( ); )

							transaction->ForceFailure();
						}
					}
					else if (transaction->Canceled( ))
					{
						gnetDebug1("[%u] [%" I64FMT "d] Canceled - Transaction type=\"%s\".", transaction->GetId(), transaction->GetRequestId(), transaction->GetTypeName());
						OUTPUT_ONLY( transaction->SpewItems( ); )

						delete transaction;
						node->Data = NULL;
						m_TransactionList.PopNode(*node);
						delete node;
					}
				}
			}

			if(!TransactionInProgress())
				node = nextNode;
			else
				node = 0;
		}
	}

	OUTPUT_ONLY( UpdateGrcDebugDraw(); )
}


#if !__NO_OUTPUT

void CNetworkShoppingMgr::UpdateGrcDebugDraw()
{
	if(!NetworkShoppingMgrSingleton::IsInstantiated())
		return;

	if (!NetworkInterface::IsNetworkOpen())
		return;

	if (!s_ActivateDebugDraw)
		return;

	//Initial render setup.
	s_TextLine = 0;

#if DEBUG_DRAW
		bool saveDebug = grcDebugDraw::GetDisplayDebugText();
		grcDebugDraw::SetDisplayDebugText(TRUE);

		static const u32 TEXT_SIZE = 256;
		Color32 color = Color_white;
		Vector2 vTextRenderPos(s_HorizontalBorder, s_VerticalBorder + s_TextLine*s_VerticalDist);

		char mainInfo[TEXT_SIZE];
		formatf(mainInfo, sizeof(mainInfo), " CATALOG - isValid=['%s':NumItems='%u'], ver=[local='%u', server='%u'], session=['%s']"
			,netCatalog::GetInstance().GetNumItems() > 7000 ? "True" : "False"
			,netCatalog::GetInstance().GetNumItems()
			,netCatalog::GetInstance().GetVersion()
			,netCatalog::GetInstance().GetLatestServerVersion()
			,GameTransactionSessionMgr::Get().GetStateString());

		grcDebugDraw::Text(vTextRenderPos, color, mainInfo, s_drawBGQuad, s_textScale);
		s_TextLine += 1;
		vTextRenderPos.y = s_VerticalBorder + s_TextLine*s_VerticalDist;

		formatf(mainInfo, sizeof(mainInfo), " CASH [EVC='%" I64FMT "d', PVC='%" I64FMT "d', BANK=['%" I64FMT "d'], W=['%" I64FMT "d','%" I64FMT "d']]"
			,StatsInterface::GetInt64Stat(STAT_CLIENT_EVC_BALANCE)
			,StatsInterface::GetInt64Stat(STAT_CLIENT_PVC_BALANCE)
			,StatsInterface::GetInt64Stat(STAT_BANK_BALANCE)
			,StatsInterface::GetInt64Stat(STAT_WALLET_BALANCE.GetHash(CHAR_MP0))
			,StatsInterface::GetInt64Stat(STAT_WALLET_BALANCE.GetHash(CHAR_MP1)));

		grcDebugDraw::Text(vTextRenderPos, color, mainInfo, s_drawBGQuad, s_textScale);
		s_TextLine += 1;
		vTextRenderPos.y = s_VerticalBorder + s_TextLine*s_VerticalDist;

		grcDebugDraw::SetDisplayDebugText( saveDebug );
#endif //DEBUG_DRAW

	atDNode< CNetShopTransactionBase*, datBase >* node     = m_TransactionList.GetHead();
	atDNode< CNetShopTransactionBase*, datBase >* nextNode = node;
	while (node)
	{
		nextNode = node->GetNext();

		CNetShopTransactionBase* transaction = node->Data;
		if (transaction)
			transaction->GrcDebugDraw();

		node = nextNode;
	}

	int i = m_TransactionListHistory.GetCount()-1;
	for (; i>0; i--)
		m_TransactionListHistory[i]->GrcDebugDraw();

	i = s_StandaloneTransactionStatusHistory.GetCount()-1;
	for (; i>0; i--)
		s_StandaloneTransactionStatusHistory[i].GrcDebugDraw();
}

void CNetworkShoppingMgr::AddToGrcDebugDraw(sStandaloneGameTransactionStatus& debug)
{
	if (s_StandaloneTransactionStatusHistory.IsFull())
		s_StandaloneTransactionStatusHistory.Reset();

	s_StandaloneTransactionStatusHistory.Push(debug);
}

#endif // !__NO_OUTPUT

bool CNetworkShoppingMgr::CreateFreeSpaces(const bool removeAlsofailed)
{
	gnetAssertf(sysThreadType::IsUpdateThread(), "CNetworkShoppingMgr::CreateFreeSpaces() used from another thread than the main thread, this may not be safe.");

	bool result = (CNetShopTransactionBase::GetPool()->GetNoOfFreeSpaces() != 0);

	if (!result)
	{
		gnetDebug1("Creating free spaces: Size=\"%d\", StorageSize=\"%d\", FreeSpaces=\"%d\"",
			CNetShopTransactionBase::GetPool()->GetSize()
			,(int)CNetShopTransactionBase::GetPool()->GetStorageSize()
			,CNetShopTransactionBase::GetPool()->GetNoOfFreeSpaces());

		atDNode<CNetShopTransactionBase*, datBase>* pNode     = m_TransactionList.GetHead();
		atDNode<CNetShopTransactionBase*, datBase>* pNextNode = 0;

		while (pNode)
		{
			pNextNode = pNode->GetNext();

			CNetShopTransactionBase* pEvent = pNode->Data;

			gnetDebug1("[%u] [%" I64FMT "d] Transaction - type=\"%s\", status=\"%s\"", pEvent->GetId(), pEvent->GetRequestId(), pEvent->GetTypeName(), pEvent->GetStatusString());

			//Find an event which can be removed.
			if (!pEvent->Pending())
			{
				//Don't remove Failed events unless stated.
				if (!pEvent->Failed() || removeAlsofailed || (pEvent->Failed() && pEvent->FailedExpired()))
				{
#if __NO_OUTPUT
					delete pEvent;
					pNode->Data = 0;
					m_TransactionList.PopNode(*pNode);
					delete pNode;
#else

					//Push it to the our fixed array of historic transactions.
					if (m_TransactionListHistory.IsFull())
					{
						gnetDebug1("[%u] [%" I64FMT "d] Delete Transaction - type=\"%s\", status=\"%s\"", m_TransactionListHistory[0]->GetId(), m_TransactionListHistory[0]->GetRequestId(), m_TransactionListHistory[0]->GetTypeName(), m_TransactionListHistory[0]->GetStatusString());
						delete m_TransactionListHistory[0];
						m_TransactionListHistory.Delete(0);
					}
					m_TransactionListHistory.Push(pEvent);

					//Pop from main list
					pNode->Data = 0;
					m_TransactionList.PopNode(*pNode);
					delete pNode;
#endif // __NO_OUTPUT
				}
			}

			pNode = pNextNode;
		}

		result = (CNetShopTransactionBase::GetPool()->GetNoOfFreeSpaces() != 0);

		if (!result)
		{
			gnetError("CNetworkShoppingMgr::CreateFreeSpaces() - FAILED to create free spaces.");
		}
	}

	return result;
}

bool  CNetworkShoppingMgr::AppendNewNode(CNetShopTransactionBase* transaction)
{
	gnetAssertf(sysThreadType::IsUpdateThread(), "CNetworkShoppingMgr::AppendNewNode() used from another thread than the main thread, this may not be safe.");
	if (gnetVerify(transaction))
	{
		atDNode< CNetShopTransactionBase*, datBase >* pNode = rage_new atDTransactionNode;
		if (pNode)
		{
			pNode->Data = transaction;
			m_TransactionList.Append(*pNode);
		}

#if !__NO_OUTPUT
		const fwBasePool* pool = CNetShopTransactionBase::GetPool();
		if (pool)
		{
			const s32 poolSize = (s32)pool->GetStorageSize();
			const s32 noOfUsedSpaces = pool->GetNoOfUsedSpaces();
			const s32 maxSpaces  = pool->GetSize();
			gnetDebug1("  ## Pool '%s' '%d' byte(s), Used='%d', Max='%d'.", pool->GetName(), poolSize, noOfUsedSpaces, maxSpaces);
		}
#endif // !__NO_OUTPUT

		return (0 != pNode);
	}

	return false;
}


/* -------------------------------------- */
/* BASKET                                 */

bool  CNetworkShoppingMgr::CreateBasket(NetShopTransactionId& id, const NetShopCategory category, const NetShopTransactionAction action, const int flags)
{
	bool result = false;

	CNetShopTransactionBase* pEvent = FindBasket( id );
	gnetAssertf(!pEvent, "Basket already exists.");

	//Create a new Basket Transaction.
	if (!pEvent && gnetVerify( CreateFreeSpaces() ))
	{
		CNetShopTransactionBasket* transaction = rage_new CNetShopTransactionBasket(NET_SHOP_TTYPE_BASKET, category, action, flags);
		if (transaction)
		{
			if (gnetVerify(AppendNewNode(transaction)))
			{
				id = transaction->GetId();
				result = true;
				gnetDebug1("[%u] [%" I64FMT "d] Transaction basketId created", transaction->GetId(), transaction->GetRequestId());
			}
			else
			{
				delete transaction;
			}
		}
	}
	else if(pEvent && gnetVerify(!pEvent->Pending()) && gnetVerify(category == pEvent->GetItemsCategory()))
	{
		CNetShopTransactionBasket* transaction = static_cast<CNetShopTransactionBasket*> (pEvent);
		transaction->Clear();
	}

	return result;
}

bool  CNetworkShoppingMgr::DeleteBasket( )
{
	bool result = false;

	gnetDebug1("Creating free spaces: delete basket");

	atDNode<CNetShopTransactionBase*, datBase>* pNode     = m_TransactionList.GetHead();
	atDNode<CNetShopTransactionBase*, datBase>* pNextNode = 0;

	while (pNode)
	{
		pNextNode = pNode->GetNext();

		CNetShopTransactionBase* pEvent = pNode->Data;

		if (pEvent->GetIsType( NET_SHOP_TTYPE_BASKET ))
		{
			gnetDebug1("[%u] [%" I64FMT "d] DeleteBasket() - Transaction type=\"%s\", status=\"%s\"", pEvent->GetId(), pEvent->GetRequestId(), pEvent->GetTypeName(), pEvent->GetStatusString());
			OUTPUT_ONLY( pEvent->SpewItems( ); )

			result = (!pEvent->Pending());

			if (gnetVerifyf( result, "Skip delete basket [%u] - basket is pending.", pEvent->GetId() ))
			{
				gnetDebug1("[%u] [%" I64FMT "d] DeleteBasket() - Transaction basketId deleted.", pEvent->GetId(), pEvent->GetRequestId());

#if __NO_OUTPUT
				delete pEvent;
				m_TransactionList.PopNode(*pNode);
				delete pNode;
#else
				//Push it to the our fixed array of historic transactions.
				if (m_TransactionListHistory.IsFull())
				{
					delete m_TransactionListHistory[0];
					m_TransactionListHistory.Delete(0);
				}
				m_TransactionListHistory.Push(pEvent);

				//Pop from main list
				pNode->Data = 0;
				m_TransactionList.PopNode(*pNode);
				delete pNode;
#endif // __NO_OUTPUT

				break;
			}
		}

		pNode = pNextNode;
	}

	return result;
}

bool  CNetworkShoppingMgr::AddItem(CNetShopItem& item)
{
	bool result = false;

	NetShopTransactionId id = NET_SHOP_INVALID_TRANS_ID;
	CNetShopTransactionBase* pEvent = FindBasket( id );

	if (pEvent)
	{
		gnetDebug1("[%u] [%" I64FMT "d] AddItem - Transaction type=\"%s\" status=\"%s\", add Id=\"%d\", ExtraInventoryId=\"%d\"", pEvent->GetId(), pEvent->GetRequestId(), pEvent->GetTypeName(), pEvent->GetStatusString(), item.m_Id, item.m_ExtraInventoryId);

		if ( gnetVerify(pEvent->None()) )
		{
			CNetShopTransactionBasket* transaction = static_cast< CNetShopTransactionBasket* > (pEvent);
			if (gnetVerify(transaction))
			{
				result = transaction->Add( item );

				//We added the item successfully - update pending cash
				if (result)
				{
					//We only register at the end of buying the property due to property trading.
					//This will be done when the checkout is called.
					if (NET_SHOP_ACTION_BUY_PROPERTY != transaction->GetAction() && NET_SHOP_ACTION_BUY_WAREHOUSE != transaction->GetAction())
					{
						gnetVerify( transaction->RegisterPendingCash( ) );
					}
				}
			}
		}
	}

	return result;
}

bool  CNetworkShoppingMgr::RemoveItem(const NetShopItemId itemId)
{
	NetShopTransactionId id = NET_SHOP_INVALID_TRANS_ID;
	CNetShopTransactionBase* pEvent = FindBasket( id );

	gnetAssertf(pEvent, "Failed to find an active Basket, itemId=\"%d\"", itemId);
	if (pEvent)
	{
		CNetShopTransactionBasket* transaction = static_cast< CNetShopTransactionBasket* > (pEvent);
		if (gnetVerify(transaction))
		{
			//We added the item successfully - update pending cash
			if (transaction->Remove( itemId ))
			{
				gnetVerify( transaction->RegisterPendingCash( ) );
				return true;
			}

			return false;
		}
	}

	return false;
}

bool  CNetworkShoppingMgr::FindItem(const NetShopItemId itemId) const 
{
	bool result = false;

	NetShopTransactionId id = NET_SHOP_INVALID_TRANS_ID;
	const CNetShopTransactionBase* pEvent = FindBasket( id );

	gnetAssertf(pEvent, "Failed to find an active Basket, itemId=\"%d\"", itemId);
	if (pEvent)
	{
		const CNetShopTransactionBasket* transaction = static_cast< const CNetShopTransactionBasket* > (pEvent);
		if (gnetVerify(transaction))
		{
			if (CNetShopTransactionBasket::MAX_NUMBER_ITEMS > transaction->Find( itemId ))
			{
				result = true;
			}
		}
	}

	return result;
}

bool  CNetworkShoppingMgr::ClearBasket( )
{
	atDNode<CNetShopTransactionBase*, datBase>* pNode     = m_TransactionList.GetHead();
	atDNode<CNetShopTransactionBase*, datBase>* pNextNode = 0;

	while (pNode)
	{
		pNextNode = pNode->GetNext();

		CNetShopTransactionBase* pEvent = pNode->Data;

		if (pEvent->GetIsType(NET_SHOP_TTYPE_BASKET))
		{
			static_cast<CNetShopTransactionBasket*>(pEvent)->Clear();
			return true;
		}

		pNode = pNextNode;
	}

	return false;
}

bool  CNetworkShoppingMgr::IsBasketEmpty( ) const
{
	return (0 == GetNumberItems( ));
}

bool  CNetworkShoppingMgr::IsBasketFull( ) const
{
	return (CNetShopTransactionBasket::MAX_NUMBER_ITEMS == GetNumberItems( ));
}

u32  CNetworkShoppingMgr::GetNumberItems( ) const
{
	NetShopTransactionId id = NET_SHOP_INVALID_TRANS_ID;

	const CNetShopTransactionBase* pEvent = FindBasket( id );
	gnetAssertf(pEvent, "Failed to find an active Basket.");

	if (pEvent)
	{
		const CNetShopTransactionBasket* transaction = static_cast< const CNetShopTransactionBasket* > (pEvent);
		if (gnetVerify(transaction))
		{
			return transaction->GetNumberItems();
		}
	}

	return 0;
}

/* -------------------------------------- */
/* SERVICES                               */

bool   CNetworkShoppingMgr::BeginService(NetShopTransactionId& id, const NetShopTransactionType type, const NetShopCategory category, const NetShopItemId service, const NetShopTransactionAction action, const int price, const int flags)
{
	id = NET_SHOP_INVALID_TRANS_ID;

	gnetAssertf(type == NET_SHOP_TTYPE_SERVICE, "Invalid trasnaction TYPE, this is not a service type=\"%s\"", NETWORK_SHOP_GET_TRANS_TYPENAME(type));
	if (type != NET_SHOP_TTYPE_SERVICE)
		return false;

	if (!gnetVerify( CreateFreeSpaces() ))
		return false;

	CNetShopTransactionBase* trans = static_cast< CNetShopTransactionBase* > ( rage_new CNetShopTransaction(type, category, service, price, action, flags ) );

	if (gnetVerify(trans))
	{
		if(gnetVerify( AppendNewNode(trans) ))
		{
			id = trans->GetId();
			gnetVerify( trans->RegisterPendingCash( ) );

			return true;
		}
		else
		{
			delete trans;
		}
	}

	return false;
}

bool CNetworkShoppingMgr::EndService( const NetShopTransactionId id )
{
	atDNode< CNetShopTransactionBase*, datBase >* node     = m_TransactionList.GetHead();
	atDNode< CNetShopTransactionBase*, datBase >* nextNode = node;

	//Check for pending requests.
	while (node)
	{
		nextNode = node->GetNext();

		CNetShopTransactionBase* transaction = node->Data;
		if (transaction)
		{
			if (transaction->GetId() == id)
			{
				gnetDebug1("[%u] [%" I64FMT "d] CNetworkShoppingMgr::EndService - found.", transaction->GetId(), transaction->GetRequestId());

				if (gnetVerify(!transaction->Pending()))
				{
					gnetDebug1("[%u] [%" I64FMT "d] CNetworkShoppingMgr::EndService - Creating space in Transaction queue. Removed Transaction type=\"%s\", status=\"%s\"", transaction->GetId(), transaction->GetRequestId(), transaction->GetTypeName(), transaction->GetStatusString());
					OUTPUT_ONLY( transaction->SpewItems( ); )

#if __NO_OUTPUT
					delete transaction;
					m_TransactionList.PopNode(*node);
					delete node;
#else
					//Push it to the our fixed array of historic transactions.
					if (m_TransactionListHistory.IsFull())
					{
						delete m_TransactionListHistory[0];
						m_TransactionListHistory.Delete(0);
					}
					m_TransactionListHistory.Push(transaction);

					//Pop from main list
					node->Data = 0;
					m_TransactionList.PopNode(*node);
					delete node;
#endif // __NO_OUTPUT

					return true;
				}

				return false;
			}

			node = nextNode;
		}
	}

	return false;
}

/* -------------------------------------- */
/* BASKET + SERVICES                      */

bool   CNetworkShoppingMgr::StartCheckout(const NetShopTransactionId id)
{
	bool result = false;

	atDNode<CNetShopTransactionBase*, datBase>* pNode     = m_TransactionList.GetHead();
	atDNode<CNetShopTransactionBase*, datBase>* pNextNode = 0;

	while (pNode)
	{
		pNextNode = pNode->GetNext();

		CNetShopTransactionBase* pEvent = pNode->Data;
		if (id == pEvent->GetId())
		{
			if (!pEvent->Pending() && !pEvent->Succeeded() && !pEvent->Canceled() && !pEvent->CanCheckout())
			{
				gnetVerify( pEvent->Checkout( ) );

				//Check if this is a is a property trade and is not a credit.
				bool isPropertyTradeCredit = false;
				if (pEvent->GetIsType(NET_SHOP_TTYPE_BASKET))
				{
					const CNetShopTransactionBasket* transaction = static_cast< const CNetShopTransactionBasket* > (pEvent);
					if (gnetVerify(transaction))
					{
						int numProperties = 0;
						bool hasOverrideItem = false;
						transaction->GetTotalPrice(isPropertyTradeCredit, numProperties, hasOverrideItem);
					}
				}

				//If by any chance this is a credit skip it!.
				if (!isPropertyTradeCredit)
				{
					gnetVerify( pEvent->RegisterPendingCash( ) );
				}

				result = true;
			}

			pNextNode = 0;
		}

		pNode = pNextNode;
	}

	return result;
}

bool   CNetworkShoppingMgr::GetStatus(const NetShopTransactionId id, netStatus::StatusCode& status) const
{
	const CNetShopTransactionBase* pEvent = FindTransaction( id );
	gnetAssertf(pEvent, "Failed to find a Transaction with id=\"%d\".", id);

	if (gnetVerify(pEvent))
	{
		status = pEvent->GetStatus( );
		return true;
	}

	return false;
}

bool CNetworkShoppingMgr::ShouldDoNullTransaction() const
{
#if (__BANK && __CONSOLE)
	if (PARAM_sc_UseBasketSystem.Get())
	{
		return false;
	}

	return true;
#else

	//Hack to allow __BANK PC builds to use the null transaction.
#if (!__FINAL && RSG_PC)
	if (PARAM_netshoptransactionnull.Get())
	{
		return true;
	}
#endif

	return (!RSG_PC);
#endif
}

bool CNetworkShoppingMgr::TransactionInProgress() const
{
	if (m_transactionInProgress)
		return true;

	return false;
}

bool   CNetworkShoppingMgr::GetFailedCode(const NetShopTransactionId id, int& code) const
{
	const CNetShopTransactionBase* pEvent = FindTransaction( id );

	if (gnetVerify(pEvent) && gnetVerify(pEvent->Failed()))
	{
		code = pEvent->GetResultCode();
		return true;
	}

	return false;
}

CNetShopTransactionBase* CNetworkShoppingMgr::FindBasket( NetShopTransactionId& id )
{
	atDNode<CNetShopTransactionBase*, datBase>* pNode     = m_TransactionList.GetHead();
	atDNode<CNetShopTransactionBase*, datBase>* pNextNode = 0;

	id = NET_SHOP_INVALID_TRANS_ID;

	while (pNode)
	{
		pNextNode = pNode->GetNext();

		CNetShopTransactionBase* pEvent = pNode->Data;

		if (pEvent->GetIsType(NET_SHOP_TTYPE_BASKET))
		{
			id = pEvent->GetId();
			return pEvent;
		}

		pNode = pNextNode;
	}


	return 0;
}

const CNetShopTransactionBase* CNetworkShoppingMgr::FindBasket( NetShopTransactionId& id ) const 
{
	const atDNode<CNetShopTransactionBase*, datBase>* pNode     = m_TransactionList.GetHead();
	const atDNode<CNetShopTransactionBase*, datBase>* pNextNode = 0;

	id = NET_SHOP_INVALID_TRANS_ID;

	while (pNode)
	{
		pNextNode = pNode->GetNext();

		const CNetShopTransactionBase* pEvent = pNode->Data;

		if (pEvent->GetIsType(NET_SHOP_TTYPE_BASKET))
		{
			id = pEvent->GetId();
			return pEvent;
		}

		pNode = pNextNode;
	}

	return 0;
}

CNetShopTransactionBase* CNetworkShoppingMgr::FindTransaction( const NetShopTransactionId id )
{
	atDNode<CNetShopTransactionBase*, datBase>* pNode     = m_TransactionList.GetHead();
	atDNode<CNetShopTransactionBase*, datBase>* pNextNode = 0;

	while (pNode)
	{
		pNextNode = pNode->GetNext();

		CNetShopTransactionBase* pEvent = pNode->Data;
		if (id == pEvent->GetId())
		{
			return pEvent;
		}

		pNode = pNextNode;
	}

	return 0;
}

const CNetShopTransactionBase* CNetworkShoppingMgr::FindTransaction( const NetShopTransactionId id ) const
{
	const atDNode<CNetShopTransactionBase*, datBase>* pNode     = m_TransactionList.GetHead();
	const atDNode<CNetShopTransactionBase*, datBase>* pNextNode = 0;

	while (pNode)
	{
		pNextNode = pNode->GetNext();

		const CNetShopTransactionBase* pEvent = pNode->Data;
		if (id == pEvent->GetId())
		{
			return pEvent;
		}

		pNode = pNextNode;
	}

	return 0;
}

CNetShopTransaction* CNetworkShoppingMgr::FindService( const NetShopTransactionType type, const NetShopItemId id )
{
	if (!gnetVerify(type == NET_SHOP_TTYPE_SERVICE))
		return 0;

	atDNode<CNetShopTransactionBase*, datBase>* pNode     = m_TransactionList.GetHead();
	atDNode<CNetShopTransactionBase*, datBase>* pNextNode = 0;

	while (pNode)
	{
		pNextNode = pNode->GetNext();

		CNetShopTransactionBase* pEvent = pNode->Data;
		if (gnetVerify(pEvent) && pEvent->GetIsType(type))
		{
			CNetShopTransaction* trans = static_cast<CNetShopTransaction*>(pEvent);
			if (trans->GetItemId() == id)
			{
				return trans;
			}
		}

		pNode = pNextNode;
	}

	return 0;
}

const CNetShopTransaction* CNetworkShoppingMgr::FindService( const NetShopTransactionType type, const NetShopItemId id ) const
{
	if (!gnetVerify(type == NET_SHOP_TTYPE_SERVICE))
		return 0;

	const atDNode<CNetShopTransactionBase*, datBase>* pNode     = m_TransactionList.GetHead();
	const atDNode<CNetShopTransactionBase*, datBase>* pNextNode = 0;

	while (pNode)
	{
		pNextNode = pNode->GetNext();

		const CNetShopTransactionBase* pEvent = pNode->Data;
		if (gnetVerify(pEvent) && pEvent->GetIsType(type))
		{
			const CNetShopTransaction* trans = static_cast<const CNetShopTransaction*>(pEvent);
			if (trans->GetItemId() == id)
			{
				return trans;
			}
		}

		pNode = pNextNode;
	}

	return 0;
}

#if !__NO_OUTPUT

void CNetworkShoppingMgr::SpewServicesInfo( const NetShopTransactionType type, const NetShopItemId id ) const
{
	const atDNode<CNetShopTransactionBase*, datBase>* pNode     = m_TransactionList.GetHead();
	const atDNode<CNetShopTransactionBase*, datBase>* pNextNode = 0;

	while (pNode)
	{
		pNextNode = pNode->GetNext();

		const CNetShopTransactionBase* pEvent = pNode->Data;
		if (gnetVerify(pEvent) && pEvent->GetIsType(type))
		{
			const CNetShopTransaction* trans = static_cast<const CNetShopTransaction*>(pEvent);
			if (trans->GetItemId() == id)
				gnetDebug1("[%u] [%" I64FMT "d] service Info: ItemId=\"%d\", trans=\"%s\", status=\"%s\".", trans->GetId(), trans->GetRequestId(), id, trans->GetTypeName(), trans->GetStatusString());
		}

		pNode = pNextNode;
	}
}

const char* CNetworkShoppingMgr::GetCategoryName(const NetShopCategory category) const 
{
	struct catEnumName { u32 id; const char* name; };
	static catEnumName sCategoryNameList[] =
	{
#define CATEGORY_ITEM(a,b) { b, #a },
		SHOP_CATEGORY_LIST
#undef  CATEGORY_ITEM
	};

	const char* result = "INVALID";

	bool found = false;

	for (int i=0; i<NELEM(sCategoryNameList); i++)
	{
		if (sCategoryNameList[i].id == category)
		{
			if (sCategoryNameList[i].name)
				result = sCategoryNameList[i].name;

			found = true;
			break;
		}
	}

	gnetAssertf(found, "GetCategoryName - Invalid item category=\"%d\"", category);

	return result;
}

const char* CNetworkShoppingMgr::GetTransactionTypeName(const NetShopTransactionType type) const 
{
	const char* result = "INVALID";

	bool found = false;

	for (int i=0; i<m_transactiontypes.GetCount(); i++)
	{
		if (m_transactiontypes[i].GetKeyHash() == type)
		{
			if (m_transactiontypes[i].TryGetCStr())
				result = m_transactiontypes[i].TryGetCStr();

			found = true;
			break;
		}
	}

	gnetAssertf(found, "GetTransactionTypeName - Invalid transaction type=\"%d\"", type);

	return result;
}

const char* CNetworkShoppingMgr::GetActionTypeName(const NetShopTransactionAction action) const
{
	const char* result = "INVALID";

	bool found = false;

	for (int i=0; i<m_actiontypes.GetCount(); i++)
	{
		if (m_actiontypes[i].GetKeyHash() == action)
		{
			if (m_actiontypes[i].TryGetCStr())
				result = m_actiontypes[i].TryGetCStr();

			found = true;
			break;
		}
	}

	gnetAssertf(found, "GetActionTypeName - Invalid item action type=\"%d\"", action);

	return result;
}
#endif // !__NO_OUTPUT

bool CNetworkShoppingMgr::GetCategoryIsValid(const NetShopCategory category) const
{
	static u32 sCategoryList[] = 
	{
#define CATEGORY_ITEM(a,b) b,
		SHOP_CATEGORY_LIST
#undef CATEGORY_ITEM
	};

	bool result = false;

	for (int i=0; i<NELEM(sCategoryList); i++)
	{
		if (sCategoryList[i] == category)
		{
			result = true;
			break;
		}
	}

	gnetAssertf(result, "GetCategoryName - Invalid item category=\"%d\"", category);

	return result;
}

bool CNetworkShoppingMgr::GetTransactionTypeIsValid(const NetShopTransactionType type) const
{
	bool result = false;

	for (int i=0; i<m_transactiontypes.GetCount(); i++)
	{
		if (m_transactiontypes[i].GetKeyHash() == type)
		{
			result = true;
			break;
		}
	}

	gnetAssertf(result, "GetTransactionTypeName - Invalid transaction type=\"%d\"", type);

	return result;
}

bool CNetworkShoppingMgr::GetActionTypeIsValid(const NetShopTransactionAction action) const
{
	bool result = false;

	for (int i=0; i<m_actiontypes.GetCount(); i++)
	{
		if (m_actiontypes[i].GetKeyHash() == action)
		{
			result = true;
			break;
		}
	}

	gnetAssertf(result, "GetActionTypeIsValid - Invalid item action type=\"%d\"", action);

	return result;
}

bool CNetworkShoppingMgr::ActionAllowsNegativeOrZeroValue(const NetShopTransactionAction action) const
{
	if(	   action == NET_SHOP_ACTION_RESET_BUSINESS_PROGRESS
		|| action == NET_SHOP_ACTION_UPDATE_BUSINESS_GOODS
        || action == NET_SHOP_ACTION_UPDATE_WAREHOUSE_VEHICLE
		)
	{
		return true;
	}
	
	return false;
}


/* -------------------------------------- */
/* WIDGETS                                */

#if __BANK

int bankFlags = CATALOG_ITEM_FLAG_WALLET_THEN_BANK;
int bankItemId = -176143638; //WP_WT_PARA_t4_v0
int bankItemCatalogId = 1;
int bankItemStatValue = 0;
int bankItemQuantity = 1;
NetShopTransactionId bankTransactionId = NET_SHOP_INVALID_TRANS_ID;
CNetShopItem bankBasketItem;

void  CNetworkShoppingMgr::Bank_InitWidgets(bkBank* bank)
{
	if (!bank)
		return;

	m_bankLatency          = 0;
	m_bankOverrideLatency  = false;

	NetworkGameTransactions::AddWidgets(*bank);

	GameTransactionSessionMgr::Get().AddWidgets(*bank);

	bank->PushGroup("DebugDraw", false);
	{
		bank->AddToggle("Activate Visual Debug",  &s_ActivateDebugDraw);
		bank->AddButton("Clear Visual Debug", datCallback(MFA(CNetworkShoppingMgr::Bank_ClearVisualDebug), (datBase*)this));

		//bank->AddSlider("s_NoOfTexts",            &s_TextLine,                0,  100,     1);
		//bank->AddSlider("s_VerticalDist",         &s_VerticalDist,         0.0f, 3.0f, 0.001f);
		//bank->AddSlider("s_HorizontalBorder",     &s_HorizontalBorder,     0.0f, 3.0f, 0.01f);
		//bank->AddSlider("s_HorizontalItemBorder", &s_HorizontalItemBorder, 0.0f, 3.0f, 0.01f);
		//bank->AddSlider("s_VerticalBorder",       &s_VerticalBorder,       0.0f, 3.0f, 0.01f);
		//bank->AddSlider("s_textScale",            &s_textScale,            0.0f, 3.0f, 0.01f);
		//bank->AddToggle("drawBGQuad",             &s_drawBGQuad);
	}
	bank->PopGroup();

	bank->PushGroup("Freemode Shopping", false);
	{
		bank->PushGroup("Catalog Parser", false);
		{
			bank->AddButton("Write to Cache", datCallback(MFA(CNetworkShoppingMgr::Bank_WriteToCache), (datBase*)this));
			bank->AddButton("Load  From Cache", datCallback(MFA(CNetworkShoppingMgr::Bank_LoadFromCache), (datBase*)this));
			bank->AddButton("Read Catalog and set stats", datCallback(MFA(CNetworkShoppingMgr::Bank_ReadCatalog), (datBase*)this));
			bank->AddButton("Write Catalog", datCallback(MFA(CNetworkShoppingMgr::Bank_WriteCatalog), (datBase*)this));
			bank->AddButton("Clear Catalog", datCallback(MFA(CNetworkShoppingMgr::Bank_ClearCatalog), (datBase*)this));
		}
		bank->PopGroup();

		bank->PushGroup("Overrides", false);
		{
			bank->AddToggle("Override latency", &m_bankOverrideLatency);
		}
		bank->PopGroup();

		WIN32PC_ONLY( bank->AddToggle("Test Asynch Transactions", &m_bankTestAsynchTransactions); )
		bank->AddSlider("Override latency", &m_bankLatency, 0, 3600000, 1);
		bank->AddButton("Spew Packed stats exceptions", datCallback(MFA(CNetworkShoppingMgr::Bank_SpewPackedStatsExceptions), (datBase*)this));
	}
	bank->PopGroup();

	bank->PushGroup("Shopping Basket");
		bank->AddButton("Create Basket", datCallback(MFA(CNetworkShoppingMgr::Bank_CreateBasket), (datBase*)this));
		bank->AddButton("Add Item To Basket", datCallback(MFA(CNetworkShoppingMgr::Bank_AddItem), (datBase*)this));
		bank->AddButton("Clear Basket", datCallback(MFA(CNetworkShoppingMgr::ClearBasket), (datBase*)this));
		bank->AddButton("Checkout", datCallback(MFA(CNetworkShoppingMgr::Bank_Checkout), (datBase*)this));
		bank->AddButton("Apply Data to Stats", datCallback(MFA(CNetworkShoppingMgr::Bank_ApplyDataToStats), (datBase*)this));
		bank->AddButton("Delete Basket", datCallback(MFA(CNetworkShoppingMgr::DeleteBasket), (datBase*)this));
	bank->PopGroup();
}

void CNetworkShoppingMgr::Bank_WriteToCache( )
{
	netCatalog::GetInstance().WriteToCache();
}

void CNetworkShoppingMgr::Bank_ClearCatalog( )
{
	netCatalog::GetInstance().Clear();
}

void CNetworkShoppingMgr::Bank_ClearVisualDebug( )
{
#if !__NO_OUTPUT
	while (m_TransactionListHistory.GetCount())
		delete m_TransactionListHistory.Pop();
	s_StandaloneTransactionStatusHistory.clear();
#endif // !__NO_OUTPUT
}

void CNetworkShoppingMgr::Bank_LoadFromCache( )
{
	netCatalog::GetInstance().LoadFromCache();
}

void  CNetworkShoppingMgr::Bank_ReadCatalog()
{
	netCatalog::GetInstance().Read();
}

void  CNetworkShoppingMgr::Bank_WriteCatalog()
{
	netCatalog::GetInstance().Write();
}

void CNetworkShoppingMgr::Bank_CreateBasket()
{
	//Find this item in the catalog data
	const netCatalog& catalog = netCatalog::GetInstance();
	atHashString itemIdHashStr((u32)bankItemId);
	const netCatalogBaseItem* catItemBase = catalog.Find(itemIdHashStr);

	if (catItemBase)
	{
		bankBasketItem = CNetShopItem(static_cast< NetShopItemId >(bankItemId),
									  static_cast< NetShopItemId >(bankItemCatalogId),
									  catItemBase->GetPrice(),
									  bankItemStatValue,
									  bankItemQuantity);

		gnetVerify(CreateBasket(bankTransactionId, static_cast<NetShopCategory>(catItemBase->GetCategory()), static_cast<NetShopTransactionAction>(NET_SHOP_ACTION_SPEND), bankFlags));
	}
}

void CNetworkShoppingMgr::Bank_AddItem()
{
	if(bankBasketItem.IsValid())
	{
		gnetVerify(AddItem(bankBasketItem));
	}
}

void CNetworkShoppingMgr::Bank_Checkout()
{
	if(bankTransactionId != NET_SHOP_INVALID_TRANS_ID)
	{
		gnetVerify(StartCheckout(bankTransactionId));
	}
}

void CNetworkShoppingMgr::Bank_ApplyDataToStats()
{
	if(bankTransactionId != NET_SHOP_INVALID_TRANS_ID)
	{
		CNetShopTransactionBase* transaction = FindTransaction(bankTransactionId);

		if (transaction)
		{
			CNetShopTransactionServerDataInfo info;
			gnetVerify(transaction->ApplyDataToStats(info));
		}
	}
}

void CNetworkShoppingMgr::Bank_SpewPackedStatsExceptions()
{
	netCatalog& catalogInst = netCatalog::GetInstance();
	const atMap < int, sPackedStatsScriptExceptions >& packedExc = catalogInst.GetScriptExceptions();

	gnetDebug1("/////////////////////////////////////////////////");
	gnetDebug1("/////////////// PACKED EXCEPTIONS ///////////////");
	gnetDebug1("/////////////////////////////////////////////////");

	atMap< int, sPackedStatsScriptExceptions >::ConstIterator it = packedExc.CreateIterator();
	for (it.Start(); !it.AtEnd(); it.Next())
	{
		if ( it.GetKey() == 0 )
			continue;

		if (it.GetData().GetCount() > 0)
		{
			gnetDebug1("Exception added for Stat='%s',", StatsInterface::GetKeyName(it.GetKey()));
			OUTPUT_ONLY( it.GetData().SpewBitShifts() );
		}
	}

	gnetDebug1("//////////////////////////////////////////////////////////////////////////");
}

#endif // __BANK

#endif // __NET_SHOP_ACTIVE

// eof 

