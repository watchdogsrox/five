//
// StatsDataMgr.cpp
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

// --- Include Files ------------------------------------------------------------

// C headers

// Rage headers
#include "parser/tree.h"
#include "parser/treenode.h"
#include "parser/manager.h"
#include "String/stringhash.h"
#include "Diag/output.h"
#include "System/param.h"
#include "data/callback.h"
#include "rline/profilestats/rlprofilestatscommon.h"
#include "script/thread.h"

#if __BANK
#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "data/callback.h"
#endif // __BANK

// Framework headers
#include "fwsys/timer.h"
#include "streaming/streamingengine.h"

// Stats headers
#include "StatsDataMgr.h"
#include "Stats/StatsMgr.h"
#include "Stats/StatsUtils.h"
#include "Stats/stats_channel.h"
#include "Stats/StatsInterface.h"

// Game headers
#include "core/game.h"
#include "Scene/DataFileMgr.h"
#include "Text/TextFile.h"
#include "script/script_channel.h"
#include "script/script.h"
#include "Peds/PlayerInfo.h"
#include "event/EventGroup.h"
#include "event/EventScript.h"
#include "frontend/ContextMenu.h"

// network headers
#include "Network/NetworkInterface.h"
#include "Network/Live/livemanager.h"
#include "Network/Network.h"
#include "Network/players/NetGamePlayer.h"
#include "Network/Stats/NetworkStockMarketStats.h"
#include "Network/Live/NetworkTelemetry.h"
#include "Network/Shop/Catalog.h"
#include "Network/Cloud/Tunables.h"
#include "Network/Cloud/UserContentManager.h"

#include "text/TextConversion.h"
#include "Core/gamesessionstatemachine.h"
#include "script/commands_stats.h"
#include "SaveLoad/GenericGameStorage.h"

#if RSG_DURANGO
#include "Network/Live/Events_durango.h"
#endif

#if __ASSERT
#include "script/thread.h"
#endif

#if __NET_SHOP_ACTIVE
#include "network/Shop/GameTransactionsSessionMgr.h"
#endif


#define __TEST_MASKED_STAT (0 && __DEV)

FRONTEND_STATS_OPTIMISATIONS()
SAVEGAME_OPTIMISATIONS()


#if __BANK
bool sStatData::ms_obfuscationDisabled = false;
#endif // __BANK

// --- Defines ------------------------------------------------------------------
RAGE_DEFINE_SUBCHANNEL(stat, profilestat, DIAG_SEVERITY_DEBUG3)
#define profileStatDebugf(fmt,...)     RAGE_DEBUGF2(stat_profilestat,fmt,##__VA_ARGS__)
#define profileStatWarningf(fmt,...)   RAGE_WARNINGF(stat_profilestat,fmt,##__VA_ARGS__)
#define profileStatErrorf(fmt,...)	   RAGE_ERRORF(stat_profilestat,fmt,##__VA_ARGS__)

#define NAMEDEF(id) {id, #id}

// The number of obfuscated stats we expect in the XML files - Used as a sanity check to make sure that
// level design don't add obfuscated stats, since they're very special purpose.
#if (RSG_ORBIS || RSG_PC || RSG_DURANGO)
static const u32 EXPECTED_NUM_OBFUSCATED_STATS = 137;
#else
static const u32 EXPECTED_NUM_OBFUSCATED_STATS = 13;
#endif // (RSG_ORBIS || RSG_PC || RSG_DURANGO)

static const unsigned CHECKDIRTYSTATS_INTERVAL = 2*60*1000;  //interval between checking again for dirty stats

//Set to 1 to block multiplayer territory takeover data to be sent to ROS servers.
#define  __BLOCK_TAKEOVERS  ( 0 )

#if __BLOCK_TAKEOVERS
//If this command is present territory takeovers are enabled
PARAM(enabletakeovers, "Enable vterritory takeovers during multiplayer");
#endif // __BLOCK_TAKEOVERS

//Maintain a Dirty stat count to avoid search done if none are dirty.
static u32  s_DirtyStatsCount = 0;
//Similarly, maintain a count for profile stats dirtied
//This is incremented in response to 
static u32  s_DirtyProfileStatsCount = 0;

//Similarly, maintain a count for savegame stats dirtied
//This is incremented in response to 
static u32  s_DirtySavegameMPStatsCount = 0;
static u32  s_DirtySavegameSPStatsCount = 0;
PARAM(spewDirtySavegameStats, "[stat_savemgr] Maintain a Count of dirty savegame stats between saves.");
PARAM(disableStatObfuscation, "Disable memory obfuscarion for stats");

// --- Constants/Static arrays --------------------------------------------------

#if __ASSERT
// For Validation after the stats have been loaded.
StatId_static*  StatId_static::sm_First = NULL;
StatId_char*    StatId_char::sm_First   = NULL;
#endif 

struct sNameDef
{
	u32 m_Id;
	const char* m_Name;
};

// Keep in Sync with eFrontendStatType
static const sNameDef sStatsTypesNames[] =
{
	NAMEDEF(STAT_TYPE_NONE),
	NAMEDEF(STAT_TYPE_INT),
	NAMEDEF(STAT_TYPE_FLOAT),
	NAMEDEF(STAT_TYPE_STRING),
	NAMEDEF(STAT_TYPE_BOOLEAN),
	NAMEDEF(STAT_TYPE_UINT8),
	NAMEDEF(STAT_TYPE_UINT16),
	NAMEDEF(STAT_TYPE_UINT32),
	NAMEDEF(STAT_TYPE_UINT64),
	NAMEDEF(STAT_TYPE_TIME),
	NAMEDEF(STAT_TYPE_CASH),
	NAMEDEF(STAT_TYPE_PERCENT),
	NAMEDEF(STAT_TYPE_DEGREES),
	NAMEDEF(STAT_TYPE_WEIGHT),
	NAMEDEF(STAT_TYPE_MILES),
	NAMEDEF(STAT_TYPE_METERS),
	NAMEDEF(STAT_TYPE_FEET),
	NAMEDEF(STAT_TYPE_SECONDS),
	NAMEDEF(STAT_TYPE_CHART),
	NAMEDEF(STAT_TYPE_VELOCITY),
	NAMEDEF(STAT_TYPE_DATE),
	NAMEDEF(STAT_TYPE_POS),
	NAMEDEF(STAT_TYPE_TEXTLABEL),
	NAMEDEF(STAT_TYPE_PACKED),
	NAMEDEF(STAT_TYPE_USERID),
	NAMEDEF(STAT_TYPE_PROFILE_SETTING),
	NAMEDEF(STAT_TYPE_INT64),
};
const int NUMBER_OF_STATS_TYPE_IDS = sizeof(sStatsTypesNames) / sizeof(sStatsTypesNames[0]);

// Keep in Sync with eFrontendStatShowFlag
static const sNameDef sStatsShowFlagsNames[] =
{
	NAMEDEF(STAT_FLAG_ALWAYS_SHOW),
	NAMEDEF(STAT_FLAG_INC_SHOW),
	NAMEDEF(STAT_FLAG_NEVER_SHOW),
};
const int NUMBER_OF_STATS_SHOW_FLAGS = sizeof(sStatsShowFlagsNames) / sizeof(sStatsShowFlagsNames[0]);

// --- Structure/Class Implementation -------------------------------------------

int GET_NUMBER_OF_BITS_FROM_NAME(const char* name)
{
	int numberOfBits = -1;

	if (name)
	{
		if( stricmp(name, "int") == 0 )
		{
			numberOfBits = 8;
		}
		else if( stricmp(name, "bool") == 0 )
		{
			numberOfBits = 1;
		}
	}

	return numberOfBits;
}

int GET_TYPE_FROM_NAME(const char* name)
{
	int tagType = STAT_TYPE_NONE;

	if (statVerify(name))
	{
		if( stricmp(name, "int") == 0 || stricmp(name, "short") == 0 )
		{
			tagType = STAT_TYPE_INT;
		}
		else if( stricmp(name, "float") == 0 || stricmp(name, "double") == 0 )
		{
			tagType = STAT_TYPE_FLOAT;
		}
		else if( stricmp(name, "string") == 0 )
		{
			tagType = STAT_TYPE_STRING;
		}
		else if( stricmp(name, "bool") == 0 )
		{
			tagType = STAT_TYPE_BOOLEAN;
		}
		else if( stricmp(name, "u8") == 0 )
		{
			tagType = STAT_TYPE_UINT8;
		}
		else if( stricmp(name, "u16") == 0 )
		{
			tagType = STAT_TYPE_UINT16;
		}
		else if( stricmp(name, "u32") == 0 )
		{
			tagType = STAT_TYPE_UINT32;
		}
		else if( stricmp(name, "u64") == 0 || stricmp(name, "x64") == 0 )
		{
			tagType = STAT_TYPE_UINT64;
		}
		else if ( stricmp(name, "date") == 0 )
		{
			tagType = STAT_TYPE_DATE;
		}
		else if ( stricmp(name, "pos") == 0 )
		{
			tagType = STAT_TYPE_POS;
		}
		else if( stricmp(name, "label") == 0 )
		{
			tagType = STAT_TYPE_TEXTLABEL;
		}
		else if( stricmp(name, "packed") == 0 )
		{
			tagType = STAT_TYPE_PACKED;
		}
		else if( stricmp(name, "userid") == 0 )
		{
			tagType = STAT_TYPE_USERID;
		}
		else if( stricmp(name, "profileSetting") == 0 )
		{
			tagType = STAT_TYPE_PROFILE_SETTING;
		}
		else if( stricmp(name, "s64") == 0 || stricmp(name, "long") == 0 )
		{
			tagType = STAT_TYPE_INT64;
		}
	}

	return tagType;
}


// --- sStatDescription ----------------------------------------------------------

void
sStatDescription::SetDirty(const bool value, const bool bDirtyProfile)
{
	if (value && !m_Desc.as_SinglePlayer.m_Dirty)
	{
		s_DirtyStatsCount++;
	}
	else if (!value && m_Desc.as_SinglePlayer.m_Dirty)
	{
		statAssert(0 != s_DirtyStatsCount);
		s_DirtyStatsCount--;
	}

	m_Desc.as_SinglePlayer.m_Dirty = value;

	if (bDirtyProfile)
	{
		SetDirtyProfile(value);
	}
}

void
sStatDescription::SetDirtyProfile(const bool value)
{
	if (value && StatsInterface::GetBlockSaveRequests())
		return;

	if (value && !m_Desc.as_SinglePlayer.m_DirtyProfile)
	{
		s_DirtyProfileStatsCount++;
	}
	else if (!value && m_Desc.as_SinglePlayer.m_DirtyProfile)
	{
		statAssert(0 != s_DirtyProfileStatsCount);
		s_DirtyProfileStatsCount--;
	}

	m_Desc.as_SinglePlayer.m_DirtyProfile = value;
}

void
sStatDescription::SetDirtySavegame(const bool value)
{
	if (PARAM_spewDirtySavegameStats.Get())
	{
		if (GetServerAuthoritative())
			return;

		if (GetIsCommunityStat())
			return;

		if (GetIsProfileSetting())
			return;

		if (value && !m_Desc.as_SinglePlayer.m_DirtySavegame)
		{
			if (m_Desc.as_SinglePlayer.m_OnlineStat)
			{
				s_DirtySavegameMPStatsCount++;
			}
			else
			{
				s_DirtySavegameSPStatsCount++;
			}
		}
		else if (!value && m_Desc.as_SinglePlayer.m_DirtySavegame)
		{
			if (m_Desc.as_SinglePlayer.m_OnlineStat)
			{
				statAssert(0 != s_DirtySavegameMPStatsCount);
				s_DirtySavegameMPStatsCount--;
			}
			else
			{
				statAssert(0 != s_DirtySavegameSPStatsCount);
				s_DirtySavegameSPStatsCount--;
			}
		}

		m_Desc.as_SinglePlayer.m_DirtySavegame = value;
	}
}

bool 
sStatDescription::SetIsPlayerInventory( )
{
	if (!statVerify(GetIsOnlineData()))
		return false;

	if (!statVerify(GetServerAuthoritative()))
		return false;

	m_Desc.as_MultiPlayer.m_PlayerInventory = true;

	return true;
}



// --- StatId_char ----------------------------------------------------------

#if __ASSERT
StatId_char::StatId_char(const char* name, const atHashString& sp0, const atHashString& sp1, const atHashString& sp2, const atHashString& mp0, const atHashString& mp1, const bool validate)
{
	m_Validate = validate;
	Set(name);
	m_Id[CHAR_MICHEAL] = sp0;
	m_Id[CHAR_FRANKLIN] = sp1;
	m_Id[CHAR_TREVOR] = sp2;
	m_Id[CHAR_MP0] = mp0;
	m_Id[CHAR_MP1] = mp1;
}
#endif // __ASSERT

StatId_char::StatId_char(const atHashString& sp0, const atHashString& sp1, const atHashString& sp2, const atHashString& mp0, const atHashString& mp1)
{
	ASSERT_ONLY(m_Validate = false;)
	m_Id[CHAR_MICHEAL] = sp0;
	m_Id[CHAR_FRANKLIN] = sp1;
	m_Id[CHAR_TREVOR] = sp2;
	m_Id[CHAR_MP0] = mp0;
	m_Id[CHAR_MP1] = mp1;
}

#if __ASSERT
StatId_char::~StatId_char()
{
	if (m_Validate)
	{
		statAssertf(0, "DONT - STATIC STAT ='%s'", m_Name);
	}
}
#endif // __ASSERT

void  StatId_char::Set(const char* str)
{
#if __ASSERT
	if (m_Validate)
	{
		m_Next   = sm_First;
		sm_First = this;
		m_Name   = str;
		statAssert(m_Name);
	}
#endif

	for (int prefix=0; prefix<MAX_STATS_CHAR_INDEXES; prefix++)
	{
		char statString[MAX_STAT_LABEL_SIZE];
		statString[0] = '\0';

		if (str)
		{
			safecatf(statString,MAX_STAT_LABEL_SIZE,"%s_%s",s_StatsModelPrefixes[prefix],str);
		}

		m_Id[prefix] = str ? HASH_STAT_ID(statString) : 0;
	}
}

u32 StatId_char::GetHash(const u32 prefix) const
{
	if(prefix < MAX_STATS_CHAR_INDEXES)
	{
		return m_Id[prefix];
	}

	return 0;
}

u32  StatId_char::GetHash() const
{
	u32 prefix = StatsInterface::GetStatsModelPrefix();
	if(prefix < MAX_STATS_CHAR_INDEXES)
	{
		return m_Id[prefix];
	}

	return 0;
}

StatId  StatId_char::GetStatId() const
{
	u32 prefix = StatsInterface::GetStatsModelPrefix();
	if(prefix < MAX_STATS_CHAR_INDEXES)
	{
		return StatId(m_Id[prefix]);
	}

	return StatId();
}

StatId  StatId_char::GetOnlineStatId(const u32 slot) const
{
	//const int slot = StatsInterface::GetIntStat(STAT_MPPLY_LAST_MP_CHAR) + CHAR_MP0;
	if (statVerify(slot>=CHAR_MP0) && statVerify(slot<=CHAR_MP_END))
	{
		return StatId(m_Id[slot]);
	}

	return StatId();
}

// --- sStatData ----------------------------------------------------------

sStatData::sStatData(const char* UNUSED_PARAM(pName), sStatDescription& desc)
{
	m_Desc = desc;

	ASSERT_ONLY( m_CoderStat = true; )

	NOTFINAL_ONLY( m_ShowChanges = false; )
	NOTFINAL_ONLY( m_ShowOnScreen = false; )
}

void 
sStatData::Reset()
{
	m_Desc.SetSynched(true);
	m_Desc.SetForcedSynched(false);
	m_Desc.SetDirtyInThisSession(false);
}

bool 
sStatData::SetData(void* pData, const unsigned sizeofData, bool& retHaschangedValue)
{
	retHaschangedValue = false;

	bool ret = false;

	if (statVerify(pData))
	{
		if (GetIsBaseType(STAT_TYPE_PROFILE_SETTING))
		{
			retHaschangedValue = true;
			return true;
		}

		if (sizeofData == GetSizeOfData() || ((GetIsBaseType(STAT_TYPE_STRING) || GetIsBaseType(STAT_TYPE_USERID)) && sizeofData <= GetSizeOfData()))
		{
			ret = true;

			//Value is not different.
			if (ValueIsEqual(pData))
				return ret;

			retHaschangedValue = true;

			switch (GetType())
			{
			case STAT_TYPE_TEXTLABEL: SetInt(*((int*)pData)); break;
			case STAT_TYPE_INT:       SetInt(*((int*)pData)); break;
			case STAT_TYPE_INT64:     SetInt64(*((s64*)pData)); break;
			case STAT_TYPE_FLOAT:     SetFloat(*((float*)pData)); break;
			case STAT_TYPE_BOOLEAN:   SetBoolean(*((bool*)pData)); break;
			case STAT_TYPE_UINT8:     SetUInt8(*((u8*)pData)); break;
			case STAT_TYPE_UINT16:    SetUInt16(*((u16*)pData)); break;
			case STAT_TYPE_UINT32:    SetUInt32(*((u32*)pData)); break;
			case STAT_TYPE_UINT64:    SetUInt64(*((u64*)pData)); break;
			case STAT_TYPE_PACKED:    SetUInt64(*((u64*)pData)); break;
			case STAT_TYPE_DATE:      SetUInt64(*((u64*)pData)); break;
			case STAT_TYPE_POS:       SetUInt64(*((u64*)pData)); break;
			case STAT_TYPE_STRING:    SetString((char*)pData); break;
			case STAT_TYPE_USERID:
				{
					if (sizeof(u64) == sizeofData)
					{
						SetUInt64(*((u64*)pData));
					}
					else
					{
						SetUserId((char*)pData);
					}
				}
				break;

			default:
				retHaschangedValue = false;
				ret = false;
				statAssertf(0, "Invalid stat tag type \"%d\"", GetType()); 
				break;
			}

			if (retHaschangedValue)
			{
				DEV_ONLY(if (m_ShowChanges) ToString();)
			}
		}
	}

	return ret;
}

void
sLabelStatData::SetInt(const int value) 
{
	sIntStatData::SetInt(value);

#if __BANK
	m_Label[0] = '\0';
	if (TheText.DoesTextLabelExist(GetInt()))
	{
		safecpy( m_Label, TheText.Get(GetInt(), "LABEL_STAT") );
	}
#endif // __BANK
}

void 
sUns64StatData::SetUInt64(const u64 value) 
{
	Encode(m_Value,value);

#if __BANK
	m_ValueLo = (u32)value;
	m_ValueHi = 0;

	if (value > MAX_UINT32)
	{
		m_ValueLo = MAX_UINT32;
		m_ValueHi = (u32)(value - MAX_UINT32);
	}
#endif // __BANK
}

void 
sPosStatData::SetUInt64(const u64 value) 
{
	Encode(m_Value,value);
	BANK_ONLY( StatsInterface::UnPackPos(Decode(m_Value), m_Position.x, m_Position.y, m_Position.z); )
}

void 
sDateStatData::SetUInt64(const u64 value) 
{
	Encode(m_Value,value);
	BANK_ONLY( StatsInterface::UnPackDate(Decode(m_Value), m_Date.m_year, m_Date.m_month, m_Date.m_day, m_Date.m_hour, m_Date.m_minutes, m_Date.m_seconds, m_Date.m_milliseconds); )
}

void 
sPackedStatData::SetUInt64(const u64 value) 
{
	Encode(m_Value,value);
	BANK_ONLY( SetupWidget(); )
}

#if __BANK

void 
sPackedStatData::SetupWidget() 
{
	if (NumberOfBitsIsValid())
	{
		u64 mask = (1 << m_NumberOfBits) - 1;

		char temp[WIDGET_STRING_SIZE];
		temp[0] = '\0';
		m_WidgetValue[0] = '\0';

		for (int i=0; i<64; i+=m_NumberOfBits)
		{
			u64 maskShifted = (mask << i);
			int data = (int)((Decode(m_Value) & maskShifted) >> i);

			temp[0] = '\0';
			formatf(temp, "%s", m_WidgetValue);

			if(i==0)
				formatf(m_WidgetValue, "%d", data);
			else
				formatf(m_WidgetValue, "%s:%d", temp, data);
		}
	}
}
#endif // __BANK

bool 
sUserIdStatData::IsDefaultValue() const 
{
	return (m_UserId == 0);
}

void 
sUserIdStatData::Reset() 
{
	sStatData::Reset();

	m_UserId = 0;

#if RLGAMERHANDLE_NP
	if (m_NextUserId)
	{
		m_NextUserId->Reset();
	}
#endif
}

bool 
sUserIdStatData::ValueIsEqual(void* pData) const
{
	if (pData)
	{
#if RLGAMERHANDLE_NP
		if (m_NextUserId)
		{
			char buffer[128];
			if (GetUserId(&buffer[0], 128))
			{
				return (stricmp(buffer, (char*)pData) == 0);
			}
		}
		else
		{
			//always update the value.
			return false;
		}
#elif RLGAMERHANDLE_SC || RLGAMERHANDLE_XBL
		char buffer[128];
		if (GetUserId(&buffer[0], 128))
		{
			return (stricmp(buffer, (char*)pData) == 0);
		}
#endif
	}

	return false;
}

unsigned
sUserIdStatData::GetSizeOfData() const 
{
#if RLGAMERHANDLE_XBL
	return 20;
#elif RLGAMERHANDLE_SC
	return 20;
#elif RLGAMERHANDLE_NP
	if (rlGamerHandle::IsUsingAccountIdAsKey())
	{
		return 20;
	}
	else
	{
		return 17; // 16+1 for the NULL terminating character
	}
#else
	return 0;
#endif
}

bool
sUserIdStatData::GetUserId(char* userId, const int bufSize) const
{
	statAssertf(userId, "Null handle");
	if (!userId)
		return false;

	Assert(userId);
	Assert(bufSize >= GetSizeOfData());

	userId[0] = '\0';

	char tmpBuf[128] = {""};

#if RLGAMERHANDLE_XBL || RLGAMERHANDLE_SC

    if(m_UserId != 0)
    {
	    formatf(tmpBuf, sizeof(tmpBuf), "%" I64FMT "u", m_UserId);
    }
#elif RLGAMERHANDLE_NP

	if (rlGamerHandle::IsUsingAccountIdAsKey())
	{
		if (m_UserId == 0 && m_NextUserId && m_NextUserId->m_UserId != 0)
		{
			formatf(tmpBuf, sizeof(tmpBuf), "%" I64FMT "u", m_NextUserId->m_UserId);
		}
	}
	else if(m_UserId != 0)
	{
		u32 shiftcounter = 0;
		for (u32 i = 0; ((m_NextUserId && i < 16) || (!m_NextUserId && i < 8)); i++, shiftcounter++)
		{
			if (i == 8)
			{
				shiftcounter = 0;
			}
			u32 shift = shiftcounter * 8;

			u64 mask = (1 << 8) - 1;
			u64 maskShifted = (mask << shift);

			int data = 0;
			if (i < 8)
			{
				data = (int)((m_UserId & maskShifted) >> shift);
			}
			else if (statVerify(m_NextUserId))
			{
				data = (int)((m_NextUserId->m_UserId & maskShifted) >> shift);
			}
			statAssert(data <= 255);

			tmpBuf[i] = static_cast<char>(data);
		}
	}
	

#elif __WIN32PC || RSG_DURANGO
	&bufSize;
#endif

	if(rlVerifyf(strlen(tmpBuf) <= (size_t) bufSize, "Buffer too small: Required:%u, Size:%d", (unsigned)strlen(tmpBuf), bufSize))
	{
		safecpy(userId, tmpBuf, bufSize);
	}

	return (userId[0] != '\0') ? true : false;
}

bool
sUserIdStatData::SetUserId(const char* userId)
{
	bool result = false;

	statAssertf(userId, "Null handle");
	if (!userId)
		return result;

	//If the value being set is 'STAT_UNKNOWN' 
	//   then it means we want to reset the user id.
	if (stricmp("STAT_UNKNOWN", userId) == 0)
	{
		result = true;

		m_UserId = 0;

#if RLGAMERHANDLE_NP
		if (m_NextUserId)
		{
			m_NextUserId->Reset();
		}
#endif //RLGAMERHANDLE_NP

		return result;
	}

#if RLGAMERHANDLE_XBL || RLGAMERHANDLE_SC

	m_UserId = 0;
	const int ret = sscanf(userId, "%" I64FMT "u", &m_UserId);
	result = (ret == 1);
	statAssertf(result, "Improperly formed User Id string:\"%s\"", userId);

#elif RLGAMERHANDLE_NP

	statAssertf(m_NextUserId, "Invalid stat missing storage for last 8 characters");

	if (m_NextUserId)
	{
		m_UserId = 0;
		m_NextUserId->m_UserId = 0;

		if (rlGamerHandle::IsUsingAccountIdAsKey())
		{
			const int ret = sscanf(userId, "%" I64FMT "u", &m_NextUserId->m_UserId);
			result = (ret == 1);
			statAssertf(result, "Improperly formed User Id string:\"%s\"", userId);
		}
		else
		{
			result = true;

			const u32 bufSize = strlen(userId);
			statAssert(bufSize <= 16);

			u32 shiftcounter = 0;
			for (u32 i = 0; i < bufSize; i++, shiftcounter++)
			{
				const u16 dataValue = static_cast<u16>(userId[i]);

				if (i == 8)
				{
					shiftcounter = 0;
				}
				u32 shift = shiftcounter * 8;

				u64 oldValue = 0;
				if (i < 8)
					oldValue = m_UserId;
				else
					oldValue = m_NextUserId->m_UserId;

				u64 dataInU64 = ((u64)dataValue) << (shift); //Data shifted to the position it should be
				u64 maskInU64 = ((u64)0xFF) << (shift);           //Shifted to the position were we want the bits changed

				oldValue = oldValue & ~maskInU64; //Old value with 0 on the bits we want to change

				if (i < 8)
					m_UserId = oldValue | dataInU64;
				else
					m_NextUserId->m_UserId = oldValue | dataInU64;
			}
		}
	}

#else
	statAssertf(result, " Unhandled online service:\"%s\"", userId);
#endif

#if __BANK
	if (result)
	{
#if RLGAMERHANDLE_NP
		if (m_NextUserId) 
			GetUserId(m_WidgetValue, 128);
#else
		GetUserId(m_WidgetValue, 128);
#endif
	}
#endif // __BANK

	return result;
}

void sUserIdStatData::SetDirty(const bool bDirtyProfile)
{
	sStatData::SetDirty(bDirtyProfile);

#if RLGAMERHANDLE_NP
	if(m_NextUserId)
		m_NextUserId->SetDirty(bDirtyProfile);
#endif // RLGAMERHANDLE_NP
}

void sUserIdStatData::ResynchServerAuthoritative()
{
	sStatData::ResynchServerAuthoritative();

#if RLGAMERHANDLE_NP
	if(m_NextUserId)
		m_NextUserId->ResynchServerAuthoritative();
#endif // RLGAMERHANDLE_NP
}


bool
sProfileSettingStatData::GetProfileSetting(void* pData) const
{
	if (statVerify(pData))
	{
		CProfileSettings& settings = CProfileSettings::GetInstance();
		if(settings.AreSettingsValid() && settings.Exists((CProfileSettings::ProfileSettingId) m_profileId))
		{
			const int currvalue = settings.GetInt((CProfileSettings::ProfileSettingId) m_profileId);

			sysMemCpy(pData, &currvalue, GetSizeOfData());

			return true;
		}
	}

	return false;
}

int
sProfileSettingStatData::GetInt() const
{
	int currvalue = 0;

	GetProfileSetting(&currvalue);

	return currvalue;
}

bool
sProfileSettingStatData::ValueIsEqual(void* pData) const
{
	return pData ? GetInt() == *((int*)pData) : false;
}

bool 
sStatData::GetData(void* pData, const unsigned sizeofData) const 
{
	bool ret = false;

	if (statVerify(pData))
	{
		if (GetIsBaseType(STAT_TYPE_PROFILE_SETTING) && statVerify(sizeofData == GetSizeOfData()))
		{
			return GetProfileSetting(pData);
		}

		if (statVerify(sizeofData == GetSizeOfData() || (GetType() == STAT_TYPE_USERID && (sizeofData >= GetSizeOfData() || sizeofData == sizeof(u64)))))
		{
			ret = true;

			switch (GetType())
			{
			case STAT_TYPE_TEXTLABEL: { int value = GetInt(); sysMemCpy(pData, &value, sizeofData); } break;
			case STAT_TYPE_INT:       { int value = GetInt(); sysMemCpy(pData, &value, sizeofData); } break;
			case STAT_TYPE_INT64:     { s64 value = GetInt64(); sysMemCpy(pData, &value, sizeofData); } break;
			case STAT_TYPE_FLOAT:     { float value = GetFloat(); sysMemCpy(pData, &value, sizeofData); } break;
			case STAT_TYPE_BOOLEAN:   { bool value = GetBoolean(); sysMemCpy(pData, &value, sizeofData); } break;
			case STAT_TYPE_UINT8:     { u8 value = GetUInt8(); sysMemCpy(pData, &value, sizeofData); } break;
			case STAT_TYPE_UINT16:    { u16 value = GetUInt16(); sysMemCpy(pData, &value, sizeofData); } break;
			case STAT_TYPE_UINT32:    { u32 value = GetUInt32(); sysMemCpy(pData, &value, sizeofData); } break;
			case STAT_TYPE_PACKED:    { u64 value = GetUInt64(); sysMemCpy(pData, &value, sizeofData); } break;
			case STAT_TYPE_UINT64:    { u64 value = GetUInt64(); sysMemCpy(pData, &value, sizeofData); } break;
			case STAT_TYPE_DATE:      { u64 value = GetUInt64(); sysMemCpy(pData, &value, sizeofData); } break;
			case STAT_TYPE_POS:       { u64 value = GetUInt64(); sysMemCpy(pData, &value, sizeofData); } break;
			case STAT_TYPE_STRING:    { formatf((char*)pData, sizeofData, GetString()); } break;
			case STAT_TYPE_USERID:
				{
					if (sizeofData == sizeof(u64))
					{
						u64 value = GetUInt64();
						sysMemCpy(pData, &value, sizeofData);
					}
					else
					{
						GetUserId((char*)pData, sizeofData); 
					}
				} 
				break;
			default:
				ret = false;
				statAssertf(0, "Invalid stat tag type \"%d\"", GetType()); 
				break;
			}

			if (GetIsBaseType(STAT_TYPE_STRING))
			{
				statAssert(sizeofData <= MAX_STAT_STRING_SIZE);
				statAssert(sizeofData >= strlen(GetString()));
			}
		}
	}

	return ret;
}

bool
sStatData::ExpectedProfileStatType(const rlProfileStatsValue* val) const
{
	bool typeIsCorrect = false;

	switch (GetType())
	{
	case STAT_TYPE_INT:
	case STAT_TYPE_TIME:
	case STAT_TYPE_CASH:
	case STAT_TYPE_PERCENT:
	case STAT_TYPE_DEGREES:
	case STAT_TYPE_WEIGHT:
	case STAT_TYPE_MILES:
	case STAT_TYPE_METERS:
	case STAT_TYPE_FEET:
	case STAT_TYPE_SECONDS:
	case STAT_TYPE_CHART:
	case STAT_TYPE_VELOCITY:
	case STAT_TYPE_TEXTLABEL:
	case STAT_TYPE_BOOLEAN:
	case STAT_TYPE_UINT8:
	case STAT_TYPE_UINT16:
	case STAT_TYPE_PROFILE_SETTING:
		typeIsCorrect = (RL_PROFILESTATS_TYPE_INT32 == val->GetType());
		break;

	case STAT_TYPE_UINT32:
	case STAT_TYPE_UINT64:
	case STAT_TYPE_POS:
	case STAT_TYPE_DATE:
	case STAT_TYPE_PACKED:
	case STAT_TYPE_USERID:
	case STAT_TYPE_INT64:
		typeIsCorrect = (RL_PROFILESTATS_TYPE_INT64 == val->GetType());
		break;

	case STAT_TYPE_FLOAT:
		typeIsCorrect = (RL_PROFILESTATS_TYPE_FLOAT == val->GetType());
		break;

	case STAT_TYPE_STRING:
	default:
		statAssert(0);
	}

	return typeIsCorrect;
}

rlProfileStatsType
sStatData::GetExpectedProfileStatType() const
{
	rlProfileStatsType typeIsCorrect = RL_PROFILESTATS_TYPE_INVALID;

	switch (GetType())
	{
	case STAT_TYPE_INT:
	case STAT_TYPE_TIME:
	case STAT_TYPE_CASH:
	case STAT_TYPE_PERCENT:
	case STAT_TYPE_DEGREES:
	case STAT_TYPE_WEIGHT:
	case STAT_TYPE_MILES:
	case STAT_TYPE_METERS:
	case STAT_TYPE_FEET:
	case STAT_TYPE_SECONDS:
	case STAT_TYPE_CHART:
	case STAT_TYPE_VELOCITY:
	case STAT_TYPE_TEXTLABEL:
	case STAT_TYPE_BOOLEAN:
	case STAT_TYPE_UINT8:
	case STAT_TYPE_UINT16:
	case STAT_TYPE_PROFILE_SETTING:
		typeIsCorrect = RL_PROFILESTATS_TYPE_INT32;
		break;

	case STAT_TYPE_UINT32:
	case STAT_TYPE_UINT64:
	case STAT_TYPE_DATE:
	case STAT_TYPE_POS:
	case STAT_TYPE_PACKED:
	case STAT_TYPE_USERID:
	case STAT_TYPE_INT64:
		typeIsCorrect = RL_PROFILESTATS_TYPE_INT64;
		break;

	case STAT_TYPE_FLOAT:
		typeIsCorrect = RL_PROFILESTATS_TYPE_FLOAT;
		break;

	case STAT_TYPE_STRING:
	default:
		statAssertf(false, "Strings are not supported by profile stats.");
	}

	statAssertf(RL_PROFILESTATS_TYPE_INVALID != typeIsCorrect, "Type '%d : %s' is not supported by profile stats", GetType(), GetTypeLabel());

	return typeIsCorrect;
}

bool 
sStatData::ProfileStatMatches(const rlProfileStatsValue* val)
{
	bool result = false;

	if (m_Desc.GetIsProfileStat() && statVerify(val))
	{
		if (ExpectedProfileStatType(val))
		{
			if (GetIsBaseType(STAT_TYPE_INT) || GetIsBaseType(STAT_TYPE_TEXTLABEL) || GetIsBaseType(STAT_TYPE_PROFILE_SETTING))
			{
				result = (val->GetInt32() == GetInt());
			}
			else if (GetIsBaseType(STAT_TYPE_FLOAT))
			{
				result = (val->GetFloat() == GetFloat());
			}
			else if (GetIsBaseType(STAT_TYPE_BOOLEAN))
			{
				result = (!(0 == val->GetInt32()) == GetBoolean());
			}
			else if (GetIsBaseType(STAT_TYPE_UINT8))
			{
				result = (static_cast<u8>(val->GetInt32()) == GetUInt8());
			}
			else if (GetIsBaseType(STAT_TYPE_UINT16))
			{
				result = (static_cast<u16>(val->GetInt32()) == GetUInt16());
			}
			else if (GetIsBaseType(STAT_TYPE_UINT32))
			{
				result = (static_cast<u32>(val->GetInt64()) == GetUInt32());
			}
			else if (GetIsBaseType(STAT_TYPE_UINT64) 
				|| GetIsBaseType(STAT_TYPE_DATE) 
				|| GetIsBaseType(STAT_TYPE_POS) 
				|| GetIsBaseType(STAT_TYPE_PACKED)
				|| GetIsBaseType(STAT_TYPE_USERID))
			{
				result = (static_cast<u64>(val->GetInt64()) == GetUInt64());
			}
			else if (GetIsBaseType(STAT_TYPE_INT64))
			{
				result = (val->GetInt64() == GetInt64());
			}
			else
			{
				statAssert(0);
			}
		}
	}

	return result;
}

bool 
sStatData::ReadInProfileStat(const rlProfileStatsValue* val, bool& flushStat)
{
	bool result = false;

	flushStat = false;

	if (m_Desc.GetIsProfileStat() && statVerify(val))
	{
		statAssert(!m_Desc.GetIsCommunityStat());

		if ( !m_Desc.GetSynched() && m_Desc.GetServerAuthoritative() && !m_Desc.GetIsCommunityStat() )
		{
			result = true;

			statAssert(!m_Desc.GetDirty());
			m_Desc.SetDirty(false);

			if (ExpectedProfileStatType(val))
			{
				m_Desc.SetSynched(true);

				if (GetIsBaseType(STAT_TYPE_INT) || GetIsBaseType(STAT_TYPE_TEXTLABEL))
				{
					SetInt(val->GetInt32());
				}
				else if (GetIsBaseType(STAT_TYPE_FLOAT))
				{
					SetFloat(val->GetFloat());
				}
				else if (GetIsBaseType(STAT_TYPE_BOOLEAN))
				{
					SetBoolean(!(0 == val->GetInt32()));
				}
				else if (GetIsBaseType(STAT_TYPE_UINT8))
				{
					SetUInt8(static_cast<u8>(val->GetInt32()));
				}
				else if (GetIsBaseType(STAT_TYPE_UINT16))
				{
					SetUInt16(static_cast<u16>(val->GetInt32()));
				}
				else if (GetIsBaseType(STAT_TYPE_UINT32))
				{
					SetUInt32(static_cast<u32>(val->GetInt64()));
				}
				else if (GetIsBaseType(STAT_TYPE_UINT64) 
					|| GetIsBaseType(STAT_TYPE_DATE) 
					|| GetIsBaseType(STAT_TYPE_POS) 
					|| GetIsBaseType(STAT_TYPE_PACKED)
					|| GetIsBaseType(STAT_TYPE_USERID))
				{
					SetUInt64(static_cast<u64>(val->GetInt64()));
				}
				else if (GetIsBaseType(STAT_TYPE_INT64))
				{
					SetInt64(val->GetInt64());
				}
				else
				{
					m_Desc.SetSynched(false);
					BANK_ONLY( statErrorf("Synch Failed for stat - Unknown Stat Type %s.", GetTypeLabel()); )
				}
			}
			else
			{
				BANK_ONLY(char buf[48];)
				BANK_ONLY( statErrorf("Synch Failed for stat - expected type does not match profile server config: \"%s\"", val->ToString(buf, 48)); )
			}
		}
		else if (m_Desc.GetSynched() && !m_Desc.GetIsCommunityStat())
		{
			//Set profile stat as Dirty, which means its current value needs to be written to the backend.

            //We do this regardless of whether or not the stat is server authoritative. Once we've synched
            //with the server, we want our value to be authoritative. This happens when, for example, we
            //reset stats. We reset the value locally, and keep the stat marked as synced, and issue a
            //reset on the server. If the reset on the server fails, we want to flush this stat again
            //and keep the value on the client

			if (m_Desc.GetBlockSynchEventFlush() && !m_Desc.GetDirtyInThisSession())
			{
				flushStat = false;
			}
			else if (!ProfileStatMatches(val))
			{
				flushStat = true;
			}
		}
	}

	return result;
}

bool   
sStatData::WriteOutProfileStat(rlProfileStatsValue* val)
{
	bool result = false;

	if (statVerify(val) && m_Desc.GetIsProfileStat() && m_Desc.GetSynched())
	{
		statAssert( !StatsInterface::GetBlockSaveRequests() );
	
		result = true;

		if (GetIsBaseType(STAT_TYPE_INT) || GetIsBaseType(STAT_TYPE_TEXTLABEL) || GetIsBaseType(STAT_TYPE_PROFILE_SETTING))
		{
			val->SetInt32(GetInt());
		}
		else if (GetIsBaseType(STAT_TYPE_FLOAT))
		{
			val->SetFloat(GetFloat());
		}
		else if (GetIsBaseType(STAT_TYPE_BOOLEAN))
		{
			val->SetInt32(static_cast<int>(GetBoolean()));
		}
		else if (GetIsBaseType(STAT_TYPE_UINT8) || GetIsBaseType(STAT_TYPE_UINT16))
		{
			val->SetInt32(GetIsBaseType(STAT_TYPE_UINT8) ? static_cast<int>(GetUInt8()) : static_cast<int>(GetUInt16()));
		}
		else if (GetIsBaseType(STAT_TYPE_UINT32) 
					|| GetIsBaseType(STAT_TYPE_UINT64) 
					|| GetIsBaseType(STAT_TYPE_DATE) 
					|| GetIsBaseType(STAT_TYPE_POS) 
					|| GetIsBaseType(STAT_TYPE_PACKED)
					|| GetIsBaseType(STAT_TYPE_USERID))
		{
			val->SetInt64(GetIsBaseType(STAT_TYPE_UINT32) ? static_cast<s64>(GetUInt32()) : static_cast<s64>(GetUInt64()));
		}
		else if (GetIsBaseType(STAT_TYPE_INT64))
		{
			val->SetInt64(GetInt64());
		}
		else
		{
			result = false;
			BANK_ONLY( statErrorf("Failed to write profile stat - Invalid type (\"%s\")", GetTypeLabel()); )
		}

#if __BLOCK_TAKEOVERS
		//Territories stats are blocked from being sent to the server.
		if (GetIsTerritoryStat() && !PARAM_enabletakeovers.Get())
		{
			if (val->GetInt32() != -1)
			{
				val->SetInt32(-1);
			}

			statAssert(-1 == val->GetInt32());
		}
#endif // __BLOCK_TAKEOVERS
	}

	return result;
}

bool sStatData::ConstWriteOutProfileStat(rlProfileStatsValue* val) const
{
	bool result = false;

	if (statVerify(val) && m_Desc.GetIsProfileStat())
	{
		result = true;

		if (GetIsBaseType(STAT_TYPE_INT) || GetIsBaseType(STAT_TYPE_TEXTLABEL) || GetIsBaseType(STAT_TYPE_PROFILE_SETTING))
		{
			val->SetInt32(GetInt());
		}
		else if (GetIsBaseType(STAT_TYPE_FLOAT))
		{
			val->SetFloat(GetFloat());
		}
		else if (GetIsBaseType(STAT_TYPE_BOOLEAN))
		{
			val->SetInt32(static_cast<int>(GetBoolean()));
		}
		else if (GetIsBaseType(STAT_TYPE_UINT8) || GetIsBaseType(STAT_TYPE_UINT16))
		{
			val->SetInt32(GetIsBaseType(STAT_TYPE_UINT8) ? static_cast<int>(GetUInt8()) : static_cast<int>(GetUInt16()));
		}
		else if (GetIsBaseType(STAT_TYPE_UINT32)
				|| GetIsBaseType(STAT_TYPE_UINT64)
				|| GetIsBaseType(STAT_TYPE_DATE)
				|| GetIsBaseType(STAT_TYPE_POS)
				|| GetIsBaseType(STAT_TYPE_PACKED)
				|| GetIsBaseType(STAT_TYPE_USERID))
		{
			val->SetInt64(GetIsBaseType(STAT_TYPE_UINT32) ? static_cast<s64>(GetUInt32()) : static_cast<s64>(GetUInt64()));
		}
		else if (GetIsBaseType(STAT_TYPE_INT64))
		{
			val->SetInt64(GetInt64());
		}
		else
		{
			result = false;
			BANK_ONLY( statErrorf("Failed to write profile stat - Invalid type (\"%s\")", GetTypeLabel()); )
		}
	}

	return result;
}

void 
sStatData::SetDirty(const bool bDirtyProfile)
{
	// Set profile stat as Dirty, which means its current value needs to be written to the backend.
	if (m_Desc.GetIsProfileStat())
	{
		m_Desc.SetDirty(true, bDirtyProfile);
		m_Desc.SetDirtyInThisSession(true);
	}
}

void 
sStatData::ResynchServerAuthoritative()
{
	if (m_Desc.GetServerAuthoritative())
	{
		m_Desc.SetSynched(false);
		m_Desc.SetDirty(false);
	}
}

#if __BANK
void 
sStatData::SetDirtyBank()
{
	// Set profile stat as Dirty, which means its current value needs to be written to the backend.
	if (m_Desc.GetIsProfileStat() && NetworkInterface::GetActiveGamerInfo())
	{
		m_Desc.SetDirty(true);
		m_Desc.SetDirtySavegame(true);
		m_Desc.SetSynched(true);
		m_Desc.SetForcedSynched(false);
	}
}
#endif // __BANK

#if !__FINAL

char*
sStatData::ValueToString(char* buf, const unsigned bufSize) const 
{
	if (buf && bufSize)
	{
		buf[0] = '\0';

		if (GetIsBaseType(STAT_TYPE_TEXTLABEL))
		{
			if (TheText.DoesTextLabelExist(GetInt()))
				safecpy( buf, TheText.Get(GetInt(), "STAT_LABEL"), bufSize );
			else if (0 == GetInt())
				formatf(buf, bufSize, "%s", "Null");
			else
				formatf(buf, bufSize, "Unknown string key=%d", GetInt());
		}
		else if (GetIsBaseType(STAT_TYPE_INT) || GetIsBaseType(STAT_TYPE_PROFILE_SETTING))
		{
			formatf(buf, bufSize, "%d", GetInt());
		}
		else if (GetIsBaseType(STAT_TYPE_FLOAT))
		{
			formatf(buf, bufSize, "%f", GetFloat());
		}
		else if (GetIsBaseType(STAT_TYPE_BOOLEAN))
		{
			formatf(buf, bufSize, "%s", GetBoolean() ? "True": "False");
		}
		else if (GetIsBaseType(STAT_TYPE_UINT8))
		{
			formatf(buf, bufSize, "%u", GetUInt8());
		}
		else if (GetIsBaseType(STAT_TYPE_UINT16))
		{
			formatf(buf, bufSize, "%u", GetUInt16());
		}
		else if (GetIsBaseType(STAT_TYPE_UINT32))
		{
			formatf(buf, bufSize, "%u", GetUInt32());
		}
		else if (GetIsBaseType(STAT_TYPE_UINT64) || GetIsBaseType(STAT_TYPE_DATE) ||GetIsBaseType(STAT_TYPE_POS) ||GetIsBaseType(STAT_TYPE_PACKED))
		{
			formatf(buf, bufSize, "%" I64FMT "d", GetUInt64());
		}
		else if (GetIsBaseType(STAT_TYPE_INT64))
		{
			formatf(buf, bufSize, "%" I64FMT "d", GetInt64());
		}
		else if (GetIsBaseType(STAT_TYPE_USERID))
		{
			GetUserId(buf, bufSize);
		}
		else
		{
			safecpy(buf, "Value Unknown", bufSize);
		}
	}

	return buf;
}

void
sStatData::ToString() const
{
	BANK_ONLY(char buff[48];)
	BANK_ONLY( statDebugf3("       - type=\"%s\", value=\"%s\"", sStatsTypesNames[GetType()].m_Name, ValueToString(buff, 48)); )
}

void 
sStatData::AssertsAtCompileTime()
{
	// If a new data type was added to enum eFrontendStatType then this struct has to know to what 
	// base type it belongs - int, char, float, etc.
	// Set methods GetIsBaseType(STAT_TYPE_TEXTLABEL), GetIsBaseType(STAT_TYPE_INT), GetIsBaseType(STAT_TYPE_FLOAT)
	CompileTimeAssert(NUMBER_OF_STATS_TYPE_IDS == MAX_STAT_TYPE);
}

#endif // !__FINAL

class CStatsDisplayListFileMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile & file)
	{
		switch(file.m_fileType)
		{
			case CDataFileMgr::SP_STATS_DISPLAY_LIST_FILE:
				CStatsMgr::GetStatsDataMgr().LoadDataXMLFile(file.m_filename, true);
				CStatsMgr::GetStatsDataMgr().PostLoad();
				return true;
			case CDataFileMgr::MP_STATS_DISPLAY_LIST_FILE:
				CStatsMgr::GetStatsDataMgr().LoadDataXMLFile(file.m_filename, false);
				CStatsMgr::GetStatsDataMgr().PostLoad();
				return true;
			default:
				return false;
		}
	}

	virtual void UnloadDataFile(const CDataFileMgr::DataFile& file) 
	{
		CStatsMgr::GetStatsDataMgr().ClearProcessFile(file.m_filename);
	}

} g_StatsDisplayListDataFileMounter;


// --- CStatsDataMetricMgr ----------------------------------------------------------

class MetricUNLOCK_AGGREGATE  : public MetricPlayerCoords
{
	RL_DECLARE_METRIC(UCKAGG, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	explicit MetricUNLOCK_AGGREGATE (const CStatsDataMetricMgr::TrackedData& data) 
		: m_Data(data)
	{
	}

	virtual bool Write(RsonWriter* rw) const
	{
		if (!Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("ALLOW_STAT_METRICS", 0x756B4E30), false))
		{
			return false;
		}

		bool result = MetricPlayStat::Write(rw);

		result &= rw->WriteUns("a", m_Data.m_Count);
		result &= rw->WriteUns("b", m_Data.m_NumStats);
		result &= rw->WriteUns("c", m_Data.m_ElapsedTime);

		if (m_Data.m_LevelUpElapsedTime > 0)
		{
			result &= rw->WriteUns("d", m_Data.m_LevelUpElapsedTime);
		}

		if (m_Data.m_Rank > -1)
		{
			result &= rw->WriteInt("e", m_Data.m_Rank);
		}

		if (m_Data.m_SessionType > -1)
		{
			result &= rw->WriteInt("f", m_Data.m_SessionType);
		}

		if (m_Data.m_MissionType > 0)
		{
			result &= rw->WriteUns("g", m_Data.m_MissionType);
		}

		if (m_Data.m_RootContentId > 0)
		{
			result &= rw->WriteUns("h", m_Data.m_RootContentId);
		}

		const u32 duration = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("ALLOW_STAT_METRICS_ELAPSED_TIME", 0x60E729EC), 0);
		if (duration > 0)
		{
			result &= rw->WriteUns("i", duration);
		}
		const u32 minCount = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("ALLOW_STAT_METRICS_MIN_THRESHOLDCOUNT", 0x8005B74D), 0);
		if (minCount > 0)
		{
			result &= rw->WriteUns("j", minCount);
		}
		const u32 minStatCount = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("ALLOW_STAT_METRICS_MIN_COUNT", 0x7AB6F82B), 500);
		if (minStatCount > 0)
		{
			result &= rw->WriteUns("l", minStatCount);
		}

		return result;
	}

private:
	CStatsDataMetricMgr::TrackedData m_Data;
};

void CStatsDataMetricMgr::Update()
{
	//We are offline - we cant send any metrics
	if (!NetworkInterface::IsLocalPlayerOnline())
		return;

	//No stats registered for tracking
	if (m_Stats.GetNumUsed() <= 0)
		return;

	//Network game is not in progress and no stats count.
	//Otherwise we will let the logic run regardless and send the metric even if we are in SP.
	if (!NetworkInterface::IsGameInProgress() && m_LastFlushTime == 0)
		return;

	//At least we need 500 in the number of times stats have changed.
	const u32 minStatCount = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("ALLOW_STAT_METRICS_MIN_COUNT", 0x7AB6F82B), 500);
	if (m_StatsUpdateCount < minStatCount)
		return;

	//We are starting up
	if (m_LastFlushTime == 0)
	{
		m_LastFlushTime = sysTimer::GetSystemMsTime();;
	}

	const u32 currentTime = sysTimer::GetSystemMsTime();
	const u32 elapsedTime = (currentTime - m_LastFlushTime);

	//Minimum elapsed time
	static const u32 MIN_ELAPSED_TIME = 1 * 60 * 1000;
	const u32 minStatThresholdCount = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("ALLOW_STAT_METRICS_MIN_THRESHOLDCOUNT", 0x8005B74D), 0);
	//Minimum number of stats threshold count reached.
	const bool minNumStatsThresholdReached = (elapsedTime > MIN_ELAPSED_TIME && (0 == minStatThresholdCount || m_StatsUpdateCount >= minStatThresholdCount));

	//Default elapsed time
	static const u32 DEFAULT_ELAPSED_TIME = 5 * 60 * 1000;
	const u32 elapsedTimeMin = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("ALLOW_STAT_METRICS_ELAPSED_TIME", 0x60E729EC), DEFAULT_ELAPSED_TIME);
	if (elapsedTime > elapsedTimeMin || minNumStatsThresholdReached)
	{
		m_LastFlushTime = currentTime;

		//Allowed to send the metric
		if (Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("ALLOW_STAT_METRICS", 0x756B4E30), false))
		{
			//Number of times metric has been flushed.
			m_FlushCount += 1;

			//total count 
			u32 numStats = 0;
			u32 totalCount = 0;
			StatsMetricsMap::Iterator iter = m_Stats.CreateIterator();
			for (iter.Start(); !iter.AtEnd(); iter.Next())
			{
				u32& count = iter.GetData();
				if (count > 0)
				{
					totalCount += count;
					numStats += 1;

					//Reset the stat count
					count = 0;
				}
			}

			CStatsDataMetricMgr::TrackedData data;
			data.m_Count = totalCount;
			data.m_NumStats = numStats;
			data.m_ElapsedTime = static_cast<u32>(elapsedTime / 1000);
			data.m_LevelUpElapsedTime = (m_LevelUpElapsedTime > 0 && currentTime > m_LevelUpElapsedTime) ? static_cast<u32>((currentTime - m_LevelUpElapsedTime) / 1000) : 0;
			data.m_Rank = StatsInterface::GetIntStat(STAT_RANK_FM);
			data.m_SessionType = NetworkInterface::IsGameInProgress() ? NetworkInterface::GetSessionTypeForTelemetry() : -1;
			data.m_MissionType = NetworkInterface::IsGameInProgress() ? CNetworkTelemetry::GetMatchStartedId() : 0;
			data.m_RootContentId = NetworkInterface::IsGameInProgress() ? CNetworkTelemetry::GetRootContentId() : 0;

			//Append and Flush out.
			MetricUNLOCK_AGGREGATE m(data);
			APPEND_METRIC_FLUSH(m, true);
		}

		//Clear last flush time because we are in SP.
		if (!NetworkInterface::IsGameInProgress())
		{
			m_LastFlushTime = 0;
		}

		//Clear stats update count
		m_StatsUpdateCount = 0;
		//Clear Elapsed Level Up time
		m_LevelUpElapsedTime = 0;
	}
}

void CStatsDataMetricMgr::RegisterNewStatMetric(sStatParser& statparser, const StatId& key)
{
	if (statparser.m_Desc.GetIsOnlineData() && !m_Stats.Access(key))
	{
		if (statparser.m_MetricSettingId == ATSTRINGHASH("TRACK_IN_DIG", 0x23A14FAD))
		{
			statDebugf1("Add new metric stat '%s'.", statparser.m_Name.c_str());
			m_Stats.Insert(key, 0);
		}
	}
}

void CStatsDataMetricMgr::RegisterStatChanged(const StatId& statKey)
{
	u32* count = m_Stats.Access(statKey);
	if (count)
	{
		*count += 1;
		m_StatsUpdateCount += 1;
	}

	if (NetworkInterface::IsGameInProgress())
	{
		//Level up 
		if (statKey == STAT_RANK_FM.GetStatId())
		{
			m_LevelUpElapsedTime = sysTimer::GetSystemMsTime();
		}
	}
}

// --- CStatsDataMgr ----------------------------------------------------------

CStatsDataMgr::~CStatsDataMgr()
{
    Shutdown(SHUTDOWN_CORE);
}

void
CStatsDataMgr::ClearProcessFile(const char* file)
{
	if (statVerify(file))
	{
		statWarningf("ClearProcessFile - %s.", file);
		atHashString filehash(file);
		m_ProcessedFiles.Delete(filehash);
	}
}

void
CStatsDataMgr::Init(unsigned initMode)
{
    if(INIT_CORE == initMode)
    {
#if __BANK
		sStatData::ms_obfuscationDisabled = PARAM_disableStatObfuscation.Get();
#endif // __BANK

		CDataFileMount::RegisterMountInterface(CDataFileMgr::SP_STATS_DISPLAY_LIST_FILE, &g_StatsDisplayListDataFileMounter, eDFMI_UnloadFirst);
		CDataFileMount::RegisterMountInterface(CDataFileMgr::MP_STATS_DISPLAY_LIST_FILE, &g_StatsDisplayListDataFileMounter, eDFMI_UnloadFirst);
		statAssert(m_aStatsData.GetCount() == 0);
		LoadDataFiles();
		PostLoad();

		m_CommunityStats.Init();
    }
    else if(INIT_SESSION == initMode)
    {
		statWarningf("INIT_SESSION - GetGameSessionState=%d.", CGameSessionStateMachine::GetGameSessionState());

		if (CGameSessionStateMachine::START_NEW_GAME == CGameSessionStateMachine::GetGameSessionState())
		{
			//We need to reload the stats xml files because we need to crc them again.
			statWarningf("Reset Processed files - m_ProcessedFiles.");
			m_ProcessedFiles.Reset();

#if __ASSERT
			m_numObfuscatedStatTypes = 0;
#endif // __ASSERT
		}

		LoadDataFiles();
		PostLoad();
		BANK_ONLY(Bank_WidgetShutdown();)
		BANK_ONLY(Bank_InitWidgets();)

		//We are starting a Fresh New Game.
		// - Reset Single Player stats and make a server profile stats reset.
		if ((CGameSessionStateMachine::START_NEW_GAME == CGameSessionStateMachine::GetGameSessionState())            //Starting a New Game
			|| (CGameSessionStateMachine::HANDLE_LOADED_SAVE_GAME == CGameSessionStateMachine::GetGameSessionState()) //Loading a savegame
			|| (CGameSessionStateMachine::AUTOLOAD_SAVE_GAME == CGameSessionStateMachine::GetGameSessionState())      //Loading a savegame
			)
		{
			statAssert(!NetworkInterface::IsGameInProgress());
            //We don't need to dirty the profile here, because we reset the group below
			ResetAllStats( true, false/*dirtyProfile*/ );

			//Only do a Profile Stats Group Reset if this is a new game.
			if (CGameSessionStateMachine::START_NEW_GAME == CGameSessionStateMachine::GetGameSessionState())
			{
				CProfileStats& profilestats = CLiveManager::GetProfileStatsMgr();
				for(u32 i=PS_GRP_SP_START; i<=PS_GRP_SP_END; i++)
					profilestats.ResetGroup(i);
			}

            //Reset our timestamps to 0 since we're on a brand new save
            m_ProfileStatsTimestampLoadedFromSavegame = 0;
            m_SavegameTimestampLoadedFromSavegame = 0;
		}

		//Update the values of all community stats.
		UpdateCommunityStatValues();
	}

	m_SaveMgr.Init(initMode);
}

void  
CStatsDataMgr::Shutdown(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_CORE)
    {
	   Clear();
    }
    else if(shutdownMode == INIT_SESSION)
    {
#if __BANK
	    Bank_WidgetShutdown();
#endif
    }

	m_SaveMgr.Shutdown(shutdownMode);
}

void
CStatsDataMgr::Update()
{
	//Saves manager update
	m_SaveMgr.Update();

	//Online bounded operations.
	//No point doing anything if the local player is not Online.
	if (!NetworkInterface::IsLocalPlayerOnline())
		return;

	// --------------------------------------------
	// Network game in progress
	// --------------------------------------------
	if (NetworkInterface::IsGameInProgress())
	{
		m_ReadSessionProfileStats.Update();
	}

	//Update stats metrics
	m_StatsMetricsMgr.Update();

#if __BANK
    if (!m_SyncConsistencyStatus.Pending() && !m_SyncConsistencyStatus.None())
    {
        m_SyncConsistencyStatus.Reset();
		profileStatDebugf("Finished profile sync consistency check. Look out for any errors above.");
	}
	UpdateDebugDraw();
#endif//#if __BANK
}

void CStatsDataMgr::LoadDataFiles()
{
	strStreamingEngine::GetLoader().CallKeepAliveCallback();

	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetLastFile(CDataFileMgr::SP_STATS_DISPLAY_LIST_FILE);
	while(DATAFILEMGR.IsValid(pData))
	{
		if (!pData->m_disabled)
		{
			LoadDataXMLFile(pData->m_filename);
		}
		pData = DATAFILEMGR.GetPrevFile(pData);
		strStreamingEngine::GetLoader().CallKeepAliveCallbackIfNecessary();
	}

	strStreamingEngine::GetLoader().CallKeepAliveCallback();

	pData = DATAFILEMGR.GetLastFile(CDataFileMgr::MP_STATS_DISPLAY_LIST_FILE);
	strStreamingEngine::GetLoader().CallKeepAliveCallbackIfNecessary();
	while(DATAFILEMGR.IsValid(pData))
	{
		if (!pData->m_disabled)
		{
			LoadDataXMLFile(pData->m_filename,false);
		}
		pData = DATAFILEMGR.GetPrevFile(pData);
		strStreamingEngine::GetLoader().CallKeepAliveCallbackIfNecessary();
	}
}

void CStatsDataMgr::PostLoad()
{
	CStatsDataMgr::StatsMap::Iterator iter = StatIterBegin();
	while (iter != StatIterEnd())
	{
		sStatDataPtr* ppStat = *iter;
		if (!ppStat->KeyIsvalid())
		{
			sStatData* pStat = *ppStat;
			if (pStat)
			{
				delete pStat;
			}

			m_aStatsData.Remove(iter);
		}

		++iter;
	}

	//Sort binary map.
	m_aStatsData.FinishInsertion();

#if __ASSERT
	StatId dup = m_aStatsData.FindDuplicates();
	statAssertf(!dup.IsValid(), "Duplicate stat slot (id=\"%s (%d)\")", dup.GetName(), dup.GetHash());

	// Sanity check the obfuscated types we're using
	if(m_numObfuscatedStatTypes != EXPECTED_NUM_OBFUSCATED_STATS)
	{
		statWarningf("Mismatched EXPECTED_NUM_OBFUSCATED_STATS detected!");
		statWarningf("The following types are obfuscated:");
		statWarningf("   short = obfuscated int");
		statWarningf("   double = obfuscated float");
		statWarningf("   x64 = obfuscated u64");
		statWarningf("   long = obfuscated s64");
		statWarningf("List of obfuscated stats:");
		for (StatsMap::Iterator iter = m_aStatsData.Begin(); iter != m_aStatsData.End(); ++iter)
		{
			const StatId& key = iter.GetKey();
			if (!key.IsValid())
				continue;

			sStatData* pStat = *iter;
			if(pStat->IsObfuscated())
			{
				atStatNameString hashStr = key;
				statWarningf("   %s", hashStr.GetCStr());
			}
		}
		statAssertf(false, "Got %d obfuscated stat types, but we expected %d! Either update "
			"EXPECTED_NUM_OBFUSCATED_STATS, or change the type of the above stats.",
			m_numObfuscatedStatTypes, EXPECTED_NUM_OBFUSCATED_STATS);
	}
#endif //__ASSERT

	DEV_ONLY( ValidateStaticStatsIds(); )
	BANK_ONLY( Bank_CalculateAllDataSizes(); )
}

void
CStatsDataMgr::ResetStat(sStatDataPtr* ppStat, const bool dirtyProfile)
{
	if (statVerify(ppStat) && statVerify(ppStat->GetKey().IsValid()))
	{
		sStatData* pStat = *ppStat;
		if (statVerify(pStat))
		{
			BANK_ONLY( statDebugf2("Reset stat %s value to 0", ppStat->GetKeyName()); )

			const bool valueWillchange = !pStat->IsDefaultValue();

			pStat->Reset();

			if (pStat->GetDesc().GetIsProfileStat() && !GetSavesMgr().GetBlockSaveRequests())
			{
				if (valueWillchange)
				{
					if (dirtyProfile)
                    {
					    pStat->GetDesc().SetDirty(true);
                    }
					pStat->GetDesc().SetDirtySavegame(true);
				}
			}

			//////////////////////////////////////////////////////
			// GTA V Exceptions for stats that are not Reset to 0
			static StatId s_NatType = HASH_STAT_ID("_NatType");
			static StatId s_AcctSubscriptionLevel = HASH_STAT_ID("_AcctSubscriptionLevel");
			static StatId s_reportStrength = ATSTRINGHASH("MPPLY_REPORT_STRENGTH", 0x850CF917);
			static StatId s_commendStrength = ATSTRINGHASH("MPPLY_COMMEND_STRENGTH", 0xADA7E7D3);
			if (ppStat->GetKey() == s_reportStrength || ppStat->GetKey() == s_commendStrength)
			{
				int data = 16; bool retHaschangedValue = false;
				pStat->SetData(&data, sizeof(int), retHaschangedValue);
			}
			else if (ppStat->GetKey() == s_NatType || ppStat->GetKey() == s_AcctSubscriptionLevel)
			{
				int data = -1; bool retHaschangedValue = false;
				pStat->SetData(&data, sizeof(int), retHaschangedValue);
			}

#if __BANK
			if(0 < pStat->GetDesc().GetFlushPriority())
			{
				OUTPUT_ONLY( char buff[48]; )
				statDebugf3("%s - Reset stat name='%s'(hash='%d',group='%d',category='%d') value to '%s'.",CTheScripts::GetCurrentScriptNameAndProgramCounter()
																,ppStat->GetKeyName()
																,(int)ppStat->GetKey().GetHash()
																,pStat->GetDesc().GetGroup()
																,pStat->GetDesc().GetCategory()
																,pStat->ValueToString(buff, 48));
			}
#endif // __BANK
		}
	}
};

void
CStatsDataMgr::ResetAllStats(const bool offlineDataOnly, const bool dirtyProfile)
{
	StatsMap::Iterator iter = m_aStatsData.Begin();
	for (;iter != m_aStatsData.End(); ++iter)
	{
		const StatId& key = iter.GetKey();

		if (!key.IsValid())
			continue;

		sStatData* pStat = *iter;

		//Reset all data or only the single player stats.
		if (pStat && (!offlineDataOnly || !pStat->GetDesc().GetIsOnlineData()))
		{
			const sStatDescription desc = pStat->GetDesc();
			if (desc.GetBlockReset())
			{
				statWarningf("Stat with key='%s' is Blocked to Reset.", iter.GetKeyName());

				if (desc.GetSynched() && desc.GetDirty())
				{
					statWarningf("[ResetAllStats] Stat with key='%s' - Clearing Dirty FLAG.", iter.GetKeyName());
					pStat->GetDesc().SetDirty( false );
				}

				//if (desc.GetSynched() && desc.GetSavegameDirty())
				//	pStat->GetDesc().SetDirtySavegame(false);

				continue;
			}
			else
			{
				ResetStat(*iter, dirtyProfile);
			}
		}
	}
}

void
CStatsDataMgr::SignedOut()
{
#if __XENON
	//We need to reload the stats xml files because we need to crc them again.
	statWarningf("Reset Processed files - m_ProcessedFiles.");
	m_ProcessedFiles.Reset();
#endif // __XENON

	StatsMap::Iterator iter = m_aStatsData.Begin();
	for (;iter != m_aStatsData.End();++iter)
	{
		const StatId& key = iter.GetKey();

		if (!key.IsValid())
			continue;

		sStatData* pStat = *iter;

		if (pStat)
		{
			pStat->Reset();
            pStat->m_Desc.SetSynched(false);
            pStat->m_Desc.SetDirty(false);
            pStat->m_Desc.SetDirtySavegame(false);
		}
	}

	EndNetworkMatch();
	m_SaveMgr.Shutdown(SHUTDOWN_CORE);
}

void
CStatsDataMgr::SignedOffline()
{
	//Clear all Online DATA
	StatsMap::Iterator iter = m_aStatsData.Begin();
	for (;iter != m_aStatsData.End();++iter)
	{
		const StatId& key = iter.GetKey();

		if (!key.IsValid())
			continue;

		sStatData* pStat = *iter;

		if (pStat && pStat->GetDesc().GetIsOnlineData())
		{
			pStat->Reset();
			pStat->m_Desc.SetSynched(false);
			pStat->m_Desc.SetDirty(false);
			pStat->m_Desc.SetDirtySavegame(false);
		}
	}

	m_SaveMgr.SignedOffline( );
}

void
CStatsDataMgr::SignedIn()
{
	m_SaveMgr.Init(INIT_SESSION);
}

#if !__NO_OUTPUT
void
CStatsDataMgr::SpewDirtyCloudOnlySats(const bool multiplayer)
{
	if (PARAM_spewDirtySavegameStats.Get() && ((multiplayer && s_DirtySavegameMPStatsCount > 0) || (!multiplayer && s_DirtySavegameSPStatsCount > 0)))
	{
		statDebugf3("-----------------------------------------------------------------");
		statDebugf3("Number of Dirty Stats: < %d >", multiplayer ? s_DirtySavegameMPStatsCount : s_DirtySavegameSPStatsCount);

		StatsMap::Iterator iter = m_aStatsData.Begin();
		for ( ; iter != m_aStatsData.End(); ++iter)
		{
			const StatId& key = iter.GetKey();
			if (!key.IsValid())
				continue;

			sStatData* pStat = *iter;
			if (pStat && pStat->GetDesc().GetSavegameDirty() && (pStat->GetDesc().GetIsOnlineData() == multiplayer))
			{
				statDebugf3(" ..... stat = < %s > ", iter.GetKey().GetName());
			}
		}

		statDebugf3("-----------------------------------------------------------------");
	}
}
#endif // !__NO_OUTPUT

void  
CStatsDataMgr::PreSaveBaseStats(const bool multiplayer)
{
    //Reset some stats when a game is saved
	StatId stat = StatsInterface::GetStatsModelHashId("KILLS_SINCE_LAST_CHECKPOINT");
	if (StatsInterface::IsKeyValid(stat))
	{
		CStatsMgr::IncrementStat(StatsInterface::GetStatsModelHashId("TOTAL_LEGITIMATE_KILLS"), (float)StatsInterface::GetIntStat(stat));
		StatsInterface::SetStatData(stat, 0);
	}

    //Setup timestamp of last save done by the single player game.
	if (!multiplayer)
	{
        StatsInterface::SetStatData(STAT_SP_SAVE_TIMESTAMP, rlGetPosixTime(), STATUPDATEFLAG_DIRTY_PROFILE);

		//Trigger a Force flush.
		if (NetworkInterface::IsLocalPlayerOnline() && rlProfileStats::IsInitialized())
		{
			CLiveManager::GetProfileStatsMgr().Flush(true, false);
		}

        //Cache our timestamps that we loaded from this savegame
        m_ProfileStatsTimestampLoadedFromSavegame = StatsInterface::GetUInt64Stat(STAT_SP_PROFILE_STAT_VERSION);
        m_SavegameTimestampLoadedFromSavegame = StatsInterface::GetUInt64Stat(STAT_SP_SAVE_TIMESTAMP);
	}

	if ((multiplayer && s_DirtySavegameMPStatsCount > 0) || (!multiplayer && s_DirtySavegameSPStatsCount > 0))
	{
		statDebugf3("-----------------------------------------------------------------");
		statDebugf3("Number of Dirty Stats: < %d >", multiplayer ? s_DirtySavegameMPStatsCount : s_DirtySavegameSPStatsCount);

		StatsMap::Iterator iter = m_aStatsData.Begin();
		for ( ; iter != m_aStatsData.End(); ++iter)
		{
			const StatId& key = iter.GetKey();
			if (!key.IsValid())
				continue;

			sStatData* pStat = *iter;
			if (pStat && pStat->GetDesc().GetSavegameDirty() && (pStat->GetDesc().GetIsOnlineData() == multiplayer))
			{
				statDebugf3(" ..... stat = < %s > ", iter.GetKey().GetName());
				sStatData* stat = *iter;
				stat->GetDesc().SetDirtySavegame(false);
			}
		}

		statDebugf3("-----------------------------------------------------------------");
	}
}

void
CStatsDataMgr::PostLoadBaseStats(const bool multiplayer)
{
    //If this is the SP save, cache the profile stats timestamp that we loaded
    //this savegame from
    if (!multiplayer)
    {
        m_ProfileStatsTimestampLoadedFromSavegame = StatsInterface::GetUInt64Stat(STAT_SP_PROFILE_STAT_VERSION);
        m_SavegameTimestampLoadedFromSavegame     = StatsInterface::GetUInt64Stat(STAT_SP_SAVE_TIMESTAMP);
    }

	//Apply community stats data.
	UpdateCommunityStatValues();
}

void  
CStatsDataMgr::Clear()
{
	m_OnlineDataIsSynched = false;

	while(m_aStatsData.GetCount() > 0)
	{
		sStatDataPtr* ppStat = *m_aStatsData.Begin();
		sStatData*     pStat = ppStat && ppStat->KeyIsvalid() ? *ppStat : 0;

		if (pStat)
		{
			delete pStat;
		}

		statDebugf2("Destroying stat (id=%s)", ppStat->GetKeyName());

		m_aStatsData.Remove(0);
	}

	statAssert(m_aStatsData.GetCount() == 0);

	statAssert(0 == m_NumHighPriorityStats);
	statAssert(!m_HighPriorityStatIds);
	statAssert(0 == m_NumLowPriorityStats);
	statAssert(!m_LowPriorityStatIds);
	for (int i=0; i<MAX_NUM_REMOTE_GAMERS; i++)
	{
		statAssert(!m_HighPriorityRecords[i].GetValues());
		statAssert(!m_LowPriorityRecords[i].GetValues());
	}

	//Just in case check this has been done
	EndNetworkMatch();

#if __BANK
	Bank_WidgetShutdown();
#endif
	
	m_ProcessedFiles.Reset();
}

void
CStatsDataMgr::ResetAllOnlineCharacterStats(const int prefix, const bool UNUSED_PARAM(saveGame), const bool localOnly)
{
	if (statVerify(-1 < prefix))
	{
		//Magic number see eProfileStatsGroups FUCK THIS SHYTE - THIS CREATED LOADS OF FUCKING ISSUES.
		const eProfileStatsGroups group = (eProfileStatsGroups)(prefix+1);

		StatsMap::Iterator iter = m_aStatsData.Begin();
		for ( ; iter != m_aStatsData.End(); ++iter)
		{
			const StatId& key = iter.GetKey();

			if (!key.IsValid())
				continue;

			sStatData* pStat = *iter;
			if (pStat)
			{
				const sStatDescription desc = pStat->GetDesc();
				if (!desc.GetIsOnlineData())
					continue;

				if (desc.GetBlockReset())
				{
					statWarningf("Stat with key='%s' is Blocked to Reset.", iter.GetKeyName());

					if (desc.GetSynched() && desc.GetDirty())
					{
						statWarningf("[ResetAllOnlineCharacterStats] Stat with key='%s' - Clearing Dirty FLAG.", iter.GetKeyName());
						pStat->GetDesc().SetDirty( false );
					}

					//if (desc.GetSynched() && desc.GetSavegameDirty())
					//	pStat->GetDesc().SetDirtySavegame(false);

					continue;
				} 
				else if (desc.GetGroup() == group)
				{
					if (!desc.GetSynched()) statErrorf("Stat %s is not synched reset wil be forced.", iter.GetKeyName());
					//We don't need to dirty the profile here, because we do a reset group below
					ResetStat(*iter, false/*dirtyProfile*/);
				}
			}
		}

		if (!localOnly)
			CLiveManager::GetProfileStatsMgr().ResetGroup( group ); 
	}
}

const sStatData* 
CStatsDataMgr::GetStat(const StatId& keyHash) const
{
	if (statVerify(IsStatsDataValid()))
	{
		const sStatDataPtr* ppStat = m_aStatsData.UnSafeGet(keyHash);
		if (ppStat && ppStat->KeyIsvalid())
		{
			return *ppStat;
		}
	}

	return 0;
}

sStatData*  
CStatsDataMgr::GetStat(const StatId& keyHash) 
{
	if (statVerify(IsStatsDataValid()))
	{
		sStatDataPtr* ppStat = m_aStatsData.UnSafeGet(keyHash);
		if (ppStat && ppStat->KeyIsvalid())
		{
			return *ppStat;
		}
	}

	return 0;
}

const sStatDataPtr*  
CStatsDataMgr::GetStatPtr(const StatId& keyHash) const
{
	if (statVerify(IsStatsDataValid()))
	{
		return m_aStatsData.SafeGet(keyHash);
	}

	return 0;
}

sStatDataPtr*  
CStatsDataMgr::GetStatPtr(const StatId& keyHash) 
{
	if (statVerify(IsStatsDataValid()))
	{
		return m_aStatsData.SafeGet(keyHash);
	}

	return 0;
}

bool 
CStatsDataMgr::IsKeyValid(const StatId& keyHash) const
{
	if (IsStatsDataValid())
	{
		const sStatDataPtr* ppStat = m_aStatsData.SafeGet(keyHash);
		return (ppStat && ppStat->KeyIsvalid());
	}

	return false;
}

#if __BANK
void
CStatsDataMgr::SetStatDirty(const StatId& keyHash)
{
	sStatData* pStat = GetStat(keyHash);
	if (pStat && pStat->GetDesc().GetIsProfileStat())
	{
		pStat->SetDirtyBank();
	}
}

bool CStatsDataMgr::GetStatObfuscationDisabled()
{
	return PARAM_disableStatObfuscation.Get();
}

void 
	CStatsDataMgr::SetBlockProfileStatFlush(const StatId& keyHash, const bool value)
{
	sStatData* stat = GetStat( keyHash );
	if (gnetVerify( stat ))
	{
		sStatDescription& desc = stat->GetDesc();
		desc.SetBlockProfileStatFlush( value );

		if (desc.GetBlockSynchEventFlush())
		{
			if (!value)
			{
				desc.SetSynched(true);
			}
			else
			{
				desc.SetSynched(false);
			}
		}
	}
}

#endif

void
CStatsDataMgr::ResynchServerAuthoritative(const bool onlinestat)
{
	StatsMap::Iterator iter = m_aStatsData.Begin();
	while (iter != m_aStatsData.End())
	{
		sStatData* pStat = 0;

		if (iter.GetKey().IsValid())
			pStat = *iter;

		if (pStat)
		{
			const sStatDescription& desc = pStat->GetDesc();
			if (desc.GetIsOnlineData() == onlinestat && desc.GetServerAuthoritative())
			{
				ResetStat(*iter, false);
				pStat->ResynchServerAuthoritative();
			}
		}
			
		iter++;
	}
}

void  
CStatsDataMgr::SetAllStatsToSynched(const int savegameSlot, const bool onlinestat, const bool includeServerAuthoritative)
{
	statDebugf1(" ***** SET ALL STATS TO SYNCHED ***** ");
	statDebugf1(" ........................... Slot = '%d'", savegameSlot);
	statDebugf1(" ......................... Online = '%s'", onlinestat?"true":"false");
	statDebugf1(" ........... Server Authoritative = '%s'", includeServerAuthoritative?"true":"false");

	StatsMap::Iterator iter = m_aStatsData.Begin();
	while (iter != m_aStatsData.End())
	{
		sStatData* pStat = 0;

		if (iter.GetKey().IsValid())
		{
			pStat = *iter;
		}

		if (pStat 
			&& !pStat->GetDesc().GetSynched() 
			&& (pStat->GetDesc().GetIsOnlineData() == onlinestat) 
			&& (includeServerAuthoritative || !pStat->GetDesc().GetServerAuthoritative()) 
			&& (-1 == savegameSlot || savegameSlot == (int)pStat->GetDesc().GetCategory()))
		{
			pStat->GetDesc().SetSynched(true);
		}

		iter++;
	}
}

void  
CStatsDataMgr::SetAllStatsToSynchedByGroup(const eProfileStatsGroups group, const bool onlinestat, const bool includeServerAuthoritative)
{
	statDebugf1(" ***** SET ALL STATS TO SYNCHED ***** ");
	statDebugf1(" ........................... Slot = '%d'", group);
	statDebugf1(" ......................... Online = '%s'", onlinestat?"true":"false");
	statDebugf1(" ........... Server Authoritative = '%s'", includeServerAuthoritative?"true":"false");
	statDebugf1(" ************************************ ");

	StatsMap::Iterator iter = m_aStatsData.Begin();
	while (iter != m_aStatsData.End())
	{
		sStatData* pStat = 0;

		if (iter.GetKey().IsValid())
		{
			pStat = *iter;
		}

		if (pStat)
		{
			sStatDescription& desc = pStat->GetDesc();
			if (!desc.GetSynched() && desc.GetIsOnlineData() == onlinestat && (includeServerAuthoritative || !desc.GetServerAuthoritative()))
			{
				if (group == desc.GetGroup())
				{
					statDebugf1(" # Reset Synch Stat = '%s'", iter.GetKeyName());
					pStat->GetDesc().SetSynched(true);
				}
			}
		}

		iter++;
	}
}

bool  
CStatsDataMgr::GetAllOnlineStatsAreSynched( )
{
	statDebugf1(" ***** GET ALL STATS TO SYNCHED ***** ");

	bool result = true;

	StatsMap::Iterator iter = m_aStatsData.Begin();
	while (iter != m_aStatsData.End())
	{
		sStatData* pStat = 0;

		if (iter.GetKey().IsValid())
		{
			pStat = *iter;
		}

		if (pStat && !pStat->GetDesc().GetSynched() && pStat->GetDesc().GetIsOnlineData() && pStat->GetDesc().GetServerAuthoritative())
		{
			statErrorf("Stat %s is not synched after loading.", iter.GetKeyName());
			result = false;
		}

		iter++;
	}

	return result;
}


void  
CStatsDataMgr::SetStatSynched(const StatId& keyHash)
{
	sStatData* pStat = GetStat(keyHash);
	if (pStat)
	{
		pStat->GetDesc().SetSynched(true);
	}
}

#if __NET_SHOP_ACTIVE

bool
CStatsDataMgr::SetIsPlayerInventory(StatsMap::Iterator& statIter)
{
	if (statIter.IsValid())
	{
		StatId& statKey = statIter.GetKey();
		DEV_ONLY(if (!statKey.IsValid()) statWarningf("Stat key=\"%s\" hash=\"%d\" is NOT VALID", statIter.GetKeyName(), statKey.GetHash());)

		sStatData* pStat = 0;
		if (statKey.IsValid())
		{
			pStat = *statIter;
		}

		if (statVerify(pStat))
		{
			if (pStat->GetDesc().GetPlayerInventory())
				return true;

			if (pStat->GetDesc().SetIsPlayerInventory())
			{
				pStat->GetDesc().SetSynched(true);
				pStat->GetDesc().SetForcedSynched(false);
				pStat->GetDesc().SetDirtyInThisSession(false);

				statDebugf1("Stat key=\"%s\" hash=\"%d\" - SETUP AS PLAYER INVENTORY.", statIter.GetKeyName(), statKey.GetHash());

				return true;
			}
			else
			{
				statErrorf("Stat key=\"%s\" hash=\"%d\" - FAILED SETUP AS PLAYER INVENTORY.", statIter.GetKeyName(), statKey.GetHash());
			}
		}
	}

	return false;
}

void
CStatsDataMgr::ClearPlayerInventory( )
{
	GameTransactionSessionMgr& sessionMgr = GameTransactionSessionMgr::Get();
	
	if (sessionMgr.IsSlotInvalid())
	{
		gnetDebug1("Avoid CStatsDataMgr::ClearPlayerInventory - GAME_SERVER slot is invalid.");
		return;
	}

	const u32 slot = (sessionMgr.GetCharacterSlot()-CHAR_MP_START);
	gnetDebug1("CStatsDataMgr::ClearPlayerInventory - GAME_SERVER slot is '%d'.", slot);

	netCatalog& catalogInst = netCatalog::GetInstance();
	const atMap < int, sPackedStatsScriptExceptions >& packedExc = catalogInst.GetScriptExceptions();

	StatsMap::Iterator iter = m_aStatsData.Begin();
	while (iter != m_aStatsData.End())
	{
		sStatData* pStat = 0;

		if (iter.GetKey().IsValid())
			pStat = *iter;

		if (pStat)
		{
			sStatDescription& desc = pStat->GetDesc();

			//Stats that are used by the player inventory - Either clear all or only clear the current selected character.
			if (desc.GetPlayerInventory() && (desc.GetIsPlayerStat() || slot == desc.GetMultiplayerCharacterSlot()))
			{
				const sPackedStatsScriptExceptions* exc = packedExc.Access(iter.GetKey().GetHash());

				//Based on certain exceptions we might have to reset it differently to make sure the
				//  bits used by script are not reset.
				if (exc)
				{
					statAssert(pStat->GetIsBaseType(STAT_TYPE_PACKED));

					//Cache current value.
					const u64 value = pStat->GetUInt64();
					pStat->Reset();

					//Only needed if values are > 0
					if (value > 0)
					{
						exc->SetExceptions(iter.GetKey(), value);
						statDebugf1("Clear player Inventory Stat key=\"%s\" hash=\"%d\", before=\"%" I64FMT "d\", after=\"%" I64FMT "d\".", iter.GetKeyName(), iter.GetKey().GetHash(), value, pStat->GetUInt64());
					}
					else
					{
						statDebugf3("Clear player Inventory Stat key=\"%s\" hash=\"%d\".", iter.GetKeyName(), iter.GetKey().GetHash());
					}
				}
				else
				{
					pStat->Reset();
					statDebugf3("Clear player Inventory Stat key=\"%s\" hash=\"%d\".", iter.GetKeyName(), iter.GetKey().GetHash());
				}
			}			
		}

		iter++;
	}
}
#endif //__NET_SHOP_ACTIVE


void  
CStatsDataMgr::SetStatSynched(StatsMap::Iterator& statIter)
{
	StatId& statKey = statIter.GetKey();
	DEV_ONLY(if (!statKey.IsValid()) statWarningf("Stat key=\"%s\" hash=\"%d\" is NOT VALID", statIter.GetKeyName(), statKey.GetHash());)

	sStatData* pStat = 0;
	if (statKey.IsValid())
	{
		pStat = *statIter;
	}

	if (statVerify(pStat))
	{
		pStat->GetDesc().SetSynched(true);
	}
}

bool 
CStatsDataMgr::GetStatIsNotNull(const StatId& keyHash) const
{
	const sStatData* stat = GetStat(keyHash);

	if (stat)
	{
		if (stat->GetIsBaseType(STAT_TYPE_INT) || stat->GetIsBaseType(STAT_TYPE_PROFILE_SETTING))
		{
			return (0 < stat->GetInt());
		}
		else if (stat->GetIsBaseType(STAT_TYPE_FLOAT))
		{
			return (0 < stat->GetFloat());
		}
		else if (stat->GetIsBaseType(STAT_TYPE_TEXTLABEL))
		{
			return (0 != stat->GetInt());
		}
		else if (stat->GetIsBaseType(STAT_TYPE_UINT8))
		{
			return (0 < stat->GetUInt8());
		}
		else if (stat->GetIsBaseType(STAT_TYPE_UINT16))
		{
			return (0 < stat->GetUInt16());
		}
		else if (stat->GetIsBaseType(STAT_TYPE_UINT32))
		{
			return (0 < stat->GetUInt32());
		}
		else if (stat->GetIsBaseType(STAT_TYPE_UINT64) 
					|| stat->GetIsBaseType(STAT_TYPE_DATE) 
					|| stat->GetIsBaseType(STAT_TYPE_POS) 
					|| stat->GetIsBaseType(STAT_TYPE_PACKED)
					|| stat->GetIsBaseType(STAT_TYPE_USERID)
					)
		{
			return (0 < stat->GetUInt64());
		}
		else if (stat->GetIsBaseType(STAT_TYPE_INT64))
		{
			return (0 < stat->GetInt64());
		}
	}

	return false;
}

const char*  
CStatsDataMgr::GetStatTypeLabel(const StatId& keyHash)  const
{
	const sStatData* pStat = GetStat(keyHash);
	if (statVerify(pStat))
	{
		return pStat->GetTypeLabel();
	}
	return "NONE";
}

bool
CStatsDataMgr::SetStatIterData(StatsMap::Iterator& statIter, void* pData, const unsigned sizeofData, const u32 flags)
{
	bool result = false;

	if (!CStatsUtils::IsStatsTrackingEnabled() && !(flags&STATUPDATEFLAG_LOAD_FROM_SAVEGAME) )
	{
		return result;
	}

	StatId& statKey = statIter.GetKey();
	DEV_ONLY(if (!statKey.IsValid()) statWarningf("Stat key=\"%s\" hash=\"%d\" is NOT VALID", statIter.GetKeyName(), statKey.GetHash());)

	sStatData* pStat = 0;
	if (statKey.IsValid())
	{
		pStat = *statIter;
	}

	if (statVerify(pStat))
	{
		sStatDescription& statDesc = pStat->GetDesc();

		//DO NOT Allow updates from level design to player inventory
		if (flags&STATUPDATEFLAG_SCRIPT_UPDATE && statDesc.GetPlayerInventory())
		{
			statDebugf3("FAILED to Change Inventory Stat with key=\"%s\" hash=\"%d\".", statIter.GetKeyName(), statKey.GetHash());
			return true;
		}

		if (flags&STATUPDATEFLAG_GAMER_SERVER && statDesc.GetPlayerInventory())
		{
			statWarningf("Change Inventory Stat with key=\"%s\" hash=\"%d\".", statIter.GetKeyName(), statKey.GetHash());
		}

		//Not a character stat (Model dependent) or the local player has a valid model.
		if (flags&STATUPDATEFLAG_LOAD_FROM_SAVEGAME || !statDesc.GetIsCharacterStat() || statVerify(StatsInterface::GetStatsPlayerModelValid()))
		{
			if (pStat->GetIsBaseType(STAT_TYPE_STRING))
			{
				statAssertf(sizeofData <= pStat->GetSizeOfData(), "Failed Change STRING stat=\"%s\",type=\"%s\",size=\"%d\". Data size=\"%d\" is invalid.", statIter.GetKeyName(), pStat->GetTypeLabel(), pStat->GetSizeOfData(), sizeofData);
			}
			else if (pStat->GetIsBaseType(STAT_TYPE_USERID))
			{
				// Do nothing
				statAssertf(sizeofData <= pStat->GetSizeOfData(), "Failed Change USER_ID stat=\"%s\",type=\"%s\",size=\"%d\". Data size=\"%d\" is invalid.", statIter.GetKeyName(), pStat->GetTypeLabel(), pStat->GetSizeOfData(), sizeofData);
			}
			else
			{
				if (pStat->GetIsBaseType(STAT_TYPE_TEXTLABEL))
				{
					statAssertf(0 == *((int*)pData) || TheText.DoesTextLabelExist(*((int*)pData)), "Invalid string label=\"%d\" set for stat=\"%s\"", *((int*)pData), statIter.GetKeyName());
				}

				statAssertf(sizeofData == pStat->GetSizeOfData(), "Failed Change stat=\"%s\",type=\"%s\",size=\"%d\". Data size=\"%d\" is invalid.", statIter.GetKeyName(), pStat->GetTypeLabel(), pStat->GetSizeOfData(), sizeofData);
			}

            //Don't load server-authoritative stats from the MP savegame. We want to keep whatever we read from profile stats
            if (flags&STATUPDATEFLAG_LOAD_FROM_SAVEGAME && statDesc.GetServerAuthoritative())
            {
                return false;
            }

            //Don't load MP stats from the SP savegame
            if (!statVerify(!(flags&STATUPDATEFLAG_LOAD_FROM_SP_SAVEGAME && statDesc.GetIsOnlineData())))
            {
                return false;
            }

			// Debug metric to help track down XP loss - B*2239619
			int oldValue=0;
			bool isXpStat = (  statKey == HASH_STAT_ID("MP0_CHAR_XP_FM")
							|| statKey == HASH_STAT_ID("MP1_CHAR_XP_FM")
							|| statKey == HASH_STAT_ID("MP2_CHAR_XP_FM")
							|| statKey == HASH_STAT_ID("MP3_CHAR_XP_FM"));

			if(isXpStat)
			{
				oldValue = pStat->GetInt();
			}

			bool retHaschangedValue = false;
			result = pStat->SetData(pData, sizeofData, retHaschangedValue);

			// Debug metric to help track down XP loss - B*2239619
			if(result && retHaschangedValue && isXpStat)
			{
				int newValue = *((int*)pData);
				if(newValue<oldValue && oldValue>0)
				{
					CNetworkTelemetry::AppendXPLoss(oldValue, newValue);
				}
			}

            //Mark as synched now that we've loaded the authoritative value (only MP stats need to be marked synched)
            if (flags&STATUPDATEFLAG_LOAD_FROM_MP_SAVEGAME && statDesc.GetIsOnlineData())
            {
                statDesc.SetSynched(true);
            }

			bool forcedStatDataChange = false;
			//Force value has changed so that we can get a prof stat flush.
			if (!retHaschangedValue && (flags&STATUPDATEFLAG_FORCE_CHANGE))
			{
				retHaschangedValue = true;
				forcedStatDataChange = true;
			}

			if (result && retHaschangedValue)
			{
#if __BANK
				if (retHaschangedValue && pStat->DebugStatChanges())
				{
					char buff[48];
					profileStatDebugf("Stat \"%s\" has CHANGED VALUE, id=(\"%d\"), Value=\"%s\"", statIter.GetKeyName(), statIter.GetKey().GetHash(), pStat->ValueToString(buff, 48));
				}
#endif // __BANK

				//Update stats metrics.
				if (!(flags&STATUPDATEFLAG_LOAD_FROM_SAVEGAME) && statDesc.GetIsOnlineData() && !forcedStatDataChange)
				{
					m_StatsMetricsMgr.RegisterStatChanged(statKey);
				}

				//Set dirty for savegame
				if (!(flags&STATUPDATEFLAG_LOAD_FROM_SAVEGAME))
				{
					statDesc.SetDirtySavegame(true);
				}

				if (!(flags&STATUPDATEFLAG_LOAD_FROM_SAVEGAME) && statDesc.GetTriggerEventValueChanged())
				{
					GetEventGlobalGroup()->Add(CEventStatChangedValue((int)statKey.GetHash()));
				}

				//Profile stat ready to be synched with ROS.
				if (statDesc.GetIsProfileStat() && statDesc.GetSynched() && !(flags&STATUPDATEFLAG_LOAD_FROM_SAVEGAME) && !StatsInterface::GetBlockSaveRequests())
				{
                    // Dirty the stat, and dirty the profile too based on the STATUPDATEFLAG_DIRTY_PROFILE flag
					pStat->SetDirty((flags&STATUPDATEFLAG_DIRTY_PROFILE) ? true : false);
				}

                //If we're modifying an SP profile stat (that isn't our SP profile stat timestamp), we need update our profile stat timestamp
                if (statDesc.GetIsProfileStat()
                    && !(flags&STATUPDATEFLAG_LOAD_FROM_SAVEGAME)
                    && statKey != STAT_SP_PROFILE_STAT_VERSION
                    && statDesc.GetGroup() >= PS_GRP_SP_START
                    && statDesc.GetGroup() <= PS_GRP_SP_END)
                {
                    StatsInterface::SetStatData(STAT_SP_PROFILE_STAT_VERSION, rlGetPosixTime(), STATUPDATEFLAG_ASSERTONLINESTATS);
                }
			}
		}

#if __ASSERT
		if ((flags&STATUPDATEFLAG_ASSERTONLINESTATS) && statDesc.GetIsOnlineData())
		{
			if( !NetworkInterface::IsLocalPlayerOnline() && !pStat->GetIsCodeStat() )
			{
				statDebugf3("Setting Stat %s online data when the player is signed off.", statIter.GetKeyName());
			}

			if (NetworkInterface::IsLocalPlayerOnline())
			{
				const u32 saveFileSlot = statDesc.GetCategory();
				int currentChar = StatsInterface::GetCurrentMultiplayerCharaterSlot();
				++currentChar;

				if (!(0 == saveFileSlot || MAX_STAT_SAVE_CATEGORY == saveFileSlot || saveFileSlot == (u32)currentChar || saveFileSlot == CStatsMgr::GetStatsDataMgr().GetSavesMgr().GetOverrideSaveSlotNumber()))
				{
					if(pStat->GetIsCodeStat())
					{
						scriptAssertf(0, "%s : CODER Stat %s has changed value but his data wont be saved in the savegame. Reason:\
										 Stat param \"SaveCategory\" has value \"%d\" but the current character savegame slot is \"%d\""
										 ,CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName(), saveFileSlot, currentChar);
					}
					else
					{
						scrThread::PrePrintStackTrace();
						scriptAssertf(0, "%s : SCRIPTER Stat %s has changed value but his data wont be saved in the savegame. Reason:\
										 Stat param \"SaveCategory\" has value \"%d\" but the current character savegame slot is \"%d\""
										 ,CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName(), saveFileSlot, currentChar);
					}
				}
			}
			else
			{
				scrThread::PrePrintStackTrace();
			}
		}
#endif // __ASSERT
	}

#if RSG_DURANGO
	events_durango::OnStatWrite(statKey, pData,sizeofData, flags);
#endif

	return result;
}

bool 
CStatsDataMgr::SetStatData(const StatId& keyHash, void* pData, const unsigned sizeofData, const u32 flags)
{
	bool result = false;

	if (!IsStatsDataValid())
		return result;

	CStatsDataMgr::StatsMap::Iterator statIter;
	if (CStatsMgr::GetStatsDataMgr().StatIterFind(keyHash, statIter))
	{
		DEV_ONLY( if (!statIter.GetKey().IsValid()) statWarningf("Stat key=\"%s : %s\" hash=\"%d : %d\" is NOT VALID", statIter.GetKey().GetName(), keyHash.GetName(), statIter.GetKey().GetHash(), keyHash.GetHash()); )

		if (statIter.GetKey().IsValid())
		{
			result = SetStatIterData(statIter, pData, sizeofData, flags);
		}
	}

	return result;
}

bool 
CStatsDataMgr::GetStatData(const StatId& keyHash, void* pData, const unsigned sizeofData) const
{
	DEV_ONLY(if (!IsKeyValid(keyHash)) statWarningf("Stat key=\"%s\" hash=\"%d\" is NOT VALID", keyHash.GetName(), keyHash.GetHash());)

	const sStatDataPtr* ppStat = GetStatPtr(keyHash);
	const sStatData*     pStat = ppStat && ppStat->KeyIsvalid() ? *ppStat : 0;
	if (statVerify(pStat))
	{
		bool validDataSize = (sizeofData == pStat->GetSizeOfData());

		if (pStat->GetType() == STAT_TYPE_USERID)
		{
			validDataSize = (sizeofData >= pStat->GetSizeOfData() || sizeofData == sizeof(u64));
		}

#if __ASSERT
		statAssertf(validDataSize, "Invalid buffer size=\"%d\" for stat key=\"%s\", type=\"%s\", size=\"%d\""
						,sizeofData
						,ppStat->GetKeyName()
						,pStat->GetTypeLabel()
						,pStat->GetSizeOfData());
#endif // __ASSERT

		if (validDataSize)
		{
			return pStat->GetData(pData, sizeofData);
		}
	}

	return false;
}

bool  
CStatsDataMgr::GetIsBaseType(const StatId& keyHash, const StatType type) const
{
	const sStatDataPtr* ppStat = GetStatPtr(keyHash);
	const sStatData*     pStat = ppStat && ppStat->KeyIsvalid() ? *ppStat : 0;

	if (pStat)
	{
		return pStat->GetIsBaseType (type);
	}

	return false;
}

u32
CStatsDataMgr::CheckForDirtyProfileStats(CProfileStats::DirtyCache& dirtyCache, const bool force, const bool online)
{
    //Important stats are stats with flush priority > RL_PROFILESTATS_MIN_PRIORITY
	u32 numImportantStatsDirty = 0;

	const unsigned curTime = fwTimer::GetTimeInMilliseconds_NonScaledClipped();

	if (s_DirtyStatsCount > 0 && (force || ((curTime - m_CheckForDirtyProfileStats) > CHECKDIRTYSTATS_INTERVAL)))
	{
        statWarningf("CheckForDirtyProfileStats - called.");

		m_CheckForDirtyProfileStats = curTime;

		//Lookout for any dirty stats
		StatsMap::Iterator iter = m_aStatsData.Begin();
		for (;iter != m_aStatsData.End(); ++iter)
		{
			sStatData* pStat = 0;
			if (iter.GetKey().IsValid())
			{
				pStat = *iter;
			}

			if (!pStat)
			{
				statWarningf("Stat %s - No stat pointer.", iter.GetKeyName());
				continue;
			}

			const sStatDescription& desc = pStat->GetDesc();

			//not a profile stat
			if (!desc.GetIsProfileStat())
				continue;

			//not Dirty
			if (!desc.GetDirty())
				continue;

			//Exceptions to the rule. These need to be marked as 
			//   dirty since the backend is still going to reset them
			if (desc.GetBlockProfileStatFlush())
			{
				statWarningf("Stat with key='%s' is Blocked to Profile Stats Flushes.", iter.GetKeyName());

				if (desc.GetDirty())
					pStat->GetDesc().SetDirty( false );

				continue;
			}

			//Single Player stats can not be flushed during the MP game..
			if (online && !desc.GetIsOnlineData() && !desc.GetAlwaysFlush())
			{
				// Unless it's a profile setting
				if(!desc.GetIsProfileSetting())
				{
					statWarningf("Single player Stat %s - Not valid to Flush in multiplayer.", iter.GetKeyName());
					continue;
				}
			}

			//Multiplayer stats can not be flushed during the SP game.
			if (!online && desc.GetIsOnlineData() && !desc.GetAlwaysFlush())
			{
				statWarningf("Multiplayer Stat %s - Not valid to Flush in Single player.", iter.GetKeyName());
				continue;
			}

			if (!desc.GetSynched())
			{
				statWarningf("Single player Stat %s - stat is not synched.", iter.GetKeyName());
				continue;
			}

			// Add this dirty stat to the dirty set
			rlProfileStatsValue dirtyStatValue;
			rlProfileDirtyStat dirtyStat;
			if (pStat->WriteOutProfileStat(&dirtyStatValue))
			{
				dirtyStat.Reset(iter.GetKey(), dirtyStatValue, pStat->GetDesc().GetFlushPriority());

#if !__FINAL
				if (iter.GetKey() == STAT_MP_PLAYING_TIME && dirtyStatValue.GetInt64() == 0)
					statAssertf(0, "STAT_MP_PLAYING_TIME == 0");
#endif // !__FINAL
			}
			else
			{
				if (desc.GetIsProfileStat() && desc.GetDirty())
				{
					pStat->GetDesc().SetDirty( false );
				}

				continue;
			}

			if (desc.GetFlushPriority() > RL_PROFILESTATS_MIN_PRIORITY)
			{
				numImportantStatsDirty += 1;
			}

			//Special case for our two SP version stats, which need the lowest/highest priority buckets
			bool addResult = false;
			if (Unlikely(iter.GetKey() == STAT_SP_PROFILE_STAT_VERSION))
			{
				//We must always flush the profile stat version, that way we know if profile stats are
				//newer. So use the highest priority bin
				addResult = dirtyCache.AddDirtyStat(dirtyStat, dirtyCache.GetMaxPriorityBin());
			}
			else if (Unlikely(iter.GetKey() == STAT_SP_SAVE_TIMESTAMP))
			{
				//We must only flush the savegame version if all other stats before it can also
				//be flushed, that way we know that profile stats has all stats from this savegame
				//version
				addResult = dirtyCache.AddDirtyStat(dirtyStat, dirtyCache.GetMinPriorityBin());
			}
			else
			{
				addResult = dirtyCache.AddDirtyStat(dirtyStat);
			}

			if (!addResult)
			{
				// Logging if we failed to fit this stat in the dirty set
				statWarningf("Stat %s - dirtyCache::AddDirtyStat() Failed", iter.GetKeyName());
			}
		}

        if (numImportantStatsDirty > 0)
		{
			statWarningf("CheckForDirtyProfileStats - Found < %d > important dirty profile stats.", numImportantStatsDirty);
		}
	}

    return numImportantStatsDirty;
}

u32 
CStatsDataMgr::GetDirtyStatCount()
{
	return s_DirtyStatsCount;
}

u32
CStatsDataMgr::GetDirtyProfileStatCount()
{
    return s_DirtyProfileStatsCount;
}

void 
CStatsDataMgr::HandleEventProfileStats(const rlProfileStatsEvent* evt)
{
	if (!rlProfileStats::IsInitialized())
		return;

	if (!statVerify(evt))
		return;

	switch (evt->GetId())
	{
	case RL_PROFILESTATS_EVENT_SYNCHRONIZE_STAT:
		{
			const rlProfileStatsSynchronizeStatEvent* synchEvt = static_cast< const rlProfileStatsSynchronizeStatEvent * >(evt);
			if(statVerify(synchEvt->m_Value))
			{
				sStatDataPtr* ppStat = GetStatPtr(STAT_ID(synchEvt->m_StatId));
				sStatData* pStat     = ppStat && ppStat->KeyIsvalid() ? *ppStat : 0;

				if(pStat)
				{
					if(!pStat->GetDesc().GetIsProfileStat()) 
					{
						BANK_ONLY( statErrorf("Synch Failed for stat=\"%s, %d\" - stat is not a profile stat", ppStat->GetKeyName(), (int)ppStat->GetKey()); )
					}
					else if(!synchEvt->m_Value)
					{
						BANK_ONLY( statErrorf("Synch Failed for stat=\"%s, %d\" - val is NULL", ppStat->GetKeyName(), (int)ppStat->GetKey()); )
					}

					bool flushStat = false;

					const sStatDescription& desc = pStat->GetDesc();

#if __BANK
					//If we're running a consistency check, verify our local value is either dirty or matches the backend value
					if (m_SyncConsistencyStatus.Pending())
					{
                        rlProfileStatsValue localValue;
                        pStat->WriteOutProfileStat(&localValue);
                        if (localValue != *synchEvt->m_Value)
                        {
                            //The stat we just synced doesn't match what was stored on the backend. Make sure that it either
                            //hasn't been synced yet, or is marked as dirty (i.e. we knew it didn't match)

						    //We can only do this on stats that we've actually synced with profile stats already. Since we don't
						    //actually keep track of that, just check whether or not CProfileStats thinks it's synced the group
						    //this stat is in
						    const CProfileStats::eSyncType syncType = desc.GetIsOnlineData() ? CProfileStats::PS_SYNC_MP : CProfileStats::PS_SYNC_SP;

						    if (CLiveManager::GetProfileStatsMgr().Synchronized(syncType) && !desc.GetDirty())
						    {
							    profileStatErrorf("Stat %s mismatches backend, but was marked as synched and isn't dirty", ppStat->GetKeyName());
						    }
                        }

						//Break so the consistency check doesn't actually affect synched or anything
						break;
					}
#endif// __BANK

					if (desc.GetServerAuthoritative())
					{
						//Tunable so we can disable this clear.
						if (Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("ENABLE_SERVER_AUTHORITATIVE_SP_SYNCHFLAG_CLEAR", 0xCC4ACF55), true))
						{
							//Clear synch flag for SP server authoritative stats
							//before reading the value. Always read in SP server 
							//Authoritative stats. GetServerAuthoritative.
							if (!desc.GetIsOnlineData() && desc.GetSynched())
							{
								profileStatDebugf("Clear Server Authoritative Synch Flag for SP stat \"%s\".", ppStat->GetKeyName());
								pStat->ResynchServerAuthoritative();
							}
						}

						if (pStat->ReadInProfileStat(synchEvt->m_Value, flushStat))
						{
							statAssert(desc.GetSynched());
							BANK_ONLY(char buff[48];)
							BANK_ONLY( profileStatDebugf("Synch Server Authoritative stat \"%s\" with Profile stats value, id=(\"%d\"), Value=\"%s\"", 
							ppStat->GetKeyName(), (int)ppStat->GetKey(), pStat->ValueToString(buff, 48)); )

							//Check for Admin award stats.
							StatsInterface::CheckForProfileAwardStat(ppStat);
						}
					}
					else
					{
						if(!pStat->ProfileStatMatches(synchEvt->m_Value))
						{
							flushStat = true;

							if (!pStat->GetIsBaseType(STAT_TYPE_PROFILE_SETTING) && pStat->GetDesc().GetGroup() == PS_GRP_SP_0 && NetworkInterface::IsNetworkOpen())
							{
								flushStat = false;
							}
						}
					}

					if (flushStat && desc.GetIsProfileStat() && desc.GetSynched())
					{
						//Set profile stat as Dirty, which means its current value needs to be written to the backend.
						pStat->GetDesc().SetDirty(true);

                        BANK_ONLY(char buff[48];)
						BANK_ONLY( profileStatDebugf("Synch stat \"%s\" with Profile stats value, id=(\"%d\"), Value=\"%s\"", ppStat->GetKeyName(), (int)ppStat->GetKey(), pStat->ValueToString(buff, 48)); )
					}
				}
				else
				{
					statWarningf("Server Profile stats differ from local stats. New Config needs to be uploaded");
					statWarningf("OnReadProfileStat stat with id=\"%d\" doesnt exist.", synchEvt->m_StatId);
				}
			}
		}
		break;

    case RL_PROFILESTATS_EVENT_SYNCHRONIZE_GROUP:
        {
            const rlProfileStatsSynchronizeGroupEvent* synchGroupEvt = static_cast< const rlProfileStatsSynchronizeGroupEvent * >(evt);

            BANK_ONLY( profileStatDebugf("Group %d was already synced with backend", synchGroupEvt->m_GroupId); )

            // Mark any stats in this group as synched with the profile
            const eProfileStatsGroups group = static_cast<eProfileStatsGroups>(synchGroupEvt->m_GroupId);

            StatsMap::Iterator iter = m_aStatsData.Begin();
            while (iter != m_aStatsData.End())
            {
                sStatData* pStat = NULL;
                if (iter.GetKey().IsValid())
                {
                    pStat = *iter;
                }

                if (pStat
                    && pStat->GetDesc().GetIsProfileStat()
                    && pStat->GetDesc().GetGroup() == group)
                {
                    // If this stat is authoritative from profile stats, then mark it as synched,
                    // since the group didn't need to be synced
                    if (pStat->GetDesc().GetServerAuthoritative())
                    {
                        pStat->GetDesc().SetSynched(true);
                    }
                }

                ++iter;
            }
        }
        break;

	case RL_PROFILESTATS_EVENT_GET_LOCAL_VALUE:
		{
			const rlProfileStatsGetLocalValueEvent* localValueEvt = static_cast< const rlProfileStatsGetLocalValueEvent * >(evt);
			if(statVerify(localValueEvt->m_Value))
			{
				sStatDataPtr* ppStat = GetStatPtr(STAT_ID(localValueEvt->m_StatId));
				sStatData* pStat     = ppStat && ppStat->KeyIsvalid() ? *ppStat : 0;
				if(pStat)
				{
					if (pStat->GetDesc().GetSynched())
					{
#if __BANK
						if (CLiveManager::GetProfileStatsMgr().PendingFlush())
						{
							char buff[48];
							profileStatDebugf("Write stat \"%s\"(\"%d\") to Profile stats server, Value=\"%s\"", ppStat->GetKeyName(), (int)ppStat->GetKey(), pStat->ValueToString(buff, 48));
						}
#endif // __BANK

						if(!localValueEvt->m_Value)
						{
							BANK_ONLY( statErrorf("Failed to write profile for stat=\"%s, %d\" - val is NULL", ppStat->GetKeyName(), (int)ppStat->GetKey()); )
						}
						else if(!pStat->GetDesc().GetIsProfileStat()) 
						{
							BANK_ONLY( statErrorf("Failed to write profile for stat=\"%s, %d\" - stat is not a profile stat", ppStat->GetKeyName(), (int)ppStat->GetKey()); )
						}

						pStat->WriteOutProfileStat(localValueEvt->m_Value);

#if __BLOCK_TAKEOVERS
						//Territories stats are blocked from being sent to the server.
						if (pStat->GetIsTerritoryStat())
						{
							BANK_ONLY( profileStatDebugf("Territory Actually Written value \"%s\" (\"%d\") Value = \"-1\"", ppStat->GetKeyName(), (int)ppStat->GetKey()); )
						}
#endif // __BLOCK_TAKEOVERS
					}
				}
				else
				{
					statWarningf("Server Profile stats differ from local stats. New Config needs to be uploaded");
					statWarningf("OnWriteProfileStats stat with id=\"%d\" doesnt exist.", localValueEvt->m_StatId);
				}
			}
		}
		break;

	case RL_PROFILESTATS_EVENT_FLUSH_STAT:
		{
			const rlProfileStatsFlushStatEvent* flushEvt = static_cast<const rlProfileStatsFlushStatEvent *>(evt);

#if __DEV
			//Hack because the game client doesnt know about this stat.
			static StatId s_saveTransfer("SERVER_TRANSFER_USED");
			if (flushEvt->m_StatId == (int)s_saveTransfer.GetHash())
				return;
#endif // __DEV

			sStatDataPtr *ppStat = GetStatPtr(STAT_ID(flushEvt->m_StatId));
			sStatData *pStat = ppStat && ppStat->KeyIsvalid() ? *ppStat : 0;

			// This event is triggered in response to us publishing the stat, so it should be safe to verify that
			// this stat is indeed a profile stat.
			if (statVerify(pStat && pStat->GetDesc().GetIsProfileStat()))
			{
				// When a stat is flushed, it should no longer be considered dirty
				pStat->GetDesc().SetDirty(false);

                BANK_ONLY(char buff[48];)
				BANK_ONLY(profileStatDebugf("Flush stat \"%s\", id=(\"%d\"), Value=\"%s\"", ppStat->GetKeyName(), (int)ppStat->GetKey(), pStat->ValueToString(buff, 48)); )
			}
		}
		break;
	}
}

void 
CStatsDataMgr::UpdateCommunityStatValues()
{
	statDebugf3("Update Community stats:");

	//Apply community stats data.
	const CCommunityStatsDataMgr& communityStats = m_CommunityStats.GetMetadata();
	for (const CCommunityStatsData& data : communityStats.m_Data)
	{
		OnSynchCommunityStats(data.m_StatId.GetHash(), data.m_Value);
	}
}

void 
CStatsDataMgr::OnSynchCommunityStats(const int statId, const float result)
{
	sStatDataPtr* ppStat = GetStatPtr(STAT_ID(statId));
	sStatData*     pStat = ppStat && ppStat->KeyIsvalid() ? *ppStat : 0;
	if (pStat && statVerify(pStat->GetDesc().GetIsCommunityStat()) && statVerify(!pStat->GetDesc().GetIsProfileStat()))
	{
		if(pStat->GetDesc().GetIsOnlineData())
		{
			pStat->GetDesc().SetSynched(true);
		}

		bool retHaschangedValue = false;
		float data = result;
		pStat->SetData(&data, sizeof(float), retHaschangedValue);
		statDebugf3("Synch Community stat \"%s\" value=\"%f\"", ppStat->GetKeyName(), data);
	} 
}



// --- Private Methods ----------------------------------------------------------

#if __DEV

void 
CStatsDataMgr::TestMaskedStats()
{
#if __TEST_MASKED_STAT

	//test u64 values.
	StatId statU64("TEST_MASKED_U64");

	StatsMap::iterator iter = GetStat(statU64);
	if (iter != StatIterEnd())
	{
		sStatData* stat = *iter;
		if (stat)
		{
			stat->SetUInt64(0x8080808080808080); //1000 0000 1000 0000 1000 0000 1000 0000 1000 0000 1000 0000 1000 0000 1000 0000

			//Going to pack the value 255 into 8 bytes
			StatsInterface::SetMaskedInt(statU64.GetHash(), 0x5C,  0, 8); //0101 1100 | 0x5C
			StatsInterface::SetMaskedInt(statU64.GetHash(), 0xFF,  8, 8); //1111 1111 | 0xFF
			StatsInterface::SetMaskedInt(statU64.GetHash(), 0x5C, 16, 8);
			StatsInterface::SetMaskedInt(statU64.GetHash(), 0xFF, 24, 8);
			StatsInterface::SetMaskedInt(statU64.GetHash(), 0x5C, 32, 8);
			StatsInterface::SetMaskedInt(statU64.GetHash(), 0xFF, 40, 8);
			StatsInterface::SetMaskedInt(statU64.GetHash(), 0x5C, 48, 8);
			StatsInterface::SetMaskedInt(statU64.GetHash(), 0xFF, 56, 8);

			int data = 0;
			StatsInterface::GetMaskedInt(statU64.GetHash(), data,  0, 8, 0);
			statAssert(data == 0x5C); data = 0;
			StatsInterface::GetMaskedInt(statU64.GetHash(), data,  8, 8, 0);
			statAssert(data == 0xFF); data = 0;
			StatsInterface::GetMaskedInt(statU64.GetHash(), data, 16, 8, 0);
			statAssert(data == 0x5C); data = 0;
			StatsInterface::GetMaskedInt(statU64.GetHash(), data, 24, 8, 0);
			statAssert(data == 0xFF); data = 0;
			StatsInterface::GetMaskedInt(statU64.GetHash(), data, 32, 8, 0);
			statAssert(data == 0x5C); data = 0;
			StatsInterface::GetMaskedInt(statU64.GetHash(), data, 40, 8, 0);
			statAssert(data == 0xFF); data = 0;
			StatsInterface::GetMaskedInt(statU64.GetHash(), data, 48, 8, 0);
			statAssert(data == 0x5C); data = 0;
			StatsInterface::GetMaskedInt(statU64.GetHash(), data, 56, 8, 0);
			statAssert(data == 0xFF); data = 0;
		}
	}

	//test int values.
	StatId statInt("TEST_MASKED_INT");
	iter = m_aStatsData.find(statInt);
	if (iter != StatIterEnd())
	{
		sStatData* stat = *iter;
		if (stat)
		{
			stat->SetInt(0x80808080); //1000 0000 1000 0000 1000 0000 1000 0000

			//Going to pack the value 255 into 8 bytes
			StatsInterface::SetMaskedInt(statInt.GetHash(), 0xFF,  0, 8); //0101 1100
			StatsInterface::SetMaskedInt(statInt.GetHash(), 0x5C,  8, 8); //1111 1111 
			StatsInterface::SetMaskedInt(statInt.GetHash(), 0x5C, 16, 8);
			StatsInterface::SetMaskedInt(statInt.GetHash(), 0xFF, 24, 8);

			int data = 0;
			StatsInterface::GetMaskedInt(statInt.GetHash(), data,  0, 8, 0);
			statAssert(data == 0xFF); data = 0;
			StatsInterface::GetMaskedInt(statInt.GetHash(), data,  8, 8, 0);
			statAssert(data == 0x5C); data = 0;
			StatsInterface::GetMaskedInt(statInt.GetHash(), data, 16, 8, 0);
			statAssert(data == 0x5C); data = 0;
			StatsInterface::GetMaskedInt(statInt.GetHash(), data, 24, 8, 0);
			statAssert(data == 0xFF); data = 0;
		}
	}

#endif // __TEST_MASKED_STAT
}

void
CStatsDataMgr::TestPosPack(float x, float y, float z, const char* statName)
{
	statDebugf1(" ");
	statDebugf1(" ");

	statDebugf1("Start TestPosPack");
	statDebugf1("... Testing pos=\"x=%f,  y=%f, z=%f\" for stat=\"%s\"", x, y, z, statName);

	sStatData*  statData = GetStat(StatId(statName));
	if (statData)
	{
		u64 prevValue = statData->GetUInt64();

		bool retHaschangedValue = false;

		u64 value = StatsInterface::PackPos(x, y, z);
		statData->SetData(&value, sizeof(u64), retHaschangedValue);
		statData->ToString();

		x = y = z = 0.0f;

		StatsInterface::UnPackPos(statData->GetUInt64(), x, y, z);
		statDebugf1("... Result pos=\"x=%f,  y=%f, z=%f\"", x, y, z);

		statData->SetData(&prevValue, sizeof(u64), retHaschangedValue);
	}

	statDebugf1("End TestPosPack");
};

void
CStatsDataMgr::TestDatePack(int year , int month , int day , int hour , int min , int sec , int mill, const char* statName)
{
	statDebugf1(" ");
	statDebugf1(" ");

	statDebugf1("Start TestDatePack");
	statDebugf1("... Testing Date= \"year=%d, month=%d, day=%d, hour=%d, min=%d, sec%d, mill=%d\" for stat=\"%s\"", year , month , day , hour , min , sec , mill, statName);

	{
		sStatData*  statData = GetStat(StatId(statName));

		if (statData)
		{
			u64 prevValue = statData->GetUInt64();

			bool retHaschangedValue = false;

			u64 value = StatsInterface::PackDate(year , month , day , hour , min , sec , mill);
			statData->SetData(&value, sizeof(u64), retHaschangedValue);
			statData->ToString();

			year = month = day = hour = min = sec = mill = 0;

			StatsInterface::UnPackDate(statData->GetUInt64(), year , month , day , hour , min , sec , mill);
			statData->ToString();

			statDebugf1("... Result Testing Date= \"year=%d, month=%d, day=%d, hour=%d, min=%d, sec=%d, mill=%d\"", year , month , day , hour , min , sec , mill);

			statData->SetData(&prevValue, sizeof(u64), retHaschangedValue);
		}
	}

	statDebugf1("End TestDatePack");
};

void 
CStatsDataMgr::ValidateStaticStatsIds() const
{
#if __ASSERT

	//Test User Id stats
	sStatDescription desc;
	sUserIdStatData statUserId("STAT_USER_ID", desc);

#if RLGAMERHANDLE_NP
	sUserIdStatData statUserIdNext("STAT_USER_ID_NEXT", desc);
	statUserId.m_NextUserId = &statUserIdNext;

	if (rlGamerHandle::IsUsingAccountIdAsKey())
	{
		statUserId.SetUserId("2535285330439972");
	}
	else
	{
		statUserId.SetUserId("rsnmax_whatwhat");
	}
#else
	statUserId.SetUserId("2535285330439972");
#endif

	char userStatBuffer[128];
	statUserId.GetUserId(userStatBuffer, 128);

#if RLGAMERHANDLE_NP
	if (rlGamerHandle::IsUsingAccountIdAsKey())
	{
		statAssert(stricmp(userStatBuffer, "2535285330439972") == 0);
	}
	else
	{
		statAssert(stricmp(userStatBuffer, "rsnmax_whatwhat") == 0);
	}
#else
	statAssert(stricmp(userStatBuffer, "2535285330439972") == 0);
#endif

	StatId_static* sid = StatId_static::sm_First;
	while(sid)
	{
		statAssertf(StatsInterface::IsKeyValid(sid->GetHash()), "CODER Stat id=\"%d\" with name=\"%s\" does not exist.", sid->GetHash(), sid->GetName());
		sid = sid->m_Next;
	}

	StatId_char* sCharId = StatId_char::sm_First;
	while(sCharId)
	{
		if (sCharId->m_Validate)
		{
			statAssertf(sCharId->m_Name, "Failed to valid stat because the name is NULL");
			if (sCharId->m_Name)
			{
				const bool isSpKeyValid = (StatsInterface::IsKeyValid(sCharId->GetHash(CHAR_MICHEAL)) && 
					StatsInterface::IsKeyValid(sCharId->GetHash(CHAR_FRANKLIN)) && 
					StatsInterface::IsKeyValid(sCharId->GetHash(CHAR_TREVOR)));

				bool isMpKeyValid = true;
				for (int i=0; i<MAX_NUM_MP_CHARS && isMpKeyValid; i++)
					isMpKeyValid &= StatsInterface::IsKeyValid(sCharId->GetHash(CHAR_MP0+i));

					statAssertf(isSpKeyValid || isMpKeyValid, "CODER Stat \"%s\" does not exist.", sCharId->m_Name);

				if (!isSpKeyValid && isMpKeyValid)
				{
					statDebugf1("CODER Stat \"%s\" is multiplayer only", sCharId->m_Name);
				}
				else if (!isMpKeyValid && isSpKeyValid)
				{
					statDebugf1("CODER Stat \"%s\" is single player only", sCharId->m_Name);
				}
			}
		}

		sCharId = sCharId->m_Next;
	}
#endif
}
#endif

void  
CStatsDataMgr::LoadDataXMLFile(const char* pFileName, bool bSinglePlayer)
{
	atHashString filenameHash(pFileName);
	if (!m_ProcessedFiles.Access(filenameHash))
	{
		if (statVerify(pFileName))
		{
			parTree* pTree = PARSER.LoadTree(pFileName, "xml");
			if(pTree)
			{
				statDebugf1("-----------------------------------------------------------------");
				statDebugf1("LoadDataXMLFile - FileName \"%s\"", pFileName);
				statDebugf1("-----------------------------------------------------------------");

				// --- Start Parsing ----------------------------------------------------------
				parTreeNode* pRootNode = pTree->GetRoot();
				if(pRootNode)
				{
					parTreeNode::ChildNodeIterator it = pRootNode->BeginChildren();
					for(; it != pRootNode->EndChildren(); ++it)
					{
						// --- All stats definitions ----------------------------------------------------------
						if(stricmp((*it)->GetElement().GetName(), "stats")    == 0)
						{
							LoadStatsData((*it), bSinglePlayer);
						}
					}
				}
				// --- End Parsing ----------------------------------------------------------

				delete pTree;

				//statDebugf2("-----------------------------------------------------------------");
			}
			else
			{
				statErrorf("LoadDataXMLFile - FileName \"%s\" - pTree NULL.", pFileName);
			}
		}
		else
		{
			statErrorf("LoadDataXMLFile - NULL FileName.");
		}

		m_ProcessedFiles.Insert(filenameHash,true);
		DEV_ONLY(if (m_aStatsData.GetCount() == 0) statWarningf("Single Player stats are empty");)
	}
	else
	{
		statDebugf1("LoadDataXMLFile - FileName \"%s\" Already Processed.", pFileName);
	}
}

void 
CStatsDataMgr::LoadStatsData(parTreeNode* pNode, bool bSinglePlayer)
{
	if(pNode)
	{
		parTreeNode::ChildNodeIterator it = pNode->BeginChildren();
		for(; it != pNode->EndChildren(); ++it)
		{
			strStreamingEngine::GetLoader().CallKeepAliveCallbackIfNecessary();
			if(stricmp((*it)->GetElement().GetName(), "stat") == 0)
			{
				Assertf((*it)->GetElement().FindAttribute("Name"),                 "Missing attribute \"Name\"");
				Assertf((*it)->GetElement().FindAttribute("Type"),                 "Missing attribute \"Type\"");
				Assertf((*it)->GetElement().FindAttribute("Comment"),              "Missing attribute \"Comment\"");

				if ((*it)->GetElement().FindAttribute("Name") && (*it)->GetElement().FindAttribute("Type"))
				{
					const char* pStatIdName     = (*it)->GetElement().FindAttributeAnyCase("Name")->GetStringValue();
					const char* pStatType       = (*it)->GetElement().FindAttributeAnyCase("Type")->GetStringValue();

					const char* pStatOwner = "coder";
					if ((*it)->GetElement().FindAttribute("Owner"))
					{
						pStatOwner = (*it)->GetElement().FindAttributeAnyCase("Owner")->GetStringValue();
					}

					if ((*it)->GetElement().FindAttribute("Flag"))
					{
						const char* pFlag = (*it)->GetElement().FindAttributeAnyCase("Flag")->GetStringValue();
						if (pFlag && istrlen(pFlag) > 0 && ATSTRINGHASH("FEATURE_GEN9_EXCLUSIVE", 0x7AFBE5A2) == atHashString(pFlag))
						{
#if !IS_GEN9_PLATFORM
							statDebugf1(" SKIP ADDDING GEN9 STAT '%s' FLAG 'FEATURE_GEN9_EXCLUSIVE' PRESENT.", pStatIdName);
							//Next Gen Stat. Skip.
							continue;
#endif //!IS_GEN9_PLATFORM
						}
					}

					const char* pStatMultiHash  = 0;
					if ((*it)->GetElement().FindAttribute("Multihash"))
					{
						pStatMultiHash = (*it)->GetElement().FindAttributeAnyCase("Multihash")->GetStringValue();
					}

					const char* pStatPacked  = 0;
					if ((*it)->GetElement().FindAttribute("packed"))
					{
						pStatPacked = (*it)->GetElement().FindAttributeAnyCase("packed")->GetStringValue();
					}

					const bool typeIsValid = (STAT_TYPE_NONE != GET_TYPE_FROM_NAME(pStatType));
					statAssertf(typeIsValid, "Invalid type for stat=\"%s\" type=\"%s\"", pStatIdName, pStatType);

					if (!statVerify(pStatIdName) || !statVerify(pStatType) || !typeIsValid)
						continue;

					/*
					##########################################
					##########################################

					          LOCAL USER STORAGE 

					##########################################
					##########################################
					*/

#if RSG_PC
					if ((*it)->GetElement().FindAttributeBoolValue("adminstat", false))
					{
						OUTPUT_ONLY( StatId statIgnored(pStatIdName); )
						gnetWarning("IGNORED ADMIN STAT - name='%s', hash='%d'.", pStatIdName, statIgnored.GetHash());
						continue;
					}
#endif // RSG_PC

					if ((*it)->GetElement().FindAttributeBoolValue("ProfileSettings", false))
					{
						statAssertf((*it)->GetElement().FindAttributeBoolValue("profile", false), 
									"Stat %s set as profile setting but its not set as a profile stat. fix that shit.", pStatIdName);
					}

					const bool isProfileSettings = (*it)->GetElement().FindAttribute("ProfileSettingId") ? true : false;
					const bool alwaysFlush = (*it)->GetElement().FindAttribute("alwaysFlush") ? true : false;

					sStatDescription desc((*it)->GetElement().FindAttributeBoolValue("online", false)
											,(*it)->GetElement().FindAttributeBoolValue("community", false)
											,(*it)->GetElement().FindAttributeBoolValue("profile", false)
											,(*it)->GetElement().FindAttributeBoolValue("ServerAuthoritative", false)
											,(u32)(*it)->GetElement().FindAttributeIntValue("ReadPriority", 0)
											,(u32)(*it)->GetElement().FindAttributeIntValue("FlushPriority", 0)
											,(u32)(*it)->GetElement().FindAttributeIntValue("SaveCategory", 0)
											,(*it)->GetElement().FindAttributeBoolValue("characterStat", false)
											,(*it)->GetElement().FindAttributeBoolValue("triggerEventValueChanged", false)
											,isProfileSettings
											,(*it)->GetElement().FindAttributeBoolValue("BlockReset", false)
											,(*it)->GetElement().FindAttributeBoolValue("BlockProfileStatFlush", false)
											,(*it)->GetElement().FindAttributeBoolValue("BlockSynchEventFlush", false)
											,(*it)->GetElement().FindAttributeBoolValue("NoSaveStat", false)
											, alwaysFlush);

#if __MPCLOUDSAVES_ONLY_ONEFILE
					if (0 < desc.GetCategory())
						statErrorf("All 'SaveCategory' must be set to '0'. Reverting xml stats '%s' setup file setting.", pStatIdName);
					desc.SetCategory(0);
#endif // __MPCLOUDSAVES_ONLY_ONEFILE

					sStatParser statparser;
					statparser.m_Name       = pStatIdName;
					statparser.m_Multihash  = pStatMultiHash;
					statparser.m_Owner      = pStatOwner;
					statparser.m_NumBitsPerValue = GET_NUMBER_OF_BITS_FROM_NAME(pStatPacked);
					statparser.m_Desc       = desc;
					statparser.m_Type       = GET_TYPE_FROM_NAME(pStatType);
					statparser.m_RawType    = pStatType;
					statparser.m_Node       = (*it);
					statparser.m_ProfileSettingId = (u16)(*it)->GetElement().FindAttributeIntValue("ProfileSettingId", 0xFFFF);
					//If the stat is tagged with metric flag
					statparser.m_MetricSettingId = (u32)(*it)->GetElement().FindAttributeIntValue("Flag1", 0, false);

					statparser.m_GroupIsSet = false;

#if !__FINAL
					if (statparser.m_Desc.GetIsOnlineData() && statparser.m_Desc.GetCategory() > 0 && !statparser.m_Desc.GetIsCharacterStat())
					{
						statWarningf("All 'SaveCategory' must be set to '0' for non character stats, stat named '%s' setup is wrong.", pStatIdName);
					}
#endif // !__FINAL

                    //If a group was specified in the config, then always use it. Otherwise this will be generated based on
                    //whether or not this is a character-specific stat, as well as the name of the stat.
                    if ((*it)->GetElement().FindAttribute("Group"))
                    {
                        statparser.m_GroupIsSet = true;
                        statparser.m_Desc.SetGroup(static_cast<eProfileStatsGroups>((*it)->GetElement().FindAttributeIntValue("Group", 0)));

#if __ASSERT
						eProfileStatsGroups grp = statparser.m_Desc.GetGroup();
						if (desc.GetIsOnlineData())
							statAssertf(grp<=PS_GRP_MP_END, "Invalid Group set in stat=%s, grp=%d", statparser.m_Name.c_str(), grp);
						else
							statAssertf(grp>=PS_GRP_SP_START && grp<=PS_GRP_SP_END, "Invalid Group set in stat=%s, grp=%d", statparser.m_Name.c_str(), grp);
#endif // __ASSERT
                    }

					//This is a stat unique per character
					if ((*it)->GetElement().FindAttributeBoolValue("characterStat", false))
					{
						// If this flag is set, we want to track a stat for each individual characters.
						RegisterNewStatForEachCharacter(statparser, bSinglePlayer);
					}
					else
					{
						// Check that the stat has correct size. Otherwise it trims its name silently, screwing anything relying on that stat
						statAssertf(strlen(statparser.m_Name.c_str()) < MAX_STAT_LABEL_SIZE, "Stat name %s is too long, please shorten it by %" I64FMT "d chars", statparser.m_Name.c_str(), strlen(statparser.m_Name.c_str()) - MAX_STAT_LABEL_SIZE + 1);
						RegisterNewStat(statparser);
					}
				}
			}
		}
	}
}

void
CStatsDataMgr::RegisterNewStat(sStatParser& statparser)
{
	statAssert(statparser.m_Name.c_str());
	statAssert(statparser.m_Owner.c_str());
	statAssert(statparser.m_Node);

	//-----------------------------------------------------------------------------------------------
	//                               Load Assert on assumptions
	statAssertf((stricmp(statparser.m_Owner.c_str(), "system") == 0) || (stricmp(statparser.m_Owner.c_str(), "coder") == 0) || (stricmp(statparser.m_Owner.c_str(), "script") == 0), 
						"Stat %s doesnt have a valid owner set values must be \"system\", \"coder\" or \"script\".", statparser.m_Name.c_str());
	if (statparser.m_Desc.GetIsProfileStat())         statAssertf(!statparser.m_Desc.GetIsCommunityStat(), "Failed for %s - (profileStat)", statparser.m_Name.c_str());
	if (statparser.m_Desc.GetDownloadPriority()  > 0) statAssertf(statparser.m_Desc.GetIsOnlineData(),       "Failed for %s - (statparser.m_Desc.GetDownloadPriority()  > 0)", statparser.m_Name.c_str());
	if (statparser.m_Desc.GetDownloadPriority()  > 0) statAssertf(statparser.m_Desc.GetIsProfileStat(),      "Failed for %s - (statparser.m_Desc.GetDownloadPriority()  > 0)", statparser.m_Name.c_str());
	if (statparser.m_Desc.GetFlushPriority() > 0)     statAssertf(statparser.m_Desc.GetIsProfileStat(),      "Failed for %s - (flushPriority  > 0)", statparser.m_Name.c_str());
	if (statparser.m_Desc.GetServerAuthoritative())   statAssertf(statparser.m_Desc.GetIsProfileStat(),      "Failed for %s - (GetServerAuthoritative())", statparser.m_Name.c_str());
	if (statparser.m_Desc.GetIsCommunityStat())       statAssertf(!statparser.m_Desc.GetIsProfileStat(),     "Failed for %s - (GetIsCommunityStat())", statparser.m_Name.c_str());
	if (statparser.m_Desc.GetIsCommunityStat())       statAssertf(!statparser.m_Desc.GetIsOnlineData(),      "Failed for %s - (GetIsCommunityStat())", statparser.m_Name.c_str());
	if (statparser.m_NumBitsPerValue > 0) statAssertf(statparser.m_Type == STAT_TYPE_PACKED, "Failed for %s - (Packed stat with wrong type - must be u64)", statparser.m_Name.c_str());
	if (statparser.m_ProfileSettingId != 0xFFFF) statAssertf(statparser.m_Type == STAT_TYPE_PROFILE_SETTING, "Failed for %s - (Profile setting stat with wrong type - must be profilesetting)", statparser.m_Name.c_str());
	//-----------------------------------------------------------------------------------------------

	sStatData* pStatSlot = 0;

#if RLGAMERHANDLE_NP
	sStatData* pStatNextSlot = 0;
#endif

    //Figure out our group based on our name, unless one was explicitly present in the config
    //For character stats, this gets set in RegisterNewStatForEachCharacter
    if (!statparser.m_Desc.GetIsCharacterStat()
        && !statparser.m_GroupIsSet)
    {
        if (statparser.m_Desc.GetIsOnlineData())
        {
            const char* pName = statparser.m_Name.c_str();
            if (statVerify(pName))
            {
                //MP0_, MP1_, MP2_, MP3_
			    if ('M' == pName[0] && 'P' == pName[1] && isdigit(pName[2]) && '_' == pName[3])
			    {
                    const int digit = atoi(&pName[2]);
                    Assert(digit >= 0 && digit <= 3);
				    statparser.m_Desc.SetGroup(static_cast<eProfileStatsGroups>(PS_GRP_MP_1 + digit));
			    }
                else
                {
                    statparser.m_Desc.SetGroup(PS_GRP_MP_6);
                }
            }
        }
        else
        {
            statparser.m_Desc.SetGroup(PS_GRP_SP_0);
        }
    }

	// Support obfuscated stat types - fake a type ID of the existing ID + MAX_STAT_TYPE
	u32 statTypeIncObfuscated = statparser.m_Type;
	if(stricmp(statparser.m_RawType, "short") == 0 ||
		stricmp(statparser.m_RawType, "double") == 0 ||
		stricmp(statparser.m_RawType, "x64") == 0 ||
		stricmp(statparser.m_RawType, "long") == 0)
	{
		statTypeIncObfuscated += MAX_STAT_TYPE;
#if __ASSERT
		++m_numObfuscatedStatTypes;
#endif // __ASSERT
	}
	switch (statTypeIncObfuscated)
	{
	case STAT_TYPE_TEXTLABEL: pStatSlot = rage_new sLabelStatData(statparser.m_Name.c_str(), statparser.m_Desc); break;
	case STAT_TYPE_INT:       pStatSlot = rage_new sIntStatData(statparser.m_Name.c_str(), statparser.m_Desc); break;
	case STAT_TYPE_FLOAT:     pStatSlot = rage_new sFloatStatData(statparser.m_Name.c_str(), statparser.m_Desc); break;
	case STAT_TYPE_BOOLEAN:   pStatSlot = rage_new sBoolStatData(statparser.m_Name.c_str(), statparser.m_Desc); break;
	case STAT_TYPE_UINT8:     pStatSlot = rage_new sUns8StatData(statparser.m_Name.c_str(), statparser.m_Desc); break;
	case STAT_TYPE_UINT16:    pStatSlot = rage_new sUns16StatData(statparser.m_Name.c_str(), statparser.m_Desc); break;
	case STAT_TYPE_UINT32:    pStatSlot = rage_new sUns32StatData(statparser.m_Name.c_str(), statparser.m_Desc); break;
	case STAT_TYPE_DATE:      pStatSlot = rage_new sDateStatData(statparser.m_Name.c_str(), statparser.m_Desc); break;
	case STAT_TYPE_POS:       pStatSlot = rage_new sPosStatData(statparser.m_Name.c_str(), statparser.m_Desc); break;
	case STAT_TYPE_STRING:    pStatSlot = rage_new sStringStatData(statparser.m_Name.c_str(), statparser.m_Desc); break;
	case STAT_TYPE_UINT64:    pStatSlot = rage_new sUns64StatData(statparser.m_Name.c_str(), statparser.m_Desc); break;
	case STAT_TYPE_PACKED:    pStatSlot = rage_new sPackedStatData(statparser.m_Name.c_str(), statparser.m_Desc, statparser.m_NumBitsPerValue); statAssert(pStatSlot && ((sPackedStatData*)pStatSlot)->NumberOfBitsIsValid()); break;
	case STAT_TYPE_USERID:    pStatSlot = rage_new sUserIdStatData(statparser.m_Name.c_str(), statparser.m_Desc); break;
	case STAT_TYPE_PROFILE_SETTING: pStatSlot = rage_new sProfileSettingStatData(statparser.m_Name.c_str(), statparser.m_Desc, statparser.m_ProfileSettingId); break;
	case STAT_TYPE_INT64:    pStatSlot = rage_new sInt64StatData(statparser.m_Name.c_str(), statparser.m_Desc); break;

	// Obfuscated types:
	case MAX_STAT_TYPE+STAT_TYPE_INT:       pStatSlot = rage_new sObfIntStatData(statparser.m_Name.c_str(), statparser.m_Desc); break;
	case MAX_STAT_TYPE+STAT_TYPE_FLOAT:     pStatSlot = rage_new sObfFloatStatData(statparser.m_Name.c_str(), statparser.m_Desc); break;
	case MAX_STAT_TYPE+STAT_TYPE_UINT64:    pStatSlot = rage_new sObfUns64StatData(statparser.m_Name.c_str(), statparser.m_Desc); break;
	case MAX_STAT_TYPE+STAT_TYPE_INT64:     pStatSlot = rage_new sObfInt64StatData(statparser.m_Name.c_str(), statparser.m_Desc); break;

	default: statAssertf(0, "Invalid stat %s with type \"%d\"", statparser.m_Name.c_str(), statparser.m_Type); break;
	}

	if (pStatSlot)
	{
		sStatDataPtr newStatPtr(pStatSlot);
		newStatPtr.SetKey(statparser.m_Name.c_str());

		if (statparser.m_Multihash.c_str())
		{
			char szName[MAX_STAT_LABEL_SIZE];
			snprintf(szName, MAX_STAT_LABEL_SIZE, "%s%s", statparser.m_Name.c_str(), statparser.m_Multihash.c_str());
			newStatPtr.SetKeyHash(atStringHash(statparser.m_Multihash.c_str(), atStringHash(statparser.m_Name.c_str())), szName);
			statAssert(newStatPtr.GetKeyName());
		}

#if RLGAMERHANDLE_NP
		//Setup the pointer to next stat because USER Id stat's have to be split in 2 u64 to hold
		//  up to 16 chars in PSN.
		if (statparser.m_Type == STAT_TYPE_USERID)
		{
			char szName[MAX_STAT_LABEL_SIZE];
			snprintf(szName, MAX_STAT_LABEL_SIZE, "%s_NEXT", statparser.m_Name.c_str());
			pStatNextSlot = rage_new sUserIdStatData(szName, statparser.m_Desc);

			if (statVerify(pStatNextSlot))
			{
				((sUserIdStatData*)pStatSlot)->m_NextUserId = static_cast<sUserIdStatData*>(pStatNextSlot);
			}
		}
#endif // RLGAMERHANDLE_NP

		if (statparser.m_Node->GetElement().FindAttribute("Default"))
		{
			switch (statparser.m_Type)
			{
			case STAT_TYPE_STRING:
				{
					char* text = const_cast<char*>(statparser.m_Node->GetElement().FindAttributeAnyCase("Default")->GetStringValue());
					if (text && 0 < strlen(text)) 
					{
						pStatSlot->SetString(text);				
					}
				}
				break;

			case STAT_TYPE_TEXTLABEL:
				{
					const char* text = statparser.m_Node->GetElement().FindAttributeAnyCase("Default")->GetStringValue();
					if (text && 0 < strlen(text) && TheText.DoesTextLabelExist(text)) 
					{
						pStatSlot->SetInt(atStringHash(text));				
					}
				}
				break;

			case STAT_TYPE_INT:
				{
					pStatSlot->SetInt(statparser.m_Node->GetElement().FindAttributeAnyCase("Default")->FindIntValue());				
				}
				break;

			case STAT_TYPE_FLOAT:
				{
					pStatSlot->SetFloat(statparser.m_Node->GetElement().FindAttributeAnyCase("Default")->FindFloatValue());
				}
				break;

			case STAT_TYPE_BOOLEAN:
				{
					bool bvalue = false;

					const char* defaultValue = statparser.m_Node->GetElement().FindAttributeAnyCase("Default")->GetStringValue();

					if (AssertVerify(defaultValue) && stricmp(defaultValue, "true") == 0)
						bvalue = true;

					pStatSlot->SetBoolean(bvalue);				
				}
				break;

			case STAT_TYPE_UINT8:
				{
					pStatSlot->SetUInt8((u8)(statparser.m_Node->GetElement().FindAttributeAnyCase("Default")->FindIntValue()));
				}
				break;

			case STAT_TYPE_UINT16:
				{
					pStatSlot->SetUInt16((u16)(statparser.m_Node->GetElement().FindAttributeAnyCase("Default")->FindIntValue()));
				}
				break;

			case STAT_TYPE_UINT32:
				{
					pStatSlot->SetUInt32((u32)(statparser.m_Node->GetElement().FindAttributeAnyCase("Default")->FindIntValue()));
				}
				break;

			case STAT_TYPE_INT64:
				{
					pStatSlot->SetInt64((s64)(statparser.m_Node->GetElement().FindAttributeAnyCase("Default")->FindIntValue()));
				}
				break;

			case STAT_TYPE_UINT64:
			case STAT_TYPE_DATE:
			case STAT_TYPE_POS:
			case STAT_TYPE_PACKED:
				{
					pStatSlot->SetUInt64((u64)(statparser.m_Node->GetElement().FindAttributeAnyCase("Default")->FindIntValue()));				
				}
				break;

			case STAT_TYPE_PROFILE_SETTING:
			case STAT_TYPE_USERID:
				break;

			default: 
				statAssertf(0, "Stat %s Invalid type=\"%s:%d\"", newStatPtr.GetKeyName(), pStatSlot->GetTypeLabel(), pStatSlot->GetType());
			}
		}

		NOTFINAL_ONLY( pStatSlot->SetOwner(stricmp(statparser.m_Owner, "coder") == 0); )

		//Silently fail to add the stat if already exists - due to 360/ps3 cRc's this can happen.
		if (statVerify(newStatPtr.GetKey().IsValid()) && !IsKeyValid(newStatPtr.GetKey()))
		{
			//Insert stat in StatsMap
			m_aStatsData.Insert(newStatPtr);

			//Register stat for metric
			m_StatsMetricsMgr.RegisterNewStatMetric(statparser, newStatPtr.GetKey());

#if RLGAMERHANDLE_NP
			//Insert second stat in StatsMap
			if (pStatNextSlot)
			{
				char szName[MAX_STAT_LABEL_SIZE];
				snprintf(szName, MAX_STAT_LABEL_SIZE, "%s_NEXT", statparser.m_Name.c_str());
				sStatDataPtr nextStatPtr(pStatNextSlot);
				nextStatPtr.SetKey(szName);
				m_aStatsData.Insert(nextStatPtr);
			}
#endif // RLGAMERHANDLE_NP
		}
		else
		{
			if (pStatSlot)
			{
				delete pStatSlot;
			}

#if RLGAMERHANDLE_NP
			if (pStatNextSlot)
			{
				delete pStatNextSlot;
			}
#endif // RLGAMERHANDLE_NP
		}
	}
}

bool
CStatsDataMgr::AddNewStat(const char* name, sStatDescription& desc, const StatType& type, const u16 profileSettingId)
{
	bool result = false;

	statAssert(name);
	if (!name)
		return result;

	StatId stat(name);
	statAssertf(!IsKeyValid(stat), "Stat with key %s already exists.", name);
	if (IsKeyValid(stat))
		return result;

	//-----------------------------------------------------------------------------------------------
	//                               Load Assert on assumptions

	if (desc.GetIsProfileStat())         statAssertf(!desc.GetIsCommunityStat(), "Failed for %s - (profileStat)", name);
	if (desc.GetDownloadPriority()  > 0) statAssertf( desc.GetIsOnlineData(),       "Failed for %s - (desc.GetDownloadPriority()  > 0)", name);
	if (desc.GetDownloadPriority()  > 0) statAssertf( desc.GetIsProfileStat(),      "Failed for %s - (desc.GetDownloadPriority()  > 0)", name);
	if (desc.GetFlushPriority() > 0)     statAssertf( desc.GetIsProfileStat(),      "Failed for %s - (flushPriority  > 0)", name);
	if (desc.GetServerAuthoritative())   statAssertf( desc.GetIsProfileStat(),      "Failed for %s - (GetServerAuthoritative())", name);
	if (desc.GetIsCommunityStat())       statAssertf(!desc.GetIsProfileStat(),     "Failed for %s - (GetIsCommunityStat())", name);
	if (desc.GetIsCommunityStat())       statAssertf(!desc.GetIsOnlineData(),      "Failed for %s - (GetIsCommunityStat())", name);
	if (desc.GetIsNoSaveStat())			 statAssertf( !desc.GetIsProfileStat(), "Failed for %s - (GetIsNoSaveStat()) - should not be a profile stat", name);

	//-----------------------------------------------------------------------------------------------

	sStatData* pStatSlot = 0;

#if RLGAMERHANDLE_NP
	sStatData* pStatNextSlot = 0;
#endif

	switch (type)
	{
	case STAT_TYPE_TEXTLABEL: pStatSlot = rage_new sLabelStatData(name, desc); break;
	case STAT_TYPE_INT:       pStatSlot = rage_new sIntStatData(name, desc); break;
	case STAT_TYPE_FLOAT:     pStatSlot = rage_new sFloatStatData(name, desc); break;
	case STAT_TYPE_BOOLEAN:   pStatSlot = rage_new sBoolStatData(name, desc); break;
	case STAT_TYPE_UINT8:     pStatSlot = rage_new sUns8StatData(name, desc); break;
	case STAT_TYPE_UINT16:    pStatSlot = rage_new sUns16StatData(name, desc); break;
	case STAT_TYPE_UINT32:    pStatSlot = rage_new sUns32StatData(name, desc); break;
	case STAT_TYPE_DATE:      pStatSlot = rage_new sDateStatData(name, desc); break;
	case STAT_TYPE_POS:       pStatSlot = rage_new sPosStatData(name, desc); break;
	case STAT_TYPE_STRING:    pStatSlot = rage_new sStringStatData(name, desc); break;
	case STAT_TYPE_UINT64:    pStatSlot = rage_new sUns64StatData(name, desc); break;
	case STAT_TYPE_PACKED:    pStatSlot = rage_new sPackedStatData(name, desc, 8); statAssert(pStatSlot && ((sPackedStatData*)pStatSlot)->NumberOfBitsIsValid()); break;
	case STAT_TYPE_USERID:    pStatSlot = rage_new sUserIdStatData(name, desc); break;
	case STAT_TYPE_PROFILE_SETTING: pStatSlot = rage_new sProfileSettingStatData(name, desc, profileSettingId); break;
	case STAT_TYPE_INT64:    pStatSlot = rage_new sInt64StatData(name, desc); break;

	default: statAssertf(0, "Invalid stat %s with type \"%d\"", name, type); break;
	}

	if (pStatSlot)
	{
		sStatDataPtr newStatPtr(pStatSlot);
		newStatPtr.SetKey(name);

#if RLGAMERHANDLE_NP
		//Setup the pointer to next stat because USER Id stat's have to be split in 2 u64 to hold
		//  up to 16 chars in PSN.
		if (type == STAT_TYPE_USERID)
		{
			char szName[MAX_STAT_LABEL_SIZE];
			snprintf(szName, MAX_STAT_LABEL_SIZE, "%s_NEXT", name);
			pStatNextSlot = rage_new sUserIdStatData(szName, desc);

			if (statVerify(pStatNextSlot))
			{
				((sUserIdStatData*)pStatSlot)->m_NextUserId = static_cast<sUserIdStatData*>(pStatNextSlot);
			}
		}
#endif

		//Silently fail to add the stat if already exists - due to 360/ps3 cRc's this can happen.
		if (statVerify(newStatPtr.GetKey().IsValid()) && !IsKeyValid(newStatPtr.GetKey()))
		{
			result = true;

			m_aStatsData.Insert(newStatPtr);

#if RLGAMERHANDLE_NP
			if (pStatNextSlot)
			{
				char szName[MAX_STAT_LABEL_SIZE];
				snprintf(szName, MAX_STAT_LABEL_SIZE, "%s_NEXT", name);
				sStatDataPtr nextStatPtr(pStatNextSlot);
				nextStatPtr.SetKey(szName);
				m_aStatsData.Insert(nextStatPtr);
			}
#endif
		}
		else
		{
			if (pStatSlot)
			{
				delete pStatSlot;
			}

#if RLGAMERHANDLE_NP
			if (pStatNextSlot)
			{
				delete pStatNextSlot;
			}
#endif
		}
	}

	return result;
}


void  
CStatsDataMgr::RegisterNewStatForEachCharacter(sStatParser& statparser, const bool singlePlayerFile)
{
	char szName[MAX_STAT_LABEL_SIZE];
	ConstString statName = statparser.m_Name;

	// Check that the stat has correct size. Otherwise it trims its name silently, screwing anything relying on that stat
	statAssertf(strlen(statparser.m_Name.c_str()) + 4 < MAX_STAT_LABEL_SIZE, "Stat name %s is too long, please shorten it by %" I64FMT "d chars", statparser.m_Name.c_str(), strlen(statparser.m_Name.c_str()) + 4 - MAX_STAT_LABEL_SIZE + 1);

	if (singlePlayerFile)
	{
		for (int i = 0; i < STAT_SP_CATEGORY_CHAR2; i++)
		{
			formatf(szName, MAX_STAT_LABEL_SIZE, "SP%d_%s", i, statName.c_str());
			statparser.m_Name = szName;

            //Set the group if it isn't already
            if (!statparser.m_GroupIsSet)
            {
                statparser.m_Desc.SetGroup(static_cast<eProfileStatsGroups>(PS_GRP_SP_1 + i));
            }

			RegisterNewStat(statparser);
		}
	}
	else
	{
		u32 category = statparser.m_Desc.GetCategory();

		for (int i=0; i<MAX_NUM_MP_CHARS; i++)
		{
			formatf(szName, MAX_STAT_LABEL_SIZE, "MP%d_%s", i, statName.c_str());
			statparser.m_Name = szName;

			//Set Savegame Category - sets the index of the savegame file for this stat.
			if (category > 0)
			{
				statparser.m_Desc.SetCategory(category+i);
			}

            //Set the group if it isn't already
            if (!statparser.m_GroupIsSet)
            {
                statparser.m_Desc.SetGroup(static_cast<eProfileStatsGroups>(PS_GRP_MP_1 + i));
            }

			RegisterNewStat(statparser);
		}
	}
}

void
CStatsDataMgr::SpewStats()
{
#if !__FINAL
	statDebugf3("-----------------------------------------------------------------");
	statDebugf3("Stats: ");
	statDebugf3("-----------------------------------------------------------------");

	StatsMap::Iterator iter = m_aStatsData.Begin();
	while ((iter != m_aStatsData.End()))
	{
		sStatData* pStat = 0;
		if (iter.GetKey().IsValid())
		{
			pStat = *iter;
		}

		if (statVerify(pStat))
		{
			statDebugf3("Stat - Id=\"%s:%d\"", iter.GetKeyName(), iter.GetKey().GetHash());
			pStat->ToString();
		}
		iter++;
	}

	statDebugf3("-----------------------------------------------------------------");
#endif // !__FINAL
}

CStatsDataMgr::StatsMap::Iterator 
CStatsDataMgr::StatIterBegin()
{
	return m_aStatsData.Begin();
}

CStatsDataMgr::StatsMap::Iterator 
CStatsDataMgr::StatIterEnd()
{
	return m_aStatsData.End();
}

bool
CStatsDataMgr::StatIterFind(const StatId& keyHash, CStatsDataMgr::StatsMap::Iterator& iter) 
{
	return m_aStatsData.GetIterator(keyHash, iter);
}

void  
CStatsDataMgr::StartNetworkMatch()
{
	m_CallbackEndReadRemoteProfileStats.Bind(this, &CStatsDataMgr::EndReadRemoteProfileStatsCallback);

	statAssert(0 == m_NumHighPriorityStats);
	statAssert(!m_HighPriorityStatIds);
	statAssert(0 == m_NumLowPriorityStats);
	statAssert(!m_LowPriorityStatIds);

	EndNetworkMatch();

	statDebugf1("Start Match");

	CStatsDataMgr::StatsMap::Iterator iter = StatIterBegin();
	while (iter != StatIterEnd())
	{
		sStatData* pStat = 0;
		if (iter.GetKey().IsValid())
		{
			pStat = *iter;
		}

		if (statVerify(pStat) && pStat->GetDesc().GetReadForRemotePlayers())
		{
			statAssert(pStat->GetDesc().GetIsOnlineData());
			statAssert(pStat->GetDesc().GetIsProfileStat());

			if (m_NumHighPriorityStats+m_NumLowPriorityStats < MAX_NUM_READ_REMOTE_GAMER_STATS)
			{
				if (PS_HIGH_PRIORITY == pStat->GetDesc().GetDownloadPriority())
				{
					m_NumHighPriorityStats++;
				}
				else if (statVerify(PS_LOW_PRIORITY == pStat->GetDesc().GetDownloadPriority()))
				{
					m_NumLowPriorityStats++;
				}
			}
		}

		iter++;
	}

	statDebugf1("...... Total High priority stats=%d", m_NumHighPriorityStats);
	if (0 < m_NumHighPriorityStats)
	{
		m_HighPriorityStatIds = rage_new int[m_NumHighPriorityStats];
	}

	statDebugf1("...... Total Low priority stats=%d", m_NumLowPriorityStats);
	if (0 < m_NumLowPriorityStats)
	{
		m_LowPriorityStatIds = rage_new int[m_NumLowPriorityStats];
	}

	unsigned indexHigh = 0;
	unsigned indexLow  = 0;
	iter = StatIterBegin();
	while (iter != StatIterEnd())
	{
		sStatData* pStat = 0;
		if (iter.GetKey().IsValid())
		{
			pStat = *iter;
		}

		if (statVerify(pStat) && pStat->GetDesc().GetReadForRemotePlayers())
		{
			statAssert(pStat->GetDesc().GetIsOnlineData());
			statAssert(pStat->GetDesc().GetIsProfileStat());

			if (indexHigh+indexLow < MAX_NUM_READ_REMOTE_GAMER_STATS)
			{
				if (PS_HIGH_PRIORITY == pStat->GetDesc().GetDownloadPriority())
				{
					statDebugf1("...... Add HIGH_PRIORITY stat=\"%s\" id=%s", iter.GetKeyName(), iter.GetKeyName());
					m_HighPriorityStatIds[indexHigh] = iter.GetKey();
					++indexHigh;
				}
				else if (statVerify(PS_LOW_PRIORITY == pStat->GetDesc().GetDownloadPriority()))
				{
					statDebugf1("...... Add LOW_PRIORITY stat=\"%s\" id=%s", iter.GetKeyName(), iter.GetKeyName());
					m_LowPriorityStatIds[indexLow] = iter.GetKey();
					++indexLow;
				}
			}
			else
			{
				statErrorf("...... Stat=\"%s\" is ignored from the profile stats that can be downloaded for remote players.", iter.GetKeyName());
			}
		}

		iter++;
	}

	//Setup profile stats for remote players
	for (int i=0; i<MAX_NUM_REMOTE_GAMERS; i++)
	{
		rlProfileStatsValue* highrecords = (0 < m_NumHighPriorityStats) ? rage_new rlProfileStatsValue[m_NumHighPriorityStats] : 0;
		m_HighPriorityRecords[i].ResetRecords(m_NumHighPriorityStats, highrecords);
		rlProfileStatsValue* lowrecords = (0 < m_NumLowPriorityStats) ? rage_new rlProfileStatsValue[m_NumLowPriorityStats] : 0;
		m_LowPriorityRecords[i].ResetRecords(m_NumLowPriorityStats, lowrecords);
	}

	m_ReadSessionProfileStats.Init(m_NumHighPriorityStats
									,m_HighPriorityStatIds
									,m_HighPriorityRecords
									,m_NumLowPriorityStats
									,m_LowPriorityStatIds
									,m_LowPriorityRecords
									,MAX_NUM_REMOTE_GAMERS
									,m_CallbackEndReadRemoteProfileStats);
}

void  
CStatsDataMgr::EndNetworkMatch()
{
	m_NumHighPriorityStats = 0;
	if (m_HighPriorityStatIds)
	{
		delete [] m_HighPriorityStatIds;
	}

	m_NumLowPriorityStats = 0;
	if (m_LowPriorityStatIds)
	{
		delete [] m_LowPriorityStatIds;
	}

	for (int i=0; i<MAX_NUM_REMOTE_GAMERS; i++)
	{
		m_HighPriorityRecords[i].ResetRecords(0, 0);
		m_LowPriorityRecords[i].ResetRecords(0, 0);
	}

	m_ReadSessionProfileStats.Shutdown();
}

void 
CStatsDataMgr::PlayerHasJoinedSession(const ActivePlayerIndex playerIndex)
{
	m_ReadSessionProfileStats.PlayerHasJoined(playerIndex);
}

void 
CStatsDataMgr::PlayerHasLeftSession(const ActivePlayerIndex playerIndex)
{
	m_ReadSessionProfileStats.PlayerHasLeft(playerIndex);
}

void  
CStatsDataMgr::EndReadRemoteProfileStatsCallback(const bool /*succeeded*/)
{
}

void  
CStatsDataMgr::SyncRemoteGamerProfileStats(const unsigned priority)
{
	if (!m_ReadSessionProfileStats.Pending())
	{
		m_ReadSessionProfileStats.ReadStats(priority);
	}
}

const rlProfileStatsValue* 
CStatsDataMgr::GetRemoteProfileStat(const rlGamerHandle& handle, const int statId) const 
{
	if (!NetworkInterface::IsGameInProgress())
		return NULL;

	bool statFound = false;
	unsigned valueColumn = 0;
	const CProfileStatsRecords* records = NULL;

	for (unsigned i=0; i<m_NumHighPriorityStats && !statFound; i++)
	{
		if (statId == m_HighPriorityStatIds[i])
		{
			records     = &m_HighPriorityRecords[0];
			valueColumn = i;
			statFound   = true;
		}
	}

	for (unsigned i=0; i<m_NumLowPriorityStats && !statFound; i++)
	{
		if (statId == m_LowPriorityStatIds[i])
		{
			records     = &m_LowPriorityRecords[0];
			valueColumn = i;
			statFound   = true;
		}
	}

	if (records)
	{
		for (int i=0; i<MAX_NUM_REMOTE_GAMERS; i++)
		{
			if (handle == records[i].GetGamerHandle() && statVerify(valueColumn < records[i].GetNumValues() && records[i].GetValues()))
			{
				return (&records[i].GetValue(valueColumn));
			}
		}
	}

	return NULL;
}



#if __BANK

static int  s_BankPriority = 1;
static int  s_Bankcharacter = 1;
static int  s_BankMpSlot    = 0;
static bool s_ToggleScriptChangesToTerritoryData = false;
static int  s_BankProfileStatGroup = 0;
static int	s_BankProfileStatsToDirty = 0;

static bkGroup* s_rGroup_af;
static bkGroup* s_rGroup_gl;
static bkGroup* s_rGroup_mr;
static bkGroup* s_rGroup_sw;
static bkGroup* s_rGroup_xz;

void
CStatsDataMgr::Bank_CreateWidgets()
{
	Assertf(GetStatObfuscationDisabled(),"You're creating stats widgets, but profile stats are obfuscated in the memory, use -disableStatObfuscation in your commandline if you wish to modify stats");
	bkBank* bank = BANKMGR.FindBank("Stats");
	if (bank)
	{
		bank->GetChild()->Destroy();

		bank->AddSeparator();

		char title[256];
		formatf(title, COUNTOF(title), "Data sizes total=\"%u\", profile=\"%d\".", Bank_CalculateAllDataSizes(), Bank_CalculateAllDataSizes(true));
		bank->AddTitle(title);

		bank->AddSeparator();

		m_SaveMgr.Bank_InitWidgets( *bank );

		bank->PushGroup("Online", false);
		{
			bank->PushGroup("Territories", false);
			{
				bank->AddButton("Clear all Territory data",  datCallback(MFA(CStatsDataMgr::Bank_ClearAllTerritoryData), (datBase*)this));
				bank->AddToggle("Debug Changes to Territory data from script", &s_ToggleScriptChangesToTerritoryData, datCallback(MFA(CStatsDataMgr::Bank_ToggleScriptChangesToTerritoryData), (datBase*)this));
			}
			bank->PopGroup();

			bank->AddSlider("Clear character", &s_Bankcharacter, 0, 4, 1);
			bank->AddButton("Clear all online character",  datCallback(MFA(CStatsDataMgr::Bank_ResetAllOnlineCharacterStats), (datBase*)this));

			bank->AddSeparator();
			bank->AddSlider("Profile stats Group",  &s_BankProfileStatGroup, 0, 255, 1);
			bank->AddButton("Clear Profile stats Group",  datCallback(MFA(CStatsDataMgr::Bank_ResetAllProfileStatsByGroup), (datBase*)this));

			bank->AddSeparator();
			bank->AddToggle("Enable Continuous Peer Reports", &CContextMenuHelper::m_bNonStopReports);

			bank->AddSeparator();
			bank->AddButton("Force Synchronize Profile stats",  datCallback(MFA(CStatsDataMgr::Bank_ForceSynchProfileStats), (datBase*)this));
            bank->AddButton("Run Profile Sync Consistency Check", datCallback(MFA(CStatsDataMgr::Bank_CheckProfileSyncConsistency), (datBase*)this));
			bank->AddButton("Flush Profile stats", datCallback(MFA(CStatsDataMgr::Bank_FlushProfileStats), (datBase*)this));
			bank->AddButton("Force Flush Profile stats", datCallback(MFA(CStatsDataMgr::Bank_ForceFlushProfileStats), (datBase*)this));
			bank->AddSlider("Command Value - Dirty some profile stats", &s_BankProfileStatsToDirty, 0, 6000, 1);
			bank->AddButton("Dirty some profile stats", datCallback(MFA(CStatsDataMgr::Bank_DirtySomeProfileStats), (datBase*)this));
			bank->AddSeparator();
			bank->AddSlider("Update Priority", &s_BankPriority, 0, 2, 1);
			bank->AddButton("Update players Profile stats", datCallback(MFA(CStatsDataMgr::Bank_ToggleUpdateProfileStats), (datBase*)this));
			bank->AddSeparator();
		}
		bank->PopGroup();

		bank->PushGroup("Debug Changes", false);
		{
			bank->AddToggle("Script changes to Stats", &m_ScriptSpew);
			bank->AddButton("Make GET commands Fail",  datCallback(MFA(CStatsDataMgr::Bank_ToggleTestStatGetFail), (datBase*)this));
			bank->AddButton("TestCommandLeaderboards2ReadRankPrediction",  datCallback(MFA(CStatsDataMgr::Bank_TestCommandLeaderboards2ReadRankPrediction), (datBase*)this));
			bank->AddButton("Enable/Disable Stats tracking",  datCallback(MFA(CStatsDataMgr::Bank_Enable_Disable_Stats_Tracking), (datBase*)this));
		}
		bank->PopGroup();

		bank->AddSeparator();

		bank->AddButton("Create Single Player Stats", datCallback(MFA(CStatsDataMgr::Bank_CreateSpWidgets), (datBase*)this));
		bank->AddSeparator();
		bank->AddSlider("Multiplayer character for mp stats", &s_BankMpSlot, 0, 4, 1);
		bank->AddButton("Create Multiplayer Player Stats", datCallback(MFA(CStatsDataMgr::Bank_CreateMpWidgets), (datBase*)this));
		bank->AddButton("Create MP Stats using Current Slot", datCallback(MFA(CStatsDataMgr::Bank_CreateMpWidgetsCurrentSlot), (datBase*)this));
		bank->AddButton("Display selected stats on screen", datCallback(MFA(CStatsDataMgr::GrcDebugDraw), (datBase*)this));
	}
}

void 
CStatsDataMgr::Bank_InitWidgets()
{
	m_ScriptSpew = false;
	m_TestFail   = false;

	m_bSpBankCreated = false;
	m_bMpBankCreated = false;

	bkBank* bank = BANKMGR.FindBank("Stats");
	if (!bank)
	{
		bank = &BANKMGR.CreateBank("Stats");
		bank->AddButton("Create stats widgets", datCallback(MFA(CStatsDataMgr::Bank_CreateWidgets), (datBase*)this));
	}
}

void CStatsDataMgr::Bank_CreateSpWidgets()
{
	m_TestFail   = false;

	bkBank* bank = BANKMGR.FindBank("Stats");

	if (bank && m_BkGroup)
	{
		bank->DeleteGroup(*m_BkGroup);
		m_BkGroup = 0;
		m_bSpBankCreated = false;
	}

	if ((!m_bSpBankCreated) && bank && !m_BkGroup)
	{
		m_bSpBankCreated = true;

		m_BkGroup = bank->PushGroup("Data Manager", false);
		{
			char namecmp0[MAX_STAT_LABEL_SIZE];
			char namecmp1[MAX_STAT_LABEL_SIZE];

			//Single Player Stats
			bank->PushGroup("SinglePlayer", false);
			{
				bank->PushGroup("System", false);
				Bank_AddStatsToGroup(bank, "_");
				bank->PopGroup();

				bank->PushGroup("SP0", false);
				Bank_AddStatsToGroup(bank, "SP0");
				bank->PopGroup();

				bank->PushGroup("SP1", false);
				Bank_AddStatsToGroup(bank, "SP1");
				bank->PopGroup();

				bank->PushGroup("SP2", false);
				Bank_AddStatsToGroup(bank, "SP2");
				bank->PopGroup();

				bank->PushGroup("APP", false);
				Bank_AddStatsToGroup(bank, "APP");
				bank->PopGroup();

				bank->PushGroup("SM", false);
				Bank_AddStatsToGroup(bank, "SM");
				bank->PopGroup();

				bank->PushGroup("MS", false);
				Bank_AddStatsToGroup(bank, "MS");
				bank->PopGroup();

				bank->PushGroup("Other", false);
				{
					StatsMap::Iterator iter = CStatsMgr::GetStatsDataMgr().StatIterBegin();
					while ((iter != CStatsMgr::GetStatsDataMgr().StatIterEnd()))
					{
						sStatDataPtr* ppStat = *iter;
						sStatData*     pStat = ppStat && ppStat->KeyIsvalid() ? *ppStat : 0;

						if (pStat && !pStat->GetDesc().GetIsOnlineData())
						{
							sysMemSet(namecmp0, 0, MAX_STAT_LABEL_SIZE);
							formatf(namecmp0, 4, iter.GetKeyName());

							sysMemSet(namecmp1, 0, MAX_STAT_LABEL_SIZE);
							formatf(namecmp1, 3, iter.GetKeyName());

							if ('_' != namecmp0[0]
							&& (stricmp(namecmp0, "SP0") != 0) 
								&& (stricmp(namecmp0, "SP1") != 0)
								&& (stricmp(namecmp0, "SP2") != 0)
								&& (stricmp(namecmp0, "APP") != 0)
								&& (stricmp(namecmp1, "SM") != 0)
								&& (stricmp(namecmp1, "MS") != 0))
							{
								bank->PushGroup(iter.GetKeyName(), false);
								{
									bank->AddToggle("Debug Changes", &pStat->m_ShowChanges, NullCB, NULL);
									bank->AddToggle("Show on screen", &pStat->m_ShowOnScreen, datCallback(MFA1(CStatsDataMgr::AddToDebugDraw), (datBase*)this, (CallbackData)pStat));
									Bank_AddStatWidgetValue(bank, ppStat);
								}
								bank->PopGroup();
							}
						}
						iter++;
					}
				}
				bank->PopGroup();
			}
			bank->PopGroup();
		}
		bank->PopGroup();
	}
}

void CStatsDataMgr::Bank_CreateMpWidgetsCurrentSlot()
{
	s_BankMpSlot = StatsInterface::GetCurrentMultiplayerCharaterSlot();
	Bank_CreateMpWidgets();
}

void CStatsDataMgr::Bank_CreateMpWidgets()
{
	m_TestFail   = false;

	bkBank* bank = BANKMGR.FindBank("Stats");

	if (bank && m_BkGroup)
	{
		bank->DeleteGroup(*m_BkGroup);
		m_BkGroup = 0;
		m_bMpBankCreated = false;
	}

	if ((!m_bMpBankCreated) && bank && !m_BkGroup)
	{
		s_rGroup_af = NULL;
		s_rGroup_gl = NULL;
		s_rGroup_mr = NULL;
		s_rGroup_sw = NULL;
		s_rGroup_xz = NULL;

		m_bMpBankCreated = true;

		m_BkGroup = bank->PushGroup("Data Manager", false);
		{
			char namecmp0[MAX_STAT_LABEL_SIZE];
			char namecmp1[MAX_STAT_LABEL_SIZE];

			//Multiplayer Player Stats
			bank->PushGroup("Multiplayer", false);
			{
				if (0 == s_BankMpSlot)
				{
					bank->PushGroup("MP0", false);
					s_rGroup_af = bank->PushGroup("a-f"); bank->PopGroup();
					s_rGroup_gl = bank->PushGroup("g-l"); bank->PopGroup();
					s_rGroup_mr = bank->PushGroup("m-r"); bank->PopGroup();
					s_rGroup_sw = bank->PushGroup("s-w"); bank->PopGroup();
					s_rGroup_xz = bank->PushGroup("x-z"); bank->PopGroup();
					bank->PopGroup();
				}
				else if (1 == s_BankMpSlot)
				{
					bank->PushGroup("MP1", false);
					s_rGroup_af = bank->PushGroup("a-f"); bank->PopGroup();
					s_rGroup_gl = bank->PushGroup("g-l"); bank->PopGroup();
					s_rGroup_mr = bank->PushGroup("m-r"); bank->PopGroup();
					s_rGroup_sw = bank->PushGroup("s-w"); bank->PopGroup();
					s_rGroup_xz = bank->PushGroup("x-z"); bank->PopGroup();
					bank->PopGroup();
				}
				else if (2 == s_BankMpSlot)
				{
					bank->PushGroup("MP2", false);
					s_rGroup_af = bank->PushGroup("a-f"); bank->PopGroup();
					s_rGroup_gl = bank->PushGroup("g-l"); bank->PopGroup();
					s_rGroup_mr = bank->PushGroup("m-r"); bank->PopGroup();
					s_rGroup_sw = bank->PushGroup("s-w"); bank->PopGroup();
					s_rGroup_xz = bank->PushGroup("x-z"); bank->PopGroup();
					bank->PopGroup();
				}
				else if (3 == s_BankMpSlot)
				{
					bank->PushGroup("MP3", false);
					s_rGroup_af = bank->PushGroup("a-f"); bank->PopGroup();
					s_rGroup_gl = bank->PushGroup("g-l"); bank->PopGroup();
					s_rGroup_mr = bank->PushGroup("m-r"); bank->PopGroup();
					s_rGroup_sw = bank->PushGroup("s-w"); bank->PopGroup();
					s_rGroup_xz = bank->PushGroup("x-z"); bank->PopGroup();
					bank->PopGroup();
				}
				else if (4 == s_BankMpSlot)
				{
					bank->PushGroup("MP4", false);
					s_rGroup_af = bank->PushGroup("a-f"); bank->PopGroup();
					s_rGroup_gl = bank->PushGroup("g-l"); bank->PopGroup();
					s_rGroup_mr = bank->PushGroup("m-r"); bank->PopGroup();
					s_rGroup_sw = bank->PushGroup("s-w"); bank->PopGroup();
					s_rGroup_xz = bank->PushGroup("x-z"); bank->PopGroup();
					bank->PopGroup();
				}

				bank->PushGroup("MPPLY", false);
				Bank_AddStatsToGroup(bank, "MPPLY");
				bank->PopGroup();

				bank->PushGroup("MPGEN", false);
				Bank_AddStatsToGroup(bank, "MPGEN");
				bank->PopGroup();

				bank->PushGroup("Other", false);
				{
					StatsMap::Iterator iter = CStatsMgr::GetStatsDataMgr().StatIterBegin();
					while ((iter != CStatsMgr::GetStatsDataMgr().StatIterEnd()))
					{
						sStatData* pStat = 0;
						if (iter.GetKey().IsValid())
						{
							pStat = *iter;
						}

						if (pStat && pStat->GetDesc().GetIsOnlineData())
						{
							sysMemSet(namecmp0, 0, MAX_STAT_LABEL_SIZE);
							formatf(namecmp0, 4, iter.GetKeyName());

							sysMemSet(namecmp1, 0, MAX_STAT_LABEL_SIZE);
							formatf(namecmp1, 6, iter.GetKeyName());

							if ((stricmp(namecmp0, "MP0") != 0) 
								&& (stricmp(namecmp0, "MP1") != 0)
								&& (stricmp(namecmp0, "MP2") != 0)
								&& (stricmp(namecmp0, "MP3") != 0)
								&& (stricmp(namecmp0, "MP4") != 0)
								&& (stricmp(namecmp1, "MPPLY") != 0)
								&& (stricmp(namecmp1, "MPGEN") != 0))
							{
								bank->PushGroup(iter.GetKeyName(), false);
								{
									bank->AddToggle("Debug Changes", &pStat->m_ShowChanges, NullCB, NULL);
									bank->AddToggle("Show on screen", &pStat->m_ShowOnScreen, datCallback(MFA1(CStatsDataMgr::AddToDebugDraw), (datBase*)this, (CallbackData)pStat));
									Bank_AddStatWidgetValue(bank, *iter);
								}
								bank->PopGroup();
							}
						}
						iter++;
					}
				}
				bank->PopGroup();
			}
			bank->PopGroup();
		}
		bank->PopGroup();

		if (statVerify(s_rGroup_af) 
			&& statVerify(s_rGroup_gl) 
			&& statVerify(s_rGroup_mr) 
			&& statVerify(s_rGroup_sw) 
			&& statVerify(s_rGroup_xz))
		{
			if (0 == s_BankMpSlot)
				Bank_AddStatsToGroupMP(bank, "MP0");
			else if (1 == s_BankMpSlot)
				Bank_AddStatsToGroupMP(bank, "MP1");
			else if (2 == s_BankMpSlot)
				Bank_AddStatsToGroupMP(bank, "MP2");
			else if (3 == s_BankMpSlot)
				Bank_AddStatsToGroupMP(bank, "MP3");
			else if (4 == s_BankMpSlot)
				Bank_AddStatsToGroupMP(bank, "MP4");
		}
	}
}

void
CStatsDataMgr::Bank_AddStatWidgetValue(bkBank* bank, sStatDataPtr* ppStat)
{
	sStatData* pStat = ppStat ? *ppStat : 0;
	
	if (bank && pStat && !pStat->IsObfuscated())
	{
		if (0 < pStat->GetType() && pStat->GetType() < NUMBER_OF_STATS_TYPE_IDS)
		{
			char title[256];
			formatf(title, COUNTOF(title), "Type \"%s\"", sStatsTypesNames[pStat->GetType()].m_Name);
			bank->AddTitle(title);
		}

		if (pStat->GetIsBaseType(STAT_TYPE_INT))
		{
			bank->AddSlider("Value", &((sIntStatData*)pStat)->m_Value, MIN_INT32, MAX_INT32, 1, datCallback(MFA1(CStatsDataMgr::Bank_SetStatDirty), (datBase*)this, (CallbackData)ppStat));
		}
		else if (pStat->GetIsBaseType(STAT_TYPE_TEXTLABEL))
		{
			bank->AddText("Value", ((sLabelStatData*)pStat)->m_Label, MAX_STAT_LABEL_SIZE, true, datCallback(MFA1(CStatsDataMgr::Bank_SetStatDirty), (datBase*)this, (CallbackData)ppStat));
		}
		else if (pStat->GetIsBaseType(STAT_TYPE_STRING))
		{
			bank->AddText("Value", ((sStringStatData*)pStat)->m_Value, MAX_STAT_STRING_SIZE, true, datCallback(MFA1(CStatsDataMgr::Bank_SetStatDirty), (datBase*)this, (CallbackData)ppStat));
		}
		else if (pStat->GetIsBaseType(STAT_TYPE_FLOAT))
		{
			bank->AddSlider("Value", &((sFloatStatData*)pStat)->m_Value, -FLT_MAX / 1000.0f, +FLT_MAX / 1000.0f, 1, datCallback(MFA1(CStatsDataMgr::Bank_SetStatDirty), (datBase*)this, (CallbackData)ppStat));
		}
		else if (pStat->GetIsBaseType(STAT_TYPE_BOOLEAN))
		{
			bank->AddToggle("Value", &((sBoolStatData*)pStat)->m_Value, datCallback(MFA1(CStatsDataMgr::Bank_SetStatDirty), (datBase*)this, (CallbackData)ppStat));
		}
		else if (pStat->GetIsBaseType(STAT_TYPE_UINT8))
		{
			bank->AddSlider("Value", &((sUns8StatData*)pStat)->m_Value, 0, MAX_UINT8, 1, datCallback(MFA1(CStatsDataMgr::Bank_SetStatDirty), (datBase*)this, (CallbackData)ppStat));
		}
		else if (pStat->GetIsBaseType(STAT_TYPE_UINT16))
		{
			bank->AddSlider("Value", &((sUns16StatData*)pStat)->m_Value, 0, MAX_UINT16, 1, datCallback(MFA1(CStatsDataMgr::Bank_SetStatDirty), (datBase*)this, (CallbackData)ppStat));
		}
		else if (pStat->GetIsBaseType(STAT_TYPE_UINT32))
		{
			bank->AddSlider("Value", &((sUns32StatData*)pStat)->m_Value, 0, MAX_UINT32, 1, datCallback(MFA1(CStatsDataMgr::Bank_SetStatDirty), (datBase*)this, (CallbackData)ppStat));
		}
		if (pStat->GetIsBaseType(STAT_TYPE_INT64))
		{
			bank->AddSlider("Value", (int*)&((sInt64StatData*)pStat)->m_Value, MIN_INT32, MAX_INT32, 1, datCallback(MFA1(CStatsDataMgr::Bank_SetStatDirty), (datBase*)this, (CallbackData)ppStat));
		}
		else if (pStat->GetIsBaseType(STAT_TYPE_DATE))
		{
			bank->AddSlider("Year", &((sDateStatData*)pStat)->m_Date.m_year, MIN_INT32, MAX_INT32, 0, datCallback(MFA1(CStatsDataMgr::Bank_SetStatDirty), (datBase*)this, (CallbackData)ppStat));
			bank->AddSlider("Month", &((sDateStatData*)pStat)->m_Date.m_month, MIN_INT32, MAX_UINT32, 0, datCallback(MFA1(CStatsDataMgr::Bank_SetStatDirty), (datBase*)this, (CallbackData)ppStat));
			bank->AddSlider("Day", &((sDateStatData*)pStat)->m_Date.m_day, MIN_INT32, MAX_UINT32, 0, datCallback(MFA1(CStatsDataMgr::Bank_SetStatDirty), (datBase*)this, (CallbackData)ppStat));
			bank->AddSlider("Hour", &((sDateStatData*)pStat)->m_Date.m_hour, MIN_INT32, MAX_UINT32, 0, datCallback(MFA1(CStatsDataMgr::Bank_SetStatDirty), (datBase*)this, (CallbackData)ppStat));
			bank->AddSlider("Minutes", &((sDateStatData*)pStat)->m_Date.m_minutes, MIN_INT32, MAX_UINT32, 0, datCallback(MFA1(CStatsDataMgr::Bank_SetStatDirty), (datBase*)this, (CallbackData)ppStat));
			bank->AddSlider("Seconds", &((sDateStatData*)pStat)->m_Date.m_seconds, MIN_INT32, MAX_UINT32, 0, datCallback(MFA1(CStatsDataMgr::Bank_SetStatDirty), (datBase*)this, (CallbackData)ppStat));
			bank->AddSlider("Milliseconds", &((sDateStatData*)pStat)->m_Date.m_milliseconds, MIN_INT32, MAX_UINT32, 0, datCallback(MFA1(CStatsDataMgr::Bank_SetStatDirty), (datBase*)this, (CallbackData)ppStat));
		}
		else if (pStat->GetIsBaseType(STAT_TYPE_POS))
		{
			bank->AddVector("Value", &((sPosStatData*)pStat)->m_Position, -FLT_MAX / 1000.0f, +FLT_MAX / 1000.0f, 0.0f, datCallback(MFA1(CStatsDataMgr::Bank_SetStatDirty), (datBase*)this, (CallbackData)ppStat));
		}
		else if (pStat->GetIsBaseType(STAT_TYPE_UINT64))
		{
			bank->AddSlider("Value Lo", &((sUns64StatData*)pStat)->m_ValueLo, 0, MAX_UINT32, 1, datCallback(MFA1(CStatsDataMgr::Bank_SetStatDirty), (datBase*)this, (CallbackData)ppStat));
			bank->AddSlider("Value Hi", &((sUns64StatData*)pStat)->m_ValueHi, 0, MAX_UINT32, 1, datCallback(MFA1(CStatsDataMgr::Bank_SetStatDirty), (datBase*)this, (CallbackData)ppStat));
		}
		else if (pStat->GetIsBaseType(STAT_TYPE_PACKED))
		{
			bank->AddText("Value", ((sPackedStatData*)pStat)->m_WidgetValue, sPackedStatData::WIDGET_STRING_SIZE, true, datCallback(MFA1(CStatsDataMgr::Bank_SetStatDirty), (datBase*)this, (CallbackData)ppStat));
			bank->AddSlider("Number Of Bits", &((sPackedStatData*)pStat)->m_NumberOfBits, MIN_INT32, MAX_INT32, 1, datCallback(MFA1(CStatsDataMgr::Bank_SetStatDirty), (datBase*)this, (CallbackData)ppStat));
		}
		else if (pStat->GetIsBaseType(STAT_TYPE_USERID))
		{
			bank->AddText("Value", ((sUserIdStatData*)pStat)->m_WidgetValue, sUserIdStatData::WIDGET_STRING_SIZE, true, datCallback(MFA1(CStatsDataMgr::Bank_SetStatDirty), (datBase*)this, (CallbackData)ppStat));
		}
	}
}

void  
CStatsDataMgr::Bank_AddStatToHisGroup(bkBank* bank, StatsMap::Iterator& iter, sStatData* pStat)
{
	if (statVerify(pStat) && statVerify(pStat))
	{
		const char* name = iter.GetKeyName();
		char ch = name[4];

		if( (ch >= 'A' && ch <= 'F') || (ch >= 'a' && ch <= 'f') )
			bank->SetCurrentGroup( *s_rGroup_af );
		else if( (ch >= 'G' && ch <= 'L') || (ch >= 'g' && ch <= 'l') )
			bank->SetCurrentGroup( *s_rGroup_gl );
		else if( (ch >= 'M' && ch <= 'R') || (ch >= 'm' && ch <= 'r') )
			bank->SetCurrentGroup( *s_rGroup_mr );
		else if( (ch >= 'S' && ch <= 'W') || (ch >= 's' && ch <= 'w') )
			bank->SetCurrentGroup( *s_rGroup_sw );
		else
			bank->SetCurrentGroup( *s_rGroup_xz );

		{
			bank->PushGroup(iter.GetKeyName(), false);
			{
				bank->AddToggle("Debug Changes", &pStat->m_ShowChanges, NullCB, NULL);
				bank->AddToggle("Show on screen", &pStat->m_ShowOnScreen, datCallback(MFA1(CStatsDataMgr::AddToDebugDraw), (datBase*)this, (CallbackData)pStat));
				Bank_AddStatWidgetValue(bank, *iter);
			}
			bank->PopGroup();
		}

		if( (ch >= 'A' && ch <= 'F') || (ch >= 'a' && ch <= 'f') )
			bank->UnSetCurrentGroup( *s_rGroup_af );
		else if( (ch >= 'G' && ch <= 'L') || (ch >= 'g' && ch <= 'l') )
			bank->UnSetCurrentGroup( *s_rGroup_gl );
		else if( (ch >= 'M' && ch <= 'R') || (ch >= 'm' && ch <= 'r') )
			bank->UnSetCurrentGroup( *s_rGroup_mr );
		else if( (ch >= 'S' && ch <= 'W') || (ch >= 's' && ch <= 'w') )
			bank->UnSetCurrentGroup( *s_rGroup_sw );
		else
			bank->UnSetCurrentGroup( *s_rGroup_xz );
	}
}

void
CStatsDataMgr::Bank_AddStatsToGroup(bkBank* bank, const char* name)
{
	if (statVerifyf(bank, "Invalid Bank Specified!") && statVerifyf(name, "Invalid stat name specified!"))
	{
		unsigned sizeofname = ustrlen(name);
		sizeofname++;

		char namecmp[MAX_STAT_LABEL_SIZE];

		StatsMap::Iterator iter = CStatsMgr::GetStatsDataMgr().StatIterBegin();
		while ((iter != CStatsMgr::GetStatsDataMgr().StatIterEnd()))
		{
			sStatData* pStat = 0;
			if (iter.GetKey().IsValid())
			{
				pStat = *iter;
			}

			if (pStat)
			{
				sysMemSet(namecmp, 0, MAX_STAT_LABEL_SIZE);
				formatf(namecmp, sizeofname, iter.GetKeyName());

				if ((stricmp(namecmp, name) == 0))
				{
					bank->PushGroup(iter.GetKeyName(), false);
					{
						bank->AddToggle("Debug Changes", &pStat->m_ShowChanges, NullCB, NULL);
						bank->AddToggle("Show on screen", &pStat->m_ShowOnScreen, datCallback(MFA1(CStatsDataMgr::AddToDebugDraw), (datBase*)this, (CallbackData)pStat));
						Bank_AddStatWidgetValue(bank, *iter);
					}
					bank->PopGroup();
				}
			}
			iter++;
		}

	}
}

void
CStatsDataMgr::Bank_AddStatsToGroupMP(bkBank* bank, const char* name)
{
	if (statVerifyf(bank, "Invalid Bank Specified!") && statVerifyf(name, "Invalid stat name specified!"))
	{
		unsigned sizeofname = ustrlen(name);
		sizeofname++;

		char namecmp[MAX_STAT_LABEL_SIZE];

		StatsMap::Iterator iter = CStatsMgr::GetStatsDataMgr().StatIterBegin();
		while ((iter != CStatsMgr::GetStatsDataMgr().StatIterEnd()))
		{
			sStatData* pStat = 0;
			if (iter.GetKey().IsValid())
			{
				pStat = *iter;
			}

			if (pStat)
			{
				sysMemSet(namecmp, 0, MAX_STAT_LABEL_SIZE);
				formatf(namecmp, sizeofname, iter.GetKeyName());

				if ((stricmp(namecmp, name) == 0))
				{
					Bank_AddStatToHisGroup(bank, iter, pStat);
				}
			}
			iter++;
		}

	}
}
void CStatsDataMgr::GrcDebugDraw()
{
	m_enableDebugDraw = !m_enableDebugDraw;
}

void CStatsDataMgr::AddToDebugDraw(sStatData* data)
{

	StatsMap::Iterator iter = CStatsMgr::GetStatsDataMgr().StatIterBegin();
	const char* statName=NULL;
	while ((iter != CStatsMgr::GetStatsDataMgr().StatIterEnd()))
	{
		sStatData* pStat = 0;
		if (iter.GetKey().IsValid())
		{
			pStat = *iter;
			if(data==pStat)
			{
				statName = iter.GetKeyName();
				break;
			}
		}
		iter++;
	}
	bool add = data->m_ShowOnScreen;
	if(add)
	{
		sStatDisplayData& debugData = m_debugDrawStatsData.Grow();
		debugData.Name=statName;
		debugData.Stat=data;		
	}
	else
	{
		sStatDisplayData findData;
		findData.Name=statName;
		findData.Stat=data;
		m_debugDrawStatsData.DeleteMatches(findData);
	}
}

void CStatsDataMgr::UpdateDebugDraw()
{
	if(m_enableDebugDraw)
	{
		bool saveDebug = grcDebugDraw::GetDisplayDebugText();
		grcDebugDraw::SetDisplayDebugText(TRUE);
		Color32 color = Color_white;
		Vector2 vTextRenderPos(0.05f,0.05f);
		char maininfo[256];
		for(int i=0;i<m_debugDrawStatsData.GetCount();i++)
		{
			char buff[48];
			sStatData* stat = m_debugDrawStatsData[i].Stat;
			const char* name = m_debugDrawStatsData[i].Name;
			vTextRenderPos.y+=0.015f;
			formatf(maininfo,"%s[%s] : %s",name, sStatsTypesNames[stat->GetType()].m_Name, stat->ValueToString(buff, 48));
			grcDebugDraw::Text(vTextRenderPos,color,maininfo,true,1.2f,1.2f);
		}
		grcDebugDraw::SetDisplayDebugText( saveDebug );
	}
}

void 
CStatsDataMgr::Bank_WidgetShutdown()
{
	bkBank* bank = BANKMGR.FindBank("Stats");
	if (bank && m_BkGroup)
	{
		bank->DeleteGroup(*m_BkGroup);
	}

	m_BkGroup    = 0;
	m_TestFail   = false;

	m_bSpBankCreated = false;
	m_bMpBankCreated = false;
}

void 
CStatsDataMgr::Bank_SetStatDirty(sStatDataPtr* ppStat)
{
	if (AssertVerify(ppStat))
	{
		sStatData* pStat = *ppStat;
		if (pStat && pStat->GetIsBaseType(STAT_TYPE_UINT64))
		{
			u64 data = ((sUns64StatData*)pStat)->m_ValueLo + ((sUns64StatData*)pStat)->m_ValueHi;
			CStatsDataMgr::SetStatData(ppStat->GetKey(), &data, sizeof(u64));
		}

		CStatsMgr::GetStatsDataMgr().SetStatDirty(ppStat->GetKey());
	}
}

void
CStatsDataMgr::Bank_FlushProfileStats()
{
	CLiveManager::GetProfileStatsMgr().Flush(false, NetworkInterface::IsNetworkOpen());
}

void
CStatsDataMgr::Bank_ForceFlushProfileStats()
{
	CLiveManager::GetProfileStatsMgr().Flush(true, NetworkInterface::IsNetworkOpen());
}

void 
CStatsDataMgr::Bank_ForceSynchProfileStats()
{
	StatsInterface::SyncProfileStats(true/*force*/, NetworkInterface::IsGameInProgress()/*multiplayer*/);
}

void
CStatsDataMgr::Bank_DirtySomeProfileStats()
{
	int dirtiedCount = 0;

	StatsMap::Iterator iter = m_aStatsData.Begin();
	while (dirtiedCount < s_BankProfileStatsToDirty &&
		   iter != m_aStatsData.End())
	{
		if (iter.GetKey().IsValid())
		{
			sStatData *pStat = *iter;

			if (pStat &&
				pStat->GetDesc().GetIsProfileStat() &&
				pStat->GetDesc().GetSynched())
			{
				pStat->GetDesc().SetDirty(true);
				dirtiedCount++;
			}
		}

		iter++;
	}
}

void 
CStatsDataMgr::Bank_ClearAllTerritoryData()
{
	StatsMap::Iterator iter = m_aStatsData.Begin();
	while (iter != m_aStatsData.End())
	{
		sStatData* pStat = 0;
		if (iter.GetKey().IsValid())
		{
			pStat = *iter;
		}

		if (pStat && pStat->GetDesc().GetIsOnlineData())
		{
			//We must dirty the profile here so that it knows about the reset
			ResetStat(*iter, true/*dirtyProfile*/);
		}
		iter++;
	}
}

void 
CStatsDataMgr::Bank_ToggleScriptChangesToTerritoryData()
{
	StatsMap::Iterator iter = m_aStatsData.Begin();
	while (iter != m_aStatsData.End())
	{
		sStatData* pStat = 0;
		if (iter.GetKey().IsValid())
		{
			pStat = *iter;
		}

		if (pStat && pStat->GetDesc().GetIsOnlineData())
		{
			pStat->m_ShowChanges = s_ToggleScriptChangesToTerritoryData;
		}

		iter++;
	}
}

void 
CStatsDataMgr::Bank_ResetAllOnlineCharacterStats()
{
	if (statVerify(-1 < s_Bankcharacter && s_Bankcharacter < 5))
	{
		ResetAllOnlineCharacterStats(s_Bankcharacter, true, true);
	}
}

void 
CStatsDataMgr::Bank_ResetAllProfileStatsByGroup()
{
	if (statVerify(-1 < s_BankProfileStatGroup))
	{
			CLiveManager::GetProfileStatsMgr().ResetGroup((u32)s_BankProfileStatGroup);
	}
}

void  
CStatsDataMgr::Bank_ToggleUpdateProfileStats()
{
	m_ReadSessionProfileStats.ReadStats((unsigned)s_BankPriority);
}

void  
CStatsDataMgr::Bank_TestCommandLeaderboards2ReadRankPrediction()
{
	stats_commands::TestCommandLeaderboards2ReadRankPrediction();
}

unsigned
CStatsDataMgr::Bank_CalculateAllDataSizes(const bool returnProfileStats)
{
	static unsigned int_stats    = 0;
	static unsigned obfint_stats = 0;
	static unsigned label_stats  = 0;
	static unsigned float_stats  = 0;
	static unsigned obffloat_stats = 0;
	static unsigned bool_stats   = 0;
	static unsigned u8_stats     = 0;
	static unsigned u16_stats    = 0;
	static unsigned u32_stats    = 0;
	static unsigned u64_stats    = 0;
	static unsigned obfu64_stats = 0;
	static unsigned pos_stats    = 0;
	static unsigned date_stats   = 0;
	static unsigned string_stats = 0;
	static unsigned packed_stats = 0;
	static unsigned userid_stats = 0;
	static unsigned profilesetting_stats = 0;
	static unsigned s64_stats = 0;
	static unsigned obfs64_stats = 0;

	static unsigned profileint_stats     = 0;
	static unsigned profilefloat_stats   = 0;
	static unsigned profilebigints_stats = 0;

	if (!int_stats)
	{
		StatsMap::Iterator iter = m_aStatsData.Begin();
		while ((iter != m_aStatsData.End()))
		{
			sStatData* pStat = 0;
			if (iter.GetKey().IsValid())
			{
				pStat = *iter;
			}

			if (pStat)
			{
				switch (pStat->GetType())
				{
				case STAT_TYPE_TEXTLABEL: ++label_stats; break;
				case STAT_TYPE_INT:       pStat->IsObfuscated() ? ++obfint_stats : ++int_stats; break;
				case STAT_TYPE_FLOAT:     pStat->IsObfuscated() ? ++obffloat_stats : ++float_stats; break;
				case STAT_TYPE_BOOLEAN:   ++bool_stats; break;
				case STAT_TYPE_UINT8:     ++u8_stats; break;
				case STAT_TYPE_UINT16:    ++u16_stats; break;
				case STAT_TYPE_UINT32:    ++u32_stats; break;
				case STAT_TYPE_UINT64:    pStat->IsObfuscated() ? ++obfu64_stats : ++u64_stats; break;
				case STAT_TYPE_DATE:      ++date_stats; break;
				case STAT_TYPE_POS:       ++pos_stats; break;
				case STAT_TYPE_STRING:    ++string_stats; break;
				case STAT_TYPE_PACKED:    ++packed_stats; break;
				case STAT_TYPE_USERID:    ++userid_stats; break;
				case STAT_TYPE_INT64:     pStat->IsObfuscated() ? ++obfs64_stats : ++s64_stats; break;
				case STAT_TYPE_PROFILE_SETTING: ++profilesetting_stats; break;
				default: statAssertf(0, "Invalid stat tag type \"%d\"", pStat->GetType()); break;
				}

				if (pStat->GetDesc().GetIsProfileStat())
				{
					switch (pStat->GetType())
					{
					case STAT_TYPE_TEXTLABEL:
					case STAT_TYPE_INT:
					case STAT_TYPE_PROFILE_SETTING:
					case STAT_TYPE_BOOLEAN:
					case STAT_TYPE_UINT8:
					case STAT_TYPE_UINT16:
						++profileint_stats;
						break;
					case STAT_TYPE_FLOAT:
						++profilefloat_stats;
						break;
					case STAT_TYPE_INT64:
					case STAT_TYPE_UINT32:
					case STAT_TYPE_UINT64:
					case STAT_TYPE_DATE:
					case STAT_TYPE_POS:
					case STAT_TYPE_PACKED:
					case STAT_TYPE_USERID:
						++profilebigints_stats;
						break;

					case STAT_TYPE_STRING:   
						break;

					default: statAssertf(0, "Invalid stat tag type \"%d\"", pStat->GetType()); break;
					}

				}
			}
			iter++;
		}
	}

	const unsigned profile_size = profileint_stats*4+profilefloat_stats*4+profilebigints_stats*12
									+((profileint_stats+profilefloat_stats+profilebigints_stats)*4);

	static unsigned total_size = int_stats*sizeof(sIntStatData)+label_stats*sizeof(sLabelStatData)+float_stats*sizeof(sFloatStatData)
								+bool_stats*sizeof(sBoolStatData)+u8_stats*sizeof(sUns8StatData)+u16_stats*sizeof(sUns16StatData)
								+u32_stats*sizeof(sUns32StatData)+u64_stats*sizeof(sUns64StatData)+date_stats*sizeof(sDateStatData)
								+pos_stats*sizeof(sPosStatData)+string_stats*sizeof(sStringStatData)+packed_stats*sizeof(sPackedStatData)
								+userid_stats*sizeof(sUserIdStatData)+profilesetting_stats*sizeof(sProfileSettingStatData)+s64_stats*sizeof(sInt64StatData)
								+obfint_stats*sizeof(sObfIntStatData)+obfs64_stats*sizeof(sObfInt64StatData)+obfu64_stats*sizeof(sObfUns64StatData)
								+obffloat_stats*sizeof(sObfFloatStatData);


	statDebugf1("-----------------------------------------------------------------");
	statDebugf1("                Number of stats = \"%d\"", m_aStatsData.GetCount());
	statDebugf1("                   sizeof Stats = \"%d\"", total_size);
	statDebugf1("           sizeof Profile Stats = \"%d\" int=%d,float=%d,bigint=%d", profile_size, profileint_stats, profilefloat_stats, profilebigints_stats);
	statDebugf1("                            int = \"%d:%" SIZETFMT "d\"", int_stats,    int_stats*sizeof(sIntStatData));
	statDebugf1("                 obfuscated int = \"%d:%" SIZETFMT "d\"", obfint_stats, obfint_stats*sizeof(sObfIntStatData));
	statDebugf1("                         labels = \"%d:%" SIZETFMT "d\"", label_stats,  label_stats*sizeof(sLabelStatData));
	statDebugf1("                          float = \"%d:%" SIZETFMT "d\"", float_stats,  float_stats*sizeof(sFloatStatData));
	statDebugf1("               obfuscated float = \"%d:%" SIZETFMT "d\"", obffloat_stats,obffloat_stats*sizeof(sObfFloatStatData));
	statDebugf1("                           bool = \"%d:%" SIZETFMT "d\"", bool_stats,   bool_stats*sizeof(sBoolStatData));
	statDebugf1("                             u8 = \"%d:%" SIZETFMT "d\"", u8_stats,     u8_stats*sizeof(sUns8StatData));
	statDebugf1("                            u16 = \"%d:%" SIZETFMT "d\"", u16_stats,    u16_stats*sizeof(sUns16StatData));
	statDebugf1("                            u32 = \"%d:%" SIZETFMT "d\"", u32_stats,    u32_stats*sizeof(sUns32StatData));
	statDebugf1("                            u64 = \"%d:%" SIZETFMT "d\"", u64_stats,    u64_stats*sizeof(sUns64StatData));
	statDebugf1("                 obfuscated u64 = \"%d:%" SIZETFMT "d\"", obfu64_stats, obfu64_stats*sizeof(sObfUns64StatData));
	statDebugf1("                            pos = \"%d:%" SIZETFMT "d\"", pos_stats,    pos_stats*sizeof(sPosStatData));
	statDebugf1("                           date = \"%d:%" SIZETFMT "d\"", date_stats,   date_stats*sizeof(sDateStatData));
	statDebugf1("                         string = \"%d:%" SIZETFMT "d\"", string_stats, string_stats*sizeof(sStringStatData));
	statDebugf1("                         packed = \"%d:%" SIZETFMT "d\"", packed_stats, packed_stats*sizeof(sPackedStatData));
	statDebugf1("                 profilesetting = \"%d:%" SIZETFMT "d\"", profilesetting_stats, profilesetting_stats*sizeof(sProfileSettingStatData));
	statDebugf1("                            s64 = \"%d:%" SIZETFMT "d\"", s64_stats, s64_stats*sizeof(sInt64StatData));
	statDebugf1("                 obfuscated s64 = \"%d:%" SIZETFMT "d\"", obfs64_stats, obfs64_stats*sizeof(sObfInt64StatData));
	statDebugf1("-----------------------------------------------------------------");

	//CompileTimeAssert(sizeof(sStatData) == 64); // 52 (inmap_node) + 12
	//TestPosPack(1984.1984, -1900.1, -17.6789, "POSITION_TEST");
	//TestDatePack(1984, 7, 23, 12, 00, 32, 927, "DATE_TEST");

#if __TEST_MASKED_STAT
	TestMaskedStats();
#endif

	if (returnProfileStats)
	{
		return profile_size;
	}

	return total_size;
}

void
CStatsDataMgr::Bank_CheckProfileSyncConsistency()
{
    if (!m_SyncConsistencyStatus.None())
    {
        profileStatDebugf("Consistency check is already in progress, skipping");
        return;
    }

    if (!CLiveManager::GetProfileStatsMgr().CanSynchronize())
    {
        profileStatDebugf("Consistency check can't run (sync is already in progress?)");
    }

    profileStatDebugf("Starting profile sync consistency check...");
    profileStatDebugf("Client sync state:");

    for (StatsMap::Iterator iter = m_aStatsData.Begin(); iter != m_aStatsData.End(); ++iter)
    {
        sStatData* pStat = NULL;
        if (iter.GetKey().IsValid())
        {
            pStat = *iter;
        }

        if (pStat
            && pStat->GetDesc().GetIsProfileStat())
        {
            const char* synched = pStat->GetDesc().GetSynched() ? "true" : "false";
            const char* dirty = pStat->GetDesc().GetDirty() ? "true" : "false";

            profileStatDebugf("Stat (%s, Group %d) Synched: %s, Dirty: %s", iter.GetKeyName(), pStat->GetDesc().GetGroup(), synched, dirty);
        }
    }

    //Now kick off a full synchronize
    m_SyncConsistencyStatus.Reset();
    rlProfileStats::Synchronize(NetworkInterface::GetLocalGamerIndex(), &m_SyncConsistencyStatus);
}

void
CStatsDataMgr::Bank_Enable_Disable_Stats_Tracking()
{
	CStatsUtils::EnableStatsTracking(!CStatsUtils::IsStatsTrackingEnabled());
}

#endif // __BANK

// EOF


