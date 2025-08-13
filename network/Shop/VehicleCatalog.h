//
// VehicleCatalog.h
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
// Stores the contents of the transaction server catalog for vehicle mods
//

#ifndef INC_NET_VEHCILE_CATALOG_H_
#define INC_NET_VEHCILE_CATALOG_H_

namespace packed_stats
{
	//Used for packed stats
	#define NUM_PACKED_INT_PER_STAT  8
	#define NUM_PACKED_BOOL_PER_STAT 64
	#define NUM_INT_PER_STAT         8
	#define NUM_BOOL_PER_STAT        1

	//Packed stats enum in script - needs to match stats_helper_packed_enums.sch
	enum eScriptPackedStatsEnum 
	{
		////////////////////////////////////////////////////////////////////////////////////////
		///     MULTI PLAYER PLAYER CHARACTER BOOLEAN PACKED STATS
		////////////////////////////////////////////////////////////////////////////////////////
		START_MP_CHAR_BOOL_PACKED = 0,
		END_MP_CHAR_BOOL_PACKED = START_MP_CHAR_BOOL_PACKED+192, //DO NOT CHANGE

		////////////////////////////////////////////////////////////////////////////////////////
		///     SINGLE PLAYER PLAYER CHARACTER BOOLEAN PACKED STATS
		////////////////////////////////////////////////////////////////////////////////////////
		START_SP_CHAR_BOOL_PACKED = END_MP_CHAR_BOOL_PACKED,
		END_SP_CHAR_BOOL_PACKED = START_SP_CHAR_BOOL_PACKED + 192, //DO NOT CHANGE

		////////////////////////////////////////////////////////////////////////////////////////
		///     MULTI PLAYER PLAYER CHARACTER INTEGER PACKED STATS
		////////////////////////////////////////////////////////////////////////////////////////
		START_MP_CHAR_INT_PACKED = END_SP_CHAR_BOOL_PACKED,
		END_MP_CHAR_INT_PACKED = START_MP_CHAR_INT_PACKED+73, //DO NOT CHANGE

		////////////////////////////////////////////////////////////////////////////////////////
		///     SINGLE PLAYER PLAYER CHARACTER INTEGER PACKED STATS
		////////////////////////////////////////////////////////////////////////////////////////
		START_SP_CHAR_INT_PACKED = END_MP_CHAR_INT_PACKED,
		END_SP_CHAR_INT_PACKED = START_SP_CHAR_INT_PACKED+56, //DO NOT CHANGE

		////////////////////////////////////////////////////////////////////////////////////////
		///     MULTIPLAYER PLAYER BOOLEAN PACKED STATS
		////////////////////////////////////////////////////////////////////////////////////////
		START_MP_BOOL_PACKED = END_SP_CHAR_INT_PACKED,
		END_MP_BOOL_PACKED = START_MP_BOOL_PACKED+192, //DO NOT CHANGE

		////////////////////////////////////////////////////////////////////////////////////////
		///     SINGLE PLAYER BOOLEAN PACKED STATS
		////////////////////////////////////////////////////////////////////////////////////////
		START_SP_BOOL_PACKED = END_MP_BOOL_PACKED,
		END_SP_BOOL_PACKED = START_SP_BOOL_PACKED+576, //DO NOT CHANGE

		////////////////////////////////////////////////////////////////////////////////////////
		///     MULTI PLAYER INTEGER PACKED STATS
		////////////////////////////////////////////////////////////////////////////////////////
		START_MP_INT_PACKED = END_SP_BOOL_PACKED,
		END_MP_INT_PACKED = START_MP_INT_PACKED+24, //DO NOT CHANGE -BenR

		////////////////////////////////////////////////////////////////////////////////////////
		///     SINGLE PLAYER INTEGER PACKED STATS
		////////////////////////////////////////////////////////////////////////////////////////
		START_SP_INT_PACKED = END_MP_INT_PACKED,
		END_SP_INT_PACKED   = START_SP_INT_PACKED+56, //DO NOT CHANGE -BenR

		// Multiplayer TITLE UPDATE INTEGER NON CHARACTER
		START_TU_MP_INT_PACKED = END_SP_INT_PACKED,
		END_TU_MP_INT_PACKED   = START_TU_MP_INT_PACKED+32,  //4 stats

		// Multiplayer TITLE UPDATE INTEGER CHARACTER
		START_TU_MP_CHAR_INT_PACKED = END_TU_MP_INT_PACKED,
		END_TU_MP_CHAR_INT_PACKED   = START_TU_MP_CHAR_INT_PACKED+1526, //127 stats

		// Multiplayer TITLE UPDATE BOOLEAN NON CHARACTER
		START_TU_MP_BOOL_PACKED = END_TU_MP_CHAR_INT_PACKED,
		END_TU_MP_BOOL_PACKED   = START_TU_MP_BOOL_PACKED+192, //3 stats

		// Multiplayer TITLE UPDATE BOOLEAN CHARACTER
		START_TU_MP_CHAR_BOOL_PACKED = END_TU_MP_BOOL_PACKED,
		END_TU_MP_CHAR_BOOL_PACKED   = START_TU_MP_CHAR_BOOL_PACKED+768,//12 stats

		////////////////////////////////////////////////////////////////////////////////////////
		///     NEXT GEN PACKED STATS
		////////////////////////////////////////////////////////////////////////////////////////

		START_NG_MP_CHAR_INT_PACKED = END_TU_MP_CHAR_BOOL_PACKED,
		END_NG_MP_CHAR_INT_PACKED   = START_NG_MP_CHAR_INT_PACKED+264, //33 stats

		START_NG_MP_INT_PACKED = END_NG_MP_CHAR_INT_PACKED,
		END_NG_MP_INT_PACKED = START_NG_MP_INT_PACKED + 64, //8 stat

		START_NG_MP_CHAR_BOOL_PACKED = END_NG_MP_INT_PACKED,
		END_NG_MP_CHAR_BOOL_PACKED   = START_NG_MP_CHAR_BOOL_PACKED+128, //2 stats

		START_NG_MP_BOOL_PACKED =  END_NG_MP_CHAR_BOOL_PACKED,
		END_NG_MP_BOOL_PACKED = START_NG_MP_BOOL_PACKED +64, //1 stats

		////////////////////////////////////////////////////////////////////////////////////////
		///     LOWRIDER STATS
		////////////////////////////////////////////////////////////////////////////////////////
		START_NG_MP_LOWRIDER_VEH_PACKED = END_NG_MP_BOOL_PACKED,
		END_NG_MP_LOWRIDER_VEH_PACKED   = START_NG_MP_LOWRIDER_VEH_PACKED+1628+1, //204 new stats (1629/8 = 203.625 stats but need to round up so 204 stats) 

		// New tattoos for Tattos LOWRIDER and BEYOND
		START_NG_TATTOOS_PURCHASED = END_NG_MP_LOWRIDER_VEH_PACKED+1,
		END_NG_TATTOOS_PURCHASED   = START_NG_TATTOOS_PURCHASED+384, // 64 * 6

		START_NG_MP_APARTMENT_VEH_PACKED = END_NG_TATTOOS_PURCHASED,
		END_NG_MP_APARTMENT_VEH_PACKED   = START_NG_MP_APARTMENT_VEH_PACKED+848+1, //107 new stats (849/8 = 106.125 stats but need to round up so 107 stats) 


		////////////////////////////////////////////////////////////////////////////////////////
		///     LOWRIDER 2 STATS
		////////////////////////////////////////////////////////////////////////////////////////

		START_NG_MP_LOWRIDER2_PACKED = END_NG_MP_APARTMENT_VEH_PACKED,
		// Vehicle Colour 5
		START_LOWRIDER2_VEHICLE_LIVERY2,
		END_LOWRIDER2_VEHICLE_LIVERY2   = START_LOWRIDER2_VEHICLE_LIVERY2+49,
		END_NG_MP_LOWRIDER2_PACKED = END_LOWRIDER2_VEHICLE_LIVERY2+1, //7 new stats (51/8 = 6.375 stats but need to round to 7

		//////////////////////////////////////////////////////////////////////////
		/// 
		//////////////////////////////////////////////////////////////////////////

		START_NG_DLC_MP_INT_PACKED = END_NG_MP_LOWRIDER2_PACKED,
		END_NG_DLC_MP_INT_PACKED = START_NG_DLC_MP_INT_PACKED + 8, //1 stat

		START_NG_DLC_MP_BOOL_PACKED =  END_NG_DLC_MP_INT_PACKED,
		END_NG_DLC_MP_BOOL_PACKED = START_NG_DLC_MP_BOOL_PACKED +64,    //1 stats

		START_NG_DLC_MP_CHAR_BOOL_PACKED = END_NG_DLC_MP_BOOL_PACKED,
		END_NG_DLC_MP_CHAR_BOOL_PACKED   = START_NG_DLC_MP_CHAR_BOOL_PACKED+256, //4 stats

		START_NG_DLC_MP_CHAR_INT_PACKED = END_NG_DLC_MP_CHAR_BOOL_PACKED,
		END_NG_DLC_MP_CHAR_INT_PACKED   = START_NG_DLC_MP_CHAR_INT_PACKED+40, //5 stats


		//////////////////////////////////////////////////////////////////////////
		/// BIKERS STATS
		//////////////////////////////////////////////////////////////////////////

		//Line 2983: 	START_NG_MP_BIKER_PACKED = END_NG_DLC_MP_CHAR_INT_PACKED,
		//Line 3640: 	END_NG_MP_BIKER_PACKED   = START_NG_MP_BIKER_PACKED+1680, //210 new stats (1675/8 = 209.375 but we round up so 210)
		START_NG_MP_BIKERS_CHAR_INT_PACKED = END_NG_DLC_MP_CHAR_INT_PACKED,
		END_NG_MP_BIKERS_CHAR_INT_PACKED = START_NG_MP_BIKERS_CHAR_INT_PACKED + 1680, //210 new stats

		//Line 3643: 	START_DLC_BIKER_MP_CHAR_BOOL_PACKED = END_NG_MP_BIKER_PACKED,
		//Line 3747: 	END_DLC_BIKER_MP_CHAR_BOOL_PACKED = START_DLC_BIKER_MP_CHAR_BOOL_PACKED + 192 //(3 stats)
		START_NG_MP_BIKERS_CHAR_BOOL_PACKED = END_NG_MP_BIKERS_CHAR_INT_PACKED,
		END_NG_MP_BIKERS_CHAR_BOOL_PACKED   = START_NG_MP_BIKERS_CHAR_BOOL_PACKED + 192, //2 stats

		//////////////////////////////////////////////////////////////////////////
		/// IMPORT EXPORT STATS
		//////////////////////////////////////////////////////////////////////////

		//Line 3752: 	START_NG_MP_IMPEXP_PACKED = END_DLC_BIKER_MP_CHAR_BOOL_PACKED,
		//Line 5794: 	END_NG_MP_IMPEXP_PACKED   = START_NG_MP_IMPEXP_PACKED+5711+1 //714 new stats (5711/8 = 713.875 but we round up so 714)
		START_NG_MP_IMPEXP_CHAR_INT_PACKED = END_NG_MP_BIKERS_CHAR_BOOL_PACKED,
		END_NG_MP_IMPEXP_CHAR_INT_PACKED = START_NG_MP_IMPEXP_CHAR_INT_PACKED + 5711 + 1, //714 new stats

		//////////////////////////////////////////////////////////////////////////
		/// GUNRUNNING STATS
		//////////////////////////////////////////////////////////////////////////

		START_NG_MP_GUNRUNNING_CHAR_INT_PACKED = END_NG_MP_IMPEXP_CHAR_INT_PACKED,
		END_NG_MP_GUNRUNNING_CHAR_INT_PACKED = START_NG_MP_GUNRUNNING_CHAR_INT_PACKED + 104, //13 new stats

		START_NG_MP_GUNRUNNING_CHAR_BOOL_PACKED = END_NG_MP_GUNRUNNING_CHAR_INT_PACKED,
		END_NG_MP_GUNRUNNING_CHAR_BOOL_PACKED = START_NG_MP_GUNRUNNING_CHAR_BOOL_PACKED + 192, //3 new stats

		START_NG_MP_GUNRUNNING_TATOOS_CHAR_BOOL_PACKED = END_NG_MP_GUNRUNNING_CHAR_BOOL_PACKED + 1,
		END_NG_MP_GUNRUNNING_TATOOS_CHAR_BOOL_PACKED = START_NG_MP_GUNRUNNING_TATOOS_CHAR_BOOL_PACKED + 384, //6 new stats

		//////////////////////////////////////////////////////////////////////////
		/// SMUGGLER STATS
		//////////////////////////////////////////////////////////////////////////

		START_NG_MP_SMUGGLER_CHAR_BOOL_PACKED = END_NG_MP_GUNRUNNING_TATOOS_CHAR_BOOL_PACKED,
		END_NG_MP_SMUGGLER_CHAR_BOOL_PACKED = START_NG_MP_SMUGGLER_CHAR_BOOL_PACKED + 64, //1 new stat

		START_NG_MP_SMUGGLER_CHAR_INT_PACKED = END_NG_MP_SMUGGLER_CHAR_BOOL_PACKED,
		END_NG_MP_SMUGGLER_CHAR_INT_PACKED = START_NG_MP_SMUGGLER_CHAR_INT_PACKED + 2088, //261 new stats

		//////////////////////////////////////////////////////////////////////////
		/// GANG OPS STATS
		//////////////////////////////////////////////////////////////////////////

		START_NG_MP_GANGOPS_CHAR_BOOL_PACKED = END_NG_MP_SMUGGLER_CHAR_INT_PACKED,
		END_NG_MP_GANGOPS_CHAR_BOOL_PACKED = START_NG_MP_GANGOPS_CHAR_BOOL_PACKED + 64, //1 new stat

		START_NG_MP_GANGOPS_CHAR_INT_PACKED = END_NG_MP_GANGOPS_CHAR_BOOL_PACKED,
		END_NG_MP_GANGOPS_CHAR_INT_PACKED = START_NG_MP_GANGOPS_CHAR_INT_PACKED + 856, //107 new stats

		//////////////////////////////////////////////////////////////////////////
		/// BUSINESS BATTLES STATS
		//////////////////////////////////////////////////////////////////////////

		START_NG_MP_BUSINESS_BATTLES_CHAR_INT_PACKED = END_NG_MP_GANGOPS_CHAR_INT_PACKED,
		END_NG_MP_BUSINESS_BATTLES_CHAR_INT_PACKED = START_NG_MP_BUSINESS_BATTLES_CHAR_INT_PACKED + 3048, //381 new stats

		START_NG_MP_BUSINESS_BATTLES_CHAR_BOOL_PACKED = END_NG_MP_BUSINESS_BATTLES_CHAR_INT_PACKED,
		END_NG_MP_BUSINESS_BATTLES_CHAR_BOOL_PACKED = START_NG_MP_BUSINESS_BATTLES_CHAR_BOOL_PACKED + 128, //2 stat

		//////////////////////////////////////////////////////////////////////////
		/// ARENA WARS
		//////////////////////////////////////////////////////////////////////////

		START_NG_MP_ARENA_WARS_CHAR_INT_PACKED = END_NG_MP_BUSINESS_BATTLES_CHAR_BOOL_PACKED,
		END_NG_MP_ARENA_WARS_CHAR_INT_PACKED = START_NG_MP_ARENA_WARS_CHAR_INT_PACKED + 2768, // 346 stat 

		START_NG_MP_ARENA_WARS_CHAR_BOOL_PACKED = END_NG_MP_ARENA_WARS_CHAR_INT_PACKED,
		END_NG_MP_ARENA_WARS_CHAR_BOOL_PACKED = START_NG_MP_ARENA_WARS_CHAR_BOOL_PACKED + 576, //9 stat

		//////////////////////////////////////////////////////////////////////////
		/// VINEWOOD / CASINO
		//////////////////////////////////////////////////////////////////////////

		START_NG_MP_VINEWOOD_CHAR_INT_PACKED = END_NG_MP_ARENA_WARS_CHAR_BOOL_PACKED,
		END_NG_MP_VINEWOOD_CHAR_INT_PACKED = START_NG_MP_VINEWOOD_CHAR_INT_PACKED + 1272, // 159 stat 

		START_NG_MP_VINEWOOD_CHAR_BOOL_PACKED = END_NG_MP_VINEWOOD_CHAR_INT_PACKED,
		END_NG_MP_VINEWOOD_CHAR_BOOL_PACKED = START_NG_MP_VINEWOOD_CHAR_BOOL_PACKED + 448, //7 stat

		//////////////////////////////////////////////////////////////////////////
		/// CASINO HEIST
		//////////////////////////////////////////////////////////////////////////

		START_NG_MP_CASINO_HEIST_CHAR_INT_PACKED = END_NG_MP_VINEWOOD_CHAR_BOOL_PACKED,
		END_NG_MP_CASINO_HEIST_CHAR_INT_PACKED = START_NG_MP_CASINO_HEIST_CHAR_INT_PACKED + 840, // 105 stat 

		START_NG_MP_CASINO_HEIST_CHAR_BOOL_PACKED = END_NG_MP_CASINO_HEIST_CHAR_INT_PACKED,
		END_NG_MP_CASINO_HEIST_CHAR_BOOL_PACKED = START_NG_MP_CASINO_HEIST_CHAR_BOOL_PACKED + 256, //4 stat

		START_NG_MP_HEIST3TATTOOSTAT_BOOL_PACKED = END_NG_MP_CASINO_HEIST_CHAR_BOOL_PACKED + 1, // We need a +1 here to match what script has.
		END_NG_MP_HEIST3TATTOOSTAT_BOOL_PACKED = START_NG_MP_HEIST3TATTOOSTAT_BOOL_PACKED + 128, //2 stat

		//////////////////////////////////////////////////////////////////////////
		/// SU20
		//////////////////////////////////////////////////////////////////////////

		START_NG_MP_SU20_CHAR_INT_PACKED = END_NG_MP_HEIST3TATTOOSTAT_BOOL_PACKED,
		END_NG_MP_SU20_CHAR_INT_PACKED = START_NG_MP_SU20_CHAR_INT_PACKED + 1744, // 218 stat 

		START_NG_MP_SU20_CHAR_BOOL_PACKED = END_NG_MP_SU20_CHAR_INT_PACKED,
		END_NG_MP_SU20_CHAR_BOOL_PACKED = START_NG_MP_SU20_CHAR_BOOL_PACKED + 128, //2 stat

		START_NG_MP_SU20_CHAR_TATTOOSTAT_BOOL_PACKED = END_NG_MP_SU20_CHAR_BOOL_PACKED,
		END_NG_MP_SU20_CHAR_TATTOOSTAT_BOOL_PACKED = START_NG_MP_SU20_CHAR_TATTOOSTAT_BOOL_PACKED + 128, //2 stat

		//////////////////////////////////////////////////////////////////////////
		/// HEIST ISLAND
		//////////////////////////////////////////////////////////////////////////

		START_NG_MP_HEIST_ISLAND_CHAR_INT_PACKED = END_NG_MP_SU20_CHAR_TATTOOSTAT_BOOL_PACKED,
		END_NG_MP_HEIST_ISLAND_CHAR_INT_PACKED = START_NG_MP_HEIST_ISLAND_CHAR_INT_PACKED + 32, // 4 stat 

		START_NG_MP_HEIST_ISLAND_CHAR_BOOL_PACKED = END_NG_MP_HEIST_ISLAND_CHAR_INT_PACKED,
		END_NG_MP_HEIST_ISLAND_CHAR_BOOL_PACKED = START_NG_MP_HEIST_ISLAND_CHAR_BOOL_PACKED + 192, //3 stat

		//////////////////////////////////////////////////////////////////////////
		/// THE TUNER
		//////////////////////////////////////////////////////////////////////////

		START_NG_MP_TUNER_CHAR_INT_PACKED = END_NG_MP_HEIST_ISLAND_CHAR_BOOL_PACKED,
		END_NG_MP_TUNER_CHAR_INT_PACKED = START_NG_MP_TUNER_CHAR_INT_PACKED + 1000, // 125 stat 

		START_NG_MP_TUNER_CHAR_BOOL_PACKED = END_NG_MP_TUNER_CHAR_INT_PACKED,
		END_NG_MP_TUNER_CHAR_BOOL_PACKED = START_NG_MP_TUNER_CHAR_BOOL_PACKED + 576, // 9 stat

		//////////////////////////////////////////////////////////////////////////
		/// FIXER
		//////////////////////////////////////////////////////////////////////////
		
		START_NG_MP_FIXER_CHAR_BOOL_PACKED = END_NG_MP_TUNER_CHAR_BOOL_PACKED,
		END_NG_MP_FIXER_CHAR_BOOL_PACKED = START_NG_MP_FIXER_CHAR_BOOL_PACKED + 128, // 2 stat
		
		START_NG_MP_FIXER_CHAR_TATTOOSTAT_BOOL_PACKED = END_NG_MP_FIXER_CHAR_BOOL_PACKED,
		END_NG_MP_FIXER_CHAR_TATTOOSTAT_BOOL_PACKED = START_NG_MP_FIXER_CHAR_TATTOOSTAT_BOOL_PACKED + 64, // 1 stat

		START_NG_MP_FIXER_CHAR_INT_PACKED = END_NG_MP_FIXER_CHAR_TATTOOSTAT_BOOL_PACKED,
		END_NG_MP_FIXER_CHAR_INT_PACKED = START_NG_MP_FIXER_CHAR_INT_PACKED + 1648, // 206 stat

		//////////////////////////////////////////////////////////////////////////
		/// GEN9
		//////////////////////////////////////////////////////////////////////////

		START_NG_MP_GEN9_CHAR_BOOL_PACKED = END_NG_MP_FIXER_CHAR_INT_PACKED,
		END_NG_MP_GEN9_CHAR_BOOL_PACKED = START_NG_MP_GEN9_CHAR_BOOL_PACKED + 128, // 2 stat

		//////////////////////////////////////////////////////////////////////////
		/// DLC12022
		//////////////////////////////////////////////////////////////////////////

		START_NG_MP_DLC12022_CHAR_BOOL_PACKED = END_NG_MP_GEN9_CHAR_BOOL_PACKED,
		END_NG_MP_DLC12022_CHAR_BOOL_PACKED = START_NG_MP_DLC12022_CHAR_BOOL_PACKED + 512, // 8 stat

		START_NG_MP_DLC12022_CHAR_INT_PACKED = END_NG_MP_DLC12022_CHAR_BOOL_PACKED,
		END_NG_MP_DLC12022_CHAR_INT_PACKED = START_NG_MP_DLC12022_CHAR_INT_PACKED + 1864, // 233 stat 

		//////////////////////////////////////////////////////////////////////////
		/// DLC22022
		//////////////////////////////////////////////////////////////////////////

		START_NG_MP_DLC22022_CHAR_BOOL_PACKED = END_NG_MP_DLC12022_CHAR_INT_PACKED,
		END_NG_MP_DLC22022_CHAR_BOOL_PACKED = START_NG_MP_DLC22022_CHAR_BOOL_PACKED + 320, // 5 stat

		START_NG_MP_DLC22022_CHAR_INT_PACKED = END_NG_MP_DLC22022_CHAR_BOOL_PACKED,
		END_NG_MP_DLC22022_CHAR_INT_PACKED = START_NG_MP_DLC22022_CHAR_INT_PACKED + 80, // 10 stat 

		//////////////////////////////////////////////////////////////////////////
		
		END_LAST_ENUM_VALUE = END_NG_MP_DLC22022_CHAR_INT_PACKED
	};

	/*
			When we add new Start/End enum stats we should add here 
			the CompileTimeAssert checks with the values script gives us. 
			This will reduce issues with being out of sync. See bug
			url:bugstar:6536259 for details.
	*/

	//Verify it matches script enum value - START_NG_MP_SU20_INT_PACKED = 28483
	CompileTimeAssert(eScriptPackedStatsEnum::START_NG_MP_SU20_CHAR_INT_PACKED == 28483);

	//Verify it matches script enum value - START_NG_MP_HEIST_ISLAND_CHAR_INT_PACKED = 30483
	CompileTimeAssert(eScriptPackedStatsEnum::START_NG_MP_HEIST_ISLAND_CHAR_INT_PACKED == 30483);

	//Verify it matches script enum value - START_NG_MP_TUNER_CHAR_INT_PACKED = 30707
	CompileTimeAssert(eScriptPackedStatsEnum::START_NG_MP_TUNER_CHAR_INT_PACKED == 30707);

	//Verify it matches script enum value - END_NG_MP_DLC12022_CHAR_INT_PACKED = 36627
	CompileTimeAssert(eScriptPackedStatsEnum::END_NG_MP_DLC12022_CHAR_INT_PACKED == 36627);

	//Verify it matches script enum value - END_NG_MP_DLC22022_CHAR_INT_PACKED = 36627
	CompileTimeAssert(eScriptPackedStatsEnum::END_NG_MP_DLC22022_CHAR_INT_PACKED == 37027);

	//Verify last matches matches the last script enum 
	// This value MUST be asked to scripters not calculated.
	#define END_LAST_ENUM_VALUE_ON_SCRIPT ( 37027 )

	CompileTimeAssert(eScriptPackedStatsEnum::END_LAST_ENUM_VALUE == END_LAST_ENUM_VALUE_ON_SCRIPT);

	//////////////////////////////////////////////////////////////////////////

	void GetStatEnumInfo(const int statEnum, int& startOffset, int& endOffset, bool& isBoolean, bool& isCharacter);

	int GetStatIndex(const int statEnum, const int startOffset, const bool isBoolean);

	bool  GetIsCharacterStat(const int statEnum);
	bool  GetIsPlayerStat(const int statEnum);
	bool  GetIsIntType(const int statEnum);
	bool  GetIsBooleanType(const int statEnum);

	bool  GetStatKey(StatId& statKey, const int statEnum, const int charSlot);
	bool  GetStatShiftValue( int& offSet, const int stat );

	static const int USE_CURRENT_CHAR_SLOT = -1;
	bool GetPackedValue(int& returnValue, const int statEnum, const int overrideCharSlot = USE_CURRENT_CHAR_SLOT);
	bool SetPackedValue(const int value, const int statEnum, const int overrideCharSlot = USE_CURRENT_CHAR_SLOT);
};

#endif // INC_NET_VEHCILE_CATALOG_H_

