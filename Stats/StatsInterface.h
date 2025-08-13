//
// StatsInterface.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#ifndef STATSINTERFACE_H
#define STATSINTERFACE_H


// --- Include Files ------------------------------------------------------------

// Rage headers
#include "string/stringhash.h"
#include "rline/cloud/rlcloud.h"

// Game headers
#include "Stats/StatsDataMgr.h"
#include "Stats/StatsTypes.h"
#include "system/control.h"


// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

namespace rage
{
	class rlProfileStatsValue;
}

class CPed;
class CWeaponInfo;

//PURPOSE
//  Interface to access in game stats.
namespace StatsInterface
{
	//Multiplayer groups divided by ped Group relationship
	const u32 MP_GROUP_COP = ATSTRINGHASH("COP", 0x0a49e591c);
	const u32 MP_GROUP_GANG_VAGOS = ATSTRINGHASH("AMBIENT_GANG_MEXICAN", 0x011a9a7e3);
	const u32 MP_GROUP_GANG_LOST  = ATSTRINGHASH("AMBIENT_GANG_LOST", 0x090c7da60);

	//Multiplayer Teams
	enum eMultiplayerTeams
	{
		MP_TEAM_INVALID      = -1
		,MP_TEAM_COP         = 0
		,MP_TEAM_CRIM_GANG_1 = 1
		,MP_TEAM_CRIM_GANG_2 = 2
		,MP_TEAM_CRIM_GANG_3 = 3
		,MP_TEAM_CRIM_GANG_4 = 4
		,MP_MAX_NUM_TEAMS    = 5
	};

	//Cache weapon hash Id.
	// - "HELDTIME"
	// - "DB_HELDTIME"
	// - "HITS"
	// - "SHOTS"
	// - "HITS_CAR"
	// - "ENEMY_KILLS"
	// - "KILLS"
	// - "DEATHS"
	// - "SHOTS"
	enum  eWeaponStatsTypes
	{
		WEAPON_STAT_HELDTIME
		,WEAPON_STAT_DB_HELDTIME
		,WEAPON_STAT_HITS
		,WEAPON_STAT_SHOTS
		,WEAPON_STAT_HITS_CAR
		,WEAPON_STAT_ENEMY_KILLS
		,WEAPON_STAT_KILLS
		,WEAPON_STAT_DEATHS
		,WEAPON_STAT_HEADSHOTS
	};

	//Asserts if the hash key is invalid and check for exceptions.
	void ValidateKey(const StatId& ASSERT_ONLY(keyHash), const char* ASSERT_ONLY(msg));

	// --- STATS DISPLAY LIST ---------------------------------------

	// PURPOSE:  Returns the number of members in a certain stats display list.
	int GetListMemberCount(const int listId);

	// Returns which character we're currently playing as.
	eCharacterIndex GetCharacterIndex();
	int GetCharacterSlot();
	bool ValidCharacterSlot(const int slot);

	// --- STATS ACESS METHODS ---------------------------------------

	// PURPOSE: Returns TRUE if the stat Exists.
	bool  IsKeyValid(const StatId& keyHash);
	bool  IsKeyValid(const int keyHash);
	bool  IsKeyValid(const char* str);

	// PURPOSE: Returns the stat Label.
	const char* GetKeyName(const StatId& keyHash);

	// PURPOSE: Returns TRUE if the stat has a value > 0 or a string has already been defined for the value.
	bool  GetStatIsNotNull(const StatId& keyHash);

	// PURPOSE: Returns the tag type of a stat.
	StatType  GetStatType(const StatId& keyHash);
	const char*   GetStatTypeLabel(const StatId& keyHash);
	rlProfileStatsType  GetExpectedProfileStatType(const StatId& keyHash);

	// PURPOSE: Returns the description of a stat.
	const sStatDescription* GetStatDesc(const StatId& keyHash);

	// PURPOSE: Returns TRUE if the base type of the stat is "type".
	bool  GetIsBaseType(const StatId& keyHash, const StatType type);

	// PURPOSE: Find a stat iterator.
	bool  FindStatIterator(const StatId& keyHash, CStatsDataMgr::StatsMap::Iterator& statIter  ASSERT_ONLY(, const char* debugText));

	// PURPOSE: Get cached value of the stat STAT_MPPLY_LAST_MP_CHAR.
	int  GetCurrentMultiplayerCharaterSlot();

	// PURPOSE: Returns if the character slot is valid
	bool IsValidCharacterSlot(const int characterSlot);

	// PURPOSE: Get the boss goon ID at the specified character slot
	u64 GetBossGoonUUID(const int characterSlot);

	// PURPOSE: Called when the player signs out locally.
	void  SignedOut();

	// PURPOSE: Called when the player signs in locally.
	void  SignedIn();

	// PURPOSE: Called when the player signs in locally.
	bool  CanAccessStat(const StatId& keyHash);

	//Multiplayer stats savegame operations
	void  SetAllStatsToSynched(const int savegameSlot, const bool onlinestat, const bool includeServerAuthoritative = false);
	void  SetAllStatsToSynchedByGroup(const eProfileStatsGroups group, const bool onlinestat, const bool includeServerAuthoritative);
	bool  LoadPending(const int slot);
	bool  SavePending();
	bool  CloudSaveFailed(const u32 slotNumber);
	bool  PendingSaveRequests();
	void  ClearSaveFailed(const int file);
	bool  Load(const eStatsLoadType type, const int slotNumber);
	bool  Save(const eStatsSaveType type, const u32 slotNumber, const eSaveTypes savetype = STAT_SAVETYPE_DEFAULT, const u32 uSaveDelay = 4000, const int reason = 0);
	bool  CloudFileLoadPending(const int file);
	bool  CloudFileLoadFailed(const int file);
	bool  GetBlockSaveRequests( );
	void  SetNoRetryOnFail(bool value);

	//THIS IS A VERY BAD NETWORK HEAVY FUNCTION! DO NOT CALL A LOT!!!!!
	void  TryMultiplayerSave( const eSaveTypes savetype = STAT_SAVETYPE_DEFAULT, u32 uSaveDelay = 4000);

	// PURPOSE: Use this to set stat data.
	bool  GetStatData(const StatId& keyHash, void* pData, const unsigned SizeofData);
	bool  SetStatData(const StatId& keyHash, void* pData, const unsigned SizeofData, const u32 flags = STATUPDATEFLAG_DEFAULT);
	void  SetStatData(const StatId& keyHash, int data, const u32 flags = STATUPDATEFLAG_DEFAULT);
	void  SetStatData(const StatId& keyHash, float data, const u32 flags = STATUPDATEFLAG_DEFAULT);
	void  SetStatData(const StatId& keyHash, char* data, const u32 flags = STATUPDATEFLAG_DEFAULT);
	void  SetStatData(const StatId& keyHash, bool data, const u32 flags = STATUPDATEFLAG_DEFAULT);
	void  SetStatData(const StatId& keyHash, u8 data, const u32 flags = STATUPDATEFLAG_DEFAULT);
	void  SetStatData(const StatId& keyHash, u16 data, const u32 flags = STATUPDATEFLAG_DEFAULT);
	void  SetStatData(const StatId& keyHash, u32 data, const u32 flags = STATUPDATEFLAG_DEFAULT);
	void  SetStatData(const StatId& keyHash, u64 data, const u32 flags = STATUPDATEFLAG_DEFAULT);
	void  SetStatData(const StatId& keyHash, s64 data, const u32 flags = STATUPDATEFLAG_DEFAULT);

	// PURPOSE: Set stat to be synched.
	void  SetSynched(const StatId& keyHash);

	// PURPOSE: Use this to get data as int.
	int  GetIntStat(const StatId& keyHash);
	// PURPOSE: Use this to get data as float.
	float  GetFloatStat(const StatId& keyHash);
	// PURPOSE: Use this to get data as bool.
	bool  GetBooleanStat(const StatId& keyHash);
	// PURPOSE: Use this to get data as u8.
	u8  GetUInt8Stat(const StatId& keyHash);
	// PURPOSE: Use this to get data as u16.
	u16  GetUInt16Stat(const StatId& keyHash);
	// PURPOSE: Use this to get data as u32.
	u32  GetUInt32Stat(const StatId& keyHash);
	// PURPOSE: Use this to get data as u64.
	u64  GetUInt64Stat(const StatId& keyHash);
	// PURPOSE: Use this to get data as s64.
	s64  GetInt64Stat(const StatId& keyHash);

	// PURPOSE:  Increments the stat by amount.
	// NOTES:    Can not be used with boolean values and strings ... 
	void  IncrementStat(const StatId& keyHash, const float fAmount, const u32 flags = STATUPDATEFLAG_DEFAULT);
	// PURPOSE:  Decrements the stat by amount.
	// NOTES:    Can not be used with boolean values and strings ... 
	void  DecrementStat(const StatId& keyHash, const float fAmount, const u32 flags = STATUPDATEFLAG_DEFAULT);
	// PURPOSE: Set a new value to the stat if the new value is greater than the current value.
	// NOTES:    Can not be used with boolean values and strings ... 
	void  SetGreater(const StatId& keyHash, const float fNewAmount, const u32 flags = STATUPDATEFLAG_DEFAULT);
	// PURPOSE: Set a new value to the stat if the new value is lesser than the current value.
	// NOTES:    Can not be used with boolean values and strings ... 
	void  SetLesser(const StatId& keyHash, const float fNewAmount, const u32 flags = STATUPDATEFLAG_DEFAULT);

	// PURPOSE: Called when the rlProfileStats should synchronize their stats.
	bool  SyncProfileStats(const bool force = false, const bool multiplayer = false);
	bool  CanSyncProfileStats();

	// PURPOSE: Methods to pack and UnPack date values into and from the stat.
	void    UnPackDate(const u64 date, int& year, int& month, int& day, int& hour, int& min, int& sec, int& mill);
	u64       PackDate(const u64 year, const u64 month, const u64 day, const u64 hour, const u64 min, const u64 sec, const u64 mill);
	void  GetPosixDate(int& year, int& month, int& day, int& hour, int& min, int& sec);
	void    GetBSTDate(int& year, int& month, int& day, int& hour, int& min, int& sec);

	// PURPOSE: Methods to pack and UnPack position values into and from the stat.
	//          Position Packed:
	//                       - Each float gets 20 bits, being 16 bits for the integer part and 4 for the decimal.
	//                       - last 4 bits are for the sign.
	void  UnPackPos(const u64 pos, float& x, float& y, float& z);
	u64     PackPos(float x, float y, float z);

	//PURPOSE: Returns true if the ped is a gang member / cop in cnc.
	bool  GetPedOfType(const CPed* ped, const u32 type);

	//PURPOSE: Returns true if the ped is a gang member.
	bool  IsGangMember(const CPed* ped);

	//PURPOSE: Returns true if the ped is a gang member.
	bool  IsSameTeam(const CPed* pedA, const CPed* pedB);

	bool  IsPlayerCop(const CPed* player);
	bool  IsPlayerVagos(const CPed* player);
	bool  IsPlayerLost(const CPed* player);

	// PURPOSE: Called when the parachute is deployed
	void  RegisterParachuteDeployed(CPed* ped); 
	// PURPOSE: Called when player presses the button to deploy parachute
	void RegisterPrepareParachuteDeploy(CPed* ped);
	// PURPOSE: Called when the parachute is destroyed (landing, crashing or bailing)
	void RegisterDestroyParachute(const CPed* ped);

    // PURPOSE: Called when the player starts the skydive animation
	void  RegisterStartSkydiving(CPed* ped); 

	// PURPOSE: Called when a player is about to be teleported back on the map after falling through it
	void FallenThroughTheMap(CPed* pPed);

	// PURPOSE: Return TRUE if the player is a cheater
	bool  IsBadSport(bool fromScript = false OUTPUT_ONLY(, const bool displayOutput = false));

	// PURPOSE: Update current player prefix used.
	int    GetPedModelPrefix(CPed* playerPed);
	void   SetStatsModelPrefix(CPed* playerPed);
	u32    GetStatsModelPrefix();
	StatId GetStatsModelHashId(const char *stat);
	bool   GetStatsPlayerModelValid();
	bool   GetStatsPlayerModelIsMultiplayer();
	bool   GetStatsPlayerModelIsSingleplayer();

	// PURPOSE: Set/Get data packed into stats (Valid for types - int's and u64).
	bool SetMaskedInt(const StatId& keyHash, int data, int shift, int numberOfBits, const bool coderAssert = false, const u32 flags = STATUPDATEFLAG_DEFAULT);
	bool GetMaskedInt(const int keyHash, int& data, int shift, int numberOfBits, int playerindex);

	// PURPOSE: Save local ped scar data.
	void SavePedScarData(CPed* ped);
	void ClearPedScarData( );

	// PURPOSE: Apply local player scar data to ped.
	void ApplyPedScarData(CPed* ped, const u32 character);

	// PURPOSE: Setup multiplayer savegames service.
	rlCloudOnlineService   GetCloudSavegameService();
	void   SetCloudSavegameService(const rlCloudOnlineService service);
	bool   CanUseEncrytionInMpSave();

	// PURPOSE: Get Currently cached weapon hash.
	void    UpdateLocalPlayerWeaponInfoStatId(const CWeaponInfo* wi, const char* vehicleName);
	StatId  GetLocalPlayerWeaponInfoStatId(const eWeaponStatsTypes type, const CWeaponInfo* wi, const CPed* playerPed);
	StatId  GetWeaponInfoHashId(const char* typeName, const CWeaponInfo* wi, const CPed* ped);

	// PURPOSE: Called when a profile setting changes value.
	void UpdateProfileSetting(const int id);

	// PURPOSE: Retrieve default flush interval.
	u32   GetVehicleLeaderboardWriteInterval( );
	bool  GetVehicleLeaderboardWriteOnSavegame( );
	bool  GetVehicleLeaderboardKillWrite( );

	// PURPOSE: Called to write vehicle leaderboard records on mp before a flush.
	void  TryVehicleLeaderboardWriteOnMatchEnd( );

	// PURPOSE: Called to write vehicle leaderboard records on mp before a flush.
	bool  TryVehicleLeaderboardWriteOnBoot( );

	// PURPOSE: Perform some checks on the stats after sync with the server
	void CheckForProfileAwardStat(sStatDataPtr* ppStat);

	bool HasLocalPlayerPurchasedCESP();

#if KEYBOARD_MOUSE_SUPPORT
	void UpdateUsingKeyboard(bool usingKeyboard, const s32 lastFrameDeviceIndex, const s32 deviceIndex);
#endif // KEYBOARD_MOUSE_SUPPORT
};

#endif // STATSINTERFACE_H

// EOF

