//
// StatsTypes.h
//
// Copyright (C) 1999-20010 Rockstar Games.  All Rights Reserved.
//

#ifndef _STATSENUMS_H_
#define _STATSENUMS_H_

// Rage includes
#include "atl/hashstring.h"
#include "string/stringhash.h"

// framework
#include "fwutil/Gen9Settings.h"

// --- Defines ------------------------------------------------------------------
#ifdef HASH_STAT_ID
#undef HASH_STAT_ID
#endif
#define HASH_STAT_ID(a) atStringHash(a)

// --- handy metric-to-imperial converters -------------------------------------
#define KGS_TO_LBS(M) (float(M)*2.20462262f)
#define LBS_TO_KGS(M) (float(M)*0.453592f)
#define METERS_TO_MILES(M) (float(M)/1609.344f)
#define KPH_TO_MPH(M) (float(M)/1.609344f)
#define METERS_TO_FEET(M) (float(M)/0.3048f)
#define METERS_TO_KILOMETERS(M) (float(M)/1000.0f)
#define MILES_TO_KILOMETERS(M) (float(M)*1.609344f)

#define STAT_LIMIT (1000.0f)

//If this is set we only use one file in the multiplayer savegames.
//  If this is set make sure all SaveCategories in the xml file are set to 0.
#define __MPCLOUDSAVES_ONLY_ONEFILE (0)

#define INCREMENT_STAT  (1)
#define DECREMENT_STAT  (0)

// if a stat increases by this amount then display message
#define STAT_DISPLAY_THRESHOLD (40)

// Stat Label is the stat unique ID
#define MAX_STAT_LABEL_SIZE 32

// Maximun size of a string
#define MAX_STAT_STRING_SIZE (32)

// Represents a invalid stat, in which the label is NULL as well
#define INVALID_STAT_ID 0

//Profile stats that have priority set to PS_HIGH_PRIORITY or PS_LOW_PRIORITY 
// will be downloaded during multiplayer game for the remote players.
#define PS_HIGH_PRIORITY 1
#define PS_LOW_PRIORITY  2

//Maximun number of profile stats that can be download for a remote player.
#define MAX_NUM_READ_REMOTE_GAMER_STATS (11)

//Maximun number os remote gamers that we will need to download profile stats.
#define MAX_NUM_REMOTE_GAMERS (MAX_NUM_ACTIVE_PLAYERS)

//Stat load types
enum eStatsLoadType
{
	STATS_LOAD_CLOUD
};

//Stat save types
enum eStatsSaveType
{
	STATS_SAVE_CLOUD
};

//These flags are used during stats updates.
enum 
{
	STATUPDATEFLAG_ASSERTONLINESTATS        = (1<<0), //Assert and make multiplayer stats verifications.
	STATUPDATEFLAG_DIRTY_PROFILE            = (1<<1), //Try to mark dirty into profile stats system.
    STATUPDATEFLAG_LOAD_FROM_SP_SAVEGAME    = (1<<2), //Stat updated from SP savegame load.
	STATUPDATEFLAG_LOAD_FROM_MP_SAVEGAME    = (1<<3), //Stat updated from MP savegame load.
	STATUPDATEFLAG_SCRIPT_UPDATE            = (1<<4), //Stat updated from Script.
	STATUPDATEFLAG_GAMER_SERVER             = (1<<5), //Stat updated from gamer server transaction.
	STATUPDATEFLAG_FORCE_CHANGE             = (1<<6), //Force prof stat flush.

	//Stat updated from savegame load (either MP or SP).
    STATUPDATEFLAG_LOAD_FROM_SAVEGAME       = STATUPDATEFLAG_LOAD_FROM_SP_SAVEGAME|STATUPDATEFLAG_LOAD_FROM_MP_SAVEGAME,

	//Default stat updates.
	STATUPDATEFLAG_DEFAULT                  = STATUPDATEFLAG_ASSERTONLINESTATS|STATUPDATEFLAG_DIRTY_PROFILE,
	STATUPDATEFLAG_DEFAULT_FORCE_CHANGE     = STATUPDATEFLAG_ASSERTONLINESTATS|STATUPDATEFLAG_DIRTY_PROFILE|STATUPDATEFLAG_FORCE_CHANGE,

	//Default stat updates from game server transactions.
	STATUPDATEFLAG_DEFAULT_GAMER_SERVER     = STATUPDATEFLAG_DEFAULT|STATUPDATEFLAG_GAMER_SERVER,

	//Default stat updates from script.
	STATUPDATEFLAG_DEFAULT_SCRIPT_UPDATE    = STATUPDATEFLAG_DEFAULT|STATUPDATEFLAG_SCRIPT_UPDATE,
	STATUPDATEFLAG_ASSERT_SCRIPT_UPDATE     = STATUPDATEFLAG_DEFAULT|STATUPDATEFLAG_SCRIPT_UPDATE|STATUPDATEFLAG_ASSERTONLINESTATS
};

//PURPOSE
//  Our stats id identifier.
class StatId
{
private:
	atStatNameString m_Id;

public:
	StatId() { }
	StatId(const char* str) { Set(str); }
	StatId(const u32 id) { m_Id.SetHash(id); }

public:
	inline u32 GetHash() const { return m_Id.GetHash(); }
	inline u32 GetHash()       { return m_Id.GetHash(); }
	OUTPUT_ONLY(inline const char* GetName() const { return m_Id.TryGetCStr(); })

	void  Set(const char* str);

	void  SetHash(const u32 hash, const char* NOTFINAL_ONLY(str))
	{
#if !__FINAL
		if(str && ustrlen(str)>0)
		{
			m_Id.SetHashWithString(hash, str);
		}
#endif //!__FINAL

		//Set the final key hash.
		m_Id.SetHash(hash);
	}

	bool  IsValid()       { return m_Id.IsNotNull(); }
	bool  IsValid() const { return m_Id.IsNotNull(); }

	bool  operator  <(const StatId& other) const { return (GetHash() < other.GetHash()); }
	bool  operator ==(const StatId& other) const { return (GetHash() == other.GetHash()); }
	bool  operator ==(const u32 id)        const { return (GetHash() == id); }

	StatId& operator=(const StatId& other) 
	{
#if !__NO_OUTPUT
		SetHash(other.GetHash(), other.GetName()); 
#else
		SetHash(other.GetHash(), 0); 
#endif // __FINAL

		return *this;
	}

	operator atStatNameString() const {return m_Id;}
	operator u32()          const {return m_Id.GetHash();}
};
#define STAT_ID(a) StatId(a)

//PURPOSE
//  Same as StatId but it wont have the name in FINAL builds since we only need the hash.
class StatId_static : public StatId
{
#if __ASSERT
public:
	// For Validation after the stats have been loaded.
	StatId_static*         m_Next;
	static StatId_static*  sm_First;
#endif // __ASSERT

public:
	StatId_static(const char* str) : StatId(str)
	{
#if __ASSERT
		m_Next   = sm_First;
		sm_First = this;
#endif //__ASSERT
	}

	StatId_static(const atHashString& hash) : StatId(hash.GetHash())
	{
#if __ASSERT
		m_Next   = sm_First;
		sm_First = this;
#endif //__ASSERT
	}

	ASSERT_ONLY(~StatId_static();)
};

#if __ASSERT
#define STATIC_STAT_ID(a, ...) StatId_static(a)
#else
#define STATIC_STAT_ID(a, ...) StatId_static(__VA_ARGS__)
#endif // __ASSERT

//PURPOSE
//  Enum that describes all the possible groups set for the profile stats.
enum eProfileStatsGroups
{
	 PS_GRP_MP_START = 0

	 /* MultiPlayer Stats Groups */
	,PS_GRP_MP_0 = PS_GRP_MP_START //All remaining MP stats
	,PS_GRP_MP_1                   //Stats with name perfixed MP0_
	,PS_GRP_MP_2                   //Stats with name perfixed MP1_
	,PS_GRP_MP_3                   //Stats with name perfixed MP2_
	,PS_GRP_MP_4                   //Stats with name perfixed MP3_
	,PS_GRP_MP_5                   //Stats with name perfixed MP4_ - NOT USED ANYMORE
	,PS_GRP_MP_6                   //Stats with name perfixed MPPLY_
	,PS_GRP_MP_7                   //Stats with name perfixed MPGEN_
	,PS_GRP_MP_END = PS_GRP_MP_7

	/* SinglePlayer Stats Groups */
	,PS_GRP_SP_START
	,PS_GRP_SP_0 = PS_GRP_SP_START //All non-character SP stats
	,PS_GRP_SP_1                   //Stats for character 1 (SP0_)
	,PS_GRP_SP_2                   //Stats for character 2 (SP1_)
	,PS_GRP_SP_3                   //Stats for character 3 (SP2_)

	,PS_GRP_SP_END = PS_GRP_SP_3
};

//Possible stats model prefixes 
enum eCharacterIndex
{
	CHAR_SP_START /*= 0*/,
	CHAR_MICHEAL = CHAR_SP_START,
	CHAR_FRANKLIN /*= 1*/,
	CHAR_TREVOR   /*= 2*/,
	CHAR_SP_END = CHAR_TREVOR,
	CHAR_MP_START /*= 3*/,
	CHAR_MP0 = CHAR_MP_START,
	CHAR_MP1      /*= 4*/,
	CHAR_MP_END = CHAR_MP1,
	MAX_STATS_CHAR_INDEXES,        //8
	CHAR_INVALID = MAX_STATS_CHAR_INDEXES
};
const u32 MAX_NUM_SP_CHARS = CHAR_MP_START;
const u32 MAX_NUM_MP_CHARS = CHAR_MP_END - CHAR_SP_END;
const u32 MAX_NUM_MP_ACTIVE_CHARS = 2;
const u32 MAX_NUM_MP_CLOUD_FILES  = MAX_NUM_MP_CHARS + 1; //1 for the default slot

extern const char * s_StatsModelPrefixes[MAX_STATS_CHAR_INDEXES];

//PURPOSE: Same as StatId but has a hash for each character.
class StatId_char
{
private:
	int  m_Id[MAX_STATS_CHAR_INDEXES];

#if __ASSERT
public:
	// For Validation after the stats have been loaded.
	StatId_char*         m_Next;
	static StatId_char*  sm_First;
	const char*          m_Name;
	bool                 m_Validate;

public:
	StatId_char(const char* str, const bool validate) {m_Validate = validate; Set(str);}
	StatId_char(const char* name, const atHashString& sp0, const atHashString& sp1, const atHashString& sp2, const atHashString& mp0, const atHashString& mp1, const bool validate);
	~StatId_char();
#endif // __ASSERT

public:
	StatId_char(const char* str) { ASSERT_ONLY(m_Validate = false;); Set(str); }
	StatId_char(const atHashString& sp0, const atHashString& sp1, const atHashString& sp2, const atHashString& mp0, const atHashString& mp1);

	u32        GetHash(const u32 prefix) const;
	u32        GetHash()                 const;
	StatId   GetStatId()                 const;
	StatId GetOnlineStatId(const u32 slot)   const;

	bool  operator ==(const StatId id) const { return (GetHash() == id.GetHash()); }
	bool  operator ==(const u32 id)    const { return (GetHash() == id); }

	// Conversion operators
	operator  StatId() const { return GetStatId(); }

private:
	void  Set(const char* str);
};
#if __ASSERT
#define STATIC_STAT_ID_CHAR(a, ...) StatId_char(a, __VA_ARGS__, true)
#else
#define STATIC_STAT_ID_CHAR(a, ...) StatId_char(__VA_ARGS__)
#endif // __ASSERT

// Our stats type identifier
typedef u32 StatType;

/*
Legend:
	-	Disabled 
		o	Save is ignored.

	-	Deferred Profile Flush 
		o	Performs a profile stats flush when 5 minutes have passed from last flush.

	-	Cloud Only 
		o	Makes a cloud save Only. Can happen a deferred flush.

	-	Profile Flush Only 
		o	Makes a Profile Flush Only.

	-	Profile Flush, Cloud Save 
		o	Makes a cloud save and profile stats Flush.

	-	Cloud Only, Deferred Profile Flush 
		o	Makes a cloud save and a deferred profile stats Flush.
*/

// These represent commom save types so that we can choose the importance of the save.
enum eSaveTypes
{
	STAT_SAVETYPE_INVALID = -1

	//Default save where the event is not identifiable.
	,STAT_SAVETYPE_DEFAULT            /* Disabled */

	//Code Saves
	,STAT_SAVETYPE_STUNTJUMP          /* Deferred Profile Flush */
	,STAT_SAVETYPE_CHEATER_CHANGE     /* Deferred Profile Flush */
	,STAT_SAVETYPE_IMMEDIATE_FLUSH    /* Disabled */
	,STAT_SAVETYPE_COMMERCE_DEPOSIT   /* Disabled */
	,STAT_SAVETYPE_EXPLOITS           /* Deferred Profile Flush */
	,STAT_SAVETYPE_STORE              /* Disabled */
	,STAT_SAVETYPE_END_CODE

	//Script Saves
	,STAT_SAVETYPE_AMBIENT = STAT_SAVETYPE_END_CODE  /* Cloud Only, Deferred Profile Flush */
	,STAT_SAVETYPE_CASH               /* Disabled */
	,STAT_SAVETYPE_END_GAMER_SETUP    /* Profile Flush Only */
	,STAT_SAVETYPE_END_SHOPPING       /* Profile Flush Only */
	,STAT_SAVETYPE_END_GARAGE         /* Cloud Only, Same as STAT_SAVETYPE_SCRIPT_MP_GLOBALS */
	,STAT_SAVETYPE_END_MISSION        /* Cloud Only, Profile Flush  */
	,STAT_SAVETYPE_END_ATM            /* Disabled */
	,STAT_SAVETYPE_PRE_STARTSTORE     /* Profile Flush, Cloud Save */
	,STAT_SAVETYPE_SCRIPT_MP_GLOBALS  /* Cloud Only */
	,STAT_SAVETYPE_PROLOGUE           /* Cloud Only */
	,STAT_SAVETYPE_CONTACTS           /* Disabled */
	,STAT_SAVETYPE_START_MATCH        /* Disabled */
	,STAT_SAVETYPE_START_SESSION      /* Cloud Only */
	,STAT_SAVETYPE_INTERACTION_MENU   /* Cloud Only, Deferred Profile Flush */
	,STAT_SAVETYPE_JOIN_SC            /* Disabled */
	,STAT_SAVETYPE_AMB_PROFILE_AWARD_TRACKER /* Cloud Only, Deferred Flush  */
	,STAT_SAVETYPE_PHOTOS             /* Deferred Profile Flush */
	,STAT_SAVETYPE_WEAPON_DROP        /* Disabled */
	,STAT_SAVETYPE_RANKUP             /* Cloud Only, Deferred Profile Flush */
	,STAT_SAVETYPE_END_MISSION_CREATOR /* Cloud Only, Deferred Profile Flush  */

	//Critical save types.
	,STAT_SAVETYPE_CRITICAL
	,STAT_SAVETYPE_END_CREATE_NEWCHAR = STAT_SAVETYPE_CRITICAL  /* Profile Flush, Cloud Save */
	,STAT_SAVETYPE_END_MATCH           /* Profile Flush, Cloud Save */
	,STAT_SAVETYPE_END_SESSION         /* Profile Flush, Cloud Save */
	,STAT_SAVETYPE_DELETE_CHAR         /* Profile Flush, Cloud Save */

	,STAT_SAVETYPE_MAX_NUMBER
};
#if !__FINAL
const char* GET_STAT_SAVETYPE_NAME(const eSaveTypes savetype);
#endif //!__FINAL


// --- Enums --------------------------------------------------------------------

enum eStatModAbilities
{
	STAT_MODIFIER_CLIMB_HEIGHT = 0,
	STAT_MODIFIER_CLIMB_SPEED,
	STAT_MODIFIER_JUMP_SPEED,
	STAT_MODIFIER_FIGHT_SPEED,
	STAT_MODIFIER_FIGHT_DAMAGE,
	STAT_MODIFIER_BMX_SPRINT_SPEED,
	STAT_MODIFIER_BMX_BUNNYHOP,
	STAT_MODIFIER_SPRINT_ENERGY,
	STAT_MODIFIER_BREATH_UNDERWATER,
	STAT_MODIFIER_MAX_HEALTH,
	STAT_MODIFIER_MAX_HEALTH_LIMIT,
	STAT_MODIFIER_BIKE_WHEELIE_MULT,
	STAT_MODIFIER_BIKE_BALANCE_FORCE,
	STAT_MODIFIER_BIKE_BALANCE_DAMPING,
	STAT_MODIFIER_CAR_INAIR_BALANCE,
	STAT_MODIFIER_TAKE_DAMAGE_MULT
};

// Frontend display flags.
enum eFrontendStatShowFlag
{
	// Always show the stat.
	STAT_FLAG_ALWAYS_SHOW = (1<<0)

	// Show only incremented stats.
	,STAT_FLAG_INC_SHOW    = (1<<1)

	// Never show the stat.
	,STAT_FLAG_NEVER_SHOW  = (1<<2)
};

// Types of stats
enum eFrontendStatType
{
	// Invalid/not set type
	STAT_TYPE_NONE = 0,

	// Base types
	STAT_TYPE_INT       = 1,
	STAT_TYPE_FLOAT     = 2,
	STAT_TYPE_STRING    = 3,
	STAT_TYPE_BOOLEAN   = 4,
	STAT_TYPE_UINT8     = 5,
	STAT_TYPE_UINT16    = 6,
	STAT_TYPE_UINT32    = 7,
	STAT_TYPE_UINT64    = 8,

	// Variations of base types... 
	// ... These variations are use when the stat is rendered to apply certain format to then ... or just use to what u want
	STAT_TYPE_TIME      = 9,
	STAT_TYPE_CASH      = 10,
	STAT_TYPE_PERCENT   = 11,
	STAT_TYPE_DEGREES   = 12,
	STAT_TYPE_WEIGHT    = 13,
	STAT_TYPE_MILES     = 14,
	STAT_TYPE_METERS    = 15,
	STAT_TYPE_FEET      = 16,
	STAT_TYPE_SECONDS   = 17,
	STAT_TYPE_CHART     = 18,
	STAT_TYPE_VELOCITY  = 19,
	STAT_TYPE_DATE      = 20,
	STAT_TYPE_POS       = 21,

	//Reminder: To notice that stats wont store an actual string but ratter the key for the map in TheText
	STAT_TYPE_TEXTLABEL = 22,
	STAT_TYPE_PACKED    = 23,
	STAT_TYPE_USERID    = 24,
	STAT_TYPE_PROFILE_SETTING = 25,
	STAT_TYPE_INT64    = 26,

	MAX_STAT_TYPE
};

//SaveGames can be split up to 14 different categories.
// When this value is used the save doesnt care about the category and doesn't split the stats.
#define MAX_STAT_SAVE_CATEGORY (15)

//SinglePlayer Stats Saves Categories:
//  - We can have to 15 categories in which the stats are split so that the savegames
//     take less space in memory.
enum eSinglePlayerStatSaveCategory
{
	STAT_SP_CATEGORY_DEFAULT = 0
	,STAT_SP_CATEGORY_CHAR0
	,STAT_SP_CATEGORY_CHAR1
	,STAT_SP_CATEGORY_CHAR2
	,STAT_SP_CATEGORY_MAX = STAT_SP_CATEGORY_CHAR2
};

//Multiplayer Stats Saves Categories:
//  - We can have to 15 categories in which the stats are split so that the savegames
//     take less space in memory.
enum eMultiPlayerStatSaveCategory
{
	STAT_INVALID_SLOT        = -1
	,STAT_MP_CATEGORY_DEFAULT = 0
	,STAT_MP_CATEGORY_CHAR0   = 1
	,STAT_MP_CATEGORY_CHAR1   = 2
	,STAT_MP_CATEGORY_MAX   = STAT_MP_CATEGORY_CHAR1
};

// Static Const Stats 

#if GEN9_STANDALONE_ENABLED
extern const StatId_static STAT_MP_PLATFORM_GENERATION;
extern const StatId_static STAT_MP0_WINDFALL_CHAR;
extern const StatId_static STAT_MP1_WINDFALL_CHAR;
extern const StatId_static STAT_MP0_WINDFALL_CHAR_CAREER;
extern const StatId_static STAT_MP1_WINDFALL_CHAR_CAREER;
#endif

extern const StatId_static STAT_TOTAL_PROGRESS_MADE;
extern const StatId_static STAT_MSG_BEING_DISPLAYED;
extern const StatId_static STAT_MOCAP_CUTSCENE_SKIPPED;
extern const StatId_static STAT_MOCAP_CUTSCENE_WATCHED;
extern const StatId_static STAT_CUTSCENES_SKIPPED;
extern const StatId_static STAT_CUTSCENES_WATCHED;
extern const StatId_static STAT_PLAYING_TIME;
extern const StatId_static STAT_STEALTH_TIME;
extern const StatId_static STAT_MP_PLAYING_TIME;
extern const StatId_static STAT_MP_PLAYING_TIME_NEW;
extern const StatId_static STAT_FIRST_PERSON_CAM_TIME;
extern const StatId_static STAT_THIRD_PERSON_CAM_TIME;
extern const StatId_static STAT_MP_FIRST_PERSON_CAM_TIME;
extern const StatId_static STAT_MP_THIRD_PERSON_CAM_TIME;
extern const StatId_static STAT_CITIES_PASSED;
extern const StatId_static STAT_MP_SESSIONS_STARTED;
extern const StatId_static STAT_MP_SESSIONS_ENDED;
extern const StatId_static STAT_MP_MOST_FRIENDS_IN_ONE_MATCH;
extern const StatId_static STAT_MP_VEHICLE_MODELS_DRIVEN;
extern const StatId_static STAT_SP_VEHICLE_MODELS_DRIVEN;
extern const StatId_static STAT_MP0_CHAR_ISACTIVE;
extern const StatId_static STAT_MP1_CHAR_ISACTIVE;
extern const StatId_static STAT_MPPLY_LAST_MP_CHAR;
extern const StatId_static STAT_ACTIVE_CREW;
extern const StatId_static STAT_MP_KILLS_PLAYERS;
extern const StatId_static STAT_MP_DEATHS_PLAYER;
extern const StatId_static STAT_MP_SUICIDES_PLAYER;
extern const StatId_static STAT_MP_CHAR_DIST_TRAVELLED;
extern const StatId_static STAT_SP_LEAST_FAVORITE_STATION;
extern const StatId_static STAT_SP_MOST_FAVORITE_STATION;
extern const StatId_static STAT_MPPLY_LEAST_FAVORITE_STATION;
extern const StatId_static STAT_MPPLY_MOST_FAVORITE_STATION;

extern const StatId_static STAT_MPPLY_OVERALL_BADSPORT;
extern const StatId_static STAT_MPPLY_IS_BADSPORT;
extern const StatId_static STAT_MPPLY_DEATHS_PLAYERS_CHEATER;
extern const StatId_static STAT_MPPLY_KILLS_PLAYERS_CHEATER;
extern const StatId_char   STAT_CASINO_CHIPS;

extern const StatId_char   STAT_CHAR_XP_FM;

extern const StatId_char   STAT_WALLET_BALANCE;      //Per-Character wallet Cash.
extern const StatId_static STAT_BANK_BALANCE;        //Bank Cash.
extern const StatId_static STAT_CLIENT_PVC_BALANCE;   
extern const StatId_static STAT_CLIENT_EVC_BALANCE;   
extern const StatId_static STAT_EVC_BALANCE_CLEARED; //Total Cleared EVC Cash when player is a cheater.
extern const StatId_static STAT_MPPLY_TOTAL_EVC;     //Total Virtual cash player has EARN, either in game or via other mechanism.
extern const StatId_char   STAT_TOTAL_EVC;           //Total Virtual cash player has EARN, either in game or via other mechanism.
extern const StatId_char   STAT_VEHICLE_WEAPONHASH;


extern const StatId_static STAT_PVC_DAILY_ADDITIONS;   //Amount of PVC added (through purchase or PVC gift receipt) by a player in 1 day.
extern const StatId_static STAT_CLIENT_PXR;			// PXR value.
extern const StatId_static STAT_PVC_USDE;          // US dollar value of the players PVC. PVC Balance / PXR.

extern const StatId_static STAT_VC_DAILY_TIMESTAMP;    //Virtual cash daily posix time - Control daily amount resets.
extern const StatId_static STAT_PVC_DAILY_TRANSFERS;   //Amount of PVC transferred out by a player to other players in 1 day.
extern const StatId_static STAT_VC_DAILY_TRANSFERS;		//Amount of VC (non-type) transferred out by a player in 1 day

extern const StatId_static STAT_MPPLY_STORE_TOTAL_MONEY_BOUGHT;
extern const StatId_static STAT_MPPLY_STORE_PURCHASE_POSIX_TIME;
extern const StatId_static STAT_MPPLY_STORE_CHECKOUTS_CANCELLED;
extern const StatId_static STAT_MPPLY_STORE_MONEY_SPENT;

extern const StatId_static STAT_SCADMIN_IS_CHEATER;
extern const StatId_static STAT_MPPLY_CHEATER_CLEAR_TIME;
extern const StatId_static STAT_CASH_GIFT_RECEIVED;
extern const StatId_static STAT_CASH_GIFT;
extern const StatId_static STAT_CASH_GIFT_LABEL_1;
extern const StatId_static STAT_CASH_GIFT_LABEL_2;
extern const StatId_static STAT_RP_GIFT_RECEIVED;
extern const StatId_static STAT_RP_GIFT;
extern const StatId_static STAT_CASH_GIFT_NEW;
extern const StatId_static STAT_CASH_GIFT_CREDITED;
extern const StatId_static STAT_CASH_GIFT_DEBITED;
extern const StatId_static STAT_CASH_GIFT_LEAVE_REMAINDER;
extern const StatId_static STAT_CASH_GIFT_MIN_BALANCE;
extern const StatId_static STAT_CASH_EVC_CORRECTION;
extern const StatId_static STAT_CASH_PVC_CORRECTION;
extern const StatId_static STAT_CASH_USDE_CORRECTION;
extern const StatId_static STAT_CASH_PXR_CORRECTION;
extern const StatId_static STAT_CASH_FIX_PVC_WB_CORRECTION;
extern const StatId_static STAT_CASH_FIX_EVC_CORRECTION;
extern const StatId_static STAT_MPPLY_SCADMIN_CESP;

/*
extern const StatId_static STAT_CHIP_GIFT;
extern const StatId_static STAT_CHIP_GIFT_LABEL_1;
extern const StatId_static STAT_CHIP_GIFT_LABEL_2;
extern const StatId_static STAT_CHIP_GIFT_CREDITED;
extern const StatId_static STAT_CHIP_GIFT_DEBITED;
extern const StatId_static STAT_CHIP_GIFT_LEAVE_REMAINDER;
*/

extern const StatId_char   STAT_BEND_PROGRESS_HASH;
extern const StatId_char   STAT_TOTAL_CASH_EARNED;
extern const StatId_char   STAT_TOTAL_CASH;
extern const StatId_char   STAT_MONEY_TOTAL_SPENT;

extern const StatId_char   STAT_RANK_FM;
extern const StatId_char   STAT_LONGEST_PLAYING_TIME;
extern const StatId_char   STAT_LONGEST_CHASE_TIME;
extern const StatId_char   STAT_LAST_CHASE_TIME;
extern const StatId_char   STAT_TOTAL_TIME_MAX_STARS;
extern const StatId_char   STAT_TOTAL_CHASE_TIME;
extern const StatId_char   STAT_STARS_EVADED;
extern const StatId_char   STAT_STARS_ATTAINED;
extern const StatId_char   STAT_STAMINA; 
extern const StatId_char   STAT_LUNG_CAPACITY;
extern const StatId_char   STAT_STRENGTH; 
extern const StatId_char   STAT_LONGEST_2WHEEL_TIME;
extern const StatId_char   STAT_LONGEST_2WHEEL_DIST;
extern const StatId_char   STAT_LONGEST_WHEELIE_DIST;
extern const StatId_char   STAT_LONGEST_WHEELIE_TIME;
extern const StatId_char   STAT_TOTAL_WHEELIE_TIME;
extern const StatId_char   STAT_LONGEST_STOPPIE_TIME;
extern const StatId_char   STAT_LONGEST_STOPPIE_DIST;
extern const StatId_char   STAT_CHAR_MISSION_PASSED;
extern const StatId_char   STAT_CHAR_MISSION_FAILED;
extern const StatId_char   STAT_ENTERED_COVER;
extern const StatId_char   STAT_ENTERED_COVER_AND_SHOT;
extern const StatId_char   STAT_CROUCHED;
extern const StatId_char   STAT_CROUCHED_AND_SHOT;
extern const StatId_char   STAT_TOTAL_PLAYING_TIME;
extern const StatId_char   STAT_KILLS_IN_FREE_AIM;
extern const StatId_char   STAT_FASTEST_SPEED;
extern const StatId_char   STAT_TOP_SPEED_CAR;
extern const StatId_char   STAT_AVERAGE_SPEED;
extern const StatId_char   STAT_WHEELIE_ABILITY;
extern const StatId_char   STAT_FLYING_ABILITY;
extern const StatId_char   STAT_SHOTS;
extern const StatId_char   STAT_HITS;
extern const StatId_char   STAT_WEAPON_ACCURACY;
extern const StatId_char   STAT_HITS_MISSION;
extern const StatId_char   STAT_HITS_PEDS_VEHICLES;
extern const StatId_char   STAT_DB_SHOTTIME;
extern const StatId_char   STAT_DB_SHOTS;
extern const StatId_char   STAT_DB_HITS;
extern const StatId_char   STAT_DB_HITS_PEDS_VEHICLES;
extern const StatId_char   STAT_PASS_DB_SHOTTIME;
extern const StatId_char   STAT_PASS_DB_SHOTS;
extern const StatId_char   STAT_PASS_DB_HITS;
extern const StatId_char   STAT_PASS_DB_HITS_PEDS_VEHICLES;
extern const StatId_char   STAT_DIST_SWIMMING;
extern const StatId_char   STAT_TIME_SWIMMING;
extern const StatId_char   STAT_DIST_WALKING;
extern const StatId_char   STAT_TIME_WALKING;
extern const StatId_char   STAT_DIST_WALKING_STEALTH;
extern const StatId_char   STAT_DIST_RUNNING;
extern const StatId_char   STAT_AWD_PARACHUTE_JUMPS_50M;
extern const StatId_char   STAT_AWD_PARACHUTE_JUMPS_10M;
extern const StatId_char   STAT_LONGEST_SURVIVED_FREEFALL;
extern const StatId_char   STAT_TIME_IN_COVER;
extern const StatId_char   STAT_TIRES_POPPED_BY_GUNSHOT;
extern const StatId_char   STAT_KILLS;
extern const StatId_char   STAT_KILLS_SINCE_LAST_CHECKPOINT;
extern const StatId_char   STAT_KILLS_SINCE_SAFEHOUSE_VISIT;
extern const StatId_char   STAT_KILLS_ARMED;
extern const StatId_char   STAT_DB_KILLS;
extern const StatId_char   STAT_PASS_DB_KILLS;
extern const StatId_char   STAT_DB_PLAYER_KILLS;
extern const StatId_char   STAT_PASS_DB_PLAYER_KILLS;
extern const StatId_char   STAT_PLAYER_HEADSHOTS;
extern const StatId_char   STAT_DB_HEADSHOTS;
extern const StatId_char   STAT_PASS_DB_HEADSHOTS;
extern const StatId_char   STAT_DEATHS;
extern const StatId_char   STAT_DIED_IN_MISSION;
extern const StatId_char   STAT_DEATHS_PLAYER;
extern const StatId_char   STAT_MOST_FLIPS_IN_ONE_JUMP;
extern const StatId_char   STAT_MOST_SPINS_IN_ONE_JUMP;
extern const StatId_char   STAT_LONGEST_DRIVE_NOCRASH;
extern const StatId_char   STAT_HIGHEST_JUMP_REACHED;
extern const StatId_char   STAT_LONGEST_SKYDIVE;
extern const StatId_char   STAT_AIR_LAUNCHES_OVER_5S;
extern const StatId_char   STAT_AIR_LAUNCHES_OVER_5M;
extern const StatId_char   STAT_AIR_LAUNCHES_OVER_40M;
extern const StatId_char   STAT_FARTHEST_JUMP_DIST;
extern const StatId_char   STAT_HYDRAULIC_JUMP;
extern const StatId_char   STAT_NUMBER_OF_AIR_LAUNCHES;
extern const StatId_char   STAT_HIGHEST_SKITTLES;
extern const StatId_char   STAT_EXPLOSIVES_USED;
extern const StatId_char   STAT_KILLS_BY_OTHERS;
extern const StatId_char   STAT_NUMBER_NEAR_MISS;
extern const StatId_char   STAT_NUMBER_NEAR_MISS_NOCRASH;
extern const StatId_char   STAT_NEAR_MISS_PRECISE;
extern const StatId_char   STAT_KILLS_FRIENDLY_GANG_MEMBERS;
extern const StatId_char   STAT_KILLS_ENEMY_GANG_MEMBERS;
extern const StatId_char   STAT_KILLS_SWAT;
extern const StatId_char   STAT_KILLS_COP;
extern const StatId_char   STAT_KILLS_INNOCENTS;
extern const StatId_char   STAT_KILLS_STEALTH;
extern const StatId_char   STAT_PLAYER_KILLS_ON_SPREE;
extern const StatId_char   STAT_COPS_KILLS_ON_SPREE;
extern const StatId_char   STAT_PEDS_KILLS_ON_SPREE;
extern const StatId_char   STAT_LONGEST_KILLING_SPREE;
extern const StatId_char   STAT_LONGEST_KILLING_SPREE_TIME;
extern const StatId_char   STAT_KILLS_PLAYERS;
extern const StatId_char   STAT_SNIPER_KILL;
extern const StatId_char   STAT_CARS_EXPLODED;
extern const StatId_char   STAT_CARS_COPS_EXPLODED;
extern const StatId_char   STAT_BIKES_EXPLODED;
extern const StatId_char   STAT_BOATS_EXPLODED;
extern const StatId_char   STAT_HELIS_EXPLODED;
extern const StatId_char   STAT_PLANES_EXPLODED;
extern const StatId_char   STAT_QUADBIKE_EXPLODED;
extern const StatId_char   STAT_BICYCLE_EXPLODED;
extern const StatId_char   STAT_SUBMARINE_EXPLODED;
extern const StatId_char   STAT_TRAIN_EXPLODED;
extern const StatId_char   STAT_VEHICLES_DESTROYED_ON_SPREE;
extern const StatId_char   STAT_COP_VEHI_DESTROYED_ON_SPREE;
extern const StatId_char   STAT_TANKS_DESTROYED_ON_SPREE;
extern const StatId_char   STAT_CARS_WRECKED;
extern const StatId_char   STAT_CARS_COPS_WRECKED;
extern const StatId_char   STAT_TOTAL_DAMAGE_CARS;
extern const StatId_char   STAT_LARGE_ACCIDENTS;
extern const StatId_char   STAT_NUMBER_CRASHES_CARS;
extern const StatId_char   STAT_TOTAL_DAMAGE_QUADBIKES;
extern const StatId_char   STAT_NUMBER_CRASHES_QUADBIKES;
extern const StatId_char   STAT_TOTAL_DAMAGE_BIKES;
extern const StatId_char   STAT_NUMBER_CRASHES_BIKES;
extern const StatId_char   STAT_DIST_AS_PASSENGER_TRAIN;
extern const StatId_char   STAT_FLIGHT_TIME;
extern const StatId_char   STAT_DIST_AS_PASSENGER_TAXI;
extern const StatId_char   STAT_DIST_DRIVING_CAR;
extern const StatId_char   STAT_DIST_DRIVING_BIKE;
extern const StatId_char   STAT_DIST_DRIVING_PLANE;
extern const StatId_char   STAT_DIST_DRIVING_QUADBIKE;
extern const StatId_char   STAT_DIST_DRIVING_HELI;
extern const StatId_char   STAT_DIST_DRIVING_BIKE;
extern const StatId_char   STAT_DIST_DRIVING_BICYCLE;
extern const StatId_char   STAT_DIST_DRIVING_BOAT;
extern const StatId_char   STAT_DIST_DRIVING_SUBMARINE;
extern const StatId_char   STAT_TIME_DRIVING_CAR;
extern const StatId_char   STAT_TIME_DRIVING_BIKE;
extern const StatId_char   STAT_TIME_DRIVING_CAR;
extern const StatId_char   STAT_TIME_DRIVING_PLANE;
extern const StatId_char   STAT_TIME_DRIVING_QUADBIKE;
extern const StatId_char   STAT_TIME_DRIVING_HELI;
extern const StatId_char   STAT_TIME_DRIVING_BIKE;
extern const StatId_char   STAT_TIME_DRIVING_BICYCLE;
extern const StatId_char   STAT_TIME_DRIVING_BOAT;
extern const StatId_char   STAT_TIME_DRIVING_SUBMARINE;
extern const StatId_char   STAT_DIST_CAR;
extern const StatId_char   STAT_DIST_PLANE;
extern const StatId_char   STAT_DIST_QUADBIKE;
extern const StatId_char   STAT_DIST_HELI;
extern const StatId_char   STAT_DIST_BIKE;
extern const StatId_char   STAT_DIST_BICYCLE;
extern const StatId_char   STAT_DIST_BOAT;
extern const StatId_char   STAT_DIST_SUBMARINE;
extern const StatId_char   STAT_LONGEST_CAM_TIME_DRIVING;
extern const StatId_char   STAT_PACKED_BOOL_0;
extern const StatId_char   STAT_STEALTH_ABILITY;
extern const StatId_char   STAT_AWD_CAR_BOMBS_ENEMY_KILLS;
extern const StatId_char   STAT_PACKED_STAT_INT0;
extern const StatId_char   STAT_DAMAGE_SCAR_NUMBER;
extern const StatId_char   STAT_NO_TIMES_WANTED_LEVEL;

extern const StatId_char   STAT_SPECIAL_ABILITY_ACTIVE_NUM;
extern const StatId_char   STAT_SPECIAL_ABILITY_ACTIVE_TIME;

extern const StatId_static STAT_MP_PACKED_INT_0;

extern const StatId_static STAT_PLAYER_MUTED_TALKERS_MET;	  
extern const StatId_static STAT_PLAYER_MUTED;				  

extern const StatId_char   STAT_EXPLOSIVE_DAMAGE_HITS;
extern const StatId_char   STAT_EXPLOSIVE_DAMAGE_HITS_ANYTHING;
extern const StatId_char   STAT_EXPLOSIVE_DAMAGE_SHOTS;	

extern const StatId_static STAT_SP_PROFILE_STAT_VERSION;
extern const StatId_static STAT_SP_SAVE_TIMESTAMP;

extern const StatId_char   STAT_UNARMED_PED_HITS;

extern const StatId_char STAT_MP_AWARD_HOLD_UP_SHOPS;
extern const StatId_char STAT_MP_AWARD_FMHORDWAVESSURVIVE;
extern const StatId_static STAT_MPPLY_TOTAL_CUSTOM_RACES_WON;
extern const StatId_char STAT_MP_STAT_CHAR_FM_PLAT_AWARD_COUNT;
extern const StatId_char STAT_MISSIONS_PASSED;

// Hero Stats
extern const StatId_char STAT_NUMBER_STOLEN_CARS;
extern const StatId_static STAT_MPPLY_KILL_DEATH_RATIO;

// CLEAR SOME STATS - After Save Migration being successful.
extern const StatId_char STAT_BIGGEST_VICTIM;
extern const StatId_char STAT_BIGGEST_VICTIM_NAME;
extern const StatId_char STAT_BIGGEST_VICTIM_KILLS;
extern const StatId_char STAT_ARCHENEMY;
extern const StatId_char STAT_ARCHENEMY_NAME;
extern const StatId_char STAT_ARCHENEMY_KILLS;
extern const StatId_char STAT_SAVE_MIGRATION_CLEAR_STAT;

// Some stats used for telemetry
extern const StatId_char STAT_PROSTITUTES_FREQUENTED;
extern const StatId_char STAT_LAP_DANCED_BOUGHT;

#if __ASSERT

extern const atHashWithStringNotFinal STATTYPE_WEAPON_HEADSHOTS;
extern const atHashWithStringNotFinal STATTYPE_WEAPON_HELDTIME;
extern const atHashWithStringNotFinal STATTYPE_WEAPON_HITS;
extern const atHashWithStringNotFinal STATTYPE_WEAPON_KILLS;
extern const atHashWithStringNotFinal STATTYPE_WEAPON_SHOTS;
extern const atHashWithStringNotFinal STATTYPE_WEAPON_ACQUIRED;

#endif // __ASSERT

extern const StatId_char STAT_NUMBER_OF_SESSIONS_FM;
extern const StatId_char STAT_AVERAGE_TIME_PER_SESSON;

#endif // _STATSENUMS_H_

// EOF

