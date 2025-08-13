//
// filename:	commands_money.cpp
// description:	
//

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "script/wrapper.h"
#include "diag/channel.h"
#include "data/bitbuffer.h"
#include "rline/rlgamerinfo.h"
#include "rline/rltelemetry.h"
#include "rline/rl.h"
#include "rline/ros/rlros.h"
#include "rline/ros/rlroscommon.h"
#include "rline/socialclub/rlsocialclubcommon.h"
#include "atl/hashstring.h"

// Game headers
#include "Network/Commerce/CommerceManager.h"
#include "Network/NetworkInterface.h"
#include "Network/Live/NetworkTransactionTelemetry.h"
#include "Network/Shop/NetworkGameTransactions.h"
#include "Network/Shop/GameTransactionsSessionMgr.h"
#include "Network/Shop/NetworkShopping.h"
#include "script/script.h"
#include "script/script_channel.h"
#include "script/script_helper.h"
#include "modelinfo/BaseModelInfo.h"
#include "modelinfo/ModelInfo.h"
#include "weapons/info/ItemInfo.h"
#include "weapons/info/WeaponInfoManager.h"
#include "event/EventGroup.h"
#include "event/EventNetwork.h"
#include "Stats/StatsInterface.h"
#include "Stats/MoneyInterface.h"
#include "frontend/FrontendStatsMgr.h"
#include "Stats/stats_channel.h"
#include "network/Network.h"
#include "network/Live/NetworkLegalRestrictionsManager.h"

#if RSG_GEN9
#include <inttypes.h>
#elif !defined(PRIu64)
#pragma message( "Current Windows SDK doesn't support inttypes.h, defining PRIu64" )
#define PRIu64 "llu"
#endif

NETWORK_OPTIMISATIONS()

XPARAM(sc_UseBasketSystem);


//Shitty faux protection to handle unity vs. non-unity 
#ifndef __SCR_TEXT_LABEL_DEFINED
#define __SCR_TEXT_LABEL_DEFINED
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrTextLabel63);
#endif

//PURPOSE
//  specific data for ambient jobs.
class srcAmbientJobdata
{
public:
	scrValue m_v1;
	scrValue m_v2;
	scrValue m_v3;
	scrValue m_v4;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(srcAmbientJobdata);

class srcSpendOfficeAndWarehouse
{
public:
    scrValue m_Location;
    scrValue m_LocationAmount;
    scrValue m_Style;
    scrValue m_StyleAmount;
    scrValue m_PaGender;
    scrValue m_PaGenderAmount;
    scrValue m_Signage;
    scrValue m_SignageAmount;
    scrValue m_GunLocker;
    scrValue m_GunLockerAmount;
    scrValue m_Vault;
    scrValue m_VaultAmount;
    scrValue m_PersonalQuarters;
    scrValue m_PersonalQuartersAmount;
    scrValue m_WarehouseSize;
    scrValue m_WarehouseSizeAmount;
    scrValue m_SmallWarehouses;
    scrValue m_MediumWarehouses;
	scrValue m_LargeWarehouses;
	scrValue m_ModShop;
	scrValue m_ModShopAmount;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(srcSpendOfficeAndWarehouse);

class srcSpendOfficeGarage
{
public:
	scrValue m_Location;
	scrValue m_LocationAmount;
	scrValue m_Numbering;
	scrValue m_NumberingAmount;
	scrValue m_NumberingStyle;
	scrValue m_NumberingStyleAmount;
	scrValue m_Lighting;
	scrValue m_LightingAmount;
	scrValue m_Wall;
	scrValue m_WallAmount;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(srcSpendOfficeGarage);

//PURPOSE
//  specific data for buying a club house.
class srcClubHouseData
{
public: 
	scrValue m_property_id;        
	scrValue m_property_id_amount;      
	scrValue m_mural_type;         
	scrValue m_mural_type_amount;
	scrValue m_wall_style;         
	scrValue m_wall_style_amount;         
	scrValue m_wall_hanging_style; 
	scrValue m_wall_hanging_style_amount; 
	scrValue m_furniture_style;    
	scrValue m_furniture_style_amount;    
	scrValue m_emblem;             
	scrValue m_emblem_amount;             
	scrValue m_gun_locker;         
	scrValue m_gun_locker_amount;         
	scrValue m_mod_shop;           
	scrValue m_mod_shop_amount;              
	scrValue m_signage;            
	scrValue m_signage_amount;        
	scrValue m_font;            
	scrValue m_font_amount;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(srcClubHouseData);

//PURPOSE
//  specific data for buying Business Property.
class srcBusinessPropertyData
{
public:
	scrValue m_businessId;
	scrValue m_businessType;
	scrValue m_upgradeType;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(srcBusinessPropertyData);

class srcSpentOnBunker
{
public:
	scrValue m_location;
	scrValue m_location_amount;
	scrValue m_style;
	scrValue m_style_amount;
	scrValue m_personalquarter;
	scrValue m_personalquarter_amount;
	scrValue m_firingrange;
	scrValue m_firingrange_amount;
	scrValue m_gunlocker;
	scrValue m_gunlocker_amount;
	scrValue m_caddy;
	scrValue m_caddy_amount;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(srcSpentOnBunker);

class srcSpentOnTruck
{
public:
	scrValue m_vehicle;
	scrValue m_vehicle_amount;
	scrValue m_trailer;
	scrValue m_trailer_amount;
	scrValue m_slot1;
	scrValue m_slot1_amount;
	scrValue m_slot2;
	scrValue m_slot2_amount;
	scrValue m_slot3;
	scrValue m_slot3_amount;
	scrValue m_colorscheme;
	scrValue m_colorscheme_amount;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(srcSpentOnTruck);

class srcSpendHangar
{
public:
	scrValue m_location;
	scrValue m_location_amount;
	scrValue m_flooring;
	scrValue m_flooring_amount;
	scrValue m_furnitures;
	scrValue m_furnitures_amount;
	scrValue m_workshop;
	scrValue m_workshop_amount;
	scrValue m_style;
	scrValue m_style_amount;
	scrValue m_lighting;
	scrValue m_lighting_amount;
	scrValue m_livingQuarter;
	scrValue m_livingQuarter_amount;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(srcSpendHangar);


class srcSpentOnBase
{
public:
	scrValue m_location;
	scrValue m_location_amount;
	scrValue m_style;
	scrValue m_style_amount;
	scrValue m_graphics;
	scrValue m_graphics_amount;
	scrValue m_orbcannon;
	scrValue m_orbcannon_amount;
	scrValue m_secroom;
	scrValue m_secroom_amount;
	scrValue m_lounge;
	scrValue m_lounge_amount;
	scrValue m_livingquarter;
	scrValue m_livingquarter_amount;
	scrValue m_privacyglass;
	scrValue m_privacyglass_amount;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(srcSpentOnBase);

class srcSpentTiltRotor
{
public:
	scrValue m_aircraft;
	scrValue m_aircraft_amount;
	scrValue m_interiortint;
	scrValue m_interiortint_amount;
	scrValue m_turret;
	scrValue m_turret_amount;
	scrValue m_weaponworkshop;
	scrValue m_weaponworkshop_amount;
	scrValue m_vehicleworkshop;
	scrValue m_vehicleworkshop_amount;
	scrValue m_countermeasures;
	scrValue m_countermeasures_amount;
	scrValue m_bombs;
	scrValue m_bombs_amount;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(srcSpentTiltRotor);

class srcSpendHackerTruck
{
public:
	scrValue m_truck;
	scrValue m_truck_amount;
	scrValue m_tint;
	scrValue m_tint_amount;
	scrValue m_pattern;
	scrValue m_pattern_amount;
	scrValue m_missileLauncher;
	scrValue m_missileLauncher_amount;
	scrValue m_droneStation;
	scrValue m_droneStation_amount;
	scrValue m_weaponWorkshop;
	scrValue m_weaponWorkshop_amount;
	scrValue m_bike;
	scrValue m_bike_amount;
	scrValue m_bikeWorkshop;
	scrValue m_bikeWorkshop_amount;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(srcSpendHackerTruck);

class srcSpendNightClubAndWarehouse
{
public:
	scrValue m_location;
	scrValue m_location_amount;
	scrValue m_dj;
	scrValue m_dj_amount;
	scrValue m_style;
	scrValue m_style_amount;
	scrValue m_tint;
	scrValue m_tint_amount;
	scrValue m_lighting;
	scrValue m_lighting_amount;
	scrValue m_staff;
	scrValue m_staff_amount;
	scrValue m_security;
	scrValue m_security_amount;
	scrValue m_equipment;
	scrValue m_equipment_amount;
	scrValue m_whGarage2;
	scrValue m_whGarage2_amount;
	scrValue m_whGarage3;
	scrValue m_whGarage3_amount;
	scrValue m_whGarage4;
	scrValue m_whGarage4_amount;
	scrValue m_whGarage5;
	scrValue m_whGarage5_amount;
	scrValue m_whBasement2;
	scrValue m_whBasement2_amount;
	scrValue m_whBasement3;
	scrValue m_whBasement3_amount;
	scrValue m_whBasement4;
	scrValue m_whBasement4_amount;
	scrValue m_whBasement5;
	scrValue m_whBasement5_amount;
	scrValue m_whTechnician2;
	scrValue m_whTechnician2_amount;
	scrValue m_whTechnician3;
	scrValue m_whTechnician3_amount;
	scrValue m_whTechnician4;
	scrValue m_whTechnician4_amount;
	scrValue m_whTechnician5;
	scrValue m_whTechnician5_amount;
	scrValue m_name;
	scrValue m_name_amount;
	scrValue m_podium;
	scrValue m_podium_amount;
	scrValue m_dryice;
	scrValue m_dryice_amount;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(srcSpendNightClubAndWarehouse);

class srcSpentOnArena
{
public:
	scrValue m_location;
	scrValue m_location_amount;
	scrValue m_style;
	scrValue m_style_amount;
	scrValue m_graphics;
	scrValue m_graphics_amount;
	scrValue m_colour;
	scrValue m_colour_amount;
	scrValue m_floor;
	scrValue m_floor_amount;
	scrValue m_mechanic;
	scrValue m_mechanic_amount;
	scrValue m_personalQuarters;
	scrValue m_personalQuarters_amount;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(srcSpentOnArena);

class srcSpentCasino
{
public:
	scrValue m_masterBedroom;
	scrValue m_masterBedroom_amount;
	scrValue m_lounge;
	scrValue m_lounge_amount;
	scrValue m_spa;
	scrValue m_spa_amount;
	scrValue m_barParty;
	scrValue m_barParty_amount;
	scrValue m_dealer;
	scrValue m_dealer_amount;
	scrValue m_extraBedroom;
	scrValue m_extraBedroom_amount;
	scrValue m_extraBedroom2;
	scrValue m_extraBedroom2_amount;
	scrValue m_mediaRoom;
	scrValue m_mediaRoom_amount;
	scrValue m_garage;
	scrValue m_garage_amount;
	scrValue m_colour;
	scrValue m_colour_amount;
	scrValue m_graphics;
	scrValue m_graphics_amount;
	scrValue m_office;
	scrValue m_office_amount;
	scrValue m_preset;
	scrValue m_preset_amount;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(srcSpentCasino);

class srcSpentArcade
{
public:
	scrValue m_location;
	scrValue m_location_amount;
	scrValue m_garage;
	scrValue m_garage_amount;
	scrValue m_sleeping_quarter;
	scrValue m_sleeping_quarter_amount;
	scrValue m_drone_station;
	scrValue m_drone_station_amount;
	scrValue m_business_management;
	scrValue m_business_management_amount;
	scrValue m_style;
	scrValue m_style_amount;
	scrValue m_mural;
	scrValue m_mural_amount;
	scrValue m_floor;
	scrValue m_floor_amount;
	scrValue m_neon_art;
	scrValue m_neon_art_amount;
	scrValue m_highscore_screen;
	scrValue m_highscore_screen_amount;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(srcSpentArcade);

class srcSpentBuySub
{
public:
	scrValue m_submarine_amount;
	scrValue m_color;
	scrValue m_color_amount;
	scrValue m_flag;
	scrValue m_flag_amount;
	scrValue m_anti_aircraft_amount;
	scrValue m_missile_station;
	scrValue m_missile_station_amount;
	scrValue m_sonar;
	scrValue m_sonar_amount;
	scrValue m_weapon_workshop;
	scrValue m_weapon_workshop_amount;
	scrValue m_avisa;
	scrValue m_avisa_pool_amount;
	scrValue m_seasparrow;
	scrValue m_seasparrow_pool_amount;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(srcSpentBuySub);

class srcSpentIslandHeist
{
public:
	scrValue m_airstrike;		// buying airstrike support crew on the planning board
	scrValue m_heavy_weapon;	// buying heavy weapon drop on the planning board
	scrValue m_sniper;			// buying sniper support crew on the planning board
	scrValue m_air_support;		// buying air support crew on the planning board
	scrValue m_drone;			// buying spy drone on the planning board
	scrValue m_weapon_stash;	// buying weapon stash on the planning board
	scrValue m_suppressor;		// buying suppressors on the planning board
	scrValue m_replay;			// paying to replay the heist
	scrValue m_prep;			// paying to skip the prep
	scrValue m_prepItem;		// prep name/ID	
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(srcSpentIslandHeist);

class srcSpentBuyAutoshop
{
public:
	scrValue m_location;
	scrValue m_location_amount;
	scrValue m_style;
	scrValue m_style_amount;
	scrValue m_tint;
	scrValue m_tint_amount;
	scrValue m_emblem;
	scrValue m_emblem_amount;
	scrValue m_crew_name;
	scrValue m_crew_name_amount;
	scrValue m_staff;
	scrValue m_staff_amount;
	scrValue m_lift;
	scrValue m_lift_amount;
	scrValue m_personal_quarter;
	scrValue m_personal_quarter_amount;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(srcSpentBuyAutoshop);

class srcSpentBuyAgency
{
public:
	scrValue m_location;
	scrValue m_location_amount;
	scrValue m_style;
	scrValue m_style_amount;
	scrValue m_wallpaper;
	scrValue m_wallpaper_amount;
	scrValue m_tint;
	scrValue m_tint_amount;
	scrValue m_personal_quarter;
	scrValue m_personal_quarter_amount;
	scrValue m_weapon_workshop;
	scrValue m_weapon_workshop_amount;
	scrValue m_vehicle_workshop;
	scrValue m_vehicle_workshop_amount;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(srcSpentBuyAgency);

class srcApartmentUtilities
{
public:
	int m_LowAptFees;
	int m_MedAptFees;
	int m_HighAptFees;
	int m_YachtFees;
	int m_FacilityFees;
	int m_PenthouseFees;
	int m_KosatkaFees;
	int m_CleanerFees;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(srcApartmentUtilities);

class srcBusinessUtilities
{
public:
	int m_BunkerFees;
	int m_WeedFees;
	int m_MethFees;
	int m_DocForgeFees;
	int m_FakeCashFees;
	int m_CocaineFees;
	int m_HangarFees;
	int m_NightclubFees;
	int m_NightclubStaff;
	int m_ExecOfficeFees;
	int m_ExecAssistantFees;
	int m_SmallWhouseFees;
	int m_MediumWhouseFees;
	int m_LargeWhouseFees;
	int m_ArcadeFees;
	int m_AutoShopFees;
	int m_FixerAgencyFees;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(srcBusinessUtilities);

namespace money_commands {

RAGE_DEFINE_SUBCHANNEL(stat, money, DIAG_SEVERITY_DEBUG3, DIAG_SEVERITY_DEBUG1)
#undef __stat_channel
#define __stat_channel stat_money

RAGE_DEFINE_SUBCHANNEL(script, money, DIAG_SEVERITY_DEBUG3, DIAG_SEVERITY_DEBUG1)
#undef __script_channel
#define __script_channel script_money

struct  sAmountCapTypes
{
public:
	sAmountCapTypes(const char* type, const int amount) : m_type(type), m_amount(amount){}
	atHashString m_type;
	int m_amount;
};
#define AMOUNTCAPTYPE(type, amount) sAmountCapTypes(type, amount)

#if !__FINAL
void VERIFY_STATS_LOADED(const char* commandname, const bool spewdebug)
{
	if (scriptVerify(commandname))
	{
		if (spewdebug) scriptDebugf1("Command called - %s", commandname);
		scriptAssertf(NetworkInterface::IsLocalPlayerOnline(), "%s : %s - Trying to access money without being online.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandname);
		scriptAssertf(!StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT), "%s : %s - Trying to access money without loading the savegame.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandname);
		scriptAssertf(!StatsInterface::CloudFileLoadFailed(STAT_MP_CATEGORY_DEFAULT), "%s : %s - Trying to access money but stats loaded have failed.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandname);
	}
}
#endif // __FINAL

bool IsHandleValid(int& handleData, int sizeOfData)
{
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

	// invalid service
	return false;
}

//PURPOSE
//  Returns TRUE if level design can make cash changes.
bool CAN_SCRIPT_CHANGE_CASH(const char* NET_SHOP_ONLY(OUTPUT_ONLY(commandname)))
{
	bool result = true;

	////Cops&Crooks doesn't have GTA dollars involved.
	//if (NetworkInterface::IsInCopsAndCrooks())
	//{
	//	return false;
	//}

#if __NET_SHOP_ACTIVE
	result = NETWORK_SHOPPING_MGR.ShouldDoNullTransaction();
	if (!result)
	{
		scriptWarningf("%s: CanScriptChangeCash - Command '%s' called but cash changes are not allowed.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandname);
	}
#endif // __NET_SHOP_ACTIVE

	return result;
}

static inline bool SendMoneyMetric(const rlMetric& m, const bool deferred = false)
{
	//If we are not saving all transactions are dumped.
	if(StatsInterface::GetBlockSaveRequests())
		return true;

	return CNetworkTelemetry::AppendMetric(m, deferred);
}

static inline bool SendDeferredMoneyMetric(const rlMetric& m)
{
	//If we are not saving all transactions are dumped.
	if(StatsInterface::GetBlockSaveRequests())
		return true;

	return CNetworkTelemetry::AppendMetric(m, true);
}

bool EarnCash(s64 amount, bool toBank, const bool deferredMetric, rlMetric* m, StatId statId, int itemId = 0, bool changeValue = false)
{
    //@@: location MONEYCOMMANDS_EARNCASH_ENTRY
	if (!CAN_SCRIPT_CHANGE_CASH("EarnCash"))
	{
#if __NET_SHOP_ACTIVE

		////Cops&Crooks doesn't have GTA dollars involved.
		//if (NetworkInterface::IsInCopsAndCrooks())
		//{
		//	return false;
		//}

		//Cash Category statistics
		if (StatsInterface::IsKeyValid(statId))
			StatsInterface::IncrementStat(statId, (float)amount);

		//Network event for the bank
		if (toBank)
		{
			CEventNetworkCashTransactionLog tEvent(-1, CEventNetworkCashTransactionLog::CASH_TRANSACTION_BANK, CEventNetworkCashTransactionLog::CASH_TRANSACTION_DEPOSIT, amount, NetworkInterface::GetLocalGamerHandle(), itemId);
			GetEventScriptNetworkGroup()->Add(tEvent);
		}

		if (amount > 0)
		{//Increment
			static const StatId STAT_MPPLY_TOTAL_EVC = StatId(ATSTRINGHASH("MPPLY_TOTAL_EVC", 0xBF3CB334));
			const s64 finalAmount = StatsInterface::GetInt64Stat(STAT_MPPLY_TOTAL_EVC) + amount;
			StatsInterface::SetStatData(STAT_MPPLY_TOTAL_EVC, finalAmount);
			statDebugf1("%s: EarnCash - Operation: Increment MPPLY_TOTAL_EVC - amount='%" I64FMT "d.' balance='%" I64FMT "d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, finalAmount);
		}

		if (GameTransactionSessionMgr::Get().GetNonceForTelemetry() == 0 && amount > 0)
		{
			statAssertf(0, "%s: EarnCash - amount < %" I64FMT "d > - NO NONCE for Telemetry set.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
			statErrorf("%s: EarnCash - amount < %" I64FMT "d > - NO NONCE for Telemetry set.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
			return true;
		}

		if (m)
		{
			if (deferredMetric)
			{
				SendDeferredMoneyMetric(*m);
			}
			else
			{
				SendMoneyMetric(*m);
			}
		}

		//Make sure we clear the nonce.
		GameTransactionSessionMgr::Get().SetNonceForTelemetry(0);

#endif //__NET_SHOP_ACTIVE

		return true;
	}
    //@@: range MONEYCOMMANDS_EARNCASH {

	scriptAssertf(NetworkInterface::IsGameInProgress(), "%s: EarnCash - Can't access network player cash while in single player", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!NetworkInterface::IsGameInProgress())
		return false;

	scriptAssertf(NetworkInterface::IsInFreeMode(), "%s: EarnCash - Can't access network player cash while not in freemode", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!NetworkInterface::IsInFreeMode())
		return false;

	scriptDebugf1("Earn transaction - changeValue='%s' - Current Cash: WALLET='%" I64FMT "d'+BANK='%" I64FMT "d' == EVC='%" I64FMT "d'+PVC='%" I64FMT "d' - toBank=%s", changeValue?"true":"false", MoneyInterface::GetVCWalletBalance(), MoneyInterface::GetVCBankBalance(), MoneyInterface::GetEVCBalance(), MoneyInterface::GetPVCBalance(), toBank?"true":"false");

	bool result = true;

	if (toBank)
	{
		result = MoneyInterface::IncrementStatBankBalance(amount, changeValue);

		if (0 != itemId && result && changeValue)
		{
			CEventNetworkCashTransactionLog tEvent(-1, CEventNetworkCashTransactionLog::CASH_TRANSACTION_BANK, CEventNetworkCashTransactionLog::CASH_TRANSACTION_DEPOSIT, amount, NetworkInterface::GetLocalGamerHandle(), itemId);
			GetEventScriptNetworkGroup()->Add(tEvent);
		}
	}
	else
	{
		result = MoneyInterface::IncrementStatWalletBalance(amount, -1, changeValue);
	}

	result = result && MoneyInterface::IncrementStatEVCBalance(amount, changeValue);

	scriptAssertf(result, "Earn transaction - FAILED - Current Cash: WALLET='%" I64FMT "d'+BANK='%" I64FMT "d' == EVC='%" I64FMT "d'+PVC='%" I64FMT "d' - toBank=%s", MoneyInterface::GetVCWalletBalance(), MoneyInterface::GetVCBankBalance(), MoneyInterface::GetEVCBalance(), MoneyInterface::GetPVCBalance(), toBank?"true":"false");
	if (result)
	{
		if (!changeValue)
		{
			//Lets change the value now.
			EarnCash(amount, toBank, deferredMetric, m, statId, itemId, true);
		}
		else
		{
			if (StatsInterface::IsKeyValid(statId))
				StatsInterface::IncrementStat(statId, (float)amount);

			if (m)
			{
				if (deferredMetric)
				{
					SendDeferredMoneyMetric(*m);
				}
				else
				{
					SendMoneyMetric(*m);
				}
			}

			MoneyInterface::UpdateHighEarnerCashAmount(amount);
			CStatsMgr::GetStatsDataMgr().GetSavesMgr().FlushProfileStats( );
		}
	}
    //@@: } MONEYCOMMANDS_EARNCASH
	else
	{
		scriptErrorf("Earn transaction - FAILED - Current Cash: WALLET='%" I64FMT "d'+BANK='%" I64FMT "d' == EVC='%" I64FMT "d'+PVC='%" I64FMT "d' - toBank=%s", MoneyInterface::GetVCWalletBalance(), MoneyInterface::GetVCBankBalance(), MoneyInterface::GetEVCBalance(), MoneyInterface::GetPVCBalance(), toBank?"true":"false");
	}
    //@@: location MONEYCOMMANDS_EARNCASH_EXIT
	return result;
}

bool SpendCash(s64 amount, bool fromBank, bool fromBankAndWallet, bool fromWalletAndBank, const bool deferredMetric, rlMetric* m, StatId statId, int itemId, int character=-1, bool changeValue = false, bool evcOnly = false)
{
	bool result = false;

	if (!CAN_SCRIPT_CHANGE_CASH("SpendCash"))
	{
		////Cops&Croks doesn't have GTA dollars involved.
		//if (NetworkInterface::IsInCopsAndCrooks())
		//{
		//	return false;
		//}

#if __NET_SHOP_ACTIVE

		//Cash Category statistics
		if (StatsInterface::IsKeyValid(statId))
			StatsInterface::IncrementStat(statId, (float)amount);

		//Network event for the bank
		if (fromBank || fromBankAndWallet || fromWalletAndBank)
		{
			CEventNetworkCashTransactionLog tEvent(-1, CEventNetworkCashTransactionLog::CASH_TRANSACTION_BANK, CEventNetworkCashTransactionLog::CASH_TRANSACTION_WITHDRAW, amount, NetworkInterface::GetLocalGamerHandle(), itemId);
			GetEventScriptNetworkGroup()->Add(tEvent);
		}

		if (amount > 0)
		{//Increment
			static const StatId STAT_MPPLY_TOTAL_SVC = ATSTRINGHASH("MPPLY_TOTAL_SVC", 0x8A748CFA);
			const s64 finalAmount = StatsInterface::GetInt64Stat(STAT_MPPLY_TOTAL_SVC) + amount;
			StatsInterface::SetStatData(STAT_MPPLY_TOTAL_SVC, finalAmount);
			statDebugf1("%s: SpendCash - Operation: Increment MPPLY_TOTAL_SVC - amount='%" I64FMT "d.' balance='%" I64FMT "d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, finalAmount);
		}

		if (GameTransactionSessionMgr::Get().GetNonceForTelemetry() == 0 && amount > 0)
		{
			statAssertf(0,"%s: SpendCash -  amount < %" I64FMT "d > - NO NONCE for Telemetry set.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
			statErrorf("%s: SpendCash -  amount < %" I64FMT "d > - NO NONCE for Telemetry set.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
			return true;
		}

		if (m)
		{
			if (deferredMetric)
			{
				SendDeferredMoneyMetric(*m);
			}
			else
			{
				SendMoneyMetric(*m);
			}
		}

		//Make sure we clear the nonce.
		GameTransactionSessionMgr::Get().SetNonceForTelemetry(0);

#endif //__NET_SHOP_ACTIVE

		return true;
	}

	scriptAssertf(amount>=0, "%s: SpendCash - invalid money amount < %" I64FMT "d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return result;

	scriptAssertf(NetworkInterface::IsGameInProgress(), "%s: SpendCash - Can't access network player cash while in single player", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!NetworkInterface::IsGameInProgress())
		return result;

	scriptAssertf(NetworkInterface::IsInFreeMode(), "%s: SpendCash - Can't access network player cash while not in freemode", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!NetworkInterface::IsInFreeMode())
		return result;

    if (amount > 0)
    {
	    scriptAssertf(amount<=MoneyInterface::GetVCBalance(), "%s: SpendCash - Not enough Virtual Cash %" I64FMT "d for this transaction amount='%" I64FMT "d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), MoneyInterface::GetVCBalance(), amount);
	    if (amount > MoneyInterface::GetVCBalance())
		    return result;

	    scriptDebugf1("Spend transaction - changeValue='%s' - Current Cash: WALLET='%" I64FMT "d'+BANK='%" I64FMT "d' == EVC='%" I64FMT "d'+PVC='%" I64FMT "d' - fromBank=%s, fromBankAndWallet=%s, fromWalletAndBank=%s", changeValue?"true":"false", MoneyInterface::GetVCWalletBalance(character), MoneyInterface::GetVCBankBalance(), MoneyInterface::GetEVCBalance(), MoneyInterface::GetPVCBalance(), fromBank?"true":"false", fromBankAndWallet?"true":"false", fromWalletAndBank?"true":"false");

		if (evcOnly)
		{
			//Check against total EVC.
			const s64 evc = MoneyInterface::GetEVCBalance();
			scriptAssertf(amount <= evc, "%s: SpendCash - Not enough money in the evc='%" I64FMT "d' for this transaction amount='%" I64FMT "d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), evc, amount);
			if (amount > evc)
			{
				return result;
			}
		}

	    if (fromBankAndWallet || fromWalletAndBank)
	    {
		    const s64 wallet_bank = MoneyInterface::GetVCBankBalance() + MoneyInterface::GetVCWalletBalance(character);
		    scriptAssertf(amount<=wallet_bank, "%s: SpendCash - Not enough money in the bank+wallet %" I64FMT "d for this transaction amount='%" I64FMT "d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), wallet_bank, amount);
		    if (amount > wallet_bank)
			    return result;
	    }
	    else if (fromBank)
	    {
		    scriptAssertf(amount<=MoneyInterface::GetVCBankBalance(), "%s: SpendCash - Not enough money in the bank='%" I64FMT "d' for this transaction amount='%" I64FMT "d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), MoneyInterface::GetVCBankBalance(), amount);
		    if (amount > MoneyInterface::GetVCBankBalance())
			    return result;
	    }
	    else
	    {
		    scriptAssertf(amount<=MoneyInterface::GetVCWalletBalance(character), "%s: SpendCash - Not enough money in the wallet='%" I64FMT "d' for this transaction amount='%" I64FMT "d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), MoneyInterface::GetVCWalletBalance(character), amount);
		    if (amount > MoneyInterface::GetVCWalletBalance(character))
			    return result;
	    }
    }

	result = true;

    if (amount > 0)
    {
	    if (fromWalletAndBank)
	    {
		    //Get the correct money amounts and stats
		    const s64 bankCash   = MoneyInterface::GetVCBankBalance();
		    const s64 walletCash = MoneyInterface::GetVCWalletBalance();

		    if ( amount <= walletCash )
		    {
			    result = result && MoneyInterface::DecrementStatWalletBalance(amount, -1, changeValue);
		    }
		    else if ( scriptVerify(amount <= (bankCash + walletCash)) )
		    {
			    if (walletCash > 0)
			    {
				    result = result && MoneyInterface::DecrementStatWalletBalance((s64)walletCash, -1, changeValue);
			    }

			    result = result && MoneyInterface::DecrementStatBankBalance((s64)(amount - walletCash), changeValue);
		    }
		    else
		    {
			    result = false;
		    }

		    //Generate script event
		    if (result && changeValue)
		    {
			    CEventNetworkCashTransactionLog tEvent(-1, CEventNetworkCashTransactionLog::CASH_TRANSACTION_BANK, CEventNetworkCashTransactionLog::CASH_TRANSACTION_WITHDRAW, amount, NetworkInterface::GetLocalGamerHandle(), itemId);
			    GetEventScriptNetworkGroup()->Add(tEvent);
		    }
	    }
	    else if (fromBankAndWallet)
	    {
		    //Get the correct money amounts and stats
		    const s64 bankCash   = MoneyInterface::GetVCBankBalance();
		    const s64 walletCash = MoneyInterface::GetVCWalletBalance();

		    if ( amount <= bankCash )
		    {
			    result = result && MoneyInterface::DecrementStatBankBalance(amount, changeValue);
		    }
		    else if ( scriptVerify(amount <= (bankCash + walletCash)) )
		    {
			    if (bankCash > 0)
			    {
				    result = result && MoneyInterface::DecrementStatBankBalance((s64)bankCash, changeValue);
			    }

			    result = result && MoneyInterface::DecrementStatWalletBalance((s64)(amount - bankCash), -1, changeValue);
		    }
		    else
		    {
			    result = false;
		    }

		    //Generate script event
		    if (result && changeValue)
		    {
			    CEventNetworkCashTransactionLog tEvent(-1, CEventNetworkCashTransactionLog::CASH_TRANSACTION_BANK, CEventNetworkCashTransactionLog::CASH_TRANSACTION_WITHDRAW, amount, NetworkInterface::GetLocalGamerHandle(), itemId);
			    GetEventScriptNetworkGroup()->Add(tEvent);
		    }
	    }
	    else if (fromBank)
	    {
		    result = result && MoneyInterface::DecrementStatBankBalance(amount, changeValue);

		    //Generate script event
		    if (result && changeValue)
		    {
			    CEventNetworkCashTransactionLog tEvent(-1, CEventNetworkCashTransactionLog::CASH_TRANSACTION_BANK, CEventNetworkCashTransactionLog::CASH_TRANSACTION_WITHDRAW, amount, NetworkInterface::GetLocalGamerHandle(), itemId);
			    GetEventScriptNetworkGroup()->Add(tEvent);
		    }
	    }
	    else
	    {
		    result = MoneyInterface::DecrementStatWalletBalance((s64)amount, character, changeValue);
	    }

	    const s64 earntCash = MoneyInterface::GetEVCBalance();
	    const s64 paidCash  = MoneyInterface::GetPVCBalance();

	    if (result)
	    {
		    if(evcOnly)
		    {
			    if(scriptVerify(amount <= earntCash))
		        {
			        result = result && MoneyInterface::DecrementStatEVCBalance((s64)(amount), changeValue);
		        }
		    }
		    else if ( amount <= paidCash )
		    {
			    result = result && MoneyInterface::DecrementStatPVCBalance((s64)amount, changeValue);
		    }
		    else if ( scriptVerify(amount <= (paidCash + earntCash)) )
		    {
			    if (paidCash > 0)
			    {
				    result = result && MoneyInterface::DecrementStatPVCBalance((s64)paidCash, changeValue);
			    }

			    result = result && MoneyInterface::DecrementStatEVCBalance((s64)(amount - paidCash), changeValue);
		    }
		    else
		    {
			    result = false;
		    }
	    }
    }

	scriptAssertf(result, "Spend transaction - FAILED - Current Cash: WALLET='%" I64FMT "d'+BANK='%" I64FMT "d' == EVC='%" I64FMT "d'+PVC='%" I64FMT "d' - fromBank=%s, fromBankAndWallet=%s, fromWalletAndBank=%s", MoneyInterface::GetVCWalletBalance(character), MoneyInterface::GetVCBankBalance(), MoneyInterface::GetEVCBalance(), MoneyInterface::GetPVCBalance(), fromBank?"true":"false", fromBankAndWallet?"true":"false", fromWalletAndBank?"true":"false");
	if (result)
	{
		if (!changeValue)
		{
			//Let's change the cash values now.
			SpendCash(amount, fromBank, fromBankAndWallet, fromWalletAndBank, deferredMetric, m, statId, itemId, character, true, evcOnly);
		}
		else
		{
			if (StatsInterface::IsKeyValid(statId))
				StatsInterface::IncrementStat(statId, (float)amount);

			if (m)
			{
				if (deferredMetric)
				{
					SendDeferredMoneyMetric(*m);
				}
				else
				{
					SendMoneyMetric(*m);
				}
			}

			//Flag we want profile stats flush
			CStatsMgr::GetStatsDataMgr().GetSavesMgr().FlushProfileStats( );
		}
	}
	else
	{
		scriptErrorf("Spend transaction - FAILED - Current Cash: WALLET='%" I64FMT "d'+BANK='%" I64FMT "d' == EVC='%" I64FMT "d'+PVC='%" I64FMT "d' - fromBank=%s, fromBankAndWallet=%s, fromWalletAndBank=%s", MoneyInterface::GetVCWalletBalance(character), MoneyInterface::GetVCBankBalance(), MoneyInterface::GetEVCBalance(), MoneyInterface::GetPVCBalance(), fromBank?"true":"false", fromBankAndWallet?"true":"false", fromWalletAndBank?"true":"false");
	}

	return result;
}

bool  GetPlayerIsHighEarner( )
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_GET_PLAYER_IS_HIGH_EARNER", false);)
	return MoneyInterface::GetPlayerIsHighEarner();
}

void InitPlayerCash(int walletamount, int bankamount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_INITIALIZE_CASH", true);)

	scriptDebugf1("NETWORK_INITIALIZE_CASH, walletamount=%d, bankamount=%d", walletamount, bankamount);

	scriptAssertf(!StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT), "%s: NETWORK_INITIALIZE_CASH - Load pending for STAT_MP_CATEGORY_DEFAULT.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	scriptAssertf(!StatsInterface::CloudFileLoadFailed(STAT_MP_CATEGORY_DEFAULT), "%s: NETWORK_INITIALIZE_CASH - Load Failed for STAT_MP_CATEGORY_DEFAULT.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	if (walletamount > 0)
	{
		if (!CAN_SCRIPT_CHANGE_CASH("NETWORK_INITIALIZE_CASH"))
		{
			SendMoneyMetric(MetricEarnInitPlayer(walletamount, false));
		}
		else
		{
			scriptAssertf(0 == MoneyInterface::GetVCWalletBalance(), "%s: NETWORK_INITIALIZE_CASH - Wallet must be at 0, current wallet amount=%" I64FMT "d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), MoneyInterface::GetVCWalletBalance());
			if (0 == MoneyInterface::GetVCWalletBalance())
			{
				MoneyInterface::IncrementStatEVCBalance((s64)walletamount);
				MoneyInterface::IncrementStatWalletBalance((s64)walletamount);
				SendMoneyMetric(MetricEarnInitPlayer(walletamount, false));
			}
		}
	}

	if (bankamount > 0)
	{
		if (!CAN_SCRIPT_CHANGE_CASH("NETWORK_INITIALIZE_CASH"))
		{
			SendMoneyMetric(MetricEarnInitPlayer(bankamount, true));
		}
		else
		{
			scriptAssertf(0 == MoneyInterface::GetVCBankBalance(), "%s: NETWORK_INITIALIZE_CASH - Bank must be at 0, current bank amount=%" I64FMT "d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), MoneyInterface::GetVCBankBalance());
			if (0 == MoneyInterface::GetVCBankBalance())
			{
				MoneyInterface::IncrementStatEVCBalance((s64)bankamount);
				MoneyInterface::IncrementStatBankBalance((s64)bankamount);
				SendMoneyMetric(MetricEarnInitPlayer(bankamount, true));
			}
		}
	}
}

void ClearCharacterWallet(int character)
{
	if (character>=0 && character<MAX_NUM_MP_CHARS)
	{
		scriptDebugf1("NETWORK_CLEAR_CHARACTER_WALLET, character slot=%d.", character);

		if (!CAN_SCRIPT_CHANGE_CASH("NETWORK_CLEAR_CHARACTER_WALLET"))
			return;

		if (MoneyInterface::GetVCWalletBalance(character) > 0)
		{
			const s64 amount = MoneyInterface::GetVCWalletBalance(character);
			MoneyInterface::DecrementStatWalletBalance((s64)amount, character);
		}
	}
}

void ManualDeleteCharacter(int character)
{
	NOTFINAL_ONLY( scriptDebugf1("NETWORK_MANUAL_DELETE_CHARACTER, character=%d.", character); )
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_MANUAL_DELETE_CHARACTER", true);)
	SendMoneyMetric(MetricManualDeleteCharater(character));
}

void DeleteCharacter(int character, bool UNUSED_PARAM(wipeWallet), bool NOTFINAL_ONLY(wipeBank))
{
	NOTFINAL_ONLY( scriptDebugf1("NETWORK_DELETE_CHARACTER, character=%d, wipeBank=%s", character, wipeBank?"true":"false"); )

	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_DELETE_CHARACTER", true);)

	if (MoneyInterface::GetVCWalletBalance(character) > 0)
	{
		if (CAN_SCRIPT_CHANGE_CASH("NETWORK_DELETE_CHARACTER"))
		{
			const s64 amount = MoneyInterface::GetVCWalletBalance(character);
			MoneyInterface::DecrementStatWalletBalance((s64)amount, character);
			MoneyInterface::IncrementStatBankBalance((s64)amount);
		}
	}

#if !__FINAL
	if (wipeBank)
	{
		SendMoneyMetric(MetricClearBank());

		if (CAN_SCRIPT_CHANGE_CASH("NETWORK_DELETE_CHARACTER"))
		{
			CStatsDataMgr& statsmgr = CStatsMgr::GetStatsDataMgr();

			statsmgr.SetStatSynched(STAT_BANK_BALANCE);
			StatsInterface::SetStatData(STAT_BANK_BALANCE.GetHash(), (s64)0);

			statsmgr.SetStatSynched(STAT_CLIENT_PVC_BALANCE);
			StatsInterface::SetStatData(STAT_CLIENT_PVC_BALANCE.GetHash(),  (s64)0);

			statsmgr.SetStatSynched(STAT_CLIENT_EVC_BALANCE);
			StatsInterface::SetStatData(STAT_CLIENT_EVC_BALANCE.GetHash(),  (s64)0);

			//We want to clear the server ones we're received too, since we've seen them already.  They've been 
			//cleared by the call to MetricClearBank() above.
			StatId STAT_PVC_BALANCE("PVC_BALANCE");
			statsmgr.SetStatSynched(STAT_PVC_BALANCE);
			StatsInterface::SetStatData(STAT_PVC_BALANCE.GetHash(),  (s64)0);

			StatId STAT_EVC_BALANCE("EVC_BALANCE");
			statsmgr.SetStatSynched(STAT_EVC_BALANCE);
			StatsInterface::SetStatData(STAT_EVC_BALANCE.GetHash(),  (s64)0);

			//statsmgr.SetStatSynched(STAT_PVC_USDE);
			//StatsInterface::SetStatData(STAT_PVC_USDE.GetHash(),     0.0f);

			//statsmgr.SetStatSynched(STAT_PERSONAL_EXCHANGE_RATE);
			//StatsInterface::SetStatData(STAT_PERSONAL_EXCHANGE_RATE.GetHash(),     0.0f);
		}
	}
#endif // !__FINAL

	SendMoneyMetric(MetricDeleteCharater(character));
}

//
// Shared Cash - Give event
//
bool CanShareJobCash()
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_CAN_SHARE_JOB_CASH", true);)

	if (!rlRos::HasPrivilege(NetworkInterface::GetLocalGamerIndex(), RLROS_PRIVILEGEID_TRANSFERVC))
	{
		scriptErrorf("Can not share job cash because RLROS_PRIVILEGEID_TRANSFERVC is FALSE.");
		return false;
	}

	return true;
}

void GivePlayerSharedJobCash(int amount, int& handleData)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_GIVE_PLAYER_JOBSHARE_CASH", true);)

	scriptAssertf(amount>0, "%s: NETWORK_GIVE_PLAYER_JOBSHARE_CASH - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 120000;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_GIVE_PLAYER_JOBSHARE_CASH - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	// make sure that handle is valid
	rlGamerHandle hGamer; 
	if(CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_GIVE_PLAYER_JOBSHARE_CASH")))
	{
		MetricGiveJobShared m(amount, hGamer);
		const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_JOBSHARED");
		const int itemId = -1;
		if (SpendCash(amount, false, false, false, false, &m, statId, itemId))
		{
			//Generate script event
			CEventNetworkCashTransactionLog tEvent(-1, CEventNetworkCashTransactionLog::CASH_TRANSACTION_GAMER, CEventNetworkCashTransactionLog::CASH_TRANSACTION_WITHDRAW, amount, hGamer);
			GetEventScriptNetworkGroup()->Add(tEvent);
		}
		else
		{
			scriptErrorf("%s: NETWORK_GIVE_PLAYER_JOBSHARE_CASH - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
}

//
// Shared cash - Receive event
//
void ReceivePlayerSharedJobCash(int amount, int& handleData)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_RECEIVE_PLAYER_JOBSHARE_CASH", true);)

	scriptAssertf(amount>0, "%s: NETWORK_RECEIVE_PLAYER_JOBSHARE_CASH - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 120000;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_RECEIVE_PLAYER_JOBSHARE_CASH - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	// make sure that handle is valid
	rlGamerHandle hGamer; 
	if(CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_RECEIVE_PLAYER_JOBSHARE_CASH")))
	{
		const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBSHARED");
		if (EarnCash(amount, false, true, NULL, statId))
		{
			//Generate script event
			CEventNetworkCashTransactionLog tEvent(-1, CEventNetworkCashTransactionLog::CASH_TRANSACTION_GAMER, CEventNetworkCashTransactionLog::CASH_TRANSACTION_DEPOSIT, amount, hGamer);
			GetEventScriptNetworkGroup()->Add(tEvent);
		}
	}
}

//
// Earn events
//
void EarnMoneyFromPickup(int amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_FROM_PICKUP", true);)

	scriptAssertf(amount>0, "%s: NETWORK_EARN_FROM_PICKUP - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 10000;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_PICKUP - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	MetricEarnFromPickup m(amount);
	if(!EarnCash(amount, false, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_PICKED_UP")))
	{
		scriptErrorf("%s: NETWORK_EARN_FROM_PICKUP - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void EarnMoneyFromCashingOut(int amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_FROM_CASHING_OUT", true);)

		scriptAssertf(amount>0, "%s: NETWORK_EARN_FROM_CASHING_OUT - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 1500;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_CASHING_OUT - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	MetricEarnFromPickup m(amount);
	if(!EarnCash(amount, false, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_PICKED_UP")))
	{
		scriptErrorf("%s: NETWORK_EARN_FROM_CASHING_OUT - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void EarnMoneyFromGangAttackPickup(int amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_FROM_GANGATTACK_PICKUP", true);)

		scriptAssertf(amount>0, "%s: NETWORK_EARN_FROM_GANGATTACK_PICKUP - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 10000;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_GANGATTACK_PICKUP - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	MetricEarnFromGangAttackPickup m(amount);
	if(!EarnCash(amount, false, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_PICKED_UP")))
	{
		scriptErrorf("%s: NETWORK_EARN_FROM_GANGATTACK_PICKUP - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void EarnMoneyFromAssassinateTargetKilled(int amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_ASSASSINATE_TARGET_KILLED", true);)

		scriptAssertf(amount>0, "%s: NETWORK_EARN_ASSASSINATE_TARGET_KILLED - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 12000;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_ASSASSINATE_TARGET_KILLED - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	MetricEarnFromAssassinateTargetKilled m(amount);
	if(!EarnCash(amount, false, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS")))
	{
		scriptErrorf("%s: NETWORK_EARN_ASSASSINATE_TARGET_KILLED - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}
void EarnMoneyFromArmoredRobbery(int amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_FROM_ROB_ARMORED_CARS", true);)

	scriptAssertf(amount>0, "%s: NETWORK_EARN_FROM_ROB_ARMORED_CARS - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 40000;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_ROB_ARMORED_CARS - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	MetricEarnFromArmoredRobbery m(amount);
	if(!EarnCash(amount, false, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS")))
	{
		scriptErrorf("%s: NETWORK_EARN_FROM_ROB_ARMORED_CARS - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnSpinTheWheelCash(int amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_SPIN_THE_WHEEL_CASH", true);)

		scriptAssertf(amount>0, "%s: NETWORK_EARN_SPIN_THE_WHEEL_CASH - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 200000;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_SPIN_THE_WHEEL_CASH - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	MetricEarnSpinTheWheelCash m(amount);
	if(!EarnCash(amount, false, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS")))
	{
		scriptErrorf("%s: NETWORK_EARN_SPIN_THE_WHEEL_CASH - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnRcTimeTrial(int amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_RC_TIME_TRIAL", true);)

		scriptAssertf(amount>0, "%s: NETWORK_EARN_RC_TIME_TRIAL - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 5040000;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_RC_TIME_TRIAL - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	MetricEarnSpinTheWheelCash m(amount);
	if(!EarnCash(amount, false, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS")))
	{
		scriptErrorf("%s: NETWORK_EARN_RC_TIME_TRIAL - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnDailyObjectiveEvent(int amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_DAILY_OBJECTIVE_EVENT", true);)

		scriptAssertf(amount>0, "%s: NETWORK_EARN_DAILY_OBJECTIVE_EVENT - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 16000000;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_DAILY_OBJECTIVE_EVENT - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	MetricEarnFromDailyObjective m(amount, "", 0);
	if(!EarnCash(amount, false, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS")))
	{
		scriptErrorf("%s: NETWORK_EARN_DAILY_OBJECTIVE_EVENT - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void EarnMoneyFromCrateDrop(int amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_FROM_CRATE_DROP", true);)

	scriptAssertf(amount>0, "%s: NETWORK_EARN_FROM_CRATE_DROP - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 40000;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_CRATE_DROP - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	MetricEarnFromCrateDrop m(amount);
	if(!EarnCash(amount, false, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS")))
	{
		scriptErrorf("%s: NETWORK_EARN_FROM_CRATE_DROP - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void EarnMoneyFromBetting(int amount, const char* uniqueMatchId)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_FROM_BETTING", true);)

	scriptAssertf(amount>0, "%s: NETWORK_EARN_FROM_BETTING - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	//Check for tunable max number of tickets.
	int amountlimit = 0;
	const bool tuneExists = Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("MAX_BET_VALUE", 0x1ae3b2a0), amountlimit);

	//Check for tunable max number of tickets.
	if (tuneExists && amountlimit > 0)
	{
		scriptAssertf(amount<=amountlimit, "%s: NETWORK_EARN_FROM_BETTING - TUNE - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, amountlimit);
		if (amount>amountlimit)
			return;
	}
	else
	{
		static const int AMOUNT_LIMIT = 1360000;
		scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_BETTING - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
		if (amount>AMOUNT_LIMIT)
			return;
	}

	if(scriptVerifyf(uniqueMatchId, "NETWORK_EARN_FROM_BETTING - no match id set"))
	{
		MetricEarnFromBetting m(amount, uniqueMatchId);
		if(!EarnCash(amount, false, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_BETTING")))
		{
			scriptErrorf("%s: NETWORK_EARN_FROM_BETTING - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
}

void EarnMoneyFromJob(int amount, const char* uniqueMatchId)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_FROM_JOB", true);)

		scriptAssertf(amount > 0, "%s: NETWORK_EARN_FROM_JOB - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount <= 0)
		return;

	static const int AMOUNT_LIMIT = 960000;
	scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_JOB - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount > AMOUNT_LIMIT)
		return;

	MetricEarnFromJob m(amount, uniqueMatchId);
	if (!EarnCash(amount, false, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS")))
	{
		scriptErrorf("%s: NETWORK_EARN_FROM_JOB - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void EarnMoneyFromJobX2(int amount, const char* uniqueMatchId)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_FROM_JOBX2", true);)

	scriptAssertf(amount > 0, "%s: NETWORK_EARN_FROM_JOBX2 - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount <= 0)
		return;

	static const int AMOUNT_LIMIT = 1440000;
	scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_JOBX2 - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount > AMOUNT_LIMIT)
		return;

	static const u32 validscript1 = ATSTRINGHASH("FM_Survival_Controller", 0x1d477306);
	static const u32 validscript2 = ATSTRINGHASH("main_persistent", 0x5700179c);

	const u32 scriptIdHash = static_cast<CGameScriptId&>(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId()).GetScriptNameHash().GetHash();
	scriptAssertf(scriptIdHash == validscript1 || scriptIdHash == validscript2, "%s: NETWORK_EARN_FROM_JOBX2 - invalid scriptid '%08X'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), scriptIdHash);
	if (scriptIdHash != validscript1 && scriptIdHash != validscript2)
	{
		return;
	}

	MetricEarnFromJob m(amount, uniqueMatchId);
	if (!EarnCash(amount, false, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS")))
	{
		scriptErrorf("%s: NETWORK_EARN_FROM_JOBX2 - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void EarnMoneyFromPremiumJob(int amount, const char* uniqueMatchId)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_FROM_PREMIUM_JOB", true);)

	scriptAssertf(amount>0, "%s: NETWORK_EARN_FROM_PREMIUM_JOB - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 24000000;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_PREMIUM_JOB - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	MetricEarnFromJob m(amount, uniqueMatchId);
	if(!EarnCash(amount, false, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS")))
	{
		scriptErrorf("%s: NETWORK_EARN_FROM_PREMIUM_JOB - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

static bool s_clearAllMatchHistoryId = false;
void EarnMoneyFromJobBonuses(int amount, const char* uniqueMatchId, const char* challenge)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_FROM_JOB_BONUS ", true);)

	scriptAssertf(amount>0, "%s: NETWORK_EARN_FROM_JOB_BONUS  - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	if (!CAN_SCRIPT_CHANGE_CASH("EarnCash") && CNetworkTelemetry::GetMatchHistoryId() == 0)
	{
		s_clearAllMatchHistoryId = true;
		CNetworkTelemetry::SetPreviousMatchHistoryId(OUTPUT_ONLY(__FUNCTION__));
	}

	MetricEarnFromJobBonuses m(amount, uniqueMatchId, challenge);
	if(!EarnCash(amount, true, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS"), ATSTRINGHASH("MONEY_EARN_FROM_JOB_BONUS", 0xad06c80b)))
	{
		scriptErrorf("%s: NETWORK_EARN_FROM_JOB_BONUS - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void EarnMoneyFromGangOps(int amount, const char* uniqueMatchId, int challenge, const char* category)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_FROM_GANGOPS", true);)

		scriptAssertf(amount>0, "%s: NETWORK_EARN_FROM_GANGOPS  - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	if (!CAN_SCRIPT_CHANGE_CASH("EarnCash") && CNetworkTelemetry::GetMatchHistoryId() == 0)
	{
		s_clearAllMatchHistoryId = true;
		CNetworkTelemetry::SetPreviousMatchHistoryId(OUTPUT_ONLY(__FUNCTION__));
	}

	MetricEarnFromGangOps m(amount, uniqueMatchId, challenge, category);
	if(!EarnCash(amount, true, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS"), ATSTRINGHASH("MONEY_EARN_FROM_JOB_BONUS", 0xad06c80b)))
	{
		scriptErrorf("%s: NETWORK_EARN_FROM_GANGOPS - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void EarnMoneyFromJobBonusesMain(int amount, const char* uniqueMatchId, const char* challenge)
{
	static const int AMOUNT_LIMIT = 200000;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_JOB_BONUS - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	EarnMoneyFromJobBonuses(amount, uniqueMatchId, challenge);
}

void EarnMoneyFromJobBonusesCriminalMind(int amount, const char* uniqueMatchId, const char* challenge)
{
	static const int AMOUNT_LIMIT = 30000000;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_CRIMINAL_MASTERMIND - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	EarnMoneyFromJobBonuses(amount, uniqueMatchId, challenge);
}

void EarnMoneyFromJobBonusesHeistAward(int amount, const char* uniqueMatchId, const char* challenge)
{
	static const int AMOUNT_LIMIT = 4000000;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_HEIST_AWARD - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	EarnMoneyFromJobBonuses(amount, uniqueMatchId, challenge);
}

void EarnMoneyFromJobBonusesFirstTimeBonus(int amount, const char* uniqueMatchId, const char* challenge)
{
	static const int AMOUNT_LIMIT = 400000;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_FIRST_TIME_BONUS - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	EarnMoneyFromJobBonuses(amount, uniqueMatchId, challenge);
}

void EarnMoneyFromBendJob(int amount, const char* uniqueMatchId)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_FROM_BEND_JOB", true);)

	scriptAssertf(amount>0, "%s: NETWORK_EARN_FROM_BEND_JOB - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 24000000;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_BEND_JOB - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	if (!CAN_SCRIPT_CHANGE_CASH("EarnCash") && CNetworkTelemetry::GetMatchHistoryId() == 0)
	{
		s_clearAllMatchHistoryId = true;
		CNetworkTelemetry::SetPreviousMatchHistoryId(OUTPUT_ONLY(__FUNCTION__));
	}

	MetricEarnFromJob m(amount, uniqueMatchId);
	if(!EarnCash(amount, true, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS"), ATSTRINGHASH("MONEY_EARN_FROM_HEIST_JOB", 0xadda0055)))
	{
		scriptErrorf("%s: NETWORK_EARN_FROM_BEND_JOB - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	if (!CAN_SCRIPT_CHANGE_CASH("EarnCash") && s_clearAllMatchHistoryId)
	{
		s_clearAllMatchHistoryId = false;
		rlTelemetry::TryForceFlushImmediate();
		CNetworkTelemetry::ClearAllMatchHistoryId();
	}
}

void EarnMoneyFromChallengeWin(int amount, const char* uniquePlaylistId, bool headToHead)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_FROM_CHALLENGE_WIN", true);)

	scriptAssertf(amount>0, "%s: NETWORK_EARN_FROM_CHALLENGE_WIN - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 1000000;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_CHALLENGE_WIN - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	MetricEarnFromChallengeWin m(amount, uniquePlaylistId, headToHead);
	if(!EarnCash(amount, false, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS")))
	{
		scriptErrorf("%s: NETWORK_EARN_FROM_CHALLENGE_WIN - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void EarnMoneyFromBounty(int amount, int& hPlaced, int& hTarget, int flags)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_FROM_BOUNTY", true);)

	scriptAssertf(amount>0, "%s: NETWORK_EARN_FROM_BOUNTY - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 60000;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_BOUNTY - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	rlGamerHandle hGamerPlaced;
	if (IsHandleValid(hPlaced, SCRIPT_GAMER_HANDLE_SIZE))
	{
		CTheScripts::ImportGamerHandle(&hGamerPlaced, hPlaced, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_EARN_FROM_BOUNTY"));
	}

	rlGamerHandle hGamerTarget;
	if (IsHandleValid(hTarget, SCRIPT_GAMER_HANDLE_SIZE))
	{
		CTheScripts::ImportGamerHandle(&hGamerTarget, hTarget, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_EARN_FROM_BOUNTY"));
	}

	MetricEarnFromBounty m(amount, hGamerPlaced, hGamerTarget, flags);
	if(!EarnCash(amount, false, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS")))
	{
		scriptErrorf("%s: NETWORK_EARN_FROM_BOUNTY - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void EarnMoneyFromImportExport(int amount, int modelHash)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_FROM_IMPORT_EXPORT", true);)

	scriptAssertf(amount>0, "%s: NETWORK_EARN_FROM_IMPORT_EXPORT - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 130000;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_IMPORT_EXPORT - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	CBaseModelInfo* mi = CModelInfo::GetBaseModelInfoFromHashKey((u32) modelHash, NULL);
	if(SCRIPT_VERIFY(mi, "NETWORK_EARN_FROM_IMPORT_EXPORT - model doesn't exist"))
	{
		MetricEarnFromImportExport m(amount, (u32)modelHash);
		if(!EarnCash(amount, false, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS")))
		{
			scriptErrorf("%s: NETWORK_EARN_FROM_IMPORT_EXPORT - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
}

void EarnMoneyFromHoldups(int amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_FROM_HOLDUPS", true);)

	scriptAssertf(amount>0, "%s: NETWORK_EARN_FROM_HOLDUPS - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 10000;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_HOLDUPS - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	MetricEarnFromHoldups m(amount);
	if(!EarnCash(amount, false, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS")))
	{
		scriptErrorf("%s: NETWORK_EARN_FROM_HOLDUPS - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void EarnMoneyFromProperty(int amount, int propertyType)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_FROM_PROPERTY", true);)

	static const int AMOUNT_LIMIT = 2500000;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_PROPERTY - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	MetricEarnFromProperty m(amount, propertyType);
	if(EarnCash(amount, true, false, &m, StatId()))
	{
		CStatsMgr::GetStatsDataMgr().GetSavesMgr().ClearPendingFlushRequests( );
	}
	else
	{
		scriptErrorf("%s: NETWORK_EARN_FROM_PROPERTY - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

#if !__FINAL
void EarnMoneyFromDebug(int amount, bool toBank)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_FROM_DEBUG", true);)

	scriptAssertf(amount>0, "%s: NETWORK_EARN_FROM_DEBUG - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;
	
	MetricEarnFromDebug m(amount, toBank);
	if(!EarnCash(amount, toBank, false, &m, StatId()))
	{
		scriptErrorf("%s: NETWORK_EARN_FROM_DEBUG - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}
#endif // !__FINAL

void EarnMoneyFromAiTargetKill(int amount, int pedhash)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_FROM_AI_TARGET_KILL", true);)

	scriptAssertf(amount>0, "%s: NETWORK_EARN_FROM_AI_TARGET_KILL - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 25000;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_AI_TARGET_KILL - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	MetricEarnFromAiTargetKill m(amount, pedhash);
	if(!EarnCash(amount, false, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS")))
	{
		scriptErrorf("%s: NETWORK_EARN_FROM_AI_TARGET_KILL - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void EarnMoneyFromNotBadSport(int amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_FROM_NOT_BADSPORT", true);)

	scriptAssertf(amount>0, "%s: NETWORK_EARN_FROM_NOT_BADSPORT - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 10000;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_NOT_BADSPORT - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	MetricEarnFromNotBadSport m(amount);
	if(!EarnCash(amount, false, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_GOOD_SPORT")))
	{
		scriptErrorf("%s: NETWORK_EARN_FROM_NOT_BADSPORT - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void EarnMoneyFromRockstar(int amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_FROM_ROCKSTAR", true);)

	scriptAssertf(amount>0, "%s: NETWORK_EARN_FROM_ROCKSTAR - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 10000000;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_ROCKSTAR - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	MetricEarnFromRockstar m(amount);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_ROCKSTAR_AWARD");
	const int itemId = ATSTRINGHASH("MONEY_EARN_ROCKSTAR_AWARD", 0xcc109b55);
	if(!EarnCash(amount, true, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_EARN_FROM_ROCKSTAR - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void EarnMoneyFromRockstar64(s64 amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_FROM_ROCKSTAR", true);)

	scriptAssertf(amount>0, "%s: NETWORK_EARN_FROM_ROCKSTAR - invalid money amount < %" I64FMT "d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	MetricEarnFromRockstar m(amount);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_ROCKSTAR_AWARD");
	const int itemId = ATSTRINGHASH("MONEY_EARN_ROCKSTAR_AWARD", 0xcc109b55);
	if(!EarnCash(amount, true, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_EARN_FROM_ROCKSTAR - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

/////////////////////////////////////////////////////////////////////////////// TELEM_SAVETYPE

class MetricCHIPS_FROM_R : public MetricPlayStat
{
	RL_DECLARE_METRIC(CHIPS_FROM_R, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

public:
	MetricCHIPS_FROM_R(const s64 chips) : m_chips(chips) { ; }
	
	virtual bool Write(RsonWriter* rw) const {
		return MetricPlayStat::Write(rw) && rw->WriteInt64("a", m_chips);
	}

	s64 m_chips;
};

s64 EarnChipsFromRockstar(s64 chips)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("EARN_CHIPS_FROM_ROCKSTAR", true);)

	scriptAssertf(chips > 0, "%s: EARN_CHIPS_FROM_ROCKSTAR - invalid chips amount < %" I64FMT "d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), chips);
	if (chips <= 0)
		return 0;

	MetricCHIPS_FROM_R m(chips);
	APPEND_METRIC(m);

#if __NET_SHOP_ACTIVE
	if (NETWORK_SHOPPING_MGR.ShouldDoNullTransaction())
	{
		StatsInterface::IncrementStat(STAT_CASINO_CHIPS, (float)chips);
	}
#elif (!RSG_PC)
	StatsInterface::IncrementStat(STAT_CASINO_CHIPS, (float)chips);
#endif // __NET_SHOP_ACTIVE

	return chips;
}

s64 SpentChipsFromRockstar(const s64 chips)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("SPEND_CHIPS_FROM_ROCKSTAR", true);)

	scriptAssertf(chips > 0, "%s: SPEND_CHIPS_FROM_ROCKSTAR - invalid chips amount < %" I64FMT "d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), chips);
	if (chips <= 0)
		return 0;

	const s64 decrement = Min((s64)StatsInterface::GetIntStat(STAT_CASINO_CHIPS), chips);

	MetricCHIPS_FROM_R m(decrement * -1);
	APPEND_METRIC(m);

#if __NET_SHOP_ACTIVE

	if (NETWORK_SHOPPING_MGR.ShouldDoNullTransaction())
	{
		StatsInterface::DecrementStat(STAT_CASINO_CHIPS, (float)decrement);
	}

#elif (!RSG_PC)

	StatsInterface::DecrementStat(STAT_CASINO_CHIPS, (float)decrement);

#endif // __NET_SHOP_ACTIVE

	return decrement;
}

void EarnMoneyFromSellingVehicle(int amount, int modelHash, int oldLevel=0, int newLevel=0, bool isOffender1=false, bool isOffender2=false, int oldThresholdLevel=0, int newThresholdLevel=0)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_FROM_VEHICLE", true);)

	scriptAssertf(amount>0, "%s: NETWORK_EARN_FROM_VEHICLE - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 1000000;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_VEHICLE - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

#if __ASSERT
	fwModelId mid;
	CModelInfo::GetBaseModelInfoFromHashKey((u32) modelHash, &mid);
	scriptAssertf(mid.IsValid(), "%s: NETWORK_EARN_FROM_VEHICLE - invalid vehicle model hash < %d >.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), modelHash);
	if (mid.IsValid())
	{
		scriptAssertf(CModelInfo::GetBaseModelInfo(mid), "%s:NETWORK_EARN_FROM_VEHICLE - Specified model < %d > isn't valid!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), modelHash);
	}
#endif

	MetricEarnFromVehicle m(amount, modelHash, false, oldLevel, newLevel, isOffender1, isOffender2, 0, oldThresholdLevel, newThresholdLevel);
	if(EarnCash(amount, false, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_SELLING_VEH")))
	{
		CStatsMgr::GetStatsDataMgr().GetSavesMgr().ClearPendingFlushRequests();
	}
	else
	{
		scriptErrorf("%s: NETWORK_EARN_FROM_VEHICLE - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

enum SellingVehicleType
{
	SVT_EARN_FROM_VEHICLE = 0,
	SVT_EARN_FROM_PERSONAL_NORMAL_VEHICLE = 1,
	SVT_EARN_FROM_PERSONAL_HIGHEND_VEHICLE = 2,
	SVT_EARN_FROM_PERSONAL_WEAPON_VEHICLE = 3,
	SVT_EARN_FROM_PERSONAL_AIRCRAFT_VEHICLE = 4,
	SVT_EARN_FROM_H2_WEAPONIZED_VEHICLE = 5,
	SVT_EARN_FROM_BATTLES_WEAPONIZED_VEHICLE = 6
};

void EarnMoneyFromSellingPersonalVehicle(int amount, int modelHash, int oldLevel=0, int newLevel=0, bool isOffender1=false, bool isOffender2=false, int vehicleType=1, int oldThresholdLevel=0, int newThresholdLevel=0)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_FROM_PERSONAL_VEHICLE", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_EARN_FROM_PERSONAL_VEHICLE - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;
	if (vehicleType == SVT_EARN_FROM_VEHICLE)
	{
		scriptAssertf(0, "%s: NETWORK_EARN_FROM_PERSONAL_VEHICLE - invalid vehicle type.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return;
	}
	if(vehicleType==SVT_EARN_FROM_PERSONAL_NORMAL_VEHICLE)
	{
		static const int NORMAL_AMOUNT_LIMIT = 1000000;
		scriptAssertf(amount<=NORMAL_AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_PERSONAL_VEHICLE - invalid money amount < %d > for normal vehicle, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, NORMAL_AMOUNT_LIMIT);
		if (amount>NORMAL_AMOUNT_LIMIT)
			return;
	}
	if(vehicleType==SVT_EARN_FROM_PERSONAL_HIGHEND_VEHICLE)
	{
		static const int HIGH_END_AMOUNT_LIMIT = 2500000;
		scriptAssertf(amount<=HIGH_END_AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_PERSONAL_VEHICLE - invalid money amount < %d > for high end vehicle, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, HIGH_END_AMOUNT_LIMIT);
		if (amount>HIGH_END_AMOUNT_LIMIT)
			return;
	}
	if(vehicleType==SVT_EARN_FROM_PERSONAL_WEAPON_VEHICLE)
	{
		static const int WEAPON_AMOUNT_LIMIT = 2600000;
		scriptAssertf(amount<=WEAPON_AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_PERSONAL_VEHICLE - invalid money amount < %d > for high end vehicle, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, WEAPON_AMOUNT_LIMIT);
		if (amount>WEAPON_AMOUNT_LIMIT)
			return;
	}
	if (vehicleType == SVT_EARN_FROM_PERSONAL_AIRCRAFT_VEHICLE)
	{
		static const int WEAPON_AMOUNT_LIMIT = 4250000;
		scriptAssertf(amount <= WEAPON_AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_PERSONAL_VEHICLE - invalid money amount < %d > for high end vehicle, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, WEAPON_AMOUNT_LIMIT);
		if (amount > WEAPON_AMOUNT_LIMIT)
			return;
	}
	if (vehicleType == SVT_EARN_FROM_H2_WEAPONIZED_VEHICLE)
	{
		static const int WEAPON_AMOUNT_LIMIT = 3250000;
		scriptAssertf(amount <= WEAPON_AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_PERSONAL_VEHICLE - invalid money amount < %d > for high end vehicle, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, WEAPON_AMOUNT_LIMIT);
		if (amount > WEAPON_AMOUNT_LIMIT)
			return;
	}
	if (vehicleType == SVT_EARN_FROM_BATTLES_WEAPONIZED_VEHICLE)
	{
		static const int WEAPON_AMOUNT_LIMIT = 3800000;
		scriptAssertf(amount <= WEAPON_AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_PERSONAL_VEHICLE - invalid money amount < %d > for high end vehicle, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, WEAPON_AMOUNT_LIMIT);
		if (amount > WEAPON_AMOUNT_LIMIT)
			return;
	}

#if __ASSERT
	fwModelId mid;
	CModelInfo::GetBaseModelInfoFromHashKey((u32) modelHash, &mid);
	scriptAssertf(mid.IsValid(), "%s: NETWORK_EARN_FROM_PERSONAL_VEHICLE - invalid vehicle model hash < %d >.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), modelHash);
	if (mid.IsValid())
	{
		scriptAssertf(CModelInfo::GetBaseModelInfo(mid), "%s:NETWORK_EARN_FROM_PERSONAL_VEHICLE - Specified model < %d > isn't valid!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), modelHash);
	}
#endif

	MetricEarnFromVehicle m(amount, modelHash, true, oldLevel, newLevel, isOffender1, isOffender2, vehicleType, oldThresholdLevel, newThresholdLevel);
	if(EarnCash(amount, false, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_SELLING_VEH")))
	{
		CStatsMgr::GetStatsDataMgr().GetSavesMgr().ClearPendingFlushRequests();
	}
	else
	{
		scriptErrorf("%s: NETWORK_EARN_FROM_PERSONAL_VEHICLE - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void EarnFromDailyObjectives(int amount, const char* description, int objective)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_FROM_DAILY_OBJECTIVES", true);)

	scriptAssertf(amount>0, "%s: NETWORK_EARN_FROM_DAILY_OBJECTIVES - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 2000000;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_DAILY_OBJECTIVES - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	MetricEarnFromDailyObjective m(amount, description, objective);
	if(!EarnCash(amount, true, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_DAILY_OBJECTIVE"), ATSTRINGHASH("MONEY_EARN_FROM_JOB_BONUS", 0xad06c80b)))
	{
		scriptErrorf("%s: NETWORK_EARN_FROM_DAILY_OBJECTIVES - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

static const sAmountCapTypes s_AmbientJob[] =
{
	AMOUNTCAPTYPE("AM_PLANE_TAKEDOWN", 800000),
	AMOUNTCAPTYPE("AM_DISTRACT_COPS", 320000),
	AMOUNTCAPTYPE("AM_DESTROY_VEH", 160000),
	AMOUNTCAPTYPE("AM_HOT_TARGET_DELIVER", 640000),
	AMOUNTCAPTYPE("AM_HOT_TARGET_KILL", 640000),
	AMOUNTCAPTYPE("AM_KILL_LIST", 800000),
	AMOUNTCAPTYPE("AM_CP_COLLECTION", 240000),
	AMOUNTCAPTYPE("AM_TIME_TRIAL", 1600000),
	AMOUNTCAPTYPE("AM_CHALLENGES", 800000),
	AMOUNTCAPTYPE("AM_HOT_TARGET_HELI", 640000),
	AMOUNTCAPTYPE("AM_DEAD_DROP", 640000),
	AMOUNTCAPTYPE("AM_PENNED_IN", 960000),
	AMOUNTCAPTYPE("AM_PASS_THE_PARCEL", 640000),
	AMOUNTCAPTYPE("AM_CRIMINAL_DAMAGE", 2400000),
	AMOUNTCAPTYPE("AM_HOT_PROPERTY", 800000),
	AMOUNTCAPTYPE("AM_KING_OF_THE_HILL", 960000),
	AMOUNTCAPTYPE("AM_HUNT_THE_BEAST", 1200000),
};
const int NUMBER_OF_AMBIENTJOB_TYPES = sizeof(s_AmbientJob) / sizeof(s_AmbientJob[0]);
void EarnFromAmbientJob(int amount, const char* description, srcAmbientJobdata* data)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_FROM_AMBIENT_JOB", true);)

	scriptAssertf(description, "%s: NETWORK_EARN_FROM_AMBIENT_JOB - NULL jobdescription.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	scriptAssertf(data, "%s: NETWORK_EARN_FROM_AMBIENT_JOB - NULL data.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	scriptAssertf(amount>0, "%s: NETWORK_EARN_FROM_AMBIENT_JOB - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	atHashString typehash(description);
	bool found = false;
	for(int i=0; i<NUMBER_OF_AMBIENTJOB_TYPES && !found; i++)
	{
		found = (typehash == s_AmbientJob[i].m_type);
		if(found)
		{
			if (s_AmbientJob[i].m_amount < amount)
			{
				scriptAssertf(0, "%s: NETWORK_EARN_FROM_AMBIENT_JOB - invalid money amount < %d > for type %s, max value is '%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, description, s_AmbientJob[i].m_amount);
				return;
			}
			break;
		}
	}

	//Go for the default MAX
	if (!found)
	{
		static const int AMOUNT_LIMIT = 1600000;
		scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_AMBIENT_JOB - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
		if (amount > AMOUNT_LIMIT)
			return;
	}

	MetricEarnFromAmbientJob::sData info;
	int idx = 0;
	info.m_values[idx] = data->m_v1.Int; idx++; gnetAssert(idx<=MetricEarnFromAmbientJob::MAX_VALUES);
	info.m_values[idx] = data->m_v2.Int; idx++; gnetAssert(idx<=MetricEarnFromAmbientJob::MAX_VALUES);
	info.m_values[idx] = data->m_v3.Int; idx++;
	info.m_values[idx] = data->m_v4.Int; idx++;

	MetricEarnFromAmbientJob m(amount, description, info);
	if(!EarnCash(amount, false, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS")))
	{
		scriptErrorf("%s: NETWORK_EARN_FROM_AMBIENT_JOB - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnPurchaseClubHouse(int amount, srcClubHouseData* data)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_PURCHASE_CLUB_HOUSE", true););

	scriptAssertf(amount>=0, "%s: NETWORK_EARN_PURCHASE_CLUB_HOUSE - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	static const int AMOUNT_LIMIT = 675000;
	scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_PURCHASE_CLUB_HOUSE - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount > AMOUNT_LIMIT)
		return;

	scriptAssertf(data, "%s: NETWORK_EARN_PURCHASE_CLUB_HOUSE - NULL srcClubHouseData.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!data)
		return;

	MetricEarnPurchaseClubHouse metric( amount );
	metric.m_b = data->m_mural_type.Int;
	metric.m_c = data->m_wall_style.Int;
	metric.m_d = data->m_wall_hanging_style.Int;
	metric.m_e = data->m_furniture_style.Int;
	metric.m_f = data->m_emblem.Int;
	metric.m_g = data->m_gun_locker.Int;
	metric.m_h = data->m_mod_shop.Int;
	metric.m_id = data->m_property_id.Int;
	metric.m_j = data->m_signage.Int;

	if(!EarnCash(amount, true, false, &metric, StatId()))
	{
		scriptErrorf("%s: NETWORK_EARN_PURCHASE_CLUB_HOUSE - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnFromBusinessProduct(int amount, int businessID, int businessType, int quantity)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_FROM_BUSINESS_PRODUCT", true););

	scriptAssertf(amount>0, "%s: NETWORK_EARN_FROM_BUSINESS_PRODUCT - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 43200000;
	scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_BUSINESS_PRODUCT - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount > AMOUNT_LIMIT)
		return;

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_BIKER_BUSINESS");
	MetricEarnFromBusinessProduct m( amount, businessID, businessType, quantity );
	if(!EarnCash(amount, true, false, &m, statId))
	{
		scriptErrorf("%s: NETWORK_EARN_FROM_BUSINESS_PRODUCT - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnFromVehicleExport(int amount, int bossID1, int bossID2)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_FROM_VEHICLE_EXPORT", true););

	scriptAssertf(amount>0, "%s: NETWORK_EARN_FROM_VEHICLE_EXPORT - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 3600000;
	scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_VEHICLE_EXPORT - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount > AMOUNT_LIMIT)
		return;

	const u64 bid0 = ((u64)bossID1) << 32;
	const u64 bid1 = (0x00000000FFFFFFFF&(u64)bossID2);
	const u64 bossId = bid0|bid1;

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_VEHICLE_EXPORT");
	MetricEarnFromVehicleExport m( amount, (s64)bossId );
	if(!EarnCash(amount, true, false, &m, statId))
	{
		scriptErrorf("%s: NETWORK_EARN_FROM_VEHICLE_EXPORT - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnFromBountyHunterReward(int amount)
{
	NOTFINAL_ONLY( VERIFY_STATS_LOADED("NETWORK_EARN_BOUNTY_HUNTER_REWARD", true); );

	scriptAssertf(amount>0, "%s: NETWORK_EARN_BOUNTY_HUNTER_REWARD - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 240000;
	scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_BOUNTY_HUNTER_REWARD - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount > AMOUNT_LIMIT)
		return;

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	MetricEarnFromBountyHunterReward m( amount );
	if(!EarnCash(amount, true, false, &m, statId))
	{
		scriptErrorf("%s: NETWORK_EARN_BOUNTY_HUNTER_REWARD - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}
void CommandEarnFromBusinessBattle(int amount)
{
	NOTFINAL_ONLY( VERIFY_STATS_LOADED("NETWORK_EARN_FROM_BUSINESS_BATTLE", true); );

	scriptAssertf(amount>=0, "%s: NETWORK_EARN_FROM_BUSINESS_BATTLE - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	static const int AMOUNT_LIMIT = 1600000;
	scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_BUSINESS_BATTLE - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount > AMOUNT_LIMIT)
		return;

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	MetricEarnFromBusinessBattle m( amount );
	if(!EarnCash(amount, true, false, &m, statId))
	{
		scriptErrorf("%s: NETWORK_EARN_FROM_BUSINESS_BATTLE - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}
void CommandEarnFromBusinessHubSell(int amount, int nightclubID,  int quantitySold)
{
	NOTFINAL_ONLY( VERIFY_STATS_LOADED("NETWORK_EARN_FROM_BUSINESS_HUB_SELL", true); );

	scriptAssertf(amount>0, "%s: NETWORK_EARN_FROM_BUSINESS_HUB_SELL - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 64000000;
	scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_BUSINESS_HUB_SELL - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount > AMOUNT_LIMIT)
		return;

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_SELL_NC_GOODS");
	MetricEarnFromBusinessHubSell m( amount, nightclubID,  quantitySold );
	if(!EarnCash(amount, true, false, &m, statId))
	{
		scriptErrorf("%s: NETWORK_EARN_FROM_BUSINESS_HUB_SELL - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}
void CommandEarnFromClubManagementParticipation(int amount, int missionId)
{
	NOTFINAL_ONLY( VERIFY_STATS_LOADED("NETWORK_EARN_FROM_CLUB_MANAGEMENT_PARTICIPATION", true); );

	scriptAssertf(amount>0, "%s: NETWORK_EARN_FROM_CLUB_MANAGEMENT_PARTICIPATION - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 1600000;
	scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_CLUB_MANAGEMENT_PARTICIPATION - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount > AMOUNT_LIMIT)
		return;

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	MetricEarnFromClubManagementParticipation m( amount, missionId);
	if(!EarnCash(amount, true, false, &m, statId))
	{
		scriptErrorf("%s: NETWORK_EARN_FROM_CLUB_MANAGEMENT_PARTICIPATION - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}
void CommandEarnFromFMBBPhonecallMission(int amount)
{
	NOTFINAL_ONLY( VERIFY_STATS_LOADED("NETWORK_EARN_FROM_FMBB_PHONECALL_MISSION", true); );

	scriptAssertf(amount>0, "%s: NETWORK_EARN_FROM_FMBB_PHONECALL_MISSION - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 1600000;
	scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_FMBB_PHONECALL_MISSION - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount > AMOUNT_LIMIT)
		return;

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	MetricEarnFromFMBBPhonecallMission m( amount );
	if(!EarnCash(amount, true, false, &m, statId))
	{
		scriptErrorf("%s: NETWORK_EARN_FROM_FMBB_PHONECALL_MISSION - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}
void CommandEarnFromFMBBBossWork(int amount)
{
	NOTFINAL_ONLY( VERIFY_STATS_LOADED("NETWORK_EARN_FROM_FMBB_BOSS_WORK", true); );

	scriptAssertf(amount>0, "%s: NETWORK_EARN_FROM_FMBB_BOSS_WORK - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 1600000;
	scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_FMBB_BOSS_WORK - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount > AMOUNT_LIMIT)
		return;

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	MetricEarnFromFMBBBossWork m( amount );
	if(!EarnCash(amount, false, false, &m, statId))
	{
		scriptErrorf("%s: NETWORK_EARN_FROM_FMBB_BOSS_WORK - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}
void CommandEarnFromFMBBWageBonus(int amount)
{
	NOTFINAL_ONLY( VERIFY_STATS_LOADED("NETWORK_EARN_FMBB_WAGE_BONUS", true); );

	scriptAssertf(amount>0, "%s: NETWORK_EARN_FMBB_WAGE_BONUS - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 1600000;
	scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_FMBB_WAGE_BONUS - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount > AMOUNT_LIMIT)
		return;

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	MetricEarnFromFMBBWageBonus m( amount );
	if(!EarnCash(amount, true, false, &m, statId))
	{
		scriptErrorf("%s: NETWORK_EARN_FMBB_WAGE_BONUS - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

static const sAmountCapTypes s_RefundTypes[] =
{
	AMOUNTCAPTYPE("DEFAULT", 5000),
	AMOUNTCAPTYPE("BACKUP_VAGOS", 4500),
	AMOUNTCAPTYPE("BACKUP_LOST", 4500),
	AMOUNTCAPTYPE("BACKUP_FAMILIES", 4500),
	AMOUNTCAPTYPE("HIRE_MUGGER", 3000),
	AMOUNTCAPTYPE("HIRE_MERCENARY", 22500),
	AMOUNTCAPTYPE("BUY_CARDROPOFF", 1500),
	AMOUNTCAPTYPE("HELI_PICKUP", 10000),
	AMOUNTCAPTYPE("BOAT_PICKUP", 750),
	AMOUNTCAPTYPE("CLEAR_WANTED", 3000),
	AMOUNTCAPTYPE("HEAD_2_HEAD", 30000),
	AMOUNTCAPTYPE("CHALLENGE", 500000),
	AMOUNTCAPTYPE("SHARE_LAST_JOB", 120000),
	AMOUNTCAPTYPE("SPEND_FAIL", 1000000),
	AMOUNTCAPTYPE("REFUNDAPPEA", 100000),
	AMOUNTCAPTYPE("ORBITAL_MANUAL", 500000),
	AMOUNTCAPTYPE("ORBITAL_AUTO", 750000),
	AMOUNTCAPTYPE("AMMO_DROP_REF", 5000),
	AMOUNTCAPTYPE("ARENA_SPEC_BOX", 100),
};
const int NUMBER_OF_REFUND_TYPES = sizeof(s_RefundTypes) / sizeof(s_RefundTypes[0]);
void RefundCash(int amount, const char* type, const char* reason, bool fromBank)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_REFUND_CASH", true);)

	scriptAssertf(amount>0, "%s: NETWORK_REFUND_CASH - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	scriptAssertf(type, "%s: NETWORK_REFUND_CASH - type is NULL.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!type)
		return;

	scriptAssertf(reason, "%s: NETWORK_REFUND_CASH - reason is NULL.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!reason)
		return;

	atHashString typehash(type);

	bool found = false;
	for(int i=0; i<NUMBER_OF_REFUND_TYPES && !found; i++)
	{
		found = (typehash == s_RefundTypes[i].m_type);
		if(found)
		{
			if (s_RefundTypes[i].m_amount < amount)
			{
				scriptAssertf(0, "%s: NETWORK_REFUND_CASH - invalid money amount < %d > for type %s, max value is '%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, type, s_RefundTypes[i].m_amount);
				return;
			}

			break;
		}
	}
	scriptAssertf(found, "%s: NETWORK_REFUND_CASH - Unknown refund cash type < %s >.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), type);

	MetricEarnRefund m(amount, type, reason, fromBank);
	const int itemId = ATSTRINGHASH("MONEY_EARN_REFUND", 0x267d85b3);
	if(!EarnCash(amount, fromBank, false, &m, StatId(), itemId))
	{
		scriptErrorf("%s: NETWORK_REFUND_CASH - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

struct  sCashDeductTypes
{
public:
	sCashDeductTypes(const char* type, const int amount) : m_type(type), m_amount(amount){}
	atHashString m_type;
	int m_amount;
};
#define DEDUCTTYPE(type, amount) sCashDeductTypes(type, amount)
static const sCashDeductTypes s_DeductTypes[] =
{
	DEDUCTTYPE("DEFAULT", 5000),
	DEDUCTTYPE("EARN_FAIL", 1000000),
};
const int NUMBER_OF_DEDUCT_TYPES = sizeof(s_DeductTypes) / sizeof(s_DeductTypes[0]);

void DeductCash(int amount, const char* type, const char* reason, bool fromBank, bool fromBankAndWallet, bool UNUSED_PARAM(fromWalletAndBank))
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_DEDUCT_CASH", true);)

	scriptAssertf(amount>0, "%s: NETWORK_DEDUCT_CASH - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	scriptAssertf(type, "%s: NETWORK_DEDUCT_CASH - type is NULL.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!type)
		return;

	scriptAssertf(reason, "%s: NETWORK_DEDUCT_CASH - reason is NULL.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!reason)
		return;

	atHashString typehash(type);

	bool found = false;
	for(int i=0; i<NUMBER_OF_DEDUCT_TYPES && !found; i++)
	{
		found = (typehash == s_DeductTypes[i].m_type);
		if(found)
		{
			if (s_DeductTypes[i].m_amount < amount)
			{
				scriptAssertf(0, "%s: NETWORK_DEDUCT_CASH - invalid money amount < %d > for type %s, max value is '%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, type, s_DeductTypes[i].m_amount);
				return;
			}

			break;
		}
	}
	scriptAssertf(found, "%s: NETWORK_DEDUCT_CASH - Unknown Deduct cash type < %s >.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), type);

	MetricSpendDeduct m(amount, type, reason, fromBank);
	const int itemId = ATSTRINGHASH("MONEY_SPEND_DEDUCT", 0xffb3c266);
	if(!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, StatId(), itemId))
	{
		scriptErrorf("%s: NETWORK_DEDUCT_CASH - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

//////////////////////////////////////////////////////////////////////////

//
// Spend events
//

bool CanSpendGtaDollars(const int amount, const bool fromBank, const bool fromBankAndWallet, const bool fromWalletAndBank, int& deficit, const int character, const bool evcOnly OUTPUT_ONLY(, const char* commandname))
{
	deficit = 0;

	bool result = false;

#if __ASSERT
	if (fromBank)
		scriptAssertf(!fromBankAndWallet && !fromWalletAndBank, "%s: %s - Make sure only one is true: fromBank, fromBankAndWallet, fromWalletAndBank.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandname);
	if (fromBankAndWallet || fromWalletAndBank)
		scriptAssertf(!fromBank, "%s: %s - Make sure only one is true: fromBank, fromBankAndWallet, fromWalletAndBank.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandname);
#endif // __ASSERT

	scriptAssertf(NetworkInterface::IsGameInProgress(), "%s: %s - FAIL - Can't access network player cash while in single player.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandname);
	if (!NetworkInterface::IsGameInProgress())
	{
		scriptErrorf("%s: %s - FAIL - amount='%d', Network Game is not in progress", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandname, amount);
		return result;
	}

	scriptAssertf(NetworkInterface::IsInFreeMode() || NetworkInterface::IsInCopsAndCrooks() || NetworkInterface::IsInFakeMultiplayerMode(), "%s: %s - Not in a valid game mode (GameMode: %u)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandname, NetworkInterface::GetNetworkGameMode());
	if (!(NetworkInterface::IsInFreeMode() || NetworkInterface::IsInCopsAndCrooks() || NetworkInterface::IsInFakeMultiplayerMode()))
	{
		scriptErrorf("%s: %s - FAIL - amount='%d', Not in a valid game mode (GameMode: %u)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandname, amount, NetworkInterface::GetNetworkGameMode());
		return result;
	}

#if __NET_SHOP_ACTIVE
	//Total amount of Wallet cash that is pending removal.
	s64 walletPending = 0;

	//Total amount of Bank cash that is pending removal.
	s64 bankPending = 0;

	//Total amount of Virtual cash that is pending removal.
	s64 vcPending = 0;

	//Total amount of Earn Virtual cash that is pending removal.
	s64 evcOnlyPending = 0;

	//If we are not doing NULL transactions grab the pending transactions cash reductions.
	if (!NETWORK_SHOPPING_MGR.ShouldDoNullTransaction())
	{
		 walletPending = NETWORK_SHOPPING_MGR.GetCashReductions().GetTotalWallet();
		   bankPending = NETWORK_SHOPPING_MGR.GetCashReductions().GetTotalBank();
		     vcPending = NETWORK_SHOPPING_MGR.GetCashReductions().GetTotalVc();
		evcOnlyPending = NETWORK_SHOPPING_MGR.GetCashReductions().GetTotalEvc();
	}

#endif // __NET_SHOP_ACTIVE

	s64 totalVC = MoneyInterface::GetVCBalance();
	NET_SHOP_ONLY(totalVC -= vcPending;);
	if (amount > totalVC)
	{
		deficit = (int)((s64)amount - totalVC);
		scriptErrorf("%s: %s - FAIL - amount='%d', Total virtual cash is not enough, vc='%" I64FMT "d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandname, amount, MoneyInterface::GetVCBalance());
		return result;
	}

	if (amount < 0)
	{
		scriptErrorf("%s: %s - FAIL - invalid negative amount='%d'", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandname, amount);
		return result;
	}

	if (fromBankAndWallet || fromWalletAndBank)
	{
		s64 wallet_bank = MoneyInterface::GetVCBankBalance() + MoneyInterface::GetVCWalletBalance(character);
		NET_SHOP_ONLY(wallet_bank -= vcPending;);
		if (amount > wallet_bank)
		{
			deficit = (int)((s64)amount - wallet_bank);
			scriptErrorf("%s: %s - FAIL - amount='%d', NOT ENOUGH CASH, wallet_bank='%" I64FMT "d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandname, amount, wallet_bank);
			return result;
		}
	}
	else if (fromBank)
	{
		s64 bank = MoneyInterface::GetVCBankBalance();
		NET_SHOP_ONLY(bank -= bankPending;);
		if (amount > bank)
		{
			deficit = (int)((s64)amount - (bank));
			scriptErrorf("%s: %s - FAIL - amount='%d', NOT ENOUGH CASH, bank='%" I64FMT "d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandname, amount, bank);
			return result;
		}
	}
	else
	{
		s64 wallet = MoneyInterface::GetVCWalletBalance(character);
		NET_SHOP_ONLY(wallet -= walletPending;);
		if (amount > wallet)
		{
			deficit = (int)((s64)amount - wallet);
			scriptErrorf("%s: %s - FAIL - amount='%d', NOT ENOUGH CASH, wallet='%" I64FMT "d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandname, amount, wallet);
			return result;
		}
	}

	if (MoneyInterface::IsMemoryTampered())
	{
		scriptErrorf("%s: %s - FAIL - amount='%d', MEMORY IS TAMPERED.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandname, amount);
		return result;
	}

	//Check against total EVC.
	if (evcOnly)
	{
		s64 evc = MoneyInterface::GetEVCBalance();
		NET_SHOP_ONLY(evc -= evcOnlyPending;);
		if (amount > evc)
		{
			deficit = (int)((s64)amount - evc);
			scriptErrorf("%s: %s - FAIL - amount='%d', NOT ENOUGH EVC CASH, evc='%" I64FMT "d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandname, amount, evc);
			return result;
		}
	}

	scriptDebugf1("%s: %s - SUCCESS - amount='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandname, amount);

	return true;
}

bool CanSpendMoney(int amount, bool fromBank, bool fromBankAndWallet, bool fromWalletAndBank, int character, bool evcOnly)
{
	int deficit = 0;

	if ( CanSpendGtaDollars(amount, fromBank, fromBankAndWallet, fromWalletAndBank, deficit, character, evcOnly OUTPUT_ONLY(, "NETWORK_CAN_SPEND_MONEY")) )
	{
		scriptDebugf1("%s: NETWORK_CAN_SPEND_MONEY - SUCCESS - amount='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
		return true;
	}

	return false;
}

bool CanSpendMoney2(int amount, bool fromBank, bool fromBankAndWallet, bool fromWalletAndBank, int& deficit, int character, bool evcOnly)
{
	deficit = 0;
	if ( CanSpendGtaDollars(amount, fromBank, fromBankAndWallet, fromWalletAndBank, deficit, character, evcOnly OUTPUT_ONLY(, "NETWORK_CAN_SPEND_MONEY2")) )
	{
		scriptDebugf1("%s: NETWORK_CAN_SPEND_MONEY - SUCCESS - amount='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
		return true;
	}

	return false;
}

void BuyItem(int amount, int itemHash, int itemType, int extra1, bool fromBank, const char* itemIdentifier, int shopNameHash, int extraItemHash, int colorHash, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_BUY_ITEM", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_BUY_ITEM - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	scriptAssertf(itemType>=BUYITEM_PURCHASE_WEAPONS && itemType<=BUYITEM_PURCHASE_MAX, "%s: NETWORK_BUY_ITEM - invalid itemType < %d >, see PURCHASE_TYPE.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), itemType);
	if (itemType<BUYITEM_PURCHASE_WEAPONS || itemType>BUYITEM_PURCHASE_MAX)
		return;

	if (itemIdentifier)
	{
		const u32 size = ustrlen(itemIdentifier);
		scriptAssertf(size<=MetricSpendShop::MAX_STRING_LENGTH, "%s: NETWORK_BUY_ITEM - invalid itemIdentifier size=< %u >, max size is < %u >.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), size, MetricSpendShop::MAX_STRING_LENGTH);

		if (size>MetricSpendShop::MAX_STRING_LENGTH)
			return;
	}

#if __ASSERT
	switch (itemType)
	{
	case BUYITEM_PURCHASE_WEAPONMODS:
	case BUYITEM_PURCHASE_WEAPONAMMO:
	case BUYITEM_PURCHASE_WEAPONS:
		{
			if(SCRIPT_VERIFY(itemHash != 0 , "NETWORK_BUY_ITEM - Weapon hash is invalid"))
			{
				scriptAssertf(CWeaponInfoManager::GetInfo(itemHash), "%s: NETWORK_BUY_ITEM - invalid weapon hash < %d >.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), itemHash);
			}
		}
		break;

	case BUYITEM_PURCHASE_CARWHEELMODS:
	case BUYITEM_PURCHASE_CARPAINT:
	case BUYITEM_PURCHASE_CARMODS:
	case BUYITEM_PURCHASE_CARIMPOUND:
	case BUYITEM_PURCHASE_CARINSURANCE:
	case BUYITEM_PURCHASE_CARREPAIR:
	case BUYITEM_PURCHASE_VEHICLES:
	case BUYITEM_PURCHASE_CARDROPOFF:
	case BUYITEM_PURCHASE_ENDMISSIONCARUP:
	case BUYITEM_PURCHASE_CASHEISTMISVEHUP:
		{
			fwModelId mid;
			CModelInfo::GetBaseModelInfoFromHashKey((u32) itemHash, &mid);
			scriptAssertf(mid.IsValid(), "%s: NETWORK_BUY_ITEM - invalid vehicle model hash < %d >.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), itemHash);
			if (mid.IsValid())
			{
				scriptAssertf(CModelInfo::GetBaseModelInfo(mid), "%s:NETWORK_BUY_ITEM - Specified model < %d > isn't valid!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), itemHash);
			}
		}
		break;

		//Item hash is using the label hash
	case BUYITEM_PURCHASE_ARMOR:
	case BUYITEM_PURCHASE_TATTOOS:
	case BUYITEM_PURCHASE_FOOD:
	case BUYITEM_PURCHASE_BARBERS:
	case BUYITEM_PURCHASE_CLOTHES:
	case BUYITEM_PURCHASE_MASKS:
	case BUYITEM_PURCHASE_FIREWORKS:
	case BUYITEM_PURCHASE_MAX:
		break;
	}
#endif

	bool deferCashTransaction = false;
	switch (itemType)
	{
	case BUYITEM_PURCHASE_VEHICLES:
		deferCashTransaction = true;
		break;

	default:
		break;
	}

	int itemId = -1;
	const char* cashcategory = 0;

	switch (itemType)
	{
	case BUYITEM_PURCHASE_WEAPONMODS:
	case BUYITEM_PURCHASE_ARMOR:
	case BUYITEM_PURCHASE_WEAPONS:
	case BUYITEM_PURCHASE_WEAPONAMMO:
	case BUYITEM_PURCHASE_FIREWORKS:
		cashcategory = "MONEY_SPENT_WEAPON_ARMOR";
		itemId = ATSTRINGHASH("MONEY_SPENT_WEAPON_ARMOR", 0x82aa0d54);
		break;

	case BUYITEM_PURCHASE_CARWHEELMODS:
	case BUYITEM_PURCHASE_CARPAINT:
	case BUYITEM_PURCHASE_CARIMPOUND:
	case BUYITEM_PURCHASE_CARINSURANCE:
	case BUYITEM_PURCHASE_CARREPAIR:
	case BUYITEM_PURCHASE_VEHICLES:
	case BUYITEM_PURCHASE_CARMODS:
	case BUYITEM_PURCHASE_ENDMISSIONCARUP:
	case BUYITEM_PURCHASE_CASHEISTMISVEHUP:
		CNetworkTelemetry::IncrementCloudCashTransactionCount();
		cashcategory = "MONEY_SPENT_VEH_MAINTENANCE";
		itemId = ATSTRINGHASH("MONEY_SPENT_VEH_MAINTENANCE", 0x3c65cc9a);
		CStatsMgr::GetStatsDataMgr().GetSavesMgr().ClearPendingFlushRequests( );
		break;

	case BUYITEM_PURCHASE_CARDROPOFF:
		cashcategory = "MONEY_SPENT_CONTACT_SERVICE";
		itemId = ATSTRINGHASH("MONEY_SPENT_CONTACT_SERVICE", 0x7EE6A4B0);
		break;

	case BUYITEM_PURCHASE_FOOD:
	case BUYITEM_PURCHASE_BARBERS:
	case BUYITEM_PURCHASE_CLOTHES:
	case BUYITEM_PURCHASE_TATTOOS:
	case BUYITEM_PURCHASE_MASKS:
		cashcategory = "MONEY_SPENT_STYLE_ENT";
		itemId = ATSTRINGHASH("MONEY_SPENT_STYLE_ENT", 0x49d67632);
		break;

	case BUYITEM_PURCHASE_MAX:
		break;
	}

	if (scriptVerify(cashcategory))
	{
		if (0 == amount)
		{
			MetricSpendShop m(amount, fromBank, (u32)itemHash, itemType, extra1, itemIdentifier, shopNameHash, extraItemHash, colorHash);
			SendMoneyMetric(m, false);
		}
		else 
		{
			MetricSpendShop m(amount, fromBank, (u32)itemHash, itemType, extra1, itemIdentifier, shopNameHash, extraItemHash, colorHash);
			const StatId statId = StatsInterface::GetStatsModelHashId(cashcategory);
			if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
			{
				scriptErrorf("%s: NETWORK_BUY_ITEM - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				scriptAssertf(0, "%s: NETWORK_BUY_ITEM - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			}
		}
	}
}

void SpentTaxi(int amount, bool fromBank, bool fromWalletAndBank, int npcProvider = 0)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_TAXI", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_TAXI - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_CONTACT_SERVICE", 0x7ee6a4b0);
	MetricSpendTaxis m(amount, fromBank, npcProvider);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_CONTACT_SERVICE");

	if (!SpendCash(amount, fromBank, false, fromWalletAndBank, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_TAXI - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandSpendRehireDJ(int amount, int dj, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_REHIRE_DJ", true);)

		scriptAssertf(amount >= 0, "%s: NETWORK_SPENT_REHIRE_DJ - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount < 0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_PROPERTY_UTIL", 0x8526aff3);
	MetricSpendRehireDJ m(amount, fromBank, dj);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_PROPERTY_UTIL");

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_REHIRE_DJ - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandSpendArenaJoinSpectator(int amount, int entryId, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_ARENA_JOIN_SPECTATOR", true);)

		scriptAssertf(amount >= 0 && amount <= 100, "%s: NETWORK_SPENT_ARENA_JOIN_SPECTATOR - invalid money amount < %d >, must be > 0 and <=100.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount < 0 || amount > 100)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_JOB_ACTIVITY", 0xda4ea4d7);
	MetricSpentArenaJoinSpectator m(amount, fromBank, entryId);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_JOB_ACTIVITY");

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_ARENA_JOIN_SPECTATOR - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandSpendArenaSpectatorBox(int amount, int itembought, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPEND_ARENA_SPECTATOR_BOX", true);)

		scriptAssertf(amount >= 0 && amount <= 10000, "%s: NETWORK_SPEND_ARENA_SPECTATOR_BOX - invalid money amount < %d >, must be > 0 and <=100.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount < 0 || amount > 10000)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_JOB_ACTIVITY", 0xda4ea4d7);
	MetricSpentArenaSpectatorBox m(amount, fromBank, itembought);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_JOB_ACTIVITY");

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPEND_ARENA_SPECTATOR_BOX - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandSpendSpinTheWheelPayment(int amount, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPEND_SPIN_THE_WHEEL_PAYMENT", true);)

		scriptAssertf(amount >= 0 && amount <= 5000, "%s: NETWORK_SPEND_SPIN_THE_WHEEL_PAYMENT - invalid money amount < %d >, must be > 0 and <=100.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount < 0 || amount > 5000)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_JOB_ACTIVITY", 0xda4ea4d7);
	MetricSpentSpinTheWheelPayment m(amount, fromBank);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_JOB_ACTIVITY");

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPEND_SPIN_THE_WHEEL_PAYMENT - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandSpendArenaWarsPremiumEvent(int amount, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPEND_ARENA_PREMIUM", true);)

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_JOB_ACTIVITY", 0xda4ea4d7);
	MetricSpentArenaPremium m(amount, fromBank);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_JOB_ACTIVITY");

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPEND_ARENA_PREMIUM - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnArenaWar(int matchEarnings, int premiumEarnings, int careerAward, int skillAward)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_ARENA_WAR", true));

	if (matchEarnings > 0)
	{
		MetricEarnGeneric m("MATCH_EARNINGS", "ARENA_WAR", matchEarnings);
		const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
		if (!EarnCash(matchEarnings, true, false, &m, statId))
		{
			scriptErrorf("%s: NETWORK_EARN_ARENA_WAR - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
	if (premiumEarnings > 0)
	{
		MetricEarnGeneric m("PREMIUM_EARNINGS", "ARENA_WAR", premiumEarnings);
		const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
		if (!EarnCash(premiumEarnings, true, false, &m, statId))
		{
			scriptErrorf("%s: NETWORK_EARN_ARENA_WAR - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
	if (careerAward > 0)
	{
		MetricEarnGeneric m("CAREER_AWARD", "ARENA_WAR", careerAward);
		const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
		if (!EarnCash(careerAward, true, false, &m, statId))
		{
			scriptErrorf("%s: NETWORK_EARN_ARENA_WAR - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
	if (skillAward > 0)
	{
		MetricEarnGeneric m("SKILL_AWARD", "ARENA_WAR", skillAward);
		const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
		if (!EarnCash(skillAward, true, false, &m, statId))
		{
			scriptErrorf("%s: NETWORK_EARN_ARENA_WAR - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
}

void CommandEarnArenaWarAssassinateTarget(int amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_ARENA_WAR_ASSASSINATE_TARGET", true));

	MetricEarnGeneric m("ASSASSINATE_TARGET", "BUSINESS_BATTLE", amount);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if (!EarnCash(amount, true, false, &m, statId))
	{
		scriptErrorf("%s: NETWORK_EARN_ARENA_WAR_ASSASSINATE_TARGET - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnArenaWarEventCargo(int amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_ARENA_WAR_EVENT_CARGO", true));

	MetricEarnGeneric m("EVENT_CARGO", "BUSINESS_BATTLE", amount);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if (!EarnCash(amount, true, false, &m, statId))
	{
		scriptErrorf("%s: NETWORK_EARN_ARENA_WAR_EVENT_CARGO - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandSpendCasinoMembership(int amount, bool fromBank, bool fromBankAndWallet, int purchasePoint)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPEND_CASINO_MEMBERSHIP", true));

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_STYLE_ENT", 0x49d67632);
	MetricSpendCasinoMembership m(amount, fromBank, purchasePoint);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_STYLE_ENT");

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPEND_CASINO_MEMBERSHIP - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void PayEmployeeWage(int amount, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_PAY_EMPLOYEE_WAGE", true);)

	scriptAssertf(amount>0, "%s: NETWORK_PAY_EMPLOYEE_WAGE - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_PROPERTY_UTIL", 0x8526aff3);
	MetricSpendEmployeeWage m(amount, fromBank);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_PROPERTY_UTIL");

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_PAY_EMPLOYEE_WAGE - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void PayMatchEntryFee(int amount, const char* uniqueMatchId, bool fromBank, bool fromWalletAndBank)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_PAY_MATCH_ENTRY_FEE", true);)

	scriptAssertf(amount>0, "%s: NETWORK_PAY_MATCH_ENTRY_FEE - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	if(scriptVerifyf(uniqueMatchId, "NETWORK_PAY_MATCH_ENTRY_FEE - no match id set"))
	{
		static const int itemId = ATSTRINGHASH("MONEY_SPENT_JOB_ACTIVITY", 0xda4ea4d7);
		MetricSpendMatchEntryFee m(amount, fromBank, uniqueMatchId);
		const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_JOB_ACTIVITY");

		if (!SpendCash(amount, fromBank, false, fromWalletAndBank, false, &m, statId, itemId))
		{
			scriptErrorf("%s: NETWORK_PAY_MATCH_ENTRY_FEE - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
}

bool CommandMoneyCanBet(int amount, bool fromBank, bool fromBankAndWallet)
{
	if (!rlRos::HasPrivilege(NetworkInterface::GetLocalGamerIndex(), RLROS_PRIVILEGEID_CANBET))
	{
		scriptErrorf("Can not bet because RLROS_PRIVILEGEID_CANBET is FALSE.");
		return false;
	}

#if __NET_SHOP_ACTIVE
	if (!NETWORK_SHOPPING_MGR.ShouldDoNullTransaction())
	{
		return true;
	}
#endif // __NET_SHOP_ACTIVE

	s64 cash = MoneyInterface::GetVCWalletBalance();

	if (fromBankAndWallet)
	{
		cash = MoneyInterface::GetVCWalletBalance() + MoneyInterface::GetVCBankBalance();
	}
	else if (fromBank)
	{
		cash = MoneyInterface::GetVCBankBalance();
	}

	//Get the correct money amounts and stats
	if (amount > cash)
	{
		scriptErrorf("Can not bet because GetVCWalletBalance=%" I64FMT "d is lower than the amount=%d.", cash, amount);
		return false;
	}

	return true;
}

bool CanBet(int amount)
{
	if (!rlRos::HasPrivilege(NetworkInterface::GetLocalGamerIndex(), RLROS_PRIVILEGEID_CANBET))
	{
		scriptErrorf("Can not bet because RLROS_PRIVILEGEID_CANBET is FALSE.");
		return false;
	}

	//Get the correct money amounts and stats
	const s64 cash = MoneyInterface::GetVCWalletBalance();
	if (amount > cash)
	{
		scriptErrorf("Can not bet because GetVCWalletBalance=%" I64FMT "d is lower than the amount=%d.", cash, amount);
		return false;
	}

	return true;
}

bool CommandCasinoCanBet(int casinogame)
{
	if ( !NETWORK_LEGAL_RESTRICTIONS.IsValid() )
	{
		scriptErrorf("%s: NETWORK_CASINO_CAN_BET - Can not gamble because legal restrictions aren't ready yet.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return false;
	}

	if (!rlRos::HasPrivilege(NetworkInterface::GetLocalGamerIndex(), RLROS_PRIVILEGEID_CANBET))
	{
		scriptErrorf("%s: NETWORK_CASINO_CAN_BET - Can not gamble because RLROS_PRIVILEGEID_CANBET is FALSE.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return false;
	}

	scriptAssertf(0 != casinogame, "%s: NETWORK_CASINO_CAN_BET - invalid casinogame='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), casinogame);

	const rlLegalTerritoryRestriction& restriction = NETWORK_LEGAL_RESTRICTIONS.Gambling((u32)casinogame);
	scriptAssertf(restriction.IsValid(), "%s: NETWORK_CASINO_CAN_BET - invalid casinogame='%d' - NOT found.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), casinogame);

	if (restriction.IsValid())
	{
		if (restriction.m_EvcAllowed)
		{
			return true;
		}

		scriptErrorf("%s: NETWORK_CASINO_CAN_BET - Can not gamble because m_EvcAllowed is FALSE, for casinogame='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), casinogame);
		return false;
	}

	return false;
}

bool CommandCasinoCanBetPVC( )
{
	return ( !NETWORK_LEGAL_RESTRICTIONS.EvcOnlyOnCasinoChipsTransactions() );
}

bool CommandCasinoCanBetAmount(int amount)
{
	//Check for ROS gambling restrictions.
	if ( !rlRos::HasPrivilege(NetworkInterface::GetLocalGamerIndex(), RLROS_PRIVILEGEID_CANBET) )
	{
		scriptErrorf("%s: NETWORK_CASINO_CAN_BET_AMOUNT - Can not gamble because RLROS_PRIVILEGEID_CANBET is FALSE.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return false;
	}

	//Check for Territory legal restrictions.
	if ( !NETWORK_LEGAL_RESTRICTIONS.CanGamble() )
	{
		scriptErrorf("%s: NETWORK_CASINO_CAN_BET_AMOUNT - Gambling is not allowed - Failed for amount='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
		return false;
	}

	// Specify where the cash is taken from.
	const bool fromBank = false;
	const bool fromBankAndWallet = true;
	const bool fromWalletAndBank = false;

	// -1 means that the game will use the current character for wallet calculations.
	const int character = -1;

	// deficit is the amount missing of cash to cover the spend call.
	int deficit = 0;
	if ( CanSpendGtaDollars(amount, fromBank, fromBankAndWallet, fromWalletAndBank, deficit, character, NETWORK_LEGAL_RESTRICTIONS.EvcOnlyOnCasinoChipsTransactions() OUTPUT_ONLY(, "NETWORK_CASINO_CAN_BET_AMOUNT")))
	{
		scriptDebugf1("%s: NETWORK_CASINO_CAN_BET_AMOUNT - SUCCESS - amount='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
		return true;
	}

	return false;
}

static const u32 MAX_NUM_VALID_SCRIPTS = 11;
static const u32 s_validCallingScriptsHashes[MAX_NUM_VALID_SCRIPTS] = 
{
	ATSTRINGHASH("AM_MP_CASINO", 0x7166bd4a),
	ATSTRINGHASH("AM_MP_CASINO_APARTMENT", 0xd76d873d),
	ATSTRINGHASH("blackjack", 0xd378365e),
	ATSTRINGHASH("CASINO_LUCKY_WHEEL", 0xbdf0ccff),
	ATSTRINGHASH("casino_slots", 0x5f1459d7),
	ATSTRINGHASH("clothes_shop_mp", 0xC0F0BBC3),
	ATSTRINGHASH("CasinoRoulette", 0x05e86d0d),
	ATSTRINGHASH("freemode", 0xc875557d),
	ATSTRINGHASH("fm_content_island_dj", 0xbe590e09),
	ATSTRINGHASH("gb_casino", 0x13645dc3),
	ATSTRINGHASH("three_card_poker", 0xc8608f32),
};
bool VERIFY_CHIPS_USE()
{
	const CGameScriptId& scriptId = static_cast<CGameScriptId&>(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId());
	for (u32 i = 0; i < MAX_NUM_VALID_SCRIPTS; i++)
	{
		if (s_validCallingScriptsHashes[i] == scriptId.GetScriptNameHash())
		{
			return true;
		}
	}

	return false;
};

bool CommandCasinoCanBuyChipsWithPvc()
{
	return ( !NETWORK_LEGAL_RESTRICTIONS.EvcOnlyOnCasinoChipsTransactions() );
}

bool CommandCasinoBuyChips(const int amount, const int chips)
{
	scriptAssertf(VERIFY_CHIPS_USE(), "%s : NETWORK_CASINO_BUY_CHIPS - This script is not allowed to perform this action - script is NOT white listed.",CTheScripts::GetCurrentScriptNameAndProgramCounter() );
	if (!VERIFY_CHIPS_USE())
	{
		scriptErrorf("%s : NETWORK_CASINO_BUY_CHIPS - script is NOT whitelisted.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return false;
	}

	scriptAssertf(amount >= 0 && chips > 0, "%s : NETWORK_CASINO_BUY_CHIPS - invalid amount='%d' or chips='%d'.",CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, chips);
	if (amount >= 0 && chips > 0)
	{
#if __NET_SHOP_ACTIVE

		if (NETWORK_SHOPPING_MGR.ShouldDoNullTransaction())
		{
			scriptAssertf(!NETWORK_LEGAL_RESTRICTIONS.IsAnyEvcAllowed() || CommandCasinoCanBetAmount(amount), "%s : NETWORK_CASINO_BUY_CHIPS - CommandCasinoCanBetAmount - amount='%d' - is false.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
			if (NETWORK_LEGAL_RESTRICTIONS.IsAnyEvcAllowed() && !CommandCasinoCanBetAmount(amount))
			{
				scriptErrorf("%s : NETWORK_CASINO_BUY_CHIPS - CommandCasinoCanBetAmount - amount='%d' - is false.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
				return false;
			}
			else if ( !NETWORK_LEGAL_RESTRICTIONS.IsAnyEvcAllowed() )
			{
				scriptAssertf(0 == amount || amount == chips, "%s : NETWORK_CASINO_BUY_CHIPS - Gambling locked and an Invalid amount='%d' was used, should be equal to chips='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, chips);
				if (0 != amount && amount != chips)
				{
					scriptErrorf("%s : NETWORK_CASINO_BUY_CHIPS - Gambling locked and an Invalid amount='%d' was used, should be equal to chips='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, chips);
					return false;
				}
			}
		}

#elif ( !RSG_PC )

		scriptAssertf(CommandCasinoCanBetAmount(amount), "%s : NETWORK_CASINO_BUY_CHIPS - CommandCasinoCanBetAmount - amount='%d' - is false.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
		if (NETWORK_LEGAL_RESTRICTIONS.IsAnyEvcAllowed() && !CommandCasinoCanBetAmount(amount))
		{
			scriptErrorf("%s : NETWORK_CASINO_BUY_CHIPS - CommandCasinoCanBetAmount - amount='%d' - is false.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
			return false;
		}
		else if ( !NETWORK_LEGAL_RESTRICTIONS.IsAnyEvcAllowed() )
		{
			scriptAssertf(0 == amount || amount == chips, "%s : NETWORK_CASINO_BUY_CHIPS - Gambling locked and an Invalid amount='%d' was used, should be equal to chips='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, chips);
			if (0 != amount && amount != chips)
			{
				scriptErrorf("%s : NETWORK_CASINO_BUY_CHIPS - Gambling locked and an Invalid amount='%d' was used, should be equal to chips='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, chips);
				return false;
			}
		}

#endif // __NET_SHOP_ACTIVE

		static const int itemId = ATSTRINGHASH("MONEY_SPENT_BETTING", 0x4b3d473e);
		const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_BETTING");
		MetricCasinoBuyChips m(amount, chips);
		if (SpendCash(amount, false, true, false, false, &m, statId, itemId, -1, false, NETWORK_LEGAL_RESTRICTIONS.EvcOnlyOnCasinoChipsTransactions()))
		{
			//////////////////////////////////////////////////////////////////////////
			//Increment chips.

			scriptDebugf1("%s: NETWORK_CASINO_BUY_CHIPS - Add chips amount='%d', chips='%d', CurrentChips='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, chips, StatsInterface::GetIntStat(STAT_CASINO_CHIPS));

#if __NET_SHOP_ACTIVE

			if ( NETWORK_SHOPPING_MGR.ShouldDoNullTransaction() )
			{
				StatsInterface::IncrementStat(STAT_CASINO_CHIPS, (float)chips);
			}

#elif (!RSG_PC)

			StatsInterface::IncrementStat(STAT_CASINO_CHIPS, (float)chips);

#endif // __NET_SHOP_ACTIVE

			return true;
		}
	}

	scriptErrorf("%s: NETWORK_CASINO_BUY_CHIPS - Failed to buy chips amount='%d' chips='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, chips);

	return false;
}

bool CommandCasinoSellChips(const int amount, const int chips)
{
	scriptAssertf(VERIFY_CHIPS_USE(), "%s : NETWORK_CASINO_SELL_CHIPS - This script is not allowed to perform this action - script is NOT white listed.",CTheScripts::GetCurrentScriptNameAndProgramCounter() );
	if (!VERIFY_CHIPS_USE())
	{
		scriptErrorf("%s : NETWORK_CASINO_SELL_CHIPS - script is NOT whitelisted.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return false;
	}

	scriptAssertf(amount >= 0 && chips > 0, "%s : NETWORK_CASINO_SELL_CHIPS - invalid amount='%d' or chips='%d'.",CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, chips);
	if (amount >= 0 && chips > 0)
	{
		MetricCasinoSellChips m(amount, chips);
		if(EarnCash(amount, true, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_BETTING")))
		{
			scriptDebugf1("%s: NETWORK_CASINO_SELL_CHIPS - Sell chips amount='%d', chips='%d', CurrentChips='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, chips, StatsInterface::GetIntStat(STAT_CASINO_CHIPS));

			//Decrement chips.
#if __NET_SHOP_ACTIVE
			if ( NETWORK_SHOPPING_MGR.ShouldDoNullTransaction() )
			{
				StatsInterface::DecrementStat(STAT_CASINO_CHIPS, (float)chips);
			}
#elif (!RSG_PC)
			StatsInterface::DecrementStat(STAT_CASINO_CHIPS, (float)chips);
#endif // __NET_SHOP_ACTIVE

			return true;
		}
	}

	scriptErrorf("%s: NETWORK_CASINO_SELL_CHIPS - Failed to sell chips amount='%d' chips='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, chips);

	return false;
}

void  DeferCashTransactionsUntilShopSave( )
{
	scriptAssertf(0, "%s: NETWORK_DEFER_CASH_TRANSACTIONS_UNTIL_SHOP_SAVE - deprecated command.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	//scriptWarningf("%s: NETWORK_DEFER_CASH_TRANSACTIONS_UNTIL_SHOP_SAVE - is being called.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	//rlTelemetry::SetImmediateFlushBlocked( true );
}

void SpentBetting(int amount, int bettype, const char* uniqueMatchId, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_BETTING", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_BETTING - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	if(scriptVerifyf(uniqueMatchId, "NETWORK_SPENT_BETTING - no match id set"))
	{
		//Check if we can bet
		if (!scriptVerify( CommandMoneyCanBet( amount, fromBank, fromBankAndWallet ) ))
		{
			scriptErrorf("CommandMoneyCanBet() - False");
			return;
		}

		static const int itemId = ATSTRINGHASH("MONEY_SPENT_BETTING", 0x4b3d473e);
		MetricSpendBetting m(amount, fromBank, uniqueMatchId, (u32)bettype);
		const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_JOB_ACTIVITY");

		if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
		{
			scriptErrorf("%s: NETWORK_SPENT_BETTING - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
}

void SpentWager(int bossId1, int bossId2, int amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_WAGER", true);)

		scriptAssertf(amount>=0, "%s: NETWORK_SPENT_WAGER - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	const u64 bid0 = ((u64)bossId1) << 32;
	const u64 bid1 = (0x00000000FFFFFFFF&(u64)bossId2);
	u64 bossId = bid0|bid1;

	if(scriptVerifyf(bossId, "%s: NETWORK_SPENT_WAGER - invalid BossId", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		//Check if we can bet
		if (!scriptVerify( CommandMoneyCanBet( amount, false, false) ))
		{
			scriptErrorf("CommandMoneyCanBet() - False");
			return;
		}

		static const int itemId = ATSTRINGHASH("NETWORK_SPENT_WAGER", 0x83190e8c);
		MetricSpendWager m(amount);
		const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_BOSS_GOON");

		if (!SpendCash(amount, false, false, false, false, &m, statId, itemId, -1, false, true))
		{
			scriptErrorf("%s: NETWORK_SPENT_WAGER - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
}

void CommandSpendNightclubEntryFee(int playerIndex, int entryType, int amount, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_NIGHTCLUB_ENTRY_FEE", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_NIGHTCLUB_ENTRY_FEE - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	CNetGamePlayer* pNetPlayer = CTheScripts::FindNetworkPlayer(playerIndex);
	if( scriptVerify(pNetPlayer) )
	{
		MetricSpendNightclubEntryFee m(amount, fromBank, pNetPlayer->GetGamerInfo().GetGamerHandle(), entryType);
		const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_PROPERTY_UTIL");
		static const int itemId = ATSTRINGHASH("MONEY_SPENT_PROPERTY_UTIL", 0x8526aff3);
		if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
		{
			scriptErrorf("%s: NETWORK_SPENT_NIGHTCLUB_ENTRY_FEE - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
}

void CommandSpendNightclubBarDrink(int drinkid, int amount, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPEND_NIGHTCLUB_BAR_DRINK", true););

	scriptAssertf(amount>=0, "%s: NETWORK_SPEND_NIGHTCLUB_BAR_DRINK - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	MetricSpendNightclubBarDrink m(amount, fromBank, drinkid);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_STYLE_ENT");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_STYLE_ENT", 0x49d67632);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPEND_NIGHTCLUB_BAR_DRINK - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

}

void CommandSpendBountyHunterMission(int amount, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPEND_BOUNTY_HUNTER_MISSION", true););

	scriptAssertf(amount>=0, "%s: NETWORK_SPEND_BOUNTY_HUNTER_MISSION - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	MetricSpendBountyHunterMission m(amount, fromBank);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_PROPERTY_UTIL");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_PROPERTY_UTIL", 0x8526aff3);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPEND_BOUNTY_HUNTER_MISSION - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void SpentOnFairgroundRides(int amount, const int rideId, bool fromBank, bool fromBankAndWallet, int npcProvider = 0)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_BUY_FAIRGROUND_RIDE", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_BUY_FAIRGROUND_RIDE - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_STYLE_ENT", 0x49d67632);
	MetricSpentOnFairgroundRide m(amount, fromBank, rideId, npcProvider);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_STYLE_ENT");

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_BUY_FAIRGROUND_RIDE - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void SpentOnJobSkip(int amount, const char* uniqueMatchId, bool fromBank, bool fromWalletAndBank)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_JOB_SKIP", true);)

	scriptAssertf(amount>0, "%s: NETWORK_SPENT_JOB_SKIP - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	if(scriptVerifyf(uniqueMatchId, "NETWORK_SPENT_JOB_SKIP - no match id set"))
	{
		static const int itemId = ATSTRINGHASH("MONEY_SPENT_JOB_ACTIVITY", 0xda4ea4d7);
		MetricSpendJobSkip m(amount, fromBank, uniqueMatchId);
		const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_JOB_ACTIVITY");

		if (!SpendCash(amount, fromBank, false, fromWalletAndBank, false, &m, statId, itemId))
		{
			scriptErrorf("%s: NETWORK_SPENT_JOB_SKIP - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
}

bool SpentOnBoss(int amount, bool fromBank, bool fromWalletAndBank)
{
	scriptAssertf(0, "%s: NETWORK_SPENT_BOSS_GOON - COMMAND DEPRECATED, use START_BEING_BOSS instead", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_BOSS_GOON", true);)

	scriptAssertf(amount>0, "%s: NETWORK_SPENT_BOSS_GOON - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return false;

	const StatId statId = StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID");
	u64 uuidValue = StatsInterface::GetUInt64Stat(statId);

	scriptAssertf(0 == uuidValue, "%s: NETWORK_SPENT_BOSS_GOON - Failed - BOSS_GOON_UUID already has a value < %" I64FMT "d >.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), uuidValue);

	//make sure its not set.
	if (0 == uuidValue)
	{
		const bool success = rlCreateUUID(&uuidValue);
		scriptAssertf(success, "%s: NETWORK_SPENT_BOSS_GOON - Failed to create the UUID - uuidValue='%" SIZETFMT "d'..", CTheScripts::GetCurrentScriptNameAndProgramCounter(), uuidValue);

		if (success)
		{
			MetricSpendBecomeBoss m(amount, fromBank, uuidValue);

			scriptDebugf1("%s:NETWORK_SPENT_BOSS_GOON - Command called - uuidValue='%" SIZETFMT "d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), uuidValue);

			if (SpendCash(amount, fromBank, false, fromWalletAndBank, false, &m, StatsInterface::GetStatsModelHashId("MONEY_SPENT_BOSS_GOON"), ATSTRINGHASH("MONEY_SPEND_BOSS_GOON", 0x8f2c0db6)))
			{
				StatsInterface::SetStatData(statId, uuidValue);
				return true;
			}

			scriptErrorf("%s: NETWORK_SPENT_BOSS_GOON - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}

	return false;
}

enum PaymentRefusedReason
{
	PRR_UNKNOWN

    //Amount is negative or 0
	,PRR_INVALID_AMOUNT

    //Not enough Cash in the Wallet
	,PRR_NOT_ENOUGH_CASH

    //Not enough Earn Cash
	,PRR_NOT_ENOUGH_EVC

	// No boss Id set
	,PRR_INVALID_BOOSID
};

bool Command_CanPayAmountToBossOrGoon(int bossId1, int bossId2, int amount, int& reason)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("CAN_PAY_AMOUNT_TO_BOSS", true);)

    bool result = false;

	reason = PRR_UNKNOWN;

    //Amount is negative or 0
	scriptAssertf(amount>0, "%s: CAN_PAY_AMOUNT_TO_BOSS - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
	{
		reason = PRR_INVALID_AMOUNT;
		return false;
	}

	const u64 bid0 = ((u64)bossId1) << 32;
	const u64 bid1 = (0x00000000FFFFFFFF&(u64)bossId2);
	u64 bossId = bid0|bid1;

	if(!scriptVerifyf(bossId, "%s: CAN_PAY_AMOUNT_TO_BOSS - invalid BossId", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
    {
		reason = PRR_INVALID_BOOSID;
		return result;
    }

    //Not enough Cash in the Wallet
	s64 wallet = MoneyInterface::GetVCWalletBalance();
	if(wallet < amount)
	{
		scriptDebugf1("%s: CAN_PAY_AMOUNT_TO_BOSS - Not enough cash in wallet", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		reason = PRR_NOT_ENOUGH_CASH;
		return result;
	}

	//Not enough Earn Cash
	s64 evc = MoneyInterface::GetEVCBalance();
	if(evc < amount)
	{
		scriptDebugf1("%s: CAN_PAY_AMOUNT_TO_BOSS - Not earn cash", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		reason = PRR_NOT_ENOUGH_EVC;
		return result;
	}

    result = true;
	return result;
}

void Command_EarnGoon(int bossId1, int bossId2, int amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_GOON", true);)

	scriptAssertf(amount>0, "%s: NETWORK_EARN_GOON - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 1240000;
	scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_GOON - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount > AMOUNT_LIMIT)
		return;

	const u64 bid0 = ((u64)bossId1) << 32;
	const u64 bid1 = (0x00000000FFFFFFFF&(u64)bossId2);
	u64 bossId = bid0|bid1;

	if(scriptVerifyf(bossId, "%s: NETWORK_EARN_GOON - invalid BossId", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		MetricEarnGoon m(amount, bossId);
		if(!EarnCash(amount, false, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_BOSS_GOON")))
		{
			scriptErrorf("%s: NETWORK_EARN_GOON - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
}

void Command_EarnBoss(int bossId1, int bossId2, int amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_BOSS", true);)

	scriptAssertf(amount>0, "%s: NETWORK_EARN_BOSS - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 150000;
	scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_BOSS - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount > AMOUNT_LIMIT)
		return;

	const u64 bid0 = ((u64)bossId1) << 32;
	const u64 bid1 = (0x00000000FFFFFFFF&(u64)bossId2);
	u64 bossId = bid0|bid1;

	if(scriptVerifyf(bossId, "%s: NETWORK_EARN_BOSS - invalid BossId", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		MetricEarnBoss m(amount, bossId);
		if(!EarnCash(amount, false, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_BOSS_GOON")))
		{
			scriptErrorf("%s: NETWORK_EARN_BOSS - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
}

void Command_EarnAgency(int bossId1, int bossId2, int amount, bool notInAGang)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_AGENCY", true);)

	scriptAssertf(amount>0, "%s: NETWORK_EARN_AGENCY - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 1200000;
	scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_AGENCY - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount > AMOUNT_LIMIT)
		return;

	const u64 bid0 = ((u64)bossId1) << 32;
	const u64 bid1 = (0x00000000FFFFFFFF&(u64)bossId2);
	u64 bossId = bid0|bid1;

	if(scriptVerifyf(bossId || notInAGang, "%s: NETWORK_EARN_AGENCY - invalid BossId when in a gang - bossId =  %" I64FMT "d   notInAGang = %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), bossId, (notInAGang) ? "true" : "false"))
	{
		MetricEarnAgency m(amount, bossId);
		if(!EarnCash(amount, false, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_BOSS_GOON")))
		{
			scriptErrorf("%s: NETWORK_EARN_AGENCY - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
}

void CommandEarnFromSmugglerAgency(int bossId1, int bossId2, int amount, bool notInAGang)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_SMUGGLER_AGENCY", true);)

	scriptAssertf(amount>0, "%s: NETWORK_EARN_SMUGGLER_AGENCY - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 720000;
	scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_SMUGGLER_AGENCY - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount > AMOUNT_LIMIT)
		return;

	const u64 bid0 = ((u64)bossId1) << 32;
	const u64 bid1 = (0x00000000FFFFFFFF&(u64)bossId2);
	u64 bossId = bid0|bid1;

	if(scriptVerifyf(bossId || notInAGang, "%s: NETWORK_EARN_SMUGGLER_AGENCY - invalid BossId when in a gang - bossId =  %" I64FMT "d   notInAGang = %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), bossId, (notInAGang) ? "true" : "false"))
	{
		MetricEarnAgencySmuggler m(amount, bossId);
		if(!EarnCash(amount, false, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_BOSS_GOON")))
		{
			scriptErrorf("%s: NETWORK_EARN_SMUGGLER_AGENCY - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
}

void CommandEarnMoneyFromWarehouse(int amount,int namehash)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_FROM_WAREHOUSE", true);)

	scriptAssertf(amount>0, "%s: NETWORK_EARN_FROM_WAREHOUSE - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 1100000;
	scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_WAREHOUSE - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount > AMOUNT_LIMIT)
		return;

	MetricEarnWarehouse m(amount, namehash);
	if(!EarnCash(amount, false, false, &m, StatId()))
	{
		scriptErrorf("%s: NETWORK_EARN_FROM_WAREHOUSE - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnMoneyFromContraband(int amount, int quantity)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_FROM_CONTRABAND", true);)

		scriptAssertf(amount>0, "%s: NETWORK_EARN_FROM_CONTRABAND - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 96000000;
	scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_CONTRABAND - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount > AMOUNT_LIMIT)
		return;

	MetricEarnContraband m(amount, quantity);
	if(!EarnCash(amount, false, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_SELL_CONTRABAND")))
	{
		scriptErrorf("%s: NETWORK_EARN_FROM_CONTRABAND - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnMoneyFromDestroyingContraband(int amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_FROM_DESTROYING_CONTRABAND", true);)

	scriptAssertf(amount>0, "%s: NETWORK_EARN_FROM_DESTROYING_CONTRABAND - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 80000;
	scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_DESTROYING_CONTRABAND - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount > AMOUNT_LIMIT)
		return;

	MetricEarnDestroyContraband m(amount);
	if(!EarnCash(amount, false, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_SELL_CONTRABAND")))
	{
		scriptErrorf("%s: NETWORK_EARN_FROM_DESTROYING_CONTRABAND - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnMoneyFromSmugglerWork(int amount, int quantity, int highDemandBonus, int additionalSaleBonus, int contrabandType)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_FROM_SMUGGLER_WORK", true);)

	static const int AMOUNT_LIMIT = 3150000;
	int amountTotal = amount + highDemandBonus + additionalSaleBonus;
	scriptAssertf(amountTotal <= AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_SMUGGLER_WORK - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amountTotal > AMOUNT_LIMIT)
		return;

#if __NET_SHOP_ACTIVE
	const s64 nonceSeed = GameTransactionSessionMgr::Get().GetNonceForTelemetry();
#endif // __NET_SHOP_ACTIVE

	if(amount > 0)
	{
		MetricEarnSmugglerWork m(amount, "SELL_CARGO", quantity, contrabandType);
		if(!EarnCash(amount, false, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_FREIGHT_SMUGGLER")))
		{
			scriptErrorf("%s: NETWORK_EARN_FROM_SMUGGLER_WORK - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}

	if(highDemandBonus > 0)
	{
#if __NET_SHOP_ACTIVE
		GameTransactionSessionMgr::Get().SetNonceForTelemetry(nonceSeed);
#endif // __NET_SHOP_ACTIVE
		MetricEarnSmugglerWork m(highDemandBonus, "HIGH_DEMAND_BONUS");
		if(!EarnCash(highDemandBonus, false, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_FREIGHT_SMUGGLER")))
		{
			scriptErrorf("%s: NETWORK_EARN_FROM_SMUGGLER_WORK - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}

	if(additionalSaleBonus > 0)
	{
#if __NET_SHOP_ACTIVE
		GameTransactionSessionMgr::Get().SetNonceForTelemetry(nonceSeed);
#endif // __NET_SHOP_ACTIVE
		MetricEarnSmugglerWork m(additionalSaleBonus, "SELL_BONUS");
		if(!EarnCash(additionalSaleBonus, false, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_FREIGHT_SMUGGLER")))
		{
			scriptErrorf("%s: NETWORK_EARN_FROM_SMUGGLER_WORK - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
}

void CommandEarnMoneyFromHangarTrade(int amount, int propertyHash)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_FROM_HANGAR_TRADE", true);)

	scriptAssertf(amount>0, "%s: NETWORK_EARN_FROM_HANGAR_TRADE - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	if(propertyHash != 0)
	{
		static const int AMOUNT_LIMIT = 3150000;
		scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_HANGAR_TRADE - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
		if (amount > AMOUNT_LIMIT)
			return;

		MetricEarnTradeHangar m(amount, "PROPERTY_UTIL", propertyHash);
		if(!EarnCash(amount, false, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_PROPERTY_TRADE")))
		{
			scriptErrorf("%s: NETWORK_EARN_FROM_HANGAR_TRADE - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
}


void SpentOnHangarProperty(int amount, const char* action, srcSpendHangar* data, bool fromBank, bool fromBankAndWallet OUTPUT_ONLY(, const char* commandName) )
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED(commandName, true);)

	scriptAssertf(amount>=0, "%s: %s - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName, amount);
	if (amount<0)
		return;

#if __NET_SHOP_ACTIVE
	const s64 nonceSeed = GameTransactionSessionMgr::Get().GetNonceForTelemetry();
#endif // __NET_SHOP_ACTIVE

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_PROPERTY_UTIL");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_PROPERTY_UTIL", 0x8526aff3);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
	{
		scriptErrorf("%s: %s - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}
	else
	{
#if __NET_SHOP_ACTIVE
		GameTransactionSessionMgr::Get().SetNonceForTelemetry(nonceSeed);
#endif // __NET_SHOP_ACTIVE

		// We need to send more than one metric, so we pass NULL to SpendCash and send the metric ourselves
		ASSERT_ONLY( int total = 0; )
			if(data->m_location_amount.Int >= 0)
			{
				ASSERT_ONLY( total += data->m_location_amount.Int; )
				MetricSpent_HangarPurchAndUpdate m(action, "LOCATION", data->m_location.Int, data->m_location_amount.Int, fromBank);
				ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
				scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
			}
			if(data->m_flooring_amount.Int >= 0)
			{
				ASSERT_ONLY( total += data->m_flooring_amount.Int; )
				MetricSpent_HangarPurchAndUpdate m(action, "FLOORING", data->m_flooring.Int, data->m_flooring_amount.Int, fromBank);
				ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
				scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
			}
			if(data->m_furnitures_amount.Int >= 0)
			{
				ASSERT_ONLY( total += data->m_furnitures_amount.Int; )
				MetricSpent_HangarPurchAndUpdate m(action, "FURNITURE", data->m_furnitures.Int, data->m_furnitures_amount.Int, fromBank);
				ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
				scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
			}
			if(data->m_workshop_amount.Int >= 0)
			{
				ASSERT_ONLY( total += data->m_workshop_amount.Int; )
				MetricSpent_HangarPurchAndUpdate m(action, "WORKSHOP", data->m_workshop.Int, data->m_workshop_amount.Int, fromBank);
				ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
				scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
			}
			if(data->m_style_amount.Int >= 0)
			{
				ASSERT_ONLY( total += data->m_style_amount.Int; )
				MetricSpent_HangarPurchAndUpdate m(action, "STYLE", data->m_style.Int, data->m_style_amount.Int, fromBank);
				ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
				scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
			}
			if(data->m_lighting_amount.Int >= 0)
			{
				ASSERT_ONLY( total += data->m_lighting_amount.Int; )
				MetricSpent_HangarPurchAndUpdate m(action, "LIGHTING", data->m_lighting.Int, data->m_lighting_amount.Int, fromBank);
				ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
				scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
			}
			if(data->m_livingQuarter_amount.Int >= 0)
			{
				ASSERT_ONLY( total += data->m_livingQuarter_amount.Int; )
				MetricSpent_HangarPurchAndUpdate m(action, "LIVING_QUARTERS", data->m_livingQuarter.Int, data->m_livingQuarter_amount.Int, fromBank);
				ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
				scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
			}
			scriptAssertf(amount == total, "%s: %s - Given amount and total dont match (amount = %d   |   total = %d)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName, amount, total);

#if __NET_SHOP_ACTIVE
			GameTransactionSessionMgr::Get().SetNonceForTelemetry(0);
#endif // __NET_SHOP_ACTIVE
	}
}

void CommandSpendPurchaseHangarProperty(int amount, srcSpendHangar* data, bool fromBank, bool fromBankAndWallet)
{
	SpentOnHangarProperty(amount, "BUY_HANGAR", data, fromBank, fromBankAndWallet OUTPUT_ONLY(, "NETWORK_SPENT_PURCHASE_HANGAR") );
}

void CommandSpendUpgradeHangarProperty(int amount, srcSpendHangar* data, bool fromBank, bool fromBankAndWallet)
{
	SpentOnHangarProperty(amount, "UPGRADE_HANGAR", data, fromBank, fromBankAndWallet OUTPUT_ONLY(, "NETWORK_SPENT_UPGRADE_HANGAR") );
}

void CommandSpentHangarUtilityCharges(int amount, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_HANGAR_UTILITY_CHARGES", true);)

		scriptAssertf(amount>0, "%s: NETWORK_SPENT_HANGAR_UTILITY_CHARGES - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_PROPERTY_UTIL", 0x8526aff3);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_PROPERTY_UTIL");
	MetricSpentHangarUtilityCharges m(amount, fromBank);

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_HANGAR_UTILITY_CHARGES - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandSpentHangarStaffCharges(int amount, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_HANGAR_STAFF_CHARGES", true);)

		scriptAssertf(amount>0, "%s: NETWORK_SPENT_HANGAR_STAFF_CHARGES - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_PROPERTY_UTIL", 0x8526aff3);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_PROPERTY_UTIL");
	MetricSpentHangarStaffCharges m(amount, fromBank);

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_HANGAR_STAFF_CHARGES - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void Command_SpentGoon(int bossId1, int bossId2, int amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_GOON", true);)

	int reason;
	if(!scriptVerifyf(Command_CanPayAmountToBossOrGoon(bossId1, bossId2, amount, reason), "%s: NETWORK_SPENT_GOON - Cannot spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return;

	const u64 bid0 = ((u64)bossId1) << 32;
	const u64 bid1 = (0x00000000FFFFFFFF&(u64)bossId2);
	u64 bossId = bid0|bid1;

	if(scriptVerifyf(bossId, "%s: NETWORK_SPENT_GOON - invalid BossId", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		static const int itemId = ATSTRINGHASH("MONEY_SPENT_GOON", 0xe8197049);
		const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_BOSS_GOON");
		MetricSpendGoon m(amount, bossId);
		if (!SpendCash(amount, false, false, false, false, &m, statId, itemId, -1, false, true))
		{
			scriptErrorf("%s: NETWORK_SPENT_GOON - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
}

void Command_SpentBoss(int bossId1, int bossId2, int amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_BOSS", true);)

	int reason;
	if(!scriptVerifyf(Command_CanPayAmountToBossOrGoon(bossId1, bossId2, amount, reason), "%s: NETWORK_SPENT_GOON - Cannot spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		return;

	const u64 bid0 = ((u64)bossId1) << 32;
	const u64 bid1 = (0x00000000FFFFFFFF&(u64)bossId2);
	u64 bossId = bid0|bid1;

	if(scriptVerifyf(bossId, "%s: NETWORK_SPENT_GOON - invalid BossId", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		static const int itemId = ATSTRINGHASH("MONEY_SPENT_BOSS", 0x3d9b2eda);
		const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_BOSS_GOON");
		MetricSpendGoon m(amount, bossId);
		if (!SpendCash(amount, false, false, false, false, &m, statId, itemId, -1, false, true))
		{
			scriptErrorf("%s: NETWORK_SPENT_BOSS - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
}

void SpentInStripClub(int amount, bool fromBank, int expenditureType, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_IN_STRIPCLUB", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_IN_STRIPCLUB - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_STYLE_ENT", 0x49d67632);
	MetricSpendInStripClub m(amount, fromBank, expenditureType);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_STYLE_ENT");

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_IN_STRIPCLUB - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandSpentJukeBox(int amount, int playlist, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_JUKEBOX", true);)

		scriptAssertf(amount>=0, "%s: NETWORK_SPENT_JUKEBOX - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_STYLE_ENT", 0x49d67632);
	MetricSpendInJukeBox m(amount, fromBank, playlist);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_STYLE_ENT");

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_JUKEBOX - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void BuyHealthCare(int amount, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_BUY_HEALTHCARE", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_BUY_HEALTHCARE - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_HEALTHCARE", 0x1c85d3af);
	MetricSpendHealthCare m(amount, fromBank);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_HEALTHCARE");

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_BUY_HEALTHCARE - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void BuyPlayerHealthCare(int amount, int playerIndex, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_PLAYER_HEALTHCARE", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_PLAYER_HEALTHCARE - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	CNetGamePlayer *pNetPlayer = CTheScripts::FindNetworkPlayer(playerIndex);
	if( scriptVerify(pNetPlayer) )
	{
		static const int itemId = ATSTRINGHASH("MONEY_SPENT_HEALTHCARE", 0x1c85d3af);
		MetricSpendPlayerHealthCare m(amount, fromBank, pNetPlayer->GetGamerInfo().GetGamerHandle());
		const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_HEALTHCARE");

		if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
		{
			scriptErrorf("%s: NETWORK_SPENT_PLAYER_HEALTHCARE - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
}

void BuyAirStrike(int amount, bool fromBank, bool fromBankAndWallet, int npcProvider = 0)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_BUY_AIRSTRIKE", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_BUY_AIRSTRIKE - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_CONTACT_SERVICE", 0x7ee6a4b0);
	MetricSpendAirStrike m(amount, fromBank, npcProvider);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_CONTACT_SERVICE");

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_BUY_HEALTHCARE - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}
void BuyHeliStrike(int amount, bool fromBank, bool fromBankAndWallet, int npcProvider = 0)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_BUY_HELI_STRIKE", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_BUY_HELI_STRIKE - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_CONTACT_SERVICE", 0x7ee6a4b0);
	MetricSpendHeliStrike m(amount, fromBank, npcProvider);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_CONTACT_SERVICE");

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_BUY_HELI_STRIKE - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void BuyGangBackup(int amount, int gangType, bool fromBank, bool fromBankAndWallet, int npcProvider = 0)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_BUY_BACKUP_GANG", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_BUY_BACKUP_GANG - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_CONTACT_SERVICE", 0x7ee6a4b0);
	MetricSpendGangBackup m(amount, fromBank, gangType, npcProvider);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_CONTACT_SERVICE");

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_BUY_BACKUP_GANG - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void SpentOnAmmoDrop(int amount, bool fromBank, bool fromBankAndWallet, int npcProvider = 0)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_AMMO_DROP", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_AMMO_DROP - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_CONTACT_SERVICE", 0x7ee6a4b0);
	MetricSpendOnAmmoDrop m(amount, fromBank, npcProvider);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_CONTACT_SERVICE");

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_AMMO_DROP - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void BuyBounty(int amount, int playerIndex, bool fromBank, bool fromBankAndWallet, int npcProvider = 0)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_BUY_BOUNTY", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_BUY_BOUNTY - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	CNetGamePlayer *pNetPlayer = CTheScripts::FindNetworkPlayer(playerIndex);
	if( scriptVerify(pNetPlayer) )
	{
		static const int itemId = ATSTRINGHASH("MONEY_SPENT_CONTACT_SERVICE", 0x7ee6a4b0);
		MetricSpentBuyBounty m(amount, fromBank, pNetPlayer->GetGamerInfo().GetGamerHandle(), npcProvider);
		const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_CONTACT_SERVICE");

		if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
		{
			scriptErrorf("%s: NETWORK_BUY_BOUNTY - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
}

void BuyProperty(int amount, int propertyType, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_BUY_PROPERTY", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_BUY_PROPERTY - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_PROPERTY_UTIL", 0x8526aff3);
	MetricSpentBuyProperty m(amount, fromBank, propertyType);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_PROPERTY_UTIL");

	if (0 == amount)
	{
		SendMoneyMetric(m);
	}
	else if (SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		CNetworkTelemetry::IncrementCloudCashTransactionCount();
		CStatsMgr::GetStatsDataMgr().GetSavesMgr().ClearPendingFlushRequests();
	}
	else
	{
		scriptErrorf("%s: NETWORK_BUY_PROPERTY - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void BuySmokes(int amount,  bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_BUY_SMOKES", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_BUY_SMOKES - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_STYLE_ENT", 0x49d67632);
	MetricSpentBuySmokes m(amount, fromBank);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_STYLE_ENT");

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_BUY_SMOKES - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void SpentOnHeliPickup(int amount, bool fromBank, bool fromBankAndWallet, int npcProvider = 0)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_HELI_PICKUP", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_HELI_PICKUP - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_CONTACT_SERVICE", 0x7ee6a4b0);
	MetricSpentHeliPickup m(amount, fromBank, npcProvider);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_CONTACT_SERVICE");

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_HELI_PICKUP - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void SpentOnBoatPickup(int amount, bool fromBank, bool fromBankAndWallet, int npcProvider = 0)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_BOAT_PICKUP", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_BOAT_PICKUP - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_CONTACT_SERVICE", 0x7ee6a4b0);
	MetricSpentBoatPickup m(amount, fromBank, npcProvider);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_CONTACT_SERVICE");

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_BOAT_PICKUP - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void SpentOnBullShark(int amount, bool fromBank, bool fromBankAndWallet, int npcProvider = 0)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_BULL_SHARK", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_BULL_SHARK - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_CONTACT_SERVICE", 0x7ee6a4b0);
	MetricSpentBullShark m(amount, fromBank, npcProvider);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_CONTACT_SERVICE");

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_BULL_SHARK - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

#if !__FINAL
void SpentMoneyFromDebug(int amount, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_FROM_DEBUG", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_FROM_DEBUG - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_FROM_DEBUG", 0xf6e21a23);
	MetricSpentFromDebug m(amount, fromBank);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_CONTACT_SERVICE");

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_FROM_DEBUG - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}
#endif

void SpentOnCashDrop(int amount, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_CASH_DROP", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_CASH_DROP - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_DROPPED_STOLEN", 0x96e3925f);
	MetricSpentFromCashDrop m(amount, fromBank);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_DROPPED_STOLEN");

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_CASH_DROP - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void SpentOnHireMugger(int amount, bool fromBank, bool fromBankAndWallet, int npcProvider = 0)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_HIRE_MUGGER", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_HIRE_MUGGER - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_CONTACT_SERVICE", 0x7ee6a4b0);
	MetricSpentHireMugger m(amount, fromBank, npcProvider);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_CONTACT_SERVICE");

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_HIRE_MUGGER - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void SpentOnRobbedByMugger(int amount, bool fromBank, bool fromBankAndWallet, int npcProvider = 0)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_ROBBED_BY_MUGGER", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_ROBBED_BY_MUGGER - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_DROPPED_STOLEN", 0x96e3925f);
	MetricSpentRobbedByMugger m(amount, fromBank, npcProvider);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_DROPPED_STOLEN");

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_ROBBED_BY_MUGGER - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void SpentOnHireMercenary(int amount, bool fromBank, bool fromBankAndWallet, int npcProvider = 0)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_HIRE_MERCENARY", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_HIRE_MERCENARY - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_CONTACT_SERVICE", 0x7ee6a4b0);
	MetricSpentHireMercenary m(amount, fromBank, npcProvider);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_CONTACT_SERVICE");

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_HIRE_MERCENARY - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void SpentOnWantedLevel(int amount, int& handleData, bool fromBank, bool fromBankAndWallet, int npcProvider = 0)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_BUY_WANTEDLEVEL", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_BUY_WANTEDLEVEL - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	// make sure that handle is valid
	rlGamerHandle hGamer; 
	if(CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_SPENT_BUY_WANTEDLEVEL")))
	{
		static const int itemId = ATSTRINGHASH("MONEY_SPENT_CONTACT_SERVICE", 0x7ee6a4b0);
		MetricSpentWantedLevel m(amount, fromBank, hGamer, npcProvider);
		const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_CONTACT_SERVICE");

		if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
		{
			scriptErrorf("%s: NETWORK_SPENT_BUY_WANTEDLEVEL - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
}

void SpentOnOffTheRadar(int amount, bool fromBank, bool fromBankAndWallet, int npcProvider = 0)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_BUY_OFFTHERADAR", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_BUY_OFFTHERADAR - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_CONTACT_SERVICE", 0x7ee6a4b0);
	MetricSpentOffRadar m(amount, fromBank, npcProvider);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_CONTACT_SERVICE");

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_BUY_OFFTHERADAR - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void SpentOnRevealPlayersInRadar(int amount, bool fromBank, bool fromBankAndWallet, int npcProvider = 0)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_BUY_REVEAL_PLAYERS", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_BUY_REVEAL_PLAYERS - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_CONTACT_SERVICE", 0x7ee6a4b0);
	MetricSpentRevealPlayersInRadar m(amount, fromBank, npcProvider);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_CONTACT_SERVICE");

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_BUY_REVEAL_PLAYERS - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void SpentOnCarWash(int amount, int id, int location, bool fromBank, bool fromWalletAndBank)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_CARWASH", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_CARWASH - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

#if __ASSERT
	fwModelId mid;
	CModelInfo::GetBaseModelInfoFromHashKey((u32) id, &mid);
	scriptAssertf(mid.IsValid(), "%s: NETWORK_SPENT_CARWASH - invalid vehicle model hash < %d >.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), id);
	if (mid.IsValid())
	{
		scriptAssertf(CModelInfo::GetBaseModelInfo(mid), "%s:NETWORK_SPENT_CARWASH - Specified model < %d > isn't valid!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), id);
	}
#endif

	MetricSpentCarWash m(amount, fromBank, (u32)id, (u32)location);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_VEH_MAINTENANCE");
	const int itemId = ATSTRINGHASH("NETWORK_SPENT_CARWASH", 0x8283e028);

	if(!SpendCash(amount, fromBank, false, fromWalletAndBank, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_CARWASH - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void SpentOnCinema(int amount, int id, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_CINEMA", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_CINEMA - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_STYLE_ENT", 0x49d67632);
	MetricSpentCinema m(amount, fromBank, id);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_STYLE_ENT");

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_CINEMA - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void SpentOnTelescope(int amount, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_TELESCOPE", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_TELESCOPE - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_STYLE_ENT", 0x49d67632);
	MetricSpentTelescope m(amount, fromBank);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_STYLE_ENT");

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_TELESCOPE - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void SpentOnHoldUps(int amount, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_HOLDUPS", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_HOLDUPS - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_HOLDUPS", 0x89112734);
	MetricSpentOnHoldUps m(amount, fromBank);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_DROPPED_STOLEN");

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_HOLDUPS - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void SpentOnPassiveMode(int amount, bool fromBank, bool fromBankAndWallet, int npcProvider = 0)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_BUY_PASSIVE_MODE", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_BUY_PASSIVE_MODE - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_CONTACT_SERVICE", 0x7ee6a4b0);
	MetricSpentOnPassiveMode m(amount, fromBank, npcProvider);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_CONTACT_SERVICE");

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_BUY_PASSIVE_MODE - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void SpentOnBankInterest(int amount, bool fromBank, bool fromBankAndWallet)
{
	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_BANK_INTEREST - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_BANKINTEREST", 0x0bba125b);
	MetricSpentOnBankInterest m(amount, fromBank);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, StatId(), itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_BANK_INTEREST - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void SpentOnProstitute(int amount, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_PROSTITUTES", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_PROSTITUTES - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_STYLE_ENT", 0x49d67632);
	MetricSpentOnProstitute m(amount, fromBank);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_STYLE_ENT");

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_PROSTITUTES - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void SpentOnArrestBail(int amount, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_ARREST_BAIL", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_ARREST_BAIL - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_HEALTHCARE", 0x1c85d3af);
	MetricSpentOnArrestBail m(amount, fromBank);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_HEALTHCARE");

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_ARREST_BAIL - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void SpentOnVehicleInsurancePremium(int amount, int itemHash, int& handleData, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_PAY_VEHICLE_INSURANCE_PREMIUM", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_PAY_VEHICLE_INSURANCE_PREMIUM - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;
	bool disableInsurancePremium = false;

	Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("DISABLE_INSURANCE_PAYOUT", 0xDA0B7FFB), disableInsurancePremium);
	if(!disableInsurancePremium)
	{
		scriptDebugf1("MP_GLOBAL:DISABLE_INSURANCE_PAYOUT Tunable is set to TRUE");
	#if __ASSERT
		fwModelId mid;
		CModelInfo::GetBaseModelInfoFromHashKey((u32) itemHash, &mid);
		scriptAssertf(mid.IsValid(), "%s: NETWORK_SPENT_PAY_VEHICLE_INSURANCE_PREMIUM - invalid vehicle model hash < %d >.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), itemHash);
		if (mid.IsValid())
			scriptAssertf(CModelInfo::GetBaseModelInfo(mid), "%s:NETWORK_SPENT_PAY_VEHICLE_INSURANCE_PREMIUM - Specified model < %d > isn't valid!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), itemHash);
	#endif

		//make sure that handle is valid
		rlGamerHandle hGamer; 
		if (IsHandleValid(handleData, SCRIPT_GAMER_HANDLE_SIZE))
		{
			if (CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_SPENT_PAY_VEHICLE_INSURANCE_PREMIUM")))
			{
				static const int itemId = ATSTRINGHASH("MONEY_SPENT_VEH_MAINTENANCE", 0x3c65cc9a);
				MetricSpentOnVehicleInsurancePremium m(amount, fromBank, itemHash, hGamer);
				const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_VEH_MAINTENANCE");

				if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
				{
					scriptErrorf("%s: NETWORK_SPENT_PAY_VEHICLE_INSURANCE_PREMIUM - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				}
			}
		}
	}
	else
	{
		scriptDebugf1("MP_GLOBAL:DISABLE_INSURANCE_PAYOUT Tunable was set to FALSE");
	}
}

void SpentOnPlayerCall(int amount, int& handleData, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_CALL_PLAYER", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_CALL_PLAYER - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	//make sure that handle is valid
	rlGamerHandle hGamer; 
	if (CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "NETWORK_SPENT_CALL_PLAYER")))
	{
		static const int itemId = ATSTRINGHASH("MONEY_SPENT_CONTACT_SERVICE", 0x7ee6a4b0);
		MetricSpentOnCall m(amount, fromBank, hGamer);
		const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_CONTACT_SERVICE");

		if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
		{
			scriptErrorf("%s: NETWORK_SPENT_CALL_PLAYER - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
}

void SpentOnBounty(int amount, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_BOUNTY", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_BOUNTY - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_JOB_ACTIVITY", 0xda4ea4d7);
	MetricSpentOnBounty m(amount, fromBank);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_JOB_ACTIVITY");

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_BOUNTY - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void SpentOnRockstar(int amount, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_FROM_ROCKSTAR", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_FROM_ROCKSTAR - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_ROCKSTAR_AWARD", 0xa32961a9);
	MetricSpentOnRockstar m(amount, fromBank);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_ROCKSTAR_AWARD");

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_FROM_ROCKSTAR - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void SpentOnNoCops(int amount, bool fromBank, bool fromBankAndWallet, int npcProvider = 0)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_NO_COPS", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_NO_COPS - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_NOCOPS", 0x72667e57);
	MetricSpentOnNoCop m(amount, fromBank, npcProvider);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_CONTACT_SERVICE");

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_NO_COPS - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void SpentOnCargoSourcing(int amount, bool fromBank, bool fromBankAndWallet, int cost, int warehouseID, int warehouseSlot)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_CARGO_SOURCING", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_CARGO_SOURCING - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_CARGO_SOURCING", 0x1107fc0a);
	MetricSpendCargoSourcing m(amount, fromBank, cost, warehouseID, warehouseSlot);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_CARGO_SOURCING");

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_CARGO_SOURCING - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void SpentOnJobRequest(int amount, bool fromBank, bool fromBankAndWallet, int npcProvider = 0)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_REQUEST_JOB", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_REQUEST_JOB - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_CONTACT_SERVICE", 0x7ee6a4b0);
	MetricSpentOnJobRequest m(amount, fromBank, npcProvider);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_CONTACT_SERVICE");

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_REQUEST_JOB - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void SpentOnBendRequest(int amount, bool fromBank, bool fromBankAndWallet, int npcProvider = 0)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_REQUEST_HEIST", true);)

	scriptAssertf(amount>0, "%s: NETWORK_SPENT_REQUEST_HEIST - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int itemId = ATSTRINGHASH("NETWORK_SPENT_REQUEST_HEIST", 0xf4287778);
	MetricSpentOnBendRequest m(amount, fromBank, npcProvider);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_CONTACT_SERVICE");

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_REQUEST_HEIST - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void Command_SpentOnMoveYacht(int amount, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_MOVE_YACHT", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_MOVE_YACHT - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_PROPERTY_UTIL", 0x8526aff3);
	MetricSpentOnMoveYacht m(amount, fromBank);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_PROPERTY_UTIL");

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_MOVE_YACHT - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void Command_SpentOnMoveSubmarine(int amount, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_MOVE_SUBMARINE", true);)

		scriptAssertf(amount >= 0, "%s: NETWORK_SPENT_MOVE_SUBMARINE - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount < 0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_PROPERTY_UTIL", 0x8526aff3);
	MetricSpentOnMoveSubmarine m(amount, fromBank);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_PROPERTY_UTIL");

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_MOVE_SUBMARINE - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandSpentRenameOrganization(int amount, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_RENAME_ORGANIZATION", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_RENAME_ORGANIZATION - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	const StatId bossIdStat = StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID");
	const u64 bossId = StatsInterface::GetUInt64Stat(bossIdStat);

	scriptAssertf(bossId, "%s: NETWORK_SPENT_RENAME_ORGANIZATION - invalid BossId.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!bossId)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_BOSS_GOON", 0xa8f16949);
	MetricSpentRenameOrganization m(bossId, amount, fromBank);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_BOSS_GOON");

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_RENAME_ORGANIZATION - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandBuyContrabandMission(int amount, int warehouseID, int missionID, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_BUY_CONTRABAND_MISSION", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_BUY_CONTRABAND_MISSION - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_PROPERTY_UTIL", 0x8526aff3);
	MetricSpentBuyContrabandMission m(amount, fromBank, warehouseID, missionID);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_BUY_CONTRABAND");

	if (0 == amount)
	{
		SendMoneyMetric(m);
	}
	else if (SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		CNetworkTelemetry::IncrementCloudCashTransactionCount();
		CStatsMgr::GetStatsDataMgr().GetSavesMgr().ClearPendingFlushRequests();
	}
	else
	{
		scriptErrorf("%s: NETWORK_BUY_CONTRABAND_MISSION - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandPayBusinessSupplies(int amount, int businessID, int businessType, int numSegments)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_PAY_BUSINESS_SUPPLIES", true);)

		scriptAssertf(amount>=0, "%s: NETWORK_SPENT_PAY_BUSINESS_SUPPLIES - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_BIKER_BUSINESS", 0x54829b2e);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_BIKER_BUSINESS");
	MetricSpentPayBussinessSupplies m(amount, businessID, businessType, numSegments);

	if (0 == amount)
	{
		SendMoneyMetric(m);
	}
	else if (SpendCash(amount, false, true, false, false, &m, statId, itemId))
	{
		CNetworkTelemetry::IncrementCloudCashTransactionCount();
		CStatsMgr::GetStatsDataMgr().GetSavesMgr().ClearPendingFlushRequests();
	}
	else
	{
		scriptErrorf("%s: NETWORK_SPENT_PAY_BUSINESS_SUPPLIES - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandSpentVehicleExportMods(int amount, bool fromBank, bool fromBankAndWallet, int bossID1, int bossID2, int buyerID, int vehicle1, int vehicle2, int vehicle3, int vehicle4)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_VEHICLE_EXPORT_MODS", true);)

		scriptAssertf(amount>0, "%s: NETWORK_SPENT_VEHICLE_EXPORT_MODS - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	const u64 bid0 = ((u64)bossID1) << 32;
	const u64 bid1 = (0x00000000FFFFFFFF&(u64)bossID2);
	const u64 bossId = bid0|bid1;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_VEH_MAINTENANCE", 0x3c65cc9a);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_VEHICLE_EXPORT");
	MetricSpentVehicleExportMods m(amount, fromBank, bossId, buyerID, vehicle1, vehicle2, vehicle3, vehicle4);

	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_VEHICLE_EXPORT_MODS - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}


//  Exec Telem

void CommandSpentPaServiceHeli(int amount, bool anotherMember, bool fromBank, bool fromBankAndWallet)
{
    NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_PA_SERVICE_HELI", true);)

    scriptAssertf(amount>=0, "%s: NETWORK_SPENT_PA_SERVICE_HELI - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
    if (amount<0)
        return;

    static const int itemId = ATSTRINGHASH("MONEY_SPENT_EXEC_PA", 0xb6fad071);
    MetricSpentPaServiceHeli m(amount, anotherMember, fromBank);
    const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_EXEC_PA");

    if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
    {
        scriptErrorf("%s: NETWORK_SPENT_PA_SERVICE_HELI - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    }
}

void CommandSpentPaServiceVehicle(int amount, int vehicleHash, bool fromBank, bool fromBankAndWallet)
{
    NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_PA_SERVICE_VEHICLE", true);)

    scriptAssertf(amount>=0, "%s: NETWORK_SPENT_PA_SERVICE_VEHICLE - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
    if (amount<0)
        return;

    static const int itemId = ATSTRINGHASH("MONEY_SPENT_EXEC_PA", 0xb6fad071);
    MetricSpentPaServiceVehicle m(amount, vehicleHash, fromBank);
    const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_EXEC_PA");

    if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
    {
        scriptErrorf("%s: NETWORK_SPENT_PA_SERVICE_VEHICLE - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    }
}

void CommandSpentPaServiceSnack(int amount, int itemHash, bool fromBank, bool fromBankAndWallet)
{
    NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_PA_SERVICE_SNACK", true);)

    scriptAssertf(amount>=0, "%s: NETWORK_SPENT_PA_SERVICE_SNACK - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
    if (amount<0)
        return;

    static const int itemId = ATSTRINGHASH("MONEY_SPENT_EXEC_PA", 0xb6fad071);
    MetricSpentPaServiceSnack m(amount, itemHash, fromBank);
    const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_EXEC_PA");

    if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
    {
        scriptErrorf("%s: NETWORK_SPENT_PA_SERVICE_SNACK - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    }
}

void CommandSpentPaServiceDancer(int amount, int value, bool fromBank, bool fromBankAndWallet)
{
    NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_PA_SERVICE_DANCER", true);)

    scriptAssertf(amount>=0, "%s: NETWORK_SPENT_PA_SERVICE_DANCER - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
    if (amount<0)
        return;

    static const int itemId = ATSTRINGHASH("MONEY_SPENT_EXEC_PA", 0xb6fad071);
    MetricSpentPaServiceDancer m(amount, value, fromBank);
    const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_EXEC_PA");

    if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
    {
        scriptErrorf("%s: NETWORK_SPENT_PA_SERVICE_DANCER - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    }
}

void CommandSpentPaServiceImpound(int amount, bool fromBank, bool fromBankAndWallet)
{
    NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_PA_SERVICE_IMPOUND", true);)

    scriptAssertf(amount>=0, "%s: NETWORK_SPENT_PA_SERVICE_IMPOUND - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
    if (amount<0)
        return;

    static const int itemId = ATSTRINGHASH("MONEY_SPENT_EXEC_PA", 0xb6fad071);
    MetricSpentPaServiceImpound m(amount, fromBank);
    const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_EXEC_PA");

    if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
    {
        scriptErrorf("%s: NETWORK_SPENT_PA_SERVICE_IMPOUND - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    }
}

void CommandSpentPaHeliPickup(int amount, int hash, bool fromBank, bool fromBankAndWallet)
{
    NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_PA_HELI_PICKUP", true);)

    scriptAssertf(amount>=0, "%s: NETWORK_SPENT_PA_HELI_PICKUP - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
    if (amount<0)
        return;

    static const int itemId = ATSTRINGHASH("MONEY_SPENT_EXEC_PA", 0xb6fad071);
    MetricSpentPaHeliPickup m(amount, hash, fromBank);
    const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_EXEC_PA");

    if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
    {
        scriptErrorf("%s: NETWORK_SPENT_PA_HELI_PICKUP - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    }
}

void SpentOnExecutiveOfficeAndWarehouseProperty(int amount, const char* action, srcSpendOfficeAndWarehouse* data, bool fromBank, bool fromBankAndWallet OUTPUT_ONLY(, const char* commandName) )
{
    NOTFINAL_ONLY(VERIFY_STATS_LOADED(commandName, true);)

    scriptAssertf(amount>=0, "%s: %s - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName, amount);
    if (amount<0)
        return;

#if __NET_SHOP_ACTIVE
	const s64 nonceSeed = GameTransactionSessionMgr::Get().GetNonceForTelemetry();
#endif // __NET_SHOP_ACTIVE

    const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_PROPERTY_UTIL");
    static const int itemId = ATSTRINGHASH("MONEY_SPENT_PROPERTY_UTIL", 0x8526aff3);
    if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
    {
        scriptErrorf("%s: %s - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
    }
    else
    {
#if __NET_SHOP_ACTIVE
		GameTransactionSessionMgr::Get().SetNonceForTelemetry(nonceSeed);
#endif // __NET_SHOP_ACTIVE

        // We need to send more than one metric, so we pass NULL to SpendCash and send the metric ourselves
        ASSERT_ONLY( int total = 0; )
        if(data->m_LocationAmount.Int >= 0)
        {
            ASSERT_ONLY( total += data->m_LocationAmount.Int; )
            MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "LOCATION", data->m_Location.Int, data->m_LocationAmount.Int, fromBank);
            ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
            scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
        }
        if(data->m_StyleAmount.Int >= 0)
        {
            ASSERT_ONLY( total += data->m_StyleAmount.Int; )
            MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "STYLE", data->m_Style.Int, data->m_StyleAmount.Int, fromBank);
            ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
            scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
        }
        if(data->m_PaGenderAmount.Int >= 0)
        {
            ASSERT_ONLY( total += data->m_PaGenderAmount.Int; )
            MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "PA_GENDER", data->m_PaGender.Int, data->m_PaGenderAmount.Int, fromBank);
            ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
            scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
        }
        if(data->m_SignageAmount.Int >= 0)
        {
            ASSERT_ONLY( total += data->m_SignageAmount.Int; )
            MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "SIGNAGE", data->m_Signage.Int, data->m_SignageAmount.Int, fromBank);
            ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
            scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
        }
        if(data->m_GunLockerAmount.Int >= 0)
        {
            ASSERT_ONLY( total += data->m_GunLockerAmount.Int; )
            MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "GUNLOCKER", data->m_GunLocker.Int, data->m_GunLockerAmount.Int, fromBank);
            ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
            scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
        }
        if(data->m_VaultAmount.Int >= 0)
        {
            ASSERT_ONLY( total += data->m_VaultAmount.Int; )
            MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "VAULT", data->m_Vault.Int, data->m_VaultAmount.Int, fromBank);
            ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
            scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
        }
        if(data->m_PersonalQuartersAmount.Int >= 0)
        {
            ASSERT_ONLY( total += data->m_PersonalQuartersAmount.Int; )
            MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "PQ", data->m_PersonalQuarters.Int, data->m_PersonalQuartersAmount.Int, fromBank);
            ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
            scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
        }
        if(data->m_WarehouseSizeAmount.Int >= 0)
        {
            ASSERT_ONLY( total += data->m_WarehouseSizeAmount.Int; )
            MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "WHSIZE", data->m_WarehouseSize.Int, data->m_WarehouseSizeAmount.Int, fromBank);
            ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
            scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
        }
        if(data->m_SmallWarehouses.Int >= 0)
        {
            MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "SWAREHOUSE", data->m_SmallWarehouses.Int, 0, fromBank);
            ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
            scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
        }
        if(data->m_MediumWarehouses.Int >= 0)
        {
            MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "MWAREHOUSE", data->m_MediumWarehouses.Int, 0, fromBank);
            ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
            scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
		}
		if(data->m_LargeWarehouses.Int >= 0)
		{
			MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "LWAREHOUSE", data->m_LargeWarehouses.Int, 0, fromBank);
			ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
			scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
		}
		if(data->m_ModShopAmount.Int >= 0)
		{
			ASSERT_ONLY( total += data->m_ModShopAmount.Int; )
			MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "MODSHOP", data->m_ModShop.Int, data->m_ModShopAmount.Int, fromBank);
			ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
			scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
		}
        scriptAssertf(amount == total, "%s: %s - Given amount and total dont match (amount = %d   |   total = %d)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName, amount, total);

#if __NET_SHOP_ACTIVE
		GameTransactionSessionMgr::Get().SetNonceForTelemetry(0);
#endif // __NET_SHOP_ACTIVE
    }
}

void CommandSpentPurchaseOfficeProperty(int amount, srcSpendOfficeAndWarehouse* data, bool fromBank, bool fromBankAndWallet)
{
    SpentOnExecutiveOfficeAndWarehouseProperty(amount, "OFFICE_PURCH", data, fromBank, fromBankAndWallet OUTPUT_ONLY(, "NETWORK_SPENT_PURCHASE_OFFICE_PROPERTY") );
}

void CommandSpentUpgradeOfficeProperty(int amount, srcSpendOfficeAndWarehouse* data, bool fromBank, bool fromBankAndWallet)
{
	if (amount >= 0)
	{
		SpentOnExecutiveOfficeAndWarehouseProperty(amount, "OFFICE_MOD", data, fromBank, fromBankAndWallet  OUTPUT_ONLY(, "NETWORK_SPENT_UPGRADE_OFFICE_PROPERTY") );
	}
}

void CommandSpentPurchaseWarehouseProperty(int amount, srcSpendOfficeAndWarehouse* data, bool fromBank, bool fromBankAndWallet)
{
	if (amount >= 0)
	{
		SpentOnExecutiveOfficeAndWarehouseProperty(amount, "WAREHOUSE_PURCH", data, fromBank, fromBankAndWallet  OUTPUT_ONLY(, "NETWORK_SPENT_PURCHASE_WAREHOUSE_PROPERTY") );
	}
}

void CommandSpentUpgradeWarehouseProperty(int amount, srcSpendOfficeAndWarehouse* data, bool fromBank, bool fromBankAndWallet)
{
	if (amount >= 0)
	{
		SpentOnExecutiveOfficeAndWarehouseProperty(amount, "WAREHOUSE_MOD", data, fromBank, fromBankAndWallet  OUTPUT_ONLY(, "NETWORK_SPENT_UPGRADE_WAREHOUSE_PROPERTY") );
	}
}

void SpentOnExecutiveOfficeGarage(int amount, const char* action, srcSpendOfficeGarage* data, bool fromBank, bool fromBankAndWallet OUTPUT_ONLY(, const char* commandName) )
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED(commandName, true);)

	scriptAssertf(amount>=0, "%s: %s - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName, amount);
	if (amount<0)
		return;

#if __NET_SHOP_ACTIVE
	const s64 nonceSeed = GameTransactionSessionMgr::Get().GetNonceForTelemetry();
#endif // __NET_SHOP_ACTIVE

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_PROPERTY_UTIL");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_PROPERTY_UTIL", 0x8526aff3);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
	{
		scriptErrorf("%s: %s - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}
	else
	{
#if __NET_SHOP_ACTIVE
		GameTransactionSessionMgr::Get().SetNonceForTelemetry(nonceSeed);
#endif // __NET_SHOP_ACTIVE

		// We need to send more than one metric, so we pass NULL to SpendCash and send the metric ourselves
		ASSERT_ONLY( int total = 0; );
		if(data->m_LocationAmount.Int >= 0)
		{
			ASSERT_ONLY( total += data->m_LocationAmount.Int; );
			MetricSpent_OfficeGarage m(action, "LOCATION", data->m_Location.Int, data->m_LocationAmount.Int, fromBank);
			ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
			scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
		}

		if(data->m_NumberingAmount.Int >= 0)
		{
			ASSERT_ONLY( total += data->m_NumberingAmount.Int; );
			MetricSpent_OfficeGarage m(action, "FLOOR_NUMBER", data->m_Numbering.Int, data->m_NumberingAmount.Int, fromBank);
			ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
			scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
		}

		if(data->m_NumberingStyleAmount.Int >= 0)
		{
			ASSERT_ONLY( total += data->m_NumberingStyleAmount.Int; );
			MetricSpent_OfficeGarage m(action, "FLOOR_NUMBER_STYLE", data->m_NumberingStyle.Int, data->m_NumberingStyleAmount.Int, fromBank);
			ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
			scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
		}

		if(data->m_LightingAmount.Int >= 0)
		{
			ASSERT_ONLY( total += data->m_LightingAmount.Int; );
			MetricSpent_OfficeGarage m(action, "LIGHTING_STYLE", data->m_Lighting.Int, data->m_LightingAmount.Int, fromBank);
			ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
			scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
		}

		if(data->m_WallAmount.Int >= 0)
		{
			ASSERT_ONLY( total += data->m_WallAmount.Int; );
			MetricSpent_OfficeGarage m(action, "WALL_STYLE", data->m_Wall.Int, data->m_WallAmount.Int, fromBank);
			ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
			scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
		}

		scriptAssertf(amount == total, "%s: %s - Given amount and total dont match (amount = %d   |   total = %d)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName, amount, total);

#if __NET_SHOP_ACTIVE
			GameTransactionSessionMgr::Get().SetNonceForTelemetry(0);
#endif // __NET_SHOP_ACTIVE
	}
}

void CommandSpentPurchaseOfficeGarage(int amount, srcSpendOfficeGarage* data, bool fromBank, bool fromBankAndWallet)
{
	SpentOnExecutiveOfficeGarage(amount, "EXEC_OFFICE_GARAGE_PURCH", data, fromBank, fromBankAndWallet OUTPUT_ONLY(, "NETWORK_SPENT_PURCHASE_OFFICE_GARAGE") );
}

void CommandSpentUpgradeOfficeGarage(int amount, srcSpendOfficeGarage* data, bool fromBank, bool fromBankAndWallet)
{
	SpentOnExecutiveOfficeGarage(amount, "EXEC_OFFICE_GARAGE_MOD", data, fromBank, fromBankAndWallet  OUTPUT_ONLY(, "NETWORK_SPENT_UPGRADE_OFFICE_GARAGE") );
}

void CommandSpentPurchaseWarehousePropertyImpExp(int amount, srcSpendOfficeAndWarehouse* data, bool fromBank, bool fromBankAndWallet)
{
	if (amount > 0)
	{
		SpentOnExecutiveOfficeAndWarehouseProperty(amount, "IMPORTEXPORT_WAREHOUSE_PURCH", data, fromBank, fromBankAndWallet  OUTPUT_ONLY(, "NETWORK_SPENT_PURCHASE_IMPEXP_WAREHOUSE_PROPERTY") );
	}
}

void CommandSpentUpgradeWarehousePropertyImpExp(int amount, srcSpendOfficeAndWarehouse* data, bool fromBank, bool fromBankAndWallet)
{
	if (amount > 0)
	{
		SpentOnExecutiveOfficeAndWarehouseProperty(amount, "IMPORTEXPORT_WAREHOUSE_MOD", data, fromBank, fromBankAndWallet  OUTPUT_ONLY(, "NETWORK_SPENT_UPGRADE_IMPEXP_WAREHOUSE_PROPERTY") );
	}
}

class MetricSpent_SpentTradeImpExpWarehouse : public MetricSpent_SpentPurchaseBusinessProperty
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:

	MetricSpent_SpentTradeImpExpWarehouse(const int amount, bool fromBank)
		: MetricSpent_SpentPurchaseBusinessProperty((s64)amount, fromBank)
	{
	}

	virtual const char* GetCategoryName() const {return "LOCATION";}
	virtual const char* GetActionName() const {return "IMPORTEXPORT_WAREHOUSE_TRADE";}
	virtual bool WriteContext(RsonWriter* rw) const { return MetricSpent_SpentPurchaseBusinessProperty::WriteContext(rw); }
};

void CommandSpentTradeWarehousePropertyImpExp(int amount, srcBusinessPropertyData* data, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_TRADE_IMPEXP_WAREHOUSE_PROPERTY", true););

	scriptAssertf(amount>0, "%s: NETWORK_SPENT_TRADE_IMPEXP_WAREHOUSE_PROPERTY - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	scriptAssertf(data, "%s: NETWORK_SPENT_TRADE_IMPEXP_WAREHOUSE_PROPERTY - NULL srcClubHouseData.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!data)
		return;

	MetricSpent_SpentTradeImpExpWarehouse metric(amount, fromBank);
	metric.m_businessId = data->m_businessId.Int;
	metric.m_businessType = data->m_businessType.Int;
	metric.m_businessUpgradeType = data->m_upgradeType.Int;

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_PROPERTY_UTIL");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_PROPERTY_UTIL", 0x8526aff3);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &metric, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_TRADE_IMPEXP_WAREHOUSE_PROPERTY - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandSpentOrderWarehouseVehicle(int amount, int hash, bool fromBank, bool fromBankAndWallet)
{
    NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_ORDER_WAREHOUSE_VEHICLE", true);)

    scriptAssertf(amount>=0, "%s: NETWORK_SPENT_ORDER_WAREHOUSE_VEHICLE - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
    if (amount<0)
        return;

    static const int itemId = ATSTRINGHASH("MONEY_SPENT_PROPERTY_UTIL", 0x8526aff3);
    MetricSpentOrderWarehouseVehicle m(amount, hash, fromBank);
    const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_PROPERTY_UTIL");

    if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
    {
        scriptErrorf("%s: NETWORK_SPENT_ORDER_WAREHOUSE_VEHICLE - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    }
}

void CommandSpentOrderBodyguardVehicle(int amount, int hash, bool fromBank, bool fromBankAndWallet)
{
    NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_ORDER_BODYGUARD_VEHICLE", true);)

    scriptAssertf(amount>=0, "%s: NETWORK_SPENT_ORDER_BODYGUARD_VEHICLE - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
    if (amount<0)
        return;

    static const int itemId = ATSTRINGHASH("MONEY_SPENT_PROPERTY_UTIL", 0x8526aff3);
    MetricSpentOrderBodyguardVehicle m(amount, hash, fromBank);
    const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_PROPERTY_UTIL");

    if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
    {
        scriptErrorf("%s: NETWORK_SPENT_ORDER_BODYGUARD_VEHICLE - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    }
}

void SpentOnClubHouseProperty(int amount, const char* action, srcClubHouseData* data, bool fromBank, bool fromBankAndWallet OUTPUT_ONLY(, const char* commandName) )
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED(commandName, true););

	scriptAssertf(data, "%s: %s - NULL srcClubHouseData.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	if (!data)
		return;
	
#if __NET_SHOP_ACTIVE
	const s64 nonceSeed = GameTransactionSessionMgr::Get().GetNonceForTelemetry();
#endif // __NET_SHOP_ACTIVEE

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_PROPERTY_UTIL");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_PROPERTY_UTIL", 0x8526aff3);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
	{
		scriptErrorf("%s: %s - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}
	else
	{
#if __NET_SHOP_ACTIVE
		GameTransactionSessionMgr::Get().SetNonceForTelemetry(nonceSeed);
#endif // __NET_SHOP_ACTIVE

		ASSERT_ONLY( int total = 0; )
		if(data->m_property_id_amount.Int >= 0)
		{
			ASSERT_ONLY( total += data->m_property_id_amount.Int; )
			MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "LOCATION", data->m_property_id.Int, data->m_property_id_amount.Int, fromBank);
			ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
			scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
		}
		if(data->m_mural_type_amount.Int >= 0)
		{
			ASSERT_ONLY( total += data->m_mural_type_amount.Int; )
			MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "MURAL", data->m_mural_type.Int, data->m_mural_type_amount.Int, fromBank);
			ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
			scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
		}
		if(data->m_wall_style_amount.Int >= 0)
		{
			ASSERT_ONLY( total += data->m_wall_style_amount.Int; )
			MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "WALL", data->m_wall_style.Int, data->m_wall_style_amount.Int, fromBank);
			ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
			scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
		}
		if(data->m_wall_hanging_style_amount.Int >= 0)
		{
			ASSERT_ONLY( total += data->m_wall_hanging_style_amount.Int; )
			MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "HANGING", data->m_wall_hanging_style.Int, data->m_wall_hanging_style_amount.Int, fromBank);
			ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
			scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
		}
		if(data->m_furniture_style_amount.Int >= 0)
		{
			ASSERT_ONLY( total += data->m_furniture_style_amount.Int; )
			MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "FURNITURE", data->m_furniture_style.Int, data->m_furniture_style_amount.Int, fromBank);
			ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
			scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
		}
		if(data->m_emblem_amount.Int >= 0)
		{
			ASSERT_ONLY( total += data->m_emblem_amount.Int; )
			MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "EMBLEM", data->m_emblem.Int, data->m_emblem_amount.Int, fromBank);
			ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
			scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
		}
		if(data->m_gun_locker_amount.Int >= 0)
		{
			ASSERT_ONLY( total += data->m_gun_locker_amount.Int; )
			MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "GUNLOCKER", data->m_gun_locker.Int, data->m_gun_locker_amount.Int, fromBank);
			ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
			scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
		}
		if(data->m_mod_shop_amount.Int >= 0)
		{
			ASSERT_ONLY( total += data->m_mod_shop_amount.Int; )
			MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "MOD_SHOP", data->m_mod_shop.Int, data->m_mod_shop_amount.Int, fromBank);
			ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
			scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
		}
		if(data->m_signage_amount.Int >= 0)
		{
			ASSERT_ONLY( total += data->m_signage_amount.Int; )
			MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "SIGNAGE", data->m_signage.Int, data->m_signage_amount.Int, fromBank);
			ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
			scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
		}
		if(data->m_font_amount.Int >= 0)
		{
			ASSERT_ONLY( total += data->m_font_amount.Int; )
			MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "FONT", data->m_font.Int, data->m_font_amount.Int, fromBank);
			ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
			scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
		}

		scriptAssertf(amount == total, "%s: %s - Given amount and total dont match (amount = %d   |   total = %d)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName, amount, total);

#if __NET_SHOP_ACTIVE
		GameTransactionSessionMgr::Get().SetNonceForTelemetry(0);
#endif // __NET_SHOP_ACTIVE

	}
}

void CommandSpentPurchaseClubHouse(int amount, srcClubHouseData* data, bool fromBank, bool fromBankAndWallet)
{
	SpentOnClubHouseProperty(amount, "CLUBHOUSE_PURCH", data, fromBank, fromBankAndWallet OUTPUT_ONLY(, "NETWORK_SPENT_PURCHASE_CLUB_HOUSE") );
}

void CommandSpentUpgradeClubHouse(int amount, srcClubHouseData* data, bool fromBank, bool fromBankAndWallet)
{
	SpentOnClubHouseProperty(amount, "CLUBHOUSE_MOD", data, fromBank, fromBankAndWallet OUTPUT_ONLY(, "NETWORK_SPENT_UPGRADE_CLUB_HOUSE") );
}

void CommandSpentPurchaseBusinessProperty(int amount, srcBusinessPropertyData* data, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_PURCHASE_BUSINESS_PROPERTY", true););

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_PURCHASE_BUSINESS_PROPERTY - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	scriptAssertf(data, "%s: NETWORK_SPENT_PURCHASE_BUSINESS_PROPERTY - NULL srcClubHouseData.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!data)
		return;

	MetricSpent_SpentPurchaseBusinessProperty metric(amount, fromBank);
	metric.m_businessId = data->m_businessId.Int;
	metric.m_businessType = data->m_businessType.Int;
	metric.m_businessUpgradeType = data->m_upgradeType.Int;

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_PROPERTY_UTIL");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_PROPERTY_UTIL", 0x8526aff3);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &metric, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_PURCHASE_BUSINESS_PROPERTY - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandSpentUpgradeBusinessProperty(int amount, srcBusinessPropertyData* data, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_UPGRADE_BUSINESS_PROPERTY", true););

	scriptAssertf(amount>0, "%s: NETWORK_SPENT_UPGRADE_BUSINESS_PROPERTY - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	scriptAssertf(data, "%s: NETWORK_SPENT_UPGRADE_BUSINESS_PROPERTY - NULL srcClubHouseData.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!data)
		return;

	MetricSpent_SpentUpgradeBusinessProperty metric(amount, fromBank);
	metric.m_businessId = data->m_businessId.Int;
	metric.m_businessType = data->m_businessType.Int;
	metric.m_businessUpgradeType = data->m_upgradeType.Int;

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_PROPERTY_UTIL");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_PROPERTY_UTIL", 0x8526aff3);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &metric, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_UPGRADE_BUSINESS_PROPERTY - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandSpentTradeBusinessProperty(int amount, srcBusinessPropertyData* data, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_TRADE_BUSINESS_PROPERTY", true););

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_TRADE_BUSINESS_PROPERTY - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	scriptAssertf(data, "%s: NETWORK_SPENT_TRADE_BUSINESS_PROPERTY - NULL srcClubHouseData.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!data)
		return;

	MetricSpent_SpentTradeBusinessProperty metric(amount, fromBank);
	metric.m_businessId = data->m_businessId.Int;
	metric.m_businessType = data->m_businessType.Int;
	metric.m_businessUpgradeType = data->m_upgradeType.Int;

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_PROPERTY_UTIL");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_PROPERTY_UTIL", 0x8526aff3);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &metric, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_TRADE_BUSINESS_PROPERTY - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandSpentMcAbility(int amount, int ability, int role, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_MC_ABILITY", true););

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_MC_ABILITY - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	MetricSpentMcAbility metric(amount, fromBank, ability, role);

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_CONTACT_SERVICE");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_CONTACT_SERVICE", 0x7ee6a4b0);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &metric, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_MC_ABILITY - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandSpentImportExportRepair(int amount, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_IMPORT_EXPORT_REPAIR", true););

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_IMPORT_EXPORT_REPAIR - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	static const int AMOUNT_LIMIT = 34000;
	scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_SPENT_IMPORT_EXPORT_REPAIR - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount > AMOUNT_LIMIT)
		return;

	MetricSpentImportExportRepair metric(amount, fromBank);

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_VEH_MAINTENANCE");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_VEH_MAINTENANCE", 0x3c65cc9a);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &metric, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_IMPORT_EXPORT_REPAIR - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandSpentBuyTruck(int amount, bool fromBank, bool fromBankAndWallet, srcSpentOnTruck* data)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_BUY_TRUCK", true););

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_BUY_TRUCK - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	const char* action = "BUY_TRUCK";

	if(data->m_vehicle.Int >= 0)
	{
		MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "VEHICLE", data->m_vehicle.Int, data->m_vehicle_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPENT_BUY_TRUCK - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	if(data->m_trailer.Int >= 0)
	{
		MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "TRAILER", data->m_trailer.Int, data->m_trailer_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPENT_BUY_TRUCK - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	if(data->m_slot1.Int >= 0)
	{
		MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "SLOT1", data->m_slot1.Int, data->m_slot1_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPENT_BUY_TRUCK - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	if(data->m_slot2.Int >= 0)
	{
		MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "SLOT2", data->m_slot2.Int, data->m_slot2_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPENT_BUY_TRUCK - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	if(data->m_slot3.Int >= 0)
	{
		MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "SLOT3", data->m_slot3.Int, data->m_slot3_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPENT_BUY_TRUCK - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	if(data->m_colorscheme.Int >= 0)
	{
		MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "COLOR_SCHEME", data->m_colorscheme.Int, data->m_colorscheme_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPENT_BUY_TRUCK - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_PROPERTY_UTIL");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_PROPERTY_UTIL", 0x8526aff3);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_BUY_TRUCK - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return;
	}
}

void CommandSpentUpgradeTruck(int amount, bool fromBank, bool fromBankAndWallet, srcSpentOnTruck* data)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_UPGRADE_TRUCK", true););

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_UPGRADE_TRUCK - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	const char* action = "UPGRADE_TRUCK";

	if(data->m_vehicle_amount.Int >= 0)
	{
		MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "VEHICLE", data->m_vehicle.Int, data->m_vehicle_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPENT_UPGRADE_TRUCK - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	if(data->m_trailer_amount.Int >= 0)
	{
		MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "TRAILER", data->m_trailer.Int, data->m_trailer_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPENT_UPGRADE_TRUCK - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	if(data->m_slot1_amount.Int >= 0)
	{
		MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "SLOT1", data->m_slot1.Int, data->m_slot1_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPENT_UPGRADE_TRUCK - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	if(data->m_slot2_amount.Int >= 0)
	{
		MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "SLOT2", data->m_slot2.Int, data->m_slot2_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPENT_UPGRADE_TRUCK - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	if(data->m_slot3_amount.Int >= 0)
	{
		MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "SLOT3", data->m_slot3.Int, data->m_slot3_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPENT_UPGRADE_TRUCK - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	if(data->m_colorscheme_amount.Int >= 0)
	{
		MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "COLOR_SCHEME", data->m_colorscheme.Int, data->m_colorscheme_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPENT_UPGRADE_TRUCK - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_PROPERTY_UTIL");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_PROPERTY_UTIL", 0x8526aff3);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_UPGRADE_TRUCK - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return;
	}
}

void CommandSpentBuyBunker(int amount, bool fromBank, bool fromBankAndWallet, srcSpentOnBunker* data)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_BUY_BUNKER", true););

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_BUY_BUNKER - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	const char* action = "BUY_BUNKER";

	if(data->m_location_amount.Int >= 0)
	{
		MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "LOCATION", data->m_location.Int, data->m_location_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPENT_BUY_BUNKER - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	if(data->m_style_amount.Int >= 0)
	{
		MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "STYLE", data->m_style.Int, data->m_style_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPENT_BUY_BUNKER - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	if(data->m_personalquarter_amount.Int >= 0)
	{
		MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "PERSONAL_QUARTER", data->m_personalquarter.Int, data->m_personalquarter_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPENT_BUY_BUNKER - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	if(data->m_firingrange_amount.Int >= 0)
	{
		MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "FIRING_RANGE", data->m_firingrange.Int, data->m_firingrange_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPENT_BUY_BUNKER - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	if(data->m_gunlocker_amount.Int >= 0)
	{
		MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "GUN_LOCKER", data->m_gunlocker.Int, data->m_gunlocker_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPENT_BUY_BUNKER - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	if(data->m_caddy_amount.Int >= 0)
	{
		MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "CADDY", data->m_caddy.Int, data->m_caddy_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPENT_BUY_BUNKER - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_PROPERTY_UTIL");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_PROPERTY_UTIL", 0x8526aff3);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_BUY_BUNKER - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandSpentUpgradeBunker(int amount, bool fromBank, bool fromBankAndWallet, srcSpentOnBunker* data)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_UPRADE_BUNKER", true););

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_UPRADE_BUNKER - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	const char* action = "UPGRADE_BUNKER";

	if(data->m_location_amount.Int >= 0)
	{
		MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "LOCATION", data->m_location.Int, data->m_location_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPENT_UPRADE_BUNKER - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	if(data->m_style_amount.Int >= 0)
	{
		MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "STYLE", data->m_style.Int, data->m_style_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPENT_UPRADE_BUNKER - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	if(data->m_personalquarter_amount.Int >= 0)
	{
		MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "PERSONAL_QUARTER", data->m_personalquarter.Int, data->m_personalquarter_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPENT_UPRADE_BUNKER - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	if(data->m_firingrange_amount.Int >= 0)
	{
		MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "FIRING_RANGE", data->m_firingrange.Int, data->m_firingrange_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPENT_UPRADE_BUNKER - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	if(data->m_gunlocker_amount.Int >= 0)
	{
		MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "GUN_LOCKER", data->m_gunlocker.Int, data->m_gunlocker_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPENT_UPRADE_BUNKER - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	if(data->m_caddy_amount.Int >= 0)
	{
		MetricSpent_OfficeAndWarehousePurchAndUpdate m(action, "CADDY", data->m_caddy.Int, data->m_caddy_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPENT_UPRADE_BUNKER - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_PROPERTY_UTIL");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_PROPERTY_UTIL", 0x8526aff3);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_UPRADE_BUNKER - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return;
	}
}

void CommandSpendGangOpsRepairCost(int amount, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPEND_GANGOPS_REPAIR_COST", true);)

	scriptAssertf(amount >= 0, "%s: NETWORK_SPEND_GANGOPS_REPAIR_COST - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount < 0)
		return;

	MetricSpentGangOpsRepairCost metric(amount, fromBank);

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_VEH_MAINTENANCE");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_VEH_MAINTENANCE", 0x3c65cc9a);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &metric, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPEND_GANGOPS_REPAIR_COST - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnFromSellBunker(int amount, int bunkerHash)
{	
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_FROM_SELL_BUNKER", true);)

	scriptAssertf(amount>=0, "%s: NETWORK_EARN_FROM_SELL_BUNKER  - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	static const int AMOUNT_LIMIT = 4000000;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_SELL_BUNKER - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	MetricEarnFromSellingBunker m(amount, bunkerHash);
	if(!EarnCash(amount, true, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_PROPERTY_TRADE")))
	{
		scriptErrorf("%s: NETWORK_EARN_FROM_SELL_BUNKER - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnFromRdrBonus(int amount, int weapon = 0)
{	
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_RDR_BONUS", true);)

		scriptAssertf(amount>0, "%s: NETWORK_EARN_RDR_BONUS  - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 250000;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_RDR_BONUS - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	MetricEarnFromRdrBonus m(amount, weapon);
	if(!EarnCash(amount, true, false, &m, StatId()))
	{
		scriptErrorf("%s: NETWORK_EARN_RDR_BONUS - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnFromWagePayment(int amount, int missionId = 0)
{	
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_WAGE_PAYMENT", true);)

		scriptAssertf(amount>0, "%s: NETWORK_EARN_WAGE_PAYMENT  - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 240000;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_WAGE_PAYMENT - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	MetricEarnFromWagePayment m(amount, "WAGE_PAYMENT", missionId);
	if(!EarnCash(amount, true, false, &m, StatId()))
	{
		scriptErrorf("%s: NETWORK_EARN_WAGE_PAYMENT - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnFromWagePaymentBonus(int amount)
{	
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_WAGE_PAYMENT_BONUS", true);)

		scriptAssertf(amount>0, "%s: NETWORK_EARN_WAGE_PAYMENT_BONUS  - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 240000;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_WAGE_PAYMENT_BONUS - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	MetricEarnFromWagePayment m(amount, "WAGE_PAYMENT_BONUS");
	if(!EarnCash(amount, true, false, &m, StatId()))
	{
		scriptErrorf("%s: NETWORK_EARN_WAGE_PAYMENT_BONUS - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}


class MetricSpentChangeAppearance: public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentChangeAppearance(int amount, bool fromBank) : MetricSpend((s64)amount, fromBank) {}
	virtual const char*   GetActionName() const {return "CHANGE_APPEARANCE";}
	virtual const char* GetCategoryName() const {return "CHANGE_APPEARANCE";}

};

void CommandChangeAppearance(int amount, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_CHANGE_APPEARANCE", true););

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_CHANGE_APPEARANCE - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	MetricSpentChangeAppearance metric(amount, fromBank);

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_STYLE_ENT");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_STYLE_ENT", 0x49d67632);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &metric, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_CHANGE_APPEARANCE - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

class MetricSpentBallisticEquipment: public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentBallisticEquipment(int amount, bool fromBank) : MetricSpend((s64)amount, fromBank) {}
	virtual const char*   GetActionName() const {return "BALLISTIC_EQUIPMENT";}
	virtual const char* GetCategoryName() const {return "BALLISTIC_EQUIPMENT";}

};

void CommandSpentBallisticEquipment(int amount, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_BALLISTIC_EQUIPMENT", true););

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_BALLISTIC_EQUIPMENT - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	MetricSpentBallisticEquipment metric(amount, fromBank);

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_WEAPON_ARMOR");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_WEAPON_ARMOR", 0x82aa0d54);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &metric, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_BALLISTIC_EQUIPMENT - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}


//////////////////////////////////////////////////////////////////////////
//Christmas 2017

class MetricSpent_Christmas2017 : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:

	MetricSpent_Christmas2017(const char* action, const char* category, u32 item, const int amount, bool fromBank)
		: MetricSpend((s64)amount, fromBank)
		, m_item(item)
	{
		safecpy(m_action, action);
		safecpy(m_category, category);
	}

	virtual const char* GetActionName() const {return m_action;}
	virtual const char* GetCategoryName() const {return m_category;}
	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw)
			&& rw->WriteUns("item", m_item);
	}

private:
	static const int CATEGORY_AND_ACTION_LENGTH = 64;

private:
	u32 m_item;
	char m_action[CATEGORY_AND_ACTION_LENGTH];
	char m_category[CATEGORY_AND_ACTION_LENGTH];
};

void CommandSpentPlayerBase(const char* action, int amount, bool fromBank, bool fromBankAndWallet, srcSpentOnBase* data OUTPUT_ONLY(, const char* commandName))
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED(commandName, true););

	scriptAssertf(amount>=0, "%s: %s - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName, amount);
	if (amount<0)
		return;
	
	if(data->m_location_amount.Int >= 0)
	{
		MetricSpent_Christmas2017 m(action, "LOCATION", data->m_location.Int, data->m_location_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}

	if(data->m_style_amount.Int >= 0)
	{
		MetricSpent_Christmas2017 m(action, "STYLE", data->m_style.Int, data->m_style_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}

	if(data->m_graphics_amount.Int >= 0)
	{
		MetricSpent_Christmas2017 m(action, "GRAPHICS", data->m_graphics.Int, data->m_graphics_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}

	if(data->m_orbcannon_amount.Int >= 0)
	{
		MetricSpent_Christmas2017 m(action, "ORBITAL_CANNON", data->m_orbcannon.Int, data->m_orbcannon_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}

	if(data->m_secroom_amount.Int >= 0)
	{
		MetricSpent_Christmas2017 m(action, "SECURITY_ROOM", data->m_secroom.Int, data->m_secroom_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}

	if(data->m_lounge_amount.Int >= 0)
	{
		MetricSpent_Christmas2017 m(action, "LOUNGE", data->m_lounge.Int, data->m_lounge_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}

	if(data->m_livingquarter_amount.Int >= 0)
	{
		MetricSpent_Christmas2017 m(action, "LIVING_QUARTERS", data->m_livingquarter.Int, data->m_livingquarter_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}

	if(data->m_privacyglass_amount.Int >= 0)
	{
		MetricSpent_Christmas2017 m(action, "PRIVACY_GLASS", data->m_privacyglass.Int, data->m_privacyglass_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_PROPERTY_UTIL");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_PROPERTY_UTIL", 0x8526aff3);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
	{
		scriptErrorf("%s: %s - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
		return;
	}
}

void CommandSpentOnBase(int amount, bool fromBank, bool fromBankAndWallet, srcSpentOnBase* data)
{
	CommandSpentPlayerBase("BUY_BASE", amount, fromBank, fromBankAndWallet, data  OUTPUT_ONLY(, "NETWORK_SPENT_BUY_BASE"));
}

void CommandSpentUpgradeBase(int amount, bool fromBank, bool fromBankAndWallet, srcSpentOnBase* data)
{
	CommandSpentPlayerBase("UPGRADE_BASE", amount, fromBank, fromBankAndWallet, data  OUTPUT_ONLY(, "NETWORK_SPENT_UPGRADE_BASE"));
}

void CommandSpentTiltRotor(const char* action, int amount, bool fromBank, bool fromBankAndWallet, srcSpentTiltRotor* data  OUTPUT_ONLY(, const char* commandName))
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED(commandName, true););

	scriptAssertf(amount>=0, "%s: %s - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName, amount);
	if (amount<0)
		return;

	if(data->m_aircraft_amount.Int >= 0)
	{
		MetricSpent_Christmas2017 m(action, "AIRCRAFT", data->m_aircraft.Int, data->m_aircraft_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}

	if(data->m_interiortint_amount.Int >= 0)
	{
		MetricSpent_Christmas2017 m(action, "INTERIOR_TINT", data->m_interiortint.Int, data->m_interiortint_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}

	if(data->m_turret_amount.Int >= 0)
	{
		MetricSpent_Christmas2017 m(action, "TURRET", data->m_turret.Int, data->m_turret_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}

	if(data->m_weaponworkshop_amount.Int >= 0)
	{
		MetricSpent_Christmas2017 m(action, "WEAPON_WORKSHOP", data->m_weaponworkshop.Int, data->m_weaponworkshop_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}

	if(data->m_vehicleworkshop_amount.Int >= 0)
	{
		MetricSpent_Christmas2017 m(action, "VEHICLE_WORKSHOP", data->m_vehicleworkshop.Int, data->m_vehicleworkshop_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}

	if(data->m_countermeasures_amount.Int >= 0)
	{
		MetricSpent_Christmas2017 m(action, "COUNTER_MEASURES", data->m_countermeasures.Int, data->m_countermeasures_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}

	if(data->m_bombs_amount.Int >= 0)
	{
		MetricSpent_Christmas2017 m(action, "BOMBS", data->m_bombs.Int, data->m_bombs_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_PROPERTY_UTIL");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_PROPERTY_UTIL", 0x8526aff3);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
	{
		scriptErrorf("%s: %s - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
		return;
	}
}

void CommandSpentOnTiltRotor(int amount, bool fromBank, bool fromBankAndWallet, srcSpentTiltRotor* data)
{
	CommandSpentTiltRotor("BUY_TILTROTOR", amount, fromBank, fromBankAndWallet, data  OUTPUT_ONLY(, "NETWORK_SPENT_BUY_TILTROTOR"));
}

void CommandSpentUpgradeTiltRotor(int amount, bool fromBank, bool fromBankAndWallet, srcSpentTiltRotor* data)
{
	CommandSpentTiltRotor("UPGRADE_TILTROTOR", amount, fromBank, fromBankAndWallet, data  OUTPUT_ONLY(, "NETWORK_SPENT_UPGRADE_TILTROTOR"));
}

class MetricSpentEmployAssassins: public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricSpentEmployAssassins(int amount, bool fromBank, int level) : MetricSpend((s64)amount, fromBank), m_level(level) {}
	virtual const char*   GetActionName() const {return "EMPLOY_ASSASSINS";}
	virtual const char* GetCategoryName() const {return "EMPLOY_ASSASSINS";}

	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw)
			&& rw->WriteUns("item", m_level);
	}

	int m_level;
};


void CommandSpentEmployAssassins(int amount, bool fromBank, bool fromBankAndWallet, int level)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPENT_EMPLOY_ASSASSINS", true););

	scriptAssertf(amount>=0, "%s: NETWORK_SPENT_EMPLOY_ASSASSINS - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	MetricSpentEmployAssassins metric(amount, fromBank, level);

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_CONTACT_SERVICE");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_CONTACT_SERVICE", 0x7ee6a4b0);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &metric, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPENT_EMPLOY_ASSASSINS - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandSpentHeistCannon(int amount, bool fromBank, bool fromBankAndWallet, int shoottype)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPEND_GANGOPS_CANNON", true););

	scriptAssertf(amount>=0, "%s: NETWORK_SPEND_GANGOPS_CANNON - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<0)
		return;

	MetricSpentHeistCannon m(amount, fromBank, shoottype);

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_WEAPON_ARMOR");
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, -1))
	{
		scriptErrorf("%s: NETWORK_SPEND_GANGOPS_CANNON - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandSpentHeistMission(int mission, int amount, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPEND_GANGOPS_SKIP_MISSION", true););

	scriptAssertf(amount >= 0, "%s: NETWORK_SPEND_GANGOPS_SKIP_MISSION - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount < 0)
		return;

	MetricSpentHeistMission m(mission, amount, fromBank);

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_JOB_ACTIVITY");
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, -1))
	{
		scriptErrorf("%s: NETWORK_SPEND_GANGOPS_SKIP_MISSION - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandSpentCasinoHeistSkipMission(int mission, int amount, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPEND_CASINO_HEIST_SKIP_MISSION", true););

	scriptAssertf(amount >= 0, "%s: NETWORK_SPEND_CASINO_HEIST_SKIP_MISSION - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount < 0)
		return;

	MetricSpentCasinoHeistSkipMission m(mission, amount, fromBank);

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_JOB_ACTIVITY");
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, -1))
	{
		scriptErrorf("%s: NETWORK_SPEND_CASINO_HEIST_SKIP_MISSION - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnSellBase(int amount, int baseHash)
{	
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_SELL_BASE", true);)

	scriptAssertf(amount>0, "%s: NETWORK_EARN_SELL_BASE  - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 4000000;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_SELL_BASE - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	MetricEarnFromSellBase m(amount, baseHash);

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_PROPERTY_TRADE");
	if(!EarnCash(amount, true, false, &m, statId))
	{
		scriptErrorf("%s: NETWORK_EARN_SELL_BASE - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnTargetRefund(int amount, int target)
{	
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_TARGET_REFUND", true);)

	scriptAssertf(amount>0, "%s: NETWORK_EARN_TARGET_REFUND  - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 750000;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_TARGET_REFUND - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	MetricEarnFromTargetRefund m(amount, target);

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_REFUND");
	if(!EarnCash(amount, true, false, &m, statId))
	{
		scriptErrorf("%s: NETWORK_EARN_TARGET_REFUND - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnHeistWages(int amount, int contentid)
{	
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_GANGOPS_WAGES", true);)

	scriptAssertf(amount>0, "%s: NETWORK_EARN_GANGOPS_WAGES  - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 240000;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_GANGOPS_WAGES - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	MetricEarnFromHeistWages m(amount, contentid);

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if(!EarnCash(amount, true, false, &m, statId, ATSTRINGHASH("MONEY_EARN_FROM_JOB_BONUS", 0xad06c80b)))
	{
		scriptErrorf("%s: NETWORK_EARN_GANGOPS_WAGES - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnHeistWagesBonus(int amount, int contentid)
{	
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_GANGOPS_WAGES_BONUS", true);)

	scriptAssertf(amount>0, "%s: NETWORK_EARN_GANGOPS_WAGES_BONUS  - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 240000;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_GANGOPS_WAGES_BONUS - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	MetricEarnFromHeistWagesBonus m(amount, contentid);

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if(!EarnCash(amount, true, false, &m, statId, ATSTRINGHASH("MONEY_EARN_FROM_HEIST_JOB", 0xadda0055)))
	{
		scriptErrorf("%s: NETWORK_EARN_GANGOPS_WAGES_BONUS - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnDarChallenge(int amount, int contentid)
{	
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_DAR_CHALLENGE", true);)

		scriptAssertf(amount>0, "%s: NETWORK_EARN_DAR_CHALLENGE  - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 750000;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_DAR_CHALLENGE - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	MetricEarnFromDarChallenge m(amount, contentid);

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if(!EarnCash(amount, true, false, &m, statId))
	{
		scriptErrorf("%s: NETWORK_EARN_DAR_CHALLENGE - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnDoomsdayFinaleBonus(int amount, int vehiclemodel)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_DOOMSDAY_FINALE_BONUS", true);)

	scriptAssertf(amount > 0, "%s: NETWORK_EARN_DOOMSDAY_FINALE_BONUS  - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount <= 0)
		return;

	static const int AMOUNT_LIMIT = 200000;
	scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_DOOMSDAY_FINALE_BONUS - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount > AMOUNT_LIMIT)
		return;

	MetricEarnFromDoomsdayFinaleBonus m(amount, vehiclemodel);

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if (!EarnCash(amount, true, false, &m, statId))
	{
		scriptErrorf("%s: NETWORK_EARN_DOOMSDAY_FINALE_BONUS - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

enum GangopsAwardType
{
	GANGOPS_AWARD_MASTERMIND_2
	,GANGOPS_AWARD_MASTERMIND_3
	,GANGOPS_AWARD_MASTERMIND_4
	,GANGOPS_AWARD_LOYALTY_AWARD_2
	,GANGOPS_AWARD_LOYALTY_AWARD_3
	,GANGOPS_AWARD_LOYALTY_AWARD_4
	,GANGOPS_AWARD_FIRST_TIME_XM_BASE
	,GANGOPS_AWARD_FIRST_TIME_XM_SUBMARINE
	,GANGOPS_AWARD_FIRST_TIME_XM_SILO
	,GANGOPS_AWARD_SUPPORTING
	,GANGOPS_AWARD_ORDER
};

void CommandEarnGangopsAward(int amount, const char* uniqueMatchId, int challenge)
{	
	int AMOUNT_LIMIT = 0;

	switch( (GangopsAwardType) challenge)
	{
		case GANGOPS_AWARD_MASTERMIND_2: AMOUNT_LIMIT = 3000000; break;
		case GANGOPS_AWARD_MASTERMIND_3: AMOUNT_LIMIT = 7000000; break;
		case GANGOPS_AWARD_MASTERMIND_4: AMOUNT_LIMIT = 15000000; break;
		case GANGOPS_AWARD_LOYALTY_AWARD_2: AMOUNT_LIMIT = 300000; break;
		case GANGOPS_AWARD_LOYALTY_AWARD_3: AMOUNT_LIMIT = 700000; break;
		case GANGOPS_AWARD_LOYALTY_AWARD_4: AMOUNT_LIMIT = 1500000; break;
		case GANGOPS_AWARD_FIRST_TIME_XM_BASE: AMOUNT_LIMIT = 200000; break;
		case GANGOPS_AWARD_FIRST_TIME_XM_SUBMARINE: AMOUNT_LIMIT = 200000; break;
		case GANGOPS_AWARD_FIRST_TIME_XM_SILO: AMOUNT_LIMIT = 200000; break;
		case GANGOPS_AWARD_SUPPORTING: AMOUNT_LIMIT = 200000; break;
		case GANGOPS_AWARD_ORDER: AMOUNT_LIMIT = 2000000; break;
		default: scriptAssertf(0, "%s: NETWORK_EARN_GANGOPS_AWARD - Unhandled GangopsAwardType", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_GANGOPS_AWARD - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	EarnMoneyFromGangOps(amount, uniqueMatchId, challenge, "AWARDS");
}

enum GangopsEliteType
{
	GANGOPS_ELITE_XM_BASE
	,GANGOPS_ELITE_XM_SUBMARINE
	,GANGOPS_ELITE_XM_SILO
};

void CommandEarnGangopsElite(int amount, const char* uniqueMatchId, int challenge)
{	
	int AMOUNT_LIMIT = 0;

	switch( (GangopsEliteType) challenge)
	{
	case GANGOPS_ELITE_XM_BASE: AMOUNT_LIMIT = 100000; break;
	case GANGOPS_ELITE_XM_SUBMARINE: AMOUNT_LIMIT = 150000; break;
	case GANGOPS_ELITE_XM_SILO: AMOUNT_LIMIT = 200000; break;
	default: scriptAssertf(0, "%s: NETWORK_EARN_GANGOPS_ELITE - Unhandled GangopsEliteType", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_GANGOPS_ELITE - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	EarnMoneyFromGangOps(amount, uniqueMatchId, challenge, "ELITE");
}

void CommandSpendGangOpsStartStrand(int strand, int amount, bool fromBank, bool fromBankAndWallet)
{	
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPEND_GANGOPS_START_STRAND", true);)

		scriptAssertf(amount>0, "%s: NETWORK_SPEND_GANGOPS_START_STRAND  - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_JOB_ACTIVITY", 0xda4ea4d7);
	MetricSpendGangOpsStartStrand m(strand, amount, fromBank);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_JOB_ACTIVITY");

	if (!SpendCash(amount, fromBank, false, fromBankAndWallet, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPEND_GANGOPS_START_STRAND - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandSpendGangOpsTripSkip(int amount, bool fromBank, bool fromBankAndWallet)
{	
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPEND_GANGOPS_TRIP_SKIP", true);)

	scriptAssertf(amount>0, "%s: NETWORK_SPEND_GANGOPS_TRIP_SKIP  - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_JOB_ACTIVITY", 0xda4ea4d7);
	MetricSpendGangOpsTripSkip m(amount, fromBank);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_JOB_ACTIVITY");

	if (!SpendCash(amount, fromBank, false, fromBankAndWallet, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPEND_GANGOPS_TRIP_SKIP - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}
void CommandEarnGangOpsPrepParticipation(int amount)
{	
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_GANGOPS_PREP_PARTICIPATION", true);)

	scriptAssertf(amount>0, "%s: NETWORK_EARN_GANGOPS_PREP_PARTICIPATION  - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 15000;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_GANGOPS_PREP_PARTICIPATION - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	MetricEarnFromGangOpsJobs m(amount, "PREP_PARTICIPATION", 0);

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if(!EarnCash(amount, true, false, &m, statId, ATSTRINGHASH("MONEY_EARN_FROM_JOB_BONUS", 0xad06c80b)))
	{
		scriptErrorf("%s: NETWORK_EARN_GANGOPS_PREP_PARTICIPATION - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}
void CommandEarnGangOpsSetup(int amount, const char* contentid)
{	
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_GANGOPS_SETUP", true);)

	scriptAssertf(contentid, "%s: NETWORK_EARN_GANGOPS_SETUP  - NULL contentId.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	scriptAssertf(amount>0, "%s: NETWORK_EARN_GANGOPS_SETUP  - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 90000;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_GANGOPS_SETUP - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	MetricEarnFromGangOpsJobs m(amount, "SETUP", contentid);

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if(!EarnCash(amount, true, false, &m, statId, ATSTRINGHASH("MONEY_EARN_FROM_HEIST_JOB", 0xadda0055)))
	{
		scriptErrorf("%s: NETWORK_EARN_GANGOPS_SETUP - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}
void CommandEarnGangOpsFinale(int amount, const char* contentid)
{	
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_GANGOPS_FINALE", true);)

	scriptAssertf(contentid, "%s: NETWORK_EARN_GANGOPS_FINALE  - NULL contentId.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	scriptAssertf(amount>0, "%s: NETWORK_EARN_GANGOPS_FINALE  - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;

	static const int AMOUNT_LIMIT = 2550000;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_GANGOPS_FINALE - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	MetricEarnFromGangOpsJobs m(amount, "FINALE", contentid);

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if(!EarnCash(amount, true, false, &m, statId, ATSTRINGHASH("MONEY_EARN_FROM_HEIST_JOB", 0xadda0055)))
	{
		scriptErrorf("%s: NETWORK_EARN_GANGOPS_FINALE - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnNightClub(int amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_NIGHTCLUB", true);)

		scriptAssertf(amount>0, "%s: NETWORK_EARN_NIGHTCLUB  - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;
	static const int AMOUNT_LIMIT = 250000;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_NIGHTCLUB - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	MetricEarnNightClub m(amount);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_NIGHTCLUB_INCOME");
	if(!EarnCash(amount, false, false, &m, statId))
	{
		scriptErrorf("%s: NETWORK_EARN_NIGHTCLUB - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnBBEventBonus(int amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_BB_EVENT_BONUS", true);)

		scriptAssertf(amount>0, "%s: NETWORK_EARN_BB_EVENT_BONUS  - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;
	static const int AMOUNT_LIMIT = 200000;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_BB_EVENT_BONUS - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	MetricEarnBBEventBonus m(amount);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if(!EarnCash(amount, false, false, &m, statId))
	{
		scriptErrorf("%s: NETWORK_EARN_BB_EVENT_BONUS - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}


void CommandEarnNightclubDancing(int amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_NIGHTCLUB_DANCING", true);)

		scriptAssertf(amount>0, "%s: NETWORK_EARN_NIGHTCLUB_DANCING  - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return;
	static const int AMOUNT_LIMIT = 60000;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_NIGHTCLUB_DANCING - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	MetricEarnNightClubDancing m(amount);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_CLUB_DANCING");
	if(!EarnCash(amount, true, false, &m, statId))
	{
		scriptErrorf("%s: NETWORK_EARN_NIGHTCLUB_DANCING - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnMoneyFromGangOpsRivalDelivery(int amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("SERVICE_EARN_GANGOPS_RIVAL_DELIVERY", true);)

		scriptAssertf(amount > 0, "%s: SERVICE_EARN_GANGOPS_RIVAL_DELIVERY  - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount <= 0)
		return;

	static const int AMOUNT_LIMIT = 10000;
	scriptAssertf(amount <= AMOUNT_LIMIT, "%s: SERVICE_EARN_GANGOPS_RIVAL_DELIVERY - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount > AMOUNT_LIMIT)
		return;

	MetricEarnFromGangOpsJobs m(amount, "RIVAL_DELIVERY", 0);

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if (!EarnCash(amount, true, false, &m, statId, ATSTRINGHASH("MONEY_EARN_FROM_JOB_BONUS", 0xad06c80b)))
	{
		scriptErrorf("%s: SERVICE_EARN_GANGOPS_RIVAL_DELIVERY - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

////////////////////////////////////////////////////////////////////////// MEGABUSINESS


#define APPEND_SPENT_AMOUNT(_name, _field)																											\
if (data->_field##_amount.Int >= 0)																													\
{																																					\
	ASSERT_ONLY(total += data->_field##_amount.Int; );																								\
	MetricSpent_MegaBusiness m(action, _name, data->_field.Int, data->_field##_amount.Int, fromBank);												\
	ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);																								\
	scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);	\
}

class MetricSpent_MegaBusiness : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:

	MetricSpent_MegaBusiness(const char* action, const char* category, u32 item, const int amount, bool fromBank)
		: MetricSpend((s64)amount, fromBank)
		, m_item(item)
		, m_bossId(StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID")))
	{
		safecpy(m_action, action);
		safecpy(m_category, category);
	}

	virtual const char* GetActionName() const { return m_action; }
	virtual const char* GetCategoryName() const { return m_category; }
	virtual bool WriteContext(RsonWriter* rw) const
	{
		return MetricSpend::WriteContext(rw)
			&& rw->WriteUns("item", m_item)
			&& rw->WriteInt64("uuid", m_bossId);
	}

private:
	static const int CATEGORY_AND_ACTION_LENGTH = 64;

private:
	u32 m_item;
	char m_action[CATEGORY_AND_ACTION_LENGTH];
	char m_category[CATEGORY_AND_ACTION_LENGTH];
	u64 m_bossId;
};

void SpentOnHackerTruck(int amount, const char* action, srcSpendHackerTruck* data, bool fromBank, bool fromBankAndWallet OUTPUT_ONLY(, const char* commandName))
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED(commandName, true);)

	scriptAssertf(amount >= 0, "%s: %s - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName, amount);
	if (amount < 0)
		return;

#if __NET_SHOP_ACTIVE
	const s64 nonceSeed = GameTransactionSessionMgr::Get().GetNonceForTelemetry();
#endif // __NET_SHOP_ACTIVE

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_PROPERTY_UTIL");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_PROPERTY_UTIL", 0x8526aff3);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
	{
		scriptErrorf("%s: %s - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}
	else
	{
#if __NET_SHOP_ACTIVE
		GameTransactionSessionMgr::Get().SetNonceForTelemetry(nonceSeed);
#endif // __NET_SHOP_ACTIVE

		// We need to send more than one metric, so we pass NULL to SpendCash and send the metric ourselves
		ASSERT_ONLY(int total = 0; );

		APPEND_SPENT_AMOUNT("TRUCK", m_truck);
		APPEND_SPENT_AMOUNT("TINT", m_tint);
		APPEND_SPENT_AMOUNT("PATTERN", m_pattern);
		APPEND_SPENT_AMOUNT("MISSILE_LAUNCHER", m_missileLauncher);
		APPEND_SPENT_AMOUNT("DRONE_STATION", m_droneStation);
		APPEND_SPENT_AMOUNT("WEAPON_WORKSHOP", m_weaponWorkshop);
		APPEND_SPENT_AMOUNT("BIKE", m_bike);
		APPEND_SPENT_AMOUNT("BIKE_WORKSHOP", m_bikeWorkshop);

		scriptAssertf(amount == total, "%s: %s - Given amount and total dont match (amount = %d   |   total = %d)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName, amount, total);

#if __NET_SHOP_ACTIVE
		GameTransactionSessionMgr::Get().SetNonceForTelemetry(0);
#endif // __NET_SHOP_ACTIVE
	}
}

void CommandSpendHackerTruck(int amount, srcSpendHackerTruck* data, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("SERVICE_SPEND_HACKER_TRUCK", true););
	SpentOnHackerTruck(amount, "BUY_HACKER", data, fromBank, fromBankAndWallet   OUTPUT_ONLY(, "NETWORK_SPENT_PURCHASE_HACKER_TRUCK"));
}

void CommandUpgradeHackerTruck(int amount, srcSpendHackerTruck* data, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("SERVICE_UPGRADE_HACKER_TRUCK", true););
	SpentOnHackerTruck(amount, "UPGRADE_HACKER", data, fromBank, fromBankAndWallet   OUTPUT_ONLY(, "NETWORK_SPENT_UPGRADE_HACKER_TRUCK"));
}



class MetricEarnFromMegaBusiness : public MetricEarn
{
	RL_DECLARE_METRIC(EARN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricEarnFromMegaBusiness(const int amount, const char* action, const char* category, int value)
		: MetricEarn((s64)amount)
		, m_value(value)
	{
		m_action[0] = '\0';
		if (AssertVerify(action))
		{
			safecpy(m_action, action, COUNTOF(m_action));
		}
		m_category[0] = '\0';
		if (AssertVerify(category))
		{
			safecpy(m_category, category, COUNTOF(m_category));
		}
	}

	virtual const char*   GetActionName() const { return m_action; }
	virtual const char* GetCategoryName() const { return m_category; }

	virtual bool WriteContext(RsonWriter* rw) const
	{
		bool result = true;
		result = result && MetricEarn::WriteContext(rw);
		result = result && rw->WriteUns("id", m_value);
		return result;
	}

private:
	static const int MAX_STRING_LENGTH = 32;
	char m_action[MAX_STRING_LENGTH];
	char m_category[MAX_STRING_LENGTH];
	int m_value;
};

void CommandEarnHackerTruck(int amount, int mission, int missionRival, int missionSolo)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("SERVICE_EARN_GANGOPS_RIVAL_DELIVERY", true);)

	scriptAssertf(amount >= 0, "%s: NETWORK_EARN_HACKER_TRUCK  - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount < 0)
		return;

	if (mission)
	{
		static const int MISSION_AMOUNT_LIMIT = 120000;
		scriptAssertf(mission <= MISSION_AMOUNT_LIMIT, "%s: NETWORK_EARN_HACKER_TRUCK - invalid money amount for mission < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), mission, MISSION_AMOUNT_LIMIT);
		if (mission > MISSION_AMOUNT_LIMIT)
			return;

		MetricEarnFromMegaBusiness m(amount, "HACKER_TRUCK", "MISSION", mission);

		const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
		if (!EarnCash(amount, true, false, &m, statId, ATSTRINGHASH("MONEY_EARN_FROM_JOB_BONUS", 0xad06c80b)))
		{
			scriptErrorf("%s: NETWORK_EARN_HACKER_TRUCK - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
	else if (missionRival)
	{
		static const int MISSION_RIVAL_AMOUNT_LIMIT = 120000;
		scriptAssertf(missionRival <= MISSION_RIVAL_AMOUNT_LIMIT, "%s: NETWORK_EARN_HACKER_TRUCK - invalid money amount for missionRival < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), missionRival, MISSION_RIVAL_AMOUNT_LIMIT);
		if (missionRival > MISSION_RIVAL_AMOUNT_LIMIT)
			return;

		MetricEarnFromMegaBusiness m(amount, "HACKER_TRUCK", "MISSION_RIVAL", missionRival);

		const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
		if (!EarnCash(amount, true, false, &m, statId, ATSTRINGHASH("MONEY_EARN_FROM_JOB_BONUS", 0xad06c80b)))
		{
			scriptErrorf("%s: NETWORK_EARN_HACKER_TRUCK - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
	else if (missionSolo)
	{
		static const int MISSION_SOLO_AMOUNT_LIMIT = 120000;
		scriptAssertf(missionSolo <= MISSION_SOLO_AMOUNT_LIMIT, "%s: NETWORK_EARN_HACKER_TRUCK - invalid money amount for missionSolo < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), missionSolo, MISSION_SOLO_AMOUNT_LIMIT);
		if (missionSolo > MISSION_SOLO_AMOUNT_LIMIT)
			return;

		MetricEarnFromMegaBusiness m(amount, "HACKER_TRUCK", "MISSION_SOLO", missionSolo);

		const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
		if (!EarnCash(amount, true, false, &m, statId, ATSTRINGHASH("MONEY_EARN_FROM_JOB_BONUS", 0xad06c80b)))
		{
			scriptErrorf("%s: NETWORK_EARN_HACKER_TRUCK - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
	else
	{
		scriptAssertf(0, "%s: NETWORK_EARN_HACKER_TRUCK  - No valid mission hash.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void SpentOnNightclubAndWarehouse(int amount, const char* action, srcSpendNightClubAndWarehouse* data, bool fromBank, bool fromBankAndWallet OUTPUT_ONLY(, const char* commandName))
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED(commandName, true);)

	scriptAssertf(amount >= 0, "%s: %s - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName, amount);
	if (amount < 0)
		return;

#if __NET_SHOP_ACTIVE
	const s64 nonceSeed = GameTransactionSessionMgr::Get().GetNonceForTelemetry();
#endif // __NET_SHOP_ACTIVE

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_PROPERTY_UTIL");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_PROPERTY_UTIL", 0x8526aff3);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
	{
		scriptErrorf("%s: %s - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}
	else
	{
#if __NET_SHOP_ACTIVE
		GameTransactionSessionMgr::Get().SetNonceForTelemetry(nonceSeed);
#endif // __NET_SHOP_ACTIVE

		// We need to send more than one metric, so we pass NULL to SpendCash and send the metric ourselves
		ASSERT_ONLY(int total = 0; );
		
		APPEND_SPENT_AMOUNT("LOCATION", m_location);
		APPEND_SPENT_AMOUNT("DJ", m_dj);
		APPEND_SPENT_AMOUNT("STYLE", m_style);
		APPEND_SPENT_AMOUNT("TINT", m_tint);
		APPEND_SPENT_AMOUNT("LIGHTING", m_lighting);
		APPEND_SPENT_AMOUNT("STAFF", m_staff);
		APPEND_SPENT_AMOUNT("SECURITY", m_security);
		APPEND_SPENT_AMOUNT("EQUIPMENT", m_equipment);
		APPEND_SPENT_AMOUNT("WH_GARAGE2", m_whGarage2);
		APPEND_SPENT_AMOUNT("WH_GARAGE3", m_whGarage3);
		APPEND_SPENT_AMOUNT("WH_GARAGE4", m_whGarage4);
		APPEND_SPENT_AMOUNT("WH_GARAGE5", m_whGarage5);
		APPEND_SPENT_AMOUNT("WH_BASEMENT2", m_whBasement2);
		APPEND_SPENT_AMOUNT("WH_BASEMENT3", m_whBasement3);
		APPEND_SPENT_AMOUNT("WH_BASEMENT4", m_whBasement4);
		APPEND_SPENT_AMOUNT("WH_BASEMENT5", m_whBasement5);
		APPEND_SPENT_AMOUNT("WH_TECHNICIAN2", m_whTechnician2);
		APPEND_SPENT_AMOUNT("WH_TECHNICIAN3", m_whTechnician3);
		APPEND_SPENT_AMOUNT("WH_TECHNICIAN4", m_whTechnician4);
		APPEND_SPENT_AMOUNT("WH_TECHNICIAN5", m_whTechnician5);
		APPEND_SPENT_AMOUNT("NAME", m_name);
		APPEND_SPENT_AMOUNT("PODIUM", m_podium);
		APPEND_SPENT_AMOUNT("DRYICE", m_dryice);

		scriptAssertf(amount == total, "%s: %s - Given amount and total dont match (amount = %d   |   total = %d)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName, amount, total);

#if __NET_SHOP_ACTIVE
		GameTransactionSessionMgr::Get().SetNonceForTelemetry(0);
#endif // __NET_SHOP_ACTIVE
	}
}

void CommandSpendNightclubAndWarehouse(int amount, srcSpendNightClubAndWarehouse* data, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("SERVICE_SPEND_NIGHTCLUB_AND_WAREHOUSE", true););
	SpentOnNightclubAndWarehouse(amount, "BUY_NIGHTCLUB", data, fromBank, fromBankAndWallet   OUTPUT_ONLY(, "NETWORK_SPENT_PURCHASE_NIGHTCLUB_AND_WAREHOUSE"));
}

void CommandUpgradeNightclubAndWarehouse(int amount, srcSpendNightClubAndWarehouse* data, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("SERVICE_UPGRADE_NIGHTCLUB_AND_WAREHOUSE", true););
	SpentOnNightclubAndWarehouse(amount, "UPGRADE_NIGHTCLUB", data, fromBank, fromBankAndWallet   OUTPUT_ONLY(, "NETWORK_SPENT_UPGRADE_NIGHTCLUB_AND_WAREHOUSE"));
}

void CommandEarnNightclubAndWarehouse(int amount, int location, int popularityEarnings, int mission, int rivalNcEarned, int sellContraband, int sellContrabandBonus)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_NIGHTCLUB_AND_WAREHOUSE", true);)

	if (location)
	{
		static const int AMOUNT_LOCATION_LIMIT = 2750000;
		scriptAssertf(amount <= AMOUNT_LOCATION_LIMIT, "%s: NETWORK_EARN_NIGHTCLUB_AND_WAREHOUSE - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LOCATION_LIMIT);
		if (amount > AMOUNT_LOCATION_LIMIT)
			return;

		MetricEarnFromMegaBusiness m(amount, "SELL_NIGHTCLUB", "LOCATION", location);

		const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_PROPERTY_TRADE");
		if (!EarnCash(amount, true, false, &m, statId, ATSTRINGHASH("MONEY_EARN_FROM_JOB_BONUS", 0xad06c80b)))
		{
			scriptErrorf("%s: NETWORK_EARN_HACKER_TRUCK - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
	else if (popularityEarnings)
	{
		static const int AMOUNT_POP_EARNING_LIMIT = 210000;
		scriptAssertf(amount <= AMOUNT_POP_EARNING_LIMIT, "%s: NETWORK_EARN_NIGHTCLUB_AND_WAREHOUSE - invalid money amount AMOUNT_POP_EARNING_LIMIT < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_POP_EARNING_LIMIT);
		if (amount > AMOUNT_POP_EARNING_LIMIT)
			return;

		MetricEarnFromMegaBusiness m(amount, "NIGHTCLUB_OPS", "POPULARITY_EARNINGS", popularityEarnings);

		const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_NIGHTCLUB_INCOME");
		if (!EarnCash(amount, true, false, &m, statId, ATSTRINGHASH("MONEY_EARN_FROM_JOB_BONUS", 0xad06c80b)))
		{
			scriptErrorf("%s: NETWORK_EARN_NIGHTCLUB_AND_WAREHOUSE - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
	else if (mission)
	{
		static const int AMOUNT_MISSION_LIMIT = 30000;
		scriptAssertf(amount <= AMOUNT_MISSION_LIMIT, "%s: NETWORK_EARN_NIGHTCLUB_AND_WAREHOUSE - invalid money amount AMOUNT_MISSION_LIMIT < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_MISSION_LIMIT);
		if (amount > AMOUNT_MISSION_LIMIT)
			return;

		MetricEarnFromMegaBusiness m(amount, "NIGHTCLUB_OPS", "MISSION", mission);

		const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
		if (!EarnCash(amount, true, false, &m, statId, ATSTRINGHASH("MONEY_EARN_FROM_JOB_BONUS", 0xad06c80b)))
		{
			scriptErrorf("%s: NETWORK_EARN_NIGHTCLUB_AND_WAREHOUSE - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
	else if (rivalNcEarned)
	{
		static const int AMOUNT_RIVALNC_LIMIT = 6000;
		scriptAssertf(amount <= AMOUNT_RIVALNC_LIMIT, "%s: NETWORK_EARN_NIGHTCLUB_AND_WAREHOUSE - invalid money amount AMOUNT_RIVALNC_LIMIT < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_RIVALNC_LIMIT);
		if (amount > AMOUNT_RIVALNC_LIMIT)
			return;

		MetricEarnFromMegaBusiness m(amount, "NIGHTCLUB_OPS", "RIVAL_NC_EARN", rivalNcEarned);

		const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
		if (!EarnCash(amount, true, false, &m, statId, ATSTRINGHASH("MONEY_EARN_FROM_JOB_BONUS", 0xad06c80b)))
		{
			scriptErrorf("%s: NETWORK_EARN_NIGHTCLUB_AND_WAREHOUSE - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
	else if (sellContraband)
	{
		static const int AMOUNT_CONTRABAND_LIMIT = 6350000;
		scriptAssertf(amount <= AMOUNT_CONTRABAND_LIMIT, "%s: NETWORK_EARN_NIGHTCLUB_AND_WAREHOUSE - invalid money amount AMOUNT_CONTRABAND_LIMIT < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_CONTRABAND_LIMIT);
		if (amount > AMOUNT_CONTRABAND_LIMIT)
			return;

		MetricEarnFromMegaBusiness m(amount, "NIGHTCLUB_OPS", "SELL_CONTRABAND", sellContraband);

		const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_SELL_NC_GOODS");
		if (!EarnCash(amount, true, false, &m, statId, ATSTRINGHASH("MONEY_EARN_FROM_JOB_BONUS", 0xad06c80b)))
		{
			scriptErrorf("%s: NETWORK_EARN_NIGHTCLUB_AND_WAREHOUSE - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
	else if (sellContrabandBonus)
	{
		static const int AMOUNT_CONTRABAND_BONUS_LIMIT = 1250000;
		scriptAssertf(amount <= AMOUNT_CONTRABAND_BONUS_LIMIT, "%s: NETWORK_EARN_NIGHTCLUB_AND_WAREHOUSE - invalid money amount AMOUNT_CONTRABAND_BONUS_LIMIT < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_CONTRABAND_BONUS_LIMIT);
		if (amount > AMOUNT_CONTRABAND_BONUS_LIMIT)
			return;

		MetricEarnFromMegaBusiness m(amount, "NIGHTCLUB_OPS", "SELL_CONTRABAND_BONUS", sellContrabandBonus);

		const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_SELL_NC_BONUS");
		if (!EarnCash(amount, true, false, &m, statId, ATSTRINGHASH("MONEY_EARN_FROM_JOB_BONUS", 0xad06c80b)))
		{
			scriptErrorf("%s: NETWORK_EARN_NIGHTCLUB_AND_WAREHOUSE - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
	else
	{
		scriptAssertf(0, "%s: NETWORK_EARN_NIGHTCLUB_AND_WAREHOUSE  - No valid earn hash.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void NightClubSpend(const char* action, const char* category, const StatId& statId, int amount, bool fromBank, bool fromBankAndWallet  ASSERT_ONLY(, const char* commandName))
{
#if __NET_SHOP_ACTIVE
	const s64 nonceSeed = GameTransactionSessionMgr::Get().GetNonceForTelemetry();
#endif // __NET_SHOP_ACTIVE

	static const int itemId = ATSTRINGHASH("MONEY_SPENT_PROPERTY_UTIL", 0x8526aff3);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
	{
		ASSERT_ONLY(scriptErrorf("%s: %s - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);)
	}
	else
	{
#if __NET_SHOP_ACTIVE
		GameTransactionSessionMgr::Get().SetNonceForTelemetry(nonceSeed);
#endif // __NET_SHOP_ACTIVE

		MetricSpent_MegaBusiness m(action, category, 0, amount, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);

#if __NET_SHOP_ACTIVE
		GameTransactionSessionMgr::Get().SetNonceForTelemetry(0);
#endif // __NET_SHOP_ACTIVE
	}
}

void CommandSpendMiscNightclub(int attendant, int entryFee, bool fromBank, bool fromBankAndWallet)
{
	ASSERT_ONLY(const char* commandName = "NETWORK_SPEND_NIGHTCLUB_AND_WAREHOUSE");

	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPEND_NIGHTCLUB_AND_WAREHOUSE", true););

	if (attendant)
	{
		NightClubSpend("NIGHTCLUB", "BATHROOM_ATTENDANT", StatsInterface::GetStatsModelHashId("MONEY_SPENT_STYLE_ENT"), attendant, fromBank, fromBankAndWallet  ASSERT_ONLY(, commandName));
	}
	if (entryFee)
	{
		NightClubSpend("NIGHTCLUB", "DAILY_STAFF_COSTS", StatsInterface::GetStatsModelHashId("MONEY_SPENT_STYLE_ENT"), entryFee, fromBank, fromBankAndWallet  ASSERT_ONLY(, commandName));
	}
}

void CommandSpendRdrHatchetBonus(int amount, bool fromBank, bool fromBankAndWallet)
{
	ASSERT_ONLY(const char* commandName = "NETWORK_SPEND_RDR_HATCHET_BONUS");
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPEND_RDR_HATCHET_BONUS", true););
	scriptAssertf(amount > 0, "%s: %s - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName, amount);
	if (amount <= 0)
		return;

	static const int AMOUNT_LIMIT = 750000;
	scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_SPEND_RDR_HATCHET_BONUS - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount > AMOUNT_LIMIT)
		return;

#if __NET_SHOP_ACTIVE
	const s64 nonceSeed = GameTransactionSessionMgr::Get().GetNonceForTelemetry();
#endif // __NET_SHOP_ACTIVE

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_PROPERTY_UTIL", 0x8526aff3);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
	{
		ASSERT_ONLY(scriptErrorf("%s: %s - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);)
	}
	else
	{
#if __NET_SHOP_ACTIVE
		GameTransactionSessionMgr::Get().SetNonceForTelemetry(nonceSeed);
#endif // __NET_SHOP_ACTIVE

		MetricSpent_MegaBusiness m("RDRHATCHET_BONUS", "HATCHET_BONUS", 0, amount, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);										
		scriptAssertf(appendResult, "%s: %s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);

#if __NET_SHOP_ACTIVE
		GameTransactionSessionMgr::Get().SetNonceForTelemetry(0);
#endif // __NET_SHOP_ACTIVE
	}
}

//////////////////////////////////////////////////////////////////////////  ARENA WARS

void CommandEarnArenaSkillLevelProgression(int amount, int level)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_ARENA_SKILL_LEVEL_PROGRESSION", true));

	static const int AMOUNT_LIMIT = 100000;
	scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_ARENA_SKILL_LEVEL_PROGRESSION - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount > AMOUNT_LIMIT)
		return;

	MetricEarnFromArenaSkillLevelProgression m(amount, level);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if (!EarnCash(amount, true, false, &m, statId))
	{
		scriptErrorf("%s: NETWORK_EARN_ARENA_SKILL_LEVEL_PROGRESSION - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnArenaCareerProgression(int amount, int tier)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_ARENA_CAREER_PROGRESSION", true));

	static const int AMOUNT_LIMIT = 200000;
	scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_ARENA_CAREER_PROGRESSION - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount > AMOUNT_LIMIT)
		return;

	MetricEarnFromArenaCareerProgression m(amount, tier);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if (!EarnCash(amount, true, false, &m, statId))
	{
		scriptErrorf("%s: NETWORK_EARN_ARENA_CAREER_PROGRESSION - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

class MetricSpendMakeItRain : public MetricSpend
{
	RL_DECLARE_METRIC(SPEND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

public:

	MetricSpendMakeItRain(const int amount, bool fromBank)
		: MetricSpend((s64)amount, fromBank)
	{
	}

	virtual const char* GetActionName() const { return "MAKE_IT_RAIN"; }
	virtual const char* GetCategoryName() const { return "STYLE_ENT"; }
};

void CommandSpendMakeItRain(int amount, bool fromBank, bool fromBankAndWallet)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPEND_MAKE_IT_RAIN", true););

	static const int AMOUNT_LIMIT = 2000;
	scriptAssertf(amount >= 0 && amount <= AMOUNT_LIMIT, "%s: NETWORK_SPEND_MAKE_IT_RAIN - invalid money amount < %d >, must be > 0 and <= %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount < 0 || amount > AMOUNT_LIMIT)
		return;

	MetricSpendMakeItRain m(amount, fromBank);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_STYLE_ENT");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_STYLE_ENT", 0x49d67632);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPEND_MAKE_IT_RAIN - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandSpentArena(const char* action, int amount, bool fromBank, bool fromBankAndWallet, srcSpentOnArena* data  ASSERT_ONLY(, const char* commandName))
{
	ASSERT_ONLY(VERIFY_STATS_LOADED(commandName, true););

	scriptAssertf(amount >= 0, "%s:%s - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName, amount);
	if (amount < 0)
		return;
	
	if (data->m_location_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "LOCATION", data->m_location.Int, data->m_location_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}

	if (data->m_style_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "STYLE", data->m_style.Int, data->m_style_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}

	if (data->m_graphics_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "GRAPHICS", data->m_graphics.Int, data->m_graphics_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}

	if (data->m_colour_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "COLOURS", data->m_colour.Int, data->m_colour_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}

	if (data->m_floor_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "FLOOR", data->m_floor.Int, data->m_floor_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}

	if (data->m_mechanic_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "MECHANIC", data->m_mechanic.Int, data->m_mechanic_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}

	if (data->m_personalQuarters_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "PERSONAL_QUARTER", data->m_personalQuarters.Int, data->m_personalQuarters_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_PROPERTY_UTIL");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_PROPERTY_UTIL", 0x8526aff3);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
	{
		ASSERT_ONLY(scriptErrorf("%s: %s - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName));
		return;
	}
}

void CommandSpentBuyArena(int amount, bool fromBank, bool fromBankAndWallet, srcSpentOnArena* data)
{
	CommandSpentArena("BUY_ARENA", amount, fromBank, fromBankAndWallet, data  ASSERT_ONLY(, "NETWORK_SPEND_BUY_ARENA"));
}

void CommandSpentUpgradeArena(int amount, bool fromBank, bool fromBankAndWallet, srcSpentOnArena* data)
{
	CommandSpentArena("UPGRADE_ARENA", amount, fromBank, fromBankAndWallet, data  ASSERT_ONLY(, "NETWORK_SPEND_UPGRADE_ARENA"));
}

////////////////////////////////////////////////////////////////////////// CASINO SPEND

void CommandSpentCasino(const char* action, int amount, bool fromBank, bool fromBankAndWallet, srcSpentCasino* data  ASSERT_ONLY(, const char* commandName))
{
	ASSERT_ONLY(VERIFY_STATS_LOADED(commandName, true););

	scriptAssertf(amount >= 0, "%s:%s - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName, amount);
	if (amount < 0)
		return;

	if (data->m_masterBedroom_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "MASTER_BEDROOM", data->m_masterBedroom.Int, data->m_masterBedroom_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}

	if (data->m_lounge_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "LOUNGE", data->m_lounge.Int, data->m_lounge_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}

	if (data->m_spa_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "SPA", data->m_spa.Int, data->m_spa_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}

	if (data->m_barParty_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "BAR_PARTY", data->m_barParty.Int, data->m_barParty_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}

	if (data->m_dealer_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "DEALER", data->m_dealer.Int, data->m_dealer_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}

	if (data->m_extraBedroom_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "EXTRA_BEDROOM", data->m_extraBedroom.Int, data->m_extraBedroom_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}

	if (data->m_extraBedroom2_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "EXTRA_BEDROOM2", data->m_extraBedroom2.Int, data->m_extraBedroom2_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}

	if (data->m_mediaRoom_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "MEDIA_ROOM", data->m_mediaRoom.Int, data->m_mediaRoom_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}

	if (data->m_garage_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "GARAGE", data->m_garage.Int, data->m_garage_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}

	if (data->m_colour_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "COLOUR", data->m_colour.Int, data->m_colour_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}

	if (data->m_graphics_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "GRAPHICS", data->m_graphics.Int, data->m_graphics_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}

	if (data->m_office_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "OFFICE", data->m_office.Int, data->m_office_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}

	if (data->m_preset_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "PRESET", data->m_preset.Int, data->m_preset_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_PROPERTY_UTIL");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_PROPERTY_UTIL", 0x8526aff3);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
	{
		ASSERT_ONLY(scriptErrorf("%s: %s - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName));
		return;
	}
}

void CommandSpentBuyCasino(int amount, bool fromBank, bool fromBankAndWallet, srcSpentCasino* data)
{
	CommandSpentCasino("BUY_PCASINO", amount, fromBank, fromBankAndWallet, data  ASSERT_ONLY(, "NETWORK_SPEND_BUY_CASINO"));
}

void CommandSpentUpgradeCasino(int amount, bool fromBank, bool fromBankAndWallet, srcSpentCasino* data)
{
	CommandSpentCasino("UPGRADE_PCASINO", amount, fromBank, fromBankAndWallet, data  ASSERT_ONLY(, "NETWORK_SPEND_UPGRADE_CASINO"));
}

void CommandSpentCasinoGeneric(int amount, bool fromBank, bool fromBankAndWallet, const char* category, int item)
{
	MetricSpent_BuyAndUpdateGeneric m("CASINO", category, item, amount, fromBank);
	APPEND_METRIC(m);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_CASINO");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_CASINO", 0x7DB96CBA);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
	{
		ASSERT_ONLY(scriptErrorf("%s: NETWORK_SPEND_CASINO_GENERIC - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter()));
		return;
	}
}

void CommandEarnCasinoTimeTrialWin(int amount)
{
	MetricEarnGeneric m("WIN", "TIME_TRIAL", amount);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if (!EarnCash(amount, true, false, &m, statId))
	{
		scriptErrorf("%s: NETWORK_EARN_CASINO_TIME_TRIAL_WIN - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnCollectableItem(int amount, int collectionHash)
{
	MetricEarnGeneric m("INDIV_ITEM", "COLLECTION", amount, collectionHash);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if (!EarnCash(amount, false, false, &m, statId))
	{
		scriptErrorf("%s: NETWORK_EARN_COLLECTABLE_ITEM - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnCollectableCompletedCollection(int amount, int collectionHash)
{
	MetricEarnGeneric m("COMPLETE", "COLLECTION", amount, collectionHash);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if (!EarnCash(amount, false, false, &m, statId, ATSTRINGHASH("MONEY_EARN_FROM_JOB_BONUS", 0xad06c80b)))
	{
		scriptErrorf("%s: NETWORK_EARN_COLLECTABLE_COMPLETED_COLLECTION - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnCollectableActionFigures(int amount)
{
	static const int AMOUNT_LIMIT = 200000;
	scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_COLLECTABLES_ACTION_FIGURES - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount > AMOUNT_LIMIT)
		return;

	MetricEarnGeneric m("ACTION_FIGURES", "COLLECTABLES", amount);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if (!EarnCash(amount, true, false, &m, statId))
	{
		scriptErrorf("%s: NETWORK_EARN_COLLECTABLES_ACTION_FIGURES - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnCasinoCollectableCompletedCollection(int amount)
{
	MetricEarnGeneric m("COMPLETE", "COLLECTION", amount);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if (!EarnCash(amount, true, false, &m, statId, ATSTRINGHASH("MONEY_EARN_FROM_JOB_BONUS", 0xad06c80b)))
	{
		scriptErrorf("%s: NETWORK_EARN_CASINO_COLLECTABLE_COMPLETED_COLLECTION - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnSellPrizeVehicle(int amount, int iType, int vehicleHash)
{
	static const int AMOUNT_LIMIT = 1500000;
	scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_SELL_PRIZE_VEHICLE - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount > AMOUNT_LIMIT)
		return;

	MetricEarnGeneric m("SELLING_VEH", "VEHICLE", amount, iType, vehicleHash);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if (!EarnCash(amount, true, false, &m, statId))
	{
		scriptErrorf("%s: NETWORK_EARN_SELL_PRIZE_VEHICLE - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnCasinoMissionReward(int amount)
{
	MetricEarnGeneric m("FM_MISSION", "CASINO", amount);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if (!EarnCash(amount, true, false, &m, statId, ATSTRINGHASH("MONEY_EARN_FROM_JOB_BONUS", 0xad06c80b)))
	{
		scriptErrorf("%s: NETWORK_EARN_CASINO_MISSION_REWARD - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnCasinoStoryMissionReward(int amount)
{
	MetricEarnGeneric m("STORY_MISSION", "CASINO", amount);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if (!EarnCash(amount, true, false, &m, statId))
	{
		scriptErrorf("%s: NETWORK_EARN_CASINO_STORY_MISSION_REWARD - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnCasinoMissionParticipation(int amount)
{
	MetricEarnGeneric m("FM_MISSION_PARTICIPATION", "CASINO", amount);
    const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
    if (!EarnCash(amount, true, false, &m, statId))
    {
		scriptErrorf("%s: NETWORK_EARN_CASINO_MISSION_PARTICIPATION - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
    }
}

void CommandEarnCasinoAward(int amount, int award)
{
	static const int AMOUNT_LIMIT = 500000;
	scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_CASINO_AWARD - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount > AMOUNT_LIMIT)
		return;

	MetricEarnFromAward m(amount, award);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if (!EarnCash(amount, true, false, &m, statId, ATSTRINGHASH("MONEY_EARN_FROM_JOB_BONUS", 0xad06c80b)))
	{
		scriptErrorf("%s: NETWORK_EARN_CASINO_AWARD - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

////////////////////////////////////////////////////////////////////////// HEIST 3 SPEND and EARNs


void SpendOrUpgradeArcade(const char* action, const int amount, bool fromBank, bool fromBankAndWallet, srcSpentArcade* data  ASSERT_ONLY(, const char* commandName))
{
	ASSERT_ONLY(VERIFY_STATS_LOADED(commandName, true));

	scriptAssertf(amount >= 0, "%s:%s - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName, amount);
	if (amount < 0)
		return;

	if (data->m_location_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "LOCATION", data->m_location.Int, data->m_location_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}
	if (data->m_garage_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "GARAGE", data->m_garage.Int, data->m_garage_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}
	if (data->m_sleeping_quarter_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "SLEEPING_QUARTER", data->m_sleeping_quarter.Int, data->m_sleeping_quarter_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}
	if (data->m_drone_station_amount.Int > 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "DRONE_STATION", data->m_drone_station.Int, data->m_drone_station_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}
	if (data->m_business_management_amount.Int > 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "BUSINESS_MANAGEMENT", data->m_business_management.Int, data->m_business_management_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}
	if (data->m_style_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "STYLE", data->m_style.Int, data->m_style_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}
	if (data->m_mural_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "MURAL", data->m_mural.Int, data->m_mural_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}
	if (data->m_floor_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "FLOOR", data->m_floor.Int, data->m_floor_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}
	if (data->m_neon_art_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "NEON", data->m_neon_art.Int, data->m_neon_art_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}
	if (data->m_highscore_screen_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "SCREEN", data->m_highscore_screen.Int, data->m_highscore_screen_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_PROPERTY_UTIL");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_PROPERTY_UTIL", 0x8526aff3);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
	{
		ASSERT_ONLY(scriptErrorf("%s: %s - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName));
		return;
	}
}

void CommandBuyArcade(const int amount, bool fromBank, bool fromBankAndWallet, srcSpentArcade* data)
{
	SpendOrUpgradeArcade("BUY_ARCADE", amount, fromBank, fromBankAndWallet, data  ASSERT_ONLY(, "NETWORK_SPEND_BUY_ARCADE"));
}

void CommandUpgradeArcade(const int amount, bool fromBank, bool fromBankAndWallet, srcSpentArcade* data)
{
	SpendOrUpgradeArcade("UPGRADE_ARCADE", amount, fromBank, fromBankAndWallet, data  ASSERT_ONLY(, "NETWORK_SPEND_UPGRADE_ARCADE"));
}

void CommandSpendCasinoHeist(const int amount, bool fromBank, bool fromBankAndWallet, const int replay, const int replay_amount, const int model, const int model_amount, const int vault_door, const int vault_door_amount, const int locks, const int locks_amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPEND_CASINO_HEIST", true));
	scriptAssertf(amount >= 0, "%s: NETWORK_SPEND_CASINO_HEIST - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount < 0)
		return;

	if (replay_amount > 0)
	{
		MetricSpendGeneric m("CASINO_HEIST", "REPLAY", replay, replay_amount, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPEND_CASINO_HEIST - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	if (model_amount > 0)
	{
		MetricSpendGeneric m("CASINO_HEIST", "MODEL", model, model_amount, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPEND_CASINO_HEIST - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	if (vault_door_amount > 0)
	{
		MetricSpendGeneric m("CASINO_HEIST", "VAULT_DOOR", vault_door, vault_door_amount, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPEND_CASINO_HEIST - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	if (locks_amount > 0)
	{
		MetricSpendGeneric m("CASINO_HEIST", "LOCKS", locks, locks_amount, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPEND_CASINO_HEIST - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_STYLE_ENT");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_STYLE_ENT", 0x49d67632);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPEND_CASINO_HEIST - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandSpendArcadeMgmt(const int amount, bool fromBank, bool fromBankAndWallet, const int cabinet, const int cabinet_amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPEND_ARCADE_MGMT", true));
	scriptAssertf(amount >= 0, "%s: NETWORK_SPEND_ARCADE_MGMT - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount < 0)
		return;

	if (cabinet_amount > 0)
	{
		MetricSpendGeneric m("ARCADE_MGMT", "CABINET", cabinet, cabinet_amount, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPEND_ARCADE_MGMT - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_STYLE_ENT");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_STYLE_ENT", 0x49d67632);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPEND_ARCADE_MGMT - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandSpendPlayArcade(const int amount, bool fromBank, bool fromBankAndWallet, const int cabinet, const int cabinet_amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPEND_PLAY_ARCADE", true));
	scriptAssertf(amount >= 0, "%s: NETWORK_SPEND_PLAY_ARCADE - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount < 0)
		return;

	if (cabinet_amount > 0)
	{
		MetricSpendGeneric m("PLAY_ARCADE", "CABINET", cabinet, cabinet_amount, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPEND_PLAY_ARCADE - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_STYLE_ENT");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_STYLE_ENT", 0x49d67632);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPEND_PLAY_ARCADE - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandSpendArcade(const int amount, bool fromBank, bool fromBankAndWallet, const int bar, const int bar_amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPEND_ARCADE", true));
	scriptAssertf(amount >= 0, "%s: NETWORK_SPEND_ARCADE - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount < 0)
		return;

	if (bar_amount > 0)
	{
		MetricSpendGeneric m("ARCADE", "BAR", bar, bar_amount, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPEND_ARCADE - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_STYLE_ENT");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_STYLE_ENT", 0x49d67632);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPEND_ARCADE - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnCasinoHeist(const int amount, const int general_prep, const int general_prep_amount, const int setup, const int setup_amount, const int finale, const int finale_amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_CASINO_HEIST", true));

	scriptAssertf(amount > 0, "%s: NETWORK_EARN_CASINO_HEIST - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount <= 0)
		return;

	if (general_prep_amount > 0)
	{
		static const int AMOUNT_LIMIT = 15000;
		scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_CASINO_HEIST - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
		if (amount <= AMOUNT_LIMIT)
		{
			MetricEarnGeneric m("GENERAL_PREP", "CASINO_HEIST", general_prep_amount, general_prep);
			if (!EarnCash(amount, false, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS")))
			{
				scriptErrorf("%s: NETWORK_EARN_CASINO_HEIST - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			}
		}
	}

	if (setup_amount > 0)
	{
		static const int AMOUNT_LIMIT = 15000;
		scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_CASINO_HEIST - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
		if (amount <= AMOUNT_LIMIT)
		{
			MetricEarnGeneric m("SETUP", "CASINO_HEIST", setup_amount, setup);
			if (!EarnCash(amount, false, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS")))
			{
				scriptErrorf("%s: NETWORK_EARN_CASINO_HEIST - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			}
		}
	}

	if (finale_amount > 0)
	{
		static const int AMOUNT_LIMIT = 7590000;
		scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_CASINO_HEIST - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
		if (amount <= AMOUNT_LIMIT)
		{
			MetricEarnGeneric m("FINALE", "CASINO_HEIST", finale_amount, finale);
			if (!EarnCash(amount, false, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS")))
			{
				scriptErrorf("%s: NETWORK_EARN_CASINO_HEIST - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			}
		}
	}
}

void CommandEarnUpgradeArcade(const int amount, const int location, const int location_amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_UPGRADE_ARCADE", true));

	scriptAssertf(amount > 0, "%s: NETWORK_EARN_UPGRADE_ARCADE - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount <= 0)
		return;

	if (location_amount > 0)
	{
		static const int AMOUNT_LIMIT = 2126250;
		scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_UPGRADE_ARCADE - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
		if (amount <= AMOUNT_LIMIT)
		{
			MetricEarnGeneric m("LOCATION", "UPGRADE_ARCADE", location_amount, location);
			if (!EarnCash(amount, false, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_PROPERTY_TRADE")))
			{
				scriptErrorf("%s: NETWORK_EARN_UPGRADE_ARCADE - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			}
		}
	}
}

void CommandEarnArcade(const int amount, const int arcade_award, const int arcade_award_amount, const int arcade_trophy, const int arcade_trophy_amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_ARCADE", true));

	scriptAssertf(amount > 0, "%s: NETWORK_EARN_ARCADE - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount <= 0)
		return;

	if (arcade_award_amount > 0)
	{
		static const int AMOUNT_LIMIT = 700000;
		scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_ARCADE - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
		if (amount <= AMOUNT_LIMIT)
		{
			MetricEarnGeneric m("ARCADE_AWARD", "ARCADE", arcade_award_amount, arcade_award);
			if (!EarnCash(amount, false, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS")))
			{
				scriptErrorf("%s: NETWORK_EARN_ARCADE - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			}
		}
	}

	if (arcade_trophy_amount > 0)
	{
		MetricEarnGeneric m("ARCADE_TROPHY", "ARCADE", arcade_trophy_amount, arcade_trophy);
		if (!EarnCash(amount, false, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS")))
		{
			scriptErrorf("%s: NETWORK_EARN_ARCADE - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
}

void CommandEarnCollectables(const int amount, const int arcade, const int arcade_amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_COLLECTABLES", true));

	scriptAssertf(amount > 0, "%s: NETWORK_EARN_COLLECTABLES - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount <= 0)
		return;

	if (arcade_amount > 0)
	{
		static const int AMOUNT_LIMIT = 300000;
		scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_COLLECTABLES - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
		if (amount <= AMOUNT_LIMIT)
		{
			MetricEarnGeneric m("ARCADE", "COLLECTABLES", arcade_amount, arcade);
			if (!EarnCash(amount, false, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS")))
			{
				scriptErrorf("%s: NETWORK_EARN_COLLECTABLES - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			}
		}
	}
}

void CommandEarnChallenge(const int amount, const int kills, const int kills_amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_CHALLENGE", true));

	scriptAssertf(amount > 0, "%s: NETWORK_EARN_CHALLENGE - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount <= 0)
		return;

	if (kills_amount > 0)
	{
		static const int AMOUNT_LIMIT = 700000;
		scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_CHALLENGE - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
		if (amount <= AMOUNT_LIMIT)
		{
			MetricEarnGeneric m("KILLS", "CHALLENGE", kills_amount, kills);
			if (!EarnCash(amount, false, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS")))
			{
				scriptErrorf("%s: NETWORK_EARN_CHALLENGE - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			}
		}
	}
}

void CommandEarnCasinoHeistAwards(const int amount, const int awards, const int awards_amount, const int elite, const int elite_amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_CASINO_HEIST_AWARDS", true));

	scriptAssertf(amount > 0, "%s: NETWORK_EARN_CASINO_HEIST_AWARDS - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount <= 0)
		return;

	if (awards_amount > 0)
	{
		static const int AMOUNT_LIMIT = 700000;
		scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_CASINO_HEIST_AWARDS - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
		if (amount <= AMOUNT_LIMIT)
		{
			MetricEarnGeneric m("AWARDS", "CASINO_HEIST", awards_amount, awards);
			if (!EarnCash(amount, true, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS")))
			{
				scriptErrorf("%s: NETWORK_EARN_CASINO_HEIST_AWARDS - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			}
		}
	}

	if (elite_amount > 0)
	{
		static const int AMOUNT_LIMIT = 700000;
		scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_CASINO_HEIST_AWARDS - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
		if (amount <= AMOUNT_LIMIT)
		{
			MetricEarnGeneric m("ELITE", "CASINO_HEIST", elite_amount, elite);
			if (!EarnCash(amount, true, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS")))
			{
				scriptErrorf("%s: NETWORK_EARN_CASINO_HEIST_AWARDS - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			}
		}
	}
}

void CommandEarnYatchMission(const int amount, const int mission)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_YATCH_MISSION", true));

	static const int AMOUNT_LIMIT = 800000;
	scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_YATCH_MISSION - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount <= AMOUNT_LIMIT)
	{
		MetricYatchMission m(amount, mission);
		if (!EarnCash(amount, false, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS")))
		{
			scriptErrorf("%s: NETWORK_EARN_YATCH_MISSION - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
}

void CommandEarnDispatchCall(const int amount, const int mission)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_DISPATCH_CALL", true));

	static const int AMOUNT_LIMIT = 800000;
	scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_DISPATCH_CALL - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount <= AMOUNT_LIMIT)
	{
		MetricYatchMission m(amount, mission);
		if (!EarnCash(amount, false, false, &m, StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS")))
		{
			scriptErrorf("%s: NETWORK_EARN_DISPATCH_CALL - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
}

////////////////////////////////////////////////////////////////////////// Heist4

void CommandSpendBeachParty(int item)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPEND_BEACH_PARTY", true);)

	MetricSpendGeneric m("BEACH_PARTY", "BAR", item, 0, false);
	ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
	scriptAssertf(appendResult, "%s:NETWORK_SPEND_BEACH_PARTY - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_STYLE_ENT");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_STYLE_ENT", 0x49d67632);
	if (!SpendCash(0, false, false, false, false, nullptr, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPEND_BEACH_PARTY - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandSpendSubmarine(const int amount, bool fromBank, bool fromBankAndWallet, const int boat_amount, int relocate_amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPEND_PLAY_ARCADE", true));
	scriptAssertf(amount >= 0, "%s: NETWORK_SPEND_PLAY_ARCADE - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount < 0)
		return;

	if (boat_amount > 0)
	{
		MetricSpendGeneric m("SUBMARINE", "BOAT", 0, boat_amount, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPEND_PLAY_ARCADE - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	if (relocate_amount > 0)
	{
		MetricSpendGeneric m("SUBMARINE", "RELOCATE", 0, relocate_amount, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPEND_PLAY_ARCADE - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_STYLE_ENT");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_STYLE_ENT", 0x49d67632);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPEND_PLAY_ARCADE - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandSpendCasinoClub(int amount, bool fromBank, bool fromBankAndWallet, int barItem, int barAmount, int vipItem, int vipAmount, int entryAmount, int attendantAmount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPEND_CASINO_CLUB", true));
	scriptAssertf(amount >= 0, "%s: NETWORK_SPEND_CASINO_CLUB - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount < 0)
		return;

	if (barAmount > 0)
	{
		MetricSpendGeneric m("CASINO_CLUB", "BAR", barItem, barAmount, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPEND_CASINO_CLUB - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	if (vipAmount > 0)
	{
		MetricSpendGeneric m("CASINO_CLUB", "VIP", vipItem, vipAmount, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPEND_CASINO_CLUB - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	if (entryAmount > 0)
	{
		MetricSpendGeneric m("CASINO_CLUB", "ENTRY", 0, entryAmount, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPEND_CASINO_CLUB - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	if (attendantAmount > 0)
	{
		MetricSpendGeneric m("CASINO_CLUB", "BATHROOM_ATTENDANT", 0, attendantAmount, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPEND_CASINO_CLUB - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_STYLE_ENT");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_STYLE_ENT", 0x49d67632);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPEND_CASINO_CLUB - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void SpendOrUpgradeBuySub(const char* action, const int amount, bool fromBank, bool fromBankAndWallet, srcSpentBuySub* data  ASSERT_ONLY(, const char* commandName))
{
	ASSERT_ONLY(VERIFY_STATS_LOADED(commandName, true));

	scriptAssertf(amount >= 0, "%s:%s - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName, amount);
	if (amount < 0)
		return;

	if (data->m_submarine_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "SUBMARINE", 0, data->m_submarine_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}
	if (data->m_color_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "COLOR", data->m_color.Int, data->m_color_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}
	if (data->m_flag_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "FLAG", data->m_flag.Int, data->m_flag_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}
	if (data->m_anti_aircraft_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "ANTI_AIRCRAFT", 0, data->m_anti_aircraft_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}
	if (data->m_missile_station_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "MISSILE_STATION", data->m_missile_station.Int, data->m_missile_station_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}
	if (data->m_sonar_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "SONAR", data->m_sonar.Int, data->m_sonar_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}
	if (data->m_weapon_workshop_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "WEAPON_WORKSHOP", data->m_weapon_workshop.Int, data->m_weapon_workshop_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}
	if (data->m_avisa_pool_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "MOON_POOL", data->m_avisa.Int, data->m_avisa_pool_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}
	if (data->m_seasparrow_pool_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "MOON_POOL", data->m_seasparrow.Int, data->m_seasparrow_pool_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_PROPERTY_UTIL");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_PROPERTY_UTIL", 0x8526aff3);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
	{
		ASSERT_ONLY(scriptErrorf("%s: %s - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName));
		return;
	}
}

void CommandBuyBuySub(const int amount, bool fromBank, bool fromBankAndWallet, srcSpentBuySub* data)
{
	SpendOrUpgradeBuySub("BUY_SUB", amount, fromBank, fromBankAndWallet, data  ASSERT_ONLY(, "NETWORK_SPEND_BUY_SUB"));
}

void CommandUpgradeBuySub(const int amount, bool fromBank, bool fromBankAndWallet, srcSpentBuySub* data)
{
	SpendOrUpgradeBuySub("UPGRADE_SUB", amount, fromBank, fromBankAndWallet, data  ASSERT_ONLY(, "NETWORK_SPEND_UPGRADE_SUB"));
}

void CommandSpendIslandHeist(const int amount, bool fromBank, bool fromBankAndWallet, srcSpentIslandHeist* data)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPEND_ISLAND_HEIST", true));
	scriptAssertf(amount >= 0, "%s: NETWORK_SPEND_ISLAND_HEIST - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount < 0)
		return;

	scriptDebugf1("%s: NETWORK_SPEND_ISLAND_HEIST - Called native - amount = %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);

	if (data->m_airstrike.Int > 0)
	{
		scriptDebugf1("%s: NETWORK_SPEND_ISLAND_HEIST - Append metric Spend AIRSTRIKE - amount = %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), data->m_airstrike.Int);
		MetricSpendGeneric m("ISLAND_HEIST", "AIRSTRIKE", 0, data->m_airstrike.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPEND_ISLAND_HEIST - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	if (data->m_heavy_weapon.Int > 0)
	{
		scriptDebugf1("%s: NETWORK_SPEND_ISLAND_HEIST - Append metric Spend HEAVY_WEAPON - amount = %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), data->m_heavy_weapon.Int);
		MetricSpendGeneric m("ISLAND_HEIST", "HEAVY_WEAPON", 0, data->m_heavy_weapon.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPEND_ISLAND_HEIST - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	if (data->m_sniper.Int > 0)
	{
		scriptDebugf1("%s: NETWORK_SPEND_ISLAND_HEIST - Append metric Spend SNIPER - amount = %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), data->m_sniper.Int);
		MetricSpendGeneric m("ISLAND_HEIST", "SNIPER", 0, data->m_sniper.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPEND_ISLAND_HEIST - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	if (data->m_air_support.Int > 0)
	{
		scriptDebugf1("%s: NETWORK_SPEND_ISLAND_HEIST - Append metric Spend AIR_SUPPORT - amount = %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), data->m_air_support.Int);
		MetricSpendGeneric m("ISLAND_HEIST", "AIR_SUPPORT", 0, data->m_air_support.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPEND_ISLAND_HEIST - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	if (data->m_drone.Int > 0)
	{
		scriptDebugf1("%s: NETWORK_SPEND_ISLAND_HEIST - Append metric Spend DRONE - amount = %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), data->m_drone.Int);
		MetricSpendGeneric m("ISLAND_HEIST", "DRONE", 0, data->m_drone.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPEND_ISLAND_HEIST - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	if (data->m_weapon_stash.Int > 0)
	{
		scriptDebugf1("%s: NETWORK_SPEND_ISLAND_HEIST - Append metric Spend WEAPON_STASH - amount = %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), data->m_weapon_stash.Int);
		MetricSpendGeneric m("ISLAND_HEIST", "WEAPON_STASH", 0, data->m_weapon_stash.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPEND_ISLAND_HEIST - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	if (data->m_suppressor.Int > 0)
	{
		scriptDebugf1("%s: NETWORK_SPEND_ISLAND_HEIST - Append metric Spend SUPPRESSOR - amount = %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), data->m_suppressor.Int);
		MetricSpendGeneric m("ISLAND_HEIST", "SUPPRESSOR", 0, data->m_suppressor.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPEND_ISLAND_HEIST - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	if (data->m_replay.Int > 0)
	{
		scriptDebugf1("%s: NETWORK_SPEND_ISLAND_HEIST - Append metric Spend REPLAY - amount = %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), data->m_replay.Int);
		MetricSpendGeneric m("ISLAND_HEIST", "REPLAY", 0, data->m_replay.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPEND_ISLAND_HEIST - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	if (data->m_prep.Int > 0)
	{
		scriptDebugf1("%s: NETWORK_SPEND_ISLAND_HEIST - Append metric Spend PREP - amount = %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), data->m_prep.Int);
		MetricSpendGeneric m("ISLAND_HEIST", "PREP", data->m_prepItem.Int, data->m_prep.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPEND_ISLAND_HEIST - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_STYLE_ENT");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_STYLE_ENT", 0x49d67632);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPEND_ISLAND_HEIST - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnIslandHeist(int amount, int finale, int award, int awardName, int prep, int prepName)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_ISLAND_HEIST", true);)

	static const int AMOUNT_LIMIT = 40000000;
	scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_ISLAND_HEIST - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount > AMOUNT_LIMIT)
		return;

	if (finale > 0)
	{
		MetricEarnGeneric m("ISLAND_HEIST", "FINALE", finale);
		const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
		if (!EarnCash(finale, true, false, &m, statId))
		{
			scriptErrorf("%s: NETWORK_EARN_ISLAND_HEIST - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
	if (award > 0)
	{
		MetricEarnGeneric m("ISLAND_HEIST", "AWARD", award, awardName);
		const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
		if (!EarnCash(award, true, false, &m, statId))
		{
			scriptErrorf("%s: NETWORK_EARN_ISLAND_HEIST - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
	if (prep > 0)
	{
		MetricEarnGeneric m("ISLAND_HEIST", "PREP", prep, prepName);
		const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
		if (!EarnCash(prep, true, false, &m, statId))
		{
			scriptErrorf("%s: NETWORK_EARN_ISLAND_HEIST - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
}

void CommandEarnBeachPartyLostFound(int amount, int item, int missionGiver)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_BEACH_PARTY_LOST_FOUND", true);)

	static const int AMOUNT_LIMIT = 2500000;
	scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_BEACH_PARTY_LOST_FOUND - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount > AMOUNT_LIMIT)
		return;

	if (amount > 0)
	{
		MetricEarnGeneric m("BEACH_PARTY", "LOST_FOUND", item, missionGiver);
		const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
		if (!EarnCash(amount, true, false, &m, statId))
		{
			scriptErrorf("%s: NETWORK_EARN_BEACH_PARTY_LOST_FOUND - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
}

void CommandEarnIslandHeistDjMission(int amount, int mission)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_FROM_ISLAND_HEIST_DJ_MISSION", true);)

	static const int AMOUNT_LIMIT = 1600000;
	scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_FROM_ISLAND_HEIST_DJ_MISSION - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount > AMOUNT_LIMIT)
		return;

	if (amount > 0)
	{
		MetricEarnGeneric m("ISLAND_HEIST", "DJMISSION", mission);
		const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
		if (!EarnCash(amount, true, false, &m, statId))
		{
			scriptErrorf("%s: NETWORK_EARN_FROM_ISLAND_HEIST_DJ_MISSION - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
}

////////////////////////////////////////////////////////////////////////// Tuner Update

void CommandSpendCarClubMembership(const int amount, bool fromBank, bool fromBankAndWallet, const int carClub_amount, int purchaseMethod)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPEND_CAR_CLUB_MEMBERSHIP", true));
	scriptAssertf(amount >= 0, "%s: NETWORK_SPEND_CAR_CLUB_MEMBERSHIP - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount < 0)
		return;

	if (carClub_amount >= 0)
	{
		MetricSpendGeneric m("MEMBERSHIP", "CARCLUB", purchaseMethod, carClub_amount, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPEND_CAR_CLUB_MEMBERSHIP - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_STYLE_ENT");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_STYLE_ENT", 0x49d67632);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPEND_CAR_CLUB_MEMBERSHIP - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandSpendCarClubBar(const int amount, bool fromBank, bool fromBankAndWallet, const int carClub_amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPEND_CAR_CLUB_BAR", true));
	scriptAssertf(amount >= 0, "%s: NETWORK_SPEND_CAR_CLUB_BAR - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount < 0)
		return;

	if (carClub_amount > 0)
	{
		MetricSpendGeneric m("CARCLUB", "BAR", 0, carClub_amount, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPEND_CAR_CLUB_BAR - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_STYLE_ENT");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_STYLE_ENT", 0x49d67632);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPEND_CAR_CLUB_BAR - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandSpendAutoShopModify(const int amount, bool fromBank, bool fromBankAndWallet, const int vehicleHash, const int autoShop_amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPEND_AUTOSHOP_MODIFY", true));
	scriptAssertf(amount >= 0, "%s: NETWORK_SPEND_AUTOSHOP_MODIFY - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount < 0)
		return;
	if (autoShop_amount > 0)
	{
		MetricSpendGeneric m("AUTOSHOP", "MODIFY", vehicleHash, autoShop_amount, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPEND_AUTOSHOP_MODIFY - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_VEH_MAINTENANCE");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_VEH_MAINTENANCE", 0x3c65cc9a);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPEND_AUTOSHOP_MODIFY - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandSpendCarClubTakeover(const int amount, bool fromBank, bool fromBankAndWallet, const int carClub_amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPEND_CAR_CLUB_TAKEOVER", true));
	scriptAssertf(amount >= 0, "%s: NETWORK_SPEND_CAR_CLUB_TAKEOVER - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount < 0)
		return;

	if (carClub_amount > 0)
	{
		MetricSpendGeneric m("CARCLUB", "TAKEOVER", 0, carClub_amount, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:NETWORK_SPEND_CAR_CLUB_TAKEOVER - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_STYLE_ENT");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_STYLE_ENT", 0x49d67632);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPEND_CAR_CLUB_TAKEOVER - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void SpendBuyOrUpgradeAutoshop(const char* action, const int amount, bool fromBank, bool fromBankAndWallet, srcSpentBuyAutoshop* data  ASSERT_ONLY(, const char* commandName))
{
	ASSERT_ONLY(VERIFY_STATS_LOADED(commandName, true));
	scriptAssertf(amount >= 0, "%s:%s - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName, amount);
	if (amount < 0)
		return;
	if (data->m_location_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "LOCATION", data->m_location.Int, data->m_location_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}
	if (data->m_style_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "STYLE", data->m_style.Int, data->m_style_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}
	if (data->m_tint_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "TINT", data->m_tint.Int, data->m_tint_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}
	if (data->m_emblem_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "EMBLEM", data->m_emblem.Int, data->m_emblem_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}
	if (data->m_crew_name_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "CREW_NAME", data->m_crew_name.Int, data->m_crew_name_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}
	if (data->m_staff_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "STAFF", data->m_staff.Int, data->m_staff_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}
	if (data->m_lift_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "LIFT", data->m_lift.Int, data->m_lift_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}
	if (data->m_personal_quarter_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "PERSONAL_QUARTER", data->m_personal_quarter.Int, data->m_personal_quarter_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_PROPERTY_UTIL");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_PROPERTY_UTIL", 0x8526aff3);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
	{
		ASSERT_ONLY(scriptErrorf("%s: %s - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName));
		return;
	}
}
void CommandBuyAutoshop(const int amount, bool fromBank, bool fromBankAndWallet, srcSpentBuyAutoshop* data)
{
	SpendBuyOrUpgradeAutoshop("BUY_AUTOSHOP", amount, fromBank, fromBankAndWallet, data  ASSERT_ONLY(, "NETWORK_SPEND_BUY_AUTOSHOP"));
}
void CommandUpgradeAutoshop(const int amount, bool fromBank, bool fromBankAndWallet, srcSpentBuyAutoshop* data)
{
	SpendBuyOrUpgradeAutoshop("UPGRADE_AUTOSHOP", amount, fromBank, fromBankAndWallet, data  ASSERT_ONLY(, "NETWORK_SPEND_UPGRADE_AUTOSHOP"));
}

void CommandEarnAutoShopBusiness(const int amount, const int carHash)
{
	MetricEarnGeneric m("BUSINESS", "AUTOSHOP", amount, carHash);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if (!EarnCash(amount, false, false, &m, statId, ATSTRINGHASH("MONEY_EARN_JOBS", 0x61b8682f)))
	{
		scriptErrorf("%s: NETWORK_EARN_AUTOSHOP_BUSINESS - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnAutoShopIncome(const int amount, const int missionId)
{
	static const int AMOUNT_LIMIT = 50000;
	scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_AUTOSHOP_INCOME - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount > AMOUNT_LIMIT)
		return;

	MetricEarnGeneric m("INCOME", "AUTOSHOP", amount, missionId);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_AUTOSHOP_INCOME");
	if (!EarnCash(amount, false, false, &m, statId, ATSTRINGHASH("MONEY_EARN_FROM_JOB_BONUS", 0xad06c80b)))
	{
		scriptErrorf("%s: NETWORK_EARN_AUTOSHOP_INCOME - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnCarClubMembership(const int amount)
{
	MetricEarnGeneric m("CARCLUB", "MEMBERSHIP", amount);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if (!EarnCash(amount, false, false, &m, statId, ATSTRINGHASH("MONEY_EARN_JOBS", 0x61b8682f)))
	{
		scriptErrorf("%s: NETWORK_EARN_CARCLUB_MEMBERSHIP - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnDailyVehicle(const int amount, const int carHash)
{
	static const int AMOUNT_LIMIT = 200000;
	scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_DAILY_VEHICLE - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount > AMOUNT_LIMIT)
		return;

	MetricEarnGeneric m("AUTOSHOP", "DAILY_VEH", amount, carHash);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if (!EarnCash(amount, false, false, &m, statId, ATSTRINGHASH("MONEY_EARN_JOBS", 0x61b8682f)))
	{
		scriptErrorf("%s: NETWORK_EARN_DAILY_VEHICLE - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnDailyVehicleBonus(const int amount)
{
	static const int AMOUNT_LIMIT = 1000000;
	scriptAssertf(amount <= AMOUNT_LIMIT, "%s:  NETWORK_EARN_DAILY_VEHICLE_BONUS - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount > AMOUNT_LIMIT)
		return;

	MetricEarnGeneric m("AUTOSHOP", "DAILY_VEH_BONUS", amount);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if (!EarnCash(amount, false, false, &m, statId, ATSTRINGHASH("MONEY_EARN_JOBS", 0x61b8682f)))
	{
		scriptErrorf("%s: NETWORK_EARN_DAILY_VEHICLE_BONUS - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnTunerAward(int amount, const char* uniqueMatchId, const char* challenge)
{
	static const int AMOUNT_LIMIT = 4000000;
	scriptAssertf(amount<=AMOUNT_LIMIT, "%s: NETWORK_EARN_TUNER_AWARD - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount>AMOUNT_LIMIT)
		return;

	MetricEarnTunerAward m(amount, uniqueMatchId, challenge);

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if (!EarnCash(amount, false, false, &m, statId, ATSTRINGHASH("MONEY_EARN_JOBS", 0x61b8682f)))
	{
		scriptErrorf("%s: NETWORK_EARN_TUNER_AWARD - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnTunerRobbery(const int amount, const int finale, const int finaleContentID, const int prep, const int prepName)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_EARN_TUNER_ROBBERY", true);)

	static const int AMOUNT_LIMIT = 5000000;
	scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_TUNER_ROBBERY - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount > AMOUNT_LIMIT)
		return;

#if __NET_SHOP_ACTIVE
	const s64 nonceSeed = GameTransactionSessionMgr::Get().GetNonceForTelemetry();
#endif // __NET_SHOP_ACTIVE

	if (finale >= 0)
	{
		MetricEarnGeneric m("TUNER_ROB", "FINALE", finale, finaleContentID);
		const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
		if (!EarnCash(amount, false, false, &m, statId, ATSTRINGHASH("MONEY_EARN_JOBS", 0x61b8682f)))
		{
			scriptErrorf("%s: NETWORK_EARN_TUNER_ROBBERY - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
	if (prep >= 0)
	{
#if __NET_SHOP_ACTIVE
		GameTransactionSessionMgr::Get().SetNonceForTelemetry(nonceSeed);
#endif // __NET_SHOP_ACTIVE
		MetricEarnGeneric m("TUNER_ROB", "PREP", prep, prepName);

		const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
		if (!EarnCash(amount, false, false, &m, statId, ATSTRINGHASH("MONEY_EARN_JOBS", 0x61b8682f)))
		{
			scriptErrorf("%s: NETWORK_EARN_TUNER_ROBBERY - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
}

void CommandEarnUpgradeAutoShop(const int amount, const int locationHash)
{
	MetricEarnGeneric m("LOCATION", "UPGRADE_AUTOSHOP", amount, locationHash);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_PROPERTY_TRADE");
	if (!EarnCash(amount, false, false, &m, statId, ATSTRINGHASH("MONEY_EARN_PROPERTY_TRADE", 0x7b1a3e5a)))
	{
		scriptErrorf("%s: NETWORK_EARN_UPGRADE_AUTOSHOP - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandSpendInteractionMenuAbility(int amount, bool fromBank, bool fromBankAndWallet, int ability)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPEND_INTERACTION_MENU_ABILITY", true));
	scriptAssertf(amount >= 0, "%s: NETWORK_SPEND_INTERACTION_MENU_ABILITY - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount < 0)
		return;

	MetricSpendGeneric m("IM", "ABILITY", ability, amount, fromBank);

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_STYLE_ENT");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_STYLE_ENT", 0x49d67632);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, &m, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPEND_INTERACTION_MENU_ABILITY - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandSpendSetCommonFields(int properties, int properties2, int heists, bool windfall)
{
	SpendMetricCommon::SetCommonFields(properties, properties2, heists, windfall);
}

void CommandSpendSetDiscount(int discount)
{
	scriptcameraDebugf1("%s: NETWORK_SPEND_SET_DISCOUNT - Set discount to %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), discount);
	SpendMetricCommon::SetDiscount(discount);
}

//////////////////////////////////////////////////////////////////////////

//Used to deduct the given amount from all possible sources (BANK and WALLET), but only from Earned cash (EVC)
int SpendEarnedCashFromAll(int /*amount*/)
{
	//Command is invalid now - Script should use instead ProcessCashGift.
	scriptAssertf(0, "%s: NETWORK_SPEND_EARNED_FROM_BANK_AND_WALLETS - Deprecated command.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	return 0;
}

bool ProcessEconomyHasFixedCrazyNumbers( )
{
	return MoneyInterface::EconomyFixedCrazyNumbers();
}

//////////////////////////////////////////////////////////////////////////

void SpendBuyOrUpgradeAgency(const char* action, const int amount, bool fromBank, bool fromBankAndWallet, srcSpentBuyAgency* data  ASSERT_ONLY(, const char* commandName))
{
	ASSERT_ONLY(VERIFY_STATS_LOADED(commandName, true));
	scriptAssertf(amount >= 0, "%s:%s - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName, amount);
	if (amount < 0)
		return;
	if (data->m_location_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "LOCATION", data->m_location.Int, data->m_location_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}
	if (data->m_style_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "STYLE", data->m_style.Int, data->m_style_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}
	if (data->m_wallpaper_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "WALLPAPER", data->m_wallpaper.Int, data->m_wallpaper_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}
	if (data->m_tint_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "TINT", data->m_tint.Int, data->m_tint_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}
	if (data->m_personal_quarter_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "PERSONAL_QUARTER", data->m_personal_quarter.Int, data->m_personal_quarter_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}
	if (data->m_weapon_workshop_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "WEAPON_WORKSHOP", data->m_weapon_workshop.Int, data->m_weapon_workshop_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}
	if (data->m_vehicle_workshop_amount.Int >= 0)
	{
		MetricSpent_BuyAndUpdateGeneric m(action, "VEHICLE_WORKSHOP", data->m_vehicle_workshop.Int, data->m_vehicle_workshop_amount.Int, fromBank);
		ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
		scriptAssertf(appendResult, "%s:%s - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
	}
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_PROPERTY_UTIL");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_PROPERTY_UTIL", 0x8526aff3);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
	{
		ASSERT_ONLY(scriptErrorf("%s: %s - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName));
		return;
	}
}
void CommandBuyAgency(const int amount, bool fromBank, bool fromBankAndWallet, srcSpentBuyAgency* data)
{
	SpendBuyOrUpgradeAgency("BUY_AGENCY", amount, fromBank, fromBankAndWallet, data  ASSERT_ONLY(, "NETWORK_SPEND_BUY_AGENCY"));
}
void CommandUpgradeAgency(const int amount, bool fromBank, bool fromBankAndWallet, srcSpentBuyAgency* data)
{
	SpendBuyOrUpgradeAgency("UPGRADE_AGENCY", amount, fromBank, fromBankAndWallet, data  ASSERT_ONLY(, "NETWORK_SPEND_UPGRADE_AGENCY"));
}

void CommandSpendAgency(const int amount, bool fromBank, bool fromBankAndWallet, int context, int option)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPEND_AGENCY", true));
	scriptAssertf(amount >= 0, "%s: NETWORK_SPEND_AGENCY - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount < 0)
		return;

	MetricSpendGeneric m("AGENCY", "CONCIERGE", context, amount, fromBank, option);
	ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
	scriptAssertf(appendResult, "%s:NETWORK_SPEND_AGENCY - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_STYLE_ENT");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_STYLE_ENT", 0x49d67632);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPEND_AGENCY - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandSpendHidden(const int amount, bool fromBank, bool fromBankAndWallet, int providerNpc)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPEND_HIDDEN", true));
	scriptAssertf(amount >= 0, "%s: NETWORK_SPEND_HIDDEN - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount < 0)
		return;

	MetricSpendGeneric m("HIDDEN", "CONTACT_SERVICE", providerNpc, amount, fromBank);
	ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
	scriptAssertf(appendResult, "%s:NETWORK_SPEND_HIDDEN - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_STYLE_ENT");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_STYLE_ENT", 0x49d67632);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPEND_HIDDEN - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandSpendSourceBike(const int amount, bool fromBank, bool fromBankAndWallet, int providerNpc)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPEND_SOURCE_BIKE", true));
	scriptAssertf(amount >= 0, "%s: NETWORK_SPEND_SOURCE_BIKE - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount < 0)
		return;

	MetricSpendGeneric m("SOURCE_BIKE", "CONTACT_SERVICE", providerNpc, amount, fromBank);
	ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
	scriptAssertf(appendResult, "%s:NETWORK_SPEND_SOURCE_BIKE - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_STYLE_ENT");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_STYLE_ENT", 0x49d67632);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPEND_SOURCE_BIKE - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandSpendCompSuv(const int amount, bool fromBank, bool fromBankAndWallet, int providerNpc)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPEND_COMP_SUV", true));
	scriptAssertf(amount >= 0, "%s: NETWORK_SPEND_COMP_SUV - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount < 0)
		return;

	MetricSpendGeneric m("COMP_SUV", "CONTACT_SERVICE", providerNpc, amount, fromBank);
	ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
	scriptAssertf(appendResult, "%s:NETWORK_SPEND_COMP_SUV - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_STYLE_ENT");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_STYLE_ENT", 0x49d67632);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPEND_COMP_SUV - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandSpendSuvFstTrvl(const int amount, bool fromBank, bool fromBankAndWallet, int providerNpc)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPEND_SUV_FST_TRVL", true));
	scriptAssertf(amount >= 0, "%s: NETWORK_SPEND_SUV_FST_TRVL - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount < 0)
		return;

	MetricSpendGeneric m("SUV_FST_TRVL", "CONTACT_SERVICE", providerNpc, amount, fromBank);
	ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
	scriptAssertf(appendResult, "%s:NETWORK_SPEND_SUV_FST_TRVL - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_STYLE_ENT");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_STYLE_ENT", 0x49d67632);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPEND_SUV_FST_TRVL - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandSpendSupply(const int amount, bool fromBank, bool fromBankAndWallet, int providerNpc)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPEND_SUPPLY", true));
	scriptAssertf(amount >= 0, "%s: NETWORK_SPEND_SUPPLY - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount < 0)
		return;

	MetricSpendGeneric m("SUPPLY", "CONTACT_SERVICE", providerNpc, amount, fromBank);
	ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
	scriptAssertf(appendResult, "%s:NETWORK_SPEND_SUPPLY - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_STYLE_ENT");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_STYLE_ENT", 0x49d67632);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPEND_SUPPLY - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandSpendBikeShop(const int amount, bool fromBank, bool fromBankAndWallet, int vehicleModel)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPEND_BIKE_SHOP", true));
	scriptAssertf(amount >= 0, "%s: NETWORK_SPEND_BIKE_SHOP - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount < 0)
		return;

	MetricSpendGeneric m("BIKE_SHOP", "MODIFY", vehicleModel, amount, fromBank);
	ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
	scriptAssertf(appendResult, "%s:NETWORK_SPEND_BIKE_SHOP - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_BIKER_BUSINESS");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_BIKER_BUSINESS", 0x54829b2e);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPEND_BIKE_SHOP - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandSpendVehicleRequested(const int amount, bool fromBank, bool fromBankAndWallet, int hashWhoRequested, int vehicleModel)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPEND_VEHICLE_REQUESTED", true));
	scriptAssertf(amount >= 0, "%s: NETWORK_SPEND_VEHICLE_REQUESTED - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount < 0)
		return;

	MetricSpendVehicleRequested m(amount, fromBank, hashWhoRequested, vehicleModel);
	ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
	scriptAssertf(appendResult, "%s:NETWORK_SPEND_VEHICLE_REQUESTED - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_CONTACT_SERVICE");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_CONTACT_SERVICE", 0x7EE6A4B0);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPEND_VEHICLE_REQUESTED - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandSpendGunRunning(const int amount, bool fromBank, bool fromBankAndWallet, int vehicleModel)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SPEND_GUNRUNNING", true));
	scriptAssertf(amount >= 0, "%s: NETWORK_SPEND_GUNRUNNING - invalid money amount < %d >, must be >= 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount < 0)
		return;

	MetricSpendGeneric m("GUNRUNNING", "CM_Veh_REQUEST", vehicleModel, amount, fromBank);
	ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
	scriptAssertf(appendResult, "%s:NETWORK_SPEND_GUNRUNNING - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_CONTACT_SERVICE");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_CONTACT_SERVICE", 0x7EE6A4B0);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPEND_GUNRUNNING - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnAgencySafe(const int amount)
{
	MetricEarnGeneric m("INCOME", "AGENCY", amount);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if (!EarnCash(amount, false, false, &m, statId, ATSTRINGHASH("MONEY_EARN_JOBS", 0x61b8682f)))
	{
		scriptErrorf("%s: NETWORK_EARN_AGENCY_SAFE - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnAwardContract(const int amount, int context)
{
	MetricEarnGeneric m("SEC_CON", "AWARD", amount, context);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if (!EarnCash(amount, false, false, &m, statId, ATSTRINGHASH("MONEY_EARN_JOBS", 0x61b8682f)))
	{
		scriptErrorf("%s: NETWORK_EARN_AWARD_CONTRACT - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnAgencyContract(const int amount, int context)
{
	MetricEarnGeneric m("SEC_CON", "AGENCY", amount, context);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if (!EarnCash(amount, false, false, &m, statId, ATSTRINGHASH("MONEY_EARN_JOBS", 0x61b8682f)))
	{
		scriptErrorf("%s: NETWORK_EARN_AGENCY_CONTRACT - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnAwardPhone(const int amount, int context)
{
	MetricEarnGeneric m("PHONE_HIT", "AWARD", amount, context);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if (!EarnCash(amount, false, false, &m, statId, ATSTRINGHASH("MONEY_EARN_JOBS", 0x61b8682f)))
	{
		scriptErrorf("%s: NETWORK_EARN_AWARD_PHONE - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnAgencyPhone(const int amount, int context, const int bonusAmount = 0)
{
	MetricEarnGeneric m("PHONE_HIT", "AGENCY", amount + bonusAmount, context, bonusAmount);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if (!EarnCash(amount + bonusAmount, false, false, &m, statId, ATSTRINGHASH("MONEY_EARN_JOBS", 0x61b8682f)))
	{
		scriptErrorf("%s: NETWORK_EARN_AGENCY_PHONE - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnAwardFixerMission(const int amount, int context)
{
	MetricEarnGeneric m("AGENCY_STORY", "AWARD", amount, context);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if (!EarnCash(amount, false, false, &m, statId, ATSTRINGHASH("MONEY_EARN_JOBS", 0x61b8682f)))
	{
		scriptErrorf("%s: NETWORK_EARN_AWARD_FIXER_MISSION - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnFixerPrep(const int amount, int context)
{
	MetricEarnGeneric m("AGENCY_STORY", "PREP", amount, context);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if (!EarnCash(amount, false, false, &m, statId, ATSTRINGHASH("MONEY_EARN_JOBS", 0x61b8682f)))
	{
		scriptErrorf("%s: NETWORK_EARN_FIXER_PREP - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnFixerFinale(const int amount, int context)
{
	MetricEarnGeneric m("AGENCY_STORY", "FINALE", amount, context);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if (!EarnCash(amount, false, false, &m, statId, ATSTRINGHASH("MONEY_EARN_JOBS", 0x61b8682f)))
	{
		scriptErrorf("%s: NETWORK_EARN_FIXER_FINALE - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnFixerAgencyShortTrip(const int amount, int context)
{
	MetricEarnGeneric m("SHORT_TRIP", "AGENCY", amount, context);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if (!EarnCash(amount, false, false, &m, statId, ATSTRINGHASH("MONEY_EARN_JOBS", 0x61b8682f)))
	{
		scriptErrorf("%s: NETWORK_EARN_FIXER_AGENCY_SHORT_TRIP- Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnAwardShortTrip(const int amount, int context)
{
	MetricEarnGeneric m("SHORT_TRIP", "AWARD", amount, context);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if (!EarnCash(amount, false, false, &m, statId, ATSTRINGHASH("MONEY_EARN_JOBS", 0x61b8682f)))
	{
		scriptErrorf("%s: NETWORK_EARN_AWARD_SHORT_TRIP - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnUpgradeAgency(const int amount, int context)
{
	MetricEarnGeneric m("LOCATION", "UPGRADE_AGENCY", amount, context);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if (!EarnCash(amount, false, false, &m, statId, ATSTRINGHASH("MONEY_EARN_JOBS", 0x61b8682f)))
	{
		scriptErrorf("%s: NETWORK_EARN_UPGRADE_AGENCY - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnFixerRivalDelivery(const int amount, int asset)
{
	MetricEarnGeneric m("SEC_CON", "RIVAL_DELIVERY", amount, asset);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if (!EarnCash(amount, false, false, &m, statId, ATSTRINGHASH("MONEY_EARN_JOBS", 0x61b8682f)))
	{
		scriptErrorf("%s: NETWORK_EARN_FIXER_RIVAL_DELIVERY - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandSpendApartmentUtilities(int amount, bool fromBank, bool fromBankAndWallet, srcApartmentUtilities* data)
{
	scriptAssertf(data, "%s: NETWORK_SPEND_APARTMENT_UTILITIES - NULL srcApartmentUtilities.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!data)
		return;

	#if __NET_SHOP_ACTIVE
	const s64 nonceSeed = GameTransactionSessionMgr::Get().GetNonceForTelemetry();
	#endif // __NET_SHOP_ACTIVEE

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_PROPERTY_UTIL");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_PROPERTY_UTIL", 0x8526aff3);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPEND_APARTMENT_UTILITIES - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	else
	{
	#if __NET_SHOP_ACTIVE
		GameTransactionSessionMgr::Get().SetNonceForTelemetry(nonceSeed);
	#endif // __NET_SHOP_ACTIVE

		if (data->m_LowAptFees >= 0)
		{
			MetricSpendUtilityBills m(data->m_LowAptFees, fromBank);
			ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
			scriptAssertf(appendResult, "%s:NETWORK_SPEND_BUSINESS_PROPERTY_FEES - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
		if (data->m_MedAptFees >= 0)
		{
			MetricSpendUtilityBills m(data->m_MedAptFees, fromBank);
			ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
			scriptAssertf(appendResult, "%s:NETWORK_SPEND_BUSINESS_PROPERTY_FEES - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
		if (data->m_HighAptFees >= 0)
		{
			MetricSpendUtilityBills m(data->m_HighAptFees, fromBank);
			ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
			scriptAssertf(appendResult, "%s:NETWORK_SPEND_BUSINESS_PROPERTY_FEES - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
		if (data->m_YachtFees >= 0)
		{
			MetricSpendUtilityBills m(data->m_YachtFees, fromBank);
			ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
			scriptAssertf(appendResult, "%s:NETWORK_SPEND_BUSINESS_PROPERTY_FEES - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
		if (data->m_FacilityFees >= 0)
		{
			MetricSpendUtilityBills m(data->m_FacilityFees, fromBank);
			ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
			scriptAssertf(appendResult, "%s:NETWORK_SPEND_BUSINESS_PROPERTY_FEES - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
		if (data->m_PenthouseFees >= 0)
		{
			MetricSpendUtilityBills m(data->m_PenthouseFees, fromBank);
			ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
			scriptAssertf(appendResult, "%s:NETWORK_SPEND_BUSINESS_PROPERTY_FEES - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
		if (data->m_KosatkaFees >= 0)
		{
			MetricSpendGeneric m("SUBMARINE", "UTILITY_FEE", 0, data->m_KosatkaFees, fromBank);
			ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
			scriptAssertf(appendResult, "%s:NETWORK_SPEND_BUSINESS_PROPERTY_FEES - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
		if (data->m_CleanerFees >= 0)
		{
			MetricSpendUtilityBills m(data->m_CleanerFees, fromBank);
			ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
			scriptAssertf(appendResult, "%s:NETWORK_SPEND_BUSINESS_PROPERTY_FEES - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
}

void CommandSpendBusinessUtilities(int amount, bool fromBank, bool fromBankAndWallet, srcBusinessUtilities* data)
{
	scriptAssertf(data, "%s: NETWORK_SPEND_BUSINESS_PROPERTY_FEES - NULL srcBusinessUtilities.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!data)
		return;

#if __NET_SHOP_ACTIVE
	const s64 nonceSeed = GameTransactionSessionMgr::Get().GetNonceForTelemetry();
#endif // __NET_SHOP_ACTIVEE

	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_SPENT_PROPERTY_UTIL");
	static const int itemId = ATSTRINGHASH("MONEY_SPENT_PROPERTY_UTIL", 0x8526aff3);
	if (!SpendCash(amount, fromBank, fromBankAndWallet, false, false, nullptr, statId, itemId))
	{
		scriptErrorf("%s: NETWORK_SPEND_BUSINESS_PROPERTY_FEES - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	else
	{
#if __NET_SHOP_ACTIVE
		GameTransactionSessionMgr::Get().SetNonceForTelemetry(nonceSeed);
#endif // __NET_SHOP_ACTIVE

		if (data->m_BunkerFees >= 0)
		{
			MetricSpendUtilityBills m(data->m_BunkerFees, fromBank);
			ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
			scriptAssertf(appendResult, "%s:NETWORK_SPEND_BUSINESS_PROPERTY_FEES - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
		if (data->m_WeedFees >= 0)
		{
			MetricSpendUtilityBills m(data->m_WeedFees, fromBank);
			ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
			scriptAssertf(appendResult, "%s:NETWORK_SPEND_BUSINESS_PROPERTY_FEES - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
		if (data->m_MethFees >= 0)
		{
			MetricSpendUtilityBills m(data->m_MethFees, fromBank);
			ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
			scriptAssertf(appendResult, "%s:NETWORK_SPEND_BUSINESS_PROPERTY_FEES - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
		if (data->m_DocForgeFees >= 0)
		{
			MetricSpendUtilityBills m(data->m_DocForgeFees, fromBank);
			ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
			scriptAssertf(appendResult, "%s:NETWORK_SPEND_BUSINESS_PROPERTY_FEES - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
		if (data->m_FakeCashFees >= 0)
		{
			MetricSpendUtilityBills m(data->m_FakeCashFees, fromBank);
			ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
			scriptAssertf(appendResult, "%s:NETWORK_SPEND_BUSINESS_PROPERTY_FEES - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
		if (data->m_CocaineFees >= 0)
		{
			MetricSpendUtilityBills m(data->m_CocaineFees, fromBank);
			ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
			scriptAssertf(appendResult, "%s:NETWORK_SPEND_BUSINESS_PROPERTY_FEES - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
		if (data->m_HangarFees >= 0)
		{
			MetricSpendUtilityBills m(data->m_HangarFees, fromBank);
			ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
			scriptAssertf(appendResult, "%s:NETWORK_SPEND_BUSINESS_PROPERTY_FEES - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
		if (data->m_NightclubFees >= 0)
		{
			MetricSpent_MegaBusiness m("NIGHTCLUB", "DAILY_UTILITY_COSTS", 0, data->m_NightclubFees, fromBank);
			ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
			scriptAssertf(appendResult, "%s:NETWORK_SPEND_BUSINESS_PROPERTY_FEES - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
		if (data->m_NightclubStaff >= 0)
		{
			MetricSpent_MegaBusiness m("NIGHTCLUB", "DAILY_STAFF_COSTS", 0, data->m_NightclubStaff, fromBank);
			ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
			scriptAssertf(appendResult, "%s:NETWORK_SPEND_BUSINESS_PROPERTY_FEES - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
		if (data->m_ExecOfficeFees >= 0)
		{
			MetricSpentVipUtilityCharges m(data->m_ExecOfficeFees, fromBank);
			ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
			scriptAssertf(appendResult, "%s:NETWORK_SPEND_BUSINESS_PROPERTY_FEES - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
		if (data->m_ExecAssistantFees > 0)
		{
			MetricSpendEmployeeWage m(data->m_ExecAssistantFees, fromBank);
			ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
			scriptAssertf(appendResult, "%s:NETWORK_SPEND_BUSINESS_PROPERTY_FEES - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
		if (data->m_SmallWhouseFees >= 0)
		{
			MetricSpentVipUtilityCharges m(data->m_SmallWhouseFees, fromBank);
			ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
			scriptAssertf(appendResult, "%s:NETWORK_SPEND_BUSINESS_PROPERTY_FEES - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
		if (data->m_MediumWhouseFees >= 0)
		{
			MetricSpentVipUtilityCharges m(data->m_MediumWhouseFees, fromBank);
			ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
			scriptAssertf(appendResult, "%s:NETWORK_SPEND_BUSINESS_PROPERTY_FEES - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
		if (data->m_LargeWhouseFees >= 0)
		{
			MetricSpentVipUtilityCharges m(data->m_LargeWhouseFees, fromBank);
			ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
			scriptAssertf(appendResult, "%s:NETWORK_SPEND_BUSINESS_PROPERTY_FEES - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
		if (data->m_ArcadeFees >= 0)
		{
			MetricSpentVipUtilityCharges m(data->m_ArcadeFees, fromBank);
			ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
			scriptAssertf(appendResult, "%s:NETWORK_SPEND_BUSINESS_PROPERTY_FEES - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
		if (data->m_AutoShopFees >= 0)
		{
			MetricSpendGeneric m("AUTOSHOP", "UTILITY_FEE", 0, data->m_AutoShopFees, fromBank);
			ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
			scriptAssertf(appendResult, "%s:NETWORK_SPEND_BUSINESS_PROPERTY_FEES - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
		if (data->m_FixerAgencyFees >= 0)
		{
			MetricSpendUtilityBills m(data->m_FixerAgencyFees, fromBank);
			ASSERT_ONLY(bool appendResult = ) APPEND_METRIC(m);
			scriptAssertf(appendResult, "%s:NETWORK_SPEND_BUSINESS_PROPERTY_FEES - Failed to append Purchase metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
}

void CommandEarnSightseeingReward(const int amount, const int locBonus, const int milBonus, int location)
{
	static const int AMOUNT_LIMIT = 540000;
	scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_SIGHTSEEING_REWARD - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount > AMOUNT_LIMIT)
		return;

#if __NET_SHOP_ACTIVE
	const s64 nonceSeed = GameTransactionSessionMgr::Get().GetNonceForTelemetry();
	GameTransactionSessionMgr::Get().SetNonceForTelemetry(nonceSeed);
#endif // __NET_SHOP_ACTIVE

	MetricEarnGeneric m("NONE", "SIGHTSEEING", amount, location, locBonus + milBonus);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if (!EarnCash(amount, false, false, &m, statId, ATSTRINGHASH("MONEY_EARN_JOBS", 0x61b8682f)))
	{
		scriptErrorf("%s: NETWORK_EARN_SIGHTSEEING_REWARD - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnBikerShop(const int amount, const int vehicleModel)
{
	static const int AMOUNT_LIMIT = 325000;
	scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_BIKER_SHOP - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount > AMOUNT_LIMIT)
		return;

	MetricEarnGeneric m("BUSINESS", "BIKE_SHOP", amount, vehicleModel);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if (!EarnCash(amount, false, false, &m, statId, ATSTRINGHASH("MONEY_EARN_JOBS", 0x61b8682f)))
	{
		scriptErrorf("%s: NETWORK_EARN_BIKER_SHOP - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnBiker(const int amount)
{
	static const int AMOUNT_LIMIT = 300000;
	scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_EARN_BIKER - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount > AMOUNT_LIMIT)
		return;

	MetricEarnGeneric m("INCOME", "BIKER", amount);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if (!EarnCash(amount, false, false, &m, statId, ATSTRINGHASH("MONEY_EARN_JOBS", 0x61b8682f)))
	{
		scriptErrorf("%s: NETWORK_EARN_BIKER - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandEarnYohanSourceGoods(const int amount, const int nightclub, const int qty, const int missionCompleted)
{
	static const int AMOUNT_LIMIT = 150000;
	scriptAssertf(amount <= AMOUNT_LIMIT, "%s: NETWORK_YOHAN_SOURCE_GOODS - invalid money amount < %d >, must be < %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, AMOUNT_LIMIT);
	if (amount > AMOUNT_LIMIT)
		return;

	MetricEarnGeneric m("NONE", "BUSINESS_HUB_SOURCE", amount, nightclub, qty, missionCompleted);
	const StatId statId = StatsInterface::GetStatsModelHashId("MONEY_EARN_JOBS");
	if (!EarnCash(amount, false, false, &m, statId, ATSTRINGHASH("MONEY_EARN_JOBS", 0x61b8682f)))
	{
		scriptErrorf("%s: NETWORK_YOHAN_SOURCE_GOODS - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

/*
//////////////////////////////////////////////////////////////////////////

char* ProcessChipGift(int& NOTPC_ONLY(result), int& NOTPC_ONLY(valueIsCredited), scrTextLabel63* NOTPC_ONLY(outLabel))
{
#if !RSG_PC
	result = 0;
	valueIsCredited = 0;

#if __NET_SHOP_ACTIVE
	//We are dealing with gifting server side.
	if (!NETWORK_SHOPPING_MGR.ShouldDoNullTransaction())
		return 0;
#endif // __NET_SHOP_ACTIVE

	NOTFINAL_ONLY(VERIFY_STATS_LOADED("PROCESS_CHIP_GIFT", false);)

	scriptAssertf(NetworkInterface::IsGameInProgress(), "%s: ProcessChipGift - Can't access network player cash while in single player", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!NetworkInterface::IsGameInProgress())
		return 0;

	scriptAssertf(NetworkInterface::IsInFreeMode(), "%s: ProcessChipGift - Can't access network player cash while not in freemode", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!NetworkInterface::IsInFreeMode())
		return 0;

	s64 chipGiftAmount = StatsInterface::GetInt64Stat(STAT_CHIP_GIFT);
	u64 labelId1 = StatsInterface::GetUInt64Stat(STAT_CHIP_GIFT_LABEL_1);
	u64 labelId2 = StatsInterface::GetUInt64Stat(STAT_CHIP_GIFT_LABEL_2);

	scriptDebugf1("STAT_CHIP_GIFT_LABEL_1 = %lu    -    STAT_CHIP_GIFT_LABEL_2 = %lu ", labelId1, labelId2);

	char label[16];
	for (int i = 0; i < sizeof(u64); i++)
	{
		label[i] = ((char*)&labelId1)[sizeof(u64) - 1 - i];
		label[i + sizeof(u64)] = ((char*)&labelId2)[sizeof(u64) - 1 - i];
	}
	safecpy(*outLabel, label);

	if ((*outLabel) && (*outLabel)[0] != 0)
	{
		if (TheText.DoesTextLabelExist((*outLabel)))
		{
			scriptDebugf1("PROCESS_CHIP_GIFT - Label %s exists ", (*outLabel));
		}
		else
		{
			scriptErrorf("PROCESS_CHIP_GIFT - Label %s does not exist", (*outLabel));
			scriptAssertf(0, "%s: ProcessChipGift - TextLabel doesnt exist : %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), (*outLabel));
			(*outLabel)[0] = 0;
		}
	}
	else
	{
		(*outLabel)[0] = 0;
	}
	StatsInterface::SetStatData(STAT_CHIP_GIFT_LABEL_1, 0);
	StatsInterface::SetStatData(STAT_CHIP_GIFT_LABEL_2, 0);

	if (chipGiftAmount > 0)
	{
		scriptDebugf1("PROCESS_CHIP_GIFT - credit %" I64FMT "d", chipGiftAmount);

		result = 1;
		valueIsCredited = 1;

		const s64 amountCredited = EarnChipsFromRockstar(chipGiftAmount);
		if (amountCredited > 0)
		{
			StatsInterface::SetStatData(STAT_CHIP_GIFT, chipGiftAmount - amountCredited);
			StatsInterface::SetStatData(STAT_CHIP_GIFT_CREDITED, StatsInterface::GetUInt64Stat(STAT_CHIP_GIFT_CREDITED) + (u64)amountCredited);
		}
	}
	else if (chipGiftAmount < 0)
	{
		result = 1;
		chipGiftAmount *= -1;

		scriptDebugf1("PROCESS_CHIP_GIFT - debit %" I64FMT "d", chipGiftAmount);

		//Do the deduction
		const s64 amountSpent = SpentChipsFromRockstar(chipGiftAmount);
		scriptDebugf1("PROCESS_CHIP_GIFT able to deduct %" I64FMT "d", amountSpent);

		//set it back to original value
		s64 cashGiftRemainder = chipGiftAmount - amountSpent;

		//The ability to specify with each gift whether or not to save any remainder 
		//  for future processing (vs. just making it a one-shot change)
		if (cashGiftRemainder != 0)
		{
			const bool leaveRemainder = StatsInterface::GetBooleanStat(STAT_CHIP_GIFT_LEAVE_REMAINDER);
			if (!leaveRemainder)
			{
				scriptDebugf1("PROCESS_CHIP_GIFT - Do NOT leave a remainder, clearing amount='%" I64FMT "d'", chipGiftAmount);
				cashGiftRemainder = 0;
			}
		}

		StatsInterface::SetStatData(STAT_CHIP_GIFT, cashGiftRemainder);
		StatsInterface::SetStatData(STAT_CHIP_GIFT_DEBITED, StatsInterface::GetUInt64Stat(STAT_CHIP_GIFT_DEBITED) + (u64)amountSpent);
	}

	static char s_chipgift[64];
	sysMemSet(s_chipgift, 0, sizeof(s_chipgift));

	if (chipGiftAmount != 0)
	{
		CTextConversion::FormatIntForHumans(chipGiftAmount, s_chipgift, NELEM(s_chipgift), "%s", false); // removing commas as per SteveW
	}

	return s_chipgift;

#else //RSG_PC

	return 0;

#endif //!RSG_PC
}

//////////////////////////////////////////////////////////////////////////
*/

//Process cash gifting during transitions. It will Credit/Debit a certain amount of cash from the players Wallets and Bank.
char* ProcessCashGift(int& NOTPC_ONLY(result), int& NOTPC_ONLY(valueIsCredited), scrTextLabel63* NOTPC_ONLY(outLabel))
{
#if !RSG_PC
	result = 0;
	valueIsCredited = 0;

#if __NET_SHOP_ACTIVE
	//We are dealing with gifting server side.
	if(!NETWORK_SHOPPING_MGR.ShouldDoNullTransaction())
		return 0;
#endif // __NET_SHOP_ACTIVE

	NOTFINAL_ONLY(VERIFY_STATS_LOADED("PROCESS_CASH_GIFT", false);)

	scriptAssertf(NetworkInterface::IsGameInProgress(), "%s: ProcessCashGift - Can't access network player cash while in single player", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!NetworkInterface::IsGameInProgress())
		return 0;

	scriptAssertf(NetworkInterface::IsInFreeMode(), "%s: ProcessCashGift - Can't access network player cash while not in freemode", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!NetworkInterface::IsInFreeMode())
		return 0;

	s64 cashGiftAmount = StatsInterface::GetInt64Stat( STAT_CASH_GIFT_NEW ) + (s64)StatsInterface::GetIntStat( STAT_CASH_GIFT );
	u64 labelId1= StatsInterface::GetUInt64Stat( STAT_CASH_GIFT_LABEL_1 );
	u64 labelId2= StatsInterface::GetUInt64Stat( STAT_CASH_GIFT_LABEL_2 );

	scriptDebugf1("STAT_CASH_GIFT_LABEL_1 = %" PRIu64 "    -    STAT_CASH_GIFT_LABEL_2 = %" PRIu64 " ", labelId1, labelId2);

	char label[16];
	for(int i=0; i<sizeof(u64); i++)
	{
		label[i]				= ( (char*) &labelId1 )[sizeof(u64)-1-i];
		label[i+sizeof(u64)]	= ( (char*) &labelId2 )[sizeof(u64)-1-i];
	}
	safecpy(*outLabel, label);

	if((*outLabel) && (*outLabel)[0] != 0)
	{
		if(TheText.DoesTextLabelExist((*outLabel)))
		{
			scriptDebugf1("PROCESS_CASH_GIFT - Label %s exists ", (*outLabel));
		}
		else
		{
			scriptErrorf("PROCESS_CASH_GIFT - Label %s does not exist", (*outLabel));
			scriptAssertf(0, "%s: ProcessCashGift - TextLabel doesnt exist : %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), (*outLabel));
			(*outLabel)[0]=0;
		}
	}
	else
	{
		(*outLabel)[0]=0;
	}
	StatsInterface::SetStatData(STAT_CASH_GIFT_LABEL_1, 0);
	StatsInterface::SetStatData(STAT_CASH_GIFT_LABEL_2, 0);

	if (cashGiftAmount > 0)
	{
		result = 1;
		valueIsCredited = 1;

		scriptDebugf1("PROCESS_CASH_GIFT - credit %" I64FMT "d", cashGiftAmount);

		StatsInterface::SetStatData(STAT_CASH_GIFT_NEW, (s64)0);
		StatsInterface::SetStatData(STAT_CASH_GIFT, 0);

		int totalcredit = StatsInterface::GetIntStat(STAT_CASH_GIFT_RECEIVED);
		if (totalcredit > 0)
		{
			StatsInterface::SetStatData(STAT_CASH_GIFT_RECEIVED, 0);
			StatsInterface::SetStatData(STAT_CASH_GIFT_CREDITED, StatsInterface::GetUInt64Stat(STAT_CASH_GIFT_CREDITED) + (u64)totalcredit);
		}

		StatsInterface::SetStatData(STAT_CASH_GIFT_CREDITED, StatsInterface::GetUInt64Stat(STAT_CASH_GIFT_CREDITED) + (u64)cashGiftAmount);

		EarnMoneyFromRockstar64( cashGiftAmount );
	}
	else if (cashGiftAmount < 0)
	{
		cashGiftAmount *= -1;

		result = 1;

		scriptDebugf1("PROCESS_CASH_GIFT - debit %" I64FMT "d", cashGiftAmount);

		//Do the deduction
		const s64 amountSpent = MoneyInterface::SpendEarnedCashFromAll( cashGiftAmount );

		//set it back to original value
		cashGiftAmount *= -1;
		s64 cashGiftRemainder = cashGiftAmount + amountSpent;

		scriptDebugf1("PROCESS_CASH_GIFT able to deduct %" I64FMT "d", amountSpent);

		//The ability to specify with each gift whether or not to save any remainder for future processing (vs. just making it a one-shot change)
		if (cashGiftRemainder != 0)
		{
			const bool leaveRemainder = StatsInterface::GetBooleanStat( STAT_CASH_GIFT_LEAVE_REMAINDER );
			if (!leaveRemainder)
			{
				scriptDebugf1("PROCESS_CASH_GIFT - Do NOT leave a remainder, clearing amount='%" I64FMT "d'", cashGiftAmount);
				cashGiftRemainder = 0;
			}
		}

		StatsInterface::SetStatData(STAT_CASH_GIFT, 0);
		StatsInterface::SetStatData(STAT_CASH_GIFT_NEW, cashGiftRemainder);
		StatsInterface::SetStatData(STAT_CASH_GIFT_DEBITED, StatsInterface::GetUInt64Stat(STAT_CASH_GIFT_DEBITED) + (u64)amountSpent);

		SendMoneyMetric(MetricSpentOnRockstar(amountSpent, true));
	}

	static char s_cashgift[64];
	sysMemSet(s_cashgift, 0, sizeof(s_cashgift));

	if (cashGiftAmount != 0)
	{
		CFrontendStatsMgr::FormatInt64ToCash(cashGiftAmount, s_cashgift, NELEM(s_cashgift), false);  // removing commas as per SteveW

		//Generate script event
		if (cashGiftAmount < 0)
		{
			static const int itemId = ATSTRINGHASH("MONEY_SPENT_ROCKSTAR_AWARD", 0xa32961a9);
			CEventNetworkCashTransactionLog tEvent(-1, CEventNetworkCashTransactionLog::CASH_TRANSACTION_BANK, CEventNetworkCashTransactionLog::CASH_TRANSACTION_WITHDRAW, cashGiftAmount, NetworkInterface::GetLocalGamerHandle(), itemId);
			GetEventScriptNetworkGroup()->Add(tEvent);
		}
	}

	return s_cashgift;

#else //RSG_PC
	return 0;

#endif //!RSG_PC
}


//////////////////////////////////////////////////////////////////////////
//Virtual cash functions

static const s64 MAX_SCRIPT_CASH_AMOUNT = 2147483647;

int CommandGetVCWalletBalance(const int character)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_GET_VC_WALLET_BALANCE", false);)

	const s64 cashAmount = MoneyInterface::GetVCWalletBalance(character);
	if (cashAmount > MAX_SCRIPT_CASH_AMOUNT)
		return ((int)MAX_SCRIPT_CASH_AMOUNT);

	return ((int)cashAmount);
}

int CommandGetVCBankBalance()
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_GET_VC_BANK_BALANCE", false);)
	scriptAssertf(!StatsInterface::CloudFileLoadPending(0), "%s :  - Trying to access money without loading the savegame.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	scriptAssertf(!StatsInterface::CloudFileLoadFailed(0), "%s :  - Trying to access money but stats loaded have failed.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	const s64 cashAmount = MoneyInterface::GetVCBankBalance();
	if (cashAmount > MAX_SCRIPT_CASH_AMOUNT)
		return ((int)MAX_SCRIPT_CASH_AMOUNT);

	return ((int)cashAmount);
}

int CommandGetVCBalance()
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_GET_VC_BALANCE", false);)

	const s64 cashAmount = MoneyInterface::GetVCBalance();
	if (cashAmount > MAX_SCRIPT_CASH_AMOUNT)
		return ((int)MAX_SCRIPT_CASH_AMOUNT);

	return ((int)cashAmount);
}

int CommandGetEVCBalance()
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_GET_EVC_BALANCE", false);)

	const s64 cashAmount = MoneyInterface::GetEVCBalance();
	if (cashAmount > MAX_SCRIPT_CASH_AMOUNT)
		return ((int)MAX_SCRIPT_CASH_AMOUNT);

	return ((int)cashAmount);
}

int CommandGetPVCBalance()
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_GET_PVC_BALANCE", false);)

	const s64 cashAmount = MoneyInterface::GetPVCBalance();
	if (cashAmount > MAX_SCRIPT_CASH_AMOUNT)
		return ((int)MAX_SCRIPT_CASH_AMOUNT);

	return ((int)cashAmount);
}

char* CommandGetStringWalletBalance(const int character)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_GET_STRING_WALLET_BALANCE", false);)

	static char s_walletBuff[64];

	const s64 cashAmount = MoneyInterface::GetVCWalletBalance(character);
	CFrontendStatsMgr::FormatInt64ToCash(cashAmount, s_walletBuff, NELEM(s_walletBuff), false);  // removing commas as per SteveW

	return s_walletBuff;
}

char* CommandGetStringBankBalance( )
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_GET_STRING_BANK_BALANCE", false);)

	static char s_bankBuff[64];

	const s64 cashAmount = MoneyInterface::GetVCBankBalance();
	CFrontendStatsMgr::FormatInt64ToCash(cashAmount, s_bankBuff, NELEM(s_bankBuff), false);  // removing commas as per SteveW

	return s_bankBuff;
}

char* CommandGetStringBankWalletBalance( const int character )
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_GET_STRING_BANK_WALLET_BALANCE", false);)

	static char s_bankWalletBuff[64];

	const s64 bankAmount = MoneyInterface::GetVCBankBalance();
	const s64 walletAmount = MoneyInterface::GetVCWalletBalance(character);
	CFrontendStatsMgr::FormatInt64ToCash(bankAmount+walletAmount, s_bankWalletBuff, NELEM(s_bankWalletBuff), false);  // removing commas as per SteveW

	return s_bankWalletBuff;
}

bool  CommandGetCanSpendFromWallet(int amount, const int character)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_GET_CAN_SPEND_FROM_WALLET", false);)
	const s64 cashAmount = MoneyInterface::GetVCWalletBalance(character);
	return (cashAmount >= ((s64)(amount)));
}

bool  CommandGetCanSpendFromBank(int amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_GET_CAN_SPEND_FROM_BANK", false);)
	const s64 cashAmount = MoneyInterface::GetVCBankBalance();
	return (cashAmount >= ((s64)(amount)));
}

bool  CommandGetCanSpendFromBanAndWallet(int amount, const int character)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_GET_CAN_SPEND_FROM_BANK_AND_WALLET", false);)
	const s64 wallet = MoneyInterface::GetVCWalletBalance(character);
	const s64 bank = MoneyInterface::GetVCBankBalance();
	return ((wallet+bank) >= ((s64)(amount)));
}

int CommandGetPVCTransferBalance()
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_GET_PVC_TRANSFER_BALANCE", false);)
	return (MoneyInterface::GetAmountAvailableForTransfer());
}

int CommandGetRemainingTransferBalance()
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_GET_REMAINING_TRANSFER_BALANCE", false);)
	return (MoneyInterface::GetAmountAvailableForTransfer());
}

bool CommandGetCanTransferCash(int amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_GET_CAN_TRANSFER_CASH", false);)

	bool result = false;

	//Verify if we have enough money in the WALLET
	if (amount > MoneyInterface::GetVCWalletBalance())
	{
		scriptDebugf1("NETWORK_GET_CAN_TRANSFER_CASH - amount=%d, WALLET=%" I64FMT "d, result=FALSE", amount, MoneyInterface::GetVCWalletBalance());
		return result;
	}

	//Get the correct money amounts and stats
	const s64 earntCash = MoneyInterface::GetEVCBalance();
	const s64 paidCash  = MoneyInterface::GetPVCBalance();

	int amountOfEVC = 0;
	int amountOfPVC = 0;

	if ( amount <= earntCash )
	{
		amountOfEVC = amount;
	}
	else if ( scriptVerify(amount <= (paidCash + earntCash)) )
	{
		amountOfEVC = (int)paidCash;
		amountOfPVC = (int)(amount - paidCash);
	}

	float totalUSDvalue = 0.0f;

	float pxr = MoneyInterface::GetPXRValue();
	if(amountOfPVC > 0 && pxr > 0.0f)
	{
		totalUSDvalue = amountOfPVC / pxr;
	}

	result = MoneyInterface::CanTransferCash((s64)amount, totalUSDvalue);

	scriptDebugf1("NETWORK_GET_CAN_TRANSFER_CASH - amount=%d, WALLET=%" I64FMT "d, result=%s", amount, MoneyInterface::GetVCWalletBalance(), result?"TRUE":"FALSE");

	return result;
}

bool  CommandGetCanReceiveCash(const int amount, const int amountOfEVC, const int amountOfPVC, const float amountOfUSDE)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_CAN_RECEIVE_PLAYER_CASH", false);)

	scriptDebugf1("NETWORK_CAN_RECEIVE_PLAYER_CASH - amount=%d, amountOfEVC=%d, amountOfPVC=%d, amountOfUSDE=%f", amount, amountOfEVC, amountOfPVC, amountOfUSDE);

	scriptAssertf(amount>0, "%s: NETWORK_CAN_RECEIVE_PLAYER_CASH - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return false;

	scriptAssertf(amount == (amountOfEVC+amountOfPVC), "%s: NETWORK_CAN_RECEIVE_PLAYER_CASH - invalid money amount=<%d>  != amountEVC=<%d>+amountPVC=<%d>.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount, amountOfEVC, amountOfPVC);
	if (amount != (amountOfEVC+amountOfPVC))
		return false;

	if (amountOfPVC > 0 && !MoneyInterface::CanBuyCash(amount, amountOfUSDE))
	{
		scriptDebugf1("NETWORK_CAN_RECEIVE_PLAYER_CASH - FALSE - CanBuyCash() returning false.");
		return false;
	}

	return true;
}

int CommandRequestVirtualCashWithdrawal( int amount )
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("WITHDRAW_VC", true);)

	scriptAssertf(amount>0, "%s: WITHDRAW_VC - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return 0;

	if (!CAN_SCRIPT_CHANGE_CASH("NETWORK_CHECK_GIVE_PLAYER_CASH"))
		return 0;

    return MoneyInterface::RequestBankCashWithdrawal( amount );
}

bool CommandVirtualCashDeposit( int amount )
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("DEPOSIT_VC", true);)

	scriptAssertf(amount>0, "%s: DEPOSIT_VC - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount<=0)
		return false;

	if (!CAN_SCRIPT_CHANGE_CASH("NETWORK_CHECK_GIVE_PLAYER_CASH"))
		return false;

	bool result = MoneyInterface::CreditBankCashStats(amount);
	scriptAssertf(result, "%s: DEPOSIT_VC - Failed to transfer money amount < %d >, from wallet to bank.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);

    return result;
}

int CommandHasVirtualCashTransferCompleted( int /*transferId*/ )
{
    return true;
}

int CommandHasVirtualCashTransferSucceeded( int /*transferId*/ )
{
    return true;
}

////////////////////////////////////////////////////////////////////////// GEN9

#if GEN9_STANDALONE_ENABLED
bool CommandGetMPWindfallAvailable()
{
	return MoneyInterface::IsWindfallAvailable();
}

bool CommandSetMPWindfallCompleted()
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_SET_MP_WINDFALL_COMPLETED", true);)

	scriptAssertf(!MoneyInterface::HasWindfallStatBeenSet(), "%s: NETWORK_SET_MP_WINDFALL_COMPLETED - Failed windfall has already been completed.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	scriptAssertf(MoneyInterface::IsWindfallAvailable(),     "%s: NETWORK_SET_MP_WINDFALL_COMPLETED - Failed windfall is not available.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if (!MoneyInterface::SetWindfallCompleted())
	{
#if !__FINAL
		scrThread::PrePrintStackTrace();
#endif
		scriptErrorf("%s: NETWORK_MP_WINDFALL_EARN - Failed to complete windfall.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return false;
	}
	return true;
}

int CommandGetMPWindfallBalance()
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_MP_WINDFALL_BALANCE", true);)

	return MoneyInterface::GetWindfallBalance();
}

int CommandGetMPWindfallAwardAmount()
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_GET_MP_WINDFALL_AWARD_AMOUNT", true);)

	return MoneyInterface::GetWindfallAwardAmount();
}

int CommandGetMPWindfallMinimumSpend()
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_GET_MP_WINDFALL_MINIMUM_SPEND", true);)

	return MoneyInterface::GetWindfallMinimumSpend();
}

void CommandMPWindfallEarn(int amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_MP_WINDFALL_EARN", true);)

	scriptAssertf(amount > 0, "%s: NETWORK_MP_WINDFALL_EARN - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount <= 0)
		return;

	if (!MoneyInterface::IncrementWindfallBalance(amount))
	{
		scriptErrorf("%s: NETWORK_MP_WINDFALL_EARN - Failed to earn money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

void CommandMPWindfallSpend(int amount)
{
	NOTFINAL_ONLY(VERIFY_STATS_LOADED("NETWORK_MP_WINDFALL_SPEND", true);)

	scriptAssertf(amount > 0, "%s: NETWORK_MP_WINDFALL_SPEND - invalid money amount < %d >, must be > 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
	if (amount <= 0)
		return;

	if (!MoneyInterface::DecrementWindfallBalance(amount))
	{
		scriptErrorf("%s: NETWORK_MP_WINDFALL_SPEND - Failed to spend money.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}
#endif

//////////////////////////////////////////////////////////////////////////

//
// name:		SetupScriptCommands
// description:	Setup streaming script commands
void SetupScriptCommands()
{
	SCR_REGISTER_SECURE_EXPORT(NETWORK_INITIALIZE_CASH,0xdc5f6189b87e2ce4, InitPlayerCash);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_DELETE_CHARACTER,0xac1facc8f57409cc, DeleteCharacter);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_MANUAL_DELETE_CHARACTER,0x28ea9998417e82ab, ManualDeleteCharacter);

	SCR_REGISTER_SECURE_EXPORT(NETWORK_GET_PLAYER_IS_HIGH_EARNER,0xf9dfcdc2378706de, GetPlayerIsHighEarner);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_CLEAR_CHARACTER_WALLET,0x403200d902b21e67, ClearCharacterWallet);

	// Give event
	SCR_REGISTER_SECURE(NETWORK_GIVE_PLAYER_JOBSHARE_CASH,0x333ce33aa1769463, GivePlayerSharedJobCash);
	// Receive event
	SCR_REGISTER_SECURE(NETWORK_RECEIVE_PLAYER_JOBSHARE_CASH,0x6d35555e34106688, ReceivePlayerSharedJobCash);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_CAN_SHARE_JOB_CASH,0x0a5c97315dfc188e, CanShareJobCash);

	// Refund cash
	SCR_REGISTER_SECURE_EXPORT(NETWORK_REFUND_CASH,0xe35a17754e4a9bbb, RefundCash);
	// Deduct cash
	SCR_REGISTER_SECURE_EXPORT(NETWORK_DEDUCT_CASH,0x9a114d9cad104da4, DeductCash);

	// Can Bet
	SCR_REGISTER_SECURE_EXPORT(NETWORK_MONEY_CAN_BET,0x53c919139434bba9, CommandMoneyCanBet);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_CAN_BET,0xf00c090e006ccd0c, CanBet);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_CASINO_CAN_BET,0x910c57e77efcf8cd, CommandCasinoCanBet);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_CASINO_CAN_BET_PVC,0x014e83a517684e5e, CommandCasinoCanBetPVC);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_CASINO_CAN_BET_AMOUNT,0x19b3ed84baaba783, CommandCasinoCanBetAmount);

	SCR_REGISTER_SECURE_EXPORT(NETWORK_CASINO_CAN_BUY_CHIPS_PVC,0x5d4af7e88039c9fa, CommandCasinoCanBuyChipsWithPvc);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_CASINO_BUY_CHIPS,0xd3ffcbee9841c8f1, CommandCasinoBuyChips);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_CASINO_SELL_CHIPS,0xf746375ef933064e, CommandCasinoSellChips);

	// DeferCashTransactionsUntilShopSave
	SCR_REGISTER_SECURE_EXPORT(NETWORK_DEFER_CASH_TRANSACTIONS_UNTIL_SHOP_SAVE,0x879902101458dbad, DeferCashTransactionsUntilShopSave);

	SCR_REGISTER_SECURE_EXPORT(CAN_PAY_AMOUNT_TO_BOSS,0x2ea806c1e6b8766c, Command_CanPayAmountToBossOrGoon);

	// Earn events
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FROM_PICKUP,0x112ad397202d855e, EarnMoneyFromPickup);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FROM_CASHING_OUT,0x230d7d7500f53c4f, EarnMoneyFromCashingOut);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FROM_GANGATTACK_PICKUP,0xa8059341a93fe044, EarnMoneyFromGangAttackPickup);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_ASSASSINATE_TARGET_KILLED,0x992fcbe7b557e5af, EarnMoneyFromAssassinateTargetKilled);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FROM_ROB_ARMORED_CARS,0xf4736694a1d9ead4, EarnMoneyFromArmoredRobbery);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FROM_CRATE_DROP,0x6a774369e0f40dea, EarnMoneyFromCrateDrop);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FROM_BETTING,0xa22b9d6b2d46cc12, EarnMoneyFromBetting);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FROM_JOB,0x65a61c657aa4707e, EarnMoneyFromJob);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FROM_JOBX2,0xaace66cc21ebd075, EarnMoneyFromJobX2);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FROM_PREMIUM_JOB,0xa34683325f050124, EarnMoneyFromPremiumJob);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FROM_BEND_JOB,0x05cc9e05474790a7, EarnMoneyFromBendJob);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FROM_CHALLENGE_WIN,0x7981f6cdfb2ac34f, EarnMoneyFromChallengeWin);
	SCR_REGISTER_SECURE(NETWORK_EARN_FROM_BOUNTY,0xe365ad8c4e7fc95a, EarnMoneyFromBounty);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FROM_IMPORT_EXPORT,0xdd82843e7135b9c4, EarnMoneyFromImportExport);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FROM_HOLDUPS,0xab0692e771961c3e, EarnMoneyFromHoldups);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FROM_PROPERTY,0xe569a2bf164237e9, EarnMoneyFromProperty);
	SCR_REGISTER_UNUSED(NETWORK_EARN_FROM_DEBUG,0xee943bc0ba675953, EarnMoneyFromDebug);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FROM_AI_TARGET_KILL,0x4fdcf5723ba47465, EarnMoneyFromAiTargetKill);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FROM_NOT_BADSPORT,0xd61770d81b66950a, EarnMoneyFromNotBadSport);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FROM_ROCKSTAR,0xc3f2e167bd70781b, EarnMoneyFromRockstar);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FROM_VEHICLE,0x1ffc73fd03bd46b4, EarnMoneyFromSellingVehicle);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FROM_PERSONAL_VEHICLE,0x1a9ec8bfea238a20, EarnMoneyFromSellingPersonalVehicle);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FROM_DAILY_OBJECTIVES,0x7e16544cdc17951c, EarnFromDailyObjectives);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FROM_AMBIENT_JOB,0xc1f6879b97d260f3, EarnFromAmbientJob);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FROM_JOB_BONUS,0x08ed30b3b6e5fd5c, EarnMoneyFromJobBonusesMain);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FROM_CRIMINAL_MASTERMIND,0xf7983ac1b8265c33, EarnMoneyFromJobBonusesCriminalMind);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_HEIST_AWARD,0x1225e74855825b89, EarnMoneyFromJobBonusesHeistAward);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FIRST_TIME_BONUS,0x7d9ebe60742172c6, EarnMoneyFromJobBonusesFirstTimeBonus);

	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_GOON,0x3243e08305457c6d, Command_EarnGoon);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_BOSS,0xffc480702d8e4846, Command_EarnBoss);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_AGENCY,0x2657fe217212334b, Command_EarnAgency);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FROM_WAREHOUSE,0xb07e86a8889a545a, CommandEarnMoneyFromWarehouse);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FROM_CONTRABAND,0xe8bb3ad37b7ca086, CommandEarnMoneyFromContraband);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FROM_DESTROYING_CONTRABAND,0x1557f4ffedf15230, CommandEarnMoneyFromDestroyingContraband);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FROM_SMUGGLER_WORK,0xc530a5e539148d87, CommandEarnMoneyFromSmugglerWork);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FROM_HANGAR_TRADE,0x7479bf52005e47fe, CommandEarnMoneyFromHangarTrade);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_PURCHASE_CLUB_HOUSE,0x057b2c72e8615612, CommandEarnPurchaseClubHouse);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FROM_BUSINESS_PRODUCT,0xcc091d3a89af23c8, CommandEarnFromBusinessProduct);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FROM_VEHICLE_EXPORT,0x7176349f942a0cfa, CommandEarnFromVehicleExport);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_SMUGGLER_AGENCY,0xe811878e2db4b386, CommandEarnFromSmugglerAgency);

	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_BOUNTY_HUNTER_REWARD,0xd07f11770f2e6012, CommandEarnFromBountyHunterReward);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FROM_BUSINESS_BATTLE,0x60ebb87a5c4cb708, CommandEarnFromBusinessBattle);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FROM_CLUB_MANAGEMENT_PARTICIPATION,0xa94478640dca926e, CommandEarnFromClubManagementParticipation);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FROM_FMBB_PHONECALL_MISSION,0xd2f8a068d640d1ac, CommandEarnFromFMBBPhonecallMission);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FROM_BUSINESS_HUB_SELL,0x83148714b0cea446, CommandEarnFromBusinessHubSell);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FROM_FMBB_BOSS_WORK,0xc1dbaeaa7b7b2570, CommandEarnFromFMBBBossWork);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FMBB_WAGE_BONUS,0x283d44f6dc1e915b, CommandEarnFromFMBBWageBonus);

	// Spend events
	//
	SCR_REGISTER_SECURE(NETWORK_CAN_SPEND_MONEY,0x616226219f841621, CanSpendMoney);
	SCR_REGISTER_SECURE(NETWORK_CAN_SPEND_MONEY2,0x83cd7741e215b226, CanSpendMoney2);
	SCR_REGISTER_SECURE(NETWORK_BUY_ITEM,0x96b0ef45fcc7b79f, BuyItem);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_TAXI,0x633310c9b4b8972c, SpentTaxi);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_PAY_EMPLOYEE_WAGE,0x4f34776bdb8e6055, PayEmployeeWage);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_PAY_MATCH_ENTRY_FEE,0x5ba45508319548c9, PayMatchEntryFee);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_BETTING,0xb52f40295cb9cef3, SpentBetting);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_WAGER,0x50742ed3b0caabbb, SpentWager);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_IN_STRIPCLUB,0xccfb2d3453e63dd0, SpentInStripClub);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_BUY_HEALTHCARE,0xc5284ef993f1f89b, BuyHealthCare);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_BUY_AIRSTRIKE,0x70e4920577d2af8f, BuyAirStrike);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_BUY_BACKUP_GANG,0xdb05d03d2a4414a3, BuyGangBackup);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_BUY_HELI_STRIKE,0x2d153944371c57b5, BuyHeliStrike);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_AMMO_DROP,0xd4757da5ca4abebd, SpentOnAmmoDrop);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_BUY_BOUNTY,0x2a2e293c460e3bc9, BuyBounty);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_BUY_PROPERTY,0x5a192000f9019354, BuyProperty);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_BUY_SMOKES,0x4e0498aed25a8fd4, BuySmokes);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_HELI_PICKUP,0x98d10c65a5ed5819, SpentOnHeliPickup);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_BOAT_PICKUP,0x60baa0f5725cac6e, SpentOnBoatPickup);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_BULL_SHARK,0xb4ce8cbf609f48c9,  SpentOnBullShark);
	SCR_REGISTER_UNUSED(NETWORK_SPENT_FROM_DEBUG,0x98e0e142e7951061, SpentMoneyFromDebug);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_CASH_DROP,0xa789ebe5183e8fe5,  SpentOnCashDrop);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_HIRE_MUGGER,0x20e0724aa92474e3, SpentOnHireMugger);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_ROBBED_BY_MUGGER,0xfd06e55daf765b32, SpentOnRobbedByMugger);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_HIRE_MERCENARY,0xb370da8d10149a81, SpentOnHireMercenary);
	SCR_REGISTER_SECURE(NETWORK_SPENT_BUY_WANTEDLEVEL,0x2e2b0a3651e8dcc2, SpentOnWantedLevel);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_BUY_OFFTHERADAR,0xddb99faa2da252af, SpentOnOffTheRadar);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_BUY_REVEAL_PLAYERS,0xa68e39f91108707e, SpentOnRevealPlayersInRadar);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_CARWASH,0x998bb74c3a957b00, SpentOnCarWash);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_CINEMA,0x2a392968944d2f3e, SpentOnCinema);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_TELESCOPE,0xab191a4200d76617, SpentOnTelescope);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_HOLDUPS,0xa6fffa63c0d556ce, SpentOnHoldUps);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_BUY_PASSIVE_MODE,0xdc639be2424a7a98, SpentOnPassiveMode);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_BANK_INTEREST,0x8c2c12d337d64f6d, SpentOnBankInterest);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_PROSTITUTES,0xa03beb503d736664, SpentOnProstitute);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_ARREST_BAIL,0xf07afb3413c8e424, SpentOnArrestBail);
	SCR_REGISTER_SECURE(NETWORK_SPENT_PAY_VEHICLE_INSURANCE_PREMIUM,0xea92f2d421adba50, SpentOnVehicleInsurancePremium);
	SCR_REGISTER_SECURE(NETWORK_SPENT_CALL_PLAYER,0x95a42c7d8591e15a, SpentOnPlayerCall);
	SCR_REGISTER_SECURE(NETWORK_SPENT_BOUNTY,0xc678817aa4802ff6, SpentOnBounty);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_FROM_ROCKSTAR,0xf922fbc9833e0af3, SpentOnRockstar);
	DLC_SCR_REGISTER_SECURE(NETWORK_SPEND_EARNED_FROM_BANK_AND_WALLETS,0xabe1e3ddb9c6dcc7, SpendEarnedCashFromAll);
	DLC_SCR_REGISTER_SECURE(PROCESS_CASH_GIFT,0x3d3f83924782979f, ProcessCashGift);
	//DLC_SCR_REGISTER_SECURE(PROCESS_CHIP_GIFT,0x6c04121c67d49e6d, ProcessChipGift);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_MOVE_SUBMARINE,0x53235ac39c25ba6f, Command_SpentOnMoveSubmarine);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_PLAYER_HEALTHCARE,0xd9f1acf1d834ea04, BuyPlayerHealthCare);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_NO_COPS,0xb69b75efecea8c8a, SpentOnNoCops);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_CARGO_SOURCING, 0x948705f6f9c50824, SpentOnCargoSourcing);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_REQUEST_JOB,0x70dd65d169b664fe, SpentOnJobRequest);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_REQUEST_HEIST,0x39746251feabf053, SpentOnBendRequest);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_BUY_FAIRGROUND_RIDE,0xe48ce8ba3679b268, SpentOnFairgroundRides);
	DLC_SCR_REGISTER_SECURE(NETWORK_ECONOMY_HAS_FIXED_CRAZY_NUMBERS,0x6b6922fad2e70431, ProcessEconomyHasFixedCrazyNumbers);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_JOB_SKIP,0x6ba4aa7a6927fb1a, SpentOnJobSkip);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_BOSS_GOON,0x2ccde4679591a98f, SpentOnBoss);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_GOON,0x08da56d3f428bd72, Command_SpentGoon);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_BOSS,0x2eabae370e609bdd, Command_SpentBoss);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_MOVE_YACHT,0x3384e99fd932f997, Command_SpentOnMoveYacht);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_RENAME_ORGANIZATION,0x06cecbac26997c80, CommandSpentRenameOrganization);
    SCR_REGISTER_SECURE_EXPORT(NETWORK_BUY_CONTRABAND_MISSION,0x8c1520bab4821fc9, CommandBuyContrabandMission);
    SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_PA_SERVICE_HELI,0x9e2318e355455f6c, CommandSpentPaServiceHeli);
    SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_PA_SERVICE_VEHICLE,0x607f5974d8f99eb5, CommandSpentPaServiceVehicle);
    SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_PA_SERVICE_SNACK,0x17709f31fb6c0f5d, CommandSpentPaServiceSnack);
    SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_PA_SERVICE_DANCER,0x0e1fba451087dc4e, CommandSpentPaServiceDancer);
    SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_PA_SERVICE_IMPOUND,0xbd990ff45c770bba, CommandSpentPaServiceImpound);
    SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_PA_HELI_PICKUP,0x610933e83410be0d, CommandSpentPaHeliPickup);
    SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_PURCHASE_OFFICE_PROPERTY,0x84f008bcb6520e57, CommandSpentPurchaseOfficeProperty);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_UPGRADE_OFFICE_PROPERTY,0x8a964690daeea758, CommandSpentUpgradeOfficeProperty);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_PURCHASE_WAREHOUSE_PROPERTY,0xfc3abc5c02a58459, CommandSpentPurchaseWarehouseProperty);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_UPGRADE_WAREHOUSE_PROPERTY,0xfe9e66f0be2c4b9a, CommandSpentUpgradeWarehouseProperty);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_PURCHASE_IMPEXP_WAREHOUSE_PROPERTY,0x20408ddf054340fb, CommandSpentPurchaseWarehousePropertyImpExp);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_UPGRADE_IMPEXP_WAREHOUSE_PROPERTY,0x44e6a6d4cbe30595, CommandSpentUpgradeWarehousePropertyImpExp);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_TRADE_IMPEXP_WAREHOUSE_PROPERTY,0x7aadb09725ab1abd, CommandSpentTradeWarehousePropertyImpExp);
    SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_ORDER_WAREHOUSE_VEHICLE,0x22f47f1e15830884, CommandSpentOrderWarehouseVehicle);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_ORDER_BODYGUARD_VEHICLE,0xfd7e887e4e1fa6c4, CommandSpentOrderBodyguardVehicle);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_JUKEBOX,0x2c43ad784c2a855e, CommandSpentJukeBox);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_PURCHASE_CLUB_HOUSE,0xddc0cb5ac421b2d8, CommandSpentPurchaseClubHouse);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_UPGRADE_CLUB_HOUSE,0x1c63650ffab39a8c, CommandSpentUpgradeClubHouse);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_PURCHASE_BUSINESS_PROPERTY,0xdd57bffb2920da9b, CommandSpentPurchaseBusinessProperty);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_UPGRADE_BUSINESS_PROPERTY,0xf3851a067fd13a25, CommandSpentUpgradeBusinessProperty);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_TRADE_BUSINESS_PROPERTY,0xfa102669ec9b3d28, CommandSpentTradeBusinessProperty);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_MC_ABILITY,0x0880c1e6befb89f0, CommandSpentMcAbility);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_PAY_BUSINESS_SUPPLIES,0xc9f62b8ae66f3061, CommandPayBusinessSupplies);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_CHANGE_APPEARANCE,0xe623560b7f071fec, CommandChangeAppearance);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_VEHICLE_EXPORT_MODS,0x2728585e17843fd3, CommandSpentVehicleExportMods);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_PURCHASE_OFFICE_GARAGE,0xf3543dc789639b25, CommandSpentPurchaseOfficeGarage);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_UPGRADE_OFFICE_GARAGE,0xd596ce20824f2e5a, CommandSpentUpgradeOfficeGarage);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_IMPORT_EXPORT_REPAIR,0x8ac1d155da1160af, CommandSpentImportExportRepair);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_PURCHASE_HANGAR,0x259105f0dabae149, CommandSpendPurchaseHangarProperty);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_UPGRADE_HANGAR,0xc004d0f8042fb572, CommandSpendUpgradeHangarProperty);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_HANGAR_UTILITY_CHARGES,0x270369ac6681c174, CommandSpentHangarUtilityCharges);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_HANGAR_STAFF_CHARGES,0xcd5f7ca42d8bdcb7, CommandSpentHangarStaffCharges);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_BUY_TRUCK,0x398794d288ced547, CommandSpentBuyTruck);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_UPGRADE_TRUCK,0x6d54e5dd7d1a2439, CommandSpentUpgradeTruck);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_BUY_BUNKER,0xa2a4c678da20a7ec, CommandSpentBuyBunker);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_UPRADE_BUNKER,0x8834d1f62459679d, CommandSpentUpgradeBunker);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FROM_SELL_BUNKER,0xa3799a68f501052c, CommandEarnFromSellBunker);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_BALLISTIC_EQUIPMENT,0xdda3ee5081b3352b, CommandSpentBallisticEquipment);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_RDR_BONUS,0xc4d51fbbc4414e41, CommandEarnFromRdrBonus);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_WAGE_PAYMENT,0x771e4ab5c79c58e6, CommandEarnFromWagePayment);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_WAGE_PAYMENT_BONUS,0xadcd06a46e4698db, CommandEarnFromWagePaymentBonus);
	
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_BUY_BASE,0x8b8cd926260a4df1, CommandSpentOnBase);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_UPGRADE_BASE,0xa788db38560d8c97, CommandSpentUpgradeBase);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_BUY_TILTROTOR,0x6a03dd233331ef8e, CommandSpentOnTiltRotor);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_UPGRADE_TILTROTOR,0x8f06c7ea77d0810d, CommandSpentUpgradeTiltRotor);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_EMPLOY_ASSASSINS,0xab62af06f39ee7cb, CommandSpentEmployAssassins);

	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_GANGOPS_CANNON,0x366b0ff831b419a7, CommandSpentHeistCannon);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_GANGOPS_SKIP_MISSION,0x2b571637c15925d5, CommandSpentHeistMission);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_CASINO_HEIST_SKIP_MISSION,0xbcf5acbed9d0b528, CommandSpentCasinoHeistSkipMission);

	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_SELL_BASE,0x339b184c2c67e101, CommandEarnSellBase);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_TARGET_REFUND,0xf989a8eec7230fbe, CommandEarnTargetRefund);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_GANGOPS_WAGES,0xd664c6e4c4b6c820, CommandEarnHeistWages);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_GANGOPS_WAGES_BONUS,0x6f1481526b91195f, CommandEarnHeistWagesBonus);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_DAR_CHALLENGE,0xa58e89ef9ed914eb, CommandEarnDarChallenge);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_DOOMSDAY_FINALE_BONUS,0xa4e011a66f70ce6d, CommandEarnDoomsdayFinaleBonus);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_GANGOPS_AWARD,0x324ce186024e7207, CommandEarnGangopsAward);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_GANGOPS_ELITE,0x388312fc0babbf1b, CommandEarnGangopsElite);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SERVICE_EARN_GANGOPS_RIVAL_DELIVERY,0x84b486fa79aeb51b, CommandEarnMoneyFromGangOpsRivalDelivery);

	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_GANGOPS_START_STRAND,0x76f9f95febf3708a, CommandSpendGangOpsStartStrand);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_GANGOPS_TRIP_SKIP,0x44b09d298448791c, CommandSpendGangOpsTripSkip);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_GANGOPS_PREP_PARTICIPATION,0x73338a57ed708319, CommandEarnGangOpsPrepParticipation);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_GANGOPS_SETUP,0x9446d0d780403f07, CommandEarnGangOpsSetup);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_GANGOPS_FINALE,0x69586c400a6cb406, CommandEarnGangOpsFinale);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_GANGOPS_REPAIR_COST,0xd4abddb11c6dbe35, CommandSpendGangOpsRepairCost);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_NIGHTCLUB,0xa82297f11d86ffbf, CommandEarnNightClub);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_NIGHTCLUB_DANCING,0xdf55b0ff68a7b01c, CommandEarnNightclubDancing);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_BB_EVENT_BONUS,0x746cdc44f782dfa6, CommandEarnBBEventBonus);

	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_PURCHASE_HACKER_TRUCK,0x6e8ad780a8cc9e28, CommandSpendHackerTruck);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_UPGRADE_HACKER_TRUCK,0xb5788763064eedb8, CommandUpgradeHackerTruck);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_HACKER_TRUCK,0x96d0dd1362b188ba, CommandEarnHackerTruck);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_PURCHASE_NIGHTCLUB_AND_WAREHOUSE,0xbf64f84699419a71, CommandSpendNightclubAndWarehouse);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_UPGRADE_NIGHTCLUB_AND_WAREHOUSE,0x82178573c53353a9, CommandUpgradeNightclubAndWarehouse);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_NIGHTCLUB_AND_WAREHOUSE,0xc21030ea0af189b3, CommandEarnNightclubAndWarehouse);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_NIGHTCLUB_AND_WAREHOUSE,0x4d840f1546bc9e99, CommandSpendMiscNightclub);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_RDR_HATCHET_BONUS,0xcbe001b6c37192a7, CommandSpendRdrHatchetBonus);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_NIGHTCLUB_ENTRY_FEE,0x9e254735ca670b43, CommandSpendNightclubEntryFee);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_NIGHTCLUB_BAR_DRINK,0x2db77846c375feee, CommandSpendNightclubBarDrink);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_BOUNTY_HUNTER_MISSION,0x2b81856aaa0e53a3, CommandSpendBountyHunterMission);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_REHIRE_DJ,0xf00cc41ba4468456, CommandSpendRehireDJ);

	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPENT_ARENA_JOIN_SPECTATOR,0xcabd18851daabfcd, CommandSpendArenaJoinSpectator);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_ARENA_SKILL_LEVEL_PROGRESSION,0x6eedc85d467a4d10, CommandEarnArenaSkillLevelProgression);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_ARENA_CAREER_PROGRESSION,0xf24f7f9e554c6869, CommandEarnArenaCareerProgression);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_MAKE_IT_RAIN,0xea09a5cbac8bfc64, CommandSpendMakeItRain);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_BUY_ARENA,0x336e4fc3e6c2fd9e, CommandSpentBuyArena);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_UPGRADE_ARENA,0x3f44ae4073f23543, CommandSpentUpgradeArena);
	
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_ARENA_SPECTATOR_BOX,0x8f78e3686e1557e4, CommandSpendArenaSpectatorBox);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_SPIN_THE_WHEEL_PAYMENT,0xb6519c7f89415a54, CommandSpendSpinTheWheelPayment);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_SPIN_THE_WHEEL_CASH,0x146d9786343d5ba6, CommandEarnSpinTheWheelCash);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_ARENA_PREMIUM,0xf51d556d6ef05e21, CommandSpendArenaWarsPremiumEvent);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_ARENA_WAR,0x1b7d1bd13341944e, CommandEarnArenaWar);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_ARENA_WAR_ASSASSINATE_TARGET,0xc8e0bee49bb952a3, CommandEarnArenaWarAssassinateTarget);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_ARENA_WAR_EVENT_CARGO,0xc7c4f92697e08526, CommandEarnArenaWarEventCargo);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_RC_TIME_TRIAL,0xc1daf08902be8028, CommandEarnRcTimeTrial);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_DAILY_OBJECTIVE_EVENT,0x1a5c2e2723945652, CommandEarnDailyObjectiveEvent);

	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_CASINO_MEMBERSHIP,0xdbffa0366cd21d2e, CommandSpendCasinoMembership);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_BUY_CASINO,0xbe42452b68c44892, CommandSpentBuyCasino);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_UPGRADE_CASINO,0x91a47dc5db7cc3a8, CommandSpentUpgradeCasino);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_CASINO_GENERIC,0x7d5d2115877f3a15, CommandSpentCasinoGeneric);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_CASINO_TIME_TRIAL_WIN,0xc5aab4fe7d8e246b, CommandEarnCasinoTimeTrialWin);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_COLLECTABLES_ACTION_FIGURES,0x2762fc624626fac2, CommandEarnCollectableActionFigures);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_CASINO_COLLECTABLE_COMPLETED_COLLECTION,0xb9b36ae985f6b8da, CommandEarnCasinoCollectableCompletedCollection);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_SELL_PRIZE_VEHICLE,0x2efb058d52030f3e, CommandEarnSellPrizeVehicle);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_CASINO_MISSION_REWARD,0xb514ad4fd69533e6, CommandEarnCasinoMissionReward);
    SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_CASINO_STORY_MISSION_REWARD,0xc2ad1683fd4c7bab, CommandEarnCasinoStoryMissionReward);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_CASINO_MISSION_PARTICIPATION,0x73477c467022c107, CommandEarnCasinoMissionParticipation);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_CASINO_AWARD,0xbf70e41a3dc416cd, CommandEarnCasinoAward);

	/////////////////////////////////////////////////////////////////////////////// Heist3
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_BUY_ARCADE,0x7204b81741d2b88a, CommandBuyArcade);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_UPGRADE_ARCADE,0xe55d5c684838f0a0, CommandUpgradeArcade);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_CASINO_HEIST,0x05d40158d56d62a9, CommandSpendCasinoHeist);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_ARCADE_MGMT,0xcb744233c15f9b4a, CommandSpendArcadeMgmt);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_PLAY_ARCADE,0x12c4c93b734ea48c, CommandSpendPlayArcade);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_ARCADE,0xe84c6c337563acbb, CommandSpendArcade);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_CASINO_HEIST,0x7a70c2f43e7510f2, CommandEarnCasinoHeist);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_UPGRADE_ARCADE,0x02bec5b0c96d3ae5, CommandEarnUpgradeArcade);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_ARCADE,0xa2ce87301cddec45, CommandEarnArcade);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_COLLECTABLES,0x651f408d7a116cf0, CommandEarnCollectables);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_CHALLENGE,0x5cb7f7b38ac42890, CommandEarnChallenge);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_CASINO_HEIST_AWARDS,0x67e5d1c7e503dd93, CommandEarnCasinoHeistAwards);

	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_COLLECTABLE_ITEM,0x08ccfd36e49f6daf, CommandEarnCollectableItem);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_COLLECTABLE_COMPLETED_COLLECTION,0x4560db772529945a, CommandEarnCollectableCompletedCollection);

	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_YATCH_MISSION,0xa00a9583e60516ae, CommandEarnYatchMission);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_DISPATCH_CALL,0xfecae0c36c187951, CommandEarnDispatchCall);

	/////////////////////////////////////////////////////////////////////////////// Heist4

	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_BEACH_PARTY,0xc9b1a37c8769239d, CommandSpendBeachParty);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_SUBMARINE,0x1c42b2d94eaf2718, CommandSpendSubmarine);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_CASINO_CLUB,0xf99635f38bf6cee1, CommandSpendCasinoClub);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_BUY_SUB,0x9b3637401377120a, CommandBuyBuySub);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_UPGRADE_SUB,0xd0389d4d14078c71, CommandUpgradeBuySub);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_ISLAND_HEIST,0x70d5f434117cc632, CommandSpendIslandHeist);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_ISLAND_HEIST,0xd9f6ca1e468c0966, CommandEarnIslandHeist);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_BEACH_PARTY_LOST_FOUND,0xb63b018c5ecc0613, CommandEarnBeachPartyLostFound);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FROM_ISLAND_HEIST_DJ_MISSION,0x6cb5d96978629b3b, CommandEarnIslandHeistDjMission);

	////////////////////////////////////////////////////////////////////////// Tuner Update

	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_CAR_CLUB_MEMBERSHIP,0xb7c7367d9eb68d11, CommandSpendCarClubMembership);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_CAR_CLUB_BAR,0xb68d72a7c9493742, CommandSpendCarClubBar);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_AUTOSHOP_MODIFY,0xf3c5fae8faa5f251, CommandSpendAutoShopModify);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_CAR_CLUB_TAKEOVER,0x2a080e6f4637cb11, CommandSpendCarClubTakeover);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_BUY_AUTOSHOP,0xe28f46dd8a8b9a99, CommandBuyAutoshop);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_UPGRADE_AUTOSHOP,0xc2b4902dfdff16ef, CommandUpgradeAutoshop);

	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_AUTOSHOP_BUSINESS,0x0329bf95d7ce7a2e, CommandEarnAutoShopBusiness);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_AUTOSHOP_INCOME,0x5aa4c7a11447c2fd, CommandEarnAutoShopIncome);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_CARCLUB_MEMBERSHIP,0x7710b3985f0de8bf, CommandEarnCarClubMembership);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_DAILY_VEHICLE,0x5284c8c1f78142bf, CommandEarnDailyVehicle);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_DAILY_VEHICLE_BONUS,0xb1f8cd326dbb8cf2, CommandEarnDailyVehicleBonus);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_TUNER_AWARD,0x66fa1b42d92ba2fc, CommandEarnTunerAward);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_TUNER_ROBBERY,0xeddb967468ec8d1f, CommandEarnTunerRobbery);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_UPGRADE_AUTOSHOP,0xed303f2773aaf9b5, CommandEarnUpgradeAutoShop);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_INTERACTION_MENU_ABILITY,0xdaf1333d739aae8e, CommandSpendInteractionMenuAbility);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_SET_COMMON_FIELDS,0xaf0dd1fc2d0c54ae, CommandSpendSetCommonFields);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_SET_DISCOUNT, 0x7e2f4e8f44caf4e0, CommandSpendSetDiscount);

	////////////////////////////////////////////////////////////////////////// Fixer

	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_BUY_AGENCY,0xea8cd3c9b3c35884, CommandBuyAgency);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_UPGRADE_AGENCY,0x6cca64840589a3b6, CommandUpgradeAgency);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_AGENCY,0x1b2120405080125c, CommandSpendAgency);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_HIDDEN,0xbf8793b91ea094a7, CommandSpendHidden);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_SOURCE_BIKE,0xd9df467cbe4398c8, CommandSpendSourceBike);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_COMP_SUV,0xd86581f9e7cda383, CommandSpendCompSuv);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_SUV_FST_TRVL,0x61a2df64ed2d396e, CommandSpendSuvFstTrvl);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_SUPPLY,0xebd482b82acb8bad, CommandSpendSupply);

	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_BIKE_SHOP,0x923aea8e78f8df0b, CommandSpendBikeShop);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_VEHICLE_REQUESTED,0x02d24a35a9cc3503, CommandSpendVehicleRequested);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_GUNRUNNING,0x2ceb0e0bc2a77c05, CommandSpendGunRunning);

	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_AGENCY_SAFE,0x663b4b9d11742a12, CommandEarnAgencySafe);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_AWARD_CONTRACT,0x146d4eb6d22a403f, CommandEarnAwardContract);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_AGENCY_CONTRACT,0x38482ad49cb905c7, CommandEarnAgencyContract);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_AWARD_PHONE,0x7397a115030f1be3, CommandEarnAwardPhone);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_AGENCY_PHONE,0xe29f3d5fa63b1b82, CommandEarnAgencyPhone);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_AWARD_FIXER_MISSION,0x88d6c327d6c57c45, CommandEarnAwardFixerMission);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FIXER_PREP, 0x6283e5de4c4460c6, CommandEarnFixerPrep);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FIXER_FINALE, 0xba154373c5fe51e8, CommandEarnFixerFinale);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FIXER_AGENCY_SHORT_TRIP, 0xf4a8e57460bf2037, CommandEarnFixerAgencyShortTrip);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_AWARD_SHORT_TRIP,0x5b4dbded84d6a420, CommandEarnAwardShortTrip);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_FIXER_RIVAL_DELIVERY,0x235d41210b3a1a5e, CommandEarnFixerRivalDelivery);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_UPGRADE_AGENCY,0xd07c7c3f1995108c, CommandEarnUpgradeAgency);


	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_APARTMENT_UTILITIES, 0x1254b5b3925efd3d, CommandSpendApartmentUtilities);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_SPEND_BUSINESS_PROPERTY_FEES, 0x92d1cfda1227ff1c, CommandSpendBusinessUtilities);

	// Summer 2022
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_SIGHTSEEING_REWARD, 0x45087ae480b233ac, CommandEarnSightseeingReward);

	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_BIKER_SHOP, 0x2c5809eb9df57257, CommandEarnBikerShop);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_EARN_BIKER, 0x71bec32fa466e105, CommandEarnBiker);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_YOHAN_SOURCE_GOODS, 0x59498bc8b1c8b15c, CommandEarnYohanSourceGoods);

	//////////////////////////////////////////////////////////////////////////

	//Bank/VC functions
	SCR_REGISTER_SECURE_EXPORT(NETWORK_GET_VC_BANK_BALANCE,0xa257acf32a90b705, CommandGetVCBankBalance);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_GET_VC_WALLET_BALANCE,0xeee6dafbf451b942, CommandGetVCWalletBalance);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_GET_VC_BALANCE,0x6fb86f8f2f03ebd3, CommandGetVCBalance);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_GET_EVC_BALANCE,0x3543f80eac8d303c, CommandGetEVCBalance);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_GET_PVC_BALANCE,0x5189fe639fbe50a7, CommandGetPVCBalance);

	SCR_REGISTER_SECURE_EXPORT(NETWORK_GET_STRING_WALLET_BALANCE,0x034feb0a8464a229, CommandGetStringWalletBalance);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_GET_STRING_BANK_BALANCE,0x5f57025291607cd8, CommandGetStringBankBalance);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_GET_STRING_BANK_WALLET_BALANCE,0xc5d480c52c8e3653, CommandGetStringBankWalletBalance);

	SCR_REGISTER_SECURE_EXPORT(NETWORK_GET_CAN_SPEND_FROM_WALLET,0x0379a27eab9f02c8, CommandGetCanSpendFromWallet);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_GET_CAN_SPEND_FROM_BANK,0xb917918314ca8a39, CommandGetCanSpendFromBank);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_GET_CAN_SPEND_FROM_BANK_AND_WALLET,0x68c9515ee9a0586c, CommandGetCanSpendFromBanAndWallet);

	//USDE limits will be applied as follows:
	// - USDE Daily Purchase Limit = $1,000 (XBL purchase limit)
	// - USDE Daily Transfer Out Limit = $1,000***
	// - USDE Balance Limit = $150 (PSN wallet limit)
	SCR_REGISTER_SECURE_EXPORT(NETWORK_GET_PVC_TRANSFER_BALANCE,0x50c8d39907798d7c, CommandGetPVCTransferBalance);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_GET_CAN_TRANSFER_CASH,0x18ef751b1f4f5e68, CommandGetCanTransferCash);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_CAN_RECEIVE_PLAYER_CASH,0xf08d186298944b05, CommandGetCanReceiveCash);
	SCR_REGISTER_SECURE_EXPORT(NETWORK_GET_REMAINING_TRANSFER_BALANCE,0x648e0ae7087aa50c, CommandGetRemainingTransferBalance);

	//Transfer Cash
	SCR_REGISTER_SECURE(WITHDRAW_VC,0xfd3c38a9030ad7f2, CommandRequestVirtualCashWithdrawal);
	SCR_REGISTER_SECURE(DEPOSIT_VC,0x0d7ce3d8d074c379, CommandVirtualCashDeposit);
	SCR_REGISTER_SECURE(HAS_VC_WITHDRAWAL_COMPLETED,0x46e0dbddea18907a, CommandHasVirtualCashTransferCompleted);
	SCR_REGISTER_SECURE(WAS_VC_WITHDRAWAL_SUCCESSFUL,0xecb38c941d121d34, CommandHasVirtualCashTransferSucceeded);

	// GEN9
#if GEN9_STANDALONE_ENABLED
	SCR_REGISTER_UNUSED(NETWORK_GET_MP_WINDFALL_AVAILABLE, 0x313a1d074b56a004, CommandGetMPWindfallAvailable);
	SCR_REGISTER_UNUSED(NETWORK_SET_MP_WINDFALL_COMPLETED, 0x7fa877948e969d93, CommandSetMPWindfallCompleted);
	SCR_REGISTER_UNUSED(NETWORK_GET_MP_WINDFALL_BALANCE, 0x37d56db822282a59, CommandGetMPWindfallBalance);
	SCR_REGISTER_UNUSED(NETWORK_GET_MP_WINDFALL_AWARD_AMOUNT, 0xb79ccac28f715daf, CommandGetMPWindfallAwardAmount);
	SCR_REGISTER_UNUSED(NETWORK_GET_MP_WINDFALL_MINIMUM_SPEND, 0xa3883dee9292fc5c, CommandGetMPWindfallMinimumSpend);
	SCR_REGISTER_UNUSED(NETWORK_EARN_MP_WINDFALL, 0x83d0db414ad6885f, CommandMPWindfallEarn);
	SCR_REGISTER_UNUSED(NETWORK_SPEND_MP_WINDFALL, 0xc10c4bab23ce7da5, CommandMPWindfallSpend);
#endif
}

};

//eof 

