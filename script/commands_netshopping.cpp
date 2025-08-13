//
// filename:	commands_money.cpp
// description:	
//

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "diag/channel.h"
#include "script/wrapper.h"
#include "data/bitbuffer.h"
#include "rline/rlgamerinfo.h"
#include "string/stringhash.h"
#include "parser/manager.h"
#include "system/param.h"

// Game headers
#include "commands_netshopping.h"
#include "Script/script.h"
#include "Script/script_channel.h"
#include "Script/script_helper.h"
#include "Stats/stats_channel.h"
#include "Stats/StatsInterface.h"
#include "Stats/StatsTypes.h"
#include "Event/EventNetwork.h"
#include "Network/NetworkInterface.h"
#include "Network/Shop/NetworkShopping.h"
#include "Network/Shop/Catalog.h"
#include "Network/Shop/Inventory.h"
#include "Network/Shop/NetworkGameTransactions.h"
#include "Network/Shop/GameTransactionsSessionMgr.h"
#include "Network/NetworkTypes.h"
#include "Network/Live/NetworkProfileStats.h"
#include "Network/Live/NetworkLegalRestrictionsManager.h"
#include "security/plugins/scripthook.h"

NETWORK_OPTIMISATIONS()
NETWORK_SHOP_OPTIMISATIONS();

//protection to handle unity vs. non-unity 
#ifndef __SCR_TEXT_LABEL_DEFINED
#define __SCR_TEXT_LABEL_DEFINED
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrTextLabel63);
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrTextLabel31);
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrTextLabel15);
#endif

XPARAM(sc_UseBasketSystem);

//////////////////////////////////////////////////////////////////////////

#if __NET_SHOP_ACTIVE

static const u32 MAX_NUM_VALID_SCRIPTS = 102;
static const u32 s_validCallingScriptsHashes[MAX_NUM_VALID_SCRIPTS] = 
{
	// *** KEEP IN ALPHABETICAL ORDER PLEASE!! *****
	ATSTRINGHASH("appbusinesshub", 0xd1be4a95),
	ATSTRINGHASH("apphackertruck", 0x57a3ec48),
	ATSTRINGHASH("appSmuggler", 0xbda16e7d),
	ATSTRINGHASH("arcade_lobby_preview", 0xdced49a7),
	ATSTRINGHASH("arena_carmod", 0xDAFC0911),
	ATSTRINGHASH("armory_aircraft_carmod", 0x78ddb08f),
	ATSTRINGHASH("am_mp_auto_shop", 0x0ba18b4c),
	ATSTRINGHASH("am_arena_shp", 0x8bb3a063),
	ATSTRINGHASH("am_contact_requests", 0xb3920707),
	ATSTRINGHASH("am_hold_up", 0x0c6dac46),
	ATSTRINGHASH("am_gang_call", 0x0266ce78),
	ATSTRINGHASH("am_luxury_showroom", 0xf4ab6fb5),
	ATSTRINGHASH("am_mp_arcade", 0x500b732f),
	ATSTRINGHASH("am_mp_arc_cab_manager", 0x4EE41F75),
	ATSTRINGHASH("am_mp_arena_box", 0x8C9C5FB8),
	ATSTRINGHASH("am_mp_armory_aircraft", 0x9c6f5716),
	ATSTRINGHASH("am_mp_armory_truck", 0x5b9b0272),
	ATSTRINGHASH("am_mp_business_hub", 0xc85b5bef),
	ATSTRINGHASH("am_mp_car_meet_property", 0x2385bdc4),
	ATSTRINGHASH("am_mp_fixer_hq", 0x995F4EA5),
	ATSTRINGHASH("am_mp_casino", 0x7166BD4A),
	ATSTRINGHASH("am_mp_casino_apartment", 0xD76D873D),
	ATSTRINGHASH("am_mp_defunct_base", 0xcab8a943),
	ATSTRINGHASH("am_mp_hacker_truck", 0x349b7463),
	ATSTRINGHASH("am_mp_hangar", 0xefc0d4ec),
	ATSTRINGHASH("am_mp_nightclub", 0x8c56b7fe),
	ATSTRINGHASH("am_mp_property_ext", 0x72ee09f2),
	ATSTRINGHASH("am_mp_property_int", 0x95b61b3c),
	ATSTRINGHASH("am_mp_simeon_showroom", 0x1ed5a03d),
	ATSTRINGHASH("am_mp_smpl_interior_ext", 0xe0a7b7ad),
	ATSTRINGHASH("am_mp_warehouse", 0x2262ddf0),
	ATSTRINGHASH("am_mp_yacht", 0xc930c939),
	ATSTRINGHASH("am_pi_menu", 0x06f27211),
	ATSTRINGHASH("am_mp_arena_garage", 0x26ebce20),
	ATSTRINGHASH("am_mp_vehicle_reward", 0x692f8e30),
	ATSTRINGHASH("am_mp_submarine", 0xe16957f2),
	ATSTRINGHASH("apparcadebusiness", 0x4ba16a46),
	ATSTRINGHASH("appbikerbusiness", 0x57fad435),
	ATSTRINGHASH("appbunkerbusiness", 0x068ba1e1),
	ATSTRINGHASH("appimportexport", 0x114e0bf4),
	ATSTRINGHASH("appinternet", 0x6a172273),
	ATSTRINGHASH("appsecuroserv", 0x25b97645),
	ATSTRINGHASH("base_carmod", 0x00e813dd),
	ATSTRINGHASH("business_battles", 0xa6526fa9),
	ATSTRINGHASH("business_battles_defend", 0x6fa239a5),
	ATSTRINGHASH("business_battles_sell", 0xf3f871da),
	ATSTRINGHASH("business_hub_carmod", 0x06e6f052),
	ATSTRINGHASH("Blackjack", 0xd378365e),
	ATSTRINGHASH("car_meet_carmod", 0xBB3D7E9D),
	ATSTRINGHASH("carmod_shop", 0x1dc6b680),
	ATSTRINGHASH("casino_slots", 0x5f1459d7),
	ATSTRINGHASH("casinoroulette", 0x05e86d0d),
	ATSTRINGHASH("CASINO_LUCKY_WHEEL", 0xbdf0ccff),
	ATSTRINGHASH("clothes_shop_mp", 0xc0f0bbc3),
	ATSTRINGHASH("clothes_shop_sp", 0xf8a3166b),
	ATSTRINGHASH("casino_lucky_wheel", 0xbdf0ccff),
	ATSTRINGHASH("copscrookswrapper", 0xa7d5c6a0),
	ATSTRINGHASH("fixer_hq_carmod", 0x41f74a04),
	ATSTRINGHASH("fm_bj_race_controler", 0xbe3e7d03),
	ATSTRINGHASH("fm_content_auto_shop_delivery", 0xfc4970dc),
	ATSTRINGHASH("fm_content_bike_shop_delivery", 0x3ff69732),
	ATSTRINGHASH("fm_content_business_battles", 0x62ea78d0),
	ATSTRINGHASH("fm_content_cargo", 0x035d0edc),
	ATSTRINGHASH("fm_content_club_source", 0x2b88e6a1),
	ATSTRINGHASH("fm_content_gunrunning", 0x1a160b48),
	ATSTRINGHASH("fm_content_island_dj", 0xbe590e09),
	ATSTRINGHASH("fm_deathmatch_controler", 0xe62128cb),
	ATSTRINGHASH("fm_horde_controler", 0x514a6d17),
	ATSTRINGHASH("fm_impromptu_dm_controler", 0xad453307),
	ATSTRINGHASH("fm_mission_controller", 0x6c2ce225),
	ATSTRINGHASH("fm_mission_controller_2020", 0xBF461AD0),
	ATSTRINGHASH("fm_race_controler", 0xeb004e0e),
	ATSTRINGHASH("fm_race_creator", 0x4410302a),
	ATSTRINGHASH("fmmc_launcher", 0xa3fb8a5e),
	ATSTRINGHASH("freemode", 0xc875557d),
	ATSTRINGHASH("gb_casino", 0x13645dc3),
	ATSTRINGHASH("gb_biker_contraband_defend", 0x56a1412c),
	ATSTRINGHASH("gb_biker_contraband_sell", 0xdffc4863),
	ATSTRINGHASH("gb_contraband_buy", 0xeb04de05),
	ATSTRINGHASH("gb_contraband_defend", 0x2055a1a4),
	ATSTRINGHASH("gb_contraband_sell", 0x7b3e31d2),
	ATSTRINGHASH("gb_deathmatch", 0x9f42b34b),
	ATSTRINGHASH("gb_gunrunning", 0x8cd0bff0),
	ATSTRINGHASH("gb_gunrunning_defend", 0x14b421aa),
	ATSTRINGHASH("gb_illicit_goods_resupply", 0x9d9d13ec),
	ATSTRINGHASH("gb_smuggler", 0xabd3de17),
	ATSTRINGHASH("gunclub_shop", 0x1beb0bf6),
	ATSTRINGHASH("hacker_truck_carmod", 0xce27944e),
	ATSTRINGHASH("hairdo_shop_mp", 0xb57685c9),
	ATSTRINGHASH("hairdo_shop_sp", 0xb7895f57),
	ATSTRINGHASH("hangar_carmod", 0x34141a78),
	ATSTRINGHASH("main_persistent", 0x5700179c),
	ATSTRINGHASH("maintransition", 0x5ff2841f),
	ATSTRINGHASH("mptestbed", 0x62c09cc6),
	ATSTRINGHASH("ob_vend1", 0xa39c1f72),
	ATSTRINGHASH("ob_vend2", 0x11c77bc7),
	ATSTRINGHASH("personal_carmod_shop", 0xdef103d2),
	ATSTRINGHASH("shop_controller", 0x39da738b),
	ATSTRINGHASH("tattoo_shop", 0xc0d26565),
	ATSTRINGHASH("Three_Card_Poker", 0xc8608f32),
	ATSTRINGHASH("tuner_property_carmod", 0x70b10f84),
	ATSTRINGHASH("valentineRpReward2", 0xd1f9d9c5)
	// *** KEEP IN ALPHABETICAL ORDER PLEASE!! *****
};

#if !__FINAL
XPARAM(dontCrashOnScriptValidation);
#endif // !__FINAL

bool VERIFY_GAMESERVER_USE( )
{
	////Cops&Crooks doesn't have GTA dollars involved.
	//if (NetworkInterface::IsInCopsAndCrooks())
	//{
	//	return false;
	//}

	const CGameScriptId& scriptId = static_cast<CGameScriptId&>(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId());

	for (u32 i = 0; i < MAX_NUM_VALID_SCRIPTS; i++)
	{
		if (s_validCallingScriptsHashes[i] == scriptId.GetScriptNameHash())
		{
			return true;
		}
	}

#if !__FINAL
	if(!PARAM_dontCrashOnScriptValidation.Get())
	{
		gnetFatalAssertf(0, "Shopping game event being called from invalid script %s", scriptId.GetScriptName());
	}
#endif // !__FINAL

	return false;
};

#endif // __NET_SHOP_ACTIVE

//////////////////////////////////////////////////////////////////////////

//PURPOSE
//  specific data for catalog items.
class ScriptCatalogItem
{
public:
	scrTextLabel63  m_key;
	scrTextLabel15  m_textTag;
	scrTextLabel63  m_name;
	scrValue        m_category;
	scrValue        m_price;
	scrValue        m_stathash;
	scrValue        m_storagetype;
	scrValue        m_bitshift;
	scrValue        m_bitsize;
	scrValue        m_statenumvalue;
	scrValue        m_statvalue;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET( ScriptCatalogItem );

//PURPOSE
//  specific data for buyng a item and adding to the basket.
class srcBasketItem
{
public:

	//Catalog item key - this will have info about the item price.
	scrValue m_catalogKey;

	//Inventory item key - this will have info about the item in 
	//  inventory, some items need 2 catalog entries one for the price and one for the inventory.
	scrValue m_extraInventoryKey;

	//Item cost - this can be the value specified in the catalog 
	//   or what the client think it is.
	scrValue m_price;

	//New stat value that will be set in the inventory.
	scrValue m_statvalue;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(srcBasketItem);

class srcBasketServerDataInfo
{
public:
	//Will be true if a cash update was received from the server.
	scrValue m_CashUpdateReceived; 

	//Local vs. Server (how much did the server values change the local value)
	scrValue m_bankCashDifference;
	scrValue m_walletCashDifference;

	//Number of inventory items received in the response (and applied)
	scrValue m_InventoryRcvd;
	scrValue m_NumValues;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(srcBasketServerDataInfo);

namespace netshopping_commands {

RAGE_DEFINE_SUBCHANNEL(stat, net_shop, DIAG_SEVERITY_DEBUG3, DIAG_SEVERITY_DEBUG3)
#undef __stat_channel
#define __stat_channel stat_net_shop

RAGE_DEFINE_SUBCHANNEL(script, net_shop, DIAG_SEVERITY_DEBUG3, DIAG_SEVERITY_DEBUG3)
#undef __script_channel
#define __script_channel script_net_shop


//Possible network shot transaction status.
enum eNETWORK_SHOP_TRANSACTION_STATUS
{
	TRANSACTION_STATUS_NONE, 
	TRANSACTION_STATUS_PENDING, 
	TRANSACTION_STATUS_FAILED, 
	TRANSACTION_STATUS_SUCCESSFULL,
	TRANSACTION_STATUS_CANCELED
};

/////////////////////////////////////////////////////////////////////
////////////////////////// HANDY FUNCTIONS //////////////////////////
/////////////////////////////////////////////////////////////////////

// security - don't leave script names in the shipping exe!
#if __ASSERT
void ASSERT_ON_INVALID_COMMAND(const char* commandname)
{
#if __NET_SHOP_ACTIVE

	scriptAssertf(PARAM_sc_UseBasketSystem.Get() || RSG_PC, "%s : %s - Command is invalid because the param sc_UseBasketSystem is not set."
					, CTheScripts::GetCurrentScriptNameAndProgramCounter()
					, commandname);

#else //!__NET_SHOP_ACTIVE

	scriptAssertf(0, "%s : %s - Command is invalid because __NET_SHOP_ACTIVE is not set.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandname);

#endif // __NET_SHOP_ACTIVE
}
#else
#define ASSERT_ON_INVALID_COMMAND(...)
#endif // __ASSERT

bool VERIFY_STATS_LOADED(ASSERT_ONLY(const char* NET_SHOP_ONLY(commandname), bool spewCommand = true) )
{
#if __NET_SHOP_ACTIVE

#if __ASSERT
	if(!gnetVerify(commandname))
		commandname = "UNKNOWN_COMMAND_NAME";

	if (spewCommand)
	{
		statDebugf1("Command called - %s", commandname);
		scriptDebugf1("Command called - %s", commandname);
	}
#endif // __ASSERT

	scriptAssertf(NetworkInterface::IsLocalPlayerOnline(), "%s : %s - Trying to shop without being online.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandname);
	if (!NetworkInterface::IsLocalPlayerOnline())
	{
		ASSERT_ONLY(scriptErrorf("%s : %s - Trying to shop without being online.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandname);)
		return false;
	}

	scriptAssertf(!StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT), "%s : %s - Trying to shop without loading the savegame.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandname);
	if ( StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT) )
	{
		ASSERT_ONLY(scriptErrorf("%s : %s - Trying to shop without loading the savegame.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandname);)
		return false;
	}

	scriptAssertf(!StatsInterface::CloudFileLoadFailed(STAT_MP_CATEGORY_DEFAULT), "%s : %s - Trying to shop but stats loaded have failed.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandname);
	if ( StatsInterface::CloudFileLoadFailed(STAT_MP_CATEGORY_DEFAULT) )
	{
		ASSERT_ONLY(scriptErrorf("%s : %s - Trying to shop but stats loaded have failed.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandname);)
		return false;
	}

	CProfileStats& profileStatsMgr = CLiveManager::GetProfileStatsMgr();
	scriptAssertf(profileStatsMgr.GetMPSynchIsDone( ), "%s : %s - Profile Stats not Synchronized.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandname);
	if (!profileStatsMgr.GetMPSynchIsDone(  ))
	{
		ASSERT_ONLY(scriptErrorf("%s : %s - Profile Stats not Synchronized - GetMPSynchIsDone FAILED.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandname);)
		return false;
	}
#endif // __NET_SHOP_ACTIVE

	return true;
}

bool IS_HANDLE_VALID(int& NET_SHOP_ONLY(handleData), int NET_SHOP_ONLY(sizeOfData))
{

#if __NET_SHOP_ACTIVE

	// script returns size in script words
	sizeOfData *= sizeof(scrValue); 
	if(sizeOfData != (SCRIPT_GAMER_HANDLE_SIZE * sizeof(scrValue)))
	{
		return false;
	}

	// buffer to write our handle into
	u8* pHandleBuffer = reinterpret_cast<u8*>(&handleData);

	// implement a copy of the import function
	datImportBuffer bb;
	bb.SetReadOnlyBytes(pHandleBuffer, sizeOfData * 4);

	u8 nService = rlGamerHandle::SERVICE_INVALID;
	bool bIsValid = bb.ReadUns(nService, 8);

    bool bValidService = bIsValid &&
#if RLGAMERHANDLE_XBL
        (nService == rlGamerHandle::SERVICE_XBL);
#elif RLGAMERHANDLE_NP
        (nService == rlGamerHandle::SERVICE_NP);
#elif RLGAMERHANDLE_SC
        (nService == rlGamerHandle::SERVICE_SC);
#else
        false;
#endif

	// if we have a valid service
    if(bValidService)
    {
		// retrieve gamer handle
		rlGamerHandle hGamer;
		unsigned nImported = 0;
		hGamer.Import(pHandleBuffer, sizeOfData * 4, &nImported);
		return hGamer.IsValid();
	}

#endif //__NET_SHOP_ACTIVE

	// invalid service
	return false;
}

bool VERIFY_ITEM_ISVALID( const int NET_SHOP_ONLY(id) ASSERT_ONLY(, const char* NET_SHOP_ONLY(commandname)) )
{
#if __NET_SHOP_ACTIVE

	bool isValid = false;

	if (!CommandNetGameServerIsCatalogValid( ))
	{
		ASSERT_ONLY( scriptErrorf("%s : %s - Catalog is not valid.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandname); )
		scriptAssertf(false, "%s : %s - Catalog is not valid.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandname);
		return isValid;
	}

	const netCatalog& catalogue = netCatalog::GetInstance();

	atHashString key((u32)id);
	const netCatalogBaseItem* item = catalogue.Find(key);
	if (item)
	{
		isValid = true;
	}
	else
	{
#if __ASSERT
		ASSERT_ONLY( scriptErrorf("%s : %s - Item='%d' is not valid.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandname, id); )
#else
		scriptErrorf("%s : VERIFY_ITEM_ISVALID - Item='%d' is not valid.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), id);
#endif //__ASSERT

		scriptAssertf(false, "%s : %s - Item='%d' is not valid.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandname, id);
	}

	return isValid;

#else //!__NET_SHOP_ACTIVE

	return false;

#endif //__NET_SHOP_ACTIVE
}

bool VERIFY_SHOP_TRANSACTION_TYPE_ISVALID( const int NET_SHOP_ONLY(type) ASSERT_ONLY(, const char* NET_SHOP_ONLY(commandname)) )
{
#if __NET_SHOP_ACTIVE
	const bool result = NETWORK_SHOPPING_MGR.GetTransactionTypeIsValid((NetShopTransactionType)type);
	scriptAssertf(result, "%s : %s - Transaction type \"%d\" is not valid.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandname, type);
	return result;
#else //!__NET_SHOP_ACTIVE
	return false;
#endif //__NET_SHOP_ACTIVE
}

bool VERIFY_SHOP_ACTION_TYPE_ISVALID( const int NET_SHOP_ONLY(action) ASSERT_ONLY(, const char* NET_SHOP_ONLY(commandname)) )
{
#if __NET_SHOP_ACTIVE
	const bool result = NETWORK_SHOPPING_MGR.GetActionTypeIsValid((NetShopTransactionAction)action);
	scriptAssertf(result, "%s : %s - Item action \"%d\" is not valid.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandname, action);

#if __ASSERT
	if (!result)
		scriptErrorf("%s : %s - Item action \"%d\" is not valid.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandname, action);
#endif
	
	return result;
#else //!__NET_SHOP_ACTIVE
	return false;
#endif //__NET_SHOP_ACTIVE
}

bool VERIFY_SHOP_TRANSACTION_CATEGORY_ISVALID( const int NET_SHOP_ONLY(category) ASSERT_ONLY(, const char* NET_SHOP_ONLY(commandname)) )
{
#if __NET_SHOP_ACTIVE
	const bool result = NETWORK_SHOPPING_MGR.GetCategoryIsValid((NetShopCategory)category);
	scriptAssertf(result, "%s : %s - Item category \"%d\" is not valid.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandname, category);
	return result;
#else //!__NET_SHOP_ACTIVE
	return false;
#endif //__NET_SHOP_ACTIVE
}

/////////////////////////////////////////////////////////////
////////////////////////// CATALOG //////////////////////////
/////////////////////////////////////////////////////////////

bool CommandNetGameServerRemoveCatalogItem(scrTextLabel63* BANK_ONLY(NET_SHOP_ONLY(catalogItemKey)))
{
	bool result = false;

	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_REMOVE_CATALOG_ITEM");

#if __BANK && __NET_SHOP_ACTIVE

	atHashString key(*catalogItemKey);

	result = netCatalog::GetInstance().RemoveItem(key);
	scriptAssertf(result, "%s : NET_GAMESERVER_REMOVE_CATALOG_ITEM - Failed to remove item with key \"%s\".", CTheScripts::GetCurrentScriptNameAndProgramCounter(), *catalogItemKey);

#endif // __BANK && __NET_SHOP_ACTIVE

	return result;
}

void CommandNetGameServerAddItemToCatalog(ScriptCatalogItem* BANK_ONLY(NET_SHOP_ONLY(item)))
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_ADD_ITEM_TO_CATALOG");

#if __BANK && __NET_SHOP_ACTIVE
	scriptAssertf(item, "%s: NET_GAMESERVER_ADD_ITEM_TO_CATALOG - NULL info.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!item)
		return;

	char stringname[128];
	const sStatDescription* statDesc = StatsInterface::GetStatDesc(item->m_stathash.Int);
	if (statDesc)
	{
		const char* statName = StatsInterface::GetKeyName(item->m_stathash.Int);
		if (statDesc->GetIsCharacterStat())
			formatf(stringname, sizeof(stringname), &statName[4]);
		else
			formatf(stringname, sizeof(stringname), statName);
	}

	atHashString key(item->m_key);
	scriptAssertf(key.GetCStr(), "%s: NET_GAMESERVER_ADD_ITEM_TO_CATALOG - NULL Key.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	netCatalogBaseItem* catalogitem = NULL;

	switch (item->m_category.Int)
	{
	case CATEGORY_SERVICE:
		catalogitem = rage_new netCatalogServiceItem((int)key.GetHash(), item->m_category.Int, item->m_price.Int);
		break;

	case CATEGORY_SERVICE_WITH_THRESHOLD:
	case CATEGORY_EARN_CURRENCY:
		catalogitem = rage_new netCatalogServiceWithThresholdItem((int)key.GetHash(), item->m_category.Int, item->m_price.Int);
		break;

	case CATEGORY_SERVICE_WITH_LIMIT: 
		catalogitem = rage_new netCatalogServiceLimitedItem((int)key.GetHash(), item->m_category.Int, item->m_price.Int); 
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
		catalogitem = rage_new netCatalogPackedStatInventoryItem((int)key.GetHash()
																	, item->m_category.Int
																	, item->m_price.Int
																	, item->m_statenumvalue.Int);
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
		{
			scriptAssertf(statDesc, "%s: NET_GAMESERVER_ADD_ITEM_TO_CATALOG - NULL statDesc - the stat with hash key='%dx' does not exist.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), item->m_stathash.Int);
			if (!statDesc)
				return;

			scriptAssertf(strlen(stringname) > 0, "%s: NET_GAMESERVER_ADD_ITEM_TO_CATALOG - NULL stringname.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

			catalogitem = rage_new netCatalogInventoryItem((int)key.GetHash(), item->m_category.Int, item->m_price.Int, stringname, statDesc->GetIsCharacterStat() ? false : true);
		}
		break;

	case CATEGORY_MART:
	case CATEGORY_WEAPON_AMMO:
	case CATEGORY_PRICE_MODIFIER:
	case CATEGORY_PRICE_OVERRIDE:
	case CATEGORY_SYSTEM:
	case CATEGORY_VEHICLE_UPGRADE:
	case CATEGORY_CONTRABAND_FLAGS:
	case CATEGORY_CASINO_CHIP_REASON:
	case CATEGORY_CURRENCY_TYPE:
		catalogitem = rage_new netCatalogOnlyItem((int)key.GetHash(), item->m_category.Int, item->m_price.Int);
		break;

	case CATEGORY_FACEPAINT:
	case CATEGORY_BLUSHER:
	case CATEGORY_LIPSTICK:
	case CATEGORY_EYEBROWS:
	case CATEGORY_CHEST_HAIR:
	case CATEGORY_HAIR:
	case CATEGORY_VEHICLE_MOD:
	case CATEGORY_VEHICLE:
	case CATEGORY_PROPERTIE: 
	case CATEGORY_BEARD: 
	case CATEGORY_MKUP:
	case CATEGORY_CONTACTS:
	case CATEGORY_PROPERTY_INTERIOR:
	case CATEGORY_WAREHOUSE:
	case CATEGORY_CONTRABAND_MISSION:
	case CATEGORY_WAREHOUSE_INTERIOR:
		catalogitem = rage_new netCatalogOnlyItemWithStat((int)key.GetHash(), item->m_category.Int, item->m_price.Int, item->m_statvalue.Int);
		break;

	//case CATEGORY_SERVICE_UNLOCKED:
	//case CATEGORY_CLOTH:
	//case CATEGORY_WEAPON:
	//case CATEGORY_TATTOO:
	//case CATEGORY_CONTRABAND_QNTY:
	//case CATEGORY_WAREHOUSE_VEHICLE_INDEX:
	//case CATEGORY_INVENTORY_PRICE_PAID:
	//case CATEGORY_CASINO_CHIPS:
	//case CATEGORY_DECORATION:
	//case CATEGORY_DATA_STORAGE:
	default:
		{
			scriptAssertf(statDesc, "%s: NET_GAMESERVER_ADD_ITEM_TO_CATALOG - NULL statDesc - the stat with hash key='%dx' does not exist.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), item->m_stathash.Int);
			if (!statDesc)
				return;

			scriptAssertf(strlen(item->m_textTag) > 0, "%s: NET_GAMESERVER_ADD_ITEM_TO_CATALOG - Invalid text tag - the stat with hash key='%dx, %s' does not exist.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), item->m_stathash.Int, key.TryGetCStr());
			if (strlen(item->m_textTag) <= 0)
				return;

			scriptAssertf(strlen(stringname) > 0, "%s: NET_GAMESERVER_ADD_ITEM_TO_CATALOG - NULL stringname.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			scriptAssertf(strlen(item->m_textTag) > 0, "%s: NET_GAMESERVER_ADD_ITEM_TO_CATALOG - NULL m_textTag.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

			catalogitem = rage_new netCatalogGeneralItem((int)key.GetHash()
														,item->m_category.Int
														,item->m_textTag
														,item->m_price.Int
														,stringname
														,item->m_storagetype.Int
														,item->m_bitshift.Int
														,item->m_bitsize.Int
														,statDesc->GetIsCharacterStat() ? false : true);
		}
		break;
	}

	if (catalogitem)
	{
		scriptAssertf(catalogitem->GetItemKeyName(), "%s: NET_GAMESERVER_ADD_ITEM_TO_CATALOG - Item='%s' - Missing category name - this will fail Write and crash.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), item->m_key);
		scriptAssertf(catalogitem->GetCategoryName(), "%s: NET_GAMESERVER_ADD_ITEM_TO_CATALOG - Item='%s' - Missing category name - this will fail Write and crash.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), item->m_key);

#if !__FINAL
		if (strlen(item->m_name) > 0)
		{
			catalogitem->SetName(item->m_name);
		}
#endif //!__FINAL

		if (strlen(item->m_textTag) > 0)
		{
			catalogitem->SetLabel(item->m_textTag);
		}
	}

	bool result = false;

	//Adding checks to make sure script cant accidentally add duplicate items
	if (netCatalog::GetInstance().Find( key ) == nullptr)
	{
		result = netCatalog::GetInstance().AddItem(key, catalogitem);
		scriptAssertf(result, "%s : NET_GAMESERVER_ADD_ITEM_TO_CATALOG - Failed to add item with key \"%s\".", CTheScripts::GetCurrentScriptNameAndProgramCounter(), item->m_key);
	}
	else 
	{
		result = false;
		scriptErrorf("%s : NET_GAMESERVER_ADD_ITEM_TO_CATALOG - Failed to add item with key \"%s\" - Already exists.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), key.GetCStr());
	}

	if (!result)
		delete catalogitem;

#endif // __BANK && __NET_SHOP_ACTIVE
}

bool CommandShopChangeItemTextLabel(const char* BANK_ONLY(NET_SHOP_ONLY(itemkey)), const char* BANK_ONLY(NET_SHOP_ONLY(label)))
{
	bool result = false;

	ASSERT_ON_INVALID_COMMAND("NETWORK_SHOP_SET_CATALOG_ITEM_TEXT_LABEL");

#if __BANK && __NET_SHOP_ACTIVE

	scriptAssertf(itemkey, "%s:NETWORK_SHOP_SET_CATALOG_ITEM_TEXT_LABEL - NULL itemkeyA.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!itemkey)
		return result;

	netCatalog& catalogue = netCatalog::GetInstance();

	atHashString itemkeyhash(itemkey);

	const netCatalogBaseItem* catalogitem = catalogue.Find(itemkeyhash);
	scriptAssertf(catalogitem, "%s:NETWORK_SHOP_SET_CATALOG_ITEM_TEXT_LABEL - Catalog Item with key='%s' is not Valid.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), itemkey);
	if (!catalogitem)
		return result;

	result = catalogue.ChangeItemTextLabel(itemkeyhash, label);

#endif // __BANK && __NET_SHOP_ACTIVE

	return result;
}

bool CommandNetGameServerChangeItemPrice(const char* BANK_ONLY(NET_SHOP_ONLY(itemkey)), const int BANK_ONLY(NET_SHOP_ONLY(price)))
{
	bool result = false;

	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_SET_CATALOG_ITEM_PRICE");

#if __BANK && __NET_SHOP_ACTIVE

	scriptAssertf(itemkey, "%s: NET_GAMESERVER_SET_CATALOG_ITEM_PRICE - NULL itemkeyA.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!itemkey)
		return result;

	netCatalog& catalogue = netCatalog::GetInstance();

	atHashString itemkeyhash(itemkey);

	const netCatalogBaseItem* catalogitem = catalogue.Find(itemkeyhash);
	scriptAssertf(catalogitem, "%s: NET_GAMESERVER_SET_CATALOG_ITEM_PRICE - Catalog Item with key='%s' is not Valid.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), itemkey);
	if (!catalogitem)
		return result;

	result = catalogue.ChangeItemPrice(itemkeyhash, price);

#endif // __BANK && __NET_SHOP_ACTIVE

	return result;
}

bool CommandNetGameServerAreCatalogItemsLinked(const char* BANK_ONLY(NET_SHOP_ONLY(itemkeyA)), const char* BANK_ONLY(NET_SHOP_ONLY(itemkeyB)), const bool BANK_ONLY(NET_SHOP_ONLY(checkForCircularLink)))
{
	bool result = false;

	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_ARE_CATALOG_ITEMS_LINKED");

#if __BANK && __NET_SHOP_ACTIVE
	scriptAssertf(itemkeyA, "%s: NET_GAMESERVER_ARE_CATALOG_ITEMS_LINKED - NULL itemkeyA.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!itemkeyA)
		return result;

	scriptAssertf(itemkeyB, "%s: NET_GAMESERVER_ARE_CATALOG_ITEMS_LINKED - NULL itemkeyB.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!itemkeyB)
		return result;

	const netCatalog& catalogue = netCatalog::GetInstance();

	atHashString itemkeyhashA(itemkeyA);
	const netCatalogBaseItem* catalogitemA = catalogue.Find(itemkeyhashA);
	scriptAssertf(catalogitemA, "%s: NET_GAMESERVER_ARE_CATALOG_ITEMS_LINKED - Catalog Item with key='%s' is not Valid.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), itemkeyA);
	if (!catalogitemA)
		return result;

	atHashString itemkeyhashB(itemkeyB);
	const netCatalogBaseItem* catalogitemB = catalogue.Find(itemkeyhashB);
	scriptAssertf(catalogitemB, "%s: NET_GAMESERVER_ARE_CATALOG_ITEMS_LINKED - Catalog Item with key='%s' is not Valid.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), itemkeyB);
	if (!catalogitemB)
		return result;

	result = catalogue.AreItemsLinked(itemkeyhashA, itemkeyhashB);
	if (checkForCircularLink)
	{
		result = result && catalogue.AreItemsLinked(itemkeyhashB, itemkeyhashA);
	}

#endif // __BANK && __NET_SHOP_ACTIVE

	return result;
}

bool CommandNetGameServerRemoveLink(const char* BANK_ONLY(NET_SHOP_ONLY(itemkeyA)), const char* BANK_ONLY(NET_SHOP_ONLY(itemkeyB)), const bool BANK_ONLY(NET_SHOP_ONLY(removeCircularLink)))
{
	bool result = false;

	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_REMOVE_LINKED_CATALOG_ITEMS");

#if __BANK && __NET_SHOP_ACTIVE
	scriptAssertf(itemkeyA, "%s: NET_GAMESERVER_REMOVE_LINKED_CATALOG_ITEMS - NULL itemkeyA.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!itemkeyA)
		return result;

	scriptAssertf(itemkeyB, "%s: NET_GAMESERVER_REMOVE_LINKED_CATALOG_ITEMS - NULL itemkeyB.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!itemkeyB)
		return result;

	netCatalog& catalogue = netCatalog::GetInstance();

	atHashString itemkeyhashA(itemkeyA);
	const netCatalogBaseItem* catalogitemA = catalogue.Find(itemkeyhashA);
	scriptAssertf(catalogitemA, "%s: NET_GAMESERVER_REMOVE_LINKED_CATALOG_ITEMS - Catalog Item with key='%s' is not Valid.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), itemkeyA);
	if (!catalogitemA)
		return result;

	atHashString itemkeyhashB(itemkeyB);
	const netCatalogBaseItem* catalogitemB = catalogue.Find(itemkeyhashB);
	scriptAssertf(catalogitemB, "%s: NET_GAMESERVER_REMOVE_LINKED_CATALOG_ITEMS - Catalog Item with key='%s' is not Valid.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), itemkeyB);
	if (!catalogitemB)
		return result;

	result = catalogue.RemoveLinkedItems(itemkeyhashA, itemkeyhashB);

	if (removeCircularLink)
	{
		result = catalogue.RemoveLinkedItems(itemkeyhashB, itemkeyhashA);
	}

#endif // __BANK && __NET_SHOP_ACTIVE

	return result;
}

bool  CommandNetGameServerLinkCatalogItems(const char* BANK_ONLY(NET_SHOP_ONLY(itemkeyA)), const char* BANK_ONLY(NET_SHOP_ONLY(itemkeyB)), const bool BANK_ONLY(NET_SHOP_ONLY(createCircularLink)))
{
	bool result = false;

	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_LINK_CATALOG_ITEMS");

#if __BANK && __NET_SHOP_ACTIVE
	scriptAssertf(itemkeyA, "%s: NET_GAMESERVER_LINK_CATALOG_ITEMS - NULL itemkeyA.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!itemkeyA)
		return result;

	scriptAssertf(itemkeyB, "%s: NET_GAMESERVER_LINK_CATALOG_ITEMS - NULL itemkeyB.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!itemkeyB)
		return result;

	netCatalog& catalogue = netCatalog::GetInstance();

	atHashString itemkeyhashA(itemkeyA);
	const netCatalogBaseItem* catalogitemA = catalogue.Find(itemkeyhashA);
	scriptAssertf(catalogitemA, "%s: NET_GAMESERVER_LINK_CATALOG_ITEMS - Catalog Item with key='%s' is not Valid.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), itemkeyA);
	if (!catalogitemA)
		return result;

	atHashString itemkeyhashB(itemkeyB);
	const netCatalogBaseItem* catalogitemB = catalogue.Find(itemkeyhashB);
	scriptAssertf(catalogitemB, "%s: NET_GAMESERVER_LINK_CATALOG_ITEMS - Catalog Item with key='%s' is not Valid.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), itemkeyB);
	if (!catalogitemB)
		return result;

	result = catalogue.LinkItems(itemkeyhashA, itemkeyhashB);

	if (createCircularLink)
	{
		result = result && catalogue.LinkItems(itemkeyhashB, itemkeyhashA);
	}

	scriptAssertf(result, "%s : NET_GAMESERVER_LINK_CATALOG_ITEMS - Failed to Link item with keyA=\"%s\", keyB=\"%s\".", CTheScripts::GetCurrentScriptNameAndProgramCounter(), itemkeyA, itemkeyB);
#endif // __BANK && __NET_SHOP_ACTIVE

	return result;
}

bool  CommandNetGameServerLinkCatalogItemsForSelling(const char* BANK_ONLY(NET_SHOP_ONLY(itemkeyA)), const char* BANK_ONLY(NET_SHOP_ONLY(itemkeyB)))
{
	bool result = false;

	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_LINK_CATALOG_ITEMS_FOR_SELLING");

#if __BANK && __NET_SHOP_ACTIVE

	scriptAssertf(itemkeyA, "%s: NET_GAMESERVER_LINK_CATALOG_ITEMS_FOR_SELLING - NULL itemkeyA.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!itemkeyA)
		return result;

	scriptAssertf(itemkeyB, "%s: NET_GAMESERVER_LINK_CATALOG_ITEMS_FOR_SELLING - NULL itemkeyB.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!itemkeyB)
		return result;

	netCatalog& catalogue = netCatalog::GetInstance();

	atHashString itemkeyhashA(itemkeyA);
	const netCatalogBaseItem* catalogitemA = catalogue.Find(itemkeyhashA);
	scriptAssertf(catalogitemA, "%s: NET_GAMESERVER_LINK_CATALOG_ITEMS - Catalog Item with key='%s' is not Valid.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), itemkeyA);
	if (!catalogitemA)
		return result;

	atHashString itemkeyhashB(itemkeyB);
	const netCatalogBaseItem* catalogitemB = catalogue.Find(itemkeyhashB);
	scriptAssertf(catalogitemB, "%s: NET_GAMESERVER_LINK_CATALOG_ITEMS_FOR_SELLING - Catalog Item with key='%s' is not Valid.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), itemkeyB);
	if (!catalogitemB)
		return result;

	result = catalogue.LinkItemsForSelling(itemkeyhashA, itemkeyhashB);
	scriptAssertf(result, "%s : NET_GAMESERVER_LINK_CATALOG_ITEMS_FOR_SELLING - Failed to Link item with keyA=\"%s\", keyB=\"%s\".", CTheScripts::GetCurrentScriptNameAndProgramCounter(), itemkeyA, itemkeyB);

#endif // __BANK && __NET_SHOP_ACTIVE

	return result;
}

bool CommandNetGameServerAreCatalogItemsLinkedForSelling(const char* BANK_ONLY(NET_SHOP_ONLY(itemkeyA)), const char* BANK_ONLY(NET_SHOP_ONLY(itemkeyB)))
{
	bool result = false;

	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_ARE_CATALOG_ITEMS_LINKED_FOR_SELLING");

#if __BANK && __NET_SHOP_ACTIVE

	scriptAssertf(itemkeyA, "%s: NET_GAMESERVER_ARE_CATALOG_ITEMS_LINKED_FOR_SELLING - NULL itemkeyA.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!itemkeyA)
		return result;

	scriptAssertf(itemkeyB, "%s: NET_GAMESERVER_ARE_CATALOG_ITEMS_LINKED_FOR_SELLING - NULL itemkeyB.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!itemkeyB)
		return result;

	const netCatalog& catalogue = netCatalog::GetInstance();

	atHashString itemkeyhashA(itemkeyA);
	const netCatalogBaseItem* catalogitemA = catalogue.Find(itemkeyhashA);
	scriptAssertf(catalogitemA, "%s: NET_GAMESERVER_ARE_CATALOG_ITEMS_LINKED_FOR_SELLING - Catalog Item with key='%s' is not Valid.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), itemkeyA);
	if (!catalogitemA)
		return result;

	atHashString itemkeyhashB(itemkeyB);
	const netCatalogBaseItem* catalogitemB = catalogue.Find(itemkeyhashB);
	scriptAssertf(catalogitemB, "%s: NET_GAMESERVER_ARE_CATALOG_ITEMS_LINKED_FOR_SELLING - Catalog Item with key='%s' is not Valid.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), itemkeyB);
	if (!catalogitemB)
		return result;

	result = catalogue.AreItemsLinkedForSelling(itemkeyhashA, itemkeyhashB);
#endif // __BANK && __NET_SHOP_ACTIVE

	return result;
}

bool CommandNetGameServerRemoveLinkForSelling(const char* BANK_ONLY(NET_SHOP_ONLY(itemkeyA)), const char* BANK_ONLY(NET_SHOP_ONLY(itemkeyB)))
{
	bool result = false;

	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_REMOVE_LINK_CATALOG_ITEMS_FOR_SELLING");

#if __BANK && __NET_SHOP_ACTIVE

	scriptAssertf(itemkeyA, "%s: NET_GAMESERVER_REMOVE_LINK_CATALOG_ITEMS_FOR_SELLING - NULL itemkeyA.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!itemkeyA)
		return result;

	scriptAssertf(itemkeyB, "%s: NET_GAMESERVER_REMOVE_LINK_CATALOG_ITEMS_FOR_SELLING - NULL itemkeyB.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!itemkeyB)
		return result;

	netCatalog& catalogue = netCatalog::GetInstance();

	atHashString itemkeyhashA(itemkeyA);
	const netCatalogBaseItem* catalogitemA = catalogue.Find(itemkeyhashA);
	scriptAssertf(catalogitemA, "%s: NET_GAMESERVER_REMOVE_LINK_CATALOG_ITEMS_FOR_SELLING - Catalog Item with key='%s' is not Valid.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), itemkeyA);
	if (!catalogitemA)
		return result;

	atHashString itemkeyhashB(itemkeyB);
	const netCatalogBaseItem* catalogitemB = catalogue.Find(itemkeyhashB);
	scriptAssertf(catalogitemB, "%s: NET_GAMESERVER_REMOVE_LINK_CATALOG_ITEMS_FOR_SELLING - Catalog Item with key='%s' is not Valid.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), itemkeyB);
	if (!catalogitemB)
		return result;

	result = catalogue.RemoveLinkedItemsForSelling(itemkeyhashA, itemkeyhashB);
#endif // __BANK && __NET_SHOP_ACTIVE

	return result;
}

bool  CommandNetGameServerCheckItemIsValid(const char* NET_SHOP_ONLY(itemkey))
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_CATALOG_ITEM_IS_VALID");

#if __NET_SHOP_ACTIVE

	scriptAssertf(itemkey, "%s: NET_GAMESERVER_CATALOG_ITEM_IS_VALID - NULL itemkey.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!itemkey)
		return false;

	const int sizeOfKey = (int)strlen(itemkey);

	scriptAssertf(sizeOfKey > 0, "%s: NET_GAMESERVER_CATALOG_ITEM_IS_VALID - invalid sizeOfKey='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), sizeOfKey);
	if (sizeOfKey <= 0)
		return false;

	scriptAssertf(sizeOfKey <= CATALOG_MAX_KEY_LABEL_SIZE, "%s: NET_GAMESERVER_CATALOG_ITEM_IS_VALID - invalid sizeOfKey='%d' max size is '%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), sizeOfKey, CATALOG_MAX_KEY_LABEL_SIZE);
	if (sizeOfKey > CATALOG_MAX_KEY_LABEL_SIZE)
		return false;

	if (!GameTransactionSessionMgr::Get().IsReady())
	{
		scriptErrorf("%s: NET_GAMESERVER_CATALOG_ITEM_IS_VALID - Session not ready.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return false;
	}

	netCatalog& catalogue = netCatalog::GetInstance();
	if (!catalogue.HasReadSucceeded())
	{
		scriptErrorf("%s: NET_GAMESERVER_CATALOG_ITEM_IS_VALID - HasReadSucceeded FAILED.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return false;
	}

	atHashString itemitemkey(itemkey);
	const netCatalogBaseItem* item = catalogue.Find( itemitemkey );
	if (item)
	{
		return true;
	}

#endif // __NET_SHOP_ACTIVE

	return false;
}

bool  CommandNetGameServerCheckItemKeyIsValid(const int NET_SHOP_ONLY(itemkey))
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_CATALOG_ITEM_KEY_IS_VALID");

#if __NET_SHOP_ACTIVE

	atHashString itemitemkey(itemkey);
	const netCatalogBaseItem* item = netCatalog::GetInstance().Find( itemitemkey );
	if (item)
	{
		return true;
	}

#endif // __NET_SHOP_ACTIVE

	return false;
}


/////////////////////////////////////////////////////////////////////
////////////////////////// CATALOG ITEMS //////////////////////////
/////////////////////////////////////////////////////////////////////


bool CommandNetGameServerIsCatalogValid( )
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_CATALOG_IS_VALID");

#if __NET_SHOP_ACTIVE
	
	const bool result = netCatalog::GetInstance().HasReadSucceeded();

#if !__NO_OUTPUT
	static bool sm_result = false;
	if (sm_result != result)
	{
		sm_result = result;
		statDebugf1("%s: NET_GAMESERVER_CATALOG_IS_VALID - result='%s'", CTheScripts::GetCurrentScriptNameAndProgramCounter(), result ? "true" : "false");
	}
#endif // !__NO_OUTPUT

	return result;

#else //!__NET_SHOP_ACTIVE
return false;
#endif //__NET_SHOP_ACTIVE
}

bool CommandNetGameServerIsCatalogCurrent()
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_IS_CATALOG_CURRENT");

#if __NET_SHOP_ACTIVE

	const bool result = netCatalog::GetInstance().HasReadSucceeded() && netCatalog::GetInstance().IsLatestVersion();

#if !__NO_OUTPUT
	static bool sm_result = false;
	if (sm_result != result)
	{
		statDebugf1("%s: NET_GAMESERVER_IS_CATALOG_CURRENT - result='%s'", CTheScripts::GetCurrentScriptNameAndProgramCounter(), result ? "true" : "false");
		sm_result = result;
	}
#endif //!__NO_OUTPUT

	return result;

#else //!__NET_SHOP_ACTIVE
	return false;
#endif //__NET_SHOP_ACTIVE
}

bool CommandNetGameServerRetrieveCatalogRefreshStatus( int& NET_SHOP_ONLY(currentStatus) )
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_RETRIEVE_CATALOG_REFRESH_STATUS");

#if __NET_SHOP_ACTIVE
	bool isValid = false;

	currentStatus = TRANSACTION_STATUS_NONE; // None

	netCatalog& catalogue = netCatalog::GetInstance();
	if (catalogue.GetStatus().Pending())
	{
		currentStatus = TRANSACTION_STATUS_PENDING; // Pending
		statDebugf1("%s: NET_GAMESERVER_RETRIEVE_CATALOG_REFRESH_STATUS - state='Pending'", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	else if (catalogue.HasReadFailed())
	{
		currentStatus = TRANSACTION_STATUS_FAILED; // Failed
		statDebugf1("%s : NET_GAMESERVER_RETRIEVE_CATALOG_REFRESH_STATUS - state='Failed'", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	else if (catalogue.HasReadSucceeded())
	{
		isValid = true;
		currentStatus = TRANSACTION_STATUS_SUCCESSFULL; // Succeeded
		statDebugf1("%s : NET_GAMESERVER_RETRIEVE_CATALOG_REFRESH_STATUS - state='Succeeded'", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	else
	{
		statDebugf1("%s: NET_GAMESERVER_RETRIEVE_CATALOG_REFRESH_STATUS - state='NONE'", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	return isValid;
#else //!__NET_SHOP_ACTIVE
	return false;
#endif //__NET_SHOP_ACTIVE
}

int CommandNetGameServerCatalogCloudCRC()
{
	ASSERT_ON_INVALID_COMMAND("NETWORK_GET_CATALOG_CLOUD_CRC");
#if __NET_SHOP_ACTIVE
	return static_cast<int>(netCatalog::GetInstance().GetCRC());
#else //!__NET_SHOP_ACTIVE
	return 0;
#endif //__NET_SHOP_ACTIVE
}

bool CommandNetGameServerRefreshServerCatalog()
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_REFRESH_SERVER_CATALOG");

#if __NET_SHOP_ACTIVE

	if (!netCatalog::GetInstance().IsLatestVersion())
	{
		GameTransactionSessionMgr::Get().FlagCatalogForRefresh();
		statDebugf1("%s:Command NET_GAMESERVER_REFRESH_SERVER_CATALOG - Command called - FlagCatalogForRefresh().", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	else
	{
		statDebugf1("%s:Command NET_GAMESERVER_REFRESH_SERVER_CATALOG - Command called - AVOID FlagCatalogForRefresh() - Catalog is Latest version.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	return true;
#else
	return false;
#endif
}

/////////////////////////////////////////////////////////////////////
//////////////////////////// SHOP SESSION ///////////////////////////
/////////////////////////////////////////////////////////////////////

bool CommandNetGameServerInitServer()
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_INIT_SESSION");

#if __NET_SHOP_ACTIVE
	GameTransactionSessionMgr& rSession = GameTransactionSessionMgr::Get();
	scriptAssertf(rSession.IsInvalid() || rSession.HasFailed(),"%s: NET_GAMESERVER_INIT_SESSION - This should onyl be called once, the first time entering MP.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	statDebugf1("%s:Command NET_GAMESERVER_INIT_SESSION - Command called", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	return rSession.InitManager(NetworkInterface::GetLocalGamerIndex());
#else //!__NET_SHOP_ACTIVE
	return false;
#endif // __NET_SHOP_ACTIVE
}

bool CommandNetGameServerRetrieveSessionInitStatus( int& NET_SHOP_ONLY(currentStatus) )
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_RETRIEVE_INIT_SESSION_STATUS");

#if __NET_SHOP_ACTIVE
	bool isValid = false;

	OUTPUT_ONLY(static int sm_status = -1;)

	currentStatus = TRANSACTION_STATUS_NONE;
	GameTransactionSessionMgr& rSession = GameTransactionSessionMgr::Get();

	if (rSession.IsInvalid())
	{
		currentStatus = TRANSACTION_STATUS_NONE;
		
#if !__NO_OUTPUT
		if (sm_status != currentStatus)
		{
			sm_status = currentStatus;
			statDebugf1("%s:Command NET_GAMESERVER_RETRIEVE_INIT_SESSION_STATUS - state='Invalid'", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
#endif // !__NO_OUTPUT
	}
	else if (rSession.IsInitPending())
	{
		currentStatus = TRANSACTION_STATUS_PENDING;

#if !__NO_OUTPUT
		if (sm_status != currentStatus)
		{
			sm_status = currentStatus;
			statDebugf1("%s:Command NET_GAMESERVER_RETRIEVE_INIT_SESSION_STATUS - state='Pending'", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
#endif // !__NO_OUTPUT
	}
	else if (rSession.HasFailed())
	{
		currentStatus = TRANSACTION_STATUS_FAILED;

#if !__NO_OUTPUT
		if (sm_status != currentStatus)
		{
			sm_status = currentStatus;
			statDebugf1("%s:Command NET_GAMESERVER_RETRIEVE_INIT_SESSION_STATUS - state='Failed', code='%d'", CTheScripts::GetCurrentScriptNameAndProgramCounter(), rSession.GetErrorCode());
		}
#endif // !__NO_OUTPUT
	}
	else if (rSession.HasInitSucceeded())
	{
		isValid = true;
		currentStatus = TRANSACTION_STATUS_SUCCESSFULL;

#if !__NO_OUTPUT
		if (sm_status != currentStatus)
		{
			sm_status = currentStatus;
			statDebugf1("%s:Command NET_GAMESERVER_RETRIEVE_INIT_SESSION_STATUS - state='Succeeded'", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
#endif // !__NO_OUTPUT
	}

	return isValid;
#else
	return false;
#endif // __NET_SHOP_ACTIVE
}

bool  CommandNetGameServerStartSessionPending()
{
#if __NET_SHOP_ACTIVE
	GameTransactionSessionMgr& rSession = GameTransactionSessionMgr::Get();
	bool result = rSession.IsCatalogComplete() && rSession.IsSlotInvalid();
	statDebugf1("%s:Command NET_GAMESERVER_START_SESSION_PENDING - result='%s'", CTheScripts::GetCurrentScriptNameAndProgramCounter(), result ? "true":"false");
	return result;
#else
	return false;
#endif // __NET_SHOP_ACTIVE
}

bool CommandNetGameServerStartSession(const int NET_SHOP_ONLY(slot))
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_START_SESSION");

#if __NET_SHOP_ACTIVE
	bool result = false;

	CProfileStats& profileStatsMgr = CLiveManager::GetProfileStatsMgr();
	if (!profileStatsMgr.GetMPSynchIsDone(  ))
	{
		scriptAssertf(0, "%s : NET_GAMESERVER_START_SESSION - Profile Stats not Synchronized - GetMPSynchIsDone FAILED.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptErrorf("%s : NET_GAMESERVER_START_SESSION - Profile Stats not Synchronized - GetMPSynchIsDone FAILED.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return false;
	}

	if( !VERIFY_STATS_LOADED(ASSERT_ONLY("NET_GAMESERVER_START_SESSION")))
	{
		scriptAssertf(0, "%s : NET_GAMESERVER_START_SESSION - VERIFY_STATS_LOADED FAILED.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptErrorf("%s : NET_GAMESERVER_START_SESSION - VERIFY_STATS_LOADED FAILED.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return false;
	}

	scriptAssertf(0 <= slot && slot < MAX_NUM_MP_CHARS,"%s:Command NET_GAMESERVER_START_SESSION - SESSION START IS NOT PENDING", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (0 <= slot && slot < MAX_NUM_MP_CHARS)
	{
		GameTransactionSessionMgr& rSession = GameTransactionSessionMgr::Get();
		result = rSession.SessionStart(NetworkInterface::GetLocalGamerIndex(), slot, true);
		statDebugf1("%s:Command NET_GAMESERVER_START_SESSION - slot='%d', result='%s'", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot, result?"true":"false");
	}
	else
	{
		scriptErrorf("%s:Command NET_GAMESERVER_START_SESSION - SESSION START - Invalid slot='%d'", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot);
	}

	return result;
#else
return false;
#endif // __NET_SHOP_ACTIVE
}

bool CommandNetGameServerStartSessionApplyDataPending()
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_START_SESSION_APPLY_DATA_PENDING");

#if __NET_SHOP_ACTIVE
	GameTransactionSessionMgr& rSession = GameTransactionSessionMgr::Get();
	bool result = rSession.StartSessionApplyDataPending();
	statDebugf1("%s:Command NET_GAMESERVER_START_SESSION_APPLY_DATA_PENDING - result='%s'", CTheScripts::GetCurrentScriptNameAndProgramCounter(), result?"true":"false");
	return result;
#else
	return false;
#endif // __NET_SHOP_ACTIVE
}


bool CommandNetGameServerRetrieveSessionStartStatus( int& NET_SHOP_ONLY(currentStatus) )
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_RETRIEVE_START_SESSION_STATUS");

#if __NET_SHOP_ACTIVE
	bool isValid = false;

	currentStatus = TRANSACTION_STATUS_NONE;
	GameTransactionSessionMgr& rSession = GameTransactionSessionMgr::Get();

	if (rSession.IsInvalid())
	{
		currentStatus = TRANSACTION_STATUS_NONE;
		statDebugf1("%s:Command NET_GAMESERVER_RETRIEVE_START_SESSION_STATUS - state='Invalid'", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	else if (rSession.IsCatalogComplete() && rSession.IsSlotInvalid())
	{
		isValid = true;
		currentStatus = TRANSACTION_STATUS_SUCCESSFULL;
		statDebugf1("%s:Command NET_GAMESERVER_RETRIEVE_START_SESSION_STATUS - state='IsCatalogComplete() && IsSlotInvalid()'", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	else if (rSession.IsStartPending())
	{
		currentStatus = TRANSACTION_STATUS_PENDING;
		statDebugf1("%s:Command NET_GAMESERVER_RETRIEVE_START_SESSION_STATUS - state='Pending'", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	else if (rSession.HasFailed())
	{
		currentStatus = TRANSACTION_STATUS_FAILED;
		statDebugf1("%s:Command NET_GAMESERVER_RETRIEVE_START_SESSION_STATUS - state='Failed', code='%d'", CTheScripts::GetCurrentScriptNameAndProgramCounter(), rSession.GetErrorCode());
	}
	else if (rSession.IsReady())
	{
		isValid = true;
		currentStatus = TRANSACTION_STATUS_SUCCESSFULL;
		statDebugf1("%s:Command NET_GAMESERVER_RETRIEVE_START_SESSION_STATUS - state='Succeeded'", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	return isValid;
#else
	return false;
#endif // __NET_SHOP_ACTIVE
}

bool CommandNetGameServerRetrieveGameServerSessionErrorCode( int& NET_SHOP_ONLY(errorCode) )
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_RETRIEVE_SESSION_ERROR_CODE");

#if __NET_SHOP_ACTIVE
	bool isValid = false;

	errorCode = 0;
	GameTransactionSessionMgr& rSession = GameTransactionSessionMgr::Get();

	if (rSession.HasFailed())
	{
		isValid = true;
		errorCode = rSession.GetErrorCode();
		statDebugf1("%s:Command NET_GAMESERVER_RETRIEVE_SESSION_ERROR_CODE - code='%d'", CTheScripts::GetCurrentScriptNameAndProgramCounter(), errorCode);
	}

	return isValid;
#else
	return false;
#endif // __NET_SHOP_ACTIVE
}

bool CommandNetGameServerClearSession( const int NET_SHOP_ONLY(slot) )
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_CLEAR_SESSION");

#if __NET_SHOP_ACTIVE
	GameTransactionSessionMgr& sessionMgr = GameTransactionSessionMgr::Get();

	scriptAssertf(slot>=0 || sessionMgr.HasFailed(),"%s:Command NET_GAMESERVER_CLEAR_SESSION - invalid slot='%d'", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot);
	scriptAssertf(slot<MAX_NUM_MP_CHARS,"%s:Command NET_GAMESERVER_CLEAR_SESSION - invalid slot='%d'", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot);
	
	statDebugf1("%s:Command NET_GAMESERVER_CLEAR_SESSION - slot='%d', HasFailed='%s'", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot, sessionMgr.HasFailed()?"true":"false");

	return sessionMgr.ClearStartSession(slot);
#else
	return false;
#endif // __NET_SHOP_ACTIVE
}

bool CommandNetGameServerIsSessionValid( const int NET_SHOP_ONLY(slot) )
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_IS_SESSION_VALID");

#if __NET_SHOP_ACTIVE
	scriptAssertf(slot>=0,"%s:Command NET_GAMESERVER_IS_SESSION_VALID - invalid slot='%d'", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot);

	scriptAssertf(slot<MAX_NUM_MP_CHARS,"%s:Command NET_GAMESERVER_IS_SESSION_VALID - invalid slot='%d'", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot);

	statDebugf1("%s:Command NET_GAMESERVER_IS_SESSION_VALID - slot='%d', currentSlot='%d'", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot, GameTransactionSessionMgr::Get().GetSlot());

	const bool result = GameTransactionSessionMgr::Get().IsSlotValid(slot);

#if !__NO_OUTPUT
	if (!result)
	{
		if (GameTransactionSessionMgr::Get().IsSlotInvalid())
			statErrorf("%s:Command NET_GAMESERVER_IS_SESSION_VALID - slot='%d' - Slot is Invalid.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot);
		if (!GameTransactionSessionMgr::Get().IsReady())
			statErrorf("%s:Command NET_GAMESERVER_IS_SESSION_VALID - slot='%d' - Is not READY.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot);
	}
#endif

	return result;
#else
	return false;
#endif // __NET_SHOP_ACTIVE
}

bool  CommandNetGameServerGetStateAndRefreshStatus(int& NET_SHOP_ONLY(currentState), int& NET_SHOP_ONLY(refreshSessionRequested))
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_GET_SESSION_STATE_AND_STATUS");

#if __NET_SHOP_ACTIVE

	const bool result = GameTransactionSessionMgr::Get().IsRefreshPending();

	currentState            = GameTransactionSessionMgr::Get().GetState();
	refreshSessionRequested = GameTransactionSessionMgr::Get().IsRefreshSessionRequested() ? 1 : 0;

	if (result)
	{
		statDebugf1("%s:Command NET_GAMESERVER_GET_SESSION_STATE_AND_STATUS - result='%s'", CTheScripts::GetCurrentScriptNameAndProgramCounter(), result ? "true" : "false");
	}

	return result;

#else

	return false;

#endif // __NET_SHOP_ACTIVE
}

bool  CommandNetGameServerGetSlotIsValid(int NET_SHOP_ONLY(slot))
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_GET_SLOT_IS_VALID");

#if __NET_SHOP_ACTIVE
	scriptAssertf(slot>=0,"%s:Command NET_GAMESERVER_GET_SLOT_IS_VALID - invalid slot='%d'", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot);
	scriptAssertf(slot<MAX_NUM_MP_CHARS,"%s:Command NET_GAMESERVER_GET_SLOT_IS_VALID - invalid slot='%d'", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot);
	scriptAssertf(GameTransactionSessionMgr::Get().IsReady(),"%s:Command NET_GAMESERVER_GET_SLOT_IS_VALID - HasInitSucceeded().", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	const bool result = GameTransactionSessionMgr::Get().GetIsSlotValid(slot);
	statDebugf1("%s:Command NET_GAMESERVER_GET_SLOT_IS_VALID - result='%s'", CTheScripts::GetCurrentScriptNameAndProgramCounter(), result ? "true" : "false");

	return result;

#else

	return false;

#endif // __NET_SHOP_ACTIVE
}


bool  CommandNetGameServerIsSessionRefreshPending( )
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_IS_SESSION_REFRESH_PENDING");

#if __NET_SHOP_ACTIVE

	const bool result = GameTransactionSessionMgr::Get().IsRefreshPending();

	if (result)
	{
		statDebugf1("%s:Command NET_GAMESERVER_IS_SESSION_REFRESH_PENDING - result='%s'", CTheScripts::GetCurrentScriptNameAndProgramCounter(), result ? "true" : "false");
	}

	return result;

#else

	return false;

#endif // __NET_SHOP_ACTIVE
}


bool CommandNetGameServerSessionApplyReceivedData( const int NET_SHOP_ONLY(slot) )
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_SESSION_APPLY_RECEIVED_DATA");

#if __NET_SHOP_ACTIVE

	scriptAssertf(slot>=0,"%s:Command NET_GAMESERVER_SESSION_APPLY_RECEIVED_DATA - invalid slot='%d'", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot);

	scriptAssertf(slot<MAX_NUM_MP_CHARS,"%s:Command NET_GAMESERVER_SESSION_APPLY_RECEIVED_DATA - invalid slot='%d'", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot);

	statDebugf1("%s:Command NET_GAMESERVER_SESSION_APPLY_RECEIVED_DATA - slot='%d'", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot);

	GameTransactionSessionMgr& rSession = GameTransactionSessionMgr::Get();
	if(scriptVerifyf(rSession.IsSlotValid(slot), "%s:Command NET_GAMESERVER_SESSION_APPLY_RECEIVED_DATA - Session is not valid for slot='%d'", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot))
	{
		if(VERIFY_STATS_LOADED(ASSERT_ONLY("NET_GAMESERVER_SESSION_APPLY_RECEIVED_DATA")))
		{
			return rSession.ApplyData();
		}
	}

	return false;
#else
	return false;
#endif // __NET_SHOP_ACTIVE
}

bool CommandNetGameServerSessionRestart(const bool NET_SHOP_ONLY(inventory), const bool NET_SHOP_ONLY(playerbalance))
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_START_SESSION_RESTART");

#if __NET_SHOP_ACTIVE
	bool result = false;

	CProfileStats& profileStatsMgr = CLiveManager::GetProfileStatsMgr();
	if (!profileStatsMgr.GetMPSynchIsDone(  ))
	{
		scriptErrorf("%s : NET_GAMESERVER_START_SESSION_RESTART - Profile Stats not Synchronized - GetMPSynchIsDone FAILED.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	GameTransactionSessionMgr& rSession = GameTransactionSessionMgr::Get();

	statAssertf(!rSession.IsQueueBusy(), "%s:Command NET_GAMESERVER_START_SESSION_RESTART - Queue is Busy.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!rSession.IsQueueBusy())
	{
		statAssertf(rSession.IsReady(), "%s:Command NET_GAMESERVER_START_SESSION_RESTART - Session is not ready.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (rSession.IsReady())
		{
			result = rSession.GetBalance(inventory, playerbalance);
			scriptDebugf1("%s:Command NET_GAMESERVER_START_SESSION_RESTART - result='%s'", CTheScripts::GetCurrentScriptNameAndProgramCounter(), result?"true":"false");
		}
		else
		{
			scriptErrorf("%s:Command NET_GAMESERVER_START_SESSION_RESTART - FAILED - Session is not ready.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
	else
	{
		scriptErrorf("%s:Command NET_GAMESERVER_START_SESSION_RESTART - FAILED - Queue is busy.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	return result;
#else
	return false;
#endif // __NET_SHOP_ACTIVE
}

bool CommandNetGameServerSessionRestartStatus( int& NET_SHOP_ONLY(currentStatus) )
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_START_SESSION_RESTART_STATUS");

#if __NET_SHOP_ACTIVE
	bool isValid = false;

	currentStatus = TRANSACTION_STATUS_NONE;
	GameTransactionSessionMgr& rSession = GameTransactionSessionMgr::Get();

	if (rSession.IsInvalid())
	{
		currentStatus = TRANSACTION_STATUS_NONE;
		statDebugf1("%s:Command NET_GAMESERVER_START_SESSION_RESTART_STATUS - state='Invalid'", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	else if (rSession.IsCatalogComplete() && rSession.IsSlotInvalid())
	{
		isValid = true;
		currentStatus = TRANSACTION_STATUS_SUCCESSFULL;
		statDebugf1("%s:Command NET_GAMESERVER_START_SESSION_RESTART_STATUS - state='IsCatalogComplete() && IsSlotInvalid()'", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	else if (rSession.IsStartPending())
	{
		currentStatus = TRANSACTION_STATUS_PENDING;
		statDebugf1("%s:Command NET_GAMESERVER_START_SESSION_RESTART_STATUS - state='Pending'", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	else if (rSession.HasFailed())
	{
		currentStatus = TRANSACTION_STATUS_FAILED;
		statDebugf1("%s:Command NET_GAMESERVER_START_SESSION_RESTART_STATUS - state='Failed', code='%d'", CTheScripts::GetCurrentScriptNameAndProgramCounter(), rSession.GetErrorCode());
	}
	else if (rSession.IsReady())
	{
		isValid = true;
		currentStatus = TRANSACTION_STATUS_SUCCESSFULL;
		statDebugf1("%s:Command NET_GAMESERVER_START_SESSION_RESTART_STATUS - state='Succeeded'", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	return isValid;
#else
	return false;
#endif // __NET_SHOP_ACTIVE
}

bool CommandNetGameServerSessionRestartErrorCode( int& NET_SHOP_ONLY(errorCode) )
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_START_SESSION_RESTART_ERROR_CODE");

#if __NET_SHOP_ACTIVE
	bool isValid = false;

	errorCode = 0;
	GameTransactionSessionMgr& rSession = GameTransactionSessionMgr::Get();

	if (rSession.HasFailed())
	{
		isValid = true;
		errorCode = rSession.GetErrorCode();
		statDebugf1("%s:Command NET_GAMESERVER_START_SESSION_RESTART_ERROR_CODE - code='%d'", CTheScripts::GetCurrentScriptNameAndProgramCounter(), errorCode);
	}

	return isValid;
#else
	return false;
#endif // __NET_SHOP_ACTIVE
}

bool CommandNetGameServerTransactionInProgress()
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_TRANSACTION_IN_PROGRESS");

#if __NET_SHOP_ACTIVE
	return GameTransactionSessionMgr::Get().IsQueueBusy();
#else
	return false;
#endif // __NET_SHOP_ACTIVE
}


/////////////////////////////////////////////////////////////////////
////////////////////////// SHOPPING BASKET //////////////////////////
/////////////////////////////////////////////////////////////////////

int CommandNetGameServerGetPrice( const int NET_SHOP_ONLY(id), int NET_SHOP_ONLY(category), const int UNUSED_PARAM(quantity) )
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_GET_PRICE");

#if __NET_SHOP_ACTIVE
	int cost = -1;

	if( !VERIFY_STATS_LOADED(ASSERT_ONLY("NET_GAMESERVER_GET_PRICE", false)) )
		return cost;

	if ( VERIFY_ITEM_ISVALID( id ASSERT_ONLY(,"NET_GAMESERVER_GET_PRICE")) && VERIFY_SHOP_TRANSACTION_CATEGORY_ISVALID( static_cast<NetShopCategory>(category) ASSERT_ONLY(,"NET_GAMESERVER_GET_PRICE")) )
	{
		const netCatalog& catalogue = netCatalog::GetInstance();

		atHashString key((u32)id);
		const netCatalogBaseItem* item = catalogue.Find(key);
		if (item)
		{
			cost = item->GetPrice();
		}
	}

	return cost;
#else
	return 0;
#endif // __NET_SHOP_ACTIVE
}

int CommandNetGameServerGetItemCategory( const int NET_SHOP_ONLY(id) )
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_GET_CATEGORY");

#if __NET_SHOP_ACTIVE
	int category = 0;

	if( !VERIFY_STATS_LOADED(ASSERT_ONLY("NET_GAMESERVER_GET_CATEGORY")) )
		return category;

	if ( VERIFY_ITEM_ISVALID( id ASSERT_ONLY(,"NET_GAMESERVER_GET_CATEGORY")) )
	{
		const netCatalog& catalogue = netCatalog::GetInstance();

		atHashString key((u32)id);
		const netCatalogBaseItem* item = catalogue.Find(key);
		if (item)
		{
			category = item->GetCategory();
		}
	}

	return category;
#else
	return 0;
#endif // __NET_SHOP_ACTIVE
}

bool CommandNetGameServerBasketStart(int& NET_SHOP_ONLY(transactionId)
								   ,int NET_SHOP_ONLY(category)
								   ,const int NET_SHOP_ONLY(action)
								   ,int NET_SHOP_ONLY(flags))
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_BASKET_START");

#if __NET_SHOP_ACTIVE

	if (!VERIFY_GAMESERVER_USE())
	{
		scriptErrorf("%s : NET_GAMESERVER_BASKET_START - script is NOT whitelisted.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return false;
	}

	bool result = false;

	transactionId = NET_SHOP_INVALID_TRANS_ID;

	if( !VERIFY_STATS_LOADED(ASSERT_ONLY("NET_GAMESERVER_BASKET_START")) )
		return result;

	if( !VERIFY_SHOP_ACTION_TYPE_ISVALID( action ASSERT_ONLY(,"NET_GAMESERVER_BASKET_START")) )
		return result;

	scriptAssertf(!GameTransactionSessionMgr::Get().IsRefreshPending(), "%s : NET_GAMESERVER_BASKET_START - Session is being REFRESHED - SHOP CLOSED.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (GameTransactionSessionMgr::Get().IsRefreshPending())
	{
		scriptErrorf("%s : NET_GAMESERVER_BASKET_START - Session is being REFRESHED - SHOP CLOSED.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return result;
	}

	if ( VERIFY_SHOP_TRANSACTION_CATEGORY_ISVALID( static_cast<NetShopCategory>(category) ASSERT_ONLY(,"NET_GAMESERVER_BASKET_START")) )
	{
		//The BuyCasinoChips we must check for evc only transactions.
		static const u32 NET_SHOP_ACTION_BUY_CASINO_CHIPS = ATSTRINGHASH("NET_SHOP_ACTION_BUY_CASINO_CHIPS", 0xf8720a1a);
		if (action == (int)NET_SHOP_ACTION_BUY_CASINO_CHIPS && NETWORK_LEGAL_RESTRICTIONS.EvcOnlyOnCasinoChipsTransactions())
		{
			flags = CATALOG_ITEM_FLAG_EVC_ONLY | CATALOG_ITEM_FLAG_BANK_THEN_WALLET;
		}

		NetShopTransactionId transid = NET_SHOP_INVALID_TRANS_ID;
		result = NETWORK_SHOPPING_MGR.CreateBasket(transid, static_cast<NetShopCategory>(category), static_cast<NetShopTransactionAction>(action), flags);
		transactionId = (int)transid;
	}

	if (result)
	{
		scriptDebugf1("%s : NET_GAMESERVER_BASKET_START - Basket created with id '%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), transactionId);
	}

	return result;
#else
	return false;
#endif // __NET_SHOP_ACTIVE
}

bool CommandNetGameServerBasketEnd( )
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_BASKET_END");

#if __NET_SHOP_ACTIVE

	if (!VERIFY_GAMESERVER_USE())
		return true;

	bool result = false;

	NetShopTransactionId transid = NET_SHOP_INVALID_TRANS_ID;
	const CNetShopTransactionBase* basket = NETWORK_SHOPPING_MGR.FindBasketConst(transid);

	if (basket)
	{
		result = NETWORK_SHOPPING_MGR.DeleteBasket( );
	}

	return result;
#else
	return false;
#endif // __NET_SHOP_ACTIVE
}

bool CommandNetGameServerBasketIsActive( )
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_BASKET_IS_ACTIVE");

#if __NET_SHOP_ACTIVE

	NetShopTransactionId transid = NET_SHOP_INVALID_TRANS_ID;
	const CNetShopTransactionBase* basket = NETWORK_SHOPPING_MGR.FindBasketConst(transid);

	return (NULL != basket);
#else
	return false;
#endif // __NET_SHOP_ACTIVE
}

bool CommandNetGameServerBasketAddItem( srcBasketItem* NET_SHOP_ONLY(item), const int NET_SHOP_ONLY(quantity) )
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_BASKET_ADD_ITEM");

#if __NET_SHOP_ACTIVE

	if (!VERIFY_GAMESERVER_USE())
		return true;

	bool result = false;

	scriptAssertf(!GameTransactionSessionMgr::Get().IsRefreshPending(), "%s : NET_GAMESERVER_BASKET_ADD_ITEM - Session is being REFRESHED - SHOP CLOSED.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (GameTransactionSessionMgr::Get().IsRefreshPending())
	{
		scriptErrorf("%s : NET_GAMESERVER_BASKET_ADD_ITEM - Session is being REFRESHED - SHOP CLOSED.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return result;
	}

	scriptAssertf(item, "%s : NET_GAMESERVER_BASKET_ADD_ITEM - NULL item.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if( !item )
	{
		scriptErrorf("%s : NET_GAMESERVER_BASKET_ADD_ITEM - NULL item.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return result;
	}

	if( !VERIFY_STATS_LOADED(ASSERT_ONLY("NET_GAMESERVER_BASKET_ADD_ITEM")) )
		return result;

	if ( !VERIFY_ITEM_ISVALID( item->m_catalogKey.Int ASSERT_ONLY(,"NET_GAMESERVER_BASKET_ADD_ITEM")) )
	{
		scriptErrorf("%s : NET_GAMESERVER_BASKET_ADD_ITEM - INVALID m_catalogKey='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), item->m_catalogKey.Int);
		return result;
	}

	if (0 != item->m_extraInventoryKey.Int)
	{
		if ( !VERIFY_ITEM_ISVALID( item->m_extraInventoryKey.Int ASSERT_ONLY(,"NET_GAMESERVER_BASKET_ADD_ITEM")) )
		{
			scriptErrorf("%s : NET_GAMESERVER_BASKET_ADD_ITEM - INVALID m_extraInventoryKey='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), item->m_extraInventoryKey.Int);
			return result;
		}
	}

	NetShopTransactionId transid = NET_SHOP_INVALID_TRANS_ID;
	const CNetShopTransactionBase* basket = NETWORK_SHOPPING_MGR.FindBasketConst(transid);
	
	scriptAssertf(basket, "%s : NET_GAMESERVER_BASKET_ADD_ITEM - Trying to shop item='%d' when the basket does not exist.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), item->m_catalogKey.Int);
	if (!basket)
	{
		scriptErrorf("%s : NET_GAMESERVER_BASKET_ADD_ITEM - Trying to shop item='%d' when the basket does not exist.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), item->m_catalogKey.Int);
		return result;
	}

	scriptAssertf(basket->None(), "%s : NET_GAMESERVER_BASKET_ADD_ITEM - Trying to shop item='%d' when the basket status='%s' is invalid.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), item->m_catalogKey.Int, basket->GetStatusString());
	
	//@NOTE: NET_SHOP_ACTION_UPDATE_BUSINESS_GOODS and NET_SHOP_ACTION_RESET_BUSINESS_PROGRESS allows negative or 0 values in quantities

	scriptAssertf(quantity > 0 || NETWORK_SHOPPING_MGR.ActionAllowsNegativeOrZeroValue(basket->GetAction()), "%s : NET_GAMESERVER_BASKET_ADD_ITEM - Trying to shop item='%d' when the quantity='%d' is invalid.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), item->m_catalogKey.Int, quantity);
		
#if !__NO_OUTPUT
	if(!basket->None()) scriptErrorf("%s : NET_GAMESERVER_BASKET_ADD_ITEM - Trying to shop item='%d' when the basket status='%s' is invalid.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), item->m_catalogKey.Int, basket->GetStatusString());
	if(quantity <= 0 && !NETWORK_SHOPPING_MGR.ActionAllowsNegativeOrZeroValue(basket->GetAction()) ) scriptErrorf("%s : NET_GAMESERVER_BASKET_ADD_ITEM - Trying to shop item='%d' when the quantity='%d' is invalid.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), item->m_catalogKey.Int, quantity);
#endif

	NetShopItemId extraInventoryId = static_cast< NetShopItemId >(item->m_extraInventoryKey.Int);
	if (0 == extraInventoryId)
		extraInventoryId = NET_SHOP_INVALID_ITEM_ID;

	CNetShopItem basketItem(static_cast< NetShopItemId >(item->m_catalogKey.Int)
							,extraInventoryId
							,item->m_price.Int
							,item->m_statvalue.Int
							,quantity);

	result = NETWORK_SHOPPING_MGR.AddItem( basketItem ); 

	scriptDebugf1("%s : NET_GAMESERVER_BASKET_ADD_ITEM - Trying to shop item='%d', statvalue='%d', result='%s'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), item->m_catalogKey.Int, item->m_statvalue.Int, result?"True":"False");

	return result;
#else
	scriptWarningf("%s : NET_GAMESERVER_BASKET_ADD_ITEM - __NET_SHOP_ACTIVE not active.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	return false;
#endif // __NET_SHOP_ACTIVE
}

bool CommandNetGameServerBasketRemoveItem( const int NET_SHOP_ONLY(id) )
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_BASKET_REMOVE_ITEM");

#if __NET_SHOP_ACTIVE
	bool result = false;

	if( !VERIFY_STATS_LOADED(ASSERT_ONLY("NET_GAMESERVER_BASKET_REMOVE_ITEM")) )
		return result;

	if ( VERIFY_ITEM_ISVALID( id ASSERT_ONLY(,"NET_GAMESERVER_BASKET_REMOVE_ITEM")) )
	{
		ASSERT_ONLY( NetShopTransactionId transid = NET_SHOP_INVALID_TRANS_ID; )
		ASSERT_ONLY( const CNetShopTransactionBase* basket = NETWORK_SHOPPING_MGR.FindBasketConst( transid ); )
		scriptAssertf(basket && basket->None(), "%s : NET_GAMESERVER_BASKET_REMOVE_ITEM - Trying to remove item='%0X08' when the basket status='%s' is invalid.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), id, basket ? basket->GetStatusString() : "No Basket");

		result = NETWORK_SHOPPING_MGR.RemoveItem( static_cast< NetShopItemId >( id ) );
	}

	return result;
#else
	return false;
#endif // __NET_SHOP_ACTIVE
}

bool CommandNetGameServerBasketItemExists( const int NET_SHOP_ONLY(id) )
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_BASKET_ITEM_EXISTS");
#if __NET_SHOP_ACTIVE
	bool result = false;

	if( !VERIFY_STATS_LOADED(ASSERT_ONLY("NET_GAMESERVER_BASKET_ITEM_EXISTS")) )
		return result;

	if ( VERIFY_ITEM_ISVALID( id ASSERT_ONLY(,"NET_GAMESERVER_BASKET_ITEM_EXISTS")) )
	{
		result = NETWORK_SHOPPING_MGR.FindItem( static_cast< NetShopItemId >( id ) );
	}

	return result;
#else
	return false;
#endif // __NET_SHOP_ACTIVE
}

bool CommandNetGameServerBasketClearAllItems( )
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_BASKET_CLEAR_ALL_ITEMS");
#if __NET_SHOP_ACTIVE
	bool result = true;

	if( !VERIFY_STATS_LOADED(ASSERT_ONLY("NET_GAMESERVER_BASKET_CLEAR_ALL_ITEMS")) )
		return result;

	scriptDebugf1("%s : NET_GAMESERVER_BASKET_CLEAR_ALL_ITEMS - Commands Called.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	result = NETWORK_SHOPPING_MGR.ClearBasket(  );

	return result;
#else
	return false;
#endif // __NET_SHOP_ACTIVE
}

bool CommandNetGameServerBasketIsEmpty( )
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_BASKET_IS_EMPTY");
#if __NET_SHOP_ACTIVE
	bool result = true;

	if( !VERIFY_STATS_LOADED(ASSERT_ONLY("NET_GAMESERVER_BASKET_IS_EMPTY")) )
		return result;

	result = NETWORK_SHOPPING_MGR.IsBasketEmpty(  );

	return result;
#else
	return false;
#endif // __NET_SHOP_ACTIVE
}

bool CommandNetGameServerBasketIsFull( )
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_BASKET_IS_FULL");
#if __NET_SHOP_ACTIVE
	bool result = true;

	if( !VERIFY_STATS_LOADED(ASSERT_ONLY("NET_GAMESERVER_BASKET_IS_FULL")) )
		return result;

	result = NETWORK_SHOPPING_MGR.IsBasketFull(  );

	return result;
#else
	return true;
#endif // __NET_SHOP_ACTIVE
}

int CommandNetGameServerBasketGetNumberOfItems( )
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_BASKET_GET_NUMBER_OF_ITEMS");
#if __NET_SHOP_ACTIVE
	int result = 0;

	if( !VERIFY_STATS_LOADED(ASSERT_ONLY("NET_GAMESERVER_BASKET_GET_NUMBER_OF_ITEMS")) )
		return result;

	result = NETWORK_SHOPPING_MGR.GetNumberItems(  );

	return result;
#else
	return false;
#endif // __NET_SHOP_ACTIVE
}

bool CommandNetGameServerBasketApplyServerData( const int NET_SHOP_ONLY(transactionId), srcBasketServerDataInfo* NET_SHOP_ONLY(outDataInfo) )
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_BASKET_APPLY_SERVER_DATA");
#if __NET_SHOP_ACTIVE
	bool result = false;

	if( !VERIFY_STATS_LOADED(ASSERT_ONLY("NET_GAMESERVER_BASKET_APPLY_SERVER_DATA")) )
		return result;

	CNetShopTransactionBase* transaction = NETWORK_SHOPPING_MGR.FindTransaction(static_cast<NetShopTransactionId>(transactionId));
	scriptAssertf(transaction, "%s : NET_GAMESERVER_BASKET_APPLY_SERVER_DATA - Trying to apply server data to NULL transaction.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	if (transaction)
	{
		scriptAssertf(transaction->Succeeded(), "%s : NET_GAMESERVER_BASKET_APPLY_SERVER_DATA - Trying to apply server data for transaction that failed", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (transaction->Succeeded())
		{
			CNetShopTransactionServerDataInfo  applyDataInfo;
			result = transaction->ApplyDataToStats(applyDataInfo);

			if(result)
			{
				if(applyDataInfo.m_playerBalanceApplyInfo.m_dataReceived)
				{
					outDataInfo->m_CashUpdateReceived.Int   = applyDataInfo.m_playerBalanceApplyInfo.m_dataReceived ? 1 : 0;
					outDataInfo->m_bankCashDifference.Int   = (s32)applyDataInfo.m_playerBalanceApplyInfo.m_bankCashDifference;
					outDataInfo->m_walletCashDifference.Int = (s32)applyDataInfo.m_playerBalanceApplyInfo.m_walletCashDifference;
				}

				if(applyDataInfo.m_inventoryApplyInfo.m_dataReceived)
				{
					outDataInfo->m_NumValues.Int = applyDataInfo.m_inventoryApplyInfo.m_numItems;
				}
			}
		}
	}

	return result;
#else
	return false;
#endif // __NET_SHOP_ACTIVE
}


///////////////////////////////////////////////////////////////////////
////////////////////////// SHOPPING CHECKOUT //////////////////////////
///////////////////////////////////////////////////////////////////////

bool CommandNetGameServerCheckoutStart( const int NET_SHOP_ONLY(transactionId) )
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_CHECKOUT_START");

#if __NET_SHOP_ACTIVE

	const CGameScriptId& scriptId = static_cast<CGameScriptId&>(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId());
	if (scriptId.GetScriptNameHash() != ATSTRINGHASH("valentineRpReward2", 0xd1f9d9c5)
		&& scriptId.GetScriptNameHash() != ATSTRINGHASH("shop_controller", 0x39da738b)
		&& scriptId.GetScriptNameHash() != ATSTRINGHASH("am_arena_shp", 0x8bb3a063)) // url:bugstar:5469053 adds AM_ARENA_SHP
	{
#if !__FINAL
		if(!PARAM_dontCrashOnScriptValidation.Get())
		{
			gnetFatalAssertf(0, "Shopping 'NET_GAMESERVER_CHECKOUT_START' game event being called from invalid script %s", scriptId.GetScriptName());
		}
#endif // !__FINAL

		return true;
	}

	bool result = false;

	if( !VERIFY_STATS_LOADED(ASSERT_ONLY("NET_GAMESERVER_CHECKOUT_START")) )
		return result;

	result = NETWORK_SHOPPING_MGR.StartCheckout( transactionId );

	return result;
#else
	return false;
#endif // __NET_SHOP_ACTIVE
}

bool CommandNetGameServerCheckoutPending( const int NET_SHOP_ONLY(transactionId) )
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_CHECKOUT_PENDING");
#if __NET_SHOP_ACTIVE
	bool result = false;

	if( !VERIFY_STATS_LOADED(ASSERT_ONLY("NET_GAMESERVER_CHECKOUT_PENDING")) )
		return result;
	
	netStatus::StatusCode code;
	result = NETWORK_SHOPPING_MGR.GetStatus(transactionId, code);

	return (code == netStatus::NET_STATUS_PENDING);
#else
	return false;
#endif // __NET_SHOP_ACTIVE
}

bool CommandNetGameServerCheckoutSuccessful( const int NET_SHOP_ONLY(transactionId) )
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_CHECKOUT_SUCCESSFUL");
#if __NET_SHOP_ACTIVE
	bool result = false;

	if( !VERIFY_STATS_LOADED(ASSERT_ONLY("NET_GAMESERVER_CHECKOUT_SUCCESSFUL")) )
		return result;

	netStatus::StatusCode code;
	result = NETWORK_SHOPPING_MGR.GetStatus(transactionId, code);

	return (code == netStatus::NET_STATUS_SUCCEEDED);
#else
	return false;
#endif // __NET_SHOP_ACTIVE
}

//////////////////////////////////////////////////////////////
////////////////////////// SERVICES //////////////////////////
//////////////////////////////////////////////////////////////

bool CommandNetGameServerBeginService(int& NET_SHOP_ONLY(transactionId)
									,int NET_SHOP_ONLY(category)
									,const int NET_SHOP_ONLY(id)
									,const int NET_SHOP_ONLY(action)
									,const int NET_SHOP_ONLY(cost)
									,const int NET_SHOP_ONLY(flags))
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_BEGIN_SERVICE");

#if __NET_SHOP_ACTIVE
	if (!VERIFY_GAMESERVER_USE())
		return true;

	static const u32 NET_SHOP_TTYPE_SERVICE = ATSTRINGHASH("NET_SHOP_TTYPE_SERVICE", 0xbc537e0d);

	bool result = false;
	
	if( !VERIFY_STATS_LOADED(ASSERT_ONLY("NET_GAMESERVER_BEGIN_SERVICE")) )
		return result;

	transactionId = NET_SHOP_INVALID_TRANS_ID;

	scriptAssertf(!GameTransactionSessionMgr::Get().IsRefreshPending(), "%s : NET_GAMESERVER_BEGIN_SERVICE - Session is being REFRESHED - SHOP CLOSED.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (GameTransactionSessionMgr::Get().IsRefreshPending())
	{
		scriptErrorf("%s : NET_GAMESERVER_BEGIN_SERVICE - Session is being REFRESHED - SHOP CLOSED.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return result;
	}
	SCRIPT_CHECK_CALLING_FUNCTION
	if (VERIFY_ITEM_ISVALID(id ASSERT_ONLY(,"NET_GAMESERVER_BEGIN_SERVICE"))
		&& VERIFY_SHOP_TRANSACTION_CATEGORY_ISVALID(category ASSERT_ONLY(,"NET_GAMESERVER_BEGIN_SERVICE"))
		&& VERIFY_SHOP_ACTION_TYPE_ISVALID(action ASSERT_ONLY(,"NET_GAMESERVER_BEGIN_SERVICE")))
	{
		scriptDebugf1("%s : NET_GAMESERVER_BEGIN_SERVICE - Command called.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptDebugf1("Already Active Services of this type:");
		OUTPUT_ONLY(NETWORK_SHOPPING_MGR.SpewServicesInfo(NET_SHOP_TTYPE_SERVICE, (NetShopItemId)id);)

		NetShopTransactionId tempTransId = NET_SHOP_INVALID_TRANS_ID;
		result = NETWORK_SHOPPING_MGR.BeginService( tempTransId, NET_SHOP_TTYPE_SERVICE, category, id, action, cost, flags );
		transactionId = static_cast< int >(tempTransId);

		if (result)
			scriptDebugf1("NET_GAMESERVER_BEGIN_SERVICE - SUCCESSFULL - with transaction id=%d.", transactionId);
		else
			scriptErrorf("NET_GAMESERVER_BEGIN_SERVICE - FAILED - with id=%d.", id);
	}

	return result;
#else
	return false;
#endif // __NET_SHOP_ACTIVE
}

bool CommandNetGameServerEndService( int NET_SHOP_ONLY(transactionId) )
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_END_SERVICE");
#if __NET_SHOP_ACTIVE

	if (!VERIFY_GAMESERVER_USE())
		return true;

	bool result = false;

	if( !VERIFY_STATS_LOADED(ASSERT_ONLY("NET_GAMESERVER_END_SERVICE")) )
		return result;

	CNetShopTransactionBase* service = NETWORK_SHOPPING_MGR.FindTransaction( static_cast<NetShopTransactionId>(transactionId) );
	scriptAssertf(service, "%s: NET_GAMESER_VEREND_SERVICE - Failed to start find service with id=\"%d\"", CTheScripts::GetCurrentScriptNameAndProgramCounter(), transactionId);

	if (service)
	{
		scriptAssertf(!service->Pending(), "%s: NET_GAMESERVER_END_SERVICE - Service is PENDING id=\"%d\"", CTheScripts::GetCurrentScriptNameAndProgramCounter(), transactionId);
		if (!service->Pending())
		{
			return NETWORK_SHOPPING_MGR.EndService( static_cast< NetShopTransactionId >(transactionId) );
		}
	}

	return result;
#else
	return false;
#endif // __NET_SHOP_ACTIVE
}

/////////////////////////////////////////////////////////////////
////////////////////////// DELETE SLOT //////////////////////////
/////////////////////////////////////////////////////////////////

#if __NET_SHOP_ACTIVE
static NetworkGameTransactions::TelemetryNonce s_deleteTelemetryNonce;
#endif // __NET_SHOP_ACTIVE

bool  CommandNetGameServerDeleteSlot(const int NET_SHOP_ONLY(slot), bool NET_SHOP_ONLY(clearbank), int NET_SHOP_ONLY(reason))
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_DELETE_CHARACTER");

#if __NET_SHOP_ACTIVE

	netStatus& deleteSlotStatus = GameTransactionSessionMgr::Get().GetDeleteStatus();

#if !__FINAL
	sStandaloneGameTransactionStatus debugDraw("DELETE", &deleteSlotStatus);
	CNetworkShoppingMgr::AddToGrcDebugDraw( debugDraw );
#endif // !__FINAL

	bool result = false;

	if (scriptVerifyf(!deleteSlotStatus.Pending(), "%s: NET_GAMESERVER_DELETE_CHARACTER - an operation to delete a slot is pending.", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		if (scriptVerifyf(!GameTransactionSessionMgr::Get().IsQueueBusy(), "%s: NET_GAMESERVER_DELETE_CHARACTER - Queue is busy.", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			if (NETWORK_SHOPPING_MGR.ShouldDoNullTransaction())
			{
				scriptDebugf1("Starting NET_GAMESERVER_DELETE_CHARACTER as NULL transaction");
				deleteSlotStatus.Reset();
				NetworkGameTransactions::SendNULLEvent(NetworkInterface::GetLocalGamerIndex(), &deleteSlotStatus);
				return true;
			}

			s_deleteTelemetryNonce.Clear();
			deleteSlotStatus.Reset();
			result = NetworkGameTransactions::DeleteSlot(NetworkInterface::GetLocalGamerIndex(), slot, clearbank, true, reason, &s_deleteTelemetryNonce, &deleteSlotStatus);
			scriptAssertf(result, "%s: NET_GAMESERVER_DELETE_CHARACTER - Failed to start NetworkGameTransactions::DeleteSlot", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			scriptDebugf1("%s: NET_GAMESERVER_DELETE_CHARACTER - start DeleteSlot='%d', result='%s'", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot, result ? "True" : "False");
		}
	}

	return result;
#else
	return false;
#endif // __NET_SHOP_ACTIVE
}

int CommandNetGameServerDeleteSlotGetStatus( )
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_DELETE_CHARACTER_GET_STATUS");

#if __NET_SHOP_ACTIVE
	netStatus& deleteSlotStatus = GameTransactionSessionMgr::Get().GetDeleteStatus();

	if (deleteSlotStatus.Pending())
		return TRANSACTION_STATUS_PENDING;

	if (deleteSlotStatus.Failed())
		return TRANSACTION_STATUS_FAILED;

	if (deleteSlotStatus.Succeeded())
		return TRANSACTION_STATUS_SUCCESSFULL;

	if (deleteSlotStatus.Canceled())
		return TRANSACTION_STATUS_CANCELED;

	return TRANSACTION_STATUS_NONE;
#else
	return false;
#endif // __NET_SHOP_ACTIVE
}

bool CommandNetGameServerDeleteSlotSetTelemetryNonceSeed( ) 
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_DELETE_SET_TELEMETRY_NONCE_SEED");

#if __NET_SHOP_ACTIVE
	bool result = false;

	if( !VERIFY_STATS_LOADED(ASSERT_ONLY("NET_GAMESERVER_DELETE_SET_TELEMETRY_NONCE_SEED")) )
		return result;

	netStatus& deleteSlotStatus = GameTransactionSessionMgr::Get().GetDeleteStatus();

	scriptAssertf(deleteSlotStatus.Succeeded(), "%s: NET_GAMESERVER_DELETE_SET_TELEMETRY_NONCE_SEED - Delete character was not successfull.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (deleteSlotStatus.Succeeded())
	{
		scriptAssertf(s_deleteTelemetryNonce.GetNonce() > 0, "%s: NET_GAMESERVER_DELETE_SET_TELEMETRY_NONCE_SEED - nonce is invalid.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (s_deleteTelemetryNonce.GetNonce())
		{
			result = true;
			GameTransactionSessionMgr::Get().SetNonceForTelemetry(s_deleteTelemetryNonce.GetNonce());
			s_deleteTelemetryNonce.Clear();
		}
	}

	return result;
#else
	return false;
#endif // __NET_SHOP_ACTIVE
}

////////////////////////////////////////////////////////////////////
////////////////////////// BANK TRANSFERS //////////////////////////
////////////////////////////////////////////////////////////////////

#if __NET_SHOP_ACTIVE
static NetworkGameTransactions::PlayerBalance  s_returnPlayerBalance;
static NetworkGameTransactions::TelemetryNonce s_cashTransferTelemetryNonce;
#endif // __NET_SHOP_ACTIVE

bool  CommandNetGameServerTransferBankToWallet(const int NET_SHOP_ONLY(slot), const int NET_SHOP_ONLY(amount))
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_TRANSFER_BANK_TO_WALLET");

#if __NET_SHOP_ACTIVE

	netStatus& cashtransferStatus = GameTransactionSessionMgr::Get().GetCashTransferStatus();

#if !__FINAL
	sStandaloneGameTransactionStatus debugDraw("ALLOT", &cashtransferStatus);
	CNetworkShoppingMgr::AddToGrcDebugDraw( debugDraw );
#endif // !__FINAL

	bool result = false;

	if (scriptVerifyf(!cashtransferStatus.Pending(), "%s: NET_GAMESERVER_TRANSFER_BANK_TO_WALLET - an operation to transfer cash is pending.", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		if (scriptVerifyf(!GameTransactionSessionMgr::Get().IsQueueBusy(), "%s: NET_GAMESERVER_TRANSFER_BANK_TO_WALLET - Queue is Busy.", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			if (scriptVerifyf(GameTransactionSessionMgr::Get().IsReady(), "%s: NET_GAMESERVER_TRANSFER_BANK_TO_WALLET - State is not ready.", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			{
				cashtransferStatus.Reset();

				if (NETWORK_SHOPPING_MGR.ShouldDoNullTransaction())
				{
					gnetDebug1("Starting NET_GAMESERVER_TRANSFER_BANK_TO_WALLET as NULL transaction");
					cashtransferStatus.Reset();
					NetworkGameTransactions::SendNULLEvent(NetworkInterface::GetLocalGamerIndex(), &cashtransferStatus);
					return true;
				}

				s_cashTransferTelemetryNonce.Clear();

				result = NetworkGameTransactions::Allot(NetworkInterface::GetLocalGamerIndex(), slot, amount, &s_returnPlayerBalance, &s_cashTransferTelemetryNonce, &cashtransferStatus);
				scriptAssertf(result, "%s: NET_GAMESERVER_TRANSFER_BANK_TO_WALLET - Failed to start NetworkGameTransactions::Allot", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			}
		}
	}

	return result;
#else
	return false;
#endif // __NET_SHOP_ACTIVE

}

bool  CommandNetGameServerTransferWalletToBank(const int NET_SHOP_ONLY(slot), const int NET_SHOP_ONLY(amount))
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_TRANSFER_WALLET_TO_BANK");

#if __NET_SHOP_ACTIVE

	netStatus& cashtransferStatus = GameTransactionSessionMgr::Get().GetCashTransferStatus();

#if !__FINAL
	sStandaloneGameTransactionStatus debugDraw("RECOUP", &cashtransferStatus);
	CNetworkShoppingMgr::AddToGrcDebugDraw( debugDraw );
#endif // !__FINAL

	bool result = false;

	if (scriptVerifyf(!cashtransferStatus.Pending(), "%s: NET_GAMESERVER_TRANSFER_WALLET_TO_BANK - an operation to transfer cash is pending.", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		if (scriptVerifyf(!GameTransactionSessionMgr::Get().IsQueueBusy(), "%s: NET_GAMESERVER_TRANSFER_WALLET_TO_BANK - Queue is busy.", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			if (scriptVerifyf(GameTransactionSessionMgr::Get().IsReady(), "%s: NET_GAMESERVER_TRANSFER_WALLET_TO_BANK - State is not ready.", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			{
				cashtransferStatus.Reset();

				if (NETWORK_SHOPPING_MGR.ShouldDoNullTransaction())
				{
					gnetDebug1("Starting NET_GAMESERVER_TRANSFER_WALLET_TO_BANK as NULL transaction");
					cashtransferStatus.Reset();
					NetworkGameTransactions::SendNULLEvent(NetworkInterface::GetLocalGamerIndex(), &cashtransferStatus);
					return true;
				}

				s_cashTransferTelemetryNonce.Clear();

				result = NetworkGameTransactions::Recoup(NetworkInterface::GetLocalGamerIndex(), slot, amount, &s_returnPlayerBalance, &s_cashTransferTelemetryNonce, &cashtransferStatus);
				scriptAssertf(result, "%s: NET_GAMESERVER_TRANSFER_WALLET_TO_BANK - Failed to start NetworkGameTransactions::Recoup", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			}
		}
	}

	return result;
#else
	return false;
#endif // __NET_SHOP_ACTIVE
}

int CommandNetGameServerTransferGetStatus( )
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_TRANSFER_BANK_TO_WALLET_GET_STATUS");

#if __NET_SHOP_ACTIVE

	netStatus& cashtransferStatus = GameTransactionSessionMgr::Get().GetCashTransferStatus();

	if (cashtransferStatus.Pending())
		return TRANSACTION_STATUS_PENDING;

	if (cashtransferStatus.Failed())
		return TRANSACTION_STATUS_FAILED;

	if (cashtransferStatus.Succeeded())
		return TRANSACTION_STATUS_SUCCESSFULL;

	if (cashtransferStatus.Canceled())
		return TRANSACTION_STATUS_CANCELED;

	return TRANSACTION_STATUS_NONE;
#else
	return 0;
#endif // __NET_SHOP_ACTIVE
}

bool CommandNetGameServerTransferSetTelemetryNonceSeed( ) 
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_CASH_TRANSFER_SET_TELEMETRY_NONCE_SEED");

#if __NET_SHOP_ACTIVE
	bool result = false;

	if( !VERIFY_STATS_LOADED(ASSERT_ONLY("NET_GAMESERVER_CASH_TRANSFER_SET_TELEMETRY_NONCE_SEED")) )
		return result;

	netStatus& cashtransferStatus = GameTransactionSessionMgr::Get().GetCashTransferStatus();

	scriptAssertf(cashtransferStatus.Succeeded(), "%s: NET_GAMESERVER_CASH_TRANSFER_SET_TELEMETRY_NONCE_SEED - Cash transfer was not successfull.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (cashtransferStatus.Succeeded())
	{
		scriptAssertf(s_cashTransferTelemetryNonce.GetNonce() > 0, "%s: NET_GAMESERVER_CASH_TRANSFER_SET_TELEMETRY_NONCE_SEED - nonce is invalid.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (s_cashTransferTelemetryNonce.GetNonce())
		{
			result = true;
			GameTransactionSessionMgr::Get().SetNonceForTelemetry(s_cashTransferTelemetryNonce.GetNonce());
			s_deleteTelemetryNonce.Clear();
		}
	}

	return result;
#else
	return false;
#endif // __NET_SHOP_ACTIVE
}

bool CommandNetGameServerUseServerTransactions()
{
#if __NET_SHOP_ACTIVE
	return !NETWORK_SHOPPING_MGR.ShouldDoNullTransaction();
#else
	return false;
#endif
}

bool CommandNetGameServerSetTelemetryNonceSeed( const int NET_SHOP_ONLY(transactionId) ) 
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_SET_TELEMETRY_NONCE_SEED");

#if __NET_SHOP_ACTIVE
	bool result = false;

	if( !VERIFY_STATS_LOADED(ASSERT_ONLY("NET_GAMESERVER_SET_TELEMETRY_NONCE_SEED")) )
		return result;

	CNetShopTransactionBase* transaction = NETWORK_SHOPPING_MGR.FindTransaction(static_cast<NetShopTransactionId>(transactionId));
	scriptAssertf(transaction, "%s : NET_GAMESERVER_SET_TELEMETRY_NONCE_SEED - Transaction id='%d' does not exist.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), transactionId);

	if (transaction)
	{
		scriptAssertf(transaction->Succeeded(), "%s : NET_GAMESERVER_SET_TELEMETRY_NONCE_SEED - Transaction id='%d' did not Succeeded.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), transactionId);
		if (transaction->Succeeded())
		{
			result = true;

			scriptDebugf1("%s : NET_GAMESERVER_SET_TELEMETRY_NONCE_SEED - Transaction id='%d' - Nonce='%" I64FMT "d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), transactionId, transaction->GetTelemetryNonce());
			GameTransactionSessionMgr::Get().SetNonceForTelemetry(transaction->GetTelemetryNonce());
		}
	}

	return result;
#else
	return false;
#endif // __NET_SHOP_ACTIVE
}


////////////////////////////////////////////////////////////////////
////////////////////////// HEARTBEAT //////////////////////////
////////////////////////////////////////////////////////////////////

bool CommandNetGameServerAddItemtoHeartBeat( int NET_SHOP_ONLY(itemId), int NET_SHOP_ONLY(value), int NET_SHOP_ONLY(price) )
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_ADD_CATALOG_ITEM_TO_HEARTBEAT");

#if __NET_SHOP_ACTIVE

	(void)value;
	(void)price;

	bool result = false;

	if( !VERIFY_STATS_LOADED(ASSERT_ONLY("NET_GAMESERVER_ADD_CATALOG_ITEM_TO_HEARTBEAT")) )
		return result;

	if ( !VERIFY_ITEM_ISVALID( itemId ASSERT_ONLY(,"NET_GAMESERVER_ADD_CATALOG_ITEM_TO_HEARTBEAT")) )
	{
		scriptErrorf("%s : NET_GAMESERVER_ADD_CATALOG_ITEM_TO_HEARTBEAT - INVALID m_catalogKey='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), itemId);
		return result;
	}

	const netCatalog& catalogue = netCatalog::GetInstance();

	atHashString key((u32)itemId);
	const netCatalogBaseItem* item = catalogue.Find(key);
	if (item)
	{
		//scriptAssertf(price == (value*item->GetPrice()), "%s : NET_GAMESERVER_ADD_CATALOG_ITEM_TO_HEARTBEAT - INVALID price='%d', for value='%d' at price='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), itemId, price, item->GetPrice());

		result = true;
		//GameTransactionSessionMgr::Get().GetHeartBeatHelper().AddItem(itemId, value, value*item->GetPrice());

		scriptDebugf1("%s : NET_GAMESERVER_ADD_CATALOG_ITEM_TO_HEARTBEAT - Trying to add item='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), itemId);
	}

	return result;
#else
	scriptWarningf("%s : NET_GAMESERVER_ADD_CATALOG_ITEM_TO_HEARTBEAT - __NET_SHOP_ACTIVE not active.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	return false;
#endif // __NET_SHOP_ACTIVE
}

int CommandNetGetCatalogVersion()
{
	ASSERT_ON_INVALID_COMMAND("NET_GAMESERVER_GET_CATALOG_VERSION");

#if __BANK && __NET_SHOP_ACTIVE
	return static_cast<int>(netCatalog::GetInstance().GetVersion());
#else // __BANK && __NET_SHOP_ACTIVE
	return 0;
#endif
}
	

//
// name:		SetupScriptCommands
// description:	Setup streaming script commands
void SetupScriptCommands()
{
	SCR_REGISTER_SECURE( NET_GAMESERVER_USE_SERVER_TRANSACTIONS,0xa50ced7fb6e38751, CommandNetGameServerUseServerTransactions);

	/////////////////////////////////////////////////////////////
	////////////////////////// CATALOG //////////////////////////
	/////////////////////////////////////////////////////////////
	SCR_REGISTER_UNUSED( NET_GAMESERVER_REMOVE_CATALOG_ITEM,0x4981147c2112fc49,  CommandNetGameServerRemoveCatalogItem);
	SCR_REGISTER_UNUSED( NET_GAMESERVER_ADD_ITEM_TO_CATALOG,0xeae106f5d3d0bc4c,  CommandNetGameServerAddItemToCatalog                );
	SCR_REGISTER_UNUSED( NET_GAMESERVER_SET_CATALOG_ITEM_PRICE,0xc09c98b726ee9579,  CommandNetGameServerChangeItemPrice                 );
	SCR_REGISTER_UNUSED( NET_GAMESERVER_LINK_CATALOG_ITEMS,0x72fe3b99a6581589,  CommandNetGameServerLinkCatalogItems                );
	SCR_REGISTER_UNUSED( NET_GAMESERVER_LINK_CATALOG_ITEMS_FOR_SELLING,0xcfa402b56b643034,  CommandNetGameServerLinkCatalogItemsForSelling      );
	SCR_REGISTER_UNUSED( NET_GAMESERVER_ARE_CATALOG_ITEMS_LINKED,0x1e62f4e93e204976,  CommandNetGameServerAreCatalogItemsLinked           );
	SCR_REGISTER_UNUSED( NET_GAMESERVER_REMOVE_LINKED_CATALOG_ITEMS,0x927fc8dbe80ddc06,  CommandNetGameServerRemoveLink                      );
	SCR_REGISTER_UNUSED( NET_GAMESERVER_ARE_CATALOG_ITEMS_LINKED_FOR_SELLING,0xac492242e9d75a50,  CommandNetGameServerAreCatalogItemsLinkedForSelling );
	SCR_REGISTER_UNUSED( NET_GAMESERVER_REMOVE_LINK_CATALOG_ITEMS_FOR_SELLING,0x6b8f793f06355be7,  CommandNetGameServerRemoveLinkForSelling            );
	SCR_REGISTER_SECURE( NET_GAMESERVER_CATALOG_ITEM_IS_VALID,0x5b1b2a8fe51fdb2d,  CommandNetGameServerCheckItemIsValid                );
	SCR_REGISTER_SECURE( NET_GAMESERVER_CATALOG_ITEM_KEY_IS_VALID,0x30057de4703c0da0,  CommandNetGameServerCheckItemKeyIsValid             );
	SCR_REGISTER_UNUSED( NET_GAMESERVER_GET_CATALOG_VERSION,0x674cb733ad247516,  CommandNetGetCatalogVersion                         );
	SCR_REGISTER_UNUSED( NETWORK_SHOP_SET_CATALOG_ITEM_TEXT_LABEL,0x727c37960f5938ad,  CommandShopChangeItemTextLabel                      );

	/////////////////////////////////////////////////////////////////////
	////////////////////////// CATALOG ITEMS //////////////////////////
	/////////////////////////////////////////////////////////////////////
	SCR_REGISTER_SECURE( NET_GAMESERVER_GET_PRICE,0xbc8c74780a771297,  CommandNetGameServerGetPrice                     );
	SCR_REGISTER_UNUSED( NET_GAMESERVER_GET_CATEGORY,0x1ea222bdb50cd0dc,  CommandNetGameServerGetItemCategory              );
	SCR_REGISTER_SECURE( NET_GAMESERVER_CATALOG_IS_VALID,0xa10787b951345252,  CommandNetGameServerIsCatalogValid               );
	SCR_REGISTER_SECURE( NET_GAMESERVER_IS_CATALOG_CURRENT,0xd6056e80737899a4,  CommandNetGameServerIsCatalogCurrent             );
	SCR_REGISTER_SECURE( NET_GAMESERVER_GET_CATALOG_CLOUD_CRC,0x5c1d76a1af94a070,  CommandNetGameServerCatalogCloudCRC              );
	SCR_REGISTER_SECURE( NET_GAMESERVER_REFRESH_SERVER_CATALOG,0xbf1625e3bc032624,  CommandNetGameServerRefreshServerCatalog         );
	SCR_REGISTER_SECURE( NET_GAMESERVER_RETRIEVE_CATALOG_REFRESH_STATUS,0x4f62f2c5dfe165d1,  CommandNetGameServerRetrieveCatalogRefreshStatus );

	/////////////////////////////////////////////////////////////////////
	////////////////////////// SHOP SESSION /////////////////////////////
	/////////////////////////////////////////////////////////////////////

	SCR_REGISTER_SECURE( NET_GAMESERVER_INIT_SESSION,0x7f3fea7d9cf0dc6e, CommandNetGameServerInitServer                         );
	SCR_REGISTER_SECURE( NET_GAMESERVER_RETRIEVE_INIT_SESSION_STATUS,0x235f4dc72bbcc553, CommandNetGameServerRetrieveSessionInitStatus          );
	SCR_REGISTER_SECURE( NET_GAMESERVER_START_SESSION,0xd18d5a2e89ffbbdd, CommandNetGameServerStartSession                       );
	SCR_REGISTER_SECURE( NET_GAMESERVER_START_SESSION_PENDING,0xfcac2ba43eebfd67, CommandNetGameServerStartSessionPending                );
	SCR_REGISTER_SECURE( NET_GAMESERVER_RETRIEVE_START_SESSION_STATUS,0xae7d86f9ae8e7023, CommandNetGameServerRetrieveSessionStartStatus         );
	SCR_REGISTER_SECURE( NET_GAMESERVER_RETRIEVE_SESSION_ERROR_CODE,0xf5cd50011aebed08, CommandNetGameServerRetrieveGameServerSessionErrorCode );
	SCR_REGISTER_SECURE( NET_GAMESERVER_IS_SESSION_VALID,0xd487b8e9c523defd, CommandNetGameServerIsSessionValid                     );
	SCR_REGISTER_SECURE( NET_GAMESERVER_CLEAR_SESSION,0xd96b781b65d4d071, CommandNetGameServerClearSession                       );
	SCR_REGISTER_SECURE( NET_GAMESERVER_SESSION_APPLY_RECEIVED_DATA,0xcbcd6607d498169c, CommandNetGameServerSessionApplyReceivedData           );
	SCR_REGISTER_UNUSED( NET_GAMESERVER_START_SESSION_APPLY_DATA_PENDING,0xc87fbaa0b0e40d50, CommandNetGameServerStartSessionApplyDataPending       );
	SCR_REGISTER_SECURE( NET_GAMESERVER_IS_SESSION_REFRESH_PENDING,0xba5b5030b7954daa, CommandNetGameServerIsSessionRefreshPending            );
	SCR_REGISTER_SECURE( NET_GAMESERVER_START_SESSION_RESTART,0xd05c5303ee30788b, CommandNetGameServerSessionRestart                     );
	SCR_REGISTER_UNUSED( NET_GAMESERVER_START_SESSION_RESTART_STATUS,0x6ed91b81a6269d6f, CommandNetGameServerSessionRestartStatus               );
	SCR_REGISTER_UNUSED( NET_GAMESERVER_START_SESSION_RESTART_ERROR_CODE,0xb7b8a0edefdb1cdf, CommandNetGameServerSessionRestartErrorCode            );
	SCR_REGISTER_SECURE( NET_GAMESERVER_TRANSACTION_IN_PROGRESS,0xb76e8f198fb54340, CommandNetGameServerTransactionInProgress              );
	SCR_REGISTER_SECURE( NET_GAMESERVER_GET_SESSION_STATE_AND_STATUS,0x5d66da471cacd32b, CommandNetGameServerGetStateAndRefreshStatus           );
	SCR_REGISTER_UNUSED( NET_GAMESERVER_GET_SLOT_IS_VALID,0x4c7642b2101d7980, CommandNetGameServerGetSlotIsValid                     );

	/////////////////////////////////////////////////////////////////////
	////////////////////////// SHOPPING BASKET //////////////////////////
	/////////////////////////////////////////////////////////////////////

	SCR_REGISTER_SECURE( NET_GAMESERVER_BASKET_START,0x76503dcd0bb2d833, CommandNetGameServerBasketStart            );
	SCR_REGISTER_SECURE( NET_GAMESERVER_BASKET_END,0x349e25ea131c0e2a, CommandNetGameServerBasketEnd              );
	SCR_REGISTER_SECURE( NET_GAMESERVER_BASKET_IS_ACTIVE,0x3f5b892c35f3ff91, CommandNetGameServerBasketIsActive         );
	SCR_REGISTER_SECURE( NET_GAMESERVER_BASKET_ADD_ITEM,0x598e220bd27f56ca, CommandNetGameServerBasketAddItem          );
	SCR_REGISTER_UNUSED( NET_GAMESERVER_BASKET_REMOVE_ITEM,0x1fefd84932535f01, CommandNetGameServerBasketRemoveItem       );
	SCR_REGISTER_UNUSED( NET_GAMESERVER_BASKET_ITEM_EXISTS,0xccbf14d35011465d, CommandNetGameServerBasketItemExists       );
	SCR_REGISTER_UNUSED( NET_GAMESERVER_BASKET_CLEAR_ALL_ITEMS,0x0c49ee0c4d33a89e, CommandNetGameServerBasketClearAllItems    );
	SCR_REGISTER_UNUSED( NET_GAMESERVER_BASKET_IS_EMPTY,0xdb42c3058f7a5848, CommandNetGameServerBasketIsEmpty          );
	SCR_REGISTER_SECURE( NET_GAMESERVER_BASKET_IS_FULL,0x03a58b305ffa997e, CommandNetGameServerBasketIsFull           );
	SCR_REGISTER_UNUSED( NET_GAMESERVER_BASKET_GET_NUMBER_OF_ITEMS,0x887808f47bc32256, CommandNetGameServerBasketGetNumberOfItems );
	SCR_REGISTER_SECURE( NET_GAMESERVER_BASKET_APPLY_SERVER_DATA,0xa51a2c9006cd8004, CommandNetGameServerBasketApplyServerData  );


	///////////////////////////////////////////////////////////////////////
	////////////////////////// SHOPPING CHECKOUT //////////////////////////
	///////////////////////////////////////////////////////////////////////

	SCR_REGISTER_SECURE( NET_GAMESERVER_CHECKOUT_START,0xe894c3f21e583743, CommandNetGameServerCheckoutStart      );
	SCR_REGISTER_UNUSED( NET_GAMESERVER_CHECKOUT_PENDING,0x46db1d48f2d7c08e, CommandNetGameServerCheckoutPending    );
	SCR_REGISTER_UNUSED( NET_GAMESERVER_CHECKOUT_SUCCESSFUL,0x68d94c40347d7aec, CommandNetGameServerCheckoutSuccessful );


	//////////////////////////////////////////////////////////////
	////////////////////////// SERVICES //////////////////////////
	//////////////////////////////////////////////////////////////

	SCR_REGISTER_SECURE( NET_GAMESERVER_BEGIN_SERVICE,0x651232f0fbbd6c7f, CommandNetGameServerBeginService );
	SCR_REGISTER_SECURE( NET_GAMESERVER_END_SERVICE,0x5ace3de15ef4a616, CommandNetGameServerEndService   );


	/////////////////////////////////////////////////////////////////
	////////////////////////// DELETE SLOT //////////////////////////
	/////////////////////////////////////////////////////////////////

	SCR_REGISTER_SECURE( NET_GAMESERVER_DELETE_CHARACTER,0x104c4d01e9978f2b, CommandNetGameServerDeleteSlot                      );
	SCR_REGISTER_SECURE( NET_GAMESERVER_DELETE_CHARACTER_GET_STATUS,0x7b4873b0578ffe2d, CommandNetGameServerDeleteSlotGetStatus             );
	SCR_REGISTER_SECURE( NET_GAMESERVER_DELETE_SET_TELEMETRY_NONCE_SEED,0x13bf5d027972316d, CommandNetGameServerDeleteSlotSetTelemetryNonceSeed );


	////////////////////////////////////////////////////////////////////
	////////////////////////// BANK TRANSFERS //////////////////////////
	////////////////////////////////////////////////////////////////////
	SCR_REGISTER_SECURE( NET_GAMESERVER_TRANSFER_BANK_TO_WALLET,0xb20c7345f1489bf1, CommandNetGameServerTransferBankToWallet          );
	SCR_REGISTER_SECURE( NET_GAMESERVER_TRANSFER_WALLET_TO_BANK,0xca60478c0d089d74, CommandNetGameServerTransferWalletToBank          );
	SCR_REGISTER_SECURE( NET_GAMESERVER_TRANSFER_BANK_TO_WALLET_GET_STATUS,0xc94b30f65c69ada8, CommandNetGameServerTransferGetStatus             );
	SCR_REGISTER_SECURE( NET_GAMESERVER_TRANSFER_WALLET_TO_BANK_GET_STATUS,0xff1451ceaa01267c, CommandNetGameServerTransferGetStatus             );
	SCR_REGISTER_SECURE( NET_GAMESERVER_TRANSFER_CASH_SET_TELEMETRY_NONCE_SEED,0x44c0c3ec2e3e2a3c, CommandNetGameServerTransferSetTelemetryNonceSeed );

	////////////////////////////////////////////////////////////////////
	////////////////////////// TELEMETRY //////////////////////////
	////////////////////////////////////////////////////////////////////
	SCR_REGISTER_SECURE( NET_GAMESERVER_SET_TELEMETRY_NONCE_SEED,0xcde1c8cef9603c08, CommandNetGameServerSetTelemetryNonceSeed );


	////////////////////////////////////////////////////////////////////
	////////////////////////// HEARTBEAT //////////////////////////
	////////////////////////////////////////////////////////////////////
	SCR_REGISTER_UNUSED( NET_GAMESERVER_ADD_CATALOG_ITEM_TO_HEARTBEAT,0x07c2993b9e3fa833, CommandNetGameServerAddItemtoHeartBeat );

}

};

//eof

