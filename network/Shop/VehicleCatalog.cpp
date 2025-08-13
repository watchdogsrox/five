//
// VehicleCatalog.h
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
// Stores the contents of the transaction server catalog for vehicle mods
//

#include "Network/Shop/VehicleCatalog.h"
#include "network/NetworkTypes.h"
#include "Stats/StatsTypes.h"
#include "Stats/StatsInterface.h"
#include "Stats/StatsUtils.h"
#include "Stats/stats_channel.h"

SAVEGAME_OPTIMISATIONS();
NETWORK_OPTIMISATIONS();
NETWORK_SHOP_OPTIMISATIONS();

// Disable previous result caching, in case it's the cause for url:bugstar:7417790
#define PACKED_STATS_CACHE_PREVIOUS_RESULT 0

namespace packed_stats
{
	void GetStatEnumInfo(const int statEnum, int& startOffset, bool& isBoolean, bool& isCharacter)
	{
		startOffset = 0;
		isBoolean = false;
		isCharacter = false;

#if PACKED_STATS_CACHE_PREVIOUS_RESULT
		static int s_StatEnum = -1;
		static int s_startOffset = 0;
		static bool s_isBoolean = false;
		static bool s_isCharacter = false;

		if (s_StatEnum == statEnum)
		{
			startOffset = s_startOffset;
			isBoolean = s_isBoolean;
			isCharacter = s_isCharacter;
			return;
		}
#endif // PACKED_STATS_CACHE_PREVIOUS_RESULT

		if (statEnum >= START_SP_CHAR_INT_PACKED  && statEnum < END_SP_CHAR_INT_PACKED)
		{
			startOffset = START_SP_CHAR_INT_PACKED;
			isCharacter = true;
		}
		else if (statEnum >= START_SP_CHAR_BOOL_PACKED  && statEnum < END_SP_CHAR_BOOL_PACKED)
		{
			startOffset = START_SP_CHAR_BOOL_PACKED;
			isBoolean = true;
			isCharacter = true;
		}
		else if (statEnum >= START_SP_INT_PACKED  && statEnum < END_SP_INT_PACKED)
		{
			startOffset = START_SP_INT_PACKED;
		}
		else if (statEnum >= START_SP_BOOL_PACKED  && statEnum < END_SP_BOOL_PACKED)
		{
			startOffset = START_SP_BOOL_PACKED;
			isBoolean = true;
		}
		//////////////////////////////////////////////////////////////////////////
		else if (statEnum >= START_MP_CHAR_INT_PACKED  && statEnum < END_MP_CHAR_INT_PACKED)
		{
			startOffset = START_MP_CHAR_INT_PACKED;
			isCharacter = true;
		}
		else if (statEnum >= START_MP_INT_PACKED  && statEnum < END_MP_INT_PACKED)
		{
			startOffset = START_MP_INT_PACKED;
		}
		else if (statEnum >= START_TU_MP_INT_PACKED  && statEnum < END_TU_MP_INT_PACKED)
		{
			startOffset = START_TU_MP_INT_PACKED;
		}
		else if (statEnum >= START_TU_MP_CHAR_INT_PACKED  && statEnum < END_TU_MP_CHAR_INT_PACKED)
		{
			startOffset = START_TU_MP_CHAR_INT_PACKED;
			isCharacter = true;
		}
		else if (statEnum >= START_NG_MP_INT_PACKED  && statEnum < END_NG_MP_INT_PACKED)
		{
			startOffset = START_NG_MP_INT_PACKED;
		}
		else if (statEnum >= START_NG_MP_CHAR_INT_PACKED  && statEnum < END_NG_MP_CHAR_INT_PACKED)
		{
			startOffset = START_NG_MP_CHAR_INT_PACKED;
			isCharacter = true;
		}
		//////////////////////////////////////////////////////////////////////////
		else if (statEnum >= START_MP_CHAR_BOOL_PACKED  && statEnum < END_MP_CHAR_BOOL_PACKED)
		{
			startOffset = START_MP_CHAR_BOOL_PACKED;
			isBoolean = true;
			isCharacter = true;
		}
		else if (statEnum >= START_MP_BOOL_PACKED && statEnum < END_MP_BOOL_PACKED)
		{
			startOffset = START_MP_BOOL_PACKED;
			isBoolean = true;
		}
		else if (statEnum >= START_TU_MP_BOOL_PACKED  && statEnum < END_TU_MP_BOOL_PACKED)
		{
			startOffset = START_TU_MP_BOOL_PACKED;
			isBoolean = true;
		}
		else if (statEnum >= START_TU_MP_CHAR_BOOL_PACKED && statEnum < END_TU_MP_CHAR_BOOL_PACKED)
		{
			startOffset = START_TU_MP_CHAR_BOOL_PACKED;
			isBoolean = true;
			isCharacter = true;
		}
		else if (statEnum >= START_NG_MP_CHAR_BOOL_PACKED && statEnum < END_NG_MP_CHAR_BOOL_PACKED)
		{
			startOffset = START_NG_MP_CHAR_BOOL_PACKED;
			isBoolean = true;
			isCharacter = true;
		}
		else if (statEnum >= START_NG_MP_BOOL_PACKED && statEnum < END_NG_MP_BOOL_PACKED)
		{
			startOffset = START_NG_MP_BOOL_PACKED;
			isBoolean = true;
		}
		else if (statEnum >= START_NG_MP_LOWRIDER_VEH_PACKED  && statEnum < END_NG_MP_LOWRIDER_VEH_PACKED)
		{
			startOffset = START_NG_MP_LOWRIDER_VEH_PACKED;
			isCharacter = true;
		}
		else if (statEnum >= START_NG_TATTOOS_PURCHASED && statEnum < END_NG_TATTOOS_PURCHASED)
		{
			startOffset = START_NG_TATTOOS_PURCHASED;
			isBoolean = true;
			isCharacter = true;
		}
		else if (statEnum >= START_NG_MP_APARTMENT_VEH_PACKED  && statEnum < END_NG_MP_APARTMENT_VEH_PACKED)
		{
			startOffset = START_NG_MP_APARTMENT_VEH_PACKED;
			isCharacter = true;
		}
		else if (statEnum >= START_NG_MP_LOWRIDER2_PACKED  && statEnum < END_NG_MP_LOWRIDER2_PACKED)
		{
			startOffset = START_NG_MP_LOWRIDER2_PACKED;
			isCharacter = true;
		}
		else if (statEnum >= START_NG_DLC_MP_INT_PACKED  && statEnum < END_NG_DLC_MP_INT_PACKED)
		{
			startOffset = START_NG_DLC_MP_INT_PACKED;
		}
		else if (statEnum >= START_NG_DLC_MP_BOOL_PACKED  && statEnum < END_NG_DLC_MP_BOOL_PACKED)
		{
			startOffset = START_NG_DLC_MP_BOOL_PACKED;
			isBoolean = true;
		}
		else if (statEnum >= START_NG_DLC_MP_CHAR_BOOL_PACKED  && statEnum < END_NG_DLC_MP_CHAR_BOOL_PACKED)
		{
			startOffset = START_NG_DLC_MP_CHAR_BOOL_PACKED;
			isBoolean = true;
			isCharacter = true;
		}
		else if (statEnum >= START_NG_DLC_MP_CHAR_INT_PACKED  && statEnum < END_NG_DLC_MP_CHAR_INT_PACKED)
		{
			startOffset = START_NG_DLC_MP_CHAR_INT_PACKED;
			isCharacter = true;
		}
		else if (statEnum >= START_NG_MP_BIKERS_CHAR_INT_PACKED  && statEnum < END_NG_MP_BIKERS_CHAR_INT_PACKED)
		{
			startOffset = START_NG_MP_BIKERS_CHAR_INT_PACKED;
			isCharacter = true;
		}
		else if (statEnum >= START_NG_MP_BIKERS_CHAR_BOOL_PACKED  && statEnum < END_NG_MP_BIKERS_CHAR_BOOL_PACKED)
		{
			startOffset = START_NG_MP_BIKERS_CHAR_BOOL_PACKED;
			isBoolean = true;
			isCharacter = true;
		}
		else if (statEnum >= START_NG_MP_IMPEXP_CHAR_INT_PACKED  && statEnum < END_NG_MP_IMPEXP_CHAR_INT_PACKED)
		{
			startOffset = START_NG_MP_IMPEXP_CHAR_INT_PACKED;
			isCharacter = true;
		}
		else if (statEnum >= START_NG_MP_GUNRUNNING_CHAR_INT_PACKED  && statEnum < END_NG_MP_GUNRUNNING_CHAR_INT_PACKED)
		{
			startOffset = START_NG_MP_GUNRUNNING_CHAR_INT_PACKED;
			isCharacter = true;
		}
		else if (statEnum >= START_NG_MP_GUNRUNNING_CHAR_BOOL_PACKED  && statEnum < END_NG_MP_GUNRUNNING_CHAR_BOOL_PACKED)
		{
			startOffset = START_NG_MP_GUNRUNNING_CHAR_BOOL_PACKED;
			isBoolean = true;
			isCharacter = true;
		}
		else if (statEnum >= START_NG_MP_GUNRUNNING_TATOOS_CHAR_BOOL_PACKED  && statEnum < END_NG_MP_GUNRUNNING_TATOOS_CHAR_BOOL_PACKED)
		{
			startOffset = START_NG_MP_GUNRUNNING_TATOOS_CHAR_BOOL_PACKED;
			isBoolean = true;
			isCharacter = true;
		}
		else if (statEnum >= START_NG_MP_SMUGGLER_CHAR_INT_PACKED  && statEnum < END_NG_MP_SMUGGLER_CHAR_INT_PACKED)
		{
			startOffset = START_NG_MP_SMUGGLER_CHAR_INT_PACKED;
			isCharacter = true;
		}
		else if (statEnum >= START_NG_MP_SMUGGLER_CHAR_BOOL_PACKED  && statEnum < END_NG_MP_SMUGGLER_CHAR_BOOL_PACKED)
		{
			startOffset = START_NG_MP_SMUGGLER_CHAR_BOOL_PACKED;
			isBoolean = true;
			isCharacter = true;
		}
		else if (statEnum >= START_NG_MP_GANGOPS_CHAR_INT_PACKED  && statEnum < END_NG_MP_GANGOPS_CHAR_INT_PACKED)
		{
			startOffset = START_NG_MP_GANGOPS_CHAR_INT_PACKED;
			isCharacter = true;
		}
		else if (statEnum >= START_NG_MP_GANGOPS_CHAR_BOOL_PACKED  && statEnum < END_NG_MP_GANGOPS_CHAR_BOOL_PACKED)
		{
			startOffset = START_NG_MP_GANGOPS_CHAR_BOOL_PACKED;
			isBoolean = true;
			isCharacter = true;
		}
		else if (statEnum >= START_NG_MP_BUSINESS_BATTLES_CHAR_INT_PACKED && statEnum < END_NG_MP_BUSINESS_BATTLES_CHAR_INT_PACKED)
		{
			startOffset = START_NG_MP_BUSINESS_BATTLES_CHAR_INT_PACKED;
			isCharacter = true;
		}
		else if (statEnum >= START_NG_MP_BUSINESS_BATTLES_CHAR_BOOL_PACKED  && statEnum < END_NG_MP_BUSINESS_BATTLES_CHAR_BOOL_PACKED)
		{
			startOffset = START_NG_MP_BUSINESS_BATTLES_CHAR_BOOL_PACKED;
			isBoolean = true;
			isCharacter = true;
		}
		else if (statEnum >= START_NG_MP_ARENA_WARS_CHAR_INT_PACKED && statEnum < END_NG_MP_ARENA_WARS_CHAR_INT_PACKED)
		{
			startOffset = START_NG_MP_ARENA_WARS_CHAR_INT_PACKED;
			isCharacter = true;
		}
		else if (statEnum >= START_NG_MP_ARENA_WARS_CHAR_BOOL_PACKED  && statEnum < END_NG_MP_ARENA_WARS_CHAR_BOOL_PACKED)
		{
			startOffset = START_NG_MP_ARENA_WARS_CHAR_BOOL_PACKED;
			isBoolean = true;
			isCharacter = true;
		}
		else if (statEnum >= START_NG_MP_VINEWOOD_CHAR_INT_PACKED && statEnum < END_NG_MP_VINEWOOD_CHAR_INT_PACKED)
		{
			startOffset = START_NG_MP_VINEWOOD_CHAR_INT_PACKED;
			isCharacter = true;
		}
		else if (statEnum >= START_NG_MP_VINEWOOD_CHAR_BOOL_PACKED  && statEnum < END_NG_MP_VINEWOOD_CHAR_BOOL_PACKED)
		{
			startOffset = START_NG_MP_VINEWOOD_CHAR_BOOL_PACKED;
			isBoolean = true;
			isCharacter = true;
		}
		else if (statEnum >= START_NG_MP_CASINO_HEIST_CHAR_INT_PACKED && statEnum < END_NG_MP_CASINO_HEIST_CHAR_INT_PACKED)
		{
			startOffset = START_NG_MP_CASINO_HEIST_CHAR_INT_PACKED;
			isCharacter = true;
		}
		else if (statEnum >= START_NG_MP_CASINO_HEIST_CHAR_BOOL_PACKED && statEnum < END_NG_MP_CASINO_HEIST_CHAR_BOOL_PACKED)
		{
			startOffset = START_NG_MP_CASINO_HEIST_CHAR_BOOL_PACKED;
			isBoolean = true;
			isCharacter = true;
		}
		else if (statEnum >= START_NG_MP_HEIST3TATTOOSTAT_BOOL_PACKED && statEnum < END_NG_MP_HEIST3TATTOOSTAT_BOOL_PACKED)
		{
			startOffset = START_NG_MP_HEIST3TATTOOSTAT_BOOL_PACKED;
			isBoolean = true;
			isCharacter = true;
		}
		else if (statEnum >= START_NG_MP_SU20_CHAR_INT_PACKED && statEnum < END_NG_MP_SU20_CHAR_INT_PACKED)
		{
			startOffset = START_NG_MP_SU20_CHAR_INT_PACKED;
			isCharacter = true;
		}
		else if (statEnum >= START_NG_MP_SU20_CHAR_BOOL_PACKED && statEnum < END_NG_MP_SU20_CHAR_BOOL_PACKED)
		{
			startOffset = START_NG_MP_SU20_CHAR_BOOL_PACKED;
			isBoolean = true;
			isCharacter = true;
		}
		else if (statEnum >= START_NG_MP_SU20_CHAR_TATTOOSTAT_BOOL_PACKED && statEnum < END_NG_MP_SU20_CHAR_TATTOOSTAT_BOOL_PACKED)
		{
			startOffset = START_NG_MP_SU20_CHAR_TATTOOSTAT_BOOL_PACKED;
			isBoolean = true;
			isCharacter = true;
		}
		else if (statEnum >= START_NG_MP_HEIST_ISLAND_CHAR_INT_PACKED && statEnum < END_NG_MP_HEIST_ISLAND_CHAR_INT_PACKED)
		{
			startOffset = START_NG_MP_HEIST_ISLAND_CHAR_INT_PACKED;
			isCharacter = true;
		}
		else if (statEnum >= START_NG_MP_HEIST_ISLAND_CHAR_BOOL_PACKED && statEnum < END_NG_MP_HEIST_ISLAND_CHAR_BOOL_PACKED)
		{
			startOffset = START_NG_MP_HEIST_ISLAND_CHAR_BOOL_PACKED;
			isBoolean = true;
			isCharacter = true;
		}
		else if (statEnum >= START_NG_MP_TUNER_CHAR_INT_PACKED && statEnum < END_NG_MP_TUNER_CHAR_INT_PACKED)
		{
			startOffset = START_NG_MP_TUNER_CHAR_INT_PACKED;
			isCharacter = true;
		}
		else if (statEnum >= START_NG_MP_TUNER_CHAR_BOOL_PACKED && statEnum < END_NG_MP_TUNER_CHAR_BOOL_PACKED)
		{
			startOffset = START_NG_MP_TUNER_CHAR_BOOL_PACKED;
			isBoolean = true;
			isCharacter = true;
		}
		else if (statEnum >= START_NG_MP_FIXER_CHAR_INT_PACKED && statEnum < END_NG_MP_FIXER_CHAR_INT_PACKED)
		{
			startOffset = START_NG_MP_FIXER_CHAR_INT_PACKED;
			isCharacter = true;
		}
		else if (statEnum >= START_NG_MP_FIXER_CHAR_BOOL_PACKED && statEnum < END_NG_MP_FIXER_CHAR_BOOL_PACKED)
		{
			startOffset = START_NG_MP_FIXER_CHAR_BOOL_PACKED;
			isBoolean = true;
			isCharacter = true;
		}
		else if (statEnum >= START_NG_MP_FIXER_CHAR_TATTOOSTAT_BOOL_PACKED && statEnum < END_NG_MP_FIXER_CHAR_TATTOOSTAT_BOOL_PACKED)
		{
			startOffset = START_NG_MP_FIXER_CHAR_TATTOOSTAT_BOOL_PACKED;
			isBoolean = true;
			isCharacter = true;
		}
		else if (statEnum >= START_NG_MP_GEN9_CHAR_BOOL_PACKED && statEnum < END_NG_MP_GEN9_CHAR_BOOL_PACKED)
		{
			startOffset = START_NG_MP_GEN9_CHAR_BOOL_PACKED;
			isBoolean = true;
			isCharacter = true;
		}
		else if (statEnum >= START_NG_MP_DLC12022_CHAR_BOOL_PACKED && statEnum < END_NG_MP_DLC12022_CHAR_BOOL_PACKED)
		{
			startOffset = START_NG_MP_DLC12022_CHAR_BOOL_PACKED;
			isBoolean = true;
			isCharacter = true;
		}
		else if (statEnum >= START_NG_MP_DLC12022_CHAR_INT_PACKED && statEnum < END_NG_MP_DLC12022_CHAR_INT_PACKED)
		{
			startOffset = START_NG_MP_DLC12022_CHAR_INT_PACKED;
			isCharacter = true;
		}
		else if (statEnum >= START_NG_MP_DLC22022_CHAR_BOOL_PACKED && statEnum < END_NG_MP_DLC22022_CHAR_BOOL_PACKED)
		{
			startOffset = START_NG_MP_DLC22022_CHAR_BOOL_PACKED;
			isBoolean = true;
			isCharacter = true;
		}
		else if (statEnum >= START_NG_MP_DLC22022_CHAR_INT_PACKED && statEnum < END_NG_MP_DLC22022_CHAR_INT_PACKED)
		{
			startOffset = START_NG_MP_DLC22022_CHAR_INT_PACKED;
			isCharacter = true;
		}
#if __ASSERT
		else
		{
			//unused stat enum value due to some edits in script side.
			if(statEnum == END_NG_MP_LOWRIDER_VEH_PACKED)
			{
				statErrorf("SCRIPT CANNOT USE stat enum value='END_NG_MP_LOWRIDER_VEH_PACKED'");
				statAssertf(false, "SCRIPT CANNOT USE stat enum value='END_NG_MP_LOWRIDER_VEH_PACKED'");
				return;
			}
			//unused stat enum value due to some edits in script side.
			else if(statEnum == END_NG_MP_GUNRUNNING_CHAR_BOOL_PACKED)
			{
				statErrorf("SCRIPT CANNOT USE stat enum value='END_NG_MP_GUNRUNNING_CHAR_BOOL_PACKED'");
				statAssertf(false, "SCRIPT CANNOT USE stat enum value='END_NG_MP_GUNRUNNING_CHAR_BOOL_PACKED'");
				return;
			}
			//unused stat enum value due to some edits in script side.
			else if(statEnum == END_NG_MP_CASINO_HEIST_CHAR_BOOL_PACKED)
			{
				statErrorf("SCRIPT CANNOT USE stat enum value='END_NG_MP_CASINO_HEIST_CHAR_BOOL_PACKED'");
				statAssertf(false, "SCRIPT CANNOT USE stat enum value='END_NG_MP_CASINO_HEIST_CHAR_BOOL_PACKED'");
				return;
			}

			statErrorf("Can't Find the start or end of >> stat enum value='%d'", statEnum);
			statAssertf(false, "Can't Find the start or end of >> stat enum value='%d'", statEnum);
			return;
		}
#endif //__ASSERT

#if PACKED_STATS_CACHE_PREVIOUS_RESULT
		s_StatEnum = statEnum;
		s_startOffset = startOffset;
		s_isBoolean = isBoolean;
		s_isCharacter = isCharacter;
#endif // PACKED_STATS_CACHE_PREVIOUS_RESULT
	}

	bool  GetStatKey(StatId& statKey, const int statEnum, const int charSlot)
	{
		char key[MAX_STAT_LABEL_SIZE] = { 0 };

		int startOffset = 0;
		bool isBoolean = false;
		bool isCharacter = false;
		GetStatEnumInfo(statEnum, startOffset, isBoolean, isCharacter);

		const int statindex = GetStatIndex(statEnum, startOffset, isBoolean);

		statAssertf((int)MAX_NUM_MP_CHARS > charSlot || (int)MAX_NUM_SP_CHARS > charSlot, "Invalid char slot='%d'", charSlot);

		if ((statEnum >= START_SP_CHAR_INT_PACKED  && statEnum < END_SP_CHAR_INT_PACKED)
			|| (statEnum >= START_SP_INT_PACKED  && statEnum < END_SP_INT_PACKED))
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "SP%d%s%d", charSlot, "_PSTAT_INT", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "SP%s%d", "_PSTAT_INT", statindex);
			}
		}
		else if ((statEnum >= START_SP_CHAR_BOOL_PACKED  && statEnum < END_SP_CHAR_BOOL_PACKED)
			|| (statEnum >= START_SP_BOOL_PACKED  && statEnum < END_SP_BOOL_PACKED))
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "SP%d%s%d", charSlot, "_PSTAT_BOOL", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "SP%s%d", "_PSTAT_BOOL", statindex);
			}
		}
		//////////////////////////////////////////////////////////////////////////
		else if ((statEnum >= START_MP_CHAR_INT_PACKED  && statEnum < END_MP_CHAR_INT_PACKED)
			|| (statEnum >= START_MP_INT_PACKED  && statEnum < END_MP_INT_PACKED))
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_PSTAT_INT", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_PSTAT_INT", statindex);
			}
		}
		else if ((statEnum >= START_TU_MP_INT_PACKED  && statEnum < END_TU_MP_INT_PACKED)
			|| (statEnum >= START_TU_MP_CHAR_INT_PACKED  && statEnum < END_TU_MP_CHAR_INT_PACKED))
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_TUPSTAT_INT", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_TUPSTAT_INT", statindex);
			}
		}
		else if ((statEnum >= START_NG_MP_INT_PACKED && statEnum < END_NG_MP_INT_PACKED)
			|| (statEnum >= START_NG_MP_CHAR_INT_PACKED && statEnum < END_NG_MP_CHAR_INT_PACKED))
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_NGPSTAT_INT", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_NGPSTAT_INT", statindex);
			}
		}
		else if (statEnum >= START_NG_MP_LOWRIDER_VEH_PACKED && statEnum < END_NG_MP_LOWRIDER_VEH_PACKED)
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_LRPSTAT_INT", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_LRPSTAT_INT", statindex);
			}
		}
		else if (statEnum >= START_NG_MP_APARTMENT_VEH_PACKED  && statEnum < END_NG_MP_APARTMENT_VEH_PACKED)
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_APAPSTAT_INT", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_APAPSTAT_INT", statindex);
			}
		}
		else if (statEnum >= START_NG_MP_LOWRIDER2_PACKED && statEnum < END_NG_MP_LOWRIDER2_PACKED)
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_LR2PSTAT_INT", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_LR2PSTAT_INT", statindex);
			}
		}
		//////////////////////////////////////////////////////////////////////////
		else if ((statEnum >= START_MP_CHAR_BOOL_PACKED  && statEnum < END_MP_CHAR_BOOL_PACKED)
			|| (statEnum >= START_MP_BOOL_PACKED  && statEnum < END_MP_BOOL_PACKED))
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_PSTAT_BOOL", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_PSTAT_BOOL", statindex);
			}
		}
		else if ((statEnum >= START_TU_MP_BOOL_PACKED  && statEnum < END_TU_MP_BOOL_PACKED)
			|| (statEnum >= START_TU_MP_CHAR_BOOL_PACKED  && statEnum < END_TU_MP_CHAR_BOOL_PACKED))
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_TUPSTAT_BOOL", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_TUPSTAT_BOOL", statindex);
			}
		}
		else if ((statEnum >= START_NG_MP_BOOL_PACKED  && statEnum < END_NG_MP_BOOL_PACKED)
			|| (statEnum >= START_NG_MP_CHAR_BOOL_PACKED  && statEnum < END_NG_MP_CHAR_BOOL_PACKED))
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_NGPSTAT_BOOL", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_NGPSTAT_BOOL", statindex);
			}
		}
		else if (statEnum >= START_NG_TATTOOS_PURCHASED && statEnum < END_NG_TATTOOS_PURCHASED)
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_NGTATPSTAT_BOOL", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_NGTATPSTAT_BOOL", statindex);
			}
		}
		else if ((statEnum >= START_NG_DLC_MP_INT_PACKED  && statEnum < END_NG_DLC_MP_INT_PACKED)
			|| (statEnum >= START_NG_DLC_MP_CHAR_INT_PACKED  && statEnum < END_NG_DLC_MP_CHAR_INT_PACKED))
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_NGDLCPSTAT_INT", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_NGDLCPSTAT_INT", statindex);
			}
		}
		else if ((statEnum >= START_NG_DLC_MP_BOOL_PACKED  && statEnum < END_NG_DLC_MP_BOOL_PACKED)
			|| (statEnum >= START_NG_DLC_MP_CHAR_BOOL_PACKED  && statEnum < END_NG_DLC_MP_CHAR_BOOL_PACKED))
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_NGDLCPSTAT_BOOL", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_NGDLCPSTAT_BOOL", statindex);
			}
		}
		else if (statEnum >= START_NG_MP_BIKERS_CHAR_INT_PACKED  && statEnum < END_NG_MP_BIKERS_CHAR_INT_PACKED)
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_BIKEPSTAT_INT", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_BIKEPSTAT_INT", statindex);
			}
		}
		else if ((statEnum >= START_NG_MP_BIKERS_CHAR_BOOL_PACKED  && statEnum < END_NG_MP_BIKERS_CHAR_BOOL_PACKED))
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_DLCBIKEPSTAT_BOOL", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_DLCBIKEPSTAT_BOOL", statindex);
			}
		}
		else if (statEnum >= START_NG_MP_IMPEXP_CHAR_INT_PACKED  && statEnum < END_NG_MP_IMPEXP_CHAR_INT_PACKED)
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_IMPEXPPSTAT_INT", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_IMPEXPPSTAT_INT", statindex);
			}
		}
		else if (statEnum >= START_NG_MP_GUNRUNNING_CHAR_INT_PACKED  && statEnum < END_NG_MP_GUNRUNNING_CHAR_INT_PACKED)
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_GUNRPSTAT_INT", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_GUNRPSTAT_INT", statindex);
			}
		}
		else if (statEnum >= START_NG_MP_GUNRUNNING_CHAR_BOOL_PACKED  && statEnum < END_NG_MP_GUNRUNNING_CHAR_BOOL_PACKED)
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_DLCGUNPSTAT_BOOL", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_DLCGUNPSTAT_BOOL", statindex);
			}
		}
		else if (statEnum >= START_NG_MP_GUNRUNNING_TATOOS_CHAR_BOOL_PACKED  && statEnum < END_NG_MP_GUNRUNNING_TATOOS_CHAR_BOOL_PACKED)
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_GUNTATPSTAT_BOOL", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_GUNTATPSTAT_BOOL", statindex);
			}
		}
		else if (statEnum >= START_NG_MP_SMUGGLER_CHAR_INT_PACKED  && statEnum < END_NG_MP_SMUGGLER_CHAR_INT_PACKED)
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_DLCSMUGCHARPSTAT_INT", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_DLCSMUGPSTAT_INT", statindex);
			}
		}
		else if (statEnum >= START_NG_MP_SMUGGLER_CHAR_BOOL_PACKED  && statEnum < END_NG_MP_SMUGGLER_CHAR_BOOL_PACKED)
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_DLCSMUGCHARPSTAT_BOOL", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_DLCSMUGPSTAT_BOOL", statindex);
			}
		}
		else if (statEnum >= START_NG_MP_GANGOPS_CHAR_INT_PACKED  && statEnum < END_NG_MP_GANGOPS_CHAR_INT_PACKED)
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_GANGOPSPSTAT_INT", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_GANGOPSPSTAT_INT", statindex);
			}
		}
		else if (statEnum >= START_NG_MP_GANGOPS_CHAR_BOOL_PACKED  && statEnum < END_NG_MP_GANGOPS_CHAR_BOOL_PACKED)
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_GANGOPSPSTAT_BOOL", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_GANGOPSPSTAT_BOOL", statindex);
			}
		}
		else if (statEnum >= START_NG_MP_BUSINESS_BATTLES_CHAR_INT_PACKED  && statEnum < END_NG_MP_BUSINESS_BATTLES_CHAR_INT_PACKED)
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_BUSINESSBATPSTAT_INT", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_BUSINESSBATPSTAT_INT", statindex);
			}
		}
		else if (statEnum >= START_NG_MP_BUSINESS_BATTLES_CHAR_BOOL_PACKED  && statEnum < END_NG_MP_BUSINESS_BATTLES_CHAR_BOOL_PACKED)
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_BUSINESSBATPSTAT_BOOL", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_BUSINESSBATPSTAT_BOOL", statindex);
			}
		}
		else if (statEnum >= START_NG_MP_ARENA_WARS_CHAR_INT_PACKED  && statEnum < END_NG_MP_ARENA_WARS_CHAR_INT_PACKED)
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_ARENAWARSPSTAT_INT", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_ARENAWARSPSTAT_INT", statindex);
			}
		}
		else if (statEnum >= START_NG_MP_ARENA_WARS_CHAR_BOOL_PACKED  && statEnum < END_NG_MP_ARENA_WARS_CHAR_BOOL_PACKED)
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_ARENAWARSPSTAT_BOOL", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_ARENAWARSPSTAT_BOOL", statindex);
			}
		}
		else if (statEnum >= START_NG_MP_VINEWOOD_CHAR_INT_PACKED  && statEnum < END_NG_MP_VINEWOOD_CHAR_INT_PACKED)
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_CASINOPSTAT_INT", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_CASINOPSTAT_INT", statindex);
			}
		}
		else if (statEnum >= START_NG_MP_VINEWOOD_CHAR_BOOL_PACKED  && statEnum < END_NG_MP_VINEWOOD_CHAR_BOOL_PACKED)
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_CASINOPSTAT_BOOL", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_CASINOPSTAT_BOOL", statindex);
			}
		}
		else if (statEnum >= START_NG_MP_CASINO_HEIST_CHAR_INT_PACKED  && statEnum < END_NG_MP_CASINO_HEIST_CHAR_INT_PACKED)
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_CASINOHSTPSTAT_INT", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_CASINOHSTPSTAT_INT", statindex);
			}
		}
		else if (statEnum >= START_NG_MP_CASINO_HEIST_CHAR_BOOL_PACKED && statEnum < END_NG_MP_CASINO_HEIST_CHAR_BOOL_PACKED)
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_CASINOHSTPSTAT_BOOL", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_CASINOHSTPSTAT_BOOL", statindex);
			}
		}
		else if (statEnum >= START_NG_MP_HEIST3TATTOOSTAT_BOOL_PACKED && statEnum < END_NG_MP_HEIST3TATTOOSTAT_BOOL_PACKED)
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_HEIST3TATTOOSTAT_BOOL", statindex);
			}
		}
		else if (statEnum >= START_NG_MP_SU20_CHAR_INT_PACKED && statEnum < END_NG_MP_SU20_CHAR_INT_PACKED)
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_SU20PSTAT_INT", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_SU20PSTAT_INT", statindex);
			}
		}
		else if ((statEnum >= START_NG_MP_SU20_CHAR_BOOL_PACKED && statEnum < END_NG_MP_SU20_CHAR_BOOL_PACKED))
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_SU20PSTAT_BOOL", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_SU20PSTAT_BOOL", statindex);
			}
		}
		else if (statEnum >= START_NG_MP_SU20_CHAR_TATTOOSTAT_BOOL_PACKED && statEnum < END_NG_MP_SU20_CHAR_TATTOOSTAT_BOOL_PACKED)
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_SU20TATTOOSTAT_BOOL", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_SU20TATTOOSTAT_BOOL", statindex);
			}
		}
		else if (statEnum >= START_NG_MP_HEIST_ISLAND_CHAR_INT_PACKED && statEnum < END_NG_MP_HEIST_ISLAND_CHAR_INT_PACKED)
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_HISLANDPSTAT_INT", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_HISLANDPSTAT_INT", statindex);
			}
		}
		else if ((statEnum >= START_NG_MP_HEIST_ISLAND_CHAR_BOOL_PACKED && statEnum < END_NG_MP_HEIST_ISLAND_CHAR_BOOL_PACKED))
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_HISLANDPSTAT_BOOL", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_HISLANDPSTAT_BOOL", statindex);
			}
		}
		else if (statEnum >= START_NG_MP_TUNER_CHAR_INT_PACKED && statEnum < END_NG_MP_TUNER_CHAR_INT_PACKED)
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_TUNERPSTAT_INT", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_TUNERPSTAT_INT", statindex);
			}
		}
		else if ((statEnum >= START_NG_MP_TUNER_CHAR_BOOL_PACKED && statEnum < END_NG_MP_TUNER_CHAR_BOOL_PACKED))
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_TUNERPSTAT_BOOL", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_TUNERPSTAT_BOOL", statindex);
			}
		}
		else if ((statEnum >= START_NG_MP_FIXER_CHAR_INT_PACKED && statEnum < END_NG_MP_FIXER_CHAR_INT_PACKED))
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_FIXERPSTAT_INT", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_FIXERPSTAT_INT", statindex);
			}
		}
		else if ((statEnum >= START_NG_MP_FIXER_CHAR_BOOL_PACKED && statEnum < END_NG_MP_FIXER_CHAR_BOOL_PACKED))
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_FIXERPSTAT_BOOL", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_FIXERPSTAT_BOOL", statindex);
			}
		}
		else if ((statEnum >= START_NG_MP_FIXER_CHAR_TATTOOSTAT_BOOL_PACKED && statEnum < END_NG_MP_FIXER_CHAR_TATTOOSTAT_BOOL_PACKED))
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_FIXERTATTOOSTAT_BOOL", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_FIXERTATTOOSTAT_BOOL", statindex);
			}
		}
		else if ((statEnum >= START_NG_MP_GEN9_CHAR_BOOL_PACKED && statEnum < END_NG_MP_GEN9_CHAR_BOOL_PACKED))
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_GEN9PSTAT_BOOL", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_GEN9PSTAT_BOOL", statindex);
			}
		}
		else if ((statEnum >= START_NG_MP_DLC12022_CHAR_INT_PACKED && statEnum < END_NG_MP_DLC12022_CHAR_INT_PACKED))
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_DLC12022PSTAT_INT", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_DLC12022PSTAT_INT", statindex);
			}
		}
		else if ((statEnum >= START_NG_MP_DLC12022_CHAR_BOOL_PACKED && statEnum < END_NG_MP_DLC12022_CHAR_BOOL_PACKED))
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_DLC12022PSTAT_BOOL", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_DLC12022PSTAT_BOOL", statindex);
			}
		}
		else if ((statEnum >= START_NG_MP_DLC22022_CHAR_INT_PACKED && statEnum < END_NG_MP_DLC22022_CHAR_INT_PACKED))
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_DLC22022PSTAT_INT", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_DLC22022PSTAT_INT", statindex);
			}
		}
		else if ((statEnum >= START_NG_MP_DLC22022_CHAR_BOOL_PACKED && statEnum < END_NG_MP_DLC22022_CHAR_BOOL_PACKED))
		{
			if (isCharacter)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%d%s%d", charSlot, "_DLC22022PSTAT_BOOL", statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "MP%s%d", "_DLC22022PSTAT_BOOL", statindex);
			}
		}
		//////////////////////////////////////////////////////////////////////////
		else
		{
			statErrorf("Invalid stat enumvalue='%d', check if it is a boolean.", statEnum);
			statAssertf(0, "Invalid stat enumvalue='%d', check if it is a boolean.", statEnum);
		}

		statKey = StatId(key);

		const bool result = StatsInterface::IsKeyValid(statKey);
		if (!result)
		{
			statErrorf("GetStatKey - Invalid stat key: [%s] enumvalue='%d', isCharacter=%s, charSlot=%d, startOffset='%d', isBoolean='%s'", key, statEnum, isCharacter ? "True" : "False", charSlot, startOffset, isBoolean ? "true" : "false");
		}

		return result;
	};

	int GetStatIndex(const int statEnum, const int startOffset, const bool isBoolean)
	{
		const int numBits = isBoolean ? NUM_PACKED_BOOL_PER_STAT : NUM_PACKED_INT_PER_STAT;
		return (int)floor((float)((statEnum - startOffset)/numBits));
	};

	bool  GetIsCharacterStat(const int statEnum)
	{
		int startOffset = 0;
		bool isBoolean = false;
		bool isCharacter = false;
		GetStatEnumInfo(statEnum, startOffset, isBoolean, isCharacter);

		return isCharacter;
	};

	bool  GetIsIntType(const int statEnum)
	{
		int startOffset = 0;
		bool isBoolean = false;
		bool isCharacter = false;
		GetStatEnumInfo(statEnum, startOffset, isBoolean, isCharacter);

		return (!isBoolean);
	};

	bool  GetIsPlayerStat(const int statEnum)
	{
		return (!GetIsCharacterStat(statEnum));
	};

	bool  GetIsBooleanType(const int statEnum)
	{
		return (!GetIsIntType(statEnum));
	};

	bool  GetStatShiftValue( int& offSet, const int statEnum )
	{
		offSet = 0;

		int startOffset = 0;
		bool isBoolean = false;
		bool isCharacter = false;
		GetStatEnumInfo(statEnum, startOffset, isBoolean, isCharacter);

		if (isBoolean)
		{
			offSet = ((statEnum - startOffset) - GetStatIndex(statEnum, startOffset, isBoolean)*NUM_PACKED_BOOL_PER_STAT);
		}
		else
		{
			offSet = ((statEnum - startOffset) - GetStatIndex(statEnum, startOffset, isBoolean)*NUM_PACKED_INT_PER_STAT) * NUM_PACKED_INT_PER_STAT;
		}

		return true;
	};

	bool GetPackedValue(int& returnValue, const int statEnum, const int overrideCharSlot /*= USE_CURRENT_CHAR_SLOT*/)
	{
		if (!CStatsUtils::IsStatsTrackingEnabled())
		{
			statDebugf1("%s : Stats are disabled", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			return false;
		}

		const int charSlot = packed_stats::GetIsCharacterStat(statEnum) ? (overrideCharSlot == USE_CURRENT_CHAR_SLOT ? StatsInterface::GetCharacterSlot() : overrideCharSlot) : -1;
		statAssertf(charSlot == -1 || StatsInterface::ValidCharacterSlot(charSlot), "GetPackedValue() : Invalid character slot '%d' statEnum '%d'.", charSlot, statEnum);

		StatId statkey;
		int shift = 0;

		if (statVerifyf(packed_stats::GetStatKey(statkey, statEnum, charSlot), "GetPackedValue() : Failed to get a valid stat key from statEnum '%d'.", statEnum) 
			&& statVerifyf(packed_stats::GetStatShiftValue(shift, statEnum), "GetPackedValue() : Failed to get a valid shift from statEnum '%d'.", statEnum)
			&& statVerifyf(statkey.IsValid(), "GetPackedValue() : Couldn't create a valid stat key from statEnum '%d'.", statEnum))
		{
			returnValue = 0;
			return StatsInterface::GetMaskedInt(statkey.GetHash(), returnValue, shift, packed_stats::GetIsBooleanType(statEnum) ? NUM_BOOL_PER_STAT : NUM_INT_PER_STAT, -1);
		}

		return false;
	};

	bool SetPackedValue(const int value, const int statEnum, const int overrideCharSlot /*= USE_CURRENT_CHAR_SLOT*/)
	{
		if (!CStatsUtils::IsStatsTrackingEnabled())
		{
			statDebugf1("%s : Stats are disabled", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			return false;
		}

		const int charSlot = packed_stats::GetIsCharacterStat(statEnum) ? (overrideCharSlot == USE_CURRENT_CHAR_SLOT ? StatsInterface::GetCharacterSlot() : overrideCharSlot) : -1;
		statAssertf(charSlot == -1 || StatsInterface::ValidCharacterSlot(charSlot), "SetPackedValue() : Invalid character slot '%d' statEnum '%d'.", charSlot, statEnum);

		StatId statkey;
		int shift = 0;

		if (statVerifyf(packed_stats::GetStatKey(statkey, statEnum, charSlot), "SetPackedValue() : Failed to get a valid stat key from statEnum '%d'.", statEnum)
			&& statVerifyf(packed_stats::GetStatShiftValue(shift, statEnum), "SetPackedValue() : Failed to get a valid shift from statEnum '%d'.", statEnum))
		{
			const sStatDescription* pStatDesc = StatsInterface::GetStatDesc(statkey);
			if (pStatDesc->GetIsOnlineData())
			{
				statAssertf(!StatsInterface::CloudFileLoadPending(0), "SetPackedValue() - Trying to change Stat with key=\"%s\" but stats are not loaded.", statkey.GetName());
				statAssertf(!StatsInterface::CloudFileLoadFailed(0), "SetPackedValue() - Trying to change Stat with key=\"%s\" but stats loaded have failed.", statkey.GetName());
				if (StatsInterface::CloudFileLoadPending(0) || StatsInterface::CloudFileLoadFailed(0))
				{
					return false;
				}
			}

			if (statVerifyf(statkey.IsValid(), "SetPackedValue() : Couldn't create a valid stat key from statEnum '%d'.", statEnum))
			{
				return StatsInterface::SetMaskedInt(statkey.GetHash(), value, shift, packed_stats::GetIsBooleanType(statEnum) ? NUM_BOOL_PER_STAT : NUM_INT_PER_STAT);
			}
		}

		return false;
	};

} // namespace
